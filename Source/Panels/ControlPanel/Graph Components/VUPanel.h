/*
  ==============================================================================

    VUPanel.h
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../../PluginProcessor.h"
#include "VUMeter.h"
#include "GraphTemplate.h"

//==============================================================================
/*
*/
class VUPanel  : public GraphTemplate, juce::Timer
{
public:
    VUPanel(FireAudioProcessor &);
    ~VUPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setFocusBandNum(int num);
    void timerCallback() override;
    
private:
    FireAudioProcessor &processor;
    int focusBandNum;
    const float VU_METER_RANGE = 96.0f;
    VUMeter vuMeterIn;
    VUMeter vuMeterOut;
    // for smoothing meter values
    int updateCounter = 0;
    float displayInputValue = 0.0f;
    float displayOutputValue = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUPanel)
};
