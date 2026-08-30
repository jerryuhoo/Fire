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

namespace
{
bool haveSameModulationRoutingState(const ModulationRouting& lhs,
                                    const ModulationRouting& rhs) noexcept
{
    return lhs.sourceLfoIndex == rhs.sourceLfoIndex
        && lhs.targetParameterID == rhs.targetParameterID
        && juce::exactlyEqual(lhs.depth, rhs.depth)
        && lhs.isBipolar == rhs.isBipolar
        && lhs.isBypassed == rhs.isBypassed;
}
} // namespace

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
        smoothnessParameterIDs[lfoIndex] = smoothnessId;

        lfoParameters[lfoIndex] = {
            treeState.getRawParameterValue(indexedParameterId(LFO_SYNC_MODE_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_RATE_SYNC_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_RATE_HZ_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_PHASE_ID, i)),
            treeState.getRawParameterValue(smoothnessId)
        };

        if (const auto* smoothness = lfoParameters[lfoIndex].smoothness)
        {
            const float value = smoothness->load(std::memory_order_relaxed);
            if (std::isfinite(value))
                lfoData[lfoIndex].smoothness = juce::jlimit(0.0f, 1.0f, value);
        }

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
    timingSignatureInitialised.fill(false);
    previousSyncModes.fill(false);
    previousActiveRateKeys.fill(0.0f);
    usedAbsoluteTimelineLastBlock.fill(false);
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

LfoManager::AudioThreadParameterSnapshot
LfoManager::captureAudioThreadParameterSnapshot() const noexcept
{
    AudioThreadParameterSnapshot snapshot;
    const auto loadParameter = [](const std::atomic<float>* parameter,
                                  float fallback) noexcept
    {
        if (parameter != nullptr)
        {
            const float value = parameter->load(std::memory_order_relaxed);
            if (std::isfinite(value))
                return value;
        }

        return fallback;
    };

    for (size_t index = 0; index < snapshot.lfos.size(); ++index)
    {
        const auto& source = lfoParameters[index];
        auto& destination = snapshot.lfos[index];
        destination.syncMode = loadParameter(source.syncMode, 1.0f);
        destination.syncedRate = loadParameter(source.syncedRate, 8.0f);
        destination.freeRate = loadParameter(source.freeRate, 1.0f);
        destination.phaseOffset = loadParameter(source.phaseOffset, 0.0f);
        destination.smoothness = loadParameter(source.smoothness, 0.0f);
    }

    return snapshot;
}

void LfoManager::processBlock(juce::AudioBuffer<float>& outputBuffer, float sampleRate, juce::AudioPlayHead* playHead, int numSamples)
{
    routingSnapshotRefreshedThisBlock = refreshRuntimeStateIfAvailable();
    renderBlock(outputBuffer,
                sampleRate,
                playHead,
                numSamples,
                captureAudioThreadParameterSnapshot(),
                true,
                0);
}

void LfoManager::processBlock(
    juce::AudioBuffer<float>& outputBuffer,
    float sampleRate,
    juce::AudioPlayHead* playHead,
    int numSamples,
    const AudioThreadParameterSnapshot& parameterSnapshot,
    juce::int64 playheadSampleOffset)
{
    // A successful candidate capture deliberately owns dataAccessLock until
    // finish or abort. Rendering while that transaction is open would make a
    // forgotten finish silently block every subsequent producer.
    jassert(! runtimeStateCaptureInProgress);
    renderBlock(outputBuffer,
                sampleRate,
                playHead,
                numSamples,
                parameterSnapshot,
                false,
                playheadSampleOffset);
}

void LfoManager::renderBlock(
    juce::AudioBuffer<float>& outputBuffer,
    float sampleRate,
    juce::AudioPlayHead* playHead,
    int numSamples,
    const AudioThreadParameterSnapshot& parameterSnapshot,
    bool readLiveRoutingBaseValues,
    juce::int64 playheadSampleOffset)
{
    modulatedValueCount = 0;

    // Smoothness is a 0.01-stepped parameter. Selecting a prebuilt row is
    // audio-thread safe; the snapshot overload deliberately does not revisit
    // APVTS while an outer state transaction may be in progress.
    for (size_t i = 0; i < lfoEngines.size(); ++i)
        lfoEngines[i].setSmoothness(parameterSnapshot.lfos[i].smoothness);

    const int samplesToProcess = juce::jlimit(0, outputBuffer.getNumSamples(), numSamples);
    if (samplesToProcess <= 0)
        return;

    const int rangeCapacity = lfoOutputBuffer.getNumSamples();
    if (rangeCapacity <= 0)
    {
        // processBlock() is an audio callback API and therefore requires a
        // matching prepare(). Allocating here would hide that contract by
        // doing heap work on the real-time thread.
        jassertfalse;
        return;
    }

    std::array<float, 4> firstLfoValues {};
    const int channelsToCopy = juce::jmin(outputBuffer.getNumChannels(),
                                          lfoOutputBuffer.getNumChannels());
    int sampleOffset = 0;
    while (sampleOffset < samplesToProcess)
    {
        const int samplesInRange = juce::jmin(
            rangeCapacity, samplesToProcess - sampleOffset);

        // Render into the fixed buffer prepared off the audio thread. The
        // timeline offset keeps transport-synchronised LFOs continuous when a
        // host exceeds its advertised maximum callback size.
        generateLfoOutput(sampleRate,
                          playHead,
                          samplesInRange,
                          parameterSnapshot,
                          playheadSampleOffset + sampleOffset);

        if (sampleOffset == 0)
            for (size_t channel = 0; channel < firstLfoValues.size(); ++channel)
                firstLfoValues[channel] = lfoOutputBuffer.getSample(
                    static_cast<int>(channel), 0);

        for (int channel = 0; channel < channelsToCopy; ++channel)
            outputBuffer.copyFrom(channel,
                                  sampleOffset,
                                  lfoOutputBuffer,
                                  channel,
                                  0,
                                  samplesInRange);

        sampleOffset += samplesInRange;
    }

    // 4. Iterate through all modulation routings to calculate final parameter values.
    for (size_t routingIndex = 0; routingIndex < runtimeRoutingCount; ++routingIndex)
    {
        const auto& routing = runtimeRoutings[routingIndex];
        if (routing.parameter == nullptr || ! juce::isPositiveAndBelow(routing.sourceLfoIndex, 4))
            continue;

        // Use the first sample of the LFO output as the representative value for the whole block.
        float lfoValue = firstLfoValues[
            static_cast<size_t>(routing.sourceLfoIndex)];
        if (! std::isfinite(lfoValue))
            continue;

        // The snapshot overload must not revisit APVTS. The legacy overload
        // retains its live-base behaviour for standalone callers and tests.
        const float rawBaseValue = readLiveRoutingBaseValues
                                       ? routing.parameter->getValue()
                                       : routing.normalisedBaseValue;
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

    // While a two-phase capture is open the processor must be able to build a
    // complete callback candidate without changing the currently audible
    // routes. Both arrays are fixed audio-thread storage; no UI container or
    // lock is touched by this lookup.
    const auto& routings = runtimeStateCaptureInProgress
                               ? candidateRuntimeRoutings
                               : runtimeRoutings;
    const auto routingCount = runtimeStateCaptureInProgress
                                  ? candidateRuntimeRoutingCount
                                  : runtimeRoutingCount;
    for (size_t i = 0; i < routingCount; ++i)
    {
        const auto& routing = routings[i];
        if (routing.parameter == parameter)
        {
            result = { routing.sourceLfoIndex, routing.depth, routing.isBipolar };
            return true;
        }
    }

    return false;
}

bool LfoManager::beginAudioThreadStateCapture(
    std::uint32_t expectedGeneration,
    const std::atomic<std::uint32_t>& generation) noexcept
{
    routingSnapshotRefreshedThisBlock = false;
    if (runtimeStateCaptureInProgress)
    {
        jassertfalse;
        return false;
    }

    if ((expectedGeneration & 1u) != 0u
        || generation.load(std::memory_order_acquire) != expectedGeneration
        || ! dataAccessLock.tryEnter())
    {
        return false;
    }

    // The outer writer publishes odd before touching APVTS/LFO state. Check
    // again after acquiring the LFO lock so a writer that won the race cannot
    // leak a partially staged route set into this candidate.
    if (generation.load(std::memory_order_acquire) != expectedGeneration)
    {
        dataAccessLock.exit();
        return false;
    }

    candidateHasPublishedRouting = captureRuntimeRoutings(
        candidateRuntimeRoutings,
        candidateRuntimeRoutingCount);
    runtimeStateCaptureInProgress = true;
    return true;
}

bool LfoManager::finishAudioThreadStateCapture(bool commitCandidate) noexcept
{
    if (! runtimeStateCaptureInProgress)
    {
        routingSnapshotRefreshedThisBlock = false;
        return false;
    }

    if (commitCandidate)
    {
        runtimeRoutings = candidateRuntimeRoutings;
        runtimeRoutingCount = candidateRuntimeRoutingCount;
        hasPublishedRouting.store(candidateHasPublishedRouting,
                                  std::memory_order_relaxed);
        for (auto& engine : lfoEngines)
            engine.publishStagedShape();
    }

    runtimeStateCaptureInProgress = false;
    dataAccessLock.exit();
    routingSnapshotRefreshedThisBlock = commitCandidate;
    return commitCandidate;
}

void LfoManager::abortAudioThreadStateCapture() noexcept
{
    if (! runtimeStateCaptureInProgress)
        return;

    runtimeStateCaptureInProgress = false;
    dataAccessLock.exit();
    routingSnapshotRefreshedThisBlock = false;
}

// =============================================================================
// Private Helper Functions
// =============================================================================

bool LfoManager::captureRuntimeRoutings(
    std::array<RuntimeRouting, maximumModulationRoutings>& destination,
    size_t& destinationCount) const
{
    destinationCount = 0;
    bool hasAnyRouting = false;

    for (const auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID.isEmpty())
            continue;

        hasAnyRouting = true;
        if (routing.isBypassed
            || ! juce::isPositiveAndBelow(routing.sourceLfoIndex, 4)
            || destinationCount >= destination.size())
        {
            continue;
        }

        auto* parameter = treeState.getParameter(routing.targetParameterID);
        if (parameter == nullptr)
            continue;

        const float depth = std::isfinite(routing.depth)
                                ? juce::jlimit(-1.0f, 1.0f, routing.depth)
                                : 0.0f;
        const float rawBaseValue = parameter->getValue();
        const float normalisedBaseValue = std::isfinite(rawBaseValue)
                                              ? juce::jlimit(0.0f,
                                                             1.0f,
                                                             rawBaseValue)
                                              : parameter->getDefaultValue();

        destination[destinationCount++] = {
            parameter,
            routing.sourceLfoIndex,
            depth,
            routing.isBipolar,
            normalisedBaseValue
        };
    }

    return hasAnyRouting;
}

bool LfoManager::refreshRuntimeStateIfAvailable()
{
    const juce::ScopedTryLock lock(dataAccessLock);
    if (! lock.isLocked())
        return false;

    const bool hasAnyRouting = captureRuntimeRoutings(runtimeRoutings,
                                                       runtimeRoutingCount);
    hasPublishedRouting.store(hasAnyRouting, std::memory_order_relaxed);

    for (size_t i = 0; i < lfoEngines.size(); ++i)
        lfoEngines[i].publishStagedShape();

    return true;
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

void LfoManager::generateLfoOutput(
    double sampleRate,
    juce::AudioPlayHead* playHead,
    int numSamples,
    const AudioThreadParameterSnapshot& parameterSnapshot,
    juce::int64 playheadSampleOffset)
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : preparedSampleRate;
    const double timelineOffsetSeconds =
        static_cast<double>(playheadSampleOffset) / safeSampleRate;

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

    const auto wrapPhase = [](double value) noexcept
    {
        if (! std::isfinite(value))
            return 0.0f;

        value -= std::floor(value);
        return static_cast<float>(value);
    };

    const auto wrappedPhaseDistance = [](float first, float second) noexcept
    {
        if (! std::isfinite(first) || ! std::isfinite(second))
            return 0.0f;

        float difference = first - second;
        difference -= std::round(difference);
        return std::abs(difference);
    };

    for (int i = 0; i < 4; ++i)
    {
        const auto lfoIndex = static_cast<size_t>(i);
        const auto& parameters = parameterSnapshot.lfos[lfoIndex];
        const float syncMode = std::isfinite(parameters.syncMode)
                                   ? parameters.syncMode
                                   : 1.0f;
        const float syncedRate = std::isfinite(parameters.syncedRate)
                                     ? juce::jlimit(0.0f,
                                                    13.0f,
                                                    parameters.syncedRate)
                                     : 8.0f;
        const float freeRate = std::isfinite(parameters.freeRate)
                                   ? parameters.freeRate
                                   : 1.0f;
        const float rawPhaseOffset = std::isfinite(parameters.phaseOffset)
                                         ? parameters.phaseOffset
                                         : 0.0f;
        const bool isInSyncMode = syncMode > 0.5f;
        const int rateIndex = static_cast<int>(syncedRate);
        const float freqInHz = juce::jmax(0.0f, freeRate);
        const float phaseOffset = juce::jlimit(0.0f,
                                               1.0f,
                                               rawPhaseOffset);
        float phaseDelta = 0.0f;
        float activeRateKey = freqInHz;
        float syncCycleLengthInBeats = 0.0f;

        if (isInSyncMode)
        {
            syncCycleLengthInBeats = getSyncCycleLengthInQuarterNotes(
                rateIndex,
                quarterNotesPerBar);
            activeRateKey = syncCycleLengthInBeats;
            if (syncCycleLengthInBeats > 0.0f)
            {
                const double samplesPerCycle = (static_cast<double>(syncCycleLengthInBeats) / currentBpm)
                                               * 60.0 * safeSampleRate;
                if (std::isfinite(samplesPerCycle) && samplesPerCycle > 0.0)
                    phaseDelta = static_cast<float>(1.0 / samplesPerCycle);
            }
        }
        else if (safeSampleRate > 0.0)
        {
            phaseDelta = freqInHz / static_cast<float>(safeSampleRate);
        }

        auto& engine = lfoEngines[lfoIndex];
        const bool phaseChanged = phaseOffsetInitialised[lfoIndex]
                                  && std::abs(phaseOffset
                                              - appliedPhaseOffsets[lfoIndex])
                                         > 1.0e-6f;
        const bool rateSignatureChanged = timingSignatureInitialised[lfoIndex]
                                          && (previousSyncModes[lfoIndex]
                                                  != isInSyncMode
                                              || std::abs(previousActiveRateKeys[lfoIndex]
                                                          - activeRateKey)
                                                     > 1.0e-6f);

        // While the host is playing, derive phase from its absolute timeline so seeks are deterministic.
        // If the host is stopped or omits the required timeline coordinate, keep free-running and apply
        // only changes in the Phase offset. Reapplying the full offset every block would make it drift.
        bool usedAbsoluteTimeline = false;
        const auto synchroniseAbsolutePhase = [&] (float canonicalPhase)
        {
            const float seekTolerance = juce::jlimit(
                1.0e-3f,
                1.0e-2f,
                4.0f * std::abs(phaseDelta));
            const bool timelineJump =
                timingSignatureInitialised[lfoIndex]
                && usedAbsoluteTimelineLastBlock[lfoIndex]
                && ! rateSignatureChanged
                && ! phaseChanged
                && wrappedPhaseDistance(engine.getPhase(), canonicalPhase)
                       > seekTolerance;
            const bool shouldCorrect =
                timingSignatureInitialised[lfoIndex]
                && (rateSignatureChanged
                    || phaseChanged
                    || ! usedAbsoluteTimelineLastBlock[lfoIndex]
                    || timelineJump);

            if (shouldCorrect)
                engine.setPhaseWithCorrection(canonicalPhase);
            else
                engine.setPhase(canonicalPhase);
            usedAbsoluteTimeline = true;
        };

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
                        synchroniseAbsolutePhase(wrapPhase(
                            (*ppq
                             + timelineOffsetSeconds * currentBpm / 60.0)
                                / cycleLengthInBeats
                            + phaseOffset));
                    }
                }
            }
            else
            {
                if (auto timeSec = positionInfo->getTimeInSeconds())
                {
                    if (std::isfinite(*timeSec))
                    {
                        synchroniseAbsolutePhase(wrapPhase(
                            (*timeSec + timelineOffsetSeconds) * freqInHz
                            + phaseOffset));
                    }
                }
            }
        }
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
                engine.setPhaseWithCorrection(
                    wrapPhase(engine.getPhase() + offsetDelta));

            appliedPhaseOffsets[lfoIndex] = phaseOffset;
            phaseOffsetInitialised[lfoIndex] = true;
        }

        timingSignatureInitialised[lfoIndex] = true;
        previousSyncModes[lfoIndex] = isInSyncMode;
        previousActiveRateKeys[lfoIndex] = activeRateKey;
        usedAbsoluteTimelineLastBlock[lfoIndex] = usedAbsoluteTimeline;

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

LfoManager::ModulationRoutingStateSnapshot
LfoManager::getModulationRoutingStateSnapshot() const
{
    const juce::ScopedLock lock(dataAccessLock);
    return { modulationRoutings, modulationRoutingRevision };
}

std::uint64_t LfoManager::getModulationRoutingRevision() const
{
    const juce::ScopedLock lock(dataAccessLock);
    return modulationRoutingRevision;
}

LfoManager::ModulationRoutingEditResult
LfoManager::updateModulationRoutingIfRevisionMatches(
    int routingIndex,
    std::uint64_t expectedRevision,
    const ModulationRouting& expectedRouting,
    const ModulationRouting& replacementRouting)
{
    auto safeReplacement = replacementRouting;
    safeReplacement.sanitise();
    if (safeReplacement.sourceLfoIndex != expectedRouting.sourceLfoIndex
        || safeReplacement.targetParameterID
               != expectedRouting.targetParameterID)
        return {};

    const juce::ScopedLock lock(dataAccessLock);
    ModulationRoutingEditResult result;
    result.revision = modulationRoutingRevision;
    if (modulationRoutingRevision != expectedRevision
        || ! juce::isPositiveAndBelow(routingIndex,
                                      modulationRoutings.size()))
        return result;

    auto& currentRouting = modulationRoutings.getReference(routingIndex);
    if (! haveSameModulationRoutingState(currentRouting, expectedRouting))
        return result;

    result.accepted = true;
    if (! haveSameModulationRoutingState(currentRouting, safeReplacement))
    {
        currentRouting = safeReplacement;
        result.changed = true;
        result.revision = advanceModulationRoutingRevisionLocked();
    }

    result.routing = currentRouting;
    return result;
}

LfoManager::ModulationRoutingEditResult
LfoManager::assignModulationRoutingIfRevisionMatches(
    int routingIndex,
    std::uint64_t expectedRevision,
    const ModulationRouting& expectedRouting,
    int sourceLfoIndex,
    const juce::String& targetParameterID)
{
    if (targetParameterID.isNotEmpty()
        && (! juce::isPositiveAndBelow(sourceLfoIndex, 4)
            || treeState.getParameter(targetParameterID) == nullptr))
        return {};

    const int safeSourceIndex = juce::jlimit(0, 3, sourceLfoIndex);
    const juce::ScopedLock lock(dataAccessLock);
    ModulationRoutingEditResult result;
    result.revision = modulationRoutingRevision;
    if (modulationRoutingRevision != expectedRevision
        || ! juce::isPositiveAndBelow(routingIndex,
                                      modulationRoutings.size()))
        return result;

    auto& currentRouting = modulationRoutings.getReference(routingIndex);
    if (! haveSameModulationRoutingState(currentRouting, expectedRouting))
        return result;

    result.accepted = true;
    if (targetParameterID.isNotEmpty())
    {
        for (int i = 0; i < modulationRoutings.size(); ++i)
        {
            auto& candidate = modulationRoutings.getReference(i);
            if (i != routingIndex
                && candidate.targetParameterID == targetParameterID)
            {
                candidate.targetParameterID.clear();
                result.changed = true;
            }
        }
    }

    if (currentRouting.sourceLfoIndex != safeSourceIndex)
    {
        currentRouting.sourceLfoIndex = safeSourceIndex;
        result.changed = true;
    }
    if (currentRouting.targetParameterID != targetParameterID)
    {
        currentRouting.targetParameterID = targetParameterID;
        result.changed = true;
    }

    if (result.changed)
        result.revision = advanceModulationRoutingRevisionLocked();

    result.routing = currentRouting;
    return result;
}

LfoManager::ModulationRoutingEditResult
LfoManager::addEmptyModulationRoutingIfRevisionMatches(
    std::uint64_t expectedRevision)
{
    const juce::ScopedLock lock(dataAccessLock);
    ModulationRoutingEditResult result;
    result.revision = modulationRoutingRevision;
    if (modulationRoutingRevision != expectedRevision)
        return result;

    result.accepted = true;
    if (modulationRoutings.size() >= maximumModulationRoutings)
        return result;

    modulationRoutings.add({});
    result.changed = true;
    result.revision = advanceModulationRoutingRevisionLocked();
    result.routing = modulationRoutings.getReference(
        modulationRoutings.size() - 1);
    return result;
}

LfoManager::ModulationRoutingEditResult
LfoManager::removeModulationRoutingIfRevisionMatches(
    int routingIndex,
    std::uint64_t expectedRevision,
    const ModulationRouting& expectedRouting)
{
    const juce::ScopedLock lock(dataAccessLock);
    ModulationRoutingEditResult result;
    result.revision = modulationRoutingRevision;
    if (modulationRoutingRevision != expectedRevision
        || ! juce::isPositiveAndBelow(routingIndex,
                                      modulationRoutings.size())
        || ! haveSameModulationRoutingState(
            modulationRoutings.getReference(routingIndex),
            expectedRouting))
        return result;

    modulationRoutings.remove(routingIndex);
    result.accepted = true;
    result.changed = true;
    result.revision = advanceModulationRoutingRevisionLocked();
    return result;
}

std::uint64_t LfoManager::advanceModulationRoutingRevisionLocked() noexcept
{
    ++modulationRoutingRevision;
    updatePublishedRoutingState();
    return modulationRoutingRevision;
}

LfoManager::SerializableStateSnapshot
LfoManager::captureSerializableStateSnapshot() const
{
    SerializableStateSnapshot snapshot;

    // APVTS is the single writable authority for smoothness. The processor's
    // outer topology-reader protocol prevents parameter/routing migrations in
    // public serializers; ordinary shape edits are independent and become
    // visible atomically under dataAccessLock below.
    snapshot.parameterState = treeState.copyState();
    {
        const juce::ScopedLock lock(dataAccessLock);
        snapshot.lfoData = lfoData;
        snapshot.routings = modulationRoutings;
    }

    for (size_t i = 0; i < snapshot.lfoData.size(); ++i)
    {
        for (const auto& child : snapshot.parameterState)
        {
            if (child.getProperty("id").toString()
                != smoothnessParameterIDs[i])
                continue;

            const float smoothness = static_cast<float>(
                child.getProperty("value"));
            if (std::isfinite(smoothness))
            {
                snapshot.lfoData[i].smoothness = juce::jlimit(
                    0.0f, 1.0f, smoothness);
            }
            break;
        }
    }

    return snapshot;
}

std::vector<LfoData> LfoManager::getLfoDataCopy() const
{
    const juce::ScopedLock lock(dataAccessLock);
    auto result = lfoData;
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

LfoManager::LfoDataSnapshot LfoManager::getLfoDataSnapshot(int index) const
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(lfoData.size())))
    {
        jassertfalse;
        return {};
    }

    const auto lfoIndex = static_cast<size_t>(index);
    LfoDataSnapshot snapshot;
    {
        const juce::ScopedLock lock(dataAccessLock);
        snapshot.data = lfoData[lfoIndex];
        snapshot.revision = lfoDataRevisions[lfoIndex];
    }

    if (const auto* parameter = lfoParameters[lfoIndex].smoothness)
    {
        const float smoothness = parameter->load(std::memory_order_relaxed);
        if (std::isfinite(smoothness))
            snapshot.data.smoothness = juce::jlimit(0.0f, 1.0f, smoothness);
    }

    return snapshot;
}

bool LfoManager::isLfoDataRevisionCurrent(
    int index,
    std::uint64_t revision) const
{
    if (! juce::isPositiveAndBelow(index,
                                   static_cast<int>(lfoDataRevisions.size())))
        return false;

    const juce::ScopedLock lock(dataAccessLock);
    return lfoDataRevisions[static_cast<size_t>(index)] == revision;
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

LfoManager::AssignmentResult LfoManager::assignLfoToTarget(
    int sourceLfoIndex,
    const juce::String& targetParameterID)
{
    if (! juce::isPositiveAndBelow(sourceLfoIndex, 4) || targetParameterID.isEmpty())
    {
        jassertfalse;
        return AssignmentResult::invalidRequest;
    }

    const juce::ScopedLock sl(dataAccessLock);
    // 1. First, check if the target parameter is already being modulated.
    //    If so, just update its LFO source.
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID == targetParameterID)
        {
            if (routing.sourceLfoIndex != sourceLfoIndex)
            {
                routing.sourceLfoIndex = sourceLfoIndex;
                advanceModulationRoutingRevisionLocked();
                return AssignmentResult::changed;
            }
            return AssignmentResult::unchanged;
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
            advanceModulationRoutingRevisionLocked();
            return AssignmentResult::changed;
        }
    }

    // 3. If all existing slots are full, dynamically add a new one.
    if (modulationRoutings.size() >= maximumModulationRoutings)
        return AssignmentResult::capacityReached;

    // Step 1: Add a new, default-constructed ModulationRouting object to the array.
    modulationRoutings.add({});

    // Step 2: Get a reference to the last element.
    auto& newRouting = modulationRoutings.getReference(modulationRoutings.size() - 1);

    // Step 3: Configure the new routing slot.
    newRouting.sourceLfoIndex = sourceLfoIndex;
    newRouting.targetParameterID = targetParameterID;
    newRouting.depth = 0.5f; // Set a sensible default depth.
    advanceModulationRoutingRevisionLocked();
    return AssignmentResult::changed;
}

void LfoManager::clearModulationForTarget(const juce::String& targetParameterID)
{
    const juce::ScopedLock sl(dataAccessLock);
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID == targetParameterID)
        {
            const auto previousRouting = routing;
            // Unbind by clearing the target ID
            routing.targetParameterID = juce::String();

            // (Optional) Reset other parameters to their default values to maintain a clean state
            routing.depth = 0.5f;
            routing.isBipolar = true;
            routing.isBypassed = false;
            if (! haveSameModulationRoutingState(routing, previousRouting))
                advanceModulationRoutingRevisionLocked();
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
            const float newDepth = std::isfinite(routing.depth)
                                       ? juce::jlimit(-1.0f, 1.0f,
                                                      -routing.depth)
                                       : -0.5f;
            if (! juce::exactlyEqual(routing.depth, newDepth))
            {
                routing.depth = newDepth;
                advanceModulationRoutingRevisionLocked();
            }
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
            advanceModulationRoutingRevisionLocked();
            return; // Assuming one routing per target for now
        }
    }
}

std::uint64_t LfoManager::setLfoData(int index, const LfoData& newData)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(lfoData.size())))
    {
        jassertfalse;
        return 0;
    }

    auto safeData = newData;
    safeData.sanitise();

    const auto lfoIndex = static_cast<size_t>(index);
    if (const auto* parameter = lfoParameters[lfoIndex].smoothness)
    {
        const float smoothness = parameter->load(std::memory_order_relaxed);
        if (std::isfinite(smoothness))
            safeData.smoothness = juce::jlimit(0.0f, 1.0f, smoothness);
    }

    const juce::ScopedLock sl(dataAccessLock);
    const bool shapeChanged = lfoData[lfoIndex].points != safeData.points
                              || lfoData[lfoIndex].curvatures != safeData.curvatures;

    if (shapeChanged)
    {
        lfoEngines[lfoIndex].stageShape(safeData);
        ++lfoDataRevisions[lfoIndex];
    }

    lfoData[lfoIndex] = std::move(safeData);
    return lfoDataRevisions[lfoIndex];
}

bool LfoManager::setLfoDataIfRevisionMatches(
    int index,
    const LfoData& newData,
    std::uint64_t expectedRevision,
    std::uint64_t& resultingRevision)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(lfoData.size())))
    {
        jassertfalse;
        return false;
    }

    auto safeData = newData;
    safeData.sanitise();

    const auto lfoIndex = static_cast<size_t>(index);
    if (const auto* parameter = lfoParameters[lfoIndex].smoothness)
    {
        const float smoothness = parameter->load(std::memory_order_relaxed);
        if (std::isfinite(smoothness))
            safeData.smoothness = juce::jlimit(0.0f, 1.0f, smoothness);
    }

    const juce::ScopedLock sl(dataAccessLock);
    if (lfoDataRevisions[lfoIndex] != expectedRevision)
        return false;

    const bool shapeChanged = lfoData[lfoIndex].points != safeData.points
                              || lfoData[lfoIndex].curvatures != safeData.curvatures;
    if (shapeChanged)
    {
        lfoEngines[lfoIndex].stageShape(safeData);
        ++lfoDataRevisions[lfoIndex];
    }

    lfoData[lfoIndex] = std::move(safeData);
    resultingRevision = lfoDataRevisions[lfoIndex];
    return true;
}

bool LfoManager::replaceLfoDataAndRoutings(
    const std::array<LfoData, 4>& newLfoData,
    juce::Array<ModulationRouting> newRoutings)
{
    if (newRoutings.size() > maximumModulationRoutings)
        return false;

    auto safeLfoData = newLfoData;
    for (size_t i = 0; i < safeLfoData.size(); ++i)
    {
        safeLfoData[i].sanitise();
        if (const auto* parameter = lfoParameters[i].smoothness)
        {
            const float smoothness = parameter->load(
                std::memory_order_relaxed);
            if (std::isfinite(smoothness))
                safeLfoData[i].smoothness = juce::jlimit(
                    0.0f, 1.0f, smoothness);
        }
    }

    {
        const juce::ScopedLock sl(dataAccessLock);
        for (size_t i = 0; i < safeLfoData.size(); ++i)
        {
            const bool shapeChanged = lfoData[i].points != safeLfoData[i].points
                                      || lfoData[i].curvatures != safeLfoData[i].curvatures;
            if (shapeChanged)
                lfoEngines[i].stageShape(safeLfoData[i]);

            lfoData[i] = std::move(safeLfoData[i]);
            // Even an identical shape belongs to a new preset/host-state
            // identity. Invalidate every editor context atomically with the
            // replacement so a delayed menu cannot write the previous state
            // back over it.
            ++lfoDataRevisions[i];
        }

        modulationRoutings = std::move(newRoutings);
        // A complete state replacement is a new routing identity even when
        // every serialised field is byte-for-byte identical.
        advanceModulationRoutingRevisionLocked();
    }

    return true;
}
