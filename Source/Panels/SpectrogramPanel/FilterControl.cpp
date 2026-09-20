/*
  ==============================================================================

    FilterControl.cpp
    Event-driven global filter overlay for the Fire analyser.

  ==============================================================================
*/

#include "FilterControl.h"
#include "../../GUI/InterfaceDefines.h"
#include "../ControlPanel/GlobalPanel.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr double minimumDisplayFrequency = 20.0;
constexpr double maximumDisplayFrequency = 20000.0;
constexpr double minimumDisplayDecibels = -24.0;
constexpr double maximumDisplayDecibels = 24.0;

double maximumUsableDisplayFrequency(double sampleRate) noexcept
{
    if (! std::isfinite(sampleRate) || sampleRate <= 0.0)
        return maximumDisplayFrequency;

    const float nyquist = static_cast<float>(sampleRate * 0.5);
    return juce::jmin(maximumDisplayFrequency,
                      static_cast<double>(std::nextafter(nyquist, 0.0f)));
}

float frequencyToDisplayX(double frequency, int width) noexcept
{
    if (width <= 0)
        return 0.0f;

    frequency = juce::jlimit(minimumDisplayFrequency,
                             maximumDisplayFrequency,
                             frequency);
    return static_cast<float>(width)
         * static_cast<float>(juce::mapFromLog10(frequency,
                                                 minimumDisplayFrequency,
                                                 maximumDisplayFrequency));
}

bool isFilterModulationTarget(const juce::String& parameterID)
{
    return parameterID == LOWCUT_FREQ_ID || parameterID == LOWCUT_GAIN_ID
        || parameterID == LOWCUT_Q_ID || parameterID == PEAK_FREQ_ID
        || parameterID == PEAK_GAIN_ID || parameterID == PEAK_Q_ID
        || parameterID == HIGHCUT_FREQ_ID || parameterID == HIGHCUT_GAIN_ID
        || parameterID == HIGHCUT_Q_ID
        || fire::eq::isAppendedParameterID(parameterID);
}
} // namespace

struct FilterControl::DragGestureSession
{
    ~DragGestureSession()
    {
        finish();
    }

    bool touch(juce::RangedAudioParameter& parameter)
    {
        const auto activeEnd = parameters.begin() + numParameters;
        if (std::find(parameters.begin(), activeEnd, &parameter) != activeEnd)
            return true;

        if (numParameters >= static_cast<int>(parameters.size()))
        {
            jassertfalse;
            return false;
        }

        // Record ownership before notifying the host. The notification can
        // synchronously remove the UI that created this session.
        parameters[static_cast<size_t>(numParameters++)] = &parameter;
        parameter.beginChangeGesture();
        return true;
    }

private:
    void finish()
    {
        const auto parametersToEnd = parameters;
        const auto countToEnd = numParameters;
        parameters.fill(nullptr);
        numParameters = 0;

        // From here on, use only processor-owned parameters. Any individual
        // callback may remove the editor, while all begun gestures still need
        // exactly one matching end notification.
        for (int index = 0; index < countToEnd; ++index)
            if (auto* parameter = parametersToEnd[static_cast<size_t>(index)])
                parameter->endChangeGesture();
    }

    std::array<juce::RangedAudioParameter*, 3> parameters {};
    int numParameters = 0;
};

//==============================================================================
FilterControl::FilterControl(FireAudioProcessor& p, GlobalPanel& panel)
    : processor(p), globalPanel(&panel)
{
    setOpaque(false);
    setWantsKeyboardFocus(true);
    setTitle("Interactive EQ spectrum");

    // The overlay only needs filter parameters. Listening to every processor
    // parameter previously rebuilt the response for unrelated automation.
    const char* const parameterIDs[] = {
        LOWCUT_FREQ_ID, LOWCUT_GAIN_ID, LOWCUT_Q_ID,
        PEAK_FREQ_ID, PEAK_GAIN_ID, PEAK_Q_ID,
        HIGHCUT_FREQ_ID, HIGHCUT_GAIN_ID, HIGHCUT_Q_ID,
        LOWCUT_SLOPE_ID, HIGHCUT_SLOPE_ID,
        LOWCUT_BYPASSED_ID, PEAK_BYPASSED_ID, HIGHCUT_BYPASSED_ID,
        FILTER_BYPASS_ID
    };

    observedParameters.reserve(std::size(parameterIDs));
    for (const auto* parameterID : parameterIDs)
    {
        if (auto* parameter = processor.treeState.getParameter(parameterID))
        {
            parameter->addListener(this);
            observedParameters.push_back(parameter);
        }
    }
    for (const auto& parameterID : fire::eq::appendedParameterIDs())
        if (auto* parameter = processor.treeState.getParameter(parameterID))
        {
            parameter->addListener(this);
            observedParameters.push_back(parameter);
        }

    processor.addChangeListener(this);

    addAndMakeVisible(draggableLowButton);
    addAndMakeVisible(draggablePeakButton);
    addAndMakeVisible(draggableHighButton);

    const auto describeNode = [](DraggableButton& button,
                                 const juce::String& title,
                                 const juce::String& tooltip)
    {
        button.setTitle(title);
        button.setTooltip(tooltip);
        button.setHelpText(tooltip + ". Use arrow keys to adjust frequency "
                           "and gain; use Page Up or Page Down for resonance");
    };
    describeNode(draggableLowButton,
                 "Low-cut filter frequency",
                 "Adjust low-cut frequency and gain");
    describeNode(draggablePeakButton,
                 "Peak filter frequency",
                 "Adjust peak-filter frequency and gain");
    describeNode(draggableHighButton,
                 "High-cut filter frequency",
                 "Adjust high-cut frequency and gain");

    draggableLowButton.onDrag = [this](DraggableButton& button, const juce::MouseEvent& event)
    {
        handleFilterDrag(button, event, LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID);
    };
    draggablePeakButton.onDrag = [this](DraggableButton& button, const juce::MouseEvent& event)
    {
        handleFilterDrag(button, event, BAND_ID, PEAK_FREQ_ID, PEAK_GAIN_ID);
    };
    draggableHighButton.onDrag = [this](DraggableButton& button, const juce::MouseEvent& event)
    {
        handleFilterDrag(button, event, HIGH_ID, HIGHCUT_FREQ_ID, HIGHCUT_GAIN_ID);
    };

    draggableLowButton.onDragFinished = [this] { finishFilterDrag(); };
    draggablePeakButton.onDragFinished = [this] { finishFilterDrag(); };
    draggableHighButton.onDragFinished = [this] { finishFilterDrag(); };

    auto setupQControl = [this](DraggableButton& button, const juce::String& parameterID)
    {
        button.onQValueChanged = [this, parameterID](float delta)
        {
            if (auto* parameter = processor.treeState.getParameter(parameterID))
            {
                const auto newValue = juce::jlimit(0.0f, 1.0f,
                                                   parameter->getValue() + delta * 0.5f);
                if (! juce::approximatelyEqual(newValue, parameter->getValue()))
                {
                    parameter->beginChangeGesture();
                    const juce::ScopeGuard finishGesture {
                        [parameter] { parameter->endChangeGesture(); }
                    };
                    parameter->setValueNotifyingHost(newValue);
                }
            }
        };
    };

    setupQControl(draggableLowButton, LOWCUT_Q_ID);
    setupQControl(draggablePeakButton, PEAK_Q_ID);
    setupQControl(draggableHighButton, HIGHCUT_Q_ID);

    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
    {
        auto& button = nodeButton(slot);
        if (slot >= 3) addChildComponent(button);
        button.setComponentID("eqSpectrumPoint" + juce::String(slot + 1));
        button.onDrag = [this, slot](DraggableButton& point, const juce::MouseEvent& event)
        {
            const juce::Component::SafePointer<FilterControl> safe(this);
            if (globalPanel) globalPanel->selectEqNode(slot);
            if (! safe) return;
            const auto state = processor.getEqNodeState(slot);
            if (! state.present) return;
            const auto selection = slot == 0 ? LOW_ID : slot == 1 ? BAND_ID : slot == 2 ? HIGH_ID : "";
            const bool hasGain = state.type != fire::eq::Type::notch && state.type != fire::eq::Type::bandPass;
            handleFilterDrag(point, event, selection,
                             fire::eq::parameterID(slot, fire::eq::Field::frequency),
                             hasGain ? fire::eq::parameterID(slot, fire::eq::Field::gain) : juce::String());
        };
        button.onDragFinished = [this] { finishFilterDrag(); };
        setupQControl(button, fire::eq::parameterID(slot, fire::eq::Field::q));
    }

    responseSampleRate = processor.getSampleRate();
    updateChain();
    updateDraggableButtonStates();
    checkAnimationStatus();
    // A processor can outlive several editor instances. Drain packets owned
    // by the previous instance before this control begins a visible session.
    suspendTelemetryPresentation();
}

DraggableButton& FilterControl::nodeButton(int slot)
{
    if (slot == 0) return draggableLowButton;
    if (slot == 1) return draggablePeakButton;
    if (slot == 2) return draggableHighButton;
    return extraNodes[static_cast<size_t>(slot - 3)];
}

void FilterControl::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.eventComponent != this || ! event.mods.isLeftButtonDown()
        || event.mods.isPopupMenu() || getWidth() <= 0 || getHeight() <= 0) return;
    const auto x = juce::jlimit(0.0f, 1.0f, event.position.x / static_cast<float>(getWidth()));
    const auto frequency = static_cast<float>(juce::jmin(maximumUsableDisplayFrequency(processor.getSampleRate()),
        juce::mapToLog10(static_cast<double>(x), minimumDisplayFrequency, maximumDisplayFrequency)));
    const auto gain = juce::jmap(juce::jlimit(0.0f, 1.0f, event.position.y / static_cast<float>(getHeight())), 24.0f, -24.0f);
    const juce::Component::SafePointer<FilterControl> safe(this);
    const int slot = processor.addEqNode(frequency, gain);
    if (! safe || slot < 0) return;
    if (globalPanel) globalPanel->selectEqNode(slot);
    if (! safe) return;
    updateChain(); updateResponseCurve(); setDraggableButtonBounds(); repaint();
}

bool FilterControl::keyPressed(const juce::KeyPress& key)
{
    if (key != juce::KeyPress::deleteKey && key != juce::KeyPress::backspaceKey) return false;
    const auto slot = globalPanel ? globalPanel->getSelectedEqNode() : -1;
    if (slot < 0) return false;
    const juce::Component::SafePointer<FilterControl> safe(this);
    dismissTransientInteraction();
    if (safe) processor.removeEqNode(slot);
    return true;
}

FilterControl::~FilterControl()
{
    processor.removeChangeListener(this);

    for (auto* parameter : observedParameters)
        parameter->removeListener(this);

    // Releasing the session emits host callbacks and is intentionally the
    // final operation that can synchronously re-enter editor ownership.
    finishDragParameterGestures();
}

void FilterControl::paint(juce::Graphics& g)
{
    const auto* enableParameter = processor.treeState.getRawParameterValue(FILTER_BYPASS_ID);
    const bool isFilterEnabled = enableParameter != nullptr && enableParameter->load() > 0.5f;
    const auto bounds = getLocalBounds().toFloat();

    if (! responseFillCurve.isEmpty())
    {
        const auto accent = isFilterEnabled ? fire::ui::colours::filter
                                            : fire::ui::colours::textMuted;
        juce::ColourGradient fill(accent.withAlpha(isFilterEnabled ? 0.14f : 0.055f),
                                  bounds.getCentreX(), bounds.getCentreY(),
                                  accent.withAlpha(0.0f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(fill);
        g.fillPath(responseFillCurve);

        if (isFilterEnabled)
        {
            g.setColour(fire::ui::colours::filter.withAlpha(0.10f));
            g.strokePath(responseCurve,
                         juce::PathStrokeType(5.0f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

            juce::ColourGradient energy(fire::ui::colours::whiteHot.withAlpha(0.92f),
                                        bounds.getX(), bounds.getCentreY(),
                                        fire::ui::colours::filter.withAlpha(0.92f),
                                        bounds.getRight(), bounds.getCentreY(), false);
            energy.addColour(0.52, fire::ui::colours::flame.withAlpha(0.96f));
            g.setGradientFill(energy);
        }
        else
        {
            g.setColour(fire::ui::colours::disabled.withAlpha(0.72f));
        }

        g.strokePath(responseCurve,
                     juce::PathStrokeType(1.5f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    }

    if (isFilterEnabled && isAnimationActive && ! lfoResponseCurve.isEmpty())
    {
        g.setColour(fire::ui::colours::modulation.withAlpha(0.16f));
        g.strokePath(lfoResponseCurve, juce::PathStrokeType(3.0f));

        juce::ColourGradient modulation(fire::ui::colours::modulation.withAlpha(0.82f),
                                        bounds.getX(), bounds.getCentreY(),
                                        fire::ui::colours::ember.withAlpha(0.72f),
                                        bounds.getRight(), bounds.getCentreY(), false);
        g.setGradientFill(modulation);
        g.strokePath(lfoResponseCurve, juce::PathStrokeType(1.0f));
    }

    if (dragTooltipVisible)
    {
        constexpr float tooltipWidth = 112.0f;
        constexpr float tooltipHeight = 44.0f;
        auto tooltip = juce::Rectangle<float>(static_cast<float>(dragTooltipAnchor.x) + 17.0f,
                                               static_cast<float>(dragTooltipAnchor.y) - tooltipHeight - 12.0f,
                                               tooltipWidth,
                                               tooltipHeight);

        if (tooltip.getRight() > bounds.getRight() - 4.0f)
            tooltip.setX(static_cast<float>(dragTooltipAnchor.x) - tooltipWidth - 17.0f);
        if (tooltip.getY() < bounds.getY() + 4.0f)
            tooltip.setY(static_cast<float>(dragTooltipAnchor.y) + 12.0f);

        tooltip.setPosition(juce::jlimit(bounds.getX() + 4.0f,
                                         bounds.getRight() - tooltipWidth - 4.0f,
                                         tooltip.getX()),
                            juce::jlimit(bounds.getY() + 4.0f,
                                         bounds.getBottom() - tooltipHeight - 4.0f,
                                         tooltip.getY()));

        fire::ui::drawGlassPill(g, tooltip, fire::ui::colours::flame, true, false, false);

        auto textArea = tooltip.reduced(9.0f, 4.0f);
        const auto frequencyText = dragFrequency >= 1000.0
                                     ? juce::String(dragFrequency / 1000.0, 2) + " kHz"
                                     : juce::String(dragFrequency, 1) + " Hz";

        g.setFont(fire::ui::displayFont(11.0f));
        g.setColour(fire::ui::colours::whiteHot);
        g.drawText(frequencyText, textArea.removeFromTop(textArea.getHeight() * 0.54f),
                   juce::Justification::centredLeft);
        g.setFont(fire::ui::bodyFont(10.0f));
        g.setColour(fire::ui::colours::textSecondary);
        g.drawText(juce::String(dragGain, 1) + " dB", textArea,
                   juce::Justification::centredLeft);
    }
}

void FilterControl::animationTick()
{
    if (! isShowing())
    {
        suspendTelemetryPresentation();
        return;
    }

    if (! telemetryPresentationActive)
    {
        requiredTelemetryCaptureEpoch =
            processor.requestFreshModulatedFilterValuesEpoch();
        telemetryPresentationActive = true;
        hasFreshTelemetryForPresentation = false;
    }

    ModulatedFilterValues modulatedValues;
    const bool hasFreshModulatedValues =
        processor.getLatestModulatedFilterValues(
            modulatedValues,
            requiredTelemetryCaptureEpoch);

    const auto currentSampleRate = processor.getSampleRate();
    const bool sampleRateChanged =
        ! juce::approximatelyEqual(currentSampleRate, responseSampleRate);
    if (sampleRateChanged)
        responseSampleRate = currentSampleRate;

    const bool parametersChanged = parameterUpdatePending.exchange(false, std::memory_order_acq_rel);
    const bool routingChanged = routingStateDirty.exchange(false, std::memory_order_acq_rel);
    bool visualStateChanged = parametersChanged || routingChanged
                           || sampleRateChanged;
    const auto selectedNode = globalPanel ? globalPanel->getSelectedEqNode() : -1;
    if (lastSelectedNode != selectedNode)
    {
        lastSelectedNode = selectedNode;
        setDraggableButtonBounds();
        visualStateChanged = true;
    }

    if (parametersChanged || sampleRateChanged)
    {
        updateChain();
        updateResponseCurve();
        setDraggableButtonBounds();

        if (sampleRateChanged)
        {
            lfoResponseCurve.clear();
            hasFreshTelemetryForPresentation = false;
        }

        juce::Component::SafePointer<FilterControl> safeThis(this);
        updateDraggableButtonStates();

        if (safeThis == nullptr)
            return;
    }

    if (routingChanged || parametersChanged || sampleRateChanged)
        checkAnimationStatus();

    if (isAnimationActive)
    {
        if (hasFreshModulatedValues)
        {
            updateLfoChain(modulatedValues);
            hasFreshTelemetryForPresentation = true;
            updateLfoResponseCurve();
            visualStateChanged = true;
        }
    }

    if (visualStateChanged)
        repaint();
}

void FilterControl::suspendTelemetryPresentation()
{
    // getLatest... is latest-wins and consumes the whole ready range. Calling
    // it on every hidden editor tick prevents short-block hosts from filling
    // the FIFO, while resetting the epoch below rejects the final packet that
    // can race a hide/show boundary.
    ModulatedFilterValues ignoredValues;
    processor.getLatestModulatedFilterValues(ignoredValues);

    telemetryPresentationActive = false;
    hasFreshTelemetryForPresentation = false;
    requiredTelemetryCaptureEpoch = 0;
    if (! lfoResponseCurve.isEmpty())
    {
        lfoResponseCurve.clear();
        repaint();
    }
}

void FilterControl::parameterValueChanged(int parameterIndex, float newValue)
{
    juce::ignoreUnused(parameterIndex, newValue);

    // This callback may run on the real-time audio thread. The editor's shared
    // animation clock coalesces the dirty flag on the message thread.
    parameterUpdatePending.store(true, std::memory_order_release);
}

void FilterControl::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &processor)
        return;

    routingStateDirty.store(true, std::memory_order_release);
}

void FilterControl::checkAnimationStatus()
{
    const auto* enableParameter = processor.treeState.getRawParameterValue(FILTER_BYPASS_ID);
    bool needsAnimation = enableParameter != nullptr && enableParameter->load() > 0.5f;

    if (needsAnimation)
    {
        needsAnimation = false;
        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        for (const auto& routing : routings)
        {
            if (! routing.isBypassed && isFilterModulationTarget(routing.targetParameterID))
            {
                needsAnimation = true;
                break;
            }
        }
    }

    if (needsAnimation != isAnimationActive)
    {
        isAnimationActive = needsAnimation;
        hasFreshTelemetryForPresentation = false;
        lfoResponseCurve.clear();
    }

}

void FilterControl::visibilityChanged()
{
    if (isShowing())
    {
        processor.setHistoryArray(FireAudioProcessor::globalHistorySourceIndex);
        parameterUpdatePending.store(true, std::memory_order_release);
        routingStateDirty.store(true, std::memory_order_release);
    }
    else
    {
        suspendTelemetryPresentation();
        dismissTransientInteraction();
    }
}

void FilterControl::dismissTransientInteraction()
{
    juce::Component::SafePointer<FilterControl> safeThis(this);
    draggableLowButton.dismissTransientInteraction();

    if (safeThis == nullptr)
        return;

    draggablePeakButton.dismissTransientInteraction();

    if (safeThis == nullptr)
        return;

    draggableHighButton.dismissTransientInteraction();

    if (safeThis == nullptr)
        return;

    for (auto& node : extraNodes)
    {
        node.dismissTransientInteraction();
        if (! safeThis) return;
    }

    dragTooltipVisible = false;
    finishDragParameterGestures();
}

void FilterControl::resized()
{
    updateResponseCurve();
    if (isAnimationActive && hasFreshTelemetryForPresentation)
        updateLfoResponseCurve();
    setDraggableButtonBounds();
}

void FilterControl::handleFilterDrag(DraggableButton& button,
                                     const juce::MouseEvent& event,
                                     const juce::String& selectionParameter,
                                     const juce::String& frequencyParameter,
                                     const juce::String& gainParameter)
{
    const auto* enableParameter = processor.treeState.getRawParameterValue(FILTER_BYPASS_ID);
    if (enableParameter == nullptr || enableParameter->load() <= 0.5f
        || getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto relativeEvent = event.getEventRelativeTo(this);
    auto point = relativeEvent.position.toFloat();
    const auto maximumFrequency =
        maximumUsableDisplayFrequency(processor.getSampleRate());
    if (maximumFrequency <= minimumDisplayFrequency)
        return;

    const auto maximumX = frequencyToDisplayX(maximumFrequency, getWidth());
    point.x = juce::jlimit(0.0f, maximumX, point.x);
    point.y = juce::jlimit(0.0f, static_cast<float>(getHeight()), point.y);
    if (gainParameter.isEmpty()) point.y = static_cast<float>(getHeight()) * 0.5f;

    const auto buttonSize = juce::jlimit(20.0f, 24.0f,
                                         getWidth() * 0.02f);
    juce::Component::SafePointer<FilterControl> safeThis(this);
    button.setBounds(juce::Rectangle<float>(buttonSize, buttonSize)
                         .withCentre(point)
                         .toNearestInt());

    if (safeThis == nullptr)
        return;

    if (auto* selection = processor.treeState.getParameter(selectionParameter))
        if (! setDragParameterValue(*selection, 1.0f) || safeThis == nullptr)
            return;

    const auto frequency = juce::mapToLog10(static_cast<double>(point.x) / getWidth(),
                                            minimumDisplayFrequency,
                                            maximumDisplayFrequency);
    const auto gain = juce::jmap(static_cast<double>(point.y),
                                 0.0,
                                 static_cast<double>(getHeight()),
                                 maximumDisplayDecibels,
                                 minimumDisplayDecibels);

    const auto setParameterValue = [this](const juce::String& parameterID,
                                          double plainValue)
    {
        if (auto* parameter = processor.treeState.getParameter(parameterID))
        {
            const auto normalisedValue = parameter->convertTo0to1(static_cast<float>(plainValue));
            return setDragParameterValue(*parameter, normalisedValue);
        }

        return true;
    };

    if (! setParameterValue(frequencyParameter, frequency) || safeThis == nullptr)
        return;

    if (! setParameterValue(gainParameter, gain) || safeThis == nullptr)
        return;

    dragFrequency = frequency;
    dragGain = gain;
    dragTooltipAnchor = point.toInt();
    dragTooltipVisible = true;
    repaint();
}

bool FilterControl::setDragParameterValue(juce::RangedAudioParameter& parameter,
                                          float normalisedValue)
{
    normalisedValue = juce::jlimit(0.0f, 1.0f, normalisedValue);
    if (juce::approximatelyEqual(parameter.getValue(), normalisedValue))
        return true;

    if (dragGestureSession == nullptr)
        dragGestureSession = std::make_shared<DragGestureSession>();

    auto session = dragGestureSession;
    juce::Component::SafePointer<FilterControl> safeThis(this);
    if (! session->touch(parameter) || safeThis == nullptr)
        return false;

    if (dragGestureSession != session)
        return false;

    // The local session token keeps the host gesture valid throughout the
    // value callback. Do not access component state until SafePointer confirms
    // that the owner survived it.
    parameter.setValueNotifyingHost(normalisedValue);

    if (safeThis == nullptr)
        return false;

    return dragGestureSession == session;
}

void FilterControl::finishDragParameterGestures() noexcept
{
    auto endingSession = std::move(dragGestureSession);

    // The last session reference can synchronously remove this FilterControl.
    // It must therefore remain the final member-related operation.
    endingSession.reset();
}

void FilterControl::finishFilterDrag()
{
    auto endingSession = std::move(dragGestureSession);

    if (dragTooltipVisible)
    {
        dragTooltipVisible = false;
        setDraggableButtonBounds();
        repaint();
    }

    // Host end callbacks may remove this component, so release last.
    endingSession.reset();
}

void FilterControl::updateDraggableButtonStates()
{
    const auto* power = processor.treeState.getRawParameterValue(FILTER_BYPASS_ID);
    const bool enabled = power != nullptr && power->load() > 0.5f;
    const juce::Component::SafePointer<FilterControl> safe(this);
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
    {
        nodeButton(slot).setState(enabled);
        if (! safe) return;
    }
}

void FilterControl::setDraggableButtonBounds()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    const auto maxFrequency = static_cast<float>(juce::jmax(minimumDisplayFrequency,
        maximumUsableDisplayFrequency(processor.getSampleRate())));
    const auto selected = globalPanel ? globalPanel->getSelectedEqNode() : -1;
    const juce::Component::SafePointer<FilterControl> safe(this);
    int ordinal = 0;
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
    {
        const auto state = processor.getEqNodeState(slot);
        auto& button = nodeButton(slot);
        button.setVisible(state.present);
        if (! safe) return;
        if (! state.present) continue;
        ++ordinal;
        const auto label = "EQ point " + juce::String(ordinal);
        button.setTitle(label);
        button.setTooltip(label + " · " + fire::eq::typeNames[static_cast<size_t>(state.type)]);
        button.getProperties().set("eqOrdinal", ordinal);
        button.getProperties().set("eqSelected", slot == selected);
        button.getProperties().set("eqBypassed", state.bypassed);
        const auto freq = juce::jlimit(20.0f, maxFrequency, state.frequency);
        const auto x = frequencyToDisplayX(freq, getWidth());
        const bool hasGain = state.type != fire::eq::Type::notch && state.type != fire::eq::Type::bandPass;
        const auto y = juce::jmap(juce::jlimit(-24.0f, 24.0f, hasGain ? state.gainDb : 0.0f), 24.0f, -24.0f, 0.0f, static_cast<float>(getHeight()));
        const auto size = slot == selected ? 29.0f : 23.0f;
        button.setBounds(juce::Rectangle<float>(size, size).withCentre({x, y}).toNearestInt());
        button.repaint();
    }
}

void FilterControl::updateChain()
{
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
        eqResponse[static_cast<size_t>(slot)] = fire::eq::makeCoefficients(
            processor.getEqNodeState(slot), processor.getSampleRate());
}

int FilterControl::getCurvePointCount() const
{
    if (getWidth() <= 1 || getHeight() <= 0)
        return 0;

    return juce::jlimit(2, 1024, getWidth());
}

void FilterControl::updateResponseCurve()
{
    const auto pointCount = getCurvePointCount();
    const auto sampleRate = processor.getSampleRate();
    const auto maximumFrequency = maximumUsableDisplayFrequency(sampleRate);
    if (pointCount == 0 || sampleRate <= 0.0 || maximumFrequency <= minimumDisplayFrequency)
    {
        responseCurve.clear();
        responseFillCurve.clear();
        return;
    }

    if (responseMagnitudes.size() != static_cast<size_t>(pointCount))
        responseMagnitudes.resize(static_cast<size_t>(pointCount));

    for (int i = 0; i < pointCount; ++i)
    {
        const auto proportion = static_cast<double>(i) / static_cast<double>(pointCount - 1);
        const auto frequency = juce::mapToLog10(proportion,
                                                minimumDisplayFrequency,
                                                maximumFrequency);
        double magnitude = 1.0;

        for (const auto& coefficients : eqResponse)
            magnitude *= coefficients.magnitudeAt(frequency, sampleRate);

        responseMagnitudes[static_cast<size_t>(i)] = juce::Decibels::gainToDecibels(magnitude);
    }

    const auto mapMagnitudeToY = [this](double magnitude)
    {
        return static_cast<float>(juce::jmap(juce::jlimit(minimumDisplayDecibels,
                                                          maximumDisplayDecibels,
                                                          magnitude),
                                             minimumDisplayDecibels,
                                             maximumDisplayDecibels,
                                             static_cast<double>(getHeight()),
                                             0.0));
    };

    responseCurve.clear();
    responseFillCurve.clear();
    responseCurve.preallocateSpace(pointCount * 3 + 8);
    responseFillCurve.preallocateSpace(pointCount * 3 + 16);

    const auto curveRight = frequencyToDisplayX(maximumFrequency, getWidth());
    const auto firstY = mapMagnitudeToY(responseMagnitudes.front());
    responseCurve.startNewSubPath(0.0f, firstY);
    responseFillCurve.startNewSubPath(0.0f, static_cast<float>(getHeight()));
    responseFillCurve.lineTo(0.0f, firstY);

    for (int i = 1; i < pointCount; ++i)
    {
        const auto x = static_cast<float>(i) / static_cast<float>(pointCount - 1)
                     * curveRight;
        const auto y = mapMagnitudeToY(responseMagnitudes[static_cast<size_t>(i)]);
        responseCurve.lineTo(x, y);
        responseFillCurve.lineTo(x, y);
    }

    responseFillCurve.lineTo(curveRight, static_cast<float>(getHeight()));
    responseFillCurve.closeSubPath();
}

void FilterControl::updateLfoChain(const ModulatedFilterValues& values)
{
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
    {
        auto state = processor.getEqNodeState(slot);
        const auto& telemetry = values.eqNodes[static_cast<size_t>(slot)];
        if (telemetry.present)
        {
            state.frequency = telemetry.frequency;
            state.gainDb = telemetry.gainDb;
            state.q = telemetry.q;
        }
        else if (slot == 0) { state.frequency = values.lowCutFreq; state.gainDb = values.lowCutGain; state.q = values.lowCutQ; }
        else if (slot == 1) { state.frequency = values.peakFreq; state.gainDb = values.peakGain; state.q = values.peakQ; }
        else if (slot == 2) { state.frequency = values.highCutFreq; state.gainDb = values.highCutGain; state.q = values.highCutQ; }
        modulatedEqResponse[static_cast<size_t>(slot)] = fire::eq::makeCoefficients(state, processor.getSampleRate());
    }
}

void FilterControl::updateLfoResponseCurve()
{
    const auto pointCount = getCurvePointCount();
    const auto sampleRate = processor.getSampleRate();
    const auto maximumFrequency = maximumUsableDisplayFrequency(sampleRate);
    if (! isAnimationActive || ! hasFreshTelemetryForPresentation
        || pointCount == 0 || sampleRate <= 0.0
        || maximumFrequency <= minimumDisplayFrequency)
    {
        lfoResponseCurve.clear();
        return;
    }

    if (lfoMagnitudes.size() != static_cast<size_t>(pointCount))
        lfoMagnitudes.resize(static_cast<size_t>(pointCount));

    for (int i = 0; i < pointCount; ++i)
    {
        const auto proportion = static_cast<double>(i) / static_cast<double>(pointCount - 1);
        const auto frequency = juce::mapToLog10(proportion,
                                                minimumDisplayFrequency,
                                                maximumFrequency);
        double magnitude = 1.0;

        for (const auto& coefficients : modulatedEqResponse)
            magnitude *= coefficients.magnitudeAt(frequency, sampleRate);

        lfoMagnitudes[static_cast<size_t>(i)] = juce::Decibels::gainToDecibels(magnitude);
    }

    const auto mapMagnitudeToY = [this](double magnitude)
    {
        return static_cast<float>(juce::jmap(juce::jlimit(minimumDisplayDecibels,
                                                          maximumDisplayDecibels,
                                                          magnitude),
                                             minimumDisplayDecibels,
                                             maximumDisplayDecibels,
                                             static_cast<double>(getHeight()),
                                             0.0));
    };

    lfoResponseCurve.clear();
    lfoResponseCurve.preallocateSpace(pointCount * 3 + 8);
    const auto curveRight = frequencyToDisplayX(maximumFrequency, getWidth());
    lfoResponseCurve.startNewSubPath(0.0f, mapMagnitudeToY(lfoMagnitudes.front()));
    for (int i = 1; i < pointCount; ++i)
    {
        const auto x = static_cast<float>(i) / static_cast<float>(pointCount - 1)
                     * curveRight;
        lfoResponseCurve.lineTo(x, mapMagnitudeToY(lfoMagnitudes[static_cast<size_t>(i)]));
    }
}
