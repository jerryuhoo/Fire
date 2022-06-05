/*
  ==============================================================================

    FreqTextLabel.h
    Created: 2 Dec 2020 7:53:08pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "VerticalLine.h"
#include "../../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class FreqTextLabel  : public juce::Component, juce::Timer
{
public:
    FreqTextLabel(VerticalLine &v);
    ~FreqTextLabel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setFreq(int freq);
    int getFreq();
    void setScale(float scale);
    bool isMouseOverCustom();
    void timerCallback() override;
    void setFade(bool update, bool isFadeIn);
    
private:
    VerticalLine &verticalLine;
    int mFrequency;
    float mScale = 1.0f;
    bool mUpdate = false;
    bool mFadeIn = true;
    int currentStep = 0;
    const int maxStep = 5;
    
    juce::Label freqLabel;
    FlatButtonLnf flatButtonLnf;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqTextLabel)
};
