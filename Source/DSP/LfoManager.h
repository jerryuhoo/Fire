/*
  ==============================================================================

    LfoManager.h
    Created: 10 Sep 2025 11:07:34pm
    Author:  Yifeng Yu

    REFACTORED to act as the central modulation controller.
    It now owns the modulation routings and calculates all final
    modulated parameter values for each processing block.

  ==============================================================================
*/

#pragma once

#include "LfoData.h"
#include "LfoEngine.h"
#include "ModulationRouting.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include <map>

class LfoManager
{
public:
    LfoManager(juce::AudioProcessorValueTreeState& apvts);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    bool isModulationActive() const;

    /**
     * @brief The new main processing function for the modulation system.
     * Call this ONCE per processBlock. It will:
     * 1. Generate all raw LFO signals for the block.
     * 2. Calculate the final value of every modulated parameter.
     * @param sampleRate The current sample rate.
     * @param playHead Optional pointer to the host's playhead for BPM sync.
     * @param numSamples The number of samples in the current block.
     */
    void processBlock(double sampleRate, juce::AudioPlayHead* playHead, int numSamples);

    /**
     * @brief Gets the final, possibly modulated, value for a given parameter.
     * If the parameter is being modulated, this returns the calculated value.
     * If not, it returns the original value from the APVTS.
     * @param parameterID The ID of the parameter to query.
     * @return The final float value to be used by the DSP.
     */
    float getModulatedValue(const juce::String& parameterID) const;

    // =============================================================================
    // SECTION: Accessors for UI and State Management
    // =============================================================================

    // Modulation routings are now owned by the LfoManager.
    juce::Array<ModulationRouting>& getModulationRoutings() { return modulationRoutings; }
    const juce::Array<ModulationRouting>& getModulationRoutings() const { return modulationRoutings; }

    // Allow access to LFO data for the UI/saving state
    std::vector<LfoData>& getLfoData() { return lfoData; }
    const std::vector<LfoData>& getLfoData() const { return lfoData; }

    // Allow access to LFO engines for UI phase display
    const std::array<LfoEngine, 4>& getLfoEngines() const { return lfoEngines; }
    float getLfoPhase(int lfoIndex) const;
    bool isDawPlaying() const { return isPlaying; }
    const juce::StringArray& getLfoRateSyncDivisions() const;
    float getLfoOutput(int lfoIndex) const;
    void assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID);
    void clearModulationForTarget(const juce::String& targetParameterID);
    void invertModulationDepth(const juce::String& targetParameterID);
    void onLfoShapeChanged(int lfoIndex);

private:
    /**
     * @brief Internal helper to generate raw LFO signals into the internal buffer.
     */
    void generateLfoOutput(double sampleRate, juce::AudioPlayHead* playHead, int numSamples);

    float mapRateSyncIndexToBeatMultiplier(int index) const;

    // =============================================================================
    // SECTION: Member Variables
    // =============================================================================

    juce::AudioProcessorValueTreeState& treeState;

    std::array<LfoEngine, 4> lfoEngines;
    std::vector<LfoData> lfoData;

    // Owns all modulation connection rules.
    juce::Array<ModulationRouting> modulationRoutings;

    // Stores the final calculated values for all modulated parameters for the current block.
    std::map<juce::String, float> modulatedValues;

    // Internal buffer to hold the raw LFO signals.
    juce::AudioBuffer<float> lfoOutputBuffer;

    bool isPlaying { false };
    bool wasPlaying { false };

    juce::StringArray lfoRateSyncDivisions;
    std::array<std::atomic<bool>, 4> shapeUpdateFlags;
};
