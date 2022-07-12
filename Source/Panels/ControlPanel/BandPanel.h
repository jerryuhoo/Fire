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
    void setFocusBandNum(int num);
    void setScale(float scale);
//    void changeSliderState(int bandNum, bool isPresetChanged);
    void setBandKnobsStates(int index, bool state, bool callFromSubBypass);
    juce::ToggleButton& getCompButton(const int index);
    juce::ToggleButton& getWidthButton(const int index);
    
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
    void initListenerKnob(juce::Slider& slider);
    void initRotarySlider(juce::Slider& slider, juce::Colour colour);
    void initFlatButton(juce::TextButton& button, juce::String paramId, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour, int index);
    void setFourComponentsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum);
    void linkValue(juce::Slider &xSlider, juce::Slider &driveSlider, juce::Slider &outputSlider, juce::TextButton& linkedButton);
    void setVisibility(juce::Array<juce::Component *> &array, bool isVisible);
    void updateBypassState(juce::ToggleButton &clickedButton, int index);
    void setBypassState(int index, bool state);
    bool canEnableSubKnobs(juce::Component &component);
    
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
    
    juce::Slider recKnob1;
    juce::Slider recKnob2;
    juce::Slider recKnob3;
    juce::Slider recKnob4;
    
    juce::Slider biasKnob1;
    juce::Slider biasKnob2;
    juce::Slider biasKnob3;
    juce::Slider biasKnob4;

    juce::Slider compRatioKnob1;
    juce::Slider compRatioKnob2;
    juce::Slider compRatioKnob3;
    juce::Slider compRatioKnob4;
    
    juce::Slider compThreshKnob1;
    juce::Slider compThreshKnob2;
    juce::Slider compThreshKnob3;
    juce::Slider compThreshKnob4;
    
    juce::Slider widthKnob1;
    juce::Slider widthKnob2;
    juce::Slider widthKnob3;
    juce::Slider widthKnob4;
    
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
    
//    juce::ToggleButton *widthBypassButton, *compressorBypassButton;
    std::unique_ptr<juce::ToggleButton>
        compressorBypassButton1,
        compressorBypassButton2,
        compressorBypassButton3,
        compressorBypassButton4,
        widthBypassButton1,
        widthBypassButton2,
        widthBypassButton3,
        widthBypassButton4;
        
    bool compBypassTemp[4] = {false};
    bool widthBypassTemp[4] = {false};
    
    // switches
    juce::TextButton
        oscSwitch,
        shapeSwitch,
        widthSwitch,
        compressorSwitch;
    
    // vectors for sliders
    juce::Array<juce::Component *> shapeVector;
    juce::Array<juce::Component *> widthVector;
    juce::Array<juce::Component *> compressorVector;
    juce::Array<juce::Component *> oscVector;
    
    
    juce::Array<juce::Component *> componentArray1;
    juce::Array<juce::Component *> componentArray2;
    juce::Array<juce::Component *> componentArray3;
    juce::Array<juce::Component *> componentArray4;
    juce::Array<juce::Component *> componentArray5;
    juce::Array<juce::Component *> componentArray6;
    juce::Array<juce::Component *> componentArray7;
    juce::Array<juce::Component *> componentArray8;
    
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
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compressorBypassAttachment1, compressorBypassAttachment2, compressorBypassAttachment3, compressorBypassAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> widthBypassAttachment1, widthBypassAttachment2, widthBypassAttachment3, widthBypassAttachment4;

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
    
    int focusBandNum;
    float scale = 1.0f;
    
    float tempDriveValue[4] = {1, 1, 1, 1};
    float tempBiasValue[4] = {0, 0, 0, 0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
