/*
  ==============================================================================

    GlobalPanel.cpp
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "GlobalPanel.h"

//==============================================================================
GlobalPanel::GlobalPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // init vec
    filterVector = { &filterLowPassButton, &filterPeakButton, &filterHighPassButton, &highcutFreqKnob, &peakFreqKnob, &lowcutFreqKnob, &highcutQKnob, &highcutGainKnob, &lowcutQKnob, &lowcutGainKnob, &highcutSlopeMode, &lowcutSlopeMode, &peakGainKnob, &peakQKnob, &postFilterPanelLabel };
    downsampleVector = { &downSampleKnob, &downSamplePanelLabel };
//    limiterVector = { &limiterThreshKnob, &limiterReleaseKnob, &limiterPanelLabel };
    
    // init panel labels
    addAndMakeVisible (postFilterPanelLabel);
    postFilterPanelLabel.setLookAndFeel(&flatLnf);
    postFilterPanelLabel.setText ("Post Filter", juce::dontSendNotification);
    postFilterPanelLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    postFilterPanelLabel.setColour (juce::Label::textColourId, FILTER_COLOUR);
    
    addAndMakeVisible (downSamplePanelLabel);
    downSamplePanelLabel.setLookAndFeel(&flatLnf);
    downSamplePanelLabel.setText ("DownSample", juce::dontSendNotification);
    downSamplePanelLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    downSamplePanelLabel.setColour (juce::Label::textColourId, DOWNSAMPLE_COLOUR);
    
//    addAndMakeVisible (limiterPanelLabel);
//    limiterPanelLabel.setLookAndFeel(&flatLnf);
//    limiterPanelLabel.setText ("Limiter", juce::dontSendNotification);
//    limiterPanelLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
//    limiterPanelLabel.setColour (juce::Label::textColourId, LIMITER_COLOUR);
    
    setRotarySlider (mixKnob, COLOUR1);
    setRotarySlider (outputKnob, COLOUR1);

    addAndMakeVisible (outputLabelGlobal);
    outputLabelGlobal.setText ("Output", juce::dontSendNotification);
    outputLabelGlobal.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    outputLabelGlobal.setColour (juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabelGlobal.attachToComponent (&outputKnob, false);
    outputLabelGlobal.setJustificationType (juce::Justification::centred);

    outputKnob.setTextValueSuffix ("db");

    addAndMakeVisible (mixLabelGlobal);
    mixLabelGlobal.setText ("Mix", juce::dontSendNotification);
    mixLabelGlobal.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    mixLabelGlobal.setColour (juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabelGlobal.attachToComponent (&mixKnob, false);
    mixLabelGlobal.setJustificationType (juce::Justification::centred);

    // downsample knob
    setRotarySlider (downSampleKnob, DOWNSAMPLE_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (downSampleLabel);
    downSampleLabel.setText ("Downsample", juce::dontSendNotification);
    downSampleLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    downSampleLabel.setColour (juce::Label::textColourId, DOWNSAMPLE_COLOUR.withBrightness (0.8f));
    downSampleLabel.attachToComponent (&downSampleKnob, false);
    downSampleLabel.setJustificationType (juce::Justification::centred);
    
    // limiter knobs
    
//    setRotarySlider (limiterThreshKnob, LIMITER_COLOUR.withBrightness (0.8f));
//    setRotarySlider (limiterReleaseKnob, LIMITER_COLOUR.withBrightness (0.8f));
//    limiterThreshKnob.setTextValueSuffix("db");

//    addAndMakeVisible (limiterThreshLabel);
//    limiterThreshLabel.setText ("Threshold", juce::dontSendNotification);
//    limiterThreshLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
//    limiterThreshLabel.setColour (juce::Label::textColourId, LIMITER_COLOUR.withBrightness (0.8f));
//    limiterThreshLabel.attachToComponent (&limiterThreshKnob, false);
//    limiterThreshLabel.setJustificationType (juce::Justification::centred);
//    
//    addAndMakeVisible (limiterReleaseLabel);
//    limiterReleaseLabel.setText ("Release", juce::dontSendNotification);
//    limiterReleaseLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
//    limiterReleaseLabel.setColour (juce::Label::textColourId, LIMITER_COLOUR.withBrightness (0.8f));
//    limiterReleaseLabel.attachToComponent (&limiterReleaseKnob, false);
//    limiterReleaseLabel.setJustificationType (juce::Justification::centred);

    filterBypassButton = std::make_unique<juce::ToggleButton>();
    downsampleBypassButton = std::make_unique<juce::ToggleButton>();
//    limiterBypassButton = std::make_unique<juce::ToggleButton>();

    addAndMakeVisible (*filterBypassButton);
    filterBypassButton->setColour (juce::ToggleButton::tickColourId, FILTER_COLOUR);
    filterBypassButton->onClick = [this]
    { setBypassState (0, filterBypassButton->getToggleState()); };
    
    addAndMakeVisible (*downsampleBypassButton);
    downsampleBypassButton->setColour (juce::ToggleButton::tickColourId, DOWNSAMPLE_COLOUR);
    downsampleBypassButton->onClick = [this]
    { setBypassState (1, downsampleBypassButton->getToggleState()); };
    
//    addAndMakeVisible (*limiterBypassButton);
//    limiterBypassButton->setColour (juce::ToggleButton::tickColourId, LIMITER_COLOUR);
//    limiterBypassButton->onClick = [this]
//    { setBypassState (2, limiterBypassButton->getToggleState()); };

    // lowcut freq knob
    setRotarySlider (lowcutFreqKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (lowcutFreqLabel);
    lowcutFreqLabel.setText ("Frequency", juce::dontSendNotification);
    lowcutFreqLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    lowcutFreqLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    lowcutFreqLabel.attachToComponent (&lowcutFreqKnob, false);
    lowcutFreqLabel.setJustificationType (juce::Justification::centred);

    // lowcut gain knob
    setRotarySlider (lowcutGainKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (lowcutGainLabel);
    lowcutGainLabel.setText ("Gain", juce::dontSendNotification);
    lowcutGainLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    lowcutGainLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    lowcutGainLabel.attachToComponent (&lowcutGainKnob, false);
    lowcutGainLabel.setJustificationType (juce::Justification::centred);

    // lowcut q knob
    setRotarySlider (lowcutQKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (lowcutQLabel);
    lowcutQLabel.setText ("Q", juce::dontSendNotification); // Resonance
    lowcutQLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    lowcutQLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    lowcutQLabel.attachToComponent (&lowcutQKnob, false);
    lowcutQLabel.setJustificationType (juce::Justification::centred);

    // highcut freq knob
    setRotarySlider (highcutFreqKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (highcutFreqLabel);
    highcutFreqLabel.setText ("Frequency", juce::dontSendNotification);
    highcutFreqLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    highcutFreqLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    highcutFreqLabel.attachToComponent (&highcutFreqKnob, false);
    highcutFreqLabel.setJustificationType (juce::Justification::centred);

    // highcut gain knob
    setRotarySlider (highcutGainKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (highcutGainLabel);
    highcutGainLabel.setText ("Gain", juce::dontSendNotification);
    highcutGainLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    highcutGainLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    highcutGainLabel.attachToComponent (&highcutGainKnob, false);
    highcutGainLabel.setJustificationType (juce::Justification::centred);

    // highcut q knob
    setRotarySlider (highcutQKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (highcutQLabel);
    highcutQLabel.setText ("Q", juce::dontSendNotification);
    highcutQLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    highcutQLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    highcutQLabel.attachToComponent (&highcutQKnob, false);
    highcutQLabel.setJustificationType (juce::Justification::centred);

    // peak knob
    setRotarySlider (peakFreqKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (peakFreqLabel);
    peakFreqLabel.setText ("Frequency", juce::dontSendNotification);
    peakFreqLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    peakFreqLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    peakFreqLabel.attachToComponent (&peakFreqKnob, false);
    peakFreqLabel.setJustificationType (juce::Justification::centred);

    setRotarySlider (peakQKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (peakQLabel);
    peakQLabel.setText ("Q", juce::dontSendNotification);
    peakQLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    peakQLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    peakQLabel.attachToComponent (&peakQKnob, false);
    peakQLabel.setJustificationType (juce::Justification::centred);

    setRotarySlider (peakGainKnob, FILTER_COLOUR.withBrightness (0.8f));

    addAndMakeVisible (peakGainLabel);
    peakGainLabel.setText ("Gain", juce::dontSendNotification);
    peakGainLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    peakGainLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    peakGainLabel.attachToComponent (&peakGainKnob, false);
    peakGainLabel.setJustificationType (juce::Justification::centred);

    // Filter State Buttons
//    setRoundButton (filterOffButton, OFF_ID, "OFF");
//    setRoundButton (filterPreButton, PRE_ID, "PRE");
//    setRoundButton (filterPostButton, POST_ID, "POST");

//    filterOffButton.setRadioGroupId (filterStateButtons);
//    filterOffButton.onClick = [this]
//    { updateToggleState(); };
//
//    filterPreButton.setRadioGroupId (filterStateButtons);
//    filterPreButton.onClick = [this]
//    { updateToggleState(); };
//
//    filterPostButton.setRadioGroupId (filterStateButtons);
//    filterPostButton.onClick = [this]
//    { updateToggleState(); };

//    addAndMakeVisible (filterStateLabel);
//    filterStateLabel.setText ("Filter", juce::dontSendNotification);
//    filterStateLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
//    filterStateLabel.setColour (juce::Label::textColourId, KNOB_FONT_COLOUR);
//    filterStateLabel.attachToComponent (&filterOffButton, false);
//    filterStateLabel.setJustificationType (juce::Justification::centred);

    // Filter Type Toggle Buttons
    setRoundButton (filterLowPassButton, LOW_ID, "LP");
    setRoundButton (filterPeakButton, BAND_ID, "BP");
    setRoundButton (filterHighPassButton, HIGH_ID, "HP");

    filterLowPassButton.setRadioGroupId (filterModeButtons);
    filterPeakButton.setRadioGroupId (filterModeButtons);
    filterHighPassButton.setRadioGroupId (filterModeButtons);

    addAndMakeVisible (filterTypeLabel);
    filterTypeLabel.setText ("Type", juce::dontSendNotification);
    filterTypeLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    filterTypeLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    filterTypeLabel.attachToComponent (&filterHighPassButton, false);
    filterTypeLabel.setJustificationType (juce::Justification::centred);
    
    addAndMakeVisible (lowcutSlopeMode);
    addAndMakeVisible (highcutSlopeMode);

    addAndMakeVisible (lowcutSlopeLabel);
    lowcutSlopeLabel.setText ("Slope", juce::dontSendNotification);
    lowcutSlopeLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    lowcutSlopeLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    lowcutSlopeLabel.attachToComponent (&lowcutSlopeMode, true);
    lowcutSlopeLabel.setJustificationType (juce::Justification::left);

    addAndMakeVisible (highcutSlopeLabel);
    highcutSlopeLabel.setText ("Slope", juce::dontSendNotification);
    highcutSlopeLabel.setFont(juce::Font{
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain")
    });
    highcutSlopeLabel.setColour (juce::Label::textColourId, FILTER_COLOUR.withBrightness (0.8f));
    highcutSlopeLabel.attachToComponent (&highcutSlopeMode, true);
    highcutSlopeLabel.setJustificationType (juce::Justification::left);

    lowcutSlopeMode.addItem ("12 db", 1);
    lowcutSlopeMode.addItem ("24 db", 2);
    lowcutSlopeMode.addItem ("36 db", 3);
    lowcutSlopeMode.addItem ("48 db", 4);

    highcutSlopeMode.addItem ("12 db", 1);
    highcutSlopeMode.addItem ("24 db", 2);
    highcutSlopeMode.addItem ("36 db", 3);
    highcutSlopeMode.addItem ("48 db", 4);

    // global switches
    addAndMakeVisible (filterSwitch);
    filterSwitch.setClickingTogglesState (true);
    filterSwitch.setRadioGroupId (switchButtonsGlobal);
    filterSwitch.setToggleState (true, juce::dontSendNotification);
    filterSwitch.setColour (juce::ToggleButton::tickDisabledColourId, FILTER_COLOUR.withBrightness (0.5f));
    filterSwitch.setColour (juce::ToggleButton::tickColourId, FILTER_COLOUR.withBrightness (0.9f));
    filterSwitch.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    filterSwitch.setLookAndFeel (&flatButtonLnf);
    filterSwitch.addListener (this);

    addAndMakeVisible (downsampleSwitch);
    downsampleSwitch.setClickingTogglesState (true);
    downsampleSwitch.setRadioGroupId (switchButtonsGlobal);
    downsampleSwitch.setToggleState (false, juce::dontSendNotification);
    downsampleSwitch.setColour (juce::ToggleButton::tickDisabledColourId, DOWNSAMPLE_COLOUR.withBrightness (0.5f));
    downsampleSwitch.setColour (juce::ToggleButton::tickColourId, DOWNSAMPLE_COLOUR.withBrightness (0.9f));
    downsampleSwitch.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    downsampleSwitch.setLookAndFeel (&flatButtonLnf);
    downsampleSwitch.addListener (this);
    
//    addAndMakeVisible (limiterSwitch);
//    limiterSwitch.setClickingTogglesState (true);
//    limiterSwitch.setRadioGroupId (switchButtonsGlobal);
//    limiterSwitch.setToggleState (false, juce::dontSendNotification);
//    limiterSwitch.setColour (juce::ToggleButton::tickDisabledColourId, LIMITER_COLOUR.withBrightness (0.5f));
//    limiterSwitch.setColour (juce::ToggleButton::tickColourId, LIMITER_COLOUR.withBrightness (0.9f));
//    limiterSwitch.setColour (juce::ComboBox::outlineColourId, COLOUR6);
//    limiterSwitch.setLookAndFeel (&flatButtonLnf);
//    limiterSwitch.addListener (this);

    // init state
    setBypassState (0, filterBypassButton->getToggleState());
    setBypassState (1, downsampleBypassButton->getToggleState());
//    setBypassState (2, limiterBypassButton->getToggleState());
    
    // init visibility
    setVisibility (downsampleVector, false);
//    setVisibility (limiterVector, false);
    downsampleBypassButton->setVisible(false);
//    limiterBypassButton->setVisible(false);

    // Attachment
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, OUTPUT_ID, outputKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, MIX_ID, mixKnob);

    lowcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, LOWCUT_FREQ_ID, lowcutFreqKnob);
    lowcutGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, LOWCUT_GAIN_ID, lowcutGainKnob);
    lowcutQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, LOWCUT_Q_ID, lowcutQKnob);
    highcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, HIGHCUT_FREQ_ID, highcutFreqKnob);
    highcutGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, HIGHCUT_GAIN_ID, highcutGainKnob);
    highcutQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, HIGHCUT_Q_ID, highcutQKnob);
    peakFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, PEAK_FREQ_ID, peakFreqKnob);
    peakQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, PEAK_Q_ID, peakQKnob);
    peakGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, PEAK_GAIN_ID, peakGainKnob);

//    filterOffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, OFF_ID, filterOffButton);
//    filterPreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, PRE_ID, filterPreButton);
//    filterPostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, POST_ID, filterPostButton);
    filterLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, LOW_ID, filterLowPassButton);
    filterBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, BAND_ID, filterPeakButton);
    filterHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, HIGH_ID, filterHighPassButton);
    
    downSampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, DOWNSAMPLE_ID, downSampleKnob);
    
//    limiterThreshAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, LIMITER_THRESH_ID, limiterThreshKnob);
//    limiterReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, LIMITER_RELEASE_ID, limiterReleaseKnob);

    lowcutModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, LOWCUT_SLOPE_ID, lowcutSlopeMode);
    highcutModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, HIGHCUT_SLOPE_ID, highcutSlopeMode);

    filterBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, FILTER_BYPASS_ID, *filterBypassButton);
    downsampleBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, DOWNSAMPLE_BYPASS_ID, *downsampleBypassButton);
//    limiterBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, LIMITER_BYPASS_ID, *limiterBypassButton);
    
//    limiterReleaseKnob.textFromValueFunction = [](double value)
//    {
//        if (value < 10)
//        {
//            return juce::String(value, 2) + "ms";
//        }
//        else if (value < 100)
//        {
//            return juce::String(value, 1) + "ms";
//        }
//        return juce::String(value / 1000.0, 2) + " s";
//    };
//    
//    limiterReleaseKnob.valueFromTextFunction = [](const juce::String& text)
//    {
//        double amount = 0;
//        if (text.contains("ms"))
//        {
//            amount = text.removeCharacters("ms").getDoubleValue();
//        }
//        else
//        {
//            amount = text.removeCharacters("s").getDoubleValue() * 1000;
//        }
//        return amount;
//    };
}

GlobalPanel::~GlobalPanel()
{
//    filterOffButton.setLookAndFeel (nullptr);
//    filterPreButton.setLookAndFeel (nullptr);
//    filterPostButton.setLookAndFeel (nullptr);
    filterLowPassButton.setLookAndFeel (nullptr);
    filterPeakButton.setLookAndFeel (nullptr);
    filterHighPassButton.setLookAndFeel (nullptr);
    filterSwitch.setLookAndFeel (nullptr);
    downsampleSwitch.setLookAndFeel (nullptr);
//    limiterSwitch.setLookAndFeel (nullptr);
    postFilterPanelLabel.setLookAndFeel(nullptr);
    downSamplePanelLabel.setLookAndFeel(nullptr);
//    limiterPanelLabel.setLookAndFeel(nullptr);
}

void GlobalPanel::paint (juce::Graphics& g)
{
    g.setColour (COLOUR6);
    g.drawRect (globalEffectArea);
    g.drawRect (outputKnobArea);
}

void GlobalPanel::resized()
{
    juce::Rectangle<int> controlArea = getLocalBounds();

    globalEffectArea = controlArea.removeFromLeft (getWidth() / 5 * 3);
    outputKnobArea = controlArea;

    juce::Rectangle<int> switchArea = globalEffectArea.removeFromLeft (getWidth() / 50);
    filterSwitch.setBounds (switchArea.removeFromTop (globalEffectArea.getHeight() / 2));
    downsampleSwitch.setBounds (switchArea.removeFromTop (globalEffectArea.getHeight() / 2));
//    limiterSwitch.setBounds (switchArea.removeFromTop (globalEffectArea.getHeight() / 3));

    juce::Rectangle<int> filterKnobArea = globalEffectArea;
    juce::Rectangle<int> filterTypeArea = filterKnobArea.removeFromLeft (globalEffectArea.getWidth() / 4);

    juce::Rectangle<int> filterTypeAreaReduced = filterTypeArea.reduced (0, getHeight() / 6);
    juce::Rectangle<int> filterTypeAreaReducedRest = filterTypeAreaReduced;

    juce::Rectangle<int> filterTypeAreaTop = filterTypeAreaReducedRest.removeFromTop (filterTypeAreaReduced.getHeight() / 3).reduced (globalEffectArea.getWidth() / 20, getHeight() / 30);
    juce::Rectangle<int> filterTypeAreaMid = filterTypeAreaReducedRest.removeFromTop (filterTypeAreaReduced.getHeight() / 3).reduced (globalEffectArea.getWidth() / 20, getHeight() / 30);
    juce::Rectangle<int> filterTypeAreaButtom = filterTypeAreaReducedRest.removeFromTop (filterTypeAreaReduced.getHeight() / 3).reduced (globalEffectArea.getWidth() / 20, getHeight() / 30);
    //    juce::Rectangle<int> filterTypeAreaTop = juce::Rectangle<int>(filterTypeAreaReduced.getX(), filterTypeAreaReduced.getY(), outputKnob.getTextBoxWidth(), outputKnob.getTextBoxHeight());
    //    juce::Rectangle<int> filterTypeAreaMid = juce::Rectangle<int>(filterTypeAreaReduced.getX(), filterTypeAreaReduced.getY() + getHeight() / 4, outputKnob.getTextBoxWidth(), outputKnob.getTextBoxHeight());
    //    juce::Rectangle<int> filterTypeAreaButtom = juce::Rectangle<int>(filterTypeAreaReduced.getX(), filterTypeAreaReduced.getY() + getHeight() / 4 * 2, outputKnob.getTextBoxWidth(), outputKnob.getTextBoxHeight());

    juce::Rectangle<int> bypassButtonArea = globalEffectArea;
    bypassButtonArea = bypassButtonArea.removeFromBottom (globalEffectArea.getHeight() / 5).reduced (globalEffectArea.getWidth() / 2 - globalEffectArea.getHeight() / 10, 0);
    filterBypassButton->setBounds (bypassButtonArea);
    downsampleBypassButton->setBounds (bypassButtonArea);
//    limiterBypassButton->setBounds (bypassButtonArea);

    juce::Rectangle<int> filterKnobAreaLeft = filterKnobArea.removeFromLeft (filterKnobArea.getWidth() / 3);
    juce::Rectangle<int> filterKnobAreaMid = filterKnobArea.removeFromLeft (filterKnobArea.getWidth() / 2);
    juce::Rectangle<int> filterKnobAreaRight = filterKnobArea;
    juce::Rectangle<int> filterMenuArea = filterKnobAreaMid;
    filterKnobAreaLeft = filterKnobAreaLeft.reduced (0, getHeight() / 5);
    filterKnobAreaMid = filterKnobAreaMid.reduced (0, getHeight() / 5);
    filterKnobAreaRight = filterKnobAreaRight.reduced (0, getHeight() / 5);

    juce::Rectangle<int> knobAreaLeft = globalEffectArea;
    juce::Rectangle<int> knobAreaRight = knobAreaLeft.removeFromRight (globalEffectArea.getWidth() / 2);
    knobAreaLeft = knobAreaLeft.reduced (0, knobAreaLeft.getHeight() / 5);
    knobAreaRight = knobAreaRight.reduced (0, knobAreaRight.getHeight() / 5);
//    limiterThreshKnob.setBounds(knobAreaLeft);
//    limiterReleaseKnob.setBounds(knobAreaRight);
    
    juce::Rectangle<int> panelLabelArea = globalEffectArea;
    panelLabelArea = panelLabelArea.removeFromLeft (globalEffectArea.getWidth() / 4);
    panelLabelArea = panelLabelArea.removeFromBottom (globalEffectArea.getHeight() / 5);
    postFilterPanelLabel.setBounds(panelLabelArea);
    downSamplePanelLabel.setBounds(panelLabelArea);
//    limiterPanelLabel.setBounds(panelLabelArea);
    
    downSampleKnob.setBounds (globalEffectArea.reduced (getHeight() / 15, getHeight() / 5));
    
    lowcutFreqKnob.setBounds (filterKnobAreaLeft);
    lowcutGainKnob.setBounds (filterKnobAreaMid);
    lowcutQKnob.setBounds (filterKnobAreaRight);
    highcutFreqKnob.setBounds (filterKnobAreaLeft);
    highcutGainKnob.setBounds (filterKnobAreaMid);
    highcutQKnob.setBounds (filterKnobAreaRight);
    peakFreqKnob.setBounds (filterKnobAreaLeft);
    peakQKnob.setBounds (filterKnobAreaRight);
    peakGainKnob.setBounds (filterKnobAreaMid);

    juce::Rectangle<int> filterModeArea = filterMenuArea.removeFromBottom (getHeight() / 4); //filterKnobAreaMid.removeFromTop(getHeight() / 4);
    filterModeArea.removeFromBottom (getHeight() / 15);

    filterModeArea.setX (filterModeArea.getX() + filterModeArea.getWidth() / 1.3);

    lowcutSlopeMode.setBounds (filterModeArea);
    highcutSlopeMode.setBounds (filterModeArea);

    filterHighPassButton.setBounds (filterTypeAreaTop);
    filterPeakButton.setBounds (filterTypeAreaMid);
    filterLowPassButton.setBounds (filterTypeAreaButtom);

    juce::Rectangle<int> outputKnobAreaLeft = outputKnobArea;
    juce::Rectangle<int> outputKnobAreaRight = outputKnobAreaLeft.removeFromRight (outputKnobArea.getWidth() / 2);
    outputKnobAreaLeft = outputKnobAreaLeft.reduced (getHeight() / 15, getHeight() / 5).reduced (outputKnobArea.getWidth() / 15, 0);
    outputKnobAreaRight = outputKnobAreaRight.reduced (getHeight() / 15, getHeight() / 5).reduced (outputKnobArea.getWidth() / 15, 0);

    outputKnob.setBounds (outputKnobAreaLeft);
    mixKnob.setBounds (outputKnobAreaRight);

    lowPassButtonLnf.scale = scale;
    bandPassButtonLnf.scale = scale;
    highPassButtonLnf.scale = scale;
}

void GlobalPanel::sliderValueChanged (juce::Slider* slider)
{
}

void GlobalPanel::comboBoxChanged (juce::ComboBox* combobox)
{
}

void GlobalPanel::timerCallback()
{
}

void GlobalPanel::buttonClicked (juce::Button* clickedButton)
{
    if (filterSwitch.getToggleState())
    {
        setVisibility (filterVector, true);
        filterBypassButton->setVisible (true);

        if (filterLowPassButton.getToggleState())
        {
            lowcutFreqKnob.setVisible (false);
            lowcutQKnob.setVisible (false);
            lowcutGainKnob.setVisible (false);
            peakFreqKnob.setVisible (false);
            peakQKnob.setVisible (false);
            peakGainKnob.setVisible (false);
            lowcutSlopeMode.setVisible (false);
        }
        if (filterHighPassButton.getToggleState())
        {

            highcutFreqKnob.setVisible (false);
            highcutQKnob.setVisible (false);
            highcutGainKnob.setVisible (false);
            peakFreqKnob.setVisible (false);
            peakQKnob.setVisible (false);
            peakGainKnob.setVisible (false);
            highcutSlopeMode.setVisible (false);
        }
        if (filterPeakButton.getToggleState())
        {
            lowcutFreqKnob.setVisible (false);
            lowcutQKnob.setVisible (false);
            lowcutGainKnob.setVisible (false);
            highcutFreqKnob.setVisible (false);
            highcutQKnob.setVisible (false);
            highcutGainKnob.setVisible (false);
            lowcutSlopeMode.setVisible (false);
            highcutSlopeMode.setVisible (false);
        }
        setVisibility (downsampleVector, false);
        downsampleBypassButton->setVisible (false);
//        setVisibility (limiterVector, false);
//        limiterBypassButton->setVisible (false);
    }
    if (downsampleSwitch.getToggleState())
    {
        setVisibility (downsampleVector, true);
        downsampleBypassButton->setVisible (true);
        setVisibility (filterVector, false);
        filterBypassButton->setVisible (false);
//        setVisibility (limiterVector, false);
//        limiterBypassButton->setVisible (false);
    }
//    if (limiterSwitch.getToggleState())
//    {
//        setVisibility (limiterVector, true);
//        setVisibility (filterVector, false);
//        filterBypassButton->setVisible (false);
//        setVisibility (downsampleVector, false);
//        downsampleBypassButton->setVisible (false);
//        limiterBypassButton->setVisible (true);
//    }
}

void GlobalPanel::setVisibility (juce::Array<juce::Component*>& array, bool isVisible)
{
    for (int i = 0; i < array.size(); ++i)
    {
        if (isVisible)
        {
            array[i]->setVisible (true);
        }
        else
        {
            array[i]->setVisible (false);
        }
    }
}

void GlobalPanel::setRotarySlider (juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
}

void GlobalPanel::setRoundButton (juce::TextButton& button, juce::String paramId, juce::String buttonName)
{
    addAndMakeVisible (button);
    button.setClickingTogglesState (true);
    //bool state = *apvts.getRawParameterValue(paramId);
    //button.setToggleState(state, juce::dontSendNotification);
    button.setColour (juce::TextButton::buttonColourId, COLOUR6.withBrightness (0.1f));
    button.setColour (juce::TextButton::buttonOnColourId, COLOUR6.withBrightness (0.1f));
    button.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour (juce::TextButton::textColourOnId, FILTER_COLOUR.withBrightness (0.8f));
    button.setColour (juce::TextButton::textColourOffId, COLOUR7.withBrightness (0.8f));

    if (&button == &filterLowPassButton)
        button.setLookAndFeel (&lowPassButtonLnf);
    else if (&button == &filterPeakButton)
        button.setLookAndFeel (&bandPassButtonLnf);
    else if (&button == &filterHighPassButton)
        button.setLookAndFeel (&highPassButtonLnf);
    else
    {
        button.setButtonText (buttonName);
        button.setLookAndFeel (&roundedButtonLnf);
    }
    button.addListener(this);
}

void GlobalPanel::setScale (float scale)
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

//void GlobalPanel::updateToggleState()
//{
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
//}

void GlobalPanel::setToggleButtonState (juce::String toggleButton)
{
    if (toggleButton == "lowcut")
        filterHighPassButton.setToggleState (true, juce::NotificationType::sendNotification);
    if (toggleButton == "peak")
        filterPeakButton.setToggleState (true, juce::NotificationType::sendNotification);
    if (toggleButton == "highcut")
        filterLowPassButton.setToggleState (true, juce::NotificationType::sendNotification);
}

void GlobalPanel::setBypassState (int index, bool state)
{
    juce::Array<juce::Component*>* componentArray;
    if (index == 0)
        componentArray = &filterVector;
    if (index == 1)
        componentArray = &downsampleVector;
//    if (index == 2)
//        componentArray = &limiterVector;

    if (state)
    {
        for (int i = 0; i < componentArray->size(); i++)
        {
            componentArray->data()[i]->setEnabled (true);
        }
    }
    else
    {
        for (int i = 0; i < componentArray->size(); i++)
        {
            componentArray->data()[i]->setEnabled (false);
        }
    }
}
