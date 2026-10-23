#include <DSP/Clouds/CloudsEngine.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
namespace fx = fire::effects;
namespace src = fire::effects::frozen_audio;
constexpr double mediaPi = 3.1415926535897932384626433832795;
fx::FrozenRecordingPtr mediaRecording(double frequency, float amplitude = .35f)
{
    auto recording = std::make_shared<fx::FrozenRecording>();
    recording->validFrames = fx::frozenRecordingFrames; recording->head = 0;
    for (int sample = 0; sample < recording->validFrames; ++sample)
    {
        const auto value = static_cast<std::int16_t>(std::round(amplitude * 32767 * std::sin(2 * mediaPi * frequency * sample / fx::frozenRecordingSampleRate)));
        recording->samples[0][static_cast<size_t>(sample)] = value;
        recording->samples[1][static_cast<size_t>(sample)] = static_cast<std::int16_t>(-value / 2);
    }
    return recording;
}
fx::CloudsParameters mediaControls(std::uint32_t publication = 0)
{
    fx::CloudsParameters p; p.freeze = true; p.publicationSequence = publication;
    p.position = .3f; p.size = .4f; p.density = .7f; p.texture = .2f; p.spread = .25f;
    return p;
}
std::array<float, 2> mediaNext(fx::CloudsEngine& engine, const fx::CloudsParameters& p)
{ float left = 0, right = 0; engine.process(left, right, p); return {left, right}; }
}

TEST_CASE("Offline Clouds SRC starts at the selected source position without startup silence", "[clouds-media-dsp][src]")
{
    constexpr int count = 4096;
    for (double rate : {8000., 16000., 44100., 48000., 96000., 192000., 384000.})
    {
        const auto guard = src::sourceGuardFrames(rate);
        const double first = guard + .375;
        const int inputCount = static_cast<int>(std::ceil(first + (count - 1) * rate / 32000)) + guard + 1;
        std::vector<float> left(static_cast<size_t>(inputCount)), right(left.size()), a(count), b(count);
        for (int sample = 0; sample < inputCount; ++sample)
        {
            left[static_cast<size_t>(sample)] = .25f * static_cast<float>(std::sin(2 * mediaPi * 997 * sample / rate));
            right[static_cast<size_t>(sample)] = -.5f * left[static_cast<size_t>(sample)];
        }
        REQUIRE(src::resampleWindow(left.data(), right.data(), inputCount, rate, first, a.data(), b.data(), count));
        float error = 0, stereoError = 0;
        for (int sample = 0; sample < count; ++sample)
        {
            const auto expected = .25f * static_cast<float>(std::sin(2 * mediaPi * 997 * (first + sample * rate / 32000) / rate));
            error = std::max(error, std::abs(a[static_cast<size_t>(sample)] - expected));
            stereoError = std::max(stereoError, std::abs(b[static_cast<size_t>(sample)] + .5f * a[static_cast<size_t>(sample)]));
        }
        CAPTURE(rate, error, stereoError); CHECK(error < .0002f); CHECK(stereoError < 1e-7f);
        std::fill(left.begin(), left.end(), .3f);
        REQUIRE(src::resampleWindow(left.data(), nullptr, inputCount, rate, first, a.data(), b.data(), count));
        CHECK(a.front() == Catch::Approx(.3).margin(1e-6)); CHECK(a.back() == Catch::Approx(.3).margin(1e-6));
        CHECK(a == b);
    }
}

TEST_CASE("Offline Clouds downsampling rejects ultrasonic input instead of aliasing it", "[clouds-media-dsp][src]")
{
    constexpr int count = 4096;
    for (double rate : {96000., 192000., 384000.})
    {
        const int guard = src::sourceGuardFrames(rate);
        const int inputCount = static_cast<int>(std::ceil(count * rate / 32000)) + 2 * guard;
        std::vector<float> input(static_cast<size_t>(inputCount)), a(count), b(count);
        std::array<double, 2> energies {};
        for (int test = 0; test < 2; ++test)
        {
            const double frequency = test == 0 ? 1000 : 22000;
            for (int sample = 0; sample < inputCount; ++sample)
                input[static_cast<size_t>(sample)] = .5f * static_cast<float>(std::sin(2 * mediaPi * frequency * sample / rate));
            REQUIRE(src::resampleWindow(input.data(), nullptr, inputCount, rate, guard, a.data(), b.data(), count));
            for (float sample : a) energies[static_cast<size_t>(test)] += sample * sample;
        }
        CAPTURE(rate, energies[0], energies[1]);
        REQUIRE(energies[0] > 100); CHECK(energies[1] < energies[0] * 1e-5);
    }
}

TEST_CASE("Offline conversion bounds cancellation and tiny edge windows are explicit", "[clouds-media-dsp][src][validation]")
{
    std::array<float, 8> left {}, right {};
    const float input = .5f;
    REQUIRE(src::resampleWindow(&input, nullptr, 1, 8000, 0, left.data(), right.data(), 4));
    for (int sample = 0; sample < 4; ++sample) CHECK(std::isfinite(left[static_cast<size_t>(sample)]));
    REQUIRE(src::resampleWindow(&input, nullptr, 1, 384000, 0, left.data(), right.data(), 1));
    CHECK(std::isfinite(left[0]));
    left.fill(.75f); right.fill(.75f);
    const std::atomic<bool> cancelled {true};
    CHECK_FALSE(src::resampleWindow(&input, nullptr, 1, 8000, 0, left.data(), right.data(), 4, &cancelled));
    CHECK(left[0] == .75f);
    CHECK_FALSE(src::resampleWindow(&input, nullptr, std::numeric_limits<int>::max(), 48000, 0, left.data(), right.data(), 1));
    CHECK_FALSE(src::resampleWindow(&input, nullptr, 1, 48000, 1, left.data(), right.data(), 1));
    CHECK_FALSE(src::resampleWindow(&input, nullptr, 1, 48000, -.1, left.data(), right.data(), 1));
    CHECK_FALSE(src::resampleWindow(&input, nullptr, 1, 0, 0, left.data(), right.data(), 1));
    CHECK(src::sourceGuardFrames(384000) == 290);
    CHECK(src::sourceGuardFrames(std::numeric_limits<double>::infinity()) == 0);
}

TEST_CASE("Smooth recording requests retain their publication guard and are immediately exportable", "[clouds-media-dsp][publication]")
{
    fx::CloudsEngine engine, reference;
    engine.prepare(48000); reference.prepare(48000);
    const auto old = mediaRecording(223), next = mediaRecording(997);
    engine.stageFrozenRecording(old); reference.stageFrozenRecording(old);
    auto p = mediaControls();
    for (int i = 0; i < 20000; ++i) { mediaNext(engine, p); mediaNext(reference, p); }
    engine.stageFrozenRecording(next, 12, true);
    const auto exported = engine.copyFrozenRecording(); REQUIRE(exported);
    CHECK(exported->samples == next->samples);
    float error = 0;
    for (int i = 0; i < 1000; ++i)
    {
        const auto actual = mediaNext(engine, p), expected = mediaNext(reference, p);
        for (size_t channel = 0; channel < 2; ++channel) error = std::max(error, std::abs(actual[channel] - expected[channel]));
    }
    CHECK(error == 0); // No fade or swap before the matching parameter publication.
    p.publicationSequence = 12;
    for (int sample = 0; sample < 240; ++sample)
    {
        const auto actual = mediaNext(engine, p), expected = mediaNext(reference, p);
        const auto gain = 1.0 - static_cast<double>(sample) / 240;
        for (size_t channel = 0; channel < 2; ++channel)
            CHECK(actual[channel] == Catch::Approx(expected[channel] * gain).margin(1e-6));
    }
    fx::CloudsEngine fresh; fresh.prepare(48000); fresh.stageFrozenRecording(next, 12);
    double energy = 0; float settledError = 0;
    for (int sample = 0; sample < 6000; ++sample)
    {
        const auto actual = mediaNext(engine, p), expected = mediaNext(fresh, p);
        REQUIRE(std::isfinite(actual[0])); REQUIRE(std::isfinite(actual[1]));
        if (sample == 0) { CHECK(actual[0] == 0); CHECK(actual[1] == 0); }
        if (sample >= 2000)
            for (size_t channel = 0; channel < 2; ++channel)
            { settledError = std::max(settledError, std::abs(actual[channel] - expected[channel])); energy += actual[channel] * actual[channel]; }
    }
    CHECK(settledError == 0); CHECK(energy > .001);
}

TEST_CASE("A newer pending recording wins while the local transition waits for its publication", "[clouds-media-dsp][publication][transition]")
{
    fx::CloudsEngine engine; engine.prepare(48000);
    engine.stageFrozenRecording(mediaRecording(223));
    auto p = mediaControls();
    for (int i = 0; i < 12000; ++i) mediaNext(engine, p);
    engine.stageFrozenRecording(mediaRecording(557), 2, true); p.publicationSequence = 2;
    for (int i = 0; i < 100; ++i) mediaNext(engine, p);
    const auto latest = mediaRecording(997);
    engine.stageFrozenRecording(latest, 4, true);
    for (int i = 100; i < 600; ++i)
    {
        const auto sample = mediaNext(engine, p);
        if (i >= 240) { CHECK(sample[0] == 0); CHECK(sample[1] == 0); }
    }
    auto pending = engine.copyFrozenRecording(); REQUIRE(pending); CHECK(pending->samples == latest->samples);
    p.publicationSequence = 4;
    for (int i = 0; i < 6000; ++i) mediaNext(engine, p);
    const auto applied = engine.copyFrozenRecording(); REQUIRE(applied); CHECK(applied->samples == latest->samples);

    // An authoritative clear supersedes a queued import and can never allow
    // that older material to reappear at the end of its fade.
    engine.stageFrozenRecording(mediaRecording(443), 6, true); p.publicationSequence = 6;
    for (int i = 0; i < 80; ++i) mediaNext(engine, p);
    engine.stageFrozenRecording({}, 8); p.publicationSequence = 8;
    CHECK_FALSE(engine.copyFrozenRecording());
    for (int i = 0; i < 6000; ++i) mediaNext(engine, p);
    CHECK_FALSE(engine.copyFrozenRecording());
}

TEST_CASE("First audio and lifecycle resets install pending material without stale local fades", "[clouds-media-dsp][lifecycle]")
{
    const auto recording = mediaRecording(337);
    fx::CloudsEngine smooth, immediate;
    smooth.prepare(48000); immediate.prepare(48000);
    smooth.stageFrozenRecording(recording, 2, true); immediate.stageFrozenRecording(recording, 2);
    auto p = mediaControls(2);
    for (int i = 0; i < 6000; ++i) CHECK(mediaNext(smooth, p) == mediaNext(immediate, p));
    const auto next = mediaRecording(887);
    smooth.stageFrozenRecording(next, 4, true);
    for (int i = 0; i < 100; ++i) mediaNext(smooth, mediaControls(4));
    smooth.reset(); immediate.reset();
    immediate.stageFrozenRecording(next, 4);
    p.publicationSequence = 4;
    for (int i = 0; i < 6000; ++i) CHECK(mediaNext(smooth, p) == mediaNext(immediate, p));
}
