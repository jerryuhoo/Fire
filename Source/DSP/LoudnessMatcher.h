#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace fire::dsp
{
// A finite comparison of two aligned mono/stereo signals, owned by the audio
// thread. This uses K-weighting with paired absolute gating, rather than the
// overlapping/relative-gated integration of a complete LUFS or EBU R128 meter.
// No method allocates, locks, changes the supplied audio, or applies the gain.
class LoudnessMatcher
{
public:
    enum class State { idle, measuring, complete, noSignal };

    LoudnessMatcher() noexcept { prepare(defaultSampleRate); }

    void prepare(double sampleRate) noexcept
    {
        // Keep the shelf below Nyquist and sample counters bounded, including
        // when a host supplies NaN, an uninitialised rate, or an extreme value.
        const double rate = std::isfinite(sampleRate)
                                && sampleRate >= 8000.0 && sampleRate <= 768000.0
                            ? sampleRate : defaultSampleRate;
        frameLength = static_cast<std::uint64_t>(std::llround(rate * 0.1));
        measurementLength = static_cast<std::uint64_t>(std::llround(rate * 3.0));
        minimumValidSamples = frameLength * 4;

        // BS.1770 K-weighting design constants; independently evaluated for
        // this sample rate as two biquads. Reference implementation/formulas:
        // https://github.com/jiixyj/libebur128/blob/master/ebur128/ebur128.c
        // (ebur128_init_filter). The RLB numerator intentionally has unity b0.
        constexpr double pi = 3.1415926535897932384626433832795;
        constexpr double shelfFrequency = 1681.974450955533;
        constexpr double shelfQ = 0.7071752369554196;
        const double highGain = std::pow(10.0, 3.999843853973347 / 20.0);
        const double middleGain = std::pow(highGain, 0.4996667741545416);
        const double shelfWarp = std::tan(pi * shelfFrequency / rate);
        const double shelfSquare = shelfWarp * shelfWarp;
        const double shelfWidth = shelfWarp / shelfQ;
        const double shelfDenominator = 1.0 + shelfWidth + shelfSquare;
        shelf = {
            { (highGain + middleGain * shelfWidth + shelfSquare) / shelfDenominator,
              2.0 * (shelfSquare - highGain) / shelfDenominator,
              (highGain - middleGain * shelfWidth + shelfSquare) / shelfDenominator },
            { 2.0 * (shelfSquare - 1.0) / shelfDenominator,
              (1.0 - shelfWidth + shelfSquare) / shelfDenominator }
        };

        const double rolloffWarp = std::tan(pi * 38.13547087602444 / rate);
        const double rolloffSquare = rolloffWarp * rolloffWarp;
        const double rolloffWidth = rolloffWarp / 0.5003270373238773;
        const double rolloffDenominator = 1.0 + rolloffWidth + rolloffSquare;
        rolloff = {
            { 1.0, -2.0, 1.0 },
            { 2.0 * (rolloffSquare - 1.0) / rolloffDenominator,
              (1.0 - rolloffWidth + rolloffSquare) / rolloffDenominator }
        };
        cancel();
    }

    void start() noexcept
    {
        clearMeasurement();
        state = State::measuring;
    }

    void cancel() noexcept
    {
        clearMeasurement();
        state = State::idle;
    }

    void process(const juce::AudioBuffer<float>& reference,
                 const juce::AudioBuffer<float>& processed) noexcept
    {
        if (state != State::measuring)
            return;

        const juce::ScopedNoDenormals noDenormals;
        const auto count = std::min(reference.getNumSamples(), processed.getNumSamples());
        std::array<const float*, 2> referenceChannels {}, processedChannels {};
        for (int channel = 0; channel < std::min(2, reference.getNumChannels()); ++channel)
            referenceChannels[static_cast<size_t>(channel)] = reference.getReadPointer(channel);
        for (int channel = 0; channel < std::min(2, processed.getNumChannels()); ++channel)
            processedChannels[static_cast<size_t>(channel)] = processed.getReadPointer(channel);

        for (int sample = 0; sample < count && state == State::measuring; ++sample)
        {
            std::array<double, 2> energy {};
            for (size_t channel = 0; channel < referenceChannels.size(); ++channel)
            {
                const auto referenceValue = referenceChannels[channel] != nullptr
                    ? referenceChannels[channel][sample] : 0.0f;
                const auto processedValue = processedChannels[channel] != nullptr
                    ? processedChannels[channel][sample] : 0.0f;
                const auto weightedReference = weight(referenceValue, filters[0][channel]);
                const auto weightedProcessed = weight(processedValue, filters[1][channel]);
                // Sum channel energies, never waveforms: anti-phase stereo is
                // just as loud here as the corresponding in-phase signal.
                energy[0] += weightedReference * weightedReference;
                energy[1] += weightedProcessed * weightedProcessed;
            }
            ++elapsedSamples;

            // The initial 100 ms warms the filters within the three-second
            // window. Frames after that use identical time spans on both sides.
            if (elapsedSamples > frameLength)
            {
                frameEnergy[0] += energy[0];
                frameEnergy[1] += energy[1];
                if (++samplesInFrame == frameLength)
                    finishFrame();
            }
            if (elapsedSamples >= measurementLength)
                finishMeasurement();
        }
    }

    State getState() const noexcept { return state; }
    float getProgress() const noexcept
    {
        return state == State::idle ? 0.0f
            : static_cast<float>(static_cast<double>(elapsedSamples)
                                 / static_cast<double>(measurementLength));
    }
    float getGainDb() const noexcept { return gainDb; }
    bool isLimited() const noexcept { return limited; }

private:
    struct Coefficients
    {
        std::array<double, 3> numerator {};
        std::array<double, 2> denominator {};
    };
    struct BiquadState
    {
        double first = 0.0, second = 0.0;
        double process(double input, const Coefficients& coefficients) noexcept
        {
            const auto result = coefficients.numerator[0] * input + first;
            first = coefficients.numerator[1] * input
                    - coefficients.denominator[0] * result + second;
            second = coefficients.numerator[2] * input
                     - coefficients.denominator[1] * result;
            if (! std::isfinite(result) || ! std::isfinite(first) || ! std::isfinite(second))
            {
                first = second = 0.0;
                return 0.0;
            }
            return result;
        }
    };
    struct FilterState
    {
        BiquadState shelf, rolloff;
    };
    double weight(float input, FilterState& filter) const noexcept
    {
        const auto value = std::isfinite(input) ? static_cast<double>(input) : 0.0;
        return filter.rolloff.process(filter.shelf.process(value, shelf), rolloff);
    }
    void clearMeasurement() noexcept
    {
        filters = {};
        frameEnergy = {};
        accumulatedEnergy = {};
        elapsedSamples = samplesInFrame = validSamples = 0;
        gainDb = 0.0f;
        limited = false;
    }
    void finishFrame() noexcept
    {
        // -70 dB of summed K-weighted mean-square energy. Pair the gate so
        // silence or a muted output cannot become an extreme gain estimate.
        const auto threshold = minimumFrameEnergy * static_cast<double>(samplesInFrame);
        if (frameEnergy[0] >= threshold && frameEnergy[1] >= threshold
            && std::isfinite(frameEnergy[0]) && std::isfinite(frameEnergy[1]))
        {
            accumulatedEnergy[0] += frameEnergy[0];
            accumulatedEnergy[1] += frameEnergy[1];
            validSamples += samplesInFrame;
        }
        frameEnergy = {};
        samplesInFrame = 0;
    }
    void finishMeasurement() noexcept
    {
        // A final partial frame at an unusual fractional sample rate is
        // discarded; four complete shared frames are required for a result.
        state = State::noSignal;
        if (validSamples < minimumValidSamples
            || accumulatedEnergy[0] <= 0.0 || accumulatedEnergy[1] <= 0.0)
            return;

        const auto correction = 10.0 * (std::log10(accumulatedEnergy[0])
                                       - std::log10(accumulatedEnergy[1]));
        if (! std::isfinite(correction))
            return;
        limited = correction < -maximumGainDb || correction > maximumGainDb;
        gainDb = static_cast<float>(std::clamp(correction, -maximumGainDb, maximumGainDb));
        state = State::complete;
    }

    static constexpr double defaultSampleRate = 48000.0;
    static constexpr double minimumFrameEnergy = 1.0e-7;
    static constexpr double maximumGainDb = 18.0;
    Coefficients shelf, rolloff;
    std::array<std::array<FilterState, 2>, 2> filters {};
    std::array<double, 2> frameEnergy {}, accumulatedEnergy {};
    std::uint64_t frameLength = 4800, measurementLength = 144000;
    std::uint64_t minimumValidSamples = 19200;
    std::uint64_t elapsedSamples = 0, samplesInFrame = 0, validSamples = 0;
    State state = State::idle;
    float gainDb = 0.0f;
    bool limited = false;
};
}
