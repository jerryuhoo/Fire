/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "0.322"
//==============================================================================
BloodAudioProcessorEditor::BloodAudioProcessorEditor(BloodAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{

    setResizable(true, true);

    // ff meter
    lnf = std::make_unique<FFAU::LevelMeterLookAndFeel>();
    // adjust the colours to how you like them, e.g.
    lnf->setColour(FFAU::LevelMeter::lmTextColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmTextClipColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmTextDeactiveColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmTicksColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmOutlineColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmBackgroundColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmBackgroundClipColour, mainColour);
    lnf->setColour(FFAU::LevelMeter::lmMeterForegroundColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmMeterOutlineColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmMeterBackgroundColour, backgroundColour);
    lnf->setColour(FFAU::LevelMeter::lmMeterMaxNormalColour, juce::Colours::yellow);
    lnf->setColour(FFAU::LevelMeter::lmMeterMaxWarnColour, juce::Colours::orange);
    lnf->setColour(FFAU::LevelMeter::lmMeterMaxOverColour, juce::Colours::darkred);
    lnf->setColour(FFAU::LevelMeter::lmMeterGradientLowColour, Colour(50,0,0));
    lnf->setColour(FFAU::LevelMeter::lmMeterGradientMidColour, Colour(100,0,0));
    lnf->setColour(FFAU::LevelMeter::lmMeterGradientMaxColour, Colour(150,0,0));
    lnf->setColour(FFAU::LevelMeter::lmMeterReductionColour, juce::Colours::orange);
    inputMeter = std::make_unique<FFAU::LevelMeter>(); // See FFAU::LevelMeter::MeterFlags for options
    inputMeter->setLookAndFeel(lnf.get());
    inputMeter->setMeterSource(&processor.getInputMeterSource());
    addAndMakeVisible(*inputMeter); //暂时停用

    outputMeter = std::make_unique<FFAU::LevelMeter>(); // See FFAU::LevelMeter::MeterFlags for options
    outputMeter->setLookAndFeel(lnf.get());
    outputMeter->setMeterSource(&processor.getOutputMeterSource());
    addAndMakeVisible(*outputMeter); //暂时停用
    
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(1000, 750);

    // input knob
    addAndMakeVisible(inputKnob);
    inputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    inputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    inputKnob.setLookAndFeel(&otherLookAndFeelRed);

    addAndMakeVisible(inputLabel);
    inputLabel.setText("Input", dontSendNotification);
    inputLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
    inputLabel.setColour(Label::textColourId, mainColour);
    inputLabel.attachToComponent(&inputKnob, false);
    inputLabel.setJustificationType (Justification::centred);

    // drive knob
    addAndMakeVisible(driveKnob);
    driveKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    driveKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    driveKnob.setLookAndFeel(&otherLookAndFeelRed);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", dontSendNotification);
    driveLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
    driveLabel.setColour(Label::textColourId, mainColour);
    driveLabel.attachToComponent(&driveKnob, false);
    driveLabel.setJustificationType (Justification::centred);
    
    // output knob
    addAndMakeVisible(outputKnob);
    outputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    outputKnob.setLookAndFeel(&otherLookAndFeelRed);

    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", dontSendNotification);
    outputLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
    outputLabel.setColour(Label::textColourId, mainColour);
    outputLabel.attachToComponent(&outputKnob, false);
    outputLabel.setJustificationType (Justification::centred);

    // mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    mixKnob.setLookAndFeel(&otherLookAndFeelRed);

    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", dontSendNotification);
    mixLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
    mixLabel.setColour(Label::textColourId, mainColour);
    mixLabel.attachToComponent(&mixKnob, false);
    mixLabel.setJustificationType (Justification::centred);
    
    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    cutoffKnob.setLookAndFeel(&otherLookAndFeelRed);

    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", dontSendNotification);
    cutoffLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
    cutoffLabel.setColour(Label::textColourId, mainColour);
    cutoffLabel.attachToComponent(&cutoffKnob, false);
    cutoffLabel.setJustificationType (Justification::centred);
    
    //r es knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    resKnob.setLookAndFeel(&otherLookAndFeelRed);

    addAndMakeVisible(cutoffLabel);
    resLabel.setText("Resonance", dontSendNotification);
    resLabel.setFont (Font ("Cascadia Code", 20.0f, Font::bold));
    resLabel.setColour(Label::textColourId, mainColour);
    resLabel.attachToComponent(&resKnob, false);
    resLabel.setJustificationType (Justification::centred);
    
    // debug
    // debugLabel.setText("debug", dontSendNotification);
    // addAndMakeVisible(debugLabel);
    
    // buttons
    
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
    
    // button2
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
    
    filterOffAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "off", filterOffButton);
    filterPreAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "pre", filterPreButton);
    filterPostAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "post", filterPostButton);
    filterLowAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "low", filterLowButton);
    filterBandAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "band", filterBandButton);
    filterHighAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "high", filterHighButton);
    
    // Visualiser
    addAndMakeVisible(p.visualiser);

    //distortion mode select
    distortionMode.addItem("Arctan Soft Clipping", 1);
    distortionMode.addItem("Exp Soft Clipping", 2);
    distortionMode.addItem("Tanh Soft Clipping", 3);
    distortionMode.addItem("Cubic Soft Clipping", 4);
    distortionMode.addItem("Hard Clipping", 5);
    distortionMode.addItem("Sin Foldback", 6);
    distortionMode.addItem("Linear Foldback", 7);
    distortionMode.addItem("Downsample", 8);
    distortionMode.setJustificationType(Justification::centred);
    distortionMode.setColour(ComboBox::textColourId, mainColour);
    distortionMode.setColour(ComboBox::arrowColourId, mainColour);
    distortionMode.setColour(ComboBox::buttonColourId, mainColour);
    distortionMode.setColour(ComboBox::outlineColourId, mainColour);
    distortionMode.setColour(ComboBox::focusedOutlineColourId, mainColour);
    distortionMode.setColour(ComboBox::backgroundColourId, secondColour);
    distortionMode.getLookAndFeel().setColour(PopupMenu::textColourId, mainColour);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, Colours::darkred);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, Colours::white);
    distortionMode.getLookAndFeel().setColour(PopupMenu::headerTextColourId, mainColour);
    distortionMode.getLookAndFeel().setColour(PopupMenu::backgroundColourId, secondColour);

    modeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);
    addAndMakeVisible(distortionMode);

    setResizeLimits(1000, 750, 1600, 1200); // set resize limits
    getConstrainer ()->setFixedAspectRatio (1.33); // set fixed resize rate: 700/525
}

BloodAudioProcessorEditor::~BloodAudioProcessorEditor()
{
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
    g.setColour(mainColour);
    
    // set background
    Image background = ImageCache::getFromMemory(BinaryData::blood_background_png, (size_t) BinaryData::blood_background_pngSize);
    g.drawImage(background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    
    g.setFont(15.0f);
    String version = (String) "Wings - Blood(close beta test) - Version " + (String)VERSION;
    g.drawFittedText(version, getLocalBounds(), Justification::centredBottom, 1);
    
    // set title "Blood"
    // g.setFont(50.0f);
    // g.setFont (Font ("华文宋体", 50.0f, Font::bold));
    // Rectangle<int> area(getWidth() / 2, (getHeight() / 3) + 100, getWidth() / 2 - 50 + 2, (getHeight() / 3 + 2));
    // g.drawText("blood", area, Justification::centredTop);
    
    // set title picture "Blood"
    Image title = ImageCache::getFromMemory(BinaryData::blood_png, (size_t) BinaryData::blood_pngSize);
    int x = getWidth() / 4 * 3 - title.getWidth() / 4;
    int y = getHeight() / 3 + 52;
    g.drawImage(title, x, y, title.getWidth()/2, title.getHeight()/2, 0, 0, title.getWidth(), title.getHeight());
    
    
    
    // wave visualiser boundary
    g.setColour(mainColour);
    g.drawRect(50 - 2, 50 - 2, getWidth() / 2 - 50 + 2, (getHeight() / 3) + 4, 2);
    
    // fill right rect background
    g.setColour(secondColour);
    g.fillRect(getWidth() / 2 + 2, 50, getWidth() / 2 - 50, (getHeight() / 3));
    
    // mode visualiser boundary
    // g.drawRect(getWidth()/2, 50-1, getWidth()/2-50+2, (getHeight() / 3+2));

    // paint distortion function
    float functionValue;
    float mixValue;
    int mode = *processor.treeState.getRawParameterValue("mode");
    float inputGain = *processor.treeState.getRawParameterValue("inputGain");
    float drive = *processor.treeState.getRawParameterValue("drive"); // (deleted drive)
    float mix = *processor.treeState.getRawParameterValue("mix");
    
    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.mix = mix;

    auto frame = getLocalBounds(); // adjust here, if you want to paint in a special location
    frame.setBounds(getWidth() / 2, 50 - 1, getWidth() / 2 - 50 + 2, (getHeight() / 3 + 2));
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

    bool firstPoint = true;

    float yPos;
    for (int i = 1; i < numPix; ++i)
    {
        value += valInc;
        xPos += posInc;
        
        functionValue = distortionProcessor.distortionProcess(value);
        
        if (mode == 8) {
            float rateDivide = distortionProcessor.controls.drive;
            if (rateDivide > 1)
                functionValue = ceilf(value*(64.f/rateDivide))/(64.f/rateDivide);
            functionValue = (1.f - mix) * value + mix * functionValue;
        }
        
        mixValue = (2.0f / 3.0f) * functionValue;
        yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;

        if (yPos < frame.getY())
        {
            //continue;
            yPos = frame.getY();
        }
        if (yPos > frame.getBottom())
        {
            //continue;
            yPos = frame.getBottom();
        }
        if (firstPoint == true)
        {
            //mixValue = ((1 - mix) * value) + (mix * functionValue);
            //yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;
            p.startNewSubPath(xPos, yPos);
            firstPoint = false;
        }
        p.lineTo(xPos, yPos);
    }
    
    
    // draw right rect
    g.setColour(mainColour);
    g.drawRect(getWidth() / 2, 50 - 2, getWidth() / 2 - 50 + 2, (getHeight() / 3+4), 2);
    
    g.setColour(mainColour);
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
    inputKnob.setBounds((getWidth() / 5) * 1 - 50, getHeight()/2+50, 100, 100);
    driveKnob.setBounds((getWidth() / 5) * 2 - 50, getHeight()/2+50, 100, 100);
    outputKnob.setBounds((getWidth() / 5) * 3 - 50, getHeight()/2+50, 100, 100);
    mixKnob.setBounds((getWidth() / 5) * 4 - 50, getHeight()/2+50, 100, 100);
    cutoffKnob.setBounds((getWidth() / 5) * 3 - 50, getHeight()/2+200, 100, 100);
    resKnob.setBounds((getWidth() / 5) * 4 - 50, getHeight()/2+200, 100, 100);
    
    // debug
    debugLabel.setBounds(0, 0, 300, 300);
    
    // buttons
    
    filterOffButton.setBounds((getWidth() / 5) * 1 - 25, getHeight()/2+200, 100, 25);
    filterPreButton.setBounds((getWidth() / 5) * 1 - 25, getHeight()/2+200+25, 100, 25);
    filterPostButton.setBounds((getWidth() / 5) * 1 - 25, getHeight()/2+200+50, 100, 25);
    filterLowButton.setBounds((getWidth() / 5) * 2 - 50, getHeight()/2+200, 100, 25);
    filterBandButton.setBounds((getWidth() / 5) * 2 - 50, getHeight()/2+200+25, 100, 25);
    filterHighButton.setBounds((getWidth() / 5) * 2 - 50, getHeight()/2+200+50, 100, 25);
    
    // visualiser
    processor.visualiser.setBounds(50, 50, getWidth() / 2 - 50, (getHeight() / 3));

    // ff meter
    // original
//    inputMeter->setBounds(15, 0, 40, getHeight() + 50);
//    outputMeter->setBounds(getWidth() - 50, 0, 40, getHeight() + 50);
    inputMeter->setBounds(getWidth()/10*9-30, getHeight()-220, 50, 240);
    outputMeter->setBounds(getWidth()/10*9+10, getHeight()-220, 50, 240);
    
    // distortion menu
    distortionMode.setBounds(50 - 2, (getHeight() / 3) + 52, getWidth() / 2 - 50 +4, 54);
    
}

void BloodAudioProcessorEditor::updateToggleState (Button* button, String name)
{
    //auto state = button->getToggleState();
}
