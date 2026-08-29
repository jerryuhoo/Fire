#include <GUI/PrimaryButton.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

struct BandPanelModeTestAccess
{
    static juce::ComboBox& getModeBox(BandPanel& panel, size_t modeIndex)
    {
        return panel.distortionModes.at(modeIndex);
    }

    static std::function<void(int)> createPopupResultHandler(
        BandPanel& panel,
        size_t modeIndex)
    {
        return panel.distortionModes.at(modeIndex)
            .createPopupResultHandler();
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
        if (auto* button = dynamic_cast<juce::Button*>(child))
            buttons.push_back(button);

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
        checkPanelButtons(panel, 12);
    }

    SECTION("global controls")
    {
        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        checkPanelButtons(panel, 8);
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
        setParameterValue(processor, HIGH_ID, 0.0f);
        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 500);
        panel.setVisible(true);
        auto* button = dynamic_cast<PrimaryTextButton*>(
            panel.findChildWithID("high_cut"));
        auto* parameter = processor.treeState.getParameter(HIGH_ID);
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
    panel.setBounds(0, 0, 1000, 500);
    panel.setVisible(true);
    panel.setSwitch(1, true);

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
    panel.setBounds(0, 0, 1000, 500);
    panel.setVisible(true);
    panel.setSwitch(1, true);

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
    panel.setBounds(0, 0, 1000, 500);
    panel.setVisible(true);
    panel.setSwitch(1, true);

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
    panel->setBounds(0, 0, 1000, 500);
    panel->setVisible(true);
    panel->setSwitch(1, true);

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
