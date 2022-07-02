/*
  ==============================================================================

    WidthGraph.h
    Created: 14 Dec 2020 3:40:40pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../../PluginProcessor.h"
#include "GraphTemplate.h"

//==============================================================================
/*
*/
class WidthGraph  : public GraphTemplate, juce::Timer
{
public:
    WidthGraph(FireAudioProcessor &);
    ~WidthGraph() override;

    void paint (juce::Graphics&) override;
    void timerCallback() override;
    void resized() override;
    
private:
    FireAudioProcessor &processor;
    juce::Array<float> historyL;
    juce::Array<float> historyR;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WidthGraph)
};
