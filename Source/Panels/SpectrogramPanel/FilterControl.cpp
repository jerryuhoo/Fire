/*
  ==============================================================================

    FilterControl.cpp
    Event-driven global filter overlay for the Fire analyser.

  ==============================================================================
*/

#include "FilterControl.h"
#include "../../GUI/InterfaceDefines.h"
#include <algorithm>

namespace
{
constexpr double minimumDisplayFrequency = 20.0;
constexpr double maximumDisplayFrequency = 20000.0;
constexpr double minimumDisplayDecibels = -24.0;
constexpr double maximumDisplayDecibels = 24.0;

bool isFilterModulationTarget(const juce::String& parameterID)
{
    return parameterID == LOWCUT_FREQ_ID || parameterID == LOWCUT_GAIN_ID
        || parameterID == LOWCUT_Q_ID || parameterID == PEAK_FREQ_ID
        || parameterID == PEAK_GAIN_ID || parameterID == PEAK_Q_ID
        || parameterID == HIGHCUT_FREQ_ID || parameterID == HIGHCUT_GAIN_ID
        || parameterID == HIGHCUT_Q_ID;
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
    : processor(p)
{
    setOpaque(false);

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

    processor.addChangeListener(this);

    addAndMakeVisible(draggableLowButton);
    addAndMakeVisible(draggablePeakButton);
    addAndMakeVisible(draggableHighButton);

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

    updateChain();
    updateDraggableButtonStates();
    checkAnimationStatus();
    juce::ignoreUnused(panel);
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
        return;

    const bool parametersChanged = parameterUpdatePending.exchange(false, std::memory_order_acq_rel);
    const bool routingChanged = routingStateDirty.exchange(false, std::memory_order_acq_rel);
    bool visualStateChanged = parametersChanged || routingChanged;

    if (parametersChanged)
    {
        updateChain();
        updateResponseCurve();
        setDraggableButtonBounds();

        juce::Component::SafePointer<FilterControl> safeThis(this);
        updateDraggableButtonStates();

        if (safeThis == nullptr)
            return;
    }

    if (routingChanged || parametersChanged)
        checkAnimationStatus();

    if (isAnimationActive)
    {
        ModulatedFilterValues modulatedValues;
        if (processor.getLatestModulatedFilterValues(modulatedValues))
        {
            updateLfoChain(modulatedValues);
            updateLfoResponseCurve();
            visualStateChanged = true;
        }
    }

    if (visualStateChanged)
        repaint();
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
        if (! isAnimationActive)
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

        dragTooltipVisible = false;
        finishDragParameterGestures();
    }
}

void FilterControl::resized()
{
    updateResponseCurve();
    if (isAnimationActive)
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
    point.x = juce::jlimit(0.0f, static_cast<float>(getWidth()), point.x);
    point.y = juce::jlimit(0.0f, static_cast<float>(getHeight()), point.y);

    const auto buttonSize = juce::jlimit(12.0f, 20.0f, getWidth() * 0.015f);
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
    const auto* enableParameter = processor.treeState.getRawParameterValue(FILTER_BYPASS_ID);
    const bool enabled = enableParameter != nullptr && enableParameter->load() > 0.5f;
    juce::Component::SafePointer<FilterControl> safeThis(this);
    draggableLowButton.setState(enabled);

    if (safeThis == nullptr)
        return;

    draggablePeakButton.setState(enabled);

    if (safeThis == nullptr)
        return;

    draggableHighButton.setState(enabled);
}

void FilterControl::setDraggableButtonBounds()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto* lowFrequency = processor.treeState.getRawParameterValue(LOWCUT_FREQ_ID);
    const auto* peakFrequency = processor.treeState.getRawParameterValue(PEAK_FREQ_ID);
    const auto* highFrequency = processor.treeState.getRawParameterValue(HIGHCUT_FREQ_ID);
    const auto* lowGain = processor.treeState.getRawParameterValue(LOWCUT_GAIN_ID);
    const auto* peakGain = processor.treeState.getRawParameterValue(PEAK_GAIN_ID);
    const auto* highGain = processor.treeState.getRawParameterValue(HIGHCUT_GAIN_ID);

    if (lowFrequency == nullptr || peakFrequency == nullptr || highFrequency == nullptr
        || lowGain == nullptr || peakGain == nullptr || highGain == nullptr)
        return;

    const auto buttonSize = juce::jlimit(12.0f, 20.0f, getWidth() * 0.015f);
    const auto pointForValues = [this](float frequency, float gain)
    {
        frequency = juce::jlimit(static_cast<float>(minimumDisplayFrequency),
                                 static_cast<float>(maximumDisplayFrequency),
                                 frequency);
        gain = juce::jlimit(static_cast<float>(minimumDisplayDecibels),
                            static_cast<float>(maximumDisplayDecibels),
                            gain);

        return juce::Point<float> {
            static_cast<float>(getWidth())
                * static_cast<float>(juce::mapFromLog10(static_cast<double>(frequency),
                                                        minimumDisplayFrequency,
                                                        maximumDisplayFrequency)),
            juce::jmap(gain,
                       static_cast<float>(maximumDisplayDecibels),
                       static_cast<float>(minimumDisplayDecibels),
                       0.0f,
                       static_cast<float>(getHeight()))
        };
    };

    const auto setButtonCentre = [buttonSize](DraggableButton& button, juce::Point<float> centre)
    {
        button.setBounds(juce::Rectangle<float>(buttonSize, buttonSize)
                             .withCentre(centre)
                             .toNearestInt());
    };

    setButtonCentre(draggableLowButton, pointForValues(lowFrequency->load(), lowGain->load()));
    setButtonCentre(draggablePeakButton, pointForValues(peakFrequency->load(), peakGain->load()));
    setButtonCentre(draggableHighButton, pointForValues(highFrequency->load(), highGain->load()));
}

void FilterControl::updateChain()
{
    auto chainSettings = getChainSettings(processor.treeState);
    const auto sampleRate = processor.getSampleRate();
    if (sampleRate <= 0.0)
        return;

    const auto maximumFilterFrequency = juce::jmax(20.0f, static_cast<float>(sampleRate * 0.5));
    chainSettings.lowCutFreq = juce::jlimit(20.0f, maximumFilterFrequency, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(20.0f, maximumFilterFrequency, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(20.0f, maximumFilterFrequency, chainSettings.highCutFreq);

    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    monoChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::HighCutQ>(chainSettings.highCutBypassed);

    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients,
                       makePeakFilter(chainSettings, sampleRate));
    updateCoefficients(monoChain.get<ChainPositions::LowCutQ>().coefficients,
                       makeLowcutQFilter(chainSettings, sampleRate));
    updateCoefficients(monoChain.get<ChainPositions::HighCutQ>().coefficients,
                       makeHighcutQFilter(chainSettings, sampleRate));
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(),
                    makeLowCutFilter(chainSettings, sampleRate),
                    chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(),
                    makeHighCutFilter(chainSettings, sampleRate),
                    chainSettings.highCutSlope);
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
    const auto maximumFrequency = juce::jmin(maximumDisplayFrequency, sampleRate * 0.5);
    if (pointCount == 0 || sampleRate <= 0.0 || maximumFrequency <= minimumDisplayFrequency)
    {
        responseCurve.clear();
        responseFillCurve.clear();
        return;
    }

    if (responseMagnitudes.size() != static_cast<size_t>(pointCount))
        responseMagnitudes.resize(static_cast<size_t>(pointCount));

    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    auto& lowCutQ = monoChain.get<ChainPositions::LowCutQ>();
    auto& highCutQ = monoChain.get<ChainPositions::HighCutQ>();

    for (int i = 0; i < pointCount; ++i)
    {
        const auto proportion = static_cast<double>(i) / static_cast<double>(pointCount - 1);
        const auto frequency = juce::mapToLog10(proportion,
                                                minimumDisplayFrequency,
                                                maximumFrequency);
        double magnitude = 1.0;

        if (! monoChain.isBypassed<ChainPositions::Peak>())
            magnitude *= peak.coefficients->getMagnitudeForFrequency(frequency, sampleRate);

        if (! monoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (! lowCut.isBypassed<0>()) magnitude *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! lowCut.isBypassed<1>()) magnitude *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! lowCut.isBypassed<2>()) magnitude *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! lowCut.isBypassed<3>()) magnitude *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            magnitude *= lowCutQ.coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }

        if (! monoChain.isBypassed<ChainPositions::HighCut>())
        {
            if (! highCut.isBypassed<0>()) magnitude *= highCut.get<0>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! highCut.isBypassed<1>()) magnitude *= highCut.get<1>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! highCut.isBypassed<2>()) magnitude *= highCut.get<2>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! highCut.isBypassed<3>()) magnitude *= highCut.get<3>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            magnitude *= highCutQ.coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }

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

    const auto firstY = mapMagnitudeToY(responseMagnitudes.front());
    responseCurve.startNewSubPath(0.0f, firstY);
    responseFillCurve.startNewSubPath(0.0f, static_cast<float>(getHeight()));
    responseFillCurve.lineTo(0.0f, firstY);

    for (int i = 1; i < pointCount; ++i)
    {
        const auto x = static_cast<float>(i) / static_cast<float>(pointCount - 1)
                     * static_cast<float>(getWidth());
        const auto y = mapMagnitudeToY(responseMagnitudes[static_cast<size_t>(i)]);
        responseCurve.lineTo(x, y);
        responseFillCurve.lineTo(x, y);
    }

    responseFillCurve.lineTo(static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    responseFillCurve.closeSubPath();
}

void FilterControl::updateLfoChain(const ModulatedFilterValues& modulatedValues)
{
    ChainSettings settings;
    settings.lowCutFreq = modulatedValues.lowCutFreq;
    settings.lowCutGainInDecibels = modulatedValues.lowCutGain;
    settings.lowCutQuality = modulatedValues.lowCutQ;
    settings.highCutFreq = modulatedValues.highCutFreq;
    settings.highCutGainInDecibels = modulatedValues.highCutGain;
    settings.highCutQuality = modulatedValues.highCutQ;
    settings.peakFreq = modulatedValues.peakFreq;
    settings.peakGainInDecibels = modulatedValues.peakGain;
    settings.peakQuality = modulatedValues.peakQ;

    auto& apvts = processor.treeState;
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue(LOWCUT_SLOPE_ID)->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID)->load());
    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load() > 0.5f;

    const auto sampleRate = processor.getSampleRate();
    if (sampleRate <= 0.0)
        return;

    const auto maximumFilterFrequency = juce::jmax(20.0f, static_cast<float>(sampleRate * 0.5));
    settings.lowCutFreq = juce::jlimit(20.0f, maximumFilterFrequency, settings.lowCutFreq);
    settings.peakFreq = juce::jlimit(20.0f, maximumFilterFrequency, settings.peakFreq);
    settings.highCutFreq = juce::jlimit(20.0f, maximumFilterFrequency, settings.highCutFreq);

    lfoMonoChain.setBypassed<ChainPositions::LowCut>(settings.lowCutBypassed);
    lfoMonoChain.setBypassed<ChainPositions::Peak>(settings.peakBypassed);
    lfoMonoChain.setBypassed<ChainPositions::HighCut>(settings.highCutBypassed);
    lfoMonoChain.setBypassed<ChainPositions::LowCutQ>(settings.lowCutBypassed);
    lfoMonoChain.setBypassed<ChainPositions::HighCutQ>(settings.highCutBypassed);

    updateCoefficients(lfoMonoChain.get<ChainPositions::Peak>().coefficients,
                       makePeakFilter(settings, sampleRate));
    updateCoefficients(lfoMonoChain.get<ChainPositions::LowCutQ>().coefficients,
                       makeLowcutQFilter(settings, sampleRate));
    updateCoefficients(lfoMonoChain.get<ChainPositions::HighCutQ>().coefficients,
                       makeHighcutQFilter(settings, sampleRate));
    updateCutFilter(lfoMonoChain.get<ChainPositions::LowCut>(),
                    makeLowCutFilter(settings, sampleRate),
                    settings.lowCutSlope);
    updateCutFilter(lfoMonoChain.get<ChainPositions::HighCut>(),
                    makeHighCutFilter(settings, sampleRate),
                    settings.highCutSlope);
}

void FilterControl::updateLfoResponseCurve()
{
    const auto pointCount = getCurvePointCount();
    const auto sampleRate = processor.getSampleRate();
    const auto maximumFrequency = juce::jmin(maximumDisplayFrequency, sampleRate * 0.5);
    if (! isAnimationActive || pointCount == 0 || sampleRate <= 0.0
        || maximumFrequency <= minimumDisplayFrequency)
    {
        lfoResponseCurve.clear();
        return;
    }

    if (lfoMagnitudes.size() != static_cast<size_t>(pointCount))
        lfoMagnitudes.resize(static_cast<size_t>(pointCount));

    auto& lowCut = lfoMonoChain.get<ChainPositions::LowCut>();
    auto& peak = lfoMonoChain.get<ChainPositions::Peak>();
    auto& highCut = lfoMonoChain.get<ChainPositions::HighCut>();
    auto& lowCutQ = lfoMonoChain.get<ChainPositions::LowCutQ>();
    auto& highCutQ = lfoMonoChain.get<ChainPositions::HighCutQ>();

    for (int i = 0; i < pointCount; ++i)
    {
        const auto proportion = static_cast<double>(i) / static_cast<double>(pointCount - 1);
        const auto frequency = juce::mapToLog10(proportion,
                                                minimumDisplayFrequency,
                                                maximumFrequency);
        double magnitude = 1.0;

        if (! lfoMonoChain.isBypassed<ChainPositions::Peak>())
            magnitude *= peak.coefficients->getMagnitudeForFrequency(frequency, sampleRate);

        if (! lfoMonoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (! lowCut.isBypassed<0>()) magnitude *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! lowCut.isBypassed<1>()) magnitude *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! lowCut.isBypassed<2>()) magnitude *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! lowCut.isBypassed<3>()) magnitude *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            magnitude *= lowCutQ.coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }

        if (! lfoMonoChain.isBypassed<ChainPositions::HighCut>())
        {
            if (! highCut.isBypassed<0>()) magnitude *= highCut.get<0>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! highCut.isBypassed<1>()) magnitude *= highCut.get<1>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! highCut.isBypassed<2>()) magnitude *= highCut.get<2>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            if (! highCut.isBypassed<3>()) magnitude *= highCut.get<3>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
            magnitude *= highCutQ.coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }

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
    lfoResponseCurve.startNewSubPath(0.0f, mapMagnitudeToY(lfoMagnitudes.front()));
    for (int i = 1; i < pointCount; ++i)
    {
        const auto x = static_cast<float>(i) / static_cast<float>(pointCount - 1)
                     * static_cast<float>(getWidth());
        lfoResponseCurve.lineTo(x, mapMagnitudeToY(lfoMagnitudes[static_cast<size_t>(i)]));
    }
}
