#include <DSP/InsertEffect.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace
{
using fire::effects::InsertEffect;
using fire::effects::Type;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

InsertEffect::Parameters staticParameters(Type type, float mix = 100.0f)
{
    InsertEffect::Parameters parameters(type);
    parameters.values[1].baseValue = 0.0f;
    parameters.values[2].baseValue = type == Type::flanger ? 2.0f : 1000.0f;
    parameters.values[3].baseValue = 0.0f;
    parameters.values[4].baseValue = 0.0f;
    parameters.values[5].baseValue = mix;
    return parameters;
}

juce::AudioBuffer<float> tone(double sampleRate, double frequency, int samples, int channels = 2)
{
    juce::AudioBuffer<float> audio(channels, samples);
    for (int i = 0; i < samples; ++i)
        for (int channel = 0; channel < channels; ++channel)
            audio.setSample(channel, i, static_cast<float>(0.2 * std::sin(twoPi * frequency * i / sampleRate)));
    return audio;
}

void processChunks(InsertEffect& effect, juce::AudioBuffer<float>& audio,
                   const InsertEffect::Parameters& parameters, int blockSize)
{
    const std::array<int, fire::effects::controlCount> sources {{2, -1, 7, 11, -1, -1}};
    for (int offset = 0; offset < audio.getNumSamples(); offset += blockSize)
        effect.process(juce::dsp::AudioBlock<float>(audio).getSubBlock(static_cast<size_t>(offset),
            static_cast<size_t>(std::min(blockSize, audio.getNumSamples() - offset))), parameters, offset, &sources);
}

juce::AudioBuffer<float> render(double sampleRate, const juce::AudioBuffer<float>& input,
                              const InsertEffect::Parameters& parameters, int blockSize = 127)
{
    InsertEffect effect;
    effect.prepare({sampleRate, static_cast<juce::uint32>(blockSize),
                    static_cast<juce::uint32>(input.getNumChannels())});
    juce::AudioBuffer<float> output;
    output.makeCopyOf(input);
    processChunks(effect, output, parameters, blockSize);
    return output;
}

float maximumDifference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float result = 0.0f;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int i = 0; i < a.getNumSamples(); ++i)
            result = std::max(result, std::abs(a.getSample(channel, i) - b.getSample(channel, i)));
    return result;
}

double steadyGain(double sampleRate, double frequency, const InsertEffect::Parameters& parameters)
{
    const auto warmup = static_cast<int>(sampleRate / 10.0);
    const auto measured = static_cast<int>(sampleRate / 5.0);
    const auto input = tone(sampleRate, frequency, warmup + measured);
    const auto output = render(sampleRate, input, parameters);
    return output.getRMSLevel(0, warmup, measured) / input.getRMSLevel(0, warmup, measured);
}
}

TEST_CASE("Flanger has the comb response of its short delay", "[insertfx][modulation-insert][dsp][frequency]")
{
    for (double sampleRate : {8000.0, 48000.0, 96000.0})
    {
        CAPTURE(sampleRate);
        const auto parameters = staticParameters(Type::flanger, 50.0f);
        // A 2 ms wet delay cancels every odd multiple of 250 Hz at half mix,
        // and adds in phase at integer multiples of 500 Hz.
        CHECK(steadyGain(sampleRate, 250.0, parameters) < 0.0001);
        CHECK(steadyGain(sampleRate, 750.0, parameters) < 0.0001);
        CHECK(steadyGain(sampleRate, 500.0, parameters) == Catch::Approx(1.0).margin(0.0001));
        CHECK(steadyGain(sampleRate, 1000.0, staticParameters(Type::flanger))
              == Catch::Approx(1.0).margin(0.0001));
    }
}

TEST_CASE("Six stage Phaser preserves wet magnitude and makes the expected mixed notches",
          "[insertfx][modulation-insert][dsp][frequency]")
{
    for (double sampleRate : {8000.0, 44100.0, 96000.0})
    {
        CAPTURE(sampleRate);
        const auto tangent = std::tan(juce::MathConstants<double>::pi * 1000.0 / sampleRate);
        const auto a = (tangent - 1.0) / (tangent + 1.0);
        for (double frequency : {100.0, 500.0, 1000.0, 2500.0})
        {
            CAPTURE(frequency);
            CHECK(steadyGain(sampleRate, frequency, staticParameters(Type::phaser))
                  == Catch::Approx(1.0).margin(0.0002));
            const auto z = std::polar(1.0, -twoPi * frequency / sampleRate);
            const auto wet = std::pow((a + z) / (1.0 + a * z), 6);
            const auto expected = std::abs((1.0 + wet) * 0.5);
            CHECK(steadyGain(sampleRate, frequency, staticParameters(Type::phaser, 50.0f))
                  == Catch::Approx(expected).margin(0.0002));
        }
        CHECK(steadyGain(sampleRate, 1000.0, staticParameters(Type::phaser, 50.0f)) < 0.0001);
    }
}

TEST_CASE("Flanger and Phaser LFO processing is independent of callback partitioning",
          "[insertfx][modulation-insert][dsp][lfo][block-size]")
{
    constexpr int samples = 14003;
    const auto input = tone(44100.0, 731.0, samples);
    std::array<std::vector<float>, 3> modulation;
    for (size_t lane = 0; lane < modulation.size(); ++lane)
    {
        modulation[lane].resize(samples);
        for (int i = 0; i < samples; ++i)
            modulation[lane][static_cast<size_t>(i)] = static_cast<float>(0.5 + 0.5
                * std::sin(static_cast<double>(i) * (0.013 + static_cast<double>(lane) * 0.029)));
    }
    for (auto type : {Type::flanger, Type::phaser})
    {
        CAPTURE(fire::effects::name(type));
        InsertEffect::Parameters parameters(type);
        parameters.normalised = true;
        for (size_t i = 0; i < parameters.values.size(); ++i)
        {
            auto& provider = parameters.values[i];
            provider.baseValue = fire::effects::controls(type)[i].toNormalised(provider.baseValue);
            provider.range = {0.0f, 1.0f};
        }
        constexpr std::array<size_t, 3> controls {{0, 2, 3}};
        for (size_t lane = 0; lane < controls.size(); ++lane)
        {
            auto& provider = parameters.values[controls[lane]];
            provider.lfoSignal = modulation[lane].data();
            provider.modulationDepth = 0.8f;
        }
        const auto whole = render(44100.0, input, parameters, samples);
        CHECK(maximumDifference(whole, input) > 0.01f);
        for (int blockSize : {1, 37, 512})
        {
            CAPTURE(blockSize);
            CHECK(maximumDifference(whole, render(44100.0, input, parameters, blockSize)) < 0.000001f);
        }
    }
}

TEST_CASE("Fast Phaser coefficient modulation cannot add energy to its zero feedback allpass cascade",
          "[insertfx][modulation-insert][dsp][stability]")
{
    constexpr int samples = 26048, impulse = 2048;
    auto parameters = staticParameters(Type::phaser);
    parameters.values[0].baseValue = 8.0f;
    parameters.values[1].baseValue = 100.0f;
    parameters.values[4].baseValue = 100.0f;
    parameters.values[2].modulationDepth = 1.0f;
    std::vector<float> modulation(samples);
    for (int i = 0; i < samples; ++i)
        modulation[static_cast<size_t>(i)] = (i / 13) % 2 == 0 ? 0.0f : 1.0f;
    parameters.values[2].lfoSignal = modulation.data();
    juce::AudioBuffer<float> input(2, samples);
    input.clear();
    input.setSample(0, impulse, 0.5f);
    input.setSample(1, impulse, 0.5f);
    const auto output = render(48000.0, input, parameters, 37);
    for (int channel = 0; channel < 2; ++channel)
    {
        double energy = 0.0;
        for (int i = 0; i < samples; ++i)
        {
            const auto value = output.getSample(channel, i);
            REQUIRE(std::isfinite(value));
            energy += static_cast<double>(value) * value;
        }
        CAPTURE(channel, energy);
        CHECK(energy > 0.00001);
        CHECK(energy <= 0.250005);
    }
}

TEST_CASE("Modulation insert stereo width preserves centred and mono signals",
          "[insertfx][modulation-insert][dsp][stereo]")
{
    for (auto type : {Type::flanger, Type::phaser})
    {
        CAPTURE(fire::effects::name(type));
        InsertEffect::Parameters parameters(type);
        parameters.values[0].baseValue = 4.0f;
        parameters.values[5].baseValue = 100.0f;
        const auto stereo = tone(48000.0, 731.0, 18000);
        parameters.values[4].baseValue = 0.0f;
        const auto centred = render(48000.0, stereo, parameters);
        double centredDifference = 0.0;
        for (int i = 0; i < centred.getNumSamples(); ++i)
            centredDifference += std::abs(centred.getSample(0, i) - centred.getSample(1, i));
        CHECK(centredDifference == 0.0);
        const auto mono = tone(48000.0, 731.0, 18000, 1);
        const auto monoNarrow = render(48000.0, mono, parameters);
        parameters.values[4].baseValue = 100.0f;
        const auto wide = render(48000.0, stereo, parameters);
        double wideDifference = 0.0;
        for (int i = 0; i < wide.getNumSamples(); ++i)
            wideDifference += std::abs(wide.getSample(0, i) - wide.getSample(1, i));
        CHECK(wideDifference > 1.0);
        CHECK(maximumDifference(monoNarrow, render(48000.0, mono, parameters)) == 0.0f);
    }
}

TEST_CASE("Modulation inserts remain finite at extreme feedback and clear their history on reset",
          "[insertfx][modulation-insert][dsp][stability][reset]")
{
    for (auto type : {Type::flanger, Type::phaser})
        for (double sampleRate : {8000.0, 48000.0, 192000.0})
            for (bool positive : {false, true})
            {
                CAPTURE(fire::effects::name(type), sampleRate, positive);
                InsertEffect effect;
                effect.prepare({sampleRate, 127, 2});
                InsertEffect::Parameters parameters(type);
                const auto& descriptors = fire::effects::controls(type);
                parameters.values[0].baseValue = descriptors[0].maximum;
                parameters.values[1].baseValue = 100.0f;
                parameters.values[2].baseValue = positive ? descriptors[2].maximum : descriptors[2].minimum;
                parameters.values[3].baseValue = positive ? descriptors[3].maximum : descriptors[3].minimum;
                parameters.values[4].baseValue = 100.0f;
                parameters.values[5].baseValue = 100.0f;
                auto audio = tone(sampleRate, 731.0, 16000);
                processChunks(effect, audio, parameters, 127);
                float maximum = 0.0f;
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < audio.getNumSamples(); ++i)
                    {
                        const auto value = audio.getSample(channel, i);
                        REQUIRE(std::isfinite(value));
                        maximum = std::max(maximum, std::abs(value));
                    }
                CHECK(maximum > 0.001f);
                CHECK(maximum < 2.0f);
                effect.reset();
                audio.clear();
                processChunks(effect, audio, parameters, 127);
                CHECK(audio.getMagnitude(0, audio.getNumSamples()) == 0.0f);
                // Reset must also restart the oscillator and coefficient clock.
                effect.reset();
                const auto input = tone(sampleRate, 731.0, 8000);
                audio.makeCopyOf(input);
                processChunks(effect, audio, parameters, 127);
                CHECK(maximumDifference(audio, render(sampleRate, input, parameters)) == 0.0f);
            }
}

TEST_CASE("Switching modulation insert type fades through dry and bypass clears the old tail",
          "[insertfx][modulation-insert][dsp][transition]")
{
    for (auto original : {Type::flanger, Type::phaser})
    {
        CAPTURE(fire::effects::name(original));
        InsertEffect effect;
        effect.prepare({48000.0, 127, 2});
        auto parameters = staticParameters(original);
        juce::AudioBuffer<float> audio(2, 4800);
        const auto fill = [&] {
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.125f, audio.getNumSamples());
        };
        fill();
        processChunks(effect, audio, parameters, 127);
        auto previous = audio.getSample(0, audio.getNumSamples() - 1);
        REQUIRE(previous == Catch::Approx(0.125f).margin(0.00001f));
        parameters = staticParameters(original == Type::flanger ? Type::phaser : Type::flanger);
        fill();
        processChunks(effect, audio, parameters, 127);
        float maximumJump = 0.0f;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto current = audio.getSample(0, i);
            REQUIRE(std::isfinite(current));
            maximumJump = std::max(maximumJump, std::abs(current - previous));
            previous = current;
        }
        CHECK(maximumJump < 0.01f);
        CHECK(previous == Catch::Approx(0.125f).margin(0.00001f));
        parameters.enabled = false;
        audio.clear();
        processChunks(effect, audio, parameters, 127);
        parameters.enabled = true;
        audio.clear();
        processChunks(effect, audio, parameters, 127);
        CHECK(audio.getMagnitude(0, audio.getNumSamples()) == 0.0f);
    }
}

TEST_CASE("Flanger cold history is filled under dry audio before its wet fade",
          "[insertfx][modulation-insert][dsp][startup]")
{
    constexpr int warmup = 960;
    for (const auto recipe : {std::array<float, 3> {{2.0f, 0.0f, 0.0f}},
                             std::array<float, 3> {{10.0f, 0.0f, 90.0f}},
                             std::array<float, 3> {{10.0f, 100.0f, -90.0f}}})
    {
        CAPTURE(recipe[0], recipe[1], recipe[2]);
        InsertEffect effect;
        effect.prepare({48000.0, 127, 2});
        auto parameters = staticParameters(Type::flanger);
        parameters.values[0].baseValue = 8.0f;
        parameters.values[1].baseValue = recipe[1];
        parameters.values[2].baseValue = recipe[0];
        parameters.values[3].baseValue = recipe[2];
        parameters.values[4].baseValue = 100.0f;
        juce::AudioBuffer<float> audio(2, 6000);
        const auto fill = [&] {
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.125f, audio.getNumSamples());
        };
        for (int lifecycle = 0; lifecycle < 3; ++lifecycle)
        {
            CAPTURE(lifecycle);
            if (lifecycle == 1) effect.reset();
            if (lifecycle == 2)
            {
                parameters.enabled = false;
                fill();
                processChunks(effect, audio, parameters, 127);
                parameters.enabled = true;
            }
            fill();
            processChunks(effect, audio, parameters, 127);
            for (int channel = 0; channel < 2; ++channel)
            {
                float largestJump = 0.0f;
                auto previous = 0.125f;
                for (int i = 0; i < audio.getNumSamples(); ++i)
                {
                    const auto sample = audio.getSample(channel, i);
                    if (i < warmup) REQUIRE(sample == 0.125f);
                    largestJump = std::max(largestJump, std::abs(sample - previous));
                    previous = sample;
                }
                CAPTURE(channel, largestJump);
                CHECK(largestJump < 0.001f);
            }
        }
    }
}

TEST_CASE("Flanger interrupted warmup and quick type reversals keep the dry attack continuous",
          "[insertfx][modulation-insert][dsp][startup][transition]")
{
    InsertEffect effect;
    effect.prepare({48000.0, 127, 2});
    auto parameters = staticParameters(Type::flanger);
    float previous = 0.125f, largestJump = 0.0f;
    const auto run = [&](int count) {
        juce::AudioBuffer<float> audio(2, count);
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.125f, count);
        processChunks(effect, audio, parameters, 127);
        for (int i = 0; i < count; ++i)
        {
            const auto sample = audio.getSample(0, i);
            REQUIRE(std::isfinite(sample));
            largestJump = std::max(largestJump, std::abs(sample - previous));
            previous = sample;
        }
    };
    run(37);
    parameters.enabled = false;
    run(1200); // Fully dormant before the first delayed wet sample.
    parameters.enabled = true;
    run(47);
    parameters = staticParameters(Type::phaser);
    run(31);
    parameters = staticParameters(Type::flanger);
    run(5000);
    CHECK(largestJump < 0.001f);
    CHECK(previous == Catch::Approx(0.125f).margin(0.00001f));
}
