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
    void setMultiband();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void markPresetAsDirty();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor& processor;
    state::StateComponent stateComponent;

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
    BandPanel bandPanel { processor };

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

    void setDistortionGraph(juce::String modeId, juce::String driveId, juce::String recId, juce::String mixId, juce::String biasId, juce::String safeId, int bandIndex);

    void setFourComponentsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum, bool isComboboxVisible);

    // override listener functions

    void sliderValueChanged(juce::Slider* slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox* combobox) override;
    // hide and show labels
    //    void sliderDragStarted (juce::Slider*) override;
    //    void sliderDragEnded (juce::Slider*) override;
    //    void changeSliderState(juce::ComboBox *combobox);

    void exitAssignMode();

    // Button attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment;

    // ComboBox attachment
    juce::ComboBox distortionMode1;
    juce::ComboBox distortionMode2;
    juce::ComboBox distortionMode3;
    juce::ComboBox distortionMode4;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment3;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
