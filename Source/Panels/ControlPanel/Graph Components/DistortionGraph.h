/*
  ==============================================================================

    DistortionGraph.h
    Created: 29 Nov 2020 10:31:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../../PluginProcessor.h"
#include "../../../GUI/LookAndFeel.h"
#include "../../../DSP/ClippingFunctions.h"
#include "GraphTemplate.h"

//==============================================================================
/*
*/
class DistortionGraph  : public GraphTemplate, juce::AudioProcessorParameter::Listener, juce::Timer
{
public:
    DistortionGraph(FireAudioProcessor &);
    ~DistortionGraph() override;

    void paint (juce::Graphics&) override;
    void setState(int mode, float rec, float mix, float bias, float drive, float rateDivide);

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { }
    void timerCallback() override;
private:
    FireAudioProcessor &processor;
    int mode = -1;
    float color = -1.0f;
    float rec = -1.0f;
    float mix = -1.0f;
    float bias = -1.0f;
    float rateDivide = -1.0f;
    float drive = -1.0f;
    float scale = 1.0f;
    juce::Atomic<bool> parametersChanged {false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionGraph)
};
