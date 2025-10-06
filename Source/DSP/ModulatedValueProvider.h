/*
  ==============================================================================

    ModulatedValueProvider.h
    Created: 5 Oct 2025 9:20:14pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "juce_audio_processors/juce_audio_processors.h"

/**
 * @struct ModulatedValueProvider
 * @brief A lightweight, real-time parameter value provider for the audio thread.
 * * This struct holds the "recipe" for calculating a parameter's value at any given sample.
 * It combines a base value (e.g., from a GUI knob) with an optional LFO signal
 * to produce a sample-accurate final value. It is designed to be created on the fly
 * by the main processor and used by DSP modules like BandProcessor.
 */
struct ModulatedValueProvider
{
    //==============================================================================
    // // Member variables
    //==============================================================================

    /**
     * @brief A read-only pointer to the LFO output buffer for a specific LFO source.
     * If this is nullptr, it signifies that the parameter is not currently modulated.
     */
    const float* lfoSignal = nullptr;

    /**
     * @brief The base value of the parameter, typically set by the user via the GUI.
     */
    float baseValue = 0.0f;

    /**
     * @brief The modulation depth, controlling how much the LFO affects the base value.
     * The range is typically [-1.0, 1.0].
     */
    float modulationDepth = 0.5f;

    /**
     * @brief Determines if the LFO signal is treated as bipolar ([-1, 1]) or unipolar ([0, 1]).
     */
    bool isBipolar = true;

    /**
     * @brief The normalisable range of the target parameter.
     * This is crucial for correctly converting between the parameter's real value
     * and its normalized representation [0, 1], where modulation is safely applied.
     */
    juce::NormalisableRange<float> range;

    //==============================================================================
    // // Main function
    //==============================================================================

    /**
     * @brief Gets the final, sample-accurate parameter value. This is the core method.
     * * If the parameter is not modulated (lfoSignal is nullptr), it simply returns the base value.
     * If modulated, it calculates the precise value for the given sample index.
     * * @param sampleIndex The index of the sample within the current processing block.
     * @return The final parameter value for that specific sample.
     */
    inline float get(int sampleIndex) const
    {
        // // This branch is highly predictable by the CPU, resulting in negligible
        // // performance cost for non-modulated parameters.
        if (lfoSignal == nullptr)
            return baseValue;

        // --- Per-sample modulation calculation ---

        // // 1. Get the LFO value for the current sample (range [0, 1]).
        const float lfoSample = lfoSignal[sampleIndex];

        // // 2. Map LFO to bipolar [-1, 1] or unipolar [0, 1] space.
        const float mappedLfo = isBipolar ? (lfoSample * 2.0f - 1.0f) : lfoSample;

        // // 3. Calculate the modulation amount in the normalized [0, 1] domain.
        const float modulationAmount = mappedLfo * modulationDepth;

        // // 4. Get the base value in its normalized form.
        const float baseNormalized = range.convertTo0to1(baseValue);

        // // 5. Add the base value and modulation amount.
        float modulatedNormalized = baseNormalized + modulationAmount;

        // // 6. Clamp the result to the valid [0, 1] range.
        modulatedNormalized = juce::jlimit(0.0f, 1.0f, modulatedNormalized);

        // // 7. Convert back from normalized to the parameter's real value and return.
        return range.convertFrom0to1(modulatedNormalized);
    }
};