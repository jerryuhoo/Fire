/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "GUI/InterfaceDefines.h"
#include "GUI/LookAndFeel.h"
#include "GUI/PrimaryButton.h"
#include "GUI/ValueEntryPopup.h"
#include "GUI/ValuePopup.h"
#include "Panels/ControlPanel/BandPanel.h"
#include "Panels/ControlPanel/GlobalPanel.h"
#include "Panels/ControlPanel/LfoPanel.h"
#include "Panels/SpectrogramPanel/FilterControl.h"
#include "Panels/SpectrogramPanel/Multiband.h"
#include "Panels/SpectrogramPanel/SpectrumBackground.h"
#include "Utility/VersionInfo.h"
#include <array>
#include <cstdint>
#include <vector>

// Note: Removed includes for individual graph components as they are now managed by BandPanel/GlobalPanel

struct DistortionGraphSourceEpochTestAccess;

struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    juce::String preRelease;

    // Parses a version string, e.g., "v1.5.0-beta" or "1.0.2"
    Version(juce::String versionString)
    {
        // Remove the optional 'v' prefix
        if (versionString.startsWith("v"))
            versionString = versionString.substring(1);

        // Separate the pre-release identifier (e.g., "-beta", "-rc1")
        int preReleaseIndex = versionString.indexOf("-");
        if (preReleaseIndex != -1)
        {
            preRelease = versionString.substring(preReleaseIndex + 1);
            versionString = versionString.substring(0, preReleaseIndex);
        }

        // Split the major, minor, and patch version numbers
        juce::StringArray parts;
        parts.addTokens(versionString, ".", "");

        if (parts.size() > 0)
            major = parts[0].getIntValue();
        if (parts.size() > 1)
            minor = parts[1].getIntValue();
        if (parts.size() > 2)
            patch = parts[2].getIntValue();
    }

    // Overload the less-than operator "<" to implement version comparison
    bool operator<(const Version& other) const
    {
        if (major != other.major)
            return major < other.major;
        if (minor != other.minor)
            return minor < other.minor;
        if (patch != other.patch)
            return patch < other.patch;

        // SemVer rule: A stable version is greater than a pre-release version.
        // If this version has a pre-release tag and the other does not, this one is smaller.
        if (! preRelease.isEmpty() && other.preRelease.isEmpty())
            return true;

        // If this version does not have a pre-release tag and the other one does, this one is greater.
        if (preRelease.isEmpty() && ! other.preRelease.isEmpty())
            return false;

        // If both have or both lack a pre-release tag, compare them numerically.
        // (Note: full SemVer pre-release comparison is more complex, but this is sufficient for this use case)
        return preRelease < other.preRelease;
    }
};

//==============================================================================
/**
*/
class FireAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public juce::Slider::Listener,
                                 public juce::ComboBox::Listener,
                                 public juce::Timer,
                                 public juce::Button::Listener,
                                 public juce::AsyncUpdater,
                                 public juce::ChangeListener
{
public:
    FireAudioProcessorEditor(FireAudioProcessor&);
    ~FireAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void markPresetAsDirty();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void showValuePopupForSlider(ModulatableSlider* slider);
    void updateValuePopupForSlider(ModulatableSlider* slider);
    void hideValuePopup();

private:
    friend struct DistortionGraphSourceEpochTestAccess;
    friend struct MeterFreshnessTestAccess;

    class UpdateCheckThread final : public juce::Thread
    {
    public:
        explicit UpdateCheckThread(FireAudioProcessorEditor& ownerToUse);
        void run() override;
        void stop();

    private:
        FireAudioProcessorEditor& owner;
        VersionInfo::FetchOperation fetchOperation;
    };

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor& processor;
    state::StateComponent stateComponent;

    ValuePopup valuePopup;
    ValueEntryPopup valueEntryPopup;
    juce::String valueEntryTargetParameterID;

    juce::Image backgroundCache;
    float currentDisplayScale = 1.0f;

    struct EmberParticle
    {
        float x = 0.0f;
        float y = 0.0f;
        float velocity = 0.0f;
        float drift = 0.0f;
        float size = 1.0f;
        float phase = 0.0f;
    };

    std::array<EmberParticle, 18> headerEmbers {};
    double lastAnimationTimeSeconds = 0.0;
    float animationSeconds = 0.0f;
    float headerEnergy = 0.0f;
    int animationFrame = 0;
    bool lastBypassedState = false;

    MeterValues cachedMeterValues;
    std::uint64_t meterPacketGeneration = 0;
    bool hasCachedMeterValues = false;

    fire::ui::DampedValue workspaceSelection;
    int activeWorkspace = 0;

    std::array<float, 2 * SpectrumProcessor::fftSize> processedFftFrame {};
    std::array<float, 2 * SpectrumProcessor::fftSize> originalFftFrame {};

    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> spectrumCardArea;
    juce::Rectangle<int> navigationArea;
    juce::Rectangle<int> contentArea;

    // create own knob style
    FireLookAndFeel fireLookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 550 };

    bool isLfoAssignMode = false;
    int lfoSourceForAssignment = 0;
    float assignModePulseAlpha = 0.0f;
    float assignModePulseAngle = 0.0f;

    void updateWhenChangingFocus(int bandIndex);
    void updateMainPanelVisibility();
    void synchroniseHistorySourceForWorkspace(int workspace);
    void rebuildBackgroundCache();
    void initialiseHeaderEmbers();
    void advanceAnimations(float deltaSeconds);
    void drawAnimatedHeader(juce::Graphics& g);
    void drawWorkspaceSelection(juce::Graphics& g);
    void selectWorkspace(int targetWorkspace, bool animateSelection);

    void buttonClicked(juce::Button* clickedButton) override;
    // init editor
    void initEditor();

    // Top Area
    juce::Rectangle<int> logoArea;
    juce::Rectangle<int> wingsArea;

    // Multiband
    Multiband multiband { processor, stateComponent };

    // Band
    BandPanel bandPanel;

    // Global
    GlobalPanel globalPanel;

    // LFO
    LfoPanel lfoPanel;

    // Filter Control
    FilterControl filterControl { processor, globalPanel };

    // Spectrum
    SpectrumComponent processedSpectrum { 1, true };
    SpectrumComponent originalSpectrum { 0, false };
    SpectrumBackground specBackground;

    // Labels
    juce::Label hqLabel;

    // Buttons
    PrimaryTextButton
        hqButton,
        windowLeftButton,
        windowRightButton,
        windowLfoButton,
        zoomButton;

    // group toggle buttons
    enum RadioButtonIds
    {
        // window selection: left, right
        windowButtons = 1003,
    };

    void setLinearSlider(juce::Slider& slider);

    // override listener functions

    void sliderValueChanged(juce::Slider* slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox* combobox) override;

    void exitAssignMode();

    void updateModulationStates();
    const std::vector<ModulatableSlider*>& getAllModulatableSliders() const noexcept;
    void refreshModulationSnapshot();
    void applyModulationSnapshot();
    std::vector<ModulatableSlider*> allModulatableSliders;
    juce::Array<ModulationRouting> modulationRoutingSnapshot;
    std::vector<int> modulationRoutingIndexBySlider;
    int modulationSnapshotFramesRemaining = 0;
    void publishAvailableUpdate(const juce::String& version);
    juce::String takeAvailableUpdate();

    // Button attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment;

    juce::CriticalSection updateResultLock;
    juce::String pendingUpdateVersion;
    UpdateCheckThread updateCheckThread;

    // ComboBoxes and attachments are now managed by BandPanel

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
