/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginEditor.h"
#include "Panels/ControlPanel/Graph Components/VUMeter.h"
#include "PluginProcessor.h"

//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), stateComponent { p.stateAB, p.statePresets }, globalPanel(processor.treeState)
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

    // 1. Check if this plugin instance has ALREADY performed an update check.
    if (! processor.hasUpdateCheckBeenPerformed)
    {
        // 2. If not, check the user's preference from the now-loaded state.
        if (*processor.treeState.getRawParameterValue(AUTO_UPDATE_ID))
        {
            // 3. If enabled, schedule the check on a background thread.
            juce::Timer::callAfterDelay(1000, [this]()
                                        {
                std::unique_ptr<VersionInfo> versionInfo = VersionInfo::fetchLatestFromUpdateServer();
                if (versionInfo != nullptr && !versionInfo->versionString.equalsIgnoreCase(juce::String("v") + juce::String(VERSION)))
                {
                    juce::String version = versionInfo->versionString;
                    const auto callback = juce::ModalCallbackFunction::create([version](int result) {
                        if (result == 1)
                        {
                            juce::URL(GITHUB_TAG_LINK + version).launchInDefaultBrowser();
                        }
                    });
                    
                    juce::MessageManager::callAsync([callback, version]() {
                        juce::NativeMessageBox::showOkCancelBox(juce::AlertWindow::InfoIcon,
                            "New Version", "New version " + version + " available, do you want to download it?", nullptr, callback);
                    });
                } });
        }

        // 4. CRITICAL: Set the flag to true, so this check will never run again
        // for this plugin instance, even if the UI is closed and reopened.
        processor.hasUpdateCheckBeenPerformed = true;
    }

    // Graph
    addAndMakeVisible(graphPanel);
    graphPanel.addMouseListener(this, true);

    addAndMakeVisible(bandPanel);
    addAndMakeVisible(globalPanel);

    // Spectrum
    addAndMakeVisible(specBackground);
    addAndMakeVisible(processedSpectrum);
    addAndMakeVisible(originalSpectrum);
    addAndMakeVisible(multiband);
    multiband.addMouseListener(this, false);
    updateWhenChangingFocus();
    addAndMakeVisible(filterControl);

    for (int i = 0; i < 4; i++)
    {
        multiband.getEnableButton(i).addListener(this);
        bandPanel.getCompButton(i).addListener(this);
        bandPanel.getWidthButton(i).addListener(this);
    }

    processedSpectrum.setInterceptsMouseClicks(false, false);
    processedSpectrum.prepareToPaintSpectrum(processor.getNumBins(), processor.getFFTData(1), processor.getSampleRate() / (float) processor.getFFTSize());
    originalSpectrum.prepareToPaintSpectrum(processor.getNumBins(), processor.getFFTData(0), processor.getSampleRate() / (float) processor.getFFTSize());

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
    getConstrainer()->setFixedAspectRatio(2); // set fixed resize rate

    setMultiband();
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
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
void FireAudioProcessorEditor::paint(juce::Graphics& g)
{
    int part1 = getHeight() / 10;
    int part2 = part1 * 3;

    // background
    g.setColour(COLOUR7);
    g.fillRect(0.0f, getHeight() * 3 / 10.0f, static_cast<float>(getWidth()), getHeight() * 7 / 10.0f);

    // title
    g.setColour(COLOUR5);
    g.fillRect(0, 0, getWidth(), part1);

    // draw version
    g.setColour(COLOUR5);
    g.setFont(juce::Font("Times New Roman", 18.0f, juce::Font::bold));
    juce::String version = (juce::String) VERSION;
    juce::Rectangle<int> area(getWidth() - 50, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, juce::Justification::topLeft, 1);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t) BinaryData::firelogo_pngSize);
    g.drawImage(logo, 0, 0, part1, part1, 0, 0, logo.getWidth(), logo.getHeight());

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t) BinaryData::firewingslogo_pngSize);
    g.drawImage(logoWings, getWidth() - part1, 0, part1, part1, 0, 0, logoWings.getWidth(), logoWings.getHeight());

    auto frame = getLocalBounds();
    frame.setBounds(0, part1, getWidth(), part2);

    int focusIndex = 0;
    focusIndex = multiband.getFocusIndex();

    if (focusIndex == 0)
    {
        setDistortionGraph(MODE_ID1, DRIVE_ID1, REC_ID1, MIX_ID1, BIAS_ID1, SAFE_ID1);
    }
    else if (focusIndex == 1)
    {
        setDistortionGraph(MODE_ID2, DRIVE_ID2, REC_ID2, MIX_ID2, BIAS_ID2, SAFE_ID2);
    }
    else if (focusIndex == 2)
    {
        setDistortionGraph(MODE_ID3, DRIVE_ID3, REC_ID3, MIX_ID3, BIAS_ID3, SAFE_ID3);
    }
    else if (focusIndex == 3)
    {
        setDistortionGraph(MODE_ID4, DRIVE_ID4, REC_ID4, MIX_ID4, BIAS_ID4, SAFE_ID4);
    }

    setFourComponentsVisibility(distortionMode1, distortionMode2, distortionMode3, distortionMode4, focusIndex);

    bool left = windowLeftButton.getToggleState();
    bool right = windowRightButton.getToggleState();

    if (left)
    { // if you select the left window, you will see audio wave and distortion function graphs.
        bandPanel.setFocusBandNum(focusIndex);
        graphPanel.setFocusBandNum(focusIndex);
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
    hqButton.setBounds(getHeight() / 10.0f, 0, getHeight() / 10.0f, getHeight() / 10.0f);
    juce::Rectangle<int> area(getLocalBounds());
    juce::Rectangle<int> topBar = area.removeFromTop(50.0f * getHeight() / INIT_HEIGHT);
    topBar.removeFromLeft(100.0f * scaleMax);
    topBar.removeFromRight(50.0f * scaleMax);
    stateComponent.setBounds(topBar);

    // spectrum and filter
    if (zoomButton.getToggleState())
    {
        juce::Rectangle<int> spectrumArea = area;
        specBackground.setBounds(spectrumArea);
        processedSpectrum.setBounds(spectrumArea);
        originalSpectrum.setBounds(spectrumArea);
        multiband.setBounds(spectrumArea);
        filterControl.setBounds(spectrumArea);
    }
    else
    {
        juce::Rectangle<int> spectrumArea = area.removeFromTop(SPEC_HEIGHT);
        specBackground.setBounds(spectrumArea);
        processedSpectrum.setBounds(spectrumArea);
        originalSpectrum.setBounds(spectrumArea);
        multiband.setBounds(spectrumArea);
        filterControl.setBounds(spectrumArea);

        // left and right window buttons
        float windowHeight = getHeight() / 20.0f;
        juce::Rectangle<int> leftWindowButtonArea = area.removeFromTop(windowHeight);
        juce::Rectangle<int> rightWindowButtonArea = leftWindowButtonArea.removeFromRight(getWidth() / 2.0f);
        windowLeftButton.setBounds(leftWindowButtonArea);
        windowRightButton.setBounds(rightWindowButtonArea);

        area.removeFromLeft(getWidth() / 20.0f);
        area.removeFromRight(getWidth() / 20.0f);

        juce::Rectangle<int> controlArea = area;
        juce::Rectangle<int> controlAreaTop = area.removeFromTop(area.getHeight() / 5.0f);
        juce::Rectangle<int> controlAreaMid = area.removeFromTop(area.getHeight() / 4.0f * 3.0f); // 3/4

        // distortion menu
        //    controlAreaTop.removeFromLeft(getWidth() / 20); // x position
        juce::Rectangle<int> distortionModeArea = controlAreaTop.removeFromLeft(OSC_WIDTH); // width
        distortionModeArea.removeFromTop(controlAreaTop.getHeight() / 4.0f);
        distortionModeArea.removeFromBottom(controlAreaTop.getHeight() / 4.0f);
        distortionMode1.setBounds(distortionModeArea);
        distortionMode2.setBounds(distortionModeArea);
        distortionMode3.setBounds(distortionModeArea);
        distortionMode4.setBounds(distortionModeArea);

        juce::Rectangle<int> graphArea = controlAreaMid.removeFromLeft(getWidth() / 7.0f * 2.0f);

        // Graph Panel
        graphPanel.setBounds(graphArea);

        controlArea.removeFromLeft(getWidth() / 7.0f * 2.0f);
        bandPanel.setBounds(controlArea);
        globalPanel.setBounds(controlAreaMid);

        juce::Rectangle<int> controlLeftKnobLeftArea = controlAreaMid.removeFromLeft(getWidth() / 7.0f * 2.0f);
        juce::Rectangle<int> controlLeftKnobRightArea = controlLeftKnobLeftArea.removeFromRight(getWidth() / 7.0f);

        controlLeftKnobLeftArea.removeFromTop(controlLeftKnobLeftArea.getHeight() / 4.0f);
        controlLeftKnobLeftArea.removeFromBottom(controlLeftKnobLeftArea.getHeight() / 5.0f);

        controlLeftKnobRightArea.removeFromTop(controlLeftKnobRightArea.getHeight() / 4.0f);
        controlLeftKnobRightArea.removeFromBottom(controlLeftKnobRightArea.getHeight() / 5.0f);
    }
    // Zoom button
    zoomButton.setBounds(getWidth() - 30.0f * scale, multiband.getY() + multiband.getHeight() - 30.0f * scale, getHeight() / 25.0f, getHeight() / 25.0f);

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

        float tempFFTDataProcessed[2 * 2048] = { 0 };
        memmove(tempFFTDataProcessed, processor.getFFTData(1), sizeof(tempFFTDataProcessed));
        processor.processFFT(tempFFTDataProcessed, 1);
        float tempFFTDataOriginal[2 * 2048] = { 0 };
        memmove(tempFFTDataOriginal, processor.getFFTData(0), sizeof(tempFFTDataOriginal));
        processor.processFFT(tempFFTDataOriginal, 0);

        // prepare to paint the spectrum
        float specAlpha = static_cast<float>(*processor.treeState.getRawParameterValue(MIX_ID));
        processedSpectrum.setSpecAlpha(specAlpha);
        originalSpectrum.setSpecAlpha(specAlpha);
        processedSpectrum.prepareToPaintSpectrum(processor.getNumBins(), tempFFTDataProcessed, processor.getSampleRate() / (float) processor.getFFTSize());
        originalSpectrum.prepareToPaintSpectrum(processor.getNumBins(), tempFFTDataOriginal, processor.getSampleRate() / (float) processor.getFFTSize());

        graphPanel.repaint();
        processedSpectrum.repaint();
        originalSpectrum.repaint();
        multiband.repaint();
        bandPanel.repaint();
        globalPanel.repaint();
        repaint();
    }
}

void FireAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
}

void FireAudioProcessorEditor::buttonClicked(juce::Button* clickedButton)
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
            if (state && ! multiband.getStateComponent().getChangedState())
            {
                multiband.setBandBypassStates(i, state);
                bandPanel.setBandKnobsStates(i, state, true);
            }
        }
    }
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox* combobox)
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

void FireAudioProcessorEditor::setDistortionGraph(juce::String modeId, juce::String driveId, juce::String recId, juce::String mixId, juce::String biasId, juce::String safeId)
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

    if (! *processor.treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID))
        rateDivide = 1;

    graphPanel.setDistortionState(mode, rec, mix, bias, drive, rateDivide);
}

void FireAudioProcessorEditor::setMultiband()
{
    //multiband.updateLines(1);
    multiband.sortLines();
    multiband.setLineRelatedBoundsByX();
    multiband.setSoloRelatedBounds();
    //    multiband.resetFocus();
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

void FireAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (e.eventComponent == graphPanel.getOscilloscope())
    {
        bandPanel.setSwitch(0, true);
    }
    if (e.eventComponent == graphPanel.getDistortionGraph())
    {
        bandPanel.setSwitch(1, true);
    }
    if (e.eventComponent == graphPanel.getVuPanel())
    {
        bandPanel.setSwitch(2, true);
    }
    if (e.eventComponent == graphPanel.getWidthGraph())
    {
        bandPanel.setSwitch(3, true);
    }
    if (e.eventComponent == &multiband)
    {
        updateWhenChangingFocus();
    }
}

void FireAudioProcessorEditor::updateWhenChangingFocus()
{
    focusIndex = multiband.getFocusIndex();

    //    if (focusIndex == 0)
    //    {
    //        setDistortionGraph(MODE_ID1, DRIVE_ID1,
    //            REC_ID1, MIX_ID1, BIAS_ID1, SAFE_ID1);
    //    }
    //    else if (focusIndex == 1)
    //    {
    //        setDistortionGraph(MODE_ID2, DRIVE_ID2,
    //            REC_ID2, MIX_ID2, BIAS_ID2, SAFE_ID2);
    //    }
    //    else if (focusIndex == 2)
    //    {
    //        setDistortionGraph(MODE_ID3, DRIVE_ID3,
    //            REC_ID3, MIX_ID3, BIAS_ID3, SAFE_ID3);
    //    }
    //    else if (focusIndex == 3)
    //    {
    //        setDistortionGraph(MODE_ID4, DRIVE_ID4,
    //            REC_ID4, MIX_ID4, BIAS_ID4, SAFE_ID4);
    //    }

    bool left = windowLeftButton.getToggleState();
    bool right = windowRightButton.getToggleState();
    if (left)
    { // if you select the left window, you will see audio wave and distortion function graphs.
        bandPanel.setFocusBandNum(focusIndex);
        graphPanel.setFocusBandNum(focusIndex);
    }
    else if (right)
    {
        graphPanel.setFocusBandNum(-1); // -1 means global
    }
    bandPanel.updateWhenChangingFocus();
    repaint();
}
