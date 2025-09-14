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

// =============================================================================
// Main Processing Logic
// =============================================================================

void LfoManager::processBlock(double sampleRate, juce::AudioPlayHead* playHead, int numSamples)
{
    // 1. Generate all raw LFO signals for the current block.
    // This fills the internal 'lfoOutputBuffer'.
    generateLfoOutput(sampleRate, playHead, numSamples);

    // 2. Clear the map of calculated values from the previous block.
    // MODIFICATION: We now store normalized values.
    modulatedValues.clear();

    // 3. Iterate through all modulation routings to calculate final parameter values.
    for (const auto& routing : modulationRoutings)
    {
        // Skip invalid or unassigned routings
        if (routing.targetParameterID.isEmpty())
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

        // --- MODIFICATION END ---
    }

    // 4. Final pass: clamp all calculated NORMALIZED values to the valid [0, 1] range.
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
        // --- MODIFICATION START: Convert normalized value to real value ---
        auto* parameter = treeState.getParameter(parameterID);
        if (parameter)
        {
            // If found, convert the final normalized value back to the parameter's real value.
            return parameter->convertFrom0to1(it->second);
        }
        // --- MODIFICATION END ---
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
    isPlaying = false;
    double currentBpm = 120.0;

    if (playHead)
    {
        if (auto position = playHead->getPosition())
        {
            isPlaying = position->getIsPlaying();
            if (auto bpm = position->getBpm())
                currentBpm = *bpm;
        }
    }

    // 2. Reset LFOs on transport start
    if (isPlaying && ! wasPlaying)
    {
        for (auto& engine : lfoEngines)
        {
            engine.reset();
        }
    }
    wasPlaying = isPlaying;

    // 3. Process each LFO
    lfoOutputBuffer.setSize(4, numSamples, false, false, true); // Ensure buffer is correct size
    lfoOutputBuffer.clear();

    for (int i = 0; i < 4; ++i)
    {
        // Get LFO parameters
        auto* syncParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, i));
        const bool isInSyncMode = syncParam != nullptr && syncParam->load() > 0.5f;

        float phaseDelta = 0.0f;

        if (isInSyncMode && isPlaying)
        {
            // --- BPM SYNC CALCULATION ---
            auto* rateSyncParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, i));
            if (rateSyncParam != nullptr)
            {
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
        }
        else // --- FREE (HZ) MODE CALCULATION ---
        {
            auto* rateHzParam = treeState.getRawParameterValue(ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, i));
            if (rateHzParam != nullptr)
            {
                const float freqInHz = rateHzParam->load();
                if (sampleRate > 0)
                    phaseDelta = freqInHz / (float) sampleRate;
            }
        }

        lfoEngines[i].setPhaseDelta(phaseDelta);
        lfoEngines[i].updateShape(lfoData[i]);

        // Generate LFO output for the entire block
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