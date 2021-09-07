/*
  ==============================================================================

    VUMeter.h
    Created: 25 Jan 2021 2:55:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

//==============================================================================
/*
*/
class VUMeter  : public juce::Component, juce::Timer
{
public:
    VUMeter(FireAudioProcessor* inProcessor);
    ~VUMeter() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setParameterId(int inParameterId);
    void timerCallback() override;
private:
    FireAudioProcessor* mProcessor;
    int mParameterId;
    float mCh0Level;
    float mCh1Level;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMeter)
};
