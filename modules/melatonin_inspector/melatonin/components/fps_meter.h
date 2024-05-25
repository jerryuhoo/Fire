#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
#include "melatonin_inspector/melatonin/helpers/colors.h"

// VBlank was added in 7.0.3
#if (JUCE_MAJOR_VERSION >= 7) && (JUCE_MINOR_VERSION >= 1 || JUCE_BUILDNUMBER >= 3)
    #define MELATONIN_VBLANK 1
#else
    #define MELATONIN_VBLANK 0
#endif

namespace melatonin
{
    class FPSMeter : public juce::Component, private juce::Timer
    {
    public:
        FPSMeter ()
        {
            // don't repaint the parent
            // unfortunately, on macOS, this no longer works
            // See FAQ in README for more info
            setOpaque (true);

            setInterceptsMouseClicks (false, false);
        }

        void setRoot (juce::Component& o)
        {
            overlay = &o;

            overlay->addChildComponent (this);
        }

        void clearRoot()
        {
            if (overlay)
                overlay->removeChildComponent (this);
        }

        void timerCallback() override
        {
            TRACE_EVENT ("component", "fps timer callback");
            repaint();
        }

        void visibilityChanged() override
        {
            frameTime = 0;
            lastTime = 0;
            fps = 0;
            if (isVisible())
            {
#if MELATONIN_VBLANK
                // right before every paint, call repaint
                // syncs to ensure each paint call is preceded by a recalculation
                vBlankCallback = { this,
                    [this] {
                        TRACE_EVENT ("component", "fps vBlankCallback");
                        this->repaint();
                    } };
#else
                // avoid as much aliasing with display refresh times as possible
                // TODO: investigate optimal value
                startTimerHz (120);
#endif
            }
            else
            {
#if MELATONIN_VBLANK
                vBlankCallback = {};
#else
                stopTimer();
#endif
            }
        }

        void paint (juce::Graphics& g) override
        {
            TRACE_COMPONENT();

            update();

            // tried to go for pixel font but didn't work :/
            g.setImageResamplingQuality (juce::Graphics::ResamplingQuality::lowResamplingQuality);
            g.setColour (colors::black);
            g.fillRect (getLocalBounds());
            g.setColour (juce::Colours::green);
            g.setFont (font);
            g.drawText (juce::String (juce::String (fps) + " FPS"), getLocalBounds(), juce::Justification::centred, true);
        }

        void update()
        {
            TRACE_COMPONENT();

            auto now = juce::Time::getMillisecondCounterHiRes();
            auto elapsed = now - lastTime;

            if (juce::approximatelyEqual (lastTime, 0.0))
            {
                lastTime = now;
                return;
            }

            if (juce::approximatelyEqual (frameTime, 0.0))
            {
                // start without any smoothing
                frameTime = elapsed;
            }
            else
            {
                // use a static number of hypothetical 30 fps to smooth the value
                // https://stackoverflow.com/a/87333
                double smoothing = std::pow (0.9f, elapsed * 30 / 1000);
                frameTime = (frameTime * smoothing) + (elapsed * (1.0f - smoothing));
            }
            lastTime = now;
            fps = juce::roundToInt (1000 / frameTime);
        }

    private:
        juce::Component* overlay = nullptr;
        juce::Rectangle<int> bounds;
        juce::Font font = juce::Font (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain);
        double lastTime = juce::Time::getMillisecondCounterHiRes();
        double frameTime = 0;
        int fps = 0;

#if MELATONIN_VBLANK
        juce::VBlankAttachment vBlankCallback;
#endif
    };
}
