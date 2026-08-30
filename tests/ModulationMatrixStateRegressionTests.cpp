#include "../Source/Panels/ControlPanel/ModulationMatrixPanel.h"
#include "../Source/Panels/TopPanel/Preset.h"
#include "../Source/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <vector>

struct ModulationMatrixRoutingComboBoxTestAccess
{
    static std::function<void(int)> createPopupResultHandler(
        ModulationMatrixRoutingComboBox& comboBox)
    {
        return comboBox.createPopupResultHandler();
    }

    static bool hasActivePointerInteraction(
        const ModulationMatrixRoutingComboBox& comboBox)
    {
        return comboBox.pointerInteractionActive;
    }

    static std::uint64_t getPopupSessionRevision(
        const ModulationMatrixRoutingComboBox& comboBox)
    {
        return comboBox.popupSessionRevision;
    }
};

struct ModulationMatrixRowTestAccess
{
    static void setAmountPointerSource(
        ModulationMatrixRow& row,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        row.amountSlider.pointerSourceType = type;
        row.amountSlider.pointerSourceIndex = index;
    }

    static bool hasActiveAmountGesture(
        const ModulationMatrixRow& row) noexcept
    {
        return row.amountSlider.primaryGestureInProgress;
    }
};

namespace
{
void replacePresetRoutings(juce::XmlElement& preset,
                           std::initializer_list<ModulationRouting> routings)
{
    auto* routingState = preset.getChildByName("MODULATION_STATE");
    if (routingState == nullptr)
        routingState = preset.createNewChildElement("MODULATION_STATE");

    routingState->deleteAllChildElements();
    for (const auto& routing : routings)
    {
        auto* routingXml = routingState->createNewChildElement("ROUTING");
        routing.writeToXml(*routingXml);
    }
}

void collectMatrixRows(juce::Component& component,
                       std::vector<ModulationMatrixRow*>& rows)
{
    if (auto* row = dynamic_cast<ModulationMatrixRow*>(&component))
        rows.push_back(row);

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            collectMatrixRows(*child, rows);
}

std::shared_ptr<ModulationRoutingEditSession> makeRoutingEditSession(
    FireAudioProcessor& processor)
{
    auto session = std::make_shared<ModulationRoutingEditSession>();
    session->revision =
        processor.getLfoManager().getModulationRoutingRevision();
    return session;
}

juce::Slider* findAmountSlider(juce::Component& component)
{
    if (auto* slider = dynamic_cast<juce::Slider*>(&component))
        return slider;

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            if (auto* slider = findAmountSlider(*child))
                return slider;

    return nullptr;
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers,
                                juce::Point<float> mouseDownPosition,
                                bool mouseWasDragged)
{
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
             mouseDownPosition,
             time,
             1,
             mouseWasDragged };
}

bool containsComboBoxText(juce::Component& component,
                          const juce::String& expectedText)
{
    if (auto* menu = dynamic_cast<juce::ComboBox*>(&component);
        menu != nullptr && menu->getText() == expectedText)
        return true;

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            if (containsComboBoxText(*child, expectedText))
                return true;

    return false;
}

juce::TextButton* findTextButton(juce::Component& component,
                                 const juce::String& buttonText)
{
    if (auto* button = dynamic_cast<juce::TextButton*>(&component);
        button != nullptr && button->getButtonText() == buttonText)
        return button;

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            if (auto* button = findTextButton(*child, buttonText))
                return button;

    return nullptr;
}

class NonParameterChangeCapture final : public juce::AudioProcessorListener
{
public:
    explicit NonParameterChangeCapture(FireAudioProcessor& processorToObserve)
        : processor(processorToObserve)
    {
        processor.addListener(this);
    }

    ~NonParameterChangeCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (details.nonParameterStateChanged)
            ++notificationCount;
    }

    FireAudioProcessor& processor;
    int notificationCount = 0;
};

class DeleteMatrixPanelOnHostNotification final
    : public juce::AudioProcessorListener
{
public:
    DeleteMatrixPanelOnHostNotification(
        FireAudioProcessor& processorToObserve,
        std::unique_ptr<ModulationMatrixPanel>& panelToDelete)
        : processor(processorToObserve), panel(panelToDelete)
    {
        processor.addListener(this);
    }

    ~DeleteMatrixPanelOnHostNotification() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (! details.nonParameterStateChanged || notificationCount != 0)
            return;

        ++notificationCount;
        panel.reset();
        callbackCompleted = true;
    }

    FireAudioProcessor& processor;
    std::unique_ptr<ModulationMatrixPanel>& panel;
    int notificationCount = 0;
    bool callbackCompleted = false;
};

class EditSliderOnHostNotification final
    : public juce::AudioProcessorListener
{
public:
    EditSliderOnHostNotification(
        FireAudioProcessor& processorToObserve,
        juce::Slider& sliderToEdit)
        : processor(processorToObserve), slider(sliderToEdit)
    {
        processor.addListener(this);
    }

    ~EditSliderOnHostNotification() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (! details.nonParameterStateChanged)
            return;

        ++notificationCount;
        if (attemptedEdit)
            return;

        attemptedEdit = true;
        slider.setValue(0.91, juce::sendNotificationSync);
        callbackCompleted = true;
    }

    FireAudioProcessor& processor;
    juce::Slider& slider;
    int notificationCount = 0;
    bool attemptedEdit = false;
    bool callbackCompleted = false;
};

void collectComboBoxes(juce::Component& component,
                       std::vector<juce::ComboBox*>& comboBoxes)
{
    if (auto* comboBox = dynamic_cast<juce::ComboBox*>(&component))
        comboBoxes.push_back(comboBox);

    for (int childIndex = 0;
         childIndex < component.getNumChildComponents();
         ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            collectComboBoxes(*child, comboBoxes);
}

ModulationMatrixRoutingComboBox* findRoutingComboBox(
    juce::Component& component,
    bool sourceMenu)
{
    std::vector<juce::ComboBox*> comboBoxes;
    collectComboBoxes(component, comboBoxes);
    const auto match = std::find_if(
        comboBoxes.begin(), comboBoxes.end(), [sourceMenu](const auto* comboBox)
        {
            const auto isSource = comboBox->getNumItems() > 0
                                  && comboBox->getItemText(0) == "LFO 1";
            return isSource == sourceMenu;
        });

    return match != comboBoxes.end()
               ? dynamic_cast<ModulationMatrixRoutingComboBox*>(*match)
               : nullptr;
}

class SliderInteractionCapture final : public juce::Slider::Listener
{
public:
    void sliderValueChanged(juce::Slider*) override { ++valueChangeCount; }
    void sliderDragStarted(juce::Slider*) override { ++dragStartCount; }
    void sliderDragEnded(juce::Slider*) override { ++dragEndCount; }

    int valueChangeCount = 0;
    int dragStartCount = 0;
    int dragEndCount = 0;
};

class ButtonClickCapture final : private juce::Button::Listener
{
public:
    explicit ButtonClickCapture(juce::Button& buttonToObserve)
        : button(buttonToObserve)
    {
        button.addListener(this);
    }

    ~ButtonClickCapture() override
    {
        button.removeListener(this);
    }

    int getClickCount() const noexcept { return clickCount; }

private:
    void buttonClicked(juce::Button*) override { ++clickCount; }

    juce::Button& button;
    int clickCount = 0;
};

std::vector<juce::ModifierKeys> getRejectedButtonModifiers()
{
    std::vector<juce::ModifierKeys> modifiers {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier }
    };

#if JUCE_MAC
    modifiers.emplace_back(juce::ModifierKeys::leftButtonModifier
                           | juce::ModifierKeys::ctrlModifier);
#endif

    return modifiers;
}

void exerciseButtonPointerGesture(juce::Button& button,
                                  juce::ModifierKeys downModifiers)
{
    const auto position = button.getLocalBounds().toFloat().getCentre();
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(button,
                                       position,
                                       downModifiers,
                                       position,
                                       false));
    component.mouseUp(makeMouseEvent(button,
                                     position,
                                     {},
                                     position,
                                     false));
}

void endButtonPointerGesture(juce::Button& button)
{
    const auto position = button.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(button).mouseUp(
        makeMouseEvent(button, position, {}, position, false));
}

void beginButtonPointerGesture(juce::Button& button,
                               juce::ModifierKeys downModifiers)
{
    const auto position = button.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(button).mouseDown(
        makeMouseEvent(button,
                       position,
                       downModifiers,
                       position,
                       false));
}
} // namespace

TEST_CASE("Modulation matrix host notifications may synchronously delete the panel",
          "[ui][modulation-matrix][lifetime][reentrancy][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    auto panel = std::make_unique<ModulationMatrixPanel>(processor);
    panel->setBounds(0, 0, 760, 420);

    SECTION("add route")
    {
        auto* addButton = findTextButton(*panel, "+ ADD ROUTE");
        REQUIRE(addButton != nullptr);
        DeleteMatrixPanelOnHostNotification host(processor, panel);

        exerciseButtonPointerGesture(
            *addButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(host.callbackCompleted);
        CHECK(host.notificationCount == 1);
        CHECK(panel == nullptr);
        CHECK(manager.getModulationRoutingsCopy().size() == 2);
    }

    SECTION("change source")
    {
        std::vector<ModulationMatrixRow*> rows;
        collectMatrixRows(*panel, rows);
        REQUIRE(rows.size() == 1);
        std::vector<juce::ComboBox*> comboBoxes;
        collectComboBoxes(*rows.front(), comboBoxes);
        const auto sourceMenu = std::find_if(
            comboBoxes.begin(), comboBoxes.end(), [](const auto* comboBox)
            {
                return comboBox->getNumItems() == 4;
            });
        REQUIRE(sourceMenu != comboBoxes.end());
        DeleteMatrixPanelOnHostNotification host(processor, panel);

        (*sourceMenu)->setSelectedId(2, juce::sendNotificationSync);

        CHECK(host.callbackCompleted);
        CHECK(host.notificationCount == 1);
        CHECK(panel == nullptr);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
    }

    SECTION("remove route")
    {
        std::vector<ModulationMatrixRow*> rows;
        collectMatrixRows(*panel, rows);
        REQUIRE(rows.size() == 1);
        auto* removeButton = dynamic_cast<juce::TextButton*>(
            rows.front()->findChildWithID("remove_button"));
        REQUIRE(removeButton != nullptr);
        DeleteMatrixPanelOnHostNotification host(processor, panel);

        exerciseButtonPointerGesture(
            *removeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(host.callbackCompleted);
        CHECK(host.notificationCount == 1);
        CHECK(panel == nullptr);
        CHECK(manager.getModulationRoutingsCopy().isEmpty());
    }
}

TEST_CASE("Modulation matrix invalidates shifted rows before host notification",
          "[ui][modulation-matrix][state][identity][reentrancy][aba][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add({});
        routings.add({});
        routings.add({});
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 3);
    auto* staleSecondAmount = findAmountSlider(*rows[1]);
    auto* firstRemoveButton = dynamic_cast<juce::TextButton*>(
        rows[0]->findChildWithID("remove_button"));
    REQUIRE(staleSecondAmount != nullptr);
    REQUIRE(firstRemoveButton != nullptr);

    const auto initialRevision = manager.getModulationRoutingRevision();
    EditSliderOnHostNotification host(processor, *staleSecondAmount);
    exerciseButtonPointerGesture(
        *firstRemoveButton,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

    CHECK(host.callbackCompleted);
    CHECK(host.notificationCount == 1);
    CHECK(panel.isUiRebuildPending());
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 1);
    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 2);
    CHECK(liveRoutings[0].depth == Catch::Approx(0.5f));
    CHECK(liveRoutings[1].depth == Catch::Approx(0.5f));
}

TEST_CASE("Modulation matrix routing menus reject auxiliary and mixed pointer input",
          "[ui][modulation-matrix][input][combo-box][pointer][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    juce::Component desktopHost;
    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    desktopHost.setBounds(0, 0, 760, 80);
    row.setBounds(0, 0, 760, 40);
    desktopHost.addAndMakeVisible(row);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);

    auto* sourceMenu = findRoutingComboBox(row, true);
    REQUIRE(sourceMenu != nullptr);
    REQUIRE(sourceMenu->isShowing());
    const auto selectedId = sourceMenu->getSelectedId();
    const auto sessionRevision =
        ModulationMatrixRoutingComboBoxTestAccess::getPopupSessionRevision(
            *sourceMenu);
    std::vector<juce::ModifierKeys> rejectedModifiers {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::middleButtonModifier }
    };
#if JUCE_MAC
    rejectedModifiers.emplace_back(juce::ModifierKeys::leftButtonModifier
                                   | juce::ModifierKeys::ctrlModifier);
#endif

    for (const auto modifiers : rejectedModifiers)
    {
        const auto position = sourceMenu->getLocalBounds().toFloat().getCentre();
        auto& component = static_cast<juce::Component&>(*sourceMenu);
        component.mouseDown(makeMouseEvent(*sourceMenu,
                                           position,
                                           modifiers,
                                           position,
                                           false));
        component.mouseUp(makeMouseEvent(*sourceMenu,
                                         position,
                                         {},
                                         position,
                                         false));

        CHECK_FALSE(sourceMenu->isPopupActive());
        CHECK_FALSE(ModulationMatrixRoutingComboBoxTestAccess::
                        hasActivePointerInteraction(*sourceMenu));
        CHECK(sourceMenu->getSelectedId() == selectedId);
        CHECK(ModulationMatrixRoutingComboBoxTestAccess::
                  getPopupSessionRevision(*sourceMenu)
              == sessionRevision);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == routing.sourceLfoIndex);
    }

    sourceMenu->setScrollWheelEnabled(true);
    juce::MouseWheelDetails wheel;
    wheel.deltaY = -0.5f;
    static_cast<juce::Component&>(*sourceMenu).mouseWheelMove(
        makeMouseEvent(*sourceMenu,
                       sourceMenu->getLocalBounds().toFloat().getCentre(),
                       {},
                       {},
                       false),
        wheel);
    CHECK(sourceMenu->getSelectedId() == selectedId);
    CHECK(manager.getModulationRoutingsCopy()[0].sourceLfoIndex
          == routing.sourceLfoIndex);
}

TEST_CASE("Modulation matrix popup results remain bound to their routing session",
          "[ui][modulation-matrix][combo-box][session][identity][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE(targets.size() >= 2);

    const ModulationRouting routing {
        0, targets[0].parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* sourceMenu = findRoutingComboBox(*rows.front(), true);
    auto* destinationMenu = findRoutingComboBox(*rows.front(), false);
    auto* amountSlider = findAmountSlider(*rows.front());
    REQUIRE(sourceMenu != nullptr);
    REQUIRE(destinationMenu != nullptr);
    REQUIRE(amountSlider != nullptr);
    REQUIRE(sourceMenu->isShowing());
    REQUIRE(destinationMenu->isShowing());

    const auto checkInitialAssignment = [&]
    {
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == routing.sourceLfoIndex);
        CHECK(routings[0].targetParameterID == routing.targetParameterID);
    };

    SECTION("source result cannot borrow a newer row revision")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        amountSlider->setValue(0.40, juce::sendNotificationSync);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());

        auto currentResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        currentResult(2);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
        CHECK(routings[0].depth == Catch::Approx(0.40f));
        CHECK(panel.isUiRebuildPending());
    }

    SECTION("destination result preserves its captured complete routing")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*destinationMenu);
        amountSlider->setValue(-0.35, juce::sendNotificationSync);
        staleResult(3);

        checkInitialAssignment();
        CHECK(destinationMenu->getSelectedId() == 2);
        CHECK_FALSE(panel.isUiRebuildPending());

        auto currentResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*destinationMenu);
        currentResult(3);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].targetParameterID == targets[1].parameterID);
        CHECK(routings[0].depth == Catch::Approx(-0.35f));
        CHECK(panel.isUiRebuildPending());
    }

    SECTION("a replacement popup owns the only consumable result")
    {
        auto supersededResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        auto currentResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);

        supersededResult(2);
        checkInitialAssignment();
        CHECK_FALSE(panel.isUiRebuildPending());

        currentResult(2);
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
        CHECK(panel.isUiRebuildPending());
    }

    SECTION("row hide and restore invalidates the old popup")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        rows.front()->setVisible(false);
        rows.front()->setVisible(true);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());
    }

    SECTION("disable and restore invalidates the old popup")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        rows.front()->setEnabled(false);
        rows.front()->setEnabled(true);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());
    }

    SECTION("panel hide and restore invalidates the old popup")
    {
        auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
            createPopupResultHandler(*sourceMenu);
        panel.setVisible(false);
        panel.setVisible(true);
        staleResult(2);

        checkInitialAssignment();
        CHECK(sourceMenu->getSelectedId() == 1);
        CHECK_FALSE(panel.isUiRebuildPending());
    }

    SECTION("keyboard direction commits synchronously")
    {
        CHECK(static_cast<juce::Component&>(*sourceMenu).keyPressed(
            juce::KeyPress { juce::KeyPress::downKey }));
        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].sourceLfoIndex == 1);
        CHECK(panel.isUiRebuildPending());
    }
}

TEST_CASE("Modulation matrix rebuilt rows reject late popup results",
          "[ui][modulation-matrix][combo-box][session][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* sourceMenu = findRoutingComboBox(*rows.front(), true);
    REQUIRE(sourceMenu != nullptr);
    auto staleResult = ModulationMatrixRoutingComboBoxTestAccess::
        createPopupResultHandler(*sourceMenu);

    panel.buildUiFromProcessorState();
    staleResult(2);

    const auto routings = manager.getModulationRoutingsCopy();
    REQUIRE(routings.size() == 1);
    CHECK(routings[0].sourceLfoIndex == routing.sourceLfoIndex);
    CHECK(routings[0].targetParameterID == routing.targetParameterID);
    CHECK_FALSE(panel.isUiRebuildPending());
}

TEST_CASE("Modulation matrix popup commit survives synchronous panel deletion",
          "[ui][modulation-matrix][combo-box][session][reentrancy][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    auto panel = std::make_unique<ModulationMatrixPanel>(processor);
    panel->setBounds(0, 0, 760, 420);
    panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel->setVisible(true);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(*panel, rows);
    REQUIRE(rows.size() == 1);
    auto* sourceMenu = findRoutingComboBox(*rows.front(), true);
    REQUIRE(sourceMenu != nullptr);
    auto popupResult = ModulationMatrixRoutingComboBoxTestAccess::
        createPopupResultHandler(*sourceMenu);
    DeleteMatrixPanelOnHostNotification host(processor, panel);

    popupResult(2);

    CHECK(host.callbackCompleted);
    CHECK(host.notificationCount == 1);
    CHECK(panel == nullptr);
    const auto routings = manager.getModulationRoutingsCopy();
    REQUIRE(routings.size() == 1);
    CHECK(routings[0].sourceLfoIndex == 1);
}

TEST_CASE("Modulation matrix amount accepts only primary-button drags",
          "[ui][modulation-matrix][input][amount-slider]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    constexpr float initialDepth = 0.25f;
    const ModulationRouting routing {
        0, targets.front().parameterID, initialDepth, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        REQUIRE_FALSE(routings.isEmpty());
        routings.set(0, routing);
    }

    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    row.setBounds(0, 0, 760, 40);
    auto* amountSlider = findAmountSlider(row);
    REQUIRE(amountSlider != nullptr);
    REQUIRE(amountSlider->getWidth() > 80);

    SliderInteractionCapture sliderCapture;
    NonParameterChangeCapture hostCapture(processor);
    amountSlider->addListener(&sliderCapture);

    const auto downPosition = juce::Point<float> {
        4.0f, amountSlider->getLocalBounds().toFloat().getCentreY()
    };
    const auto dragPosition = juce::Point<float> {
        static_cast<float>(amountSlider->getWidth() - 70), downPosition.y
    };

    const auto exercisePointerGesture = [&](juce::ModifierKeys modifiers)
    {
        amountSlider->mouseDown(makeMouseEvent(*amountSlider,
                                               downPosition,
                                               modifiers,
                                               downPosition,
                                               false));
        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               modifiers,
                                               downPosition,
                                               true));
        amountSlider->mouseUp(makeMouseEvent(*amountSlider,
                                             dragPosition,
                                             modifiers,
                                             downPosition,
                                             true));
    };

    const auto checkRejectedGesture = [&](juce::ModifierKeys modifiers)
    {
        exercisePointerGesture(modifiers);

        CHECK(amountSlider->getValue() == Catch::Approx(initialDepth));
        CHECK(amountSlider->getThumbBeingDragged() == -1);
        CHECK(sliderCapture.valueChangeCount == 0);
        CHECK(sliderCapture.dragStartCount == 0);
        CHECK(sliderCapture.dragEndCount == 0);
        CHECK(hostCapture.notificationCount == 0);

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(routings.isEmpty());
        CHECK(routings[0].depth == Catch::Approx(initialDepth));
    };

    SECTION("physical right click")
    {
        checkRejectedGesture(juce::ModifierKeys {
            juce::ModifierKeys::rightButtonModifier });
    }

    SECTION("physical middle click")
    {
        checkRejectedGesture(juce::ModifierKeys {
            juce::ModifierKeys::middleButtonModifier });
    }

#if JUCE_MAC
    SECTION("macOS Control-click")
    {
        checkRejectedGesture(juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier
            | juce::ModifierKeys::ctrlModifier });
    }
#endif

    SECTION("left-button drag")
    {
        exercisePointerGesture(juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier });

        CHECK(amountSlider->getValue() != Catch::Approx(initialDepth));
        CHECK(amountSlider->getThumbBeingDragged() == -1);
        CHECK(sliderCapture.valueChangeCount > 0);
        CHECK(sliderCapture.dragStartCount == 1);
        CHECK(sliderCapture.dragEndCount == 1);
        CHECK(hostCapture.notificationCount > 0);

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(routings.isEmpty());
        CHECK(routings[0].depth
              == Catch::Approx(static_cast<float>(amountSlider->getValue())));
    }

    SECTION("a foreign MouseInputSource cannot drag or release the owner gesture")
    {
        const auto primary = juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier };
        const auto ownerDown = makeMouseEvent(*amountSlider,
                                              downPosition,
                                              primary,
                                              downPosition,
                                              false);
        amountSlider->mouseDown(ownerDown);
        REQUIRE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        CHECK(sliderCapture.dragStartCount == 1);
        const auto valueAfterOwnerDown = amountSlider->getValue();

        const auto ownerType = ownerDown.source.getType();
        const auto ownerIndex = ownerDown.source.getIndex();
        const auto foreignType = ownerType == juce::MouseInputSource::mouse
                                     ? juce::MouseInputSource::touch
                                     : juce::MouseInputSource::mouse;
        ModulationMatrixRowTestAccess::setAmountPointerSource(
            row, foreignType, ownerIndex + 17);

        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               primary,
                                               downPosition,
                                               true));
        amountSlider->mouseUp(makeMouseEvent(*amountSlider,
                                             dragPosition,
                                             {},
                                             downPosition,
                                             true));
        CHECK(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        CHECK(sliderCapture.dragEndCount == 0);
        CHECK(amountSlider->getValue() == Catch::Approx(valueAfterOwnerDown));

        ModulationMatrixRowTestAccess::setAmountPointerSource(
            row, ownerType, ownerIndex);
        amountSlider->mouseDrag(makeMouseEvent(*amountSlider,
                                               dragPosition,
                                               primary,
                                               downPosition,
                                               true));
        amountSlider->mouseUp(makeMouseEvent(*amountSlider,
                                             dragPosition,
                                             {},
                                             downPosition,
                                             true));
        CHECK_FALSE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
        CHECK(sliderCapture.dragEndCount == 1);
        CHECK(amountSlider->getValue() != Catch::Approx(initialDepth));
    }

    amountSlider->removeListener(&sliderCapture);
}

TEST_CASE("Modulation matrix row dismissal closes amount and button gestures",
          "[ui][modulation-matrix][input][dismissal]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    const ModulationRouting routing {
        0, targets.front().parameterID, 0.25f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        manager.getModulationRoutings().set(0, routing);
    }

    ModulationMatrixRow row(
        processor, 0, routing, makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    juce::Component visibleParent;
    visibleParent.setBounds(0, 0, 760, 40);
    visibleParent.addAndMakeVisible(row);
    row.setBounds(0, 0, 760, 40);
    auto* amountSlider = findAmountSlider(row);
    auto* polarityButton = findTextButton(row, "Bi");
    REQUIRE(amountSlider != nullptr);
    REQUIRE(polarityButton != nullptr);
    SliderInteractionCapture sliderCapture;
    amountSlider->addListener(&sliderCapture);

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier };
    const auto sliderPosition = amountSlider->getLocalBounds().toFloat().getCentre();
    amountSlider->mouseDown(makeMouseEvent(*amountSlider,
                                          sliderPosition,
                                          primary,
                                          sliderPosition,
                                          false));
    static_cast<juce::Component&>(*polarityButton).mouseDown(
        makeMouseEvent(*polarityButton,
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       primary,
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       false));
    REQUIRE(sliderCapture.dragStartCount == 1);

    row.setVisible(false);
    CHECK_FALSE(ModulationMatrixRowTestAccess::hasActiveAmountGesture(row));
    CHECK(sliderCapture.dragEndCount == 1);

    const auto polarityBeforeRelease = routing.isBipolar;
    static_cast<juce::Component&>(*polarityButton).mouseUp(
        makeMouseEvent(*polarityButton,
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       {},
                       polarityButton->getLocalBounds().toFloat().getCentre(),
                       false));
    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE_FALSE(liveRoutings.isEmpty());
    CHECK(liveRoutings[0].isBipolar == polarityBeforeRelease);

    amountSlider->removeListener(&sliderCapture);
}

TEST_CASE("Modulation matrix buttons accept only complete primary-button clicks",
          "[ui][modulation-matrix][input][buttons]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.5f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    int deleteCount = 0;
    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [&deleteCount](std::uint64_t, ModulationRouting)
        {
            ++deleteCount;
        });
    row.setBounds(0, 0, 760, 40);
    auto* polarityButton = findTextButton(row, "Bi");
    auto* bypassButton = findTextButton(row, "Off");
    auto* removeButton = dynamic_cast<juce::TextButton*>(
        row.findChildWithID("remove_button"));
    REQUIRE(polarityButton != nullptr);
    REQUIRE(bypassButton != nullptr);
    REQUIRE(removeButton != nullptr);

    ModulationMatrixPanel panel(processor);
    panel.setBounds(0, 0, 760, 420);
    auto* addButton = findTextButton(panel, "+ ADD ROUTE");
    auto* closeButton = findTextButton(panel, "Close");
    REQUIRE(addButton != nullptr);
    REQUIRE(closeButton != nullptr);

    NonParameterChangeCapture host(processor);

    const auto exerciseRejectedGestures = [](juce::Button& button,
                                             const auto& checkInvariant)
    {
        for (const auto modifiers : getRejectedButtonModifiers())
        {
            exerciseButtonPointerGesture(button, modifiers);
            checkInvariant();
        }
    };

    SECTION("polarity")
    {
        ButtonClickCapture clicks(*polarityButton);
        const auto checkUnchanged = [&]
        {
            const auto routings = manager.getModulationRoutingsCopy();
            REQUIRE(routings.size() == 1);
            CHECK(routings[0].isBipolar);
            CHECK(polarityButton->getToggleState());
            CHECK(polarityButton->getButtonText() == "Bi");
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*polarityButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *polarityButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK_FALSE(routings[0].isBipolar);
        CHECK_FALSE(polarityButton->getToggleState());
        CHECK(polarityButton->getButtonText() == "Uni");
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 1);
    }

    SECTION("bypass")
    {
        ButtonClickCapture clicks(*bypassButton);
        const auto checkUnchanged = [&]
        {
            const auto routings = manager.getModulationRoutingsCopy();
            REQUIRE(routings.size() == 1);
            CHECK_FALSE(routings[0].isBypassed);
            CHECK_FALSE(bypassButton->getToggleState());
            CHECK(bypassButton->getButtonText() == "Off");
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*bypassButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *bypassButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        const auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].isBypassed);
        CHECK(bypassButton->getToggleState());
        CHECK(bypassButton->getButtonText() == "On");
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 1);
    }

    SECTION("remove")
    {
        ButtonClickCapture clicks(*removeButton);
        const auto checkUnchanged = [&]
        {
            CHECK(manager.getModulationRoutingsCopy().size() == 1);
            CHECK(deleteCount == 0);
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*removeButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *removeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(manager.getModulationRoutingsCopy().size() == 1);
        CHECK(deleteCount == 1);
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 0);
    }

    SECTION("add")
    {
        ButtonClickCapture clicks(*addButton);
        const auto checkUnchanged = [&]
        {
            CHECK(manager.getModulationRoutingsCopy().size() == 1);
            CHECK_FALSE(panel.isUiRebuildPending());
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*addButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *addButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(manager.getModulationRoutingsCopy().size() == 2);
        CHECK(panel.isUiRebuildPending());
        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 1);
    }

    SECTION("close")
    {
        ButtonClickCapture clicks(*closeButton);
        const auto checkUnchanged = [&]
        {
            CHECK(clicks.getClickCount() == 0);
            CHECK(host.notificationCount == 0);
        };

        exerciseRejectedGestures(*closeButton, checkUnchanged);
        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK(host.notificationCount == 0);
    }

    SECTION("a new primary down replaces stale rejected ownership")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier });

        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }

    SECTION("a new primary down safely replaces stale primary ownership")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        REQUIRE(closeButton->isDown());

        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }

    SECTION("hiding cancels a primary gesture without clicking")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        REQUIRE(closeButton->isDown());

        closeButton->setVisible(false);
        CHECK(clicks.getClickCount() == 0);
        CHECK_FALSE(closeButton->isDown());

        closeButton->setVisible(true);
        endButtonPointerGesture(*closeButton);
        CHECK(clicks.getClickCount() == 0);
        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }

    SECTION("disabling cancels a primary gesture without clicking")
    {
        ButtonClickCapture clicks(*closeButton);
        beginButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        REQUIRE(closeButton->isDown());

        closeButton->setEnabled(false);
        CHECK(clicks.getClickCount() == 0);
        CHECK_FALSE(closeButton->isDown());

        closeButton->setEnabled(true);
        endButtonPointerGesture(*closeButton);
        CHECK(clicks.getClickCount() == 0);
        exerciseButtonPointerGesture(
            *closeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

        CHECK(clicks.getClickCount() == 1);
        CHECK_FALSE(closeButton->isDown());
    }
}

TEST_CASE("Modulation matrix primary buttons preserve non-pointer activation",
          "[ui][modulation-matrix][input][buttons][keyboard]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.5f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    int deleteCount = 0;
    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [&deleteCount](std::uint64_t, ModulationRouting)
        {
            ++deleteCount;
        });
    row.setBounds(0, 0, 760, 40);
    auto* polarityButton = findTextButton(row, "Bi");
    auto* bypassButton = findTextButton(row, "Off");
    auto* removeButton = dynamic_cast<juce::TextButton*>(
        row.findChildWithID("remove_button"));
    REQUIRE(polarityButton != nullptr);
    REQUIRE(bypassButton != nullptr);
    REQUIRE(removeButton != nullptr);

    ModulationMatrixPanel panel(processor);
    panel.setBounds(0, 0, 760, 420);
    auto* addButton = findTextButton(panel, "+ ADD ROUTE");
    auto* closeButton = findTextButton(panel, "Close");
    REQUIRE(addButton != nullptr);
    REQUIRE(closeButton != nullptr);

    SECTION("polarity triggerClick")
    {
        polarityButton->triggerClick();
        auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK_FALSE(routings[0].isBipolar);

        row.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK_FALSE(routings[0].isBipolar);
    }

    SECTION("bypass triggerClick")
    {
        bypassButton->triggerClick();
        auto routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].isBypassed);

        row.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        routings = manager.getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].isBypassed);
    }

    SECTION("remove triggerClick")
    {
        removeButton->triggerClick();
        CHECK(deleteCount == 1);

        row.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(deleteCount == 1);
    }

    SECTION("add triggerClick")
    {
        addButton->triggerClick();
        CHECK(manager.getModulationRoutingsCopy().size() == 2);

        panel.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(manager.getModulationRoutingsCopy().size() == 2);
    }

    SECTION("close Return key")
    {
        ButtonClickCapture clicks(*closeButton);
        CHECK(static_cast<juce::Component&>(*closeButton).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        CHECK(clicks.getClickCount() == 1);
    }

    SECTION("close Space key")
    {
        ButtonClickCapture clicks(*closeButton);
        CHECK(static_cast<juce::Component&>(*closeButton).keyPressed(
            juce::KeyPress { juce::KeyPress::spaceKey }));
        CHECK(clicks.getClickCount() == 1);
    }
}

TEST_CASE("Modulation matrix toggle buttons publish only real model changes",
          "[ui][modulation-matrix][state][button]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        0, targets.front().parameterID, 0.5f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        REQUIRE_FALSE(routings.isEmpty());
        routings.set(0, routing);
    }

    ModulationMatrixRow row(
        processor,
        0,
        routing,
        makeRoutingEditSession(processor),
        [](std::uint64_t, ModulationRouting) {});
    row.setBounds(0, 0, 760, 40);
    auto* polarityButton = findTextButton(row, "Bi");
    auto* bypassButton = findTextButton(row, "Off");
    REQUIRE(polarityButton != nullptr);
    REQUIRE(bypassButton != nullptr);
    NonParameterChangeCapture host(processor);

    SECTION("polarity")
    {
        polarityButton->setState(juce::Button::buttonOver);
        polarityButton->setState(juce::Button::buttonDown);
        polarityButton->setState(juce::Button::buttonOver);
        polarityButton->setState(juce::Button::buttonNormal);

        auto liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK(liveRoutings[0].isBipolar);
        CHECK(host.notificationCount == 0);

        polarityButton->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);

        liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK_FALSE(liveRoutings[0].isBipolar);
        CHECK(polarityButton->getButtonText() == "Uni");
        CHECK(host.notificationCount == 1);
    }

    SECTION("bypass")
    {
        bypassButton->setState(juce::Button::buttonOver);
        bypassButton->setState(juce::Button::buttonDown);
        bypassButton->setState(juce::Button::buttonOver);
        bypassButton->setState(juce::Button::buttonNormal);

        auto liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK_FALSE(liveRoutings[0].isBypassed);
        CHECK(host.notificationCount == 0);

        bypassButton->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);

        liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE_FALSE(liveRoutings.isEmpty());
        CHECK(liveRoutings[0].isBypassed);
        CHECK(bypassButton->getButtonText() == "On");
        CHECK(host.notificationCount == 1);
    }
}

TEST_CASE("Modulation matrix follows externally recalled routings without stale-row writes",
          "[ui][modulation-matrix][state][recall]")
{
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE(targets.size() >= 3);

    const ModulationRouting initialRouting {
        0, targets[0].parameterID, 0.10f, true, false
    };
    const ModulationRouting recalledFirstRouting {
        2, targets[1].parameterID, 0.25f, false, false
    };
    const ModulationRouting recalledSecondRouting {
        3, targets[2].parameterID, -0.45f, true, true
    };

    juce::XmlElement initialPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, initialPreset);
    replacePresetRoutings(initialPreset, { initialRouting });
    state::loadStateFromXml(initialPreset, processor);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);

    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    CHECK(containsComboBoxText(*rows.front(), targets[0].displayText));

    juce::XmlElement recalledPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, recalledPreset);
    replacePresetRoutings(recalledPreset,
                          { recalledFirstRouting, recalledSecondRouting });
    state::loadStateFromXml(recalledPreset, processor);

    // ChangeBroadcaster delivery and the panel rebuild are asynchronous. Until
    // they run, the visible row still represents the previous target and must
    // not use its cached array index to edit the newly recalled first route.
    auto* staleAmountSlider = findAmountSlider(*rows.front());
    REQUIRE(staleAmountSlider != nullptr);
    staleAmountSlider->setValue(0.91, juce::sendNotificationSync);

    auto liveRoutings = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 2);
    CHECK(liveRoutings[0].targetParameterID == recalledFirstRouting.targetParameterID);
    CHECK(liveRoutings[0].depth == Catch::Approx(recalledFirstRouting.depth));

    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    rows.clear();
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 2);

    const auto rowsDisplaying = [&](const juce::String& targetText)
    {
        return static_cast<int>(std::count_if(rows.begin(), rows.end(), [&](auto* row)
                                              { return containsComboBoxText(*row, targetText); }));
    };

    CHECK(rowsDisplaying(targets[1].displayText) == 1);
    CHECK(rowsDisplaying(targets[2].displayText) == 1);
    CHECK(rowsDisplaying(targets[0].displayText) == 0);
}

TEST_CASE("Modulation matrix rejects stale rows after same-target state recall",
          "[ui][modulation-matrix][state][recall][identity][aba][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting initialRouting {
        0, targets.front().parameterID, 0.10f, true, false
    };
    const ModulationRouting recalledRouting {
        2, targets.front().parameterID, -0.45f, true, false
    };

    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(initialRouting);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* staleRow = rows.front();

    juce::XmlElement recalledPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, recalledPreset);
    replacePresetRoutings(recalledPreset, { recalledRouting });
    REQUIRE(state::loadStateFromXml(recalledPreset, processor));
    REQUIRE_FALSE(panel.isUiRebuildPending());
    const auto revisionAfterRecall =
        manager.getModulationRoutingRevision();
    NonParameterChangeCapture host(processor);

    const auto checkRecalledAuthority = [&]
    {
        const auto liveRoutings = manager.getModulationRoutingsCopy();
        REQUIRE(liveRoutings.size() == 1);
        CHECK(liveRoutings[0].sourceLfoIndex
              == recalledRouting.sourceLfoIndex);
        CHECK(liveRoutings[0].targetParameterID
              == recalledRouting.targetParameterID);
        CHECK(liveRoutings[0].depth
              == Catch::Approx(recalledRouting.depth));
        CHECK(liveRoutings[0].isBipolar
              == recalledRouting.isBipolar);
        CHECK(liveRoutings[0].isBypassed
              == recalledRouting.isBypassed);
        CHECK(host.notificationCount == 0);
        CHECK(panel.isUiRebuildPending());
        CHECK(manager.getModulationRoutingRevision()
              == revisionAfterRecall);
    };

    SECTION("amount")
    {
        auto* amountSlider = findAmountSlider(*staleRow);
        REQUIRE(amountSlider != nullptr);
        amountSlider->setValue(0.91, juce::sendNotificationSync);
        checkRecalledAuthority();
    }

    SECTION("polarity")
    {
        auto* polarityButton = findTextButton(*staleRow, "Bi");
        REQUIRE(polarityButton != nullptr);
        exerciseButtonPointerGesture(
            *polarityButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        checkRecalledAuthority();
    }

    SECTION("source")
    {
        std::vector<juce::ComboBox*> comboBoxes;
        collectComboBoxes(*staleRow, comboBoxes);
        const auto sourceMenu = std::find_if(
            comboBoxes.begin(), comboBoxes.end(), [](const auto* comboBox)
            {
                return comboBox->getNumItems() == 4;
            });
        REQUIRE(sourceMenu != comboBoxes.end());
        (*sourceMenu)->setSelectedId(2, juce::sendNotificationSync);
        checkRecalledAuthority();
    }

    SECTION("remove")
    {
        auto* removeButton = dynamic_cast<juce::TextButton*>(
            staleRow->findChildWithID("remove_button"));
        REQUIRE(removeButton != nullptr);
        exerciseButtonPointerGesture(
            *removeButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
        checkRecalledAuthority();
    }
}

TEST_CASE("Modulation matrix invalidates rows after identical state recall",
          "[ui][modulation-matrix][state][recall][identity][aba][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());

    const ModulationRouting routing {
        1, targets.front().parameterID, 0.25f, false, true
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(routing);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 1);
    auto* staleAmountSlider = findAmountSlider(*rows.front());
    REQUIRE(staleAmountSlider != nullptr);

    juce::XmlElement identicalPreset { "WINGSFIRE" };
    state::saveStateToXml(processor, identicalPreset);
    const auto revisionBeforeRecall =
        manager.getModulationRoutingRevision();
    REQUIRE(state::loadStateFromXml(identicalPreset, processor));
    CHECK(manager.getModulationRoutingRevision() != revisionBeforeRecall);
    REQUIRE_FALSE(panel.isUiRebuildPending());
    const auto revisionAfterRecall =
        manager.getModulationRoutingRevision();

    NonParameterChangeCapture host(processor);
    staleAmountSlider->setValue(0.73, juce::sendNotificationSync);

    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 1);
    CHECK(liveRoutings[0].sourceLfoIndex == routing.sourceLfoIndex);
    CHECK(liveRoutings[0].targetParameterID == routing.targetParameterID);
    CHECK(liveRoutings[0].depth == Catch::Approx(routing.depth));
    CHECK(liveRoutings[0].isBipolar == routing.isBipolar);
    CHECK(liveRoutings[0].isBypassed == routing.isBypassed);
    CHECK(host.notificationCount == 0);
    CHECK(panel.isUiRebuildPending());
    CHECK(manager.getModulationRoutingRevision() == revisionAfterRecall);
}

TEST_CASE("Modulation matrix keeps its edit session current after local edits",
          "[ui][modulation-matrix][state][identity][session][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE(targets.size() >= 2);

    const ModulationRouting firstRouting {
        0, targets[0].parameterID, 0.10f, true, false
    };
    const ModulationRouting secondRouting {
        1, targets[1].parameterID, -0.10f, true, false
    };
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        routings.clear();
        routings.add(firstRouting);
        routings.add(secondRouting);
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);
    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() == 2);
    auto* firstAmount = findAmountSlider(*rows[0]);
    auto* secondAmount = findAmountSlider(*rows[1]);
    REQUIRE(firstAmount != nullptr);
    REQUIRE(secondAmount != nullptr);
    NonParameterChangeCapture host(processor);
    const auto initialRevision = manager.getModulationRoutingRevision();

    firstAmount->setValue(0.20, juce::sendNotificationSync);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 1);
    CHECK(host.notificationCount == 1);
    CHECK_FALSE(panel.isUiRebuildPending());

    firstAmount->setValue(0.30, juce::sendNotificationSync);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 2);
    CHECK(host.notificationCount == 2);
    CHECK_FALSE(panel.isUiRebuildPending());

    secondAmount->setValue(-0.40, juce::sendNotificationSync);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 3);
    CHECK(host.notificationCount == 3);
    CHECK_FALSE(panel.isUiRebuildPending());

    auto* firstPolarity = findTextButton(*rows[0], "Bi");
    REQUIRE(firstPolarity != nullptr);
    exerciseButtonPointerGesture(
        *firstPolarity,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 4);
    CHECK(host.notificationCount == 4);
    CHECK_FALSE(panel.isUiRebuildPending());

    auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 2);
    CHECK(liveRoutings[0].depth == Catch::Approx(0.30f));
    CHECK_FALSE(liveRoutings[0].isBipolar);
    CHECK(liveRoutings[1].depth == Catch::Approx(-0.40f));

    auto* removeButton = dynamic_cast<juce::TextButton*>(
        rows[0]->findChildWithID("remove_button"));
    REQUIRE(removeButton != nullptr);
    exerciseButtonPointerGesture(
        *removeButton,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier });

    liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() == 1);
    CHECK(liveRoutings[0].targetParameterID
          == secondRouting.targetParameterID);
    CHECK(manager.getModulationRoutingRevision() == initialRevision + 5);
    CHECK(host.notificationCount == 5);
    CHECK(panel.isUiRebuildPending());
}

TEST_CASE("Modulation matrix ignores empty rows while a structural rebuild is pending",
          "[ui][modulation-matrix][state][identity]")
{
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        auto& routings = manager.getModulationRoutings();
        REQUIRE(routings.size() >= 3);
        routings.getReference(0).depth = 0.10f;
        routings.getReference(1).depth = 0.20f;
        routings.getReference(2).depth = 0.30f;
    }

    ModulationMatrixPanel panel { processor };
    panel.setBounds(0, 0, 760, 420);

    std::vector<ModulationMatrixRow*> rows;
    collectMatrixRows(panel, rows);
    REQUIRE(rows.size() >= 3);
    auto* staleSecondAmount = findAmountSlider(*rows[1]);
    REQUIRE(staleSecondAmount != nullptr);
    NonParameterChangeCapture host(processor);

    // Removing the first empty slot shifts every later empty row while their
    // target IDs remain indistinguishable. The revision must make the old row
    // reject the shifted slot without relying on a pre-existing rebuild flag.
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        manager.getModulationRoutings().remove(0);
        manager.advanceModulationRoutingRevisionLocked();
    }
    REQUIRE_FALSE(panel.isUiRebuildPending());
    staleSecondAmount->setValue(0.91, juce::sendNotificationSync);

    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() >= 2);
    CHECK(liveRoutings[1].depth == Catch::Approx(0.30f));
    CHECK(host.notificationCount == 0);
    CHECK(panel.isUiRebuildPending());

    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    rows.clear();
    collectMatrixRows(panel, rows);
    CHECK(rows.size() == static_cast<size_t>(liveRoutings.size()));
}
