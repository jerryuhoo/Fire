/*
  ==============================================================================

    SoloButton.h
    Created: 3 Dec 2020 8:18:19pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class SoloButton  : public juce::Component
{
public:
    SoloButton();
    ~SoloButton() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    
    bool getState();
    
private:
    bool isEntered = false;
    bool mState = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoloButton)
};
