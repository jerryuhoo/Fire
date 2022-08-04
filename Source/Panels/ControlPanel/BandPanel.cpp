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
BandPanel::BandPanel (FireAudioProcessor& p) : processor (p),
                                               compressorBypassButton1 (std::make_unique<juce::ToggleButton>()),
                                               compressorBypassButton2 (std::make_unique<juce::ToggleButton>()),
                                               compressorBypassButton3 (std::make_unique<juce::ToggleButton>()),
                                               compressorBypassButton4 (std::make_unique<juce::ToggleButton>()),
                                               widthBypassButton1 (std::make_unique<juce::ToggleButton>()),
                                               widthBypassButton2 (std::make_unique<juce::ToggleButton>()),
                                               widthBypassButton3 (std::make_unique<juce::ToggleButton>()),
                                               widthBypassButton4 (std::make_unique<juce::ToggleButton>())
{
    // init vec
    shapeVector = { &recKnob1, &recKnob2, &recKnob3, &recKnob4, &biasKnob1, &biasKnob2, &biasKnob3, &biasKnob4, &shapePanelLabel };
    widthVector = { &widthKnob1, &widthKnob2, &widthKnob3, &widthKnob4, &*widthBypassButton1, &*widthBypassButton2, &*widthBypassButton3, &*widthBypassButton4, &widthPanelLabel };

    compressorVector = { &compThreshKnob1, &compThreshKnob2, &compThreshKnob3, &compThreshKnob4, &compRatioKnob1, &compRatioKnob2, &compRatioKnob3, &compRatioKnob4, &*compressorBypassButton1, &*compressorBypassButton2, &*compressorBypassButton3, &*compressorBypassButton4, &compressorPanelLabel };

    // init panel labels
    addAndMakeVisible (shapePanelLabel);
    shapePanelLabel.setLookAndFeel(&flatLnf);
    shapePanelLabel.setText ("Shape", juce::dontSendNotification);
    shapePanelLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    shapePanelLabel.setColour (juce::Label::textColourId, SHAPE_COLOUR);
    
    addAndMakeVisible (compressorPanelLabel);
    compressorPanelLabel.setLookAndFeel(&flatLnf);
    compressorPanelLabel.setText ("Compressor", juce::dontSendNotification);
    compressorPanelLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    compressorPanelLabel.setColour (juce::Label::textColourId, COMP_COLOUR);
    
    addAndMakeVisible (widthPanelLabel);
    widthPanelLabel.setLookAndFeel(&flatLnf);
    widthPanelLabel.setText ("Stereo", juce::dontSendNotification);
    widthPanelLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    widthPanelLabel.setColour (juce::Label::textColourId, WIDTH_COLOUR);
    
    // drive knobs
    driveKnob1.setLookAndFeel (&driveLookAndFeel1);
    driveKnob2.setLookAndFeel (&driveLookAndFeel2);
    driveKnob3.setLookAndFeel (&driveLookAndFeel3);
    driveKnob4.setLookAndFeel (&driveLookAndFeel4);
    initListenerKnob (driveKnob1);
    initListenerKnob (driveKnob2);
    initListenerKnob (driveKnob3);
    initListenerKnob (driveKnob4);

    addAndMakeVisible (driveLabel);
    driveLabel.setText ("Drive", juce::dontSendNotification);
    driveLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    driveLabel.setColour (juce::Label::textColourId, DRIVE_COLOUR.withBrightness (0.9f));
    driveLabel.attachToComponent (&driveKnob1, false);
    driveLabel.setJustificationType (juce::Justification::centred);

    // output knobs
    initListenerKnob (outputKnob1);
    initListenerKnob (outputKnob2);
    initListenerKnob (outputKnob3);
    initListenerKnob (outputKnob4);
    outputKnob1.setColour (juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob2.setColour (juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob3.setColour (juce::Slider::rotarySliderFillColourId, COLOUR1);
    outputKnob4.setColour (juce::Slider::rotarySliderFillColourId, COLOUR1);

    addAndMakeVisible (outputLabel);
    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabel.setColour (juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent (&outputKnob1, false);
    outputLabel.setJustificationType (juce::Justification::centred);

    outputKnob1.setTextValueSuffix ("db");
    outputKnob2.setTextValueSuffix ("db");
    outputKnob3.setTextValueSuffix ("db");
    outputKnob4.setTextValueSuffix ("db");

    // mix knobs
    initRotarySlider (mixKnob1, COLOUR1);
    initRotarySlider (mixKnob2, COLOUR1);
    initRotarySlider (mixKnob3, COLOUR1);
    initRotarySlider (mixKnob4, COLOUR1);

    // compressor ratio knobs
    initRotarySlider (compRatioKnob1, COMP_COLOUR);
    initRotarySlider (compRatioKnob2, COMP_COLOUR);
    initRotarySlider (compRatioKnob3, COMP_COLOUR);
    initRotarySlider (compRatioKnob4, COMP_COLOUR);

    addAndMakeVisible (CompRatioLabel);
    CompRatioLabel.setText ("Ratio", juce::dontSendNotification);
    CompRatioLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompRatioLabel.setColour (juce::Label::textColourId, COMP_COLOUR);
    CompRatioLabel.attachToComponent (&compRatioKnob1, false);
    CompRatioLabel.setJustificationType (juce::Justification::centred);

    // compressor threashold knobs
    initRotarySlider (compThreshKnob1, COMP_COLOUR);
    initRotarySlider (compThreshKnob2, COMP_COLOUR);
    initRotarySlider (compThreshKnob3, COMP_COLOUR);
    initRotarySlider (compThreshKnob4, COMP_COLOUR);

    compThreshKnob1.setTextValueSuffix ("db");
    compThreshKnob2.setTextValueSuffix ("db");
    compThreshKnob3.setTextValueSuffix ("db");
    compThreshKnob4.setTextValueSuffix ("db");

    addAndMakeVisible (CompThreshLabel);
    CompThreshLabel.setText ("Threshold", juce::dontSendNotification);
    CompThreshLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    CompThreshLabel.setColour (juce::Label::textColourId, COMP_COLOUR);
    CompThreshLabel.attachToComponent (&compThreshKnob1, false);
    CompThreshLabel.setJustificationType (juce::Justification::centred);

    // width knobs
    initRotarySlider (widthKnob1, WIDTH_COLOUR);
    initRotarySlider (widthKnob2, WIDTH_COLOUR);
    initRotarySlider (widthKnob3, WIDTH_COLOUR);
    initRotarySlider (widthKnob4, WIDTH_COLOUR);

    addAndMakeVisible (widthLabel);
    widthLabel.setText ("Width", juce::dontSendNotification);
    widthLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    widthLabel.setColour (juce::Label::textColourId, WIDTH_COLOUR);
    widthLabel.attachToComponent (&widthKnob1, false);
    widthLabel.setJustificationType (juce::Justification::centred);

    // bias knobs
    initRotarySlider (biasKnob1, SHAPE_COLOUR);
    initRotarySlider (biasKnob2, SHAPE_COLOUR);
    initRotarySlider (biasKnob3, SHAPE_COLOUR);
    initRotarySlider (biasKnob4, SHAPE_COLOUR);

    addAndMakeVisible (biasLabel);
    biasLabel.setText ("Bias", juce::dontSendNotification);
    biasLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    biasLabel.setColour (juce::Label::textColourId, SHAPE_COLOUR);
    biasLabel.attachToComponent (&biasKnob1, false);
    biasLabel.setJustificationType (juce::Justification::centred);

    // rec knobs
    initRotarySlider (recKnob1, SHAPE_COLOUR);
    initRotarySlider (recKnob2, SHAPE_COLOUR);
    initRotarySlider (recKnob3, SHAPE_COLOUR);
    initRotarySlider (recKnob4, SHAPE_COLOUR);

    addAndMakeVisible (recLabel);
    recLabel.setText ("Rectification", juce::dontSendNotification);
    recLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    recLabel.setColour (juce::Label::textColourId, SHAPE_COLOUR);
    recLabel.attachToComponent (&recKnob1, false);
    recLabel.setJustificationType (juce::Justification::centred);

    // Linked Button
    initFlatButton (linkedButton1, LINKED_ID1, "L");
    initFlatButton (linkedButton2, LINKED_ID2, "L");
    initFlatButton (linkedButton3, LINKED_ID3, "L");
    initFlatButton (linkedButton4, LINKED_ID4, "L");

    // safe overload Button
    initFlatButton (safeButton1, SAFE_ID1, "S");
    initFlatButton (safeButton2, SAFE_ID2, "S");
    initFlatButton (safeButton3, SAFE_ID3, "S");
    initFlatButton (safeButton4, SAFE_ID4, "S");
    
    // extreme Button
    initFlatButton (extremeButton1, EXTREME_ID1, "E");
    initFlatButton (extremeButton2, EXTREME_ID2, "E");
    initFlatButton (extremeButton3, EXTREME_ID3, "E");
    initFlatButton (extremeButton4, EXTREME_ID4, "E");

    addAndMakeVisible (mixLabel);
    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setFont (juce::Font (KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabel.setColour (juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent (&mixKnob1, false);
    mixLabel.setJustificationType (juce::Justification::centred);

    // switches
    addAndMakeVisible (oscSwitch);
    oscSwitch.setRadioGroupId (switchButtons);
    oscSwitch.setToggleState (true, juce::dontSendNotification);
    oscSwitch.setColour (juce::ToggleButton::tickDisabledColourId, DRIVE_COLOUR.withBrightness (0.5f));
    oscSwitch.setColour (juce::ToggleButton::tickColourId, DRIVE_COLOUR.withBrightness (0.9f));
    oscSwitch.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    oscSwitch.setLookAndFeel (&flatLnf);
    oscSwitch.addListener (this);

    addAndMakeVisible (shapeSwitch);
    shapeSwitch.setRadioGroupId (switchButtons);
    shapeSwitch.setToggleState (false, juce::dontSendNotification);
    shapeSwitch.setColour (juce::ToggleButton::tickDisabledColourId, SHAPE_COLOUR.withBrightness (0.5f));
    shapeSwitch.setColour (juce::ToggleButton::tickColourId, SHAPE_COLOUR.withBrightness (0.9f));
    shapeSwitch.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    shapeSwitch.setLookAndFeel (&flatLnf);
    shapeSwitch.addListener (this);

    addAndMakeVisible (compressorSwitch);
    compressorSwitch.setRadioGroupId (switchButtons);
    compressorSwitch.setToggleState (false, juce::dontSendNotification);
    compressorSwitch.setColour (juce::ToggleButton::tickDisabledColourId, COMP_COLOUR.withBrightness (0.5f));
    compressorSwitch.setColour (juce::ToggleButton::tickColourId, COMP_COLOUR.withBrightness (0.9f));
    compressorSwitch.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    compressorSwitch.setLookAndFeel (&flatLnf);
    compressorSwitch.addListener (this);

    addAndMakeVisible (widthSwitch);
    widthSwitch.setRadioGroupId (switchButtons);
    widthSwitch.setToggleState (false, juce::dontSendNotification);
    widthSwitch.setColour (juce::ToggleButton::tickDisabledColourId, WIDTH_COLOUR.withBrightness (0.5f));
    widthSwitch.setColour (juce::ToggleButton::tickColourId, WIDTH_COLOUR.withBrightness (0.9f));
    widthSwitch.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    widthSwitch.setLookAndFeel (&flatLnf);
    widthSwitch.addListener (this);

    initBypassButton (*compressorBypassButton1, COMP_COLOUR, 0);
    initBypassButton (*compressorBypassButton2, COMP_COLOUR, 1);
    initBypassButton (*compressorBypassButton3, COMP_COLOUR, 2);
    initBypassButton (*compressorBypassButton4, COMP_COLOUR, 3);

    initBypassButton (*widthBypassButton1, WIDTH_COLOUR, 4);
    initBypassButton (*widthBypassButton2, WIDTH_COLOUR, 5);
    initBypassButton (*widthBypassButton3, WIDTH_COLOUR, 6);
    initBypassButton (*widthBypassButton4, WIDTH_COLOUR, 7);
    
    setVisibility (shapeVector, false);
    setVisibility (compressorVector, false);
    setVisibility (widthVector, false);

    // Attachment
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    driveAttachment1 = std::make_unique<SliderAttach> (processor.treeState, DRIVE_ID1, driveKnob1);
    driveAttachment2 = std::make_unique<SliderAttach> (processor.treeState, DRIVE_ID2, driveKnob2);
    driveAttachment3 = std::make_unique<SliderAttach> (processor.treeState, DRIVE_ID3, driveKnob3);
    driveAttachment4 = std::make_unique<SliderAttach> (processor.treeState, DRIVE_ID4, driveKnob4);

    compRatioAttachment1 = std::make_unique<SliderAttach> (processor.treeState, COMP_RATIO_ID1, compRatioKnob1);
    compRatioAttachment2 = std::make_unique<SliderAttach> (processor.treeState, COMP_RATIO_ID2, compRatioKnob2);
    compRatioAttachment3 = std::make_unique<SliderAttach> (processor.treeState, COMP_RATIO_ID3, compRatioKnob3);
    compRatioAttachment4 = std::make_unique<SliderAttach> (processor.treeState, COMP_RATIO_ID4, compRatioKnob4);

    compThreshAttachment1 = std::make_unique<SliderAttach> (processor.treeState, COMP_THRESH_ID1, compThreshKnob1);
    compThreshAttachment2 = std::make_unique<SliderAttach> (processor.treeState, COMP_THRESH_ID2, compThreshKnob2);
    compThreshAttachment3 = std::make_unique<SliderAttach> (processor.treeState, COMP_THRESH_ID3, compThreshKnob3);
    compThreshAttachment4 = std::make_unique<SliderAttach> (processor.treeState, COMP_THRESH_ID4, compThreshKnob4);

    outputAttachment1 = std::make_unique<SliderAttach> (processor.treeState, OUTPUT_ID1, outputKnob1);
    outputAttachment2 = std::make_unique<SliderAttach> (processor.treeState, OUTPUT_ID2, outputKnob2);
    outputAttachment3 = std::make_unique<SliderAttach> (processor.treeState, OUTPUT_ID3, outputKnob3);
    outputAttachment4 = std::make_unique<SliderAttach> (processor.treeState, OUTPUT_ID4, outputKnob4);

    mixAttachment1 = std::make_unique<SliderAttach> (processor.treeState, MIX_ID1, mixKnob1);
    mixAttachment2 = std::make_unique<SliderAttach> (processor.treeState, MIX_ID2, mixKnob2);
    mixAttachment3 = std::make_unique<SliderAttach> (processor.treeState, MIX_ID3, mixKnob3);
    mixAttachment4 = std::make_unique<SliderAttach> (processor.treeState, MIX_ID4, mixKnob4);

    biasAttachment1 = std::make_unique<SliderAttach> (processor.treeState, BIAS_ID1, biasKnob1);
    biasAttachment2 = std::make_unique<SliderAttach> (processor.treeState, BIAS_ID2, biasKnob2);
    biasAttachment3 = std::make_unique<SliderAttach> (processor.treeState, BIAS_ID3, biasKnob3);
    biasAttachment4 = std::make_unique<SliderAttach> (processor.treeState, BIAS_ID4, biasKnob4);

    recAttachment1 = std::make_unique<SliderAttach> (processor.treeState, REC_ID1, recKnob1);
    recAttachment2 = std::make_unique<SliderAttach> (processor.treeState, REC_ID2, recKnob2);
    recAttachment3 = std::make_unique<SliderAttach> (processor.treeState, REC_ID3, recKnob3);
    recAttachment4 = std::make_unique<SliderAttach> (processor.treeState, REC_ID4, recKnob4);

    linkedAttachment1 = std::make_unique<ButtonAttach> (processor.treeState, LINKED_ID1, linkedButton1);
    linkedAttachment2 = std::make_unique<ButtonAttach> (processor.treeState, LINKED_ID2, linkedButton2);
    linkedAttachment3 = std::make_unique<ButtonAttach> (processor.treeState, LINKED_ID3, linkedButton3);
    linkedAttachment4 = std::make_unique<ButtonAttach> (processor.treeState, LINKED_ID4, linkedButton4);

    safeAttachment1 = std::make_unique<ButtonAttach> (processor.treeState, SAFE_ID1, safeButton1);
    safeAttachment2 = std::make_unique<ButtonAttach> (processor.treeState, SAFE_ID2, safeButton2);
    safeAttachment3 = std::make_unique<ButtonAttach> (processor.treeState, SAFE_ID3, safeButton3);
    safeAttachment4 = std::make_unique<ButtonAttach> (processor.treeState, SAFE_ID4, safeButton4);
    
    extremeAttachment1 = std::make_unique<ButtonAttach> (processor.treeState, EXTREME_ID1, extremeButton1);
    extremeAttachment2 = std::make_unique<ButtonAttach> (processor.treeState, EXTREME_ID2, extremeButton2);
    extremeAttachment3 = std::make_unique<ButtonAttach> (processor.treeState, EXTREME_ID3, extremeButton3);
    extremeAttachment4 = std::make_unique<ButtonAttach> (processor.treeState, EXTREME_ID4, extremeButton4);

    widthAttachment1 = std::make_unique<SliderAttach> (processor.treeState, WIDTH_ID1, widthKnob1);
    widthAttachment2 = std::make_unique<SliderAttach> (processor.treeState, WIDTH_ID2, widthKnob2);
    widthAttachment3 = std::make_unique<SliderAttach> (processor.treeState, WIDTH_ID3, widthKnob3);
    widthAttachment4 = std::make_unique<SliderAttach> (processor.treeState, WIDTH_ID4, widthKnob4);

    compressorBypassAttachment1 = std::make_unique<ButtonAttach> (processor.treeState, COMP_BYPASS_ID1, *compressorBypassButton1);
    compressorBypassAttachment2 = std::make_unique<ButtonAttach> (processor.treeState, COMP_BYPASS_ID2, *compressorBypassButton2);
    compressorBypassAttachment3 = std::make_unique<ButtonAttach> (processor.treeState, COMP_BYPASS_ID3, *compressorBypassButton3);
    compressorBypassAttachment4 = std::make_unique<ButtonAttach> (processor.treeState, COMP_BYPASS_ID4, *compressorBypassButton4);
    widthBypassAttachment1 = std::make_unique<ButtonAttach> (processor.treeState, WIDTH_BYPASS_ID1, *widthBypassButton1);
    widthBypassAttachment2 = std::make_unique<ButtonAttach> (processor.treeState, WIDTH_BYPASS_ID2, *widthBypassButton2);
    widthBypassAttachment3 = std::make_unique<ButtonAttach> (processor.treeState, WIDTH_BYPASS_ID3, *widthBypassButton3);
    widthBypassAttachment4 = std::make_unique<ButtonAttach> (processor.treeState, WIDTH_BYPASS_ID4, *widthBypassButton4);

    // save xml state to temp
    compBypassTemp[0] = *processor.treeState.getRawParameterValue (COMP_BYPASS_ID1);
    compBypassTemp[1] = *processor.treeState.getRawParameterValue (COMP_BYPASS_ID2);
    compBypassTemp[2] = *processor.treeState.getRawParameterValue (COMP_BYPASS_ID3);
    compBypassTemp[3] = *processor.treeState.getRawParameterValue (COMP_BYPASS_ID4);

    widthBypassTemp[0] = *processor.treeState.getRawParameterValue (WIDTH_BYPASS_ID1);
    widthBypassTemp[1] = *processor.treeState.getRawParameterValue (WIDTH_BYPASS_ID2);
    widthBypassTemp[2] = *processor.treeState.getRawParameterValue (WIDTH_BYPASS_ID3);
    widthBypassTemp[3] = *processor.treeState.getRawParameterValue (WIDTH_BYPASS_ID4);

    // init band knobs states
    setBandKnobsStates (0, *processor.treeState.getRawParameterValue (BAND_ENABLE_ID1), false);
    setBandKnobsStates (1, *processor.treeState.getRawParameterValue (BAND_ENABLE_ID2), false);
    setBandKnobsStates (2, *processor.treeState.getRawParameterValue (BAND_ENABLE_ID3), false);
    setBandKnobsStates (3, *processor.treeState.getRawParameterValue (BAND_ENABLE_ID4), false);
}

BandPanel::~BandPanel()
{
    driveKnob1.setLookAndFeel (nullptr);
    driveKnob2.setLookAndFeel (nullptr);
    driveKnob3.setLookAndFeel (nullptr);
    driveKnob4.setLookAndFeel (nullptr);
    linkedButton1.setLookAndFeel (nullptr);
    linkedButton2.setLookAndFeel (nullptr);
    linkedButton3.setLookAndFeel (nullptr);
    linkedButton4.setLookAndFeel (nullptr);
    safeButton1.setLookAndFeel (nullptr);
    safeButton2.setLookAndFeel (nullptr);
    safeButton3.setLookAndFeel (nullptr);
    safeButton4.setLookAndFeel (nullptr);
    extremeButton1.setLookAndFeel (nullptr);
    extremeButton2.setLookAndFeel (nullptr);
    extremeButton3.setLookAndFeel (nullptr);
    extremeButton4.setLookAndFeel (nullptr);
    oscSwitch.setLookAndFeel (nullptr);
    shapeSwitch.setLookAndFeel (nullptr);
    widthSwitch.setLookAndFeel (nullptr);
    compressorSwitch.setLookAndFeel (nullptr);
    shapePanelLabel.setLookAndFeel (nullptr);
    compressorPanelLabel.setLookAndFeel (nullptr);
    widthPanelLabel.setLookAndFeel (nullptr);
}

void BandPanel::paint (juce::Graphics& g)
{
    setFourComponentsVisibility (driveKnob1, driveKnob2, driveKnob3, driveKnob4, focusBandNum);
    setFourComponentsVisibility (outputKnob1, outputKnob2, outputKnob3, outputKnob4, focusBandNum);
    setFourComponentsVisibility (mixKnob1, mixKnob2, mixKnob3, mixKnob4, focusBandNum);
    setFourComponentsVisibility (linkedButton1, linkedButton2, linkedButton3, linkedButton4, focusBandNum);
    setFourComponentsVisibility (safeButton1, safeButton2, safeButton3, safeButton4, focusBandNum);
    setFourComponentsVisibility (extremeButton1, extremeButton2, extremeButton3, extremeButton4, focusBandNum);

    g.setColour (COLOUR6);
    if (! oscSwitch.getToggleState())
    {
        g.drawRect (bandKnobArea);
        g.drawRect (driveKnobArea);
    }
    g.drawRect (outputKnobArea);

    // drive reduction
    driveLookAndFeel1.sampleMaxValue = processor.getSampleMaxValue (SAFE_ID1);
    driveLookAndFeel1.reductionPrecent = processor.getReductionPrecent (SAFE_ID1);
    driveLookAndFeel2.sampleMaxValue = processor.getSampleMaxValue (SAFE_ID2);
    driveLookAndFeel2.reductionPrecent = processor.getReductionPrecent (SAFE_ID2);
    driveLookAndFeel3.sampleMaxValue = processor.getSampleMaxValue (SAFE_ID3);
    driveLookAndFeel3.reductionPrecent = processor.getReductionPrecent (SAFE_ID3);
    driveLookAndFeel4.sampleMaxValue = processor.getSampleMaxValue (SAFE_ID4);
    driveLookAndFeel4.reductionPrecent = processor.getReductionPrecent (SAFE_ID4);
}

void BandPanel::resized()
{
    juce::Rectangle<int> controlArea = getLocalBounds();

    bandKnobArea = controlArea.removeFromLeft (getWidth() / 5 * 2);
    driveKnobArea = controlArea.removeFromLeft (getWidth() / 5);

    outputKnobArea = controlArea;
    driveKnobArea.removeFromTop (getHeight() / 5);
    driveKnobArea.removeFromBottom (getHeight() / 5);
    outputKnobArea.removeFromTop (getHeight() / 5);
    outputKnobArea.removeFromBottom (getHeight() / 5);
    bandKnobArea.removeFromTop (getHeight() / 5);
    bandKnobArea.removeFromBottom (getHeight() / 5);

    juce::Rectangle<int> switchArea = bandKnobArea.removeFromLeft (getWidth() / 50);
    oscSwitch.setBounds (switchArea.removeFromTop (bandKnobArea.getHeight() / 4));
    shapeSwitch.setBounds (switchArea.removeFromTop (bandKnobArea.getHeight() / 4));
    compressorSwitch.setBounds (switchArea.removeFromTop (bandKnobArea.getHeight() / 4));
    widthSwitch.setBounds (switchArea.removeFromTop (bandKnobArea.getHeight() / 4));

    juce::Rectangle<int> bigDriveArea = getLocalBounds().removeFromLeft (getWidth() / 5 * 3).reduced (getHeight() / 10);
    if (oscSwitch.getToggleState())
    {
        driveKnob1.setBounds (bigDriveArea);
        driveKnob2.setBounds (bigDriveArea);
        driveKnob3.setBounds (bigDriveArea);
        driveKnob4.setBounds (bigDriveArea);
    }
    else
    {
        driveKnob1.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
        driveKnob2.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
        driveKnob3.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
        driveKnob4.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
    }

    // shape
    juce::Rectangle<int> bandKnobAreaLeft = bandKnobArea;
    juce::Rectangle<int> bandKnobAreaRight = bandKnobAreaLeft.removeFromRight (bandKnobArea.getWidth() / 2);
    bandKnobAreaLeft = bandKnobAreaLeft.reduced (0, bandKnobAreaLeft.getHeight() / 5);
    bandKnobAreaRight = bandKnobAreaRight.reduced (0, bandKnobAreaRight.getHeight() / 5);
    recKnob1.setBounds (bandKnobAreaLeft);
    recKnob2.setBounds (bandKnobAreaLeft);
    recKnob3.setBounds (bandKnobAreaLeft);
    recKnob4.setBounds (bandKnobAreaLeft);

    biasKnob1.setBounds (bandKnobAreaRight);
    biasKnob2.setBounds (bandKnobAreaRight);
    biasKnob3.setBounds (bandKnobAreaRight);
    biasKnob4.setBounds (bandKnobAreaRight);

    // width
    juce::Rectangle<int> bypassButtonArea = bandKnobArea;
    bypassButtonArea = bypassButtonArea.removeFromBottom (bandKnobArea.getHeight() / 5).reduced (bandKnobArea.getWidth() / 2 - bandKnobArea.getHeight() / 10, 0);
    widthBypassButton1->setBounds (bypassButtonArea);
    widthBypassButton2->setBounds (bypassButtonArea);
    widthBypassButton3->setBounds (bypassButtonArea);
    widthBypassButton4->setBounds (bypassButtonArea);
    widthKnob1.setBounds (bandKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
    widthKnob2.setBounds (bandKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
    widthKnob3.setBounds (bandKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
    widthKnob4.setBounds (bandKnobArea.reduced (0, bandKnobArea.getHeight() / 5));

    // compressor
    compressorBypassButton1->setBounds (bypassButtonArea);
    compressorBypassButton2->setBounds (bypassButtonArea);
    compressorBypassButton3->setBounds (bypassButtonArea);
    compressorBypassButton4->setBounds (bypassButtonArea);
    compThreshKnob1.setBounds (bandKnobAreaLeft);
    compThreshKnob2.setBounds (bandKnobAreaLeft);
    compThreshKnob3.setBounds (bandKnobAreaLeft);
    compThreshKnob4.setBounds (bandKnobAreaLeft);

    compRatioKnob1.setBounds (bandKnobAreaRight);
    compRatioKnob2.setBounds (bandKnobAreaRight);
    compRatioKnob3.setBounds (bandKnobAreaRight);
    compRatioKnob4.setBounds (bandKnobAreaRight);
    
    // labels
    juce::Rectangle<int> panelLabelArea = bandKnobArea;
    panelLabelArea = panelLabelArea.removeFromLeft (bandKnobArea.getWidth() / 3);
    panelLabelArea = panelLabelArea.removeFromBottom (bandKnobArea.getHeight() / 5);
    shapePanelLabel.setBounds(panelLabelArea);
    compressorPanelLabel.setBounds(panelLabelArea);
    widthPanelLabel.setBounds(panelLabelArea);

    juce::Rectangle<int> outputKnobAreaLeft = outputKnobArea;
    juce::Rectangle<int> outputKnobAreaRight = outputKnobAreaLeft.removeFromRight (bandKnobArea.getWidth() / 2);
    outputKnobAreaLeft = outputKnobAreaLeft.reduced (0, outputKnobAreaLeft.getHeight() / 5);
    outputKnobAreaRight = outputKnobAreaRight.reduced (0, outputKnobAreaRight.getHeight() / 5);

    outputKnob1.setBounds (outputKnobAreaLeft);
    outputKnob2.setBounds (outputKnobAreaLeft);
    outputKnob3.setBounds (outputKnobAreaLeft);
    outputKnob4.setBounds (outputKnobAreaLeft);

    mixKnob1.setBounds (outputKnobAreaRight);
    mixKnob2.setBounds (outputKnobAreaRight);
    mixKnob3.setBounds (outputKnobAreaRight);
    mixKnob4.setBounds (outputKnobAreaRight);

    // buttons
    juce::Rectangle<int> outputButtonArea = outputKnobArea;
    outputButtonArea = outputButtonArea.removeFromLeft (getWidth() / 25);
    juce::Rectangle<int> outputButtonArea1 = outputButtonArea;
    juce::Rectangle<int> outputButtonArea2 = outputButtonArea1.removeFromBottom (outputButtonArea.getHeight() / 3 * 2);
    juce::Rectangle<int> outputButtonArea3 = outputButtonArea2.removeFromBottom (outputButtonArea.getHeight() / 3);
    linkedButton1.setBounds (outputButtonArea1);
    linkedButton2.setBounds (outputButtonArea1);
    linkedButton3.setBounds (outputButtonArea1);
    linkedButton4.setBounds (outputButtonArea1);

    safeButton1.setBounds (outputButtonArea2);
    safeButton2.setBounds (outputButtonArea2);
    safeButton3.setBounds (outputButtonArea2);
    safeButton4.setBounds (outputButtonArea2);
    
    extremeButton1.setBounds (outputButtonArea3);
    extremeButton2.setBounds (outputButtonArea3);
    extremeButton3.setBounds (outputButtonArea3);
    extremeButton4.setBounds (outputButtonArea3);

    // set look and feel scale
    driveLookAndFeel1.scale = scale;
    driveLookAndFeel2.scale = scale;
    driveLookAndFeel3.scale = scale;
    driveLookAndFeel4.scale = scale;
    flatLnf.scale = scale;
}

void BandPanel::sliderValueChanged (juce::Slider* slider)
{
    linkValue (*slider, driveKnob1, outputKnob1, linkedButton1);
    linkValue (*slider, driveKnob2, outputKnob2, linkedButton2);
    linkValue (*slider, driveKnob3, outputKnob3, linkedButton3);
    linkValue (*slider, driveKnob4, outputKnob4, linkedButton4);
}

void BandPanel::linkValue (juce::Slider& xSlider, juce::Slider& driveSlider, juce::Slider& outputSlider, juce::TextButton& linkedButton)
{
    // x changes, then y will change
    if (linkedButton.getToggleState() == true)
    {
        if (&xSlider == &driveSlider)
        {
            outputSlider.setValue (-xSlider.getValue() * 0.1f); // use defalut notification type
        }
    }
}

void BandPanel::comboBoxChanged (juce::ComboBox* combobox)
{
}

void BandPanel::timerCallback()
{
}

void BandPanel::updateBypassState (juce::ToggleButton& clickedButton, int index)
{
    bool state = clickedButton.getToggleState();
    setBypassState (index, state);
}

void BandPanel::buttonClicked (juce::Button* clickedButton)
{
    juce::Rectangle<int> bigDriveArea = getLocalBounds().removeFromLeft (getWidth() / 5 * 3).reduced (getHeight() / 10);

    if (clickedButton == &oscSwitch)
    {
        if (oscSwitch.getToggleState())
        {
            driveKnob1.setBounds (bigDriveArea);
            driveKnob2.setBounds (bigDriveArea);
            driveKnob3.setBounds (bigDriveArea);
            driveKnob4.setBounds (bigDriveArea);
            setVisibility (shapeVector, false);
            setVisibility (compressorVector, false);
            setVisibility (widthVector, false);
            recLabel.setVisible (false);
            biasLabel.setVisible (false);
            widthLabel.setVisible (false);
            CompRatioLabel.setVisible (false);
            CompThreshLabel.setVisible (false);
        }
        else
        {
            driveKnob1.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
            driveKnob2.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
            driveKnob3.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
            driveKnob4.setBounds (driveKnobArea.reduced (0, bandKnobArea.getHeight() / 5));
        }
    }
    if (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
    {
        setVisibility(shapeVector, true);
        setFourComponentsVisibility (recKnob1, recKnob2, recKnob3, recKnob4, focusBandNum);
        setFourComponentsVisibility (biasKnob1, biasKnob2, biasKnob3, biasKnob4, focusBandNum);
        setVisibility (compressorVector, false);
        setVisibility (widthVector, false);
        CompRatioLabel.setVisible (false);
        CompThreshLabel.setVisible (false);
        recLabel.setVisible (true);
        biasLabel.setVisible (true);
        widthLabel.setVisible (false);
    }
    if (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
    {
        setVisibility (compressorVector, true);
        setFourComponentsVisibility (compRatioKnob1, compRatioKnob2, compRatioKnob3, compRatioKnob4, focusBandNum);
        setFourComponentsVisibility (compThreshKnob1, compThreshKnob2, compThreshKnob3, compThreshKnob4, focusBandNum);
        setFourComponentsVisibility (*compressorBypassButton1, *compressorBypassButton2, *compressorBypassButton3, *compressorBypassButton4, focusBandNum);
        setVisibility (shapeVector, false);
        setVisibility (widthVector, false);
        CompRatioLabel.setVisible (true);
        CompThreshLabel.setVisible (true);
        recLabel.setVisible (false);
        biasLabel.setVisible (false);
        widthLabel.setVisible (false);
    }
    if (clickedButton == &widthSwitch && widthSwitch.getToggleState())
    {
        setVisibility (widthVector, true);
        setFourComponentsVisibility (widthKnob1, widthKnob2, widthKnob3, widthKnob4, focusBandNum); // put after setVisibility
        setFourComponentsVisibility (*widthBypassButton1, *widthBypassButton2, *widthBypassButton3, *widthBypassButton4, focusBandNum);
        setVisibility (shapeVector, false);
        setVisibility (compressorVector, false);
        CompRatioLabel.setVisible (false);
        CompThreshLabel.setVisible (false);
        recLabel.setVisible (false);
        biasLabel.setVisible (false);
        widthLabel.setVisible (true);
    }
    repaint();
}

void BandPanel::initListenerKnob (juce::Slider& slider)
{
    addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.addListener (this);
}

void BandPanel::initRotarySlider (juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
}

void BandPanel::initFlatButton (juce::TextButton& button, juce::String paramId, juce::String buttonName)
{
    addAndMakeVisible (button);
    button.setClickingTogglesState (true);
    //    bool state = processor.treeState.getRawParameterValue(paramId);
    //    bool state = true;
    //    button.setToggleState(state, juce::dontSendNotification);
    button.setColour (juce::TextButton::buttonColourId, COLOUR7);
    button.setColour (juce::TextButton::buttonOnColourId, COLOUR6.withBrightness (0.1f));
    button.setColour (juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour (juce::TextButton::textColourOnId, COLOUR1);
    button.setColour (juce::TextButton::textColourOffId, COLOUR7.withBrightness (0.8f));
    button.setButtonText (buttonName);
    button.setLookAndFeel (&flatLnf);
}

void BandPanel::initBypassButton (juce::ToggleButton& bypassButton, juce::Colour colour, int index)
{
    addAndMakeVisible (bypassButton);
    bypassButton.setColour (juce::ToggleButton::tickColourId, colour);
    bypassButton.onClick = [this, &bypassButton, index]
    { updateBypassState (bypassButton, index); };
    updateBypassState (bypassButton, index);
    bypassButton.addListener (this);
}

void BandPanel::setFourComponentsVisibility (juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum)
{
    driveLabel.setVisible (true);
    mixLabel.setVisible (true);
    outputLabel.setVisible (true);
    if (bandNum == 0)
    {
        component1.setVisible (true);
        component2.setVisible (false);
        component3.setVisible (false);
        component4.setVisible (false);
    }
    else if (bandNum == 1)
    {
        component1.setVisible (false);
        component2.setVisible (true);
        component3.setVisible (false);
        component4.setVisible (false);
    }
    else if (bandNum == 2)
    {
        component1.setVisible (false);
        component2.setVisible (false);
        component3.setVisible (true);
        component4.setVisible (false);
    }
    else if (bandNum == 3)
    {
        component1.setVisible (false);
        component2.setVisible (false);
        component3.setVisible (false);
        component4.setVisible (true);
    }
}

void BandPanel::setFocusBandNum (int num)
{
    focusBandNum = num;
}

void BandPanel::setScale (float scale)
{
    this->scale = scale;
}

void BandPanel::setVisibility (juce::Array<juce::Component*>& array, bool isVisible)
{
    for (int i = 0; i < array.size(); ++i)
    {
        if (isVisible)
        {
            array[i]->setVisible (true);
        }
        else
        {
            array[i]->setVisible (false);
        }
    }
}

void BandPanel::setBypassState (int index, bool state)
{
    componentArray1 = { &compThreshKnob1, &compRatioKnob1 };
    componentArray2 = { &compThreshKnob2, &compRatioKnob2 };
    componentArray3 = { &compThreshKnob3, &compRatioKnob3 };
    componentArray4 = { &compThreshKnob4, &compRatioKnob4 };
    componentArray5 = { &widthKnob1 };
    componentArray6 = { &widthKnob2 };
    componentArray7 = { &widthKnob3 };
    componentArray8 = { &widthKnob4 };

    juce::Array<juce::Component*>* componentArray;
    // 0-3 -> comp 1-4
    // 4-7 -> width 1-4
    if (index == 0)
        componentArray = &componentArray1;
    if (index == 1)
        componentArray = &componentArray2;
    if (index == 2)
        componentArray = &componentArray3;
    if (index == 3)
        componentArray = &componentArray4;
    if (index == 4)
        componentArray = &componentArray5;
    if (index == 5)
        componentArray = &componentArray6;
    if (index == 6)
        componentArray = &componentArray7;
    if (index == 7)
        componentArray = &componentArray8;

    // when changing subbypass buttons, also save the states to temp arrays
    if (index >= 0 && index < 4)
    {
        compBypassTemp[index] = state;
    }
    else if (index >= 4 && index < 8)
    {
        widthBypassTemp[index - 4] = state;
    }
    // set subknobs states
    for (int i = 0; i < componentArray->size(); i++)
    {
        componentArray->data()[i]->setEnabled (state);
    }
}

void BandPanel::setBandKnobsStates (int index, bool state, bool callFromSubBypass)
{
    componentArray1 = { &driveKnob1, &outputKnob1, &mixKnob1, &recKnob1, &biasKnob1, &compThreshKnob1, &compRatioKnob1, &widthKnob1, &linkedButton1, &safeButton1, &extremeButton1 };
    componentArray2 = { &driveKnob2, &outputKnob2, &mixKnob2, &recKnob2, &biasKnob2, &compThreshKnob2, &compRatioKnob2, &widthKnob2, &linkedButton2, &safeButton2, &extremeButton2 };
    componentArray3 = { &driveKnob3, &outputKnob3, &mixKnob3, &recKnob3, &biasKnob3, &compThreshKnob3, &compRatioKnob3, &widthKnob3, &linkedButton3, &safeButton3, &extremeButton3 };
    componentArray4 = { &driveKnob4, &outputKnob4, &mixKnob4, &recKnob4, &biasKnob4, &compThreshKnob4, &compRatioKnob4, &widthKnob4, &linkedButton4, &safeButton4, &extremeButton4 };

    bool widthBypassState, compBypassState;
    // turn off bypassbuttons, subbypass buttons will also turn off
    if (! state)
    {
        widthBypassState = false;
        compBypassState = false;
    }
    else // turn on bypassbuttons, subbypass buttons states will depends on original states(temp)
    {
        widthBypassState = widthBypassTemp[index];
        compBypassState = compBypassTemp[index];
    }

    juce::Array<juce::Component*>* componentArray;
    if (index == 0)
    {
        componentArray = &componentArray1;
        if (! callFromSubBypass)
        {
            widthBypassButton1->setToggleState (widthBypassState, juce::NotificationType::dontSendNotification);
            compressorBypassButton1->setToggleState (compBypassState, juce::NotificationType::dontSendNotification);
        }
    }
    else if (index == 1)
    {
        componentArray = &componentArray2;
        if (! callFromSubBypass)
        {
            widthBypassButton2->setToggleState (widthBypassState, juce::NotificationType::dontSendNotification);
            compressorBypassButton2->setToggleState (compBypassState, juce::NotificationType::dontSendNotification);
        }
    }
    else if (index == 2)
    {
        componentArray = &componentArray3;
        if (! callFromSubBypass)
        {
            widthBypassButton3->setToggleState (widthBypassState, juce::NotificationType::dontSendNotification);
            compressorBypassButton3->setToggleState (compBypassState, juce::NotificationType::dontSendNotification);
        }
    }
    else if (index == 3)
    {
        componentArray = &componentArray4;
        if (! callFromSubBypass)
        {
            widthBypassButton4->setToggleState (widthBypassState, juce::NotificationType::dontSendNotification);
            compressorBypassButton4->setToggleState (compBypassState, juce::NotificationType::dontSendNotification);
        }
    }

    if (state)
    {
        for (int i = 0; i < componentArray->size(); i++)
        {
            if (canEnableSubKnobs (*componentArray->data()[i]))
            {
                continue;
            }
            componentArray->data()[i]->setEnabled (true);
        }
    }
    else
    {
        for (int i = 0; i < componentArray->size(); i++)
        {
            componentArray->data()[i]->setEnabled (false);
        }
    }
}

bool BandPanel::canEnableSubKnobs (juce::Component& component)
{
    if (&component == &compThreshKnob1 && ! compressorBypassButton1->getToggleState())
        return true;
    else if (&component == &compRatioKnob1 && ! compressorBypassButton1->getToggleState())
        return true;
    else if (&component == &compThreshKnob2 && ! compressorBypassButton2->getToggleState())
        return true;
    else if (&component == &compRatioKnob2 && ! compressorBypassButton2->getToggleState())
        return true;
    else if (&component == &compThreshKnob3 && ! compressorBypassButton3->getToggleState())
        return true;
    else if (&component == &compRatioKnob3 && ! compressorBypassButton3->getToggleState())
        return true;
    else if (&component == &compThreshKnob4 && ! compressorBypassButton4->getToggleState())
        return true;
    else if (&component == &compRatioKnob4 && ! compressorBypassButton4->getToggleState())
        return true;
    else if (&component == &widthKnob1 && ! widthBypassButton1->getToggleState())
        return true;
    else if (&component == &widthKnob2 && ! widthBypassButton2->getToggleState())
        return true;
    else if (&component == &widthKnob3 && ! widthBypassButton3->getToggleState())
        return true;
    else if (&component == &widthKnob4 && ! widthBypassButton4->getToggleState())
        return true;
    else
        return false;
}

juce::ToggleButton& BandPanel::getCompButton (const int index)
{
    if (index == 0)
        return *compressorBypassButton1;
    else if (index == 1)
        return *compressorBypassButton2;
    else if (index == 2)
        return *compressorBypassButton3;
    else if (index == 3)
        return *compressorBypassButton4;
    else
        jassertfalse;
    return *compressorBypassButton1;
}

juce::ToggleButton& BandPanel::getWidthButton (const int index)
{
    if (index == 0)
        return *widthBypassButton1;
    else if (index == 1)
        return *widthBypassButton2;
    else if (index == 2)
        return *widthBypassButton3;
    else if (index == 3)
        return *widthBypassButton4;
    else
        jassertfalse;
    return *compressorBypassButton1;
}

void BandPanel::setSwitch(const int index, bool state)
{
    if (index == 0)
        oscSwitch.setToggleState(state, juce::sendNotificationSync);
    else if (index == 1)
        shapeSwitch.setToggleState(state, juce::sendNotificationSync);
    else if (index == 2)
        compressorSwitch.setToggleState(state, juce::sendNotificationSync);
    else if (index == 3)
        widthSwitch.setToggleState(state, juce::sendNotificationSync);
}

void BandPanel::updateWhenChangingFocus()
{

    if (oscSwitch.getToggleState())
    {
        setVisibility (shapeVector, false);
        setVisibility (compressorVector, false);
        setVisibility (widthVector, false);
        recLabel.setVisible (false);
        biasLabel.setVisible (false);
        widthLabel.setVisible (false);
        CompRatioLabel.setVisible (false);
        CompThreshLabel.setVisible (false);
    }

    if (shapeSwitch.getToggleState())
    {
        setFourComponentsVisibility (recKnob1, recKnob2, recKnob3, recKnob4, focusBandNum);
        setFourComponentsVisibility (biasKnob1, biasKnob2, biasKnob3, biasKnob4, focusBandNum);
        setVisibility (compressorVector, false);
        setVisibility (widthVector, false);
        CompRatioLabel.setVisible (false);
        CompThreshLabel.setVisible (false);
        recLabel.setVisible (true);
        biasLabel.setVisible (true);
        widthLabel.setVisible (false);
    }

    if (compressorSwitch.getToggleState())
    {
        setVisibility (compressorVector, true);
        setFourComponentsVisibility (compRatioKnob1, compRatioKnob2, compRatioKnob3, compRatioKnob4, focusBandNum);
        setFourComponentsVisibility (compThreshKnob1, compThreshKnob2, compThreshKnob3, compThreshKnob4, focusBandNum);
        setFourComponentsVisibility (*compressorBypassButton1, *compressorBypassButton2, *compressorBypassButton3, *compressorBypassButton4, focusBandNum);
        setVisibility (shapeVector, false);
        setVisibility (widthVector, false);
        CompRatioLabel.setVisible (true);
        CompThreshLabel.setVisible (true);
        recLabel.setVisible (false);
        biasLabel.setVisible (false);
        widthLabel.setVisible (false);
    }

    if (widthSwitch.getToggleState())
    {
        setVisibility (widthVector, true);
        setFourComponentsVisibility (widthKnob1, widthKnob2, widthKnob3, widthKnob4, focusBandNum); // put after setVisibility
        setFourComponentsVisibility (*widthBypassButton1, *widthBypassButton2, *widthBypassButton3, *widthBypassButton4, focusBandNum);
        setVisibility (shapeVector, false);
        setVisibility (compressorVector, false);
        CompRatioLabel.setVisible (false);
        CompThreshLabel.setVisible (false);
        recLabel.setVisible (false);
        biasLabel.setVisible (false);
        widthLabel.setVisible (true);
    }
    repaint();
}
