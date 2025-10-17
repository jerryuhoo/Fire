/*
  ==============================================================================

    DraggableButton.h
    Created: 7 Oct 2021 11:42:21pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

//==============================================================================
/*
*/
class DraggableButton : public juce::Component
{
public:
    DraggableButton();
    ~DraggableButton() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void setState(const bool state);
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    std::function<void(float)> onQValueChanged;

private:
    bool isEntered = false;
    juce::Colour getColour();
    bool mState = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DraggableButton)
};
