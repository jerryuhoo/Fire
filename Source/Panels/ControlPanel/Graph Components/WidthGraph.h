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

//==============================================================================
/*
*/
class WidthGraph  : public juce::Component
{
public:
    WidthGraph(FireAudioProcessor &);
    ~WidthGraph() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool getZoomState();
    void setZoomState(bool zoomState);
    void mouseDown(const juce::MouseEvent &e) override;
    
private:
    FireAudioProcessor &processor;
    juce::Array<float> historyL;
    juce::Array<float> historyR;
    bool mZoomState = false; // false means small
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WidthGraph)
};
