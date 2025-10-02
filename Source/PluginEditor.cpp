/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginEditor.h"
#include "Panels/ControlPanel/Graph Components/VUMeter.h"
#include "PluginProcessor.h"
#include "Utility/AudioHelpers.h"

//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), stateComponent { p.stateAB, p.statePresets, p.treeState }, globalPanel(processor), lfoPanel(p)
{
    processor.addChangeListener(this);
    // timer
    juce::Timer::startTimerHz(60.0f);

    lfoPanel.onAssignButtonClicked = [this](int lfoIndex)
    {
        if (isLfoAssignMode)
        {
            // If already in assignment mode, exit directly.
            // (The cleanup logic has been moved to a separate function for reusability).
            exitAssignMode();
        }
        else
        {
            // --- Enter assignment mode ---
            isLfoAssignMode = true;
            lfoSourceForAssignment = lfoIndex;
            lfoPanel.assignButton.setToggleState(true, juce::dontSendNotification);

            // Define the callback function to be executed when a slider is clicked.
            auto sliderClickCallback = [this](const juce::String& parameterID)
            {
                // This callback now does only one thing: tell the processor to assign the LFO.
                // It no longer handles any UI state changes.
                if (isLfoAssignMode) // Check again just in case.
                {
                    processor.assignLfoToTarget(lfoSourceForAssignment, parameterID);
                }
            };

            // Assign the callback function to all modulatable sliders.
            for (auto* slider : bandPanel.modulatableSliders)
                slider->onClickInAssignMode = sliderClickCallback;
            for (auto* slider : globalPanel.modulatableSliders)
                slider->onClickInAssignMode = sliderClickCallback;
        }
    };

    lfoPanel.setOnDataChangedCallback([this]
                                      { processor.lfoDataHasChanged(); });

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
        bool shouldCheckForUpdate = processor.getAppSettings().getBoolValue(AUTO_UPDATE_ID, true);
        // 2. If not, check the user's preference from the now-loaded state.
        if (shouldCheckForUpdate)
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
    addAndMakeVisible(lfoPanel);

    // Spectrum
    addAndMakeVisible(specBackground);
    addAndMakeVisible(processedSpectrum);
    addAndMakeVisible(originalSpectrum);
    addAndMakeVisible(multiband);
    multiband.addMouseListener(this, false);
    updateWhenChangingFocus();
    addAndMakeVisible(filterControl);

    // Listen to ALL relevant buttons
    for (int i = 0; i < 4; ++i)
        multiband.getEnableButton(i).addListener(this);

    // Add the editor as a listener DIRECTLY to BandPanel's public buttons
    bandPanel.compressorBypassButton.addListener(this);
    bandPanel.widthBypassButton.addListener(this);

    processedSpectrum.setInterceptsMouseClicks(false, false);
    processedSpectrum.updateSpectrum(processor.getFFTData(1), processor.getNumBins(), processor.getSampleRate() / (float) processor.getFFTSize());
    originalSpectrum.updateSpectrum(processor.getFFTData(0), processor.getNumBins(), processor.getSampleRate() / (float) processor.getFFTSize());

    // presets
    addAndMakeVisible(stateComponent);
    stateComponent.getPresetBox()->addListener(this);
    stateComponent.getToggleABButton()->addListener(this);
    stateComponent.getCopyABButton()->addListener(this);
    stateComponent.getPreviousButton()->addListener(this);
    stateComponent.getNextButton()->addListener(this);

    setLookAndFeel(&fireLookAndFeel);

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
    windowRightButton.addListener(this);

    // Setup for the new LFO button
    addAndMakeVisible(windowLfoButton);
    windowLfoButton.setClickingTogglesState(true);
    windowLfoButton.setRadioGroupId(windowButtons);
    windowLfoButton.setButtonText("LFO");
    windowLfoButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    windowLfoButton.setColour(juce::TextButton::buttonColourId, COLOUR6.withAlpha(0.5f));
    windowLfoButton.setColour(juce::TextButton::buttonOnColourId, COLOUR7);
    windowLfoButton.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.0f));
    windowLfoButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    windowLfoButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkgrey);
    windowLfoButton.addListener(this);

    const bool isBandView = windowLeftButton.getToggleState();
    const bool isGlobalView = windowRightButton.getToggleState();
    const bool isLfoView = windowLfoButton.getToggleState();

    multiband.setVisible(isBandView || isLfoView);
    bandPanel.setVisible(isBandView);
    globalPanel.setVisible(isGlobalView);
    lfoPanel.setVisible(isLfoView);
    filterControl.setVisible(isGlobalView);
    graphPanel.setVisible(isBandView || isGlobalView);
    setFourComponentsVisibility(distortionMode1, distortionMode2, distortionMode3, distortionMode4, focusIndex, isBandView);

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
    zoomButton.setComponentID("zoom");

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

    modeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, ParameterIDAndName::getIDString(MODE_ID, 0), distortionMode1);
    modeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, ParameterIDAndName::getIDString(MODE_ID, 1), distortionMode2);
    modeAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, ParameterIDAndName::getIDString(MODE_ID, 2), distortionMode3);
    modeAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, ParameterIDAndName::getIDString(MODE_ID, 3), distortionMode4);

    // set resize
    setResizable(true, true);
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    // resize limit
    setResizeLimits(INIT_WIDTH, INIT_HEIGHT, 2000, 1000); // set resize limits
    getConstrainer()->setFixedAspectRatio(2); // set fixed resize rate

    setMultiband();

    graphPanel.setLayoutMode(GraphPanel::LayoutMode::Band);

    auto& params = processor.getParameters();
    for (auto param : params)
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            processor.treeState.addParameterListener(p->paramID, this);
        }
    }
}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    stopTimer();

    auto& params = processor.getParameters();
    for (auto param : params)
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            processor.treeState.removeParameterListener(p->paramID, this);
        }
    }

    // Mouse Listeners
    graphPanel.removeMouseListener(this);
    multiband.removeMouseListener(this);

    // StateComponent Listeners
    stateComponent.getPresetBox()->removeListener(this);
    stateComponent.getToggleABButton()->removeListener(this);
    stateComponent.getCopyABButton()->removeListener(this);
    stateComponent.getPreviousButton()->removeListener(this);
    stateComponent.getNextButton()->removeListener(this);

    // Window Button Listeners
    windowLeftButton.removeListener(this);
    windowRightButton.removeListener(this);
    windowLfoButton.removeListener(this);

    // Distortion Mode ComboBox Listeners
    distortionMode1.removeListener(this);
    distortionMode2.removeListener(this);
    distortionMode3.removeListener(this);
    distortionMode4.removeListener(this);

    // Other Button Listeners
    zoomButton.removeListener(this);

    for (int i = 0; i < 4; ++i)
    {
        multiband.getEnableButton(i).removeListener(this);
    }

    bandPanel.compressorBypassButton.removeListener(this);
    bandPanel.widthBypassButton.removeListener(this);

    setLookAndFeel(nullptr);
    processor.removeChangeListener(this);
}

void FireAudioProcessorEditor::markPresetAsDirty()
{
    stateComponent.markAsDirty();
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
    float part1 = getHeight() / 10.0f;
    float part2 = part1 * 3.0f;

    // background
    g.setColour(COLOUR7);
    g.fillRect(0.0f, getHeight() * 3.0f / 10.0f, static_cast<float>(getWidth()), getHeight() * 7.0f / 10.0f);

    // title
    g.setColour(COLOUR5);
    const float scale = getWidth() / (float) INIT_WIDTH;
    const auto topBarHeight = juce::roundToInt(50.0f * scale);
    g.fillRect(0, 0, getWidth(), topBarHeight);

    // draw version
    g.setColour(COLOUR5);
    g.setFont(juce::Font {
        juce::FontOptions()
            .withName("Times New Roman")
            .withHeight(18.0f)
            .withStyle("Bold") });
    juce::String version = (juce::String) VERSION;
    juce::Rectangle<int> area(getWidth() - 50, getHeight() - 25, 100, 50);
    g.drawFittedText(version, area, juce::Justification::topLeft, 1);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t) BinaryData::firelogo_pngSize);
    g.drawImageWithin(logo, logoArea.getX(), logoArea.getY(), logoArea.getWidth(), logoArea.getHeight(), juce::RectanglePlacement::centred);

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t) BinaryData::firewingslogo_pngSize);
    g.drawImageWithin(logoWings, wingsArea.getX(), wingsArea.getY(), wingsArea.getWidth(), wingsArea.getHeight(), juce::RectanglePlacement::centred);

    auto frame = getLocalBounds();
    frame.setBounds(0, part1, getWidth(), part2);

    int focusIndex = 0;
    focusIndex = multiband.getFocusIndex();

    // TODO: change it to mouse click
    setFourComponentsVisibility(distortionMode1, distortionMode2, distortionMode3, distortionMode4, focusIndex, windowLeftButton.getToggleState());

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
    const float scale = juce::jmin(getHeight() / (float) INIT_HEIGHT, getWidth() / (float) INIT_WIDTH);
    // set look and feel scale
    fireLookAndFeel.scale = scale;

    juce::Rectangle<int> bounds(getLocalBounds());

    // ===== Top Bar Layout =====
    const auto topBarHeight = juce::roundToInt(50.0f * scale);
    auto topBar = bounds.removeFromTop(topBarHeight);

    // 1. Reserve space for the right logo (which is painted manually)
    wingsArea = topBar.removeFromRight(topBar.getHeight());

    // 2. Reserve space for the left logo (which is painted manually)
    logoArea = topBar.removeFromLeft(topBar.getHeight());

    // 3. Place the HQ button immediately to the right of the logo's space
    hqButton.setBounds(topBar.removeFromLeft(topBar.getHeight()));

    // 4. Reserve space for the right logo
    topBar.removeFromRight(topBar.getHeight());

    // 5. The StateComponent takes all the remaining space in the middle
    stateComponent.setBounds(topBar);

    // spectrum and filter
    if (zoomButton.getToggleState())
    {
        specBackground.setBounds(bounds);
        processedSpectrum.setBounds(bounds);
        originalSpectrum.setBounds(bounds);
        multiband.setBounds(bounds);
        filterControl.setBounds(bounds);
    }
    else
    {
        // 1. spectrum
        auto spectrumArea = bounds.removeFromTop(SPEC_HEIGHT);
        specBackground.setBounds(spectrumArea);
        processedSpectrum.setBounds(spectrumArea);
        originalSpectrum.setBounds(spectrumArea);
        multiband.setBounds(spectrumArea);
        filterControl.setBounds(spectrumArea);

        // 2. Switch
        const auto windowButtonHeight = juce::roundToInt(getHeight() / 20.0f);
        auto windowButtonsArea = bounds.removeFromTop(windowButtonHeight);

        const int buttonWidth = windowButtonsArea.getWidth() / 3;
        windowLeftButton.setBounds(windowButtonsArea.removeFromLeft(buttonWidth));
        windowLfoButton.setBounds(windowButtonsArea.removeFromLeft(buttonWidth));
        windowRightButton.setBounds(windowButtonsArea);

        // 3. Left and right margin
        bounds.removeFromLeft(juce::roundToInt(getWidth() / 20.0f));
        bounds.removeFromRight(juce::roundToInt(getWidth() / 20.0f));

        // First, determine the current view state
        const bool isBandView = windowLeftButton.getToggleState();
        const bool isGlobalView = windowRightButton.getToggleState();

        auto mainControlsArea = bounds;
        mainControlsArea.removeFromBottom(mainControlsArea.getHeight() / 6);
        auto topSection = mainControlsArea.removeFromTop(mainControlsArea.getHeight() / 5);

        if (isBandView)
        {
            // --- Band View Layout (Your Original Logic) ---
            auto mainControlsAreaForBand = bounds;
            // auto mainControlsArea = bounds;
            // mainControlsArea.removeFromBottom(mainControlsArea.getHeight() / 6);

            // auto topSection = mainControlsArea.removeFromTop(mainControlsArea.getHeight() / 5);

            auto distortionModeArea = topSection.removeFromLeft(juce::roundToInt(OSC_WIDTH));
            distortionModeArea.reduce(0, topSection.getHeight() / 4);
            distortionMode1.setBounds(distortionModeArea);
            distortionMode2.setBounds(distortionModeArea);
            distortionMode3.setBounds(distortionModeArea);
            distortionMode4.setBounds(distortionModeArea);

            const int graphPanelWidth = juce::roundToInt(getWidth() / 7.0f * 2.0f);
            auto graphArea = mainControlsArea.removeFromLeft(graphPanelWidth);
            mainControlsAreaForBand.removeFromLeft(graphPanelWidth);
            graphPanel.setBounds(graphArea);

            bandPanel.setBounds(mainControlsAreaForBand);
        }
        else if (isGlobalView)
        {
            const int graphPanelWidth = juce::roundToInt(getWidth() / 6.0f);
            auto graphArea = mainControlsArea;
            graphArea.removeFromRight(graphArea.getWidth() - graphPanelWidth);
            graphPanel.setBounds(graphArea);

            auto rightHandPanelsArea = mainControlsArea;
            rightHandPanelsArea.removeFromLeft(graphPanelWidth);
            globalPanel.setBounds(rightHandPanelsArea);
        }
        else // isLfoView
        {
            lfoPanel.setBounds(mainControlsArea);
        }
    }

    // Zoom button
    zoomButton.setBounds(getWidth() - 30.0f * scale,
                         multiband.getY() + multiband.getHeight() - 30.0f * scale,
                         getHeight() / 25.0f,
                         getHeight() / 25.0f);
}

void FireAudioProcessorEditor::timerCallback()
{
    if (isLfoAssignMode)
    {
        // Increment the angle for the sine wave animation
        assignModePulseAngle += 0.05f;
        if (assignModePulseAngle > juce::MathConstants<float>::twoPi)
            assignModePulseAngle -= juce::MathConstants<float>::twoPi;

        // Calculate a smooth alpha value using a sine wave, mapped to a pleasant range (e.g., 0.1 to 0.4)
        float minAlpha = 0.1f;
        float maxAlpha = 0.4f;
        assignModePulseAlpha = minAlpha + (maxAlpha - minAlpha) * ((std::sin(assignModePulseAngle) + 1.0f) / 2.0f);

        // Pass the new alpha value to all modulatable sliders
        for (auto* slider : bandPanel.modulatableSliders)
            slider->assignModeGlowAlpha = assignModePulseAlpha;
        for (auto* slider : globalPanel.modulatableSliders)
            slider->assignModeGlowAlpha = assignModePulseAlpha;

        // Repaint the panels on every frame to ensure smooth animation
        bandPanel.repaint();
        globalPanel.repaint();
    }

    auto updateSliderState = [&](ModulatableSlider& slider)
    {
        if (slider.parameterID.isEmpty())
            return;

        auto modInfo = processor.getModulationInfoForParameter(slider.parameterID);
        slider.isModulated = modInfo.isModulated;

        // Always update the values. If not modulated, depth will be 0 from getModulationInfoForParameter.
        slider.lfoSource = modInfo.sourceLfoIndex;
        slider.lfoAmount = modInfo.depth;
        slider.lfoValue = modInfo.currentValue;
        slider.isBipolar = modInfo.isBipolar;
    };

    // Loop through the public list of modulatable sliders from the bandPanel.
    // This is much cleaner and automatically adapts to any new sliders you add.
    for (auto* slider : bandPanel.modulatableSliders)
    {
        if (slider != nullptr) // A good safety check
        {
            updateSliderState(*slider);
        }
    }
    // Repaint the entire panel once, which is more efficient than repainting individual sliders.
    bandPanel.repaint();

    for (auto* slider : globalPanel.modulatableSliders)
    {
        if (slider != nullptr)
        {
            updateSliderState(*slider);
        }
    }
    globalPanel.repaint();

    int currentBand = bandPanel.getFocusBandNum();
    setDistortionGraph(ParameterIDAndName::getIDString(MODE_ID, currentBand),
                       ParameterIDAndName::getIDString(DRIVE_ID, currentBand),
                       ParameterIDAndName::getIDString(REC_ID, currentBand),
                       ParameterIDAndName::getIDString(MIX_ID, currentBand),
                       ParameterIDAndName::getIDString(BIAS_ID, currentBand),
                       ParameterIDAndName::getIDString(SAFE_ID, currentBand),
                       currentBand);

    float realtimeThreshold = processor.getRealtimeModulatedThreshold(currentBand);
    graphPanel.getVuPanel()->updateRealtimeThreshold(realtimeThreshold);

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
        processedSpectrum.updateSpectrum(tempFFTDataProcessed, processor.getNumBins(), processor.getSampleRate() / (float) processor.getFFTSize());
        originalSpectrum.updateSpectrum(tempFFTDataOriginal, processor.getNumBins(), processor.getSampleRate() / (float) processor.getFFTSize());
        bandPanel.updateDriveMeter();
        graphPanel.repaint();
        processedSpectrum.repaint();
        originalSpectrum.repaint();
        multiband.repaint();
        bandPanel.repaint();
        globalPanel.repaint();
        lfoPanel.repaint();
    }
}

void FireAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
}

void FireAudioProcessorEditor::updateMainPanelVisibility()
{
    const bool isBandView = windowLeftButton.getToggleState();
    const bool isGlobalView = windowRightButton.getToggleState();
    const bool isLfoView = windowLfoButton.getToggleState();

    multiband.setVisible(isBandView || isLfoView);
    bandPanel.setVisible(isBandView);
    globalPanel.setVisible(isGlobalView);
    lfoPanel.setVisible(isLfoView);
    filterControl.setVisible(isGlobalView);

    // The graph panel should be visible for Band and Global views, but not LFO view.
    graphPanel.setVisible(isBandView || isGlobalView);
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
    if (multiband.getStateComponent().getChangedState())
    {
        multiband.setFocusIndex(0);
        multiband.getStateComponent().setChangedState(false);
    }
    if (clickedButton == &zoomButton)
    {
        // Since setClickingTogglesState is false, we manually toggle the state.
        // This flips the state from its previous value.
        zoomButton.setToggleState(! zoomButton.getToggleState(), juce::dontSendNotification);

        const bool isNowZoomed = zoomButton.getToggleState();

        // Define all components that are hidden when zoomed.
        std::array<juce::Component*, 7> componentsToHideOnZoom = {
            &windowLeftButton, &windowRightButton, &windowLfoButton, &distortionMode1, &distortionMode2, &distortionMode3, &distortionMode4
        };

        // Set visibility for the top-level controls.
        // If we are zoomed, these are NOT visible. If not zoomed, they ARE visible.
        for (auto* comp : componentsToHideOnZoom)
            comp->setVisible(! isNowZoomed);

        if (isNowZoomed)
        {
            // When entering zoom, hide all main panels.
            graphPanel.setVisible(false);
            bandPanel.setVisible(false);
            globalPanel.setVisible(false);
            lfoPanel.setVisible(false);
        }
        else
        {
            // When exiting zoom, restore the correct panel visibility using our helper function.
            updateMainPanelVisibility();
        }
        resized();
    }
    if (clickedButton == &windowLeftButton || clickedButton == &windowRightButton || clickedButton == &windowLfoButton)
    {
        const bool isBandView = windowLeftButton.getToggleState();
        const bool isGlobalView = windowRightButton.getToggleState();
        const bool isLfoView = windowLfoButton.getToggleState();

        bandPanel.setVisible(isBandView);
        globalPanel.setVisible(isGlobalView);
        lfoPanel.setVisible(isLfoView);

        multiband.setVisible(isBandView || isLfoView);
        filterControl.setVisible(isGlobalView);

        graphPanel.setVisible(isBandView || isGlobalView);

        if (isBandView)
        {
            graphPanel.setLayoutMode(GraphPanel::LayoutMode::Band);
        }
        else if (isGlobalView)
        {
            graphPanel.setLayoutMode(GraphPanel::LayoutMode::Global);
        }

        setFourComponentsVisibility(distortionMode1, distortionMode2, distortionMode3, distortionMode4, focusIndex, isBandView);

        resized();
    }
    for (int i = 0; i < 4; i++)
    {
        // bypass button for each band
        if (clickedButton == &multiband.getEnableButton(i))
        {
            // Only command the panel if the button belongs to the currently viewed band
            if (i == bandPanel.getFocusBandNum())
            {
                bandPanel.setBandKnobsStates(clickedButton->getToggleState(), false);
            }
        }
    }
    // This block replaces the one you just deleted.
    if (clickedButton == &bandPanel.compressorBypassButton || clickedButton == &bandPanel.widthBypassButton)
    {
        // 1. Command the BandPanel to save the new bypass state to its internal "memory".
        bandPanel.saveBypassStatesToMemory();

        // 2. Command the BandPanel to re-evaluate all knob states based on this new bypass setting.
        // We pass the current state of the main enableButton to ensure correct logic.
        int focusBand = bandPanel.getFocusBandNum();
        bool isMainBandEnabled = multiband.getEnableButton(focusBand).getToggleState();
        bandPanel.setBandKnobsStates(isMainBandEnabled, true); // Use 'true' to prevent feedback on the bypass button's visual state
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
        setMultiband();
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

void FireAudioProcessorEditor::setDistortionGraph(juce::String modeId, juce::String driveId, juce::String recId, juce::String mixId, juce::String biasId, juce::String safeId, int bandIndex)
{
    // paint distortion function
    int mode = static_cast<int>(*processor.treeState.getRawParameterValue(modeId));
    float drive = *processor.treeState.getRawParameterValue(driveId);
    float rec = *processor.treeState.getRawParameterValue(recId);
    float mix = *processor.treeState.getRawParameterValue(mixId);
    float bias = *processor.treeState.getRawParameterValue(biasId);
    bool isSafeModeOn = *processor.treeState.getRawParameterValue(safeId);

    // protection
    drive = drive * 6.5f / 100.0f;
    float powerDrive = powf(2, drive);
    float sampleMaxValue = processor.getSampleMaxValue(bandIndex);

    if (isSafeModeOn && sampleMaxValue * powerDrive > 2.0f)
    {
        drive = 2.0f / sampleMaxValue + 0.1 * std::log2f(powerDrive);
    }
    else
    {
        drive = powerDrive;
    }

    // get modulated value
    auto getModulatedValue = [&](const juce::String& paramId, float baseValue) -> float
    {
        auto modInfo = processor.getModulationInfoForParameter(paramId);
        if (! modInfo.isModulated)
            return baseValue;

        if (auto* parameter = processor.treeState.getParameter(paramId))
        {
            auto range = parameter->getNormalisableRange();
            float parameterRange = range.end - range.start;
            float modulationOffset = 0.0f;

            if (modInfo.isBipolar)
                modulationOffset = modInfo.currentValue * modInfo.depth * parameterRange * 0.5f;
            else
                modulationOffset = modInfo.currentValue * modInfo.depth * parameterRange;

            return juce::jlimit(range.start, range.end, baseValue + modulationOffset);
        }
        return baseValue;
    };

    // apply modulation
    float finalRec = getModulatedValue(recId, rec);
    float finalBias = getModulatedValue(biasId, bias);

    // apply downsampling
    float rateDivide = static_cast<float>(*processor.treeState.getRawParameterValue(DOWNSAMPLE_ID));
    if (! *processor.treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID))
        rateDivide = 1;

    graphPanel.getDistortionGraph()->setState(mode, finalRec, mix, finalBias, drive, rateDivide);
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

void FireAudioProcessorEditor::setFourComponentsVisibility(juce::Component& component1, juce::Component& component2, juce::Component& component3, juce::Component& component4, int bandNum, bool isComboboxVisible)
{
    const bool shouldShowAny = ! zoomButton.getToggleState() && isComboboxVisible;

    component1.setVisible(shouldShowAny && (bandNum == 0));
    component2.setVisible(shouldShowAny && (bandNum == 1));
    component3.setVisible(shouldShowAny && (bandNum == 2));
    component4.setVisible(shouldShowAny && (bandNum == 3));
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
        graphPanel.toggleZoom(graphPanel.getOscilloscope()); // ++ ADD THIS LINE
    }
    else if (e.eventComponent == graphPanel.getDistortionGraph())
    {
        bandPanel.setSwitch(1, true);
        graphPanel.toggleZoom(graphPanel.getDistortionGraph()); // ++ ADD THIS LINE
    }
    else if (e.eventComponent == graphPanel.getVuPanel())
    {
        bandPanel.setSwitch(2, true);
        graphPanel.toggleZoom(graphPanel.getVuPanel()); // ++ ADD THIS LINE
    }
    else if (e.eventComponent == graphPanel.getWidthGraph())
    {
        bandPanel.setSwitch(3, true);
        graphPanel.toggleZoom(graphPanel.getWidthGraph()); // ++ ADD THIS LINE
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

void FireAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    triggerAsyncUpdate();
}

void FireAudioProcessorEditor::handleAsyncUpdate()
{
    // This function is now the single entry point after a successful assignment.

    // 1. First, formally exit the assignment mode and clean up all related states.
    exitAssignMode();

    // 2. Then, repaint all panels.
    //    Since the modulation relationship has now been established in the Processor,
    //    updateSliderState in timerCallback will get the latest state and display it correctly.
    bandPanel.repaint();
    globalPanel.repaint();

    // 3. Finally, repaint the entire editor to ensure all UI elements are synchronized.
    repaint();
}

void FireAudioProcessorEditor::exitAssignMode()
{
    if (! isLfoAssignMode)
        return;

    isLfoAssignMode = false;
    lfoPanel.assignButton.setToggleState(false, juce::dontSendNotification);

    // Iterate through all modulatable sliders, clear their callback functions and stop flashing.
    for (auto* slider : bandPanel.modulatableSliders)
    {
        slider->onClickInAssignMode = nullptr;
        slider->assignModeGlowAlpha = 0.0f;
    }
    for (auto* slider : globalPanel.modulatableSliders)
    {
        slider->onClickInAssignMode = nullptr;
        slider->assignModeGlowAlpha = 0.0f;
    }
}

void FireAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &processor)
    {
        lfoPanel.refreshLfoDisplay();
        // globalPanel.repaint();
        // bandPanel.repaint();
    }
}