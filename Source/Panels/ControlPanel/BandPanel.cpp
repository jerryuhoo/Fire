/*
  ==============================================================================

    BandPanel.cpp
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "BandPanel.h"

//==============================================================================
BandPanel::BandPanel(FireAudioProcessor &p) : processor(p)
{
    // init vec
    shapeVector.add(recKnob1 = new juce::Slider());
    shapeVector.add(recKnob2 = new juce::Slider());
    shapeVector.add(recKnob3 = new juce::Slider());
    shapeVector.add(recKnob4 = new juce::Slider());
    shapeVector.add(biasKnob1 = new juce::Slider());
    shapeVector.add(biasKnob2 = new juce::Slider());
    shapeVector.add(biasKnob3 = new juce::Slider());
    shapeVector.add(biasKnob4 = new juce::Slider());
    
    widthVector.add(widthKnob1 = new juce::Slider());
    widthVector.add(widthKnob2 = new juce::Slider());
    widthVector.add(widthKnob3 = new juce::Slider());
    widthVector.add(widthKnob4 = new juce::Slider());
    
    compressorVector.add(compThreshKnob1 = new juce::Slider());
    compressorVector.add(compThreshKnob2 = new juce::Slider());
    compressorVector.add(compThreshKnob3 = new juce::Slider());
    compressorVector.add(compThreshKnob4 = new juce::Slider());
    compressorVector.add(compRatioKnob1 = new juce::Slider());
    compressorVector.add(compRatioKnob2 = new juce::Slider());
    compressorVector.add(compRatioKnob3 = new juce::Slider());
    compressorVector.add(compRatioKnob4 = new juce::Slider());
    
    // drive knobs
    driveKnob1.setLookAndFeel(&driveLookAndFeel1);
    driveKnob2.setLookAndFeel(&driveLookAndFeel2);
    driveKnob3.setLookAndFeel(&driveLookAndFeel3);
    driveKnob4.setLookAndFeel(&driveLookAndFeel4);
    setListenerKnob(driveKnob1);
    setListenerKnob(driveKnob2);
    setListenerKnob(driveKnob3);
    setListenerKnob(driveKnob4);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    driveLabel.setColour(juce::Label::textColourId, juce::Colours::red.withBrightness(0.9f));
    driveLabel.attachToComponent(&driveKnob1, false);
    driveLabel.setJustificationType(juce::Justification::centred);
    
    // output knobs
    setListenerKnob(outputKnob1);
    setListenerKnob(outputKnob2);
    setListenerKnob(outputKnob3);
    setListenerKnob(outputKnob4);
    outputKnob1.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob2.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob3.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob4.setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    
    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent(&outputKnob1, false);
    outputLabel.setJustificationType(juce::Justification::centred);
    
    outputKnob1.setTextValueSuffix("db");
    outputKnob2.setTextValueSuffix("db");
    outputKnob3.setTextValueSuffix("db");
    outputKnob4.setTextValueSuffix("db");
    
    // mix knobs
    setRotarySlider(mixKnob1, COLOUR1);
    setRotarySlider(mixKnob2, COLOUR1);
    setRotarySlider(mixKnob3, COLOUR1);
    setRotarySlider(mixKnob4, COLOUR1);
    
    // compressor ratio knobs
    setRotarySlider(*compRatioKnob1, juce::Colours::yellowgreen);
    setRotarySlider(*compRatioKnob2, juce::Colours::yellowgreen);
    setRotarySlider(*compRatioKnob3, juce::Colours::yellowgreen);
    setRotarySlider(*compRatioKnob4, juce::Colours::yellowgreen);

    addAndMakeVisible(CompRatioLabel);
    CompRatioLabel.setText("Ratio", juce::dontSendNotification);
    CompRatioLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompRatioLabel.setColour(juce::Label::textColourId, juce::Colours::yellowgreen);
    CompRatioLabel.attachToComponent(compRatioKnob1, false);
    CompRatioLabel.setJustificationType(juce::Justification::centred);
    
    // compressor threashold knobs
    setRotarySlider(*compThreshKnob1, juce::Colours::yellowgreen);
    setRotarySlider(*compThreshKnob2, juce::Colours::yellowgreen);
    setRotarySlider(*compThreshKnob3, juce::Colours::yellowgreen);
    setRotarySlider(*compThreshKnob4, juce::Colours::yellowgreen);

    compThreshKnob1->setTextValueSuffix("db");
    compThreshKnob2->setTextValueSuffix("db");
    compThreshKnob3->setTextValueSuffix("db");
    compThreshKnob4->setTextValueSuffix("db");
    
    addAndMakeVisible(CompThreshLabel);
    CompThreshLabel.setText("Threshold", juce::dontSendNotification);
    CompThreshLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompThreshLabel.setColour(juce::Label::textColourId, juce::Colours::yellowgreen);
    CompThreshLabel.attachToComponent(compThreshKnob1, false);
    CompThreshLabel.setJustificationType(juce::Justification::centred);
    
    // width knobs
    setRotarySlider(*widthKnob1, juce::Colours::skyblue);
    setRotarySlider(*widthKnob2, juce::Colours::skyblue);
    setRotarySlider(*widthKnob3, juce::Colours::skyblue);
    setRotarySlider(*widthKnob4, juce::Colours::skyblue);

    addAndMakeVisible(widthLabel);
    widthLabel.setText("Width", juce::dontSendNotification);
    widthLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    widthLabel.setColour(juce::Label::textColourId, juce::Colours::skyblue);
    widthLabel.attachToComponent(widthKnob1, false);
    widthLabel.setJustificationType(juce::Justification::centred);

    // bias knobs
    setRotarySlider(*biasKnob1, COLOUR1);
    setRotarySlider(*biasKnob2, COLOUR1);
    setRotarySlider(*biasKnob3, COLOUR1);
    setRotarySlider(*biasKnob4, COLOUR1);
    
    addAndMakeVisible(biasLabel);
    biasLabel.setText("Bias", juce::dontSendNotification);
    biasLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    biasLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    biasLabel.attachToComponent(biasKnob1, false);
    biasLabel.setJustificationType(juce::Justification::centred);
    
    // rec knobs
    setRotarySlider(*recKnob1, COLOUR1);
    setRotarySlider(*recKnob2, COLOUR1);
    setRotarySlider(*recKnob3, COLOUR1);
    setRotarySlider(*recKnob4, COLOUR1);

    addAndMakeVisible(recLabel);
    recLabel.setText("Rectification", juce::dontSendNotification);
    recLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    recLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    recLabel.attachToComponent(recKnob1, false);
    recLabel.setJustificationType(juce::Justification::centred);
    
    // Linked Button
    setFlatButton(linkedButton1, LINKED_ID1, "L");
    setFlatButton(linkedButton2, LINKED_ID2, "L");
    setFlatButton(linkedButton3, LINKED_ID3, "L");
    setFlatButton(linkedButton4, LINKED_ID4, "L");
    
    // safe overload Button
    setFlatButton(safeButton1, SAFE_ID1, "S");
    setFlatButton(safeButton2, SAFE_ID2, "S");
    setFlatButton(safeButton3, SAFE_ID3, "S");
    setFlatButton(safeButton4, SAFE_ID4, "S");
    
    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent(&mixKnob1, false);
    mixLabel.setJustificationType(juce::Justification::centred);
    
    // switches
    addAndMakeVisible(oscSwitch);
    oscSwitch.setClickingTogglesState(true);
    oscSwitch.setRadioGroupId(switchButtons);
    oscSwitch.setButtonText("");
    oscSwitch.setToggleState(true, juce::dontSendNotification);
    oscSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::red.withBrightness(0.5f));
    oscSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red.withBrightness(0.9f));
    oscSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    oscSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    oscSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    oscSwitch.setLookAndFeel(&flatButtonLnf);
    oscSwitch.addListener(this);
    
    addAndMakeVisible(shapeSwitch);
    shapeSwitch.setClickingTogglesState(true);
    shapeSwitch.setRadioGroupId(switchButtons);
    shapeSwitch.setButtonText("");
    shapeSwitch.setToggleState(false, juce::dontSendNotification);
    shapeSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::yellow.withBrightness(0.5f));
    shapeSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow.withBrightness(0.9f));
    shapeSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    shapeSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    shapeSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    shapeSwitch.setLookAndFeel(&flatButtonLnf);
    shapeSwitch.addListener(this);
    
    addAndMakeVisible(compressorSwitch);
    compressorSwitch.setClickingTogglesState(true);
    compressorSwitch.setRadioGroupId(switchButtons);
    compressorSwitch.setButtonText("");
    compressorSwitch.setToggleState(false, juce::dontSendNotification);
    compressorSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::yellowgreen.withBrightness(0.5f));
    compressorSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellowgreen.withBrightness(0.9f));
    compressorSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    compressorSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    compressorSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    compressorSwitch.setLookAndFeel(&flatButtonLnf);
    compressorSwitch.addListener(this);
    
    addAndMakeVisible(widthSwitch);
    widthSwitch.setClickingTogglesState(true);
    widthSwitch.setRadioGroupId(switchButtons);
    widthSwitch.setButtonText("");
    widthSwitch.setToggleState(false, juce::dontSendNotification);
    widthSwitch.setColour(juce::TextButton::buttonColourId, juce::Colours::skyblue.withBrightness(0.5f));
    widthSwitch.setColour(juce::TextButton::buttonOnColourId, juce::Colours::skyblue.withBrightness(0.9f));
    widthSwitch.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    widthSwitch.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    widthSwitch.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    widthSwitch.setLookAndFeel(&flatButtonLnf);
    widthSwitch.addListener(this);
    
    // Attachment
    driveAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID1, driveKnob1);
    driveAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID2, driveKnob2);
    driveAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID3, driveKnob3);
    driveAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID4, driveKnob4);
    
    compRatioAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID1, *compRatioKnob1);
    compRatioAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID2, *compRatioKnob2);
    compRatioAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID3, *compRatioKnob3);
    compRatioAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_RATIO_ID4, *compRatioKnob4);
    
    compThreshAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID1, *compThreshKnob1);
    compThreshAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID2, *compThreshKnob2);
    compThreshAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID3, *compThreshKnob3);
    compThreshAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COMP_THRESH_ID4, *compThreshKnob4);
    
    outputAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID1, outputKnob1);
    outputAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID2, outputKnob2);
    outputAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID3, outputKnob3);
    outputAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID4, outputKnob4);
    
    mixAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID1, mixKnob1);
    mixAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID2, mixKnob2);
    mixAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID3, mixKnob3);
    mixAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID4, mixKnob4);
    
    biasAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID1, *biasKnob1);
    biasAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID2, *biasKnob2);
    biasAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID3, *biasKnob3);
    biasAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID4, *biasKnob4);
    
    recAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID1, *recKnob1);
    recAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID2, *recKnob2);
    recAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID3, *recKnob3);
    recAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID4, *recKnob4);
    
    linkedAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID1, linkedButton1);
    linkedAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID2, linkedButton2);
    linkedAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID3, linkedButton3);
    linkedAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID4, linkedButton4);
    
    safeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID1, safeButton1);
    safeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID2, safeButton2);
    safeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID3, safeButton3);
    safeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID4, safeButton4);
    
    widthAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID1, *widthKnob1);
    widthAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID2, *widthKnob2);
    widthAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID3, *widthKnob3);
    widthAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID4, *widthKnob4);
}

BandPanel::~BandPanel()
{
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
    oscSwitch.setLookAndFeel(nullptr);
    shapeSwitch.setLookAndFeel(nullptr);
    widthSwitch.setLookAndFeel(nullptr);
    compressorSwitch.setLookAndFeel(nullptr);
}

void BandPanel::paint (juce::Graphics& g)
{
    bool isOscSwitchOn = oscSwitch.getToggleState();
    bool isWidthSwitchOn = widthSwitch.getToggleState();
    bool isShapeSwitchOn = shapeSwitch.getToggleState();
    bool isCompressorSwitchOn = compressorSwitch.getToggleState();
    
    setFourKnobsVisibility(driveKnob1, driveKnob2, driveKnob3, driveKnob4, focusBandNum);
    
    if (isOscSwitchOn)
    {
        setInvisible(shapeVector);
        setInvisible(compressorVector);
        setInvisible(widthVector);
        recLabel.setVisible(false);
        biasLabel.setVisible(false);
        widthLabel.setVisible(false);
        CompRatioLabel.setVisible(false);
        CompThreshLabel.setVisible(false);
    }
    if (isCompressorSwitchOn)
    {
        setFourKnobsVisibility(*compRatioKnob1, *compRatioKnob2, *compRatioKnob3, *compRatioKnob4, focusBandNum);
        setFourKnobsVisibility(*compThreshKnob1, *compThreshKnob2, *compThreshKnob3, *compThreshKnob4, focusBandNum);
        setInvisible(shapeVector);
        setInvisible(widthVector);
        CompRatioLabel.setVisible(true);
        CompThreshLabel.setVisible(true);
        recLabel.setVisible(false);
        biasLabel.setVisible(false);
        widthLabel.setVisible(false);
        
//            graphPanel.getVuPanel()->setZoomState(true);
//            graphPanel.getOscilloscope()->setZoomState(false);
//            graphPanel.getDistortionGraph()->setZoomState(false);
//            graphPanel.getWidthGraph()->setZoomState(false);
    }
    if (isShapeSwitchOn)
    {
        setFourKnobsVisibility(*recKnob1, *recKnob2, *recKnob3, *recKnob4, focusBandNum);
        setFourKnobsVisibility(*biasKnob1, *biasKnob2, *biasKnob3, *biasKnob4, focusBandNum);
        setInvisible(compressorVector);
        setInvisible(widthVector);
        CompRatioLabel.setVisible(false);
        CompThreshLabel.setVisible(false);
        recLabel.setVisible(true);
        biasLabel.setVisible(true);
        widthLabel.setVisible(false);
        
//            graphPanel.getVuPanel()->setZoomState(false);
//            graphPanel.getOscilloscope()->setZoomState(false);
//            graphPanel.getDistortionGraph()->setZoomState(true);
//            graphPanel.getWidthGraph()->setZoomState(false);
    }
    if (isWidthSwitchOn)
    {
        setFourKnobsVisibility(*widthKnob1, *widthKnob2, *widthKnob3, *widthKnob4, focusBandNum);
        setInvisible(shapeVector);
        setInvisible(compressorVector);
        CompRatioLabel.setVisible(false);
        CompThreshLabel.setVisible(false);
        recLabel.setVisible(false);
        biasLabel.setVisible(false);
        widthLabel.setVisible(true);
        
//            graphPanel.getVuPanel()->setZoomState(false);
//            graphPanel.getOscilloscope()->setZoomState(false);
//            graphPanel.getDistortionGraph()->setZoomState(false);
//            graphPanel.getWidthGraph()->setZoomState(true);
    }
    
    setFourKnobsVisibility(outputKnob1, outputKnob2, outputKnob3, outputKnob4, focusBandNum);
    setFourKnobsVisibility(mixKnob1, mixKnob2, mixKnob3, mixKnob4, focusBandNum);
    
    setFourKnobsVisibility(linkedButton1, linkedButton2, linkedButton3, linkedButton4, focusBandNum);
    setFourKnobsVisibility(safeButton1, safeButton2, safeButton3, safeButton4, focusBandNum);

    g.setColour(COLOUR6);
    if (!isOscSwitchOn)
    {
        g.drawRect(bandKnobArea);
        g.drawRect(driveKnobArea);
    }
    g.drawRect(outputKnobArea);
    
    // drive reduction
    driveLookAndFeel1.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID1);
    driveLookAndFeel1.reductionPrecent = processor.getReductionPrecent(SAFE_ID1);
    driveLookAndFeel2.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID2);
    driveLookAndFeel2.reductionPrecent = processor.getReductionPrecent(SAFE_ID2);
    driveLookAndFeel3.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID3);
    driveLookAndFeel3.reductionPrecent = processor.getReductionPrecent(SAFE_ID3);
    driveLookAndFeel4.sampleMaxValue = processor.getSampleMaxValue(SAFE_ID4);
    driveLookAndFeel4.reductionPrecent = processor.getReductionPrecent(SAFE_ID4);
}

void BandPanel::resized()
{
    juce::Rectangle<int> controlArea = getLocalBounds();
    
    
    bandKnobArea = controlArea.removeFromLeft(getWidth() / 5 * 2);
    driveKnobArea = controlArea.removeFromLeft(getWidth() / 5);
    
    outputKnobArea = controlArea;
    driveKnobArea.removeFromTop(getHeight() / 5);
    driveKnobArea.removeFromBottom(getHeight() / 5);
    outputKnobArea.removeFromTop(getHeight() / 5);
    outputKnobArea.removeFromBottom(getHeight() / 5);
    bandKnobArea.removeFromTop(getHeight() / 5);
    bandKnobArea.removeFromBottom(getHeight() / 5);
    
    juce::Rectangle<int> switchArea = bandKnobArea.removeFromLeft(getWidth() / 50);
    oscSwitch.setBounds(switchArea.removeFromTop(bandKnobArea.getHeight() / 4));
    shapeSwitch.setBounds(switchArea.removeFromTop(bandKnobArea.getHeight() / 4));
    compressorSwitch.setBounds(switchArea.removeFromTop(bandKnobArea.getHeight() / 4));
    widthSwitch.setBounds(switchArea.removeFromTop(bandKnobArea.getHeight() / 4));

    juce::Rectangle<int> bigDriveArea = getLocalBounds().removeFromLeft(getWidth() / 5 * 3).reduced(getHeight() / 10);
    if(oscSwitch.getToggleState())
    {
        driveKnob1.setBounds(bigDriveArea);
        driveKnob2.setBounds(bigDriveArea);
        driveKnob3.setBounds(bigDriveArea);
        driveKnob4.setBounds(bigDriveArea);
    }
    else
    {
        driveKnob1.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
        driveKnob2.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
        driveKnob3.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
        driveKnob4.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
    }
    
    // shape
    juce::Rectangle<int> bandKnobAreaLeft = bandKnobArea;
    juce::Rectangle<int> bandKnobAreaRight = bandKnobAreaLeft.removeFromRight(bandKnobArea.getWidth() / 2);
    bandKnobAreaLeft = bandKnobAreaLeft.reduced(0, bandKnobAreaLeft.getHeight() / 5);
    bandKnobAreaRight = bandKnobAreaRight.reduced(0, bandKnobAreaRight.getHeight() / 5);
    recKnob1->setBounds(bandKnobAreaLeft);
    recKnob2->setBounds(bandKnobAreaLeft);
    recKnob3->setBounds(bandKnobAreaLeft);
    recKnob4->setBounds(bandKnobAreaLeft);

    biasKnob1->setBounds(bandKnobAreaRight);
    biasKnob2->setBounds(bandKnobAreaRight);
    biasKnob3->setBounds(bandKnobAreaRight);
    biasKnob4->setBounds(bandKnobAreaRight);
    
    // width
    widthKnob1->setBounds(bandKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
    widthKnob2->setBounds(bandKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
    widthKnob3->setBounds(bandKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
    widthKnob4->setBounds(bandKnobArea.reduced(0, bandKnobArea.getHeight() / 5));

    // compressor
    
    compThreshKnob1->setBounds(bandKnobAreaLeft);
    compThreshKnob2->setBounds(bandKnobAreaLeft);
    compThreshKnob3->setBounds(bandKnobAreaLeft);
    compThreshKnob4->setBounds(bandKnobAreaLeft);
    
    compRatioKnob1->setBounds(bandKnobAreaRight);
    compRatioKnob2->setBounds(bandKnobAreaRight);
    compRatioKnob3->setBounds(bandKnobAreaRight);
    compRatioKnob4->setBounds(bandKnobAreaRight);

    juce::Rectangle<int> outputKnobAreaLeft = outputKnobArea;
    juce::Rectangle<int> outputKnobAreaRight = outputKnobAreaLeft.removeFromRight(bandKnobArea.getWidth() / 2);
    outputKnobAreaLeft = outputKnobAreaLeft.reduced(0, outputKnobAreaLeft.getHeight() / 5);
    outputKnobAreaRight = outputKnobAreaRight.reduced(0, outputKnobAreaRight.getHeight() / 5);
    mixKnob1.setBounds(outputKnobAreaLeft);
    mixKnob2.setBounds(outputKnobAreaLeft);
    mixKnob3.setBounds(outputKnobAreaLeft);
    mixKnob4.setBounds(outputKnobAreaLeft);

    outputKnob1.setBounds(outputKnobAreaRight);
    outputKnob2.setBounds(outputKnobAreaRight);
    outputKnob3.setBounds(outputKnobAreaRight);
    outputKnob4.setBounds(outputKnobAreaRight);

    // buttons
    juce::Rectangle<int> outputButtonArea = outputKnobArea;
    outputButtonArea = outputButtonArea.removeFromLeft(getWidth() / 25);
    juce::Rectangle<int> outputButtonAreaUp = outputButtonArea;
    juce::Rectangle<int> outputButtonAreaDown = outputButtonAreaUp.removeFromBottom(outputButtonArea.getHeight() / 2);
    linkedButton1.setBounds(outputButtonAreaUp);
    linkedButton2.setBounds(outputButtonAreaUp);
    linkedButton3.setBounds(outputButtonAreaUp);
    linkedButton4.setBounds(outputButtonAreaUp);

    safeButton1.setBounds(outputButtonAreaDown);
    safeButton2.setBounds(outputButtonAreaDown);
    safeButton3.setBounds(outputButtonAreaDown);
    safeButton4.setBounds(outputButtonAreaDown);
    
    // set look and feel scale
//    otherLookAndFeel.scale = scale;
//    roundedButtonLnf.scale = scale;
    driveLookAndFeel1.scale = scale;
    driveLookAndFeel2.scale = scale;
    driveLookAndFeel3.scale = scale;
    driveLookAndFeel4.scale = scale;

}

void BandPanel::sliderValueChanged(juce::Slider *slider)
{
    linkValue(*slider, driveKnob1, outputKnob1, linkedButton1);
    linkValue(*slider, driveKnob2, outputKnob2, linkedButton2);
    linkValue(*slider, driveKnob3, outputKnob3, linkedButton3);
    linkValue(*slider, driveKnob4, outputKnob4, linkedButton4);
}

void BandPanel::linkValue(juce::Slider &xSlider, juce::Slider &driveSlider, juce::Slider &outputSlider, juce::TextButton& linkedButton)
{
    // x changes, then y will change
    if (linkedButton.getToggleState() == true)
    {
        if (&xSlider == &driveSlider)
        {
            outputSlider.setValue(-xSlider.getValue() * 0.1f); // use defalut notification type
        }
    }
}

void BandPanel::comboBoxChanged(juce::ComboBox *combobox)
{

}

void BandPanel::timerCallback()
{
}

void BandPanel::buttonClicked(juce::Button *clickedButton)
{
    juce::Rectangle<int> bigDriveArea = getLocalBounds().removeFromLeft(getWidth() / 5 * 3).reduced(getHeight() / 10);
    if(oscSwitch.getToggleState())
    {
        driveKnob1.setBounds(bigDriveArea);
        driveKnob2.setBounds(bigDriveArea);
        driveKnob3.setBounds(bigDriveArea);
        driveKnob4.setBounds(bigDriveArea);
    }
    else
    {
        driveKnob1.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
        driveKnob2.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
        driveKnob3.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
        driveKnob4.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
    }
    repaint();
}

void BandPanel::setListenerKnob(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.addListener(this);
}


void BandPanel::setRotarySlider(juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
}

void BandPanel::setFlatButton(juce::TextButton& button, juce::String paramId, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
//    bool state = processor.treeState.getRawParameterValue(paramId);
    //bool state = true;
    //button.setToggleState(state, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonColourId, COLOUR7);
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, COLOUR1);
    button.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
    button.setButtonText(buttonName);
    button.setLookAndFeel(&flatButtonLnf);
}

void BandPanel::setFourKnobsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum)
{
    driveLabel.setVisible(true);
    mixLabel.setVisible(true);
    outputLabel.setVisible(true);
    if (bandNum == 0)
    {
        component1.setVisible(true);
        component2.setVisible(false);
        component3.setVisible(false);
        component4.setVisible(false);
    }
    else if (bandNum == 1)
    {
        component1.setVisible(false);
        component2.setVisible(true);
        component3.setVisible(false);
        component4.setVisible(false);
    }
    else if (bandNum == 2)
    {
        component1.setVisible(false);
        component2.setVisible(false);
        component3.setVisible(true);
        component4.setVisible(false);
    }
    else if (bandNum == 3)
    {
        component1.setVisible(false);
        component2.setVisible(false);
        component3.setVisible(false);
        component4.setVisible(true);
    }
}

void BandPanel::setFocusBandNum(int num)
{
    focusBandNum = num;
}

void BandPanel::setScale(float scale)
{
    this->scale = scale;
}

void BandPanel::setInvisible(juce::OwnedArray<juce::Component, juce::CriticalSection> &array)
{
    for (int i = 0; i < array.size(); ++i)
    {
        array[i]->setVisible(false);
    }
}


void BandPanel::changeSliderState(int bandNum, bool isPresetChanged)
{
    if (bandNum == 0)
    {
        setSliderState(&processor, driveKnob1, MODE_ID1, tempDriveValue[0], isPresetChanged);
        setSliderState(&processor, *biasKnob1, MODE_ID1, tempBiasValue[0], isPresetChanged);
    }
    else if (bandNum == 1)
    {
        setSliderState(&processor, driveKnob2, MODE_ID2, tempDriveValue[1], isPresetChanged);
        setSliderState(&processor, *biasKnob2, MODE_ID2, tempBiasValue[1], isPresetChanged);
    }
    else if (bandNum == 2)
    {
        setSliderState(&processor, driveKnob3, MODE_ID3, tempDriveValue[2], isPresetChanged);
        setSliderState(&processor, *biasKnob3, MODE_ID3, tempBiasValue[2], isPresetChanged);
    }
    else if (bandNum == 3)
    {
        setSliderState(&processor, driveKnob4, MODE_ID4, tempDriveValue[3], isPresetChanged);
        setSliderState(&processor, *biasKnob4, MODE_ID4, tempBiasValue[3], isPresetChanged);
    }
}

void BandPanel::setSliderState(FireAudioProcessor* processor, juce::Slider& slider, juce::String paramId, float &tempValue, bool isPresetChanged)
{
    auto val = processor->treeState.getRawParameterValue(paramId);
    int selection = val->load();
    if (selection == 0)
    {
        tempValue = slider.getValue();
        slider.setValue(0);
        slider.setEnabled(false);
    }
    else if (isPresetChanged)
    {
        slider.setEnabled(true);
    }
    else if (!slider.isEnabled())
    {
        slider.setValue(tempValue);
        slider.setEnabled(true);
    }
}

