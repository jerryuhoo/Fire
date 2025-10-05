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
    valueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.85f));
    valueLabel.setColour(juce::Label::textColourId, COLOUR1);
    valueLabel.setBorderSize({ 1, 1, 1, 1 });
    valueLabel.setColour(juce::Label::outlineColourId, COLOUR6);
    valueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(valueLabel);

    // Make sure it doesn't interfere with mouse events for components underneath it.
    setInterceptsMouseClicks(false, false);
}

ValuePopup::~ValuePopup()
{
}

void ValuePopup::paint(juce::Graphics& g)
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
