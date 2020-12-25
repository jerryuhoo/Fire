/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DSP/Distortion.h"
#include "GUI/LookAndFeel.h"
#include "Graph Components/Oscilloscope.h"
#include "Graph Components/DistortionGraph.h"
#include "Graph Components/WidthGraph.h"
#include "Multiband/Multiband.h"
#include "GUI/InterfaceDefines.h"

//==============================================================================
/**
*/
class FireAudioProcessorEditor : public juce::AudioProcessorEditor, //2019/12/28
                                 public juce::Slider::Listener,
                                 public juce::ComboBox::Listener, //2020/08/08
                                 public juce::Timer               //edited 2020/07/03 fps
{
public:
    //typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    FireAudioProcessorEditor(FireAudioProcessor &);
    ~FireAudioProcessorEditor();

    //==============================================================================
    void paint(juce::Graphics &g) override;
    void resized() override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor &processor;
    state::StateComponent stateComponent;
    
    // Oscilloscope
    Oscilloscope oscilloscope {processor};
    
    // Distortion graph
    DistortionGraph distortionGraph;
    
    // Width graph
    WidthGraph widthGraph;
    
    // Multiband
    Multiband multiband;
    
    // TODO: this is temporary method. Maybe should create custom attachments. juce::ParameterAttachment?
    
    juce::Slider multiFocusSlider1, multiFocusSlider2, multiFocusSlider3, multiFocusSlider4;
    juce::Slider multiStateSlider1, multiStateSlider2, multiStateSlider3, multiStateSlider4;
    juce::Slider multiFreqSlider1, multiFreqSlider2, multiFreqSlider3, multiFreqSlider4;
    juce::Slider lineNumSlider;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiFocusAttachment1, multiFocusAttachment2, multiFocusAttachment3, multiFocusAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiStateAttachment1, multiStateAttachment2, multiStateAttachment3, multiStateAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiFreqAttachment1, multiFreqAttachment2, multiFreqAttachment3;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lineNumSliderAttachment;
    
    bool multibandFocus[4];
    bool multibandState[4];
    int multibandFreq[3];
    
    // Spectrum
    SpectrumComponent spectrum;
    
    // ff meter
    foleys::LevelMeterLookAndFeel lnf;
    foleys::LevelMeter inputMeter{foleys::LevelMeter::Minimal};
    foleys::LevelMeter outputMeter{foleys::LevelMeter::Minimal};

    // Sliders
    juce::Slider
        driveKnob1,
        outputKnob1,
        mixKnob1,
        dynamicKnob1,
        widthKnob1,
    
        driveKnob2,
        outputKnob2,
        mixKnob2,
        dynamicKnob2,
        widthKnob2,
    
        driveKnob3,
        outputKnob3,
        mixKnob3,
        dynamicKnob3,
        widthKnob3,
    
        driveKnob4,
        outputKnob4,
        mixKnob4,
        dynamicKnob4,
        widthKnob4,
    
        mixKnob,
        recKnob1,
        recKnob2,
        recKnob3,
        recKnob4,
      
        biasKnob1,
        biasKnob2,
        biasKnob3,
        biasKnob4,
    
        downSampleKnob,
        cutoffKnob,
        resKnob,
        colorKnob;
    
    int knobSize = KNOBSIZE;
    float tempDriveValue = 1;
    float tempBiasValue = 0;

    // Labels
    juce::Label
        hqLabel,
        driveLabel,
        dynamicLabel,
        widthLabel,
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

    // Buttons
    juce::TextButton
        hqButton,
        linkedButton1,
        linkedButton2,
        linkedButton3,
        linkedButton4,
        safeButton1,
        safeButton2,
        safeButton3,
        safeButton4,
        filterOffButton,
        filterPreButton,
        filterPostButton,
        filterLowButton,
        filterBandButton,
        filterHighButton,
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
    };

    void updateToggleState();
    
    void setMenu(juce::ComboBox* combobox);
    void setNormalSlider(juce::Slider* slider);
    void setListenerSlider(juce::Slider* slider);
    void disableSlider(FireAudioProcessor* processor, juce::Slider& slider, juce::String paramID);

    // override listener functions
    // linked
    void sliderValueChanged(juce::Slider *slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox *combobox) override;

    // Slider attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        //inputAttachment,
        driveAttachment1,
        driveAttachment2,
        driveAttachment3,
        driveAttachment4,
    
        outputAttachment1,
        outputAttachment2,
        outputAttachment3,
        outputAttachment4,
    
        mixAttachment1,
        mixAttachment2,
        mixAttachment3,
        mixAttachment4,
        mixAttachment,
    
        dynamicAttachment1,
        dynamicAttachment2,
        dynamicAttachment3,
        dynamicAttachment4,
    
        widthAttachment1,
        widthAttachment2,
        widthAttachment3,
        widthAttachment4,
    
        recAttachment1,
        recAttachment2,
        recAttachment3,
        recAttachment4,
        
        biasAttachment1,
        biasAttachment2,
        biasAttachment3,
        biasAttachment4,
    
        downSampleAttachment,
        
        cutoffAttachment,
        resAttachment,
        colorAttachment;

    // Button attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hqAttachment,
        linkedAttachment1,
        linkedAttachment2,
        linkedAttachment3,
        linkedAttachment4,
        safeAttachment1,
        safeAttachment2,
        safeAttachment3,
        safeAttachment4,
        filterOffAttachment,
        filterPreAttachment,
        filterPostAttachment,
        filterLowAttachment,
        filterBandAttachment,
        filterHighAttachment,
        windowLeftButtonAttachment,
        windowRightButtonAttachment;

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
    //FlatButtonLnf flatButtonLnf;
    
    Distortion distortionProcessor;

    
    //return function value by different modes
    //float getFunctionValue(FireAudioProcessor& processor, float value);
    //int modeChoice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
