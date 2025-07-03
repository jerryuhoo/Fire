/*
  ==============================================================================

    SettingsComponent.h
    Created: 3 Jul 2025 1:03:50pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "InterfaceDefines.h"

// A dedicated component for all settings.
class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(juce::AudioProcessorValueTreeState& apvts)
    {
        addAndMakeVisible(autoUpdateToggle);
        autoUpdateToggle.setButtonText("Auto-check for updates on startup");

        // Attach the toggle button to our parameter in the processor.
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, AUTO_UPDATE_ID, autoUpdateToggle);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        autoUpdateToggle.setBounds(bounds.removeFromTop(24));
        // Future settings components can be laid out here...
    }

private:
    juce::ToggleButton autoUpdateToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
