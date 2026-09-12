#include <DSP/InsertEffect.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>

using fire::effects::InsertEffect;
using fire::effects::Type;
namespace
{
constexpr int rate = 48000;
juce::AudioBuffer<float> render(Type type, const juce::AudioBuffer<float>& input, int blockSize,
                                const std::function<void(InsertEffect::Parameters&)>& configure = {})
{
    InsertEffect effect;
    effect.prepare({rate, static_cast<juce::uint32>(blockSize), 2});
    InsertEffect::Parameters parameters(type);
    if (configure) configure(parameters);
    juce::AudioBuffer<float> output;
    output.makeCopyOf(input);
    for (int offset = 0; offset < output.getNumSamples(); offset += blockSize)
        effect.process(juce::dsp::AudioBlock<float>(output).getSubBlock(static_cast<size_t>(offset),
            static_cast<size_t>(juce::jmin(blockSize, output.getNumSamples() - offset))), parameters, offset);
    return output;
}
juce::AudioBuffer<float> signal(int length)
{
    juce::AudioBuffer<float> audio(2, length);
    for (int sample = 0; sample < length; ++sample)
        for (int channel = 0; channel < 2; ++channel)
            audio.setSample(channel, sample, 0.1f * std::sin(static_cast<float>(sample) * 0.057f));
    return audio;
}
float difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float maximum = 0;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int sample = 0; sample < a.getNumSamples(); ++sample)
            maximum = juce::jmax(maximum, std::abs(a.getSample(channel, sample) - b.getSample(channel, sample)));
    return maximum;
}
}

TEST_CASE("Insert effects preserve bypass and zero mix and produce distinct wet signals", "[insertfx][dsp][bypass]")
{
    const auto input = signal(24000);
    for (const auto type : {Type::chorus, Type::delay, Type::reverb, Type::granular, Type::lofi})
    {
        CAPTURE(fire::effects::name(type));
        CHECK(difference(input, render(type, input, 127, [](auto& p) {p.enabled = false;})) == 0);
        CHECK(difference(input, render(type, input, 127, [](auto& p) {p.values[5].baseValue = 0;})) == 0);
        CHECK(difference(input, render(type, input, 127)) > 0.001f);
    }
}

TEST_CASE("Insert delay repeats at its configured time and follows host tempo", "[insertfx][dsp][delay]")
{
    juce::AudioBuffer<float> input(2, 40000);
    input.clear(); input.setSample(0, 2000, 1);
    const auto delay = render(Type::delay, input, 127, [](auto& p) {
        p.values[0].baseValue = 100; p.values[1].baseValue = 0;
        p.values[3].baseValue = 0; p.values[5].baseValue = 100;
    });
    CHECK(delay.getMagnitude(0, 2000, 4799) == 0);
    CHECK(delay.getSample(0, 6800) == Catch::Approx(1.0f));
    const auto synced = render(Type::delay, input, 64, [](auto& p) {
        p.bpm = 120; p.values[4].baseValue = 4; p.values[1].baseValue = 0; p.values[5].baseValue = 100;
    });
    CHECK(synced.getSample(0, 26000) == Catch::Approx(1.0f));
    CHECK(synced.getMagnitude(1, 0, 40000) == 0);
}

TEST_CASE("Insert reverb leaves a decaying stereo tail and reset removes stored audio", "[insertfx][dsp][reverb][reset]")
{
    InsertEffect effect;
    effect.prepare({rate, 256, 2});
    InsertEffect::Parameters p(Type::reverb);
    p.values[5].baseValue = 100;
    juce::AudioBuffer<float> audio(2, rate * 3);
    audio.clear(); audio.setSample(0, 2000, 1);
    effect.process(juce::dsp::AudioBlock<float>(audio), p);
    CHECK(audio.getMagnitude(0, 5000, 20000) > 0.001f);
    CHECK(audio.getMagnitude(1, 5000, 20000) > 0.001f);
    CHECK(audio.getRMSLevel(0, rate * 2, rate) < audio.getRMSLevel(0, 5000, rate));
    effect.reset(); audio.clear();
    effect.process(juce::dsp::AudioBlock<float>(audio), p);
    CHECK(audio.getMagnitude(0, audio.getNumSamples()) == 0);
}

TEST_CASE("Insert effects and LFO recipes are independent of host block partitioning", "[insertfx][dsp][lfo][block-size]")
{
    const auto input = signal(22000);
    std::vector<float> lfo(22000);
    for (size_t i = 0; i < lfo.size(); ++i) lfo[i] = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.001f);
    const auto configure = [&](auto& p) {p.values[0].lfoSignal = lfo.data();};
    for (auto type : {Type::chorus, Type::delay, Type::reverb, Type::granular, Type::lofi})
    {
        CAPTURE(fire::effects::name(type));
        const auto whole = render(type, input, 22000, configure);
        CHECK(difference(whole, render(type, input, 37, configure)) < 0.000001f);
        CHECK(difference(whole, render(type, input, 256, configure)) < 0.000001f);
    }
}

TEST_CASE("Tape flutter is neutral at zero and modulates pitch without adding noise", "[insertfx][dsp][tape]")
{
    fire::effects::TapeFlutter tape;
    tape.prepare(rate);
    float differenceSum = 0;
    for (int sample = 0; sample < rate; ++sample)
    {
        const float input = 0.1f * std::sin(static_cast<float>(sample) * 0.057f);
        auto left = input, right = input;
        tape.process(left, right, 0, 0, 0);
        REQUIRE(left == input); REQUIRE(right == input);
        left = right = input;
        tape.process(left, right, 0.5f, 0.4f, 0.6f);
        CHECK(left == right);
        differenceSum += std::abs(left - input);
    }
    CHECK(differenceSum > 10);
    tape.reset();
    for (int sample = 0; sample < rate; ++sample)
    {
        float left = 0, right = 0;
        tape.process(left, right, 1, 1, 1);
        REQUIRE(left == 0); REQUIRE(right == 0);
    }
}

TEST_CASE("Insert effect switching and invalid controls stay bounded", "[insertfx][dsp][robustness]")
{
    InsertEffect effect;
    effect.prepare({rate, 256, 2});
    juce::AudioBuffer<float> audio(2, 4096);
    for (auto type : {Type::chorus, Type::delay, Type::reverb, Type::granular, Type::lofi, Type::none})
    {
        InsertEffect::Parameters p(type);
        p.values[0].baseValue = std::numeric_limits<float>::quiet_NaN();
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            for (int channel = 0; channel < 2; ++channel) audio.setSample(channel, sample, 0.05f);
        effect.process(juce::dsp::AudioBlock<float>(audio), p);
        for (int sample = 1; sample < audio.getNumSamples(); ++sample)
        {
            REQUIRE(std::isfinite(audio.getSample(0, sample)));
            CHECK(std::abs(audio.getSample(0, sample)) < 1);
            CHECK(std::abs(audio.getSample(0, sample) - audio.getSample(0, sample - 1)) < 0.15f);
        }
    }
}

TEST_CASE("Changing insert type fades the audible LFO value instead of its unmodulated base", "[insertfx][dsp][lfo][transition]")
{
    InsertEffect effect;
    effect.prepare({rate, 9600, 2});
    InsertEffect::Parameters p(Type::delay);
    p.values[0].baseValue = 100; p.values[1].baseValue = 0; p.values[3].baseValue = 0;
    p.values[5].baseValue = 50; p.values[5].modulationDepth = 1;
    std::vector<float> lfo(9600, 1);
    p.values[5].lfoSignal = lfo.data();
    juce::AudioBuffer<float> audio(2, 9600);
    for (int channel = 0; channel < 2; ++channel)
    {
        juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.2f, 8600);
        audio.clear(channel, 8600, 1000);
    }
    effect.process(juce::dsp::AudioBlock<float>(audio), p);
    const auto previous = audio.getSample(0, 9599);
    REQUIRE(previous == Catch::Approx(0.2f));
    audio.clear(); p.type = Type::chorus;
    effect.process(juce::dsp::AudioBlock<float>(audio).getSubBlock(0, 512), p);
    CHECK(std::abs(audio.getSample(0, 0) - previous) < 0.001f);
}
