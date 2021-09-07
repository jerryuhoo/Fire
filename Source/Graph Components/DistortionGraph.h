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
    void setScale(float scale);
    
private:
    int mode = -1;
    float color = -1.0f;
    float rec = -1.0f;
    float mix = -1.0f;
    float bias = -1.0f;
    float rateDivide = -1.0f;
    float drive = -1.0f;
    Distortion distortionProcessor;
    float scale = 1.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionGraph)
};
