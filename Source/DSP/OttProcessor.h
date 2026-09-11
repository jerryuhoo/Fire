#pragma once

#include "ModulatedValueProvider.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// One stereo-linked upward/downward compressor. Fire's existing crossover
// supplies the bands, so this stage adds neither another crossover nor latency.
class OttProcessor
{
public:
    enum Control { depth, timeScale, upward, downward, output, mix, controlCount };
    inline static constexpr std::array<float, controlCount> defaults { 0.5f, 100.0f, -48.0f, -18.0f, 0.0f, 1.0f };
    inline static constexpr std::array<float, controlCount> minimums { 0.0f, 10.0f, -72.0f, -36.0f, -24.0f, 0.0f };
    inline static constexpr std::array<float, controlCount> maximums { 1.0f, 400.0f, -6.0f, 0.0f, 24.0f, 1.0f };

    struct Parameters
    {
        bool enabled = false;
        std::array<ModulatedValueProvider, controlCount> controls;
        std::array<int, controlCount> sources { -1, -1, -1, -1, -1, -1 };
        Parameters()
        {
            for (size_t i = 0; i < controls.size(); ++i)
            {
                controls[i].baseValue = defaults[i];
                controls[i].range = { minimums[i], maximums[i] };
            }
        }
    };

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = std::isfinite(spec.sampleRate) && spec.sampleRate > 0.0 ? spec.sampleRate : 48000.0;
        for (auto& smoother : baseSmoothers)
            smoother.reset(sampleRate, 0.02);
        for (auto& route : routes)
            route.blend.reset(sampleRate, 0.01);
        enableSmoother.reset(sampleRate, 0.02);
        reset();
    }

    void reset() noexcept
    {
        for (size_t i = 0; i < baseSmoothers.size(); ++i)
        {
            baseSmoothers[i].setCurrentAndTargetValue(defaults[i]);
            routes[i].blend.setCurrentAndTargetValue(1.0f);
            routes[i].lastValue = defaults[i];
            routes[i].routed = false;
        }
        enableSmoother.setCurrentAndTargetValue(0.0f);
        envelope = 0.0f;
        levelDb = -120.0f;
        gainChangeDb = 0.0f;
        dynamicsActivityDb = 0.0f;
        lastTime = -1.0f;
        initialised = false;
    }

    static float gainForLevel(float inputDb, float lower, float upper, float amount) noexcept
    {
        upper = sanitise(downward, upper);
        lower = juce::jmin(sanitise(upward, lower), upper - 6.0f);
        amount = sanitise(depth, amount);
        if (! std::isfinite(inputDb) || inputDb <= -96.0f)
            return 0.0f;
        // 4:1 upward and 8:1 downward compression, joined by 6 dB soft knees.
        const auto boost = juce::jmin(24.0f, 0.75f * softPositive(lower - inputDb));
        const auto reduction = juce::jmin(48.0f, 0.875f * softPositive(inputDb - upper));
        const auto gate = juce::jlimit(0.0f, 1.0f, (inputDb + 96.0f) / 18.0f);
        return amount * (boost * gate * gate * (3.0f - 2.0f * gate) - reduction);
    }

    void process(juce::dsp::AudioBlock<float> block, const Parameters& parameters) noexcept
    {
        juce::ScopedNoDenormals noDenormals;
        const auto channels = block.getNumChannels();
        const auto samples = block.getNumSamples();
        if (channels == 0 || samples == 0)
            return;

        for (size_t i = 0; i < baseSmoothers.size(); ++i)
        {
            const auto value = sanitise(i, parameters.controls[i].baseValue);
            if (! initialised)
                baseSmoothers[i].setCurrentAndTargetValue(value);
            else
                baseSmoothers[i].setTargetValue(value);
            auto& route = routes[i];
            const auto& provider = parameters.controls[i];
            const bool routed = provider.lfoSignal != nullptr;
            const auto routeDepth = std::isfinite(provider.modulationDepth)
                ? juce::jlimit(-1.0f, 1.0f, provider.modulationDepth) : 0.0f;
            if (initialised && (route.routed != routed
                || (routed && (route.source != parameters.sources[i]
                    || ! juce::exactlyEqual(route.depth, routeDepth)
                    || route.bipolar != provider.isBipolar))))
            {
                route.anchor = route.lastValue;
                route.blend.setCurrentAndTargetValue(0.0f);
                route.blend.setTargetValue(1.0f);
            }
            route.routed = routed;
            route.source = parameters.sources[i];
            route.depth = routeDepth;
            route.bipolar = provider.isBipolar;
        }
        if (! initialised)
            enableSmoother.setCurrentAndTargetValue(parameters.enabled ? 1.0f : 0.0f);
        else
            enableSmoother.setTargetValue(parameters.enabled ? 1.0f : 0.0f);
        initialised = true;

        for (size_t sample = 0; sample < samples; ++sample)
        {
            std::array<float, controlCount> values;
            for (size_t i = 0; i < values.size(); ++i)
            {
                values[i] = sanitise(i, parameters.controls[i].get(static_cast<int>(sample), baseSmoothers[i].getNextValue()));
                auto& route = routes[i];
                const auto blend = route.blend.getNextValue();
                values[i] = route.anchor + blend * (values[i] - route.anchor);
                route.lastValue = values[i];
            }
            updateCoefficients(values[timeScale]);
            float peak = 0.0f;
            for (size_t channel = 0; channel < channels; ++channel)
            {
                auto& input = block.getChannelPointer(channel)[sample];
                if (! std::isfinite(input))
                    input = 0.0f;
                peak = juce::jmax(peak, std::abs(input));
            }
            // Follow both channels with one detector and one applied gain.
            const auto coefficient = peak > envelope ? attack : release;
            envelope = coefficient * envelope + (1.0f - coefficient) * peak;
            if (! std::isfinite(envelope))
                envelope = peak;
            if (envelope < 1.0e-12f)
                envelope = 0.0f;

            const auto wet = enableSmoother.getNextValue() * values[mix];
            if (wet <= 0.0f)
            {
                gainChangeDb = 0.0f;
                dynamicsActivityDb = 0.0f;
                continue; // Stable bypass and zero mix are exactly transparent.
            }
            const auto db = juce::Decibels::gainToDecibels(envelope, -120.0f);
            const auto dynamics = gainForLevel(db, values[upward], values[downward], values[depth]);
            // A wet-weighted activity signal for UI direction/intensity. It
            // deliberately excludes output trim and is not a gain recipe.
            dynamicsActivityDb = dynamics * wet;
            const auto wetGain = juce::Decibels::decibelsToGain(dynamics + values[output]);
            const auto gain = 1.0f + wet * (wetGain - 1.0f);
            gainChangeDb = juce::Decibels::gainToDecibels(gain, -120.0f);
            for (size_t channel = 0; channel < channels; ++channel)
            {
                auto& input = block.getChannelPointer(channel)[sample];
                const auto result = input * gain;
                input = std::isfinite(result) ? result : 0.0f;
            }
        }
        levelDb = juce::Decibels::gainToDecibels(envelope, -120.0f);
    }

    float getInputLevelDb() const noexcept { return levelDb; }
    float getGainChangeDb() const noexcept { return gainChangeDb; }
    float getDynamicsActivityDb() const noexcept { return dynamicsActivityDb; }

private:
    static float sanitise(size_t control, float value) noexcept
    {
        return std::isfinite(value) ? juce::jlimit(minimums[control], maximums[control], value) : defaults[control];
    }
    static float softPositive(float value) noexcept
    {
        if (value <= -3.0f) return 0.0f;
        if (value >= 3.0f) return value;
        return (value + 3.0f) * (value + 3.0f) / 12.0f;
    }
    void updateCoefficients(float timePercent) noexcept
    {
        if (juce::exactlyEqual(timePercent, lastTime))
            return;
        lastTime = timePercent;
        const auto factor = static_cast<double>(timePercent) * 0.01;
        attack = static_cast<float>(std::exp(-1.0 / (sampleRate * 0.005 * factor)));
        release = static_cast<float>(std::exp(-1.0 / (sampleRate * 0.100 * factor)));
    }

    double sampleRate = 48000.0;
    struct Route
    {
        juce::SmoothedValue<float> blend;
        float anchor = 0.0f, lastValue = 0.0f, depth = 0.0f;
        int source = -1;
        bool routed = false, bipolar = true;
    };
    std::array<Route, controlCount> routes;
    std::array<juce::SmoothedValue<float>, controlCount> baseSmoothers;
    juce::SmoothedValue<float> enableSmoother;
    float envelope = 0.0f, levelDb = -120.0f, gainChangeDb = 0.0f;
    float dynamicsActivityDb = 0.0f;
    float attack = 0.0f, release = 0.0f, lastTime = -1.0f;
    bool initialised = false;
};
