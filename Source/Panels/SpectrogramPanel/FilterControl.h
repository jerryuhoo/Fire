/*
  ==============================================================================
 
    FilterControl.h
    Created: 18 Sep 2021 9:54:04pm
    Author:  羽翼深蓝Wings
 
  ==============================================================================
*/

#pragma once

#include "../../PluginProcessor.h"
#include "../../Utility/AudioHelpers.h"
#include "../ControlPanel/GlobalPanel.h"
#include "DraggableButton.h"
//==============================================================================
/*
 */
class FilterControl : public juce::Component, juce::AudioProcessorParameter::Listener, juce::Timer
{
public:
    FilterControl(FireAudioProcessor&, GlobalPanel&);
    ~FilterControl() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    //    void setParams(float lowCut,
    //                   float highCut,
    //                   float cutRes,
    //                   float peak,
    //                   float peakRes);
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

private:
    FireAudioProcessor& processor;
    GlobalPanel& globalPanel;
    //    float mLowCut;
    //    float mHighCut;
    //    float mCutRes;
    //    float mPeak;
    //    float mPeakRes;
    juce::Path responseCurve;

    using Filter = juce::dsp::IIR::Filter<float>;
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter, Filter, Filter>;
    // lowcut, peak, highcut, lowcut Q, highcut Q
    MonoChain monoChain;

    void updateResponseCurve();
    void updateChain();
    void setDraggableButtonBounds();
    juce::Atomic<bool> parametersChanged { false };
    DraggableButton draggableLowButton, draggablePeakButton, draggableHighButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterControl)
};
