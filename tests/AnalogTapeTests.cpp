#include <DSP/AnalogTape.h>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double pi = 3.14159265358979323846, rate = 48000;
std::vector<double> render(fire::analog::Tape::Settings settings, double amplitude, double frequency)
{
    fire::analog::Tape tape;
    tape.prepare(rate, settings);
    std::vector<double> output;
    for (int sample = 0; sample < 9600; ++sample)
    {
        const auto value = tape.process(amplitude * std::sin(2 * pi * frequency * sample / rate));
        REQUIRE(std::isfinite(value));
        if (sample >= 4800) output.push_back(value);
    }
    return output;
}
double rms(const std::vector<double>& values)
{
    double energy = 0; for (auto value : values) energy += value * value;
    return std::sqrt(energy / static_cast<double>(values.size()));
}
double harmonic(const std::vector<double>& values, double frequency)
{
    double real = 0, imaginary = 0;
    for (size_t sample = 0; sample < values.size(); ++sample)
    {
        const auto angle = 2 * pi * frequency * static_cast<double>(sample) / rate;
        real += values[sample] * std::cos(angle); imaginary += values[sample] * std::sin(angle);
    }
    return std::hypot(real, imaginary);
}
}

TEST_CASE("Tape keeps its ultrasonic bias clock across processing rates and silence is exact",
          "[analog-physical][tape][dsp][silence]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        CAPTURE(sampleRate);
        fire::analog::Tape tape;
        tape.prepare(sampleRate);
        REQUIRE(tape.getBiasFrequency() > 54000); REQUIRE(tape.getBiasFrequency() < 56000);
        const auto internalRate = tape.getInternalSampleRate();
        for (int sample = 0; sample < 2048; ++sample) CHECK(tape.process(0) == 0);
        tape.setProcessingRate(sampleRate * 4);
        CHECK(tape.getInternalSampleRate() == internalRate);
        for (int sample = 0; sample < 2048; ++sample) CHECK(tape.process(0) == 0);
        tape.process(.5);
        for (int sample = 0; sample < 4096; ++sample) REQUIRE(std::isfinite(tape.process(0)));
        CHECK(std::abs(tape.process(0)) < 1e-7);
        tape.reset(); CHECK(tape.process(0) == 0);
    }
}

TEST_CASE("Playback head loss responds to tape speed and spacing",
          "[analog-physical][tape][dsp][head-loss]")
{
    fire::analog::Tape::Settings standard, slow, separated;
    slow.speed = .1905; separated.spacing = 2e-6;
    const auto fastSignal = render(standard, .03, 10000);
    const auto slowSignal = render(slow, .03, 10000);
    const auto spacedSignal = render(separated, .03, 10000);
    CHECK(rms(fastSignal) > .001);
    CHECK(rms(slowSignal) < rms(fastSignal) * .85);
    CHECK(rms(spacedSignal) < rms(fastSignal) * .85);
}

TEST_CASE("Recording bias linearises quiet tape signals while magnetic overload compresses peaks",
          "[analog-physical][tape][dsp][bias][overload]")
{
    fire::analog::Tape::Settings biased, unbiased;
    unbiased.bias = 0;
    const auto quiet = render(biased, .04, 1000);
    const auto withoutBias = render(unbiased, .04, 1000);
    const auto loud = render(biased, 2.5, 1000); // Above the 1.25 bias field: magnetic overload.
    const auto biasedDistortion = harmonic(quiet, 3000) / harmonic(quiet, 1000);
    const auto unbiasedDistortion = harmonic(withoutBias, 3000) / harmonic(withoutBias, 1000);
    CAPTURE(biasedDistortion, unbiasedDistortion);
    CHECK(biasedDistortion < unbiasedDistortion);
    CHECK(rms(loud) / rms(quiet) < 50); // Input level increased by 62.5x.
}

TEST_CASE("A settled tape tail becomes idle and resumes at the continuously running bias phase",
          "[analog-physical][tape][dsp][silence][realtime]")
{
    for (const double processingRate : {48000.0, 192000.0})
    {
        CAPTURE(processingRate);
        fire::analog::Tape played, idle;
        played.prepare(48000); idle.prepare(48000);
        played.setProcessingRate(processingRate); idle.setProcessingRate(processingRate);
        constexpr int noteSamples = 2389, quietSamples = 4921;
        for (int sample = 0; sample < noteSamples; ++sample)
        {
            played.process(.4 * std::sin(2 * pi * 371 * sample / processingRate));
            CHECK(idle.process(0) == 0);
        }
        REQUIRE_FALSE(played.isIdle());
        for (int sample = 0; sample < quietSamples; ++sample)
        {
            played.process(0); idle.process(0);
        }
        REQUIRE(played.isIdle());
        CHECK(played.process(0) == 0); CHECK(idle.process(0) == 0);
        for (int sample = 0; sample < 1024; ++sample)
        {
            const auto input = .1 * std::sin(2 * pi * 1307 * sample / processingRate);
            CHECK(std::abs(played.process(input) - idle.process(input)) < 1e-12);
        }
    }
}
