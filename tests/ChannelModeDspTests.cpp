#include <DSP/ChannelModeProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace
{
using ChannelProcessor = fire::dsp::ChannelModeProcessor;
namespace modes = fire::channel_mode;
constexpr juce::dsp::ProcessSpec channelSpec {48000, 64, 2};
using ReferenceDelay = juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::Thiran>;
float midAt(const juce::AudioBuffer<float>& audio, int sample)
{ return .5f * audio.getSample(0, sample) + .5f * audio.getSample(1, sample); }
float sideAt(const juce::AudioBuffer<float>& audio, int sample)
{ return .5f * audio.getSample(0, sample) - .5f * audio.getSample(1, sample); }
}

TEST_CASE("Channel mode Stereo leaves every sample bit unchanged", "[channel-mode][dsp]")
{
    ChannelProcessor processor; processor.prepare(channelSpec, 32);
    juce::AudioBuffer<float> audio(2, 8);
    const std::array<float, 8> values {0, -0.0f, .13f, -.42f, 1, -1,
        std::numeric_limits<float>::min(), std::numeric_limits<float>::max()};
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < 8; ++i) audio.setSample(channel, i, values[static_cast<size_t>(i)]);
    for (int mode : std::array<int, 3>{modes::stereo, -1, 999})
    {
        processor.begin(audio, mode, 7.25f); processor.end(audio);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 8; ++i)
                CHECK(std::bit_cast<std::uint32_t>(audio.getSample(channel, i))
                    == std::bit_cast<std::uint32_t>(values[static_cast<size_t>(i)]));
    }
}

TEST_CASE("Channel mode isolates the selected component and folds stereo effects", "[channel-mode][dsp]")
{
    for (int mode : {modes::mid, modes::side})
    {
        ChannelProcessor processor; processor.prepare(channelSpec, 32);
        juce::AudioBuffer<float> audio(2, 64);
        std::array<float, 64> mids {}, sides {};
        for (int i = 0; i < 64; ++i)
        {
            mids[static_cast<size_t>(i)] = .35f * std::sin(static_cast<float>(i) * .11f);
            sides[static_cast<size_t>(i)] = .21f * std::cos(static_cast<float>(i) * .23f);
            audio.setSample(0, i, mids[static_cast<size_t>(i)] + sides[static_cast<size_t>(i)]);
            audio.setSample(1, i, mids[static_cast<size_t>(i)] - sides[static_cast<size_t>(i)]);
        }
        processor.begin(audio, mode, 0);
        for (int i = 0; i < 64; ++i)
        {
            const auto selected = mode == modes::mid ? mids[static_cast<size_t>(i)] : sides[static_cast<size_t>(i)];
            CHECK(audio.getSample(0, i) == Catch::Approx(selected).margin(1e-7));
            CHECK(audio.getSample(1, i) == audio.getSample(0, i));
        }
        audio.applyGain(0, 0, 64, 2); audio.applyGain(1, 0, 64, 4);
        processor.end(audio);
        for (int i = 0; i < 64; ++i)
        {
            CHECK(midAt(audio, i) == Catch::Approx(mids[static_cast<size_t>(i)] * (mode == modes::mid ? 3 : 1)).margin(2e-7));
            CHECK(sideAt(audio, i) == Catch::Approx(sides[static_cast<size_t>(i)] * (mode == modes::side ? 3 : 1)).margin(2e-7));
        }
    }
}

TEST_CASE("Channel mode identity chains align retained and processed components at fractional latency", "[channel-mode][dsp][latency]")
{
    for (float latency : {0.0f, 16.0f, 7.375f})
        for (int mode : {modes::mid, modes::side})
        {
            ChannelProcessor processor; processor.prepare(channelSpec, 32);
            ReferenceDelay wet(32), reference(32);
            wet.prepare(channelSpec); reference.prepare(channelSpec);
            wet.setDelay(latency); reference.setDelay(latency);
            juce::AudioBuffer<float> audio(2, 64), expected(2, 64);
            for (int block = 0; block < 8; ++block)
            {
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 64; ++i)
                    {
                        const auto input = .4f * std::sin(static_cast<float>(block * 64 + i) * (channel == 0 ? .23f : .17f));
                        audio.setSample(channel, i, input);
                        reference.pushSample(channel, input);
                        expected.setSample(channel, i, static_cast<float>(reference.popSample(channel)));
                    }
                processor.begin(audio, mode, latency);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 64; ++i)
                    {
                        wet.pushSample(channel, audio.getSample(channel, i));
                        audio.setSample(channel, i, static_cast<float>(wet.popSample(channel)));
                    }
                processor.end(audio);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 64; ++i)
                        CHECK(audio.getSample(channel, i) == Catch::Approx(expected.getSample(channel, i)).margin(2e-7));
            }
        }
}

TEST_CASE("Channel mode retains delay history while Stereo is audible", "[channel-mode][dsp][latency]")
{
    ChannelProcessor processor; processor.prepare(channelSpec, 32);
    juce::AudioBuffer<float> audio(2, 1);
    for (int i = 0; i < 9; ++i)
    {
        audio.setSample(0, 0, i == 0 ? .75f : 0); audio.setSample(1, 0, i == 0 ? -.75f : 0);
        processor.begin(audio, i < 8 ? modes::stereo : modes::mid, 8);
        if (i == 8) audio.clear(); // Mid chain has nothing; retained Side must carry the impulse.
        processor.end(audio);
        if (i == 8) { CHECK(audio.getSample(0, 0) == .75f); CHECK(audio.getSample(1, 0) == -.75f); }
    }
}

TEST_CASE("Mono Mid processes normally and mono Side only returns aligned original audio", "[channel-mode][dsp][mono]")
{
    auto spec = channelSpec; spec.numChannels = 1;
    for (int mode : {modes::mid, modes::side})
    {
        ChannelProcessor processor; processor.prepare(spec, 32);
        juce::AudioBuffer<float> audio(1, 64);
        audio.clear(); audio.setSample(0, 0, .5f);
        processor.begin(audio, mode, mode == modes::side ? 9 : 0);
        if (mode == modes::mid) audio.applyGain(2);
        else
        {
            CHECK(audio.getMagnitude(0, 64) == 0);
            for (int i = 0; i < 64; ++i) audio.setSample(0, i, 3); // Bias/frozen audio must not manufacture Side.
        }
        processor.end(audio);
        for (int i = 0; i < 64; ++i)
            CHECK(audio.getSample(0, i) == (mode == modes::mid ? (i == 0 ? 1.0f : 0) : (i == 9 ? .5f : 0)));
    }
}

TEST_CASE("Channel component arithmetic cannot overflow or poison its delay history", "[channel-mode][dsp][non-finite]")
{
    for (int mode : {modes::mid, modes::side})
    {
        ChannelProcessor processor; processor.prepare(channelSpec, 32);
        juce::AudioBuffer<float> audio(2, 64);
        for (int pass = 0; pass < 12; ++pass)
        {
            for (int i = 0; i < 64; ++i)
            {
                audio.setSample(0, i, pass == 0 ? std::numeric_limits<float>::max() : 0);
                audio.setSample(1, i, pass == 0 ? -std::numeric_limits<float>::max() : 0);
            }
            if (pass == 1)
            { audio.setSample(0, 0, std::numeric_limits<float>::infinity()); audio.setSample(1, 0, std::numeric_limits<float>::quiet_NaN()); }
            processor.begin(audio, mode, 7.375f);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 64; ++i) REQUIRE(std::isfinite(audio.getSample(channel, i)));
            processor.end(audio);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 64; ++i) REQUIRE(std::isfinite(audio.getSample(channel, i)));
        }
        processor.reset(); audio.clear(); processor.begin(audio, mode, 7.375f); processor.end(audio);
        CHECK(audio.getMagnitude(0, 64) == 0);
    }
}

TEST_CASE("Channel component delay is independent of host block partitions", "[channel-mode][dsp]")
{
    const auto render = [](int blockSize)
    {
        ChannelProcessor processor; processor.prepare(channelSpec, 32);
        juce::AudioBuffer<float> output(2, 512);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i) output.setSample(channel, i, .3f * std::sin(static_cast<float>(i) * (.07f + .1f * channel)));
        for (int start = 0; start < 512; start += blockSize)
        {
            juce::AudioBuffer<float> part(output.getArrayOfWritePointers(), 2, start, juce::jmin(blockSize, 512 - start));
            processor.begin(part, modes::side, 13.25f);
            part.applyGain(.4f);
            processor.end(part);
        }
        return output;
    };
    const auto reference = render(64);
    for (int size : {1, 7, 31})
    {
        const auto actual = render(size);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i) CHECK(actual.getSample(channel, i) == reference.getSample(channel, i));
    }
}
