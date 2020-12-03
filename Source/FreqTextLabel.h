/*
  ==============================================================================

    FreqTextLabel.h
    Created: 2 Dec 2020 7:53:08pm
    Author:  俞轶风

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "VerticalLine.h"

//==============================================================================
/*
*/
class FreqTextLabel  : public juce::Component
{
public:
    FreqTextLabel(VerticalLine &v);
    ~FreqTextLabel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setFreq(int freq);
    
private:
    VerticalLine &verticalLine;
    int mFrequency;
    bool mState;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqTextLabel)
};
