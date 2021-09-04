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
#include "../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class FreqTextLabel  : public juce::Label
{
public:
    FreqTextLabel(VerticalLine &v);
    ~FreqTextLabel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setFreq(int freq);
    int getFreq();
    void setScale(float scale);
    
private:
    VerticalLine &verticalLine;
    int mFrequency;
    bool mState;
    float mScale = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqTextLabel)
};
