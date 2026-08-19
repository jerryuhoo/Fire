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
#include <array>
#include <atomic>

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
    void processBlock(juce::AudioBuffer<float>& outputBuffer, float sampleRate, juce::AudioPlayHead* playHead, int numSamples);

    /**
     * @brief Gets the final, possibly modulated, value for a given parameter.
     * If the parameter is being modulated, this returns the calculated value.
     * If not, it returns the original value from the APVTS.
     * @param parameterID The ID of the parameter to query.
     * @return The final float value to be used by the DSP.
     */
    float getModulatedValue(const juce::String& parameterID) const;

    struct AudioThreadRoutingInfo
    {
        int sourceLfoIndex = -1;
        float depth = 0.0f;
        bool isBipolar = true;
    };

    /** Looks up the already-published routing snapshot. Audio thread only. */
    bool getAudioThreadRoutingInfo(const juce::RangedAudioParameter* parameter,
                                   AudioThreadRoutingInfo& result) const noexcept;

    /** True when processBlock refreshed the fixed routing snapshot this call. */
    bool wasRoutingSnapshotRefreshedThisBlock() const noexcept
    {
        return routingSnapshotRefreshedThisBlock;
    }

    // =============================================================================
    // SECTION: Accessors for UI and State Management
    // =============================================================================

    // Modulation routings are now owned by the LfoManager.
    // Legacy reference accessors: callers must hold getLfoDataLock() for the whole access.
    juce::Array<ModulationRouting>& getModulationRoutings() { return modulationRoutings; }
    const juce::Array<ModulationRouting>& getModulationRoutings() const { return modulationRoutings; }
    juce::Array<ModulationRouting> getModulationRoutingsCopy() const;

    // Allow access to LFO data for the UI/saving state
    // The reference accessor has the same external-lock requirement as above.
    const std::vector<LfoData>& getLfoData() const { return lfoData; }
    std::vector<LfoData> getLfoDataCopy() const;
    void setLfoData(int index, const LfoData& newData);
    void clearAllLfoData();

    // Allow access to LFO engines for UI phase display
    const std::array<LfoEngine, 4>& getLfoEngines() const { return lfoEngines; }
    float getLfoPhase(int lfoIndex) const;
    bool isDawPlaying() const { return isPlaying.load(std::memory_order_relaxed); }
    const juce::StringArray& getLfoRateSyncDivisions() const;
    float getLfoOutput(int lfoIndex) const;
    void assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID);
    void clearModulationForTarget(const juce::String& targetParameterID);
    void invertModulationDepth(const juce::String& targetParameterID);
    void onLfoShapeChanged(int lfoIndex);
    void toggleBypassForRouting(const juce::String& targetParameterID);
    juce::CriticalSection& getLfoDataLock() { return dataAccessLock; }
private:
    static constexpr size_t maxRuntimeRoutings = 128;

    struct LfoParameterPointers
    {
        std::atomic<float>* syncMode = nullptr;
        std::atomic<float>* syncedRate = nullptr;
        std::atomic<float>* freeRate = nullptr;
        std::atomic<float>* phaseOffset = nullptr;
        std::atomic<float>* smoothness = nullptr;
        juce::RangedAudioParameter* smoothnessParameter = nullptr;
    };

    struct RuntimeRouting
    {
        juce::RangedAudioParameter* parameter = nullptr;
        int sourceLfoIndex = 0;
        float depth = 0.0f;
        bool isBipolar = true;
    };

    struct RuntimeModulatedValue
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float normalisedValue = 0.0f;
    };

    /**
     * @brief Internal helper to generate raw LFO signals into the internal buffer.
     */
    void generateLfoOutput(double sampleRate, juce::AudioPlayHead* playHead, int numSamples);
    bool refreshRuntimeStateIfAvailable();
    void updatePublishedRoutingState() noexcept;

    float mapRateSyncIndexToBeatMultiplier(int index) const;
    float getSyncCycleLengthInQuarterNotes(int index, float quarterNotesPerBar) const noexcept;

    // =============================================================================
    // SECTION: Member Variables
    // =============================================================================

    juce::AudioProcessorValueTreeState& treeState;

    std::array<LfoEngine, 4> lfoEngines;
    std::array<LfoParameterPointers, 4> lfoParameters;
    std::vector<LfoData> lfoData;

    juce::CriticalSection dataAccessLock;

    // Owns all modulation connection rules.
    juce::Array<ModulationRouting> modulationRoutings;

    // Fixed-capacity, audio-thread-owned snapshots avoid per-block allocation and UI lock waits.
    std::array<RuntimeRouting, maxRuntimeRoutings> runtimeRoutings {};
    std::array<RuntimeModulatedValue, maxRuntimeRoutings> modulatedValues {};
    size_t runtimeRoutingCount = 0;
    size_t modulatedValueCount = 0;
    std::atomic<bool> hasPublishedRouting { false };
    bool routingSnapshotRefreshedThisBlock = false;

    // Internal buffer to hold the raw LFO signals.
    juce::AudioBuffer<float> lfoOutputBuffer;
    double preparedSampleRate { 44100.0 };

    // Audio-thread-owned bookkeeping for Phase while no absolute host timeline
    // is available. The knob is an offset, so changes are applied as a delta
    // instead of being added again at every callback boundary.
    std::array<float, 4> appliedPhaseOffsets {};
    std::array<bool, 4> phaseOffsetInitialised {};

    // Audio-thread-only signature of the timing recipe used on the previous
    // callback. It distinguishes a real live parameter/transport change from
    // the host's ordinary absolute re-anchor at every block boundary.
    std::array<bool, 4> timingSignatureInitialised {};
    std::array<bool, 4> previousSyncModes {};
    std::array<float, 4> previousActiveRateKeys {};
    std::array<bool, 4> usedAbsoluteTimelineLastBlock {};

    std::atomic<bool> isPlaying { false };

    juce::StringArray lfoRateSyncDivisions;
};
