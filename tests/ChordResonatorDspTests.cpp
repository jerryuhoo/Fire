#include <DSP/ChordResonator.h>
#include <DSP/InsertEffect.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <complex>
#include <cstdint>
#include <limits>

namespace
{
using fire::chord_resonator::Engine;
using fire::chord_resonator::Parameters;
using fire::effects::InsertEffect;
using fire::effects::Type;
constexpr double pi2 = juce::MathConstants<double>::twoPi;

double frequency(double midi) { return 440.0 * std::exp2((midi - 69.0) / 12.0); }

float noise(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int>(state >> 8) - 8388608) * (0.15f / 8388608.0f);
}

std::vector<float> impulseResponse(const Parameters& parameters, int samples = 48000)
{
    Engine engine;
    engine.prepare(48000.0);
    std::vector<float> result(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i)
    {
        float left = i == 0 ? 0.2f : 0.0f, right = left;
        engine.process(left, right, parameters, false);
        result[static_cast<size_t>(i)] = left;
    }
    return result;
}

double magnitudeAt(const std::vector<float>& samples, double hz)
{
    std::complex<double> sum {};
    const auto step = std::polar(1.0, -pi2 * hz / 48000.0);
    std::complex<double> phase {1.0, 0.0};
    for (auto sample : samples) { sum += static_cast<double>(sample) * phase; phase *= step; }
    return std::abs(sum);
}

double rms(const std::vector<float>& samples, int start, int count)
{
    double energy = 0.0;
    for (int i = start; i < start + count; ++i)
    {
        const auto sample = samples[static_cast<size_t>(i)];
        energy += static_cast<double>(sample) * sample;
    }
    return std::sqrt(energy / count);
}

juce::AudioBuffer<float> renderInsert(const juce::AudioBuffer<float>& input,
                                     const InsertEffect::Parameters& parameters, int blockSize)
{
    InsertEffect effect;
    effect.prepare({48000.0, static_cast<juce::uint32>(blockSize), static_cast<juce::uint32>(input.getNumChannels())});
    juce::AudioBuffer<float> output;
    output.makeCopyOf(input);
    const std::array<int, fire::effects::controlCount> sources {{0, 1, 2, 3, -1, -1}};
    for (int offset = 0; offset < output.getNumSamples(); offset += blockSize)
        effect.process(juce::dsp::AudioBlock<float>(output).getSubBlock(static_cast<size_t>(offset),
            static_cast<size_t>(std::min(blockSize, output.getNumSamples() - offset))), parameters, offset, &sources);
    return output;
}

float difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float result = 0.0f;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int i = 0; i < a.getNumSamples(); ++i)
            result = std::max(result, std::abs(a.getSample(channel, i) - b.getSample(channel, i)));
    return result;
}
}

TEST_CASE("Chord Resonator places distinct major and minor resonances at musical pitches",
          "[chord-resonator][insertfx][dsp][frequency]")
{
    Parameters parameters;
    parameters.root = 60; parameters.color = 0; parameters.width = 0;
    parameters.chord = 0;
    const auto major = impulseResponse(parameters);
    parameters.chord = 1;
    const auto minor = impulseResponse(parameters);
    CHECK(magnitudeAt(major, frequency(64)) > 3.0 * magnitudeAt(major, frequency(63)));
    CHECK(magnitudeAt(minor, frequency(63)) > 3.0 * magnitudeAt(minor, frequency(64)));
    for (double note : {60.0, 67.0})
    {
        CAPTURE(note);
        CHECK(magnitudeAt(major, frequency(note)) > 2.0 * magnitudeAt(major, frequency(note + 0.75)));
        CHECK(magnitudeAt(minor, frequency(note)) > 2.0 * magnitudeAt(minor, frequency(note + 0.75)));
    }
    parameters.root = 48; parameters.chord = 7; parameters.color = 0;
    const auto dark = impulseResponse(parameters);
    parameters.color = 1;
    const auto bright = impulseResponse(parameters);
    CHECK(magnitudeAt(bright, 3.0 * frequency(48)) > 4.0 * magnitudeAt(dark, 3.0 * frequency(48)));
}

TEST_CASE("Chord Resonator noise energy remains audible when decay is extended",
          "[chord-resonator][insertfx][dsp][level]")
{
    std::array<double, 3> gains {};
    constexpr std::array<float, 3> decays {{0.1f, 0.6f, 3.0f}};
    for (size_t setting = 0; setting < decays.size(); ++setting)
    {
        Engine engine;
        engine.prepare(16000.0);
        Parameters parameters;
        parameters.decay = decays[setting];
        parameters.width = 0;
        std::uint32_t random = 17;
        double inputEnergy = 0, outputEnergy = 0;
        bool finite = true;
        for (int i = 0; i < 96000; ++i)
        {
            const auto input = noise(random);
            auto left = input, right = input;
            engine.process(left, right, parameters, false);
            finite = finite && std::isfinite(left);
            if (i >= 64000)
            {
                inputEnergy += static_cast<double>(input) * input;
                outputEnergy += static_cast<double>(left) * left;
            }
        }
        gains[setting] = std::sqrt(outputEnergy / inputEnergy);
        CAPTURE(decays[setting], gains[setting]);
        CHECK(finite);
        CHECK(gains[setting] > 0.4);
        CHECK(gains[setting] < 1.8);
    }
    // Decay controls ringing time, not a many-decibel wet-volume fade.
    CHECK(gains[2] / gains[0] > 0.55);
    CHECK(gains[2] / gains[0] < 1.8);
}

TEST_CASE("Chord Resonator decay is RT60 after impulses and protected sustained tones",
          "[chord-resonator][insertfx][dsp][tail]")
{
    for (float decay : {0.1f, 0.6f, 2.0f})
        for (bool sustained : {false, true})
        {
            CAPTURE(decay, sustained);
            constexpr int sampleRate = 48000, noteOff = 24000, window = 1200;
            const auto separation = juce::roundToInt(decay * sampleRate * 0.5f);
            const auto first = noteOff + 4800;
            std::vector<float> output(static_cast<size_t>(first + separation + window));
            Engine engine;
            engine.prepare(sampleRate);
            Parameters parameters;
            parameters.root = 69; parameters.chord = 7; parameters.color = parameters.width = 0;
            parameters.decay = decay;
            float drivenPeak = 0;
            for (int i = 0; i < static_cast<int>(output.size()); ++i)
            {
                float input = 0;
                if (sustained && i < noteOff) input = static_cast<float>(0.1 * std::sin(pi2 * 440.0 * i / sampleRate));
                else if (! sustained && i == noteOff - 1) input = 0.2f;
                float left = input, right = input;
                engine.process(left, right, parameters, false);
                output[static_cast<size_t>(i)] = left;
                if (i < noteOff) drivenPeak = std::max(drivenPeak, std::abs(left));
            }
            const auto initial = rms(output, first, window);
            const auto later = rms(output, first + separation, window);
            REQUIRE(initial > 1.0e-9);
            CHECK(20.0 * std::log10(later / initial) == Catch::Approx(-30.0).margin(0.1));
            if (sustained)
            {
                CHECK(drivenPeak > 0.05f);
                CHECK(drivenPeak < 0.35f); // Nominal +6 dB protection, allowing attack overshoot.
            }
        }
}

TEST_CASE("Chord Resonator LFO retuning and continuous controls preserve callback partitioning",
          "[chord-resonator][insertfx][dsp][lfo][block-size]")
{
    constexpr int samples = 13001;
    juce::AudioBuffer<float> input(2, samples);
    std::uint32_t random = 211;
    std::array<std::vector<float>, 4> lfo;
    for (auto& lane : lfo) lane.resize(samples);
    for (int i = 0; i < samples; ++i)
    {
        const auto value = noise(random);
        input.setSample(0, i, value); input.setSample(1, i, -value);
        lfo[0][static_cast<size_t>(i)] = (i / 17) % 2 == 0 ? 0.0f : 1.0f;
        lfo[1][static_cast<size_t>(i)] = static_cast<float>((i / 31) % 8) / 7.0f;
        lfo[2][static_cast<size_t>(i)] = static_cast<float>(0.5 + 0.5 * std::sin(i * 0.017));
        lfo[3][static_cast<size_t>(i)] = static_cast<float>(0.5 + 0.5 * std::sin(i * 0.011));
    }
    InsertEffect::Parameters parameters(Type::chordResonator);
    parameters.normalised = true;
    for (size_t i = 0; i < parameters.values.size(); ++i)
    {
        auto& provider = parameters.values[i];
        provider.range = {0.0f, 1.0f};
        provider.baseValue = i < lfo.size() ? 0.5f : i == 5 ? 1.0f : 0.7f;
        if (i < lfo.size()) { provider.lfoSignal = lfo[i].data(); provider.modulationDepth = 1.0f; }
    }
    const auto reference = renderInsert(input, parameters, samples);
    CHECK(difference(reference, input) > 0.01f);
    for (int blockSize : {1, 37, 512})
    {
        CAPTURE(blockSize);
        CHECK(difference(reference, renderInsert(input, parameters, blockSize)) < 0.000001f);
    }
    parameters.values[5].baseValue = 0.0f;
    CHECK(difference(input, renderInsert(input, parameters, 127)) == 0.0f);
}

TEST_CASE("Chord Resonator discrete insert selections go directly through the engine retune",
          "[chord-resonator][insertfx][dsp][transition]")
{
    Engine reference;
    reference.prepare(48000.0);
    InsertEffect insert;
    insert.prepare({48000.0, 1, 1});
    Parameters p;
    p.color = p.width = 0; p.chord = 7;
    InsertEffect::Parameters controls(Type::chordResonator);
    controls.values[1].baseValue = 7;
    controls.values[2].baseValue = controls.values[4].baseValue = 0;
    controls.values[5].baseValue = 100;
    juce::AudioBuffer<float> one(1, 1);
    float error = 0;
    for (int i = 0; i < 10000; ++i)
    {
        if (i == 4500)
        {
            p.root = controls.values[0].baseValue = 72;
            p.chord = controls.values[1].baseValue = 3;
        }
        const auto input = i < 2000 ? 0.0f : static_cast<float>(0.1 * std::sin(i * 0.037));
        auto left = input, right = input;
        reference.process(left, right, p, false);
        one.setSample(0, 0, input);
        insert.process(juce::dsp::AudioBlock<float>(one), controls);
        if (i >= 2000) error = std::max(error, std::abs(left - one.getSample(0, 0)));
    }
    CHECK(error < 0.000001f);
}

TEST_CASE("Chord Resonator resets silent and keeps retuning and rate extremes bounded",
          "[chord-resonator][insertfx][dsp][reset][stability]")
{
    for (double rate : {8000.0, 48000.0, 192000.0})
    {
        CAPTURE(rate);
        Engine engine;
        engine.prepare(rate);
        Parameters p;
        std::uint32_t random = 51;
        bool finite = true;
        float peak = 0;
        for (int i = 0; i < 12000; ++i)
        {
            p.root = (i / 29) % 2 == 0 ? 36.0f : 72.0f;
            p.chord = static_cast<float>((i / 43) % 8);
            p.decay = (i / 61) % 2 == 0 ? 0.05f : 3.0f;
            p.color = p.width = (i / 71) % 2 == 0 ? 0.0f : 1.0f;
            auto left = noise(random), right = -left;
            engine.process(left, right, p);
            finite = finite && std::isfinite(left) && std::isfinite(right);
            peak = std::max(peak, std::max(std::abs(left), std::abs(right)));
        }
        CHECK(finite);
        CHECK(peak > 0.0001f);
        CHECK(peak < 0.8f);
        engine.reset();
        float silentPeak = 0;
        p.root = p.chord = p.color = p.decay = p.width = std::numeric_limits<float>::quiet_NaN();
        for (int i = 0; i < 4000; ++i)
        {
            float left = 0, right = 0;
            engine.process(left, right, p);
            silentPeak = std::max(silentPeak, std::max(std::abs(left), std::abs(right)));
        }
        CHECK(silentPeak == 0.0f);
    }

    Engine changed, unchanged;
    changed.prepare(48000.0); unchanged.prepare(48000.0);
    Parameters p;
    for (int i = 0; i < 4000; ++i)
    {
        float left = i == 3900 ? 0.3f : 0.0f, right = left;
        auto otherLeft = left, otherRight = right;
        changed.process(left, right, p);
        unchanged.process(otherLeft, otherRight, p);
    }
    float left = 0, right = 0, otherLeft = 0, otherRight = 0;
    unchanged.process(otherLeft, otherRight, p);
    p.root = 72; p.chord = 0;
    changed.process(left, right, p);
    CHECK(std::abs(left - otherLeft) < 0.0001f);
    CHECK(std::abs(right - otherRight) < 0.0001f);
}

TEST_CASE("Chord Resonator safely reveals hidden harmonic energy and preserves mono width",
          "[chord-resonator][insertfx][dsp][level][stereo]")
{
    Engine engine;
    engine.prepare(48000.0);
    Parameters p;
    p.root = 48; p.chord = 7; p.color = p.width = 0; p.decay = 3.0f;
    float revealedPeak = 0;
    // The third harmonic has a long-lived state even when Color hides it.
    // A fast automation/LFO reveal must not bypass resonant-level protection.
    for (int i = 0; i < 105600; ++i)
    {
        if (i == 96000) p.color = p.width = 1;
        auto left = static_cast<float>(0.1 * std::sin(pi2 * frequency(48) * 3.0 * i / 48000.0));
        auto right = left;
        engine.process(left, right, p);
        if (i >= 96000) revealedPeak = std::max(revealedPeak, std::max(std::abs(left), std::abs(right)));
    }
    CAPTURE(revealedPeak);
    CHECK(revealedPeak > 0.05f);
    CHECK(revealedPeak < 0.4f);

    Engine narrow, wide, stereo;
    narrow.prepare(48000.0); wide.prepare(48000.0); stereo.prepare(48000.0);
    Parameters normal;
    std::uint32_t random = 37;
    float monoDifference = 0, antiPhaseError = 0;
    double stereoDifference = 0;
    for (int i = 0; i < 16000; ++i)
    {
        const auto input = noise(random);
        float a = input, b = input, c = input, d = input, e = input, f = -input;
        normal.width = 0;
        narrow.process(a, b, normal, false);
        stereo.process(e, f, normal);
        antiPhaseError = std::max(antiPhaseError, std::abs(e + f));
        normal.width = 1;
        wide.process(c, d, normal, false);
        monoDifference = std::max(monoDifference, std::abs(a - c));
    }
    CHECK(monoDifference == 0.0f);
    CHECK(antiPhaseError == 0.0f);
    stereo.reset(); normal.width = 1;
    for (int i = 0; i < 16000; ++i)
    {
        float left = noise(random), right = left;
        stereo.process(left, right, normal);
        stereoDifference += std::abs(left - right);
    }
    CHECK(stereoDifference > 1.0);
}

TEST_CASE("Chord Resonator preserves tuning and decay at a 768 kHz host rate",
          "[chord-resonator][insertfx][dsp][sample-rate]")
{
    Engine engine;
    engine.prepare(768000.0);
    Parameters parameters;
    parameters.root = 69; parameters.chord = 7;
    parameters.color = parameters.width = 0; parameters.decay = 0.05f;
    std::vector<float> output(65536);
    std::complex<double> expected {}, wrongRate {};
    std::complex<double> expectedPhase {1.0, 0.0}, wrongPhase {1.0, 0.0};
    const auto expectedStep = std::polar(1.0, -pi2 * 440.0 / 768000.0);
    const auto wrongStep = std::polar(1.0, -pi2 * 7040.0 / 768000.0);
    for (size_t i = 0; i < output.size(); ++i)
    {
        float left = i == 0 ? 0.2f : 0.0f, right = left;
        engine.process(left, right, parameters, false);
        output[i] = left;
        expected += static_cast<double>(left) * expectedPhase;
        wrongRate += static_cast<double>(left) * wrongPhase;
        expectedPhase *= expectedStep; wrongPhase *= wrongStep;
    }
    CHECK(std::abs(expected) > 0.1);
    CHECK(std::abs(expected) > 10.0 * std::abs(wrongRate));
    // 25 ms is exactly eleven cycles of A4, avoiding window-phase ambiguity.
    const auto initial = rms(output, 38400, 3840);
    const auto later = rms(output, 57600, 3840);
    CHECK(20.0 * std::log10(later / initial) == Catch::Approx(-30.0).margin(0.1));
}

TEST_CASE("Chord Resonator rejects biased input without extending its shortest modal tail",
          "[chord-resonator][insertfx][dsp][dc][tail]")
{
    for (bool stereo : {false, true})
    {
        Engine engine;
        engine.prepare(48000.0);
        Parameters parameters;
        parameters.root = 36; parameters.color = parameters.width = 1; parameters.decay = 0.05f;
        float remainingDc = 0;
        for (int i = 0; i < 24000; ++i)
        {
            float left = 0.1f, right = -0.15f;
            engine.process(left, right, parameters, stereo);
            if (i >= 19200) remainingDc = std::max(remainingDc, std::max(std::abs(left), std::abs(right)));
        }
        CAPTURE(stereo, remainingDc);
        CHECK(remainingDc < 0.00001f);
    }
    Parameters parameters;
    parameters.root = 69; parameters.chord = 7; parameters.color = parameters.width = 0; parameters.decay = 0.05f;
    const auto output = impulseResponse(parameters, 8000);
    const auto initial = rms(output, 3600, 600);
    const auto later = rms(output, 4800, 600);
    REQUIRE(initial > 1.0e-8);
    CHECK(20.0 * std::log10(later / initial) == Catch::Approx(-30.0).margin(0.1));
}
