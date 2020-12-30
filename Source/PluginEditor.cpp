/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "[Early Beta] 0.80.1"


//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p), stateComponent{p.stateAB, p.statePresets, multiband}
{
    // timer
    juce::Timer::startTimerHz(30.0f);

    // set resize
    setResizable(true, true);

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
    setSize(1000, 500);

    setLookAndFeel(&otherLookAndFeel);

    // drive knob 1
    
    setListenerSlider(driveKnob1);
    setListenerSlider(driveKnob2);
    setListenerSlider(driveKnob3);
    setListenerSlider(driveKnob4);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    driveLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    driveLabel.attachToComponent(&driveKnob1, false);
    driveLabel.setJustificationType(juce::Justification::centred);

    
    // output knob
    setListenerSlider(outputKnob1);
    setListenerSlider(outputKnob2);
    setListenerSlider(outputKnob3);
    setListenerSlider(outputKnob4);
    setNormalSlider(outputKnob);
    
    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent(&outputKnob1, false);
    outputLabel.setJustificationType(juce::Justification::centred);
    
    // mix knob
    setNormalSlider(mixKnob1);
    setNormalSlider(mixKnob2);
    setNormalSlider(mixKnob3);
    setNormalSlider(mixKnob4);
    setNormalSlider(mixKnob);
    
    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent(&mixKnob1, false);
    mixLabel.setJustificationType(juce::Justification::centred);
    
    // dynamic knob
    setNormalSlider(dynamicKnob1);
    setNormalSlider(dynamicKnob2);
    setNormalSlider(dynamicKnob3);
    setNormalSlider(dynamicKnob4);

    addAndMakeVisible(dynamicLabel);
    dynamicLabel.setText("Dynamic", juce::dontSendNotification);
    dynamicLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    dynamicLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    dynamicLabel.attachToComponent(&dynamicKnob1, false);
    dynamicLabel.setJustificationType(juce::Justification::centred);
    
    // width knob
    setNormalSlider(widthKnob1);
    setNormalSlider(widthKnob2);
    setNormalSlider(widthKnob3);
    setNormalSlider(widthKnob4);

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
    setNormalSlider(biasKnob1);
    setNormalSlider(biasKnob2);
    setNormalSlider(biasKnob3);
    setNormalSlider(biasKnob4);
    
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
    setNormalSlider(recKnob1);
    setNormalSlider(recKnob2);
    setNormalSlider(recKnob3);
    setNormalSlider(recKnob4);

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
    bool windowLeftButtonState = *processor.treeState.getRawParameterValue("windowLeft");
    windowLeftButton.setToggleState(windowLeftButtonState, juce::dontSendNotification);
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
    bool windowRightButtonState = *processor.treeState.getRawParameterValue("windowRight");
    windowRightButton.setToggleState(windowRightButtonState, juce::dontSendNotification);
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
    
    dynamicAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID1, dynamicKnob1);
    dynamicAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID2, dynamicKnob2);
    dynamicAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID3, dynamicKnob3);
    dynamicAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID4, dynamicKnob4);
    
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
    
    windowLeftButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, WINDOW_LEFT_ID, windowLeftButton);
    windowRightButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, WINDOW_RIGHT_ID, windowRightButton);
    
    multiFocusAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID1, multiFocusSlider1);
    multiFocusAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID2, multiFocusSlider2);
    multiFocusAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID3, multiFocusSlider3);
    multiFocusAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID4, multiFocusSlider4);
    
    multiStateAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID1, multiStateSlider1);
    multiStateAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID2, multiStateSlider2);
    multiStateAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID3, multiStateSlider3);
    multiStateAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID4, multiStateSlider4);
    
    multiFreqAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID1, multiFreqSlider1);
    multiFreqAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID2, multiFreqSlider2);
    multiFreqAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID3, multiFreqSlider3);
    
    lineNumSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINENUM_ID, lineNumSlider);
    
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

    // resize limit
    setResizeLimits(1000, 500, 2000, 1000); // set resize limits
    //getConstrainer ()->setFixedAspectRatio (2); // set fixed resize rate

    updateToggleState();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    inputMeter.setLookAndFeel(nullptr);
    outputMeter.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
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
    juce::Rectangle<int> area(getWidth() - 150, getHeight() - 25, 150, 50);
    g.drawFittedText(version, area, juce::Justification::topLeft, 1);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t)BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, part1, part1, 0, 0, logo.getWidth(), logo.getHeight());

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t)BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - part1, 0, part1, part1, 0, 0, logoWings.getWidth(), logoWings.getHeight());

    bool left = *processor.treeState.getRawParameterValue(WINDOW_LEFT_ID);
    bool right = *processor.treeState.getRawParameterValue(WINDOW_RIGHT_ID);
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
    if (stateComponent.getPresetName() != lastPresetName)
    {
        setMultiband();
        lastPresetName = stateComponent.getPresetName();
    }

    multiband.getFocusArray(multibandFocus);
    multiFocusSlider1.setValue(multibandFocus[0]);
    multiFocusSlider2.setValue(multibandFocus[1]);
    multiFocusSlider3.setValue(multibandFocus[2]);
    multiFocusSlider4.setValue(multibandFocus[3]);

    multiband.getStateArray(multibandState);
    multiStateSlider1.setValue(multibandState[0]);
    multiStateSlider2.setValue(multibandState[1]);
    multiStateSlider3.setValue(multibandState[2]);
    multiStateSlider4.setValue(multibandState[3]);

    multiband.getFreqArray(multibandFreq);
    multiFreqSlider1.setValue(multibandFreq[0]);
    multiFreqSlider2.setValue(multibandFreq[1]);
    multiFreqSlider3.setValue(multibandFreq[2]);

    multiband.getLinePos(linePos);
    linePosSlider1.setValue(linePos[0]);
    linePosSlider2.setValue(linePos[1]);
    linePosSlider3.setValue(linePos[2]);

    int lineNum = multiband.getLineNum();
    lineNumSlider.setValue(lineNum);

    multiband.getLineState(lineState);
    lineStateSlider1.setValue(lineState[0]);
    lineStateSlider2.setValue(lineState[1]);
    lineStateSlider3.setValue(lineState[2]);

    if (left) { // if you select the left window, you will see audio wave and distortion function graphs.
        
//        spectrum.setVisible(false);
//        multiband.setVisible(false);
//
//        distortionGraph.setVisible(true);
//        distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
//        oscilloscope.setVisible(true);
        
        
        if (multibandFocus[0])
        {
            driveKnob1.setVisible(true);
            driveKnob2.setVisible(false);
            driveKnob3.setVisible(false);
            driveKnob4.setVisible(false);
            
            dynamicKnob1.setVisible(true);
            dynamicKnob2.setVisible(false);
            dynamicKnob3.setVisible(false);
            dynamicKnob4.setVisible(false);
            
            outputKnob1.setVisible(true);
            outputKnob2.setVisible(false);
            outputKnob3.setVisible(false);
            outputKnob4.setVisible(false);
            
            mixKnob1.setVisible(true);
            mixKnob2.setVisible(false);
            mixKnob3.setVisible(false);
            mixKnob4.setVisible(false);
            
            widthKnob1.setVisible(true);
            widthKnob2.setVisible(false);
            widthKnob3.setVisible(false);
            widthKnob4.setVisible(false);
            
            linkedButton1.setVisible(true);
            linkedButton2.setVisible(false);
            linkedButton3.setVisible(false);
            linkedButton4.setVisible(false);
            
            safeButton1.setVisible(true);
            safeButton2.setVisible(false);
            safeButton3.setVisible(false);
            safeButton4.setVisible(false);
            
            recKnob1.setVisible(true);
            recKnob2.setVisible(false);
            recKnob3.setVisible(false);
            recKnob4.setVisible(false);
            
            biasKnob1.setVisible(true);
            biasKnob2.setVisible(false);
            biasKnob3.setVisible(false);
            biasKnob4.setVisible(false);

            distortionMode1.setVisible(true);
            distortionMode2.setVisible(false);
            distortionMode3.setVisible(false);
            distortionMode4.setVisible(false);
        }
        else if (multibandFocus[1])
        {
            driveKnob1.setVisible(false);
            driveKnob2.setVisible(true);
            driveKnob3.setVisible(false);
            driveKnob4.setVisible(false);
            
            dynamicKnob1.setVisible(false);
            dynamicKnob2.setVisible(true);
            dynamicKnob3.setVisible(false);
            dynamicKnob4.setVisible(false);
            
            outputKnob1.setVisible(false);
            outputKnob2.setVisible(true);
            outputKnob3.setVisible(false);
            outputKnob4.setVisible(false);
            
            mixKnob1.setVisible(false);
            mixKnob2.setVisible(true);
            mixKnob3.setVisible(false);
            mixKnob4.setVisible(false);
            
            widthKnob1.setVisible(false);
            widthKnob2.setVisible(true);
            widthKnob3.setVisible(false);
            widthKnob4.setVisible(false);
            
            linkedButton1.setVisible(false);
            linkedButton2.setVisible(true);
            linkedButton3.setVisible(false);
            linkedButton4.setVisible(false);
            
            safeButton1.setVisible(false);
            safeButton2.setVisible(true);
            safeButton3.setVisible(false);
            safeButton4.setVisible(false);
            
            recKnob1.setVisible(false);
            recKnob2.setVisible(true);
            recKnob3.setVisible(false);
            recKnob4.setVisible(false);
            
            biasKnob1.setVisible(false);
            biasKnob2.setVisible(true);
            biasKnob3.setVisible(false);
            biasKnob4.setVisible(false);

            distortionMode1.setVisible(false);
            distortionMode2.setVisible(true);
            distortionMode3.setVisible(false);
            distortionMode4.setVisible(false);
        }
        else if (multibandFocus[2])
        {
            driveKnob1.setVisible(false);
            driveKnob2.setVisible(false);
            driveKnob3.setVisible(true);
            driveKnob4.setVisible(false);
            
            dynamicKnob1.setVisible(false);
            dynamicKnob2.setVisible(false);
            dynamicKnob3.setVisible(true);
            dynamicKnob4.setVisible(false);
            
            outputKnob1.setVisible(false);
            outputKnob2.setVisible(false);
            outputKnob3.setVisible(true);
            outputKnob4.setVisible(false);
            
            mixKnob1.setVisible(false);
            mixKnob2.setVisible(false);
            mixKnob3.setVisible(true);
            mixKnob4.setVisible(false);
            
            widthKnob1.setVisible(false);
            widthKnob2.setVisible(false);
            widthKnob3.setVisible(true);
            widthKnob4.setVisible(false);
            
            linkedButton1.setVisible(false);
            linkedButton2.setVisible(false);
            linkedButton3.setVisible(true);
            linkedButton4.setVisible(false);
            
            safeButton1.setVisible(false);
            safeButton2.setVisible(false);
            safeButton3.setVisible(true);
            safeButton4.setVisible(false);
            
            recKnob1.setVisible(false);
            recKnob2.setVisible(false);
            recKnob3.setVisible(true);
            recKnob4.setVisible(false);
            
            biasKnob1.setVisible(false);
            biasKnob2.setVisible(false);
            biasKnob3.setVisible(true);
            biasKnob4.setVisible(false);

            distortionMode1.setVisible(false);
            distortionMode2.setVisible(false);
            distortionMode3.setVisible(true);
            distortionMode4.setVisible(false);
        }
        else if (multibandFocus[3])
        {
            setDistortionGraph(MODE_ID4, DRIVE_ID4,
                REC_ID4, MIX_ID4, BIAS_ID4);
            driveKnob1.setVisible(false);
            driveKnob2.setVisible(false);
            driveKnob3.setVisible(false);
            driveKnob4.setVisible(true);
            
            dynamicKnob1.setVisible(false);
            dynamicKnob2.setVisible(false);
            dynamicKnob3.setVisible(false);
            dynamicKnob4.setVisible(true);
            
            outputKnob1.setVisible(false);
            outputKnob2.setVisible(false);
            outputKnob3.setVisible(false);
            outputKnob4.setVisible(true);
            
            mixKnob1.setVisible(false);
            mixKnob2.setVisible(false);
            mixKnob3.setVisible(false);
            mixKnob4.setVisible(true);
            
            widthKnob1.setVisible(false);
            widthKnob2.setVisible(false);
            widthKnob3.setVisible(false);
            widthKnob4.setVisible(true);
            
            linkedButton1.setVisible(false);
            linkedButton2.setVisible(false);
            linkedButton3.setVisible(false);
            linkedButton4.setVisible(true);
            
            safeButton1.setVisible(false);
            safeButton2.setVisible(false);
            safeButton3.setVisible(false);
            safeButton4.setVisible(true);
            
            recKnob1.setVisible(false);
            recKnob2.setVisible(false);
            recKnob3.setVisible(false);
            recKnob4.setVisible(true);
            
            biasKnob1.setVisible(false);
            biasKnob2.setVisible(false);
            biasKnob3.setVisible(false);
            biasKnob4.setVisible(true);

            distortionMode1.setVisible(false);
            distortionMode2.setVisible(false);
            distortionMode3.setVisible(false);
            distortionMode4.setVisible(true);
        }
        
        driveLabel.setVisible(true);
        outputLabel.setVisible(true);
        mixLabel.setVisible(true);
        dynamicLabel.setVisible(true);
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
        
        dynamicKnob1.setVisible(false);
        dynamicKnob2.setVisible(false);
        dynamicKnob3.setVisible(false);
        dynamicKnob4.setVisible(false);
        
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
        dynamicLabel.setVisible(false);
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

    // knobs
    int knobNum = 8;
    float scale = juce::jmin(getHeight() / 500.f, getWidth() / 1000.f);
    float scaleMax = juce::jmax(getHeight() / 500.f, getWidth() / 1000.f);
    int scaledKnobSize = SCALED_KNOBSIZE;
    int startX = getWidth() / knobNum;
    int secondShadowY = getHeight() / 10 * 4;
    int firstPartY = secondShadowY + (getHeight() - secondShadowY) * 2 / 5 - scaledKnobSize / 2;
    int secondPartY = secondShadowY + (getHeight() - secondShadowY) * 4 / 5 - scaledKnobSize / 2;

    
    // save presets
    juce::Rectangle<int> r(getLocalBounds());
    r = r.removeFromTop(50 * getHeight() / 500);
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
    dynamicKnob1.setBounds(DYNAMIC_X, secondPartY, scaledKnobSize, scaledKnobSize);
    dynamicKnob2.setBounds(DYNAMIC_X, secondPartY, scaledKnobSize, scaledKnobSize);
    dynamicKnob3.setBounds(DYNAMIC_X, secondPartY, scaledKnobSize, scaledKnobSize);
    dynamicKnob4.setBounds(DYNAMIC_X, secondPartY, scaledKnobSize, scaledKnobSize);
    
    
    cutoffKnob.setBounds(CUTOFF_X, firstPartY, scaledKnobSize, scaledKnobSize);
    resKnob.setBounds(RES_X, firstPartY, scaledKnobSize, scaledKnobSize);
    
    mixKnob1.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob2.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob3.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob4.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    mixKnob.setBounds(MIX_X, secondPartY, scaledKnobSize, scaledKnobSize);
    
    // first line
    hqButton.setBounds(getHeight() / 10, 0, getHeight() / 10, getHeight() / 10);
    linkedButton1.setBounds(DRIVE_X + scaledKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - scaledKnobSize / 2 - 25, scaledKnobSize / 2, 0.05 * getHeight());
    linkedButton2.setBounds(DRIVE_X + scaledKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - scaledKnobSize / 2 - 25, scaledKnobSize / 2, 0.05 * getHeight());
    linkedButton3.setBounds(DRIVE_X + scaledKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - scaledKnobSize / 2 - 25, scaledKnobSize / 2, 0.05 * getHeight());
    linkedButton4.setBounds(DRIVE_X + scaledKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - scaledKnobSize / 2 - 25, scaledKnobSize / 2, 0.05 * getHeight());
    
    float windowHeight = getHeight() / 20;
    windowLeftButton.setBounds(0, secondShadowY, getWidth() / 2, windowHeight);
    windowRightButton.setBounds(getWidth() / 2, secondShadowY, getWidth() / 2, windowHeight);
    
    // second line
    safeButton1.setBounds(DRIVE_X + scaledKnobSize * 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    safeButton2.setBounds(DRIVE_X + scaledKnobSize * 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    safeButton3.setBounds(DRIVE_X + scaledKnobSize * 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
    safeButton4.setBounds(DRIVE_X + scaledKnobSize * 2, secondPartY, scaledKnobSize / 2, 0.05 * getHeight());
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

    otherLookAndFeel.scale = scale;
    roundedButtonLnf.scale = scale;
    
    
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
    
    // spectrum
    if (processor.isFFTBlockReady())
    {
        
        multiband.repaint();
        oscilloscope.repaint();
        distortionGraph.repaint();
        processor.processFFT();
        spectrum.prepareToPaintSpectrum(processor.getFFTSize() , processor.getFFTData());
        spectrum.repaint();
    }
    
    
//    repaint();
}



void FireAudioProcessorEditor::sliderValueChanged(juce::Slider *slider)
{
    // TODO: put this into one function!!!(linkValue)
    if (linkedButton1.getToggleState() == true)
    {
        if (slider == &driveKnob1)
        {
            outputKnob1.setValue(-driveKnob1.getValue() * 0.1);
        }
        else if (slider == &outputKnob1 && driveKnob1.isEnabled())
        {
            if (outputKnob1.getValue() <= 0 && outputKnob1.getValue() >= -10)
                driveKnob1.setValue(-outputKnob1.getValue() * 10);
            else if (outputKnob1.getValue() > 0)
                driveKnob1.setValue(0);
            else if (outputKnob1.getValue() < -10)
                driveKnob1.setValue(100);
        }
    }
    if (linkedButton2.getToggleState() == true)
    {
        if (slider == &driveKnob2)
        {
            outputKnob2.setValue(-driveKnob2.getValue() * 0.1);
        }
        else if (slider == &outputKnob2 && driveKnob2.isEnabled())
        {
            if (outputKnob2.getValue() <= 0 && outputKnob2.getValue() >= -10)
                driveKnob2.setValue(-outputKnob2.getValue() * 10);
            else if (outputKnob2.getValue() > 0)
                driveKnob2.setValue(0);
            else if (outputKnob2.getValue() < -10)
                driveKnob2.setValue(100);
        }
    }
    if (linkedButton3.getToggleState() == true)
    {
        if (slider == &driveKnob3)
        {
            outputKnob3.setValue(-driveKnob3.getValue() * 0.1);
        }
        else if (slider == &outputKnob3 && driveKnob3.isEnabled())
        {
            if (outputKnob3.getValue() <= 0 && outputKnob3.getValue() >= -10)
                driveKnob3.setValue(-outputKnob3.getValue() * 10);
            else if (outputKnob3.getValue() > 0)
                driveKnob3.setValue(0);
            else if (outputKnob3.getValue() < -10)
                driveKnob3.setValue(100);
        }
    }
    if (linkedButton4.getToggleState() == true)
    {
        if (slider == &driveKnob4)
        {
            outputKnob4.setValue(-driveKnob4.getValue() * 0.1);
        }
        else if (slider == &outputKnob4 && driveKnob4.isEnabled())
        {
            if (outputKnob4.getValue() <= 0 && outputKnob4.getValue() >= -10)
                driveKnob4.setValue(-outputKnob4.getValue() * 10);
            else if (outputKnob4.getValue() > 0)
                driveKnob4.setValue(0);
            else if (outputKnob4.getValue() < -10)
                driveKnob4.setValue(100);
        }
    }
}

void FireAudioProcessorEditor::linkValue(juce::Slider &xSlider, juce::Slider &ySlider)
{
    // x changes, then y will change
    
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox *combobox)
{
    if (combobox == &distortionMode1)
    {
        disableSlider(&processor, driveKnob1, MODE_ID1, tempDriveValue[0]);
        disableSlider(&processor, biasKnob1, MODE_ID1, tempBiasValue[0]);
    }
    else if (combobox == &distortionMode2)
    {
        disableSlider(&processor, driveKnob2, MODE_ID2, tempDriveValue[1]);
        disableSlider(&processor, biasKnob2, MODE_ID2, tempBiasValue[1]);
    }
    else if (combobox == &distortionMode3)
    {
        disableSlider(&processor, driveKnob3, MODE_ID3, tempDriveValue[2]);
        disableSlider(&processor, biasKnob3, MODE_ID3, tempBiasValue[2]);
    }
    else if (combobox == &distortionMode4)
    {
        disableSlider(&processor, driveKnob4, MODE_ID4, tempDriveValue[3]);
        disableSlider(&processor, biasKnob4, MODE_ID4, tempBiasValue[3]);
    }
}

void FireAudioProcessorEditor::disableSlider(FireAudioProcessor* processor, juce::Slider& slider, juce::String paramId, float &tempValue)
{
    auto val = processor->treeState.getRawParameterValue(paramId);
    int selection = val->load();
    if (selection == 0)
    {
        tempValue = slider.getValue();
        if (&slider == &driveKnob1 || &slider == &driveKnob2 || &slider == &driveKnob3 || &slider == &driveKnob4)
        {
            slider.setValue(1);
        }
        else if (&slider == &biasKnob1 || &slider == &biasKnob2 || &slider == &biasKnob3 || &slider == &biasKnob4)
        {
            slider.setValue(0);
        }
        
        slider.setEnabled(false);
    }
    else if (selection != 0 && !slider.isEnabled())
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
    combobox->addItem("Arctan Soft Clipping", 2);
    combobox->addItem("Exp Soft Clipping", 3);
    combobox->addItem("Tanh Soft Clipping", 4);
    combobox->addItem("Cubic Soft Clipping", 5);
    combobox->addSeparator();

    combobox->addSectionHeading("Hard Clipping");
    combobox->addItem("Hard Clipping", 6);
    combobox->addItem("Sausage Fattener", 7);
    combobox->addSeparator();

    combobox->addSectionHeading("Foldback");
    combobox->addItem("Sin Foldback", 8);
    combobox->addItem("Linear Foldback", 9);
    combobox->addSeparator();

    combobox->addSectionHeading("Asymmetrical Clipping");
    combobox->addItem("Diode Clipping 1", 10);
    combobox->addSeparator();

    combobox->setJustificationType(juce::Justification::centred);
    combobox->addListener(this);
}

void FireAudioProcessorEditor::setListenerSlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.addListener(this);
}

void FireAudioProcessorEditor::setNormalSlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
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
    int mode = *processor.treeState.getRawParameterValue(modeId);
    float drive = *processor.treeState.getRawParameterValue(driveId);//TODO: replace it!
//    float drive = processor.getNewDrive();
    float color = *processor.treeState.getRawParameterValue(COLOR_ID);
    float rec = *processor.treeState.getRawParameterValue(recId);
    float mix = *processor.treeState.getRawParameterValue(mixId);
    float bias = *processor.treeState.getRawParameterValue(biasId);
    float rateDivide = *processor.treeState.getRawParameterValue(DOWNSAMPLE_ID);

    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;

    distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
}

void FireAudioProcessorEditor::setMultiband()
{
    multiband.setLineNum(*processor.treeState.getRawParameterValue(LINENUM_ID));
    int freq1 = *processor.treeState.getRawParameterValue(FREQ_ID1);
    int freq2 = *processor.treeState.getRawParameterValue(FREQ_ID2);
    int freq3 = *processor.treeState.getRawParameterValue(FREQ_ID3);
    multiband.setFrequency(freq1, freq2, freq3);

    bool state1 = *processor.treeState.getRawParameterValue(LINE_STATE_ID1);
    bool state2 = *processor.treeState.getRawParameterValue(LINE_STATE_ID2);
    bool state3 = *processor.treeState.getRawParameterValue(LINE_STATE_ID3);
    multiband.setLineState(state1, state2, state3);

    float pos1 = *processor.treeState.getRawParameterValue(LINEPOS_ID1);
    float pos2 = *processor.treeState.getRawParameterValue(LINEPOS_ID2);
    float pos3 = *processor.treeState.getRawParameterValue(LINEPOS_ID3);
    multiband.setLinePos(pos1, pos2, pos3);
    multiband.updateLines(false, -1);
    multiband.setCloseButtonState();

}
