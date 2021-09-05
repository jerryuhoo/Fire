/*
  ==============================================================================

    FreqDividerGroup.h
    Created: 4 Sep 2021 1:14:37pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FreqTextLabel.h"
#include "CloseButton.h"
#include "VerticalLine.h"
//==============================================================================
/*
*/
class FreqDividerGroup  : public juce::Component
{
public:
    FreqDividerGroup();
    ~FreqDividerGroup() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    VerticalLine verticalLine;
    
    int getFrequency();
    void setFrequency(int frequency);
    
    void moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[], int sortedIndex[]);
    
private:
    float margin = 7.5f;
    float size = 15.0f;
    float width = 5.0f;
    int mFrequency = -1;
    CloseButton closeButton;
    FreqTextLabel freqTextLabel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqDividerGroup)
};