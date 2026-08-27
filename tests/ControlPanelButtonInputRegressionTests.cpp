#include <GUI/PrimaryButton.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>

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
