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
#include "../Utility/LfoBankParameters.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

class LfoManager
{
public:
    static constexpr int maximumModulationRoutings = 128;

    enum class AssignmentResult
    {
        changed,
        unchanged,
        capacityReached,
        invalidRequest
    };

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

    struct AudioThreadParameterSnapshot
    {
        struct Parameters
        {
            float syncMode = 1.0f;
            float syncedRate = 8.0f;
            float freeRate = 1.0f;
            float phaseOffset = 0.0f;
            float smoothness = 0.0f;
            bool present = false;
            std::uint32_t generation = 0;
        };

        std::array<Parameters, fire::lfo_bank::capacity> lfos {};
        AudioThreadParameterSnapshot()
        {
            for (int index = 0; index < fire::lfo_bank::defaultCount; ++index)
                lfos[static_cast<size_t>(index)].present = true;
        }
    };

    /** Captures the fixed LFO timing/smoothness controls from APVTS atomics.

        The returned object owns only scalar values and is safe to retain as an
        audio-thread callback snapshot. An outer state-publication sequence is
        still required when several APVTS values must belong to one generation.
    */
    AudioThreadParameterSnapshot
    captureAudioThreadParameterSnapshot() const noexcept;

    /** Renders from a caller-owned parameter snapshot.

        This overload neither refreshes UI-owned routing/shape data nor reads
        APVTS parameters. Call beginAudioThreadStateCapture() followed by
        finishAudioThreadStateCapture() (or abort) before rendering when
        a new routing/shape generation may be published.
    */
    void processBlock(juce::AudioBuffer<float>& outputBuffer,
                      float sampleRate,
                      juce::AudioPlayHead* playHead,
                      int numSamples,
                      const AudioThreadParameterSnapshot& parameterSnapshot,
                      juce::int64 playheadSampleOffset = 0);

    /** Starts a non-blocking candidate capture of routes and staged shapes.

        On success the LFO data lock remains held until finish or abort. During
        that interval getAudioThreadRoutingInfo() reads the candidate routes,
        while the active audio-thread routes and wavetable banks remain intact.
        The caller must pair every successful begin with finish or abort; a
        ScopeGuard is recommended.
    */
    bool beginAudioThreadStateCapture(
        std::uint32_t expectedGeneration,
        const std::atomic<std::uint32_t>& generation) noexcept;

    /** Commits or discards the current candidate at the caller's linearization point.

        Pass true only after the caller's final generation check accepted all
        scalar state captured while this object retained the LFO data lock.
        Commit then unconditionally copies the candidate fixed route array and
        publishes the matching staged shape banks before releasing that lock.
        A writer which turns the outer generation odd after the caller's check
        is ordered after this complete callback publication.
    */
    bool finishAudioThreadStateCapture(bool commitCandidate) noexcept;

    /** Discards the current candidate and releases the LFO data lock. */
    void abortAudioThreadStateCapture() noexcept;

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

    /** Looks up the active route, or the candidate during a two-phase capture.
        Audio thread only.
    */
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

    struct ModulationRoutingStateSnapshot
    {
        juce::Array<ModulationRouting> routings;
        std::uint64_t revision = 0;
    };

    struct ModulationRoutingEditResult
    {
        bool accepted = false;
        bool changed = false;
        std::uint64_t revision = 0;
        ModulationRouting routing;
    };

    ModulationRoutingStateSnapshot getModulationRoutingStateSnapshot() const;
    std::uint64_t getModulationRoutingRevision() const;
    ModulationRoutingEditResult updateModulationRoutingIfRevisionMatches(
        int routingIndex,
        std::uint64_t expectedRevision,
        const ModulationRouting& expectedRouting,
        const ModulationRouting& replacementRouting);
    ModulationRoutingEditResult assignModulationRoutingIfRevisionMatches(
        int routingIndex,
        std::uint64_t expectedRevision,
        const ModulationRouting& expectedRouting,
        int sourceLfoIndex,
        const juce::String& targetParameterID);
    ModulationRoutingEditResult addEmptyModulationRoutingIfRevisionMatches(
        std::uint64_t expectedRevision);
    ModulationRoutingEditResult removeModulationRoutingIfRevisionMatches(
        int routingIndex,
        std::uint64_t expectedRevision,
        const ModulationRouting& expectedRouting);

    /** Advances the routing identity after a caller has changed the live
        routing array while holding getLfoDataLock(). */
    std::uint64_t advanceModulationRoutingRevisionLocked() noexcept;

    struct SerializableStateSnapshot
    {
        juce::ValueTree parameterState;
        std::vector<LfoData> lfoData;
        juce::Array<ModulationRouting> routings;
    };

    SerializableStateSnapshot captureSerializableStateSnapshot() const;

    // Allow access to LFO data for the UI/saving state
    // The reference accessor has the same external-lock requirement as above.
    const std::vector<LfoData>& getLfoData() const { return lfoData; }
    std::vector<LfoData> getLfoDataCopy() const;
    struct LfoDataSnapshot
    {
        LfoData data;
        std::uint64_t revision = 0;
    };
    LfoDataSnapshot getLfoDataSnapshot(int index) const;
    bool isLfoDataRevisionCurrent(int index,
                                  std::uint64_t revision) const;
    // APVTS is the only writable Smoothness authority. These shape APIs keep
    // the incoming points/curvatures but replace LfoData::smoothness with the
    // corresponding APVTS value before committing.
    std::uint64_t setLfoData(int index, const LfoData& newData);
    bool setLfoDataIfRevisionMatches(
        int index,
        const LfoData& newData,
        std::uint64_t expectedRevision,
        std::uint64_t& resultingRevision);
    bool replaceLfoDataAndRoutings(
        const std::array<LfoData, fire::lfo_bank::capacity>& newLfoData,
        juce::Array<ModulationRouting> newRoutings);
    bool replaceLfoDataAndRoutings(
        const std::array<LfoData, fire::lfo_bank::defaultCount>& newLfoData,
        juce::Array<ModulationRouting> newRoutings);
    bool isLfoPresent(int index) const noexcept;
    struct VisualState
    {
        float phase = -1.0f;
        // Monotonic publication sequence, including lifecycle invalidations.
        // Zero means no coherent snapshot was available; retain the previous UI frame.
        std::uint64_t renderSequence = 0;
    };
    VisualState getLfoVisualState(int index) const noexcept;
    // Message-thread slot lifecycle, called inside the processor's coherent
    // parameter transaction after the five timing values have been reset.
    void resetLfoSlot(int index);

    // Allow access to LFO engines for UI phase display
    const std::array<LfoEngine, fire::lfo_bank::capacity>& getLfoEngines() const { return lfoEngines; }
    float getLfoPhase(int lfoIndex) const;
    bool isDawPlaying() const { return isPlaying.load(std::memory_order_relaxed); }
    const juce::StringArray& getLfoRateSyncDivisions() const;
    float getLfoOutput(int lfoIndex) const;
    AssignmentResult assignLfoToTarget(
        int sourceLfoIndex,
        const juce::String& targetParameterID);
    bool clearModulationForTarget(const juce::String& targetParameterID);
    bool invertModulationDepth(const juce::String& targetParameterID);
    void onLfoShapeChanged(int lfoIndex);
    bool toggleBypassForRouting(const juce::String& targetParameterID);
    juce::CriticalSection& getLfoDataLock() { return dataAccessLock; }
private:
    struct LfoParameterPointers
    {
        std::atomic<float>* syncMode = nullptr;
        std::atomic<float>* syncedRate = nullptr;
        std::atomic<float>* freeRate = nullptr;
        std::atomic<float>* phaseOffset = nullptr;
        std::atomic<float>* smoothness = nullptr;
        std::atomic<float>* present = nullptr;
    };

    struct RuntimeRouting
    {
        juce::RangedAudioParameter* parameter = nullptr;
        int sourceLfoIndex = 0;
        float depth = 0.0f;
        bool isBipolar = true;
        float normalisedBaseValue = 0.0f;
    };

    struct RuntimeModulatedValue
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float normalisedValue = 0.0f;
    };

    /**
     * @brief Internal helper to generate raw LFO signals into the internal buffer.
     */
    void generateLfoOutput(
        double sampleRate,
        juce::AudioPlayHead* playHead,
        int numSamples,
        const AudioThreadParameterSnapshot& parameterSnapshot,
        juce::int64 playheadSampleOffset);
    void renderBlock(juce::AudioBuffer<float>& outputBuffer,
                     float sampleRate,
                     juce::AudioPlayHead* playHead,
                     int numSamples,
                     const AudioThreadParameterSnapshot& parameterSnapshot,
                     bool readLiveRoutingBaseValues,
                     juce::int64 playheadSampleOffset);
    bool captureRuntimeRoutings(
        std::array<RuntimeRouting, maximumModulationRoutings>& destination,
        size_t& destinationCount) const;
    bool refreshRuntimeStateIfAvailable();
    void updatePublishedRoutingState() noexcept;
    void publishVisualState(const AudioThreadParameterSnapshot* parameters) noexcept;

    float mapRateSyncIndexToBeatMultiplier(int index) const;
    float getSyncCycleLengthInQuarterNotes(int index, float quarterNotesPerBar) const noexcept;

    // =============================================================================
    // SECTION: Member Variables
    // =============================================================================

    juce::AudioProcessorValueTreeState& treeState;

    // Each engine owns prebuilt smoothing tables. Keep the fixed bank on the
    // heap so standalone/stack-owned managers do not exceed thread stack size.
    using EngineBank = std::array<LfoEngine, fire::lfo_bank::capacity>;
    std::unique_ptr<EngineBank> engineStorage = std::make_unique<EngineBank>();
    EngineBank& lfoEngines = *engineStorage;
    std::array<LfoParameterPointers, fire::lfo_bank::capacity> lfoParameters;
    std::array<juce::String, fire::lfo_bank::capacity> smoothnessParameterIDs;
    std::vector<LfoData> lfoData;
    std::array<std::uint64_t, fire::lfo_bank::capacity> lfoDataRevisions {};
    std::array<std::atomic<std::uint32_t>, fire::lfo_bank::capacity> slotGenerations {};
    std::array<std::uint32_t, fire::lfo_bank::capacity> appliedSlotGenerations {};
    std::array<bool, fire::lfo_bank::capacity> previouslyPresent {};
    std::array<std::atomic<float>, fire::lfo_bank::capacity> visualPhases {};
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "LFO visual publication must remain lock-free");
    std::atomic<std::uint64_t> visualSequence { 0 };

    juce::CriticalSection dataAccessLock;

    // Owns all modulation connection rules.
    juce::Array<ModulationRouting> modulationRoutings;
    std::uint64_t modulationRoutingRevision = 0;

    // Fixed-capacity, audio-thread-owned snapshots avoid per-block allocation and UI lock waits.
    std::array<RuntimeRouting, maximumModulationRoutings> runtimeRoutings {};
    std::array<RuntimeRouting, maximumModulationRoutings> candidateRuntimeRoutings {};
    std::array<RuntimeModulatedValue, maximumModulationRoutings> modulatedValues {};
    size_t runtimeRoutingCount = 0;
    size_t candidateRuntimeRoutingCount = 0;
    size_t modulatedValueCount = 0;
    bool candidateHasPublishedRouting = false;
    bool runtimeStateCaptureInProgress = false;
    std::atomic<bool> hasPublishedRouting { false };
    bool routingSnapshotRefreshedThisBlock = false;

    // Internal buffer to hold the raw LFO signals.
    juce::AudioBuffer<float> lfoOutputBuffer;
    double preparedSampleRate { 44100.0 };

    // Audio-thread-owned bookkeeping for Phase while no absolute host timeline
    // is available. The knob is an offset, so changes are applied as a delta
    // instead of being added again at every callback boundary.
    std::array<float, fire::lfo_bank::capacity> appliedPhaseOffsets {};
    std::array<bool, fire::lfo_bank::capacity> phaseOffsetInitialised {};

    // Audio-thread-only signature of the timing recipe used on the previous
    // callback. It distinguishes a real live parameter/transport change from
    // the host's ordinary absolute re-anchor at every block boundary.
    std::array<bool, fire::lfo_bank::capacity> timingSignatureInitialised {};
    std::array<bool, fire::lfo_bank::capacity> previousSyncModes {};
    std::array<float, fire::lfo_bank::capacity> previousActiveRateKeys {};
    std::array<bool, fire::lfo_bank::capacity> usedAbsoluteTimelineLastBlock {};

    std::atomic<bool> isPlaying { false };

    juce::StringArray lfoRateSyncDivisions;
};
