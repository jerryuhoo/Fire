/*
  ==============================================================================

    ValuePopup.h
    Created: 4 Oct 2025 7:36:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "InterfaceDefines.h"
#include "juce_gui_basics/juce_gui_basics.h"


// A simple component to display a floating label, typically for slider values.
class ValuePopup : public juce::Component
{
public:
    ValuePopup();
    ~ValuePopup() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Sets the text to be displayed in the popup.
    void setText(const juce::String& text);

private:
    juce::Label valueLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValuePopup)
};