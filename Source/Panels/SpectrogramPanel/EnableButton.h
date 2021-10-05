/*
  ==============================================================================

    EnableButton.h
    Created: 3 Dec 2020 8:18:45pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class EnableButton  : public juce::ToggleButton
{
public:
    EnableButton();
    ~EnableButton() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    
private:
    bool isEntered = false;
    juce::Colour getColour();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnableButton)
};
