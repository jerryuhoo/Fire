/*
  ==============================================================================

    CloseButton.h
    Created: 8 Nov 2020 7:57:32pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "VerticalLine.h"
#include "../LookAndFeel.h"

//==============================================================================
/*
*/
class CloseButton : public juce::Component
{
public:
    CloseButton(VerticalLine &v);
    ~CloseButton() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
private:
    VerticalLine &verticalLine;
    bool isEntered = false;
    
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;

//    bool mState = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloseButton)
};
