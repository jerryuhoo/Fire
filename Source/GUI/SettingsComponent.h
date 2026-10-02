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
    static constexpr int minimumContentWidth = 300;
    static constexpr int minimumContentHeight = 250;
    static constexpr int minimumDialogWidth = 340;
    static constexpr int minimumDialogHeight = 300;

    SettingsComponent(juce::PropertiesFile& props)
        : appProperties(props)
    {
        setLookAndFeel(&fireLookAndFeel);
        setOpaque(true);

        // --- Version Label Setup ---
        // Use the getVersionString() function you already have
        versionLabel.setText("FIRE  /  VERSION " + juce::String(VERSION), juce::dontSendNotification);
        versionLabel.setJustificationType(juce::Justification::centred);
        versionLabel.setFont(fire::ui::displayFont(16.0f));
        versionLabel.setColour(juce::Label::textColourId, fire::ui::colours::whiteHot);
        addAndMakeVisible(versionLabel);

        // --- Author Label Setup ---
        authorLabel.setText("Designed & developed by Yifeng Yu", juce::dontSendNotification);
        authorLabel.setJustificationType(juce::Justification::centred);
        authorLabel.setFont(fire::ui::bodyFont(12.5f));
        authorLabel.setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
        addAndMakeVisible(authorLabel);

        companyLabel.setButtonText("BLUE WINGS MUSIC");
        companyLabel.setURL(juce::URL("https://bluewingsmusic.com"));
        companyLabel.setJustificationType(juce::Justification::centred);
        companyLabel.setFont(fire::ui::labelFont(12.0f), false);
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
        skinLabel.setText("APPEARANCE", juce::dontSendNotification);
        skinLabel.setFont(fire::ui::labelFont(10)); addAndMakeVisible(skinLabel);
        const juce::Component::SafePointer<SettingsComponent> safe(this);
        for (auto* button : {&modernSkin, &vintageSkin})
        {
            addAndMakeVisible(*button); button->setClickingTogglesState(true); button->setRadioGroupId(7301);
        }
        modernSkin.setButtonText("Modern"); modernSkin.setTitle("Modern skin"); modernSkin.setComponentID("skinModern");
        vintageSkin.setButtonText("Vintage"); vintageSkin.setTitle("Vintage skin"); vintageSkin.setComponentID("skinVintage");
        modernSkin.setTooltip("Graphite, aluminium and contemporary controls");
        vintageSkin.setTooltip("Warm metal, brass and vintage studio controls");
        const auto select = [safe](fire::ui::Skin skin)
        {
            const auto alive = safe;
            if (!alive) return;
            alive->applySkin(skin);
            if (!alive) return;
            alive->appProperties.setValue(fire::ui::skinSetting, static_cast<int>(skin));
            if (alive) alive->appProperties.saveIfNeeded();
        };
        modernSkin.onClick = [safe, select]
        {if (safe && safe->modernSkin.getToggleState()) {auto action = select; action(fire::ui::Skin::modern);}};
        vintageSkin.onClick = [safe, select]
        {if (safe && safe->vintageSkin.getToggleState()) {auto action = select; action(fire::ui::Skin::vintage);}};
        applySkin(fire::ui::skinFromValue(appProperties.getIntValue(fire::ui::skinSetting, 0)));
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
        fire::ui::drawCanvas(g, getLocalBounds().toFloat(), fire::ui::skinFor(*this));
        fire::ui::drawTechGrid(g, getLocalBounds().toFloat(), 26.0f, 0.06f, fire::ui::skinFor(*this));
        fire::ui::drawPanel(g, getLocalBounds().toFloat().reduced(12.0f),
                            fire::ui::colours::ember, true,
                            fire::ui::skinFor(*this));
        fire::ui::drawFireGlyph(g, fireGlyphArea.toFloat());
    }

    void resized() override
    {
        // The dialog constrainer prevents the production window from becoming
        // smaller than this layout. Keep the component itself defensive for
        // tests and unusual embedders: proportional outer padding preserves
        // useful controls before the preferred 28 px inset is available.
        const int horizontalInset = juce::jlimit(12, 28, getWidth() / 10);
        const int verticalInset = juce::jlimit(12, 20, getHeight() / 12);
        auto bounds = getLocalBounds().reduced(horizontalInset, verticalInset);
        const auto logoHeight = juce::jmin(58, juce::roundToInt(bounds.getHeight() * 0.20f));
        auto logoArea = bounds.removeFromTop(logoHeight);
        fireGlyphArea = logoArea.withSizeKeepingCentre(juce::jmin(logoHeight, 72),
                                                       juce::jmin(logoHeight, 72));

        bounds.removeFromTop(8);

        // 2. Place the version label
        versionLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5); // A little space

        // 3. Place the author label
        authorLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5);

        // 4. Place the company label
        companyLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(8);
        skinLabel.setBounds(bounds.removeFromTop(16));
        auto skinRow = bounds.removeFromTop(30);
        const int skinWidth = (skinRow.getWidth() - 8) / 2;
        modernSkin.setBounds(skinRow.removeFromLeft(skinWidth)); skinRow.removeFromLeft(8);
        vintageSkin.setBounds(skinRow); bounds.removeFromTop(8);

        // 5. Place the toggle button
        autoUpdateToggle.setBounds(bounds.removeFromTop(24));

        // Future settings components can continue to be laid out from the remaining bounds...
    }

private:
    friend struct SettingsComponentTestAccess;
    void applySkin(fire::ui::Skin skin)
    {
        const auto previous = fire::ui::skinFor(*this);
        fire::ui::setSkin(*this, skin); fireLookAndFeel.setSkin(skin);
        fire::ui::remapSkinColours(*this, previous, skin);
        skinLabel.setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textMuted);
        companyLabel.setColour(juce::HyperlinkButton::textColourId,
            skin == fire::ui::Skin::vintage ? juce::Colour(0xff226a74) : fire::ui::colours::signalCool);
        modernSkin.setToggleState(skin == fire::ui::Skin::modern, juce::dontSendNotification);
        vintageSkin.setToggleState(skin == fire::ui::Skin::vintage, juce::dontSendNotification);
        sendLookAndFeelChange(); repaint();
    }

    /** Settings-page link chrome which consumes PrimaryPointerButton's shared
        animation state instead of falling back to JUCE's abrupt hyperlink
        darkening. The native HyperlinkButton remains in the inheritance chain,
        preserving its URL, cursor, and hyperlink accessibility semantics.
    */
    class SettingsLinkButton : public PrimaryHyperlinkButton
    {
    public:
        void setFont(const juce::Font& newFont,
                     bool resizeToMatchComponentHeight,
                     juce::Justification justificationType =
                         juce::Justification::horizontallyCentred)
        {
            displayFont = newFont;
            resizeFont = resizeToMatchComponentHeight;
            juce::HyperlinkButton::setFont(newFont,
                                            resizeToMatchComponentHeight,
                                            justificationType);
        }

    private:
        void paintButton(juce::Graphics& g, bool, bool) override
        {
            using namespace fire::ui;

            const auto hover = getHoverAnimation();
            const auto press = getPressAnimation();
            const auto focus = getFocusAnimation();
            const auto disabled = getDisabledAnimation();
            const auto interactive = 1.0f - disabled;
            auto bounds = getLocalBounds().toFloat().reduced(0.5f);
            const auto radius = juce::jmin(Metrics::radiusSmall,
                                            bounds.getHeight() * 0.5f);
            const auto emphasis = juce::jmax(hover, focus);

            if (hover > 0.001f || press > 0.001f || focus > 0.001f)
            {
                auto wash = colours::surface2.interpolatedWith(colours::raised,
                                                                press);
                const auto washAlpha = juce::jlimit(
                    0.0f,
                    1.0f,
                    (0.075f * hover + 0.105f * press + 0.055f * focus)
                        * interactive);
                g.setColour(wash.withAlpha(washAlpha));
                g.fillRoundedRectangle(bounds, radius);
            }

            if (focus > 0.001f)
            {
                g.setColour(findColour(juce::HyperlinkButton::textColourId)
                                .withAlpha(0.58f * focus * interactive));
                g.drawRoundedRectangle(bounds.reduced(0.5f),
                                       radius,
                                       1.0f);
            }

            const auto font = resizeFont
                                  ? displayFont.withHeight(
                                        static_cast<float>(getHeight()) * 0.7f)
                                  : displayFont;
            auto textColour = findColour(
                juce::HyperlinkButton::textColourId);
            textColour = textColour.brighter(0.13f * hover)
                             .darker(0.11f * press)
                             .interpolatedWith(colours::textMuted,
                                               disabled * 0.72f);
            g.setColour(textColour);
            g.setFont(font);
            g.drawFittedText(getButtonText(),
                             getLocalBounds().reduced(6, 1),
                             getJustificationType().getOnlyHorizontalFlags()
                                 | juce::Justification::verticallyCentred,
                             1);

            // A short signal rail identifies this as a link without JUCE's
            // full-width default underline. It expands smoothly for hover and
            // keyboard focus, matching the editor's other compact controls.
            const auto measuredTextWidth = juce::GlyphArrangement::getStringWidth(
                font, getButtonText());
            const auto maximumRailWidth = juce::jmax(
                0.0f,
                juce::jmin(measuredTextWidth,
                           bounds.getWidth() - 12.0f));
            const auto railWidth = maximumRailWidth
                                   * (0.24f + 0.76f * emphasis);
            const auto railHeight = 1.0f + 0.35f * focus;
            g.setColour(findColour(juce::HyperlinkButton::textColourId)
                            .withAlpha((0.38f + 0.50f * emphasis)
                                       * interactive));
            g.fillRoundedRectangle(bounds.getCentreX() - railWidth * 0.5f,
                                   bounds.getBottom() - railHeight,
                                   railWidth,
                                   railHeight,
                                   railHeight * 0.5f);
        }

        juce::Font displayFont = fire::ui::bodyFont(14.0f);
        bool resizeFont = false;
    };

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
        if (safeThis != nullptr) modernSkin.dismissPointerGesture();
        if (safeThis != nullptr) vintageSkin.dismissPointerGesture();
    }

    juce::PropertiesFile& appProperties;

    juce::Rectangle<int> fireGlyphArea;
    FireLookAndFeel fireLookAndFeel;
    juce::Label versionLabel;
    juce::Label authorLabel;
    juce::Label skinLabel;
    DialogSessionButton<PrimaryTextButton> modernSkin, vintageSkin;
    DialogSessionButton<SettingsLinkButton> companyLabel;

    DialogSessionButton<PrimaryToggleButton> autoUpdateToggle;
};
