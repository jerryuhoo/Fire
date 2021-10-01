/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
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
    FireAudioProcessorEditor(FireAudioProcessor &);
    ~FireAudioProcessorEditor();

    //==============================================================================
    void paint(juce::Graphics &g) override;
    void resized() override;
    void timerCallback() override;
    void setMultiband();

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor &processor;
    state::StateComponent stateComponent;
    
    void buttonClicked(juce::Button *clickedButton) override;
    
    // init editor
    void initEditor();
    
    // Graph panel
    GraphPanel graphPanel {processor};
    
    // Multiband
    Multiband multiband {processor};
    
    // Filter Control
    FilterControl filterControl {processor};
    
    // Band
    BandPanel bandPanel {processor};
    
    // Global
    GlobalPanel globalPanel;

    // Spectrum
    SpectrumComponent spectrum;

    // Labels
    juce::Label hqLabel;

    // Buttons
    juce::TextButton
        hqButton,
        windowLeftButton,
        windowRightButton;

    // group toggle buttons
    enum RadioButtonIds
    {
        // filter state: off, pre, post
        filterStateButtons = 1001,
        // filter mode: low, band, high
        filterModeButtons = 1002,
        // window selection: left, right
        windowButtons = 1003,
        // switches band
        switchButtons = 1004,
        // switches global
        switchButtonsGlobal = 1005
    };

    void setMenu(juce::ComboBox* combobox);

    void setLinearSlider(juce::Slider& slider);
 
    void setDistortionGraph(juce::String modeId, juce::String driveId,
        juce::String recId, juce::String mixId, juce::String biasId, juce::String safeId);

    void initState();
    void setFourKnobsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum);

    // override listener functions
    // linked
    void sliderValueChanged(juce::Slider *slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox *combobox) override;
    // hide and show labels
//    void sliderDragStarted (juce::Slider*) override;
//    void sliderDragEnded (juce::Slider*) override;
    void changeSliderState(juce::ComboBox *combobox);
    
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
    RoundedButtonLnf roundedButtonLnf;
    DriveLookAndFeel driveLookAndFeel1;
    DriveLookAndFeel driveLookAndFeel2;
    DriveLookAndFeel driveLookAndFeel3;
    DriveLookAndFeel driveLookAndFeel4;
    LowPassButtonLnf lowPassButtonLnf;
    BandPassButtonLnf bandPassButtonLnf;
    HighPassButtonLnf highPassButtonLnf;
    FlatButtonLnf flatButtonLnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
