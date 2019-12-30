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
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (800, 400);
    
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
    
    //range knob
    //addAndMakeVisible(rangeKnob);
    rangeKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    rangeKnob.setRange(0, 100, 1);
    rangeKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);
    rangeKnob.setLookAndFeel(&otherLookAndFeel);
    
    addAndMakeVisible (rangeLabel);
    rangeLabel.setText ("range", dontSendNotification);
    rangeLabel.setColour(Label::textColourId, Colours::red);
    rangeLabel.attachToComponent (&rangeKnob, false);
    
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
    gainLabel.setText ("volume", dontSendNotification);
    gainLabel.setColour(Label::textColourId, Colours::red);
    gainLabel.attachToComponent (&gainKnob, false);
    
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob);
    rangeAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "range", rangeKnob);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "gain", gainKnob);
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
    g.drawFittedText ("Wings - Blood - ver 0.121", getLocalBounds(), Justification::bottomRight, 1);
}

void BloodAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    driveKnob.setBounds((getWidth()/5)*1-50, getHeight()-130, 100, 100);
    rangeKnob.setBounds((getWidth()/5)*2-50, getHeight()-130, 100, 100);
    mixKnob.setBounds((getWidth()/5)*3-50, getHeight()-130, 100, 100);
    gainKnob.setBounds((getWidth()/5)*4-50, getHeight()-130, 100, 100);
}
