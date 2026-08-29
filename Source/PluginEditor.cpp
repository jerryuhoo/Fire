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
#include "Utility/VersionInfo.h"
#include <array>

FireAudioProcessorEditor::UpdateCheckThread::UpdateCheckThread(FireAudioProcessorEditor& ownerToUse)
    : juce::Thread("Fire update check"), owner(ownerToUse)
{
}

void FireAudioProcessorEditor::UpdateCheckThread::run()
{
    // Keep the original one-second delay, but make it interruptible when the
    // editor is closed.
    if (wait(1000.0) || threadShouldExit())
        return;

    auto versionInfo = fetchOperation.fetchLatest();
    if (threadShouldExit() || versionInfo == nullptr)
        return;

    const Version currentVersion { juce::String(VERSION) };
    const Version fetchedVersion { versionInfo->versionString };
    if (currentVersion < fetchedVersion && ! threadShouldExit())
        owner.publishAvailableUpdate(versionInfo->versionString);
}

void FireAudioProcessorEditor::UpdateCheckThread::stop()
{
    signalThreadShouldExit();
    fetchOperation.cancel();
    stopThread(-1);
}

//==============================================================================
FireAudioProcessorEditor::FireAudioProcessorEditor(FireAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      stateComponent { p.stateAB, p.statePresets, p.treeState },
      // Initialize bandPanel and globalPanel with the popup callbacks
      bandPanel(p, {}, {}, {}, {}, {}),
      globalPanel(processor, {}, {}, {}, {}, {}),
      lfoPanel(p),
      updateCheckThread(*this)
{
    // Meter telemetry has one audio-thread producer and one message-thread
    // reader. Drop packets left by a previous editor before establishing this
    // editor's freshness epoch; the first visible value must come from a
    // packet published after this editor was opened.
    MeterValues staleMeterValues;
    processor.getLatestMeterValues(staleMeterValues);

    addAndMakeVisible(valuePopup);
    valuePopup.setAlwaysOnTop(true);
    valuePopup.setVisible(false);

    addChildComponent(valueEntryPopup);
    valueEntryPopup.setAlwaysOnTop(true);

    valueEntryPopup.onOk = [this](double value)
    {
        const auto targetParameterID = valueEntryTargetParameterID;
        valueEntryTargetParameterID.clear();

        if (targetParameterID.isNotEmpty())
        {
            processor.setModulationValue(targetParameterID, (float) value);
            modulationSnapshotFramesRemaining = 0;
        }
    };

    valueEntryPopup.onCancel = [this]()
    {
        valueEntryTargetParameterID.clear();
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
                    // This callback runs on the message thread, so complete the
                    // one-shot assignment interaction here instead of relying on
                    // a broad parameter-listener notification.
                    exitAssignMode();
                    modulationSnapshotFramesRemaining = 0;
                    updateModulationStates();
                }
            };

            // Assign the callback function to all modulatable sliders.
            for (auto* slider : bandPanel.getModulatableSliders())
                slider->onClickInAssignMode = sliderClickCallback;
            for (auto* slider : globalPanel.getModulatableSliders())
                slider->onClickInAssignMode = sliderClickCallback;
        }
    };

    lfoPanel.onCurrentLfoChanged = [this](int lfoIndex)
    {
        if (isLfoAssignMode)
            lfoSourceForAssignment = lfoIndex;
    };

    lfoPanel.setOnDataChangedCallback([this]
                                      { processor.lfoDataHasChanged(); });

    allModulatableSliders.reserve(bandPanel.getModulatableSliders().size()
                                  + globalPanel.getModulatableSliders().size());
    allModulatableSliders.insert(allModulatableSliders.end(),
                                 bandPanel.getModulatableSliders().begin(),
                                 bandPanel.getModulatableSliders().end());
    allModulatableSliders.insert(allModulatableSliders.end(),
                                 globalPanel.getModulatableSliders().begin(),
                                 globalPanel.getModulatableSliders().end());
    refreshModulationSnapshot();

    auto bypassCallback = [this](const juce::String& parameterID)
    {
        processor.getLfoManager().toggleBypassForRouting(parameterID);
        processor.lfoDataHasChanged();
        modulationSnapshotFramesRemaining = 0;
        updateModulationStates();
    };

    // Use the new helper function to get all sliders and assign the callback in a single loop
    for (auto* slider : getAllModulatableSliders())
    {
        // --- Main Knob Drag Callbacks (NO Value Popup) ---
        // These callbacks now only handle the special case for the drive knob.
        slider->onMainDragStart = [this](ModulatableSlider* s)
        {
            // If the dragged slider is the drive knob, trigger the graph visibility change.
            if (s == bandPanel.getDriveKnob())
                bandPanel.setGraphVisibilityForDriveDrag(true);
        };

        slider->onMainDragMove = [](ModulatableSlider*) {}; // Main knob drag does not need continuous updates here.

        slider->onMainDragEnd = [this](ModulatableSlider* s)
        {
            // If the drag ended on the drive knob, restore the graph visibility.
            if (s == bandPanel.getDriveKnob())
                bandPanel.setGraphVisibilityForDriveDrag(false);
        };

        // --- Modulation Handle Drag Callbacks (WITH Value Popup) ---
        // This is where the popup logic should be.
        slider->onModDragStart = [this](ModulatableSlider* s)
        {
            showValuePopupForSlider(s);
        };

        slider->onModDragMove = [this](ModulatableSlider* s)
        {
            updateValuePopupForSlider(s);
        };

        slider->onModDragEnd = [this](ModulatableSlider*)
        {
            hideValuePopup();
        };

        // --- Hover Callbacks (Still show popups for hover) ---
        // This behavior remains unchanged.
        slider->onHoverStart = [this](ModulatableSlider* s)
        {
            showValuePopupForSlider(s);
        };
        slider->onHoverEnd = [this](ModulatableSlider*)
        {
            hideValuePopup();
        };

        slider->onModAmountSetValue = [this, slider](double newValue)
        {
            processor.setModulationValue(slider->getParamID(), (float) newValue);
            modulationSnapshotFramesRemaining = 0;
        };

        slider->onSetValueRequested = [this](ModulatableSlider* sliderToEdit,
                                             const juce::String& targetParameterID)
        {
            if (sliderToEdit == nullptr || targetParameterID.isEmpty())
            {
                valueEntryTargetParameterID.clear();
                valueEntryPopup.setVisible(false);
                return;
            }

            // A programmatic request can replace an already-open editor even
            // without an intervening outside click. End the old session before
            // binding the shared popup to its new immutable target.
            valueEntryPopup.dismissSession();
            valueEntryTargetParameterID = targetParameterID;

            auto sliderBounds = sliderToEdit->getScreenBounds();

            auto localBounds = getLocalArea(nullptr, sliderBounds);

            const auto popupBounds = juce::Rectangle<int>(localBounds.getCentreX() - 80,
                                                           localBounds.getCentreY() - 30,
                                                           160,
                                                           60)
                                         .constrainedWithin(getLocalBounds());
            valueEntryPopup.setBounds(popupBounds);
            valueEntryPopup.setVisible(true);
            if (valueEntryPopup.isShowing())
                valueEntryPopup.grabKeyboardFocus();
        };

        slider->onLfoAssignmentRequested = [this](int lfoIndex,
                                                   const juce::String& targetParameterID)
        {
            if (! juce::isPositiveAndBelow(lfoIndex, 4)
                || targetParameterID.isEmpty())
                return;

            processor.assignLfoToTarget(lfoIndex, targetParameterID);
            modulationSnapshotFramesRemaining = 0;
            updateModulationStates();
            if (isLfoAssignMode)
                exitAssignMode();
        };

        slider->onBypassToggled = [bypassCallback](const juce::String& targetParameterID)
        {
            bypassCallback(targetParameterID);
        };

        slider->onModulationCleared = [this](const juce::String& targetParameterID)
        {
            processor.clearModulationForParameter(targetParameterID);
            modulationSnapshotFramesRemaining = 0;
        };

        slider->onModulationInverted = [this](const juce::String& targetParameterID)
        {
            processor.invertModulationDepthForParameter(targetParameterID);
            modulationSnapshotFramesRemaining = 0;
        };

        slider->onBipolarModeToggled = [this](const juce::String& targetParameterID)
        {
            processor.toggleBipolarMode(targetParameterID);
            modulationSnapshotFramesRemaining = 0;
        };

        slider->onModAmountChanged = [this, slider](float newDepth)
        {
            processor.setModulationDepth(slider->getParamID(), newDepth);
            modulationSnapshotFramesRemaining = 0;
        };

        slider->onModulationReset = [this, slider]()
        {
            processor.resetModulation(slider->getParamID());
            modulationSnapshotFramesRemaining = 0;
        };
    }

    // 1. Check if this plugin instance has ALREADY performed an update check.
    if (! processor.hasUpdateCheckBeenPerformed)
    {
        bool shouldCheckForUpdate = processor.getAppSettings().getBoolValue(AUTO_UPDATE_ID, true);
        // 2. If not, check the user's preference from the now-loaded state.
        if (shouldCheckForUpdate)
        {
            // The editor owns this thread and joins it during destruction, so
            // neither network code nor a queued UI callback can outlive the
            // plugin module.
            updateCheckThread.startThread();
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
    multiband.setFocusChangedCallback([this](int bandIndex)
    {
        updateWhenChangingFocus(bandIndex);
    });
    updateWhenChangingFocus(multiband.getFocusIndex());
    addAndMakeVisible(filterControl);

    // Listen to ALL relevant buttons
    for (int i = 0; i < 4; ++i)
        multiband.getEnableButton(i).addListener(this);

    processedSpectrum.setInterceptsMouseClicks(false, false);
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
    const auto* hqParameter = processor.treeState.getRawParameterValue(HQ_ID);
    const bool hqButtonState = hqParameter != nullptr && hqParameter->load() > 0.5f;
    hqButton.setToggleState(hqButtonState, juce::dontSendNotification);
    hqButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    hqButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    hqButton.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    hqButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::flame);
    hqButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    hqButton.setButtonText("HQ");
    hqButton.setComponentID("header_hq");
    hqButton.setTooltip("High-quality oversampling");

    // Window Left Button
    addAndMakeVisible(windowLeftButton);
    windowLeftButton.setClickingTogglesState(true);
    windowLeftButton.setRadioGroupId(windowButtons);
    windowLeftButton.setButtonText("BAND LAB");
    windowLeftButton.setToggleState(true, juce::NotificationType::dontSendNotification);
    windowLeftButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    windowLeftButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    windowLeftButton.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    windowLeftButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::flame);
    windowLeftButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    windowLeftButton.setComponentID("workspace_tab");
    windowLeftButton.getProperties().set("fireAnimatedSelection", true);
    windowLeftButton.addListener(this);

    // Window Right Button
    addAndMakeVisible(windowRightButton);
    windowRightButton.setClickingTogglesState(true);
    windowRightButton.setRadioGroupId(windowButtons);
    windowRightButton.setButtonText("MASTER LAB");
    windowRightButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    windowRightButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    windowRightButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    windowRightButton.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    windowRightButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::flame);
    windowRightButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    windowRightButton.setComponentID("workspace_tab");
    windowRightButton.getProperties().set("fireAnimatedSelection", true);
    windowRightButton.addListener(this);

    // Setup for the new LFO button
    addAndMakeVisible(windowLfoButton);
    windowLfoButton.setClickingTogglesState(true);
    windowLfoButton.setRadioGroupId(windowButtons);
    windowLfoButton.setButtonText("MOD FORGE");
    windowLfoButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    windowLfoButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    windowLfoButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    windowLfoButton.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    windowLfoButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::modulation);
    windowLfoButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    windowLfoButton.setComponentID("workspace_tab");
    windowLfoButton.getProperties().set("fireAnimatedSelection", true);
    windowLfoButton.addListener(this);

    const bool isBandView = windowLeftButton.getToggleState();
    const bool isGlobalView = windowRightButton.getToggleState();
    const bool isLfoView = windowLfoButton.getToggleState();

    activeWorkspace = isGlobalView ? 2 : (isLfoView ? 1 : 0);
    synchroniseHistorySourceForWorkspace(activeWorkspace);

    multiband.setVisible(isBandView || isLfoView);
    bandPanel.setVisible(isBandView);
    globalPanel.setVisible(isGlobalView);
    lfoPanel.setVisible(isLfoView);
    filterControl.setVisible(isGlobalView);

    workspaceSelection.snapTo(static_cast<float>(activeWorkspace));

    hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, HQ_ID, hqButton);

    // zoom button
    addAndMakeVisible(zoomButton);
    zoomButton.setClickingTogglesState(false);
    zoomButton.addListener(this);
    zoomButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface0.withAlpha(0.85f));
    zoomButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::surface2);
    zoomButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    zoomButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::whiteHot);
    zoomButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
    zoomButton.setComponentID("zoom");
    zoomButton.setTitle("Toggle spectrum zoom");
    zoomButton.setTooltip("Toggle spectrum zoom");

    initialiseHeaderEmbers();
    lastAnimationTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // set resize
    setResizable(true, true);
    setSize(processor.getSavedWidth(), processor.getSavedHeight());
    // resize limit
    setResizeLimits(INIT_WIDTH, INIT_HEIGHT, 2000, 1000); // set resize limits
    getConstrainer()->setFixedAspectRatio(2); // set fixed resize rate

    multiband.resortAndRedrawLines();

}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    // End SliderAttachment gestures before the panels (and their attachment
    // maps) begin member teardown. This also removes any hover/value popup
    // state owned by a slider that never received mouseUp from the host.
    for (auto* slider : allModulatableSliders)
        if (slider != nullptr)
            slider->dismissTransientInteraction();
    bandPanel.dismissTransientInteraction();
    globalPanel.dismissTransientInteraction();
    lfoPanel.dismissTransientInteraction();
    lfoPanel.dismissModulationMatrixDialog();
    stateComponent.dismissSettingsDialog();
    hideValuePopup();
    valueEntryPopup.dismissSession();

    stopTimer();
    cancelPendingUpdate();
    updateCheckThread.stop();
    // The worker checks threadShouldExit before publishing, but cancel once
    // more to close the narrow race between the first cancel and the join.
    cancelPendingUpdate();

    // The Multiband outlives the BandPanel member, so clear the callback before
    // member teardown can leave it referring to this editor.
    multiband.setFocusChangedCallback({});

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

    if (std::abs(newDisplayScale - currentDisplayScale) > 0.01f)
    {
        currentDisplayScale = newDisplayScale;
        rebuildBackgroundCache();
    }

    if (! backgroundCache.isNull())
        g.drawImage(backgroundCache, getLocalBounds().toFloat());
    else
        fire::ui::drawCanvas(g, getLocalBounds().toFloat());

    drawWorkspaceSelection(g);
    drawAnimatedHeader(g);
}

void FireAudioProcessorEditor::resized()
{
    processor.setSavedHeight(getHeight());
    processor.setSavedWidth(getWidth());

    const float scale = juce::jmin(getHeight() / (float) INIT_HEIGHT, getWidth() / (float) INIT_WIDTH);
    fireLookAndFeel.scale = scale;
    bandPanel.setScale(scale);
    lfoPanel.setScale(scale);
    globalPanel.setScale(scale);

    const auto gap = juce::jmax(4, juce::roundToInt(8.0f * scale));
    auto bounds = getLocalBounds();
    headerArea = bounds.removeFromTop(juce::roundToInt(56.0f * scale));

    auto headerContent = headerArea.reduced(gap, juce::jmax(3, gap / 2));
    logoArea = headerContent.removeFromLeft(juce::roundToInt(172.0f * scale));
    headerContent.removeFromLeft(gap);
    wingsArea = headerContent.removeFromRight(juce::roundToInt(112.0f * scale));
    headerContent.removeFromRight(gap);
    auto hqSlot = headerContent.removeFromRight(juce::roundToInt(44.0f * scale));
    const auto headerControlHeight = juce::roundToInt(32.0f * scale);
    hqButton.setBounds(hqSlot.withSizeKeepingCentre(hqSlot.getWidth(), headerControlHeight));
    headerContent.removeFromRight(gap);
    stateComponent.setBounds(headerContent);

    bounds.reduce(gap, gap);
    contentArea = {};
    navigationArea = {};

    if (zoomButton.getToggleState())
    {
        spectrumCardArea = bounds;
    }
    else
    {
        const auto spectrumHeight = juce::roundToInt(static_cast<float>(bounds.getHeight()) * 0.36f);
        spectrumCardArea = bounds.removeFromTop(spectrumHeight);
        bounds.removeFromTop(gap);

        navigationArea = bounds.removeFromTop(juce::roundToInt(34.0f * scale));
        bounds.removeFromTop(gap);
        contentArea = bounds;

        auto tabs = navigationArea.withSizeKeepingCentre(
            juce::jmin(navigationArea.getWidth(), juce::roundToInt(510.0f * scale)),
            juce::jmin(navigationArea.getHeight(), juce::roundToInt(30.0f * scale)));
        const auto tabGap = juce::jmax(3, juce::roundToInt(5.0f * scale));
        const int tabWidth = (tabs.getWidth() - tabGap * 2) / 3;
        windowLeftButton.setBounds(tabs.removeFromLeft(tabWidth));
        tabs.removeFromLeft(tabGap);
        windowLfoButton.setBounds(tabs.removeFromLeft(tabWidth));
        tabs.removeFromLeft(tabGap);
        windowRightButton.setBounds(tabs);

        bandPanel.setBounds(contentArea);
        globalPanel.setBounds(contentArea);
        lfoPanel.setBounds(contentArea);
    }

    const auto spectrumBounds = spectrumCardArea.reduced(1);
    specBackground.setBounds(spectrumBounds);
    processedSpectrum.setBounds(spectrumBounds);
    originalSpectrum.setBounds(spectrumBounds);
    multiband.setBounds(spectrumBounds);
    filterControl.setBounds(spectrumBounds);

    const auto zoomSize = juce::jmax(22, juce::roundToInt(27.0f * scale));
    zoomButton.setBounds(spectrumCardArea.getRight() - zoomSize - gap,
                         spectrumCardArea.getBottom() - zoomSize - gap,
                         zoomSize,
                         zoomSize);

    rebuildBackgroundCache();
}

void FireAudioProcessorEditor::visibilityChanged()
{
    juce::AudioProcessorEditor::visibilityChanged();

    if (isShowing())
    {
        synchroniseHistorySourceForWorkspace(activeWorkspace);
    }
    else
    {
        for (auto* slider : allModulatableSliders)
            if (slider != nullptr)
                slider->dismissTransientInteraction();
        bandPanel.dismissTransientInteraction();
        globalPanel.dismissTransientInteraction();
        lfoPanel.dismissTransientInteraction();
        lfoPanel.dismissModulationMatrixDialog();
        stateComponent.dismissSettingsDialog();
        hideValuePopup();
        hqButton.dismissPointerGesture();
        windowLeftButton.dismissPointerGesture();
        windowRightButton.dismissPointerGesture();
        windowLfoButton.dismissPointerGesture();
        zoomButton.dismissPointerGesture();
        stateComponent.dismissPointerGestures();
        valueEntryPopup.dismissSession();
        multiband.dismissTransientUi();
    }
}

void FireAudioProcessorEditor::rebuildBackgroundCache()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto displayScale = juce::jmax(1.0f, currentDisplayScale);
    backgroundCache = juce::Image(juce::Image::ARGB,
                                  juce::jmax(1, juce::roundToInt(getWidth() * displayScale)),
                                  juce::jmax(1, juce::roundToInt(getHeight() * displayScale)),
                                  true);
    juce::Graphics cacheGraphics(backgroundCache);
    cacheGraphics.addTransform(juce::AffineTransform::scale(displayScale));
    fire::ui::drawCanvas(cacheGraphics, getLocalBounds().toFloat());
    fire::ui::drawTechGrid(cacheGraphics, getLocalBounds().toFloat(),
                           juce::jmax(20.0f, 28.0f * fireLookAndFeel.scale), 0.055f);

    auto header = headerArea.toFloat();
    juce::ColourGradient headerFill(fire::ui::colours::surface2, header.getX(), header.getY(),
                                    fire::ui::colours::surface0, header.getRight(), header.getBottom(), false);
    headerFill.addColour(0.56, fire::ui::colours::surface1);
    cacheGraphics.setGradientFill(headerFill);
    cacheGraphics.fillRect(header);
    cacheGraphics.setColour(fire::ui::colours::hairline.withAlpha(0.82f));
    cacheGraphics.drawHorizontalLine(headerArea.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));

    // Workspace tabs intentionally sit directly on the canvas.  Their short
    // active rail is enough hierarchy and avoids another framed container.
}

void FireAudioProcessorEditor::initialiseHeaderEmbers()
{
    uint32_t state = 0x46495245u;
    auto random01 = [&state]()
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>((state >> 8) & 0x00ffffffu) / static_cast<float>(0x01000000u);
    };

    for (auto& ember : headerEmbers)
    {
        ember.x = random01();
        ember.y = random01();
        ember.velocity = 0.08f + random01() * 0.18f;
        ember.drift = (random01() - 0.5f) * 0.08f;
        ember.size = 0.65f + random01() * 1.5f;
        ember.phase = random01() * juce::MathConstants<float>::twoPi;
    }
}

void FireAudioProcessorEditor::advanceAnimations(float deltaSeconds)
{
    deltaSeconds = juce::jlimit(0.0f, 0.05f, deltaSeconds);
    animationSeconds += deltaSeconds;

    const auto focusedBand = juce::jlimit(0, 3, bandPanel.getFocusBandNum());
    const auto signalEnergy = juce::jlimit(0.0f, 1.0f,
                                           processor.getSampleMaxValue(focusedBand) * 1.8f);
    const auto targetEnergy = processor.isDawPlaying() ? 0.28f + signalEnergy * 0.72f : 0.14f;
    headerEnergy += (targetEnergy - headerEnergy) * juce::jmin(1.0f, deltaSeconds * 7.0f);

    for (auto& ember : headerEmbers)
    {
        ember.y -= ember.velocity * deltaSeconds * (0.55f + headerEnergy);
        ember.x += (ember.drift + std::sin(animationSeconds * 0.8f + ember.phase) * 0.012f)
                   * deltaSeconds;
        if (ember.y < -0.08f)
        {
            ember.y = 1.08f;
            ember.x = std::fmod(ember.x + 0.37f, 1.0f);
        }
        if (ember.x < 0.0f)
            ember.x += 1.0f;
        else if (ember.x > 1.0f)
            ember.x -= 1.0f;
    }

    if (workspaceSelection.advance(deltaSeconds, 0.07f))
        repaint(navigationArea);
}

void FireAudioProcessorEditor::drawAnimatedHeader(juce::Graphics& g)
{
    if (headerArea.isEmpty())
        return;

    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(headerArea);

    const auto particleArea = headerArea.toFloat();
    for (const auto& ember : headerEmbers)
    {
        const auto alpha = (0.07f + 0.18f * headerEnergy)
                           * (0.45f + 0.55f * std::sin(ember.phase + animationSeconds * 1.3f)
                                             * std::sin(ember.phase + animationSeconds * 1.3f));
        const juce::Point<float> point {
            particleArea.getX() + ember.x * particleArea.getWidth(),
            particleArea.getY() + ember.y * particleArea.getHeight()
        };
        g.setColour(fire::ui::colours::flame.withAlpha(alpha));
        g.fillEllipse(juce::Rectangle<float>(ember.size, ember.size * 1.7f).withCentre(point));
    }

    const auto seamY = static_cast<float>(headerArea.getBottom()) - 1.0f;
    const auto hotSpot = static_cast<float>(headerArea.getWidth())
                         * (0.18f + 0.64f * (0.5f + 0.5f * std::sin(animationSeconds * 0.42f)));
    juce::ColourGradient seam(fire::ui::colours::ember.withAlpha(0.0f), 0.0f, seamY,
                              fire::ui::colours::whiteHot.withAlpha(0.45f * headerEnergy), hotSpot, seamY, false);
    seam.addColour(0.55, fire::ui::colours::flame.withAlpha(0.36f * headerEnergy));
    seam.addColour(1.0, fire::ui::colours::ember.withAlpha(0.0f));
    g.setGradientFill(seam);
    g.fillRect(0.0f, seamY, static_cast<float>(headerArea.getWidth()), 1.0f);

    auto brand = logoArea.toFloat().reduced(2.0f, 3.0f);
    auto glyph = brand.removeFromLeft(brand.getHeight()).reduced(8.0f * fireLookAndFeel.scale);
    fire::ui::drawFireGlyph(g, glyph, headerEnergy);
    brand.removeFromLeft(5.0f * fireLookAndFeel.scale);
    auto title = brand.removeFromTop(brand.getHeight() * 0.62f);
    g.setFont(fire::ui::displayFont(18.0f * fireLookAndFeel.scale));
    g.setColour(fire::ui::colours::textPrimary);
    g.drawText("FIRE", title, juce::Justification::centredLeft);
    g.setFont(fire::ui::labelFont(8.5f * fireLookAndFeel.scale));
    g.setColour(fire::ui::colours::textMuted);
    g.drawText("MULTIBAND REACTOR", brand, juce::Justification::centredLeft);

    auto signature = wingsArea.toFloat().reduced(4.0f, 5.0f);
    auto bars = signature.removeFromLeft(24.0f * fireLookAndFeel.scale);
    for (int i = 0; i < 3; ++i)
    {
        const auto height = bars.getHeight() * (0.28f + 0.18f * static_cast<float>(i)
                                                + 0.08f * headerEnergy);
        const auto barWidth = juce::jmax(1.0f, 2.0f * fireLookAndFeel.scale);
        g.setColour(fire::ui::colours::flame.withAlpha(0.45f + 0.16f * static_cast<float>(i)));
        g.fillRoundedRectangle(bars.getX() + static_cast<float>(i) * 6.0f * fireLookAndFeel.scale,
                               bars.getCentreY() - height * 0.5f,
                               barWidth, height, barWidth * 0.5f);
    }
    g.setFont(fire::ui::labelFont(8.0f * fireLookAndFeel.scale));
    g.setColour(fire::ui::colours::textSecondary);
    g.drawText("BLUE WINGS", signature.removeFromTop(signature.getHeight() * 0.55f),
               juce::Justification::centredLeft);
    g.setFont(fire::ui::bodyFont(7.5f * fireLookAndFeel.scale));
    g.setColour(fire::ui::colours::textMuted);
    g.drawText("v" VERSION, signature, juce::Justification::centredLeft);
}

void FireAudioProcessorEditor::drawWorkspaceSelection(juce::Graphics& g)
{
    if (navigationArea.isEmpty() || zoomButton.getToggleState())
        return;

    const std::array<juce::Rectangle<float>, 3> tabBounds {
        windowLeftButton.getBounds().toFloat(),
        windowLfoButton.getBounds().toFloat(),
        windowRightButton.getBounds().toFloat()
    };
    const std::array<juce::Colour, 3> tabColours {
        fire::ui::colours::flame,
        fire::ui::colours::modulation,
        fire::ui::colours::flame
    };

    const auto position = juce::jlimit(0.0f, 2.0f, workspaceSelection.current);
    const auto lower = juce::jlimit(0, 2, static_cast<int>(std::floor(position)));
    const auto upper = juce::jmin(2, lower + 1);
    const auto amount = position - static_cast<float>(lower);

    const auto& from = tabBounds[static_cast<size_t>(lower)];
    const auto& to = tabBounds[static_cast<size_t>(upper)];
    auto selector = juce::Rectangle<float>(
        juce::jmap(amount, from.getX(), to.getX()),
        juce::jmap(amount, from.getY(), to.getY()),
        juce::jmap(amount, from.getWidth(), to.getWidth()),
        juce::jmap(amount, from.getHeight(), to.getHeight()));
    const auto accent = tabColours[static_cast<size_t>(lower)]
                            .interpolatedWith(tabColours[static_cast<size_t>(upper)], amount);

    selector = selector.reduced(0.5f);
    g.setColour(accent.withAlpha(0.075f));
    g.fillRoundedRectangle(selector, fire::ui::Metrics::radiusSmall * fireLookAndFeel.scale);

    const auto railWidth = juce::jmin(selector.getWidth() * 0.34f,
                                      30.0f * fireLookAndFeel.scale);
    const auto railHeight = juce::jmax(1.0f, 1.5f * fireLookAndFeel.scale);
    g.setColour(accent.withAlpha(0.94f));
    g.fillRoundedRectangle(selector.getCentreX() - railWidth * 0.5f,
                           selector.getBottom() - railHeight,
                           railWidth,
                           railHeight,
                           railHeight * 0.5f);
}

void FireAudioProcessorEditor::timerCallback()
{
    MeterValues latestMeterValues;
    if (processor.getLatestMeterValues(latestMeterValues))
    {
        cachedMeterValues = latestMeterValues;
        hasCachedMeterValues = true;
        ++meterPacketGeneration;
        if (meterPacketGeneration == 0)
            ++meterPacketGeneration;
    }

    const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto deltaSeconds = static_cast<float>(nowSeconds - lastAnimationTimeSeconds);
    lastAnimationTimeSeconds = nowSeconds;

    // NUM_BANDS is the authoritative topology parameter. Keep the visual
    // dividers, hit targets and BandPanel attachments coherent even when a
    // generic host surface automates only that parameter.
    multiband.synchroniseBandCountFromParameter();

    // Always consume graph telemetry, even while the host keeps this editor
    // instance hidden. A hidden frame deliberately discards the value below;
    // showing the editor again must wait for a packet from the current source
    // epoch rather than replaying FIFO backlog.
    DistortionGraphValues latestDistortionGraphValues;
    const bool hasLatestDistortionGraphValues =
        processor.getLatestDistortionGraphValues(latestDistortionGraphValues);

    // Hosts commonly keep an editor instance alive after hiding its window.
    // Keep the timer itself cheap in that state and resume from a fresh clock
    // when the peer becomes visible again.
    if (! isShowing())
    {
        for (auto* slider : allModulatableSliders)
            if (slider != nullptr)
                slider->dismissTransientInteraction();
        bandPanel.dismissTransientInteraction();
        globalPanel.dismissTransientInteraction();
        lfoPanel.dismissTransientInteraction();
        lfoPanel.dismissModulationMatrixDialog();
        stateComponent.dismissSettingsDialog();
        hideValuePopup();
        valueEntryPopup.dismissSession();
        multiband.dismissTransientUi();
        return;
    }

    if (hasLatestDistortionGraphValues)
    {
        bandPanel.getDistortionGraph()->setState(
            latestDistortionGraphValues.mode,
            latestDistortionGraphValues.rec,
            latestDistortionGraphValues.mix,
            latestDistortionGraphValues.bias,
            latestDistortionGraphValues.drive,
            latestDistortionGraphValues.rateDivide);
    }

    if (hasCachedMeterValues)
    {
        bandPanel.presentMeterValues(cachedMeterValues, meterPacketGeneration);
        globalPanel.presentMeterValues(cachedMeterValues, meterPacketGeneration);
    }

    advanceAnimations(deltaSeconds);

    ++animationFrame;
    if ((animationFrame & 1) == 0)
        repaint(headerArea);

    for (auto* slider : allModulatableSliders)
        if (slider != nullptr && slider->isShowing() && slider->advanceAnimation(deltaSeconds))
            slider->repaint();

    if (isLfoAssignMode)
    {
        assignModePulseAngle = std::fmod(animationSeconds * 3.6f,
                                         juce::MathConstants<float>::twoPi);
        assignModePulseAlpha = 0.16f + 0.20f * (0.5f + 0.5f * std::sin(assignModePulseAngle));
        for (auto* slider : allModulatableSliders)
        {
            if (slider == nullptr)
                continue;
            slider->assignModeGlowAlpha = assignModePulseAlpha;
            if (slider->isShowing())
                slider->repaint();
        }
    }

    if (--modulationSnapshotFramesRemaining <= 0)
        refreshModulationSnapshot();
    applyModulationSnapshot();

    const int currentBand = bandPanel.getFocusBandNum();

    if (bandPanel.isShowing())
    {
        bandPanel.updateRealtimeThreshold(processor.getRealtimeModulatedThreshold(currentBand));
        // Safe Drive metering is independent of the spectrum FIFO.  Poll the
        // lock-free DSP meters on the existing UI clock and let BandPanel
        // repaint only the Drive knob when the visible value changes.
        bandPanel.updateDriveMeter();
    }

    const bool isBypassed = processor.getBypassedState();
    if (isBypassed != lastBypassedState)
    {
        multiband.repaint();
        lastBypassedState = isBypassed;
    }

    if (! isBypassed && spectrumCardArea.intersects(getLocalBounds()))
    {
        if (processor.popLatestFFTFrames(processedFftFrame.data(),
                                         static_cast<int>(processedFftFrame.size()),
                                         originalFftFrame.data(),
                                         static_cast<int>(originalFftFrame.size())))
        {
            const auto fftBufferSize = static_cast<int>(processedFftFrame.size());
            if (! processor.processFFT(processedFftFrame.data(), fftBufferSize)
                || ! processor.processFFT(originalFftFrame.data(), fftBufferSize))
                return;

            const auto* mixParameter = processor.treeState.getRawParameterValue(MIX_ID);
            const float specAlpha = mixParameter != nullptr ? mixParameter->load() : 1.0f;
            processedSpectrum.setSpecAlpha(specAlpha);
            originalSpectrum.setSpecAlpha(1.0f - specAlpha);
            const float binWidth = static_cast<float>(processor.getSampleRate())
                                   / static_cast<float>(processor.getFFTSize());
            processedSpectrum.updateSpectrum(processedFftFrame.data(), processor.getNumBins(), binWidth);
            originalSpectrum.updateSpectrum(originalFftFrame.data(), processor.getNumBins(), binWidth);
        }
    }

    if ((animationFrame & 1) == 0)
        filterControl.animationTick();
    multiband.animationTick(deltaSeconds);
    bandPanel.animationTick(deltaSeconds);
    globalPanel.animationTick(deltaSeconds);
    lfoPanel.animationTick(deltaSeconds);
}

void FireAudioProcessorEditor::sliderValueChanged(juce::Slider*)
{
}

void FireAudioProcessorEditor::updateMainPanelVisibility()
{
    const auto selectedWorkspace = windowRightButton.getToggleState()
                                       ? 2
                                       : (windowLfoButton.getToggleState() ? 1 : 0);
    selectWorkspace(selectedWorkspace, false);
}

void FireAudioProcessorEditor::selectWorkspace(int targetWorkspace, bool animateSelection)
{
    targetWorkspace = juce::jlimit(0, 2, targetWorkspace);

    const bool willHideBandPanel = targetWorkspace != 0 && bandPanel.isVisible();
    const bool willHideGlobalPanel = targetWorkspace != 2 && globalPanel.isVisible();
    if (willHideBandPanel || willHideGlobalPanel)
    {
        if (willHideBandPanel)
            bandPanel.dismissTransientInteraction();

        if (willHideGlobalPanel)
            globalPanel.dismissTransientInteraction();

        hideValuePopup();
        valueEntryPopup.dismissSession();
    }

    activeWorkspace = targetWorkspace;
    // Publish the target before any graph-bearing panel becomes visible.
    // Graph visibility callbacks can synchronously repaint, so changing the
    // source afterwards exposes one frame from the previous workspace.
    synchroniseHistorySourceForWorkspace(activeWorkspace);
    if (animateSelection && isShowing() && ! navigationArea.isEmpty())
        workspaceSelection.setTarget(static_cast<float>(activeWorkspace));
    else
        workspaceSelection.snapTo(static_cast<float>(activeWorkspace));

    const std::array<juce::Component*, 3> panels { &bandPanel, &lfoPanel, &globalPanel };
    for (int index = 0; index < static_cast<int>(panels.size()); ++index)
    {
        panels[static_cast<size_t>(index)]->setAlpha(1.0f);
        panels[static_cast<size_t>(index)]->setVisible(index == activeWorkspace);
    }

    multiband.setAlpha(1.0f);
    filterControl.setAlpha(1.0f);
    multiband.setVisible(activeWorkspace != 2);
    filterControl.setVisible(activeWorkspace == 2);

    repaint(navigationArea.getUnion(contentArea));
}

void FireAudioProcessorEditor::buttonClicked(juce::Button* clickedButton)
{
    if (clickedButton == stateComponent.getToggleABButton())
    {
        const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(
            this);
        stateComponent.getProcStateAB()->toggleAB();

        if (safeThis == nullptr)
            return;

        clickedButton->setButtonText(stateComponent.getProcStateAB()->isCurrentA() ? "A" : "B");
        multiband.resortAndRedrawLines();

        if (safeThis == nullptr)
            return;
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
            for (auto* slider : allModulatableSliders)
                if (slider != nullptr)
                    slider->dismissTransientInteraction();
            hideValuePopup();
            valueEntryPopup.dismissSession();

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
        const auto targetWorkspace = clickedButton == &windowLeftButton
                                         ? 0
                                         : (clickedButton == &windowLfoButton ? 1 : 2);
        selectWorkspace(targetWorkspace, true);
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
}

void FireAudioProcessorEditor::comboBoxChanged(juce::ComboBox* combobox)
{
    if (combobox == stateComponent.getPresetBox())
    {
        const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(
            this);
        const int selectedId = combobox->getSelectedId();

        // Programmatic synchronisation always uses dontSendNotification, so a
        // real ComboBox callback is a user request to load that preset. This
        // must also work when a dirty preset has kept its manager identity but
        // cleared the ComboBox selection to allow an explicit restore.
        if (selectedId > 0)
            stateComponent.updatePresetBox(selectedId);

        if (safeThis == nullptr)
            return;

        // Resorting can synchronously notify the host through a crossover
        // repair. Keep it as the final operation in this callback.
        multiband.resortAndRedrawLines();
    }
}

void FireAudioProcessorEditor::setLinearSlider(juce::Slider& slider)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
}

void FireAudioProcessorEditor::updateWhenChangingFocus(int bandIndex)
{
    // MOD FORGE shares the focused band's history.  Keep this current even
    // while the editor has no desktop peer so the first graph frame is never
    // sourced from the processor's default global history.  MASTER LAB must
    // retain its global source while a hidden topology change clamps focus.
    if (activeWorkspace != 2)
        processor.setHistoryArray(juce::jlimit(0, 3, bandIndex));

    // Keep attachments authoritative even while BAND LAB is hidden.  The old
    // mouse-listener path missed close-button clicks and selections made from
    // MOD FORGE, leaving the visible rail and the edited DSP band out of sync.
    bandPanel.setFocusBandNum(juce::jlimit(0, 3, bandIndex), true);
    modulationSnapshotFramesRemaining = 0;
    updateModulationStates();
    repaint();
}

void FireAudioProcessorEditor::synchroniseHistorySourceForWorkspace(int workspace)
{
    processor.setHistoryArray(
        workspace == 2
            ? FireAudioProcessor::globalHistorySourceIndex
            : multiband.getFocusIndex());
}

void FireAudioProcessorEditor::handleAsyncUpdate()
{
    const auto availableVersion = takeAvailableUpdate();
    if (availableVersion.isNotEmpty())
    {
        const auto callback = juce::ModalCallbackFunction::create([availableVersion](int result)
        {
            if (result == 1)
                juce::URL(GITHUB_TAG_LINK + availableVersion).launchInDefaultBrowser();
        });

        juce::NativeMessageBox::showOkCancelBox(
            juce::AlertWindow::InfoIcon,
            "New Version",
            "New version " + availableVersion + " available, do you want to download it?",
            this,
            callback);
    }

    // Processor-side routing changes can arrive through AsyncUpdater. Refresh the
    // view here, but don't end assignment mode: only a successful slider click is
    // a one-shot assignment completion.
    modulationSnapshotFramesRemaining = 0;
    updateModulationStates();
    repaint(headerArea);
}

void FireAudioProcessorEditor::publishAvailableUpdate(const juce::String& version)
{
    {
        const juce::ScopedLock lock(updateResultLock);
        pendingUpdateVersion = version;
    }

    triggerAsyncUpdate();
}

juce::String FireAudioProcessorEditor::takeAvailableUpdate()
{
    const juce::ScopedLock lock(updateResultLock);
    auto result = pendingUpdateVersion;
    pendingUpdateVersion.clear();
    return result;
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
        if (slider->isShowing())
            slider->repaint();
    }
    for (auto* slider : globalPanel.getModulatableSliders())
    {
        slider->onClickInAssignMode = nullptr;
        slider->assignModeGlowAlpha = 0.0f;
        if (slider->isShowing())
            slider->repaint();
    }
}

void FireAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &processor)
    {
        modulationSnapshotFramesRemaining = 0;
        stateComponent.synchronisePresetSelectionFromManager();
        stateComponent.synchroniseABButtonFromManager();
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
    auto* rawValue = processor.treeState.getRawParameterValue(paramID);
    if (rawValue == nullptr)
        return;

    float baseValue = rawValue->load();

    // 2. Get all modulation data from the processor
    auto modInfo = processor.getModulationInfoForParameter(paramID);
    float extremeValue = baseValue; // Start with the base value

    // 3. Preview the LFO=1 endpoint through the same normalised-domain recipe
    // used by the audio thread. Adding a physical-unit offset is only correct
    // for linear ranges and gives misleading values for frequency and other
    // skewed parameters. This is the configured handle endpoint even while a
    // route is bypassed, so dragging its grey handle still has useful feedback.
    if (modInfo.isModulated)
    {
        const float endpointLfoSample = 1.0f;
        ModulatedValueProvider endpointProvider;
        endpointProvider.lfoSignal = &endpointLfoSample;
        endpointProvider.baseValue = baseValue;
        endpointProvider.modulationDepth = modInfo.depth;
        endpointProvider.isBipolar = modInfo.isBipolar;
        endpointProvider.range = param->getNormalisableRange();
        extremeValue = endpointProvider.get(0);
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
    const auto popupBounds = juce::Rectangle<int>(localBounds.getCentreX() - popupWidth / 2,
                                                   localBounds.getY() - popupHeight,
                                                   popupWidth,
                                                   popupHeight)
                                 .constrainedWithin(getLocalBounds());
    valuePopup.setBounds(popupBounds);
}

void FireAudioProcessorEditor::hideValuePopup()
{
    valuePopup.setVisible(false);
}

const std::vector<ModulatableSlider*>& FireAudioProcessorEditor::getAllModulatableSliders() const noexcept
{
    return allModulatableSliders;
}

void FireAudioProcessorEditor::updateModulationStates()
{
    refreshModulationSnapshot();
    applyModulationSnapshot();
}

void FireAudioProcessorEditor::refreshModulationSnapshot()
{
    modulationRoutingSnapshot = processor.getLfoManager().getModulationRoutingsCopy();
    modulationRoutingIndexBySlider.assign(allModulatableSliders.size(), -1);

    for (size_t sliderIndex = 0; sliderIndex < allModulatableSliders.size(); ++sliderIndex)
    {
        const auto* slider = allModulatableSliders[sliderIndex];
        if (slider == nullptr || slider->getParamID().isEmpty())
            continue;

        for (int routingIndex = 0; routingIndex < modulationRoutingSnapshot.size(); ++routingIndex)
        {
            if (modulationRoutingSnapshot.getReference(routingIndex).targetParameterID
                == slider->getParamID())
            {
                modulationRoutingIndexBySlider[sliderIndex] = routingIndex;
                break;
            }
        }
    }

    modulationSnapshotFramesRemaining = 15;
}

void FireAudioProcessorEditor::applyModulationSnapshot()
{
    std::array<float, 4> lfoOutputs {};
    for (int i = 0; i < static_cast<int>(lfoOutputs.size()); ++i)
        lfoOutputs[static_cast<size_t>(i)] = processor.getLfoManager().getLfoOutput(i);

    for (size_t sliderIndex = 0; sliderIndex < allModulatableSliders.size(); ++sliderIndex)
    {
        auto* slider = allModulatableSliders[sliderIndex];
        if (slider == nullptr || slider->getParamID().isEmpty())
            continue;

        const ModulationRouting* matchedRouting = nullptr;
        if (sliderIndex < modulationRoutingIndexBySlider.size())
        {
            const int routingIndex = modulationRoutingIndexBySlider[sliderIndex];
            if (juce::isPositiveAndBelow(routingIndex, modulationRoutingSnapshot.size()))
                matchedRouting = &modulationRoutingSnapshot.getReference(routingIndex);
        }

        const bool isModulated = matchedRouting != nullptr;
        const int sourceIndex = isModulated ? juce::jlimit(0, 3, matchedRouting->sourceLfoIndex) : 0;
        const float rawLfo = isModulated ? lfoOutputs[static_cast<size_t>(sourceIndex)] : 0.0f;
        const double displayLfo = isModulated && matchedRouting->isBipolar
                                      ? static_cast<double>(rawLfo * 2.0f - 1.0f)
                                      : static_cast<double>(rawLfo);
        const double depth = isModulated ? static_cast<double>(matchedRouting->depth) : 0.0;
        const bool bipolar = isModulated ? matchedRouting->isBipolar : true;
        const bool bypassed = isModulated && matchedRouting->isBypassed;

        const bool changed = slider->isModulated != isModulated
                             || slider->lfoSource != (isModulated ? sourceIndex + 1 : 0)
                             || std::abs(slider->lfoAmount - depth) > 1.0e-6
                             || std::abs(slider->lfoValue - displayLfo) > 1.0e-4
                             || slider->isBipolar != bipolar
                             || slider->isBypassed != bypassed;

        slider->isModulated = isModulated;
        slider->lfoSource = isModulated ? sourceIndex + 1 : 0;
        slider->lfoAmount = depth;
        slider->lfoValue = displayLfo;
        slider->isBipolar = bipolar;
        slider->isBypassed = bypassed;

        if (changed && slider->isShowing())
        {
            slider->repaint();
        }
    }
}
