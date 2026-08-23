/*
  ==============================================================================

    ZeroLatencyModulatedDryWetMixer.h

  ==============================================================================
*/

#pragma once

#include "ModulatedValueProvider.h"
#include "juce_dsp/juce_dsp.h"
#include <cmath>
#include <limits>

/**
 * A linear dry/wet mixer for paths whose wet signal has no added latency.
 *
 * Static mix and enable changes retain the legacy 50 ms JUCE ramp. A routed
 * LFO is evaluated directly at every sample after smoothing only its base
 * value, so the LFO trajectory is not accidentally low-pass filtered by
 * repeatedly retargeting a DryWetMixer. Discrete routing-recipe changes use a
 * separate 10 ms held-anchor bridge.
 */
class ZeroLatencyModulatedDryWetMixer
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        const double safeSampleRate = std::isfinite(spec.sampleRate)
                                          && spec.sampleRate > 0.0
                                      ? spec.sampleRate
                                      : 48000.0;
        sampleRate = safeSampleRate;

        const auto channels = juce::jmax(1, static_cast<int>(spec.numChannels));
        const auto requestedCapacity = juce::jmax(
            1,
            static_cast<int>(juce::jmin<juce::uint64>(
                spec.maximumBlockSize,
                static_cast<juce::uint64>(std::numeric_limits<int>::max()))));
        const auto fifoCapacity = juce::nextPowerOfTwo(requestedCapacity);
        dryBuffer.setSize(channels, fifoCapacity, false, false, true);
        fifo = juce::SingleThreadedAbstractFifo(fifoCapacity);

        legacyDryMix.reset(sampleRate, legacyRampSeconds);
        legacyWetMix.reset(sampleRate, legacyRampSeconds);
        routedBaseMix.reset(sampleRate, legacyRampSeconds);
        routedEnableMix.reset(sampleRate, legacyRampSeconds);
        routeTransitionMix.reset(sampleRate, routeRampSeconds);
        reset();
    }

    void reset() noexcept
    {
        legacyDryMix.reset(sampleRate, legacyRampSeconds);
        legacyWetMix.reset(sampleRate, legacyRampSeconds);
        routedBaseMix.reset(sampleRate, legacyRampSeconds);
        routedEnableMix.reset(sampleRate, legacyRampSeconds);
        routeTransitionMix.reset(sampleRate, routeRampSeconds);

        legacyDryMix.setCurrentAndTargetValue(0.0f);
        legacyWetMix.setCurrentAndTargetValue(1.0f);
        routedBaseMix.setCurrentAndTargetValue(1.0f);
        routedEnableMix.setCurrentAndTargetValue(1.0f);
        routeTransitionMix.setCurrentAndTargetValue(1.0f);

        fifo = juce::SingleThreadedAbstractFifo(
            juce::jmax(1, dryBuffer.getNumSamples()));
        recipe = {};
        anchorDryMix = 0.0f;
        anchorWetMix = 1.0f;
        lastAppliedDryMix = 0.0f;
        lastAppliedWetMix = 1.0f;
        initialised = false;
    }

    void pushDrySamples(
        const juce::dsp::AudioBlock<const float> drySamples) noexcept
    {
        const auto numSamples = static_cast<int>(drySamples.getNumSamples());
        if (numSamples <= 0)
            return;

        jassert(drySamples.getNumChannels()
                <= static_cast<size_t>(dryBuffer.getNumChannels()));
        jassert(numSamples <= fifo.getRemainingSpace());
        if (drySamples.getNumChannels()
                > static_cast<size_t>(dryBuffer.getNumChannels())
            || numSamples > fifo.getRemainingSpace())
            return;

        int sourceOffset = 0;
        for (const auto& range : fifo.write(numSamples))
        {
            const auto rangeLength = range.getLength();
            if (rangeLength <= 0)
                continue;

            auto destination = juce::dsp::AudioBlock<float>(dryBuffer)
                                   .getSubsetChannelBlock(
                                       0, drySamples.getNumChannels())
                                   .getSubBlock(
                                       static_cast<size_t>(range.getStart()),
                                       static_cast<size_t>(rangeLength));
            destination.copyFrom(drySamples.getSubBlock(
                static_cast<size_t>(sourceOffset),
                static_cast<size_t>(rangeLength)));
            sourceOffset += rangeLength;
        }
    }

    void mixWetSamples(juce::dsp::AudioBlock<float> wetSamples,
                       const ModulatedValueProvider& provider,
                       float legacyBaseMix,
                       int sourceIndex,
                       bool enabled) noexcept
    {
        const auto numSamples = static_cast<int>(wetSamples.getNumSamples());
        if (numSamples <= 0)
            return;

        jassert(wetSamples.getNumChannels()
                <= static_cast<size_t>(dryBuffer.getNumChannels()));
        jassert(numSamples <= fifo.getNumReadable());
        if (wetSamples.getNumChannels()
                > static_cast<size_t>(dryBuffer.getNumChannels())
            || numSamples > fifo.getNumReadable())
            return;

        const float safeLegacyBase = sanitiseMix(legacyBaseMix);
        const bool routed = provider.lfoSignal != nullptr;
        const float safeRoutedBase = sanitiseMix(
            routed ? provider.baseValue : safeLegacyBase);
        const float legacyTarget = enabled ? safeLegacyBase : 0.0f;

        const RecipeSignature nextRecipe {
            routed,
            routed ? sourceIndex : -1,
            routed ? sanitiseDepth(provider.modulationDepth) : 0.0f,
            routed ? provider.isBipolar : true
        };

        if (! initialised)
        {
            legacyWetMix.setCurrentAndTargetValue(legacyTarget);
            legacyDryMix.setCurrentAndTargetValue(1.0f - legacyTarget);
            routedBaseMix.setCurrentAndTargetValue(safeRoutedBase);
            routedEnableMix.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
            routeTransitionMix.setCurrentAndTargetValue(1.0f);
            recipe = nextRecipe;

            if (routed)
            {
                lastAppliedWetMix = sanitiseMix(
                    routedEnableMix.getCurrentValue()
                    * provider.get(0, routedBaseMix.getCurrentValue()));
                lastAppliedDryMix = 1.0f - lastAppliedWetMix;
            }
            else
            {
                lastAppliedWetMix = legacyWetMix.getCurrentValue();
                lastAppliedDryMix = legacyDryMix.getCurrentValue();
            }

            anchorWetMix = lastAppliedWetMix;
            anchorDryMix = lastAppliedDryMix;
            initialised = true;
        }
        else if (! sameRecipe(recipe, nextRecipe))
        {
            anchorWetMix = lastAppliedWetMix;
            anchorDryMix = lastAppliedDryMix;
            routeTransitionMix.setCurrentAndTargetValue(0.0f);
            routeTransitionMix.setTargetValue(1.0f);
            recipe = nextRecipe;
        }

        legacyWetMix.setTargetValue(legacyTarget);
        legacyDryMix.setTargetValue(1.0f - legacyTarget);
        routedBaseMix.setTargetValue(safeRoutedBase);
        routedEnableMix.setTargetValue(enabled ? 1.0f : 0.0f);

        if (! recipe.routed && ! routeTransitionMix.isSmoothing())
        {
            mixLegacyBlock(wetSamples);
            return;
        }

        int outputOffset = 0;
        for (const auto& range : fifo.read(numSamples))
        {
            const auto rangeLength = range.getLength();
            for (int rangeSample = 0; rangeSample < rangeLength;
                 ++rangeSample)
            {
                const int sampleIndex = outputOffset + rangeSample;
                const float legacyWet = legacyWetMix.getNextValue();
                const float legacyDry = legacyDryMix.getNextValue();
                const float routedBase = routedBaseMix.getNextValue();
                const float routedEnable = routedEnableMix.getNextValue();

                float targetWet = legacyWet;
                float targetDry = legacyDry;
                if (recipe.routed)
                {
                    targetWet = sanitiseMix(
                        routedEnable
                        * provider.get(sampleIndex, routedBase));
                    targetDry = 1.0f - targetWet;
                }

                const float transition = routeTransitionMix.getCurrentValue();
                const float wetMix = transition <= 0.0f
                                         ? anchorWetMix
                                     : transition >= 1.0f
                                         ? targetWet
                                         : anchorWetMix
                                               + transition
                                                     * (targetWet
                                                        - anchorWetMix);
                const float dryMix = transition <= 0.0f
                                         ? anchorDryMix
                                     : transition >= 1.0f
                                         ? targetDry
                                         : anchorDryMix
                                               + transition
                                                     * (targetDry
                                                        - anchorDryMix);

                for (size_t channel = 0;
                     channel < wetSamples.getNumChannels(); ++channel)
                {
                    auto* wet = wetSamples.getChannelPointer(channel);
                    const auto* dry = dryBuffer.getReadPointer(
                        static_cast<int>(channel));
                    wet[sampleIndex] *= wetMix;
                    wet[sampleIndex] += dry[range.getStart() + rangeSample]
                                        * dryMix;
                }

                lastAppliedWetMix = wetMix;
                lastAppliedDryMix = dryMix;
                routeTransitionMix.getNextValue();
            }

            outputOffset += rangeLength;
        }
    }

private:
    struct RecipeSignature
    {
        bool routed = false;
        int sourceIndex = -1;
        float modulationDepth = 0.0f;
        bool isBipolar = true;
    };

    static constexpr double legacyRampSeconds = 0.05;
    static constexpr double routeRampSeconds = 0.01;

    static float sanitiseMix(float value) noexcept
    {
        return std::isfinite(value) ? juce::jlimit(0.0f, 1.0f, value)
                                    : 0.0f;
    }

    static float sanitiseDepth(float value) noexcept
    {
        return std::isfinite(value) ? juce::jlimit(-1.0f, 1.0f, value)
                                    : 0.0f;
    }

    static bool sameRecipe(const RecipeSignature& lhs,
                           const RecipeSignature& rhs) noexcept
    {
        if (lhs.routed != rhs.routed)
            return false;
        if (! lhs.routed)
            return true;

        return lhs.sourceIndex == rhs.sourceIndex
               && juce::exactlyEqual(lhs.modulationDepth,
                                     rhs.modulationDepth)
               && lhs.isBipolar == rhs.isBipolar;
    }

    void mixLegacyBlock(juce::dsp::AudioBlock<float> wetSamples) noexcept
    {
        const auto numSamples = static_cast<int>(wetSamples.getNumSamples());
        int outputOffset = 0;

        for (const auto& range : fifo.read(numSamples))
        {
            const auto rangeLength = range.getLength();
            if (rangeLength <= 0)
                continue;

            auto wet = wetSamples.getSubBlock(
                static_cast<size_t>(outputOffset),
                static_cast<size_t>(rangeLength));
            auto dry = juce::dsp::AudioBlock<float>(dryBuffer)
                           .getSubsetChannelBlock(
                               0, wetSamples.getNumChannels())
                           .getSubBlock(
                               static_cast<size_t>(range.getStart()),
                               static_cast<size_t>(rangeLength));

            wet.multiplyBy(legacyWetMix);
            dry.multiplyBy(legacyDryMix);
            wet.add(dry);
            outputOffset += rangeLength;
        }

        const bool advanceRoutedBase = routedBaseMix.isSmoothing();
        const bool advanceRoutedEnable = routedEnableMix.isSmoothing();
        for (int sample = 0;
             sample < numSamples
             && (advanceRoutedBase || advanceRoutedEnable);
             ++sample)
        {
            if (advanceRoutedBase)
                routedBaseMix.getNextValue();
            if (advanceRoutedEnable)
                routedEnableMix.getNextValue();
        }

        lastAppliedWetMix = legacyWetMix.getCurrentValue();
        lastAppliedDryMix = legacyDryMix.getCurrentValue();
    }

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        legacyDryMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        legacyWetMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routedBaseMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routedEnableMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routeTransitionMix;

    juce::AudioBuffer<float> dryBuffer;
    juce::SingleThreadedAbstractFifo fifo;
    RecipeSignature recipe;
    double sampleRate = 48000.0;
    float anchorDryMix = 0.0f;
    float anchorWetMix = 1.0f;
    float lastAppliedDryMix = 0.0f;
    float lastAppliedWetMix = 1.0f;
    bool initialised = false;
};
