#pragma once
#include "../helpers/colors.h"
namespace melatonin
{
    //==============================================================================*/
    class ColourPropertyComponent : public juce::PropertyComponent,
                                    private juce::Value::Listener
    {
    public:
        ColourPropertyComponent (const juce::Value& valueToControl, const juce::String& propertyName, bool displayRGBA = true, bool showAlpha = false)
            : juce::PropertyComponent (propertyName), value (valueToControl), container (value, displayRGBA, showAlpha)
        {
            addAndMakeVisible (container);
            value.addListener (this);
        }

        juce::Value& getValueObject()
        {
            return value;
        }

        void refresh() override
        {
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            PropertyComponent::paint (g);

            g.setColour (findColour (juce::BooleanPropertyComponent::backgroundColourId));
            g.fillRect (container.getBounds());

            g.setColour (findColour (juce::BooleanPropertyComponent::outlineColourId));
            g.drawRect (container.getBounds());
        }

    private:
        juce::Value value;
        void valueChanged (juce::Value&) override
        {
            refresh();
        }

        class ColorSelector : public juce::ColourSelector, private juce::ChangeListener
        {
        public:
            explicit ColorSelector (int selectorFlags = (showAlphaChannel | showColourAtTop | showSliders | showColourspace),
                int _edgeGap = 4,
                int _gapAroundColourSpaceComponent = 7)
                : juce::ColourSelector (selectorFlags, _edgeGap, _gapAroundColourSpaceComponent)
            {
                addChangeListener (this);
            }

            ~ColorSelector() override
            {
                if (onDismiss)
                    onDismiss();
            }

            void changeListenerCallback (juce::ChangeBroadcaster*) override
            {
                if (onChange)
                    onChange();
            }

            std::function<void()> onDismiss;
            std::function<void()> onChange;
        };

        class Container : public Component
        {
        public:
            Container (juce::Value& value_, bool rgba, bool a)
                : value (value_), displayRGBA (rgba), alpha (a)
            {
            }

            void paint (juce::Graphics& g) override
            {
                auto c = juce::Colour ((uint32_t) int (value.getValue()));

                auto area = getLocalBounds();

                g.setColour (c);
                auto colorRect = area.removeFromLeft (20).withSizeKeepingCentre (18, 18).toFloat();
                g.fillRoundedRectangle (colorRect, 1.f);

                // help out the dark blue/purple/blacks
                if (!colors::areContrasting (c, colors::panelBackgroundLighter, 0.1f))
                {
                    g.setColour (juce::Colours::grey.withAlpha (0.3f));
                    g.drawRoundedRectangle (colorRect, 1.f, 1.f);
                }

                area.removeFromLeft (8);
                g.setColour (colors::propertyValue);
                g.drawText (displayRGBA ? colors::rgbaString (c) : colors::hexString (c), area.withTrimmedBottom (1), juce::Justification::centredLeft);
            }

            void mouseUp (const juce::MouseEvent& e) override
            {
                if (e.mouseWasClicked())
                {
                    auto selectorFlags = juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace;
                    if (alpha)
                        selectorFlags |= juce::ColourSelector::showAlphaChannel;

                    auto colourSelector = std::make_unique<ColorSelector> (selectorFlags);

                    colourSelector->setLookAndFeel (&getLookAndFeel());
                    colourSelector->setSize (300, 300);
                    colourSelector->setCurrentColour (juce::Colour ((uint32_t) int (value.getValue())), juce::dontSendNotification);
                    colourSelector->onDismiss = [this, parentRef = juce::WeakReference<juce::Component> (this), cs = colourSelector.get()]() {
                        if (! parentRef.wasObjectDeleted())
                        {
                            value = (int) cs->getCurrentColour().getARGB();
                            repaint();
                        }
                    };
                    colourSelector->onChange = [this, cs = colourSelector.get()]() {
                        value = (int) cs->getCurrentColour().getARGB();
                        repaint();
                    };

                    auto& callout = juce::CallOutBox::launchAsynchronously (std::move (colourSelector), getScreenBounds().removeFromLeft (20), nullptr);
                    callout.setLookAndFeel (&getLookAndFeel());
                }
            }

            juce::Value& value;
            bool displayRGBA;
            bool alpha;
        };

        Container container;
    };
}
