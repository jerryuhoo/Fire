/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "0.69"
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
    lnf.setColour(foleys::LevelMeter::lmTextColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmTextClipColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmTextDeactiveColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmTicksColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmOutlineColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmBackgroundColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmBackgroundClipColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmMeterForegroundColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmMeterOutlineColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmMeterBackgroundColour, Colour1);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxNormalColour, Colour3);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxWarnColour, Colour5);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxOverColour, Colour6);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientLowColour, Colour5);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMidColour, Colour5);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMaxColour, Colour5);
    lnf.setColour(foleys::LevelMeter::lmMeterReductionColour, Colour3);
    
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
    inputLabel.setFont (Font ("Cascadia Code", 16.0f, Font::plain));
    inputLabel.setColour(Label::textColourId, Colour5);
    inputLabel.attachToComponent(&inputKnob, false);
    inputLabel.setJustificationType (Justification::centred);

    // drive knob
    addAndMakeVisible(driveKnob);
    driveKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    driveKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    driveKnob.addListener(this);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", dontSendNotification);
    driveLabel.setFont (Font ("Cascadia Code", 16.0f, Font::plain));
    driveLabel.setColour(Label::textColourId, Colour5);
    driveLabel.attachToComponent(&driveKnob, false);
    driveLabel.setJustificationType (Justification::centred);
    
    // down sample knob
    addAndMakeVisible(downSampleKnob);
    downSampleKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    downSampleKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    downSampleKnob.addListener(this);

    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", dontSendNotification);
    downSampleLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    downSampleLabel.setColour(Label::textColourId, Colour5);
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(Justification::centred);

    // output knob
    addAndMakeVisible(outputKnob);
    outputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    outputKnob.addListener(this);

    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", dontSendNotification);
    outputLabel.setFont (Font ("Cascadia Code", 16.0f, Font::plain));
    outputLabel.setColour(Label::textColourId, Colour5);
    outputLabel.attachToComponent(&outputKnob, false);
    outputLabel.setJustificationType (Justification::centred);

    // mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", dontSendNotification);
    mixLabel.setFont (Font ("Cascadia Code", 16.0f, Font::plain));
    mixLabel.setColour(Label::textColourId, Colour5);
    mixLabel.attachToComponent(&mixKnob, false);
    mixLabel.setJustificationType (Justification::centred);
    
    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    
    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", dontSendNotification);
    cutoffLabel.setFont (Font ("Cascadia Code", 16.0f, Font::plain));
    cutoffLabel.setColour(Label::textColourId, Colour5);
    cutoffLabel.attachToComponent(&cutoffKnob, false);
    cutoffLabel.setJustificationType (Justification::centred);
    
    // res knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(resLabel);
    resLabel.setText("Q", dontSendNotification); // Resonance
    resLabel.setFont (Font ("Cascadia Code", 16.0f, Font::plain));
    resLabel.setColour(Label::textColourId, Colour5);
    resLabel.attachToComponent(&resKnob, false);
    resLabel.setJustificationType (Justification::centred);
    
    // HQ(oversampling) Toggle Buttons
    addAndMakeVisible(hqButton);
    hqButton.setClickingTogglesState(true);
    bool hqButtonState = *processor.treeState.getRawParameterValue("hq");
    hqButton.setToggleState(hqButtonState, dontSendNotification);
    hqButton.onClick = [this] { updateToggleState(&hqButton, "Hq"); };
    hqButton.setColour(ToggleButton::textColourId, Colour5);
    hqButton.setColour(ToggleButton::tickColourId, Colour5);
    hqButton.setColour(ToggleButton::tickDisabledColourId, Colour5);

    //addAndMakeVisible(hqLabel);
    //hqLabel.setText("HQ", dontSendNotification);
    //hqLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    //hqLabel.setColour(Label::textColourId, Colour5);
    //hqLabel.attachToComponent(&hqButton, false);
    //hqLabel.setJustificationType(Justification::centredRight);

    // Linked Toggle Buttons
    addAndMakeVisible(linkedButton);
    linkedButton.setClickingTogglesState(true);
    bool linkedButtonState = *processor.treeState.getRawParameterValue("linked");
    linkedButton.setToggleState(linkedButtonState, dontSendNotification);
    linkedButton.onClick = [this] { updateToggleState (&linkedButton, "Linked"); };
    linkedButton.setColour(ToggleButton::textColourId, Colour5);
    linkedButton.setColour(ToggleButton::tickColourId, Colour5);
    linkedButton.setColour(ToggleButton::tickDisabledColourId, Colour5);
    
    //addAndMakeVisible(linkedLabel);
    //linkedLabel.setText("Link", dontSendNotification);
    //linkedLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    //linkedLabel.setColour(Label::textColourId, Colour5);
    //linkedLabel.attachToComponent(&linkedButton, false);
    //linkedLabel.setJustificationType(Justification::centredBottom);
    //linkedLabel.setBounds(linkedButton.getX(), linkedButton.getY(), 100, 25);

    // Rectification Toggle Buttons
    addAndMakeVisible(recOffButton);
    recOffButton.setClickingTogglesState(true);
    recOffButton.setRadioGroupId(recStateButtons);
    bool refOffButtonState = *processor.treeState.getRawParameterValue("recOff");
    recOffButton.setToggleState(refOffButtonState, dontSendNotification);
    recOffButton.onClick = [this] { updateToggleState(&recOffButton, "RecOff"); };
    recOffButton.setColour(ToggleButton::textColourId, Colour5);
    recOffButton.setColour(ToggleButton::tickColourId, Colour5);
    recOffButton.setColour(ToggleButton::tickDisabledColourId, Colour5);

    addAndMakeVisible(recOffLabel);
    recOffLabel.setText("Rectification", dontSendNotification);
    recOffLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    recOffLabel.setColour(Label::textColourId, Colour5);
    recOffLabel.attachToComponent(&recOffButton, false);
    recOffLabel.setJustificationType(Justification::centred);

    addAndMakeVisible(recHalfButton);
    recHalfButton.setClickingTogglesState(true);
    recHalfButton.setRadioGroupId(recStateButtons);
    bool refHalfButtonState = *processor.treeState.getRawParameterValue("recHalf");
    recHalfButton.setToggleState(refHalfButtonState, dontSendNotification);
    recHalfButton.onClick = [this] { updateToggleState(&recHalfButton, "RecHalf"); };
    recHalfButton.setColour(ToggleButton::textColourId, Colour5);
    recHalfButton.setColour(ToggleButton::tickColourId, Colour5);
    recHalfButton.setColour(ToggleButton::tickDisabledColourId, Colour5);

    //addAndMakeVisible(recHalfLabel);
    //recHalfLabel.setText("Rec Half", dontSendNotification);
    //recHalfLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    //recHalfLabel.setColour(Label::textColourId, Colour5);
    //recHalfLabel.attachToComponent(&recHalfButton, false);
    //recHalfLabel.setJustificationType(Justification::bottomRight);

    addAndMakeVisible(recFullButton);
    recFullButton.setClickingTogglesState(true);
    recFullButton.setRadioGroupId(recStateButtons);
    bool recFullButtonState = *processor.treeState.getRawParameterValue("recFull");
    recFullButton.setToggleState(recFullButtonState, dontSendNotification);
    recFullButton.onClick = [this] { updateToggleState(&recFullButton, "RecFull"); };
    recFullButton.setColour(ToggleButton::textColourId, Colour5);
    recFullButton.setColour(ToggleButton::tickColourId, Colour5);
    recFullButton.setColour(ToggleButton::tickDisabledColourId, Colour5);

    //addAndMakeVisible(recFullLabel);
    //recFullLabel.setText("Rec Full", dontSendNotification);
    //recFullLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    //recFullLabel.setColour(Label::textColourId, Colour5);
    //recFullLabel.attachToComponent(&recFullButton, false);
    //recFullLabel.setJustificationType(Justification::bottomRight);

    // Filter State Toggle Buttons
    addAndMakeVisible(filterOffButton);
    filterOffButton.setClickingTogglesState(true);
    filterOffButton.setRadioGroupId(filterStateButtons);
    bool filterOffButtonState = *processor.treeState.getRawParameterValue("off");
    filterOffButton.setToggleState(filterOffButtonState, dontSendNotification);
    filterOffButton.onClick = [this] { updateToggleState (&filterOffButton, "Off"); };
    filterOffButton.setColour(ToggleButton::textColourId, Colour5);
    filterOffButton.setColour(ToggleButton::tickColourId, Colour5);
    filterOffButton.setColour(ToggleButton::tickDisabledColourId, Colour5);
    
    addAndMakeVisible(filterOffLabel);
    filterOffLabel.setText("Filter", dontSendNotification);
    filterOffLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    filterOffLabel.setColour(Label::textColourId, Colour5);
    filterOffLabel.attachToComponent(&filterOffButton, false);
    filterOffLabel.setJustificationType(Justification::centred);

    addAndMakeVisible(filterPreButton);
    filterPreButton.setClickingTogglesState(true);
    filterPreButton.setRadioGroupId(filterStateButtons);
    bool filterPreButtonState = *processor.treeState.getRawParameterValue("pre");
    filterPreButton.setToggleState(filterPreButtonState, dontSendNotification);
    filterPreButton.onClick = [this] { updateToggleState (&filterPreButton, "Pre"); };
    filterPreButton.setColour(ToggleButton::textColourId, Colour5);
    filterPreButton.setColour(ToggleButton::tickColourId, Colour5);
    filterPreButton.setColour(ToggleButton::tickDisabledColourId, Colour5);
    
    //addAndMakeVisible(filterPreLabel);
    //filterPreLabel.setText("Filter Pre", dontSendNotification);
    //filterPreLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    //filterPreLabel.setColour(Label::textColourId, Colour5);
    //filterPreLabel.attachToComponent(&filterPreButton, false);
    //filterPreLabel.setJustificationType(Justification::bottomRight);

    addAndMakeVisible(filterPostButton);
    filterPostButton.setClickingTogglesState(true);
    filterPostButton.setRadioGroupId(filterStateButtons);
    bool filterPostButtonState = *processor.treeState.getRawParameterValue("post");
    filterPostButton.setToggleState(filterPostButtonState, dontSendNotification);
    filterPostButton.onClick = [this] { updateToggleState (&filterPostButton, "Post"); };
    filterPostButton.setColour(ToggleButton::textColourId, Colour5);
    filterPostButton.setColour(ToggleButton::tickColourId, Colour5);
    filterPostButton.setColour(ToggleButton::tickDisabledColourId, Colour5);
    
    //addAndMakeVisible(filterPostLabel);
    //filterPostLabel.setText("Filter Post", dontSendNotification);
    //filterPostLabel.setFont(Font("Cascadia Code", 16.0f, Font::plain));
    //filterPostLabel.setColour(Label::textColourId, Colour5);
    //filterPostLabel.attachToComponent(&filterPostButton, false);
    //filterPostLabel.setJustificationType(Justification::bottomRight);

    // Filter Type Toggle Buttons
    addAndMakeVisible(filterLowButton);
    filterLowButton.setClickingTogglesState(true);
    filterLowButton.setRadioGroupId(filterModeButtons);
    bool filterLowButtonState = *processor.treeState.getRawParameterValue("low");
    filterLowButton.setToggleState(filterLowButtonState, dontSendNotification);
    filterLowButton.onClick = [this] { updateToggleState (&filterLowButton, "Low"); };
    filterLowButton.setColour(ToggleButton::textColourId, Colour5);
    filterLowButton.setColour(ToggleButton::tickColourId, Colour5);
    filterLowButton.setColour(ToggleButton::tickDisabledColourId, Colour5);
    
    addAndMakeVisible(filterBandButton);
    filterBandButton.setClickingTogglesState(true);
    filterBandButton.setRadioGroupId(filterModeButtons);
    bool filterBandButtonState = *processor.treeState.getRawParameterValue("band");
    filterBandButton.setToggleState(filterBandButtonState, dontSendNotification);
    filterBandButton.onClick = [this] { updateToggleState (&filterBandButton, "Band"); };
    filterBandButton.setColour(ToggleButton::textColourId, Colour5);
    filterBandButton.setColour(ToggleButton::tickColourId, Colour5);
    filterBandButton.setColour(ToggleButton::tickDisabledColourId, Colour5);
    
    addAndMakeVisible(filterHighButton);
    filterHighButton.setClickingTogglesState(true);
    filterHighButton.setRadioGroupId(filterModeButtons);
    bool filterHighButtonState = *processor.treeState.getRawParameterValue("high");
    filterHighButton.setToggleState(filterHighButtonState, dontSendNotification);
    filterHighButton.onClick = [this] { updateToggleState (&filterHighButton, "High"); };
    filterHighButton.setColour(ToggleButton::textColourId, Colour5);
    filterHighButton.setColour(ToggleButton::tickColourId, Colour5);
    filterHighButton.setColour(ToggleButton::tickDisabledColourId, Colour5);
    
    // Attachment
    inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob);
    downSampleAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "downSample", downSampleKnob);
    outputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "outputGain", outputKnob);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    cutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "cutoff", cutoffKnob);
    resAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "res", resKnob);
    
    hqAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "hq", hqButton);
    linkedAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "linked", linkedButton);
    recOffAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "recOff", recOffButton);
    recHalfAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "recHalf", recHalfButton);
    recFullAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "recFull", recFullButton);
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
    distortionMode.setColour(ComboBox::textColourId, Colour1);
    distortionMode.setColour(ComboBox::arrowColourId, Colour1);
    distortionMode.setColour(ComboBox::buttonColourId, Colour1);
    distortionMode.setColour(ComboBox::outlineColourId, Colour5);
    distortionMode.setColour(ComboBox::focusedOutlineColourId, Colour1);
    distortionMode.setColour(ComboBox::backgroundColourId, Colour5);
    distortionMode.getLookAndFeel().setColour(PopupMenu::textColourId, Colour1);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, Colour2);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, Colour5);
    distortionMode.getLookAndFeel().setColour(PopupMenu::headerTextColourId, Colour1);
    distortionMode.getLookAndFeel().setColour(PopupMenu::backgroundColourId, Colour5);

    modeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);
    
    
    // about button
    addAndMakeVisible(aboutButton);
    aboutButton.setColour(TextButton::buttonColourId, Colour3);
    aboutButton.setColour(TextButton::textColourOnId, Colour3);
    aboutButton.setColour(TextButton::textColourOffId, Colour3);
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
    g.fillAll(Colour3);

    // title
    g.setColour(Colour5);
    g.fillRect(0, 0, getWidth(), 50);

    g.setColour(Colour1);
    g.fillRoundedRectangle(25, getHeight() / 2 - 50, getWidth() - 50, getHeight() / 2 + 25, 25);
    // set background
    // Image background = ImageCache::getFromMemory(BinaryData::blood_background_png, (size_t) BinaryData::blood_background_pngSize);
    // g.drawImage(background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    
    // draw version
    g.setColour(Colour5);
    g.setFont (Font ("Times New Roman", 18.0f, Font::bold));
    String version = (String) "Ver. " + (String)VERSION;
    Rectangle<int> area(getWidth() - 100, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, Justification::topLeft, 1);
    
    // set title "Fire"
    g.setFont (Font ("Times New Roman", 30.0f, Font::bold));
    Rectangle<int> titleArea(5, 5, 300, 50);
    g.drawText("BLOOD", titleArea, Justification::centred);

    
    // set title picture "Fire"
//    Image title = ImageCache::getFromMemory(BinaryData::blood_png, (size_t) BinaryData::blood_pngSize);
//    int x = getWidth() / 2 - title.getWidth() / 4;
//    int y = getHeight() / 3 + 60;
//    g.drawImage(title, x, y, title.getWidth()/2, title.getHeight()/2, 0, 0, title.getWidth(), title.getHeight());

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
    g.setColour(Colour4);
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

        // g.setColour(Colour1);
        ColourGradient grad(Colour1, frame.getX() + frame.getWidth() / 2, frame.getY() + frame.getHeight() / 2,
            Colour4, frame.getX(), frame.getY() + frame.getHeight() / 2, true);
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
    inputKnob.setBounds((getWidth() / 7) * 1 - knobSize / 2, getHeight() / 2 + 7 - 25, knobSize, knobSize);
    driveKnob.setBounds((getWidth() / 7) * 2 - knobSize / 2, getHeight() / 2 + 7 - 25, knobSize, knobSize);
    downSampleKnob.setBounds((getWidth() / 7) * 3 - knobSize / 2, getHeight() / 2 + 7 - 25, knobSize, knobSize);
    outputKnob.setBounds((getWidth() / 7) * 5 - knobSize / 2, getHeight() / 2 + 7 - 25, knobSize, knobSize);
    mixKnob.setBounds((getWidth() / 7) * 6 - knobSize / 2, getHeight() / 2 + 7 - 25, knobSize, knobSize);

    cutoffKnob.setBounds((getWidth() / 7) * 5 - knobSize / 2, getHeight() / 4 * 3, knobSize, knobSize);
    resKnob.setBounds((getWidth() / 7) * 6 - knobSize / 2, getHeight() / 4 * 3, knobSize, knobSize);
    
    // toggle buttons
    hqButton.setBounds((getWidth() / 7) * 1 - 120, getHeight() / 2 + 7 + 25 - 25, 100, 25);
    linkedButton.setBounds((getWidth() / 7) * 5 + 25, getHeight() / 2 + 7 + 25 - 25, 100, 25);
    recOffButton.setBounds((getWidth() / 7) * 4 - knobSize / 2, getHeight() / 2 + 7 - 25, 100, 25);
    recHalfButton.setBounds((getWidth() / 7) * 4 - knobSize / 2, getHeight() / 2 + 7 + 25 - 25, 100, 25);
    recFullButton.setBounds((getWidth() / 7) * 4 - knobSize / 2, getHeight() / 2 + 7 + 50 - 25, 100, 25);

    filterOffButton.setBounds((getWidth() / 7) * 1 - knobSize / 2, getHeight() / 4 * 3, 100, 25);
    filterPreButton.setBounds((getWidth() / 7) * 1 - knobSize / 2, getHeight() / 4 * 3 +25, 100, 25);
    filterPostButton.setBounds((getWidth() / 7) * 1 - knobSize / 2, getHeight() / 4 * 3 +50, 100, 25);
    filterLowButton.setBounds((getWidth() / 7) * 2 - knobSize / 2, getHeight() / 4 * 3, 100, 25);
    filterBandButton.setBounds((getWidth() / 7) * 2 - knobSize / 2, getHeight() / 4 * 3 +25, 100, 25);
    filterHighButton.setBounds((getWidth() / 7) * 2 - knobSize / 2, getHeight() / 4 * 3 +50, 100, 25);
    
    // about
    aboutButton.setBounds(getWidth() - 100, 0, 100, 50);
    
    // visualiser
    // processor.visualiser.setBounds(0, 50, getWidth() / 2, getHeight() / 2 - 125);

    // ff meter
    inputMeter.setBounds(getWidth() / 2 - 25, getHeight() - 175, 30, 150);
    outputMeter.setBounds(getWidth() / 2 + 25, getHeight() - 175, 30, 150);
    
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
