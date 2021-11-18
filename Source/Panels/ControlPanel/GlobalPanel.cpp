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
    setRotarySlider(downSampleKnob, DOWNSAMPLE_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", juce::dontSendNotification);
    downSampleLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    downSampleLabel.setColour(juce::Label::textColourId, DOWNSAMPLE_COLOUR.withBrightness(0.8f));
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(juce::Justification::centred);

    filterBypassButton = std::make_unique<juce::ToggleButton>();
    downsampleBypassButton = std::make_unique<juce::ToggleButton>();

    addAndMakeVisible(*filterBypassButton);
    filterBypassButton->setColour(juce::ToggleButton::tickColourId, FILTER_COLOUR);
    filterBypassButton->onClick = [this] { updateBypassState (*filterBypassButton, 0); };
    addAndMakeVisible(*downsampleBypassButton);
    downsampleBypassButton->setColour(juce::ToggleButton::tickColourId, DOWNSAMPLE_COLOUR);
    downsampleBypassButton->onClick = [this] { updateBypassState (*downsampleBypassButton, 1); };
    
    
    // lowcut freq knob
    setRotarySlider(lowcutFreqKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(lowcutFreqLabel);
    lowcutFreqLabel.setText("Frequency", juce::dontSendNotification);
    lowcutFreqLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    lowcutFreqLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    lowcutFreqLabel.attachToComponent(&lowcutFreqKnob, false);
    lowcutFreqLabel.setJustificationType(juce::Justification::centred);
    
    // lowcut gain knob
    setRotarySlider(lowcutGainKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(lowcutGainLabel);
    lowcutGainLabel.setText("Gain", juce::dontSendNotification);
    lowcutGainLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    lowcutGainLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    lowcutGainLabel.attachToComponent(&lowcutGainKnob, false);
    lowcutGainLabel.setJustificationType(juce::Justification::centred);

    // lowcut q knob
    setRotarySlider(lowcutQKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(lowcutQLabel);
    lowcutQLabel.setText("Q", juce::dontSendNotification); // Resonance
    lowcutQLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    lowcutQLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    lowcutQLabel.attachToComponent(&lowcutQKnob, false);
    lowcutQLabel.setJustificationType(juce::Justification::centred);
    
    // highcut freq knob
    setRotarySlider(highcutFreqKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(highcutFreqLabel);
    highcutFreqLabel.setText("Frequency", juce::dontSendNotification);
    highcutFreqLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    highcutFreqLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    highcutFreqLabel.attachToComponent(&highcutFreqKnob, false);
    highcutFreqLabel.setJustificationType(juce::Justification::centred);
    
    // highcut gain knob
    setRotarySlider(highcutGainKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(highcutGainLabel);
    highcutGainLabel.setText("Gain", juce::dontSendNotification);
    highcutGainLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    highcutGainLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    highcutGainLabel.attachToComponent(&highcutGainKnob, false);
    highcutGainLabel.setJustificationType(juce::Justification::centred);

    // highcut q knob
    setRotarySlider(highcutQKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(highcutQLabel);
    highcutQLabel.setText("Q", juce::dontSendNotification);
    highcutQLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    highcutQLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    highcutQLabel.attachToComponent(&highcutQKnob, false);
    highcutQLabel.setJustificationType(juce::Justification::centred);
    
    // peak knob
    setRotarySlider(peakFreqKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(peakFreqLabel);
    peakFreqLabel.setText("Frequency", juce::dontSendNotification);
    peakFreqLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    peakFreqLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    peakFreqLabel.attachToComponent(&peakFreqKnob, false);
    peakFreqLabel.setJustificationType(juce::Justification::centred);
    
    setRotarySlider(peakQKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(peakQLabel);
    peakQLabel.setText("Q", juce::dontSendNotification);
    peakQLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    peakQLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    peakQLabel.attachToComponent(&peakQKnob, false);
    peakQLabel.setJustificationType(juce::Justification::centred);
    
    setRotarySlider(peakGainKnob, FILTER_COLOUR.withBrightness(0.8f));
    
    addAndMakeVisible(peakGainLabel);
    peakGainLabel.setText("Gain", juce::dontSendNotification);
    peakGainLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    peakGainLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
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
    filterTypeLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    filterTypeLabel.attachToComponent(&filterHighPassButton, false);
    filterTypeLabel.setJustificationType(juce::Justification::centred);

    // global switches
    addAndMakeVisible(filterSwitch);
    filterSwitch.setClickingTogglesState(true);
    filterSwitch.setRadioGroupId(switchButtonsGlobal);
    filterSwitch.setButtonText("");
    filterSwitch.setToggleState(true, juce::dontSendNotification);
    filterSwitch.setColour(juce::TextButton::buttonColourId, FILTER_COLOUR.withBrightness(0.5f));
    filterSwitch.setColour(juce::TextButton::buttonOnColourId, FILTER_COLOUR.withBrightness(0.9f));
    filterSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterSwitch.setLookAndFeel(&flatButtonLnf);
    
    addAndMakeVisible(otherSwitch);
    otherSwitch.setClickingTogglesState(true);
    otherSwitch.setRadioGroupId(switchButtonsGlobal);
    otherSwitch.setButtonText("");
    otherSwitch.setToggleState(false, juce::dontSendNotification);
    otherSwitch.setColour(juce::TextButton::buttonColourId, DOWNSAMPLE_COLOUR.withBrightness(0.5f));
    otherSwitch.setColour(juce::TextButton::buttonOnColourId, DOWNSAMPLE_COLOUR.withBrightness(0.9f));
    otherSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    otherSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    otherSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    otherSwitch.setLookAndFeel(&flatButtonLnf);
    
    // init state
    setBypassState(0, filterBypassButton->getToggleState());
    setBypassState(1, downsampleBypassButton->getToggleState());

    // Attachment
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, OUTPUT_ID, outputKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, MIX_ID, mixKnob);
    
    downSampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, DOWNSAMPLE_ID, downSampleKnob);

    lowcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, LOWCUT_FREQ_ID, lowcutFreqKnob);
    lowcutGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, LOWCUT_GAIN_ID, lowcutGainKnob);
    lowcutQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, LOWCUT_Q_ID, lowcutQKnob);
    highcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, HIGHCUT_FREQ_ID, highcutFreqKnob);
    highcutGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, HIGHCUT_GAIN_ID, highcutGainKnob);
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
    
    addAndMakeVisible(lowcutSlopeLabel);
    lowcutSlopeLabel.setText("Slope", juce::dontSendNotification);
    lowcutSlopeLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    lowcutSlopeLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    lowcutSlopeLabel.attachToComponent(&lowcutSlopeMode, true);
    lowcutSlopeLabel.setJustificationType(juce::Justification::left);
    
    addAndMakeVisible(highcutSlopeLabel);
    highcutSlopeLabel.setText("Slope", juce::dontSendNotification);
    highcutSlopeLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    highcutSlopeLabel.setColour(juce::Label::textColourId, FILTER_COLOUR.withBrightness(0.8f));
    highcutSlopeLabel.attachToComponent(&highcutSlopeMode, true);
    highcutSlopeLabel.setJustificationType(juce::Justification::left);
    
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
    
    filterBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, FILTER_BYPASS_ID, *filterBypassButton);
    downsampleBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, DOWNSAMPLE_BYPASS_ID, *downsampleBypassButton);
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
        filterBypassButton->setVisible(true);
        filterHighPassButton.setVisible(true);
        filterPeakButton.setVisible(true);
        filterLowPassButton.setVisible(true);
        if (filterLowPassButton.getToggleState())
        {
            lowcutFreqKnob.setVisible(false);
            lowcutQKnob.setVisible(false);
            lowcutGainKnob.setVisible(false);
            highcutFreqKnob.setVisible(true);
            highcutQKnob.setVisible(true);
            highcutGainKnob.setVisible(true);
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
            lowcutGainKnob.setVisible(true);
            highcutFreqKnob.setVisible(false);
            highcutQKnob.setVisible(false);
            highcutGainKnob.setVisible(false);
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
            lowcutGainKnob.setVisible(false);
            highcutFreqKnob.setVisible(false);
            highcutQKnob.setVisible(false);
            highcutGainKnob.setVisible(false);
            peakFreqKnob.setVisible(true);
            peakQKnob.setVisible(true);
            peakGainKnob.setVisible(true);
            lowcutSlopeMode.setVisible(false);
            highcutSlopeMode.setVisible(false);
        }
        downSampleKnob.setVisible(false);
        downsampleBypassButton->setVisible(false);
    }
    if (isOtherSwitchOn)
    {
        filterBypassButton->setVisible(false);
        filterHighPassButton.setVisible(false);
        filterPeakButton.setVisible(false);
        filterLowPassButton.setVisible(false);
        
        downsampleBypassButton->setVisible(true);
        downSampleKnob.setVisible(true);
        lowcutFreqKnob.setVisible(false);
        lowcutQKnob.setVisible(false);
        lowcutGainKnob.setVisible(false);
        highcutFreqKnob.setVisible(false);
        highcutQKnob.setVisible(false);
        highcutGainKnob.setVisible(false);
        peakFreqKnob.setVisible(false);
        peakQKnob.setVisible(false);
        peakGainKnob.setVisible(false);
        lowcutSlopeMode.setVisible(false);
        highcutSlopeMode.setVisible(false);
    }
    
    g.setColour(COLOUR6);
    g.drawRect(globalEffectArea);
    g.drawRect(outputKnobArea);
}

void GlobalPanel::resized()
{
    juce::Rectangle<int> controlArea = getLocalBounds();
    
    
    globalEffectArea = controlArea.removeFromLeft(getWidth() / 5 * 3);
    outputKnobArea = controlArea;
    
    juce::Rectangle<int> switchArea = globalEffectArea.removeFromLeft(getWidth() / 50);
    filterSwitch.setBounds(switchArea.removeFromTop(globalEffectArea.getHeight() / 2));
    otherSwitch.setBounds(switchArea.removeFromTop(globalEffectArea.getHeight() / 2));
    
    juce::Rectangle<int> filterKnobArea = globalEffectArea;
    juce::Rectangle<int> filterTypeArea = filterKnobArea.removeFromLeft(globalEffectArea.getWidth() / 4);
    
    juce::Rectangle<int> filterTypeAreaReduced = filterTypeArea.reduced(0,  getHeight() / 6);
    juce::Rectangle<int> filterTypeAreaReducedRest = filterTypeAreaReduced;
    
    juce::Rectangle<int> filterTypeAreaTop = filterTypeAreaReducedRest.removeFromTop(filterTypeAreaReduced.getHeight() / 3).reduced(globalEffectArea.getWidth() / 20, getHeight() / 30);
    juce::Rectangle<int> filterTypeAreaMid = filterTypeAreaReducedRest.removeFromTop(filterTypeAreaReduced.getHeight() / 3).reduced(globalEffectArea.getWidth() / 20, getHeight() / 30);
    juce::Rectangle<int> filterTypeAreaButtom = filterTypeAreaReducedRest.removeFromTop(filterTypeAreaReduced.getHeight() / 3).reduced(globalEffectArea.getWidth() / 20, getHeight() / 30);
//    juce::Rectangle<int> filterTypeAreaTop = juce::Rectangle<int>(filterTypeAreaReduced.getX(), filterTypeAreaReduced.getY(), outputKnob.getTextBoxWidth(), outputKnob.getTextBoxHeight());
//    juce::Rectangle<int> filterTypeAreaMid = juce::Rectangle<int>(filterTypeAreaReduced.getX(), filterTypeAreaReduced.getY() + getHeight() / 4, outputKnob.getTextBoxWidth(), outputKnob.getTextBoxHeight());
//    juce::Rectangle<int> filterTypeAreaButtom = juce::Rectangle<int>(filterTypeAreaReduced.getX(), filterTypeAreaReduced.getY() + getHeight() / 4 * 2, outputKnob.getTextBoxWidth(), outputKnob.getTextBoxHeight());
    
    juce::Rectangle<int> bypassButtonArea = globalEffectArea;
    bypassButtonArea = bypassButtonArea.removeFromBottom(globalEffectArea.getHeight() / 5).reduced(globalEffectArea.getWidth() / 2 - globalEffectArea.getHeight() / 10, 0);
    filterBypassButton->setBounds(bypassButtonArea);
    downsampleBypassButton->setBounds(bypassButtonArea);
    
    juce::Rectangle<int> filterKnobAreaLeft = filterKnobArea.removeFromLeft(filterKnobArea.getWidth() / 3);
    juce::Rectangle<int> filterKnobAreaMid = filterKnobArea.removeFromLeft(filterKnobArea.getWidth() / 2);
    juce::Rectangle<int> filterKnobAreaRight = filterKnobArea;
    juce::Rectangle<int> filterMenuArea = filterKnobAreaMid;
    filterKnobAreaLeft = filterKnobAreaLeft.reduced(0, getHeight() / 5);
    filterKnobAreaMid = filterKnobAreaMid.reduced(0, getHeight() / 5);
    filterKnobAreaRight = filterKnobAreaRight.reduced(0, getHeight() / 5);
    
    downSampleKnob.setBounds(globalEffectArea.reduced(getHeight() / 15, getHeight() / 5));
    lowcutFreqKnob.setBounds(filterKnobAreaLeft);
    lowcutGainKnob.setBounds(filterKnobAreaMid);
    lowcutQKnob.setBounds(filterKnobAreaRight);
    highcutFreqKnob.setBounds(filterKnobAreaLeft);
    highcutGainKnob.setBounds(filterKnobAreaMid);
    highcutQKnob.setBounds(filterKnobAreaRight);
    peakFreqKnob.setBounds(filterKnobAreaLeft);
    peakQKnob.setBounds(filterKnobAreaRight);
    peakGainKnob.setBounds(filterKnobAreaMid);
   
    juce::Rectangle<int> filterModeArea = filterMenuArea.removeFromBottom(getHeight() / 4);//filterKnobAreaMid.removeFromTop(getHeight() / 4);
    filterModeArea.removeFromBottom(getHeight() / 15);

    filterModeArea.setX(filterModeArea.getX() + filterModeArea.getWidth() / 1.3);

    lowcutSlopeMode.setBounds(filterModeArea);
    highcutSlopeMode.setBounds(filterModeArea);

    filterHighPassButton.setBounds(filterTypeAreaTop);
    filterPeakButton.setBounds(filterTypeAreaMid);
    filterLowPassButton.setBounds(filterTypeAreaButtom);
    
    juce::Rectangle<int> outputKnobAreaLeft = outputKnobArea;
    juce::Rectangle<int> outputKnobAreaRight = outputKnobAreaLeft.removeFromRight(outputKnobArea.getWidth() / 2);
    outputKnobAreaLeft = outputKnobAreaLeft.reduced(getHeight() / 15, getHeight() / 5).reduced(outputKnobArea.getWidth() / 15, 0);
    outputKnobAreaRight = outputKnobAreaRight.reduced(getHeight() / 15, getHeight() / 5).reduced(outputKnobArea.getWidth() / 15, 0);
    
    outputKnob.setBounds(outputKnobAreaLeft);
    mixKnob.setBounds(outputKnobAreaRight);
    
    lowPassButtonLnf.scale = scale;
    bandPassButtonLnf.scale = scale;
    highPassButtonLnf.scale = scale;
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
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, FILTER_COLOUR.withBrightness(0.8f));
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

void GlobalPanel::setScale(float scale)
{
    this->scale = scale;
}

juce::Slider& GlobalPanel::getLowcutFreqKnob()
{
    return lowcutFreqKnob;
}

juce::Slider& GlobalPanel::getPeakFreqKnob()
{
    return peakFreqKnob;
}

juce::Slider& GlobalPanel::getHighcutFreqKnob()
{
    return highcutFreqKnob;
}

juce::Slider& GlobalPanel::getLowcutGainKnob()
{
    return lowcutGainKnob;
}

juce::Slider& GlobalPanel::getPeakGainKnob()
{
    return peakGainKnob;
}

juce::Slider& GlobalPanel::getHighcutGainKnob()
{
    return highcutGainKnob;
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

void GlobalPanel::updateBypassState(juce::ToggleButton &clickedButton, int index)
{
    if (clickedButton.getToggleState())
    {
        setBypassState(index, true);
    }
    else
    {
        setBypassState(index, false);
    }
}

void GlobalPanel::setToggleButtonState(juce::String toggleButton)
{
    if (toggleButton == "lowcut")
        filterHighPassButton.setToggleState(true, juce::NotificationType::dontSendNotification);
    if (toggleButton == "peak")
        filterPeakButton.setToggleState(true, juce::NotificationType::dontSendNotification);
    if (toggleButton == "highcut")
        filterLowPassButton.setToggleState(true, juce::NotificationType::dontSendNotification);
}

void GlobalPanel::setBypassState(int index, bool state)
{
    componentArray1 = { &filterLowPassButton, &filterPeakButton, &filterHighPassButton, &highcutFreqKnob, &peakFreqKnob, &lowcutFreqKnob, &highcutQKnob, &highcutGainKnob, &lowcutQKnob, &lowcutGainKnob, &highcutSlopeMode, &lowcutSlopeMode, &peakGainKnob, &peakQKnob };
    componentArray2 = { &downSampleKnob };

    juce::Array<juce::Component*>* componentArray;
    if (index == 0) componentArray = &componentArray1;
    if (index == 1) componentArray = &componentArray2;

    if (state)
    {
        for (int i = 0; i < componentArray->size(); i++)
        {
            componentArray->data()[i]->setEnabled(true);
        }
    }
    else
    {
        for (int i = 0; i < componentArray->size(); i++)
        {
            componentArray->data()[i]->setEnabled(false);
        }
    }
}
