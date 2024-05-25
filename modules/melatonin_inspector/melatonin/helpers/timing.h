#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace melatonin
{
    class ComponentTimer
    {
    public:
        explicit ComponentTimer (juce::Component* c) : component (c)
        {
            startTimeTicks = juce::Time::getHighResolutionTicks();
        }

        ~ComponentTimer()
        {
            static double scalar = 1.0 / static_cast<double> (juce::Time::getHighResolutionTicksPerSecond());
            result = static_cast<double> (juce::Time::getHighResolutionTicks() - startTimeTicks) * scalar;

            auto& props = component->getProperties();

            // if this new time is slower, make it the max
            if (result > (double) props.getWithDefault ("timing1", 0.0))
                props.set ("timingMax", result);

            // push the 1st and 2nd timings down
            props.set ("timing3", props.getWithDefault ("timing2", 0.0));
            props.set ("timing2", props.getWithDefault ("timing1", 0.0));
            props.set ("timing1", result);
        }

    private:
        juce::Component* component;
        juce::int64 startTimeTicks;
        double result = 0;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComponentTimer)
    };

}
