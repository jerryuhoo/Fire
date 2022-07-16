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
    stateComponent{p.stateAB, p.statePresets}, globalPanel(processor.treeState)
{
    // timer
    juce::Timer::startTimerHz(60.0f);

    // This is not a perfect fix for Vst3 plugins
    // Vst3 calls constructor before setStateInformation in processor,
    // however, AU plugin calls constructor after setStateInformation/
    // So I set delay of 1 ms to reset size and other stuff.
    // call function after 1 ms
//    std::function<void()> initFunction = [this]() { initEditor(); };
//    juce::Timer::callAfterDelay(1, initFunction);
    //initEditor();
    
    // Graph
    addAndMakeVisible(graphPanel);
    
    addAndMakeVisible(bandPanel);
    addAndMakeVisible(globalPanel);
    
    // Spectrum
    addAndMakeVisible(spectrum);
    addAndMakeVisible(multiband);
    addAndMakeVisible(filterControl);

    for (int i = 0; i < 4; i++)
    {
        multiband.getEnableButton(i).addListener(this);
        bandPanel.getCompButton(i).addListener(this);
        bandPanel.getWidthButton(i).addListener(this);
    }
        
    spectrum.setInterceptsMouseClicks(false, false);
    spectrum.prepareToPaintSpectrum(processor.getNumBins(), processor.getFFTData(), processor.getSampleRate() / (float) processor.getFFTSize());
    
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
    windowLeftButton.addListener(this);
    
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
    windowRightButton.addListener(this);
       
    if (windowLeftButton.getToggleState())
    {
        multiband.setVisible(true);
        bandPanel.setVisible(true);
        filterControl.setVisible(false);
        globalPanel.setVisible(false);
    }

    if (windowRightButton.getToggleState())
    {
        multiband.setVisible(false);
        bandPanel.setVisible(false);
        filterControl.setVisible(true);
        globalPanel.setVisible(true);
    }
         
    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HQ_ID, hqButton);

    setMenu(&distortionMode1);
    setMenu(&distortionMode2);
    setMenu(&distortionMode3);
    setMenu(&distortionMode4);

    // zoom button
    addAndMakeVisible(zoomButton);
    zoomButton.setClickingTogglesState(false);
//    zoomButton.setButtonText("Z");
    zoomButton.addListener(this);
    zoomButton.setColour(juce::TextButton::buttonColourId, COLOUR5.withAlpha(0.5f));
    zoomButton.setColour(juce::TextButton::buttonOnColourId, COLOUR5.withAlpha(0.5f));
    zoomButton.setColour(juce::ComboBox::outlineColourId, COLOUR5.withAlpha(0.5f));
    zoomButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    zoomButton.setColour(juce::TextButton::textColourOffId, COLOUR1.withAlpha(0.5f));
    zoomButton.setLookAndFeel(&zoomLookAndFeel);
        
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

    setMultiband();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    
    setLookAndFeel(nullptr); // if this is missing - YOU WILL HIT THE ASSERT 2020/6/28
    windowRightButton.setLookAndFeel(nullptr);
    windowLeftButton.setLookAndFeel(nullptr);
    zoomButton.setLookAndFeel(nullptr);
//    distortionMode1.setLookAndFeel(nullptr);
//    distortionMode2.setLookAndFeel(nullptr);
//    distortionMode3.setLookAndFeel(nullptr);
//    distortionMode4.setLookAndFeel(nullptr);
//    stateComponent.setLookAndFeel(nullptr);
}

void FireAudioProcessorEditor::initEditor()
{
//    setSize(processor.getSavedWidth(), processor.getSavedHeight());
//    processor.setLineNum(multiband.getLineNum());
    //processor.setPresetId(processor.getPresetId());
    //lastPresetName = stateComponent.getPresetName();
    //multiband.updateLines(1);
    //setMultiband();
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

    auto frame = getLocalBounds();
    frame.setBounds(0, part1, getWidth(), part2);

    int focusBand = 0;
    focusBand = multiband.getFocusBand();
    
    if (focusBand == 0)
    {
        setDistortionGraph(MODE_ID1, DRIVE_ID1,
            REC_ID1, MIX_ID1, BIAS_ID1, SAFE_ID1);
    }
    else if (focusBand == 1)
    {
        setDistortionGraph(MODE_ID2, DRIVE_ID2,
            REC_ID2, MIX_ID2, BIAS_ID2, SAFE_ID2);
    }
    else if (focusBand == 2)
    {
        setDistortionGraph(MODE_ID3, DRIVE_ID3,
            REC_ID3, MIX_ID3, BIAS_ID3, SAFE_ID3);
    }
    else if (focusBand == 3)
    {
        setDistortionGraph(MODE_ID4, DRIVE_ID4,
            REC_ID4, MIX_ID4, BIAS_ID4, SAFE_ID4);
    }
    
    setFourComponentsVisibility(distortionMode1, distortionMode2, distortionMode3, distortionMode4, focusBand);
    
    bool left = windowLeftButton.getToggleState();
    bool right = windowRightButton.getToggleState();
    
    if (left) { // if you select the left window, you will see audio wave and distortion function graphs.
        bandPanel.setFocusBandNum(focusBand);
        graphPanel.setFocusBandNum(focusBand);
    }
    else if (right)
    {
        graphPanel.setFocusBandNum(-1); // -1 means global
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
    if (zoomButton.getToggleState())
    {
        juce::Rectangle<int> spectrumArea = area;
        spectrum.setBounds(spectrumArea);
        multiband.setBounds(spectrumArea);
        filterControl.setBounds(spectrumArea);
    }
    else
    {
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
        
        area.removeFromLeft(getWidth() / 20);
        area.removeFromRight(getWidth() / 20);
        
        juce::Rectangle<int> controlArea = area;
        juce::Rectangle<int> controlAreaTop = area.removeFromTop(area.getHeight() / 5);
        juce::Rectangle<int> controlAreaMid = area.removeFromTop(area.getHeight() / 4 * 3); // 3/4

        // distortion menu
    //    controlAreaTop.removeFromLeft(getWidth() / 20); // x position
        juce::Rectangle<int> distortionModeArea = controlAreaTop.removeFromLeft(OSC_WIDTH); // width
        distortionModeArea.removeFromTop(controlAreaTop.getHeight() / 4);
        distortionModeArea.removeFromBottom(controlAreaTop.getHeight() / 4);
        distortionMode1.setBounds(distortionModeArea);
        distortionMode2.setBounds(distortionModeArea);
        distortionMode3.setBounds(distortionModeArea);
        distortionMode4.setBounds(distortionModeArea);

        juce::Rectangle<int> graphArea = controlAreaMid.removeFromLeft(getWidth() / 7 * 2);
        
        // Graph Panel
        graphPanel.setBounds(graphArea);
        
        controlArea.removeFromLeft(getWidth() / 7 * 2);
        bandPanel.setBounds(controlArea);
        globalPanel.setBounds(controlAreaMid);
        
        juce::Rectangle<int> controlLeftKnobLeftArea = controlAreaMid.removeFromLeft(getWidth() / 7 * 2);
        juce::Rectangle<int> controlLeftKnobRightArea = controlLeftKnobLeftArea.removeFromRight(getWidth() / 7);
        
        controlLeftKnobLeftArea.removeFromTop(controlLeftKnobLeftArea.getHeight() / 4);
        controlLeftKnobLeftArea.removeFromBottom(controlLeftKnobLeftArea.getHeight() / 5);

        controlLeftKnobRightArea.removeFromTop(controlLeftKnobRightArea.getHeight() / 4);
        controlLeftKnobRightArea.removeFromBottom(controlLeftKnobRightArea.getHeight() / 5);
    }
    // Zoom button
    zoomButton.setBounds(getWidth() - 30, multiband.getY() + multiband.getHeight() - 30, getHeight() / 25, getHeight() / 25);

    // set look and feel scale
    otherLookAndFeel.scale = scale;
    bandPanel.setScale(scale);
    globalPanel.setScale(scale);
    multiband.setScale(scale);
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
        spectrum.prepareToPaintSpectrum(processor.getNumBins(), tempFFTData, processor.getSampleRate() / (float) processor.getFFTSize());

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
//        initState();
        setMultiband();
    }
    if (clickedButton == &zoomButton)
    {
        if (zoomButton.getToggleState())
        {
            windowLeftButton.setVisible(true);
            windowRightButton.setVisible(true);
            if (windowLeftButton.getToggleState())
            {
                multiband.setVisible(true);
                bandPanel.setVisible(true);
                filterControl.setVisible(false);
                globalPanel.setVisible(false);
            }
            else
            {
                multiband.setVisible(false);
                bandPanel.setVisible(false);
                filterControl.setVisible(true);
                globalPanel.setVisible(true);
            }
            graphPanel.setVisible(true);
            zoomButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        }
        else
        {
            windowLeftButton.setVisible(false);
            windowRightButton.setVisible(false);
            distortionMode1.setVisible(false);
            distortionMode2.setVisible(false);
            distortionMode3.setVisible(false);
            distortionMode4.setVisible(false);
            graphPanel.setVisible(false);
            bandPanel.setVisible(false);
            globalPanel.setVisible(false);
            zoomButton.setToggleState(true, juce::NotificationType::dontSendNotification);
        }
        resized();
    }
    if (clickedButton == &windowLeftButton)
    {
        if (windowLeftButton.getToggleState())
        {
            multiband.setVisible(true);
            bandPanel.setVisible(true);
            filterControl.setVisible(false);
            globalPanel.setVisible(false);
        }
        else
        {
            multiband.setVisible(false);
            bandPanel.setVisible(false);
            filterControl.setVisible(true);
            globalPanel.setVisible(true);
        }
    }
    for (int i = 0; i < 4; i++)
    {
        // bypass button for each band
        if (clickedButton == &multiband.getEnableButton(i))
        {
            bandPanel.setBandKnobsStates(i, clickedButton->getToggleState(), false);
        }
        // bypass button for band compressor/width
        if (clickedButton == &bandPanel.getCompButton(i) || clickedButton == &bandPanel.getWidthButton(i))
        {
            bool state = clickedButton->getToggleState();
            if (state)
            {
                multiband.setBandBypassStates(i, state);
                bandPanel.setBandKnobsStates(i, state, true);
            }   
        }
    }
    
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox *combobox)
{
//    if (combobox == &distortionMode1 || combobox == &distortionMode2
//        || combobox == &distortionMode3 || combobox == &distortionMode4)
//    {
//        changeSliderState(combobox);
//    }
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
        //setMultiband();
    }
}

void FireAudioProcessorEditor::setMenu(juce::ComboBox* combobox)
{
    /** To add more distortion functions, you need to change:
     1. here
     2. createParameters in PluginProcessor.cpp
     3. processDistortion in PluginProcessor.cpp
     4. ClippingFunctions.h
     5. DistortionGraph.cpp
     */
    // Distortion mode select
    addAndMakeVisible(combobox);

    combobox->addSectionHeading("Soft Clipping");
    combobox->addItem("Arctan", 1);
    combobox->addItem("Exp", 2);
    combobox->addItem("Tanh", 3);
    combobox->addItem("Cubic", 4);
    combobox->addSeparator();

    combobox->addSectionHeading("Hard Clipping");
    combobox->addItem("Hard", 5);
    combobox->addItem("Sausage", 6);
    combobox->addSeparator();

    combobox->addSectionHeading("Foldback");
    combobox->addItem("Sin", 7);
    combobox->addItem("Linear", 8);
    combobox->addSeparator();
    
    combobox->addSectionHeading("Other");
    combobox->addItem("Limit", 9);
    combobox->addItem("Single Sin", 10);
    combobox->addItem("Logic", 11);
    combobox->addItem("Pit", 12);
    combobox->addSeparator();

//    combobox->addSectionHeading("Asymmetrical Clipping");
//    combobox->addItem("Diode 1 (beta)", 9);
//    combobox->addSeparator();

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
    juce::String recId, juce::String mixId, juce::String biasId, juce::String safeId)
{
    // paint distortion function
    int mode = static_cast<int>(*processor.treeState.getRawParameterValue(modeId));

    // protection
    float drive = static_cast<int>(*processor.treeState.getRawParameterValue(driveId));
    drive = drive * 6.5f / 100.0f;
    float powerDrive = powf(2, drive);

    float sampleMaxValue = processor.getSampleMaxValue(safeId);
    bool isSafeModeOn = *processor.treeState.getRawParameterValue(safeId);

    if (isSafeModeOn && sampleMaxValue * powerDrive > 2.0f)
    {
        drive = 2.0f / sampleMaxValue + 0.1 * std::log2f(powerDrive);
    }
    else
    {
        drive = powerDrive;
    }
    
    /// bypass audio will not run processblock
    //drive = processor.safeMode(drive, processor.getHistoryArrayL(), safeId);
    
    float rec = static_cast<float>(*processor.treeState.getRawParameterValue(recId));
    float mix = static_cast<float>(*processor.treeState.getRawParameterValue(mixId));
    float bias = static_cast<float>(*processor.treeState.getRawParameterValue(biasId));
    float rateDivide = static_cast<float>(*processor.treeState.getRawParameterValue(DOWNSAMPLE_ID));
    
    if (!*processor.treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID)) rateDivide = 1;
    
    graphPanel.setDistortionState(mode, rec, mix, bias, drive, rateDivide);
}

void FireAudioProcessorEditor::setMultiband()
{
//    bool lineState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID1));
//    bool lineState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID2));
//    bool lineState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(LINE_STATE_ID3));
//    multiband.setLineState(lineState1, lineState2, lineState3);
//
    int freq1 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID1));
    int freq2 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID2));
    int freq3 = static_cast<int>(*processor.treeState.getRawParameterValue(FREQ_ID3));

    multiband.setFrequency(freq1, freq2, freq3);
//
//    float pos1 = static_cast<float>(SpectrumComponent::transformToLog(freq1));
//    float pos2 = static_cast<float>(SpectrumComponent::transformToLog(freq2));
//    float pos3 = static_cast<float>(SpectrumComponent::transformToLog(freq3));
//    multiband.setLinePos(pos1, pos2, pos3);
//
//    bool enableState1 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID1));
//    bool enableState2 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID2));
//    bool enableState3 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID3));
//    bool enableState4 = static_cast<bool>(*processor.treeState.getRawParameterValue(BAND_ENABLE_ID4));
//    multiband.setEnableState(enableState1, enableState2, enableState3, enableState4);
    
    multiband.updateLines(1);
    multiband.setSoloRelatedBounds();
//    multiband.setFocus();
//    processor.setLineNum(multiband.getLineNum());
}

void FireAudioProcessorEditor::setFourComponentsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum)
{
    if (zoomButton.getToggleState())
    {
        component1.setVisible(false);
        component2.setVisible(false);
        component3.setVisible(false);
        component4.setVisible(false);
    }
    else
    {
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
}

//void FireAudioProcessorEditor::changeSliderState(juce::ComboBox *combobox)
//{
//    if (combobox == &distortionMode1)
//    {
//        bandPanel.changeSliderState(0, stateComponent.getChangedState());
//    }
//    else if (combobox == &distortionMode2)
//    {
//        bandPanel.changeSliderState(1, stateComponent.getChangedState());
//    }
//    else if (combobox == &distortionMode3)
//    {
//        bandPanel.changeSliderState(2, stateComponent.getChangedState());
//    }
//    else if (combobox == &distortionMode4)
//    {
//        bandPanel.changeSliderState(3, stateComponent.getChangedState());
//    }
//}
