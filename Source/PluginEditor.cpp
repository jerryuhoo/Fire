/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Panels/ControlPanel/Graph Components/VUMeter.h"

//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p),
    stateComponent{p.stateAB, p.statePresets, multiband}, globalPanel(processor.treeState)
{
    // timer
    juce::Timer::startTimerHz(60.0f);

    // This is not a perfect fix for Vst3 plugins
    // Vst3 calls constructor before setStateInformation in processor,
    // however, AU plugin calls constructor after setStateInformation/
    // So I set delay of 1 ms to reset size and other stuff.
    // call function after 1 ms
    std::function<void()> initFunction = [this]() { initEditor(); };
    juce::Timer::callAfterDelay(1, initFunction);
    
    // Graph
    addAndMakeVisible(graphPanel);
    
    addAndMakeVisible(bandPanel);
    addAndMakeVisible(globalPanel);
    
    // Spectrum
    addAndMakeVisible(spectrum);
    addAndMakeVisible(multiband);
    addAndMakeVisible(filterControl);
    
    multiFreqSlider1.addListener(this);
    multiFreqSlider2.addListener(this);
    multiFreqSlider3.addListener(this);
    
    spectrum.setInterceptsMouseClicks(false, false);
    spectrum.prepareToPaintSpectrum(processor.getFFTSize(), processor.getFFTData());
    
    // presets
    addAndMakeVisible(stateComponent);
    stateComponent.getPresetBox()->addListener(this);
    stateComponent.getToggleABButton()->addListener(this);
    stateComponent.getCopyABButton()->addListener(this);
    stateComponent.getPreviousButton()->addListener(this);
    stateComponent.getNextButton()->addListener(this);

    setLookAndFeel(&otherLookAndFeel);
    
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

    // Window Left Button
    addAndMakeVisible(windowLeftButton);
    windowLeftButton.setClickingTogglesState(true);
    windowLeftButton.setRadioGroupId(windowButtons);
    windowLeftButton.setButtonText("Band Effect");
    windowLeftButton.setToggleState(true, juce::NotificationType::dontSendNotification);
    windowLeftButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowLeftButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowLeftButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.0f));
    windowLeftButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowLeftButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    windowLeftButton.setLookAndFeel(&otherLookAndFeel);
    
    // Window Right Button
    addAndMakeVisible(windowRightButton);
    windowRightButton.setClickingTogglesState(true);
    windowRightButton.setRadioGroupId(windowButtons);
    windowRightButton.setButtonText("Global Effect");
    windowRightButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    windowRightButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowRightButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowRightButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.0f));
    windowRightButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowRightButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    windowRightButton.setLookAndFeel(&otherLookAndFeel);

    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HQ_ID, hqButton);

    multiFocusAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID1, multiFocusSlider1);
    multiFocusAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID2, multiFocusSlider2);
    multiFocusAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID3, multiFocusSlider3);
    multiFocusAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID4, multiFocusSlider4);
    
    multiEnableAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID1, multiEnableSlider1);
    multiEnableAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID2, multiEnableSlider2);
    multiEnableAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID3, multiEnableSlider3);
    multiEnableAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_ENABLE_ID4, multiEnableSlider4);
    
    multiFreqAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID1, multiFreqSlider1);
    multiFreqAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID2, multiFreqSlider2);
    multiFreqAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID3, multiFreqSlider3);
    
    lineStateSliderAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID1, lineStateSlider1);
    lineStateSliderAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID2, lineStateSlider2);
    lineStateSliderAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID3, lineStateSlider3);

    setMenu(&distortionMode1);
    setMenu(&distortionMode2);
    setMenu(&distortionMode3);
    setMenu(&distortionMode4);

    // use global lookandfeel
    getLookAndFeel().setColour(juce::ComboBox::textColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::ComboBox::arrowColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::ComboBox::buttonColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::outlineColourId, COLOUR6);
    getLookAndFeel().setColour(juce::ComboBox::focusedOutlineColourId, COLOUR1);
    getLookAndFeel().setColour(juce::ComboBox::backgroundColourId, COLOUR6);
    getLookAndFeel().setColour(juce::PopupMenu::textColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedBackgroundColourId, COLOUR5);
    getLookAndFeel().setColour(juce::PopupMenu::highlightedTextColourId, COLOUR1);
    getLookAndFeel().setColour(juce::PopupMenu::headerTextColourId, KNOB_SUBFONT_COLOUR);
    getLookAndFeel().setColour(juce::PopupMenu::backgroundColourId, juce::Colours::transparentWhite);

    modeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID1, distortionMode1);
    modeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID2, distortionMode2);
    modeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID3, distortionMode3);
    modeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, MODE_ID4, distortionMode4);
    
    // set resize
    setResizable(true, true);
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    // resize limit
    setResizeLimits(INIT_WIDTH, INIT_HEIGHT, 2000, 1000); // set resize limits
    getConstrainer ()->setFixedAspectRatio (2); // set fixed resize rate

    //updateToggleState();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
    windowRightButton.setLookAndFeel(nullptr);
    windowLeftButton.setLookAndFeel(nullptr);
//    distortionMode1.setLookAndFeel(nullptr);
//    distortionMode2.setLookAndFeel(nullptr);
//    distortionMode3.setLookAndFeel(nullptr);
//    distortionMode4.setLookAndFeel(nullptr);
//    stateComponent.setLookAndFeel(nullptr);
}

void FireAudioProcessorEditor::initEditor()
{
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    
    //DBG(processor.getPresetId());
    //processor.setPresetId(processor.getPresetId());
    //lastPresetName = stateComponent.getPresetName();
    
    setMultiband();
}

//==============================================================================
void FireAudioProcessorEditor::paint(juce::Graphics &g)
{
    int part1 = getHeight() / 10;
    int part2 = part1 * 3;
    
    // background
    g.fillAll(COLOUR7);

    // title
    g.setColour(COLOUR5);
    g.fillRect(0, 0, getWidth(), part1);

    // draw version
    g.setColour(COLOUR5);
    g.setFont(juce::Font("Times New Roman", 18.0f, juce::Font::bold));
    juce::String version = (juce::String)VERSION;
    juce::Rectangle<int> area(getWidth() - 50, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, juce::Justification::topLeft, 1);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t)BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, part1, part1, 0, 0, logo.getWidth(), logo.getHeight());

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t)BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - part1, 0, part1, part1, 0, 0, logoWings.getWidth(), logoWings.getHeight());

    bool left = windowLeftButton.getToggleState();
    bool right = windowRightButton.getToggleState();

    auto frame = getLocalBounds();
    frame.setBounds(0, part1, getWidth(), part2);
    
    // draw layer 2
    g.setColour(COLOUR6);
    g.fillRect(0, part1, getWidth(), part2);
    
    if (stateComponent.getChangedState()) // only for clicked the same preset in preset box
    {
        initState();
        stateComponent.setChangedState(false);
    }

    // set value only when line is deleted, added, moving
    if (multiband.getDeleteState() || multiband.getAddState() || multiband.getMovingState())
    {
        if (multiband.getDeleteState())
        {
            multiband.updateLines("delete", multiband.getChangedIndex());
            multiband.setDeleteState(false);
        }
        else if (multiband.getAddState())
        {
            multiband.updateLines("add", multiband.getChangedIndex());
            multiband.setAddState(false);
        }
        else if (multiband.getMovingState())
        {
            multiband.updateLines("moving", multiband.getChangedIndex());
            multiband.setMovingState(false);
        }
        updateFreqArray();

        multiband.getLineState(lineState);
        lineStateSlider1.setValue(lineState[0]);
        lineStateSlider2.setValue(lineState[1]);
        lineStateSlider3.setValue(lineState[2]);
        
        processor.setLineNum(multiband.getLineNum());
    }
    
    multiband.getFocusArray(multibandFocus);
    multiFocusSlider1.setValue(multibandFocus[0]);
    multiFocusSlider2.setValue(multibandFocus[1]);
    multiFocusSlider3.setValue(multibandFocus[2]);
    multiFocusSlider4.setValue(multibandFocus[3]);
    
    // TODO: put this inside
    multiband.getEnableArray(multibandEnable);
    multiEnableSlider1.setValue(multibandEnable[0]);
    multiEnableSlider2.setValue(multibandEnable[1]);
    multiEnableSlider3.setValue(multibandEnable[2]);
    multiEnableSlider4.setValue(multibandEnable[3]);
    
    int bandNum = 0;
    if (multibandFocus[0])
    {
        bandNum = 1;
    }
    else if (multibandFocus[1])
    {
        bandNum = 2;
    }
    else if (multibandFocus[2])
    {
        bandNum = 3;
    }
    else if (multibandFocus[3])
    {
        bandNum = 4;
    }
    setFourKnobsVisibility(distortionMode1, distortionMode2, distortionMode3, distortionMode4, bandNum);
    
    if (left) { // if you select the left window, you will see audio wave and distortion function graphs.

        multiband.setVisible(true);
        filterControl.setVisible(false);
    
        bandPanel.setBandNum(bandNum);
        bandPanel.setVisible(true);
        globalPanel.setVisible(false);
        
        

    }
    else if (right)
    {
        multiband.setVisible(false);
        bandPanel.setVisible(false);
        
        filterControl.setVisible(true);
        globalPanel.setVisible(true);
    }
    
    if (multibandFocus[0])
    {
        setDistortionGraph(MODE_ID1, DRIVE_ID1,
            REC_ID1, MIX_ID1, BIAS_ID1);
    }
    else if (multibandFocus[1])
    {
        setDistortionGraph(MODE_ID2, DRIVE_ID2,
            REC_ID2, MIX_ID2, BIAS_ID2);
    }
    else if (multibandFocus[2])
    {
        setDistortionGraph(MODE_ID3, DRIVE_ID3,
            REC_ID3, MIX_ID3, BIAS_ID3);
    }
    else if (multibandFocus[3])
    {
        setDistortionGraph(MODE_ID4, DRIVE_ID4,
            REC_ID4, MIX_ID4, BIAS_ID4);
    }
}

void FireAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    // save resized size
    processor.setSavedHeight(getHeight());
    processor.setSavedWidth(getWidth());

    // knobs

    float scale = juce::jmin(getHeight() / INIT_HEIGHT, getWidth() / INIT_WIDTH);
    float scaleMax = juce::jmax(getHeight() / INIT_HEIGHT, getWidth() / INIT_WIDTH);

    // top bar
    hqButton.setBounds(getHeight() / 10, 0, getHeight() / 10, getHeight() / 10);
    juce::Rectangle<int> area(getLocalBounds());
    juce::Rectangle<int> topBar = area.removeFromTop(50 * getHeight() / INIT_HEIGHT);
    topBar.removeFromLeft(100 * scaleMax);
    topBar.removeFromRight(50 * scaleMax);
    stateComponent.setBounds(topBar);

    // spectrum and filter
    juce::Rectangle<int> spectrumArea = area.removeFromTop(SPEC_HEIGHT);
    spectrum.setBounds(spectrumArea);
    multiband.setBounds(spectrumArea);
    filterControl.setBounds(spectrumArea);
    
    // left and right window buttons
    float windowHeight = getHeight() / 20;
    juce::Rectangle<int> leftWindowButtonArea = area.removeFromTop(windowHeight);
    juce::Rectangle<int> rightWindowButtonArea = leftWindowButtonArea.removeFromRight(getWidth() / 2);
    windowLeftButton.setBounds(leftWindowButtonArea);
    windowRightButton.setBounds(rightWindowButtonArea);
    
    juce::Rectangle<int> controlArea = area;
    juce::Rectangle<int> controlAreaTop = area.removeFromTop(area.getHeight() / 5);
    juce::Rectangle<int> controlAreaMid = area.removeFromTop(area.getHeight() / 4 * 3);

    
    // distortion menu
    controlAreaTop.removeFromLeft(OSC_X); // x position
    juce::Rectangle<int> distortionModeArea = controlAreaTop.removeFromLeft(OSC_WIDTH); // width
    distortionModeArea.removeFromTop(controlAreaTop.getHeight() / 4);
    distortionModeArea.removeFromBottom(controlAreaTop.getHeight() / 4);
    distortionMode1.setBounds(distortionModeArea);
    distortionMode2.setBounds(distortionModeArea);
    distortionMode3.setBounds(distortionModeArea);
    distortionMode4.setBounds(distortionModeArea);

    
    juce::Rectangle<int> graphArea = controlAreaMid.removeFromLeft(getWidth() / 7 * 2);
    graphArea.removeFromLeft(graphArea.getWidth() / 10);
    // Graph Panel
    graphPanel.setBounds(graphArea);
    
    controlArea.removeFromLeft(getWidth() / 7 * 2);
    bandPanel.setBounds(controlArea);
    globalPanel.setBounds(controlAreaMid);
//    oscSwitch.setBounds(switchArea.removeFromTop(switchArea.getHeight() / 4));
//    shapeSwitch.setBounds(switchArea.removeFromTop(switchArea.getHeight() / 3));
//    compressorSwitch.setBounds(switchArea.removeFromTop(switchArea.getHeight() / 2));
//    widthSwitch.setBounds(switchArea);
//    
//    filterSwitch.setBounds(oscSwitch.getX(), oscSwitch.getY(), SWITCH_WIDTH, oscSwitch.getHeight() * 2);
//    otherSwitch.setBounds(compressorSwitch.getX(), compressorSwitch.getY(), SWITCH_WIDTH, oscSwitch.getHeight() * 2);
    
    juce::Rectangle<int> controlLeftKnobLeftArea = controlAreaMid.removeFromLeft(getWidth() / 7 * 2);
    juce::Rectangle<int> controlLeftKnobRightArea = controlLeftKnobLeftArea.removeFromRight(getWidth() / 7);
    
    controlLeftKnobLeftArea.removeFromTop(controlLeftKnobLeftArea.getHeight() / 4);
    controlLeftKnobLeftArea.removeFromBottom(controlLeftKnobLeftArea.getHeight() / 5);

    
    controlLeftKnobRightArea.removeFromTop(controlLeftKnobRightArea.getHeight() / 4);
    controlLeftKnobRightArea.removeFromBottom(controlLeftKnobRightArea.getHeight() / 5);
    

    // set look and feel scale
    otherLookAndFeel.scale = scale;
//    roundedButtonLnf.scale = scale;
    bandPanel.setScale(scale);
//    lowPassButtonLnf.scale = scale;
//    bandPassButtonLnf.scale = scale;
//    highPassButtonLnf.scale = scale;
}



void FireAudioProcessorEditor::updateFreqArray()
{
    multiband.getFreqArray(multibandFreq);
    multiFreqSlider1.setValue(multibandFreq[0]);
    multiFreqSlider2.setValue(multibandFreq[1]);
    multiFreqSlider3.setValue(multibandFreq[2]);
}

void FireAudioProcessorEditor::timerCallback()
{
    // bypassed
    if (processor.getBypassedState())
    {
        graphPanel.repaint();
        multiband.repaint();
    }
    else if (processor.isFFTBlockReady())
    {
        // not bypassed, repaint at the same time
        //(1<<11)
        // create a temp ddtData because sometimes pushNextSampleIntoFifo will replace the original
        // fftData after doingProcess and before painting.
        float tempFFTData[2 * 2048] = {0};
        memmove(tempFFTData, processor.getFFTData(), sizeof(tempFFTData));
        // doing process, fifo data to fft data
        processor.processFFT(tempFFTData);
        // prepare to paint the spectrum
        spectrum.prepareToPaintSpectrum(processor.getFFTSize(), tempFFTData);

        graphPanel.repaint();
        spectrum.repaint();
        multiband.repaint();
        bandPanel.repaint();
        globalPanel.repaint();
        repaint();
    }
}

void FireAudioProcessorEditor::sliderValueChanged(juce::Slider *slider)
{
    

    // ableton move sliders
    if (slider == &multiFreqSlider1 && lineState[0])
    {
        if (!multiband.getMovingState())
        {
            multiband.dragLinesByFreq(multiFreqSlider1.getValue(), multiband.getSortedIndex(0));
            multiband.getFreqArray(multibandFreq);
            multiFreqSlider2.setValue(multibandFreq[1], juce::NotificationType::dontSendNotification);
            multiFreqSlider3.setValue(multibandFreq[2], juce::NotificationType::dontSendNotification);
        }
    }
    if (slider == &multiFreqSlider2 && lineState[1])
    {
        if (!multiband.getMovingState())
        {
            multiband.dragLinesByFreq(multiFreqSlider2.getValue(), multiband.getSortedIndex(1));
            multiband.getFreqArray(multibandFreq);
            multiFreqSlider1.setValue(multibandFreq[0], juce::NotificationType::dontSendNotification);
            multiFreqSlider3.setValue(multibandFreq[2], juce::NotificationType::dontSendNotification);
        }
    }
    if (slider == &multiFreqSlider3 && lineState[2])
    {
        if (!multiband.getMovingState())
        {
            multiband.dragLinesByFreq(multiFreqSlider3.getValue(), multiband.getSortedIndex(2));
            multiband.getFreqArray(multibandFreq);
            multiFreqSlider1.setValue(multibandFreq[0], juce::NotificationType::dontSendNotification);
            multiFreqSlider2.setValue(multibandFreq[1], juce::NotificationType::dontSendNotification);
        }
    }
}



void FireAudioProcessorEditor::buttonClicked(juce::Button *clickedButton)
{
    if (clickedButton == stateComponent.getToggleABButton())
    {
        stateComponent.getProcStateAB()->toggleAB();

        if (clickedButton->getButtonText() == "A")
            clickedButton->setButtonText("B");
        else
            clickedButton->setButtonText("A");
        initState();
    }
    
//    if (clickedButton == &copyABButton)
//        procStateAB.copyAB();
//    if (clickedButton == &previousButton)
//        setPreviousPreset();
//    if (clickedButton == &nextButton)
//        setNextPreset();
//    if (clickedButton == &savePresetButton)
//        savePresetAlertWindow();
//    //if (clickedButton == &deletePresetButton)
//    //    deletePresetAndRefresh();
//    if (clickedButton == &menuButton)
//        //openPresetFolder();
//        popPresetMenu();
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox *combobox)
{
    if (combobox == &distortionMode1 || combobox == &distortionMode2
        || combobox == &distortionMode3 || combobox == &distortionMode4)
    {
        changeSliderState(combobox);
    }
    if (combobox == stateComponent.getPresetBox())
    {
        int selectedId = combobox->getSelectedId();

        // do this because open and close GUI will use this function, but will reset the value if the presetbox is not "init"
        // next, previous, change combobox will change the selectedId, but currentId will change only after this.
        // and then, it will load the preset.
        
        if (stateComponent.getProcStatePresets()->getCurrentPresetId() != selectedId)
        {
            stateComponent.updatePresetBox(selectedId);
        }
        initState(); // see line 598
    }
}

void FireAudioProcessorEditor::initState()
{
    // init
    setMultiband();
    //updateToggleState(); // TEMP !!!!
    changeSliderState(&distortionMode1);
    changeSliderState(&distortionMode2);
    changeSliderState(&distortionMode3);
    changeSliderState(&distortionMode4);
}



void FireAudioProcessorEditor::setMenu(juce::ComboBox* combobox)
{
    // Distortion mode select
    addAndMakeVisible(combobox);

    combobox->addItem("None", 1);
    combobox->addSeparator();

    combobox->addSectionHeading("Soft Clipping");
    combobox->addItem("Arctan", 2);
    combobox->addItem("Exp", 3);
    combobox->addItem("Tanh", 4);
    combobox->addItem("Cubic", 5);
    combobox->addSeparator();

    combobox->addSectionHeading("Hard Clipping");
    combobox->addItem("Hard", 6);
    combobox->addItem("Sausage", 7);
    combobox->addSeparator();

    combobox->addSectionHeading("Foldback");
    combobox->addItem("Sin", 8);
    combobox->addItem("Linear", 9);
    combobox->addSeparator();

    combobox->addSectionHeading("Asymmetrical Clipping");
    combobox->addItem("Diode 1 (beta)", 10);
    combobox->addSeparator();

    combobox->setJustificationType(juce::Justification::centred);
    combobox->addListener(this);
}




void FireAudioProcessorEditor::setLinearSlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
}







void FireAudioProcessorEditor::setDistortionGraph(juce::String modeId, juce::String driveId, 
    juce::String recId, juce::String mixId, juce::String biasId)
{
    // paint distortion function
    int mode = static_cast<int>(*processor.treeState.getRawParameterValue(modeId));
    float drive = processor.getNewDrive(driveId);
    float rec = static_cast<float>(*processor.treeState.getRawParameterValue(recId));
    float mix = static_cast<float>(*processor.treeState.getRawParameterValue(mixId));
    float bias = static_cast<float>(*processor.treeState.getRawParameterValue(biasId));
    float rateDivide = static_cast<float>(*processor.treeState.getRawParameterValue(DOWNSAMPLE_ID));

    graphPanel.setDistortionState(mode, rec, mix, bias, drive, rateDivide);
}

void FireAudioProcessorEditor::setMultiband()
{
    int freq1 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID1));
    int freq2 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID2));
    int freq3 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID3));
    multiband.setFrequency(freq1, freq2, freq3);

    bool lineState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID1));
    bool lineState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID2));
    bool lineState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID3));
    multiband.setLineState(lineState1, lineState2, lineState3);

    float pos1 = static_cast<float>(SpectrumComponent::transformToLog(freq1 / (44100 / 2.0)));
    float pos2 = static_cast<float>(SpectrumComponent::transformToLog(freq2 / (44100 / 2.0)));
    float pos3 = static_cast<float>(SpectrumComponent::transformToLog(freq3 / (44100 / 2.0)));
    multiband.setLinePos(pos1, pos2, pos3);
    
    bool enableState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID1));
    bool enableState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID2));
    bool enableState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID3));
    bool enableState4 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID4));
    multiband.setEnableState(enableState1, enableState2, enableState3, enableState4);
    
    multiband.updateLines("reset", -1);
    multiband.setCloseButtonState();
    multiband.setFocus();
    processor.setLineNum(multiband.getLineNum());
}

void FireAudioProcessorEditor::setFourKnobsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum)
{
    if (bandNum == 1)
    {
        component1.setVisible(true);
        component2.setVisible(false);
        component3.setVisible(false);
        component4.setVisible(false);
    }
    else if (bandNum == 2)
    {
        component1.setVisible(false);
        component2.setVisible(true);
        component3.setVisible(false);
        component4.setVisible(false);
    }
    else if (bandNum == 3)
    {
        component1.setVisible(false);
        component2.setVisible(false);
        component3.setVisible(true);
        component4.setVisible(false);
    }
    else if (bandNum == 4)
    {
        component1.setVisible(false);
        component2.setVisible(false);
        component3.setVisible(false);
        component4.setVisible(true);
    }
}

void FireAudioProcessorEditor::changeSliderState(juce::ComboBox *combobox)
{
    if (combobox == &distortionMode1)
    {
        bandPanel.changeSliderState(0, stateComponent.getChangedState());
    }
    else if (combobox == &distortionMode2)
    {
        bandPanel.changeSliderState(1, stateComponent.getChangedState());
    }
    else if (combobox == &distortionMode3)
    {
        bandPanel.changeSliderState(2, stateComponent.getChangedState());
    }
    else if (combobox == &distortionMode4)
    {
        bandPanel.changeSliderState(3, stateComponent.getChangedState());
    }
}
