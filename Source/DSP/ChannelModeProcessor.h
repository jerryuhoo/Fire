#pragma once

#include "../Utility/ChannelModeParameters.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fire::dsp
{
// Wrap a prepared stereo chain with a single-component M/S domain. The caller
// owns the fade/reset when the domain changes, and supplies the chain's current
// latency (not the plug-in's total PDC). begin/end never allocate.
class ChannelModeProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec, int maximumDelay)
    {
        capacity = juce::jmax(1, static_cast<int>(std::min<juce::uint64>(
            spec.maximumBlockSize, static_cast<juce::uint64>(std::numeric_limits<int>::max()))));
        auto delaySpec = spec;
        delaySpec.numChannels = 2;
        delaySpec.maximumBlockSize = static_cast<juce::uint32>(capacity);
        delaySpec.sampleRate = std::isfinite(spec.sampleRate) && spec.sampleRate > 0 ? spec.sampleRate : 48000;
        componentDelay.setMaximumDelayInSamples(juce::jmax(0, maximumDelay));
        componentDelay.prepare(delaySpec);
        delayedComponents.setSize(2, capacity);
        reset();
    }

    void reset() noexcept
    {
        componentDelay.reset();
        delayedComponents.clear();
        pendingSamples = pendingChannels = 0;
        activeMode = channel_mode::stereo;
    }

    void begin(juce::AudioBuffer<float>& buffer, int requestedMode, float delay) noexcept
    {
        pendingSamples = pendingChannels = 0;
        const auto samples = buffer.getNumSamples();
        const auto channels = buffer.getNumChannels();
        jassert(samples <= capacity && channels <= 2);
        if (samples <= 0 || channels <= 0 || channels > 2 || samples > capacity)
            return;

        pendingSamples = samples;
        pendingChannels = channels;
        activeMode = channel_mode::valid(requestedMode) ? requestedMode : channel_mode::stereo;
        componentDelay.setDelay(std::isfinite(delay)
            ? juce::jlimit(0.0, static_cast<double>(componentDelay.getMaximumDelayInSamples()), static_cast<double>(delay))
            : 0.0);
        auto* left = buffer.getWritePointer(0);
        auto* right = channels == 2 ? buffer.getWritePointer(1) : nullptr;
        auto* dryMid = delayedComponents.getWritePointer(0);
        auto* drySide = delayedComponents.getWritePointer(1);
        for (int sample = 0; sample < samples; ++sample)
        {
            const double l = finite(left[sample]);
            const double r = right != nullptr ? finite(right[sample]) : l;
            const double mid = .5 * l + .5 * r;
            const double side = .5 * l - .5 * r;
            componentDelay.pushSample(0, mid);
            componentDelay.pushSample(1, side);
            dryMid[sample] = bounded(componentDelay.popSample(0));
            drySide[sample] = bounded(componentDelay.popSample(1));

            // Preserve the original Stereo path exactly, including its float
            // bit patterns. Its delay history still advances for later edits.
            if (activeMode != channel_mode::stereo)
            {
                const auto selected = bounded(activeMode == channel_mode::mid ? mid : side);
                left[sample] = selected;
                if (right != nullptr) right[sample] = selected;
            }
        }
    }

    void end(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto samples = pendingSamples;
        const auto channels = pendingChannels;
        pendingSamples = pendingChannels = 0;
        jassert(samples == 0 || (samples == buffer.getNumSamples() && channels == buffer.getNumChannels()));
        if (samples <= 0 || samples != buffer.getNumSamples() || channels != buffer.getNumChannels()
            || activeMode == channel_mode::stereo)
            return;

        auto* left = buffer.getWritePointer(0);
        if (channels == 1)
        {
            // A mono bus has no Side. Let the silent chain advance, but do not
            // allow bias, frozen playback or a reverb tail to create one.
            if (activeMode == channel_mode::side)
                juce::FloatVectorOperations::copy(left, delayedComponents.getReadPointer(0), samples);
            return;
        }
        auto* right = buffer.getWritePointer(1);
        const auto* retained = delayedComponents.getReadPointer(activeMode == channel_mode::mid ? 1 : 0);
        for (int sample = 0; sample < samples; ++sample)
        {
            // Stereo generators inside the chain are folded into the selected
            // component; the other component is never overwritten by them.
            const double processed = .5 * finite(left[sample]) + .5 * finite(right[sample]);
            const double mid = activeMode == channel_mode::mid ? processed : retained[sample];
            const double side = activeMode == channel_mode::side ? processed : retained[sample];
            left[sample] = bounded(mid + side);
            right[sample] = bounded(mid - side);
        }
    }

private:
    static double finite(float value) noexcept { return std::isfinite(value) ? static_cast<double>(value) : 0.0; }
    static float bounded(double value) noexcept
    {
        if (!std::isfinite(value)) return 0;
        constexpr auto limit = static_cast<double>(std::numeric_limits<float>::max());
        return static_cast<float>(juce::jlimit(-limit, limit, value));
    }

    // Extra precision keeps all-pass arithmetic finite even for the largest
    // finite host samples. Only the returned float is saturated if necessary.
    juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::Thiran> componentDelay;
    juce::AudioBuffer<float> delayedComponents;
    int capacity = 0, pendingSamples = 0, pendingChannels = 0;
    int activeMode = channel_mode::stereo;
};
}
