/*
  ==============================================================================

    EnableButton.h
    Created: 3 Dec 2020 8:18:45pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../GUI/FireTheme.h"

//==============================================================================
/*
 */
class EnableButton : public juce::ToggleButton
{
public:
    EnableButton();
    ~EnableButton() override;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void dismissPointerGesture() noexcept;
    
private:
    void visibilityChanged() override;
    void enablementChanged() override;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;

    bool isEntered = false;
    bool primaryPointerDown = false;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    juce::Colour getColour();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnableButton)
};
