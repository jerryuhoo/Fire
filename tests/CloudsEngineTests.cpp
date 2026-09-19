#include <DSP/Clouds/CloudsEngine.h>
#include <DSP/Clouds/vendor/clouds/dsp/audio_buffer.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
using Engine = fire::effects::CloudsEngine;
using Parameters = fire::effects::CloudsParameters;
constexpr double pi = 3.1415926535897932384626433832795;
float input(int sample, double rate)
{
    return static_cast<float>(0.15 * std::cos(2.0 * pi * 223.0 * sample / rate)
                              + 0.04 * std::sin(2.0 * pi * 997.0 * sample / rate));
}
std::vector<float> render(Engine& engine, double rate, int samples, Parameters parameters,
                         int partition = 128, bool silence = false)
{
    std::vector<float> result(static_cast<size_t>(samples) * 2);
    for (int offset = 0; offset < samples; offset += partition)
        for (int sample = offset; sample < std::min(samples, offset + partition); ++sample)
        {
            float left = silence ? 0.0f : input(sample, rate), right = left;
            engine.process(left, right, parameters);
            result[static_cast<size_t>(sample) * 2] = left;
            result[static_cast<size_t>(sample) * 2 + 1] = right;
        }
    return result;
}
double energy(const std::vector<float>& values, size_t begin = 0)
{
    double sum = 0.0;
    bool finite = true;
    float maximum = 0.0f;
    for (size_t sample = begin; sample < values.size(); ++sample)
    {
        finite = finite && std::isfinite(values[sample]);
        maximum = std::max(maximum, std::abs(values[sample]));
        sum += static_cast<double>(values[sample]) * values[sample];
    }
    REQUIRE(finite);
    REQUIRE(maximum < 1.1f);
    return sum;
}
}

TEST_CASE("Clouds normal mode renders mono input at supported host rates", "[clouds][dsp][rates]")
{
    for (double rate : {8000.0, 16000.0, 32000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0})
    {
        CAPTURE(rate);
        Engine engine;
        engine.prepare(rate);
        const auto result = render(engine, rate, static_cast<int>(rate * 0.65), {});
        CHECK(energy(result) > 0.01);
    }
}

TEST_CASE("Clouds resampling and control changes do not depend on host callback partitioning", "[clouds][dsp][block-size]")
{
    constexpr double rate = 44100.0;
    constexpr int samples = 66150;
    const auto renderControls = [&](int partition)
    {
        Engine engine;
        engine.prepare(rate);
        std::vector<float> output(static_cast<size_t>(samples) * 2);
        Parameters parameters;
        for (int offset = 0; offset < samples; offset += partition)
            for (int sample = offset; sample < std::min(samples, offset + partition); ++sample)
            {
                parameters.position = sample < 22050 ? 0.1f : 0.7f;
                parameters.texture = sample < 33075 ? 0.5f : 1.0f;
                parameters.pitch = sample < 44100 ? 0.0f : 7.0f;
                parameters.freeze = sample >= 50000 && sample < 60000;
                float left = input(sample, rate), right = -left * 0.5f;
                engine.process(left, right, parameters);
                output[static_cast<size_t>(sample) * 2] = left;
                output[static_cast<size_t>(sample) * 2 + 1] = right;
            }
        return output;
    };
    const auto reference = renderControls(512);
    for (int partition : {1, 37, 128})
        CHECK(renderControls(partition) == reference);
}

TEST_CASE("Clouds reset clears recording feedback and effects without leaking old audio", "[clouds][dsp][reset]")
{
    Engine used, fresh;
    used.prepare(16000.0);
    fresh.prepare(16000.0);
    Parameters parameters;
    parameters.feedback = 0.95f;
    parameters.reverb = parameters.texture = 1.0f;
    parameters.density = 0.9f;
    CHECK(energy(render(used, 16000.0, 24000, parameters)) > 0.01);
    used.reset();
    const auto silence = render(used, 16000.0, 8000, parameters, 128, true);
    CHECK(energy(silence) == 0.0);
    used.reset();
    CHECK(render(used, 16000.0, 24000, parameters) == render(fresh, 16000.0, 24000, parameters));
}

TEST_CASE("Clouds freeze from empty history captures sound and then ignores live input", "[clouds][dsp][freeze]")
{
    Engine first, second;
    first.prepare(16000.0);
    second.prepare(16000.0);
    Parameters parameters;
    parameters.freeze = true;
    CHECK(energy(render(first, 16000.0, 8000, parameters, 128, true)) == 0.0);
    render(second, 16000.0, 8000, parameters, 128, true);
    const auto captured = render(first, 16000.0, 32000, parameters);
    const auto otherCaptured = render(second, 16000.0, 32000, parameters);
    CHECK(captured == otherCaptured);
    REQUIRE(energy(captured, 32000) > 0.01);
    const auto frozen = render(first, 16000.0, 8000, parameters, 128, true);
    const auto inputContinues = render(second, 16000.0, 8000, parameters);
    CHECK(frozen == inputContinues);
    CHECK(energy(frozen) > 0.01);
    parameters.freeze = false;
    CHECK(energy(render(first, 16000.0, 24000, parameters, 128, true), 40000) < 1.0e-8);
}

TEST_CASE("Clouds RNG and reset are isolated between simultaneously processed engines", "[clouds][dsp][isolation]")
{
    Engine reference, interleaved, neighbour;
    reference.prepare(16000.0);
    interleaved.prepare(16000.0);
    neighbour.prepare(48000.0);
    Parameters parameters;
    parameters.density = 0.85f;
    parameters.spread = 1.0f;
    const auto expected = render(reference, 16000.0, 24000, parameters);
    std::vector<float> actual(expected.size());
    for (int sample = 0; sample < 24000; ++sample)
    {
        float left = input(sample, 16000.0), right = left;
        interleaved.process(left, right, parameters);
        actual[static_cast<size_t>(sample) * 2] = left;
        actual[static_cast<size_t>(sample) * 2 + 1] = right;
        float otherLeft = -0.15f, otherRight = 0.07f;
        neighbour.process(otherLeft, otherRight, parameters);
        if (sample % 257 == 0) neighbour.reset();
    }
    CHECK(actual == expected);
}

TEST_CASE("Clouds extreme controls and non-finite input stay bounded", "[clouds][dsp][robustness][endpoints]")
{
    for (float pitch : {-24.0f, 0.0f, 24.0f})
    {
        CAPTURE(pitch);
        Engine engine;
        engine.prepare(16000.0);
        Parameters parameters;
        parameters.size = parameters.texture = parameters.spread = 1.0f;
        parameters.feedback = parameters.reverb = parameters.density = 1.0f;
        parameters.pitch = pitch;
        const auto result = render(engine, 16000.0, 24000, parameters);
        CHECK(energy(result) > 0.0);
    }
    Engine clean, contaminated;
    clean.prepare(16000.0);
    contaminated.prepare(std::numeric_limits<double>::quiet_NaN());
    contaminated.prepare(16000.0);
    Parameters parameters, invalid;
    invalid.size = std::numeric_limits<float>::quiet_NaN();
    invalid.feedback = std::numeric_limits<float>::infinity();
    for (int sample = 0; sample < 16000; ++sample)
    {
        float left = input(sample, 16000.0), right = left;
        float expectedLeft = left, expectedRight = right;
        if (sample % 127 == 0)
        {
            left = std::numeric_limits<float>::quiet_NaN();
            right = std::numeric_limits<float>::infinity();
            expectedLeft = expectedRight = 0.0f;
        }
        clean.process(expectedLeft, expectedRight, parameters);
        contaminated.process(left, right, invalid);
        REQUIRE(left == expectedLeft);
        REQUIRE(right == expectedRight);
    }
}

TEST_CASE("Clouds short freeze crossfades only initialized tail samples", "[clouds][dsp][freeze][bounds]")
{
    using namespace fire_clouds_vendor;
    for (int freezeLength : {1, 31, 32, 127, 255, 256, 512})
    {
        CAPTURE(freezeLength);
        std::array<int16_t, 1032> storage;
        std::array<int16_t, 256> tail;
        tail.fill(std::numeric_limits<int16_t>::min());
        AudioBuffer<RESOLUTION_16_BIT> buffer;
        buffer.Init(storage.data(), static_cast<int32_t>(storage.size()), tail.data());
        const float initial = 0.2f;
        for (int i = 0; i < 512; ++i) buffer.Write(initial);
        std::vector<float> frozen(static_cast<size_t>(freezeLength), initial);
        buffer.WriteFade(frozen.data(), freezeLength, 1, false);
        const std::array<float, 32> live {};
        for (int block = 0; block < 8; ++block)
        {
            buffer.WriteFade(live.data(), static_cast<int32_t>(live.size()), 1, true);
            const auto value = buffer.Read<INTERPOLATION_LINEAR>(buffer.head() - 1, 0);
            CHECK(value >= 0.0f);
            CHECK(value <= initial);
        }
    }
}

TEST_CASE("Clouds recording started in a sustained note has no delayed hard edge", "[clouds][dsp][startup]")
{
    Engine engine;
    engine.prepare(32000.0);
    Parameters parameters;
    float previous = 0.0f, maximumStep = 0.0f;
    double sum = 0.0;
    for (int sample = 0; sample < 32000; ++sample)
    {
        float left = 0.2f, right = left;
        engine.process(left, right, parameters);
        if (sample > 1600) maximumStep = std::max(maximumStep, std::abs(left - previous));
        sum += left * left;
        previous = left;
    }
    CHECK(sum > 0.1);
    CHECK(maximumStep < 0.02f);
}

TEST_CASE("Clouds host rate conversion rejects out-of-band folding in both directions", "[clouds][dsp][src]")
{
    const auto tonePower = [](double rate, double frequency, float pitch)
    {
        Engine engine;
        engine.prepare(rate);
        Parameters parameters;
        parameters.pitch = pitch;
        double energy = 0.0;
        const int samples = static_cast<int>(rate * 2.0);
        for (int sample = 0; sample < samples; ++sample)
        {
            float left = static_cast<float>(0.2 * std::sin(2.0 * pi * frequency * sample / rate));
            float right = left;
            engine.process(left, right, parameters);
            if (sample >= static_cast<int>(rate)) energy += left * left + right * right;
        }
        return energy / rate;
    };
    const auto inBand = tonePower(48000.0, 1000.0, 0.0f);
    REQUIRE(inBand > 0.00001);
    CHECK(tonePower(48000.0, 22000.0, 0.0f) < inBand * 0.0001);
    const auto internalSixKhz = tonePower(32000.0, 1500.0, 24.0f);
    REQUIRE(internalSixKhz > 0.00001);
    CHECK(tonePower(8000.0, 1500.0, 24.0f) < internalSixKhz * 0.001);
}

TEST_CASE("Clouds freezes an existing partial recording immediately", "[clouds][dsp][freeze][partial]")
{
    Engine silenceAfterFreeze, liveAfterFreeze;
    silenceAfterFreeze.prepare(16000.0);
    liveAfterFreeze.prepare(16000.0);
    Parameters parameters;
    const auto first = render(silenceAfterFreeze, 16000.0, 8000, parameters);
    REQUIRE(first == render(liveAfterFreeze, 16000.0, 8000, parameters));
    parameters.freeze = true;
    const auto frozen = render(silenceAfterFreeze, 16000.0, 16000, parameters, 128, true);
    const auto continues = render(liveAfterFreeze, 16000.0, 16000, parameters);
    CHECK(frozen == continues);
    CHECK(energy(frozen) > 0.01);
}
