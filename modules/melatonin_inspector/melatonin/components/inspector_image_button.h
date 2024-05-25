#pragma once
#include "../helpers/misc.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace melatonin
{
    class InspectorImageButton : public juce::Component
    {
    public:
        bool on = false;
        std::function<void()> onClick;
        std::function<void()> onDoubleClick;

        explicit InspectorImageButton (const juce::String& filename, juce::Point<int> o = { 0, 0 }, bool toggleable = false) : offset (o)
        {
            setInterceptsMouseClicks (true, false);
            setName (filename);
            if (!toggleable)
                imageOn = getIcon (filename);
            else
            {
                // this is by convention
                // the filenames should look like: icon-on.png / icon-off.png
                imageOn = getIcon (filename + "on");
                imageOff = getIcon (filename + "off");
            }
        }

        void paint (juce::Graphics& g) override
        {
            // Assumes exported at 2x
            if (on || imageOff.isNull())
                g.drawImageTransformed (imageOn, juce::AffineTransform::scale (0.5f).translated (offset).translated ((float) offset.getX(), (float) offset.getY()));
            else
                g.drawImageTransformed (imageOff, juce::AffineTransform::scale (0.5f).translated (offset).translated ((float) offset.getX(), (float) offset.getY()));
        }

        void mouseDown (const juce::MouseEvent& /*event*/) override
        {
            on = !on;
            if (onClick != nullptr)
                onClick();
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& /*event*/) override
        {
            if (onDoubleClick != nullptr)
                onDoubleClick();
        }

    private:
        juce::Image imageOn;
        juce::Image imageOff;
        juce::Point<int> offset;
    };
}
