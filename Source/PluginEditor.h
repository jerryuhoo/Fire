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
class FireAudioProcessorEditor : public AudioProcessorEditor,//edited 12/28
                                  public Slider::Listener,
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
    
    // linked
    void sliderValueChanged (Slider* slider) override
    {
        float thresh = 20.f;
        float changeThresh = 3.f;
        if (linkedButton.getToggleState() == true)
        {
            if (slider == &driveKnob)
            {
                if (driveKnob.getValue()>2)
                    outputKnob.setValue(-changeThresh - (driveKnob.getValue()-1)/31*(thresh-changeThresh));
                else
                    outputKnob.setValue((driveKnob.getValue()-1)*(-3));
            }
            else if (slider == &outputKnob && driveKnob.isEnabled())
            {
                if (outputKnob.getValue() < -changeThresh && outputKnob.getValue() > -thresh)
                    driveKnob.setValue(1-(outputKnob.getValue()+changeThresh)*31/(thresh-changeThresh));
                else if (outputKnob.getValue() >=-changeThresh && outputKnob.getValue() <0)
                    driveKnob.setValue(1 + outputKnob.getValue()/(-changeThresh));
                else if (outputKnob.getValue() >= 0)
                    driveKnob.setValue(1);
                else if (outputKnob.getValue() <= -10)
                    driveKnob.setValue(32);
            }
        }
    }
    
    
    //    Visualiser visualiser;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor &processor;

    // ff meter
    foleys::LevelMeterLookAndFeel lnf;
    foleys::LevelMeter inputMeter{ foleys::LevelMeter::Minimal };
    foleys::LevelMeter outputMeter{ foleys::LevelMeter::Minimal };

    // sliders
    Slider
        inputKnob,
        driveKnob,
        downSampleKnob,
        outputKnob,
        recKnob,
        mixKnob,
        cutoffKnob,
        resKnob;
    int knobSize = 100;

    // labels
    Label
        hqLabel,
        inputLabel,
        driveLabel,
        downSampleLabel,
        outputLabel,
        recLabel,
        mixLabel,
        cutoffLabel,
        resLabel,
        linkedLabel,
        recOffLabel,
        recHalfLabel,
        recFullLabel,
        filterOffLabel,
        filterLowLabel;

    TextButton
        hqButton,
        linkedButton,
        filterOffButton,
        filterPreButton,
        filterPostButton,
        filterLowButton,
        filterBandButton,
        filterHighButton;
        
    
    // toggle buttons
//    ToggleButton linkedButton{ "Link" };
        //hqButton{ "HQ" }, // high quality (oversampling)
        //recOffButton{ "Off" },
        //recHalfButton{ "Half" },
        //recFullButton{ "Full" },
//        filterOffButton{ "Off" },
//        filterPreButton{ "Pre" },
//        filterPostButton{ "Post" },
//        filterLowButton{ "Low Pass" },
//        filterBandButton{ "Band Pass" },
//        filterHighButton{ "High Pass" };
    
    // about
    TextButton aboutButton {"about"};
//    AlertWindow aboutDialog;
    // group toggle buttons
    enum RadioButtonIds
    {
        // filter state: off, pre, post
        filterStateButtons = 1001,
        // filter mode: low, band, high
        filterModeButtons = 1002,
        // rec state: off, half, full
        //recStateButtons = 1003
    };
    
    void updateToggleState (Button* button, String name);
    void setKnobState(Slider* slider);
    void setButtonState(TextButton* textButton, Slider* slider, Slider* slider2);
    
    // Slider attachment
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>
        inputAttachment,
        driveAttachment,
        downSampleAttachment,
        recAttachment,
        outputAttachment,
        mixAttachment,
        cutoffAttachment,
        resAttachment;
    
    // Toggle Button attachment
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment,
        linkedAttachment,
        //recOffAttachment,
        //recHalfAttachment,
        //recFullAttachment,
        filterOffAttachment,
        filterPreAttachment,
        filterPostAttachment,
        filterLowAttachment,
        filterBandAttachment,
        filterHighAttachment;
    
    // ComboBox attachment
    ComboBox distortionMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    
    //ComboBoxParameterAttachment
    
    // create own knob style
    OtherLookAndFeel otherLookAndFeel;
    OtherLookAndFeelRed otherLookAndFeelRed;
    ButtonLnf buttonLnf;
    
    Distortion distortionProcessor;

    //return function value by different modes
    //float getFunctionValue(FireAudioProcessor& processor, float value);
    //int modeChoice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
