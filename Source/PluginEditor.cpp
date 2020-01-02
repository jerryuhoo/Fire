/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BloodAudioProcessorEditor::BloodAudioProcessorEditor (BloodAudioProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
    
    setResizable (true, true);
    
    // ff meter
    lnf = std::make_unique<FFAU::LevelMeterLookAndFeel>();
    // adjust the colours to how you like them, e.g.
    lnf->setColour (FFAU::LevelMeter::lmTextColour,             backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmTextClipColour,         backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmTextDeactiveColour,     backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmTicksColour,            backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmOutlineColour,          backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmBackgroundColour,       backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmBackgroundClipColour,   mainColour);
    lnf->setColour (FFAU::LevelMeter::lmMeterForegroundColour,  backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmMeterOutlineColour,     backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmMeterBackgroundColour,  backgroundColour);
    lnf->setColour (FFAU::LevelMeter::lmMeterMaxNormalColour,   juce::Colours::yellow);
    lnf->setColour (FFAU::LevelMeter::lmMeterMaxWarnColour,     juce::Colours::orange);
    lnf->setColour (FFAU::LevelMeter::lmMeterMaxOverColour,     juce::Colours::darkred);
    lnf->setColour (FFAU::LevelMeter::lmMeterGradientLowColour, secondColour);
    lnf->setColour (FFAU::LevelMeter::lmMeterGradientMidColour, mainColour);
    lnf->setColour (FFAU::LevelMeter::lmMeterGradientMaxColour, juce::Colours::deeppink);
    lnf->setColour (FFAU::LevelMeter::lmMeterReductionColour,   juce::Colours::orange);
    inputMeter = std::make_unique<FFAU::LevelMeter>(); // See FFAU::LevelMeter::MeterFlags for options
    inputMeter->setLookAndFeel(lnf.get());
    inputMeter->setMeterSource (&processor.getInputMeterSource());
    addAndMakeVisible(*inputMeter);
    
    outputMeter = std::make_unique<FFAU::LevelMeter>(); // See FFAU::LevelMeter::MeterFlags for options
    outputMeter->setLookAndFeel(lnf.get());
    outputMeter->setMeterSource (&processor.getOutputMeterSource());
    addAndMakeVisible(*outputMeter);
    
    
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (800, 600);
    
    //input knob
    addAndMakeVisible(inputKnob);
    inputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    // inputKnob.setRange(-36, 36, 0.1);
    inputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    inputKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (inputLabel);
    inputLabel.setText ("Drive", dontSendNotification);
    inputLabel.setColour(Label::textColourId, mainColour);
    inputLabel.attachToComponent (&inputKnob, false);
    
    
    //drive knob (deleted)
//    addAndMakeVisible(driveKnob);
//    driveKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
//    // driveKnob.setRange(1, 16, 0.01);
//    driveKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
//    driveKnob.setLookAndFeel(&otherLookAndFeel);
//
//    addAndMakeVisible (driveLabel);
//    driveLabel.setText ("drive", dontSendNotification);
//    driveLabel.setColour(Label::textColourId, mainColour);
//    driveLabel.attachToComponent (&driveKnob, false);
    
    
    
    //mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    // mixKnob.setRange(0, 1, 0.01);
    // mixKnob.setValue(100);
    mixKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    mixKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (mixLabel);
    mixLabel.setText ("Mix", dontSendNotification);
    mixLabel.setColour(Label::textColourId, mainColour);
    mixLabel.attachToComponent (&mixKnob, false);
    
    //volume knob
    addAndMakeVisible(gainKnob);
    gainKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    // gainKnob.setRange(-48, 6, 0.1);
    // gainKnob.setValue(0);
    gainKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    gainKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (gainLabel);
    gainLabel.setText ("Output volume", dontSendNotification);
    gainLabel.setColour(Label::textColourId, mainColour);
    gainLabel.attachToComponent (&gainKnob, false);
    
    // driveAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob); // (deleted drive)
    inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "outputGain", gainKnob);
    
    
    //visualiser
    //addAndMakeVisible(visualiser);
    addAndMakeVisible(p.visualiser);
    
    //distortion mode select
    
    distortionMode.addItem("Arctan Soft Clipping", 1);
    distortionMode.addItem("Exp Soft Clipping", 2);
    distortionMode.addItem("Tanh Soft Clipping", 3);
    distortionMode.addItem("Cubic Soft Clipping", 4);
    distortionMode.addItem("Hard Clipping", 5);
    distortionMode.addItem("Square Wave Clipping", 6);
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

    modeSelection = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);
    addAndMakeVisible(distortionMode);
    
    setResizeLimits (700, 525, 4000, 3000);
}

BloodAudioProcessorEditor::~BloodAudioProcessorEditor()
{
}

float f(float x)
{
    return 0.5;
}

//==============================================================================
void BloodAudioProcessorEditor::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    //g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillAll (backgroundColour);
    g.setColour (mainColour);
    g.setFont (15.0f);
    g.drawFittedText ("Wings - Blood - ver 0.26 -----", getLocalBounds(), Justification::bottomRight, 1);
    g.setFont (50.0f);
    Rectangle<int> area (getWidth()/2, (getHeight() / 3)+100, getWidth()/2-50+2, (getHeight() / 3+2));
    g.drawText ("BLOOD", area, Justification::centredTop);
    // wave visualiser boundary
    g.drawRect(50-1, 50-1, getWidth()/2-50+2, (getHeight() / 3)+2);
    // mode visualiser boundary
    // g.drawRect(getWidth()/2, 50-1, getWidth()/2-50+2, (getHeight() / 3+2));
    
    
    // paint distortion function
    
    float functionValue;
    float mixValue;
    float mode = *processor.treeState.getRawParameterValue("mode");
    float inputGain = *processor.treeState.getRawParameterValue("inputGain");
    float mix = *processor.treeState.getRawParameterValue("mix");
    // float drive = *processor.treeState.getRawParameterValue("drive"); // (deleted drive)
    
    
    auto frame = getLocalBounds();        // adjust here, if you want to paint in a special location
    frame.setBounds(getWidth()/2, 50-1, getWidth()/2-50+2, (getHeight() / 3+2));
    const int numPix = frame.getWidth();  // you might experiment here, if you want less steps to speed up
    
    // float maxValue = 2.0f * drive * mix + 2.0f * (1 - mix);                  // maximum (rightmost) value for your graph
    
    float driveScale = 1;
    if (inputGain >= 0) {
        driveScale = pow(2, inputGain/6.0f);
    }
    float maxValue = 2.0f * driveScale * mix + 2.0f * (1 - mix);
    float value  = -maxValue;               // minimum (leftmost)  value for your graph
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
        
        if (mode == 1.0f) {
            functionValue = processor.arctanSoftClipping(value, 1.0f);
        }
        else if (mode == 2.0f) {
            functionValue = processor.expSoftClipping(value, 1.0f);
        }
        else if (mode == 3.0f) {
            functionValue = processor.tanhSoftClipping(value, 1.0f);
        }
        else if (mode == 4.0f) {
            functionValue = processor.cubicSoftClipping(value, 1.0f);
        }
        else if (mode == 5.0f) {
            functionValue = processor.hardClipping(value, 1.0f);
        }
        else if (mode == 6.0f) {
            functionValue = processor.squareWaveClipping(value, 1.0f);
        }
        else {
            functionValue = 1.0f;
        }
        mixValue = (((1 - mix) * value) + (mix * functionValue)) * (2.0f/3.0f); // 2/3 to make the graph looks better
        yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;
        
        if (yPos < frame.getY())
        {
            continue;
        }
        if (yPos > frame.getBottom())
        {
            continue;
        }
        if (firstPoint == true)
        {
            //mixValue = ((1 - mix) * value) + (mix * functionValue);
            //yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;
            p.startNewSubPath (xPos, yPos);
            firstPoint = false;
        }
        p.lineTo (xPos, yPos);  // replace f(x) with your function
    }
    g.setColour (mainColour);
    g.drawRect (frame, 1);
    g.setColour (secondColour);
    g.fillRect(getWidth()/2+1, 50, getWidth()/2-50, (getHeight() / 3));
    g.setColour (mainColour);
    g.strokePath (p, PathStrokeType (3.0));
    repaint();
}

void BloodAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    inputKnob.setBounds((getWidth()/4)*1-50, getHeight()-150, 100, 100);
    // driveKnob.setBounds((getWidth()/5)*2-50, getHeight()-150, 100, 100); // (deleted drive)
    mixKnob.setBounds((getWidth()/4)*2-50, getHeight()-150, 100, 100);
    gainKnob.setBounds((getWidth()/4)*3-50, getHeight()-150, 100, 100);
    
    // visualiser
    processor.visualiser.setBounds(50, 50, getWidth()/2-50, (getHeight() / 3));
    
    // ff meter
    inputMeter->setBounds(0, 0, 40, getHeight()+50);
    outputMeter->setBounds(getWidth()-40, 0, 40, getHeight()+50);
    
    // distortion menu
    distortionMode.setBounds(50, (getHeight() / 3)+100, getWidth()/2-50, 50);
}


//float getFunctionValue(BloodAudioProcessor& processor, float value)
//{
//    float functionValue;
//
//    if (*processor.treeState.getRawParameterValue("mode") == 1.0f) {
//        functionValue = processor.hardClipping(value, 1.0f);
//    }
//    else if (*processor.treeState.getRawParameterValue("mode") == 2.0f) {
//        functionValue = processor.expSoftClipping(value, 1.0f);
//    }
//    else if (*processor.treeState.getRawParameterValue("mode") == 3.0f) {
//        functionValue = processor.halfWaveClipping(value, 1.0f);
//    }
//    else if (*processor.treeState.getRawParameterValue("mode") == 4.0f) {
//        functionValue = processor.cubicSoftClipping(value, 1.0f);
//    }
//    else if (*processor.treeState.getRawParameterValue("mode") == 5.0f) {
//        functionValue = processor.tanhSoftClipping(value, 1.0f);
//    }
//    else if (*processor.treeState.getRawParameterValue("mode") == 6.0f) {
//        functionValue = processor.arctanSoftClipping(value, 1.0f);
//    }
//    else {
//        functionValue = 1.0f;
//    }
//    return functionValue;
//}
