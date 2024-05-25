/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "GUI/LookAndFeel.h"
#include "Panels/ControlPanel/Graph Components/Oscilloscope.h"
#include "Panels/ControlPanel/Graph Components/DistortionGraph.h"
#include "Panels/ControlPanel/Graph Components/WidthGraph.h"
#include "Panels/ControlPanel/Graph Components/VUPanel.h"
#include "Panels/ControlPanel/Graph Components/GraphPanel.h"
#include "Panels/ControlPanel/BandPanel.h"
#include "Panels/ControlPanel/GlobalPanel.h"
#include "Panels/SpectrogramPanel/Multiband.h"
#include "Panels/SpectrogramPanel/FilterControl.h"
#include "GUI/InterfaceDefines.h"
#include "Panels/SpectrogramPanel/SpectrumBackground.h"

//==============================================================================
/**
*/
class FireAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public juce::Slider::Listener,
                                 public juce::ComboBox::Listener,
                                 public juce::Timer,
                                 public juce::Button::Listener
{
public:
    FireAudioProcessorEditor (FireAudioProcessor&);
    ~FireAudioProcessorEditor();

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void setMultiband();

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor& processor;
    state::StateComponent stateComponent;
    
    int focusIndex = 0;
    void updateWhenChangingFocus();

    void buttonClicked (juce::Button* clickedButton) override;
    void mouseDown (const juce::MouseEvent& e) override;

    // init editor
    void initEditor();

    // Graph panel
    GraphPanel graphPanel { processor };

    // Multiband
    Multiband multiband { processor, stateComponent };

    // Band
    BandPanel bandPanel { processor };

    // Global
    GlobalPanel globalPanel;

    // Filter Control
    FilterControl filterControl { processor, globalPanel };

    // Spectrum
    SpectrumComponent processedSpectrum{1, true};
    SpectrumComponent originalSpectrum{0, false};
    SpectrumBackground specBackground;

    // Labels
    juce::Label hqLabel;

    // Buttons
    juce::TextButton
        hqButton,
        windowLeftButton,
        windowRightButton,
        zoomButton;

    // group toggle buttons
    enum RadioButtonIds
    {
        // window selection: left, right
        windowButtons = 1003,
    };

    void setMenu (juce::ComboBox* combobox);

    void setLinearSlider (juce::Slider& slider);

    void setDistortionGraph (juce::String modeId, juce::String driveId, juce::String recId, juce::String mixId, juce::String biasId, juce::String safeId);

    void setFourComponentsVisibility (juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum);

    // override listener functions

    void sliderValueChanged (juce::Slider* slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged (juce::ComboBox* combobox) override;
    // hide and show labels
    //    void sliderDragStarted (juce::Slider*) override;
    //    void sliderDragEnded (juce::Slider*) override;
    //    void changeSliderState(juce::ComboBox *combobox);

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

    // create own knob style
    OtherLookAndFeel otherLookAndFeel;
    ZoomLookAndFeel zoomLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FireAudioProcessorEditor)
};
