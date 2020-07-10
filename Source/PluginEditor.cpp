/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "0.695"
//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    // timer
    Timer::startTimerHz(20.0f);


    // resize not avaialble
    setResizable(true, true);

    
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
    lnf.setColour(foleys::LevelMeter::lmMeterGradientLowColour, COLOUR2);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMidColour, COLOUR2);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMaxColour, COLOUR4);
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

    // input knob
    addAndMakeVisible(inputKnob);
    inputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    inputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(inputLabel);
    inputLabel.setText("Input", dontSendNotification);
    inputLabel.setFont (Font (KNOB_FONT, 16.0f, Font::plain));
    inputLabel.setColour(Label::textColourId, COLOUR5);
    inputLabel.attachToComponent(&inputKnob, false);
    inputLabel.setJustificationType (Justification::centred);

    // drive knob
    addAndMakeVisible(driveKnob);
    driveKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    driveKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    driveKnob.addListener(this);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", dontSendNotification);
    driveLabel.setFont (Font (KNOB_FONT, 16.0f, Font::plain));
    driveLabel.setColour(Label::textColourId, COLOUR5);
    driveLabel.attachToComponent(&driveKnob, false);
    driveLabel.setJustificationType (Justification::centred);
    
    // down sample knob
    addAndMakeVisible(downSampleKnob);
    downSampleKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    downSampleKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    downSampleKnob.addListener(this);

    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", dontSendNotification);
    downSampleLabel.setFont(Font(KNOB_FONT, 16.0f, Font::plain));
    downSampleLabel.setColour(Label::textColourId, COLOUR5);
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(Justification::centred);

    // output knob
    addAndMakeVisible(outputKnob);
    outputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    outputKnob.addListener(this);

    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", dontSendNotification);
    outputLabel.setFont (Font (KNOB_FONT, 16.0f, Font::plain));
    outputLabel.setColour(Label::textColourId, COLOUR5);
    outputLabel.attachToComponent(&outputKnob, false);
    outputLabel.setJustificationType (Justification::centred);

    // rec knob
    addAndMakeVisible(recKnob);
    recKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    recKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    recKnob.addListener(this);

    addAndMakeVisible(recLabel);
    recLabel.setText("Rectification", dontSendNotification);
    recLabel.setFont (Font (KNOB_FONT, 16.0f, Font::plain));
    recLabel.setColour(Label::textColourId, COLOUR5);
    recLabel.attachToComponent(&recKnob, false);
    recLabel.setJustificationType (Justification::centred);
    
    // mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", dontSendNotification);
    mixLabel.setFont (Font (KNOB_FONT, 16.0f, Font::plain));
    mixLabel.setColour(Label::textColourId, COLOUR5);
    mixLabel.attachToComponent(&mixKnob, false);
    mixLabel.setJustificationType (Justification::centred);
    
    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    
    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", dontSendNotification);
    cutoffLabel.setFont (Font (KNOB_FONT, 16.0f, Font::plain));
    cutoffLabel.setColour(Label::textColourId, COLOUR5);
    cutoffLabel.attachToComponent(&cutoffKnob, false);
    cutoffLabel.setJustificationType (Justification::centred);
    
    // res knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(resLabel);
    resLabel.setText("Q", dontSendNotification); // Resonance
    resLabel.setFont (Font (KNOB_FONT, 16.0f, Font::plain));
    resLabel.setColour(Label::textColourId, COLOUR5);
    resLabel.attachToComponent(&resKnob, false);
    resLabel.setJustificationType (Justification::centred);
    
    // HQ(oversampling) Toggle Buttons
    addAndMakeVisible(hqButton);
    hqButton.setClickingTogglesState(true);
    bool hqButtonState = *processor.treeState.getRawParameterValue("hq");
    hqButton.setToggleState(hqButtonState, dontSendNotification);
    hqButton.onClick = [this] { updateToggleState(&hqButton, "Hq"); };
    hqButton.setColour(ToggleButton::textColourId, COLOUR5);
    hqButton.setColour(ToggleButton::tickColourId, COLOUR5);
    hqButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);

    //addAndMakeVisible(hqLabel);
    //hqLabel.setText("HQ", dontSendNotification);
    //hqLabel.setFont(Font(KNOB_FONT, 16.0f, Font::plain));
    //hqLabel.setColour(Label::textColourId, COLOUR5);
    //hqLabel.attachToComponent(&hqButton, false);
    //hqLabel.setJustificationType(Justification::centredRight);

    // Linked Toggle Buttons
    addAndMakeVisible(linkedButton);
    linkedButton.setClickingTogglesState(true);
    bool linkedButtonState = *processor.treeState.getRawParameterValue("linked");
    linkedButton.setToggleState(linkedButtonState, dontSendNotification);
    linkedButton.onClick = [this] { updateToggleState (&linkedButton, "Linked"); };
    linkedButton.setColour(ToggleButton::textColourId, COLOUR5);
    linkedButton.setColour(ToggleButton::tickColourId, COLOUR5);
    linkedButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);

    //addAndMakeVisible(linkedLabel);
    //linkedLabel.setText("Link", dontSendNotification);
    //linkedLabel.setFont(Font(KNOB_FONT, 16.0f, Font::plain));
    //linkedLabel.setColour(Label::textColourId, COLOUR5);
    //linkedLabel.attachToComponent(&linkedButton, false);
    //linkedLabel.setJustificationType(Justification::centredBottom);
    //linkedLabel.setBounds(linkedButton.getX(), linkedButton.getY(), 100, 25);

    // Rectification Toggle Buttons
    //addAndMakeVisible(recOffButton);
//    recOffButton.setClickingTogglesState(true);
//    recOffButton.setRadioGroupId(recStateButtons);
//    bool refOffButtonState = *processor.treeState.getRawParameterValue("recOff");
//    recOffButton.setToggleState(refOffButtonState, dontSendNotification);
//    recOffButton.onClick = [this] { updateToggleState(&recOffButton, "RecOff"); };
//    recOffButton.setColour(ToggleButton::textColourId, COLOUR5);
//    recOffButton.setColour(ToggleButton::tickColourId, COLOUR5);
//    recOffButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);

    //addAndMakeVisible(recHalfButton);
//    recHalfButton.setClickingTogglesState(true);
//    recHalfButton.setRadioGroupId(recStateButtons);
//    bool refHalfButtonState = *processor.treeState.getRawParameterValue("recHalf");
//    recHalfButton.setToggleState(refHalfButtonState, dontSendNotification);
//    recHalfButton.onClick = [this] { updateToggleState(&recHalfButton, "RecHalf"); };
//    recHalfButton.setColour(ToggleButton::textColourId, COLOUR5);
//    recHalfButton.setColour(ToggleButton::tickColourId, COLOUR5);
//    recHalfButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);

    //addAndMakeVisible(recFullButton);
//    recFullButton.setClickingTogglesState(true);
//    recFullButton.setRadioGroupId(recStateButtons);
//    bool recFullButtonState = *processor.treeState.getRawParameterValue("recFull");
//    recFullButton.setToggleState(recFullButtonState, dontSendNotification);
//    recFullButton.onClick = [this] { updateToggleState(&recFullButton, "RecFull"); };
//    recFullButton.setColour(ToggleButton::textColourId, COLOUR5);
//    recFullButton.setColour(ToggleButton::tickColourId, COLOUR5);
//    recFullButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);

    // Filter State Toggle Buttons
    addAndMakeVisible(filterOffButton);
    filterOffButton.setClickingTogglesState(true);
    filterOffButton.setRadioGroupId(filterStateButtons);
    bool filterOffButtonState = *processor.treeState.getRawParameterValue("off");
    filterOffButton.setToggleState(filterOffButtonState, dontSendNotification);
    filterOffButton.onClick = [this] { updateToggleState (&filterOffButton, "Off"); };
    filterOffButton.setColour(ToggleButton::textColourId, COLOUR5);
    filterOffButton.setColour(ToggleButton::tickColourId, COLOUR5);
    filterOffButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);
    
    addAndMakeVisible(filterOffLabel);
    filterOffLabel.setText("Filter", dontSendNotification);
    filterOffLabel.setFont(Font(KNOB_FONT, 16.0f, Font::plain));
    filterOffLabel.setColour(Label::textColourId, COLOUR5);
    filterOffLabel.attachToComponent(&filterOffButton, false);
    filterOffLabel.setJustificationType(Justification::centred);

    addAndMakeVisible(filterPreButton);
    filterPreButton.setClickingTogglesState(true);
    filterPreButton.setRadioGroupId(filterStateButtons);
    bool filterPreButtonState = *processor.treeState.getRawParameterValue("pre");
    filterPreButton.setToggleState(filterPreButtonState, dontSendNotification);
    filterPreButton.onClick = [this] { updateToggleState (&filterPreButton, "Pre"); };
    filterPreButton.setColour(ToggleButton::textColourId, COLOUR5);
    filterPreButton.setColour(ToggleButton::tickColourId, COLOUR5);
    filterPreButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);
    
    //addAndMakeVisible(filterPreLabel);
    //filterPreLabel.setText("Filter Pre", dontSendNotification);
    //filterPreLabel.setFont(Font(KNOB_FONT, 16.0f, Font::plain));
    //filterPreLabel.setColour(Label::textColourId, COLOUR5);
    //filterPreLabel.attachToComponent(&filterPreButton, false);
    //filterPreLabel.setJustificationType(Justification::bottomRight);

    addAndMakeVisible(filterPostButton);
    filterPostButton.setClickingTogglesState(true);
    filterPostButton.setRadioGroupId(filterStateButtons);
    bool filterPostButtonState = *processor.treeState.getRawParameterValue("post");
    filterPostButton.setToggleState(filterPostButtonState, dontSendNotification);
    filterPostButton.onClick = [this] { updateToggleState (&filterPostButton, "Post"); };
    filterPostButton.setColour(ToggleButton::textColourId, COLOUR5);
    filterPostButton.setColour(ToggleButton::tickColourId, COLOUR5);
    filterPostButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);
    
    //addAndMakeVisible(filterPostLabel);
    //filterPostLabel.setText("Filter Post", dontSendNotification);
    //filterPostLabel.setFont(Font(KNOB_FONT, 16.0f, Font::plain));
    //filterPostLabel.setColour(Label::textColourId, COLOUR5);
    //filterPostLabel.attachToComponent(&filterPostButton, false);
    //filterPostLabel.setJustificationType(Justification::bottomRight);

    // Filter Type Toggle Buttons
    addAndMakeVisible(filterLowButton);
    filterLowButton.setClickingTogglesState(true);
    filterLowButton.setRadioGroupId(filterModeButtons);
    bool filterLowButtonState = *processor.treeState.getRawParameterValue("low");
    filterLowButton.setToggleState(filterLowButtonState, dontSendNotification);
    filterLowButton.onClick = [this] { updateToggleState (&filterLowButton, "Low"); };
    filterLowButton.setColour(ToggleButton::textColourId, COLOUR5);
    filterLowButton.setColour(ToggleButton::tickColourId, COLOUR5);
    filterLowButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);
    
    addAndMakeVisible(filterLowLabel);
    filterLowLabel.setText("Type", dontSendNotification);
    filterLowLabel.setFont(Font(KNOB_FONT, 16.0f, Font::plain));
    filterLowLabel.setColour(Label::textColourId, COLOUR5);
    filterLowLabel.attachToComponent(&filterLowButton, false);
    filterLowLabel.setJustificationType(Justification::centred);
    
    addAndMakeVisible(filterBandButton);
    filterBandButton.setClickingTogglesState(true);
    filterBandButton.setRadioGroupId(filterModeButtons);
    bool filterBandButtonState = *processor.treeState.getRawParameterValue("band");
    filterBandButton.setToggleState(filterBandButtonState, dontSendNotification);
    filterBandButton.onClick = [this] { updateToggleState (&filterBandButton, "Band"); };
    filterBandButton.setColour(ToggleButton::textColourId, COLOUR5);
    filterBandButton.setColour(ToggleButton::tickColourId, COLOUR5);
    filterBandButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);
    
    addAndMakeVisible(filterHighButton);
    filterHighButton.setClickingTogglesState(true);
    filterHighButton.setRadioGroupId(filterModeButtons);
    bool filterHighButtonState = *processor.treeState.getRawParameterValue("high");
    filterHighButton.setToggleState(filterHighButtonState, dontSendNotification);
    filterHighButton.onClick = [this] { updateToggleState (&filterHighButton, "High"); };
    filterHighButton.setColour(ToggleButton::textColourId, COLOUR5);
    filterHighButton.setColour(ToggleButton::tickColourId, COLOUR5);
    filterHighButton.setColour(ToggleButton::tickDisabledColourId, COLOUR5);
    
    // Attachment
    inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob);
    downSampleAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "downSample", downSampleKnob);
    recAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "rec", recKnob);
    outputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "outputGain", outputKnob);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    cutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "cutoff", cutoffKnob);
    resAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "res", resKnob);
    
    hqAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "hq", hqButton);
    linkedAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "linked", linkedButton);
    //recOffAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "recOff", recOffButton);
    //recHalfAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "recHalf", recHalfButton);
    //recFullAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "recFull", recFullButton);
    filterOffAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "off", filterOffButton);
    filterPreAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "pre", filterPreButton);
    filterPostAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "post", filterPostButton);
    filterLowAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "low", filterLowButton);
    filterBandAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "band", filterBandButton);
    filterHighAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "high", filterHighButton);
    
    // Visualiser
    // addAndMakeVisible(p.visualiser);

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
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Foldback");
    distortionMode.addItem("Sin Foldback", 7);
    distortionMode.addItem("Linear Foldback", 8);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Asymmetrical Clipping");
    distortionMode.addItem("Diode Clipping 1", 9);
    distortionMode.addSeparator();

    // distortionMode.setSelectedId (1);
    distortionMode.setJustificationType(Justification::centred);
    distortionMode.setColour(ComboBox::textColourId, COLOUR1);
    distortionMode.setColour(ComboBox::arrowColourId, COLOUR1);
    distortionMode.setColour(ComboBox::buttonColourId, COLOUR1);
    distortionMode.setColour(ComboBox::outlineColourId, COLOUR5);
    distortionMode.setColour(ComboBox::focusedOutlineColourId, COLOUR1);
    distortionMode.setColour(ComboBox::backgroundColourId, COLOUR5);
    distortionMode.getLookAndFeel().setColour(PopupMenu::textColourId, COLOUR1);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, COLOUR2);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, COLOUR5);
    distortionMode.getLookAndFeel().setColour(PopupMenu::headerTextColourId, COLOUR1);
    distortionMode.getLookAndFeel().setColour(PopupMenu::backgroundColourId, COLOUR5);

    modeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);
    
    
    // about button
    addAndMakeVisible(aboutButton);
    aboutButton.setColour(TextButton::buttonColourId, COLOUR3);
    aboutButton.setColour(TextButton::textColourOnId, COLOUR3);
    aboutButton.setColour(TextButton::textColourOffId, COLOUR3);
//    aboutButton.onClick = [this] { aboutDialog.showMessageBoxAsync(AlertWindow::InfoIcon, "Made by Wings", "And fuck you", "OK"); };
    // resize not available now
    setResizeLimits(1000, 500, 2000, 1000); // set resize limits
    //getConstrainer ()->setFixedAspectRatio (1.33); // set fixed resize rate: 700/525
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    inputMeter.setLookAndFeel(nullptr);
    outputMeter.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
}

float f(float x)
{
    return 0.5;
}

//==============================================================================
void FireAudioProcessorEditor::paint(Graphics &g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillAll(COLOUR3);

    // title
    g.setColour(COLOUR5);
    g.fillRect(0, 0, getWidth(), 50);

    // draw knob background
    g.setColour(COLOUR1);
    g.fillRoundedRectangle(25, getHeight() / 2 - 50, getWidth() - 50, getHeight() / 2 + 25, 25);
    
    // draw ff meter background
    int ffBackgroundWidth = getWidth() / 4;
    g.setColour(COLOUR0);
    g.fillRoundedRectangle(getWidth() / 2 - ffBackgroundWidth / 2, getHeight() / 4 * 3 - 25, ffBackgroundWidth, getHeight() / 4 - 25, 25);
    // set background
    // Image background = ImageCache::getFromMemory(BinaryData::blood_background_png, (size_t) BinaryData::blood_background_pngSize);
    // g.drawImage(background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    
    // draw version
    g.setColour(COLOUR5);
    g.setFont (Font ("Times New Roman", 18.0f, Font::bold));
    String version = (String) "Ver. " + (String)VERSION;
    Rectangle<int> area(getWidth() - 100, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, Justification::topLeft, 1);
       
    // set logo "Fire"
    Image logo = ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t) BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, logo.getWidth()/4, logo.getHeight()/4, 0, 0, logo.getWidth(), logo.getHeight());
    
    // set logo "Wings"
    Image logoWings = ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t) BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - 50, 0, logoWings.getWidth()/4, logoWings.getHeight()/4, 0, 0, logoWings.getWidth(), logoWings.getHeight());
    
    // paint distortion function
    float functionValue = 0;
    float mixValue;

    int mode = *processor.treeState.getRawParameterValue("mode");
    float inputGain = *processor.treeState.getRawParameterValue("inputGain");
    float drive = *processor.treeState.getRawParameterValue("drive"); // (deleted drive)
    float mix = *processor.treeState.getRawParameterValue("mix");

    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.mix = mix;


    auto frame = getLocalBounds(); // adjust here, if you want to paint in a special location
    // frame.setBounds(getWidth() / 2, 50 - 1, getWidth() / 2 - 50 + 2, (getHeight() / 3 + 2)); // this is old
    frame.setBounds(getWidth() / 2, 50, getWidth() / 2, getHeight() / 2 - 125);
    
    // draw layer 2
    g.setColour(COLOUR4);
    g.fillRect(0, 50, getWidth(), getHeight() / 2 - 125);
    
    if (mode < 8)
    {
        const int numPix = frame.getWidth(); // you might experiment here, if you want less steps to speed up

        float driveScale = 1;
        if (inputGain >= 0)
        {
            driveScale = pow(2, inputGain / 6.0f);
        }
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
            if (mode < 8)
            {
                functionValue = distortionProcessor.distortionProcess(value);
            }
            
            // downsample
            float rateDivide = *processor.treeState.getRawParameterValue("downSample");
            if (rateDivide > 1)
            {
                functionValue = ceilf(functionValue * (64.f / rateDivide)) / (64.f / rateDivide);
            }
            
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

        // g.setColour(COLOUR1);
        ColourGradient grad(COLOUR1, frame.getX() + frame.getWidth() / 2, frame.getY() + frame.getHeight() / 2,
            COLOUR4, frame.getX(), frame.getY() + frame.getHeight() / 2, true);
        g.setGradientFill(grad);
        g.strokePath(p, PathStrokeType(2.0));

        
        
        // draw combobox outside rect
        // g.setColour(mainColour);
        // g.drawRect(50-2, (getHeight() / 3) + 50-2, getWidth() / 2 - 50+4, 50+4, 2);
    }
    
    // draw shadow 1
    ColourGradient shadowGrad1(Colour(0, 0, 0).withAlpha(0.5f), 0, 50,
                              Colour(0, 0, 0).withAlpha(0.f), 0, 55, false);
    g.setGradientFill(shadowGrad1);
    g.fillRect(0, 50, getWidth(), 25);
    
    // draw shadow 2
    ColourGradient shadowGrad2(Colour(0, 0, 0).withAlpha(0.5f), getWidth()/2, getHeight() / 2 - 75,
                              Colour(0, 0, 0).withAlpha(0.f), getWidth()/2, getHeight() / 2 - 70, false);
    g.setGradientFill(shadowGrad2);
    g.fillRect(0, getHeight() / 2 - 75, getWidth(), 25);
    
    // repaint(); // HIGH CPU WARNING!!!
}

void FireAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    // knobs
    int knobNum = 7;
    float scale = jmin(getHeight() / 500.f, getWidth() / 1000.f);
    int newKnobSize = static_cast<int> (knobSize * scale);
    int startX = getWidth() / knobNum;
    int secondShadowY = getHeight() / 2 - 75;
    int firstLineY = secondShadowY + (getHeight() - secondShadowY) / 4 + 25 - newKnobSize / 2;
    int secondLineY = secondShadowY + (getHeight() - secondShadowY) / 4 * 3 - newKnobSize / 2;
    
    // first line
    inputKnob.setBounds(startX * 1 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    driveKnob.setBounds(startX * 2 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    downSampleKnob.setBounds(startX * 3 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    recKnob.setBounds(startX * 4 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    outputKnob.setBounds(startX * 5 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    mixKnob.setBounds(startX * 6 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    
    // second line
    cutoffKnob.setBounds(startX * 5 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);
    resKnob.setBounds(startX * 6 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);
    
    // toggle buttons
    
    // first line
    hqButton.setBounds(startX * 1 - 120, firstLineY, 100, 25);
    linkedButton.setBounds(startX * 5 + newKnobSize / 2, firstLineY, 100, 25);
    //recOffButton.setBounds(startX * 4 - knobSize / 2, getHeight() / 2 + 7 - 25, 100, 25);
    //recHalfButton.setBounds(startX * 4 - knobSize / 2, getHeight() / 2 + 7 + 25 - 25, 100, 25);
    //recFullButton.setBounds(startX * 4 - knobSize / 2, getHeight() / 2 + 7 + 50 - 25, 100, 25);

    // second line
    filterOffButton.setBounds(startX * 1 - knobSize / 2, secondLineY, 100, 25);
    filterPreButton.setBounds(startX * 1 - knobSize / 2, secondLineY +25, 100, 25);
    filterPostButton.setBounds(startX * 1 - knobSize / 2, secondLineY +50, 100, 25);
    filterLowButton.setBounds(startX * 2 - knobSize / 2, secondLineY, 100, 25);
    filterBandButton.setBounds(startX * 2 - knobSize / 2, secondLineY +25, 100, 25);
    filterHighButton.setBounds(startX * 2 - knobSize / 2, secondLineY +50, 100, 25);
    
    // about
    // aboutButton.setBounds(getWidth() - 100, 0, 100, 50);
    
    // visualiser
    // processor.visualiser.setBounds(0, 50, getWidth() / 2, getHeight() / 2 - 125);

    // ff meter
    int ffWidth = 50;
    int ffHeightStart = getHeight() / 4 * 3 - 25;
    int ffHeight = getHeight() / 4 - 25;
    inputMeter.setBounds(getWidth() / 2 - ffWidth - 10, ffHeightStart, ffWidth, ffHeight + 2);
    outputMeter.setBounds(getWidth() / 2 + 10, ffHeightStart, ffWidth, ffHeight + 2);
    
    // distortion menu
    distortionMode.setBounds(getWidth() /2 - 100, 0, 200, 50);
    
}

void FireAudioProcessorEditor::updateToggleState (Button* button, String name)
{
    //auto state = button->getToggleState();
}

void FireAudioProcessorEditor::timerCallback()
{
    repaint();
}
