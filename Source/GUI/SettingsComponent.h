/*
  ==============================================================================

    SettingsComponent.h
    Created: 3 Jul 2025 1:03:50pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once
#include "InterfaceDefines.h"
#include "LookAndFeel.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"

// A dedicated component for all settings.
class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(juce::PropertiesFile& props)
        : appProperties(props)
    {
        setLookAndFeel(&fireLookAndFeel);
        setOpaque(true);

        // --- Version Label Setup ---
        // Use the getVersionString() function you already have
        versionLabel.setText("FIRE  /  VERSION " + juce::String(VERSION), juce::dontSendNotification);
        versionLabel.setJustificationType(juce::Justification::centred);
        versionLabel.setFont(juce::Font {
            juce::FontOptions()
                .withHeight(14.0f)
                .withStyle("italic") });
        versionLabel.setColour(juce::Label::textColourId, fire::ui::colours::whiteHot);
        addAndMakeVisible(versionLabel);

        // --- Author Label Setup ---
        authorLabel.setText("Designed & developed by Yifeng Yu", juce::dontSendNotification);
        authorLabel.setJustificationType(juce::Justification::centred);
        authorLabel.setFont(juce::Font {
            juce::FontOptions()
                .withHeight(14.0f)
                .withStyle("Plain") });
        authorLabel.setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
        addAndMakeVisible(authorLabel);

        companyLabel.setButtonText("BLUE WINGS MUSIC");
        companyLabel.setURL(juce::URL("https://bluewingsmusic.com"));
        companyLabel.setJustificationType(juce::Justification::centred);
        companyLabel.setFont(juce::Font {
                                 juce::FontOptions()
                                     .withHeight(14.0f)
                                     .withStyle("Plain") },
                             false);
        companyLabel.setColour(juce::HyperlinkButton::textColourId, fire::ui::colours::signalCool);
        addAndMakeVisible(companyLabel);

        // --- Auto-Update Toggle ---
        addAndMakeVisible(autoUpdateToggle);
        autoUpdateToggle.setButtonText("Auto-check for updates on startup");
        autoUpdateToggle.setColour(juce::ToggleButton::textColourId, fire::ui::colours::textSecondary);
        autoUpdateToggle.setColour(juce::ToggleButton::tickColourId, fire::ui::colours::flame);
        bool shouldAutoUpdate = appProperties.getBoolValue(AUTO_UPDATE_ID, true);
        autoUpdateToggle.setToggleState(shouldAutoUpdate, juce::dontSendNotification);

        autoUpdateToggle.onClick = [this]
        {
            bool newValue = autoUpdateToggle.getToggleState();
            appProperties.setValue(AUTO_UPDATE_ID, newValue);
        };
    }

    ~SettingsComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        fire::ui::drawCanvas(g, getLocalBounds().toFloat());
        fire::ui::drawTechGrid(g, getLocalBounds().toFloat(), 26.0f, 0.06f);
        fire::ui::drawPanel(g, getLocalBounds().toFloat().reduced(12.0f),
                            fire::ui::colours::ember, true,
                            fire::ui::Metrics::radiusLarge);
        fire::ui::drawFireGlyph(g, fireGlyphArea.toFloat(), 0.72f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(28);
        const auto logoHeight = juce::roundToInt(bounds.getHeight() * 0.30f);
        auto logoArea = bounds.removeFromTop(logoHeight);
        fireGlyphArea = logoArea.withSizeKeepingCentre(juce::jmin(logoHeight, 72),
                                                       juce::jmin(logoHeight, 72));

        bounds.removeFromTop(14);

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

    juce::Rectangle<int> fireGlyphArea;
    FireLookAndFeel fireLookAndFeel;
    juce::Label versionLabel;
    juce::Label authorLabel;
    juce::HyperlinkButton companyLabel;

    juce::ToggleButton autoUpdateToggle;
};
