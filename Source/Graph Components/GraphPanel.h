/*
  ==============================================================================

    GraphPanel.h
    Created: 30 Aug 2021 11:52:16pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "VUPanel.h"
#include "DistortionGraph.h"
#include "Oscilloscope.h"
#include "WidthGraph.h"

//==============================================================================
/*
*/
class GraphPanel  : public juce::Component
{
public:
    GraphPanel(FireAudioProcessor &);
    ~GraphPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setDistortionState(int mode, float color, float rec, float mix,
                            float bias, float drive, float rateDivide);
    
private:
    FireAudioProcessor &processor;
    
    // Oscilloscope
    Oscilloscope oscilloscope {processor};
    
    // VU Meters
    VUPanel vuPanel {processor};
    
    // Distortion Graph
    DistortionGraph distortionGraph;
    
    // Width Graph
    WidthGraph widthGraph {processor};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphPanel)
};
