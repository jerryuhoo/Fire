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
#include "PrimaryButton.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"

struct SettingsComponentTestAccess;

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

    void visibilityChanged() override
    {
        if (! isShowing())
            dismissPointerGestures();
    }

    void enablementChanged() override
    {
        // Parent enablement changes do not form a valid continuation of a
        // pointer gesture that began in the previous settings-dialog state.
        dismissPointerGestures();
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
    friend struct SettingsComponentTestAccess;

    template <typename PrimaryButtonType>
    class DialogSessionButton final : public PrimaryButtonType
    {
    public:
        using PrimaryButtonType::PrimaryButtonType;

        void triggerClick() override
        {
            if (! canActivateInCurrentDialog())
                return;

            // Accessibility presses use triggerClick(), whose JUCE default is
            // posted. Complete this dialog-local action synchronously so it
            // cannot arrive after the settings window has been hidden/reused.
            // This is the final operation because the callback may delete the
            // dialog, its editor, and this button.
            this->internalClickCallback(juce::ModifierKeys::currentModifiers);
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key.isKeyCode(juce::KeyPress::returnKey)
                || key.isKeyCode(juce::KeyPress::spaceKey))
            {
                if (! canActivateInCurrentDialog())
                    return false;

                // Match native button keyboard expectations without posting a
                // command into a later settings-dialog session.
                this->internalClickCallback(key.getModifiers());
                return true;
            }

            return PrimaryButtonType::keyPressed(key);
        }

    private:
        bool canActivateInCurrentDialog() const noexcept
        {
            return this->isEnabled() && this->isShowing();
        }
    };

    void dismissPointerGestures() noexcept
    {
        const juce::Component::SafePointer<SettingsComponent> safeThis(this);
        companyLabel.dismissPointerGesture();

        if (safeThis != nullptr)
            autoUpdateToggle.dismissPointerGesture();
    }

    juce::PropertiesFile& appProperties;

    juce::Rectangle<int> fireGlyphArea;
    FireLookAndFeel fireLookAndFeel;
    juce::Label versionLabel;
    juce::Label authorLabel;
    DialogSessionButton<PrimaryHyperlinkButton> companyLabel;

    DialogSessionButton<PrimaryToggleButton> autoUpdateToggle;
};
