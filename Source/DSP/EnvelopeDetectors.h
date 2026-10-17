#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Utility/EnvelopeParameters.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace fire::dsp
{
// Four independent detector paths. Their taps are taken from the raw input,
// before any audible band effects, so a modulation route cannot feed itself.
// All storage is fixed; prepare is the only lifecycle operation required.
class EnvelopeDetectors
{
public:
    static constexpr size_t count = fire::envelope_params::count;
    using Configuration = fire::envelope_params::Configuration;
    using Configurations = fire::envelope_params::Bank;
    struct StereoFrame
    {
        float left = 0, right = 0;
        bool stereo = false;
    };
    struct Result
    {
        std::array<StereoFrame, count> filtered {};
        std::array<float, count> levels {};
    };

    void prepare(double rate) noexcept
    {
        sampleRate = std::isfinite(rate) && rate > 0 ? rate : 48000;
        for (auto& band : bands) { band.high.prepare(sampleRate); band.low.prepare(sampleRate); }
        for (auto& detector : detectors)
        {
            detector.high.prepare(sampleRate); detector.low.prepare(sampleRate);
            detector.highMix.reset(sampleRate, .02); detector.lowMix.reset(sampleRate, .02);
            for (auto& weight : detector.weights) weight.reset(sampleRate, .02);
        }
        reset();
    }

    void reset() noexcept
    {
        primed = false;
        for (auto& band : bands) { band.high.reset(); band.low.reset(); }
        for (auto& detector : detectors)
        {
            detector.high.reset(); detector.low.reset(); detector.level = 0;
            detector.highMix.setCurrentAndTargetValue(0);
            detector.lowMix.setCurrentAndTargetValue(0);
            for (auto& weight : detector.weights) weight.setCurrentAndTargetValue(0);
        }
    }

    void setParameters(const Configurations& configuration, int activeBands,
                       const std::array<float, 3>& crossovers) noexcept
    {
        bandCount = juce::jlimit(1, 4, activeBands);
        const auto cutoff = [&](float value, double fallback)
        { return juce::jlimit(std::min(5.0, sampleRate * .475), sampleRate * .475,
            std::isfinite(value) ? static_cast<double>(value) : fallback); };
        std::array<double, 3> frequencies {};
        for (size_t i = 0; i < frequencies.size(); ++i)
            frequencies[i] = cutoff(crossovers[i], std::array<double, 3>{200, 2000, 8000}[i]);
        std::sort(frequencies.begin(), frequencies.end());
        for (size_t i = 0; i < bands.size(); ++i)
        {
            bands[i].high.setFrequency(i > 0 ? frequencies[i - 1] : 20, !primed);
            bands[i].low.setFrequency(i < frequencies.size() ? frequencies[i] : sampleRate * .475, !primed);
        }
        for (size_t i = 0; i < count; ++i)
        {
            const auto& settings = configuration[i];
            auto& detector = detectors[i];
            const auto finite = [](float value, float fallback, float low, float high)
            { return juce::jlimit(low, high, std::isfinite(value) ? value : fallback); };
            const auto attack = finite(settings.attack, 10, .1f, 500);
            const auto release = finite(settings.release, 200, 5, 2000);
            const auto sensitivity = finite(settings.sensitivity, 0, -24, 24);
            if (!primed || attack != detector.attackTime)
                detector.attack = std::exp(-1.0 / (sampleRate * attack * .001));
            if (!primed || release != detector.releaseTime)
                detector.release = std::exp(-1.0 / (sampleRate * release * .001));
            if (!primed || sensitivity != detector.sensitivityDb)
                detector.sensitivity = std::pow(10.0f, sensitivity * .05f);
            detector.attackTime = attack; detector.releaseTime = release; detector.sensitivityDb = sensitivity;
            detector.high.setFrequency(cutoff(settings.highPassFrequency, 80), !primed);
            detector.low.setFrequency(cutoff(settings.lowPassFrequency, 12000), !primed);
            const auto target = [&](auto& smoother, auto value)
            { if (!primed) smoother.setCurrentAndTargetValue(value); else smoother.setTargetValue(value); };
            target(detector.highMix, settings.highPassEnabled ? 1.0f : 0.0f);
            target(detector.lowMix, settings.lowPassEnabled ? 1.0f : 0.0f);
            const auto source = juce::jlimit(0, 5, settings.source);
            for (size_t j = 0; j < detector.weights.size(); ++j)
                target(detector.weights[j], static_cast<int>(j) == source ? 1.0f : 0.0f);
        }
        primed = true;
    }

    Result next(StereoFrame main, StereoFrame external) noexcept
    {
        main = clean(main); external = clean(external);
        std::array<StereoFrame, 6> sources { main, external };
        bool needBands = false;
        for (const auto& detector : detectors)
            for (size_t i = 2; i < detector.weights.size(); ++i)
                needBands |= detector.weights[i].getCurrentValue() > 0 || detector.weights[i].getTargetValue() > 0;
        if (needBands)
            for (int band = 0; band < bandCount; ++band)
            {
                auto value = main;
                auto& filters = bands[static_cast<size_t>(band)];
                if (band > 0) value = filters.high.process(value, true, true);
                if (band + 1 < bandCount) value = filters.low.process(value, false, true);
                sources[static_cast<size_t>(band + 2)] = value;
            }
        Result result;
        for (size_t i = 0; i < count; ++i)
        {
            auto& detector = detectors[i];
            StereoFrame signal;
            double left = 0, right = 0;
            for (size_t j = 0; j < sources.size(); ++j)
            {
                const auto weight = detector.weights[j].getNextValue();
                if (weight == 0) continue;
                left += static_cast<double>(sources[j].left) * weight;
                right += static_cast<double>(sources[j].right) * weight;
                signal.stereo |= sources[j].stereo;
            }
            signal.left = safeFloat(left); signal.right = safeFloat(right);
            signal = applyFilter(signal, detector.high, detector.highMix, true);
            signal = applyFilter(signal, detector.low, detector.lowMix, false);
            result.filtered[i] = signal;
            const double l = signal.left, r = signal.stereo ? signal.right : l;
            const double input = std::min(16.0, std::sqrt(signal.stereo ? (l * l + r * r) * .5 : l * l));
            const double coefficient = input > detector.level ? detector.attack : detector.release;
            detector.level = input + coefficient * (detector.level - input);
            if (detector.level < 1.0e-20) detector.level = 0;
            result.levels[i] = juce::jlimit(0.0f, 1.0f, static_cast<float>(detector.level) * detector.sensitivity);
        }
        return result;
    }

private:
    static float safeFloat(double value) noexcept
    {
        return std::isfinite(value) ? static_cast<float>(juce::jlimit(-static_cast<double>(std::numeric_limits<float>::max()),
            static_cast<double>(std::numeric_limits<float>::max()), value)) : 0;
    }
    static StereoFrame clean(StereoFrame frame) noexcept
    {
        if (!std::isfinite(frame.left)) frame.left = 0;
        if (!frame.stereo) frame.right = frame.left;
        else if (!std::isfinite(frame.right)) frame.right = frame.left;
        return frame;
    }
    // A topology-preserving state-variable Butterworth stage. Two cascaded
    // stages produce LR24 band taps. Interpolating the integrator coefficient
    // keeps cutoff automation stable without coefficient allocation or tan()
    // work inside the sample loop.
    struct Filter
    {
        struct State { double integrator1 = 0, integrator2 = 0; };
        std::array<std::array<State, 2>, 2> state {};
        juce::SmoothedValue<double> coefficient;
        double rate = 48000, frequency = -1;
        void prepare(double value) noexcept { rate = value; coefficient.reset(rate, .02); reset(); }
        void reset() noexcept { state = {}; frequency = -1; }
        void setFrequency(double value, bool immediate) noexcept
        {
            if (value == frequency && !immediate) return;
            frequency = value;
            const auto g = std::tan(juce::MathConstants<double>::pi * value / rate);
            if (immediate) coefficient.setCurrentAndTargetValue(g); else coefficient.setTargetValue(g);
        }
        StereoFrame process(StereoFrame input, bool highpass, bool fourthOrder) noexcept
        {
            const auto g = coefficient.getNextValue();
            constexpr double damping = 1.4142135623730950488;
            const auto a = 1.0 / (1.0 + g * (g + damping));
            StereoFrame output = input;
            for (size_t channel = 0; channel < 2; ++channel)
            {
                double value = juce::jlimit(-1.0e12, 1.0e12, static_cast<double>(channel == 0 ? input.left : input.right));
                for (size_t stage = 0; stage < (fourthOrder ? 2u : 1u); ++stage)
                {
                    auto& memory = state[channel][stage];
                    const auto v3 = value - memory.integrator2;
                    const auto v1 = a * memory.integrator1 + g * a * v3;
                    const auto v2 = memory.integrator2 + g * v1;
                    memory.integrator1 = 2 * v1 - memory.integrator1;
                    memory.integrator2 = 2 * v2 - memory.integrator2;
                    value = highpass ? value - damping * v1 - v2 : v2;
                    if (!std::isfinite(value)) { memory = {}; value = 0; }
                    if (std::abs(memory.integrator1) < 1.0e-30) memory.integrator1 = 0;
                    if (std::abs(memory.integrator2) < 1.0e-30) memory.integrator2 = 0;
                }
                if (channel == 0) output.left = safeFloat(value); else output.right = safeFloat(value);
            }
            return output;
        }
    };
    static StereoFrame applyFilter(StereoFrame input, Filter& filter, juce::SmoothedValue<float>& mix, bool highpass) noexcept
    {
        const auto amount = mix.getNextValue();
        if (amount == 0 && !mix.isSmoothing()) return input;
        const auto filtered = filter.process(input, highpass, false);
        if (amount == 1) return filtered;
        input.left = safeFloat(static_cast<double>(input.left) + amount * (static_cast<double>(filtered.left) - input.left));
        input.right = safeFloat(static_cast<double>(input.right) + amount * (static_cast<double>(filtered.right) - input.right));
        return input;
    }
    struct Detector
    {
        Filter high, low;
        juce::SmoothedValue<float> highMix, lowMix;
        std::array<juce::SmoothedValue<float>, 6> weights;
        double level = 0, attack = 0, release = 0;
        float sensitivity = 1;
        float attackTime = 10, releaseTime = 200, sensitivityDb = 0;
    };
    struct Band { Filter high, low; };
    std::array<Detector, count> detectors;
    std::array<Band, count> bands;
    double sampleRate = 48000;
    int bandCount = 1;
    bool primed = false;
};
}
