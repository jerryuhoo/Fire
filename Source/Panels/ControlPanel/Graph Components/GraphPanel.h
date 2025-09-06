/*
  ==============================================================================

    GraphPanel.h
    Created: 30 Aug 2021 11:52:16pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../PluginProcessor.h"
#include "DistortionGraph.h"
#include "Oscilloscope.h"
#include "VUPanel.h"
#include "WidthGraph.h"

//==============================================================================
/*
*/
class GraphPanel : public juce::Component
{
public:
    GraphPanel(FireAudioProcessor&);
    ~GraphPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setFocusBandNum(int num);

    Oscilloscope* getOscilloscope();
    VUPanel* getVuPanel();
    DistortionGraph* getDistortionGraph();
    WidthGraph* getWidthGraph();

private:
    FireAudioProcessor& processor;

    // Oscilloscope
    Oscilloscope oscilloscope { processor };

    // VU Meters
    VUPanel vuPanel { processor };

    // Distortion Graph
    DistortionGraph distortionGraph { processor };

    // Width Graph
    WidthGraph widthGraph { processor };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphPanel)
};
