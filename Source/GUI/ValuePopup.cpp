/*
  ==============================================================================

    ValuePopup.cpp
    Created: 4 Oct 2025 7:36:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ValuePopup.h"

#include <cmath>

ValuePopup::ValuePopup()
{
    // Style the label for the popup window
    valueLabel.setColour(juce::Label::backgroundColourId, fire::ui::colours::surface0.withAlpha(0.96f));
    valueLabel.setColour(juce::Label::textColourId, fire::ui::colours::whiteHot);
    valueLabel.setColour(juce::Label::outlineColourId, fire::ui::colours::ember.withAlpha(0.68f));
    valueLabel.setJustificationType(juce::Justification::centred);
    setUiScale(1.0f);
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

void ValuePopup::setUiScale(float newScale)
{
    const auto safeScale = std::isfinite(newScale)
                               ? juce::jlimit(0.5f, 3.0f, newScale)
                               : 1.0f;
    if (juce::approximatelyEqual(uiScale, safeScale))
        return;

    uiScale = safeScale;
    const auto border = juce::jmax(1, juce::roundToInt(uiScale));
    valueLabel.setBorderSize({ border, border, border, border });
    valueLabel.setFont(fire::ui::bodyFont(12.0f * uiScale));
    valueLabel.repaint();
}
