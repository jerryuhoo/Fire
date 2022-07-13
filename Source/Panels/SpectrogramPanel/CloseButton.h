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
#include "../../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class CloseButton : public juce::ToggleButton//juce::Component
{
public:
    CloseButton(VerticalLine &v);
    ~CloseButton() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VerticalLine &verticalLine;
    bool isEntered = false;
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloseButton)
};
