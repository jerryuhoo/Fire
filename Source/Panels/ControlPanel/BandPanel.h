/*
  ==============================================================================

    BandPanel.h
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "../../PluginProcessor.h"
#include "juce_gui_basics/juce_gui_basics.h"

//==============================================================================
/*
*/
class BandPanel : public juce::Component,
                  public juce::AudioProcessorValueTreeState::Listener,
                  public juce::ComboBox::Listener,
                  public juce::AsyncUpdater,
                  public juce::Button::Listener
{
public:
    BandPanel(FireAudioProcessor&);
    ~BandPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setFocusBandNum(int num);
    void setScale(float scale);

    void handleAsyncUpdate() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    
    //    void changeSliderState(int bandNum, bool isPresetChanged);
    void setBandKnobsStates(int index, bool state, bool callFromSubBypass);
    juce::ToggleButton& getCompButton(const int index);
    juce::ToggleButton& getWidthButton(const int index);
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();

private:
    FireAudioProcessor& processor;
    juce::Rectangle<int> bandKnobArea;
    juce::Rectangle<int> driveKnobArea;
    juce::Rectangle<int> outputKnobArea;

    void updateLinkedValue(int bandIndex);

    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox* combobox) override;
    void buttonClicked(juce::Button* clickedButton) override;
    // void initListenerKnob(juce::Slider& slider);
    void initRotarySlider(juce::Slider& slider, juce::Colour colour);
    void initFlatButton(juce::TextButton& button, juce::String paramId, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour, int index);
    void setFourComponentsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum);
    void linkValue(juce::Slider& xSlider, juce::Slider& driveSlider, juce::Slider& outputSlider, juce::TextButton& linkedButton);
    void setVisibility(juce::Array<juce::Component*>& array, bool isVisible);
    void updateBypassState(juce::ToggleButton& clickedButton, int index);
    void setBypassState(int index, bool state);
    bool canEnableSubKnobs(juce::Component& component);

    enum RadioButtonIds
    {
        // switches band
        switchButtons = 1004,
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
        mixKnob4,

        recKnob1,
        recKnob2,
        recKnob3,
        recKnob4,

        biasKnob1,
        biasKnob2,
        biasKnob3,
        biasKnob4,

        compRatioKnob1,
        compRatioKnob2,
        compRatioKnob3,
        compRatioKnob4,

        compThreshKnob1,
        compThreshKnob2,
        compThreshKnob3,
        compThreshKnob4,

        widthKnob1,
        widthKnob2,
        widthKnob3,
        widthKnob4;

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
        biasLabel,
        shapePanelLabel,
        compressorPanelLabel,
        widthPanelLabel;

    juce::TextButton
        linkedButton1,
        linkedButton2,
        linkedButton3,
        linkedButton4,
        safeButton1,
        safeButton2,
        safeButton3,
        safeButton4,
        extremeButton1,
        extremeButton2,
        extremeButton3,
        extremeButton4;

    std::unique_ptr<juce::ToggleButton>
        compressorBypassButton1,
        compressorBypassButton2,
        compressorBypassButton3,
        compressorBypassButton4,
        widthBypassButton1,
        widthBypassButton2,
        widthBypassButton3,
        widthBypassButton4;

    bool compBypassTemp[4] = { false };
    bool widthBypassTemp[4] = { false };

    // switches
    juce::ToggleButton
        oscSwitch,
        shapeSwitch,
        widthSwitch,
        compressorSwitch;

    // vectors for sliders
    juce::Array<juce::Component*>
        shapeVector,
        widthVector,
        compressorVector,
        oscVector,

        componentArray1,
        componentArray2,
        componentArray3,
        componentArray4,
        componentArray5,
        componentArray6,
        componentArray7,
        componentArray8;

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
        safeAttachment4,
        extremeAttachment1,
        extremeAttachment2,
        extremeAttachment3,
        extremeAttachment4;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        compressorBypassAttachment1,
        compressorBypassAttachment2,
        compressorBypassAttachment3,
        compressorBypassAttachment4,
        widthBypassAttachment1,
        widthBypassAttachment2,
        widthBypassAttachment3,
        widthBypassAttachment4;

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
    FlatLnf flatLnf;

    int focusBandNum;
    float scale = 1.0f;

    float tempDriveValue[4] = { 1, 1, 1, 1 };
    float tempBiasValue[4] = { 0, 0, 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPanel)
};
