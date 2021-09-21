/*
  ==============================================================================

    GlobalPanel.h
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class GlobalPanel  : public juce::Component,
                     public juce::Slider::Listener,
                     public juce::ComboBox::Listener,
                     public juce::Timer,
                     public juce::Button::Listener
{
public:
    GlobalPanel(juce::AudioProcessorValueTreeState& apvts);
    ~GlobalPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    
private:
    // override listener functions
    // linked
    void sliderValueChanged(juce::Slider *slider) override;
    // combobox changed and set knob enable/disable
    void comboBoxChanged(juce::ComboBox *combobox) override;
    void buttonClicked(juce::Button *clickedButton) override;
    void setRotarySlider(juce::Slider& slider, juce::Colour colour);
    void setRoundButton(juce::TextButton& button, juce::String paramId, juce::String buttonName);
    void updateToggleState();
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
    
    juce::Rectangle<int> filterArea;
    juce::Rectangle<int> otherArea;
    juce::Rectangle<int> outputKnobArea;
    
    // Sliders
    juce::Slider
        downSampleKnob,
        lowcutFreqKnob,
        lowcutQKnob,
        highcutFreqKnob,
        highcutQKnob,
        peakFreqKnob,
        peakQKnob,
        peakGainKnob,
        mixKnob,
        outputKnob;
    
    // Labels
    juce::Label
        downSampleLabel,
        outputLabelGlobal,
        mixLabelGlobal,
        lowcutFreqLabel,
        lowcutQLabel,
        highcutFreqLabel,
        highcutQLabel,
        peakFreqLabel,
        peakQLabel,
        peakGainLabel,
        filterStateLabel,
        filterTypeLabel;

    // Buttons
    juce::TextButton
        filterOffButton,
        filterPreButton,
        filterPostButton,
        filterLowPassButton,
        filterPeakButton,
        filterHighPassButton;

    juce::TextButton filterSwitch, otherSwitch;
    
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
    
    // Slider attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        outputAttachment,
        mixAttachment,
        downSampleAttachment,
        lowcutFreqAttachment,
        lowcutQAttachment,
        highcutFreqAttachment,
        highcutQAttachment,
        peakFreqAttachment,
        peakQAttachment,
        peakGainAttachment;

    // Button attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        filterOffAttachment,
        filterPreAttachment,
        filterPostAttachment,
        filterLowAttachment,
        filterBandAttachment,
        filterHighAttachment;
    
    // ComboBox attachment

    juce::ComboBox lowcutSlopeMode;
    juce::ComboBox highcutSlopeMode;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lowcutModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> highcutModeAttachment;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalPanel)
};
