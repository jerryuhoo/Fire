/*
  ==============================================================================

    LfoManager.cpp
    Created: 10 Sep 2025 11:07:34pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "LfoManager.h"
#include "../Utility/AudioHelpers.h"

LfoManager::LfoManager(juce::AudioProcessorValueTreeState& apvts) : treeState(apvts)
{
    lfoData.resize(4);

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
}

void LfoManager::reset()
{
    for (auto& engine : lfoEngines)
    {
        engine.reset();
    }
    wasPlaying = false;
}

void LfoManager::process(juce::AudioBuffer<float>& lfoOutputBuffer, double sampleRate, juce::AudioPlayHead* playHead)
{
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
        for (int sample = 0; sample < lfoOutputBuffer.getNumSamples(); ++sample)
        {
            writer[sample] = lfoEngines[i].process();
        }
    }
}

// You need to move this helper function from PluginProcessor.cpp as well
float LfoManager::mapRateSyncIndexToBeatMultiplier(int index) const
{
    // This mapping should be consistent with the one in your processor
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

float LfoManager::getLfoPhase(int lfoIndex) const
{
    if (juce::isPositiveAndBelow(lfoIndex, (int) lfoEngines.size()))
    {
        return lfoEngines[lfoIndex].getPhase();
    }
    jassertfalse;
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

    jassertfalse; // Should not happen if lfoIndex is always valid
    return 0.0f;
}