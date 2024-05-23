/*
  ==============================================================================

    VUMeter.h
    Created: 25 Jan 2021 2:55:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
/*
*/
class VUMeter : public juce::Component, juce::Timer
{
public:
    VUMeter (FireAudioProcessor* inProcessor);
    ~VUMeter() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setParameters (bool isInput, juce::String meterName);
    void timerCallback() override;
    float getLeftChannelLevel();
    float getRightChannelLevel();

private:
    FireAudioProcessor* mProcessor;
    bool mIsInput;
    juce::String mMeterName;
    float mCh0Level;
    float mCh1Level;
    float mMaxCh0Level;
    float mMaxCh1Level;
    int mMaxValueDecayCounter;
    const int MAX_VALUE_HOLD_FRAMES = 60;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMeter)
};
