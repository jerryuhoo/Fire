/*
  ==============================================================================

    FilterControl.h
    Created: 18 Sep 2021 9:54:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class FilterControl  : public juce::Component
{
public:
    FilterControl();
    ~FilterControl() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setParams(float lowCut,
                   float highCut,
                   float cutRes,
                   float peak,
                   float peakRes);
    
private:
    float mLowCut;
    float mHighCut;
    float mCutRes;
    float mPeak;
    float mPeakRes;
    juce::Path responseCurve;
    //juce::MonoChain monoChain;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterControl)
};
