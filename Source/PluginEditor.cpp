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
#include <utility>

namespace
{
void showLfoAssignmentResult(
    LfoPanel& panel,
    int lfoIndex,
    LfoManager::AssignmentResult result)
{
    switch (result)
    {
        case LfoManager::AssignmentResult::changed:
            panel.showAssignCompleted(lfoIndex);
            break;
        case LfoManager::AssignmentResult::unchanged:
            panel.showAssignUnchanged(lfoIndex);
            break;
        case LfoManager::AssignmentResult::capacityReached:
            panel.showAssignCapacityReached();
            break;
        case LfoManager::AssignmentResult::invalidRequest:
            panel.showAssignCancelled();
            break;
    }
}
} // namespace

FireAudioProcessorEditor::UpdateCheckThread::UpdateCheckThread(FireAudioProcessorEditor& ownerToUse)
    : juce::Thread("Fire update check"),
      owner(ownerToUse),
      sessionGeneration(owner.captureUpdateCheckSession())
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
        owner.publishAvailableUpdate(versionInfo->versionString,
                                     sessionGeneration);
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

    // Modulation writes notify the host synchronously, and a host callback may
    // delete this editor. Finish editor mutations first, or guard any required
    // post-write refresh with a SafePointer.
    valueEntryPopup.onOk = [this](double value)
    {
        const auto targetParameterID = valueEntryTargetParameterID;
        valueEntryTargetParameterID.clear();

        if (targetParameterID.isNotEmpty())
        {
            modulationSnapshotFramesRemaining = 0;
            processor.setModulationValue(targetParameterID, (float) value);
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
            lfoPanel.showAssignArmed(lfoIndex);

            // Define the callback function to be executed when a slider is clicked.
            auto sliderClickCallback = [this](const juce::String& parameterID)
            {
                if (isLfoAssignMode) // Check again just in case.
                {
                    const auto sourceLfoIndex = lfoSourceForAssignment;
                    const juce::Component::SafePointer<
                        FireAudioProcessorEditor> safeThis(this);
                    // This callback runs on the message thread, so complete the
                    // one-shot assignment interaction here instead of relying on
                    // a broad parameter-listener notification.
                    exitAssignMode(false);
                    if (safeThis == nullptr)
                        return;

                    const auto result = processor.assignLfoToTarget(
                        sourceLfoIndex, parameterID);
                    if (safeThis != nullptr)
                    {
                        showLfoAssignmentResult(safeThis->lfoPanel,
                                                sourceLfoIndex,
                                                result);
                        if (result == LfoManager::AssignmentResult::changed)
                        {
                            safeThis->modulationSnapshotFramesRemaining = 0;
                            safeThis->updateModulationStates();
                        }
                    }
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
        {
            lfoSourceForAssignment = lfoIndex;
            lfoPanel.showAssignArmed(lfoIndex);
        }
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

    auto bypassCallback =
        [safeThis = juce::Component::SafePointer<
             FireAudioProcessorEditor>(this)](
            const juce::String& parameterID)
    {
        if (safeThis == nullptr)
            return;

        auto& processorToNotify = safeThis->processor;
        if (! processorToNotify.toggleModulationBypassForParameter(
                parameterID)
            || safeThis == nullptr)
            return;

        safeThis->modulationSnapshotFramesRemaining = 0;
        safeThis->updateModulationStates();
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
            modulationSnapshotFramesRemaining = 0;
            processor.setModulationValue(slider->getParamID(), (float) newValue);
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

            const juce::Component::SafePointer<FireAudioProcessorEditor>
                safeThis(this);
            if (isLfoAssignMode)
                exitAssignMode(false);
            if (safeThis == nullptr)
                return;

            const auto result =
                processor.assignLfoToTarget(lfoIndex, targetParameterID);
            if (safeThis != nullptr)
            {
                showLfoAssignmentResult(safeThis->lfoPanel,
                                        lfoIndex,
                                        result);
                if (result == LfoManager::AssignmentResult::changed)
                {
                    safeThis->modulationSnapshotFramesRemaining = 0;
                    safeThis->updateModulationStates();
                }
            }
        };

        slider->onBypassToggled = [bypassCallback](const juce::String& targetParameterID)
        {
            bypassCallback(targetParameterID);
        };

        slider->onModulationCleared = [this](const juce::String& targetParameterID)
        {
            modulationSnapshotFramesRemaining = 0;
            processor.clearModulationForParameter(targetParameterID);
        };

        slider->onModulationInverted = [this](const juce::String& targetParameterID)
        {
            modulationSnapshotFramesRemaining = 0;
            processor.invertModulationDepthForParameter(targetParameterID);
        };

        slider->onBipolarModeToggled = [this](const juce::String& targetParameterID)
        {
            modulationSnapshotFramesRemaining = 0;
            processor.toggleBipolarMode(targetParameterID);
        };

        slider->onModAmountChanged = [this, slider](float newDepth)
        {
            modulationSnapshotFramesRemaining = 0;
            processor.setModulationDepth(slider->getParamID(), newDepth);
        };

        slider->onModulationReset = [this, slider]()
        {
            modulationSnapshotFramesRemaining = 0;
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

    // The processor may already be host-bypassed when an editor is opened.
    // Establish that state without ever painting a retained analyser frame.
    synchroniseSpectrumHostBypassState(false);

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
    hqButton.setTitle("High-quality oversampling");
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
    windowLeftButton.setTitle("Band processing workspace");
    windowLeftButton.setTooltip("Edit multiband processing");
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
    windowRightButton.setTitle("Master processing workspace");
    windowRightButton.setTooltip("Edit global processing and filters");
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
    windowLfoButton.setTitle("LFO modulation workspace");
    windowLfoButton.setTooltip("Edit and assign LFO modulation");
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
    zoomButton.setClickingTogglesState(true);
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
    const auto rawSavedEditorSize = processor.getSavedEditorSize();
    const auto savedEditorSize = FireAudioProcessor::normaliseEditorSize(
        rawSavedEditorSize.width,
        rawSavedEditorSize.height);
    // resize limit
    setResizeLimits(INIT_WIDTH, INIT_HEIGHT, 2000, 1000); // set resize limits
    getConstrainer()->setFixedAspectRatio(2); // set fixed resize rate
    setSize(savedEditorSize.width, savedEditorSize.height);

    multiband.resortAndRedrawLines();

}

FireAudioProcessorEditor::~FireAudioProcessorEditor()
{
    // Reject a worker already returning from fetchLatest(), and remove any UI
    // result before teardown can run nested message loops.
    invalidateUpdateCheckSessionForDestruction();
    dismissAvailableUpdateAlert();
    cancelPendingUpdate();

    // End SliderAttachment gestures before the panels (and their attachment
    // maps) begin member teardown. This also removes any hover/value popup
    // state owned by a slider that never received mouseUp from the host.
    for (auto* slider : allModulatableSliders)
        if (slider != nullptr)
            slider->dismissTransientInteraction();
    bandPanel.dismissTransientInteraction();
    globalPanel.dismissTransientInteraction();
    filterControl.dismissTransientInteraction();
    lfoPanel.dismissTransientInteraction();
    lfoPanel.dismissModulationMatrixDialog();
    stateComponent.dismissSettingsDialog();
    tooltipWindow.hideTip();
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
    else
    {
        // A live resize keeps painting the previous complete cache, stretched
        // to the current bounds, until the resize stream settles. If a paint
        // arrives after the debounce deadline before the editor timer does,
        // finish the pending rebuild here as well.
        rebuildPendingBackgroundCache(juce::Time::getMillisecondCounter());
    }

    if (! backgroundCache.isNull())
        g.drawImage(backgroundCache, getLocalBounds().toFloat());
    else
        fire::ui::drawCanvas(g, getLocalBounds().toFloat());

    drawWorkspaceSelection(g);
    drawAnimatedHeader(g);
}

void FireAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    const auto opacity = juce::jlimit(0.0f,
                                      1.0f,
                                      hostBypassIndicatorOpacity.current);
    if (opacity <= 0.001f || spectrumCardArea.isEmpty())
        return;

    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(spectrumCardArea);
    g.setOpacity(opacity);

    const auto scale = fireLookAndFeel.scale;
    const auto spectrumBounds = spectrumCardArea.reduced(1).toFloat();
    g.setColour(fire::ui::colours::canvas.withAlpha(0.16f));
    g.fillRoundedRectangle(spectrumBounds,
                           fire::ui::Metrics::radius * scale);

    const auto pillWidth = juce::jmin(spectrumBounds.getWidth()
                                          - 16.0f * scale,
                                      148.0f * scale);
    const auto pillHeight = juce::jmin(spectrumBounds.getHeight()
                                           - 8.0f * scale,
                                       30.0f * scale);
    if (pillWidth <= 0.0f || pillHeight <= 0.0f)
        return;

    auto pill = juce::Rectangle<float>(pillWidth, pillHeight)
                    .withCentre(spectrumBounds.getCentre());
    fire::ui::drawGlassPill(g,
                            pill,
                            fire::ui::colours::flame,
                            true,
                            false,
                            false);

    const auto dotRadius = juce::jmax(1.5f, 2.25f * scale);
    const auto dotCentre = juce::Point<float>(pill.getX() + 15.0f * scale,
                                               pill.getCentreY());
    g.setColour(fire::ui::colours::flame.withAlpha(0.92f));
    g.fillEllipse(juce::Rectangle<float>(dotRadius * 2.0f,
                                          dotRadius * 2.0f)
                      .withCentre(dotCentre));

    auto textArea = pill.reduced(12.0f * scale, 2.0f * scale)
                        .withTrimmedLeft(12.0f * scale);
    g.setFont(fire::ui::labelFont(juce::jlimit(9.0f,
                                               12.0f,
                                               10.5f * scale))
                  .withExtraKerningFactor(0.08f));
    g.setColour(fire::ui::colours::textPrimary);
    g.drawText("HOST BYPASS",
               textArea,
               juce::Justification::centred,
               false);
}

void FireAudioProcessorEditor::resized()
{
    processor.setSavedEditorSize(getWidth(), getHeight());

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

    requestBackgroundCacheRebuild();
}

void FireAudioProcessorEditor::visibilityChanged()
{
    const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(this);
    juce::AudioProcessorEditor::visibilityChanged();

    if (safeThis == nullptr)
        return;

    updateUpdateCheckVisibilitySession();

    const bool ownVisibility = isVisible();
    const bool resumedNoPeerOwnVisibility =
        ownVisibility && editorOwnVisibilityWasHidden;
    editorOwnVisibilityWasHidden = ! ownVisibility;

    if (isShowing())
    {
        hiddenUiCleanupComplete = false;
        provisionalUiCleanupComplete = false;
        // A hidden peer may have crossed an entire bypass session without an
        // editor timer tick. Reattach from a clear frame and synchronise the
        // final processor state before any retained path can be painted.
        synchroniseSpectrumHostBypassState(false);
        synchroniseHistorySourceForWorkspace(activeWorkspace);
    }
    else
    {
        bool isProvisionalVisibleState = false;
        {
            const juce::ScopedLock lock(updateResultLock);
            isProvisionalVisibleState =
                ownVisibility
                && (resumedNoPeerOwnVisibility
                    || provisionalUiCleanupComplete
                    || updateCheckVisibilityState
                           == UpdateCheckVisibilityState::provisional);
        }

        if (isProvisionalVisibleState)
        {
            // A visible Component without a desktop peer is not a completed
            // hidden session. It still gets one provisional cleanup pass,
            // but must leave a later explicit hide armed.
            hiddenUiCleanupComplete = false;
            if (provisionalUiCleanupComplete)
                return;
            provisionalUiCleanupComplete = true;
        }
        else
        {
            provisionalUiCleanupComplete = false;
            if (hiddenUiCleanupComplete)
                return;
            hiddenUiCleanupComplete = true;
        }

        // Closing or detaching the peer is a session boundary. Perform the
        // potentially broad transient-state cleanup once for that boundary,
        // rather than repeating it on every 60 Hz timer tick while hidden.
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        ++hiddenUiCleanupCountForTesting;
#endif
        tooltipWindow.hideTip();
        suspendSpectrumPresentation();
        if (isLfoAssignMode)
            exitAssignMode(false);
        lfoPanel.clearAssignFeedback();

        for (auto* slider : allModulatableSliders)
        {
            if (slider != nullptr)
                slider->dismissTransientInteraction();

            if (safeThis == nullptr)
                return;
        }

        bandPanel.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        globalPanel.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        filterControl.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        lfoPanel.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        lfoPanel.dismissModulationMatrixDialog();
        if (safeThis == nullptr)
            return;

        stateComponent.dismissSettingsDialog();
        if (safeThis == nullptr)
            return;

        hideValuePopup();
        if (safeThis == nullptr)
            return;

        hqButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        windowLeftButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        windowRightButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        windowLfoButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        zoomButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        stateComponent.dismissPointerGestures();
        if (safeThis == nullptr)
            return;

        valueEntryPopup.dismissSession();

        if (safeThis == nullptr)
            return;

        multiband.dismissTransientUi();
    }
}

void FireAudioProcessorEditor::enablementChanged()
{
    const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(this);
    juce::AudioProcessorEditor::enablementChanged();

    if (safeThis == nullptr)
        return;

    if (! isEnabled())
    {
        tooltipWindow.hideTip();
        if (isLfoAssignMode)
            exitAssignMode(false);
        lfoPanel.clearAssignFeedback();

        filterControl.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        // Disabling is a session boundary even if the same editor is enabled
        // again before a worker or alert callback arrives.
        invalidateUpdateCheckSessionForDisable();
        return;
    }

    const juce::ScopedLock lock(updateResultLock);
    if (! updateCheckDestructionStarted)
        updateCheckEnabled = true;
}

void FireAudioProcessorEditor::requestBackgroundCacheRebuild()
{
    const auto logicalSize = juce::Point<int>(getWidth(), getHeight());
    const auto displayScale = juce::jmax(1.0f, currentDisplayScale);

    if (! backgroundCache.isNull()
        && backgroundCacheLogicalSize == logicalSize
        && std::abs(backgroundCacheDisplayScale - displayScale) <= 0.01f)
    {
        // resized() is also called manually when the spectrum zoom changes,
        // even though the cached canvas/header geometry is unchanged.
        backgroundCacheRebuildPending = false;
        return;
    }

    backgroundCacheRebuildPending = true;
    backgroundCacheRebuildRequestedAtMs = juce::Time::getMillisecondCounter();

    // The first editor frame must have a complete background. Subsequent
    // resize frames can safely stretch that complete image while coalescing
    // the expensive physical-pixel allocation and redraw.
    if (backgroundCache.isNull())
        rebuildBackgroundCache();
}

void FireAudioProcessorEditor::rebuildPendingBackgroundCache(
    std::uint32_t nowMs)
{
    if (! backgroundCacheRebuildPending)
        return;

    if (static_cast<std::uint32_t>(nowMs
                                   - backgroundCacheRebuildRequestedAtMs)
        < backgroundCacheResizeDebounceMs)
        return;

    rebuildBackgroundCache();
}

void FireAudioProcessorEditor::rebuildBackgroundCache()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto displayScale = juce::jmax(1.0f, currentDisplayScale);
    const auto logicalSize = juce::Point<int>(getWidth(), getHeight());
    const auto pixelWidth = juce::jmax(
        1, juce::roundToInt(getWidth() * displayScale));
    const auto pixelHeight = juce::jmax(
        1, juce::roundToInt(getHeight() * displayScale));

    if (! backgroundCache.isNull()
        && backgroundCacheLogicalSize == logicalSize
        && std::abs(backgroundCacheDisplayScale - displayScale) <= 0.01f
        && backgroundCache.getWidth() == pixelWidth
        && backgroundCache.getHeight() == pixelHeight)
    {
        backgroundCacheRebuildPending = false;
        return;
    }

    juce::Image newBackgroundCache(
        juce::Image::ARGB, pixelWidth, pixelHeight, true);
    if (newBackgroundCache.isNull())
    {
        // Preserve the previous complete image instead of replacing it with
        // an empty cache under memory pressure. A later resize or DPI change
        // will make another bounded attempt.
        backgroundCacheRebuildPending = false;
        return;
    }

    {
        juce::Graphics cacheGraphics(newBackgroundCache);
        cacheGraphics.addTransform(
            juce::AffineTransform::scale(displayScale));
        fire::ui::drawCanvas(cacheGraphics, getLocalBounds().toFloat());
        fire::ui::drawTechGrid(
            cacheGraphics,
            getLocalBounds().toFloat(),
            juce::jmax(20.0f, 28.0f * fireLookAndFeel.scale),
            0.055f);

        auto header = headerArea.toFloat();
        juce::ColourGradient headerFill(
            fire::ui::colours::surface2,
            header.getX(),
            header.getY(),
            fire::ui::colours::surface0,
            header.getRight(),
            header.getBottom(),
            false);
        headerFill.addColour(0.56, fire::ui::colours::surface1);
        cacheGraphics.setGradientFill(headerFill);
        cacheGraphics.fillRect(header);
        cacheGraphics.setColour(
            fire::ui::colours::hairline.withAlpha(0.82f));
        cacheGraphics.drawHorizontalLine(
            headerArea.getBottom() - 1,
            0.0f,
            static_cast<float>(getWidth()));
    }

    backgroundCache = std::move(newBackgroundCache);
    backgroundCacheLogicalSize = logicalSize;
    backgroundCacheDisplayScale = displayScale;
    backgroundCacheRebuildPending = false;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    ++backgroundCacheBuildCountForTesting;
#endif
    repaint();

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

    if (hostBypassIndicatorOpacity.advance(deltaSeconds, 0.10f))
        repaint(spectrumCardArea);
}

void FireAudioProcessorEditor::synchroniseSpectrumHostBypassState(
    bool animateTransition)
{
    if (spectrumPresentationSuspended && isShowing())
    {
        // Ask the audio producer for a new capture epoch. Any completed or
        // in-flight hidden-session publication carries an older epoch and is
        // rejected by the consumer until the next audio block starts fresh.
        requiredFftCaptureEpoch = processor.requestFreshFFTFrameEpoch();
        spectrumPresentationSuspended = false;
    }

    const auto presentationEpoch =
        processor.getHostBypassPresentationEpoch();
    const bool isBypassed = processor.getBypassedState();
    const bool stateChanged = ! spectrumBypassPresentationInitialised
                           || isBypassed != lastBypassedState;
    const bool epochChanged =
        spectrumBypassPresentationInitialised
        && presentationEpoch != lastHostBypassPresentationEpoch;

    // The timer polls this helper at 60 Hz. A steady visible session needs no
    // component mutation or broad spectrum repaint. Non-animated calls are
    // still forced because they also restore a presentation suspended while
    // the editor peer was hidden.
    if (! stateChanged && ! epochChanged && animateTransition)
        return;

    // A complete host-bypass session may begin and end between two UI ticks.
    // Its monotonic processor epoch is therefore authoritative even when the
    // sampled boolean stayed false.  Drive a zero-duration logical entry
    // before the normal exit state so retained paths cannot interpolate into
    // the first post-bypass frame.
    if (epochChanged && ! lastBypassedState && ! isBypassed)
    {
        processedSpectrum.setHostBypassed(true, animateTransition);
        originalSpectrum.setHostBypassed(true, animateTransition);
    }

    processedSpectrum.setHostBypassed(isBypassed, animateTransition);
    originalSpectrum.setHostBypassed(isBypassed, animateTransition);

    if (animateTransition)
        hostBypassIndicatorOpacity.setTarget(isBypassed ? 1.0f : 0.0f);
    else
        hostBypassIndicatorOpacity.snapTo(isBypassed ? 1.0f : 0.0f);

    spectrumBypassPresentationInitialised = true;
    lastBypassedState = isBypassed;
    lastHostBypassPresentationEpoch = presentationEpoch;
    if (stateChanged || epochChanged)
        multiband.repaint();
    repaint(spectrumCardArea);
}

void FireAudioProcessorEditor::suspendSpectrumPresentation()
{
    // Hidden editors do not animate. Clear immediately and require a frame
    // published after reattachment before either spectrum can reappear.
    processedSpectrum.setHostBypassed(true, false);
    originalSpectrum.setHostBypassed(true, false);
    spectrumPresentationSuspended = true;

    // Some hosts detach and later restore the peer without delivering a
    // visibilityChanged() callback in either direction.  Mark the cached
    // presentation state dirty so the first visible timer tick cannot take
    // the steady-state fast path and leave both spectra rejecting updates.
    spectrumBypassPresentationInitialised = false;
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
    const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(this);

    // Coalesce a stream of native live-resize callbacks into one physical
    // pixel cache rebuild after the final size has remained stable briefly.
    // This runs before the hidden-editor early return so a resized detached
    // editor is already crisp when its peer is shown again.
    rebuildPendingBackgroundCache(juce::Time::getMillisecondCounter());

    // Some hosts minimise or detach their peer without changing this
    // component's own visible flag. Observe that boundary here too.
    updateUpdateCheckVisibilitySession();

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

    if (safeThis == nullptr)
        return;

    // Always consume graph telemetry, even while the host keeps this editor
    // instance hidden. A hidden frame deliberately discards the value below;
    // showing the editor again must wait for a packet from the current source
    // epoch rather than replaying FIFO backlog.
    DistortionGraphValues latestDistortionGraphValues;
    const bool hasLatestDistortionGraphValues =
        processor.getLatestDistortionGraphValues(latestDistortionGraphValues);

    // FilterControl normally drains its own latest-wins telemetry from the
    // shared animation clock. A detached/minimised peer returns before that
    // clock, so explicitly keep the queue drained and end its presentation
    // epoch while this editor is not drawable.
    if (! isShowing())
        filterControl.suspendTelemetryPresentation();

    // Hosts commonly keep an editor instance alive after hiding its window.
    // Keep the timer itself cheap in that state and resume from a fresh clock
    // when the peer becomes visible again.
    if (! isShowing())
    {
        bool visibleEditorSessionEnded = false;
        bool isProvisionalVisibleState = false;
        {
            const juce::ScopedLock lock(updateResultLock);
            const bool ownVisibility = isVisible();
            if (! ownVisibility)
                editorOwnVisibilityWasHidden = true;
            visibleEditorSessionEnded =
                updateCheckVisibilityState
                == UpdateCheckVisibilityState::hidden;
            isProvisionalVisibleState =
                ownVisibility
                && (provisionalUiCleanupComplete
                    || updateCheckVisibilityState
                           == UpdateCheckVisibilityState::provisional);
        }

        if (isProvisionalVisibleState)
        {
            hiddenUiCleanupComplete = false;
            if (provisionalUiCleanupComplete)
                return;
            provisionalUiCleanupComplete = true;
        }
        else
        {
            provisionalUiCleanupComplete = false;
            if (hiddenUiCleanupComplete)
                return;
            hiddenUiCleanupComplete = true;
        }

        // A host may detach the peer without delivering visibilityChanged().
        // The timer owns the same one-shot cleanup fallback for that case.
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
        ++hiddenUiCleanupCountForTesting;
#endif
        tooltipWindow.hideTip();
        suspendSpectrumPresentation();

        // Unlike a workspace change, a detached/minimised editor peer ends
        // the visible UI session even when the host leaves this Component's
        // own visible flag set. visibilityChanged() is therefore not
        // guaranteed to perform the Assign cleanup for this boundary. Keep
        // the normal pre-peer construction state provisional, though: it is
        // not a session that has become hidden.
        if (visibleEditorSessionEnded)
        {
            if (isLfoAssignMode)
                exitAssignMode(false);
            else
                lfoPanel.clearAssignFeedback();
        }

        if (safeThis == nullptr)
            return;

        for (auto* slider : allModulatableSliders)
        {
            if (slider != nullptr)
                slider->dismissTransientInteraction();

            if (safeThis == nullptr)
                return;
        }

        bandPanel.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        globalPanel.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        filterControl.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        lfoPanel.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        lfoPanel.dismissModulationMatrixDialog();
        if (safeThis == nullptr)
            return;

        stateComponent.dismissSettingsDialog();
        if (safeThis == nullptr)
            return;

        hideValuePopup();
        if (safeThis == nullptr)
            return;

        hqButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        windowLeftButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        windowRightButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        windowLfoButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        zoomButton.dismissPointerGesture();
        if (safeThis == nullptr)
            return;

        stateComponent.dismissPointerGestures();
        if (safeThis == nullptr)
            return;

        valueEntryPopup.dismissSession();
        if (safeThis == nullptr)
            return;

        multiband.dismissTransientUi();
        return;
    }

    // A reattached peer begins a new visible session. The next hide/detach
    // must therefore be allowed to run one cleanup pass again.
    hiddenUiCleanupComplete = false;
    provisionalUiCleanupComplete = false;
    editorOwnVisibilityWasHidden = false;

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

    synchroniseSpectrumHostBypassState(true);
    const bool isBypassed = lastBypassedState;

    if (! isBypassed && spectrumCardArea.intersects(getLocalBounds()))
    {
        if (processor.popLatestFFTFrames(processedFftFrame.data(),
                                         static_cast<int>(processedFftFrame.size()),
                                         originalFftFrame.data(),
                                         static_cast<int>(originalFftFrame.size()),
                                         requiredFftCaptureEpoch))
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
    const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(this);
    targetWorkspace = juce::jlimit(0, 2, targetWorkspace);

    const bool willHideBandPanel = targetWorkspace != 0 && bandPanel.isVisible();
    const bool willHideGlobalPanel = targetWorkspace != 2 && globalPanel.isVisible();
    if (willHideBandPanel || willHideGlobalPanel)
    {
        if (willHideBandPanel)
        {
            bandPanel.dismissTransientInteraction();

            if (safeThis == nullptr)
                return;
        }

        if (willHideGlobalPanel)
        {
            globalPanel.dismissTransientInteraction();

            if (safeThis == nullptr)
                return;
        }

        hideValuePopup();
        if (safeThis == nullptr)
            return;

        valueEntryPopup.dismissSession();
        if (safeThis == nullptr)
            return;
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

        if (safeThis == nullptr)
            return;
    }

    multiband.setAlpha(1.0f);
    filterControl.setAlpha(1.0f);
    multiband.setVisible(activeWorkspace != 2);
    if (safeThis == nullptr)
        return;

    filterControl.setVisible(activeWorkspace == 2);
    if (safeThis == nullptr)
        return;

    repaint(navigationArea.getUnion(contentArea));
}

void FireAudioProcessorEditor::buttonClicked(juce::Button* clickedButton)
{
    const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(this);

    if (clickedButton == stateComponent.getToggleABButton())
    {
        stateComponent.getProcStateAB()->toggleAB();

        if (safeThis == nullptr)
            return;

        clickedButton->setButtonText(stateComponent.getProcStateAB()->isCurrentA() ? "A" : "B");
        multiband.resortAndRedrawLines();

        if (safeThis == nullptr)
            return;
    }
    if (clickedButton == &zoomButton)
    {
        // The button owns its toggle transition, so pointer, keyboard and
        // accessibility activation all expose the same checked state.
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
        {
            comp->setVisible(! isNowZoomed);

            if (safeThis == nullptr)
                return;
        }

        if (isNowZoomed)
        {
            for (auto* slider : allModulatableSliders)
            {
                if (slider != nullptr)
                    slider->dismissTransientInteraction();

                if (safeThis == nullptr)
                    return;
            }

            hideValuePopup();
            if (safeThis == nullptr)
                return;

            valueEntryPopup.dismissSession();
            if (safeThis == nullptr)
                return;

            // When entering zoom, hide all main panels.
            bandPanel.setVisible(false);
            if (safeThis == nullptr)
                return;

            globalPanel.setVisible(false);
            if (safeThis == nullptr)
                return;

            lfoPanel.setVisible(false);
            if (safeThis == nullptr)
                return;
        }
        else
        {
            // When exiting zoom, restore the correct panel visibility using our helper function.
            updateMainPanelVisibility();

            if (safeThis == nullptr)
                return;
        }
        resized();
        return;
    }
    if (clickedButton == &windowLeftButton || clickedButton == &windowRightButton || clickedButton == &windowLfoButton)
    {
        const auto targetWorkspace = clickedButton == &windowLeftButton
                                         ? 0
                                         : (clickedButton == &windowLfoButton ? 1 : 2);
        selectWorkspace(targetWorkspace, true);
        return;
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

                if (safeThis == nullptr)
                    return;
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
    const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(this);

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

    if (safeThis == nullptr)
        return;

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
        const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(
            this);
        presentAvailableUpdate(availableVersion);

        if (safeThis == nullptr)
            return;
    }

    // Processor-side routing changes can arrive through AsyncUpdater. Refresh the
    // view here, but don't end assignment mode: only a successful slider click is
    // a one-shot assignment completion.
    modulationSnapshotFramesRemaining = 0;
    updateModulationStates();
    repaint(headerArea);
}

std::uint64_t FireAudioProcessorEditor::captureUpdateCheckSession()
{
    const juce::ScopedLock lock(updateResultLock);
    return updateCheckDestructionStarted || ! updateCheckEnabled
               ? 0
               : updateCheckSessionGeneration;
}

bool FireAudioProcessorEditor::publishAvailableUpdate(
    const juce::String& version,
    std::uint64_t sessionGeneration)
{
    if (version.isEmpty() || sessionGeneration == 0)
        return false;

    {
        const juce::ScopedLock lock(updateResultLock);
        if (updateCheckDestructionStarted
            || ! updateCheckEnabled
            || updateCheckVisibilityState
                   == UpdateCheckVisibilityState::hidden
            || sessionGeneration != updateCheckSessionGeneration)
            return false;

        pendingUpdateResult = { version, sessionGeneration };
    }

    triggerAsyncUpdate();
    return true;
}

juce::String FireAudioProcessorEditor::takeAvailableUpdate()
{
    // This is a message-thread consumer, so it can safely establish the first
    // visible session or observe a peer that has just disappeared.
    updateUpdateCheckVisibilitySession(false);

    const juce::ScopedLock lock(updateResultLock);
    if (pendingUpdateResult.version.isEmpty())
        return {};

    if (updateCheckDestructionStarted
        || ! updateCheckEnabled
        || pendingUpdateResult.sessionGeneration
               != updateCheckSessionGeneration
        || updateCheckVisibilityState
               == UpdateCheckVisibilityState::hidden)
    {
        pendingUpdateResult = {};
        return {};
    }

    // Before the host attaches the first peer, retain a valid result instead
    // of treating normal editor construction as a hidden-window boundary.
    if (updateCheckVisibilityState
        == UpdateCheckVisibilityState::provisional)
        return {};

    auto result = std::move(pendingUpdateResult.version);
    pendingUpdateResult = {};
    return result;
}

void FireAudioProcessorEditor::presentAvailableUpdate(
    const juce::String& version)
{
    if (version.isEmpty())
        return;

    updateUpdateCheckVisibilitySession(false);

    std::uint64_t sessionGeneration = 0;
    {
        const juce::ScopedLock lock(updateResultLock);
        if (updateCheckDestructionStarted
            || ! updateCheckEnabled
            || updateCheckVisibilityState
                   != UpdateCheckVisibilityState::visible)
            return;

        sessionGeneration = updateCheckSessionGeneration;
    }

    if (! isShowing() || ! isEnabled())
        return;

    // Closing the previous member-held alert first invalidates its callback.
    // A late result from that alert therefore cannot consume this replacement.
    dismissAvailableUpdateAlert();
    const auto alertGeneration = availableUpdateAlertGeneration;
    availableUpdateAlertActive = true;

    const auto options = juce::MessageBoxOptions()
                             .withIconType(
                                 juce::MessageBoxIconType::InfoIcon)
                             .withTitle("New Version")
                             .withMessage(
                                 "New version " + version
                                 + " available, do you want to download it?")
                             .withButton("Download")
                             .withButton("Cancel")
                             .withAssociatedComponent(this)
                             .withParentComponent(this);

    const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(
        this);
    UpdateAlertCompletion completion =
        [safeThis,
         alertGeneration,
         sessionGeneration,
         version](int result)
        {
            if (safeThis != nullptr)
                safeThis->handleAvailableUpdateAlertResult(
                    result,
                    alertGeneration,
                    sessionGeneration,
                    version);
        };

    juce::ScopedMessageBox newAlert;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (availableUpdateAlertFactoryForTesting)
    {
        // The factory is copied because a test seam is allowed to invoke the
        // completion synchronously, including deleting this editor.
        auto factory = availableUpdateAlertFactoryForTesting;
        newAlert = factory(options, std::move(completion));
    }
    else
#endif
    {
        newAlert = juce::NativeMessageBox::showScopedAsync(
            options,
            std::move(completion));
    }

    if (safeThis == nullptr)
    {
        newAlert.close();
        return;
    }

    // A synchronous completion, hide, or replacement may have invalidated
    // this alert while its platform handle was being created.
    if (! safeThis->availableUpdateAlertActive
        || safeThis->availableUpdateAlertGeneration != alertGeneration
        || ! safeThis->isShowing()
        || ! safeThis->isEnabled()
        || ! safeThis->isCurrentVisibleUpdateCheckSession(
            sessionGeneration))
    {
        newAlert.close();
        return;
    }

    safeThis->availableUpdateAlert = std::move(newAlert);
}

void FireAudioProcessorEditor::handleAvailableUpdateAlertResult(
    int result,
    std::uint64_t alertGeneration,
    std::uint64_t sessionGeneration,
    const juce::String& version)
{
    updateUpdateCheckVisibilitySession(false);

    // Check the alert generation before touching the member handle. In
    // particular, an old OK callback must not close a replacement alert.
    if (! availableUpdateAlertActive
        || alertGeneration != availableUpdateAlertGeneration)
        return;

    const bool shouldLaunch =
        result == 1
        && isShowing()
        && isEnabled()
        && isCurrentVisibleUpdateCheckSession(sessionGeneration);
    const auto downloadUrl = juce::String(GITHUB_TAG_LINK) + version;

    // Invalidate and close before invoking an external URL handler. This is
    // deliberately the last editor mutation in the successful callback path.
    dismissAvailableUpdateAlert();

    if (! shouldLaunch)
        return;

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (availableUpdateUrlLauncherForTesting)
    {
        auto launcher = availableUpdateUrlLauncherForTesting;
        launcher(downloadUrl);
        return;
    }
#endif

    juce::URL(downloadUrl).launchInDefaultBrowser();
}

void FireAudioProcessorEditor::dismissAvailableUpdateAlert()
{
    const bool wasActive = availableUpdateAlertActive;
    availableUpdateAlertActive = false;
    ++availableUpdateAlertGeneration;
    if (availableUpdateAlertGeneration == 0)
        ++availableUpdateAlertGeneration;
    availableUpdateAlert.close();

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (wasActive)
        ++availableUpdateAlertDismissalCountForTesting;
#else
    juce::ignoreUnused(wasActive);
#endif
}

bool FireAudioProcessorEditor::isCurrentVisibleUpdateCheckSession(
    std::uint64_t sessionGeneration)
{
    const juce::ScopedLock lock(updateResultLock);
    return ! updateCheckDestructionStarted
           && updateCheckEnabled
           && updateCheckVisibilityState
                  == UpdateCheckVisibilityState::visible
           && sessionGeneration != 0
           && sessionGeneration == updateCheckSessionGeneration;
}

void FireAudioProcessorEditor::invalidateUpdateCheckSessionForDisable()
{
    bool shouldDismissAlert = false;
    {
        const juce::ScopedLock lock(updateResultLock);
        if (updateCheckDestructionStarted || ! updateCheckEnabled)
            return;

        updateCheckEnabled = false;
        ++updateCheckSessionGeneration;
        if (updateCheckSessionGeneration == 0)
            ++updateCheckSessionGeneration;
        pendingUpdateResult = {};
        shouldDismissAlert = availableUpdateAlertActive;
    }

    if (shouldDismissAlert)
        dismissAvailableUpdateAlert();
}

void FireAudioProcessorEditor::updateUpdateCheckVisibilitySession(
    bool retriggerPendingResult)
{
    const bool showing = isShowing();
    bool shouldTriggerPendingResult = false;
    bool shouldDismissAlert = false;

    {
        const juce::ScopedLock lock(updateResultLock);
        if (updateCheckDestructionStarted)
            return;

        if (showing)
        {
            updateCheckVisibilityState =
                UpdateCheckVisibilityState::visible;
            shouldTriggerPendingResult =
                retriggerPendingResult
                && updateCheckEnabled
                && pendingUpdateResult.version.isNotEmpty()
                && pendingUpdateResult.sessionGeneration
                       == updateCheckSessionGeneration;
        }
        else if (updateCheckVisibilityState
                 == UpdateCheckVisibilityState::visible)
        {
            // Only a window that has actually been visible can become hidden.
            // The initial no-peer construction state remains provisional.
            updateCheckVisibilityState =
                UpdateCheckVisibilityState::hidden;
            ++updateCheckSessionGeneration;
            if (updateCheckSessionGeneration == 0)
                ++updateCheckSessionGeneration;
            pendingUpdateResult = {};
            shouldDismissAlert = availableUpdateAlertActive;
        }
    }

    if (shouldDismissAlert)
        dismissAvailableUpdateAlert();

    if (shouldTriggerPendingResult)
        triggerAsyncUpdate();
}

void FireAudioProcessorEditor::invalidateUpdateCheckSessionForDestruction()
    noexcept
{
    const juce::ScopedLock lock(updateResultLock);
    updateCheckDestructionStarted = true;
    updateCheckEnabled = false;
    updateCheckVisibilityState = UpdateCheckVisibilityState::hidden;
    ++updateCheckSessionGeneration;
    if (updateCheckSessionGeneration == 0)
        ++updateCheckSessionGeneration;
    pendingUpdateResult = {};
}

void FireAudioProcessorEditor::exitAssignMode(bool showCancellationFeedback)
{
    if (! isLfoAssignMode)
        return;

    isLfoAssignMode = false;
    if (showCancellationFeedback)
        lfoPanel.showAssignCancelled();
    else
        lfoPanel.clearAssignFeedback();

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
        const juce::Component::SafePointer<FireAudioProcessorEditor> safeThis(
            this);
        const bool resetFocusAfterStateLoad =
            stateComponent.consumeFocusResetAfterStateLoad();

        modulationSnapshotFramesRemaining = 0;
        stateComponent.synchronisePresetSelectionFromManager();
        if (safeThis == nullptr)
            return;

        stateComponent.synchroniseABButtonFromManager();
        if (safeThis == nullptr)
            return;

        lfoPanel.refreshLfoDisplay();
        if (safeThis == nullptr)
            return;

        if (resetFocusAfterStateLoad)
        {
            multiband.setFocusIndex(0);
            if (safeThis == nullptr)
                return;
        }

        // Crossover repair can synchronously notify a host which destroys the
        // editor, so keep this as the final operation in the callback.
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
            const auto& routing = modulationRoutingSnapshot.getReference(routingIndex);
            if (routing.targetParameterID == slider->getParamID()
                && juce::isPositiveAndBelow(routing.sourceLfoIndex, 4))
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

        const int sourceIndex = matchedRouting != nullptr
                                    ? matchedRouting->sourceLfoIndex
                                    : -1;
        const bool isModulated = juce::isPositiveAndBelow(
            sourceIndex, static_cast<int>(lfoOutputs.size()));
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
