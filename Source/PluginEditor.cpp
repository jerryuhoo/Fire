/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#define VERSION "0.76"
//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p), stateComponent{p.stateAB, p.statePresets}
{
    // timer
    juce::Timer::startTimerHz(20.0f);

    // resize not avaialble
    setResizable(true, true);

    // Visualiser
    addAndMakeVisible(p.visualiser);

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

    /*
    // input knob
    addAndMakeVisible(inputKnob);
    inputKnob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    // inputKnob.setSliderStyle(Slider::LinearVertical);
    inputKnob.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(inputLabel);
    inputLabel.setText("Input", dontSendNotification);
    inputLabel.setFont (Font (KNOB_FONT, KNOB_FONT_SIZE, Font::plain));
    inputLabel.setColour(Label::textColourId, COLOUR5);
    inputLabel.attachToComponent(&inputKnob, false);
    inputLabel.setJustificationType (Justification::centred);
    */

    // drive knob
    addAndMakeVisible(driveKnob);
    driveKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);
    driveKnob.addListener(this);

    addAndMakeVisible(driveLabel);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    driveLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    driveLabel.attachToComponent(&driveKnob, false);
    driveLabel.setJustificationType(juce::Justification::centred);

    // color knob
    addAndMakeVisible(colorKnob);
    colorKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    colorKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(colorLabel);
    colorLabel.setText("Color", juce::dontSendNotification);
    colorLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    colorLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    colorLabel.attachToComponent(&colorKnob, false);
    colorLabel.setJustificationType(juce::Justification::centred);

    // bias knob
    addAndMakeVisible(biasKnob);
    biasKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    biasKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(biasLabel);
    biasLabel.setText("Bias", juce::dontSendNotification);
    biasLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    biasLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    biasLabel.attachToComponent(&biasKnob, false);
    biasLabel.setJustificationType(juce::Justification::centred);

    // downsample knob
    addAndMakeVisible(downSampleKnob);
    downSampleKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    downSampleKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(downSampleLabel);
    downSampleLabel.setText("Downsample", juce::dontSendNotification);
    downSampleLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    downSampleLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    downSampleLabel.attachToComponent(&downSampleKnob, false);
    downSampleLabel.setJustificationType(juce::Justification::centred);

    // output knob
    addAndMakeVisible(outputKnob);
    outputKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);
    outputKnob.addListener(this);

    addAndMakeVisible(outputLabel);
    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    outputLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    outputLabel.attachToComponent(&outputKnob, false);
    outputLabel.setJustificationType(juce::Justification::centred);

    // rec knob
    addAndMakeVisible(recKnob);
    recKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    recKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(recLabel);
    recLabel.setText("Rectification", juce::dontSendNotification);
    recLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    recLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    recLabel.attachToComponent(&recKnob, false);
    recLabel.setJustificationType(juce::Justification::centred);

    // mix knob
    addAndMakeVisible(mixKnob);
    mixKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    mixLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    mixLabel.attachToComponent(&mixKnob, false);
    mixLabel.setJustificationType(juce::Justification::centred);

    // cutoff knob
    addAndMakeVisible(cutoffKnob);
    cutoffKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);

    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setFont(juce::Font(KNOB_FONT, KNOB_FONT_SIZE, juce::Font::plain));
    cutoffLabel.setColour(juce::Label::textColourId, KNOB_FONT_COLOUR);
    cutoffLabel.attachToComponent(&cutoffKnob, false);
    cutoffLabel.setJustificationType(juce::Justification::centred);

    // res knob
    addAndMakeVisible(resKnob);
    resKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    resKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 30);

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
    addAndMakeVisible(linkedButton);
    linkedButton.setClickingTogglesState(true);
    bool linkedButtonState = *processor.treeState.getRawParameterValue("linked");
    linkedButton.setToggleState(linkedButtonState, juce::dontSendNotification);
    linkedButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    linkedButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    linkedButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    linkedButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    linkedButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    linkedButton.setButtonText("LINK");
    linkedButton.setLookAndFeel(&roundedButtonLnf);

    // safe overload Button
    addAndMakeVisible(safeButton);
    safeButton.setClickingTogglesState(true);
    bool safeButtonState = *processor.treeState.getRawParameterValue("safe");
    safeButton.setToggleState(safeButtonState, juce::dontSendNotification);
    safeButton.setColour(juce::TextButton::buttonColourId, COLOUR6);
    safeButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5);
    safeButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    safeButton.setColour(juce::TextButton::textColourOnId, KNOB_FONT_COLOUR);
    safeButton.setColour(juce::TextButton::textColourOffId, KNOB_FONT_COLOUR);
    safeButton.setButtonText("SAFE");
    safeButton.setLookAndFeel(&roundedButtonLnf);

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
    bool windowLeftButtonState = *processor.treeState.getRawParameterValue("windowLeft");
    windowLeftButton.setToggleState(windowLeftButtonState, juce::dontSendNotification);
    windowLeftButton.onClick = [this] { updateWindowState(); };
    windowLeftButton.setColour(juce::TextButton::buttonColourId, COLOUR1.withAlpha(0.2f));
    windowLeftButton.setColour(juce::TextButton::buttonOnColourId, COLOUR1.withAlpha(0.8f));
    windowLeftButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.f));
    windowLeftButton.setColour(juce::TextButton::textColourOnId, COLOUR5);
    windowLeftButton.setColour(juce::TextButton::textColourOffId, juce::Colour(100, 20, 20));
    //windowLeftButton.setButtonText("1");
    windowLeftButton.setLookAndFeel(&otherLookAndFeel);
    
    // Window Right Button
    addAndMakeVisible(windowRightButton);
    windowRightButton.setClickingTogglesState(true);
    windowRightButton.setRadioGroupId(windowButtons);
    bool windowRightButtonState = *processor.treeState.getRawParameterValue("windowRight");
    windowRightButton.setToggleState(windowRightButtonState, juce::dontSendNotification);
    windowRightButton.onClick = [this] { updateWindowState(); };
    windowRightButton.setColour(juce::TextButton::buttonColourId, COLOUR1.withAlpha(0.2f));
    windowRightButton.setColour(juce::TextButton::buttonOnColourId, COLOUR1.withAlpha(0.8f));
    windowRightButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.f));
    windowRightButton.setColour(juce::TextButton::textColourOnId, COLOUR5);
    windowRightButton.setColour(juce::TextButton::textColourOffId, juce::Colour(100, 20, 20));
    windowRightButton.setLookAndFeel(&otherLookAndFeel);
    
    // Attachment
    //inputAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "inputGain", inputKnob);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "drive", driveKnob);
    colorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "color", colorKnob);
    biasAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "bias", biasKnob);
    downSampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "downSample", downSampleKnob);
    recAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "rec", recKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "outputGain", outputKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "mix", mixKnob);
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "cutoff", cutoffKnob);
    resAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, "res", resKnob);

    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "hq", hqButton);
    linkedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "linked", linkedButton);
    safeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "safe", safeButton);
    filterOffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "off", filterOffButton);
    filterPreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "pre", filterPreButton);
    filterPostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "post", filterPostButton);
    filterLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "low", filterLowButton);
    filterBandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "band", filterBandButton);
    filterHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "high", filterHighButton);
    
    windowLeftButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "windowLeft", windowLeftButton);
    windowRightButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, "windowRight", windowRightButton);

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
    distortionMode.addItem("Sausage Fattener", 7);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Foldback");
    distortionMode.addItem("Sin Foldback", 8);
    distortionMode.addItem("Linear Foldback", 9);
    distortionMode.addSeparator();

    distortionMode.addSectionHeading("Asymmetrical Clipping");
    distortionMode.addItem("Diode Clipping 1", 10);
    distortionMode.addSeparator();

    distortionMode.setJustificationType(juce::Justification::centred);
    distortionMode.addListener(this);
    //    distortionMode.setColour(ComboBox::textColourId, COLOUR1);
    //    distortionMode.setColour(ComboBox::arrowColourId, COLOUR1);
    //    distortionMode.setColour(ComboBox::buttonColourId, COLOUR1);
    //    distortionMode.setColour(ComboBox::outlineColourId, COLOUR7);
    //    distortionMode.setColour(ComboBox::focusedOutlineColourId, COLOUR1);
    //    distortionMode.setColour(ComboBox::backgroundColourId, COLOUR7);
    //    distortionMode.getLookAndFeel().setColour(PopupMenu::textColourId, COLOUR1);
    //    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId, COLOUR5);
    //    distortionMode.getLookAndFeel().setColour(PopupMenu::highlightedTextColourId, COLOUR1);
    //    distortionMode.getLookAndFeel().setColour(PopupMenu::headerTextColourId, COLOUR1);
    //    distortionMode.getLookAndFeel().setColour(PopupMenu::backgroundColourId, COLOUR6);

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

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, "mode", distortionMode);

    // about button
    //    addAndMakeVisible(aboutButton);
    //    aboutButton.setColour(juce::TextButton::buttonColourId, COLOUR3);
    //    aboutButton.setColour(juce::TextButton::textColourOnId, COLOUR3);
    //    aboutButton.setColour(juce::TextButton::textColourOffId, COLOUR3);
    //    aboutButton.onClick = [this] { aboutDialog.showMessageBoxAsync(AlertWindow::InfoIcon, "Made by Wings", "And fuck you", "OK"); };
    // resize limit
    setResizeLimits(1000, 500, 2000, 1000); // set resize limits
    //getConstrainer ()->setFixedAspectRatio (1.33); // set fixed resize rate: 700/525

    updateToggleState();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    inputMeter.setLookAndFeel(nullptr);
    outputMeter.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
    linkedButton.setLookAndFeel(nullptr);
    safeButton.setLookAndFeel(nullptr);
    filterOffButton.setLookAndFeel(nullptr);
    filterPreButton.setLookAndFeel(nullptr);
    filterPostButton.setLookAndFeel(nullptr);
    filterLowButton.setLookAndFeel(nullptr);
    filterBandButton.setLookAndFeel(nullptr);
    filterHighButton.setLookAndFeel(nullptr);
    windowRightButton.setLookAndFeel(nullptr);
    windowLeftButton.setLookAndFeel(nullptr);
}

float f(float x)
{
    return 0.5;
}

//==============================================================================
void FireAudioProcessorEditor::paint(juce::Graphics &g)
{
    int part1 = getHeight() / 10;
    int part2 = part1 * 3;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.fillAll(COLOUR7);

    // title
    g.setColour(COLOUR5);
    // g.fillRect(0, 0, getWidth(), static_cast<int> (getHeight()/10.f));
    g.fillRect(0, 0, getWidth(), part1);

    // draw version
    g.setColour(COLOUR5);
    g.setFont(juce::Font("Times New Roman", 18.0f, juce::Font::bold));
    juce::String version = (juce::String) "Ver. " + (juce::String)VERSION;
    juce::Rectangle<int> area(getWidth() - 100, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, juce::Justification::topLeft, 1);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t)BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, part1, part1, 0, 0, logo.getWidth(), logo.getHeight());

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t)BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - part1, 0, part1, part1, 0, 0, logoWings.getWidth(), logoWings.getHeight());

    // paint distortion function
    bool left = *processor.treeState.getRawParameterValue("windowLeft");
    bool right = *processor.treeState.getRawParameterValue("windowRight");
    
    float functionValue = 0;
    float mixValue;

    int mode = *processor.treeState.getRawParameterValue("mode");
    //float inputGain = *processor.treeState.getRawParameterValue("inputGain");
    //float drive = *processor.treeState.getRawParameterValue("drive");
    float color = *processor.treeState.getRawParameterValue("color");
    float rec = *processor.treeState.getRawParameterValue("rec");
    float mix = *processor.treeState.getRawParameterValue("mix");
    float bias = *processor.treeState.getRawParameterValue("bias");

    float drive = processor.getNewDrive();

    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;

    auto frame = getLocalBounds(); // adjust here, if you want to paint in a special location
    frame.setBounds(getWidth() / 2, part1, getWidth() / 2, part2);
    
    // draw layer 2
    g.setColour(COLOUR6);
    g.fillRect(0, part1, getWidth(), part2);
    
    if (left) {
        if (mode < 9)
        {
            const int numPix = frame.getWidth(); // you might experiment here, if you want less steps to speed up

            float driveScale = 1;
            float maxValue = 2.0f * driveScale * mix + 2.0f * (1 - mix);
            float value = -maxValue; // minimum (leftmost)  value for your graph
            float valInc = (maxValue - value) / numPix;
            float xPos = frame.getX();
            const float posInc = frame.getWidth() / numPix;

            juce::Path p;

            bool edgePointL = false;
            bool edgePointR = false;

            for (int i = 1; i < numPix; ++i)
            {
                value += valInc;
                xPos += posInc;

                // symmetrical distortion
                if (mode < 9)
                {
                    functionValue = distortionProcessor.distortionProcess(value);
                }

                // downsample
                float rateDivide = *processor.treeState.getRawParameterValue("downSample");
                if (rateDivide > 1)
                {
                    functionValue = ceilf(functionValue * (64.f / rateDivide)) / (64.f / rateDivide);
                }

                // retification
                functionValue = distortionProcessor.rectificationProcess(functionValue);

                // mix
                functionValue = (1.f - mix) * value + mix * functionValue;
                mixValue = (2.0f / 3.0f) * functionValue;
                float yPos = frame.getCentreY() - frame.getHeight() * mixValue / 2.0f;

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

            int colour_r = 244;
            int colour_g = (208 - drive * 2 < 0) ? 0 : (208 - drive * 2);
            int colour_b = 63;

            juce::ColourGradient grad(juce::Colour(colour_r, colour_g, colour_b), frame.getX() + frame.getWidth() / 2, frame.getY() + frame.getHeight() / 2,
                                      COLOUR6, frame.getX(), frame.getY() + frame.getHeight() / 2, true);
            g.setGradientFill(grad);
            g.strokePath(p, juce::PathStrokeType(2.0));
        }
        
        processor.visualiser.setVisible(true);
        // WARNING!! should write my own visualiser instead because it flashes when switching to right window
    }
    else if (right)
    {
        processor.visualiser.setVisible(false);
        // WARNING!! should write my own visualiser instead because it flashes when switching to right window
    }

    // draw shadow 1
    //ColourGradient shadowGrad1(Colour(0, 0, 0).withAlpha(0.5f), 0, 50,
    //                          Colour(0, 0, 0).withAlpha(0.f), 0, 55, false);
    //g.setGradientFill(shadowGrad1);
    //g.fillRect(0, part1, getWidth(), 25);

    // draw shadow 2
    juce::ColourGradient shadowGrad2(juce::Colour(0, 0, 0).withAlpha(0.5f), getWidth() / 2.f, getHeight() / 2.f - 75,
                                     juce::Colour(0, 0, 0).withAlpha(0.f), getWidth() / 2.f, getHeight() / 2.f - 70, false);
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
    int firstLineY = secondShadowY + (getHeight() - secondShadowY) * 2 / 5 - newKnobSize / 2;
    int secondLineY = secondShadowY + (getHeight() - secondShadowY) * 4 / 5 - newKnobSize / 2;

    // save presets
    juce::Rectangle<int> r(getLocalBounds());
    r = r.removeFromTop(50 * getHeight() / 500);
    r.removeFromLeft(100 * scaleMax);
    r.removeFromRight(50 * scaleMax);

    stateComponent.setBounds(r);

    // first line
    // inputKnob.setBounds(startX * 1 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    driveKnob.setBounds(startX * 1 - newKnobSize / 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize * 2, newKnobSize * 2);
    //colorKnob.setBounds(startX * 1 + newKnobSize * 1.2, secondLineY, newKnobSize, newKnobSize);
    downSampleKnob.setBounds(startX * 3 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    recKnob.setBounds(startX * 4 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    colorKnob.setBounds(startX * 5 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    biasKnob.setBounds(startX * 6 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);
    outputKnob.setBounds(startX * 7 - newKnobSize / 2, firstLineY, newKnobSize, newKnobSize);

    // second line
    cutoffKnob.setBounds(startX * 5 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);
    resKnob.setBounds(startX * 6 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);
    mixKnob.setBounds(startX * 7 - newKnobSize / 2, secondLineY, newKnobSize, newKnobSize);

    // first line
    hqButton.setBounds(getHeight() / 10, 0, getHeight() / 10, getHeight() / 10);
    linkedButton.setBounds(startX * 1 + newKnobSize * 3 / 2, secondShadowY + (getHeight() - secondShadowY) / 2 - newKnobSize / 2 - 25, newKnobSize / 2, 0.05 * getHeight());
    
    windowLeftButton.setBounds(0, secondShadowY - getHeight() / 50, getWidth() / 2, getHeight() / 50);
    windowRightButton.setBounds(getWidth() / 2, secondShadowY - getHeight() / 50, getWidth() / 2, getHeight() / 50);
    
    // second line
    safeButton.setBounds(startX * 1 + newKnobSize * 3 / 2, secondLineY, newKnobSize / 2, 0.05 * getHeight());
    filterOffButton.setBounds(startX * 3 - newKnobSize / 4, secondLineY, newKnobSize / 2, 0.05 * getHeight());
    filterPreButton.setBounds(startX * 3 - newKnobSize / 4, secondLineY + 0.055 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterPostButton.setBounds(startX * 3 - newKnobSize / 4, secondLineY + 0.11 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterLowButton.setBounds(startX * 4 - newKnobSize / 4, secondLineY, newKnobSize / 2, 0.05 * getHeight());
    filterBandButton.setBounds(startX * 4 - newKnobSize / 4, secondLineY + 0.055 * getHeight(), newKnobSize / 2, 0.05 * getHeight());
    filterHighButton.setBounds(startX * 4 - newKnobSize / 4, secondLineY + 0.11 * getHeight(), newKnobSize / 2, 0.05 * getHeight());

    // about
    // aboutButton.setBounds(getWidth() - 100, 0, 100, 50);

    // visualiser
    // WARNING!! should write my own visualiser instead because it flashes when switching to right window
    processor.visualiser.setBounds(0, getHeight() / 10 + 10, getWidth() / 2, getHeight() / 10 * 3 - 10);
    
    // ff meter
    int ffWidth = 20;
    int ffHeightStart = getHeight() / 10;
    int ffHeight = getHeight() / 10 * 3;
    inputMeter.setBounds(getWidth() / 2 - ffWidth - 2, ffHeightStart, ffWidth, ffHeight + 2);
    outputMeter.setBounds(getWidth() / 2 + 2, ffHeightStart, ffWidth, ffHeight + 2);

    // distortion menu
    distortionMode.setBounds(0, getHeight() / 10 * 4, getWidth() / 5, getHeight() / 10);

    otherLookAndFeel.scale = scale;
    roundedButtonLnf.scale = scale;
}

void FireAudioProcessorEditor::updateToggleState()
{
    if (*processor.treeState.getRawParameterValue("off"))
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

void FireAudioProcessorEditor::updateWindowState()
{
    
}

void FireAudioProcessorEditor::timerCallback()
{
    repaint();
}

void FireAudioProcessorEditor::sliderValueChanged(juce::Slider *slider)
{
    //    float thresh = 20.f;
    //    float changeThresh = 3.f;
    if (linkedButton.getToggleState() == true)
    {
        if (slider == &driveKnob)
        {
            outputKnob.setValue(-driveKnob.getValue() * 0.1);
            //            if (driveKnob.getValue()>2)
            //                outputKnob.setValue(-changeThresh - (driveKnob.getValue()-1)/31*(thresh-changeThresh));
            //            else
            //                outputKnob.setValue((driveKnob.getValue()-1)*(-3));
        }
        else if (slider == &outputKnob && driveKnob.isEnabled())
        {
            //            if (outputKnob.getValue() < -changeThresh && outputKnob.getValue() > -thresh)
            //                driveKnob.setValue(1-(outputKnob.getValue()+changeThresh)*31/(thresh-changeThresh));
            //            else if (outputKnob.getValue() >=-changeThresh && outputKnob.getValue() <0)
            //                driveKnob.setValue(1 + outputKnob.getValue()/(-changeThresh));
            //            else if (outputKnob.getValue() >= 0)
            //                driveKnob.setValue(1);
            //            else if (outputKnob.getValue() <= -10)
            //                driveKnob.setValue(32);
            if (outputKnob.getValue() <= 0 && outputKnob.getValue() >= -10)
                driveKnob.setValue(-outputKnob.getValue() * 10);
            else if (outputKnob.getValue() > 0)
                driveKnob.setValue(0);
            else if (outputKnob.getValue() < -10)
                driveKnob.setValue(100);
        }
    }
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox *combobox)
{
    if (combobox == &distortionMode)
    {
        // set drive knob
        if (*processor.treeState.getRawParameterValue("mode") == 0)
        {
            tempDriveValue = driveKnob.getValue();
            driveKnob.setValue(1);
            driveKnob.setEnabled(false);
        }
        else
        {
            if (driveKnob.getValue() == 1)
            {
                driveKnob.setValue(tempDriveValue);
            }
            driveKnob.setEnabled(true);
        }

        // set bias knob
        int mode = *processor.treeState.getRawParameterValue("mode");
        if (mode == 9 || mode == 0)
        {
            tempBiasValue = biasKnob.getValue();
            biasKnob.setValue(0);
            biasKnob.setEnabled(false);
        }
        else
        {
            if (biasKnob.getValue() == 0)
            {
                biasKnob.setValue(tempBiasValue);
            }
            biasKnob.setEnabled(true);
        }
    }
}
