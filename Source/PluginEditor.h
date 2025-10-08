/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "BinaryData.h"
#include "GUI/InterfaceDefines.h"
#include "GUI/LookAndFeel.h"
#include "GUI/ValueEntryPopup.h"
#include "GUI/ValuePopup.h"
#include "Panels/ControlPanel/BandPanel.h"
#include "Panels/ControlPanel/GlobalPanel.h"
#include "Panels/ControlPanel/Graph Components/DistortionGraph.h"
#include "Panels/ControlPanel/Graph Components/GraphPanel.h"
#include "Panels/ControlPanel/Graph Components/Oscilloscope.h"
#include "Panels/ControlPanel/Graph Components/VUPanel.h"
#include "Panels/ControlPanel/Graph Components/WidthGraph.h"
#include "Panels/ControlPanel/LfoPanel.h"
#include "Panels/SpectrogramPanel/FilterControl.h"
#include "Panels/SpectrogramPanel/Multiband.h"
#include "Panels/SpectrogramPanel/SpectrumBackground.h"

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
                                 public juce::AudioProcessorValueTreeState::Listener,
                                 public juce::AsyncUpdater,
                                 public juce::ChangeListener
{
public:
    FireAudioProcessorEditor(FireAudioProcessor&);
    ~FireAudioProcessorEditor();

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void markPresetAsDirty();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void showValuePopupForSlider(ModulatableSlider* slider);
    void updateValuePopupForSlider(ModulatableSlider* slider);
    void hideValuePopup();

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor& processor;
    state::StateComponent stateComponent;

    ValuePopup valuePopup;
    ValueEntryPopup valueEntryPopup;
    ModulatableSlider* sliderForValueEntry = nullptr;

    juce::Image backgroundCache;
    float currentDisplayScale = 1.0f;

    // create own knob style
    FireLookAndFeel fireLookAndFeel;

    bool isLfoAssignMode = false;
    int lfoSourceForAssignment = 0;
    float assignModePulseAlpha = 0.0f;
    float assignModePulseAngle = 0.0f;

    int focusIndex = 0;
    void updateWhenChangingFocus();
    void updateMainPanelVisibility();

    void buttonClicked(juce::Button* clickedButton) override;
    void mouseDown(const juce::MouseEvent& e) override;

    // init editor
    void initEditor();

    // Top Area
    juce::Rectangle<int> logoArea;
    juce::Rectangle<int> wingsArea;

    // Graph panel
    GraphPanel graphPanel { processor };

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
    juce::TextButton
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

    void setMenu(juce::ComboBox* combobox);

    void setLinearSlider(juce::Slider& slider);

    void updateDistortionModeVisibility();

    // override listener functions

    void sliderValueChanged(juce::Slider* slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox* combobox) override;
    // hide and show labels
    //    void sliderDragStarted (juce::Slider*) override;
    //    void sliderDragEnded (juce::Slider*) override;
    //    void changeSliderState(juce::ComboBox *combobox);

    void exitAssignMode();

    void updateModulationStates();
    std::vector<ModulatableSlider*> getAllModulatableSliders();

    // Button attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment;

    // ComboBox attachment
    std::array<juce::ComboBox, 4> distortionModes;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 4> modeAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
