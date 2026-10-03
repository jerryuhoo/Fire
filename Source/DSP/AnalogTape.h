#pragma once

#include "AnalogMagnetics.h"
#include <map>
#include <memory>
#include <mutex>

namespace fire::analog
{
class Tape
{
public:
    struct Settings
    {
        double speed = .381;          // metres/second: 15 ips
        double gap = 2.5e-6;          // playback gap, metres
        double spacing = .6e-6;       // tape/head spacing, metres
        double coating = 3.5e-6;      // active magnetic coating, metres
        double bias = 1.25;           // normalised recording field
    };

    void prepare(double rate) { prepare(rate, Settings{}); }
    void prepare(double rate, Settings settings)
    {
        baseRate = std::isfinite(rate) && rate > 0 ? rate : 48000;
        int multiplier = 1;
        while (baseRate * multiplier < 700000 && multiplier < 16) multiplier *= 2;
        internalRate = baseRate * multiplier;
        const auto desiredBias = std::min(55000.0, internalRate * .08);
        const int cycles = std::clamp(static_cast<int>(std::round(desiredBias / internalRate * period)), 1, period / 8);
        reference = acquireBias(cycles, std::clamp(settings.bias, 0.0, 4.0));
        actualBiasFrequency = internalRate * cycles / period;
        signal.configure(material);
        const auto speed = std::clamp(settings.speed, .05, 1.0);
        gapSamples = std::clamp(internalRate * std::clamp(settings.gap, 0.5e-6, 10e-6) / speed, 1.0, 30.0);
        gapWhole = static_cast<int>(gapSamples);
        gapFraction = gapSamples - gapWhole;
        // Match the -3 dB points of exp(-k d) and (1-exp(-k delta))/(k delta).
        // The rectangular gap average supplies the sinc playback-gap loss.
        spacingPole = pole(.34657359028 * speed / (2 * pi * std::clamp(settings.spacing, .05e-6, 10e-6)));
        thicknessPole = pole(.73 * speed / (2 * pi * std::clamp(settings.coating, .5e-6, 20e-6)));
        recordPole = pole(3180);
        const auto recordDecay = 1 - recordPole;
        playbackPole = 1 - 1.5 * recordDecay / (1 + .5 * recordDecay);
        constexpr std::array q{.5097955791, .6013448869, .8999762231, 2.5629154477};
        for (size_t index = 0; index < antialias.size(); ++index)
            antialias[index].prepare(internalRate, std::min(20000.0, baseRate * .42), q[index]);
        setProcessingRate(baseRate);
        reset();
    }

    void setProcessingRate(double rate) noexcept
    {
        const auto safe = std::isfinite(rate) && rate > 0 ? rate : baseRate;
        subdivisions = std::clamp(static_cast<int>(std::round(internalRate / safe)), 1, 16);
    }

    void reset() noexcept
    {
        phase = 0;
        previousInput = recordLow = playbackLow = spacingLow = thicknessLow = gapSum = 0;
        gapPosition = 0;
        gapBuffer.fill(0);
        excited = false;
        signal.reset();
        if (reference) signal.commit(reference->states.back());
        for (auto& filter : antialias) filter.reset();
    }

    double process(double input) noexcept
    {
        if (! reference) return 0;
        input = std::isfinite(input) ? std::clamp(input, -32.0, 32.0) : 0.0;
        if (! excited && input == 0)
        {
            phase = (phase + subdivisions) & (period - 1);
            signal.commit(reference->states[static_cast<size_t>((phase - 1) & (period - 1))]);
            return 0;
        }
        excited = true;
        double output = 0;
        for (int step = 0; step < subdivisions; ++step)
        {
            const auto interpolated = previousInput + (input - previousInput) * (step + 1) / subdivisions;
            recordLow += recordPole * (interpolated - recordLow);
            const auto recordVoltage = interpolated + .5 * (interpolated - recordLow);
            const auto& biasOnly = reference->states[static_cast<size_t>(phase)];
            // The bias-only path removes RF feedthrough and its start-up transient.
            // Its steady periodic JA solution is immutable and shared. Audio's
            // irreversible magnetisation remains live and is never looked up.
            output = 1.2 * (signal.processMagnetisation(recordVoltage + biasOnly.field) - biasOnly.magnetisation);
            phase = (phase + 1) & (period - 1);
            const int oldest = (gapPosition - gapWhole) & 31;
            gapSum += output - gapBuffer[static_cast<size_t>(oldest)];
            gapBuffer[static_cast<size_t>(gapPosition)] = output;
            output = (gapSum + gapFraction * gapBuffer[static_cast<size_t>(oldest)]) / gapSamples;
            gapPosition = (gapPosition + 1) & 31;
            spacingLow += spacingPole * (output - spacingLow);
            thicknessLow += thicknessPole * (spacingLow - thicknessLow);
            playbackLow += playbackPole * (thicknessLow - playbackLow);
            // Reciprocal record/playback shelving keeps the small-signal EQ
            // flat; magnetic overload and physical head losses are retained.
            output = thicknessLow - (1.0 / 3) * (thicknessLow - playbackLow);
            for (auto& filter : antialias) output = filter.process(output);
        }
        previousInput = input;
        return output;
    }

    double getBiasFrequency() const noexcept { return actualBiasFrequency; }
    double getInternalSampleRate() const noexcept { return internalRate; }
    double getMagnetisation() const noexcept { return signal.getState().magnetisation; }

private:
    static constexpr int period = 2048;
    static constexpr double pi = 3.14159265358979323846;
    // Uncoupled-domain JA material: retain reversible/irreversible motion,
    // pinning and remanence, while omitting weak inter-domain coupling so many
    // instances remain practical. Transformer retains its mean-field coupling.
    // These are normalised generic material parameters, not a fitted machine.
    static constexpr JilesAtherton::Material material{.18, .22, .17, 0};
    struct BiasReference { std::array<JilesAtherton::State, period> states; };

    static std::shared_ptr<const BiasReference> acquireBias(int cycles, double amplitude)
    {
        using Key = std::pair<int, double>;
        static std::mutex mutex;
        static std::map<Key, std::weak_ptr<const BiasReference>> cache;
        const std::lock_guard lock(mutex); // prepare only; never called by process/setProfile.
        const Key key{cycles, amplitude};
        if (const auto found = cache.find(key); found != cache.end())
            if (auto value = found->second.lock()) return value;
        for (auto it = cache.begin(); it != cache.end();)
            if (it->second.expired()) it = cache.erase(it); else ++it;
        auto result = std::make_shared<BiasReference>();
        JilesAtherton referenceCore;
        referenceCore.configure(material);
        for (int cycle = 0; cycle < 8; ++cycle)
            for (int step = 0; step < period; ++step)
            {
                const auto field = amplitude * std::sin(2 * pi * cycles * step / period);
                referenceCore.processMagnetisation(field);
                if (cycle == 7) result->states[static_cast<size_t>(step)] = referenceCore.getState();
            }
        cache[key] = result;
        return result;
    }

    struct Lowpass
    {
        void prepare(double rate, double frequency, double q) noexcept
        {
            const auto k = std::tan(pi * std::min(frequency, rate * .45) / rate);
            const auto norm = 1 / (1 + k / q + k * k);
            b0 = k * k * norm; b1 = 2 * b0; b2 = b0;
            a1 = 2 * (k * k - 1) * norm; a2 = (1 - k / q + k * k) * norm;
        }
        void reset() noexcept { z1 = z2 = 0; }
        double process(double input) noexcept
        {
            const auto output = b0 * input + z1;
            z1 = b1 * input - a1 * output + z2;
            z2 = b2 * input - a2 * output;
            return output;
        }
        double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    };

    double pole(double frequency) const noexcept { return -std::expm1(-2 * pi * std::min(frequency, internalRate * .45) / internalRate); }
    JilesAtherton signal;
    std::shared_ptr<const BiasReference> reference;
    std::array<Lowpass, 4> antialias;
    std::array<double, 32> gapBuffer{};
    double baseRate = 48000, internalRate = 768000, actualBiasFrequency = 55000;
    double previousInput = 0, recordLow = 0, playbackLow = 0, spacingLow = 0, thicknessLow = 0;
    double recordPole = 0, playbackPole = 0, spacingPole = 0, thicknessPole = 0;
    double gapSamples = 1, gapFraction = 0, gapSum = 0;
    int phase = 0, subdivisions = 16, gapPosition = 0, gapWhole = 1;
    bool excited = false;
};
}
