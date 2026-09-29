#include <GUI/PrimaryButton.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

struct ContextAwareComboBoxTestAccess
{
    static std::function<void(int)> createPopupResultHandler(
        ContextAwareComboBox& comboBox)
    {
        return comboBox.createPopupResultHandler();
    }

    static bool hasActivePointerInteraction(
        const ContextAwareComboBox& comboBox)
    {
        return comboBox.pointerInteractionActive;
    }

    static bool isCancelPending(const ContextAwareComboBox& comboBox)
    {
        return comboBox.cancelPendingPointerRelease;
    }

    static juce::MouseInputSource::InputSourceType getPointerSourceType(
        const ContextAwareComboBox& comboBox)
    {
        return comboBox.pointerSourceType;
    }

    static int getPointerSourceIndex(const ContextAwareComboBox& comboBox)
    {
        return comboBox.pointerSourceIndex;
    }

    static void setPointerSource(
        ContextAwareComboBox& comboBox,
        juce::MouseInputSource::InputSourceType sourceType,
        int sourceIndex)
    {
        comboBox.pointerSourceType = sourceType;
        comboBox.pointerSourceIndex = sourceIndex;
    }

    static std::uint64_t getPopupSessionRevision(
        const ContextAwareComboBox& comboBox)
    {
        return comboBox.popupSessionRevision;
    }

    static void capturePopupRequest(ContextAwareComboBox& comboBox)
    {
        comboBox.capturePopupRequest();
    }
};

struct BandPanelModeTestAccess
{
    static ContextAwareComboBox& getModeBox(BandPanel& panel,
                                             size_t modeIndex)
    {
        return panel.distortionModes.at(modeIndex);
    }

    static std::function<void(int)> createPopupResultHandler(
        BandPanel& panel,
        size_t modeIndex)
    {
        return ContextAwareComboBoxTestAccess::createPopupResultHandler(
            panel.distortionModes.at(modeIndex));
    }
};

struct GlobalPanelSlopeTestAccess
{
    static ContextAwareComboBox& getSlopeBox(GlobalPanel& panel)
    {
        auto& controls = panel.getEqControls();
        const auto id = fire::eq::parameterID(controls.getSelectedNode(), fire::eq::Field::slope);
        ContextAwareComboBox* result = nullptr;
        for (auto* child : controls.getChildren())
            if (auto* menu = dynamic_cast<ContextAwareComboBox*>(child);
                menu != nullptr && menu->getComponentID() == id)
                result = menu;
        REQUIRE(result != nullptr);
        return *result;
    }

    static std::function<void(int)> createPopupResultHandler(
        GlobalPanel& panel,
        bool lowCut)
    {
        REQUIRE(panel.getSelectedEqNode() == (lowCut ? 0 : 2));
        return ContextAwareComboBoxTestAccess::createPopupResultHandler(getSlopeBox(panel));
    }

    static void setFilterEnabled(GlobalPanel& panel, bool enabled)
    {
        REQUIRE(panel.filterBypassButton != nullptr);
        panel.filterBypassButton->setToggleState(
            enabled, juce::sendNotificationSync);
    }

    static void selectModule(GlobalPanel& panel, int moduleIndex)
    {
        auto* button = moduleIndex == 0 ? &panel.filterSwitch
                     : moduleIndex == 1 ? &panel.downsampleSwitch
                                        : &panel.graphSwitch;
        button->setToggleState(true, juce::sendNotificationSync);
    }

    static std::array<juce::Button*, 3> getIconButtons(
        GlobalPanel& panel)
    {
        REQUIRE(panel.filterBypassButton != nullptr);
        REQUIRE(panel.downsampleBypassButton != nullptr);
        juce::Button* pointPower = nullptr;
        for (auto* child : panel.getEqControls().getChildren())
            if (auto* button = dynamic_cast<juce::Button*>(child);
                button != nullptr && button->getTitle().endsWith(" power"))
                pointPower = button;
        REQUIRE(pointPower != nullptr);
        return { panel.filterBypassButton.get(), panel.downsampleBypassButton.get(), pointPower };
    }
};

namespace
{
class ParameterGestureRecorder final
    : public juce::AudioProcessorParameter::Listener
{
public:
    void parameterValueChanged(int, float) override {}

    void parameterGestureChanged(int, bool gestureIsStarting) override
    {
        gestures.push_back(gestureIsStarting);
    }

    std::vector<bool> gestures;
};

class OneShotParameterValueCallback final
    : public juce::AudioProcessorParameter::Listener
{
public:
    explicit OneShotParameterValueCallback(std::function<void()> callbackToUse)
        : callback(std::move(callbackToUse))
    {
    }

    void parameterValueChanged(int, float) override
    {
        auto callbackToInvoke = std::move(callback);

        if (callbackToInvoke != nullptr)
            callbackToInvoke();
    }

    void parameterGestureChanged(int, bool) override {}

private:
    std::function<void()> callback;
};

template <typename PanelType>
class DeletePanelOnSliderDragEnd final : public juce::Slider::Listener
{
public:
    DeletePanelOnSliderDragEnd(juce::Slider& sliderToObserve,
                               std::unique_ptr<PanelType>& panelToDelete)
        : slider(&sliderToObserve), panel(panelToDelete)
    {
        sliderToObserve.addListener(this);
    }

    ~DeletePanelOnSliderDragEnd() override
    {
        if (slider != nullptr)
            slider->removeListener(this);
    }

    void sliderValueChanged(juce::Slider*) override {}
    void sliderDragStarted(juce::Slider*) override {}

    void sliderDragEnded(juce::Slider*) override
    {
        ++dragEndCount;
        if (deletionStarted)
            return;

        deletionStarted = true;
        panel.reset();
        callbackCompleted = true;
    }

    juce::Component::SafePointer<juce::Slider> slider;
    std::unique_ptr<PanelType>& panel;
    int dragEndCount = 0;
    bool deletionStarted = false;
    bool callbackCompleted = false;
};

void setParameterValue(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float normalizedValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(normalizedValue);
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::ModifierKeys modifiers,
                                bool wasDragged = false)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             modifiers,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             time,
             position,
             time,
             1,
             wasDragged };
}

void beginPrimaryClick(juce::Button& button)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(
        component,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
}

void endPrimaryClick(juce::Button& button)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseUp(makeMouseEvent(component, {}));
}

template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;

    for (auto* child : root.getChildren())
        if (child != nullptr)
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;

    return nullptr;
}

std::vector<juce::Button*> collectDirectButtons(juce::Component& panel)
{
    std::vector<juce::Button*> buttons;

    for (auto* child : panel.getChildren())
    {
        if (auto* button = dynamic_cast<juce::Button*>(child))
        {
            const auto id = button->getComponentID();
            // Empty insert slots are deliberately absent from the module rail.
            if ((id.startsWith("masterFx") || id.startsWith("bandFx")) && ! button->isVisible()) continue;
            buttons.push_back(button);
        }
        else if (dynamic_cast<juce::ScrollBar*>(child) == nullptr)
        {
            auto nested = collectDirectButtons(*child);
            buttons.insert(buttons.end(), nested.begin(), nested.end());
        }
    }

    return buttons;
}

std::vector<juce::ComboBox*> collectDirectComboBoxes(juce::Component& panel)
{
    std::vector<juce::ComboBox*> comboBoxes;

    for (auto* child : panel.getChildren())
        if (auto* comboBox = dynamic_cast<juce::ComboBox*>(child))
            comboBoxes.push_back(comboBox);

    return comboBoxes;
}

void selectGlobalSlopeType(GlobalPanel& panel, bool lowCut)
{
    panel.selectEqNode(lowCut ? 0 : 2);
}

void prepareBandShapePanel(BandPanel& panel)
{
    panel.setBounds(0, 0, 1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    panel.setSwitch(1, true);
}

void prepareGlobalSlopePanel(GlobalPanel& panel, bool lowCut)
{
    panel.setBounds(0, 0, 1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    selectGlobalSlopeType(panel, lowCut);
}

void dismissGlobalSlopePopups(GlobalPanel& panel)
{
    GlobalPanelSlopeTestAccess::getSlopeBox(panel).hidePopup();
    juce::PopupMenu::dismissAllActiveMenus();
}

std::vector<juce::ModifierKeys> rejectedPointerModifiers()
{
    std::vector<juce::ModifierKeys> modifiers {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::rightButtonModifier }
    };

#if JUCE_MAC
    modifiers.emplace_back(juce::ModifierKeys::leftButtonModifier
                           | juce::ModifierKeys::ctrlModifier);
#endif

    return modifiers;
}

void checkPanelButtons(juce::Component& panel, int expectedButtonCount)
{
    auto buttons = collectDirectButtons(panel);
    REQUIRE(buttons.size() == static_cast<size_t>(expectedButtonCount));
    // The insert page now owns one additional PrimaryTextButton for Clouds
    // Freeze. Keep it in the same rejected-gesture checks as every old button.
    REQUIRE(std::count_if(buttons.begin(), buttons.end(), [](const auto* button)
    {
        return button->getButtonText() == "Freeze";
    }) == 1);

    for (auto* button : buttons)
    {
        REQUIRE(button != nullptr);
        INFO("button text: " << button->getButtonText());
        INFO("component ID: " << button->getComponentID());
        CHECK((dynamic_cast<PrimaryTextButton*>(button) != nullptr
               || dynamic_cast<PrimaryToggleButton*>(button) != nullptr));

        for (const auto modifiers : rejectedPointerModifiers())
        {
            CAPTURE(modifiers.getRawFlags());
            const auto oldToggleState = button->getToggleState();
            auto& component = static_cast<juce::Component&>(*button);

            component.mouseDown(makeMouseEvent(component, modifiers));
            component.mouseDrag(makeMouseEvent(component, modifiers, true));
            component.mouseUp(makeMouseEvent(component, {}, true));

            CHECK_FALSE(button->isDown());
            CHECK(button->getToggleState() == oldToggleState);
        }
    }
}
} // namespace

TEST_CASE("Control-panel buttons reject popup and auxiliary pointer gestures",
          "[control-panel][ui][input][primary-button]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    SECTION("band controls")
    {
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        checkPanelButtons(panel, 17);
    }

    SECTION("global controls")
    {
        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        checkPanelButtons(panel, 10 + 3 + EqControlsPanel::capacity);
    }
}

TEST_CASE("Clouds Freeze rejects auxiliary gestures without changing its attached parameter",
          "[control-panel][clouds-ui][ui][input][primary-button]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int scope : { 1, 0 })
    {
        CAPTURE(scope);
        FireAudioProcessor processor;
        const int slot = processor.addInsertEffect(scope, fire::effects::Type::granular);
        REQUIRE(slot >= 0);
        std::unique_ptr<juce::Component> panel;
        if (scope == 1)
            panel = std::make_unique<BandPanel>(processor, nullptr, nullptr, nullptr, nullptr, nullptr);
        else
            panel = std::make_unique<GlobalPanel>(processor, nullptr, nullptr, nullptr, nullptr, nullptr);
        panel->setBounds(0, 0, 1000, 500);
        panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel->setVisible(true);

        auto buttons = collectDirectButtons(*panel);
        const auto slotID = fire::effects::parameterID(scope, slot, fire::effects::typeField);
        auto slotButton = std::find_if(buttons.begin(), buttons.end(), [&](const auto* button)
        { return button->getComponentID() == slotID; });
        REQUIRE(slotButton != buttons.end());
        (*slotButton)->triggerClick();

        buttons = collectDirectButtons(*panel);
        const auto freezeID = fire::clouds_params::parameterID(scope, slot, fire::clouds_params::freezeField);
        auto found = std::find_if(buttons.begin(), buttons.end(), [&](const auto* button)
        { return button->getComponentID() == freezeID; });
        REQUIRE(found != buttons.end());
        auto& freeze = **found;
        REQUIRE(freeze.isShowing());
        REQUIRE(dynamic_cast<PrimaryTextButton*>(&freeze) != nullptr);
        auto* parameter = processor.treeState.getRawParameterValue(freezeID);
        REQUIRE(parameter != nullptr);
        const auto originalValue = parameter->load();
        const auto originalToggle = freeze.getToggleState();
        for (const auto modifiers : rejectedPointerModifiers())
        {
            CAPTURE(modifiers.getRawFlags());
            auto& component = static_cast<juce::Component&>(freeze);
            component.mouseDown(makeMouseEvent(component, modifiers));
            component.mouseDrag(makeMouseEvent(component, modifiers, true));
            component.mouseUp(makeMouseEvent(component, {}, true));
            CHECK_FALSE(freeze.isDown());
            CHECK(freeze.getToggleState() == originalToggle);
            CHECK(juce::exactlyEqual(parameter->load(), originalValue));
        }
        beginPrimaryClick(freeze);
        endPrimaryClick(freeze);
        CHECK(freeze.getToggleState() != originalToggle);
        CHECK((parameter->load() > 0.5f) == freeze.getToggleState());
    }
}

TEST_CASE("Band button releases cannot cross attachment targets",
          "[control-panel][band][ui][input][attachment][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto band0ID =
        ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0);
    const auto band1ID =
        ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 1);
    setParameterValue(processor, band0ID, 0.0f);
    setParameterValue(processor, band1ID, 0.0f);

    BandPanel panel(processor, {}, {}, {}, {}, {});
    panel.setBounds(0, 0, 1000, 500);
    auto* band0Parameter = processor.treeState.getParameter(band0ID);
    auto* band1Parameter = processor.treeState.getParameter(band1ID);
    REQUIRE(band0Parameter != nullptr);
    REQUIRE(band1Parameter != nullptr);
    ParameterGestureRecorder band0Gestures;
    ParameterGestureRecorder band1Gestures;
    band0Parameter->addListener(&band0Gestures);
    band1Parameter->addListener(&band1Gestures);
    const juce::ScopeGuard removeListeners { [&]
    {
        band0Parameter->removeListener(&band0Gestures);
        band1Parameter->removeListener(&band1Gestures);
    } };

    auto& button = panel.driveBypassButton;
    beginPrimaryClick(button);
    REQUIRE(button.isDown());

    panel.setFocusBandNum(1);
    CHECK(panel.getFocusBandNum() == 1);
    CHECK_FALSE(button.isDown());
    endPrimaryClick(button);

    CHECK(band0Parameter->getValue() == 0.0f);
    CHECK(band1Parameter->getValue() == 0.0f);
    CHECK(band0Gestures.gestures.empty());
    CHECK(band1Gestures.gestures.empty());

    beginPrimaryClick(button);
    REQUIRE(button.isDown());
    endPrimaryClick(button);

    CHECK(band0Parameter->getValue() == 0.0f);
    CHECK(band1Parameter->getValue() == 1.0f);
    CHECK(band0Gestures.gestures.empty());
    CHECK(band1Gestures.gestures
          == std::vector<bool> { true, false });
}

TEST_CASE("Band button Return activates its current attachment synchronously",
          "[control-panel][band][ui][input][keyboard][attachment]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto band0ID =
        ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0);
    const auto band1ID =
        ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 1);
    setParameterValue(processor, band0ID, 0.0f);
    setParameterValue(processor, band1ID, 0.0f);
    BandPanel panel(processor, {}, {}, {}, {}, {});
    panel.setBounds(0, 0, 1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    auto* band0Parameter = processor.treeState.getParameter(band0ID);
    auto* band1Parameter = processor.treeState.getParameter(band1ID);
    REQUIRE(band0Parameter != nullptr);
    REQUIRE(band1Parameter != nullptr);
    ParameterGestureRecorder band0Gestures;
    ParameterGestureRecorder band1Gestures;
    band0Parameter->addListener(&band0Gestures);
    band1Parameter->addListener(&band1Gestures);
    const juce::ScopeGuard removeListeners { [&]
    {
        band0Parameter->removeListener(&band0Gestures);
        band1Parameter->removeListener(&band1Gestures);
    } };

    REQUIRE(panel.driveBypassButton.isEnabled());
    REQUIRE(panel.driveBypassButton.isVisible());
    REQUIRE(panel.driveBypassButton.isShowing());
    REQUIRE(panel.driveBypassButton.keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    CHECK(band0Parameter->getValue() == 1.0f);
    CHECK(band1Parameter->getValue() == 0.0f);
    CHECK(band0Gestures.gestures
          == std::vector<bool> { true, false });
    CHECK(band1Gestures.gestures.empty());

    panel.setFocusBandNum(1);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    CHECK(panel.getFocusBandNum() == 1);
    CHECK(band0Parameter->getValue() == 1.0f);
    CHECK(band1Parameter->getValue() == 0.0f);
    CHECK(band0Gestures.gestures
          == std::vector<bool> { true, false });
    CHECK(band1Gestures.gestures.empty());
}

TEST_CASE("Control-panel buttons discard gestures at panel and host boundaries",
          "[control-panel][ui][input][host-visibility][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("direct BandPanel hide")
    {
        FireAudioProcessor processor;
        const auto parameterID =
            ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0);
        setParameterValue(processor, parameterID, 0.0f);
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        panel.setVisible(true);
        auto* parameter = processor.treeState.getParameter(parameterID);
        REQUIRE(parameter != nullptr);
        ParameterGestureRecorder gestures;
        parameter->addListener(&gestures);
        const juce::ScopeGuard removeListener {
            [&] { parameter->removeListener(&gestures); }
        };

        beginPrimaryClick(panel.driveBypassButton);
        REQUIRE(panel.driveBypassButton.isDown());
        panel.setVisible(false);
        CHECK_FALSE(panel.driveBypassButton.isDown());
        panel.setVisible(true);
        endPrimaryClick(panel.driveBypassButton);

        CHECK(parameter->getValue() == 0.0f);
        CHECK(gestures.gestures.empty());
    }

    SECTION("direct GlobalPanel hide")
    {
        FireAudioProcessor processor;
        setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
        setParameterValue(processor, HIGHCUT_BYPASSED_ID, 0.0f);
        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        panel.setVisible(true);
        panel.selectEqNode(2);
        auto* button = dynamic_cast<PrimaryToggleButton*>(
            GlobalPanelSlopeTestAccess::getIconButtons(panel)[2]);
        auto* parameter = processor.treeState.getParameter(HIGHCUT_BYPASSED_ID);
        REQUIRE(button != nullptr);
        REQUIRE(parameter != nullptr);
        ParameterGestureRecorder gestures;
        parameter->addListener(&gestures);
        const juce::ScopeGuard removeListener {
            [&] { parameter->removeListener(&gestures); }
        };

        beginPrimaryClick(*button);
        REQUIRE(button->isDown());
        panel.setVisible(false);
        CHECK_FALSE(button->isDown());
        panel.setVisible(true);
        endPrimaryClick(*button);

        CHECK(parameter->getValue() == 0.0f);
        CHECK(gestures.gestures.empty());
    }

    SECTION("host editor hide")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        const auto parameterID =
            ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0);
        setParameterValue(processor, parameterID, 0.0f);
        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        auto* panel = findDescendant<BandPanel>(*editor);
        auto* parameter = processor.treeState.getParameter(parameterID);
        REQUIRE(panel != nullptr);
        REQUIRE(parameter != nullptr);
        ParameterGestureRecorder gestures;
        parameter->addListener(&gestures);
        const juce::ScopeGuard removeListener {
            [&] { parameter->removeListener(&gestures); }
        };

        beginPrimaryClick(panel->driveBypassButton);
        REQUIRE(panel->driveBypassButton.isDown());
        editor->setVisible(false);
        CHECK_FALSE(panel->driveBypassButton.isDown());
        editor->setVisible(true);
        endPrimaryClick(panel->driveBypassButton);

        CHECK(parameter->getValue() == 0.0f);
        CHECK(gestures.gestures.empty());
        editor->removeFromDesktop();
    }
}

TEST_CASE("BandPanel closes shape mode popups at transient boundaries",
          "[control-panel][band][ui][shape][mode][popup][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    BandPanel panel(processor, {}, {}, {}, {}, {});
    prepareBandShapePanel(panel);

    const auto modeBoxes = collectDirectComboBoxes(panel);
    REQUIRE(modeBoxes.size() == 4);
    const auto visibleModeIterator = std::find_if(
        modeBoxes.begin(), modeBoxes.end(), [](const auto* modeBox)
        {
            return modeBox->isVisible();
        });
    REQUIRE(visibleModeIterator != modeBoxes.end());
    auto* visibleMode = *visibleModeIterator;
    REQUIRE(visibleMode != nullptr);
    const juce::ScopeGuard cleanup { [&]
    {
        for (auto* modeBox : modeBoxes)
            modeBox->hidePopup();
        juce::PopupMenu::dismissAllActiveMenus();
        panel.removeFromDesktop();
    } };

    // Return marks the ComboBox popup active synchronously, before JUCE's
    // queued platform-window creation. This exercises the lifecycle contract
    // deterministically without requiring a display server.
    REQUIRE(visibleMode->keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    REQUIRE(visibleMode->isPopupActive());

    SECTION("focus change")
    {
        panel.setFocusBandNum(1, true);
        CHECK_FALSE(visibleMode->isPopupActive());
        panel.setFocusBandNum(0, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK_FALSE(visibleMode->isPopupActive());
    }

    SECTION("module change")
    {
        panel.setSwitch(0, true);
        CHECK_FALSE(visibleMode->isPopupActive());
        panel.setSwitch(1, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK_FALSE(visibleMode->isPopupActive());
    }

    SECTION("panel hide")
    {
        panel.setVisible(false);
        CHECK_FALSE(visibleMode->isPopupActive());
        panel.setVisible(true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK_FALSE(visibleMode->isPopupActive());
    }
}

TEST_CASE("BandPanel rejects stale shape mode popup results",
          "[control-panel][band][ui][shape][mode][popup][attachment][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    BandPanel panel(processor, {}, {}, {}, {}, {});
    prepareBandShapePanel(panel);

    const auto band0ID =
        ParameterIDAndName::getIDString(MODE_ID, 0);
    const auto band1ID =
        ParameterIDAndName::getIDString(MODE_ID, 1);
    auto* band0Parameter = processor.treeState.getParameter(band0ID);
    auto* band1Parameter = processor.treeState.getParameter(band1ID);
    REQUIRE(band0Parameter != nullptr);
    REQUIRE(band1Parameter != nullptr);

    const auto initialBand0Value = band0Parameter->getValue();
    const auto initialBand1Value = band1Parameter->getValue();
    ParameterGestureRecorder band0Gestures;
    ParameterGestureRecorder band1Gestures;
    int comboBoxChangeCount = 0;
    BandPanelModeTestAccess::getModeBox(panel, 0).onChange = [&]
    {
        ++comboBoxChangeCount;
    };
    band0Parameter->addListener(&band0Gestures);
    band1Parameter->addListener(&band1Gestures);
    const juce::ScopeGuard removeListeners { [&]
    {
        band0Parameter->removeListener(&band0Gestures);
        band1Parameter->removeListener(&band1Gestures);
        panel.removeFromDesktop();
    } };

    auto staleResult =
        BandPanelModeTestAccess::createPopupResultHandler(panel, 0);

    SECTION("focus ABA")
    {
        panel.setFocusBandNum(1, true);
        panel.setFocusBandNum(0, true);
    }

    SECTION("module ABA")
    {
        panel.setSwitch(0, true);
        panel.setSwitch(1, true);
    }

    SECTION("visibility ABA")
    {
        panel.setVisible(false);
        panel.setVisible(true);
    }

    // Open a replacement session before the old callback arrives. The stale
    // callback must neither commit nor close/invalidate this newer session.
    auto currentResult =
        BandPanelModeTestAccess::createPopupResultHandler(panel, 0);
    staleResult(6);

    CHECK(band0Parameter->getValue()
          == Catch::Approx(initialBand0Value));
    CHECK(band1Parameter->getValue()
          == Catch::Approx(initialBand1Value));
    CHECK(band0Gestures.gestures.empty());
    CHECK(band1Gestures.gestures.empty());
    CHECK(comboBoxChangeCount == 0);

    currentResult(6);

    CHECK(band0Parameter->getValue()
          == Catch::Approx(band0Parameter->convertTo0to1(5.0f)));
    CHECK(band1Parameter->getValue()
          == Catch::Approx(initialBand1Value));
    CHECK(band0Gestures.gestures
          == std::vector<bool> { true, false });
    CHECK(band1Gestures.gestures.empty());
    CHECK(comboBoxChangeCount == 1);
}

TEST_CASE("BandPanel late shape mode pointer release cannot reopen a popup",
          "[control-panel][band][ui][shape][mode][popup][mouse][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    BandPanel panel(processor, {}, {}, {}, {}, {});
    prepareBandShapePanel(panel);
    const juce::ScopeGuard cleanup { [&]
    {
        for (auto* modeBox : collectDirectComboBoxes(panel))
            modeBox->hidePopup();
        juce::PopupMenu::dismissAllActiveMenus();
        panel.removeFromDesktop();
    } };

    const auto modeBoxes = collectDirectComboBoxes(panel);
    const auto visibleModeIterator = std::find_if(
        modeBoxes.begin(), modeBoxes.end(), [](const auto* modeBox)
        {
            return modeBox->isVisible();
        });
    REQUIRE(visibleModeIterator != modeBoxes.end());
    auto* visibleMode = *visibleModeIterator;
    REQUIRE(visibleMode != nullptr);
    auto* modeLabel = findDescendant<juce::Label>(*visibleMode);
    REQUIRE(modeLabel != nullptr);

    auto& component = static_cast<juce::Component&>(*visibleMode);
    component.mouseDown(makeMouseEvent(
        *modeLabel,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    REQUIRE(visibleMode->isPopupActive());

    panel.setFocusBandNum(1, true);
    CHECK_FALSE(visibleMode->isPopupActive());
    panel.setFocusBandNum(0, true);

    SECTION("late release")
    {
        component.mouseUp(makeMouseEvent(*modeLabel, {}));
    }

    SECTION("late drag and release")
    {
        component.mouseDrag(makeMouseEvent(
            *modeLabel,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
            true));
        component.mouseUp(makeMouseEvent(*modeLabel, {}, true));
    }

    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    CHECK_FALSE(visibleMode->isPopupActive());
}

TEST_CASE("BandPanel shape mode commit survives synchronous panel destruction",
          "[control-panel][band][ui][shape][mode][popup][attachment][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto panel = std::make_unique<BandPanel>(
        processor,
        std::function<void(ModulatableSlider*)> {},
        std::function<void(ModulatableSlider*)> {},
        std::function<void(ModulatableSlider*)> {},
        std::function<void(ModulatableSlider*)> {},
        std::function<void(ModulatableSlider*)> {});
    prepareBandShapePanel(*panel);

    const auto parameterID =
        ParameterIDAndName::getIDString(MODE_ID, 0);
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);

    ParameterGestureRecorder gestures;
    OneShotParameterValueCallback destroyPanel { [&]
    {
        panel.reset();
    } };
    parameter->addListener(&gestures);
    parameter->addListener(&destroyPanel);
    const juce::ScopeGuard removeListeners { [&]
    {
        parameter->removeListener(&gestures);
        parameter->removeListener(&destroyPanel);
    } };

    auto result =
        BandPanelModeTestAccess::createPopupResultHandler(*panel, 0);
    result(6);

    CHECK(panel == nullptr);
    CHECK(parameter->getValue()
          == Catch::Approx(parameter->convertTo0to1(5.0f)));
    CHECK(gestures.gestures
          == std::vector<bool> { true, false });
}

TEST_CASE("GlobalPanel closes queued slope popups across context ABA",
          "[control-panel][global][filter][slope][popup][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool lowCut : { true, false })
    {
        DYNAMIC_SECTION((lowCut ? "low-cut" : "high-cut")
                        << " queued popup")
        {
            FireAudioProcessor processor;
            setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
            GlobalPanel panel(processor, {}, {}, {}, {}, {});
            prepareGlobalSlopePanel(panel, lowCut);

            auto* lowParameter =
                processor.treeState.getParameter(LOWCUT_SLOPE_ID);
            auto* highParameter =
                processor.treeState.getParameter(HIGHCUT_SLOPE_ID);
            REQUIRE(lowParameter != nullptr);
            REQUIRE(highParameter != nullptr);
            const auto initialLowValue = lowParameter->getValue();
            const auto initialHighValue = highParameter->getValue();
            ParameterGestureRecorder lowGestures;
            ParameterGestureRecorder highGestures;
            lowParameter->addListener(&lowGestures);
            highParameter->addListener(&highGestures);
            const juce::ScopeGuard cleanup { [&]
            {
                dismissGlobalSlopePopups(panel);
                panel.removeFromDesktop();
                lowParameter->removeListener(&lowGestures);
                highParameter->removeListener(&highGestures);
            } };

            auto exerciseBoundary = [&](const char* boundaryName,
                                        const std::function<void()>& boundary)
            {
                INFO("boundary: " << boundaryName);
                auto& targetContext =
                    GlobalPanelSlopeTestAccess::getSlopeBox(panel);
                auto& target = static_cast<juce::ComboBox&>(targetContext);
                REQUIRE(target.keyPressed(
                    juce::KeyPress { juce::KeyPress::returnKey }));
                REQUIRE(target.isPopupActive());

                boundary();

                CHECK_FALSE(target.isPopupActive());
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
                CHECK_FALSE(GlobalPanelSlopeTestAccess::getSlopeBox(panel).isPopupActive());
                CHECK(lowParameter->getValue()
                      == Catch::Approx(initialLowValue));
                CHECK(highParameter->getValue()
                      == Catch::Approx(initialHighValue));
                CHECK(lowGestures.gestures.empty());
                CHECK(highGestures.gestures.empty());
            };

            exerciseBoundary("filter type", [&]
            {
                selectGlobalSlopeType(panel, ! lowCut);
                selectGlobalSlopeType(panel, lowCut);
            });
            exerciseBoundary("panel visibility", [&]
            {
                panel.setVisible(false);
                panel.setVisible(true);
            });
            exerciseBoundary("filter enabled", [&]
            {
                GlobalPanelSlopeTestAccess::setFilterEnabled(panel, false);
                GlobalPanelSlopeTestAccess::setFilterEnabled(panel, true);
            });
            exerciseBoundary("module", [&]
            {
                GlobalPanelSlopeTestAccess::selectModule(panel, 1);
                GlobalPanelSlopeTestAccess::selectModule(panel, 0);
            });
            exerciseBoundary("ancestor enabled", [&]
            {
                panel.setEnabled(false);
                panel.setEnabled(true);
            });
        }
    }
}

TEST_CASE("GlobalPanel late slope label release cannot reopen a popup",
          "[control-panel][global][filter][slope][popup][mouse][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool lowCut : { true, false })
    {
        for (const bool dragged : { false, true })
        {
            DYNAMIC_SECTION((lowCut ? "low-cut" : "high-cut")
                            << (dragged ? " late drag/release" : " late release"))
            {
                FireAudioProcessor processor;
                setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
                GlobalPanel panel(processor, {}, {}, {}, {}, {});
                prepareGlobalSlopePanel(panel, lowCut);

                auto* lowParameter =
                    processor.treeState.getParameter(LOWCUT_SLOPE_ID);
                auto* highParameter =
                    processor.treeState.getParameter(HIGHCUT_SLOPE_ID);
                REQUIRE(lowParameter != nullptr);
                REQUIRE(highParameter != nullptr);
                const auto initialLowValue = lowParameter->getValue();
                const auto initialHighValue = highParameter->getValue();
                ParameterGestureRecorder lowGestures;
                ParameterGestureRecorder highGestures;
                lowParameter->addListener(&lowGestures);
                highParameter->addListener(&highGestures);
                const juce::ScopeGuard cleanup { [&]
                {
                    dismissGlobalSlopePopups(panel);
                    panel.removeFromDesktop();
                    lowParameter->removeListener(&lowGestures);
                    highParameter->removeListener(&highGestures);
                } };

                auto& targetContext =
                    GlobalPanelSlopeTestAccess::getSlopeBox(panel);
                auto& target = static_cast<juce::ComboBox&>(targetContext);
                const auto initialSelectedId = target.getSelectedId();
                auto* otherParameter = lowCut ? highParameter : lowParameter;
                const int initialOtherSelectedId = juce::roundToInt(
                    otherParameter->convertFrom0to1(otherParameter->getValue())) + 1;
                auto* label = findDescendant<juce::Label>(target);
                REQUIRE(label != nullptr);
                auto& component = static_cast<juce::Component&>(target);

                component.mouseDown(makeMouseEvent(
                    *label,
                    juce::ModifierKeys {
                        juce::ModifierKeys::leftButtonModifier }));
                REQUIRE(target.isPopupActive());

                selectGlobalSlopeType(panel, ! lowCut);
                CHECK_FALSE(target.isPopupActive());
                selectGlobalSlopeType(panel, lowCut);

                if (dragged)
                    component.mouseDrag(makeMouseEvent(
                        *label,
                        juce::ModifierKeys {
                            juce::ModifierKeys::leftButtonModifier },
                        true));

                component.mouseUp(makeMouseEvent(*label, {}, dragged));
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

                CHECK_FALSE(target.isPopupActive());
                CHECK(target.getSelectedId() == initialSelectedId);
                CHECK(panel.getSelectedEqNode() == (lowCut ? 0 : 2));
                CHECK(lowParameter->getValue()
                      == Catch::Approx(initialLowValue));
                CHECK(highParameter->getValue()
                      == Catch::Approx(initialHighValue));
                CHECK(lowGestures.gestures.empty());
                CHECK(highGestures.gestures.empty());

                REQUIRE(target.keyPressed(
                    juce::KeyPress { juce::KeyPress::returnKey }));
                CHECK(target.isPopupActive());
                target.hidePopup();
                selectGlobalSlopeType(panel, ! lowCut);
                CHECK(GlobalPanelSlopeTestAccess::getSlopeBox(panel).getSelectedId()
                      == initialOtherSelectedId);
            }
        }
    }
}

TEST_CASE("GlobalPanel rejects stale slope results without invalidating a replacement session",
          "[control-panel][global][filter][slope][popup][attachment][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    constexpr std::array<const char*, 5> boundaries {
        "filter type", "panel visibility", "filter enabled", "module", "ancestor enabled"
    };

    for (const bool lowCut : { true, false })
    {
        for (size_t boundaryIndex = 0;
             boundaryIndex < boundaries.size();
             ++boundaryIndex)
        {
            DYNAMIC_SECTION((lowCut ? "low-cut " : "high-cut ")
                            << boundaries[boundaryIndex])
            {
                FireAudioProcessor processor;
                setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
                GlobalPanel panel(processor, {}, {}, {}, {}, {});
                prepareGlobalSlopePanel(panel, lowCut);

                auto* lowParameter =
                    processor.treeState.getParameter(LOWCUT_SLOPE_ID);
                auto* highParameter =
                    processor.treeState.getParameter(HIGHCUT_SLOPE_ID);
                REQUIRE(lowParameter != nullptr);
                REQUIRE(highParameter != nullptr);
                const auto initialLowValue = lowParameter->getValue();
                const auto initialHighValue = highParameter->getValue();
                // The real EQ slope control is shared by the selected point.
                // Keep independent model snapshots/listeners for both targets.
                auto& targetBox = GlobalPanelSlopeTestAccess::getSlopeBox(panel);
                const auto initialTargetId = targetBox.getSelectedId();
                auto* otherParameterForUi = lowCut ? highParameter : lowParameter;
                const int initialOtherId = juce::roundToInt(otherParameterForUi->convertFrom0to1(
                    otherParameterForUi->getValue())) + 1;
                ParameterGestureRecorder lowGestures;
                ParameterGestureRecorder highGestures;
                int uiChangeCount = 0;
                targetBox.onChange = [&] { ++uiChangeCount; };
                lowParameter->addListener(&lowGestures);
                highParameter->addListener(&highGestures);
                const juce::ScopeGuard cleanup { [&]
                {
                    dismissGlobalSlopePopups(panel);
                    panel.removeFromDesktop();
                    lowParameter->removeListener(&lowGestures);
                    highParameter->removeListener(&highGestures);
                } };

                auto staleResult =
                    GlobalPanelSlopeTestAccess::createPopupResultHandler(
                        panel, lowCut);

                switch (boundaryIndex)
                {
                    case 0:
                        selectGlobalSlopeType(panel, ! lowCut);
                        selectGlobalSlopeType(panel, lowCut);
                        break;
                    case 1:
                        panel.setVisible(false);
                        panel.setVisible(true);
                        break;
                    case 2:
                        GlobalPanelSlopeTestAccess::setFilterEnabled(
                            panel, false);
                        GlobalPanelSlopeTestAccess::setFilterEnabled(
                            panel, true);
                        break;
                    case 3:
                        GlobalPanelSlopeTestAccess::selectModule(panel, 1);
                        GlobalPanelSlopeTestAccess::selectModule(panel, 0);
                        break;
                    case 4:
                        panel.setEnabled(false);
                        panel.setEnabled(true);
                        break;
                    default:
                        FAIL("Unexpected slope boundary index");
                }

                auto currentResult =
                    GlobalPanelSlopeTestAccess::createPopupResultHandler(
                        panel, lowCut);
                staleResult(4);

                CHECK(lowParameter->getValue()
                      == Catch::Approx(initialLowValue));
                CHECK(highParameter->getValue()
                      == Catch::Approx(initialHighValue));
                CHECK(targetBox.getSelectedId() == initialTargetId);
                CHECK(targetBox.getComponentID() == (lowCut ? LOWCUT_SLOPE_ID : HIGHCUT_SLOPE_ID));
                CHECK(lowGestures.gestures.empty());
                CHECK(highGestures.gestures.empty());
                CHECK(uiChangeCount == 0);

                currentResult(4);

                auto* const targetParameter =
                    lowCut ? lowParameter : highParameter;
                auto* const otherParameter =
                    lowCut ? highParameter : lowParameter;
                const auto otherInitialValue =
                    lowCut ? initialHighValue : initialLowValue;
                CHECK(targetParameter->getValue()
                      == Catch::Approx(
                          targetParameter->convertTo0to1(3.0f)));
                CHECK(otherParameter->getValue()
                      == Catch::Approx(otherInitialValue));
                CHECK(targetBox.getSelectedId() == 4);
                CHECK((lowCut ? lowGestures.gestures
                              : highGestures.gestures)
                      == std::vector<bool> { true, false });
                CHECK((lowCut ? highGestures.gestures
                              : lowGestures.gestures).empty());
                CHECK(uiChangeCount == 1);
                selectGlobalSlopeType(panel, ! lowCut);
                CHECK(GlobalPanelSlopeTestAccess::getSlopeBox(panel).getSelectedId() == initialOtherId);
                CHECK(otherParameter->getValue() == Catch::Approx(otherInitialValue));
                selectGlobalSlopeType(panel, lowCut);
                CHECK(GlobalPanelSlopeTestAccess::getSlopeBox(panel).getSelectedId() == 4);
            }
        }
    }
}

TEST_CASE("GlobalPanel slope commit survives synchronous panel destruction",
          "[control-panel][global][filter][slope][popup][attachment][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool lowCut : { true, false })
    {
        DYNAMIC_SECTION((lowCut ? "low-cut" : "high-cut")
                        << " synchronous destruction")
        {
            FireAudioProcessor processor;
            setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
            auto panel = std::make_unique<GlobalPanel>(
                processor,
                std::function<void(ModulatableSlider*)> {},
                std::function<void(ModulatableSlider*)> {},
                std::function<void(ModulatableSlider*)> {},
                std::function<void(ModulatableSlider*)> {},
                std::function<void(ModulatableSlider*)> {});
            prepareGlobalSlopePanel(*panel, lowCut);

            auto* lowParameter =
                processor.treeState.getParameter(LOWCUT_SLOPE_ID);
            auto* highParameter =
                processor.treeState.getParameter(HIGHCUT_SLOPE_ID);
            REQUIRE(lowParameter != nullptr);
            REQUIRE(highParameter != nullptr);
            auto* const targetParameter =
                lowCut ? lowParameter : highParameter;
            auto* const otherParameter =
                lowCut ? highParameter : lowParameter;
            const auto otherInitialValue = otherParameter->getValue();
            ParameterGestureRecorder targetGestures;
            ParameterGestureRecorder otherGestures;
            OneShotParameterValueCallback destroyPanel { [&]
            {
                panel.reset();
            } };
            targetParameter->addListener(&targetGestures);
            targetParameter->addListener(&destroyPanel);
            otherParameter->addListener(&otherGestures);
            const juce::ScopeGuard removeListeners { [&]
            {
                targetParameter->removeListener(&targetGestures);
                targetParameter->removeListener(&destroyPanel);
                otherParameter->removeListener(&otherGestures);
            } };

            auto result =
                GlobalPanelSlopeTestAccess::createPopupResultHandler(
                    *panel, lowCut);
            result(4);

            CHECK(panel == nullptr);
            CHECK(targetParameter->getValue()
                  == Catch::Approx(
                      targetParameter->convertTo0to1(3.0f)));
            CHECK(otherParameter->getValue()
                  == Catch::Approx(otherInitialValue));
            CHECK(targetGestures.gestures
                  == std::vector<bool> { true, false });
            CHECK(otherGestures.gestures.empty());
        }
    }
}

TEST_CASE("Global slope direction keys commit before a later context boundary",
          "[control-panel][global][filter][slope][keyboard][attachment][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr std::array<const char*, 5> boundaries {
        "filter type", "panel visibility", "filter enabled", "module", "ancestor enabled"
    };

    for (const bool lowCut : { true, false })
    {
        for (size_t boundaryIndex = 0;
             boundaryIndex < boundaries.size();
             ++boundaryIndex)
        {
            DYNAMIC_SECTION((lowCut ? "low-cut " : "high-cut ")
                            << boundaries[boundaryIndex])
            {
                FireAudioProcessor processor;
                setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
                GlobalPanel panel(processor, {}, {}, {}, {}, {});
                prepareGlobalSlopePanel(panel, lowCut);

                auto* targetParameter = processor.treeState.getParameter(
                    lowCut ? LOWCUT_SLOPE_ID : HIGHCUT_SLOPE_ID);
                auto* otherParameter = processor.treeState.getParameter(
                    lowCut ? HIGHCUT_SLOPE_ID : LOWCUT_SLOPE_ID);
                REQUIRE(targetParameter != nullptr);
                REQUIRE(otherParameter != nullptr);
                auto& target = GlobalPanelSlopeTestAccess::getSlopeBox(panel);
                const auto initialIndex = target.getSelectedItemIndex();
                const auto nextIndex = initialIndex + 1;
                REQUIRE(juce::isPositiveAndBelow(nextIndex,
                                                 target.getNumItems()));
                const auto expectedValue = static_cast<float>(nextIndex)
                                           / static_cast<float>(
                                               target.getNumItems() - 1);
                const auto otherInitialValue = otherParameter->getValue();
                ParameterGestureRecorder targetGestures;
                ParameterGestureRecorder otherGestures;
                targetParameter->addListener(&targetGestures);
                otherParameter->addListener(&otherGestures);
                const juce::ScopeGuard cleanup { [&]
                {
                    dismissGlobalSlopePopups(panel);
                    panel.removeFromDesktop();
                    targetParameter->removeListener(&targetGestures);
                    otherParameter->removeListener(&otherGestures);
                } };

                auto& comboBox = static_cast<juce::ComboBox&>(target);
                REQUIRE(comboBox.keyPressed(
                    juce::KeyPress { juce::KeyPress::rightKey }));

                // The key event itself is the user decision. It must not leave
                // a stock ComboBoxAttachment notification queued behind it.
                CHECK(targetParameter->getValue()
                      == Catch::Approx(expectedValue));
                CHECK(targetGestures.gestures
                      == std::vector<bool> { true, false });
                CHECK(otherParameter->getValue()
                      == Catch::Approx(otherInitialValue));
                CHECK(otherGestures.gestures.empty());

                switch (boundaryIndex)
                {
                    case 0:
                        selectGlobalSlopeType(panel, ! lowCut);
                        selectGlobalSlopeType(panel, lowCut);
                        break;
                    case 1:
                        panel.setVisible(false);
                        panel.setVisible(true);
                        break;
                    case 2:
                        GlobalPanelSlopeTestAccess::setFilterEnabled(
                            panel, false);
                        GlobalPanelSlopeTestAccess::setFilterEnabled(
                            panel, true);
                        break;
                    case 3:
                        GlobalPanelSlopeTestAccess::selectModule(panel, 1);
                        GlobalPanelSlopeTestAccess::selectModule(panel, 0);
                        break;
                    case 4:
                        panel.setEnabled(false);
                        panel.setEnabled(true);
                        break;
                    default:
                        FAIL("Unexpected slope boundary index");
                }

                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
                CHECK(targetParameter->getValue()
                      == Catch::Approx(expectedValue));
                CHECK(targetGestures.gestures
                      == std::vector<bool> { true, false });
                CHECK(otherParameter->getValue()
                      == Catch::Approx(otherInitialValue));
                CHECK(otherGestures.gestures.empty());
            }
        }
    }
}

TEST_CASE("Global slope direction keys reject an already invalid context",
          "[control-panel][global][filter][slope][keyboard][context][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool lowCut : { true, false })
    {
        for (const bool wrongModule : { false, true })
        {
            DYNAMIC_SECTION((lowCut ? "low-cut " : "high-cut ")
                            << (wrongModule ? "wrong module" : "wrong type"))
            {
                FireAudioProcessor processor;
                setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
                GlobalPanel panel(processor, {}, {}, {}, {}, {});
                prepareGlobalSlopePanel(panel, lowCut);

                auto* targetParameter = processor.treeState.getParameter(
                    lowCut ? LOWCUT_SLOPE_ID : HIGHCUT_SLOPE_ID);
                REQUIRE(targetParameter != nullptr);
                auto& target = GlobalPanelSlopeTestAccess::getSlopeBox(panel);
                const auto initialValue = targetParameter->getValue();
                const auto initialSelectedId = target.getSelectedId();
                ParameterGestureRecorder gestures;
                targetParameter->addListener(&gestures);
                const juce::ScopeGuard cleanup { [&]
                {
                    dismissGlobalSlopePopups(panel);
                    panel.removeFromDesktop();
                    targetParameter->removeListener(&gestures);
                } };

                if (wrongModule)
                    GlobalPanelSlopeTestAccess::selectModule(panel, 1);
                else
                {
                    setParameterValue(processor, fire::eq::parameterID(lowCut ? 0 : 2,
                        fire::eq::Field::type), 0.0f); // Bell has no slope control.
                    panel.getEqControls().refresh();
                    REQUIRE_FALSE(target.isEnabled());
                }

                auto& comboBox = static_cast<juce::ComboBox&>(target);
                REQUIRE(comboBox.keyPressed(
                    juce::KeyPress { juce::KeyPress::rightKey }));
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

                CHECK(targetParameter->getValue()
                      == Catch::Approx(initialValue));
                CHECK(target.getSelectedId() == initialSelectedId);
                CHECK(gestures.gestures.empty());
            }
        }
    }
}

TEST_CASE("Context-aware ComboBox rejects auxiliary and mixed pointer presses",
          "[control-panel][global][filter][slope][combobox][mouse][primary][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const std::array rejectedModifiers {
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::middleButtonModifier }
    };

    for (size_t modifierIndex = 0;
         modifierIndex < rejectedModifiers.size();
         ++modifierIndex)
    {
        const auto* sectionName = modifierIndex == 0
                                      ? "middle button"
                                      : "left plus middle buttons";
        DYNAMIC_SECTION(sectionName)
        {
            FireAudioProcessor processor;
            setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
            GlobalPanel panel(processor, {}, {}, {}, {}, {});
            prepareGlobalSlopePanel(panel, true);

            auto* parameter =
                processor.treeState.getParameter(LOWCUT_SLOPE_ID);
            REQUIRE(parameter != nullptr);
            auto& target =
                GlobalPanelSlopeTestAccess::getSlopeBox(panel);
            const auto initialSelectedId = target.getSelectedId();
            const auto initialValue = parameter->getValue();
            ParameterGestureRecorder gestures;
            parameter->addListener(&gestures);
            const juce::ScopeGuard cleanup { [&]
            {
                dismissGlobalSlopePopups(panel);
                panel.removeFromDesktop();
                parameter->removeListener(&gestures);
            } };

            auto& component = static_cast<juce::Component&>(target);
            component.mouseDown(makeMouseEvent(
                component, rejectedModifiers[modifierIndex]));

            CHECK_FALSE(target.isPopupActive());
            CHECK_FALSE(
                ContextAwareComboBoxTestAccess::hasActivePointerInteraction(
                    target));
            CHECK(ContextAwareComboBoxTestAccess::getPointerSourceIndex(target)
                  == -1);

            component.mouseDrag(makeMouseEvent(
                component, rejectedModifiers[modifierIndex], true));
            component.mouseUp(makeMouseEvent(component, {}, true));
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            CHECK_FALSE(target.isPopupActive());
            CHECK(target.getSelectedId() == initialSelectedId);
            CHECK(parameter->getValue() == Catch::Approx(initialValue));
            CHECK(gestures.gestures.empty());
        }
    }
}

TEST_CASE("Context-aware ComboBox owns and recovers its opener pointer",
          "[control-panel][global][filter][slope][combobox][mouse][source][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    prepareGlobalSlopePanel(panel, true);

    auto* parameter = processor.treeState.getParameter(LOWCUT_SLOPE_ID);
    REQUIRE(parameter != nullptr);
    auto& target = GlobalPanelSlopeTestAccess::getSlopeBox(panel);
    auto& component = static_cast<juce::Component&>(target);
    const auto primaryDown = makeMouseEvent(
        component,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
    const auto sourceType = primaryDown.source.getType();
    const auto sourceIndex = primaryDown.source.getIndex();
    const auto initialValue = parameter->getValue();
    ParameterGestureRecorder gestures;
    parameter->addListener(&gestures);
    const juce::ScopeGuard cleanup { [&]
    {
        dismissGlobalSlopePopups(panel);
        panel.removeFromDesktop();
        parameter->removeListener(&gestures);
    } };

    SECTION("foreign events cannot finish the opener press")
    {
        component.mouseDown(primaryDown);
        REQUIRE(target.isPopupActive());
        REQUIRE(ContextAwareComboBoxTestAccess::hasActivePointerInteraction(
            target));
        CHECK(ContextAwareComboBoxTestAccess::getPointerSourceType(target)
              == sourceType);
        CHECK(ContextAwareComboBoxTestAccess::getPointerSourceIndex(target)
              == sourceIndex);
        const auto sessionRevision =
            ContextAwareComboBoxTestAccess::getPopupSessionRevision(target);

        ContextAwareComboBoxTestAccess::setPointerSource(
            target, juce::MouseInputSource::touch, sourceIndex + 17);
        component.mouseDrag(makeMouseEvent(
            component,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
            true));
        component.mouseUp(makeMouseEvent(component, {}, true));
        component.mouseMove(makeMouseEvent(component, {}));

        CHECK(ContextAwareComboBoxTestAccess::hasActivePointerInteraction(
            target));
        CHECK(ContextAwareComboBoxTestAccess::getPointerSourceType(target)
              == juce::MouseInputSource::touch);
        CHECK(ContextAwareComboBoxTestAccess::getPointerSourceIndex(target)
              == sourceIndex + 17);
        CHECK(target.isPopupActive());
        CHECK(ContextAwareComboBoxTestAccess::getPopupSessionRevision(target)
              == sessionRevision);

        ContextAwareComboBoxTestAccess::setPointerSource(
            target, sourceType, sourceIndex);
        component.mouseMove(makeMouseEvent(component, {}));

        CHECK_FALSE(
            ContextAwareComboBoxTestAccess::hasActivePointerInteraction(
                target));
        CHECK(ContextAwareComboBoxTestAccess::getPointerSourceIndex(target)
              == -1);
        CHECK(target.isPopupActive());
        CHECK(ContextAwareComboBoxTestAccess::getPopupSessionRevision(target)
              == sessionRevision);

        component.mouseUp(makeMouseEvent(component, {}, true));
        CHECK_FALSE(
            ContextAwareComboBoxTestAccess::hasActivePointerInteraction(
                target));
        CHECK(target.isPopupActive());
        CHECK(ContextAwareComboBoxTestAccess::getPopupSessionRevision(target)
              == sessionRevision);
        CHECK(parameter->getValue() == Catch::Approx(initialValue));
        CHECK(gestures.gestures.empty());
    }

    SECTION("missing release recovery preserves a replacement popup")
    {
        component.mouseDown(primaryDown);
        REQUIRE(target.isPopupActive());
        REQUIRE(ContextAwareComboBoxTestAccess::hasActivePointerInteraction(
            target));

        selectGlobalSlopeType(panel, false);
        selectGlobalSlopeType(panel, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        REQUIRE_FALSE(target.isPopupActive());
        REQUIRE(ContextAwareComboBoxTestAccess::isCancelPending(target));

        auto& comboBox = static_cast<juce::ComboBox&>(target);
        REQUIRE(comboBox.keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        REQUIRE(target.isPopupActive());
        const auto replacementRevision =
            ContextAwareComboBoxTestAccess::getPopupSessionRevision(target);

        ContextAwareComboBoxTestAccess::setPointerSource(
            target, sourceType, sourceIndex);
        component.mouseMove(makeMouseEvent(component, {}));

        CHECK_FALSE(
            ContextAwareComboBoxTestAccess::hasActivePointerInteraction(
                target));
        CHECK_FALSE(ContextAwareComboBoxTestAccess::isCancelPending(target));
        CHECK(ContextAwareComboBoxTestAccess::getPointerSourceIndex(target)
              == -1);
        CHECK(target.isPopupActive());
        CHECK(ContextAwareComboBoxTestAccess::getPopupSessionRevision(target)
              == replacementRevision);

        component.mouseUp(makeMouseEvent(component, {}, true));
        CHECK(target.isPopupActive());
        CHECK(ContextAwareComboBoxTestAccess::getPopupSessionRevision(target)
              == replacementRevision);
        CHECK(parameter->getValue() == Catch::Approx(initialValue));
        CHECK(gestures.gestures.empty());
    }
}

TEST_CASE("Context-aware ComboBox never routes mouse wheel input to an attachment",
          "[control-panel][global][filter][slope][mouse-wheel][attachment][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool lowCut : { true, false })
    {
        for (const float deltaY : { -0.25f, 0.25f })
        {
            DYNAMIC_SECTION((lowCut ? "low-cut " : "high-cut ")
                            << (deltaY < 0.0f ? "wheel down" : "wheel up"))
            {
                FireAudioProcessor processor;
                setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
                setParameterValue(
                    processor,
                    lowCut ? LOWCUT_SLOPE_ID : HIGHCUT_SLOPE_ID,
                    1.0f / 3.0f);
                GlobalPanel panel(processor, {}, {}, {}, {}, {});
                prepareGlobalSlopePanel(panel, lowCut);

                auto* targetParameter = processor.treeState.getParameter(
                    lowCut ? LOWCUT_SLOPE_ID : HIGHCUT_SLOPE_ID);
                REQUIRE(targetParameter != nullptr);
                auto& target = GlobalPanelSlopeTestAccess::getSlopeBox(panel);
                REQUIRE(target.getSelectedItemIndex() == 1);
                const auto initialValue = targetParameter->getValue();
                const auto initialSelectedId = target.getSelectedId();
                ParameterGestureRecorder gestures;
                targetParameter->addListener(&gestures);
                const juce::ScopeGuard cleanup { [&]
                {
                    dismissGlobalSlopePopups(panel);
                    panel.removeFromDesktop();
                    targetParameter->removeListener(&gestures);
                } };

                // Protect the invariant even if future theme/setup code turns
                // JUCE's normally-disabled ComboBox wheel option back on.
                target.setScrollWheelEnabled(true);
                juce::MouseWheelDetails wheel;
                wheel.deltaY = deltaY;
                auto& component = static_cast<juce::Component&>(target);
                component.mouseWheelMove(makeMouseEvent(component, {}),
                                         wheel);

                CHECK(target.getSelectedId() == initialSelectedId);
                CHECK(targetParameter->getValue()
                      == Catch::Approx(initialValue));
                CHECK(gestures.gestures.empty());
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
                CHECK(target.getSelectedId() == initialSelectedId);
                CHECK(targetParameter->getValue()
                      == Catch::Approx(initialValue));
                CHECK(gestures.gestures.empty());
            }
        }
    }
}

TEST_CASE("Context-aware ComboBox rejects cached commands outside its live peer",
          "[control-panel][ui][combobox][accessibility][lifecycle][stale][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (int boundaryIndex = 0; boundaryIndex < 3; ++boundaryIndex)
    {
        const auto* boundaryName = boundaryIndex == 0 ? "hidden"
                                 : boundaryIndex == 1 ? "disabled"
                                                      : "peer detached";
        DYNAMIC_SECTION(boundaryName)
        {
            FireAudioProcessor processor;
            auto* parameter =
                processor.treeState.getParameter(LOWCUT_SLOPE_ID);
            REQUIRE(parameter != nullptr);
            parameter->setValueNotifyingHost(0.0f);

            ContextAwareComboBox comboBox;
            comboBox.setBounds(0, 0, 120, 28);
            for (int itemId = 1; itemId <= 4; ++itemId)
                comboBox.addItem("Slope " + juce::String(itemId), itemId);
            comboBox.setSelectedId(1, juce::dontSendNotification);
            comboBox.configurePopupSession(
                [] { return std::uint64_t { 7 }; },
                [] { return true; },
                parameter);
            comboBox.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            comboBox.setVisible(true);
            REQUIRE(comboBox.isShowing());

            ParameterGestureRecorder gestures;
            parameter->addListener(&gestures);
            const juce::ScopeGuard cleanup { [&]
            {
                comboBox.hidePopup();
                juce::PopupMenu::dismissAllActiveMenus();
                comboBox.removeFromDesktop();
                parameter->removeListener(&gestures);
            } };

            auto* accessibility = comboBox.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            REQUIRE(accessibility->getActions().contains(
                juce::AccessibilityActionType::press));
            auto cachedResult =
                ContextAwareComboBoxTestAccess::createPopupResultHandler(
                    comboBox);
            ContextAwareComboBoxTestAccess::capturePopupRequest(comboBox);

            if (boundaryIndex == 0)
                comboBox.setVisible(false);
            else if (boundaryIndex == 1)
                comboBox.setEnabled(false);
            else
                comboBox.removeFromDesktop();

            if (boundaryIndex == 1)
            {
                REQUIRE_FALSE(comboBox.isEnabled());
                REQUIRE(comboBox.isShowing());
            }
            else
            {
                REQUIRE_FALSE(comboBox.isShowing());
            }
            comboBox.showPopup();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            CHECK_FALSE(comboBox.isPopupActive());

            cachedResult(2);
            REQUIRE(accessibility->getActions().invoke(
                juce::AccessibilityActionType::press));
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            CHECK_FALSE(comboBox.isPopupActive());
            CHECK(parameter->getValue() == Catch::Approx(0.0f));
            CHECK(gestures.gestures.empty());
        }
    }
}

TEST_CASE("Context-aware ComboBox direction sequences remain synchronous and menu-exclusive",
          "[control-panel][global][filter][slope][keyboard][sequence][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    prepareGlobalSlopePanel(panel, true);

    auto* parameter = processor.treeState.getParameter(LOWCUT_SLOPE_ID);
    REQUIRE(parameter != nullptr);
    auto& target = GlobalPanelSlopeTestAccess::getSlopeBox(panel);
    ParameterGestureRecorder gestures;
    parameter->addListener(&gestures);
    const juce::ScopeGuard cleanup { [&]
    {
        dismissGlobalSlopePopups(panel);
        panel.removeFromDesktop();
        parameter->removeListener(&gestures);
    } };
    auto& comboBox = static_cast<juce::ComboBox&>(target);

    REQUIRE(comboBox.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    REQUIRE(comboBox.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    REQUIRE(comboBox.keyPressed(
        juce::KeyPress { juce::KeyPress::leftKey }));
    CHECK(target.getSelectedItemIndex() == 1);
    CHECK(parameter->getValue() == Catch::Approx(1.0f / 3.0f));
    CHECK(gestures.gestures
          == std::vector<bool> {
              true, false, true, false, true, false
          });

    REQUIRE(comboBox.keyPressed(
        juce::KeyPress { juce::KeyPress::leftKey }));
    REQUIRE(target.getSelectedItemIndex() == 0);
    REQUIRE(gestures.gestures
            == std::vector<bool> {
                true, false, true, false, true, false, true, false
            });
    REQUIRE(comboBox.keyPressed(
        juce::KeyPress { juce::KeyPress::leftKey }));
    CHECK(target.getSelectedItemIndex() == 0);
    CHECK(gestures.gestures
          == std::vector<bool> {
              true, false, true, false, true, false, true, false
          });

    REQUIRE(comboBox.keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    REQUIRE(target.isPopupActive());
    const auto valueBeforePopupDirection = parameter->getValue();
    const auto gesturesBeforePopupDirection = gestures.gestures;
    REQUIRE(comboBox.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    CHECK(parameter->getValue()
          == Catch::Approx(valueBeforePopupDirection));
    CHECK(gestures.gestures == gesturesBeforePopupDirection);
}

TEST_CASE("Context-aware ComboBox keyboard commits survive synchronous panel destruction",
          "[control-panel][ui][combobox][keyboard][attachment][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("GlobalPanel slope")
    {
        FireAudioProcessor processor;
        setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
        auto panel = std::make_unique<GlobalPanel>(
            processor,
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {});
        prepareGlobalSlopePanel(*panel, true);

        auto* parameter = processor.treeState.getParameter(LOWCUT_SLOPE_ID);
        REQUIRE(parameter != nullptr);
        auto& target = GlobalPanelSlopeTestAccess::getSlopeBox(*panel);
        const auto nextIndex = target.getSelectedItemIndex() + 1;
        REQUIRE(juce::isPositiveAndBelow(nextIndex, target.getNumItems()));
        const auto expectedValue = static_cast<float>(nextIndex)
                                   / static_cast<float>(target.getNumItems() - 1);
        ParameterGestureRecorder gestures;
        OneShotParameterValueCallback destroyPanel { [&]
        {
            panel.reset();
        } };
        parameter->addListener(&gestures);
        parameter->addListener(&destroyPanel);
        const juce::ScopeGuard cleanup { [&]
        {
            parameter->removeListener(&gestures);
            parameter->removeListener(&destroyPanel);
            if (panel != nullptr)
            {
                dismissGlobalSlopePopups(*panel);
                panel->removeFromDesktop();
            }
        } };

        auto& comboBox = static_cast<juce::ComboBox&>(target);
        REQUIRE(comboBox.keyPressed(
            juce::KeyPress { juce::KeyPress::rightKey }));

        CHECK(panel == nullptr);
        CHECK(parameter->getValue() == Catch::Approx(expectedValue));
        CHECK(gestures.gestures == std::vector<bool> { true, false });
    }

    SECTION("BandPanel distortion mode")
    {
        FireAudioProcessor processor;
        auto panel = std::make_unique<BandPanel>(
            processor,
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {});
        prepareBandShapePanel(*panel);

        auto* parameter = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(MODE_ID, 0));
        REQUIRE(parameter != nullptr);
        auto& target = BandPanelModeTestAccess::getModeBox(*panel, 0);
        const auto nextIndex = target.getSelectedItemIndex() + 1;
        REQUIRE(juce::isPositiveAndBelow(nextIndex, target.getNumItems()));
        const auto expectedValue = static_cast<float>(nextIndex)
                                   / static_cast<float>(target.getNumItems() - 1);
        ParameterGestureRecorder gestures;
        OneShotParameterValueCallback destroyPanel { [&]
        {
            panel.reset();
        } };
        parameter->addListener(&gestures);
        parameter->addListener(&destroyPanel);
        const juce::ScopeGuard cleanup { [&]
        {
            parameter->removeListener(&gestures);
            parameter->removeListener(&destroyPanel);
        } };

        auto& comboBox = static_cast<juce::ComboBox&>(target);
        REQUIRE(comboBox.keyPressed(
            juce::KeyPress { juce::KeyPress::rightKey }));

        CHECK(panel == nullptr);
        CHECK(parameter->getValue() == Catch::Approx(expectedValue));
        CHECK(gestures.gestures == std::vector<bool> { true, false });
    }
}

TEST_CASE("Control-panel bulk lifecycle changes survive synchronous panel destruction",
          "[control-panel][ui][slider][lifecycle][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("BandPanel module switch")
    {
        FireAudioProcessor processor;
        setParameterValue(
            processor,
            ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
            1.0f);
        auto panel = std::make_unique<BandPanel>(
            processor,
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {});
        panel->setBounds(0, 0, 1000, 500);
        panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel->setVisible(true);
        auto* drive = panel->getDriveKnob();
        REQUIRE(drive != nullptr);
        REQUIRE(drive->isShowing());
        drive->mouseDown(makeMouseEvent(
            *drive,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
        REQUIRE(drive->hasActiveInteraction());
        DeletePanelOnSliderDragEnd<BandPanel> destroyOnDragEnd(
            *drive, panel);

        panel->setSwitch(1, true);

        CHECK(destroyOnDragEnd.dragEndCount == 1);
        CHECK(destroyOnDragEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }

    SECTION("BandPanel disable")
    {
        FireAudioProcessor processor;
        setParameterValue(
            processor,
            ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
            1.0f);
        auto panel = std::make_unique<BandPanel>(
            processor,
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {});
        panel->setBounds(0, 0, 1000, 500);
        panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel->setVisible(true);
        auto* drive = panel->getDriveKnob();
        REQUIRE(drive != nullptr);
        REQUIRE(drive->isShowing());
        REQUIRE(drive->isEnabled());
        drive->mouseDown(makeMouseEvent(
            *drive,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
        REQUIRE(drive->hasActiveInteraction());
        DeletePanelOnSliderDragEnd<BandPanel> destroyOnDragEnd(
            *drive, panel);

        panel->setBandKnobsStates(false, false);

        CHECK(destroyOnDragEnd.dragEndCount == 1);
        CHECK(destroyOnDragEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }

    SECTION("GlobalPanel filter bypass")
    {
        FireAudioProcessor processor;
        setParameterValue(processor, FILTER_BYPASS_ID, 1.0f);
        auto panel = std::make_unique<GlobalPanel>(
            processor,
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {},
            std::function<void(ModulatableSlider*)> {});
        panel->setBounds(0, 0, 1000, 500);
        panel->selectEqNode(0);
        panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel->setVisible(true);
        auto& lowcutFrequency = panel->getLowcutFreqKnob();
        REQUIRE(lowcutFrequency.isShowing());
        REQUIRE(lowcutFrequency.isEnabled());
        lowcutFrequency.mouseDown(makeMouseEvent(
            lowcutFrequency,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
        REQUIRE(lowcutFrequency.hasActiveInteraction());
        DeletePanelOnSliderDragEnd<GlobalPanel> destroyOnDragEnd(
            lowcutFrequency, panel);

        GlobalPanelSlopeTestAccess::setFilterEnabled(*panel, false);

        CHECK(destroyOnDragEnd.dragEndCount == 1);
        CHECK(destroyOnDragEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }
}

TEST_CASE("Control-panel routing menus expose stable accessibility semantics",
          "[control-panel][ui][accessibility][combo-box][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("Band distortion modes")
    {
        FireAudioProcessor processor;
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        panel.setSwitch(1, true);

        std::array<int, 4> selectedIds {};
        for (size_t modeIndex = 0; modeIndex < selectedIds.size(); ++modeIndex)
        {
            auto& modeBox = BandPanelModeTestAccess::getModeBox(
                panel, modeIndex);
            selectedIds[modeIndex] = modeBox.getSelectedId();
            CHECK(modeBox.getAccessibilityHandler() == nullptr);
        }

        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);

        for (size_t modeIndex = 0; modeIndex < selectedIds.size(); ++modeIndex)
        {
            CAPTURE(modeIndex);
            auto& modeBox = BandPanelModeTestAccess::getModeBox(
                panel, modeIndex);
            const auto bandNumber = juce::String(
                static_cast<int>(modeIndex) + 1);
            const auto expectedTitle = "Band " + bandNumber
                                     + " distortion mode";
            const auto expectedHelp = "Select the distortion mode for band "
                                    + bandNumber;

            CHECK(modeBox.getTitle() == expectedTitle);
            CHECK(modeBox.getTooltip() == expectedHelp);
            CHECK(modeBox.getSelectedId() == selectedIds[modeIndex]);

            auto* accessibility = modeBox.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK(accessibility->getRole()
                  == juce::AccessibilityRole::comboBox);
            CHECK(accessibility->getTitle() == expectedTitle);
            CHECK(accessibility->getHelp() == expectedHelp);
        }

        panel.removeFromDesktop();
        for (size_t modeIndex = 0; modeIndex < selectedIds.size(); ++modeIndex)
            CHECK(BandPanelModeTestAccess::getModeBox(panel, modeIndex)
                      .getAccessibilityHandler()
                  == nullptr);
    }

    SECTION("Selected EQ point slope")
    {
        FireAudioProcessor processor;
        setParameterValue(processor, LOWCUT_SLOPE_ID, 1.0f / 3.0f);
        setParameterValue(processor, HIGHCUT_SLOPE_ID, 2.0f / 3.0f);
        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        selectGlobalSlopeType(panel, true);
        auto& slopeBox = GlobalPanelSlopeTestAccess::getSlopeBox(panel);
        CHECK(slopeBox.getAccessibilityHandler() == nullptr);
        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);

        // Both parameter identities now use this one visible control. Visit
        // each target and return to the first to catch stale attachment state.
        for (bool lowCut : {true, false, true})
        {
            CAPTURE(lowCut);
            selectGlobalSlopeType(panel, lowCut);
            CHECK(&GlobalPanelSlopeTestAccess::getSlopeBox(panel) == &slopeBox);
            CHECK(slopeBox.isShowing());
            CHECK(slopeBox.getComponentID() == (lowCut ? LOWCUT_SLOPE_ID : HIGHCUT_SLOPE_ID));
            CHECK(slopeBox.getSelectedId() == (lowCut ? 2 : 3));
            CHECK(slopeBox.getTitle() == "EQ point slope");
            CHECK(slopeBox.getTooltip() == "Select the slope of the selected low-cut or high-cut point");
            auto* accessibility = slopeBox.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK(accessibility->getRole() == juce::AccessibilityRole::comboBox);
            CHECK(accessibility->getTitle() == slopeBox.getTitle());
            CHECK(accessibility->getHelp() == slopeBox.getTooltip());
        }
        CHECK(processor.treeState.getRawParameterValue(LOWCUT_SLOPE_ID)->load() == Catch::Approx(1.0f));
        CHECK(processor.treeState.getRawParameterValue(HIGHCUT_SLOPE_ID)->load() == Catch::Approx(2.0f));
        panel.removeFromDesktop();
        CHECK(slopeBox.getAccessibilityHandler() == nullptr);
    }
}

TEST_CASE("Control-panel icon buttons expose complete accessibility semantics",
          "[control-panel][ui][accessibility][button][tooltip][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto checkEmptyTextButtons = [](juce::Component& panel,
                                          size_t expectedCount)
    {
        size_t emptyTextButtonCount = 0;
        for (auto* button : collectDirectButtons(panel))
        {
            REQUIRE(button != nullptr);
            if (! button->getButtonText().trim().isEmpty())
                continue;

            ++emptyTextButtonCount;
            CAPTURE(button->getComponentID());
            CHECK_FALSE(button->getTitle().trim().isEmpty());
            CHECK_FALSE(button->getTooltip().trim().isEmpty());

            auto* accessibility = button->getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK(accessibility->getTitle() == button->getTitle());
            CHECK(accessibility->getHelp() == button->getTooltip());
        }

        CHECK(emptyTextButtonCount == expectedCount);
    };

    SECTION("Band controls follow their rebound band")
    {
        FireAudioProcessor processor;
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);

        const std::array<juce::Button*, 6> buttons {
            &panel.driveBypassButton,
            &panel.shapeBypassButton,
            &panel.compressorBypassButton,
            &panel.widthBypassButton,
            &panel.ottBypassButton,
            &panel.dcFilterButton
        };
        const std::array<juce::String, 6> functions {
            "Drive power",
            "Shape power",
            "Compressor power",
            "Stereo power",
            "OTT power",
            "DC filter"
        };
        const std::array<juce::String, 6> helpPrefixes {
            "Enable or bypass Drive processing",
            "Enable or bypass Shape processing",
            "Enable or bypass Compressor processing",
            "Enable or bypass Stereo processing",
            "Enable or bypass OTT processing",
            "Enable or disable the DC filter"
        };

        for (const auto bandIndex : { 0, 2 })
        {
            panel.setFocusBandNum(bandIndex, true);
            const auto bandNumber = juce::String(bandIndex + 1);

            for (size_t buttonIndex = 0;
                 buttonIndex < buttons.size();
                 ++buttonIndex)
            {
                auto& button = *buttons[buttonIndex];
                CAPTURE(bandIndex, buttonIndex);
                CHECK(button.getButtonText().isEmpty());
                CHECK(button.getTitle()
                      == "Band " + bandNumber + " "
                           + functions[buttonIndex]);
                CHECK(button.getTooltip()
                      == helpPrefixes[buttonIndex] + " for band "
                           + bandNumber);

                auto* accessibility = button.getAccessibilityHandler();
                REQUIRE(accessibility != nullptr);
                CHECK(accessibility->getTitle() == button.getTitle());
                CHECK(accessibility->getHelp() == button.getTooltip());
            }
        }

        checkEmptyTextButtons(panel, buttons.size());
    }

    SECTION("Global power and selected EQ point controls")
    {
        FireAudioProcessor processor;
        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);
        panel.selectEqNode(0);

        const auto buttons =
            GlobalPanelSlopeTestAccess::getIconButtons(panel);
        const std::array<juce::String, 3> titles {
            "Global EQ power", "Global Lo-Fi power", "EQ point 1 power"
        };
        const std::array<juce::String, 3> help {
            "Enable or bypass the global EQ",
            "Enable or bypass global Lo-Fi processing",
            "Enable or bypass the selected EQ point"
        };

        for (size_t buttonIndex = 0;
             buttonIndex < buttons.size();
             ++buttonIndex)
        {
            auto& button = *buttons[buttonIndex];
            CAPTURE(buttonIndex);
            CHECK(button.getButtonText().isEmpty());
            CHECK(button.getTitle() == titles[buttonIndex]);
            CHECK(button.getTooltip() == help[buttonIndex]);

            auto* accessibility = button.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK(accessibility->getTitle() == titles[buttonIndex]);
            CHECK(accessibility->getHelp() == help[buttonIndex]);
        }

        // Retain the tree-wide metadata check, including all navigation
        // slots and the three hidden legacy selection listeners.
        checkEmptyTextButtons(panel, 5 + EqControlsPanel::capacity + 1);
    }
}
