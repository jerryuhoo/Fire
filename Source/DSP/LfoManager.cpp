/*
  ==============================================================================

    LfoManager.cpp
    Created: 10 Sep 2025 11:07:34pm
    Author:  Yifeng Yu
 
    REFACTORED to act as the central modulation controller.

  ==============================================================================
*/

#include "LfoManager.h"
#include "../GUI/InterfaceDefines.h"
#include <cmath>
#include <limits>

LfoManager::LfoManager(juce::AudioProcessorValueTreeState& apvts) : treeState(apvts)
{
    const auto indexedParameterId = [](const char* baseId, int index)
    {
        return juce::String(baseId) + juce::String(index + 1);
    };

    // Initialize LFO data containers for 4 LFOs
    lfoData.resize(4);

    // Define the string representations for synced LFO rates
    lfoRateSyncDivisions = {
        "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T", "1/2", "1 Bar", "2 Bars", "4 Bars"
    };

    for (int i = 0; i < 4; ++i)
        modulationRoutings.add({});

    for (int i = 0; i < 4; ++i)
    {
        const auto lfoIndex = static_cast<size_t>(i);
        const auto smoothnessId = indexedParameterId(LFO_SMOOTH_ID, i);

        lfoParameters[lfoIndex] = {
            treeState.getRawParameterValue(indexedParameterId(LFO_SYNC_MODE_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_RATE_SYNC_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_RATE_HZ_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_PHASE_ID, i)),
            treeState.getRawParameterValue(smoothnessId),
            treeState.getParameter(smoothnessId)
        };

        // Build and publish the default table while the processor is being
        // constructed, before any audio callback can run.
        lfoEngines[lfoIndex].stageShape(lfoData[lfoIndex]);
        lfoEngines[lfoIndex].publishStagedShape();
    }
}

void LfoManager::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto& engine : lfoEngines)
    {
        engine.prepare(spec);
    }

    preparedSampleRate = std::isfinite(spec.sampleRate) && spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    // Keep the prepared capacity. Smaller blocks no longer resize this buffer on the audio thread.
    const auto safeMaximumBlockSize = static_cast<int>(juce::jmin<uint64_t>(
        spec.maximumBlockSize, static_cast<uint64_t>(std::numeric_limits<int>::max())));
    lfoOutputBuffer.setSize(4, juce::jmax(1, safeMaximumBlockSize), false, true, false);
}

void LfoManager::reset()
{
    for (auto& engine : lfoEngines)
    {
        engine.reset();
    }
    appliedPhaseOffsets.fill(0.0f);
    phaseOffsetInitialised.fill(false);
    isPlaying.store(false, std::memory_order_relaxed);
    modulatedValueCount = 0;
    lfoOutputBuffer.clear();
}

bool LfoManager::isModulationActive() const
{
    // This is called from processBlock in older processor code, so it must never wait for the UI.
    const juce::ScopedTryLock lock(dataAccessLock);
    if (lock.isLocked())
    {
        for (const auto& routing : modulationRoutings)
            if (routing.targetParameterID.isNotEmpty())
                return true;

        return false;
    }

    return hasPublishedRouting.load(std::memory_order_relaxed);
}

// =============================================================================
// Main Processing Logic
// =============================================================================

void LfoManager::processBlock(juce::AudioBuffer<float>& outputBuffer, float sampleRate, juce::AudioPlayHead* playHead, int numSamples)
{
    refreshRuntimeStateIfAvailable();
    modulatedValueCount = 0;

    // Smoothness is a 0.01-stepped APVTS parameter. Selecting a prebuilt row is
    // audio-thread safe and remains responsive even if the UI currently holds
    // the shape/routing lock.
    for (size_t i = 0; i < lfoEngines.size(); ++i)
    {
        const auto* parameter = lfoParameters[i].smoothness;
        const float smoothness = parameter != nullptr
                                     ? parameter->load(std::memory_order_relaxed)
                                     : 0.0f;
        lfoEngines[i].setSmoothness(smoothness);
    }

    const int samplesToProcess = juce::jlimit(0, outputBuffer.getNumSamples(), numSamples);
    if (samplesToProcess <= 0)
        return;

    // Hosts should honour maximumBlockSize, but grow safely if a host sends a larger block.
    if (samplesToProcess > lfoOutputBuffer.getNumSamples())
        lfoOutputBuffer.setSize(4, samplesToProcess, false, false, true);

    // 1. Generate all raw LFO signals for the current block.
    // This fills the internal 'lfoOutputBuffer'.
    generateLfoOutput(sampleRate, playHead, samplesToProcess);

    // 2. Copy the generated LFO signals to the output buffer.
    const int channelsToCopy = juce::jmin(outputBuffer.getNumChannels(), lfoOutputBuffer.getNumChannels());
    for (int channel = 0; channel < channelsToCopy; ++channel)
    {
        outputBuffer.copyFrom(channel, 0, lfoOutputBuffer, channel, 0, samplesToProcess);
    }

    // 4. Iterate through all modulation routings to calculate final parameter values.
    for (size_t routingIndex = 0; routingIndex < runtimeRoutingCount; ++routingIndex)
    {
        const auto& routing = runtimeRoutings[routingIndex];
        if (routing.parameter == nullptr || ! juce::isPositiveAndBelow(routing.sourceLfoIndex, 4))
            continue;

        // Use the first sample of the LFO output as the representative value for the whole block.
        float lfoValue = lfoOutputBuffer.getSample(routing.sourceLfoIndex, 0);
        if (! std::isfinite(lfoValue))
            continue;

        // Get the parameter's original NORMALIZED value (from the GUI knob)
        const float rawBaseValue = routing.parameter->getValue();
        const float normalizedBaseValue = std::isfinite(rawBaseValue)
                                              ? juce::jlimit(0.0f, 1.0f, rawBaseValue)
                                              : routing.parameter->getDefaultValue();

        // For bipolar mode, the effective modulation depth should be halved to match
        // the perceived range of unipolar mode.
        float effectiveDepth = routing.depth;

        // Apply bipolar (-1 to 1) or unipolar (0 to 1) mapping to the LFO signal.
        if (routing.isBipolar)
        {
            lfoValue = lfoValue * 2.0f - 1.0f; // Map LFO from [0, 1] to [-1, 1]
            effectiveDepth *= 0.5f; // Halve the depth for bipolar
        }

        // The modulation amount is now a simple multiplication in the normalized space.
        // The depth parameter scales the LFO output directly.
        const float normalizedModulationAmount = lfoValue * effectiveDepth;

        // If this parameter hasn't been touched yet in this block, initialize it with its base normalized value.
        size_t valueIndex = 0;
        while (valueIndex < modulatedValueCount
               && modulatedValues[valueIndex].parameter != routing.parameter)
            ++valueIndex;

        if (valueIndex == modulatedValueCount)
        {
            if (modulatedValueCount >= modulatedValues.size())
                continue;

            modulatedValues[valueIndex] = { routing.parameter, normalizedBaseValue };
            ++modulatedValueCount;
        }

        // Add the normalized modulation amount. This allows multiple LFOs to target the same parameter.
        modulatedValues[valueIndex].normalisedValue += normalizedModulationAmount;
    }

    // 5. Final pass: clamp all calculated NORMALIZED values to the valid [0, 1] range.
    for (size_t i = 0; i < modulatedValueCount; ++i)
    {
        auto& value = modulatedValues[i].normalisedValue;
        value = std::isfinite(value) ? juce::jlimit(0.0f, 1.0f, value) : 0.0f;
    }
}

float LfoManager::getModulatedValue(const juce::String& parameterID) const
{
    if (auto* parameter = treeState.getParameter(parameterID))
    {
        for (size_t i = 0; i < modulatedValueCount; ++i)
        {
            if (modulatedValues[i].parameter == parameter)
                return parameter->convertFrom0to1(modulatedValues[i].normalisedValue);
        }
    }

    // If not found, it means the parameter is not being modulated.
    // Return its original value directly from the APVTS.
    // NOTE: It's safer to get the parameter and ask for its real value.
    if (auto* rawValue = treeState.getRawParameterValue(parameterID))
    {
        const float value = rawValue->load(std::memory_order_relaxed);
        if (std::isfinite(value))
            return value;
    }

    if (auto* parameter = treeState.getParameter(parameterID))
        return parameter->convertFrom0to1(parameter->getDefaultValue());

    jassertfalse; // Parameter not found
    return 0.0f;
}

bool LfoManager::getAudioThreadRoutingInfo(const juce::RangedAudioParameter* parameter,
                                           AudioThreadRoutingInfo& result) const noexcept
{
    if (parameter == nullptr)
        return false;

    // refreshRuntimeStateIfAvailable() publishes this fixed snapshot at the
    // start of the same audio callback. No UI-owned storage or lock is touched.
    for (size_t i = 0; i < runtimeRoutingCount; ++i)
    {
        const auto& routing = runtimeRoutings[i];
        if (routing.parameter == parameter)
        {
            result = { routing.sourceLfoIndex, routing.depth, routing.isBipolar };
            return true;
        }
    }

    return false;
}

// =============================================================================
// Private Helper Functions
// =============================================================================

void LfoManager::refreshRuntimeStateIfAvailable()
{
    const juce::ScopedTryLock lock(dataAccessLock);
    if (! lock.isLocked())
        return;

    runtimeRoutingCount = 0;
    bool hasAnyRouting = false;

    for (const auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID.isEmpty())
            continue;

        hasAnyRouting = true;

        if (routing.isBypassed || ! juce::isPositiveAndBelow(routing.sourceLfoIndex, 4)
            || runtimeRoutingCount >= runtimeRoutings.size())
            continue;

        auto* parameter = treeState.getParameter(routing.targetParameterID);
        if (parameter == nullptr)
            continue;

        const float depth = std::isfinite(routing.depth)
                                ? juce::jlimit(-1.0f, 1.0f, routing.depth)
                                : 0.0f;

        runtimeRoutings[runtimeRoutingCount++] = {
            parameter,
            routing.sourceLfoIndex,
            depth,
            routing.isBipolar
        };
    }

    hasPublishedRouting.store(hasAnyRouting, std::memory_order_relaxed);

    for (size_t i = 0; i < lfoEngines.size(); ++i)
    {
        lfoEngines[i].publishStagedShape();

        const auto* smoothnessParameter = lfoParameters[i].smoothness;
        if (smoothnessParameter != nullptr)
        {
            const float rawSmoothness = smoothnessParameter->load(std::memory_order_relaxed);
            const float smoothness = std::isfinite(rawSmoothness)
                                         ? juce::jlimit(0.0f, 1.0f, rawSmoothness)
                                         : 0.0f;
            if (std::abs(lfoData[i].smoothness - smoothness) > 1.0e-5f)
                lfoData[i].smoothness = smoothness;
        }
    }
}

void LfoManager::updatePublishedRoutingState() noexcept
{
    bool hasAnyRouting = false;
    for (const auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID.isNotEmpty())
        {
            hasAnyRouting = true;
            break;
        }
    }

    hasPublishedRouting.store(hasAnyRouting, std::memory_order_relaxed);
}

void LfoManager::generateLfoOutput(double sampleRate, juce::AudioPlayHead* playHead, int numSamples)
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : preparedSampleRate;

    juce::Optional<juce::AudioPlayHead::PositionInfo> positionInfo;
    bool transportIsPlaying = false;
    double currentBpm = 120.0;
    float quarterNotesPerBar = 4.0f;

    if (playHead)
    {
        positionInfo = playHead->getPosition();
        if (positionInfo)
        {
            transportIsPlaying = positionInfo->getIsPlaying();
            if (auto bpm = positionInfo->getBpm())
                if (std::isfinite(*bpm) && *bpm > 0.0)
                    currentBpm = *bpm;

            if (const auto signature = positionInfo->getTimeSignature();
                signature && signature->numerator > 0 && signature->denominator > 0)
            {
                const float candidate = static_cast<float>(signature->numerator) * 4.0f
                                        / static_cast<float>(signature->denominator);
                if (std::isfinite(candidate) && candidate > 0.0f)
                    quarterNotesPerBar = candidate;
            }
        }
    }

    isPlaying.store(transportIsPlaying, std::memory_order_relaxed);

    const auto loadParameter = [](const std::atomic<float>* parameter, float fallback) noexcept
    {
        if (parameter != nullptr)
        {
            const float value = parameter->load(std::memory_order_relaxed);
            if (std::isfinite(value))
                return value;
        }
        return fallback;
    };

    const auto wrapPhase = [](double value) noexcept
    {
        if (! std::isfinite(value))
            return 0.0f;

        value -= std::floor(value);
        return static_cast<float>(value);
    };

    for (int i = 0; i < 4; ++i)
    {
        const auto lfoIndex = static_cast<size_t>(i);
        const auto& parameters = lfoParameters[lfoIndex];
        const bool isInSyncMode = loadParameter(parameters.syncMode, 1.0f) > 0.5f;
        const int rateIndex = static_cast<int>(loadParameter(parameters.syncedRate, 8.0f));
        const float freqInHz = juce::jmax(0.0f, loadParameter(parameters.freeRate, 1.0f));
        const float phaseOffset = juce::jlimit(0.0f, 1.0f, loadParameter(parameters.phaseOffset, 0.0f));
        float phaseDelta = 0.0f;

        if (isInSyncMode)
        {
            const float beatsPerCycle = getSyncCycleLengthInQuarterNotes(rateIndex,
                                                                          quarterNotesPerBar);
            if (beatsPerCycle > 0.0f)
            {
                const double samplesPerCycle = (static_cast<double>(beatsPerCycle) / currentBpm)
                                               * 60.0 * safeSampleRate;
                if (std::isfinite(samplesPerCycle) && samplesPerCycle > 0.0)
                    phaseDelta = static_cast<float>(1.0 / samplesPerCycle);
            }
        }
        else if (safeSampleRate > 0.0)
        {
            phaseDelta = freqInHz / static_cast<float>(safeSampleRate);
        }

        // While the host is playing, derive phase from its absolute timeline so seeks are deterministic.
        // If the host is stopped or omits the required timeline coordinate, keep free-running and apply
        // only changes in the Phase offset. Reapplying the full offset every block would make it drift.
        bool usedAbsoluteTimeline = false;
        if (transportIsPlaying && positionInfo)
        {
            if (isInSyncMode)
            {
                if (auto ppq = positionInfo->getPpqPosition())
                {
                    const float cycleLengthInBeats = getSyncCycleLengthInQuarterNotes(
                        rateIndex, quarterNotesPerBar);

                    if (cycleLengthInBeats > 0.0f && std::isfinite(*ppq))
                    {
                        lfoEngines[lfoIndex].setPhase(
                            wrapPhase(*ppq / cycleLengthInBeats + phaseOffset));
                        usedAbsoluteTimeline = true;
                    }
                }
            }
            else
            {
                if (auto timeSec = positionInfo->getTimeInSeconds())
                {
                    if (std::isfinite(*timeSec))
                    {
                        lfoEngines[lfoIndex].setPhase(
                            wrapPhase(*timeSec * freqInHz + phaseOffset));
                        usedAbsoluteTimeline = true;
                    }
                }
            }
        }

        auto& engine = lfoEngines[lfoIndex];
        if (usedAbsoluteTimeline)
        {
            appliedPhaseOffsets[lfoIndex] = phaseOffset;
            phaseOffsetInitialised[lfoIndex] = true;
        }
        else
        {
            const float previousOffset = phaseOffsetInitialised[lfoIndex]
                                             ? appliedPhaseOffsets[lfoIndex]
                                             : 0.0f;
            const float offsetDelta = phaseOffset - previousOffset;
            if (offsetDelta != 0.0f)
                engine.setPhase(wrapPhase(engine.getPhase() + offsetDelta));

            appliedPhaseOffsets[lfoIndex] = phaseOffset;
            phaseOffsetInitialised[lfoIndex] = true;
        }

        engine.setPhaseDelta(phaseDelta);

        auto* writer = lfoOutputBuffer.getWritePointer(i);
        for (int sample = 0; sample < numSamples; ++sample)
            writer[sample] = engine.process();
    }
}

float LfoManager::getSyncCycleLengthInQuarterNotes(int index,
                                                   float quarterNotesPerBar) const noexcept
{
    const float safeQuarterNotesPerBar = std::isfinite(quarterNotesPerBar)
                                             && quarterNotesPerBar > 0.0f
                                         ? quarterNotesPerBar
                                         : 4.0f;

    if (index >= 11 && index <= 13)
        return safeQuarterNotesPerBar * static_cast<float>(1 << (index - 11));

    return mapRateSyncIndexToBeatMultiplier(index) * 4.0f;
}

float LfoManager::mapRateSyncIndexToBeatMultiplier(int index) const
{
    // "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T", "1/2", "1 Bar", "2 Bars", "4 Bars"
    switch (index)
    {
        case 0:
            return 1.0f / 64.0f; // 1/64
        case 1:
            return 1.0f / 32.0f * 2.0f / 3.0f; // 1/32T
        case 2:
            return 1.0f / 32.0f; // 1/32
        case 3:
            return 1.0f / 16.0f * 2.0f / 3.0f; // 1/16T
        case 4:
            return 1.0f / 16.0f; // 1/16
        case 5:
            return 1.0f / 8.0f * 2.0f / 3.0f; // 1/8T
        case 6:
            return 1.0f / 8.0f; // 1/8
        case 7:
            return 1.0f / 4.0f * 2.0f / 3.0f; // 1/4T
        case 8:
            return 1.0f / 4.0f; // 1/4
        case 9:
            return 1.0f / 2.0f * 2.0f / 3.0f; // 1/2T
        case 10:
            return 1.0f / 2.0f; // 1/2
        case 11:
            return 1.0f; // 1 Bar
        case 12:
            return 2.0f; // 2 Bars
        case 13:
            return 4.0f; // 4 Bars
        default:
            return 1.0f / 4.0f;
    }
}

void LfoManager::onLfoShapeChanged(int lfoIndex)
{
    const juce::ScopedLock lock(dataAccessLock);

    if (lfoIndex < 0)
    {
        for (size_t i = 0; i < lfoEngines.size(); ++i)
            lfoEngines[i].stageShape(lfoData[i]);
    }
    else if (juce::isPositiveAndBelow(lfoIndex, 4))
    {
        const auto index = static_cast<size_t>(lfoIndex);
        lfoEngines[index].stageShape(lfoData[index]);
    }
}

// =============================================================================
// Accessors for UI
// =============================================================================

float LfoManager::getLfoPhase(int lfoIndex) const
{
    if (juce::isPositiveAndBelow(lfoIndex, (int) lfoEngines.size()))
    {
        return lfoEngines[static_cast<size_t>(lfoIndex)].getPhase();
    }
    jassertfalse; // Invalid LFO index requested
    return 0.0f;
}

const juce::StringArray& LfoManager::getLfoRateSyncDivisions() const
{
    return lfoRateSyncDivisions;
}

juce::Array<ModulationRouting> LfoManager::getModulationRoutingsCopy() const
{
    const juce::ScopedLock lock(dataAccessLock);
    return modulationRoutings;
}

std::vector<LfoData> LfoManager::getLfoDataCopy() const
{
    const juce::ScopedLock lock(dataAccessLock);
    auto result = lfoData;

    // Smoothness is an automatable APVTS parameter and may have changed while
    // the transport/editor was stopped, before the audio thread had a chance
    // to mirror it into lfoData. Always expose a coherent, authoritative copy.
    for (size_t i = 0; i < result.size() && i < lfoParameters.size(); ++i)
    {
        if (const auto* parameter = lfoParameters[i].smoothness)
        {
            const float value = parameter->load(std::memory_order_relaxed);
            if (std::isfinite(value))
                result[i].smoothness = juce::jlimit(0.0f, 1.0f, value);
        }
    }

    return result;
}

float LfoManager::getLfoOutput(int lfoIndex) const
{
    if (juce::isPositiveAndBelow(lfoIndex, (int) lfoEngines.size()))
    {
        return lfoEngines[static_cast<size_t>(lfoIndex)].getLastOutput();
    }

    jassertfalse; // Invalid LFO index requested
    return 0.0f;
}

void LfoManager::assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID)
{
    if (! juce::isPositiveAndBelow(sourceLfoIndex, 4) || targetParameterID.isEmpty())
    {
        jassertfalse;
        return;
    }

    const juce::ScopedLock sl(dataAccessLock);
    // 1. First, check if the target parameter is already being modulated.
    //    If so, just update its LFO source.
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID == targetParameterID)
        {
            routing.sourceLfoIndex = sourceLfoIndex;
            updatePublishedRoutingState();
            return; // Assignment complete.
        }
    }

    // 2. If the target is not modulated, find the first available empty slot.
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID.isEmpty())
        {
            routing.sourceLfoIndex = sourceLfoIndex;
            routing.targetParameterID = targetParameterID;
            if (juce::approximatelyEqual(routing.depth, 0.0f))
                routing.depth = 0.5f; // Set a sensible default depth.
            routing.isBypassed = false;
            updatePublishedRoutingState();
            return; // Assignment complete.
        }
    }

    // 3. If all existing slots are full, dynamically add a new one.

    // Step 1: Add a new, default-constructed ModulationRouting object to the array.
    modulationRoutings.add({});

    // Step 2: Get a reference to the last element.
    auto& newRouting = modulationRoutings.getReference(modulationRoutings.size() - 1);

    // Step 3: Configure the new routing slot.
    newRouting.sourceLfoIndex = sourceLfoIndex;
    newRouting.targetParameterID = targetParameterID;
    newRouting.depth = 0.5f; // Set a sensible default depth.
    updatePublishedRoutingState();
}

void LfoManager::clearModulationForTarget(const juce::String& targetParameterID)
{
    const juce::ScopedLock sl(dataAccessLock);
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID == targetParameterID)
        {
            // Unbind by clearing the target ID
            routing.targetParameterID = juce::String();

            // (Optional) Reset other parameters to their default values to maintain a clean state
            routing.depth = 0.5f;
            routing.isBipolar = true;
            routing.isBypassed = false;
            updatePublishedRoutingState();
            return; // Exit after finding and clearing
        }
    }
}

void LfoManager::invertModulationDepth(const juce::String& targetParameterID)
{
    const juce::ScopedLock sl(dataAccessLock);
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID == targetParameterID)
        {
            routing.depth = std::isfinite(routing.depth)
                                ? juce::jlimit(-1.0f, 1.0f, -routing.depth)
                                : -0.5f;
            return;
        }
    }
}

void LfoManager::toggleBypassForRouting(const juce::String& targetParameterID)
{
    const juce::ScopedLock sl(dataAccessLock);
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID == targetParameterID)
        {
            routing.isBypassed = ! routing.isBypassed;
            return; // Assuming one routing per target for now
        }
    }
}

void LfoManager::setLfoData(int index, const LfoData& newData)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(lfoData.size())))
    {
        jassertfalse;
        return;
    }

    auto safeData = newData;
    safeData.sanitise();

    const juce::ScopedLock sl(dataAccessLock);
    const auto lfoIndex = static_cast<size_t>(index);
    const bool shapeChanged = lfoData[lfoIndex].points != safeData.points
                              || lfoData[lfoIndex].curvatures != safeData.curvatures;

    // APVTS is the automation/state authority for smoothness. Keep it in sync
    // whenever a complete shape is restored or edited so the next audio block
    // cannot overwrite the shape's smoothness with a stale/default parameter.
    if (auto* parameter = lfoParameters[lfoIndex].smoothnessParameter)
    {
        const float normalisedSmoothness = parameter->convertTo0to1(safeData.smoothness);
        if (! juce::approximatelyEqual(parameter->getValue(), normalisedSmoothness))
            parameter->setValueNotifyingHost(normalisedSmoothness);
    }

    if (shapeChanged)
        lfoEngines[lfoIndex].stageShape(safeData);

    lfoData[lfoIndex] = std::move(safeData);
}

void LfoManager::clearAllLfoData()
{
    const juce::ScopedLock sl(dataAccessLock);
    for (size_t i = 0; i < lfoData.size(); ++i)
    {
        lfoData[i].resetToDefault();
        lfoEngines[i].stageShape(lfoData[i]);
    }
}
