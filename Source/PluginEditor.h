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
#include "Graph Components/VUPanel.h"
#include "Graph Components/GraphPanel.h"
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
    void setMultiband();
   

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FireAudioProcessor &processor;
    state::StateComponent stateComponent;
    juce::String lastPresetName;
    
    // init editor
    void initEditor();
    
    // Graph panel
    GraphPanel graphPanel {processor};
    
    // Multiband
    Multiband multiband;
    
    // TODO: this is temporary method. Maybe should create custom attachments. juce::ParameterAttachment?
    
    juce::Slider multiFocusSlider1, multiFocusSlider2, multiFocusSlider3, multiFocusSlider4;
    juce::Slider multiEnableSlider1, multiEnableSlider2, multiEnableSlider3, multiEnableSlider4;
    juce::Slider multiFreqSlider1, multiFreqSlider2, multiFreqSlider3;
    juce::Slider lineStateSlider1, lineStateSlider2, lineStateSlider3;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiFocusAttachment1, multiFocusAttachment2, multiFocusAttachment3, multiFocusAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiEnableAttachment1, multiEnableAttachment2, multiEnableAttachment3, multiEnableAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiFreqAttachment1, multiFreqAttachment2, multiFreqAttachment3;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lineStateSliderAttachment1, lineStateSliderAttachment2, lineStateSliderAttachment3;
    
    bool multibandFocus[4];
    bool multibandEnable[4];
    int multibandFreq[3] = { 0, 0, 0 };
    bool lineState[3];
    float linePos[3];
    
    // Spectrum
    SpectrumComponent spectrum;


    // Sliders
    juce::Slider
        driveKnob1,
        driveKnob2,
        driveKnob3,
        driveKnob4,
    
        outputKnob1,
        outputKnob2,
        outputKnob3,
        outputKnob4,
    
        mixKnob1,
        mixKnob2,
        mixKnob3,
        mixKnob4,

        downSampleKnob,
        cutoffKnob,
        resKnob,
        colorKnob,
        mixKnob,
        outputKnob;
    
    juce::Slider* recKnob1;
    juce::Slider* recKnob2;
    juce::Slider* recKnob3;
    juce::Slider* recKnob4;
    
    juce::Slider* biasKnob1;
    juce::Slider* biasKnob2;
    juce::Slider* biasKnob3;
    juce::Slider* biasKnob4;

    juce::Slider* compRatioKnob1;
    juce::Slider* compRatioKnob2;
    juce::Slider* compRatioKnob3;
    juce::Slider* compRatioKnob4;
    
    juce::Slider* compThreshKnob1;
    juce::Slider* compThreshKnob2;
    juce::Slider* compThreshKnob3;
    juce::Slider* compThreshKnob4;
    
    juce::Slider* widthKnob1;
    juce::Slider* widthKnob2;
    juce::Slider* widthKnob3;
    juce::Slider* widthKnob4;
    
    int knobSize = KNOBSIZE;
    float tempDriveValue[4] = {1, 1, 1, 1};
    float tempBiasValue[4] = {0, 0, 0, 0};

    // Labels
    juce::Label
        hqLabel,
        driveLabel,
        CompRatioLabel,
        CompThreshLabel,
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
        filterStateLabel,
        filterTypeLabel,
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

    // switches
    juce::TextButton shapeSwitch, widthSwitch, compressorSwitch;
    
    // vectors for sliders
    juce::OwnedArray<juce::Component, juce::CriticalSection> shapeVector;
    juce::OwnedArray<juce::Component, juce::CriticalSection> widthVector;
    juce::OwnedArray<juce::Component, juce::CriticalSection> compressorVector;
    
    // group toggle buttons
    enum RadioButtonIds
    {
        // filter state: off, pre, post
        filterStateButtons = 1001,
        // filter mode: low, band, high
        filterModeButtons = 1002,
        // window selection: left, right
        windowButtons = 1003,
        // switches
        switchButtons = 1004,
    };

    void updateToggleState();
    

    void setMenu(juce::ComboBox* combobox);
    void setListenerKnob(juce::Slider& slider);
    void setRotarySlider(juce::Slider& slider);
    void setLinearSlider(juce::Slider& slider);
    void setRoundButton(juce::TextButton& button, juce::String paramId, juce::String buttonName);
    void changeSliderState(juce::ComboBox *combobox);
    void updateFreqArray();
    void setSliderState(FireAudioProcessor* processor, juce::Slider& slider, juce::String paramId, float &tempValue);
    void linkValue(juce::Slider &xSlider, juce::Slider &driveSlider, juce::Slider &outputSlider, juce::TextButton& linkedButton);
    void setDistortionGraph(juce::String modeId, juce::String driveId,
        juce::String recId, juce::String mixId, juce::String biasId);

    void initState();
    void setFourKnobsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum);

    bool isPresetChanged();

    // override listener functions
    // linked
    void sliderValueChanged(juce::Slider *slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox *combobox) override;
    // hide and show labels
//    void sliderDragStarted (juce::Slider*) override;
//    void sliderDragEnded (juce::Slider*) override;
    void setInvisible(juce::OwnedArray<juce::Component, juce::CriticalSection> &array);
    
    // Slider attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        driveAttachment1,
        driveAttachment2,
        driveAttachment3,
        driveAttachment4,
    
        outputAttachment1,
        outputAttachment2,
        outputAttachment3,
        outputAttachment4,
        outputAttachment,
    
        mixAttachment1,
        mixAttachment2,
        mixAttachment3,
        mixAttachment4,
        mixAttachment,
    
        compRatioAttachment1,
        compRatioAttachment2,
        compRatioAttachment3,
        compRatioAttachment4,
    
        compThreshAttachment1,
        compThreshAttachment2,
        compThreshAttachment3,
        compThreshAttachment4,
    
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
        filterHighAttachment;

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
    DriveLookAndFeel driveLookAndFeel;
    LowPassButtonLnf lowPassButtonLnf;
    BandPassButtonLnf bandPassButtonLnf;
    HighPassButtonLnf highPassButtonLnf;
    FlatButtonLnf flatButtonLnf;
    
    Distortion distortionProcessor;

    //return function value by different modes
    //float getFunctionValue(FireAudioProcessor& processor, float value);
    //int modeChoice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessorEditor)
};
