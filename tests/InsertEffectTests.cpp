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
        p.values[3].baseValue = 0;
    });
    CHECK(synced.getSample(0, 26000) == Catch::Approx(1.0f));
    CHECK(synced.getMagnitude(1, 0, 40000) == 0);
}

TEST_CASE("Insert ping-pong alternates centred mono echoes between left and right", "[insertfx][dsp][delay][stereo]")
{
    juce::AudioBuffer<float> input(2, 20000);
    input.clear(); input.setSample(0, 2000, 1); input.setSample(1, 2000, 1);
    const auto output = render(Type::delay, input, 127, [](auto& p) {
        p.values[0].baseValue = 100; p.values[1].baseValue = 50;
        p.values[3].baseValue = 100; p.values[5].baseValue = 100;
    });
    CHECK(output.getSample(0, 6800) == Catch::Approx(1.0f));
    CHECK(std::abs(output.getSample(1, 6800)) < 0.000001f);
    CHECK(output.getSample(1, 11600) > 0.25f);
    CHECK(std::abs(output.getSample(0, 11600)) < 0.000001f);
    CHECK(output.getSample(0, 16400) > 0.05f);
    CHECK(std::abs(output.getSample(1, 16400)) < 0.000001f);
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

TEST_CASE("Fractional insert history preserves interpolation across wrap and logical reset", "[insertfx][dsp][history]")
{
    fire::effects::StereoHistory history;
    history.prepare(32.0, 0.5); // 24 storage slots, including the interpolation guard.
    std::vector<float> written;
    for (int sample = 0; sample < 2000; ++sample)
    {
        if (sample % 137 == 0)
        {
            history.reset();
            written.clear();
        }
        const float input = static_cast<float>(sample) * 0.1f;
        history.write(input, -input);
        written.push_back(input);
        for (float age : {0.0f, 1.0f, 1.25f, 3.75f, 21.5f, 22.75f, 23.0f, 24.0f})
        {
            const auto whole = static_cast<size_t>(age);
            float expected = 0.0f;
            if (age >= 1.0f && whole <= written.size() && whole + 1 < 24)
                expected = juce::jmap(age - static_cast<float>(whole),
                    written[written.size() - whole],
                    whole < written.size() ? written[written.size() - whole - 1] : 0.0f);
            REQUIRE(history.read(0, age) == expected);
            REQUIRE(history.read(1, age) == -expected);
        }
    }
}

TEST_CASE("Normalised insert controls follow changing bit depth after settling and reprepare", "[insertfx][dsp][lfo][cache]")
{
    InsertEffect effect;
    InsertEffect::Parameters parameters(Type::lofi);
    parameters.normalised = true;
    for (size_t i = 0; i < fire::effects::controlCount; ++i)
    {
        parameters.values[i].range = {0.0f, 1.0f};
        parameters.values[i].baseValue = 0.0f;
    }
    parameters.values[5].baseValue = 1.0f;
    std::array<float, 127> lfo;
    parameters.values[1].lfoSignal = lfo.data();
    parameters.values[1].isBipolar = false;
    parameters.values[1].modulationDepth = 1.0f;
    juce::AudioBuffer<float> audio(2, static_cast<int>(lfo.size()));
    for (double sampleRate : {44100.0, 96000.0})
    {
        effect.prepare({sampleRate, static_cast<juce::uint32>(lfo.size()), 2});
        for (int block = 0; block < 24; ++block)
        {
            for (size_t i = 0; i < lfo.size(); ++i)
            {
                lfo[i] = block < 20 ? 0.25f : (i < 40 ? 0.0f : (i < 80 ? 0.33f : 0.77f));
                audio.setSample(0, static_cast<int>(i), 0.137f);
                audio.setSample(1, static_cast<int>(i), -0.271f);
            }
            effect.process(juce::dsp::AudioBlock<float>(audio), parameters);
            if (block < 20) continue; // Allow the enable ramp to settle.
            for (size_t i = 0; i < lfo.size(); ++i)
            {
                const auto bits = fire::effects::controls(Type::lofi)[1].fromNormalised(lfo[i]);
                const auto steps = std::pow(2.0f, bits - 1.0f);
                CHECK(audio.getSample(0, static_cast<int>(i)) == Catch::Approx(std::round(0.137f * steps) / steps).margin(1.0e-7f));
                CHECK(audio.getSample(1, static_cast<int>(i)) == Catch::Approx(std::round(-0.271f * steps) / steps).margin(1.0e-7f));
            }
        }
    }
}

TEST_CASE("Tape tone coefficients refresh when the sample rate changes", "[insertfx][dsp][tape][prepare]")
{
    fire::effects::TapeFlutter reused;
    reused.prepare(48000.0);
    for (int sample = 0; sample < 1024; ++sample)
    {
        float left = 0.1f, right = -0.2f;
        reused.process(left, right, 0.6f, 0.4f, 0.3f);
    }
    for (double sampleRate : {22050.0, 96000.0})
    {
        fire::effects::TapeFlutter fresh;
        fresh.prepare(sampleRate);
        reused.prepare(sampleRate);
        for (int sample = 0; sample < 4096; ++sample)
        {
            float left = std::sin(static_cast<float>(sample) * 0.05f) * 0.2f;
            float right = -left * 0.5f;
            auto expectedLeft = left, expectedRight = right;
            reused.process(left, right, 0.6f, 0.4f, 0.3f);
            fresh.process(expectedLeft, expectedRight, 0.6f, 0.4f, 0.3f);
            REQUIRE(left == expectedLeft);
            REQUIRE(right == expectedRight);
        }
    }
}

TEST_CASE("Insert delay preserves the first impulse after reset at integer and fractional times", "[insertfx][dsp][delay][history][reset]")
{
    for (float delayMs : {100.0f, 100.01f})
    {
        CAPTURE(delayMs);
        InsertEffect effect;
        effect.prepare({rate, 64, 2});
        InsertEffect::Parameters parameters(Type::delay);
        parameters.values[0].baseValue = delayMs;
        parameters.values[1].baseValue = 0.0f;
        parameters.values[3].baseValue = 0.0f;
        parameters.values[5].baseValue = 100.0f;
        const auto delaySamples = delayMs * 0.001f * static_cast<float>(rate);
        const auto firstEchoSample = static_cast<int>(std::floor(delaySamples));
        const auto fraction = delaySamples - static_cast<float>(firstEchoSample);
        juce::AudioBuffer<float> audio(2, firstEchoSample + 128);
        for (int pass = 0; pass < 2; ++pass)
        {
            CAPTURE(pass);
            audio.clear();
            audio.setSample(0, 0, 1.0f);
            audio.setSample(1, 0, -0.5f);
            effect.process(juce::dsp::AudioBlock<float>(audio), parameters);
            // The enable ramp has finished before this first delayed echo.
            CHECK(audio.getSample(0, firstEchoSample) == Catch::Approx(1.0f - fraction));
            CHECK(audio.getSample(0, firstEchoSample + 1) == Catch::Approx(fraction));
            CHECK(audio.getSample(1, firstEchoSample) == Catch::Approx(-0.5f * (1.0f - fraction)));
            CHECK(audio.getSample(1, firstEchoSample + 1) == Catch::Approx(-0.5f * fraction));
            CHECK(audio.getMagnitude(0, 1, firstEchoSample - 1) == 0.0f);
            effect.reset();
        }
    }
}
