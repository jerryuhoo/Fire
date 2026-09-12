#include <DSP/InsertRack.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr float input = 0.2f;
constexpr int routeRampSamples = 480;

fire::effects::RackParameters makeParameters()
{
    using namespace fire::effects;
    RackParameters parameters;
    auto& slot = parameters[0];
    slot.effect.type = Type::delay;
    slot.effect.normalised = true;
    for (size_t i = 0; i < controlCount; ++i)
    {
        const auto& control = controls(Type::delay)[i];
        slot.effect.values[i].baseValue = control.toNormalised(control.initial);
        slot.effect.values[i].range = {0.0f, 1.0f};
    }
    slot.effect.values[0].baseValue = 1.0f; // Two-second delay: no echoes in these probes.
    slot.effect.values[1].baseValue = 0.0f;
    slot.effect.values[3].baseValue = 0.0f;
    slot.effect.values[5].baseValue = 0.0f;
    slot.effect.values[5].modulationDepth = 1.0f;
    slot.effect.values[5].isBipolar = false;
    slot.sources[5] = 0;
    return parameters;
}

struct Fixture
{
    fire::effects::InsertRack rack;
    fire::effects::RackParameters parameters = makeParameters();
    juce::AudioBuffer<float> lfo {4, 4096};

    Fixture()
    {
        rack.prepare({sampleRate, 128, 2});
        lfo.clear();
        juce::FloatVectorOperations::fill(lfo.getWritePointer(0), 1.0f, lfo.getNumSamples());
    }

    juce::AudioBuffer<float> process(int samples)
    {
        juce::AudioBuffer<float> audio(2, samples);
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(audio.getWritePointer(channel), input, samples);
        rack.processSlot(juce::dsp::AudioBlock<float>(audio), 0, parameters, lfo);
        return audio;
    }
};
}

TEST_CASE("Insert LFO assignment source depth polarity and removal bridge the last audible value", "[insertfx][dsp][lfo][routing][transition]")
{
    Fixture fixture;
    REQUIRE(fixture.process(2048).getSample(0, 2047) == 0.0f);
    auto& slot = fixture.parameters[0];
    auto& mix = slot.effect.values[5];
    const auto checkTransition = [&](float before, float after)
    {
        const auto audio = fixture.process(routeRampSamples + 1);
        CHECK(audio.getSample(0, 0) == Catch::Approx(before).margin(1.0e-7f));
        CHECK(audio.getSample(0, routeRampSamples / 2) == Catch::Approx((before + after) * 0.5f).margin(2.0e-6f));
        CHECK(audio.getSample(0, routeRampSamples) == Catch::Approx(after).margin(1.0e-7f));
        for (int sample = 1; sample < audio.getNumSamples(); ++sample)
        {
            REQUIRE(std::abs(audio.getSample(0, sample) - audio.getSample(0, sample - 1))
                    <= std::abs(after - before) / static_cast<float>(routeRampSamples) + 2.0e-6f);
            REQUIRE(audio.getSample(0, sample) == audio.getSample(1, sample));
        }
    };
    slot.sources[5] = -1;
    checkTransition(0.0f, input);
    slot.sources[5] = 0;
    checkTransition(input, 0.0f);
    slot.sources[5] = 1;
    checkTransition(0.0f, input);
    slot.sources[5] = 0;
    checkTransition(input, 0.0f);
    mix.modulationDepth = 0.5f;
    checkTransition(0.0f, input * 0.5f);
    mix.isBipolar = true;
    checkTransition(input * 0.5f, input * 0.75f);
    mix.modulationDepth = -0.5f;
    checkTransition(input * 0.75f, input);
}

TEST_CASE("Retargeting an insert LFO bridge holds the currently applied value", "[insertfx][dsp][lfo][routing][transition]")
{
    Fixture fixture;
    fixture.process(2048);
    fixture.parameters[0].sources[5] = -1;
    const auto removing = fixture.process(200);
    const auto last = removing.getSample(0, removing.getNumSamples() - 1);
    REQUIRE(last > 0.0f);
    REQUIRE(last < input);
    fixture.parameters[0].sources[5] = 0;
    const auto reassigned = fixture.process(routeRampSamples + 1);
    CHECK(reassigned.getSample(0, 0) == last);
    CHECK(reassigned.getSample(0, routeRampSamples) == 0.0f);
}

TEST_CASE("Stable insert LFOs remain sample accurate when their buffer address changes", "[insertfx][dsp][lfo][routing]")
{
    Fixture fixture;
    fixture.process(2048);
    constexpr int samples = 64;
    juce::AudioBuffer<float> replacementLfo(4, samples);
    replacementLfo.clear();
    for (int sample = 0; sample < samples; ++sample)
        replacementLfo.setSample(0, sample, sample % 2 == 0 ? 0.0f : 1.0f);
    juce::AudioBuffer<float> audio(2, samples);
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::fill(audio.getWritePointer(channel), input, samples);
    fixture.rack.processSlot(juce::dsp::AudioBlock<float>(audio), 0, fixture.parameters, replacementLfo);
    for (int sample = 0; sample < samples; ++sample)
        REQUIRE(audio.getSample(0, sample) == (sample % 2 == 0 ? input : 0.0f));
}

TEST_CASE("Insert LFO recipe transitions are independent of callback partitioning", "[insertfx][dsp][lfo][routing][block-size]")
{
    constexpr int length = 6144;
    const auto render = [&](int blockSize)
    {
        fire::effects::InsertRack rack;
        rack.prepare({sampleRate, static_cast<juce::uint32>(blockSize), 2});
        auto parameters = makeParameters();
        juce::AudioBuffer<float> audio(2, length), lfo(4, length);
        lfo.clear();
        for (int sample = 0; sample < length; ++sample)
        {
            const float signal = 0.5f + 0.5f * std::sin(static_cast<float>(sample) * 0.013f);
            lfo.setSample(0, sample, signal);
            lfo.setSample(1, sample, 1.0f - signal);
            audio.setSample(0, sample, input);
            audio.setSample(1, sample, -input * 0.5f);
        }
        for (int phase = 0; phase < length / 512; ++phase)
        {
            auto& slot = parameters[0];
            if (phase == 4 || phase == 9) slot.sources[5] = -1;
            if (phase == 5) slot.sources[5] = 0;
            if (phase == 6) slot.sources[5] = 1;
            if (phase == 7) slot.effect.values[5].modulationDepth = 0.7f;
            if (phase == 8) slot.effect.values[5].isBipolar = true;
            for (int offset = phase * 512; offset < (phase + 1) * 512; offset += blockSize)
            {
                const auto count = std::min(blockSize, (phase + 1) * 512 - offset);
                rack.processSlot(juce::dsp::AudioBlock<float>(audio).getSubBlock(
                    static_cast<size_t>(offset), static_cast<size_t>(count)), 0, parameters, lfo, offset);
            }
        }
        return audio;
    };
    const auto reference = render(512);
    for (int blockSize : {1, 37, 128})
    {
        CAPTURE(blockSize);
        const auto audio = render(blockSize);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < length; ++sample)
                REQUIRE(audio.getSample(channel, sample) == reference.getSample(channel, sample));
    }
}
