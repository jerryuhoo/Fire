/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p), stateComponent{p.stateAB, p.statePresets, multiband}
{
    // timer
    juce::Timer::startTimerHz(30.0f);

    // This is not a perfect fix for Vst3 plugins
    // Vst3 calls constructor before setStateInformation in processor,
    // however, AU plugin calls constructor after setStateInformation/
    // So I set delay of 1 ms to reset size and other stuff.
    // call function after 1 ms
    std::function<void()> initFunction = [this]() { initEditor(); };
    juce::Timer::callAfterDelay(1, initFunction);
    
    // Visualiser
    addAndMakeVisible(oscilloscope);
    
    // Distortion graph
    addAndMakeVisible(distortionGraph);
    
    // Width graph
    addAndMakeVisible(widthGraph);
    
    // Spectrum
    addAndMakeVisible(spectrum);
    addAndMakeVisible(multiband);
    
    setMultiband();

    spectrum.setInterceptsMouseClicks(false, false);
    spectrum.prepareToPaintSpectrum(processor.getFFTSize(), processor.getFFTData());
    
    // presets
    addAndMakeVisible(stateComponent);
    lastPresetName = stateComponent.getPresetName();

    // ff meter
    //lnf = std::make_unique<foleys::LevelMeterLookAndFeel>();
    // adjust the colours to how you like them, e.g.
    lnf.setColour(foleys::LevelMeter::lmTextColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmTextClipColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmTextDeactiveColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmTicksColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmOutlineColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmBackgroundColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmBackgroundClipColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmMeterForegroundColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmMeterOutlineColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmMeterBackgroundColour, COLOUR0.withAlpha(0.f));
    lnf.setColour(foleys::LevelMeter::lmMeterMaxNormalColour, COLOUR3);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxWarnColour, COLOUR5);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxOverColour, COLOUR5);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientLowColour, COLOUR1);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMidColour, COLOUR1);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMaxColour, COLOUR5);
    lnf.setColour(foleys::LevelMeter::lmMeterReductionColour, COLOUR3);

    inputMeter.setLookAndFeel(&lnf);
    inputMeter.setMeterSource(&processor.getInputMeterSource());
    addAndMakeVisible(inputMeter);

    outputMeter.setLookAndFeel(&lnf);
    outputMeter.setMeterSource(&processor.getOutputMeterSource());
    addAndMakeVisible(outputMeter);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    //setSize(INIT_WIDTH, INIT_HEIGHT);

    setLookAndFeel(&otherLookAndFeel);

    // drive knob 1
    driveKnob1.setLookAndFeel(&driveLookAndFeel);
    driveKnob2.setLookAndFeel(&driveLookAndFeel);
    driveKnob3.setLookAndFeel(&driveLookAndFeel);
    driveKnob4.setLookAndFeel(&driveLookAndFeel);
    setListenerKnob(driveKnob1);
    setListenerKnob(driveKnob2);
    setListenerKnob(driveKnob3);
    setListenerKnob(driveKnob4);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    driveLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    driveLabel.attachToComponent(&driveKnob1, false);
    driveLabel.setJustificationType(juce::Justification::centred);

    
    // output knob
    setListenerKnob(outputKnob1);
    setListenerKnob(outputKnob2);
    setListenerKnob(outputKnob3);
    setListenerKnob(outputKnob4);
    setRotarySlider(outputKnob);
    
    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent(&outputKnob1, false);
    outputLabel.setJustificationType(juce::Justification::centred);
    
    // mix knob
    setRotarySlider(mixKnob1);
    setRotarySlider(mixKnob2);
    setRotarySlider(mixKnob3);
    setRotarySlider(mixKnob4);
    setRotarySlider(mixKnob);
    
    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent(&mixKnob1, false);
    mixLabel.setJustificationType(juce::Justification::centred);
    
    // compressor ratio knob
    setLinearSlider(compRatioKnob1);
    setLinearSlider(compRatioKnob2);
    setLinearSlider(compRatioKnob3);
    setLinearSlider(compRatioKnob4);

    addAndMakeVisible(CompRatioLabel);
    CompRatioLabel.setText("Ratio", juce::dontSendNotification);
    CompRatioLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompRatioLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    CompRatioLabel.attachToComponent(&compRatioKnob1, false);
    CompRatioLabel.setJustificationType(juce::Justification::centred);
    
    // compressor ratio knob
    setLinearSlider(compThreshKnob1);
    setLinearSlider(compThreshKnob2);
    setLinearSlider(compThreshKnob3);
    setLinearSlider(compThreshKnob4);

    addAndMakeVisible(CompThreshLabel);
    CompThreshLabel.setText("Threshold", juce::dontSendNotification);
    CompThreshLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompThreshLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    CompThreshLabel.attachToComponent(&compThreshKnob1, false);
    CompThreshLabel.setJustificationType(juce::Justification::centred);
    
    // width knob
    setRotarySlider(widthKnob1);
    setRotarySlider(widthKnob2);
    setRotarySlider(widthKnob3);
    setRotarySlider(widthKnob4);

    addAndMakeVisible(widthLabel);
    widthLabel.setText("Width", juce::dontSendNotification);
    widthLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    widthLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    widthLabel.attachToComponent(&widthKnob1, false);
    widthLabel.setJustificationType(juce::Justification::centred);
    
    // color knob
    addAndMakeVisible(colorKnob);
    colorKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    colorKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(colorLabel);
    colorLabel.setText("Color", juce::dontSendNotification);
    colorLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    colorLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    colorLabel.attachToComponent(&colorKnob, false);
    colorLabel.setJustificationType(juce::Justification::centred);

    // bias knob
    setRotarySlider(biasKnob1);
    setRotarySlider(biasKnob2);
    setRotarySlider(biasKnob3);
    setRotarySlider(biasKnob4);
    
    addAndMakeVisible(biasLabel);
    biasLabel.setText("Bias", juce::dontSendNotification);
    biasLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    biasLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    biasLabel.attachToComponent(&biasKnob1, false);
    biasLabel.setJustificationType(juce::Justification::centred);

    // downsample knob
    addAndMakeVisible(downSampleKnob);
    downSampleKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    downSampleKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", juce::dontSendNotification);
    downSampleLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    downSampleLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(juce::Justification::centred);

    // rec knob
    setRotarySlider(recKnob1);
    setRotarySlider(recKnob2);
    setRotarySlider(recKnob3);
    setRotarySlider(recKnob4);

    addAndMakeVisible(recLabel);
    recLabel.setText("Rectification", juce::dontSendNotification);
    recLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    recLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    recLabel.attachToComponent(&recKnob1, false);
    recLabel.setJustificationType(juce::Justification::centred);

    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    cutoffLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    cutoffLabel.attachToComponent(&cutoffKnob, false);
    cutoffLabel.setJustificationType(juce::Justification::centred);

    // res knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(resLabel);
    resLabel.setText("Q", juce::dontSendNotification); // Resonance
    resLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    resLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    resLabel.attachToComponent(&resKnob, false);
    resLabel.setJustificationType(juce::Justification::centred);

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
    setRoundButton(linkedButton1, LINKED_ID1, "LINK");
    setRoundButton(linkedButton2, LINKED_ID2, "LINK");
    setRoundButton(linkedButton3, LINKED_ID3, "LINK");
    setRoundButton(linkedButton4, LINKED_ID4, "LINK");
    
    // safe overload Button
    setRoundButton(safeButton1, SAFE_ID1, "SAFE");
    setRoundButton(safeButton2, SAFE_ID2, "SAFE");
    setRoundButton(safeButton3, SAFE_ID3, "SAFE");
    setRoundButton(safeButton4, SAFE_ID4, "SAFE");
  
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
    setRoundButton(filterLowButton, LOW_ID, "LP");
    setRoundButton(filterBandButton, BAND_ID, "BP");
    setRoundButton(filterHighButton, HIGH_ID, "HP");
    
    filterLowButton.setRadioGroupId(filterModeButtons);
    filterLowButton.onClick = [this] { updateToggleState(); };
  
    filterBandButton.setRadioGroupId(filterModeButtons);
    filterBandButton.onClick = [this] { updateToggleState(); };

    filterHighButton.setRadioGroupId(filterModeButtons);
    filterHighButton.onClick = [this] { updateToggleState(); };
  
    addAndMakeVisible(filterTypeLabel);
    filterTypeLabel.setText("Type", juce::dontSendNotification);
    filterTypeLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    filterTypeLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    filterTypeLabel.attachToComponent(&filterLowButton, false);
    filterTypeLabel.setJustificationType(juce::Justification::centred);
    
    // Window Left Button
    addAndMakeVisible(windowLeftButton);
    windowLeftButton.setClickingTogglesState(true);
    windowLeftButton.setRadioGroupId(windowButtons);
    windowLeftButton.setButtonText("Band Effect");
    windowLeftButton.setToggleState(true, juce::NotificationType::dontSendNotification);
    windowLeftButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowLeftButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowLeftButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.f));
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
    windowRightButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.f));
    windowRightButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowRightButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    windowRightButton.setLookAndFeel(&otherLookAndFeel);
    
    // Attachment
    //inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    driveAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID1, driveKnob1);
    driveAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID2, driveKnob2);
    driveAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID3, driveKnob3);
    driveAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID4, driveKnob4);
    
    compRatioAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID1, compRatioKnob1);
    compRatioAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID2, compRatioKnob2);
    compRatioAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID3, compRatioKnob3);
    compRatioAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID4, compRatioKnob4);
    
    compThreshAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID1, compThreshKnob1);
    compThreshAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID2, compThreshKnob2);
    compThreshAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID3, compThreshKnob3);
    compThreshAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID4, compThreshKnob4);
    
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
    
    biasAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID1, biasKnob1);
    biasAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID2, biasKnob2);
    biasAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID3, biasKnob3);
    biasAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID4, biasKnob4);
    
    recAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID1, recKnob1);
    recAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID2, recKnob2);
    recAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID3, recKnob3);
    recAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID4, recKnob4);
    
    colorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COLOR_ID, colorKnob);
    
    downSampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DOWNSAMPLE_ID, downSampleKnob);

    
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, CUTOFF_ID, cutoffKnob);
    resAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, RES_ID, resKnob);

    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HQ_ID, hqButton);
    
    linkedAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID1, linkedButton1);
    linkedAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID2, linkedButton2);
    linkedAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID3, linkedButton3);
    linkedAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID4, linkedButton4);
    
    safeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID1, safeButton1);
    safeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID2, safeButton2);
    safeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID3, safeButton3);
    safeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID4, safeButton4);
    
    widthAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID1, widthKnob1);
    widthAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID2, widthKnob2);
    widthAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID3, widthKnob3);
    widthAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID4, widthKnob4);
    
    filterOffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, OFF_ID, filterOffButton);
    filterPreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, PRE_ID, filterPreButton);
    filterPostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, POST_ID, filterPostButton);
    filterLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LOW_ID, filterLowButton);
    filterBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ID, filterBandButton);
    filterHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HIGH_ID, filterHighButton);
    
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
    
    linePosSliderAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINEPOS_ID1, linePosSlider1);
    linePosSliderAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINEPOS_ID2, linePosSlider2);
    linePosSliderAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINEPOS_ID3, linePosSlider3);

    setMenu(&distortionMode1);
    setMenu(&distortionMode2);
    setMenu(&distortionMode3);
    setMenu(&distortionMode4);

    // use global lookandfeel
    getLookAndFeel().setColour(juce::ComboBox::textColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::arrowColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::buttonColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::outlineColourId, COLOUR7);
    getLookAndFeel().setColour(juce::ComboBox::focusedOutlineColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::backgroundColourId, COLOUR7);
    getLookAndFeel().setColour(juce::PopupMenu::textColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedBackgroundColourId, COLOUR5);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedTextColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::headerTextColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::backgroundColourId, COLOUR6);

    modeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID1, distortionMode1);
    modeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID2, distortionMode2);
    modeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID3, distortionMode3);
    modeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID4, distortionMode4);
    
    // set resize
    setResizable(true, true);
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    // resize limit
    setResizeLimits(INIT_WIDTH, INIT_HEIGHT, 2000, 1000); // set resize limits
    // getConstrainer ()->setFixedAspectRatio (2); // set fixed resize rate

    updateToggleState();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    inputMeter.setLookAndFeel(nullptr);
    outputMeter.setLookAndFeel(nullptr);
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
    filterLowButton.setLookAndFeel(nullptr);
    filterBandButton.setLookAndFeel(nullptr);
    filterHighButton.setLookAndFeel(nullptr);
    windowRightButton.setLookAndFeel(nullptr);
    windowLeftButton.setLookAndFeel(nullptr);
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
    /*
    // paint distortion function
    int mode = *processor.treeState.getRawParameterValue(MODE_ID1);
    float drive = *processor.treeState.getRawParameterValue(DRIVE_ID1);//TODO: replace it!
//    float drive = processor.getNewDrive();
    float color = *processor.treeState.getRawParameterValue(COLOR_ID);
    float rec = *processor.treeState.getRawParameterValue(REC_ID1);
    float mix = *processor.treeState.getRawParameterValue(MIX_ID1);
    float bias = *processor.treeState.getRawParameterValue(BIAS_ID1);
    float rateDivide = *processor.treeState.getRawParameterValue(DOWNSAMPLE_ID);
    
    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;
    */
    auto frame = getLocalBounds();
    frame.setBounds(0, part1, getWidth(), part2);
    
    // draw layer 2
    g.setColour(COLOUR6);
    g.fillRect(0, part1, getWidth(), part2);
    
    //distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
    
    // DBG(lastPresetName);
    if (isPresetChanged()) // preset change
    {
        initState();
    }
    if (stateComponent.getInitState()) // click init
    {
        initState();
        stateComponent.setInitState(false);
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

        multiband.getFreqArray(multibandFreq);
        multiFreqSlider1.setValue(multibandFreq[0]);
        multiFreqSlider2.setValue(multibandFreq[1]);
        multiFreqSlider3.setValue(multibandFreq[2]);

        multiband.getLinePos(linePos);
        linePosSlider1.setValue(linePos[0]);
        linePosSlider2.setValue(linePos[1]);
        linePosSlider3.setValue(linePos[2]);

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
    
    if (left) { // if you select the left window, you will see audio wave and distortion function graphs.
//        spectrum.setVisible(false);
//        multiband.setVisible(false);
//
//        distortionGraph.setVisible(true);
//        distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
//        oscilloscope.setVisible(true);

        setFourKnobsVisibility(driveKnob1, driveKnob2, driveKnob3, driveKnob4, bandNum);
        setFourKnobsVisibility(compRatioKnob1, compRatioKnob2, compRatioKnob3, compRatioKnob4, bandNum);
        setFourKnobsVisibility(compThreshKnob1, compThreshKnob2, compThreshKnob3, compThreshKnob4, bandNum);
        setFourKnobsVisibility(outputKnob1, outputKnob2, outputKnob3, outputKnob4, bandNum);
        setFourKnobsVisibility(mixKnob1, mixKnob2, mixKnob3, mixKnob4, bandNum);
        setFourKnobsVisibility(widthKnob1, widthKnob2, widthKnob3, widthKnob4, bandNum);
        setFourKnobsVisibility(recKnob1, recKnob2, recKnob3, recKnob4, bandNum);
        setFourKnobsVisibility(biasKnob1, biasKnob2, biasKnob3, biasKnob4, bandNum);
        setFourKnobsVisibility(linkedButton1, linkedButton2, linkedButton3, linkedButton4, bandNum);
        setFourKnobsVisibility(safeButton1, safeButton2, safeButton3, safeButton4, bandNum);
        
        driveLabel.setVisible(true);
        outputLabel.setVisible(true);
        mixLabel.setVisible(true);
        CompRatioLabel.setVisible(true);
        CompThreshLabel.setVisible(true);
        widthLabel.setVisible(true);
        recLabel.setVisible(true);
        biasLabel.setVisible(true);
        
        // set other effects invisible
        downSampleKnob.setVisible(false);
        resKnob.setVisible(false);
        filterLowButton.setVisible(false);
        filterOffButton.setVisible(false);
        filterPreButton.setVisible(false);
        filterBandButton.setVisible(false);
        filterPostButton.setVisible(false);
        filterHighButton.setVisible(false);
        colorKnob.setVisible(false);
        cutoffKnob.setVisible(false);
        mixKnob.setVisible(false);
        outputKnob.setVisible(false); 
    }
    else if (right) // if you select the left window, you can use muti-band distortion
    {
        // set other effects visible
        
        downSampleKnob.setVisible(true);
        resKnob.setVisible(true);
        filterLowButton.setVisible(true);
        filterOffButton.setVisible(true);
        filterPreButton.setVisible(true);
        filterBandButton.setVisible(true);
        filterPostButton.setVisible(true);
        filterHighButton.setVisible(true);
        colorKnob.setVisible(true);
        cutoffKnob.setVisible(true);
        mixKnob.setVisible(true);
        outputKnob.setVisible(true);
        
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
        
        compRatioKnob1.setVisible(false);
        compRatioKnob2.setVisible(false);
        compRatioKnob3.setVisible(false);
        compRatioKnob4.setVisible(false);
        
        compThreshKnob1.setVisible(false);
        compThreshKnob2.setVisible(false);
        compThreshKnob3.setVisible(false);
        compThreshKnob4.setVisible(false);
        
        widthKnob1.setVisible(false);
        widthKnob2.setVisible(false);
        widthKnob3.setVisible(false);
        widthKnob4.setVisible(false);
        
        linkedButton1.setVisible(false);
        linkedButton2.setVisible(false);
        linkedButton3.setVisible(false);
        linkedButton4.setVisible(false);
        
        safeButton1.setVisible(false);
        safeButton2.setVisible(false);
        safeButton3.setVisible(false);
        safeButton4.setVisible(false);
        
        recKnob1.setVisible(false);
        recKnob2.setVisible(false);
        recKnob3.setVisible(false);
        recKnob4.setVisible(false);
        
        biasKnob1.setVisible(false);
        biasKnob2.setVisible(false);
        biasKnob3.setVisible(false);
        biasKnob4.setVisible(false);
        
        driveLabel.setVisible(false);
        outputLabel.setVisible(false);
        mixLabel.setVisible(true);
        CompRatioLabel.setVisible(false);
        CompThreshLabel.setVisible(false);
        widthLabel.setVisible(false);
        recLabel.setVisible(false);
        biasLabel.setVisible(false);
        outputLabel.setVisible(true);

//        spectrum.prepareToPaintSpectrum(processor.getFFTSize(), processor.getFFTData());   
    }
    
    // draw shadow 1
    //juce::ColourGradient shadowGrad1(juce::Colour(0, 0, 0).withAlpha(0.5f), 0, 50,
    //                                 juce::Colour(0, 0, 0).withAlpha(0.f), 0, 55, false);
    //g.setGradientFill(shadowGrad1);
    //g.fillRect(0, part1, getWidth(), 25);

    // draw shadow 2
    //juce::ColourGradient shadowGrad2(juce::Colour(0, 0, 0).withAlpha(0.5f), getWidth() / 2.f, getHeight() / 2.f - 75,
    //                                 juce::Colour(0, 0, 0).withAlpha(0.f), getWidth() / 2.f, getHeight() / 2.f - 70, false);
    
    //g.setGradientFill(shadowGrad2);
    //g.fillRect(0, part1 + part2, getWidth(), 25);

    //g.setColour(COLOUR1);
    //g.fillRect(outputKnob.getX(), outputKnob.getY(), outputKnob.getWidth(), outputKnob.getHeight());
    //g.fillRect(cutoffKnob.getX(), cutoffKnob.getY(), cutoffKnob.getWidth(), cutoffKnob.getHeight());
    
    
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
    int knobNum = 8;
    float scale = juce::jmin(getHeight() / INIT_HEIGHT, getWidth() / INIT_WIDTH);
    float scaleMax = juce::jmax(getHeight() / INIT_HEIGHT, getWidth() / INIT_WIDTH);
    int scaledKnobSize = SCALED_KNOBSIZE;
    int startX = getWidth() / knobNum;
    int secondShadowY = getHeight() / 10 * 4;
    int firstPartY = secondShadowY + (getHeight() - secondShadowY) * 2 / 5 - scaledKnobSize / 2;
    int secondPartY = secondShadowY + (getHeight() - secondShadowY) * 4 / 5 - scaledKnobSize / 2;

    
    // save presets
    juce::Rectangle<int> r(getLocalBounds());
    r = r.removeFromTop(50 * getHeight() / INIT_HEIGHT);
    r.removeFromLeft(100 * scaleMax);
    r.removeFromRight(50 * scaleMax);

    stateComponent.setBounds(r);

    // first line
    // inputKnob.setBounds(startX * 1 - scaledKnobSize / 2, firstLineY, scaledKnobSize, scaledKnobSize);
    float driveX = DRIVE_X;
    float driveY = DRIVE_Y;
    float driveSize = scaledKnobSize * 2;
    driveKnob1.setBounds(driveX, driveY, driveSize, driveSize);
    driveKnob2.setBounds(driveX, driveY, driveSize, driveSize);
    driveKnob3.setBounds(driveX, driveY, driveSize, driveSize);
    driveKnob4.setBounds(driveX, driveY, driveSize, driveSize);
    
    //colorKnob.setBounds(startX * 1 + scaledKnobSize * 1.2, secondLineY, scaledKnobSize, scaledKnobSize);
    downSampleKnob.setBounds(DOWNSAMPLE_X, firstPartY, scaledKnobSize, scaledKnobSize);
    
    recKnob1.setBounds(REC_X, firstPartY, scaledKnobSize, scaledKnobSize);
    recKnob2.setBounds(REC_X, firstPartY, scaledKnobSize, scaledKnobSize);
    recKnob3.setBounds(REC_X, firstPartY, scaledKnobSize, scaledKnobSize);
    recKnob4.setBounds(REC_X, firstPartY, scaledKnobSize, scaledKnobSize);
    
    colorKnob.setBounds(COLOR_X, secondPartY, scaledKnobSize, scaledKnobSize);
    
    biasKnob1.setBounds(BIAS_X, secondPartY, scaledKnobSize, scaledKnobSize);
    biasKnob2.setBounds(BIAS_X, secondPartY, scaledKnobSize, scaledKnobSize);
    biasKnob3.setBounds(BIAS_X, secondPartY, scaledKnobSize, scaledKnobSize);
    biasKnob4.setBounds(BIAS_X, secondPartY, scaledKnobSize, scaledKnobSize);
    
    widthKnob1.setBounds(WIDTH_X, firstPartY, scaledKnobSize, scaledKnobSize);
    widthKnob2.setBounds(WIDTH_X, firstPartY, scaledKnobSize, scaledKnobSize);
    widthKnob3.setBounds(WIDTH_X, firstPartY, scaledKnobSize, scaledKnobSize);
    widthKnob4.setBounds(WIDTH_X, firstPartY, scaledKnobSize, scaledKnobSize);
    
    
    outputKnob1.setBounds(OUTPUT_X, firstPartY, scaledKnobSize, scaledKnobSize);
    outputKnob2.setBounds(OUTPUT_X, firstPartY, scaledKnobSize, scaledKnobSize);
    outputKnob3.setBounds(OUTPUT_X, firstPartY, scaledKnobSize, scaledKnobSize);
    outputKnob4.setBounds(OUTPUT_X, firstPartY, scaledKnobSize, scaledKnobSize);
    outputKnob.setBounds(OUTPUT_X, firstPartY, scaledKnobSize, scaledKnobSize);

    // second line
    compRatioKnob1.setBounds(COMP_RATIO_X, secondPartY, scaledKnobSize, scaledKnobSize);
    compRatioKnob2.setBounds(COMP_RATIO_X, secondPartY, scaledKnobSize, scaledKnobSize);
    compRatioKnob3.setBounds(COMP_RATIO_X, secondPartY, scaledKnobSize, scaledKnobSize);
    compRatioKnob4.setBounds(COMP_RATIO_X, secondPartY, scaledKnobSize, scaledKnobSize);
    
    // second line
    compThreshKnob1.setBounds(COMP_THRESH_X, secondPartY, scaledKnobSize, scaledKnobSize);
    compThreshKnob2.setBounds(COMP_THRESH_X, secondPartY, scaledKnobSize, scaledKnobSize);
    compThreshKnob3.setBounds(COMP_THRESH_X, secondPartY, scaledKnobSize, scaledKnobSize);
    compThreshKnob4.setBounds(COMP_THRESH_X, secondPartY, scaledKnobSize, scaledKnobSize);
    
    cutoffKnob.setBounds(CUTOFF_X, firstPartY, scaledKnobSize, scaledKnobSize);
    resKnob.setBounds(RES_X, firstPartY, scaledKnobSize, scaledKnobSize);
    
    mixKnob1.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob2.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob3.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob4.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    
    // first line
    hqButton.setBounds(getHeight() / 10, 0, getHeight() / 10, getHeight() / 10);
    linkedButton1.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY - 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    linkedButton2.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY - 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    linkedButton3.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY - 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    linkedButton4.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY - 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    
    float windowHeight = getHeight() / 20;
    windowLeftButton.setBounds(0, secondShadowY, getWidth() / 2, windowHeight);
    windowRightButton.setBounds(getWidth() / 2, secondShadowY, getWidth() / 2, windowHeight);
    
    // second line
    safeButton1.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    safeButton2.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    safeButton3.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    safeButton4.setBounds(DRIVE_X - scaledKnobSize / 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    filterOffButton.setBounds(FILTER_STATE_X, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    filterPreButton.setBounds(FILTER_STATE_X, secondPartY + 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    filterPostButton.setBounds(FILTER_STATE_X, secondPartY + 0.11 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    filterLowButton.setBounds(FILTER_TYPE_X, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    filterBandButton.setBounds(FILTER_TYPE_X, secondPartY + 0.055 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());
    filterHighButton.setBounds(FILTER_TYPE_X, secondPartY + 0.11 * getHeight(), scaledKnobSize / 2, 0.05 * getHeight());

    // visualiser
    oscilloscope.setBounds(OSC_X, OSC_Y, OSC_WIDTH, OSC_HEIGHT);
    distortionGraph.setBounds(D_GRAPH_X, D_GRAPH_Y, D_GRAPH_WIDTH, D_GRAPH_HEIGHT);
    //widthGraph.setBounds(WIDTH_GRAPH_X, WIDTH_GRAPH_Y, WIDTH_GRAPH_WIDTH, WIDTH_GRAPH_HEIGHT);
    
    // spectrum
    spectrum.setBounds(SPEC_X, SPEC_Y, SPEC_WIDTH, SPEC_HEIGHT);
    multiband.setBounds(SPEC_X, SPEC_Y, SPEC_WIDTH, SPEC_HEIGHT);
    
    // ff meter
//    int ffWidth = 20;
//    int ffHeightStart = getHeight() / 10;
//    int ffHeight = getHeight() / 10 * 3;
//    inputMeter.setBounds(getWidth() / 2 - ffWidth - 2, ffHeightStart, ffWidth, ffHeight + 2);
//    outputMeter.setBounds(getWidth() / 2 + 2, ffHeightStart, ffWidth, ffHeight + 2);

    // distortion menu
    distortionMode1.setBounds(0, secondShadowY + windowHeight, getWidth() / 5, getHeight() / 12);
    distortionMode2.setBounds(0, secondShadowY + windowHeight, getWidth() / 5, getHeight() / 12);
    distortionMode3.setBounds(0, secondShadowY + windowHeight, getWidth() / 5, getHeight() / 12);
    distortionMode4.setBounds(0, secondShadowY + windowHeight, getWidth() / 5, getHeight() / 12);

    // set look and feel scale
    otherLookAndFeel.scale = scale;
    roundedButtonLnf.scale = scale;
    driveLookAndFeel.scale = scale;
}

void FireAudioProcessorEditor::updateToggleState()
{
    if (*processor.treeState.getRawParameterValue(OFF_ID))
    {
        filterLowButton.setEnabled(false);
        filterBandButton.setEnabled(false);
        filterHighButton.setEnabled(false);
        cutoffKnob.setEnabled(false);
        resKnob.setEnabled(false);
    }
    else
    {
        filterLowButton.setEnabled(true);
        filterBandButton.setEnabled(true);
        filterHighButton.setEnabled(true);
        cutoffKnob.setEnabled(true);
        resKnob.setEnabled(true);
    }
}

void FireAudioProcessorEditor::timerCallback()
{
    // bypassed
    if (processor.getBypassedState())
    {
        distortionGraph.repaint();
        multiband.repaint();
    }
    else if (processor.isFFTBlockReady())
    {
        // not bypassed, repaint at the same time
        processor.processFFT();
        spectrum.prepareToPaintSpectrum(processor.getFFTSize(), processor.getFFTData());
        spectrum.repaint();
        oscilloscope.repaint();
        distortionGraph.repaint();
        multiband.repaint();
        driveKnob1.repaint();
        driveKnob2.repaint();
        driveKnob3.repaint();
        driveKnob4.repaint();
    }
}

void FireAudioProcessorEditor::sliderValueChanged(juce::Slider *slider)
{
    linkValue(*slider, driveKnob1, outputKnob1, linkedButton1);
    linkValue(*slider, driveKnob2, outputKnob2, linkedButton2);
    linkValue(*slider, driveKnob3, outputKnob3, linkedButton3);
    linkValue(*slider, driveKnob4, outputKnob4, linkedButton4);
    
    // TODO: can't do vertical line position automation
//    if (slider == &linePosSlider1)
//    {
//        multiband.dragLines(linePosSlider1.getValue());
//    }
//    if (slider == &linePosSlider2)
//    {
//        multiband.dragLines(linePosSlider2.getValue());
//    }
//    if (slider == &linePosSlider3)
//    {
//        multiband.dragLines(linePosSlider3.getValue());
//    }
}

void FireAudioProcessorEditor::linkValue(juce::Slider &xSlider, juce::Slider &driveSlider, juce::Slider &outputSlider, juce::TextButton& linkedButton)
{
    // x changes, then y will change
    if (linkedButton.getToggleState() == true)
    {
        if (&xSlider == &driveSlider)
        {
            outputSlider.setValue(-xSlider.getValue() * 0.1f, juce :: dontSendNotification);
        }
        else if (&xSlider == &outputSlider && driveSlider.isEnabled())
        {
            if (outputSlider.getValue() <= 0 && outputSlider.getValue() >= -10)
                driveSlider.setValue(-outputKnob1.getValue() * 10, juce :: dontSendNotification);
            else if (outputSlider.getValue() > 0)
                driveSlider.setValue(0, juce :: dontSendNotification);
            else if (outputSlider.getValue() < -10)
                driveSlider.setValue(100, juce :: dontSendNotification);
        }
    }
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox *combobox)
{
    changeSliderState(combobox);
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
    lastPresetName = stateComponent.getPresetName();
}

void FireAudioProcessorEditor::changeSliderState(juce::ComboBox *combobox)
{
    if (combobox == &distortionMode1)
    {
        setSliderState(&processor, driveKnob1, MODE_ID1, tempDriveValue[0]);
        setSliderState(&processor, biasKnob1, MODE_ID1, tempBiasValue[0]);
    }
    else if (combobox == &distortionMode2)
    {
        setSliderState(&processor, driveKnob2, MODE_ID2, tempDriveValue[1]);
        setSliderState(&processor, biasKnob2, MODE_ID2, tempBiasValue[1]);
    }
    else if (combobox == &distortionMode3)
    {
        setSliderState(&processor, driveKnob3, MODE_ID3, tempDriveValue[2]);
        setSliderState(&processor, biasKnob3, MODE_ID3, tempBiasValue[2]);
    }
    else if (combobox == &distortionMode4)
    {
        setSliderState(&processor, driveKnob4, MODE_ID4, tempDriveValue[3]);
        setSliderState(&processor, biasKnob4, MODE_ID4, tempBiasValue[3]);
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
    else if (isPresetChanged())
    {
        slider.setEnabled(true);
    }
    else if (!slider.isEnabled())
    {
        slider.setValue(tempValue);
        slider.setEnabled(true);
    }
}

bool FireAudioProcessorEditor::isPresetChanged()
{
    return stateComponent.getPresetName() != lastPresetName;
}

void FireAudioProcessorEditor::setMenu(juce::ComboBox* combobox)
{
    // Distortion mode select
    addAndMakeVisible(combobox);

    combobox->addItem("None", 1);
    combobox->addSeparator();

    combobox->addSectionHeading("Soft Clipping");
    combobox->addItem("Arctan Soft Clipping", 2);
    combobox->addItem("Exp Soft Clipping", 3);
    combobox->addItem("Tanh Soft Clipping", 4);
    combobox->addItem("Cubic Soft Clipping", 5);
    combobox->addSeparator();

    combobox->addSectionHeading("Hard Clipping");
    combobox->addItem("Hard Clipping", 6);
    combobox->addItem("Sausage", 7);
    combobox->addSeparator();

    combobox->addSectionHeading("Foldback");
    combobox->addItem("Sin Foldback", 8);
    combobox->addItem("Linear Foldback", 9);
    combobox->addSeparator();

    combobox->addSectionHeading("Asymmetrical Clipping");
    combobox->addItem("Diode Clipping 1 (beta)", 10);
    combobox->addSeparator();

    combobox->setJustificationType(juce::Justification::centred);
    combobox->addListener(this);
}

void FireAudioProcessorEditor::setListenerKnob(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.addListener(this);
}

void FireAudioProcessorEditor::setRotarySlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
}

void FireAudioProcessorEditor::setLinearSlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
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
    button.setColour(juce::TextButton::buttonColourId, COLOUR6);
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    button.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    button.setButtonText(buttonName);
    button.setLookAndFeel(&roundedButtonLnf);
}

void FireAudioProcessorEditor::setDistortionGraph(juce::String modeId, juce::String driveId, 
    juce::String recId, juce::String mixId, juce::String biasId)
{
    // paint distortion function
    int mode = static_cast<int>(*processor.treeState.getRawParameterValue(modeId));
//    float drive = *processor.treeState.getRawParameterValue(driveId);//TODO: replace it!
    float drive = processor.getNewDrive(driveId);
    float color = static_cast<float>(*processor.treeState.getRawParameterValue(COLOR_ID));
    float rec = static_cast<float>(*processor.treeState.getRawParameterValue(recId));
    float mix = static_cast<float>(*processor.treeState.getRawParameterValue(mixId));
    float bias = static_cast<float>(*processor.treeState.getRawParameterValue(biasId));
    float rateDivide = static_cast<float>(*processor.treeState.getRawParameterValue(DOWNSAMPLE_ID));

    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;

    distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
}

void FireAudioProcessorEditor::setMultiband()
{
    //multiband.setLineNum(processor.getLineNum());
    int freq1 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID1));
    int freq2 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID2));
    int freq3 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID3));
    multiband.setFrequency(freq1, freq2, freq3);

    bool lineState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID1));
    bool lineState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID2));
    bool lineState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID3));
    multiband.setLineState(lineState1, lineState2, lineState3);

    float pos1 = static_cast<float>(*processor.treeState.getRawParameterValue(LINEPOS_ID1));
    float pos2 = static_cast<float>(*processor.treeState.getRawParameterValue(LINEPOS_ID2));
    float pos3 = static_cast<float>(*processor.treeState.getRawParameterValue(LINEPOS_ID3));
    multiband.setLinePos(pos1, pos2, pos3);
    
    bool enableState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID1));
    bool enableState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID2));
    bool enableState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID3));
    bool enableState4 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID4));
    multiband.setEnableState(enableState1, enableState2, enableState3, enableState4);
    
    multiband.updateLines("reset", -1);
    multiband.setCloseButtonState();
    multiband.setFocus();
}
