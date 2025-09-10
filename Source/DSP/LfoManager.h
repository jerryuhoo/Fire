/*
  ==============================================================================

    LfoManager.h
    Created: 10 Sep 2025 11:07:34pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../GUI/InterfaceDefines.h"
#include "LfoData.h"
#include "LfoEngine.h"
#include "juce_audio_processors/juce_audio_processors.h"


class LfoManager
{
public:
    LfoManager(juce::AudioProcessorValueTreeState& apvts);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /**
     * @brief Processes all LFOs and fills the output buffer with their signals.
     * * @param lfoOutputBuffer The buffer to fill with LFO output.
     * @param sampleRate The current sample rate.
     * @param playHead Optional pointer to the host's playhead for BPM sync.
     */
    void process(juce::AudioBuffer<float>& lfoOutputBuffer, double sampleRate, juce::AudioPlayHead* playHead);

    // Allow access to LFO data for the UI/saving state
    std::vector<LfoData>& getLfoData() { return lfoData; }
    const std::vector<LfoData>& getLfoData() const { return lfoData; }

    // Allow access to LFO engines for UI phase display
    const std::array<LfoEngine, 4>& getLfoEngines() const { return lfoEngines; }
    float getLfoPhase(int lfoIndex) const;
    bool isDawPlaying() const { return isPlaying; }
    const juce::StringArray& getLfoRateSyncDivisions() const;
    float getLfoOutput(int lfoIndex) const;

private:
    float mapRateSyncIndexToBeatMultiplier(int index) const;

    juce::AudioProcessorValueTreeState& treeState;

    std::array<LfoEngine, 4> lfoEngines;
    std::vector<LfoData> lfoData;

    bool isPlaying { false };
    bool wasPlaying { false };

    juce::StringArray lfoRateSyncDivisions;
};
