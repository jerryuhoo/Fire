/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Panels/ControlPanel/Graph Components/VUMeter.h"

//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p), stateComponent{p.stateAB, p.statePresets, multiband}
{
    
    
    // init vec
    
    shapeVector.add(recKnob1 = new juce::Slider());
    shapeVector.add(recKnob2 = new juce::Slider());
    shapeVector.add(recKnob3 = new juce::Slider());
    shapeVector.add(recKnob4 = new juce::Slider());
    shapeVector.add(biasKnob1 = new juce::Slider());
    shapeVector.add(biasKnob2 = new juce::Slider());
    shapeVector.add(biasKnob3 = new juce::Slider());
    shapeVector.add(biasKnob4 = new juce::Slider());
    
    widthVector.add(widthKnob1 = new juce::Slider());
    widthVector.add(widthKnob2 = new juce::Slider());
    widthVector.add(widthKnob3 = new juce::Slider());
    widthVector.add(widthKnob4 = new juce::Slider());
    
    compressorVector.add(compThreshKnob1 = new juce::Slider());
    compressorVector.add(compThreshKnob2 = new juce::Slider());
    compressorVector.add(compThreshKnob3 = new juce::Slider());
    compressorVector.add(compThreshKnob4 = new juce::Slider());
    compressorVector.add(compRatioKnob1 = new juce::Slider());
    compressorVector.add(compRatioKnob2 = new juce::Slider());
    compressorVector.add(compRatioKnob3 = new juce::Slider());
    compressorVector.add(compRatioKnob4 = new juce::Slider());
    
    // timer
    juce::Timer::startTimerHz(60.0f);

    // This is not a perfect fix for Vst3 plugins
    // Vst3 calls constructor before setStateInformation in processor,
    // however, AU plugin calls constructor after setStateInformation/
    // So I set delay of 1 ms to reset size and other stuff.
    // call function after 1 ms
    std::function<void()> initFunction = [this]() { initEditor(); };
    juce::Timer::callAfterDelay(1, initFunction);
    
    // Graph
    addAndMakeVisible(graphPanel);
    
    // Spectrum
    addAndMakeVisible(spectrum);
    addAndMakeVisible(multiband);
    addAndMakeVisible(filterControl);
    
    multiFreqSlider1.addListener(this);
    multiFreqSlider2.addListener(this);
    multiFreqSlider3.addListener(this);
    
    spectrum.setInterceptsMouseClicks(false, false);
    spectrum.prepareToPaintSpectrum(processor.getFFTSize(), processor.getFFTData());
    
    // presets
    addAndMakeVisible(stateComponent);
    stateComponent.getPresetBox()->addListener(this);
    stateComponent.getToggleABButton()->addListener(this);
    stateComponent.getCopyABButton()->addListener(this);
    stateComponent.getPreviousButton()->addListener(this);
    stateComponent.getNextButton()->addListener(this);

    setLookAndFeel(&otherLookAndFeel);

    // drive knobs
    driveKnob1.setLookAndFeel(&driveLookAndFeel1);
    driveKnob2.setLookAndFeel(&driveLookAndFeel2);
    driveKnob3.setLookAndFeel(&driveLookAndFeel3);
    driveKnob4.setLookAndFeel(&driveLookAndFeel4);
    setListenerKnob(driveKnob1);
    setListenerKnob(driveKnob2);
    setListenerKnob(driveKnob3);
    setListenerKnob(driveKnob4);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    driveLabel.setColour(juce::Label::textColourId, juce::Colours::red.withBrightness(0.9f));
    driveLabel.attachToComponent(&driveKnob1, false);
    driveLabel.setJustificationType(juce::Justification::centred);
    
    // output knobs
    setListenerKnob(outputKnob1);
    setListenerKnob(outputKnob2);
    setListenerKnob(outputKnob3);
    setListenerKnob(outputKnob4);
    outputKnob1.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob2.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob3.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob4.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    setRotarySlider(outputKnob, COLOUR1);
    
    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent(&outputKnob1, false);
    outputLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(outputLabelGlobal);
    outputLabelGlobal.setText("Output", juce::dontSendNotification);
    outputLabelGlobal.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabelGlobal.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabelGlobal.attachToComponent(&outputKnob, false);
    outputLabelGlobal.setJustificationType(juce::Justification::centred);
    
    outputKnob.setTextValueSuffix("db");
    outputKnob1.setTextValueSuffix("db");
    outputKnob2.setTextValueSuffix("db");
    outputKnob3.setTextValueSuffix("db");
    outputKnob4.setTextValueSuffix("db");
    
    // mix knobs
    setRotarySlider(mixKnob1, COLOUR1);
    setRotarySlider(mixKnob2, COLOUR1);
    setRotarySlider(mixKnob3, COLOUR1);
    setRotarySlider(mixKnob4, COLOUR1);
    setRotarySlider(mixKnob, COLOUR1);
    
    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent(&mixKnob1, false);
    mixLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(mixLabelGlobal);
    mixLabelGlobal.setText("Mix", juce::dontSendNotification);
    mixLabelGlobal.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabelGlobal.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabelGlobal.attachToComponent(&mixKnob, false);
    mixLabelGlobal.setJustificationType(juce::Justification::centred);
    
    // compressor ratio knobs
    setRotarySlider(*compRatioKnob1, juce::Colours::yellowgreen);
    setRotarySlider(*compRatioKnob2, juce::Colours::yellowgreen);
    setRotarySlider(*compRatioKnob3, juce::Colours::yellowgreen);
    setRotarySlider(*compRatioKnob4, juce::Colours::yellowgreen);

    addAndMakeVisible(CompRatioLabel);
    CompRatioLabel.setText("Ratio", juce::dontSendNotification);
    CompRatioLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompRatioLabel.setColour(juce::Label::textColourId, juce::Colours::yellowgreen);
    CompRatioLabel.attachToComponent(compRatioKnob1, false);
    CompRatioLabel.setJustificationType(juce::Justification::centred);
    
    // compressor threashold knobs
    setRotarySlider(*compThreshKnob1, juce::Colours::yellowgreen);
    setRotarySlider(*compThreshKnob2, juce::Colours::yellowgreen);
    setRotarySlider(*compThreshKnob3, juce::Colours::yellowgreen);
    setRotarySlider(*compThreshKnob4, juce::Colours::yellowgreen);

    compThreshKnob1->setTextValueSuffix("db");
    compThreshKnob2->setTextValueSuffix("db");
    compThreshKnob3->setTextValueSuffix("db");
    compThreshKnob4->setTextValueSuffix("db");
    
    addAndMakeVisible(CompThreshLabel);
    CompThreshLabel.setText("Threshold", juce::dontSendNotification);
    CompThreshLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompThreshLabel.setColour(juce::Label::textColourId, juce::Colours::yellowgreen);
    CompThreshLabel.attachToComponent(compThreshKnob1, false);
    CompThreshLabel.setJustificationType(juce::Justification::centred);
    
    // width knobs
    setRotarySlider(*widthKnob1, juce::Colours::skyblue);
    setRotarySlider(*widthKnob2, juce::Colours::skyblue);
    setRotarySlider(*widthKnob3, juce::Colours::skyblue);
    setRotarySlider(*widthKnob4, juce::Colours::skyblue);

    addAndMakeVisible(widthLabel);
    widthLabel.setText("Width", juce::dontSendNotification);
    widthLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    widthLabel.setColour(juce::Label::textColourId, juce::Colours::skyblue);
    widthLabel.attachToComponent(widthKnob1, false);
    widthLabel.setJustificationType(juce::Justification::centred);

    // bias knobs
    setRotarySlider(*biasKnob1, COLOUR1);
    setRotarySlider(*biasKnob2, COLOUR1);
    setRotarySlider(*biasKnob3, COLOUR1);
    setRotarySlider(*biasKnob4, COLOUR1);
    
    addAndMakeVisible(biasLabel);
    biasLabel.setText("Bias", juce::dontSendNotification);
    biasLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    biasLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    biasLabel.attachToComponent(biasKnob1, false);
    biasLabel.setJustificationType(juce::Justification::centred);

    // downsample knob
    setRotarySlider(downSampleKnob, juce::Colours::purple.withBrightness(0.8f));
    
    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", juce::dontSendNotification);
    downSampleLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    downSampleLabel.setColour(juce::Label::textColourId, juce::Colours::purple.withBrightness(0.8f));
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(juce::Justification::centred);

    // rec knobs
    setRotarySlider(*recKnob1, COLOUR1);
    setRotarySlider(*recKnob2, COLOUR1);
    setRotarySlider(*recKnob3, COLOUR1);
    setRotarySlider(*recKnob4, COLOUR1);

    addAndMakeVisible(recLabel);
    recLabel.setText("Rectification", juce::dontSendNotification);
    recLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    recLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    recLabel.attachToComponent(recKnob1, false);
    recLabel.setJustificationType(juce::Justification::centred);

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
    
    // HQ(oversampling) Button
    addAndMakeVisible(hqButton);
    hqButton.setClickingTogglesState(true);
    bool hqButtonState = *processor.treeState.getRawParameterValue("hq");
    hqButton.setToggleState(hqButtonState, juce::dontSendNotification);
    hqButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    hqButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    hqButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    hqButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    hqButton.setColour(juce::TextButton::textColourOffId, juce::Colour(100, 20, 20));
    hqButton.setButtonText("HQ");

    // Linked Button
    setFlatButton(linkedButton1, LINKED_ID1, "L");
    setFlatButton(linkedButton2, LINKED_ID2, "L");
    setFlatButton(linkedButton3, LINKED_ID3, "L");
    setFlatButton(linkedButton4, LINKED_ID4, "L");
    
    // safe overload Button
    setFlatButton(safeButton1, SAFE_ID1, "S");
    setFlatButton(safeButton2, SAFE_ID2, "S");
    setFlatButton(safeButton3, SAFE_ID3, "S");
    setFlatButton(safeButton4, SAFE_ID4, "S");
  
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
    
    // Window Left Button
    addAndMakeVisible(windowLeftButton);
    windowLeftButton.setClickingTogglesState(true);
    windowLeftButton.setRadioGroupId(windowButtons);
    windowLeftButton.setButtonText("Band Effect");
    windowLeftButton.setToggleState(true, juce::NotificationType::dontSendNotification);
    windowLeftButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowLeftButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowLeftButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.0f));
    windowLeftButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowLeftButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    //windowLeftButton.setButtonText("1");
    windowLeftButton.setLookAndFeel(&otherLookAndFeel);
    
    // Window Right Button
    addAndMakeVisible(windowRightButton);
    windowRightButton.setClickingTogglesState(true);
    windowRightButton.setRadioGroupId(windowButtons);
    windowRightButton.setButtonText("Global Effect");
    windowRightButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    windowRightButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowRightButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowRightButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.0f));
    windowRightButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowRightButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    windowRightButton.setLookAndFeel(&otherLookAndFeel);
    
    // switches
    addAndMakeVisible(oscSwitch);
    oscSwitch.setClickingTogglesState(true);
    oscSwitch.setRadioGroupId(switchButtons);
    oscSwitch.setButtonText("");
    oscSwitch.setToggleState(true, juce::dontSendNotification);
    oscSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::red.withBrightness(0.5f));
    oscSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red.withBrightness(0.9f));
    oscSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    oscSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    oscSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    oscSwitch.setLookAndFeel(&flatButtonLnf);
    
    addAndMakeVisible(shapeSwitch);
    shapeSwitch.setClickingTogglesState(true);
    shapeSwitch.setRadioGroupId(switchButtons);
    shapeSwitch.setButtonText("");
    shapeSwitch.setToggleState(false, juce::dontSendNotification);
    shapeSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::yellow.withBrightness(0.5f));
    shapeSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow.withBrightness(0.9f));
    shapeSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    shapeSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    shapeSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    shapeSwitch.setLookAndFeel(&flatButtonLnf);

    addAndMakeVisible(compressorSwitch);
    compressorSwitch.setClickingTogglesState(true);
    compressorSwitch.setRadioGroupId(switchButtons);
    compressorSwitch.setButtonText("");
    compressorSwitch.setToggleState(false, juce::dontSendNotification);
    compressorSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::yellowgreen.withBrightness(0.5f));
    compressorSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellowgreen.withBrightness(0.9f));
    compressorSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    compressorSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    compressorSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    compressorSwitch.setLookAndFeel(&flatButtonLnf);
    
    addAndMakeVisible(widthSwitch);
    widthSwitch.setClickingTogglesState(true);
    widthSwitch.setRadioGroupId(switchButtons);
    widthSwitch.setButtonText("");
    widthSwitch.setToggleState(false, juce::dontSendNotification);
    widthSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::skyblue.withBrightness(0.5f));
    widthSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::skyblue.withBrightness(0.9f));
    widthSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    widthSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    widthSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    widthSwitch.setLookAndFeel(&flatButtonLnf);
    
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
    driveAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID1, driveKnob1);
    driveAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID2, driveKnob2);
    driveAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID3, driveKnob3);
    driveAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID4, driveKnob4);
    
    compRatioAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID1, *compRatioKnob1);
    compRatioAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID2, *compRatioKnob2);
    compRatioAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID3, *compRatioKnob3);
    compRatioAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID4, *compRatioKnob4);
    
    compThreshAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID1, *compThreshKnob1);
    compThreshAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID2, *compThreshKnob2);
    compThreshAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID3, *compThreshKnob3);
    compThreshAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID4, *compThreshKnob4);
    
    outputAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID1, outputKnob1);
    outputAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID2, outputKnob2);
    outputAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID3, outputKnob3);
    outputAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID4, outputKnob4);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID, outputKnob);
    
    mixAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID1, mixKnob1);
    mixAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID2, mixKnob2);
    mixAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID3, mixKnob3);
    mixAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID4, mixKnob4);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID, mixKnob);
    
    biasAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID1, *biasKnob1);
    biasAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID2, *biasKnob2);
    biasAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID3, *biasKnob3);
    biasAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID4, *biasKnob4);
    
    recAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID1, *recKnob1);
    recAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID2, *recKnob2);
    recAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID3, *recKnob3);
    recAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID4, *recKnob4);
    
    downSampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DOWNSAMPLE_ID, downSampleKnob);

    
    lowcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LOWCUT_FREQ_ID, lowcutFreqKnob);
    lowcutQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LOWCUT_Q_ID, lowcutQKnob);
    highcutFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, HIGHCUT_FREQ_ID, highcutFreqKnob);
    highcutQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, HIGHCUT_Q_ID, highcutQKnob);
    peakFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, PEAK_FREQ_ID, peakFreqKnob);
    peakQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, PEAK_Q_ID, peakQKnob);
    peakGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, PEAK_GAIN_ID, peakGainKnob);

    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HQ_ID, hqButton);
    
    linkedAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID1, linkedButton1);
    linkedAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID2, linkedButton2);
    linkedAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID3, linkedButton3);
    linkedAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID4, linkedButton4);
    
    safeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID1, safeButton1);
    safeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID2, safeButton2);
    safeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID3, safeButton3);
    safeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID4, safeButton4);
    
    widthAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID1, *widthKnob1);
    widthAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID2, *widthKnob2);
    widthAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID3, *widthKnob3);
    widthAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID4, *widthKnob4);
    
    filterOffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, OFF_ID, filterOffButton);
    filterPreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, PRE_ID, filterPreButton);
    filterPostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, POST_ID, filterPostButton);
    filterLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LOW_ID, filterLowPassButton);
    filterBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ID, filterPeakButton);
    filterHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HIGH_ID, filterHighPassButton);
    
    multiFocusAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID1, multiFocusSlider1);
    multiFocusAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID2, multiFocusSlider2);
    multiFocusAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID3, multiFocusSlider3);
    multiFocusAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID4, multiFocusSlider4);
    
    multiEnableAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID1, multiEnableSlider1);
    multiEnableAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID2, multiEnableSlider2);
    multiEnableAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID3, multiEnableSlider3);
    multiEnableAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID4, multiEnableSlider4);
    
    multiFreqAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID1, multiFreqSlider1);
    multiFreqAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID2, multiFreqSlider2);
    multiFreqAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID3, multiFreqSlider3);
    
    lineStateSliderAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID1, lineStateSlider1);
    lineStateSliderAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID2, lineStateSlider2);
    lineStateSliderAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID3, lineStateSlider3);

    setMenu(&distortionMode1);
    setMenu(&distortionMode2);
    setMenu(&distortionMode3);
    setMenu(&distortionMode4);

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
    
    lowcutModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, LOWCUT_SLOPE_ID, lowcutSlopeMode);
    highcutModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, HIGHCUT_SLOPE_ID, highcutSlopeMode);
    
    // use global lookandfeel
    getLookAndFeel().setColour(juce::ComboBox::textColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::ComboBox::arrowColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::ComboBox::buttonColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::outlineColourId, COLOUR6);
    getLookAndFeel().setColour(juce::ComboBox::focusedOutlineColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::backgroundColourId, COLOUR6);
    getLookAndFeel().setColour(juce::PopupMenu::textColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedBackgroundColourId, COLOUR5);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedTextColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::headerTextColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::PopupMenu::backgroundColourId, juce::Colours::transparentWhite);

    modeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID1, distortionMode1);
    modeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID2, distortionMode2);
    modeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID3, distortionMode3);
    modeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID4, distortionMode4);
    
    // set resize
    setResizable(true, true);
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    // resize limit
    setResizeLimits(INIT_WIDTH, INIT_HEIGHT, 2000, 1000); // set resize limits
    getConstrainer ()->setFixedAspectRatio (2); // set fixed resize rate

    updateToggleState();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
    driveKnob1.setLookAndFeel(nullptr);
    driveKnob2.setLookAndFeel(nullptr);
    driveKnob3.setLookAndFeel(nullptr);
    driveKnob4.setLookAndFeel(nullptr);
    linkedButton1.setLookAndFeel(nullptr);
    linkedButton2.setLookAndFeel(nullptr);
    linkedButton3.setLookAndFeel(nullptr);
    linkedButton4.setLookAndFeel(nullptr);
    safeButton1.setLookAndFeel(nullptr);
    safeButton2.setLookAndFeel(nullptr);
    safeButton3.setLookAndFeel(nullptr);
    safeButton4.setLookAndFeel(nullptr);
    filterOffButton.setLookAndFeel(nullptr);
    filterPreButton.setLookAndFeel(nullptr);
    filterPostButton.setLookAndFeel(nullptr);
    filterLowPassButton.setLookAndFeel(nullptr);
    filterPeakButton.setLookAndFeel(nullptr);
    filterHighPassButton.setLookAndFeel(nullptr);
    windowRightButton.setLookAndFeel(nullptr);
    windowLeftButton.setLookAndFeel(nullptr);
    oscSwitch.setLookAndFeel(nullptr);
    shapeSwitch.setLookAndFeel(nullptr);
    widthSwitch.setLookAndFeel(nullptr);
    compressorSwitch.setLookAndFeel(nullptr);
    filterSwitch.setLookAndFeel(nullptr);
    otherSwitch.setLookAndFeel(nullptr);
//    distortionMode1.setLookAndFeel(nullptr);
//    distortionMode2.setLookAndFeel(nullptr);
//    distortionMode3.setLookAndFeel(nullptr);
//    distortionMode4.setLookAndFeel(nullptr);
//    stateComponent.setLookAndFeel(nullptr);
}

void FireAudioProcessorEditor::initEditor()
{
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    
    //DBG(processor.getPresetId());
    //processor.setPresetId(processor.getPresetId());
    //lastPresetName = stateComponent.getPresetName();
    
    setMultiband();
}

//==============================================================================
void FireAudioProcessorEditor::paint(juce::Graphics &g)
{
    int part1 = PART1;//getHeight() / 10;
    int part2 = PART2;//part1 * 3;
    
    // background
    g.fillAll(COLOUR7);

    // title
    g.setColour(COLOUR5);
    g.fillRect(0, 0, getWidth(), part1);

    // draw version
    g.setColour(COLOUR5);
    g.setFont(juce::Font("Times New Roman", 18.0f, juce::Font::bold));
    juce::String version = (juce::String)VERSION;
    juce::Rectangle<int> area(getWidth() - 50, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, juce::Justification::topLeft, 1);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t)BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, part1, part1, 0, 0, logo.getWidth(), logo.getHeight());

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t)BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - part1, 0, part1, part1, 0, 0, logoWings.getWidth(), logoWings.getHeight());

    bool left = windowLeftButton.getToggleState();
    bool right = windowRightButton.getToggleState();

    auto frame = getLocalBounds();
    frame.setBounds(0, part1, getWidth(), part2);
    
    // draw layer 2
    g.setColour(COLOUR6);
    g.fillRect(0, part1, getWidth(), part2);
    
    if (stateComponent.getChangedState()) // only for clicked the same preset in preset box
    {
        initState();
        stateComponent.setChangedState(false);
    }

    // set value only when line is deleted, added, moving
    if (multiband.getDeleteState() || multiband.getAddState() || multiband.getMovingState())
    {
        if (multiband.getDeleteState())
        {
            multiband.updateLines("delete", multiband.getChangedIndex());
            multiband.setDeleteState(false);
        }
        else if (multiband.getAddState())
        {
            multiband.updateLines("add", multiband.getChangedIndex());
            multiband.setAddState(false);
        }
        else if (multiband.getMovingState())
        {
            multiband.updateLines("moving", multiband.getChangedIndex());
            multiband.setMovingState(false);
        }
        updateFreqArray();
        


        multiband.getLineState(lineState);
        lineStateSlider1.setValue(lineState[0]);
        lineStateSlider2.setValue(lineState[1]);
        lineStateSlider3.setValue(lineState[2]);
        
        processor.setLineNum(multiband.getLineNum());
    }
    
    multiband.getFocusArray(multibandFocus);
    multiFocusSlider1.setValue(multibandFocus[0]);
    multiFocusSlider2.setValue(multibandFocus[1]);
    multiFocusSlider3.setValue(multibandFocus[2]);
    multiFocusSlider4.setValue(multibandFocus[3]);
    
    // TODO: put this inside
    multiband.getEnableArray(multibandEnable);
    multiEnableSlider1.setValue(multibandEnable[0]);
    multiEnableSlider2.setValue(multibandEnable[1]);
    multiEnableSlider3.setValue(multibandEnable[2]);
    multiEnableSlider4.setValue(multibandEnable[3]);
    
    int bandNum = 0;
    if (multibandFocus[0])
    {
        bandNum = 1;
    }
    else if (multibandFocus[1])
    {
        bandNum = 2;
    }
    else if (multibandFocus[2])
    {
        bandNum = 3;
    }
    else if (multibandFocus[3])
    {
        bandNum = 4;
    }
    setFourKnobsVisibility(distortionMode1, distortionMode2, distortionMode3, distortionMode4, bandNum);
    
    // mix and output knob group panel
//    g.setColour(COLOUR_GROUP);
//    g.fillRoundedRectangle(getWidth() / 10 * 9 - KNOBSIZE / 2, PART1 + PART2 + getHeight() / 12, KNOBSIZE, getHeight() - PART1 - PART2 - getHeight() / 10, 10);
    
    if (left) { // if you select the left window, you will see audio wave and distortion function graphs.

        multiband.setVisible(true);
        filterControl.setVisible(false);
        
        // draw layer 2
        g.setColour(COLOUR6);
        g.drawRect(KNOB_PANEL_3_X, OSC_Y, KNOB_PANEL_1_WIDTH, OSC_HEIGHT * 2);
        
        bool isOscSwitchOn = oscSwitch.getToggleState();
        bool isWidthSwitchOn = widthSwitch.getToggleState();
        bool isShapeSwitchOn = shapeSwitch.getToggleState();
        bool isCompressorSwitchOn = compressorSwitch.getToggleState();
        
        setFourKnobsVisibility(driveKnob1, driveKnob2, driveKnob3, driveKnob4, bandNum);
        
        oscSwitch.setVisible(true);
        shapeSwitch.setVisible(true);
        widthSwitch.setVisible(true);
        compressorSwitch.setVisible(true);
        filterSwitch.setVisible(false);
        otherSwitch.setVisible(false);
        lowcutFreqKnob.setVisible(false);
        lowcutQKnob.setVisible(false);
        highcutFreqKnob.setVisible(false);
        highcutQKnob.setVisible(false);
        peakFreqKnob.setVisible(false);
        peakQKnob.setVisible(false);
        peakGainKnob.setVisible(false);
        lowcutSlopeMode.setVisible(false);
        highcutSlopeMode.setVisible(false);
        
        // drive reduction
        driveLookAndFeel1.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID1);
        driveLookAndFeel1.reductionPrecent = processor.getReductionPrecent(SAFE_ID1);
        driveLookAndFeel2.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID2);
        driveLookAndFeel2.reductionPrecent = processor.getReductionPrecent(SAFE_ID2);
        driveLookAndFeel3.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID3);
        driveLookAndFeel3.reductionPrecent = processor.getReductionPrecent(SAFE_ID3);
        driveLookAndFeel4.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID4);
        driveLookAndFeel4.reductionPrecent = processor.getReductionPrecent(SAFE_ID4);
        
        float scale = juce::jmin(getHeight() / INIT_HEIGHT, getWidth() / INIT_WIDTH);
        
        if (isOscSwitchOn)
        {
            driveKnob1.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y - getHeight() / 20, DRIVE_SIZE, DRIVE_SIZE);
            driveKnob2.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y - getHeight() / 20, DRIVE_SIZE, DRIVE_SIZE);
            driveKnob3.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y - getHeight() / 20, DRIVE_SIZE, DRIVE_SIZE);
            driveKnob4.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y - getHeight() / 20, DRIVE_SIZE, DRIVE_SIZE);
        }
        else
        {
            driveKnob1.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
            driveKnob2.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
            driveKnob3.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
            driveKnob4.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
        }
        
        if (isOscSwitchOn)
        {
            //g.drawRect(KNOB_PANEL_1_X, OSC_Y, KNOB_PANEL_1_WIDTH + KNOB_PANEL_2_WIDTH, OSC_HEIGHT * 2);
            setInvisible(shapeVector);
            setInvisible(compressorVector);
            setInvisible(widthVector);
            recLabel.setVisible(false);
            biasLabel.setVisible(false);
            widthLabel.setVisible(false);
            CompRatioLabel.setVisible(false);
            CompThreshLabel.setVisible(false);
        }
        if (isCompressorSwitchOn)
        {
            g.drawRect(KNOB_PANEL_1_X, OSC_Y, KNOB_PANEL_1_WIDTH, OSC_HEIGHT * 2);
            g.drawRect(KNOB_PANEL_2_X, OSC_Y, KNOB_PANEL_2_WIDTH, OSC_HEIGHT * 2);
            setFourKnobsVisibility(*compRatioKnob1, *compRatioKnob2, *compRatioKnob3, *compRatioKnob4, bandNum);
            setFourKnobsVisibility(*compThreshKnob1, *compThreshKnob2, *compThreshKnob3, *compThreshKnob4, bandNum);
            setInvisible(shapeVector);
            setInvisible(widthVector);
            CompRatioLabel.setVisible(true);
            CompThreshLabel.setVisible(true);
            recLabel.setVisible(false);
            biasLabel.setVisible(false);
            widthLabel.setVisible(false);
            
//            graphPanel.getVuPanel()->setZoomState(true);
//            graphPanel.getOscilloscope()->setZoomState(false);
//            graphPanel.getDistortionGraph()->setZoomState(false);
//            graphPanel.getWidthGraph()->setZoomState(false);
        }
        if (isShapeSwitchOn)
        {
            g.drawRect(KNOB_PANEL_1_X, OSC_Y, KNOB_PANEL_1_WIDTH, OSC_HEIGHT * 2);
            g.drawRect(KNOB_PANEL_2_X, OSC_Y, KNOB_PANEL_2_WIDTH, OSC_HEIGHT * 2);
            setFourKnobsVisibility(*recKnob1, *recKnob2, *recKnob3, *recKnob4, bandNum);
            setFourKnobsVisibility(*biasKnob1, *biasKnob2, *biasKnob3, *biasKnob4, bandNum);
            setInvisible(compressorVector);
            setInvisible(widthVector);
            CompRatioLabel.setVisible(false);
            CompThreshLabel.setVisible(false);
            recLabel.setVisible(true);
            biasLabel.setVisible(true);
            widthLabel.setVisible(false);
            
//            graphPanel.getVuPanel()->setZoomState(false);
//            graphPanel.getOscilloscope()->setZoomState(false);
//            graphPanel.getDistortionGraph()->setZoomState(true);
//            graphPanel.getWidthGraph()->setZoomState(false);
        }
        if (isWidthSwitchOn)
        {
            g.drawRect(KNOB_PANEL_1_X, OSC_Y, KNOB_PANEL_1_WIDTH, OSC_HEIGHT * 2);
            g.drawRect(KNOB_PANEL_2_X, OSC_Y, KNOB_PANEL_2_WIDTH, OSC_HEIGHT * 2);
            setFourKnobsVisibility(*widthKnob1, *widthKnob2, *widthKnob3, *widthKnob4, bandNum);
            setInvisible(shapeVector);
            setInvisible(compressorVector);
            CompRatioLabel.setVisible(false);
            CompThreshLabel.setVisible(false);
            recLabel.setVisible(false);
            biasLabel.setVisible(false);
            widthLabel.setVisible(true);
            
//            graphPanel.getVuPanel()->setZoomState(false);
//            graphPanel.getOscilloscope()->setZoomState(false);
//            graphPanel.getDistortionGraph()->setZoomState(false);
//            graphPanel.getWidthGraph()->setZoomState(true);
        }
        
        setFourKnobsVisibility(outputKnob1, outputKnob2, outputKnob3, outputKnob4, bandNum);
        setFourKnobsVisibility(mixKnob1, mixKnob2, mixKnob3, mixKnob4, bandNum);
        
        setFourKnobsVisibility(linkedButton1, linkedButton2, linkedButton3, linkedButton4, bandNum);
        setFourKnobsVisibility(safeButton1, safeButton2, safeButton3, safeButton4, bandNum);
        
//        if (driveKnob1.isMouseOverOrDragging())
//        {
//            driveLabel.setVisible(false);
////            driveKnob1.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::TextBoxBelow, true, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
//        }
//        else
//        {
//            driveLabel.setVisible(true);
////            driveKnob1.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
//        }
        driveLabel.setVisible(true);
        outputLabel.setVisible(true);
        mixLabel.setVisible(true);
        // set other effects invisible
        downSampleKnob.setVisible(false);
        lowcutQKnob.setVisible(false);
        filterLowPassButton.setVisible(false);
        filterOffButton.setVisible(false);
        filterPreButton.setVisible(false);
        filterPeakButton.setVisible(false);
        filterPostButton.setVisible(false);
        filterHighPassButton.setVisible(false);
        lowcutFreqKnob.setVisible(false);
        mixKnob.setVisible(false);
        outputKnob.setVisible(false); 
    }
    else if (right) // if you select the left window, you can use muti-band distortion
    {
        // set other effects' visibility
        multiband.setVisible(false);
        shapeSwitch.setVisible(false);
        widthSwitch.setVisible(false);
        compressorSwitch.setVisible(false);
        oscSwitch.setVisible(false);
        
        filterSwitch.setVisible(true);
        otherSwitch.setVisible(true);
        filterControl.setVisible(true);

        filterLowPassButton.setVisible(true);
        filterOffButton.setVisible(true);
        filterPreButton.setVisible(true);
        filterPeakButton.setVisible(true);
        filterPostButton.setVisible(true);
        filterHighPassButton.setVisible(true);
     
        mixKnob.setVisible(true);
        outputKnob.setVisible(true);
        
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
        
        // set invisible
        driveKnob1.setVisible(false);
        driveKnob2.setVisible(false);
        driveKnob3.setVisible(false);
        driveKnob4.setVisible(false);
        
        outputKnob1.setVisible(false);
        outputKnob2.setVisible(false);
        outputKnob3.setVisible(false);
        outputKnob4.setVisible(false);
        
        mixKnob1.setVisible(false);
        mixKnob2.setVisible(false);
        mixKnob3.setVisible(false);
        mixKnob4.setVisible(false);
        
        compRatioKnob1->setVisible(false);
        compRatioKnob2->setVisible(false);
        compRatioKnob3->setVisible(false);
        compRatioKnob4->setVisible(false);
        
        compThreshKnob1->setVisible(false);
        compThreshKnob2->setVisible(false);
        compThreshKnob3->setVisible(false);
        compThreshKnob4->setVisible(false);
        
        widthKnob1->setVisible(false);
        widthKnob2->setVisible(false);
        widthKnob3->setVisible(false);
        widthKnob4->setVisible(false);
        
        linkedButton1.setVisible(false);
        linkedButton2.setVisible(false);
        linkedButton3.setVisible(false);
        linkedButton4.setVisible(false);
        
        safeButton1.setVisible(false);
        safeButton2.setVisible(false);
        safeButton3.setVisible(false);
        safeButton4.setVisible(false);
        
        recKnob1->setVisible(false);
        recKnob2->setVisible(false);
        recKnob3->setVisible(false);
        recKnob4->setVisible(false);
        
        biasKnob1->setVisible(false);
        biasKnob2->setVisible(false);
        biasKnob3->setVisible(false);
        biasKnob4->setVisible(false);
        
        driveLabel.setVisible(false);
        outputLabel.setVisible(false);
        mixLabel.setVisible(false);
        CompRatioLabel.setVisible(false);
        CompThreshLabel.setVisible(false);
        widthLabel.setVisible(false);
        recLabel.setVisible(false);
        biasLabel.setVisible(false);
    }
    
    if (multibandFocus[0])
    {
        setDistortionGraph(MODE_ID1, DRIVE_ID1,
            REC_ID1, MIX_ID1, BIAS_ID1);
    }
    else if (multibandFocus[1])
    {
        setDistortionGraph(MODE_ID2, DRIVE_ID2,
            REC_ID2, MIX_ID2, BIAS_ID2);
    }
    else if (multibandFocus[2])
    {
        setDistortionGraph(MODE_ID3, DRIVE_ID3,
            REC_ID3, MIX_ID3, BIAS_ID3);
    }
    else if (multibandFocus[3])
    {
        setDistortionGraph(MODE_ID4, DRIVE_ID4,
            REC_ID4, MIX_ID4, BIAS_ID4);
    }
}

void FireAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    // save resized size
    processor.setSavedHeight(getHeight());
    processor.setSavedWidth(getWidth());

    // knobs
    int knobNum = 10;
    float scale = juce::jmin(getHeight() / INIT_HEIGHT, getWidth() / INIT_WIDTH);
    float scaleMax = juce::jmax(getHeight() / INIT_HEIGHT, getWidth() / INIT_WIDTH);
    int scaledKnobSize = SCALED_KNOBSIZE;
    int startX = getWidth() / knobNum;
    int secondShadowY = getHeight() / 10 * 4;
    int firstPartY = secondShadowY + (getHeight() - secondShadowY) * 2 / 5 - scaledKnobSize / 2;
    int secondPartY = secondShadowY + (getHeight() - secondShadowY) * 4 / 5 - scaledKnobSize / 2;

    
    // top bar
    hqButton.setBounds(getHeight() / 10, 0, getHeight() / 10, getHeight() / 10);
    juce::Rectangle<int> area(getLocalBounds());
    juce::Rectangle<int> topBar = area.removeFromTop(50 * getHeight() / INIT_HEIGHT);
    topBar.removeFromLeft(100 * scaleMax);
    topBar.removeFromRight(50 * scaleMax);
    stateComponent.setBounds(topBar);

    // spectrum and filter
    juce::Rectangle<int> spectrumArea = area.removeFromTop(SPEC_HEIGHT);
    spectrum.setBounds(spectrumArea);
    multiband.setBounds(spectrumArea);
    filterControl.setBounds(spectrumArea);
    
    // left and right window buttons
    float windowHeight = getHeight() / 20;
    juce::Rectangle<int> leftWindowButtonArea = area.removeFromTop(windowHeight);
    juce::Rectangle<int> rightWindowButtonArea = leftWindowButtonArea.removeFromRight(getWidth() / 2);
    windowLeftButton.setBounds(leftWindowButtonArea);
    windowRightButton.setBounds(rightWindowButtonArea);
    
    juce::Rectangle<int> controlAreaTop = area.removeFromTop(area.getHeight() / 5);
    juce::Rectangle<int> controlAreaMid = area.removeFromTop(area.getHeight() / 4 * 3);
    juce::Rectangle<int> controlAreaButton = area;
    
    // distortion menu
    controlAreaTop.removeFromLeft(OSC_X); // x position
    juce::Rectangle<int> distortionModeArea = controlAreaTop.removeFromLeft(OSC_WIDTH); // width
    distortionModeArea.removeFromTop(controlAreaTop.getHeight() / 4);
    distortionModeArea.removeFromBottom(controlAreaTop.getHeight() / 4);
    distortionMode1.setBounds(distortionModeArea);
    distortionMode2.setBounds(distortionModeArea);
    distortionMode3.setBounds(distortionModeArea);
    distortionMode4.setBounds(distortionModeArea);
//    if (oscSwitch.getToggleState())
//    {
//        driveKnob1.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y, DRIVE_SIZE, DRIVE_SIZE);
//        driveKnob2.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y, DRIVE_SIZE, DRIVE_SIZE);
//        driveKnob3.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y, DRIVE_SIZE, DRIVE_SIZE);
//        driveKnob4.setBounds(getWidth() / 2 - DRIVE_SIZE / 2.0f, OSC_Y, DRIVE_SIZE, DRIVE_SIZE);
//    }
//    else
//    {
//        driveKnob1.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
//        driveKnob2.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
//        driveKnob3.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
//        driveKnob4.setBounds(DRIVE_X, DRIVE_Y, SCALED_KNOBSIZE, SCALED_KNOBSIZE);
//    }
    
    juce::Rectangle<int> graphArea = controlAreaMid.removeFromLeft(getWidth() / 7 * 2);
    graphArea.removeFromLeft(graphArea.getWidth() / 10);
    // Graph Panel
    graphPanel.setBounds(graphArea);
    
    juce::Rectangle<int> switchArea = controlAreaMid.removeFromLeft(SWITCH_WIDTH);
    
    oscSwitch.setBounds(switchArea.removeFromTop(switchArea.getHeight() / 4));
    shapeSwitch.setBounds(switchArea.removeFromTop(switchArea.getHeight() / 3));
    compressorSwitch.setBounds(switchArea.removeFromTop(switchArea.getHeight() / 2));
    widthSwitch.setBounds(switchArea);
    
    filterSwitch.setBounds(oscSwitch.getX(), oscSwitch.getY(), SWITCH_WIDTH, oscSwitch.getHeight() * 2);
    otherSwitch.setBounds(compressorSwitch.getX(), compressorSwitch.getY(), SWITCH_WIDTH, oscSwitch.getHeight() * 2);
    
    juce::Rectangle<int> controlLeftKnobLeftArea = controlAreaMid.removeFromLeft(getWidth() / 7 * 2);
    juce::Rectangle<int> controlLeftKnobRightArea = controlLeftKnobLeftArea.removeFromRight(getWidth() / 7);
    
    controlLeftKnobLeftArea.removeFromTop(controlLeftKnobLeftArea.getHeight() / 4);
    controlLeftKnobLeftArea.removeFromBottom(controlLeftKnobLeftArea.getHeight() / 5);

    
    controlLeftKnobRightArea.removeFromTop(controlLeftKnobRightArea.getHeight() / 4);
    controlLeftKnobRightArea.removeFromBottom(controlLeftKnobRightArea.getHeight() / 5);

    // shape
    recKnob1->setBounds(controlLeftKnobLeftArea);
    recKnob2->setBounds(controlLeftKnobLeftArea);
    recKnob3->setBounds(controlLeftKnobLeftArea);
    recKnob4->setBounds(controlLeftKnobLeftArea);
    
    biasKnob1->setBounds(controlLeftKnobRightArea);
    biasKnob2->setBounds(controlLeftKnobRightArea);
    biasKnob3->setBounds(controlLeftKnobRightArea);
    biasKnob4->setBounds(controlLeftKnobRightArea);
//    recKnob1->setBounds(REC_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
//    recKnob2->setBounds(REC_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
//    recKnob3->setBounds(REC_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
//    recKnob4->setBounds(REC_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
//
//    biasKnob1->setBounds(BIAS_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
//    biasKnob2->setBounds(BIAS_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
//    biasKnob3->setBounds(BIAS_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
//    biasKnob4->setBounds(BIAS_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    
    // width
    widthKnob1->setBounds(WIDTH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    widthKnob2->setBounds(WIDTH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    widthKnob3->setBounds(WIDTH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    widthKnob4->setBounds(WIDTH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);

    // compressor
    compRatioKnob1->setBounds(COMP_RATIO_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    compRatioKnob2->setBounds(COMP_RATIO_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    compRatioKnob3->setBounds(COMP_RATIO_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    compRatioKnob4->setBounds(COMP_RATIO_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);

    compThreshKnob1->setBounds(COMP_THRESH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    compThreshKnob2->setBounds(COMP_THRESH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    compThreshKnob3->setBounds(COMP_THRESH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    compThreshKnob4->setBounds(COMP_THRESH_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);

    //colorKnob.setBounds(startX * 1 + scaledKnobSize * 1.2, secondLineY, scaledKnobSize, scaledKnobSize);
    downSampleKnob.setBounds(DOWNSAMPLE_X, firstPartY, scaledKnobSize, scaledKnobSize);
    lowcutFreqKnob.setBounds(CUTOFF_X, firstPartY, scaledKnobSize, scaledKnobSize);
    lowcutQKnob.setBounds(RES_X, firstPartY, scaledKnobSize, scaledKnobSize);
    highcutFreqKnob.setBounds(CUTOFF_X, firstPartY, scaledKnobSize, scaledKnobSize);
    highcutQKnob.setBounds(RES_X, firstPartY, scaledKnobSize, scaledKnobSize);
    peakFreqKnob.setBounds(CUTOFF_X, firstPartY, scaledKnobSize, scaledKnobSize);
    peakQKnob.setBounds(RES_X, firstPartY, scaledKnobSize, scaledKnobSize);
    peakGainKnob.setBounds((CUTOFF_X+RES_X)/2.0f, firstPartY, scaledKnobSize, scaledKnobSize);
    
    lowcutSlopeMode.setBounds(getWidth() / 2 - 20, secondShadowY + getHeight() / 20 + 10, OSC_WIDTH / 1.5f, getHeight() / 20);
    highcutSlopeMode.setBounds(getWidth() / 2 - 20, secondShadowY + getHeight() / 20 + 10, OSC_WIDTH / 1.5f, getHeight() / 20);

    
    mixKnob1.setBounds(MIX_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    mixKnob2.setBounds(MIX_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    mixKnob3.setBounds(MIX_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    mixKnob4.setBounds(MIX_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    mixKnob.setBounds(MIX_X_G, secondPartY, scaledKnobSize, scaledKnobSize);
    
    outputKnob1.setBounds(OUTPUT_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    outputKnob2.setBounds(OUTPUT_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    outputKnob3.setBounds(OUTPUT_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    outputKnob4.setBounds(OUTPUT_X, OSC_Y + OSC_HEIGHT / 2.0f, scaledKnobSize, scaledKnobSize);
    outputKnob.setBounds(OUTPUT_X_G, firstPartY, scaledKnobSize, scaledKnobSize);
    
    // buttons
    
    linkedButton1.setBounds(BUTTON_X, OSC_Y, BUTTON_WIDTH, OSC_HEIGHT);
    linkedButton2.setBounds(BUTTON_X, OSC_Y, BUTTON_WIDTH, OSC_HEIGHT);
    linkedButton3.setBounds(BUTTON_X, OSC_Y, BUTTON_WIDTH, OSC_HEIGHT);
    linkedButton4.setBounds(BUTTON_X, OSC_Y, BUTTON_WIDTH, OSC_HEIGHT);
    
    safeButton1.setBounds(BUTTON_X, OSC_Y + OSC_HEIGHT, BUTTON_WIDTH, OSC_HEIGHT);
    safeButton2.setBounds(BUTTON_X, OSC_Y + OSC_HEIGHT, BUTTON_WIDTH, OSC_HEIGHT);
    safeButton3.setBounds(BUTTON_X, OSC_Y + OSC_HEIGHT, BUTTON_WIDTH, OSC_HEIGHT);
    safeButton4.setBounds(BUTTON_X, OSC_Y + OSC_HEIGHT, BUTTON_WIDTH, OSC_HEIGHT);
    
    

    filterOffButton.setBounds(FILTER_STATE_X, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    filterPreButton.setBounds(FILTER_STATE_X, secondPartY + 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    filterPostButton.setBounds(FILTER_STATE_X, secondPartY + 0.11 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    filterLowPassButton.setBounds(FILTER_TYPE_X, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    filterPeakButton.setBounds(FILTER_TYPE_X, secondPartY + 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    filterHighPassButton.setBounds(FILTER_TYPE_X, secondPartY + 0.11 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());

    
    
    
    
    

    // set look and feel scale
    otherLookAndFeel.scale = scale;
    roundedButtonLnf.scale = scale;
    driveLookAndFeel1.scale = scale;
    driveLookAndFeel2.scale = scale;
    driveLookAndFeel3.scale = scale;
    driveLookAndFeel4.scale = scale;
    lowPassButtonLnf.scale = scale;
    bandPassButtonLnf.scale = scale;
    highPassButtonLnf.scale = scale;
}

void FireAudioProcessorEditor::updateToggleState()
{
    if (*processor.treeState.getRawParameterValue(OFF_ID))
    {
        filterLowPassButton.setEnabled(false);
        filterPeakButton.setEnabled(false);
        filterHighPassButton.setEnabled(false);
        lowcutFreqKnob.setEnabled(false);
        lowcutQKnob.setEnabled(false);
    }
    else
    {
        filterLowPassButton.setEnabled(true);
        filterPeakButton.setEnabled(true);
        filterHighPassButton.setEnabled(true);
        lowcutFreqKnob.setEnabled(true);
        lowcutQKnob.setEnabled(true);
    }
}

void FireAudioProcessorEditor::updateFreqArray()
{
    multiband.getFreqArray(multibandFreq);
    multiFreqSlider1.setValue(multibandFreq[0]);
    multiFreqSlider2.setValue(multibandFreq[1]);
    multiFreqSlider3.setValue(multibandFreq[2]);
}

void FireAudioProcessorEditor::timerCallback()
{
    // bypassed
    if (processor.getBypassedState())
    {
        graphPanel.repaint();
        multiband.repaint();
    }
    else if (processor.isFFTBlockReady())
    {
        // not bypassed, repaint at the same time
        //(1<<11)
        // create a temp ddtData because sometimes pushNextSampleIntoFifo will replace the original
        // fftData after doingProcess and before painting.
        float tempFFTData[2 * 2048] = {0};
        memmove(tempFFTData, processor.getFFTData(), sizeof(tempFFTData));
        // doing process, fifo data to fft data
        processor.processFFT(tempFFTData);
        // prepare to paint the spectrum
        spectrum.prepareToPaintSpectrum(processor.getFFTSize(), tempFFTData);

        graphPanel.repaint();
        spectrum.repaint();
        multiband.repaint();
        driveKnob1.repaint();
        driveKnob2.repaint();
        driveKnob3.repaint();
        driveKnob4.repaint();
        repaint();
    }
}

void FireAudioProcessorEditor::sliderValueChanged(juce::Slider *slider)
{
    linkValue(*slider, driveKnob1, outputKnob1, linkedButton1);
    linkValue(*slider, driveKnob2, outputKnob2, linkedButton2);
    linkValue(*slider, driveKnob3, outputKnob3, linkedButton3);
    linkValue(*slider, driveKnob4, outputKnob4, linkedButton4);

    // ableton move sliders
    if (slider == &multiFreqSlider1 && lineState[0])
    {
        if (!multiband.getMovingState())
        {
            multiband.dragLinesByFreq(multiFreqSlider1.getValue(), multiband.getSortedIndex(0));
            multiband.getFreqArray(multibandFreq);
            multiFreqSlider2.setValue(multibandFreq[1], juce::NotificationType::dontSendNotification);
            multiFreqSlider3.setValue(multibandFreq[2], juce::NotificationType::dontSendNotification);
        }
    }
    if (slider == &multiFreqSlider2 && lineState[1])
    {
        if (!multiband.getMovingState())
        {
            multiband.dragLinesByFreq(multiFreqSlider2.getValue(), multiband.getSortedIndex(1));
            multiband.getFreqArray(multibandFreq);
            multiFreqSlider1.setValue(multibandFreq[0], juce::NotificationType::dontSendNotification);
            multiFreqSlider3.setValue(multibandFreq[2], juce::NotificationType::dontSendNotification);
        }
    }
    if (slider == &multiFreqSlider3 && lineState[2])
    {
        if (!multiband.getMovingState())
        {
            multiband.dragLinesByFreq(multiFreqSlider3.getValue(), multiband.getSortedIndex(2));
            multiband.getFreqArray(multibandFreq);
            multiFreqSlider1.setValue(multibandFreq[0], juce::NotificationType::dontSendNotification);
            multiFreqSlider2.setValue(multibandFreq[1], juce::NotificationType::dontSendNotification);
        }
    }
}

void FireAudioProcessorEditor::linkValue(juce::Slider &xSlider, juce::Slider &driveSlider, juce::Slider &outputSlider, juce::TextButton& linkedButton)
{
    // x changes, then y will change
    if (linkedButton.getToggleState() == true)
    {
        if (&xSlider == &driveSlider)
        {
            outputSlider.setValue(-xSlider.getValue() * 0.1f); // use defalut notification type
        }
    }
}

void FireAudioProcessorEditor::buttonClicked(juce::Button *clickedButton)
{
    if (clickedButton == stateComponent.getToggleABButton())
    {
        stateComponent.getProcStateAB()->toggleAB();

        if (clickedButton->getButtonText() == "A")
            clickedButton->setButtonText("B");
        else
            clickedButton->setButtonText("A");
        initState();
    }
    
//    if (clickedButton == &copyABButton)
//        procStateAB.copyAB();
//    if (clickedButton == &previousButton)
//        setPreviousPreset();
//    if (clickedButton == &nextButton)
//        setNextPreset();
//    if (clickedButton == &savePresetButton)
//        savePresetAlertWindow();
//    //if (clickedButton == &deletePresetButton)
//    //    deletePresetAndRefresh();
//    if (clickedButton == &menuButton)
//        //openPresetFolder();
//        popPresetMenu();
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox *combobox)
{
    if (combobox == &distortionMode1 || combobox == &distortionMode2
        || combobox == &distortionMode3 || combobox == &distortionMode4)
    {
        changeSliderState(combobox);
    }
    if (combobox == stateComponent.getPresetBox())
    {
        int selectedId = combobox->getSelectedId();

        // do this because open and close GUI will use this function, but will reset the value if the presetbox is not "init"
        // next, previous, change combobox will change the selectedId, but currentId will change only after this.
        // and then, it will load the preset.
        
        if (stateComponent.getProcStatePresets()->getCurrentPresetId() != selectedId)
        {
            stateComponent.updatePresetBox(selectedId);
        }
        initState(); // see line 598
    }
}

void FireAudioProcessorEditor::initState()
{
    // init
    setMultiband();
    updateToggleState();
    changeSliderState(&distortionMode1);
    changeSliderState(&distortionMode2);
    changeSliderState(&distortionMode3);
    changeSliderState(&distortionMode4);
}

void FireAudioProcessorEditor::changeSliderState(juce::ComboBox *combobox)
{
    if (combobox == &distortionMode1)
    {
        setSliderState(&processor, driveKnob1, MODE_ID1, tempDriveValue[0]);
        setSliderState(&processor, *biasKnob1, MODE_ID1, tempBiasValue[0]);
    }
    else if (combobox == &distortionMode2)
    {
        setSliderState(&processor, driveKnob2, MODE_ID2, tempDriveValue[1]);
        setSliderState(&processor, *biasKnob2, MODE_ID2, tempBiasValue[1]);
    }
    else if (combobox == &distortionMode3)
    {
        setSliderState(&processor, driveKnob3, MODE_ID3, tempDriveValue[2]);
        setSliderState(&processor, *biasKnob3, MODE_ID3, tempBiasValue[2]);
    }
    else if (combobox == &distortionMode4)
    {
        setSliderState(&processor, driveKnob4, MODE_ID4, tempDriveValue[3]);
        setSliderState(&processor, *biasKnob4, MODE_ID4, tempBiasValue[3]);
    }
}

void FireAudioProcessorEditor::setSliderState(FireAudioProcessor* processor, juce::Slider& slider, juce::String paramId, float &tempValue)
{
    auto val = processor->treeState.getRawParameterValue(paramId);
    int selection = val->load();
    if (selection == 0)
    {
        tempValue = slider.getValue();
//        if (&slider == &driveKnob1 || &slider == &driveKnob2 || &slider == &driveKnob3 || &slider == &driveKnob4)
//        {
//            slider.setValue(1);
//        }
//        else if (&slider == &biasKnob1 || &slider == &biasKnob2 || &slider == &biasKnob3 || &slider == &biasKnob4)
//        {
//            slider.setValue(0);
//        }
        
        slider.setValue(0);
        slider.setEnabled(false);
    }
    else if (stateComponent.getChangedState())
    {
        slider.setEnabled(true);
    }
    else if (!slider.isEnabled())
    {
        slider.setValue(tempValue);
        slider.setEnabled(true);
    }
}

void FireAudioProcessorEditor::setMenu(juce::ComboBox* combobox)
{
    // Distortion mode select
    addAndMakeVisible(combobox);

    combobox->addItem("None", 1);
    combobox->addSeparator();

    combobox->addSectionHeading("Soft Clipping");
    combobox->addItem("Arctan", 2);
    combobox->addItem("Exp", 3);
    combobox->addItem("Tanh", 4);
    combobox->addItem("Cubic", 5);
    combobox->addSeparator();

    combobox->addSectionHeading("Hard Clipping");
    combobox->addItem("Hard", 6);
    combobox->addItem("Sausage", 7);
    combobox->addSeparator();

    combobox->addSectionHeading("Foldback");
    combobox->addItem("Sin", 8);
    combobox->addItem("Linear", 9);
    combobox->addSeparator();

    combobox->addSectionHeading("Asymmetrical Clipping");
    combobox->addItem("Diode 1 (beta)", 10);
    combobox->addSeparator();

    combobox->setJustificationType(juce::Justification::centred);
    combobox->addListener(this);
}

void FireAudioProcessorEditor::setListenerKnob(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.addListener(this);
}

void FireAudioProcessorEditor::setRotarySlider(juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
}

void FireAudioProcessorEditor::setLinearSlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
}

void FireAudioProcessorEditor::setFourKnobsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum)
{
    if (bandNum == 1)
    {
        component1.setVisible(true);
        component2.setVisible(false);
        component3.setVisible(false);
        component4.setVisible(false);
    }
    else if (bandNum == 2)
    {
        component1.setVisible(false);
        component2.setVisible(true);
        component3.setVisible(false);
        component4.setVisible(false);
    }
    else if (bandNum == 3)
    {
        component1.setVisible(false);
        component2.setVisible(false);
        component3.setVisible(true);
        component4.setVisible(false);
    }
    else if (bandNum == 4)
    {
        component1.setVisible(false);
        component2.setVisible(false);
        component3.setVisible(false);
        component4.setVisible(true);
    }
}

void FireAudioProcessorEditor::setRoundButton(juce::TextButton& button, juce::String paramId, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    bool state = *processor.treeState.getRawParameterValue(paramId);
    button.setToggleState(state, juce::dontSendNotification);
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

void FireAudioProcessorEditor::setFlatButton(juce::TextButton& button, juce::String paramId, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    bool state = *processor.treeState.getRawParameterValue(paramId);
    button.setToggleState(state, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonColourId, COLOUR7);
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, COLOUR1);
    button.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
    button.setButtonText(buttonName);
    button.setLookAndFeel(&flatButtonLnf);
}

void FireAudioProcessorEditor::setDistortionGraph(juce::String modeId, juce::String driveId, 
    juce::String recId, juce::String mixId, juce::String biasId)
{
    // paint distortion function
    int mode = static_cast<int>(*processor.treeState.getRawParameterValue(modeId));
    float drive = processor.getNewDrive(driveId);
    float rec = static_cast<float>(*processor.treeState.getRawParameterValue(recId));
    float mix = static_cast<float>(*processor.treeState.getRawParameterValue(mixId));
    float bias = static_cast<float>(*processor.treeState.getRawParameterValue(biasId));
    float rateDivide = static_cast<float>(*processor.treeState.getRawParameterValue(DOWNSAMPLE_ID));

    graphPanel.setDistortionState(mode, rec, mix, bias, drive, rateDivide);
}

void FireAudioProcessorEditor::setMultiband()
{
    int freq1 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID1));
    int freq2 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID2));
    int freq3 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID3));
    multiband.setFrequency(freq1, freq2, freq3);

    bool lineState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID1));
    bool lineState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID2));
    bool lineState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID3));
    multiband.setLineState(lineState1, lineState2, lineState3);

    float pos1 = static_cast<float>(SpectrumComponent::transformToLog(freq1 / (44100 / 2.0)));
    float pos2 = static_cast<float>(SpectrumComponent::transformToLog(freq2 / (44100 / 2.0)));
    float pos3 = static_cast<float>(SpectrumComponent::transformToLog(freq3 / (44100 / 2.0)));
    multiband.setLinePos(pos1, pos2, pos3);
    
    bool enableState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID1));
    bool enableState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID2));
    bool enableState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID3));
    bool enableState4 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID4));
    multiband.setEnableState(enableState1, enableState2, enableState3, enableState4);
    
    multiband.updateLines("reset", -1);
    multiband.setCloseButtonState();
    multiband.setFocus();
    processor.setLineNum(multiband.getLineNum());
}

void FireAudioProcessorEditor::setInvisible(juce::OwnedArray<juce::Component, juce::CriticalSection> &array)
{
    for (int i = 0; i < array.size(); ++i)
    {
        array[i]->setVisible(false);
    }
}
