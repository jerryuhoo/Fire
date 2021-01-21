/*
  ==============================================================================

    EnableButton.h
    Created: 3 Dec 2020 8:18:45pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class EnableButton  : public juce::Component
{
public:
    EnableButton();
    ~EnableButton() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    bool getState();
    void setState(bool state);
private:
    bool isEntered = false;
    bool mState = true;
    juce::Colour getColour();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnableButton)
};
