#include <DSP/LoudnessMatcher.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace
{
using Matcher = fire::dsp::LoudnessMatcher;
constexpr double pi = 3.1415926535897932384626433832795;

juce::AudioBuffer<float> tone(double sampleRate, int channels, double frequency = 997.0,
                              float amplitude = 0.2f, bool antiPhase = false)
{
    juce::AudioBuffer<float> buffer(channels, static_cast<int>(std::llround(sampleRate * 3.0)));
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = amplitude * static_cast<float>(std::sin(2.0 * pi * frequency * sample / sampleRate));
        for (int channel = 0; channel < channels; ++channel)
            buffer.setSample(channel, sample, channel == 1 && antiPhase ? -value : value);
    }
    return buffer;
}

juce::AudioBuffer<float> scaled(const juce::AudioBuffer<float>& original, float gainDb)
{
    juce::AudioBuffer<float> result;
    result.makeCopyOf(original);
    result.applyGain(std::pow(10.0f, gainDb / 20.0f));
    return result;
}

void feed(Matcher& matcher, juce::AudioBuffer<float>& reference,
          juce::AudioBuffer<float>& processed, int blockSize)
{
    const auto length = std::min(reference.getNumSamples(), processed.getNumSamples());
    for (int offset = 0; offset < length; offset += blockSize)
    {
        const auto count = std::min(blockSize, length - offset);
        juce::AudioBuffer<float> referenceBlock(reference.getArrayOfWritePointers(),
                                               reference.getNumChannels(), offset, count);
        juce::AudioBuffer<float> processedBlock(processed.getArrayOfWritePointers(),
                                               processed.getNumChannels(), offset, count);
        matcher.process(referenceBlock, processedBlock);
    }
}

void keepWindow(juce::AudioBuffer<float>& buffer, double rate, double fromSeconds, double untilSeconds)
{
    const auto begin = static_cast<int>(std::llround(rate * fromSeconds));
    const auto end = static_cast<int>(std::llround(rate * untilSeconds));
    buffer.clear(0, begin);
    buffer.clear(end, buffer.getNumSamples() - end);
}
}

TEST_CASE("Loudness matcher finds known gain changes at different sample rates", "[loudness-match][dsp]")
{
    for (double sampleRate : {8000.0, 32000.0, 44100.0, 48000.0, 96000.0})
        for (float gainDb : {-6.0f, 6.0f})
        {
            CAPTURE(sampleRate, gainDb);
            auto reference = tone(sampleRate, 1);
            auto processed = scaled(reference, gainDb);
            Matcher matcher;
            matcher.prepare(sampleRate);
            matcher.start();
            feed(matcher, reference, processed, 127);
            CHECK(matcher.getState() == Matcher::State::complete);
            CHECK(matcher.getProgress() == 1.0f);
            CHECK(matcher.getGainDb() == Catch::Approx(-gainDb).margin(0.002f));
            CHECK_FALSE(matcher.isLimited());
        }
}

TEST_CASE("Loudness matcher sums stereo energy without cancelling anti-phase channels", "[loudness-match][dsp][stereo]")
{
    for (bool antiPhase : {false, true})
    {
        CAPTURE(antiPhase);
        auto reference = tone(48000.0, 2, 997.0, 0.2f, antiPhase);
        auto processed = scaled(reference, -6.0f);
        Matcher matcher;
        matcher.start();
        feed(matcher, reference, processed, 512);
        REQUIRE(matcher.getState() == Matcher::State::complete);
        CHECK(matcher.getGainDb() == Catch::Approx(6.0f).margin(0.002f));
    }

    auto reference = tone(48000.0, 2);
    auto processed = scaled(reference, 0.0f);
    processed.clear(1, 0, processed.getNumSamples());
    Matcher matcher;
    matcher.start();
    feed(matcher, reference, processed, 512);
    CHECK(matcher.getGainDb() == Catch::Approx(3.0103f).margin(0.002f));
}

TEST_CASE("Loudness matcher gates silence and requires shared usable audio", "[loudness-match][dsp][silence]")
{
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        CAPTURE(scenario);
        auto reference = tone(48000.0, 2);
        auto processed = scaled(reference, -6.0f);
        if (scenario == 0) { reference.clear(); processed.clear(); }
        if (scenario == 1) processed.clear();
        if (scenario == 2) reference.clear();
        if (scenario == 3)
        {
            keepWindow(reference, 48000.0, 1.0, 1.1);
            keepWindow(processed, 48000.0, 1.0, 1.1);
        }
        if (scenario == 4)
        {
            keepWindow(reference, 48000.0, 0.5, 1.1);
            keepWindow(processed, 48000.0, 1.5, 2.1);
        }
        if (scenario == 5) { reference.applyGain(0.00001f); processed.applyGain(0.00001f); }
        Matcher matcher;
        matcher.start();
        feed(matcher, reference, processed, 1024);
        CHECK(matcher.getState() == Matcher::State::noSignal);
        CHECK(matcher.getProgress() == 1.0f);
        CHECK(matcher.getGainDb() == 0.0f);
        CHECK_FALSE(matcher.isLimited());
    }

    auto reference = tone(48000.0, 2);
    keepWindow(reference, 48000.0, 0.8, 1.4);
    auto processed = scaled(reference, -6.0f);
    Matcher matcher;
    matcher.start();
    feed(matcher, reference, processed, 1024);
    CHECK(matcher.getState() == Matcher::State::complete);
    CHECK(matcher.getGainDb() == Catch::Approx(6.0f).margin(0.002f));
}

TEST_CASE("Loudness matcher uses spectral K-weighting rather than plain RMS", "[loudness-match][dsp][weighting]")
{
    // Independent published 48 kHz BS.1770 coefficients, evaluated in the
    // frequency domain, check both the shelf and the low-frequency rolloff.
    const auto response = [](double frequency)
    {
        const auto z = std::polar(1.0, -2.0 * pi * frequency / 48000.0);
        const auto shelf = (1.53512485958697 - 2.69169618940638 * z + 1.19839281085285 * z * z)
            / (1.0 - 1.69065929318241 * z + 0.73248077421585 * z * z);
        const auto highpass = (1.0 - 2.0 * z + z * z)
            / (1.0 - 1.99004745483398 * z + 0.99007225036621 * z * z);
        return std::abs(shelf * highpass);
    };
    const auto expected = 20.0 * std::log10(response(50.0) / response(4000.0));
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        CAPTURE(sampleRate);
        auto reference = tone(sampleRate, 1, 50.0);
        auto processed = tone(sampleRate, 1, 4000.0);
        REQUIRE(reference.getRMSLevel(0, 0, reference.getNumSamples())
                == Catch::Approx(processed.getRMSLevel(0, 0, processed.getNumSamples())).margin(0.00001f));
        Matcher matcher;
        matcher.prepare(sampleRate);
        matcher.start();
        feed(matcher, reference, processed, 257);
        CHECK(matcher.getState() == Matcher::State::complete);
        CHECK(matcher.getGainDb() < -6.0f);
        // Small bilinear-warp differences are allowed across rates. Reusing
        // fixed 48 kHz coefficients at another rate moves the filters enough
        // to fail this frequency-selective comparison.
        CHECK(matcher.getGainDb() == Catch::Approx(expected).margin(0.03));
    }
}

TEST_CASE("Loudness matcher is independent of callback partitioning and freezes at three seconds", "[loudness-match][dsp][block-size]")
{
    auto reference = tone(48000.0, 2, 997.0, 0.2f, true);
    auto processed = scaled(reference, 6.0f);
    float expected = 0.0f;
    for (int blockSize : {144000, 1, 37, 512})
    {
        CAPTURE(blockSize);
        Matcher matcher;
        matcher.start();
        feed(matcher, reference, processed, blockSize);
        REQUIRE(matcher.getState() == Matcher::State::complete);
        if (blockSize == 144000) expected = matcher.getGainDb();
        CHECK(juce::exactlyEqual(matcher.getGainDb(), expected));
        reference.clear();
        matcher.process(reference, processed);
        CHECK(matcher.getState() == Matcher::State::complete);
        CHECK(juce::exactlyEqual(matcher.getGainDb(), expected));
        CHECK(matcher.getProgress() == 1.0f);
        reference = tone(48000.0, 2, 997.0, 0.2f, true);
    }

    Matcher matcher;
    matcher.start();
    juce::AudioBuffer<float> almostReference(reference.getArrayOfWritePointers(), 2, 0, 143999);
    juce::AudioBuffer<float> almostProcessed(processed.getArrayOfWritePointers(), 2, 0, 143999);
    matcher.process(almostReference, almostProcessed);
    CHECK(matcher.getState() == Matcher::State::measuring);
    CHECK(matcher.getProgress() < 1.0f);
    juce::AudioBuffer<float> lastReference(reference.getArrayOfWritePointers(), 2, 143999, 1);
    juce::AudioBuffer<float> lastProcessed(processed.getArrayOfWritePointers(), 2, 143999, 1);
    matcher.process(lastReference, lastProcessed);
    CHECK(matcher.getState() == Matcher::State::complete);
    CHECK(matcher.getProgress() == 1.0f);
}

TEST_CASE("Loudness matcher limits correction and resets on restart cancel and prepare", "[loudness-match][dsp][lifecycle]")
{
    auto reference = tone(48000.0, 1);
    Matcher matcher;
    CHECK(matcher.getState() == Matcher::State::idle);
    for (float gain : {-24.0f, 24.0f})
    {
        auto processed = scaled(reference, gain);
        matcher.start();
        CHECK(matcher.getProgress() == 0.0f);
        CHECK(matcher.getGainDb() == 0.0f);
        CHECK_FALSE(matcher.isLimited());
        feed(matcher, reference, processed, 512);
        CHECK(matcher.getState() == Matcher::State::complete);
        CHECK(matcher.isLimited());
        CHECK(juce::exactlyEqual(matcher.getGainDb(), gain < 0.0f ? 18.0f : -18.0f));
    }
    auto processed = scaled(reference, -6.0f);
    matcher.start();
    juce::AudioBuffer<float> partialReference(reference.getArrayOfWritePointers(), 1, 0, 24000);
    juce::AudioBuffer<float> partialProcessed(processed.getArrayOfWritePointers(), 1, 0, 24000);
    matcher.process(partialReference, partialProcessed);
    CHECK(matcher.getProgress() == Catch::Approx(1.0f / 6.0f));
    matcher.cancel();
    matcher.process(reference, processed);
    CHECK(matcher.getState() == Matcher::State::idle);
    CHECK(matcher.getProgress() == 0.0f);
    CHECK(matcher.getGainDb() == 0.0f);
    matcher.start();
    feed(matcher, reference, processed, 512);
    CHECK(matcher.getGainDb() == Catch::Approx(6.0f).margin(0.002f));
    CHECK_FALSE(matcher.isLimited());
    matcher.prepare(96000.0);
    CHECK(matcher.getState() == Matcher::State::idle);
    CHECK(matcher.getGainDb() == 0.0f);
}

TEST_CASE("Loudness matcher sanitises invalid rates and non-finite input without changing audio", "[loudness-match][dsp][robustness]")
{
    for (double rate : {0.0, -1.0, 1.0, 1.0e30, std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()})
    {
        auto reference = tone(48000.0, 1);
        auto processed = scaled(reference, 6.0f);
        reference.setSample(0, 12345, std::numeric_limits<float>::infinity());
        processed.setSample(0, 12345, std::numeric_limits<float>::quiet_NaN());
        Matcher matcher;
        matcher.prepare(rate);
        matcher.start();
        feed(matcher, reference, processed, 512);
        CHECK(matcher.getState() == Matcher::State::complete);
        CHECK(matcher.getGainDb() == Catch::Approx(-6.0f).margin(0.002f));
        CHECK(std::isinf(reference.getSample(0, 12345)));
        CHECK(std::isnan(processed.getSample(0, 12345)));
    }
}
