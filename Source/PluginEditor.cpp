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
    : AudioProcessorEditor(&p),
      processor(p),
      stateComponent { p.stateAB, p.statePresets, p.treeState },
      // Initialize bandPanel and globalPanel with the popup callbacks
      bandPanel(p,
                // Drag callbacks
                [this](ModulatableSlider* s)
                { showValuePopupForSlider(s); },
                [this](ModulatableSlider* s)
                { updateValuePopupForSlider(s); },
                [this](ModulatableSlider* s)
                { hideValuePopup(); },
                // Hover callbacks
                [this](ModulatableSlider* s)
                { showValuePopupForSlider(s); },
                [this](ModulatableSlider* s)
                { hideValuePopup(); }),
      globalPanel(processor,
                  // Drag callbacks
                  [this](ModulatableSlider* s)
                  { showValuePopupForSlider(s); },
                  [this](ModulatableSlider* s)
                  { updateValuePopupForSlider(s); },
                  [this](ModulatableSlider* s)
                  { hideValuePopup(); },
                  // Hover callbacks
                  [this](ModulatableSlider* s)
                  { showValuePopupForSlider(s); },
                  [this](ModulatableSlider* s)
                  { hideValuePopup(); }),
      lfoPanel(p)
{
    addAndMakeVisible(valuePopup);
    valuePopup.setAlwaysOnTop(true);
    valuePopup.setVisible(false);

    addAndMakeVisible(valueEntryPopup);
    valueEntryPopup.setAlwaysOnTop(true);
    valueEntryPopup.setVisible(false);

    valueEntryPopup.onOk = [this](double value)
    {
        if (sliderForValueEntry != nullptr)
        {
            processor.setModulationValue(sliderForValueEntry->getParamID(), (float) value);
        }

        valueEntryPopup.setVisible(false);
        sliderForValueEntry = nullptr;
    };

    valueEntryPopup.onCancel = [this]()
    {
        valueEntryPopup.setVisible(false);
        sliderForValueEntry = nullptr;
    };

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
            for (auto* slider : bandPanel.getModulatableSliders())
                slider->onClickInAssignMode = sliderClickCallback;
            for (auto* slider : globalPanel.getModulatableSliders())
                slider->onClickInAssignMode = sliderClickCallback;
        }
    };

    lfoPanel.setOnDataChangedCallback([this]
                                      { processor.lfoDataHasChanged(); });

    auto bypassCallback = [this](const juce::String& parameterID)
    {
        processor.getLfoManager().toggleBypassForRouting(parameterID);
        updateModulationStates();
    };

    // Use the new helper function to get all sliders and assign the callback in a single loop
    for (auto* slider : getAllModulatableSliders())
    {
        slider->onModAmountSetValue = [this, slider](double newValue)
        {
            processor.setModulationValue(slider->getParamID(), (float) newValue);
        };

        slider->onSetValueRequested = [this](ModulatableSlider* sliderToEdit)
        {
            sliderForValueEntry = sliderToEdit;

            auto sliderBounds = sliderToEdit->getScreenBounds();

            auto localBounds = getLocalArea(nullptr, sliderBounds);

            valueEntryPopup.setBounds(localBounds.getCentreX() - 80,
                                      localBounds.getCentreY() - 30,
                                      160,
                                      60);
            valueEntryPopup.setVisible(true);
            valueEntryPopup.grabKeyboardFocus();
        };

        slider->onBypassToggled = [slider, bypassCallback]()
        {
            bypassCallback(slider->getParamID());
        };

        slider->onModulationCleared = [this, slider]()
        {
            processor.clearModulationForParameter(slider->getParamID());
        };

        slider->onModulationInverted = [this, slider]()
        {
            processor.invertModulationDepthForParameter(slider->getParamID());
        };

        slider->onBipolarModeToggled = [this, slider]()
        {
            processor.toggleBipolarMode(slider->getParamID());
        };

        slider->onModAmountChanged = [this, slider](float newDepth)
        {
            processor.setModulationDepth(slider->getParamID(), newDepth);
        };

        slider->onModulationReset = [this, slider]()
        {
            processor.resetModulation(slider->getParamID());
        };
    }

    // 1. Check if this plugin instance has ALREADY performed an update check.
    if (! processor.hasUpdateCheckBeenPerformed)
    {
        bool shouldCheckForUpdate = processor.getAppSettings().getBoolValue(AUTO_UPDATE_ID, true);
        // 2. If not, check the user's preference from the now-loaded state.
        if (shouldCheckForUpdate)
        {
            // Schedule the check on a background thread.
            juce::Timer::callAfterDelay(1000, []()
                                        {
                std::unique_ptr<VersionInfo> versionInfo = VersionInfo::fetchLatestFromUpdateServer();
                // We must use the static VERSION macro from JucePluginDefines.h here, not a member variable.
                if (versionInfo != nullptr)
                {
                    // ==============================================================
                    // ** 2. Use the new Version struct for comparison **
                    // ==============================================================
                    Version currentVersion(juce::String(VERSION));
                    Version fetchedVersion(versionInfo->versionString);

                    // Only prompt for an update if the fetched version is strictly greater than the current one.
                    if (currentVersion < fetchedVersion)
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
                    }
                } });
        }

        // 4. CRITICAL: Set the flag to true, so this check will never run again
        // for this plugin instance, even if the UI is closed and reopened.
        processor.hasUpdateCheckBeenPerformed = true;
    }

    // Add main panels
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

    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HQ_ID, hqButton);

    // zoom button
    addAndMakeVisible(zoomButton);
    zoomButton.setClickingTogglesState(false);
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

    // set resize
    setResizable(true, true);
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    // resize limit
    setResizeLimits(INIT_WIDTH, INIT_HEIGHT, 2000, 1000); // set resize limits
    getConstrainer()->setFixedAspectRatio(2); // set fixed resize rate

    multiband.resortAndRedrawLines();

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
    const float newDisplayScale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (newDisplayScale != currentDisplayScale)
    {
        currentDisplayScale = newDisplayScale;
        resized();
        return;
    }
    g.drawImage(backgroundCache, getLocalBounds().toFloat());

    int focusIndex = multiband.getFocusIndex();

    bool left = windowLeftButton.getToggleState();

    if (left)
    {
        bandPanel.setFocusBandNum(focusIndex);
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
    bandPanel.scale = scale;
    globalPanel.scale = scale;

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

        // First, determine the current view state
        const bool isBandView = windowLeftButton.getToggleState();
        const bool isGlobalView = windowRightButton.getToggleState();

        if (isBandView)
        {
            bandPanel.setBounds(bounds);
        }
        else if (isGlobalView)
        {
            globalPanel.setBounds(bounds);
        }
        else // isLfoView
        {
            lfoPanel.setBounds(bounds);
        }
    }

    // Zoom button
    zoomButton.setBounds(getWidth() - 30.0f * scale,
                         multiband.getY() + multiband.getHeight() - 30.0f * scale,
                         getHeight() / 25.0f,
                         getHeight() / 25.0f);

    const float displayScale = currentDisplayScale;

    backgroundCache = juce::Image(juce::Image::ARGB,
                                  juce::roundToInt(getWidth() * displayScale),
                                  juce::roundToInt(getHeight() * displayScale),
                                  true);
    juce::Graphics g(backgroundCache);
    g.addTransform(juce::AffineTransform::scale(displayScale));
    // background
    g.setColour(COLOUR7);
    g.fillRect(0.0f, getHeight() * 3.0f / 10.0f, static_cast<float>(getWidth()), getHeight() * 7.0f / 10.0f);

    // title
    g.setColour(COLOUR5);
    g.fillRect(0, 0, getWidth(), topBarHeight);

    // set logo "Fire"
    juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::firelogo_png, (size_t) BinaryData::firelogo_pngSize);
    g.drawImageWithin(logo, logoArea.getX(), logoArea.getY(), logoArea.getWidth(), logoArea.getHeight(), juce::RectanglePlacement::centred);

    // set logo "Wings"
    juce::Image logoWings = juce::ImageCache::getFromMemory(BinaryData::firewingslogo_png, (size_t) BinaryData::firewingslogo_pngSize);
    g.drawImageWithin(logoWings, wingsArea.getX(), wingsArea.getY(), wingsArea.getWidth(), wingsArea.getHeight(), juce::RectanglePlacement::centred);
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
        for (auto* slider : bandPanel.getModulatableSliders())
            slider->assignModeGlowAlpha = assignModePulseAlpha;
        for (auto* slider : globalPanel.getModulatableSliders())
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
    for (auto* slider : bandPanel.getModulatableSliders())
    {
        if (slider != nullptr) // A good safety check
        {
            updateSliderState(*slider);
        }
    }
    // Repaint the entire panel once, which is more efficient than repainting individual sliders.
    bandPanel.repaint();

    for (auto* slider : globalPanel.getModulatableSliders())
    {
        if (slider != nullptr)
        {
            updateSliderState(*slider);
        }
    }
    globalPanel.repaint();

    int currentBand = bandPanel.getFocusBandNum();

    // Update graphs now inside BandPanel
    bandPanel.updateRealtimeThreshold(processor.getRealtimeModulatedThreshold(currentBand));

    // bypassed
    if (processor.getBypassedState())
    {
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
        originalSpectrum.setSpecAlpha(1.0f - specAlpha);
        processedSpectrum.updateSpectrum(tempFFTDataProcessed, processor.getNumBins(), processor.getSampleRate() / (float) processor.getFFTSize());
        originalSpectrum.updateSpectrum(tempFFTDataOriginal, processor.getNumBins(), processor.getSampleRate() / (float) processor.getFFTSize());
        bandPanel.updateDriveMeter();
        processedSpectrum.repaint();
        originalSpectrum.repaint();
        multiband.repaint();

        globalPanel.repaint();
    }

    updateModulationStates();

    DistortionGraphValues latestValues;
    if (processor.getLatestDistortionGraphValues(latestValues))
    {
        // Update graph now inside BandPanel
        bandPanel.getDistortionGraph()->setState(
            latestValues.mode,
            latestValues.rec,
            latestValues.mix,
            latestValues.bias,
            latestValues.drive,
            latestValues.rateDivide);
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
        multiband.resortAndRedrawLines();
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
        std::vector<juce::Component*> componentsToHideOnZoom;
        componentsToHideOnZoom.push_back(&windowLeftButton);
        componentsToHideOnZoom.push_back(&windowRightButton);
        componentsToHideOnZoom.push_back(&windowLfoButton);

        // Distortion modes are now in BandPanel, so we don't hide them here.
        // 失真模式现在在BandPanel中，所以我们不在这里隐藏它们。

        // Set visibility for the top-level controls.
        // If we are zoomed, these are NOT visible. If not zoomed, they ARE visible.
        for (auto* comp : componentsToHideOnZoom)
            comp->setVisible(! isNowZoomed);

        if (isNowZoomed)
        {
            // When entering zoom, hide all main panels.
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
        updateMainPanelVisibility();
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
        multiband.resortAndRedrawLines();
    }
}

void FireAudioProcessorEditor::setLinearSlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
}

void FireAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    // The logic for clicking on graphs has been moved to BandPanel.
    // 单击图形的逻辑已移至 BandPanel。

    if (e.eventComponent == &multiband)
    {
        updateWhenChangingFocus();
    }
}

void FireAudioProcessorEditor::updateWhenChangingFocus()
{
    focusIndex = multiband.getFocusIndex();
    bool left = windowLeftButton.getToggleState();
    if (left)
    {
        bandPanel.setFocusBandNum(focusIndex);
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
    for (auto* slider : bandPanel.getModulatableSliders())
    {
        slider->onClickInAssignMode = nullptr;
        slider->assignModeGlowAlpha = 0.0f;
    }
    for (auto* slider : globalPanel.getModulatableSliders())
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
        multiband.resortAndRedrawLines();
    }
}

void FireAudioProcessorEditor::showValuePopupForSlider(ModulatableSlider* slider)
{
    valuePopup.setVisible(true);
    updateValuePopupForSlider(slider);
}

void FireAudioProcessorEditor::updateValuePopupForSlider(ModulatableSlider* slider)
{
    if (! slider)
        return;

    auto paramID = slider->getParamID();
    auto* param = processor.treeState.getParameter(paramID);
    if (! param)
        return;

    // --- LOGIC FOR EXTREME VALUE DISPLAY ---
    // 1. Get the parameter's base value in its real-world units (e.g., -6.0f for -6dB)
    float baseValue = *processor.treeState.getRawParameterValue(paramID);

    // 2. Get all modulation data from the processor
    auto modInfo = processor.getModulationInfoForParameter(paramID);
    float extremeValue = baseValue; // Start with the base value

    // 3. If it's being modulated, calculate the extreme value in real-world units
    if (modInfo.isModulated)
    {
        auto range = param->getNormalisableRange();
        float parameterRange = range.end - range.start;
        float maxOffset = 0.0f;

        // We use 1.0f as the LFO value to calculate the maximum possible offset
        if (modInfo.isBipolar)
            maxOffset = 1.0f * modInfo.depth * parameterRange * 0.5f;
        else
            maxOffset = 1.0f * modInfo.depth * parameterRange;

        extremeValue += maxOffset;
        extremeValue = juce::jlimit(range.start, range.end, extremeValue);
    }

    // 4. Convert the final extreme value back to a normalized value [0, 1] that getText() expects
    float finalNormalizedValue = param->convertTo0to1(extremeValue);
    valuePopup.setText(param->getText(finalNormalizedValue, 0));

    // --- FIX FOR VISIBILITY (remains the same) ---
    // 5. Get slider's absolute screen bounds
    auto sliderBounds = slider->getScreenBounds();

    // 6. Convert the screen coordinates to be local to this editor component
    auto localBounds = getLocalArea(nullptr, sliderBounds);

    int popupWidth = 80;
    int popupHeight = 20;

    // 7. Set the popup's bounds using the converted local coordinates
    valuePopup.setBounds(localBounds.getCentreX() - popupWidth / 2, localBounds.getY() - popupHeight, popupWidth, popupHeight);
}

void FireAudioProcessorEditor::hideValuePopup()
{
    valuePopup.setVisible(false);
}

std::vector<ModulatableSlider*> FireAudioProcessorEditor::getAllModulatableSliders()
{
    std::vector<ModulatableSlider*> allSliders;

    // Get sliders from BandPanel
    allSliders.insert(allSliders.end(), bandPanel.getModulatableSliders().begin(), bandPanel.getModulatableSliders().end());

    // Get sliders from GlobalPanel
    allSliders.insert(allSliders.end(), globalPanel.getModulatableSliders().begin(), globalPanel.getModulatableSliders().end());

    return allSliders;
}

void FireAudioProcessorEditor::updateModulationStates()
{
    auto& routings = processor.getLfoManager().getModulationRoutings();

    for (auto* slider : getAllModulatableSliders()) // Assuming you have a way to get all sliders
    {
        bool isModulated = false;
        for (const auto& routing : routings)
        {
            if (routing.targetParameterID == slider->getParamID())
            {
                slider->isModulated = true;
                slider->lfoSource = routing.sourceLfoIndex + 1;
                slider->lfoAmount = routing.depth;
                slider->isBipolar = routing.isBipolar;
                slider->isBypassed = routing.isBypassed; // Sync the bypass state!
                isModulated = true;
                break;
            }
        }

        if (! isModulated)
        {
            slider->isModulated = false;
            slider->isBypassed = false;
        }
    }
}