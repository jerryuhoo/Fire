#pragma once
#include "../helpers/colors.h"
#include "../helpers/inspector_settings.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include <utility>

namespace melatonin
{
    // disclosure triangle component with toggle button to show/hide piece of content
    class CollapsablePanel : public juce::Component
    {
    public:
        explicit CollapsablePanel (juce::String n, juce::Component* c, bool d = false) : name (std::move (n)), drawTopDivider (d), content (c)
        {
            toggleButton.setLookAndFeel (&toggleButtonLookAndFeel);
            addAndMakeVisible (toggleButton);
            addChildComponent (content);
            toggleButton.setButtonText (name);
            toggleButton.onClick = [this] {
                toggle (toggleButton.getToggleState());
            };
        }
        void paint (juce::Graphics& g) override
        {
            if (drawTopDivider)
            {
                g.setColour (colors::panelLineSeparator);
                g.drawHorizontalLine (0, 0, (float) getWidth());
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();
            if (drawTopDivider)
                area.removeFromTop (1); // pixel perfect, please

            toggleButton.setBounds (area.reduced (8, 2).removeFromLeft (200));
        }

        // when the inspector as a whole is toggled, recall our content's visibility
        void visibilityChanged() override
        {
            if (isVisible())
                toggle (settings->props->getBoolValue (name, true));
        }

        // called when panel is toggled or overall inspector is toggled
        void toggle (bool enabled)
        {
            settings->props->setValue (name, enabled);
            content->setVisible (enabled);
            toggleButton.setToggleState (enabled, juce::dontSendNotification);
            if (getParentComponent())
                getParentComponent()->resized();
        }

    private:
        struct ToggleButtonLnF : juce::LookAndFeel_V4
        {
            ToggleButtonLnF()
            {
                setColour (juce::ToggleButton::textColourId, colors::disclosure);
                setColour (juce::ToggleButton::tickColourId, colors::disclosure);
            }

            void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
            {
                auto font = juce::Font ("Verdana", 14.5, juce::Font::FontStyleFlags::plain).withExtraKerningFactor (0.1f);
                auto tickWidth = font.getHeight();

                drawTickBox (g, button, 6.0f, ((float) button.getHeight() - tickWidth) * 0.5f, tickWidth, tickWidth, button.getToggleState(), button.isEnabled(), shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

                g.setColour (button.findColour (juce::ToggleButton::textColourId));
                g.setFont (font);

                if (!button.isEnabled())
                    g.setOpacity (0.5f);

                g.drawText (button.getButtonText(),
                    button.getLocalBounds().withTrimmedLeft (juce::roundToInt (tickWidth) + 12).withTrimmedRight (2),
                    juce::Justification::centredLeft);
            }

            void drawTickBox (juce::Graphics& g, juce::Component& /*component*/, float x, float y, float w, float h, const bool ticked, const bool isEnabled, const bool /*shouldDrawButtonAsHighlighted*/, const bool /*shouldDrawButtonAsDown*/) override
            {
                juce::Rectangle<float> tickBounds (x, y, w, h);

                auto boxSize = tickBounds.getHeight() - 5;

                juce::Path p;
                p.addTriangle (tickBounds.getX(), tickBounds.getY(), tickBounds.getX() + boxSize + 2, tickBounds.getY(), tickBounds.getX() + boxSize * 0.5f + 1, tickBounds.getY() + boxSize);

                auto tickColour = findColour (juce::ToggleButton::tickColourId);
                g.setColour (isEnabled ? tickColour : tickColour.darker());

                auto transform = juce::AffineTransform::rotation (!ticked ? juce::degreesToRadians (270.0f) : 0,
                    tickBounds.getCentreX(),
                    tickBounds.getCentreY());

                if (!ticked)
                    transform = transform.translated (0, -boxSize * 0.25f + 1);
                else
                    transform = transform.translated (-1, 3);

                g.fillPath (p, transform);
            }
        };
        ToggleButtonLnF toggleButtonLookAndFeel;

        juce::ToggleButton toggleButton;
        juce::String name;
        bool drawTopDivider;
        Component::SafePointer<Component> content;
        juce::SharedResourcePointer<InspectorSettings> settings;
    };
}
