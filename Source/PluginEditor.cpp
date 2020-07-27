/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "0.711"
//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p)
    , processor(p)
    , stateComponent {p.stateAB, p.statePresets}
{
    // timer
    Timer::startTimerHz(20.0f);

    // resize not avaialble
    setResizable(true, true);

    // Visualiser
    addAndMakeVisible(p.visualiser);
    
    // presets
    addAndMakeVisible(stateComponent);
    
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
    
    /*
    // input knob
    addAndMakeVisible(inputKnob);
    inputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    // inputKnob.setSliderStyle(Slider::LinearVertical);
    inputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(inputLabel);
    inputLabel.setText("Input", dontSendNotification);
    inputLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    inputLabel.setColour(Label::textColourId, COLOUR5);
    inputLabel.attachToComponent(&inputKnob, false);
    inputLabel.setJustificationType (Justification::centred);
    */
    
    // drive knob
    addAndMakeVisible(driveKnob);
    driveKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    driveKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    driveKnob.addListener(this);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", dontSendNotification);
    driveLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    driveLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    driveLabel.attachToComponent(&driveKnob, false);
    driveLabel.setJustificationType (Justification::centred);
    
    // color knob
    addAndMakeVisible(colorKnob);
    colorKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    colorKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    colorKnob.addListener(this);

    addAndMakeVisible(colorLabel);
    colorLabel.setText("Color", dontSendNotification);
    colorLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    colorLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    colorLabel.attachToComponent(&colorKnob, false);
    colorLabel.setJustificationType (Justification::centred);

    // bias knob
    addAndMakeVisible(biasKnob);
    biasKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    biasKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    biasKnob.addListener(this);

    addAndMakeVisible(biasLabel);
    biasLabel.setText("Bias", dontSendNotification);
    biasLabel.setFont(Font(KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    biasLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    biasLabel.attachToComponent(&biasKnob, false);
    biasLabel.setJustificationType(Justification::centred);
    
    // downsample knob
    addAndMakeVisible(downSampleKnob);
    downSampleKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    downSampleKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    downSampleKnob.addListener(this);

    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", dontSendNotification);
    downSampleLabel.setFont(Font(KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    downSampleLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(Justification::centred);

    // output knob
    addAndMakeVisible(outputKnob);
    outputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    outputKnob.addListener(this);

    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", dontSendNotification);
    outputLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    outputLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent(&outputKnob, false);
    outputLabel.setJustificationType (Justification::centred);

    // rec knob
    addAndMakeVisible(recKnob);
    recKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    recKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    recKnob.addListener(this);

    addAndMakeVisible(recLabel);
    recLabel.setText("Rectification", dontSendNotification);
    recLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    recLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    recLabel.attachToComponent(&recKnob, false);
    recLabel.setJustificationType (Justification::centred);
    
    // mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", dontSendNotification);
    mixLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    mixLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent(&mixKnob, false);
    mixLabel.setJustificationType (Justification::centred);
    
    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    
    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", dontSendNotification);
    cutoffLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    cutoffLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    cutoffLabel.attachToComponent(&cutoffKnob, false);
    cutoffLabel.setJustificationType (Justification::centred);
    
    // res knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(resLabel);
    resLabel.setText("Q", dontSendNotification); // Resonance
    resLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    resLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    resLabel.attachToComponent(&resKnob, false);
    resLabel.setJustificationType (Justification::centred);
    
    // HQ(oversampling) Button
    addAndMakeVisible(hqButton);
    hqButton.setClickingTogglesState(true);
    bool hqButtonState = *processor.treeState.getRawParameterValue("hq");
    hqButton.setToggleState(hqButtonState, dontSendNotification);
    hqButton.onClick = [this] { updateToggleState(&hqButton, "Hq"); };
    hqButton.setColour(TextButton::buttonColourId, COLOUR5);
    hqButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    hqButton.setColour(ComboBox::outlineColourId, COLOUR5);
    hqButton.setColour(TextButton::textColourOnId, COLOUR1);
    hqButton.setColour(TextButton::textColourOffId, Colour(100, 20, 20));
    hqButton.setButtonText("HQ");

    // Linked Button
    addAndMakeVisible(linkedButton);
    linkedButton.setClickingTogglesState(true);
    bool linkedButtonState = *processor.treeState.getRawParameterValue("linked");
    linkedButton.setToggleState(linkedButtonState, dontSendNotification);
    linkedButton.onClick = [this] { updateToggleState (&linkedButton, "Linked"); };
    linkedButton.setColour(TextButton::buttonColourId, COLOUR6);
    linkedButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    linkedButton.setColour(ComboBox::outlineColourId, COLOUR6);
    linkedButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    linkedButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    linkedButton.setButtonText("LINK");
    linkedButton.setLookAndFeel(&roundedButtonLnf);

    // safe overload Button
    addAndMakeVisible(safeButton);
    safeButton.setClickingTogglesState(true);
    bool safeButtonState = *processor.treeState.getRawParameterValue("safe");
    safeButton.setToggleState(safeButtonState, dontSendNotification);
    safeButton.onClick = [this] { updateToggleState(&safeButton, "Safe"); };
    safeButton.setColour(TextButton::buttonColourId, COLOUR6);
    safeButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    safeButton.setColour(ComboBox::outlineColourId, COLOUR6);
    safeButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    safeButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    safeButton.setButtonText("SAFE");
    safeButton.setLookAndFeel(&roundedButtonLnf);

    // Filter State Buttons
    addAndMakeVisible(filterOffButton);
    filterOffButton.setClickingTogglesState(true);
    filterOffButton.setRadioGroupId(filterStateButtons);
    bool filterOffButtonState = *processor.treeState.getRawParameterValue("off");
    filterOffButton.setToggleState(filterOffButtonState, dontSendNotification);
    filterOffButton.onClick = [this] { updateToggleState (&filterOffButton, "Off"); };
    filterOffButton.setColour(TextButton::buttonColourId, COLOUR6);
    filterOffButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    filterOffButton.setColour(ComboBox::outlineColourId, COLOUR6);
    filterOffButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterOffButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterOffButton.setButtonText("OFF");
    filterOffButton.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(filterOffLabel);
    filterOffLabel.setText("Filter", dontSendNotification);
    filterOffLabel.setFont(Font(KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    filterOffLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    filterOffLabel.attachToComponent(&filterOffButton, false);
    filterOffLabel.setJustificationType(Justification::centred);

    addAndMakeVisible(filterPreButton);
    filterPreButton.setClickingTogglesState(true);
    filterPreButton.setRadioGroupId(filterStateButtons);
    bool filterPreButtonState = *processor.treeState.getRawParameterValue("pre");
    filterPreButton.setToggleState(filterPreButtonState, dontSendNotification);
    filterPreButton.onClick = [this] { updateToggleState (&filterPreButton, "Pre"); };
    filterPreButton.setColour(TextButton::buttonColourId, COLOUR6);
    filterPreButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    filterPreButton.setColour(ComboBox::outlineColourId, COLOUR6);
    filterPreButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterPreButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterPreButton.setButtonText("PRE");
    filterPreButton.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(filterPostButton);
    filterPostButton.setClickingTogglesState(true);
    filterPostButton.setRadioGroupId(filterStateButtons);
    bool filterPostButtonState = *processor.treeState.getRawParameterValue("post");
    filterPostButton.setToggleState(filterPostButtonState, dontSendNotification);
    filterPostButton.onClick = [this] { updateToggleState (&filterPostButton, "Post"); };
    filterPostButton.setColour(TextButton::buttonColourId, COLOUR6);
    filterPostButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    filterPostButton.setColour(ComboBox::outlineColourId, COLOUR6);
    filterPostButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterPostButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterPostButton.setButtonText("POST");
    filterPostButton.setLookAndFeel(&roundedButtonLnf);
    
    // Filter Type Toggle Buttons
    addAndMakeVisible(filterLowButton);
    filterLowButton.setClickingTogglesState(true);
    filterLowButton.setRadioGroupId(filterModeButtons);
    bool filterLowButtonState = *processor.treeState.getRawParameterValue("low");
    filterLowButton.setToggleState(filterLowButtonState, dontSendNotification);
    filterLowButton.onClick = [this] { updateToggleState (&filterLowButton, "Low"); };
    filterLowButton.setColour(TextButton::buttonColourId, COLOUR6);
    filterLowButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    filterLowButton.setColour(ComboBox::outlineColourId, COLOUR6);
    filterLowButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterLowButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterLowButton.setButtonText("LP");
    filterLowButton.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(filterLowLabel);
    filterLowLabel.setText("Type", dontSendNotification);
    filterLowLabel.setFont(Font(KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    filterLowLabel.setColour(Label::textColourId, KNOB_FONT_COLOUR);
    filterLowLabel.attachToComponent(&filterLowButton, false);
    filterLowLabel.setJustificationType(Justification::centred);
    
    addAndMakeVisible(filterBandButton);
    filterBandButton.setClickingTogglesState(true);
    filterBandButton.setRadioGroupId(filterModeButtons);
    bool filterBandButtonState = *processor.treeState.getRawParameterValue("band");
    filterBandButton.setToggleState(filterBandButtonState, dontSendNotification);
    filterBandButton.onClick = [this] { updateToggleState (&filterBandButton, "Band"); };
    filterBandButton.setColour(TextButton::buttonColourId, COLOUR6);
    filterBandButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    filterBandButton.setColour(ComboBox::outlineColourId, COLOUR6);
    filterBandButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterBandButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterBandButton.setButtonText("BP");
    filterBandButton.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(filterHighButton);
    filterHighButton.setClickingTogglesState(true);
    filterHighButton.setRadioGroupId(filterModeButtons);
    bool filterHighButtonState = *processor.treeState.getRawParameterValue("high");
    filterHighButton.setToggleState(filterHighButtonState, dontSendNotification);
    filterHighButton.onClick = [this] { updateToggleState (&filterHighButton, "High"); };
    filterHighButton.setColour(TextButton::buttonColourId, COLOUR6);
    filterHighButton.setColour(TextButton::buttonOnColourId, COLOUR5);
    filterHighButton.setColour(ComboBox::outlineColourId, COLOUR6);
    filterHighButton.setColour(TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterHighButton.setColour(TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterHighButton.setButtonText("HP");
    filterHighButton.setLookAndFeel(&roundedButtonLnf);
    
    // Attachment
    //inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob);
    colorAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "color", colorKnob);
    biasAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "bias", biasKnob);
    downSampleAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "downSample", downSampleKnob);
    recAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "rec", recKnob);
    outputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "outputGain", outputKnob);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    cutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "cutoff", cutoffKnob);
    resAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "res", resKnob);
    
    hqAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "hq", hqButton);
    linkedAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "linked", linkedButton);
    safeAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "safe", safeButton);
    filterOffAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "off", filterOffButton);
    filterPreAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "pre", filterPreButton);
    filterPostAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "post", filterPostButton);
    filterLowAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "low", filterLowButton);
    filterBandAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "band", filterBandButton);
    filterHighAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "high", filterHighButton);

    // Distortion mode select
    addAndMakeVisible(distortionMode);

    distortionMode.addItem("None", 1);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Soft Clipping");
    distortionMode.addItem("Arctan Soft Clipping", 2);
    distortionMode.addItem("Exp Soft Clipping", 3);
    distortionMode.addItem("Tanh Soft Clipping", 4);
    distortionMode.addItem("Cubic Soft Clipping", 5);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Hard Clipping");
    distortionMode.addItem("Hard Clipping", 6);
    distortionMode.addItem("Sausage Fattener", 7);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Foldback");
    distortionMode.addItem("Sin Foldback", 8);
    distortionMode.addItem("Linear Foldback", 9);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Asymmetrical Clipping");
    distortionMode.addItem("Diode Clipping 1", 10);
    distortionMode.addSeparator();

    distortionMode.setJustificationType(Justification::centred);
//    distortionMode.setColour(ComboBox::textColourId, COLOUR1);
//    distortionMode.setColour(ComboBox::arrowColourId, COLOUR1);
//    distortionMode.setColour(ComboBox::buttonColourId, COLOUR1);
//    distortionMode.setColour(ComboBox::outlineColourId, COLOUR7);
//    distortionMode.setColour(ComboBox::focusedOutlineColourId, COLOUR1);
//    distortionMode.setColour(ComboBox::backgroundColourId, COLOUR7);
//    distortionMode.getLookAndFeel().setColour(PopupMenu::textColourId, COLOUR1);
//    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, COLOUR5);
//    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, COLOUR1);
//    distortionMode.getLookAndFeel().setColour(PopupMenu::headerTextColourId, COLOUR1);
//    distortionMode.getLookAndFeel().setColour(PopupMenu::backgroundColourId, COLOUR6);
    
    // use global lookandfeel
    getLookAndFeel().setColour(ComboBox::textColourId, COLOUR1);
    getLookAndFeel().setColour(ComboBox::arrowColourId, COLOUR1);
    getLookAndFeel().setColour(ComboBox::buttonColourId, COLOUR1);
    getLookAndFeel().setColour(ComboBox::outlineColourId, COLOUR7);
    getLookAndFeel().setColour(ComboBox::focusedOutlineColourId, COLOUR1);
    getLookAndFeel().setColour(ComboBox::backgroundColourId, COLOUR7);
    getLookAndFeel().setColour(PopupMenu::textColourId, COLOUR1);
    getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, COLOUR5);
    getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, COLOUR1);
    getLookAndFeel().setColour(PopupMenu::headerTextColourId, COLOUR1);
    getLookAndFeel().setColour(PopupMenu::backgroundColourId, COLOUR6);

    modeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);
    
    // about button
    addAndMakeVisible(aboutButton);
    aboutButton.setColour(TextButton::buttonColourId, COLOUR3);
    aboutButton.setColour(TextButton::textColourOnId, COLOUR3);
    aboutButton.setColour(TextButton::textColourOffId, COLOUR3);
//    aboutButton.onClick = [this] { aboutDialog.showMessageBoxAsync(AlertWindow::InfoIcon, "Made by Wings", "And fuck you", "OK"); };
    // resize limit
    setResizeLimits(1000, 500, 2000, 1000); // set resize limits
    //getConstrainer ()->setFixedAspectRatio (1.33); // set fixed resize rate: 700/525
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    inputMeter.setLookAndFeel(nullptr);
    outputMeter.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
    linkedButton.setLookAndFeel(nullptr);
    safeButton.setLookAndFeel(nullptr);
    filterOffButton.setLookAndFeel(nullptr);
    filterPreButton.setLookAndFeel(nullptr);
    filterPostButton.setLookAndFeel(nullptr);
    filterLowButton.setLookAndFeel(nullptr);
    filterBandButton.setLookAndFeel(nullptr);
    filterHighButton.setLookAndFeel(nullptr);
}

float f(float x)
{
    return 0.5;
}

//==============================================================================
void FireAudioProcessorEditor::paint(Graphics &g)
{
    // set states
    setDriveKnobState(&driveKnob);
    //setColorKnobState(&colorKnob);
    setCutoffButtonState(&filterLowButton, &cutoffKnob, &resKnob);
    setCutoffButtonState(&filterBandButton, &cutoffKnob, &resKnob);
    setCutoffButtonState(&filterHighButton, &cutoffKnob, &resKnob);
    
    int part1 = getHeight()/10;
    int part2 = part1 * 3;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillAll(COLOUR7);
    
    // title
    g.setColour(COLOUR5);
    // g.fillRect(0, 0, getWidth(), static_cast<int> (getHeight()/10.f));
    g.fillRect(0, 0, getWidth(), part1);
    
    // draw version
    g.setColour(COLOUR5);
    g.setFont (Font ("Times New Roman", 18.0f, Font::bold));
    String version = (String) "Ver. " + (String)VERSION;
    Rectangle<int> area(getWidth() - 100, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, Justification::topLeft, 1);
       
    // set logo "Fire"
    Image logo = ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t) BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, part1, part1, 0, 0, logo.getWidth(), logo.getHeight());
    
    // set logo "Wings"
    Image logoWings = ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t) BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - part1, 0, part1, part1, 0, 0, logoWings.getWidth(), logoWings.getHeight());
    
    // paint distortion function
    float functionValue = 0;
    float mixValue;

    int mode = *processor.treeState.getRawParameterValue("mode");
    //float inputGain = *processor.treeState.getRawParameterValue("inputGain");
    float drive = *processor.treeState.getRawParameterValue("drive");
    float color = *processor.treeState.getRawParameterValue("color");
    float rec = *processor.treeState.getRawParameterValue("rec");
    float mix = *processor.treeState.getRawParameterValue("mix");
    float bias = *processor.treeState.getRawParameterValue("bias");
    bool safe = *processor.treeState.getRawParameterValue("safe");

    // sausage max is 40/3 * input | 15% = *2 | 30% = *4 | 100% = *40/3
    if (mode == 6) {
        drive = (drive - 1) * 6.5 / 31.f;
        drive = powf(2, drive);
    }
    
    
    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.mix = mix;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;

    auto frame = getLocalBounds(); // adjust here, if you want to paint in a special location
    frame.setBounds(getWidth() / 2, part1, getWidth() / 2, part2);
    
    // draw layer 2
    g.setColour(COLOUR6);
    g.fillRect(0, part1, getWidth(), part2);
    if (mode < 9)
    {
        const int numPix = frame.getWidth(); // you might experiment here, if you want less steps to speed up

        float driveScale = 1;
//        if (inputGain >= 0)
//        {
//            driveScale = pow(2, inputGain / 6.0f);
//        }
        float maxValue = 2.0f * driveScale * mix + 2.0f * (1 - mix);
        float value = -maxValue; // minimum (leftmost)  value for your graph
        float valInc = (maxValue - value) / numPix;
        float xPos = frame.getX();
        const float posInc = frame.getWidth() / numPix;

        Path p;

        bool edgePointL = false;
        bool edgePointR = false;
        float yPos;
        for (int i = 1; i < numPix; ++i)
        {
            value += valInc;
            xPos += posInc;

            // symmetrical distortion
            if (mode < 9)
            {
                functionValue = distortionProcessor.distortionProcess(value);
            }
            
            // downsample
            float rateDivide = *processor.treeState.getRawParameterValue("downSample");
            if (rateDivide > 1)
            {
                functionValue = ceilf(functionValue * (64.f / rateDivide)) / (64.f / rateDivide);
            }
            
            // retification
            functionValue = distortionProcessor.rectificationProcess(functionValue);
            
            // mix
            functionValue = (1.f - mix) * value + mix * functionValue;
            mixValue = (2.0f / 3.0f) * functionValue;
            yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;

            // draw points
            if (yPos < frame.getY())
            {
                if (edgePointR == false)
                {
                    yPos = frame.getY();
                    edgePointR = true;
                }
                else
                {
                    continue;
                }
            }

            if (yPos > frame.getBottom())
            {
                if (edgePointL == false)
                {
                    continue;
                }
                else
                {
                    yPos = frame.getBottom();
                }
            }
            else if (edgePointL == false)
            {
                if (mode == 0)
                {
                    p.startNewSubPath(xPos, frame.getBottom());
                    p.lineTo(xPos, yPos);
                }
                else
                {
                    p.startNewSubPath(xPos, yPos);
                }
                edgePointL = true;
            }
            p.lineTo(xPos, yPos);
        }

        ColourGradient grad(COLOUR1, frame.getX() + frame.getWidth() / 2, frame.getY() + frame.getHeight() / 2,
            COLOUR6, frame.getX(), frame.getY() + frame.getHeight() / 2, true);
        g.setGradientFill(grad);
        g.strokePath(p, PathStrokeType(2.0));

    }
    
    // draw shadow 1
    //ColourGradient shadowGrad1(Colour(0, 0, 0).withAlpha(0.5f), 0, 50,
    //                          Colour(0, 0, 0).withAlpha(0.f), 0, 55, false);
    //g.setGradientFill(shadowGrad1);
    //g.fillRect(0, part1, getWidth(), 25);
    
    // draw shadow 2
    ColourGradient shadowGrad2(Colour(0, 0, 0).withAlpha(0.5f), getWidth() / 2.f, getHeight() / 2.f - 75,
                              Colour(0, 0, 0).withAlpha(0.f), getWidth() / 2.f, getHeight() / 2.f - 70, false);
    //g.setGradientFill(shadowGrad2);
    //g.fillRect(0, part1 + part2, getWidth(), 25);
    
    //g.setColour(COLOUR1);
    //g.fillRect(outputKnob.getX(), outputKnob.getY(), outputKnob.getWidth(), outputKnob.getHeight());
    //g.fillRect(cutoffKnob.getX(), cutoffKnob.getY(), cutoffKnob.getWidth(), cutoffKnob.getHeight());
}

void FireAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    // knobs
    int knobNum = 8;
    float scale = jmin(getHeight() / 500.f, getWidth() / 1000.f);
    float scaleMax = jmax(getHeight() / 500.f, getWidth() / 1000.f);
    int newKnobSize = static_cast<int> (knobSize * scale);
    int startX = getWidth() / knobNum;
    int secondShadowY = getHeight() / 10 * 4;
    int firstLineY = secondShadowY + (getHeight() - secondShadowY) * 2 / 5 - newKnobSize / 2;
    int secondLineY = secondShadowY + (getHeight() - secondShadowY) * 4 / 5  - newKnobSize / 2;
    
    // save presets
    Rectangle<int> r (getLocalBounds());
    r = r.removeFromTop (50 * getHeight() / 500);
    r.removeFromLeft(100 * scaleMax);
    r.removeFromRight(50 * scaleMax);
    
    stateComponent.setBounds (r);
        
    // first line
    // inputKnob.setBounds(startX * 1 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    driveKnob.setBounds(startX * 1 - newKnobSize / 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize * 2, newKnobSize * 2);
    //colorKnob.setBounds(startX * 1 + newKnobSize * 1.2, secondLineY, newKnobSize, newKnobSize);
    downSampleKnob.setBounds(startX * 3 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    recKnob.setBounds(startX * 4 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    colorKnob.setBounds(startX * 5 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    biasKnob.setBounds(startX * 6 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    outputKnob.setBounds(startX * 7 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    
    
    // second line
    cutoffKnob.setBounds(startX * 5 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);
    resKnob.setBounds(startX * 6 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);
    mixKnob.setBounds(startX * 7 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);

    // first line
    hqButton.setBounds(getHeight() / 10, 0, getHeight() / 10, getHeight() / 10);
    linkedButton.setBounds(startX * 1 + newKnobSize * 3 / 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize / 2, 0.05 * getHeight());
    

    // second line
    safeButton.setBounds(startX * 1 + newKnobSize * 3 / 2, secondLineY, newKnobSize / 2, 0.05 * getHeight());
    filterOffButton.setBounds(startX * 3 - newKnobSize / 4, secondLineY, newKnobSize / 2, 0.05 * getHeight());
    filterPreButton.setBounds(startX * 3 - newKnobSize / 4, secondLineY + 0.055 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterPostButton.setBounds(startX * 3 - newKnobSize / 4, secondLineY + 0.11 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterLowButton.setBounds(startX * 4 - newKnobSize / 4, secondLineY, newKnobSize / 2, 0.05 * getHeight());
    filterBandButton.setBounds(startX * 4 - newKnobSize / 4, secondLineY + 0.055 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterHighButton.setBounds(startX * 4 - newKnobSize / 4, secondLineY + 0.11 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    
    // about
    // aboutButton.setBounds(getWidth() - 100, 0, 100, 50);
    
    // visualiser
    processor.visualiser.setBounds(0, getHeight() / 10 + 10, getWidth() / 2, getHeight() / 10 * 3 - 10);

    // ff meter
    int ffWidth = 20;
    int ffHeightStart = getHeight() / 10;
    int ffHeight = getHeight() / 10 * 3;
    inputMeter.setBounds(getWidth() / 2 - ffWidth - 2, ffHeightStart, ffWidth, ffHeight + 2);
    outputMeter.setBounds(getWidth() / 2 + 2, ffHeightStart, ffWidth, ffHeight + 2);
    
    // distortion menu
    distortionMode.setBounds(0, getHeight() / 10 * 4, getWidth() / 5, getHeight() / 10);
    
    otherLookAndFeel.scale = scale;
    roundedButtonLnf.scale = scale;
}

void FireAudioProcessorEditor::updateToggleState (Button* button, String name)
{
    //auto state = button->getToggleState();
}

void FireAudioProcessorEditor::timerCallback()
{
    repaint();
}

void FireAudioProcessorEditor::setDriveKnobState(Slider* slider)
{
    if (*processor.treeState.getRawParameterValue("mode") == 0)
    {
        slider->setValue(1);
        slider->setEnabled(false);
    }
    else
    {
        slider->setEnabled(true);
    }
}

//void FireAudioProcessorEditor::setColorKnobState(Slider* slider)
//{
//    if (*processor.treeState.getRawParameterValue("mode") == 6)
//    {
//        slider->setEnabled(true);
//    }
//    else
//    {
//        //slider->setValue(0);
//        slider->setEnabled(false);
//    }
//}

// if filter is off, set lp, bp, hp buttons and cutoff knob disable.
void FireAudioProcessorEditor::setCutoffButtonState(TextButton* textButton, Slider* slider1, Slider* slider2)
{
    if (*processor.treeState.getRawParameterValue("off"))
    {
        textButton->setEnabled(false);
        slider1->setEnabled(false);
        slider2->setEnabled(false);
    }
    else
    {
        textButton->setEnabled(true);
        slider1->setEnabled(true);
        slider2->setEnabled(true);
    }
}
