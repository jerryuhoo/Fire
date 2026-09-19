#include <DSP/LoudnessMatchState.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
using MatchState = fire::dsp::LoudnessMatchState;
constexpr double rate = 16000.0;
constexpr int windowSamples = 48000;
constexpr int rampSamples = 800;
constexpr int blockSize = 256;

juce::AudioBuffer<float> signal(int offset, int samples)
{
    juce::AudioBuffer<float> audio(2, samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi
                           * static_cast<double>(offset + sample) / rate;
        const auto value = static_cast<float>(0.1 * std::sin(1000.0 * phase)
                                              + 0.03 * std::sin(250.0 * phase));
        audio.setSample(0, sample, value);
        audio.setSample(1, sample, -value * 0.7f);
    }
    return audio;
}

void render(MatchState& state, int side, float processedGainDb,
            int samples, int& timeline, bool stable = true)
{
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const auto count = std::min(blockSize, samples - offset);
        auto reference = signal(timeline, count);
        juce::AudioBuffer<float> output;
        output.makeCopyOf(reference);
        output.applyGain(juce::Decibels::decibelsToGain(processedGainDb));
        state.process(reference, output, state.capture(side), stable);
        timeline += count;
    }
}

MatchState::Settings calibrated(float firstGain = 6.0f, float secondGain = -3.0f)
{
    MatchState::Settings settings;
    settings.enabled = true;
    settings.ready = {true, true};
    settings.gainDb = {firstGain, secondGain};
    return settings;
}

juce::AudioBuffer<float> processConstant(MatchState& state, const MatchState::Frame& frame, int samples)
{
    juce::AudioBuffer<float> reference(2, samples), output(2, samples);
    for (int channel = 0; channel < 2; ++channel)
    {
        juce::FloatVectorOperations::fill(reference.getWritePointer(channel), 0.2f, samples);
        juce::FloatVectorOperations::fill(output.getWritePointer(channel), 0.2f, samples);
    }
    state.process(reference, output, frame, true);
    return output;
}
}

TEST_CASE("Loudness state rejects a stale measurement frame after restore clear or relearn", "[loudness-match][controller][stale]")
{
    for (int command = 0; command < 3; ++command)
    {
        CAPTURE(command);
        MatchState state;
        state.prepare(rate);
        state.setEnabled(true, 0);
        int timeline = 0;
        render(state, 0, 6.0f, windowSamples - blockSize, timeline);
        REQUIRE(state.view(0, false).measuring);
        REQUIRE(state.view(0, false).progress > 0.9f);
        const auto stale = state.capture(0);
        if (command == 0) state.restore(calibrated(-3.0f, 4.0f));
        if (command == 1) state.clearSide(0);
        if (command == 2) state.requestLearn(0);
        const auto fresh = state.capture(0);
        REQUIRE(stale.records[0] != fresh.records[0]);
        auto reference = signal(timeline, blockSize);
        juce::AudioBuffer<float> output;
        output.makeCopyOf(reference);
        output.applyGain(juce::Decibels::decibelsToGain(6.0f));
        state.process(reference, output, stale, true);
        CHECK(state.capture(0).records == fresh.records);
        CHECK_FALSE(state.takeCompletedNotification());
        if (command == 0)
        {
            CHECK(state.view(0, false).ready);
            CHECK(juce::exactlyEqual(state.view(0, false).gainDb, -3.0f));
            CHECK(state.view(1, false).gainDb == 4.0f);
        }
        else
        {
            CHECK_FALSE(state.view(0, false).ready);
            CHECK(state.view(0, false).progress == 0.0f);
            render(state, 0, 6.0f, blockSize, timeline);
            CHECK_FALSE(state.view(0, false).ready);
        }
    }
}

TEST_CASE("Loudness gain follows the accepted audio frame until the next callback", "[loudness-match][controller][snapshot]")
{
    MatchState state;
    state.prepare(rate);
    state.restore(calibrated(6.0f, -3.0f));
    const auto accepted = state.capture(0);
    state.restore(calibrated(-6.0f, 3.0f));
    const auto newer = state.capture(0);
    const auto oldAudio = processConstant(state, accepted, rampSamples + 1);
    CHECK(oldAudio.getSample(0, rampSamples) == Catch::Approx(0.2f * juce::Decibels::decibelsToGain(6.0f)));
    CHECK(juce::exactlyEqual(state.view(0, false).gainDb, -6.0f));
    const auto newAudio = processConstant(state, newer, rampSamples + 1);
    CHECK(newAudio.getSample(0, rampSamples) == Catch::Approx(0.2f * juce::Decibels::decibelsToGain(-6.0f)));
}

TEST_CASE("Loudness state leaves cold and settled OFF audio bit exact", "[loudness-match][controller][off]")
{
    MatchState state;
    state.prepare(rate);
    const auto checkOff = [&]
    {
        auto original = signal(0, blockSize);
        original.setSample(0, 0, -0.0f);
        original.setSample(0, 1, std::numeric_limits<float>::denorm_min());
        original.setSample(0, 2, std::numeric_limits<float>::quiet_NaN());
        juce::AudioBuffer<float> output;
        output.makeCopyOf(original);
        const auto frame = state.capture(0);
        REQUIRE_FALSE(frame.enabled);
        CHECK_FALSE(state.needsReference(frame));
        state.process(original, output, frame, true);
        for (int channel = 0; channel < 2; ++channel)
            CHECK(std::memcmp(original.getReadPointer(channel), output.getReadPointer(channel),
                              sizeof(float) * static_cast<size_t>(blockSize)) == 0);
    };
    checkOff();
    state.restore(calibrated());
    processConstant(state, state.capture(0), rampSamples + 1);
    state.setEnabled(false, 0);
    const auto fadingOff = processConstant(state, state.capture(0), rampSamples);
    CHECK(fadingOff.getSample(0, 0) > 0.2f);
    CHECK(juce::exactlyEqual(fadingOff.getSample(0, rampSamples - 1), 0.2f));
    checkOff();
}

TEST_CASE("Loudness state freezes completion and measures uncorrected audio when relearning", "[loudness-match][controller][lifecycle]")
{
    MatchState state;
    state.prepare(rate);
    state.setEnabled(true, 0);
    int timeline = 0;
    render(state, 0, 6.0f, windowSamples, timeline);
    REQUIRE(state.view(0, false).ready);
    REQUIRE_FALSE(state.view(0, false).measuring);
    const auto learned = state.view(0, false).gainDb;
    CHECK(learned == Catch::Approx(-6.0f).margin(0.002f));
    CHECK(state.takeCompletedNotification());
    CHECK_FALSE(state.takeCompletedNotification());
    render(state, 0, -6.0f, windowSamples, timeline);
    CHECK(juce::exactlyEqual(state.view(0, false).gainDb, learned));
    CHECK_FALSE(state.takeCompletedNotification());
    state.requestLearn(0);
    REQUIRE(state.needsReference(state.capture(0)));
    CHECK(juce::exactlyEqual(state.view(0, false).gainDb, learned));
    render(state, 0, -6.0f, windowSamples, timeline);
    CHECK(state.view(0, false).gainDb == Catch::Approx(6.0f).margin(0.002f));
    CHECK(state.takeCompletedNotification());

    state.requestLearn(0);
    juce::AudioBuffer<float> silence(2, windowSamples);
    silence.clear();
    state.process(silence, silence, state.capture(0), true);
    CHECK(state.view(0, false).ready);
    CHECK_FALSE(state.view(0, false).measuring);
    CHECK(state.view(0, false).noSignal);
    CHECK(state.view(0, false).gainDb == Catch::Approx(6.0f).margin(0.002f));
    CHECK_FALSE(state.needsReference(state.capture(0)));
}

TEST_CASE("Loudness cancellation distinguishes an unmeasured side from an explicit user cancel", "[loudness-match][controller][cancel]")
{
    MatchState state;
    state.prepare(rate);
    state.setEnabled(true, 0);
    state.cancelMeasurements();
    CHECK_FALSE(state.view(0, false).measuring);
    CHECK_FALSE(state.view(0, false).noSignal);
    CHECK_FALSE(state.view(1, false).noSignal);
    CHECK(state.needsReference(state.capture(1)));
    processConstant(state, state.capture(1), blockSize);
    CHECK(state.view(1, false).measuring);
    state.cancelMeasurement(1);
    CHECK_FALSE(state.view(1, false).measuring);
    CHECK(state.view(1, false).noSignal);
    CHECK_FALSE(state.needsReference(state.capture(1)));
    CHECK(state.needsReference(state.capture(0)));

    state.restore(calibrated());
    state.requestLearn(0);
    state.cancelMeasurement(0);
    CHECK(state.view(0, false).ready);
    CHECK(state.view(0, false).gainDb == 6.0f);
    CHECK_FALSE(state.view(0, false).measuring);
    CHECK_FALSE(state.view(0, false).noSignal);
    CHECK(state.view(1, false).ready);
}

TEST_CASE("Loudness side switching restarts incomplete windows and preserves the other gain", "[loudness-match][controller][ab]")
{
    MatchState state;
    state.prepare(rate);
    auto settings = calibrated();
    settings.ready[0] = false;
    state.restore(settings);
    state.setEnabled(true, 0);
    int timeline = 0;
    render(state, 0, 6.0f, 32000, timeline);
    REQUIRE(state.view(0, false).progress > 0.6f);
    processConstant(state, state.capture(1), blockSize);
    CHECK_FALSE(state.view(0, false).measuring);
    CHECK_FALSE(state.view(0, false).noSignal);
    CHECK(juce::exactlyEqual(state.view(1, false).gainDb, -3.0f));
    render(state, 0, 6.0f, 32000, timeline);
    CHECK_FALSE(state.view(0, false).ready);
    CHECK(state.view(0, false).progress < 0.7f);
    render(state, 0, 6.0f, 20000, timeline);
    CHECK(state.view(0, false).gainDb == Catch::Approx(-6.0f).margin(0.002f));
    CHECK(juce::exactlyEqual(state.view(1, false).gainDb, -3.0f));

    state.requestLearn(1);
    const auto staleDestination = state.capture(1);
    state.copyToOtherSide(0);
    const auto copied = state.capture(1);
    processConstant(state, staleDestination, blockSize);
    CHECK(state.capture(1).records == copied.records);
    CHECK(juce::exactlyEqual(state.view(1, false).gainDb, state.view(0, false).gainDb));
    CHECK_FALSE(state.view(1, false).measuring);
}

TEST_CASE("Loudness state rejects invalid restored gains without publishing unusable flags", "[loudness-match][controller][restore]")
{
    MatchState state;
    state.prepare(rate);
    for (float value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                        -18.01f, 18.01f})
    {
        auto settings = calibrated(value, value);
        settings.limited = {true, true};
        state.restore(settings);
        const auto actual = state.settings();
        for (int side = 0; side < 2; ++side)
        {
            const auto index = static_cast<size_t>(side);
            CHECK_FALSE(actual.ready[index]);
            CHECK_FALSE(actual.limited[index]);
            CHECK(actual.gainDb[index] == 0.0f);
            CHECK(state.needsReference(state.capture(side)));
        }
    }
    auto settings = calibrated(-18.0f, 18.0f);
    settings.limited = {true, true};
    state.restore(settings);
    CHECK(state.settings().gainDb == settings.gainDb);
    CHECK(state.settings().ready == settings.ready);
    CHECK(state.settings().limited == settings.limited);
}
