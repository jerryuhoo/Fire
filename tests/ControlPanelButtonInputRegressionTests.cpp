#include <GUI/PrimaryButton.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
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
