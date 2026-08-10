/*
  ==============================================================================

    ValuePopup.cpp
    Created: 4 Oct 2025 7:36:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ValuePopup.h"

ValuePopup::ValuePopup()
{
    // Style the label for the popup window
    valueLabel.setColour(juce::Label::backgroundColourId, fire::ui::colours::surface0.withAlpha(0.96f));
    valueLabel.setColour(juce::Label::textColourId, fire::ui::colours::whiteHot);
    valueLabel.setBorderSize({ 1, 1, 1, 1 });
    valueLabel.setColour(juce::Label::outlineColourId, fire::ui::colours::ember.withAlpha(0.68f));
    valueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(valueLabel);

    // Make sure it doesn't interfere with mouse events for components underneath it.
    setInterceptsMouseClicks(false, false);
}

ValuePopup::~ValuePopup()
{
}

void ValuePopup::paint(juce::Graphics&)
{
    // Nothing to paint here, as the child label handles everything.
}

void ValuePopup::resized()
{
    valueLabel.setBounds(getLocalBounds());
}

void ValuePopup::setText(const juce::String& text)
{
    valueLabel.setText(text, juce::dontSendNotification);
}
