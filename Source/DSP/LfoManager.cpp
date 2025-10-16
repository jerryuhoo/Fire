/*
  ==============================================================================

    LfoManager.cpp
    Created: 10 Sep 2025 11:07:34pm
    Author:  Yifeng Yu
 
    REFACTORED to act as the central modulation controller.

  ==============================================================================
*/

#include "LfoManager.h"
#include "../Utility/AudioHelpers.h"

LfoManager::LfoManager(juce::AudioProcessorValueTreeState& apvts) : treeState(apvts)
{
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
        shapeUpdateFlags[i] = true;
    }
}

void LfoManager::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto& engine : lfoEngines)
    {
        engine.prepare(spec);
    }
    // Ensure the internal buffer is ready
    lfoOutputBuffer.setSize(4, spec.maximumBlockSize);
}

void LfoManager::reset()
{
    for (auto& engine : lfoEngines)
    {
        engine.reset();
    }
    wasPlaying = false;
    modulatedValues.clear();
}

bool LfoManager::isModulationActive() const
{
    // Iterate through all modulation routings.
    for (const auto& routing : modulationRoutings)
    {
        // If we find any routing with a valid target parameter,
        // it means modulation is active.
        if (routing.targetParameterID.isNotEmpty())
        {
            return true;
        }
    }

    // If we get through the whole loop without finding an active routing,
    // then no modulation is active.
    return false;
}

// =============================================================================
// Main Processing Logic
// =============================================================================

void LfoManager::processBlock(juce::AudioBuffer<float>& outputBuffer, float sampleRate, juce::AudioPlayHead* playHead, int numSamples)
{
    const juce::ScopedLock sl(dataAccessLock);
    // 1. Generate all raw LFO signals for the current block.
    // This fills the internal 'lfoOutputBuffer'.
    generateLfoOutput(sampleRate, playHead, numSamples);

    // 2. Copy the generated LFO signals to the output buffer.
    jassert(outputBuffer.getNumSamples() == lfoOutputBuffer.getNumSamples());
    jassert(outputBuffer.getNumChannels() >= lfoOutputBuffer.getNumChannels());

    for (int channel = 0; channel < lfoOutputBuffer.getNumChannels(); ++channel)
    {
        outputBuffer.copyFrom(channel, 0, lfoOutputBuffer, channel, 0, numSamples);
    }

    // 3. Clear the map of calculated values from the previous block.
    // We now store normalized values.
    modulatedValues.clear();

    // 4. Iterate through all modulation routings to calculate final parameter values.
    for (const auto& routing : modulationRoutings)
    {
        // Skip invalid or unassigned routings
        if (routing.isBypassed || routing.targetParameterID.isEmpty())
            continue;

        // Use the first sample of the LFO output as the representative value for the whole block.
        float lfoValue = lfoOutputBuffer.getSample(routing.sourceLfoIndex, 0);

        // Get the RangedAudioParameter for conversions
        auto* parameter = treeState.getParameter(routing.targetParameterID);
        if (parameter == nullptr)
            continue;

        // Get the parameter's original NORMALIZED value (from the GUI knob)
        const float normalizedBaseValue = parameter->getValue();

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
        if (modulatedValues.find(routing.targetParameterID) == modulatedValues.end())
        {
            modulatedValues[routing.targetParameterID] = normalizedBaseValue;
        }

        // Add the normalized modulation amount. This allows multiple LFOs to target the same parameter.
        modulatedValues[routing.targetParameterID] += normalizedModulationAmount;
    }

    // 5. Final pass: clamp all calculated NORMALIZED values to the valid [0, 1] range.
    for (auto const& [paramID, val] : modulatedValues)
    {
        modulatedValues[paramID] = juce::jlimit(0.0f, 1.0f, val);
    }
}

float LfoManager::getModulatedValue(const juce::String& parameterID) const
{
    // Check if the parameter ID exists in our map of modulated normalized values.
    auto it = modulatedValues.find(parameterID);

    if (it != modulatedValues.end())
    {
        // Convert normalized value to real value ---
        auto* parameter = treeState.getParameter(parameterID);
        if (parameter)
        {
            // If found, convert the final normalized value back to the parameter's real value.
            return parameter->convertFrom0to1(it->second);
        }
    }

    // If not found, it means the parameter is not being modulated.
    // Return its original value directly from the APVTS.
    // NOTE: It's safer to get the parameter and ask for its real value.
    if (auto* param = treeState.getRawParameterValue(parameterID))
        return *param;

    jassertfalse; // Parameter not found
    return 0.0f;
}

// =============================================================================
// Private Helper Functions
// =============================================================================

void LfoManager::generateLfoOutput(double sampleRate, juce::AudioPlayHead* playHead, int numSamples)
{
    // This is the original 'process' function, now repurposed as a private helper.
    // Its sole responsibility is to generate the raw LFO signals.

    // 1. Get Transport State from Host
    juce::Optional<juce::AudioPlayHead::PositionInfo> positionInfo;
    isPlaying = false;
    double currentBpm = 120.0;

    if (playHead)
    {
        positionInfo = playHead->getPosition();
        if (positionInfo)
        {
            isPlaying = positionInfo->getIsPlaying();
            if (auto bpm = positionInfo->getBpm())
                currentBpm = *bpm;
        }
    }

    wasPlaying = isPlaying;

    // 3. Process each LFO
    lfoOutputBuffer.setSize(4, numSamples, false, false, true); // Ensure buffer is correct size
    lfoOutputBuffer.clear();

    for (int i = 0; i < 4; ++i)
    {
        // Shape update logic (unchanged)
        bool needsUpdate = true;
        if (shapeUpdateFlags[i].compare_exchange_strong(needsUpdate, false))
        {
            lfoEngines[i].updateShape(lfoData[i]);
        }

        auto* syncParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, i));
        const bool isInSyncMode = syncParam != nullptr && syncParam->load() > 0.5f;
        float phaseDelta = 0.0f;

        // 1. Calculate phaseDelta for advancing phase within the block (or for free-running when stopped)
        if (isInSyncMode)
        {
            auto* rateSyncParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, i));
            const int rateIndex = static_cast<int>(rateSyncParam->load());
            const float beatMultiplier = mapRateSyncIndexToBeatMultiplier(rateIndex);
            const float beatsPerCycle = beatMultiplier * 4.0f;
            if (beatsPerCycle > 0.0 && currentBpm > 0.0)
            {
                const float samplesPerCycle = (beatsPerCycle / currentBpm) * 60.0f * (float) sampleRate;
                if (samplesPerCycle > 0)
                    phaseDelta = 1.0f / samplesPerCycle;
            }
        }
        else // Free (Hz) mode
        {
            auto* rateHzParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, i));
            const float freqInHz = rateHzParam->load();
            if (sampleRate > 0)
                phaseDelta = freqInHz / (float) sampleRate;
        }

        // Get the phase offset value from the new parameter we created
        auto* phaseOffsetParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_PHASE_ID, i));
        const float phaseOffset = (phaseOffsetParam != nullptr) ? phaseOffsetParam->load() : 0.0f;

        // 2. If playing, calculate and set the absolute start phase for the block.
        //    Otherwise, the LFO continues from its last phase (free-running).
        if (isPlaying && positionInfo)
        {
            if (isInSyncMode)
            {
                if (auto ppq = positionInfo->getPpqPosition())
                {
                    const double ppqAtStartOfBlock = *ppq;
                    auto* rateSyncParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, i));
                    const int rateIndex = static_cast<int>(rateSyncParam->load());
                    const float beatMultiplier = mapRateSyncIndexToBeatMultiplier(rateIndex);
                    const float cycleLengthInBeats = beatMultiplier * 4.0f;

                    if (cycleLengthInBeats > 0.0f)
                    {
                        // Calculate phase from timeline
                        float startPhase = std::fmod((float) ppqAtStartOfBlock, cycleLengthInBeats) / cycleLengthInBeats;

                        // Apply the user-defined phase offset and wrap around 1.0
                        startPhase = std::fmod(startPhase + phaseOffset, 1.0f);

                        lfoEngines[i].setPhase(startPhase); // Set the final, offset phase
                    }
                }
            }
            else // Hz mode
            {
                if (auto timeSec = positionInfo->getTimeInSeconds())
                {
                    const double timeAtStartOfBlock = *timeSec;
                    auto* rateHzParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, i));
                    const float freqInHz = rateHzParam->load();

                    // Calculate phase from timeline
                    float startPhase = std::fmod((float) timeAtStartOfBlock * freqInHz, 1.0f);

                    // Apply the user-defined phase offset and wrap around 1.0
                    startPhase = std::fmod(startPhase + phaseOffset, 1.0f);

                    lfoEngines[i].setPhase(startPhase); // Set the final, offset phase
                }
            }
        }

        // 3. Set the delta and generate samples for the block (unchanged)
        lfoEngines[i].setPhaseDelta(phaseDelta);

        auto* writer = lfoOutputBuffer.getWritePointer(i);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            writer[sample] = lfoEngines[i].process();
        }
    }
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
    const juce::ScopedLock sl(dataAccessLock);
    if (lfoIndex < 0)
    {
        for (int i = 0; i < 4; ++i)
            shapeUpdateFlags[i] = true;
    }
    else if (juce::isPositiveAndBelow(lfoIndex, 4))
    {
        shapeUpdateFlags[lfoIndex] = true;
    }
}

// =============================================================================
// Accessors for UI
// =============================================================================

float LfoManager::getLfoPhase(int lfoIndex) const
{
    if (juce::isPositiveAndBelow(lfoIndex, (int) lfoEngines.size()))
    {
        return lfoEngines[lfoIndex].getPhase();
    }
    jassertfalse; // Invalid LFO index requested
    return 0.0f;
}

const juce::StringArray& LfoManager::getLfoRateSyncDivisions() const
{
    return lfoRateSyncDivisions;
}

float LfoManager::getLfoOutput(int lfoIndex) const
{
    if (juce::isPositiveAndBelow(lfoIndex, (int) lfoEngines.size()))
    {
        return lfoEngines[lfoIndex].getLastOutput();
    }

    jassertfalse; // Invalid LFO index requested
    return 0.0f;
}

void LfoManager::assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID)
{
    const juce::ScopedLock sl(dataAccessLock);
    // 1. First, check if the target parameter is already being modulated.
    //    If so, just update its LFO source.
    for (auto& routing : modulationRoutings)
    {
        if (routing.targetParameterID == targetParameterID)
        {
            routing.sourceLfoIndex = sourceLfoIndex;
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
            routing.depth *= -1.0f;
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
    const juce::ScopedLock sl(dataAccessLock);
    if (juce::isPositiveAndBelow(index, (int) lfoData.size()))
    {
        lfoData[index] = newData;
        shapeUpdateFlags[index].store(true);
    }
}

void LfoManager::clearAllLfoData()
{
    const juce::ScopedLock sl(dataAccessLock);
    for (auto& lfo : lfoData)
    {
        lfo = LfoData(); // Reset to default state
    }
    // Flag all shapes for update on the audio thread
    onLfoShapeChanged(-1);
}