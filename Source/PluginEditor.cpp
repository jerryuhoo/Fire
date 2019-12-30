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
    
    
    
    // ff meter
    lnf = std::make_unique<FFAU::LevelMeterLookAndFeel>();
    // adjust the colours to how you like them, e.g.
    lnf->setColour (FFAU::LevelMeter::lmTextColour,             juce::Colours::red);
    lnf->setColour (FFAU::LevelMeter::lmTextClipColour,         juce::Colours::red);
    lnf->setColour (FFAU::LevelMeter::lmTextDeactiveColour,     juce::Colours::black);
    lnf->setColour (FFAU::LevelMeter::lmTicksColour,            juce::Colours::red);
    lnf->setColour (FFAU::LevelMeter::lmOutlineColour,          juce::Colours::red);
    lnf->setColour (FFAU::LevelMeter::lmBackgroundColour,       juce::Colour (0xff050a29));
    lnf->setColour (FFAU::LevelMeter::lmBackgroundClipColour,   juce::Colours::red);
    lnf->setColour (FFAU::LevelMeter::lmMeterForegroundColour,  juce::Colours::green);
    lnf->setColour (FFAU::LevelMeter::lmMeterOutlineColour,     juce::Colours::red);
    lnf->setColour (FFAU::LevelMeter::lmMeterBackgroundColour,  juce::Colours::black);
    lnf->setColour (FFAU::LevelMeter::lmMeterMaxNormalColour,   juce::Colours::white);
    lnf->setColour (FFAU::LevelMeter::lmMeterMaxWarnColour,     juce::Colours::orange);
    lnf->setColour (FFAU::LevelMeter::lmMeterMaxOverColour,     juce::Colours::darkred);
    lnf->setColour (FFAU::LevelMeter::lmMeterGradientLowColour, juce::Colour (50, 0, 0));
    lnf->setColour (FFAU::LevelMeter::lmMeterGradientMidColour, juce::Colours::darkred);
    lnf->setColour (FFAU::LevelMeter::lmMeterGradientMaxColour, juce::Colours::red);
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
    inputKnob.setRange(-48, 6, 0.1);
    inputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    inputKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (inputLabel);
    inputLabel.setText ("input volume", dontSendNotification);
    inputLabel.setColour(Label::textColourId, Colours::red);
    inputLabel.attachToComponent (&inputKnob, false);
    
    
    //drive knob
    addAndMakeVisible(driveKnob);
    driveKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    driveKnob.setRange(0, 100, 1);
    driveKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    driveKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (driveLabel);
    driveLabel.setText ("drive", dontSendNotification);
    driveLabel.setColour(Label::textColourId, Colours::red);
    driveLabel.attachToComponent (&driveKnob, false);
    
    
    
    //mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setRange(0, 1, 0.01);
    mixKnob.setValue(100);
    mixKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    mixKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (mixLabel);
    mixLabel.setText ("mix", dontSendNotification);
    mixLabel.setColour(Label::textColourId, Colours::red);
    mixLabel.attachToComponent (&mixKnob, false);
    
    //volume knob
    addAndMakeVisible(gainKnob);
    gainKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    gainKnob.setRange(-48, 6, 0.1);
    gainKnob.setValue(0);
    gainKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    gainKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (gainLabel);
    gainLabel.setText ("output volume", dontSendNotification);
    gainLabel.setColour(Label::textColourId, Colours::red);
    gainLabel.attachToComponent (&gainKnob, false);
    
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob);
    inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "outputGain", gainKnob);
    
    
    //visualiser
    //addAndMakeVisible(visualiser);
    addAndMakeVisible(p.visualiser);
    
    //distortion mode select
    
    distortionMode.addItem("mode 1", 1);
    distortionMode.addItem("mode 2", 2);
    distortionMode.addItem("mode 3", 3);
    distortionMode.setJustificationType(Justification::centred);
    distortionMode.setColour(ComboBox::textColourId, Colours::red);
    distortionMode.setColour(ComboBox::arrowColourId, Colours::red);
    distortionMode.setColour(ComboBox::buttonColourId, Colours::red);
    distortionMode.setColour(ComboBox::outlineColourId, Colours::red);
    distortionMode.setColour(ComboBox::focusedOutlineColourId, Colours::red);
    distortionMode.setColour(ComboBox::backgroundColourId, Colour (50, 0, 0));
    distortionMode.getLookAndFeel().setColour(PopupMenu::textColourId, Colours::red);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, Colours::darkred);
    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, Colours::white);
    distortionMode.getLookAndFeel().setColour(PopupMenu::headerTextColourId, Colours::red);
    distortionMode.getLookAndFeel().setColour(PopupMenu::backgroundColourId, Colour (50, 0, 0));

    modeSelection = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);
    addAndMakeVisible(distortionMode);
}

BloodAudioProcessorEditor::~BloodAudioProcessorEditor()
{
}

//==============================================================================
void BloodAudioProcessorEditor::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    //g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillAll (Colours::black);
    g.setColour (Colours::red);
    g.setFont (15.0f);
    g.drawFittedText ("Wings - Blood - ver 0.203", getLocalBounds(), Justification::bottomRight, 1);
}

void BloodAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    inputKnob.setBounds((getWidth()/5)*1-50, getHeight()-130, 100, 100);
    driveKnob.setBounds((getWidth()/5)*2-50, getHeight()-130, 100, 100);
    mixKnob.setBounds((getWidth()/5)*3-50, getHeight()-130, 100, 100);
    gainKnob.setBounds((getWidth()/5)*4-50, getHeight()-130, 100, 100);
    
    // visualiser
    processor.visualiser.setBounds((getWidth()/5)*2-50, 50, 250, (getHeight() / 6));
    
    // ff meter
    inputMeter->setBounds((getWidth()/5)*1-50, 50, 100, (getHeight() / 2));
    outputMeter->setBounds((getWidth()/5)*4-50, 50, 100, (getHeight() / 2));
    
    // distortion menu
//    Rectangle<int> area = getLocalBounds().reduced(40);
    distortionMode.setBounds((getWidth()/5)*2-50, 250, 250, 50);
}


