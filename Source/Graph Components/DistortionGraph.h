/*
  ==============================================================================

    DistortionGraph.h
    Created: 29 Nov 2020 10:31:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../DSP/Distortion.h"
#include "../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class DistortionGraph  : public juce::Component
{
public:
    DistortionGraph();
    ~DistortionGraph() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setState(int mode, float color, float rec, float mix, float bias, float drive, float rateDivide);
    
private:
    int mode;
    //float inputGain = *processor.treeState.getRawParameterValue("inputGain");
    //float drive = *processor.treeState.getRawParameterValue("drive");
    float color;
    float rec;
    float mix;
    float bias;
    float rateDivide;
    float drive;
    Distortion distortionProcessor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionGraph)
};
