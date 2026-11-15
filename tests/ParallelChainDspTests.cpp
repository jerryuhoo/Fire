#include <DSP/ParallelChainProcessor.h>
#include <DSP/InsertRack.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
using Parallel = fire::dsp::ParallelChainProcessor;
using TestDelay = juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::Thiran>;
ModulatedValueProvider blendProvider(float value)
{ ModulatedValueProvider p; p.baseValue = value; p.range = {0, 1}; return p; }
void delayBranch(juce::AudioBuffer<float>& audio, TestDelay& delay)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            delay.pushSample(channel, audio.getSample(channel, sample));
            audio.setSample(channel, sample, static_cast<float>(delay.popSample(channel)));
        }
}
}

TEST_CASE("Parallel branches share their input and blend linearly at both endpoints", "[parallel-chain-dsp][dsp]")
{
    for (int channels : {1, 2})
        for (float blend : {0.0f, .5f, 1.0f})
        {
            Parallel processor; processor.prepare({48000, 64, static_cast<juce::uint32>(channels)}, 32);
            juce::AudioBuffer<float> audio(channels, 64);
            for (int channel = 0; channel < channels; ++channel)
                for (int sample = 0; sample < 64; ++sample) audio.setSample(channel, sample, channel == 0 ? .125f : -.25f);
            auto second = processor.begin(audio);
            audio.applyGain(2); second.applyGain(4);
            processor.finish(audio, 0, 0, blendProvider(blend), blend, -1);
            const auto expected = 2 * (1 - blend) + 4 * blend;
            for (int sample = 0; sample < 64; ++sample)
            {
                CHECK(audio.getSample(0, sample) == .125f * expected);
                if (channels == 2) CHECK(audio.getSample(1, sample) == -.25f * expected);
            }
        }
}

TEST_CASE("Parallel padding aligns different branch lengths to one unchanged total budget", "[parallel-chain-dsp][dsp][latency]")
{
    for (float total : {20.0f, 20.375f})
    {
        Parallel processor; processor.prepare({48000, 64, 2}, 32);
        TestDelay a(32), b(32), reference(32);
        for (auto* delay : {&a, &b, &reference}) delay->prepare({48000, 64, 2});
        a.setDelay(3); b.setDelay(11); reference.setDelay(total);
        juce::AudioBuffer<float> audio(2, 64), expected(2, 64);
        for (int offset = 0; offset < 1024; offset += 64)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 64; ++sample)
                {
                    const auto value = .3f * std::sin(static_cast<float>(offset + sample) * (.07f + channel * .11f));
                    audio.setSample(channel, sample, value); expected.setSample(channel, sample, value);
                }
            auto second = processor.begin(audio);
            delayBranch(audio, a); delayBranch(second, b); delayBranch(expected, reference);
            processor.finish(audio, total - 3, total - 11, blendProvider(.5f), .5f, -1);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 64; ++sample)
                    CHECK(audio.getSample(channel, sample) == Catch::Approx(expected.getSample(channel, sample)).margin(1e-7));
        }
    }
}

TEST_CASE("Parallel Blend follows each routed sample without extra coefficient smoothing", "[parallel-chain-dsp][dsp][modulation]")
{
    Parallel processor; processor.prepare({48000, 64, 1}, 0);
    juce::AudioBuffer<float> audio(1, 64); audio.clear();
    auto second = processor.begin(audio);
    for (int sample = 0; sample < 64; ++sample) second.setSample(0, sample, 1);
    std::array<float, 64> modulation;
    for (int sample = 0; sample < 64; ++sample) modulation[static_cast<size_t>(sample)] = static_cast<float>(sample) / 63;
    auto provider = blendProvider(0);
    provider.modulationDepth = 1; provider.isBipolar = false; provider.lfoSignal = modulation.data();
    processor.finish(audio, 0, 0, provider, 0, 24);
    for (int sample = 0; sample < 64; ++sample)
        CHECK(audio.getSample(0, sample) == Catch::Approx(modulation[static_cast<size_t>(sample)]).margin(1e-7));
}

TEST_CASE("Parallel scalar and routed Blend transitions are independent of callback partition", "[parallel-chain-dsp][dsp][modulation]")
{
    const auto render = [](int blockSize)
    {
        Parallel processor; processor.prepare({48000, 257, 1}, 32);
        std::vector<float> result(4096), modulation(4096);
        for (int sample = 0; sample < 4096; ++sample) modulation[static_cast<size_t>(sample)] = .3f * std::sin(static_cast<float>(sample) * .01f);
        juce::AudioBuffer<float> audio(1, 257);
        int offset = 0;
        while (offset < 4096)
        {
            const auto nextBoundary = offset < 512 ? 512 : offset < 1024 ? 1024 : offset < 2048 ? 2048 : 4096;
            const auto count = juce::jmin(blockSize, nextBoundary - offset);
            juce::AudioBuffer<float> part(audio.getArrayOfWritePointers(), 1, count);
            for (int sample = 0; sample < count; ++sample) part.setSample(0, sample, .25f * std::sin(static_cast<float>(offset + sample) * .17f));
            auto second = processor.begin(part); second.applyGain(-.5f);
            const auto base = offset < 512 ? .2f : .8f;
            auto provider = blendProvider(base);
            const bool routed = offset >= 1024 && offset < 2048;
            if (routed)
            {
                provider.modulationDepth = 1; provider.signalIsOffset = true;
                provider.lfoSignal = modulation.data() + offset;
            }
            processor.finish(part, 7.25f, 1.25f, provider, base, routed ? 28 : -1);
            for (int sample = 0; sample < count; ++sample) result[static_cast<size_t>(offset + sample)] = part.getSample(0, sample);
            offset += count;
        }
        return result;
    };
    const auto reference = render(1);
    for (int size : {7, 64, 257})
    {
        const auto actual = render(size);
        for (size_t sample = 0; sample < reference.size(); ++sample)
            CHECK(actual[sample] == Catch::Approx(reference[sample]).margin(2e-7));
    }
}

TEST_CASE("Prepared parallel scratch is reused and delay reset preserves Blend smoothing", "[parallel-chain-dsp][dsp][lifecycle]")
{
    Parallel reset, uninterrupted;
    for (auto* processor : {&reset, &uninterrupted}) processor->prepare({48000, 257, 1}, 32);
    juce::AudioBuffer<float> a(1, 257), b(1, 257);
    const float* storage = nullptr;
    for (int pass = 0; pass < 8; ++pass)
    {
        const int count = pass % 2 == 0 ? 257 : 7;
        juce::AudioBuffer<float> left(a.getArrayOfWritePointers(), 1, count), right(b.getArrayOfWritePointers(), 1, count);
        left.clear(); right.clear();
        if (pass == 4) reset.resetDelays();
        auto l = reset.begin(left), r = uninterrupted.begin(right);
        if (storage == nullptr) storage = l.getReadPointer(0);
        CHECK(l.getReadPointer(0) == storage);
        for (int sample = 0; sample < count; ++sample) { l.setSample(0, sample, 1); r.setSample(0, sample, 1); }
        const auto blend = pass == 0 ? 0.0f : 1.0f;
        reset.finish(left, 0, 0, blendProvider(blend), blend, -1);
        uninterrupted.finish(right, 0, 0, blendProvider(blend), blend, -1);
        for (int sample = 0; sample < count; ++sample) CHECK(left.getSample(0, sample) == right.getSample(0, sample));
    }
    reset.reset(); a.clear();
    auto second = reset.begin(a); second.clear();
    reset.finish(a, 16, 16, blendProvider(.5f), .5f, -1);
    CHECK(a.getMagnitude(0, 257) == 0);
}

TEST_CASE("Parallel delay histories remain finite with extreme and malformed branch audio", "[parallel-chain-dsp][dsp][non-finite]")
{
    Parallel processor; processor.prepare({48000, 64, 2}, 32);
    juce::AudioBuffer<float> audio(2, 64);
    for (int pass = 0; pass < 32; ++pass)
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 64; ++sample)
                audio.setSample(channel, sample, pass == 0 ? (sample % 2 ? -1 : 1) * std::numeric_limits<float>::max() : 0);
        auto second = processor.begin(audio);
        if (pass == 0)
        {
            second.setSample(0, 3, std::numeric_limits<float>::infinity());
            second.setSample(1, 7, std::numeric_limits<float>::quiet_NaN());
        }
        processor.finish(audio, 7.375f, 12.125f, blendProvider(.5f), .5f, -1);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 64; ++sample) REQUIRE(std::isfinite(audio.getSample(channel, sample)));
    }
    CHECK(audio.getMagnitude(0, 64) < 1e-8f);
}

TEST_CASE("Insert rack exposes only the active per-slot timing budget", "[parallel-chain-dsp][dsp][latency]")
{
    fire::effects::InsertRack rack; rack.prepare({48000, 64, 2}, true, false);
    rack.setLatencyActive(false);
    CHECK(rack.getProcessingSlotLatency() == 0);
    rack.setLatencyActive(true);
    CHECK(rack.getProcessingSlotLatency() * fire::effects::slotCount == rack.getProcessingBudget());
    CHECK(rack.getProcessingBudget() == rack.getReservedLatency());
}

TEST_CASE("Zero parallel padding preserves tiny samples immediately after extreme finite values", "[parallel-chain-dsp][dsp][latency]")
{
    const std::array<float, 8> input {std::numeric_limits<float>::max(), 1e-20f,
        -std::numeric_limits<float>::max(), -1e-20f, .125f, -.25f, 0, 1};
    for (float blend : {0.0f, .5f, 1.0f})
    {
        Parallel processor; processor.prepare({48000, 8, 1}, 32);
        juce::AudioBuffer<float> audio(1, 8);
        for (int sample = 0; sample < 8; ++sample) audio.setSample(0, sample, input[static_cast<size_t>(sample)]);
        auto second = processor.begin(audio);
        juce::ignoreUnused(second);
        processor.finish(audio, 0, 0, blendProvider(blend), blend, -1);
        for (int sample = 0; sample < 8; ++sample) CHECK(audio.getSample(0, sample) == input[static_cast<size_t>(sample)]);
    }
}
