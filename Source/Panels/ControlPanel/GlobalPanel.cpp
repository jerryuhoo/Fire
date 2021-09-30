/*
  ==============================================================================

    GlobalPanel.cpp
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "GlobalPanel.h"

//==============================================================================
GlobalPanel::GlobalPanel(juce::AudioProcessorValueTreeState& apvts)
{
    setRotarySlider(mixKnob, COLOUR1);
    setRotarySlider(outputKnob, COLOUR1);
    
    addAndMakeVisible(outputLabelGlobal);
    outputLabelGlobal.setText("Output", juce::dontSendNotification);
    outputLabelGlobal.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabelGlobal.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabelGlobal.attachToComponent(&outputKnob, false);
    outputLabelGlobal.setJustificationType(juce::Justification::centred);
    
    outputKnob.setTextValueSuffix("db");
    
    
    
    
    addAndMakeVisible(mixLabelGlobal);
    mixLabelGlobal.setText("Mix", juce::dontSendNotification);
    mixLabelGlobal.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabelGlobal.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabelGlobal.attachToComponent(&mixKnob, false);
    mixLabelGlobal.setJustificationType(juce::Justification::centred);
    

    // downsample knob
    setRotarySlider(downSampleKnob, juce::Colours::purple.withBrightness(0.8f));
    
    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", juce::dontSendNotification);
    downSampleLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    downSampleLabel.setColour(juce::Label::textColourId, juce::Colours::purple.withBrightness(0.8f));
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(juce::Justification::centred);

    

    // lowcut knob
    setRotarySlider(lowcutFreqKnob, juce::Colours::hotpink.withBrightness(0.8f));
    
    addAndMakeVisible(lowcutFreqLabel);
    lowcutFreqLabel.setText("Frequency", juce::dontSendNotification);
    lowcutFreqLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    lowcutFreqLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    lowcutFreqLabel.attachToComponent(&lowcutFreqKnob, false);
    lowcutFreqLabel.setJustificationType(juce::Justification::centred);

    setRotarySlider(lowcutQKnob, juce::Colours::hotpink.withBrightness(0.8f));
    
    addAndMakeVisible(lowcutQLabel);
    lowcutQLabel.setText("Q", juce::dontSendNotification); // Resonance
    lowcutQLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    lowcutQLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    lowcutQLabel.attachToComponent(&lowcutQKnob, false);
    lowcutQLabel.setJustificationType(juce::Justification::centred);
    
    // highcut knob
    setRotarySlider(highcutFreqKnob, juce::Colours::hotpink.withBrightness(0.8f));
    
    addAndMakeVisible(highcutFreqLabel);
    highcutFreqLabel.setText("Frequency", juce::dontSendNotification);
    highcutFreqLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    highcutFreqLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    highcutFreqLabel.attachToComponent(&highcutFreqKnob, false);
    highcutFreqLabel.setJustificationType(juce::Justification::centred);

    setRotarySlider(highcutQKnob, juce::Colours::hotpink.withBrightness(0.8f));
    
    addAndMakeVisible(highcutQLabel);
    highcutQLabel.setText("Q", juce::dontSendNotification);
    highcutQLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    highcutQLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    highcutQLabel.attachToComponent(&highcutQKnob, false);
    highcutQLabel.setJustificationType(juce::Justification::centred);
    
    // peak knob
    setRotarySlider(peakFreqKnob, juce::Colours::hotpink.withBrightness(0.8f));
    
    addAndMakeVisible(peakFreqLabel);
    peakFreqLabel.setText("Frequency", juce::dontSendNotification);
    peakFreqLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    peakFreqLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    peakFreqLabel.attachToComponent(&peakFreqKnob, false);
    peakFreqLabel.setJustificationType(juce::Justification::centred);
    
    setRotarySlider(peakQKnob, juce::Colours::hotpink.withBrightness(0.8f));
    
    addAndMakeVisible(peakQLabel);
    peakQLabel.setText("Q", juce::dontSendNotification);
    peakQLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    peakQLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    peakQLabel.attachToComponent(&peakQKnob, false);
    peakQLabel.setJustificationType(juce::Justification::centred);
    
    setRotarySlider(peakGainKnob, juce::Colours::hotpink.withBrightness(0.8f));
    
    addAndMakeVisible(peakGainLabel);
    peakGainLabel.setText("Gain", juce::dontSendNotification);
    peakGainLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    peakGainLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    peakGainLabel.attachToComponent(&peakGainKnob, false);
    peakGainLabel.setJustificationType(juce::Justification::centred);
    
    
  
    // Filter State Buttons
    setRoundButton(filterOffButton, OFF_ID, "OFF");
    setRoundButton(filterPreButton, PRE_ID, "PRE");
    setRoundButton(filterPostButton, POST_ID, "POST");
    
    filterOffButton.setRadioGroupId(filterStateButtons);
    filterOffButton.onClick = [this] { updateToggleState(); };

    filterPreButton.setRadioGroupId(filterStateButtons);
    filterPreButton.onClick = [this] { updateToggleState(); };

    filterPostButton.setRadioGroupId(filterStateButtons);
    filterPostButton.onClick = [this] { updateToggleState(); };
    
    addAndMakeVisible(filterStateLabel);
    filterStateLabel.setText("Filter", juce::dontSendNotification);
    filterStateLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    filterStateLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    filterStateLabel.attachToComponent(&filterOffButton, false);
    filterStateLabel.setJustificationType(juce::Justification::centred);

    // Filter Type Toggle Buttons
    setRoundButton(filterLowPassButton, LOW_ID, "LP");
    setRoundButton(filterPeakButton, BAND_ID, "BP");
    setRoundButton(filterHighPassButton, HIGH_ID, "HP");
    
    filterLowPassButton.setRadioGroupId(filterModeButtons);
    filterLowPassButton.onClick = [this] { updateToggleState(); };
    
    filterPeakButton.setRadioGroupId(filterModeButtons);
    filterPeakButton.onClick = [this] { updateToggleState(); };

    filterHighPassButton.setRadioGroupId(filterModeButtons);
    filterHighPassButton.onClick = [this] { updateToggleState(); };
  
    addAndMakeVisible(filterTypeLabel);
    filterTypeLabel.setText("Type", juce::dontSendNotification);
    filterTypeLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    filterTypeLabel.setColour(juce::Label::textColourId, juce::Colours::hotpink.withBrightness(0.8f));
    filterTypeLabel.attachToComponent(&filterLowPassButton, false);
    filterTypeLabel.setJustificationType(juce::Justification::centred);
    
    
    // global switches
    addAndMakeVisible(filterSwitch);
    filterSwitch.setClickingTogglesState(true);
    filterSwitch.setRadioGroupId(switchButtonsGlobal);
    filterSwitch.setButtonText("");
    filterSwitch.setToggleState(true, juce::dontSendNotification);
    filterSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::hotpink.withBrightness(0.5f));
    filterSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::hotpink.withBrightness(0.9f));
    filterSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterSwitch.setLookAndFeel(&flatButtonLnf);
    
    addAndMakeVisible(otherSwitch);
    otherSwitch.setClickingTogglesState(true);
    otherSwitch.setRadioGroupId(switchButtonsGlobal);
    otherSwitch.setButtonText("");
    otherSwitch.setToggleState(false, juce::dontSendNotification);
    otherSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::purple.withBrightness(0.5f));
    otherSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::purple.withBrightness(0.9f));
    otherSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    otherSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    otherSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    otherSwitch.setLookAndFeel(&flatButtonLnf);
    
    // Attachment
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, OUTPUT_ID, outputKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, MIX_ID, mixKnob);
    
    downSampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, DOWNSAMPLE_ID, downSampleKnob);

    
    lowcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, LOWCUT_FREQ_ID, lowcutFreqKnob);
    lowcutQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, LOWCUT_Q_ID, lowcutQKnob);
    highcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, HIGHCUT_FREQ_ID, highcutFreqKnob);
    highcutQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, HIGHCUT_Q_ID, highcutQKnob);
    peakFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, PEAK_FREQ_ID, peakFreqKnob);
    peakQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, PEAK_Q_ID, peakQKnob);
    peakGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, PEAK_GAIN_ID, peakGainKnob);

    
    filterOffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, OFF_ID, filterOffButton);
    filterPreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, PRE_ID, filterPreButton);
    filterPostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, POST_ID, filterPostButton);
    filterLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, LOW_ID, filterLowPassButton);
    filterBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, BAND_ID, filterPeakButton);
    filterHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, HIGH_ID, filterHighPassButton);
    
    addAndMakeVisible(lowcutSlopeMode);
    addAndMakeVisible(highcutSlopeMode);
    
    lowcutSlopeMode.addItem("12 db", 1);
    lowcutSlopeMode.addItem("24 db", 2);
    lowcutSlopeMode.addItem("36 db", 3);
    lowcutSlopeMode.addItem("48 db", 4);
    
    highcutSlopeMode.addItem("12 db", 1);
    highcutSlopeMode.addItem("24 db", 2);
    highcutSlopeMode.addItem("36 db", 3);
    highcutSlopeMode.addItem("48 db", 4);
    
    lowcutModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, LOWCUT_SLOPE_ID, lowcutSlopeMode);
    highcutModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, HIGHCUT_SLOPE_ID, highcutSlopeMode);
}

GlobalPanel::~GlobalPanel()
{
    filterOffButton.setLookAndFeel(nullptr);
    filterPreButton.setLookAndFeel(nullptr);
    filterPostButton.setLookAndFeel(nullptr);
    filterLowPassButton.setLookAndFeel(nullptr);
    filterPeakButton.setLookAndFeel(nullptr);
    filterHighPassButton.setLookAndFeel(nullptr);
    filterSwitch.setLookAndFeel(nullptr);
    otherSwitch.setLookAndFeel(nullptr);
}

void GlobalPanel::paint (juce::Graphics& g)
{
    bool isFilterSwitchOn = filterSwitch.getToggleState();
    bool isOtherSwitchOn = otherSwitch.getToggleState();
    
    if (isFilterSwitchOn)
    {
        if (filterLowPassButton.getToggleState())
        {
            lowcutFreqKnob.setVisible(false);
            lowcutQKnob.setVisible(false);
            highcutFreqKnob.setVisible(true);
            highcutQKnob.setVisible(true);
            peakFreqKnob.setVisible(false);
            peakQKnob.setVisible(false);
            peakGainKnob.setVisible(false);
            lowcutSlopeMode.setVisible(false);
            highcutSlopeMode.setVisible(true);
        }
        if (filterHighPassButton.getToggleState())
        {
            lowcutFreqKnob.setVisible(true);
            lowcutQKnob.setVisible(true);
            highcutFreqKnob.setVisible(false);
            highcutQKnob.setVisible(false);
            peakFreqKnob.setVisible(false);
            peakQKnob.setVisible(false);
            peakGainKnob.setVisible(false);
            lowcutSlopeMode.setVisible(true);
            highcutSlopeMode.setVisible(false);
        }
        if (filterPeakButton.getToggleState())
        {
            lowcutFreqKnob.setVisible(false);
            lowcutQKnob.setVisible(false);
            highcutFreqKnob.setVisible(false);
            highcutQKnob.setVisible(false);
            peakFreqKnob.setVisible(true);
            peakQKnob.setVisible(true);
            peakGainKnob.setVisible(true);
            lowcutSlopeMode.setVisible(false);
            highcutSlopeMode.setVisible(false);
        }
        downSampleKnob.setVisible(false);
    }
    if (isOtherSwitchOn)
    {
        downSampleKnob.setVisible(true);
        lowcutFreqKnob.setVisible(false);
        lowcutQKnob.setVisible(false);
        highcutFreqKnob.setVisible(false);
        highcutQKnob.setVisible(false);
        peakFreqKnob.setVisible(false);
        peakQKnob.setVisible(false);
        peakGainKnob.setVisible(false);
        lowcutSlopeMode.setVisible(false);
        highcutSlopeMode.setVisible(false);
    }
    
    g.setColour(COLOUR6);
    g.drawRect(filterArea);
    g.drawRect(otherArea);
    g.drawRect(outputKnobArea);
}

void GlobalPanel::resized()
{
    juce::Rectangle<int> controlArea = getLocalBounds();
    
    
    filterArea = controlArea.removeFromLeft(getWidth() / 5 * 2);
    otherArea = controlArea.removeFromLeft(getWidth() / 5);
    outputKnobArea = controlArea;

    otherArea.removeFromTop(getHeight() / 5);
    otherArea.removeFromBottom(getHeight() / 5);
    outputKnobArea.removeFromTop(getHeight() / 5);
    outputKnobArea.removeFromBottom(getHeight() / 5);
    
    juce::Rectangle<int> switchArea = filterArea.removeFromLeft(getWidth() / 50);
    filterSwitch.setBounds(switchArea.removeFromTop(filterArea.getHeight() / 2));
    otherSwitch.setBounds(switchArea.removeFromTop(filterArea.getHeight() / 2));
    
    juce::Rectangle<int> filterKnobAreaLeft = filterArea;
    juce::Rectangle<int> filterKnobAreaMid = filterKnobAreaLeft.removeFromRight(filterArea.getWidth() / 3 * 2);
    juce::Rectangle<int> filterKnobAreaRight = filterKnobAreaMid.removeFromRight(filterArea.getWidth() / 3);
    filterKnobAreaLeft = filterKnobAreaLeft.reduced(getHeight() / 15, getHeight() / 5);
    filterKnobAreaMid = filterKnobAreaMid.reduced(getHeight() / 15, getHeight() / 5);
    filterKnobAreaRight = filterKnobAreaRight.reduced(getHeight() / 15, getHeight() / 5);
    
    downSampleKnob.setBounds(otherArea);
    lowcutFreqKnob.setBounds(filterKnobAreaLeft);
    lowcutQKnob.setBounds(filterKnobAreaRight);
    highcutFreqKnob.setBounds(filterKnobAreaLeft);
    highcutQKnob.setBounds(filterKnobAreaRight);
    peakFreqKnob.setBounds(filterKnobAreaLeft);
    peakQKnob.setBounds(filterKnobAreaRight);
    peakGainKnob.setBounds(filterKnobAreaMid);
   
    juce::Rectangle<int> filterModeArea = filterKnobAreaMid.removeFromBottom(getHeight() / 8);
    
    lowcutSlopeMode.setBounds(filterModeArea);
    highcutSlopeMode.setBounds(filterModeArea);

    juce::Rectangle<int> filterTypeArea = filterArea;
    filterTypeArea = filterTypeArea.removeFromBottom(getHeight() / 10);
    juce::Rectangle<int> filterTypeAreaLeft = filterTypeArea.removeFromLeft(filterArea.getWidth() / 3).reduced(filterArea.getWidth() / 15, 0);
    juce::Rectangle<int> filterTypeAreaMid = filterTypeArea.removeFromLeft(filterArea.getWidth() / 3).reduced(filterArea.getWidth() / 15, 0);
    juce::Rectangle<int> filterTypeAreaRight = filterTypeArea.removeFromLeft(filterArea.getWidth() / 3).reduced(filterArea.getWidth() / 15, 0);
    
//    filterOffButton.setBounds(FILTER_STATE_X, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
//    filterPreButton.setBounds(FILTER_STATE_X, secondPartY + 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
//    filterPostButton.setBounds(FILTER_STATE_X, secondPartY + 0.11 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    filterHighPassButton.setBounds(filterTypeAreaLeft);
    filterPeakButton.setBounds(filterTypeAreaMid);
    filterLowPassButton.setBounds(filterTypeAreaRight);
    
    juce::Rectangle<int> outputKnobAreaLeft = outputKnobArea;
    juce::Rectangle<int> outputKnobAreaRight = outputKnobAreaLeft.removeFromRight(outputKnobArea.getWidth() / 2);
    outputKnobAreaLeft = outputKnobAreaLeft;
    outputKnobAreaRight = outputKnobAreaRight;
    
    mixKnob.setBounds(outputKnobAreaLeft);
    outputKnob.setBounds(outputKnobAreaRight);
   
    
}

void GlobalPanel::sliderValueChanged(juce::Slider *slider)
{
}

void GlobalPanel::comboBoxChanged(juce::ComboBox *combobox)
{

}

void GlobalPanel::timerCallback()
{
}

void GlobalPanel::buttonClicked(juce::Button *clickedButton)
{
}

void GlobalPanel::setRotarySlider(juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
}

void GlobalPanel::setRoundButton(juce::TextButton& button, juce::String paramId, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    //bool state = *apvts.getRawParameterValue(paramId);
    //button.setToggleState(state, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR4);
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, COLOUR1);
    button.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
    
    if (&button == &filterLowPassButton) button.setLookAndFeel(&lowPassButtonLnf);
    else if (&button == &filterPeakButton) button.setLookAndFeel(&bandPassButtonLnf);
    else if (&button == &filterHighPassButton) button.setLookAndFeel(&highPassButtonLnf);
    else
    {
        button.setButtonText(buttonName);
        button.setLookAndFeel(&roundedButtonLnf);
    }
}

void GlobalPanel::updateToggleState()
{
//    if (*apvts.getRawParameterValue(OFF_ID))
//    {
//        filterLowPassButton.setEnabled(false);
//        filterPeakButton.setEnabled(false);
//        filterHighPassButton.setEnabled(false);
//        lowcutFreqKnob.setEnabled(false);
//        lowcutQKnob.setEnabled(false);
//    }
//    else
//    {
//        filterLowPassButton.setEnabled(true);
//        filterPeakButton.setEnabled(true);
//        filterHighPassButton.setEnabled(true);
//        lowcutFreqKnob.setEnabled(true);
//        lowcutQKnob.setEnabled(true);
//    }
}