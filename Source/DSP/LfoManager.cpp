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
#include "../Utility/ChannelModeParameters.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
bool haveSameModulationRoutingState(const ModulationRouting& lhs,
                                    const ModulationRouting& rhs) noexcept
{
    return lhs.hasSameState(rhs);
}
} // namespace

LfoManager::LfoManager(juce::AudioProcessorValueTreeState& apvts) : treeState(apvts)
{
    const auto indexedParameterId = [](const char* baseId, int index)
    {
        return juce::String(baseId) + juce::String(index + 1);
    };

    // Stable slots retain their shape and timing identity across deletions.
    lfoData.resize(fire::lfo_bank::capacity);
    for (auto& generation : slotGenerations) generation.store(0u, std::memory_order_relaxed);
    for (auto& phase : visualPhases) phase.store(-1.0f, std::memory_order_relaxed);
    // Define the string representations for synced LFO rates
    lfoRateSyncDivisions = {
        "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T", "1/2", "1 Bar", "2 Bars", "4 Bars"
    };

    for (size_t index = 0; index < auxiliaryParameters.size(); ++index)
        auxiliaryParameters[index] = treeState.getRawParameterValue(fire::mod_sources::ids[index]);
    for (int envelope = 0; envelope < fire::envelope_params::count; ++envelope)
        for (int field = 0; field < fire::envelope_params::fieldCount; ++field)
            envelopeParameters[static_cast<size_t>(envelope)][static_cast<size_t>(field)] =
                treeState.getRawParameterValue(fire::envelope_params::parameterID(envelope, field));
    envelopeBandCountParameter = treeState.getRawParameterValue(NUM_BANDS_ID);
    for (size_t index = 0; index < envelopeCrossoverParameters.size(); ++index)
        envelopeCrossoverParameters[index] = treeState.getRawParameterValue(juce::String(FREQ_ID) + juce::String(index + 1));
    for (int i = 0; i < fire::lfo_bank::defaultCount; ++i)
        modulationRoutings.add({});

    for (int i = 0; i < fire::lfo_bank::capacity; ++i)
    {
        const auto lfoIndex = static_cast<size_t>(i);
        const auto smoothnessId = indexedParameterId(LFO_SMOOTH_ID, i);
        smoothnessParameterIDs[lfoIndex] = smoothnessId;

        lfoParameters[lfoIndex] = {
            treeState.getRawParameterValue(indexedParameterId(LFO_SYNC_MODE_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_RATE_SYNC_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_RATE_HZ_ID, i)),
            treeState.getRawParameterValue(indexedParameterId(LFO_PHASE_ID, i)),
            treeState.getRawParameterValue(smoothnessId),
            treeState.getRawParameterValue(fire::lfo_bank::presentParameterID(i))
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
    const auto safeMaximumBlockSize = static_cast<int>(std::min<uint64_t>(
        spec.maximumBlockSize, static_cast<uint64_t>(std::numeric_limits<int>::max())));
    lfoOutputBuffer.setSize(fire::mod_sources::signalCount, juce::jmax(1, safeMaximumBlockSize), false, true, false);
    aggregateTransitionSamples = juce::jmax(1, juce::roundToInt(preparedSampleRate * .020));
    aggregateTransitions = {};
    auxiliary.prepare(preparedSampleRate);
    envelopeDetectors.prepare(preparedSampleRate);
    for (auto& level : auxiliaryLevels) level.store(0, std::memory_order_relaxed);
    publishVisualState(nullptr);
}

void LfoManager::reset()
{
    auxiliary.reset();
    envelopeDetectors.reset();
    aggregateTransitions = {};
    for (auto& level : auxiliaryLevels) level.store(0, std::memory_order_relaxed);
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
    previouslyPresent.fill(false);
    appliedSlotGenerations.fill(0u);
    isPlaying.store(false, std::memory_order_relaxed);
    modulatedValueCount = 0;
    lfoOutputBuffer.clear();
    publishVisualState(nullptr);
}

void LfoManager::publishVisualState(const AudioThreadParameterSnapshot* parameters) noexcept
{
    // One audio/lifecycle writer, atomic payload, no reader locks or clock
    // calls. Release payload stores let the reader's acquire fence observe
    // the odd version whenever it sees any sample from this new publication.
    const auto previous = visualSequence.fetch_add(1u, std::memory_order_acq_rel);
    jassert((previous & 1u) == 0u);
    juce::ignoreUnused(previous);
    for (size_t index = 0; index < visualPhases.size(); ++index)
    {
        const auto phase = parameters != nullptr && parameters->lfos[index].present
                               ? lfoEngines[index].getLastRenderedPhaseForAudioThread() : -1.0f;
        visualPhases[index].store(phase, std::memory_order_release);
    }
    visualSequence.fetch_add(1u, std::memory_order_release);
}

LfoManager::VisualState LfoManager::getLfoVisualState(int index) const noexcept
{
    if (! fire::lfo_bank::validIndex(index)) return {};
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const auto before = visualSequence.load(std::memory_order_acquire);
        if ((before & 1u) != 0u) continue;
        const auto phase = visualPhases[static_cast<size_t>(index)].load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        const auto after = visualSequence.load(std::memory_order_relaxed);
        if (before == after)
            return { std::isfinite(phase) && phase >= 0.0f && phase <= 1.0f ? phase : -1.0f, after };
    }
    // Sequence zero means no coherent read, not an inactive source. The UI
    // retains its previous phase until a complete publication is available.
    return {};
}

bool LfoManager::isModulationActive() const
{
    // This is called from processBlock in older processor code, so it must never wait for the UI.
    const juce::ScopedTryLock lock(dataAccessLock);
    if (lock.isLocked())
    {
        for (const auto& routing : modulationRoutings)
            if (routing.targetParameterID.isNotEmpty() && isModulationSourcePresent(routing.sourceLfoIndex))
                return true;

        return false;
    }

    return hasPublishedRouting.load(std::memory_order_relaxed);
}

bool LfoManager::isLfoPresent(int index) const noexcept
{
    if (! fire::lfo_bank::validIndex(index)) return false;
    if (const auto* present = lfoParameters[static_cast<size_t>(index)].present)
    {
        const auto value = present->load(std::memory_order_relaxed);
        if (std::isfinite(value)) return value > 0.5f;
    }
    // Standalone legacy users can still supply only the original timing IDs.
    return fire::lfo_bank::defaultPresent(index);
}

void LfoManager::resetLfoSlot(int index)
{
    if (! fire::lfo_bank::validIndex(index)) return;
    const juce::ScopedLock lock(dataAccessLock);
    const auto slot = static_cast<size_t>(index);
    for (int route = modulationRoutings.size(); --route >= 0;)
        if (modulationRoutings.getReference(route).sourceLfoIndex == index)
            modulationRoutings.remove(route);
    lfoData[slot] = LfoData {};
    lfoEngines[slot].stageShape(lfoData[slot]);
    ++lfoDataRevisions[slot];
    slotGenerations[slot].fetch_add(1u, std::memory_order_relaxed);
    // Slot deletion/reuse invalidates popups even when no routes existed.
    advanceModulationRoutingRevisionLocked();
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
        destination.present = loadParameter(source.present,
            fire::lfo_bank::defaultPresent(static_cast<int>(index)) ? 1.0f : 0.0f) > 0.5f;
        destination.generation = slotGenerations[index].load(std::memory_order_relaxed);
    }

    for (size_t index = 0; index < auxiliaryParameters.size(); ++index)
        snapshot.auxiliary[index] = loadParameter(auxiliaryParameters[index], fire::mod_sources::defaults[index]);
    for (size_t index = 0; index < snapshot.envelopes.size(); ++index)
    {
        namespace ep = fire::envelope_params;
        const auto load = [&](int field) { return loadParameter(envelopeParameters[index][static_cast<size_t>(field)], ep::defaults[static_cast<size_t>(field)]); };
        auto& destination = snapshot.envelopes[index];
        destination.source = juce::roundToInt(juce::jlimit(0.0f, 5.0f, load(ep::source)));
        destination.highPassEnabled = load(ep::highPassEnabled) > .5f;
        destination.lowPassEnabled = load(ep::lowPassEnabled) > .5f;
        destination.highPassFrequency = load(ep::highPassFrequency);
        destination.lowPassFrequency = load(ep::lowPassFrequency);
        destination.attack = index == 0 ? snapshot.auxiliary[0] : load(ep::attack);
        destination.release = index == 0 ? snapshot.auxiliary[1] : load(ep::release);
        destination.sensitivity = index == 0 ? snapshot.auxiliary[2] : load(ep::sensitivity);
    }
    snapshot.envelopeBandCount = juce::roundToInt(juce::jlimit(1.0f, 4.0f, loadParameter(envelopeBandCountParameter, 1)));
    for (size_t index = 0; index < snapshot.envelopeCrossovers.size(); ++index)
        snapshot.envelopeCrossovers[index] = loadParameter(envelopeCrossoverParameters[index], snapshot.envelopeCrossovers[index]);
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
        if (parameterSnapshot.lfos[i].present)
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

    auxiliary.setParameters(parameterSnapshot.auxiliary);
    auto envelopeConfiguration = parameterSnapshot.envelopes;
    // Preserve the public pre-bank snapshot contract and ENV 1 arithmetic.
    // These three legacy parameter IDs remain its timing/sensitivity authority.
    envelopeConfiguration[0].attack = parameterSnapshot.auxiliary[0];
    envelopeConfiguration[0].release = parameterSnapshot.auxiliary[1];
    envelopeConfiguration[0].sensitivity = parameterSnapshot.auxiliary[2];
    envelopeDetectors.setParameters(envelopeConfiguration, parameterSnapshot.envelopeBandCount,
                                   parameterSnapshot.envelopeCrossovers);
    std::array<float, fire::mod_sources::signalCount> firstLfoValues {};
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

        for (int sample = 0; sample < samplesInRange; ++sample)
        {
            const auto inputSample = sampleOffset + sample;
            const auto read = [](const juce::AudioBuffer<float>* input, juce::int64 position)
            {
                fire::dsp::EnvelopeDetectors::StereoFrame frame;
                if (input == nullptr || input->getNumChannels() == 0 || position < 0 || position >= input->getNumSamples()) return frame;
                frame.stereo = input->getNumChannels() > 1;
                frame.left = input->getSample(0, static_cast<int>(position));
                frame.right = frame.stereo ? input->getSample(1, static_cast<int>(position)) : frame.left;
                return frame;
            };
            const auto detectors = envelopeDetectors.next(read(audioInput, inputSample),
                read(externalAudioInput, static_cast<juce::int64>(externalAudioInputOffset) + inputSample));
            const auto& envelope1 = detectors.filtered[0];
            const auto values = auxiliary.next(envelope1.left, envelope1.right, envelope1.stereo);
            for (size_t index = 0; index < values.size(); ++index)
            {
                lfoOutputBuffer.setSample(fire::mod_sources::envelope + static_cast<int>(index), sample, values[index]);
                if (sample + 1 == samplesInRange) auxiliaryLevels[index].store(values[index], std::memory_order_relaxed);
            }
            for (int envelope = 1; envelope < fire::envelope_params::count; ++envelope)
            {
                const auto source = fire::mod_sources::envelopeSource(envelope);
                const auto value = detectors.levels[static_cast<size_t>(envelope)];
                lfoOutputBuffer.setSample(source, sample, value);
                if (sample + 1 == samplesInRange)
                    auxiliaryLevels[static_cast<size_t>(source - fire::mod_sources::envelope)].store(value, std::memory_order_relaxed);
            }
            if (envelopeAuditionOutput != nullptr && inputSample < envelopeAuditionOutput->getNumSamples())
            {
                const auto frame = juce::isPositiveAndBelow(auditionEnvelopeIndex, fire::envelope_params::count)
                    ? detectors.filtered[static_cast<size_t>(auditionEnvelopeIndex)]
                    : fire::dsp::EnvelopeDetectors::StereoFrame{};
                for (int channel = 0; channel < envelopeAuditionOutput->getNumChannels(); ++channel)
                    envelopeAuditionOutput->setSample(channel, inputSample, channel == 0 ? frame.left : frame.right);
            }
        }

        renderAggregateSignals(samplesInRange, parameterSnapshot);

        if (sampleOffset == 0)
        {
            for (int channel = 0; channel < fire::mod_sources::sourceCount; ++channel)
                firstLfoValues[static_cast<size_t>(channel)] = lfoOutputBuffer.getSample(channel, 0);
            for (size_t index = 0; index < runtimeActiveTargetCount; ++index)
            {
                const auto slot = static_cast<size_t>(runtimeActiveTargetSlots[index]);
                if (! runtimeTargets[slot].aggregate) continue;
                const auto channel = fire::mod_sources::firstAggregateSignal + static_cast<int>(slot);
                firstLfoValues[static_cast<size_t>(channel)] = lfoOutputBuffer.getSample(channel, 0);
            }
        }

        for (int channel = 0; channel < juce::jmin(channelsToCopy, fire::mod_sources::sourceCount); ++channel)
            outputBuffer.copyFrom(channel, sampleOffset, lfoOutputBuffer, channel, 0, samplesInRange);
        for (size_t index = 0; index < runtimeActiveTargetCount; ++index)
        {
            const auto slot = static_cast<size_t>(runtimeActiveTargetSlots[index]);
            const int channel = fire::mod_sources::firstAggregateSignal + static_cast<int>(slot);
            if (runtimeTargets[slot].aggregate && channel < channelsToCopy)
                outputBuffer.copyFrom(channel, sampleOffset, lfoOutputBuffer, channel, 0, samplesInRange);
        }

        sampleOffset += samplesInRange;
    }

    publishVisualState(&parameterSnapshot);

    // Standalone callers may supply only the physical source channels.
    // The first-sample representative still uses the internal aggregate bank.
    for (size_t index = 0; index < runtimeActiveTargetCount; ++index)
    {
        const auto slot = static_cast<size_t>(runtimeActiveTargetSlots[index]);
        const auto& target = runtimeTargets[slot];
        if (target.parameter == nullptr || target.firstRoute < 0) continue;
        const auto& routing = runtimeRoutings[static_cast<size_t>(target.firstRoute)];
        const float rawBaseValue = readLiveRoutingBaseValues ? routing.parameter->getValue()
                                                            : routing.normalisedBaseValue;
        const float base = std::isfinite(rawBaseValue) ? juce::jlimit(0.0f, 1.0f, rawBaseValue)
                                                       : routing.parameter->getDefaultValue();
        float amount = 0.0f;
        if (target.aggregate)
            amount = firstLfoValues[static_cast<size_t>(fire::mod_sources::firstAggregateSignal) + slot];
        else
        {
            if (fire::lfo_bank::validIndex(routing.sourceLfoIndex)
                && ! parameterSnapshot.lfos[static_cast<size_t>(routing.sourceLfoIndex)].present) continue;
            // Preserve the original single-source arithmetic and its order.
            float value = firstLfoValues[static_cast<size_t>(routing.sourceLfoIndex)];
            if (! std::isfinite(value)) continue;
            float depth = routing.depth;
            if (routing.isBipolar) { value = value * 2.0f - 1.0f; depth *= .5f; }
            amount = value * depth;
        }
        const auto value = base + amount;
        modulatedValues[modulatedValueCount++] = { target.parameter,
            std::isfinite(value) ? juce::jlimit(0.0f, 1.0f, value) : 0.0f };
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
    const auto& targets = runtimeStateCaptureInProgress ? candidateRuntimeTargets : runtimeTargets;
    const auto& routings = runtimeStateCaptureInProgress ? candidateRuntimeRoutings : runtimeRoutings;
    const auto& activeSlots = runtimeStateCaptureInProgress ? candidateActiveTargetSlots : runtimeActiveTargetSlots;
    const auto count = runtimeStateCaptureInProgress ? candidateActiveTargetCount : runtimeActiveTargetCount;
    for (size_t index = 0; index < count; ++index)
    {
        const auto slot = static_cast<size_t>(activeSlots[index]);
        const auto& target = targets[slot];
        if (target.parameter != parameter || target.firstRoute < 0) continue;
        if (target.aggregate)
            result = { fire::mod_sources::firstAggregateSignal + static_cast<int>(slot), 1.0f, false, true };
        else
        {
            const auto& routing = routings[static_cast<size_t>(target.firstRoute)];
            result = { routing.sourceLfoIndex, routing.depth, routing.isBipolar, false };
        }
        return true;
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
        candidateRuntimeRoutingCount,
        candidateRuntimeTargets, candidateActiveTargetSlots, candidateActiveTargetCount);
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
        commitRuntimeTargets();
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

ModulationRouting LfoManager::RuntimeRouting::mapping() const noexcept
{
    // Empty target string: the audio recipe owns only scalar configuration;
    // this temporary never acquires or releases UI-owned text storage.
    ModulationRouting result;
    result.sourceLfoIndex = sourceLfoIndex;
    result.depth = depth;
    result.isBipolar = isBipolar;
    result.customRange = customRange;
    result.rangeStart = rangeStart;
    result.rangeEnd = rangeEnd;
    result.curve = curve;
    result.depthMacro = depthMacro;
    return result;
}

bool LfoManager::RuntimeRouting::sameMapping(const RuntimeRouting& other) const noexcept
{
    return sourceLfoIndex == other.sourceLfoIndex && depth == other.depth
        && isBipolar == other.isBipolar && customRange == other.customRange
        && rangeStart == other.rangeStart && rangeEnd == other.rangeEnd
        && curve == other.curve && depthMacro == other.depthMacro;
}

void LfoManager::commitRuntimeTargets() noexcept
{
    for (size_t index = 0; index < runtimeActiveTargetCount; ++index)
    {
        const auto slot = static_cast<size_t>(runtimeActiveTargetSlots[index]);
        if (candidateRuntimeTargets[slot].parameter != runtimeTargets[slot].parameter)
            aggregateTransitions[slot] = {};
    }
    for (size_t index = 0; index < candidateActiveTargetCount; ++index)
    {
        const auto slot = static_cast<size_t>(candidateActiveTargetSlots[index]);
        const auto& target = candidateRuntimeTargets[slot];
        auto& transition = aggregateTransitions[slot];
        if (transition.parameter != target.parameter)
        {
            transition = {};
            transition.parameter = target.parameter;
        }
        if (target.parameter == nullptr) continue;
        if (transition.configuration != target.configuration && transition.initialised && target.aggregate)
        {
            transition.anchor = transition.lastOffset;
            transition.remaining = aggregateTransitionSamples;
        }
        if (! target.aggregate) transition.remaining = 0;
        transition.configuration = target.configuration;
    }
    runtimeTargets = candidateRuntimeTargets;
    runtimeActiveTargetCount = candidateActiveTargetCount;
    std::copy_n(candidateActiveTargetSlots.begin(), runtimeActiveTargetCount, runtimeActiveTargetSlots.begin());
}

void LfoManager::renderAggregateSignals(int samples, const AudioThreadParameterSnapshot& parameters) noexcept
{
    if (samples <= 0) return;
    for (size_t index = 0; index < runtimeActiveTargetCount; ++index)
    {
        const auto slot = static_cast<size_t>(runtimeActiveTargetSlots[index]);
        if (runtimeTargets[slot].aggregate)
            juce::FloatVectorOperations::clear(lfoOutputBuffer.getWritePointer(
                fire::mod_sources::firstAggregateSignal + static_cast<int>(slot)), samples);
    }

    for (size_t index = 0; index < runtimeRoutingCount; ++index)
    {
        const auto& routing = runtimeRoutings[index];
        if (routing.targetSlot < 0) continue;
        if (fire::lfo_bank::validIndex(routing.sourceLfoIndex)
            && ! parameters.lfos[static_cast<size_t>(routing.sourceLfoIndex)].present) continue;
        const auto slot = static_cast<size_t>(routing.targetSlot);
        const auto mapping = routing.mapping();
        const auto* signal = lfoOutputBuffer.getReadPointer(routing.sourceLfoIndex);
        const auto* macro = routing.depthMacro < 0 ? nullptr : lfoOutputBuffer.getReadPointer(
            fire::mod_sources::firstMacro + routing.depthMacro);
        if (! runtimeTargets[slot].aggregate)
        {
            auto& transition = aggregateTransitions[slot];
            transition.lastOffset = mapping.offset(signal[samples - 1], macro ? macro[samples - 1] : 1.0f);
            transition.initialised = true;
            continue;
        }
        auto* destination = lfoOutputBuffer.getWritePointer(
            fire::mod_sources::firstAggregateSignal + routing.targetSlot);
        for (int sample = 0; sample < samples; ++sample)
            destination[sample] += mapping.offset(signal[sample], macro ? macro[sample] : 1.0f);
    }
    for (size_t index = 0; index < runtimeActiveTargetCount; ++index)
    {
        const auto slot = static_cast<size_t>(runtimeActiveTargetSlots[index]);
        if (! runtimeTargets[slot].aggregate) continue;
        auto& transition = aggregateTransitions[slot];
        auto* signal = lfoOutputBuffer.getWritePointer(fire::mod_sources::firstAggregateSignal + static_cast<int>(slot));
        for (int sample = 0; sample < samples; ++sample)
        {
            // Sum every contributor before clipping. Cancellation therefore
            // works even when an individual contributor saturates the target.
            const auto offset = juce::jlimit(-1.0f, 1.0f, signal[sample]);
            float value = offset;
            if (transition.initialised && transition.remaining > 0)
            {
                const auto blend = 1.0f - static_cast<float>(transition.remaining)
                                            / static_cast<float>(aggregateTransitionSamples);
                value = transition.anchor + blend * (offset - transition.anchor);
                --transition.remaining;
            }
            transition.initialised = true;
            transition.lastOffset = value;
            signal[sample] = value;
        }
    }
}

bool LfoManager::captureRuntimeRoutings(
    std::array<RuntimeRouting, maximumModulationRoutings>& destination,
    size_t& destinationCount,
    std::array<RuntimeTarget, maximumModulationRoutings>& targets,
    std::array<int, maximumModulationRoutings>& activeSlots,
    size_t& activeCount) const
{
    destinationCount = 0;
    bool hasAnyRouting = false;

    for (const auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID.isEmpty() || fire::channel_mode::isParameterID(routing.targetParameterID)
            || ! isModulationSourcePresent(routing.sourceLfoIndex))
            continue;

        hasAnyRouting = true;
        if (routing.isBypassed
            || ! fire::mod_sources::validSource(routing.sourceLfoIndex)
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
            normalisedBaseValue,
            routing.customRange,
            std::isfinite(routing.rangeStart) ? juce::jlimit(-1.0f, 1.0f, routing.rangeStart) : 0.0f,
            std::isfinite(routing.rangeEnd) ? juce::jlimit(-1.0f, 1.0f, routing.rangeEnd) : 1.0f,
            juce::jlimit(0, 3, routing.curve),
            juce::jlimit(-1, fire::mod_sources::macroCount - 1, routing.depthMacro),
            -1
        };
    }

    targets = {};
    activeCount = 0;
    // Reserve every surviving target's old slot before allocating new ones;
    // adding/removing a row for another target never changes its channel ID.
    for (size_t index = 0; index < runtimeActiveTargetCount; ++index)
    {
        const auto slot = static_cast<size_t>(runtimeActiveTargetSlots[index]);
        for (size_t route = 0; route < destinationCount; ++route)
            if (runtimeTargets[slot].parameter == destination[route].parameter)
            {
                targets[slot].parameter = destination[route].parameter;
                activeSlots[activeCount++] = static_cast<int>(slot);
                break;
            }
    }
    for (size_t route = 0; route < destinationCount; ++route)
    {
        auto& routing = destination[route];
        size_t index = 0;
        while (index < activeCount && targets[static_cast<size_t>(activeSlots[index])].parameter != routing.parameter) ++index;
        size_t slot = index < activeCount ? static_cast<size_t>(activeSlots[index]) : 0;
        if (index == activeCount)
        {
            while (slot < targets.size() && targets[slot].parameter != nullptr) ++slot;
            jassert(slot < targets.size());
            if (slot == targets.size()) continue;
            activeSlots[activeCount++] = static_cast<int>(slot);
        }
        auto& target = targets[slot];
        target.parameter = routing.parameter;
        routing.targetSlot = static_cast<int>(slot);
        if (target.firstRoute < 0) target.firstRoute = static_cast<int>(route);
        ++target.routeCount;
        target.aggregate = target.routeCount > 1 || target.aggregate
            || routing.customRange || routing.curve != 0 || routing.depthMacro >= 0;
    }
    for (size_t index = 0; index < activeCount; ++index)
    {
        const auto slot = static_cast<size_t>(activeSlots[index]);
        auto& target = targets[slot];
        if (target.parameter == nullptr) continue;
        const auto& previous = runtimeTargets[slot];
        bool equal = target.parameter == previous.parameter && target.routeCount == previous.routeCount;
        size_t oldRoute = 0;
        for (size_t route = 0; equal && route < destinationCount; ++route)
        {
            if (destination[route].targetSlot != static_cast<int>(slot)) continue;
            while (oldRoute < runtimeRoutingCount && runtimeRoutings[oldRoute].targetSlot != static_cast<int>(slot)) ++oldRoute;
            equal = oldRoute < runtimeRoutingCount && destination[route].sameMapping(runtimeRoutings[oldRoute++]);
        }
        target.configuration = equal ? previous.configuration : previous.configuration + 1;
    }
    return hasAnyRouting;
}

bool LfoManager::refreshRuntimeStateIfAvailable()
{
    const juce::ScopedTryLock lock(dataAccessLock);
    if (! lock.isLocked())
        return false;

    const bool hasAnyRouting = captureRuntimeRoutings(candidateRuntimeRoutings,
                                                       candidateRuntimeRoutingCount, candidateRuntimeTargets,
                                                       candidateActiveTargetSlots, candidateActiveTargetCount);
    runtimeRoutings = candidateRuntimeRoutings;
    runtimeRoutingCount = candidateRuntimeRoutingCount;
    commitRuntimeTargets();
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
        if (routing.targetParameterID.isNotEmpty() && isModulationSourcePresent(routing.sourceLfoIndex))
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

    for (int i = 0; i < fire::lfo_bank::capacity; ++i)
    {
        const auto lfoIndex = static_cast<size_t>(i);
        const auto& parameters = parameterSnapshot.lfos[lfoIndex];
        const bool identityChanged = parameters.generation != appliedSlotGenerations[lfoIndex];
        if (identityChanged || parameters.present != previouslyPresent[lfoIndex])
        {
            // Delete/add/rebind can complete between callbacks, leaving the
            // target's source/depth/polarity recipe unchanged. Retain the
            // staged shape crossfade in that case, and restart canonical phase
            // with the existing correction bridge instead of hard-cutting the
            // still-audible source to its new default output.
            if (identityChanged && parameters.present && previouslyPresent[lfoIndex])
                lfoEngines[lfoIndex].setPhaseWithCorrection(0.0f);
            else
                lfoEngines[lfoIndex].reset();
            appliedPhaseOffsets[lfoIndex] = 0.0f;
            phaseOffsetInitialised[lfoIndex] = false;
            timingSignatureInitialised[lfoIndex] = false;
            usedAbsoluteTimelineLastBlock[lfoIndex] = false;
            appliedSlotGenerations[lfoIndex] = parameters.generation;
            previouslyPresent[lfoIndex] = parameters.present;
            if (! parameters.present)
                lfoOutputBuffer.clear(i, 0, lfoOutputBuffer.getNumSamples());
        }
        if (! parameters.present) continue;
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
    else if (fire::lfo_bank::validIndex(lfoIndex))
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
    if (fire::channel_mode::isParameterID(safeReplacement.targetParameterID)) return {};
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
        && (! fire::mod_sources::validSource(sourceLfoIndex)
            || treeState.getParameter(targetParameterID) == nullptr))
        return {};

    const int safeSourceIndex = juce::jlimit(0, fire::mod_sources::sourceCount - 1, sourceLfoIndex);
    if (fire::mod_sources::isParameterID(targetParameterID)
        || fire::channel_mode::isParameterID(targetParameterID)) return {};
    const juce::ScopedLock lock(dataAccessLock);
    if (targetParameterID.isNotEmpty() && ! isModulationSourcePresent(safeSourceIndex)) return {};
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

    if (currentRouting.sourceLfoIndex != safeSourceIndex)
    {
        currentRouting.sourceLfoIndex = safeSourceIndex;
        if (fire::mod_sources::isAuxiliary(safeSourceIndex)) currentRouting.isBipolar = false;
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
    if (onStateEdited) onStateEdited();
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
    if (fire::mod_sources::isAuxiliary(lfoIndex))
        return auxiliaryLevels[static_cast<size_t>(lfoIndex - fire::mod_sources::envelope)].load(std::memory_order_relaxed);
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
    if (! fire::mod_sources::validSource(sourceLfoIndex) || targetParameterID.isEmpty()
        || fire::mod_sources::isParameterID(targetParameterID)
        || fire::channel_mode::isParameterID(targetParameterID))
    {
        jassertfalse;
        return AssignmentResult::invalidRequest;
    }

    const juce::ScopedLock sl(dataAccessLock);
    if (! isModulationSourcePresent(sourceLfoIndex)) return AssignmentResult::invalidRequest;
    for (const auto& routing : modulationRoutings)
        if (routing.targetParameterID == targetParameterID && routing.sourceLfoIndex == sourceLfoIndex)
            return AssignmentResult::unchanged;

    // 2. If the target is not modulated, find the first available empty slot.
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID.isEmpty())
        {
            routing = {};
            routing.sourceLfoIndex = sourceLfoIndex;
            if (fire::mod_sources::isAuxiliary(sourceLfoIndex)) routing.isBipolar = false;
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
    if (fire::mod_sources::isAuxiliary(sourceLfoIndex)) newRouting.isBipolar = false;
    newRouting.targetParameterID = targetParameterID;
    newRouting.depth = 0.5f; // Set a sensible default depth.
    advanceModulationRoutingRevisionLocked();
    return AssignmentResult::changed;
}

bool LfoManager::clearModulationForTarget(const juce::String& targetParameterID)
{
    if (targetParameterID.isEmpty()) return false;
    const juce::ScopedLock lock(dataAccessLock);
    bool changed = false;
    for (auto& routing : modulationRoutings)
        if (routing.targetParameterID == targetParameterID) { routing = {}; changed = true; }
    if (changed) advanceModulationRoutingRevisionLocked();
    return changed;
}

bool LfoManager::invertModulationDepth(const juce::String& targetParameterID)
{
    if (targetParameterID.isEmpty()) return false;
    const juce::ScopedLock lock(dataAccessLock);
    bool changed = false;
    for (auto& routing : modulationRoutings)
        if (routing.targetParameterID == targetParameterID)
        {
            const auto depth = std::isfinite(routing.depth) ? juce::jlimit(-1.0f, 1.0f, -routing.depth) : -.5f;
            if (! juce::exactlyEqual(routing.depth, depth)) { routing.depth = depth; changed = true; }
        }
    if (changed) advanceModulationRoutingRevisionLocked();
    return changed;
}

bool LfoManager::toggleBypassForRouting(const juce::String& targetParameterID)
{
    if (targetParameterID.isEmpty()) return false;
    const juce::ScopedLock lock(dataAccessLock);
    bool found = false, anyActive = false;
    for (const auto& routing : modulationRoutings)
        if (routing.targetParameterID == targetParameterID)
        { found = true; anyActive = anyActive || ! routing.isBypassed; }
    if (! found) return false;
    // A partially enabled target bypasses all; an entirely bypassed target
    // enables all. One user action publishes one coherent routing revision.
    for (auto& routing : modulationRoutings)
        if (routing.targetParameterID == targetParameterID) routing.isBypassed = anyActive;
    advanceModulationRoutingRevisionLocked();
    return true;
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
    if (shapeChanged && onStateEdited) onStateEdited();
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
    if (shapeChanged && onStateEdited) onStateEdited();
    return true;
}

bool LfoManager::replaceLfoDataAndRoutings(
    const std::array<LfoData, fire::lfo_bank::capacity>& newLfoData,
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

bool LfoManager::replaceLfoDataAndRoutings(
    const std::array<LfoData, fire::lfo_bank::defaultCount>& newLfoData,
    juce::Array<ModulationRouting> newRoutings)
{
    std::array<LfoData, fire::lfo_bank::capacity> expanded;
    std::copy(newLfoData.begin(), newLfoData.end(), expanded.begin());
    return replaceLfoDataAndRoutings(expanded, std::move(newRoutings));
}
