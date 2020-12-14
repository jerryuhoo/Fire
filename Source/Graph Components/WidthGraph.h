/*
  ==============================================================================

    WidthGraph.h
    Created: 14 Dec 2020 3:40:40pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class WidthGraph  : public juce::Component
{
public:
    WidthGraph();
    ~WidthGraph() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WidthGraph)
};
