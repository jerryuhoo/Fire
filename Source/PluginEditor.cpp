/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "[Early Beta] 0.792"


//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p), stateComponent{p.stateAB, p.statePresets, multiband}
{
    // timer
    juce::Timer::startTimerHz(30.0f);

    // set resize
    setResizable(true, true);

    // Visualiser
    addAndMakeVisible(oscilloscope);
    
    // Distortion graph
    addAndMakeVisible(distortionGraph);
    
    // Width graph
    addAndMakeVisible(widthGraph);
    
    // Spectrum
    addAndMakeVisible(spectrum);
    addAndMakeVisible(multiband);
    
    spectrum.setInterceptsMouseClicks(false, false);
    spectrum.prepareToPaintSpectrum(processor.getFFTSize(), processor.getFFTData());
    
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

    // drive knob 1
    addAndMakeVisible(driveKnob1);
    driveKnob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveKnob1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    driveKnob1.addListener(this);
    
    addAndMakeVisible(driveKnob2);
    driveKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveKnob2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    driveKnob2.addListener(this);
    
    addAndMakeVisible(driveKnob3);
    driveKnob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveKnob3.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    driveKnob3.addListener(this);
    
    addAndMakeVisible(driveKnob4);
    driveKnob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveKnob4.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    driveKnob4.addListener(this);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    driveLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    driveLabel.attachToComponent(&driveKnob1, false);
    driveLabel.setJustificationType(juce::Justification::centred);

    
    // output knob
    addAndMakeVisible(outputKnob1);
    outputKnob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    outputKnob1.addListener(this);

    addAndMakeVisible(outputKnob2);
    outputKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    outputKnob2.addListener(this);
    
    addAndMakeVisible(outputKnob3);
    outputKnob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob3.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    outputKnob3.addListener(this);
    
    addAndMakeVisible(outputKnob4);
    outputKnob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob4.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    outputKnob4.addListener(this);
    
    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent(&outputKnob1, false);
    outputLabel.setJustificationType(juce::Justification::centred);
    
    // mix knob
    addAndMakeVisible(mixKnob1);
    mixKnob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(mixKnob2);
    mixKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(mixKnob3);
    mixKnob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob3.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(mixKnob4);
    mixKnob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob4.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent(&mixKnob1, false);
    mixLabel.setJustificationType(juce::Justification::centred);
    
    // dynamic knob
    addAndMakeVisible(dynamicKnob1);
    dynamicKnob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dynamicKnob1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(dynamicKnob2);
    dynamicKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dynamicKnob2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(dynamicKnob3);
    dynamicKnob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dynamicKnob3.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(dynamicKnob4);
    dynamicKnob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dynamicKnob4.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(dynamicLabel);
    dynamicLabel.setText("Dynamic", juce::dontSendNotification);
    dynamicLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    dynamicLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    dynamicLabel.attachToComponent(&dynamicKnob1, false);
    dynamicLabel.setJustificationType(juce::Justification::centred);
    
    // width knob
    addAndMakeVisible(widthKnob1);
    widthKnob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    widthKnob1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(widthKnob2);
    widthKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    widthKnob2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(widthKnob3);
    widthKnob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    widthKnob3.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(widthKnob4);
    widthKnob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    widthKnob4.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(widthLabel);
    widthLabel.setText("Width", juce::dontSendNotification);
    widthLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    widthLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    widthLabel.attachToComponent(&widthKnob1, false);
    widthLabel.setJustificationType(juce::Justification::centred);
    
    // color knob
    addAndMakeVisible(colorKnob);
    colorKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    colorKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(colorLabel);
    colorLabel.setText("Color", juce::dontSendNotification);
    colorLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    colorLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    colorLabel.attachToComponent(&colorKnob, false);
    colorLabel.setJustificationType(juce::Justification::centred);

    // bias knob
    addAndMakeVisible(biasKnob1);
    biasKnob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    biasKnob1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(biasKnob2);
    biasKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    biasKnob2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(biasKnob3);
    biasKnob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    biasKnob3.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(biasKnob4);
    biasKnob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    biasKnob4.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(biasLabel);
    biasLabel.setText("Bias", juce::dontSendNotification);
    biasLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    biasLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    biasLabel.attachToComponent(&biasKnob1, false);
    biasLabel.setJustificationType(juce::Justification::centred);

    // downsample knob
    addAndMakeVisible(downSampleKnob);
    downSampleKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    downSampleKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", juce::dontSendNotification);
    downSampleLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    downSampleLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(juce::Justification::centred);

    

    // rec knob
    addAndMakeVisible(recKnob1);
    recKnob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    recKnob1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(recKnob2);
    recKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    recKnob2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(recKnob3);
    recKnob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    recKnob3.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    
    addAndMakeVisible(recKnob4);
    recKnob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    recKnob4.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(recLabel);
    recLabel.setText("Rectification", juce::dontSendNotification);
    recLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    recLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    recLabel.attachToComponent(&recKnob1, false);
    recLabel.setJustificationType(juce::Justification::centred);

    

    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    cutoffLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    cutoffLabel.attachToComponent(&cutoffKnob, false);
    cutoffLabel.setJustificationType(juce::Justification::centred);

    // res knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);

    addAndMakeVisible(resLabel);
    resLabel.setText("Q", juce::dontSendNotification); // Resonance
    resLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    resLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    resLabel.attachToComponent(&resKnob, false);
    resLabel.setJustificationType(juce::Justification::centred);

    // HQ(oversampling) Button
    addAndMakeVisible(hqButton);
    hqButton.setClickingTogglesState(true);
    bool hqButtonState = *processor.treeState.getRawParameterValue("hq");
    hqButton.setToggleState(hqButtonState, juce::dontSendNotification);
    hqButton.setColour(juce::TextButton::buttonColourId, COLOUR5);
    hqButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    hqButton.setColour(juce::ComboBox::outlineColourId, COLOUR5);
    hqButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    hqButton.setColour(juce::TextButton::textColourOffId, juce::Colour(100, 20, 20));
    hqButton.setButtonText("HQ");

    // Linked Button
    addAndMakeVisible(linkedButton1);
    linkedButton1.setClickingTogglesState(true);
    bool linkedButtonState1 = *processor.treeState.getRawParameterValue("linked1");
    linkedButton1.setToggleState(linkedButtonState1, juce::dontSendNotification);
    linkedButton1.setColour(juce::TextButton::buttonColourId, COLOUR6);
    linkedButton1.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    linkedButton1.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    linkedButton1.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    linkedButton1.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    linkedButton1.setButtonText("LINK");
    linkedButton1.setLookAndFeel(&roundedButtonLnf);

    addAndMakeVisible(linkedButton2);
    linkedButton2.setClickingTogglesState(true);
    bool linkedButtonState2 = *processor.treeState.getRawParameterValue("linked2");
    linkedButton2.setToggleState(linkedButtonState2, juce::dontSendNotification);
    linkedButton2.setColour(juce::TextButton::buttonColourId, COLOUR6);
    linkedButton2.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    linkedButton2.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    linkedButton2.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    linkedButton2.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    linkedButton2.setButtonText("LINK");
    linkedButton2.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(linkedButton3);
    linkedButton3.setClickingTogglesState(true);
    bool linkedButtonState3 = *processor.treeState.getRawParameterValue("linked3");
    linkedButton3.setToggleState(linkedButtonState3, juce::dontSendNotification);
    linkedButton3.setColour(juce::TextButton::buttonColourId, COLOUR6);
    linkedButton3.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    linkedButton3.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    linkedButton3.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    linkedButton3.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    linkedButton3.setButtonText("LINK");
    linkedButton3.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(linkedButton4);
    linkedButton4.setClickingTogglesState(true);
    bool linkedButtonState4 = *processor.treeState.getRawParameterValue("linked4");
    linkedButton4.setToggleState(linkedButtonState4, juce::dontSendNotification);
    linkedButton4.setColour(juce::TextButton::buttonColourId, COLOUR6);
    linkedButton4.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    linkedButton4.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    linkedButton4.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    linkedButton4.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    linkedButton4.setButtonText("LINK");
    linkedButton4.setLookAndFeel(&roundedButtonLnf);
    
    // safe overload Button
    addAndMakeVisible(safeButton1);
    safeButton1.setClickingTogglesState(true);
    bool safeButtonState1 = *processor.treeState.getRawParameterValue("safe1");
    safeButton1.setToggleState(safeButtonState1, juce::dontSendNotification);
    safeButton1.setColour(juce::TextButton::buttonColourId, COLOUR6);
    safeButton1.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    safeButton1.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    safeButton1.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    safeButton1.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    safeButton1.setButtonText("SAFE");
    safeButton1.setLookAndFeel(&roundedButtonLnf);

    addAndMakeVisible(safeButton2);
    safeButton2.setClickingTogglesState(true);
    bool safeButtonState2 = *processor.treeState.getRawParameterValue("safe2");
    safeButton2.setToggleState(safeButtonState2, juce::dontSendNotification);
    safeButton2.setColour(juce::TextButton::buttonColourId, COLOUR6);
    safeButton2.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    safeButton2.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    safeButton2.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    safeButton2.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    safeButton2.setButtonText("SAFE");
    safeButton2.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(safeButton3);
    safeButton3.setClickingTogglesState(true);
    bool safeButtonState3 = *processor.treeState.getRawParameterValue("safe3");
    safeButton3.setToggleState(safeButtonState3, juce::dontSendNotification);
    safeButton3.setColour(juce::TextButton::buttonColourId, COLOUR6);
    safeButton3.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    safeButton3.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    safeButton3.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    safeButton3.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    safeButton3.setButtonText("SAFE");
    safeButton3.setLookAndFeel(&roundedButtonLnf);
    
    addAndMakeVisible(safeButton4);
    safeButton4.setClickingTogglesState(true);
    bool safeButtonState4 = *processor.treeState.getRawParameterValue("safe4");
    safeButton4.setToggleState(safeButtonState4, juce::dontSendNotification);
    safeButton4.setColour(juce::TextButton::buttonColourId, COLOUR6);
    safeButton4.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    safeButton4.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    safeButton4.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    safeButton4.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    safeButton4.setButtonText("SAFE");
    safeButton4.setLookAndFeel(&roundedButtonLnf);
    
    // Filter State Buttons
    addAndMakeVisible(filterOffButton);
    filterOffButton.setClickingTogglesState(true);
    filterOffButton.setRadioGroupId(filterStateButtons);
    bool filterOffButtonState = *processor.treeState.getRawParameterValue("off");
    filterOffButton.setToggleState(filterOffButtonState, juce::dontSendNotification);
    filterOffButton.onClick = [this] { updateToggleState(); };
    filterOffButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    filterOffButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    filterOffButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterOffButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterOffButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterOffButton.setButtonText("OFF");
    filterOffButton.setLookAndFeel(&roundedButtonLnf);

    addAndMakeVisible(filterOffLabel);
    filterOffLabel.setText("Filter", juce::dontSendNotification);
    filterOffLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    filterOffLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    filterOffLabel.attachToComponent(&filterOffButton, false);
    filterOffLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(filterPreButton);
    filterPreButton.setClickingTogglesState(true);
    filterPreButton.setRadioGroupId(filterStateButtons);
    bool filterPreButtonState = *processor.treeState.getRawParameterValue("pre");
    filterPreButton.setToggleState(filterPreButtonState, juce::dontSendNotification);
    filterPreButton.onClick = [this] { updateToggleState(); };
    filterPreButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    filterPreButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    filterPreButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterPreButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterPreButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterPreButton.setButtonText("PRE");
    filterPreButton.setLookAndFeel(&roundedButtonLnf);

    addAndMakeVisible(filterPostButton);
    filterPostButton.setClickingTogglesState(true);
    filterPostButton.setRadioGroupId(filterStateButtons);
    bool filterPostButtonState = *processor.treeState.getRawParameterValue("post");
    filterPostButton.setToggleState(filterPostButtonState, juce::dontSendNotification);
    filterPostButton.onClick = [this] { updateToggleState(); };
    filterPostButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    filterPostButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    filterPostButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterPostButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterPostButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterPostButton.setButtonText("POST");
    filterPostButton.setLookAndFeel(&roundedButtonLnf);

    // Filter Type Toggle Buttons
    addAndMakeVisible(filterLowButton);
    filterLowButton.setClickingTogglesState(true);
    filterLowButton.setRadioGroupId(filterModeButtons);
    bool filterLowButtonState = *processor.treeState.getRawParameterValue("low");
    filterLowButton.setToggleState(filterLowButtonState, juce::dontSendNotification);
    filterLowButton.onClick = [this] { updateToggleState(); };
    filterLowButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    filterLowButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    filterLowButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterLowButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterLowButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterLowButton.setButtonText("LP");
    filterLowButton.setLookAndFeel(&roundedButtonLnf);

    addAndMakeVisible(filterLowLabel);
    filterLowLabel.setText("Type", juce::dontSendNotification);
    filterLowLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    filterLowLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    filterLowLabel.attachToComponent(&filterLowButton, false);
    filterLowLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(filterBandButton);
    filterBandButton.setClickingTogglesState(true);
    filterBandButton.setRadioGroupId(filterModeButtons);
    bool filterBandButtonState = *processor.treeState.getRawParameterValue("band");
    filterBandButton.setToggleState(filterBandButtonState, juce::dontSendNotification);
    filterBandButton.onClick = [this] { updateToggleState(); };
    filterBandButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    filterBandButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    filterBandButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterBandButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterBandButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterBandButton.setButtonText("BP");
    filterBandButton.setLookAndFeel(&roundedButtonLnf);

    addAndMakeVisible(filterHighButton);
    filterHighButton.setClickingTogglesState(true);
    filterHighButton.setRadioGroupId(filterModeButtons);
    bool filterHighButtonState = *processor.treeState.getRawParameterValue("high");
    filterHighButton.setToggleState(filterHighButtonState, juce::dontSendNotification);
    filterHighButton.onClick = [this] { updateToggleState(); };
    filterHighButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    filterHighButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    filterHighButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    filterHighButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    filterHighButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    filterHighButton.setButtonText("HP");
    filterHighButton.setLookAndFeel(&roundedButtonLnf);
    
    // Window Left Button
    addAndMakeVisible(windowLeftButton);
    windowLeftButton.setClickingTogglesState(true);
    windowLeftButton.setRadioGroupId(windowButtons);
    windowLeftButton.setButtonText("Band Effect");
    bool windowLeftButtonState = *processor.treeState.getRawParameterValue("windowLeft");
    windowLeftButton.setToggleState(windowLeftButtonState, juce::dontSendNotification);
    windowLeftButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowLeftButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowLeftButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.f));
    windowLeftButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowLeftButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    //windowLeftButton.setButtonText("1");
    windowLeftButton.setLookAndFeel(&otherLookAndFeel);
    
    // Window Right Button
    addAndMakeVisible(windowRightButton);
    windowRightButton.setClickingTogglesState(true);
    windowRightButton.setRadioGroupId(windowButtons);
    windowRightButton.setButtonText("Global Effect");
    bool windowRightButtonState = *processor.treeState.getRawParameterValue("windowRight");
    windowRightButton.setToggleState(windowRightButtonState, juce::dontSendNotification);
    windowRightButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowRightButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowRightButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.f));
    windowRightButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowRightButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    windowRightButton.setLookAndFeel(&otherLookAndFeel);
    
    // Attachment
    //inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    driveAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID1, driveKnob1);
    driveAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID2, driveKnob2);
    driveAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID3, driveKnob3);
    driveAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DRIVE_ID4, driveKnob4);
    
    dynamicAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID1, dynamicKnob1);
    dynamicAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID2, dynamicKnob2);
    dynamicAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID3, dynamicKnob3);
    dynamicAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DYNAMIC_ID4, dynamicKnob4);
    
    outputAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID1, outputKnob1);
    outputAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID2, outputKnob2);
    outputAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID3, outputKnob3);
    outputAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, OUTPUT_ID4, outputKnob4);
    
    mixAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID1, mixKnob1);
    mixAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID2, mixKnob2);
    mixAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID3, mixKnob3);
    mixAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID4, mixKnob4);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, MIX_ID, mixKnob);
    
    biasAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID1, biasKnob1);
    biasAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID2, biasKnob2);
    biasAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID3, biasKnob3);
    biasAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BIAS_ID4, biasKnob4);
    
    recAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID1, recKnob1);
    recAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID2, recKnob2);
    recAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID3, recKnob3);
    recAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, REC_ID4, recKnob4);
    
    colorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, COLOR_ID, colorKnob);
    
    downSampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, DOWNSAMPLE_ID, downSampleKnob);

    
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, CUTOFF_ID, cutoffKnob);
    resAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, RES_ID, resKnob);

    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HQ_ID, hqButton);
    
    linkedAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID1, linkedButton1);
    linkedAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID2, linkedButton2);
    linkedAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID3, linkedButton3);
    linkedAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LINKED_ID4, linkedButton4);
    
    safeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID1, safeButton1);
    safeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID2, safeButton2);
    safeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID3, safeButton3);
    safeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, SAFE_ID4, safeButton4);
    
    widthAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID1, widthKnob1);
    widthAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID2, widthKnob2);
    widthAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID3, widthKnob3);
    widthAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, WIDTH_ID4, widthKnob4);
    
    filterOffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, OFF_ID, filterOffButton);
    filterPreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, PRE_ID, filterPreButton);
    filterPostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, POST_ID, filterPostButton);
    filterLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, LOW_ID, filterLowButton);
    filterBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ID, filterBandButton);
    filterHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HIGH_ID, filterHighButton);
    
    windowLeftButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, WINDOW_LEFT_ID, windowLeftButton);
    windowRightButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, WINDOW_RIGHT_ID, windowRightButton);

    
    multiband.getFocusArray(multibandFocus);
    multiFocusSlider1.setValue(multibandFocus[0]);
    multiFocusSlider2.setValue(multibandFocus[1]);
    multiFocusSlider3.setValue(multibandFocus[2]);
    multiFocusSlider4.setValue(multibandFocus[3]);
    
    multiband.getStateArray(multibandState);
    multiStateSlider1.setValue(multibandState[0]);
    multiStateSlider2.setValue(multibandState[1]);
    multiStateSlider3.setValue(multibandState[2]);
    multiStateSlider4.setValue(multibandState[3]);
    
    multiband.getFreqArray(multibandFreq);
    multiFreqSlider1.setValue(multibandFreq[0]);
    multiFreqSlider2.setValue(multibandFreq[1]);
    multiFreqSlider3.setValue(multibandFreq[2]);
    
    int lineNum = multiband.getLineNum();
    lineNumSlider.setValue(lineNum);
    
    multiFocusAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID1, multiFocusSlider1);
    multiFocusAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID2, multiFocusSlider2);
    multiFocusAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID3, multiFocusSlider3);
    multiFocusAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID4, multiFocusSlider4);
    
    multiStateAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID1, multiStateSlider1);
    multiStateAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID2, multiStateSlider2);
    multiStateAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID3, multiStateSlider3);
    multiStateAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_STATE_ID4, multiStateSlider4);
    
    multiFreqAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID1, multiFreqSlider1);
    multiFreqAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID2, multiFreqSlider2);
    multiFreqAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID3, multiFreqSlider3);
    
    lineNumSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINENUM_ID, lineNumSlider);
    
    
    // Distortion mode select
    addAndMakeVisible(distortionMode1);

    distortionMode1.addItem("None", 1);
    distortionMode1.addSeparator();

    distortionMode1.addSectionHeading("Soft Clipping");
    distortionMode1.addItem("Arctan Soft Clipping", 2);
    distortionMode1.addItem("Exp Soft Clipping", 3);
    distortionMode1.addItem("Tanh Soft Clipping", 4);
    distortionMode1.addItem("Cubic Soft Clipping", 5);
    distortionMode1.addSeparator();

    distortionMode1.addSectionHeading("Hard Clipping");
    distortionMode1.addItem("Hard Clipping", 6);
    distortionMode1.addItem("Sausage Fattener", 7);
    distortionMode1.addSeparator();

    distortionMode1.addSectionHeading("Foldback");
    distortionMode1.addItem("Sin Foldback", 8);
    distortionMode1.addItem("Linear Foldback", 9);
    distortionMode1.addSeparator();

    distortionMode1.addSectionHeading("Asymmetrical Clipping");
    distortionMode1.addItem("Diode Clipping 1", 10);
    distortionMode1.addSeparator();

    distortionMode1.setJustificationType(juce::Justification::centred);
    distortionMode1.addListener(this);


    // use global lookandfeel
    getLookAndFeel().setColour(juce::ComboBox::textColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::arrowColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::buttonColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::outlineColourId, COLOUR7);
    getLookAndFeel().setColour(juce::ComboBox::focusedOutlineColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::backgroundColourId, COLOUR7);
    getLookAndFeel().setColour(juce::PopupMenu::textColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedBackgroundColourId, COLOUR5);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedTextColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::headerTextColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::backgroundColourId, COLOUR6);

    modeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID1, distortionMode1);
    modeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID2, distortionMode2);
    modeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID3, distortionMode3);
    modeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID4, distortionMode4);

    // resize limit
    setResizeLimits(1000, 500, 2000, 1000); // set resize limits
    //getConstrainer ()->setFixedAspectRatio (2); // set fixed resize rate

    updateToggleState();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    inputMeter.setLookAndFeel(nullptr);
    outputMeter.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
    linkedButton1.setLookAndFeel(nullptr);
    linkedButton2.setLookAndFeel(nullptr);
    linkedButton3.setLookAndFeel(nullptr);
    linkedButton4.setLookAndFeel(nullptr);
    safeButton1.setLookAndFeel(nullptr);
    safeButton2.setLookAndFeel(nullptr);
    safeButton3.setLookAndFeel(nullptr);
    safeButton4.setLookAndFeel(nullptr);
    filterOffButton.setLookAndFeel(nullptr);
    filterPreButton.setLookAndFeel(nullptr);
    filterPostButton.setLookAndFeel(nullptr);
    filterLowButton.setLookAndFeel(nullptr);
    filterBandButton.setLookAndFeel(nullptr);
    filterHighButton.setLookAndFeel(nullptr);
    windowRightButton.setLookAndFeel(nullptr);
    windowLeftButton.setLookAndFeel(nullptr);
}


//==============================================================================
void FireAudioProcessorEditor::paint(juce::Graphics &g)
{
    int part1 = PART1;//getHeight() / 10;
    int part2 = PART2;//part1 * 3;
    
    // background
    g.fillAll(COLOUR7);

    // title
    g.setColour(COLOUR5);
    g.fillRect(0, 0, getWidth(), part1);

    // draw version
    g.setColour(COLOUR5);
    g.setFont(juce::Font("Times New Roman", 18.0f, juce::Font::bold));
    juce::String version = (juce::String)VERSION;
    juce::Rectangle<int> area(getWidth() - 150, getHeight() - 25, 150, 50);
    g.drawFittedText(version, area, juce::Justification::topLeft, 1);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t)BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, part1, part1, 0, 0, logo.getWidth(), logo.getHeight());

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t)BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - part1, 0, part1, part1, 0, 0, logoWings.getWidth(), logoWings.getHeight());

    bool left = *processor.treeState.getRawParameterValue(WINDOW_LEFT_ID);
    bool right = *processor.treeState.getRawParameterValue(WINDOW_RIGHT_ID);

    // paint distortion function
    int mode = *processor.treeState.getRawParameterValue(MODE_ID1);
    float drive = *processor.treeState.getRawParameterValue(DRIVE_ID1);//TODO: replace it!
//    float drive = processor.getNewDrive();
    float color = *processor.treeState.getRawParameterValue(COLOR_ID);
    float rec = *processor.treeState.getRawParameterValue(REC_ID1);
    float mix = *processor.treeState.getRawParameterValue(MIX_ID1);
    float bias = *processor.treeState.getRawParameterValue(BIAS_ID1);
    float rateDivide = *processor.treeState.getRawParameterValue(DOWNSAMPLE_ID);
    
    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;

    auto frame = getLocalBounds();
    frame.setBounds(0, part1, getWidth(), part2);
    
    // draw layer 2
    g.setColour(COLOUR6);
    g.fillRect(0, part1, getWidth(), part2);
    
    distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
    
    if (left) { // if you select the left window, you will see audio wave and distortion function graphs.
        
//        spectrum.setVisible(false);
//        multiband.setVisible(false);
//
//        distortionGraph.setVisible(true);
//        distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
//        oscilloscope.setVisible(true);
        multiband.getFocusArray(multibandFocus);
        multiFocusSlider1.setValue(multibandFocus[0]);
        multiFocusSlider2.setValue(multibandFocus[1]);
        multiFocusSlider3.setValue(multibandFocus[2]);
        multiFocusSlider4.setValue(multibandFocus[3]);
        
        multiband.getStateArray(multibandState);
        multiStateSlider1.setValue(multibandState[0]);
        multiStateSlider2.setValue(multibandState[1]);
        multiStateSlider3.setValue(multibandState[2]);
        multiStateSlider4.setValue(multibandState[3]);
        
        multiband.getFreqArray(multibandFreq);
        multiFreqSlider1.setValue(multibandFreq[0]);
        multiFreqSlider2.setValue(multibandFreq[1]);
        multiFreqSlider3.setValue(multibandFreq[2]);
        
        int lineNum = multiband.getLineNum();
        lineNumSlider.setValue(lineNum);
        
        if (multibandFocus[0])
        {
            driveKnob1.setVisible(true);
            driveKnob2.setVisible(false);
            driveKnob3.setVisible(false);
            driveKnob4.setVisible(false);
            
            dynamicKnob1.setVisible(true);
            dynamicKnob2.setVisible(false);
            dynamicKnob3.setVisible(false);
            dynamicKnob4.setVisible(false);
            
            outputKnob1.setVisible(true);
            outputKnob2.setVisible(false);
            outputKnob3.setVisible(false);
            outputKnob4.setVisible(false);
            
            mixKnob1.setVisible(true);
            mixKnob2.setVisible(false);
            mixKnob3.setVisible(false);
            mixKnob4.setVisible(false);
            
            widthKnob1.setVisible(true);
            widthKnob2.setVisible(false);
            widthKnob3.setVisible(false);
            widthKnob4.setVisible(false);
            
            linkedButton1.setVisible(true);
            linkedButton2.setVisible(false);
            linkedButton3.setVisible(false);
            linkedButton4.setVisible(false);
            
            safeButton1.setVisible(true);
            safeButton2.setVisible(false);
            safeButton3.setVisible(false);
            safeButton4.setVisible(false);
            
            recKnob1.setVisible(true);
            recKnob2.setVisible(false);
            recKnob3.setVisible(false);
            recKnob4.setVisible(false);
            
            biasKnob1.setVisible(true);
            biasKnob2.setVisible(false);
            biasKnob3.setVisible(false);
            biasKnob4.setVisible(false);
        }
        else if (multibandFocus[1])
        {
            driveKnob1.setVisible(false);
            driveKnob2.setVisible(true);
            driveKnob3.setVisible(false);
            driveKnob4.setVisible(false);
            
            dynamicKnob1.setVisible(false);
            dynamicKnob2.setVisible(true);
            dynamicKnob3.setVisible(false);
            dynamicKnob4.setVisible(false);
            
            outputKnob1.setVisible(false);
            outputKnob2.setVisible(true);
            outputKnob3.setVisible(false);
            outputKnob4.setVisible(false);
            
            mixKnob1.setVisible(false);
            mixKnob2.setVisible(true);
            mixKnob3.setVisible(false);
            mixKnob4.setVisible(false);
            
            widthKnob1.setVisible(false);
            widthKnob2.setVisible(true);
            widthKnob3.setVisible(false);
            widthKnob4.setVisible(false);
            
            linkedButton1.setVisible(false);
            linkedButton2.setVisible(true);
            linkedButton3.setVisible(false);
            linkedButton4.setVisible(false);
            
            safeButton1.setVisible(false);
            safeButton2.setVisible(true);
            safeButton3.setVisible(false);
            safeButton4.setVisible(false);
            
            recKnob1.setVisible(false);
            recKnob2.setVisible(true);
            recKnob3.setVisible(false);
            recKnob4.setVisible(false);
            
            biasKnob1.setVisible(false);
            biasKnob2.setVisible(true);
            biasKnob3.setVisible(false);
            biasKnob4.setVisible(false);
        }
        else if (multibandFocus[2])
        {
            driveKnob1.setVisible(false);
            driveKnob2.setVisible(false);
            driveKnob3.setVisible(true);
            driveKnob4.setVisible(false);
            
            dynamicKnob1.setVisible(false);
            dynamicKnob2.setVisible(false);
            dynamicKnob3.setVisible(true);
            dynamicKnob4.setVisible(false);
            
            outputKnob1.setVisible(false);
            outputKnob2.setVisible(false);
            outputKnob3.setVisible(true);
            outputKnob4.setVisible(false);
            
            mixKnob1.setVisible(false);
            mixKnob2.setVisible(false);
            mixKnob3.setVisible(true);
            mixKnob4.setVisible(false);
            
            widthKnob1.setVisible(false);
            widthKnob2.setVisible(false);
            widthKnob3.setVisible(true);
            widthKnob4.setVisible(false);
            
            linkedButton1.setVisible(false);
            linkedButton2.setVisible(false);
            linkedButton3.setVisible(true);
            linkedButton4.setVisible(false);
            
            safeButton1.setVisible(false);
            safeButton2.setVisible(false);
            safeButton3.setVisible(true);
            safeButton4.setVisible(false);
            
            recKnob1.setVisible(false);
            recKnob2.setVisible(false);
            recKnob3.setVisible(true);
            recKnob4.setVisible(false);
            
            biasKnob1.setVisible(false);
            biasKnob2.setVisible(false);
            biasKnob3.setVisible(true);
            biasKnob4.setVisible(false);
        }
        else if (multibandFocus[3])
        {
            driveKnob1.setVisible(false);
            driveKnob2.setVisible(false);
            driveKnob3.setVisible(false);
            driveKnob4.setVisible(true);
            
            dynamicKnob1.setVisible(false);
            dynamicKnob2.setVisible(false);
            dynamicKnob3.setVisible(false);
            dynamicKnob4.setVisible(true);
            
            outputKnob1.setVisible(false);
            outputKnob2.setVisible(false);
            outputKnob3.setVisible(false);
            outputKnob4.setVisible(true);
            
            mixKnob1.setVisible(false);
            mixKnob2.setVisible(false);
            mixKnob3.setVisible(false);
            mixKnob4.setVisible(true);
            
            widthKnob1.setVisible(false);
            widthKnob2.setVisible(false);
            widthKnob3.setVisible(false);
            widthKnob4.setVisible(true);
            
            linkedButton1.setVisible(false);
            linkedButton2.setVisible(false);
            linkedButton3.setVisible(false);
            linkedButton4.setVisible(true);
            
            safeButton1.setVisible(false);
            safeButton2.setVisible(false);
            safeButton3.setVisible(false);
            safeButton4.setVisible(true);
            
            recKnob1.setVisible(false);
            recKnob2.setVisible(false);
            recKnob3.setVisible(false);
            recKnob4.setVisible(true);
            
            biasKnob1.setVisible(false);
            biasKnob2.setVisible(false);
            biasKnob3.setVisible(false);
            biasKnob4.setVisible(true);
        }
        
        driveLabel.setVisible(true);
        outputLabel.setVisible(true);
        mixLabel.setVisible(true);
        dynamicLabel.setVisible(true);
        widthLabel.setVisible(true);
        recLabel.setVisible(true);
        biasLabel.setVisible(true);
        
        // set other effects invisible
        downSampleKnob.setVisible(false);
        resKnob.setVisible(false);
        filterLowButton.setVisible(false);
        filterOffButton.setVisible(false);
        filterPreButton.setVisible(false);
        filterBandButton.setVisible(false);
        filterPostButton.setVisible(false);
        filterHighButton.setVisible(false);
        colorKnob.setVisible(false);
        cutoffKnob.setVisible(false);
        mixKnob.setVisible(false);
        
        
    }
    else if (right) // if you select the left window, you can use muti-band distortion
    {
        // set other effects visible
        
        downSampleKnob.setVisible(true);
        resKnob.setVisible(true);
        filterLowButton.setVisible(true);
        filterOffButton.setVisible(true);
        filterPreButton.setVisible(true);
        filterBandButton.setVisible(true);
        filterPostButton.setVisible(true);
        filterHighButton.setVisible(true);
        colorKnob.setVisible(true);
        cutoffKnob.setVisible(true);
        mixKnob.setVisible(true);
        
        // set invisible
        driveKnob1.setVisible(false);
        driveKnob2.setVisible(false);
        driveKnob3.setVisible(false);
        driveKnob4.setVisible(false);
        
        outputKnob1.setVisible(false);
        outputKnob2.setVisible(false);
        outputKnob3.setVisible(false);
        outputKnob4.setVisible(false);
        
        mixKnob1.setVisible(false);
        mixKnob2.setVisible(false);
        mixKnob3.setVisible(false);
        mixKnob4.setVisible(false);
        
        dynamicKnob1.setVisible(false);
        dynamicKnob2.setVisible(false);
        dynamicKnob3.setVisible(false);
        dynamicKnob4.setVisible(false);
        
        widthKnob1.setVisible(false);
        widthKnob2.setVisible(false);
        widthKnob3.setVisible(false);
        widthKnob4.setVisible(false);
        
        linkedButton1.setVisible(false);
        linkedButton2.setVisible(false);
        linkedButton3.setVisible(false);
        linkedButton4.setVisible(false);
        
        safeButton1.setVisible(false);
        safeButton2.setVisible(false);
        safeButton3.setVisible(false);
        safeButton4.setVisible(false);
        
        recKnob1.setVisible(false);
        recKnob2.setVisible(false);
        recKnob3.setVisible(false);
        recKnob4.setVisible(false);
        
        biasKnob1.setVisible(false);
        biasKnob2.setVisible(false);
        biasKnob3.setVisible(false);
        biasKnob4.setVisible(false);
        
        driveLabel.setVisible(false);
        outputLabel.setVisible(false);
        mixLabel.setVisible(true);
        dynamicLabel.setVisible(false);
        widthLabel.setVisible(false);
        recLabel.setVisible(false);
        biasLabel.setVisible(false);
//        oscilloscope.setVisible(false);
//        distortionGraph.setVisible(false);
        // Spectrum
//        spectrum.prepareToPaintSpectrum(processor.getFFTSize(), processor.getFFTData());
//        spectrum.setVisible(true);
//        multiband.setVisible(true);
        
        

        
        
        
        
    }

    // draw shadow 1
    //juce::ColourGradient shadowGrad1(juce::Colour(0, 0, 0).withAlpha(0.5f), 0, 50,
    //                                 juce::Colour(0, 0, 0).withAlpha(0.f), 0, 55, false);
    //g.setGradientFill(shadowGrad1);
    //g.fillRect(0, part1, getWidth(), 25);

    // draw shadow 2
    //juce::ColourGradient shadowGrad2(juce::Colour(0, 0, 0).withAlpha(0.5f), getWidth() / 2.f, getHeight() / 2.f - 75,
    //                                 juce::Colour(0, 0, 0).withAlpha(0.f), getWidth() / 2.f, getHeight() / 2.f - 70, false);
    
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
    float scale = juce::jmin(getHeight() / 500.f, getWidth() / 1000.f);
    float scaleMax = juce::jmax(getHeight() / 500.f, getWidth() / 1000.f);
    int newKnobSize = static_cast<int>(knobSize * scale);
    int startX = getWidth() / knobNum;
    int secondShadowY = getHeight() / 10 * 4;
    int firstPartY = secondShadowY + (getHeight() - secondShadowY) * 2 / 5 - newKnobSize / 2;
    int secondPartY = secondShadowY + (getHeight() - secondShadowY) * 4 / 5 - newKnobSize / 2;

    
    // save presets
    juce::Rectangle<int> r(getLocalBounds());
    r = r.removeFromTop(50 * getHeight() / 500);
    r.removeFromLeft(100 * scaleMax);
    r.removeFromRight(50 * scaleMax);

    stateComponent.setBounds(r);

    // first line
    // inputKnob.setBounds(startX * 1 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    float driveX = DRIVE_X;
    float driveY = DRIVE_Y;
    float driveSize = newKnobSize * 2;
    driveKnob1.setBounds(driveX, driveY, driveSize, driveSize);
    driveKnob2.setBounds(driveX, driveY, driveSize, driveSize);
    driveKnob3.setBounds(driveX, driveY, driveSize, driveSize);
    driveKnob4.setBounds(driveX, driveY, driveSize, driveSize);
    
    //colorKnob.setBounds(startX * 1 + newKnobSize * 1.2, secondLineY, newKnobSize, newKnobSize);
    downSampleKnob.setBounds(startX * 3 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    
    recKnob1.setBounds(startX * 5.5 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    recKnob2.setBounds(startX * 5.5 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    recKnob3.setBounds(startX * 5.5 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    recKnob4.setBounds(startX * 5.5 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    
    colorKnob.setBounds(startX * 5 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    
    biasKnob1.setBounds(startX * 5.5 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    biasKnob2.setBounds(startX * 5.5 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    biasKnob3.setBounds(startX * 5.5 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    biasKnob4.setBounds(startX * 5.5 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    
    widthKnob1.setBounds(startX * 6 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    widthKnob2.setBounds(startX * 6 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    widthKnob3.setBounds(startX * 6 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    widthKnob4.setBounds(startX * 6 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    
    
    outputKnob1.setBounds(startX * 7 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    outputKnob2.setBounds(startX * 7 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    outputKnob3.setBounds(startX * 7 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);
    outputKnob4.setBounds(startX * 7 - newKnobSize / 2, firstPartY, newKnobSize, newKnobSize);

    // second line
    dynamicKnob1.setBounds(startX * 6 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    dynamicKnob2.setBounds(startX * 6 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    dynamicKnob3.setBounds(startX * 6 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    dynamicKnob4.setBounds(startX * 6 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    
    
    cutoffKnob.setBounds(startX * 5 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    resKnob.setBounds(startX * 6 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    
    mixKnob1.setBounds(startX * 7 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    mixKnob2.setBounds(startX * 7 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    mixKnob3.setBounds(startX * 7 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    mixKnob4.setBounds(startX * 7 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    mixKnob.setBounds(startX * 7 - newKnobSize / 2, secondPartY, newKnobSize, newKnobSize);
    
    // first line
    hqButton.setBounds(getHeight() / 10, 0, getHeight() / 10, getHeight() / 10);
    linkedButton1.setBounds(DRIVE_X + newKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize / 2, 0.05 * getHeight());
    linkedButton2.setBounds(DRIVE_X + newKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize / 2, 0.05 * getHeight());
    linkedButton3.setBounds(DRIVE_X + newKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize / 2, 0.05 * getHeight());
    linkedButton4.setBounds(DRIVE_X + newKnobSize * 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize / 2, 0.05 * getHeight());
    
    float windowHeight = getHeight() / 20;
    windowLeftButton.setBounds(0, secondShadowY, getWidth() / 2, windowHeight);
    windowRightButton.setBounds(getWidth() / 2, secondShadowY, getWidth() / 2, windowHeight);
    
    // second line
    safeButton1.setBounds(DRIVE_X + newKnobSize * 2, secondPartY, newKnobSize / 2, 0.05 * getHeight());
    safeButton2.setBounds(DRIVE_X + newKnobSize * 2, secondPartY, newKnobSize / 2, 0.05 * getHeight());
    safeButton3.setBounds(DRIVE_X + newKnobSize * 2, secondPartY, newKnobSize / 2, 0.05 * getHeight());
    safeButton4.setBounds(DRIVE_X + newKnobSize * 2, secondPartY, newKnobSize / 2, 0.05 * getHeight());
    filterOffButton.setBounds(startX * 3 - newKnobSize / 4, secondPartY, newKnobSize / 2, 0.05 * getHeight());
    filterPreButton.setBounds(startX * 3 - newKnobSize / 4, secondPartY + 0.055 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterPostButton.setBounds(startX * 3 - newKnobSize / 4, secondPartY + 0.11 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterLowButton.setBounds(startX * 4 - newKnobSize / 4, secondPartY, newKnobSize / 2, 0.05 * getHeight());
    filterBandButton.setBounds(startX * 4 - newKnobSize / 4, secondPartY + 0.055 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterHighButton.setBounds(startX * 4 - newKnobSize / 4, secondPartY + 0.11 * getHeight(), newKnobSize / 2, 0.05 * getHeight());

    // visualiser
    oscilloscope.setBounds(OSC_X, OSC_Y, OSC_WIDTH, OSC_HEIGHT);
    distortionGraph.setBounds(D_GRAPH_X, D_GRAPH_Y, D_GRAPH_WIDTH, D_GRAPH_HEIGHT);
    widthGraph.setBounds(WIDTH_GRAPH_X, WIDTH_GRAPH_Y, WIDTH_GRAPH_WIDTH, WIDTH_GRAPH_HEIGHT);
    
    // spectrum
    spectrum.setBounds(SPEC_X, SPEC_Y, SPEC_WIDTH, SPEC_HEIGHT);
    multiband.setBounds(SPEC_X, SPEC_Y, SPEC_WIDTH, SPEC_HEIGHT);
    
    // ff meter
//    int ffWidth = 20;
//    int ffHeightStart = getHeight() / 10;
//    int ffHeight = getHeight() / 10 * 3;
//    inputMeter.setBounds(getWidth() / 2 - ffWidth - 2, ffHeightStart, ffWidth, ffHeight + 2);
//    outputMeter.setBounds(getWidth() / 2 + 2, ffHeightStart, ffWidth, ffHeight + 2);

    // distortion menu
    distortionMode1.setBounds(0, secondShadowY + windowHeight, getWidth() / 5, getHeight() / 12);

    otherLookAndFeel.scale = scale;
    roundedButtonLnf.scale = scale;
    
    
}

void FireAudioProcessorEditor::updateToggleState()
{
    if (*processor.treeState.getRawParameterValue(OFF_ID))
    {
        filterLowButton.setEnabled(false);
        filterBandButton.setEnabled(false);
        filterHighButton.setEnabled(false);
        cutoffKnob.setEnabled(false);
        resKnob.setEnabled(false);
    }
    else
    {
        filterLowButton.setEnabled(true);
        filterBandButton.setEnabled(true);
        filterHighButton.setEnabled(true);
        cutoffKnob.setEnabled(true);
        resKnob.setEnabled(true);
    }
}


void FireAudioProcessorEditor::timerCallback()
{
    
    // spectrum
    if (processor.isFFTBlockReady())
    {
        
        multiband.repaint();
        oscilloscope.repaint();
        distortionGraph.repaint();
        processor.processFFT();
        spectrum.prepareToPaintSpectrum(processor.getFFTSize() , processor.getFFTData());
        spectrum.repaint();
    }
    
    
//    repaint();
}



void FireAudioProcessorEditor::sliderValueChanged(juce::Slider *slider)
{
    //    float thresh = 20.f;
    //    float changeThresh = 3.f;
    if (linkedButton1.getToggleState() == true)
    {
        if (slider == &driveKnob1)
        {
            outputKnob1.setValue(-driveKnob1.getValue() * 0.1);
        }
        else if (slider == &outputKnob1 && driveKnob1.isEnabled())
        {
            if (outputKnob1.getValue() <= 0 && outputKnob1.getValue() >= -10)
                driveKnob1.setValue(-outputKnob1.getValue() * 10);
            else if (outputKnob1.getValue() > 0)
                driveKnob1.setValue(0);
            else if (outputKnob1.getValue() < -10)
                driveKnob1.setValue(100);
        }
    }
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox *combobox)
{
    // TODO: !put this in a function
    if (combobox == &distortionMode1)
    {
        // set drive knob
        if (*processor.treeState.getRawParameterValue(MODE_ID1) == 0)
        {
            tempDriveValue = driveKnob1.getValue();
            driveKnob1.setValue(1);
            driveKnob1.setEnabled(false);
        }
        else
        {
            if (driveKnob1.getValue() == 1)
            {
                driveKnob1.setValue(tempDriveValue);
            }
            driveKnob1.setEnabled(true);
        }

        // set bias knob
        int mode = *processor.treeState.getRawParameterValue(MODE_ID1);
        if (mode == 9 || mode == 0)
        {
            tempBiasValue = biasKnob1.getValue();
            biasKnob1.setValue(0);
            biasKnob1.setEnabled(false);
        }
        else
        {
            if (biasKnob1.getValue() == 0)
            {
                biasKnob1.setValue(tempBiasValue);
            }
            biasKnob1.setEnabled(true);
        }
    }
}
