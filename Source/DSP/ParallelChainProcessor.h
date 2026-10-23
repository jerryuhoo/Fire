#pragma once

#include "ZeroLatencyModulatedDryWetMixer.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace fire::dsp
{
// Owns only the second branch's audio and the two alignment delays. Module
// ownership/order and the Serial fast path stay in the enclosing processor.
// Every begin() must finish before the next begin(); all views are non-owning.
class ParallelChainProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec, int maximumDelay)
    {
        capacity = juce::jmax(1, static_cast<int>(std::min<juce::uint64>(
            spec.maximumBlockSize, static_cast<juce::uint64>(std::numeric_limits<int>::max()))));
        preparedChannels = juce::jlimit(1, 2, static_cast<int>(spec.numChannels));
        auto prepared = spec;
        prepared.sampleRate = std::isfinite(spec.sampleRate) && spec.sampleRate > 0 ? spec.sampleRate : 48000;
        prepared.maximumBlockSize = static_cast<juce::uint32>(capacity);
        prepared.numChannels = static_cast<juce::uint32>(preparedChannels);
        branchB.setSize(preparedChannels, capacity);
        for (auto& delay : padding)
        {
            delay.setMaximumDelayInSamples(juce::jmax(0, maximumDelay));
            delay.prepare(prepared);
        }
        mixer.prepare(prepared);
        reset();
    }

    void reset() noexcept
    {
        resetDelays();
        mixer.reset();
    }

    // HQ changes alter the processed audio feeding these histories. Clear
    // them under the enclosing muted warm-up, without restarting Blend ramps.
    void resetDelays() noexcept
    {
        for (auto& delay : padding) delay.reset();
        pendingSamples = pendingChannels = 0;
    }

    juce::AudioBuffer<float> begin(juce::AudioBuffer<float>& branchA) noexcept
    {
        jassert(pendingSamples == 0);
        pendingSamples = pendingChannels = 0;
        const auto count = branchA.getNumSamples(), channels = branchA.getNumChannels();
        jassert(count <= capacity && channels <= preparedChannels);
        if (count <= 0 || channels <= 0 || count > capacity || channels > preparedChannels) return {};
        for (int channel = 0; channel < channels; ++channel)
            juce::FloatVectorOperations::copy(branchB.getWritePointer(channel), branchA.getReadPointer(channel), count);
        pendingSamples = count; pendingChannels = channels;
        // At most two channel pointers use AudioBuffer's inline pointer array;
        // returning this prvalue does not copy samples or allocate storage.
        return juce::AudioBuffer<float>(branchB.getArrayOfWritePointers(), channels, count);
    }

    void finish(juce::AudioBuffer<float>& branchA, float padA, float padB,
                const ModulatedValueProvider& provider, float base, int source) noexcept
    {
        const auto count = pendingSamples, channels = pendingChannels;
        pendingSamples = pendingChannels = 0;
        jassert(count == 0 || (count == branchA.getNumSamples() && channels == branchA.getNumChannels()));
        if (count <= 0 || count != branchA.getNumSamples() || channels != branchA.getNumChannels()) return;
        juce::AudioBuffer<float> second(branchB.getArrayOfWritePointers(), channels, count);
        align(branchA, padding[0], padA);
        align(second, padding[1], padB);
        mixer.pushDrySamples(juce::dsp::AudioBlock<float>(branchA));
        // provider.lfoSignal already points at this range's first LFO sample.
        mixer.mixWetSamples(juce::dsp::AudioBlock<float>(second), provider, base, source, true);
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto* mixed = second.getReadPointer(channel);
            auto* output = branchA.getWritePointer(channel);
            for (int sample = 0; sample < count; ++sample)
                output[sample] = std::isfinite(mixed[sample]) ? mixed[sample] : 0;
        }
    }

private:
    using Delay = juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::Thiran>;
    static void align(juce::AudioBuffer<float>& buffer, Delay& delay, float requested) noexcept
    {
        const auto amount = std::isfinite(requested)
            ? juce::jlimit(0.0, static_cast<double>(delay.getMaximumDelayInSamples()), static_cast<double>(requested)) : 0;
        delay.setDelay(amount);
        constexpr auto maximum = static_cast<double>(std::numeric_limits<float>::max());
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* samples = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto original = std::isfinite(samples[sample]) ? samples[sample] : 0.0f;
                delay.pushSample(channel, static_cast<double>(original));
                const auto output = delay.popSample(channel);
                // Thiran's zero-delay recurrence can lose a tiny sample after
                // a very large one through cancellation. Advance its history,
                // but keep the original sample for an exact zero-padding path.
                samples[sample] = amount == 0 ? original
                    : std::isfinite(output) ? static_cast<float>(juce::jlimit(-maximum, maximum, output)) : 0;
            }
        }
    }

    juce::AudioBuffer<float> branchB;
    std::array<Delay, 2> padding;
    ZeroLatencyModulatedDryWetMixer mixer;
    int capacity = 0, preparedChannels = 0, pendingSamples = 0, pendingChannels = 0;
};
}
