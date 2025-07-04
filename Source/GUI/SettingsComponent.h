/*
  ==============================================================================

    SettingsComponent.h
    Created: 3 Jul 2025 1:03:50pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once
#include "InterfaceDefines.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "juce_audio_processors/juce_audio_processors.h"

class FireAudioProcessor;


// A dedicated component for all settings.
class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(juce::PropertiesFile& props)
        : appProperties(props)
    {
        addAndMakeVisible(autoUpdateToggle);
        autoUpdateToggle.setButtonText("Auto-check for updates on startup");

        // Read the initial state of the toggle from the properties file.
        // If the setting doesn't exist yet, default to `true`.
        bool shouldAutoUpdate = appProperties.getBoolValue(AUTO_UPDATE_ID, true);
        autoUpdateToggle.setToggleState(shouldAutoUpdate, juce::dontSendNotification);

        // When the button is clicked, immediately save the new value.
        autoUpdateToggle.onClick = [this]
        {
            bool newValue = autoUpdateToggle.getToggleState();
            // setValue() will automatically save the file to disk.
            appProperties.setValue(AUTO_UPDATE_ID, newValue);
        };
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        autoUpdateToggle.setBounds(bounds.removeFromTop(24));
        // Future settings components can be laid out here...
    }

private:
    juce::PropertiesFile& appProperties;
    juce::ToggleButton autoUpdateToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
