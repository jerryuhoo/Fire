#include "../Source/Panels/ControlPanel/ModulationMatrixPanel.h"
#include "../Source/Panels/TopPanel/Preset.h"
#include "../Source/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <initializer_list>
#include <vector>

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
} // namespace

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

    ModulationMatrixRow row(processor, 0, routing, [] {});
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

    amountSlider->removeListener(&sliderCapture);
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

    ModulationMatrixRow row(processor, 0, routing, [] {});
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

    // Removing the first empty slot shifts every later empty row while their
    // target IDs remain indistinguishable. Once a rebuild is pending, the old
    // row objects must stop writing by cached index.
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        manager.getModulationRoutings().remove(0);
    }
    panel.requestUiRebuild();
    staleSecondAmount->setValue(0.91, juce::sendNotificationSync);

    const auto liveRoutings = manager.getModulationRoutingsCopy();
    REQUIRE(liveRoutings.size() >= 2);
    CHECK(liveRoutings[1].depth == Catch::Approx(0.30f));

    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    rows.clear();
    collectMatrixRows(panel, rows);
    CHECK(rows.size() == static_cast<size_t>(liveRoutings.size()));
}
