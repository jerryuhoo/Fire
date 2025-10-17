/*
  ==============================================================================

    SettingsComponent.h
    Created: 3 Jul 2025 1:03:50pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once
#include "../GUI/InterfaceDefines.h"
#include "../Utility/VersionInfo.h" // 2. Include VersionInfo to get version string
#include "BinaryData.h" // 1. Include BinaryData to access embedded assets
#include "InterfaceDefines.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"

class FireAudioProcessor;

// A dedicated component for all settings.
class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(juce::PropertiesFile& props)
        : appProperties(props)
    {
        // --- Load background image ---
        // This assumes 'fire_anime.png' has been added to your project's BinaryData
        backgroundImage = juce::ImageCache::getFromMemory(BinaryData::fire_anime_png, BinaryData::fire_anime_pngSize);

        // --- Logo Setup ---
        // Load the logo image from the embedded binary data
        logoImage = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, BinaryData::firewingslogo_pngSize);
        logo.setImage(logoImage);
        addAndMakeVisible(logo);

        // --- Version Label Setup ---
        // Use the getVersionString() function you already have
        versionLabel.setText("Version: " + juce::String(VERSION), juce::dontSendNotification);
        versionLabel.setJustificationType(juce::Justification::centred);
        versionLabel.setFont(juce::Font {
            juce::FontOptions()
                .withHeight(14.0f)
                .withStyle("italic") });
        versionLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.8f));
        addAndMakeVisible(versionLabel);

        // --- Author Label Setup ---
        authorLabel.setText("Developed by Yifeng Yu", juce::dontSendNotification);
        authorLabel.setJustificationType(juce::Justification::centred);
        authorLabel.setFont(juce::Font {
            juce::FontOptions()
                .withHeight(14.0f)
                .withStyle("Plain") });
        authorLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.8f));
        addAndMakeVisible(authorLabel);

        companyLabel.setButtonText("BlueWingsMusic @ 2025");
        companyLabel.setURL(juce::URL("https://bluewingsmusic.com"));
        companyLabel.setJustificationType(juce::Justification::centred);
        companyLabel.setFont(juce::Font {
                                 juce::FontOptions()
                                     .withHeight(14.0f)
                                     .withStyle("Plain") },
                             false);
        companyLabel.setColour(juce::HyperlinkButton::textColourId, juce::Colours::skyblue);
        addAndMakeVisible(companyLabel);

        // --- Auto-Update Toggle ---
        addAndMakeVisible(autoUpdateToggle);
        autoUpdateToggle.setButtonText("Auto-check for updates on startup");
        autoUpdateToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::whitesmoke);
        bool shouldAutoUpdate = appProperties.getBoolValue(AUTO_UPDATE_ID, true);
        autoUpdateToggle.setToggleState(shouldAutoUpdate, juce::dontSendNotification);

        autoUpdateToggle.onClick = [this]
        {
            bool newValue = autoUpdateToggle.getToggleState();
            appProperties.setValue(AUTO_UPDATE_ID, newValue);
        };
    }

    void paint(juce::Graphics& g) override
    {
        // Fill background with a fallback color
        g.fillAll(juce::Colours::black);

        // Check if the background image is valid before trying to draw it
        if (backgroundImage.isValid())
        {
            // Draw the background image to fill the component, maintaining aspect ratio and centering it.
            auto bounds = getLocalBounds();
            g.drawImageWithin(backgroundImage,
                              bounds.getX(),
                              bounds.getY(),
                              bounds.getWidth(),
                              bounds.getHeight(),
                              juce::RectanglePlacement::fillDestination);
        }

        // Draw a semi-transparent dark overlay to ensure text is readable
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillRect(getLocalBounds());
    }

    void resized() override
    {
        // Create a pleasing layout with padding
        auto bounds = getLocalBounds().reduced(20);

        // 1. Place the logo at the top
        // Make it a square, centered horizontally, occupying 40% of the height
        auto logoHeight = bounds.getHeight() * 0.4f;
        auto logoArea = bounds.removeFromTop(logoHeight);
        logo.setBounds(logoArea.withSizeKeepingCentre(logoHeight, logoHeight));

        bounds.removeFromTop(20); // Add some space after the logo

        // 2. Place the version label
        versionLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5); // A little space

        // 3. Place the author label
        authorLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5);

        // 4. Place the company label
        companyLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(20); // More space before the setting

        // 5. Place the toggle button
        autoUpdateToggle.setBounds(bounds.removeFromTop(24));

        // Future settings components can continue to be laid out from the remaining bounds...
    }

private:
    juce::PropertiesFile& appProperties;

    juce::Image backgroundImage;
    juce::Image logoImage;
    juce::ImageComponent logo;
    juce::Label versionLabel;
    juce::Label authorLabel;
    juce::HyperlinkButton companyLabel;

    juce::ToggleButton autoUpdateToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
