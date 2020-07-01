/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "0.60"
//==============================================================================
BloodAudioProcessorEditor::BloodAudioProcessorEditor(BloodAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    // resize not avaialble
    //setResizable(true, true);

    
    // ff meter
    //lnf = std::make_unique<foleys::LevelMeterLookAndFeel>();
    // adjust the colours to how you like them, e.g.
    lnf.setColour(foleys::LevelMeter::lmTextColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmTextClipColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmTextDeactiveColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmTicksColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmOutlineColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmBackgroundColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmBackgroundClipColour, mainColour);
    lnf.setColour(foleys::LevelMeter::lmMeterForegroundColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmMeterOutlineColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmMeterBackgroundColour, backgroundColour);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxNormalColour, juce::Colours::yellow);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxWarnColour, juce::Colours::orange);
    lnf.setColour(foleys::LevelMeter::lmMeterMaxOverColour, juce::Colours::darkred);
    lnf.setColour(foleys::LevelMeter::lmMeterGradientLowColour, Colour(110,0,0));
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMidColour, Colour(130,0,0));
    lnf.setColour(foleys::LevelMeter::lmMeterGradientMaxColour, Colour(150,0,0));
    lnf.setColour(foleys::LevelMeter::lmMeterReductionColour, juce::Colours::orange);
    
    inputMeter.setLookAndFeel(&lnf);
    inputMeter.setMeterSource(&processor.getInputMeterSource());
    addAndMakeVisible(inputMeter); //暂时停用

    outputMeter.setLookAndFeel(&lnf);
    outputMeter.setMeterSource(&processor.getOutputMeterSource());
    addAndMakeVisible(outputMeter); //暂时停用
    


    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(1000, 750);

    setLookAndFeel(&otherLookAndFeelRed);

    // input knob
    addAndMakeVisible(inputKnob);
    inputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    inputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

//     addAndMakeVisible(inputLabel);
//    inputLabel.setText("Input", dontSendNotification);
//    inputLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
//    inputLabel.setColour(Label::textColourId, mainColour);
//    inputLabel.attachToComponent(&inputKnob, false);
//    inputLabel.setJustificationType (Justification::centred);

    // drive knob
    addAndMakeVisible(driveKnob);
    driveKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    driveKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    driveKnob.addListener(this);

//    addAndMakeVisible(driveLabel);
//    driveLabel.setText("Drive", dontSendNotification);
//    driveLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
//    driveLabel.setColour(Label::textColourId, mainColour);
//    driveLabel.attachToComponent(&driveKnob, false);
//    driveLabel.setJustificationType (Justification::centred);
    
    // output knob
    addAndMakeVisible(outputKnob);
    outputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    outputKnob.addListener(this);
//    addAndMakeVisible(outputLabel);
//    outputLabel.setText("Output", dontSendNotification);
//    outputLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
//    outputLabel.setColour(Label::textColourId, mainColour);
//    outputLabel.attachToComponent(&outputKnob, false);
//    outputLabel.setJustificationType (Justification::centred);

    // mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

//    addAndMakeVisible(mixLabel);
//    mixLabel.setText("Mix", dontSendNotification);
//    mixLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
//    mixLabel.setColour(Label::textColourId, mainColour);
//    mixLabel.attachToComponent(&mixKnob, false);
//    mixLabel.setJustificationType (Justification::centred);
    
    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    
//    addAndMakeVisible(cutoffLabel);
//    cutoffLabel.setText("Cutoff", dontSendNotification);
//    cutoffLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
//    cutoffLabel.setColour(Label::textColourId, mainColour);
//    cutoffLabel.attachToComponent(&cutoffKnob, false);
//    cutoffLabel.setJustificationType (Justification::centred);
    
    // res knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

//    addAndMakeVisible(resLabel);
//    resLabel.setText("Resonance", dontSendNotification);
//    resLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
//    resLabel.setColour(Label::textColourId, mainColour);
//    resLabel.attachToComponent(&resKnob, false);
//    resLabel.setJustificationType (Justification::centred);
    
    // Linked Toggle Buttons
    addAndMakeVisible(linkedButton);
    linkedButton.setClickingTogglesState(true);
    bool linkedButtonState = *processor.treeState.getRawParameterValue("linked");
    linkedButton.setToggleState(linkedButtonState, dontSendNotification);
    linkedButton.onClick = [this] { updateToggleState (&linkedButton, "Linked"); };
    linkedButton.setColour(ToggleButton::textColourId, mainColour);
    linkedButton.setColour(ToggleButton::tickColourId, mainColour);
    linkedButton.setColour(ToggleButton::tickDisabledColourId, mainColour);
    
    // Rectification Toggle Buttons
    addAndMakeVisible(recOffButton);
    recOffButton.setClickingTogglesState(true);
    recOffButton.setRadioGroupId(recStateButtons);
    bool refOffButtonState = *processor.treeState.getRawParameterValue("recOff");
    recOffButton.setToggleState(refOffButtonState, dontSendNotification);
    recOffButton.onClick = [this] { updateToggleState(&recOffButton, "RecOff"); };
    recOffButton.setColour(ToggleButton::textColourId, mainColour);
    recOffButton.setColour(ToggleButton::tickColourId, mainColour);
    recOffButton.setColour(ToggleButton::tickDisabledColourId, mainColour);

    addAndMakeVisible(recHalfButton);
    recHalfButton.setClickingTogglesState(true);
    recHalfButton.setRadioGroupId(recStateButtons);
    bool refHalfButtonState = *processor.treeState.getRawParameterValue("recHalf");
    recHalfButton.setToggleState(refHalfButtonState, dontSendNotification);
    recHalfButton.onClick = [this] { updateToggleState(&recHalfButton, "RecHalf"); };
    recHalfButton.setColour(ToggleButton::textColourId, mainColour);
    recHalfButton.setColour(ToggleButton::tickColourId, mainColour);
    recHalfButton.setColour(ToggleButton::tickDisabledColourId, mainColour);

    addAndMakeVisible(recFullButton);
    recFullButton.setClickingTogglesState(true);
    recFullButton.setRadioGroupId(recStateButtons);
    bool recFullButtonState = *processor.treeState.getRawParameterValue("recFull");
    recFullButton.setToggleState(recFullButtonState, dontSendNotification);
    recFullButton.onClick = [this] { updateToggleState(&recFullButton, "RecFull"); };
    recFullButton.setColour(ToggleButton::textColourId, mainColour);
    recFullButton.setColour(ToggleButton::tickColourId, mainColour);
    recFullButton.setColour(ToggleButton::tickDisabledColourId, mainColour);

    // Filter State Toggle Buttons
    addAndMakeVisible(filterOffButton);
    filterOffButton.setClickingTogglesState(true);
    filterOffButton.setRadioGroupId(filterStateButtons);
    bool filterOffButtonState = *processor.treeState.getRawParameterValue("off");
    filterOffButton.setToggleState(filterOffButtonState, dontSendNotification);
    filterOffButton.onClick = [this] { updateToggleState (&filterOffButton, "Off"); };
    filterOffButton.setColour(ToggleButton::textColourId, mainColour);
    filterOffButton.setColour(ToggleButton::tickColourId, mainColour);
    filterOffButton.setColour(ToggleButton::tickDisabledColourId, mainColour);
    
    addAndMakeVisible(filterPreButton);
    filterPreButton.setClickingTogglesState(true);
    filterPreButton.setRadioGroupId(filterStateButtons);
    bool filterPreButtonState = *processor.treeState.getRawParameterValue("pre");
    filterPreButton.setToggleState(filterPreButtonState, dontSendNotification);
    filterPreButton.onClick = [this] { updateToggleState (&filterPreButton, "Pre"); };
    filterPreButton.setColour(ToggleButton::textColourId, mainColour);
    filterPreButton.setColour(ToggleButton::tickColourId, mainColour);
    filterPreButton.setColour(ToggleButton::tickDisabledColourId, mainColour);
    
    addAndMakeVisible(filterPostButton);
    filterPostButton.setClickingTogglesState(true);
    filterPostButton.setRadioGroupId(filterStateButtons);
    bool filterPostButtonState = *processor.treeState.getRawParameterValue("post");
    filterPostButton.setToggleState(filterPostButtonState, dontSendNotification);
    filterPostButton.onClick = [this] { updateToggleState (&filterPostButton, "Post"); };
    filterPostButton.setColour(ToggleButton::textColourId, mainColour);
    filterPostButton.setColour(ToggleButton::tickColourId, mainColour);
    filterPostButton.setColour(ToggleButton::tickDisabledColourId, mainColour);
    
    // Filter Type Toggle Buttons
    addAndMakeVisible(filterLowButton);
    filterLowButton.setClickingTogglesState(true);
    filterLowButton.setRadioGroupId(filterModeButtons);
    bool filterLowButtonState = *processor.treeState.getRawParameterValue("low");
    filterLowButton.setToggleState(filterLowButtonState, dontSendNotification);
    filterLowButton.onClick = [this] { updateToggleState (&filterLowButton, "Low"); };
    filterLowButton.setColour(ToggleButton::textColourId, mainColour);
    filterLowButton.setColour(ToggleButton::tickColourId, mainColour);
    filterLowButton.setColour(ToggleButton::tickDisabledColourId, mainColour);
    
    addAndMakeVisible(filterBandButton);
    filterBandButton.setClickingTogglesState(true);
    filterBandButton.setRadioGroupId(filterModeButtons);
    bool filterBandButtonState = *processor.treeState.getRawParameterValue("band");
    filterBandButton.setToggleState(filterBandButtonState, dontSendNotification);
    filterBandButton.onClick = [this] { updateToggleState (&filterBandButton, "Band"); };
    filterBandButton.setColour(ToggleButton::textColourId, mainColour);
    filterBandButton.setColour(ToggleButton::tickColourId, mainColour);
    filterBandButton.setColour(ToggleButton::tickDisabledColourId, mainColour);
    
    addAndMakeVisible(filterHighButton);
    filterHighButton.setClickingTogglesState(true);
    filterHighButton.setRadioGroupId(filterModeButtons);
    bool filterHighButtonState = *processor.treeState.getRawParameterValue("high");
    filterHighButton.setToggleState(filterHighButtonState, dontSendNotification);
    filterHighButton.onClick = [this] { updateToggleState (&filterHighButton, "High"); };
    filterHighButton.setColour(ToggleButton::textColourId, mainColour);
    filterHighButton.setColour(ToggleButton::tickColourId, mainColour);
    filterHighButton.setColour(ToggleButton::tickDisabledColourId, mainColour);
    
    // Attachment
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob);
    inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    outputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "outputGain", outputKnob);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    cutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "cutoff", cutoffKnob);
    resAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "res", resKnob);
    
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
    addAndMakeVisible(p.visualiser);

    // Distortion mode select
    addAndMakeVisible(distortionMode);

    distortionMode.addSectionHeading("Soft Clipping");
    distortionMode.addItem("Arctan Soft Clipping", 1);
    distortionMode.addItem("Exp Soft Clipping", 2);
    distortionMode.addItem("Tanh Soft Clipping", 3);
    distortionMode.addItem("Cubic Soft Clipping", 4);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Hard Clipping");
    distortionMode.addItem("Hard Clipping", 5);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Foldback");
    distortionMode.addItem("Sin Foldback", 6);
    distortionMode.addItem("Linear Foldback", 7);
    distortionMode.addSeparator();
    /*
    distortionMode.addSectionHeading("Rectification");
    distortionMode.addItem("Half Rectification", 8);
    distortionMode.addItem("Full Rectification", 9);
    distortionMode.addSeparator();
    */
    distortionMode.addSectionHeading("Downsample");
    distortionMode.addItem("Downsample", 8);
    distortionMode.addSeparator();

    // distortionMode.setSelectedId (1);
    distortionMode.setJustificationType(Justification::centred);
    distortionMode.setColour(ComboBox::textColourId, mainColour);
    distortionMode.setColour(ComboBox::arrowColourId, mainColour);
    distortionMode.setColour(ComboBox::buttonColourId, mainColour);
    distortionMode.setColour(ComboBox::outlineColourId, secondColour);
    distortionMode.setColour(ComboBox::focusedOutlineColourId, mainColour);
    distortionMode.setColour(ComboBox::backgroundColourId, secondColour);
    distortionMode.getLookAndFeel().setColour(PopupMenu::textColourId, mainColour);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, Colours::darkred);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, Colours::white);
    distortionMode.getLookAndFeel().setColour(PopupMenu::headerTextColourId, mainColour);
    distortionMode.getLookAndFeel().setColour(PopupMenu::backgroundColourId, secondColour);

    modeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);
    
    
    // about button
    //addAndMakeVisible(aboutButton);
    aboutButton.setColour(TextButton::buttonColourId, Colour(50, 0, 0));
    aboutButton.setColour(TextButton::textColourOnId, Colour(100, 0, 0));
    aboutButton.setColour(TextButton::textColourOffId, Colour(100, 0, 0));
//    aboutButton.onClick = [this] { aboutDialog.showMessageBoxAsync(AlertWindow::InfoIcon, "Made by Wings", "And fuck you", "OK"); };
    // resize not available now
    //setResizeLimits(1000, 750, 1500, 1125); // set resize limits
    //getConstrainer ()->setFixedAspectRatio (1.33); // set fixed resize rate: 700/525
}

BloodAudioProcessorEditor::~BloodAudioProcessorEditor()
{
    inputMeter.setLookAndFeel(nullptr);
    outputMeter.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
}

float f(float x)
{
    return 0.5;
}

//==============================================================================
void BloodAudioProcessorEditor::paint(Graphics &g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    //g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    //g.fillAll(backgroundColour);
    //g.setColour(mainColour);
    
    // set background
    Image background = ImageCache::getFromMemory(BinaryData::blood_background_png, (size_t) BinaryData::blood_background_pngSize);
    g.drawImage(background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    
    // draw version
    g.setColour(Colour(100, 0, 0));
    g.setFont (Font ("Times New Roman", 18.0f, Font::bold));
    String version = (String) "Ver. " + (String)VERSION;
    Rectangle<int> area(900, 720, 100, 50);
    g.drawFittedText(version, area, Justification::topLeft, 1);
    
    // set title "Blood"
    // g.setFont(50.0f);
    // g.setFont (Font ("华文宋体", 50.0f, Font::bold));
    // Rectangle<int> area(getWidth() / 2, (getHeight() / 3) + 100, getWidth() / 2 - 50 + 2, (getHeight() / 3 + 2));
    // g.drawText("blood", area, Justification::centredTop);
    
    // set title picture "Blood"
//    Image title = ImageCache::getFromMemory(BinaryData::blood_png, (size_t) BinaryData::blood_pngSize);
//    int x = getWidth() / 2 - title.getWidth() / 4;
//    int y = getHeight() / 3 + 60;
//    g.drawImage(title, x, y, title.getWidth()/2, title.getHeight()/2, 0, 0, title.getWidth(), title.getHeight());
    
    
    
    // wave visualiser boundary
//    g.setColour(mainColour);
//    g.drawRect(50 - 2, 50 - 2, getWidth() / 2 - 50 + 2, (getHeight() / 3) + 4, 2);
    
    // fill right rect background
//    g.setColour(secondColour);
//    g.fillRect(getWidth() / 2 + 2, 50, getWidth() / 2 - 50, (getHeight() / 3));
    
    // mode visualiser boundary
    // g.drawRect(getWidth()/2, 50-1, getWidth()/2-50+2, (getHeight() / 3+2));

    // paint distortion function
    float functionValue;
    float mixValue;
    // int mode = *processor.treeState.getRawParameterValue("mode"); // for JUCE 5.4.7
    int mode = *processor.treeState.getRawParameterValue("mode") + 1; // for JUCE 6
    float inputGain = *processor.treeState.getRawParameterValue("inputGain");
    float drive = *processor.treeState.getRawParameterValue("drive"); // (deleted drive)
    float mix = *processor.treeState.getRawParameterValue("mix");

    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.mix = mix;

    auto frame = getLocalBounds(); // adjust here, if you want to paint in a special location
    // frame.setBounds(getWidth() / 2, 50 - 1, getWidth() / 2 - 50 + 2, (getHeight() / 3 + 2)); // this is old
    frame.setBounds(getWidth() / 2 + 10, 80, getWidth() / 2 - 50 + 2, getHeight() / 3 - 4);
    const int numPix = frame.getWidth(); // you might experiment here, if you want less steps to speed up

    // float maxValue = 2.0f * drive * mix + 2.0f * (1 - mix);                  // maximum (rightmost) value for your graph

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

    //bool firstPoint = true;
    bool edgePointL = false;
    bool edgePointR = false;
    float yPos;
    for (int i = 1; i < numPix; ++i)
    {
        value += valInc;
        xPos += posInc;
        
        functionValue = distortionProcessor.distortionProcess(value);
        
        // downsample
        if (mode == 8) 
        {
            float rateDivide = distortionProcessor.controls.drive;
            if (rateDivide > 1)
                functionValue = ceilf(value*(64.f/rateDivide))/(64.f/rateDivide);
        }
        
        functionValue = (1.f - mix) * value + mix * functionValue;

        mixValue = (2.0f / 3.0f) * functionValue;
        yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;

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
            if (mode == 8)
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
//        if (firstPoint == true)
//        {
//            //mixValue = ((1 - mix) * value) + (mix * functionValue);
//            //yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;
//            p.startNewSubPath(xPos, yPos);
//            firstPoint = false;
//            edgePointL = true;
//        }
        p.lineTo(xPos, yPos);
    }
    
    
    // draw right rect
//    g.setColour(mainColour);
//    g.drawRect(getWidth() / 2, 50 - 2, getWidth() / 2 - 50 + 2, (getHeight() / 3+4), 2);
    
    g.setColour(Colour(150,0,0));
    g.strokePath(p, PathStrokeType(2.0));
    
    // draw combobox outside rect
    // g.setColour(mainColour);
    // g.drawRect(50-2, (getHeight() / 3) + 50-2, getWidth() / 2 - 50+4, 50+4, 2);
    
    repaint();
}

void BloodAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    // knobs
    inputKnob.setBounds((getWidth() / 5) * 1 - 85, getHeight()/2+7, 100, 100);
    driveKnob.setBounds((getWidth() / 5) * 2 - 85, getHeight()/2+7, 100, 100);
    outputKnob.setBounds((getWidth() / 5) * 3 - 85, getHeight()/2+7, 100, 100);
    mixKnob.setBounds((getWidth() / 5) * 4 - 85, getHeight()/2+7, 100, 100);
    cutoffKnob.setBounds((getWidth() / 5) * 3 - 85, getHeight()/2+175, 100, 100);
    resKnob.setBounds((getWidth() / 5) * 4 - 85, getHeight()/2+175, 100, 100);
    
    // debug
    debugLabel.setBounds(0, 0, 300, 300);
    
    // toggle buttons
    linkedButton.setBounds((getWidth() / 5) * 2 + 50, getHeight()/2+7+25, 100, 25);
    recOffButton.setBounds((getWidth() / 5) * 3, getHeight() / 2 + 7, 100, 25);
    recHalfButton.setBounds((getWidth() / 5) * 3, getHeight() / 2 + 7 + 25, 100, 25);
    recFullButton.setBounds((getWidth() / 5) * 3, getHeight() / 2 + 7 + 50, 100, 25);
    filterOffButton.setBounds((getWidth() / 5) * 1 - 60, getHeight()/2+175, 100, 25);
    filterPreButton.setBounds((getWidth() / 5) * 1 - 60, getHeight()/2+175+25, 100, 25);
    filterPostButton.setBounds((getWidth() / 5) * 1 - 60, getHeight()/2+175+50, 100, 25);
    filterLowButton.setBounds((getWidth() / 5) * 2 - 85, getHeight()/2+175, 100, 25);
    filterBandButton.setBounds((getWidth() / 5) * 2 - 85, getHeight()/2+175+25, 100, 25);
    filterHighButton.setBounds((getWidth() / 5) * 2 - 85, getHeight()/2+175+50, 100, 25);
    
    // about
    aboutButton.setBounds(getWidth() - 130, getHeight()-50, 100, 30);
    
    // visualiser
    processor.visualiser.setBounds(55, 95, getWidth() / 2 - 80 + 2, getHeight() / 3 - 28);
    //processor.visualiser.setBounds(38, 90, getWidth() / 2 - 50 + 2, getHeight() / 3 - 14); // old
    
    // ff meter
    inputMeter.setBounds(getWidth()/10*9-28, getHeight()-405, 30, 320);
    outputMeter.setBounds(getWidth()/10*9+21, getHeight()-405, 30, 320);
    
    // distortion menu
    distortionMode.setBounds(37, 25, getWidth() / 4 - 50 +4, 38);
    
}

void BloodAudioProcessorEditor::updateToggleState (Button* button, String name)
{
    //auto state = button->getToggleState();
}
