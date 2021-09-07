/*
  ==============================================================================

    Oscilloscope.h
    Created: 25 Oct 2020 7:26:35pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

//==============================================================================
/*
*/
class Oscilloscope : public juce::Component//, private juce::Timer
{
public:
    Oscilloscope(FireAudioProcessor &);
    ~Oscilloscope() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setScale(float scale);
    
private:
    FireAudioProcessor &processor;
    
    juce::Array<float> historyL;
    juce::Array<float> historyR;
    juce::Image historyImage;
   
    float scale = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Oscilloscope)
};
