/*
  ==============================================================================

    FilterControl.h
    Created: 18 Sep 2021 9:54:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../Utility/AudioHelpers.h"
//==============================================================================
/*
*/
class FilterControl  : public juce::Component, juce::AudioProcessorParameter::Listener, juce::Timer
{
public:
    FilterControl(FireAudioProcessor &);
    ~FilterControl() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setParams(float lowCut,
                   float highCut,
                   float cutRes,
                   float peak,
                   float peakRes);
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { }
    void timerCallback() override;
private:
    FireAudioProcessor &processor;
    float mLowCut;
    float mHighCut;
    float mCutRes;
    float mPeak;
    float mPeakRes;
    juce::Path responseCurve;
    
    using Filter = juce::dsp::IIR::Filter<float>;
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
    MonoChain monoChain;
    
    juce::Atomic<bool> parametersChanged {false};
    void updateResponseCurve();
    void updateChain();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterControl)
};
