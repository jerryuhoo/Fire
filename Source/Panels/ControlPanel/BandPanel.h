/*
  ==============================================================================

    BandPanel.h
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../GUI/LookAndFeel.h"
#include "../../PluginProcessor.h"
//==============================================================================
/*
*/
class BandPanel  : public juce::Component,
                   public juce::Slider::Listener,
                   public juce::ComboBox::Listener,
                   public juce::Timer,
                   public juce::Button::Listener
{
public:
    BandPanel(FireAudioProcessor &);
    ~BandPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void setBandNum(int num);
    void setScale(float scale);
    void changeSliderState(int bandNum, bool isPresetChanged);
private:
    FireAudioProcessor &processor;
    juce::Rectangle<int> bandKnobArea;
    juce::Rectangle<int> driveKnobArea;
    juce::Rectangle<int> outputKnobArea;
    
    // override listener functions
    // linked
    void sliderValueChanged(juce::Slider *slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox *combobox) override;
    void buttonClicked(juce::Button *clickedButton) override;
    void setListenerKnob(juce::Slider& slider);
    void setRotarySlider(juce::Slider& slider, juce::Colour colour);
    void setFlatButton(juce::TextButton& button, juce::String paramId, juce::String buttonName);
    void setFourKnobsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum);
    void linkValue(juce::Slider &xSlider, juce::Slider &driveSlider, juce::Slider &outputSlider, juce::TextButton& linkedButton);
    void setInvisible(juce::OwnedArray<juce::Component, juce::CriticalSection> &array);
    void setSliderState(FireAudioProcessor* processor, juce::Slider& slider, juce::String paramId, float &tempValue, bool isPresetChanged);
    
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
        mixKnob4;
    
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
    
    juce::Label
        driveLabel,
        CompRatioLabel,
        CompThreshLabel,
        widthLabel,
        outputLabel,
        recLabel,
        mixLabel,
        linkedLabel,
        safeLabel,
        biasLabel;
    
    juce::TextButton
        
        linkedButton1,
        linkedButton2,
        linkedButton3,
        linkedButton4,
        safeButton1,
        safeButton2,
        safeButton3,
        safeButton4;
    
    // switches
    juce::TextButton
        oscSwitch,
        shapeSwitch,
        widthSwitch,
        compressorSwitch;
    
    // vectors for sliders
    juce::OwnedArray<juce::Component, juce::CriticalSection> shapeVector;
    juce::OwnedArray<juce::Component, juce::CriticalSection> widthVector;
    juce::OwnedArray<juce::Component, juce::CriticalSection> compressorVector;
    juce::OwnedArray<juce::Component, juce::CriticalSection> oscVector;
    
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
    
        mixAttachment1,
        mixAttachment2,
        mixAttachment3,
        mixAttachment4,
    
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
        biasAttachment4;
      
    // Button attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        
        linkedAttachment1,
        linkedAttachment2,
        linkedAttachment3,
        linkedAttachment4,
        safeAttachment1,
        safeAttachment2,
        safeAttachment3,
        safeAttachment4;

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
    
    int bandNum;
    float scale = 1.0f;
    
    float tempDriveValue[4] = {1, 1, 1, 1};
    float tempBiasValue[4] = {0, 0, 0, 0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
