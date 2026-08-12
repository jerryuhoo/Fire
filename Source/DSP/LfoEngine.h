/*
  ==============================================================================

    LfoEngine.h
    Created: 4 Aug 2025 6:15:46pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "LfoData.h"
#include "juce_dsp/juce_dsp.h"
#include <array>
#include <atomic>

//==============================================================================
/**
    LfoEngine
 
    Generates an LFO signal from either a built-in shape (Sine, Saw, etc.)
    or a user-defined shape, using a high-performance wavetable lookup.
*/
class LfoEngine
{
public:
    LfoEngine();

    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);

    /** Builds a complete shape bank in the inactive slot.

        The caller must serialise this with publishStagedShape(). LfoManager
        does that with its existing data lock, so the audio thread can keep
        reading the active bank while a new one is built.
    */
    void stageShape(const LfoData& shapeData);

    /** Publishes a previously staged bank. Call only at an audio-block boundary
        while holding the same external lock used by stageShape().
    */
    void publishStagedShape() noexcept;

    /** Selects one of the parameter's 101 declared smoothness steps. Audio-thread only. */
    void setSmoothness(float newSmoothness) noexcept;

    /** Processes one sample of the LFO. Returns a unipolar [0, 1] signal. */
    float process();

    // Directly sets the LFO's current phase.
    void setPhase(float newPhase);

    /** Sets the phase increment per sample, controlling the LFO's speed. */
    void setPhaseDelta(float newPhaseDelta);

    float getPhase() const;
    float getLastOutput() const;

private:
    static constexpr size_t wavetableSize = 1024;
    static constexpr size_t wavetableGuardSize = wavetableSize + 1;
    static constexpr size_t smoothnessStepCount = 101;

    using Wavetable = std::array<float, wavetableGuardSize>;
    using WavetableBank = std::array<Wavetable, smoothnessStepCount>;

    float lookupTable(const Wavetable& table, float lookupPhase) const noexcept;
    void captureCurrentAudibleTable() noexcept;
    void beginTableTransition() noexcept;

    float phase = 0.0f;
    float phaseDelta = 0.0f;
    float lastOutput = 0.0f;

    // The UI reads these while the audio thread updates the working values above.
    std::atomic<float> publishedPhase { 0.0f };
    std::atomic<float> publishedOutput { 0.0f };

    // Two fixed banks provide a lock-coordinated producer/consumer handoff.
    // No table storage is allocated or modified by the audio thread.
    std::array<WavetableBank, 2> wavetableBanks {};
    int activeBank = 0;
    int stagedBank = 1;
    int activeSmoothnessStep = 0;
    bool stagedBankReady = false;

    // Smooth parameter changes crossfade from the currently audible table to
    // the newly selected prebuilt row. Keeping a private snapshot makes rapid
    // target changes continuous without modifying either shared bank.
    Wavetable transitionSourceTable {};
    int transitionLengthSamples = 0;
    int transitionSamplesProcessed = 0;
};
