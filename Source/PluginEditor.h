/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Distortion.h"
#include "LookAndFeel.h"

//class Visualiser : public AudioVisualiserComponent
//{
//public:
//    Visualiser() : AudioVisualiserComponent (2)
//    {
//        setBufferSize(512);
//        setSamplesPerBlock(256);
//        setColours(Colour (50, 0, 0), Colours::red);
//    }
//};

//==============================================================================
/**
*/
class FireAudioProcessorEditor : public AudioProcessorEditor,//2019/12/28
                                 public Slider::Listener,
                                 public ComboBox::Listener, //2020/08/08
                                 public Timer //edited 2020/07/03 fps
{
public:
    //typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    FireAudioProcessorEditor(FireAudioProcessor &);
    ~FireAudioProcessorEditor();

    //==============================================================================
    void paint(Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    //    Visualiser visualiser;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor &processor;
    state::StateComponent stateComponent;
    
    // ff meter
    foleys::LevelMeterLookAndFeel lnf;
    foleys::LevelMeter inputMeter{ foleys::LevelMeter::Minimal };
    foleys::LevelMeter outputMeter{ foleys::LevelMeter::Minimal };

    // sliders
    Slider
        //inputKnob,
        driveKnob,
        downSampleKnob,
        outputKnob,
        recKnob,
        mixKnob,
        cutoffKnob,
        resKnob,
        colorKnob,
        biasKnob;
    int knobSize = 100;
    float tempDriveValue = 1;
    float tempBiasValue = 0;
    
    // labels
    Label
        hqLabel,
        //inputLabel,
        driveLabel,
        downSampleLabel,
        outputLabel,
        recLabel,
        mixLabel,
        cutoffLabel,
        resLabel,
        linkedLabel,
        safeLabel,
        recOffLabel,
        recHalfLabel,
        recFullLabel,
        filterOffLabel,
        filterLowLabel,
        colorLabel,
        biasLabel;

    // buttons
    TextButton
        hqButton,
        linkedButton,
        safeButton,
        filterOffButton,
        filterPreButton,
        filterPostButton,
        filterLowButton,
        filterBandButton,
        filterHighButton;

    // about
    TextButton aboutButton {"about"};

    // group toggle buttons
    enum RadioButtonIds
    {
        // filter state: off, pre, post
        filterStateButtons = 1001,
        // filter mode: low, band, high
        filterModeButtons = 1002,
    };
    
    void updateToggleState (Button* button, String name);
    //void setCutoffButtonState(TextButton* textButton, Slider* slider, Slider* slider2);
    
    // override listener functions
    // linked
    void sliderValueChanged (Slider* slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged (ComboBox *combobox) override;

    // Slider attachment
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>
        //inputAttachment,
        driveAttachment,
        downSampleAttachment,
        recAttachment,
        outputAttachment,
        mixAttachment,
        cutoffAttachment,
        resAttachment,
        colorAttachment,
        biasAttachment;
    
    // Toggle Button attachment
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment,
        linkedAttachment,
        safeAttachment,
        filterOffAttachment,
        filterPreAttachment,
        filterPostAttachment,
        filterLowAttachment,
        filterBandAttachment,
        filterHighAttachment;
    
    // ComboBox attachment
    ComboBox distortionMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    
    // create own knob style
    OtherLookAndFeel otherLookAndFeel;
    RoundedButtonLnf roundedButtonLnf;
    
    Distortion distortionProcessor;

    //return function value by different modes
    //float getFunctionValue(FireAudioProcessor& processor, float value);
    //int modeChoice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
