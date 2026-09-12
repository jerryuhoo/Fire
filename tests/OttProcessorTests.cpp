#include <DSP/OttProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>
#include <vector>

namespace {
constexpr double sampleRate = 48000.0;
std::vector<float> render(const std::vector<float>& input, int blockSize,
                          OttProcessor::Parameters parameters,
                          const std::vector<float>* lfo = nullptr)
{
    OttProcessor processor;
    processor.prepare({sampleRate, static_cast<juce::uint32>(blockSize), 2});
    auto result = input;
    std::vector<float> right(input.size());
    for (size_t i = 0; i < input.size(); ++i) right[i] = input[i] * -0.5f;
    for (size_t offset = 0; offset < input.size(); offset += static_cast<size_t>(blockSize))
    {
        const auto count = juce::jmin(static_cast<size_t>(blockSize), input.size() - offset);
        float* channels[] {result.data() + offset, right.data() + offset};
        if (lfo) parameters.controls[OttProcessor::depth].lfoSignal = lfo->data() + offset;
        processor.process(juce::dsp::AudioBlock<float>(channels, 2, count), parameters);
    }
    for (size_t i = 0; i < result.size(); ++i)
        CHECK(right[i] == Catch::Approx(result[i] * -0.5f).margin(1.0e-7f));
    return result;
}
}

TEST_CASE("OTT raises quiet signals and compresses loud signals with a neutral middle region", "[ott][dsp]")
{
    CHECK(OttProcessor::gainForLevel(-60, -48, -18, 1) == Catch::Approx(9.0f));
    CHECK(OttProcessor::gainForLevel(-30, -48, -18, 1) == 0.0f);
    CHECK(OttProcessor::gainForLevel(-6, -48, -18, 1) == Catch::Approx(-10.5f));
    CHECK(OttProcessor::gainForLevel(-60, -48, -18, 0.5f) == Catch::Approx(4.5f));
    CHECK(OttProcessor::gainForLevel(-120, -48, -18, 1) == 0.0f);
    CHECK(OttProcessor::gainForLevel(-80, -6, 0, 1) <= 24.0f);

    OttProcessor::Parameters parameters;
    parameters.enabled = true;
    parameters.controls[OttProcessor::depth].baseValue = 1.0f;
    for (float inputDb : {-60.0f, -30.0f, -6.0f})
    {
        const auto input = juce::Decibels::decibelsToGain(inputDb);
        const auto samples = render(std::vector<float>(24000, input), 127, parameters);
        const auto expectedGain = OttProcessor::gainForLevel(inputDb, -48, -18, 1);
        CHECK(samples.back() == Catch::Approx(input * juce::Decibels::decibelsToGain(expectedGain)).epsilon(0.003));
    }
}

TEST_CASE("OTT bypass zero mix and zero depth preserve samples exactly", "[ott][dsp][bypass]")
{
    std::vector<float> input(2048);
    for (size_t i = 0; i < input.size(); ++i) input[i] = 0.1f * std::sin(static_cast<float>(i) * 0.1f);
    OttProcessor::Parameters parameters;
    CHECK(render(input, 64, parameters) == input);
    parameters.enabled = true;
    parameters.controls[OttProcessor::mix].baseValue = 0.0f;
    CHECK(render(input, 64, parameters) == input);
    parameters.controls[OttProcessor::mix].baseValue = 1.0f;
    parameters.controls[OttProcessor::depth].baseValue = 0.0f;
    CHECK(render(input, 64, parameters) == input);
}

TEST_CASE("OTT remains block-size independent with stereo-linked sample-accurate modulation", "[ott][dsp][lfo][block-size]")
{
    std::vector<float> input(4096), lfo(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.01f * std::sin(static_cast<float>(i) * 0.08f);
        lfo[i] = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.013f);
    }
    OttProcessor::Parameters parameters;
    parameters.enabled = true;
    parameters.sources[OttProcessor::depth] = 0;
    parameters.controls[OttProcessor::depth].modulationDepth = 0.8f;
    const auto reference = render(input, 4096, parameters, &lfo);
    for (int blockSize : {1, 37, 256, 1024})
    {
        const auto result = render(input, blockSize, parameters, &lfo);
        CHECK(result == reference);
    }
}

TEST_CASE("OTT contains non-finite input and recovers without adding signal to silence", "[ott][dsp][robustness]")
{
    OttProcessor processor;
    processor.prepare({sampleRate, 64, 1});
    OttProcessor::Parameters parameters;
    parameters.enabled = true;
    parameters.controls[OttProcessor::upward].baseValue = std::numeric_limits<float>::quiet_NaN();
    parameters.controls[OttProcessor::downward].baseValue = -1000.0f;
    juce::AudioBuffer<float> buffer(1, 64);
    buffer.clear();
    buffer.setSample(0, 2, std::numeric_limits<float>::infinity());
    buffer.setSample(0, 3, std::numeric_limits<float>::quiet_NaN());
    processor.process(juce::dsp::AudioBlock<float>(buffer), parameters);
    for (int i = 0; i < 64; ++i) CHECK(buffer.getSample(0, i) == 0.0f);
    CHECK(std::isfinite(processor.getInputLevelDb()));
    CHECK(std::isfinite(processor.getGainChangeDb()));
    buffer.clear();
    buffer.setSample(0, 10, 0.1f);
    processor.process(juce::dsp::AudioBlock<float>(buffer), parameters);
    CHECK(buffer.getSample(0, 10) > 0.0f);
    CHECK(std::isfinite(buffer.getSample(0, 10)));
}

TEST_CASE("OTT bypass and parameter automation use continuous gain transitions", "[ott][dsp][automation]")
{
    OttProcessor processor;
    processor.prepare({sampleRate, 64, 1});
    OttProcessor::Parameters parameters;
    juce::AudioBuffer<float> buffer(1, 64);
    auto process = [&] {
        juce::FloatVectorOperations::fill(buffer.getWritePointer(0), 0.001f, 64);
        processor.process(juce::dsp::AudioBlock<float>(buffer), parameters);
    };
    for (int block = 0; block < 400; ++block) process();
    parameters.enabled = true;
    process();
    CHECK(std::abs(buffer.getSample(0, 0) - 0.001f) < 0.00001f);
    for (int block = 0; block < 100; ++block) process();
    const auto before = buffer.getSample(0, 63);
    parameters.controls[OttProcessor::output].baseValue = 24.0f;
    process();
    CHECK(std::abs(buffer.getSample(0, 0) - before) < 0.0001f);
    for (int block = 0; block < 100; ++block) process();
    const auto beforeBypass = buffer.getSample(0, 63);
    parameters.enabled = false;
    process();
    CHECK(std::abs(buffer.getSample(0, 0) - beforeBypass) < 0.0001f);
    for (int block = 0; block < 100; ++block) process();
    CHECK(buffer.getSample(0, 63) == 0.001f);
}

TEST_CASE("OTT routing changes bridge from the last applied control value", "[ott][dsp][lfo][routing]")
{
    OttProcessor processor;
    processor.prepare({sampleRate, 64, 1});
    OttProcessor::Parameters parameters;
    parameters.enabled = true;
    juce::AudioBuffer<float> buffer(1, 64);
    std::array<float, 64> lfo;
    lfo.fill(1.0f);
    const auto process = [&] {
        juce::FloatVectorOperations::fill(buffer.getWritePointer(0), 0.001f, 64);
        processor.process(juce::dsp::AudioBlock<float>(buffer), parameters);
    };
    for (int block = 0; block < 400; ++block) process();
    const auto before = buffer.getSample(0, 63);
    parameters.controls[OttProcessor::depth].lfoSignal = lfo.data();
    parameters.controls[OttProcessor::depth].modulationDepth = 0.8f;
    parameters.sources[OttProcessor::depth] = 0;
    process();
    CHECK(std::abs(buffer.getSample(0, 0) - before) < 0.00001f);
    for (int block = 0; block < 20; ++block) process();
    const auto routed = buffer.getSample(0, 63);
    CHECK(routed > before * 1.4f);
    lfo.fill(0.0f);
    parameters.sources[OttProcessor::depth] = 1;
    process();
    CHECK(std::abs(buffer.getSample(0, 0) - routed) < 0.00001f);
}

TEST_CASE("OTT activity distinguishes dynamics from output trim and wet balance", "[ott][dsp][telemetry]")
{
    OttProcessor processor;
    processor.prepare({sampleRate, 64, 2});
    OttProcessor::Parameters parameters;
    parameters.enabled = true;
    parameters.controls[OttProcessor::depth].baseValue = 0.0f;
    parameters.controls[OttProcessor::output].baseValue = 24.0f;
    juce::AudioBuffer<float> audio(2, 64);
    const auto settle = [&] {
        for (int block = 0; block < 800; ++block)
        {
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.5f, 64);
            processor.process(juce::dsp::AudioBlock<float>(audio), parameters);
        }
    };
    settle();
    CHECK(processor.getGainChangeDb() > 23.9f);
    CHECK(processor.getDynamicsActivityDb() == 0.0f);
    parameters.controls[OttProcessor::depth].baseValue = 1.0f;
    settle();
    CHECK(processor.getGainChangeDb() > 0.0f);
    CHECK(processor.getDynamicsActivityDb() < -10.0f);
    parameters.controls[OttProcessor::mix].baseValue = 0.0f;
    settle();
    CHECK(processor.getDynamicsActivityDb() == 0.0f);
    processor.reset();
    CHECK(processor.getDynamicsActivityDb() == 0.0f);
}

TEST_CASE("OTT gain telemetry reports the final applied sample including zero wet", "[ott][dsp][telemetry]")
{
    OttProcessor processor;
    processor.prepare({sampleRate, 127, 2});
    OttProcessor::Parameters parameters;
    parameters.enabled = true;
    parameters.controls[OttProcessor::depth].baseValue = 0.0f;
    parameters.controls[OttProcessor::output].baseValue = 0.0f;
    parameters.controls[OttProcessor::output].modulationDepth = 1.0f;
    parameters.sources[OttProcessor::output] = 0;
    std::array<float, 127> lfo;
    parameters.controls[OttProcessor::output].lfoSignal = lfo.data();
    juce::AudioBuffer<float> audio(2, static_cast<int>(lfo.size()));
    for (int block = 0; block < 32; ++block)
    {
        for (size_t i = 0; i < lfo.size(); ++i)
            lfo[i] = static_cast<float>((block * 7 + static_cast<int>(i)) % 127) / 126.0f;
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.2f, audio.getNumSamples());
        if (block == 16) parameters.controls[OttProcessor::mix].baseValue = 0.0f;
        processor.process(juce::dsp::AudioBlock<float>(audio), parameters);
        const auto appliedGain = audio.getSample(0, audio.getNumSamples() - 1) / 0.2f;
        CHECK(processor.getGainChangeDb() == Catch::Approx(juce::Decibels::gainToDecibels(appliedGain, -120.0f)).margin(2.0e-5f));
    }
    CHECK(processor.getGainChangeDb() == 0.0f);
}

TEST_CASE("OTT settled controls retain detector history through bypass and automation", "[ott][dsp][automation][cache]")
{
    OttProcessor processor, fallbackProcessor;
    processor.prepare({sampleRate, 127, 2});
    fallbackProcessor.prepare({sampleRate, 127, 2});
    OttProcessor::Parameters parameters, fallbackParameters;
    std::array<float, 127> invalidLfo;
    invalidLfo.fill(std::numeric_limits<float>::quiet_NaN());
    juce::AudioBuffer<float> audio(2, static_cast<int>(invalidLfo.size())), reference;
    for (int block = 0; block < 96; ++block)
    {
        parameters.enabled = block >= 12 && (block < 48 || block >= 64);
        parameters.controls[OttProcessor::depth].baseValue = block < 28 ? 0.75f : 0.25f;
        parameters.controls[OttProcessor::timeScale].baseValue = block < 36 ? 50.0f : 200.0f;
        parameters.controls[OttProcessor::output].baseValue = block < 72 ? 3.0f : -6.0f;
        fallbackParameters = parameters;
        // Invalid LFO samples fall back to the same base value. Keep this
        // route present from the first block so there is no recipe transition.
        fallbackParameters.controls[OttProcessor::depth].lfoSignal = invalidLfo.data();
        fallbackParameters.sources[OttProcessor::depth] = 0;
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto input = 0.2f * std::sin(static_cast<float>(block * audio.getNumSamples() + sample) * 0.07f);
            audio.setSample(0, sample, input);
            audio.setSample(1, sample, input * -0.5f);
        }
        reference.makeCopyOf(audio);
        processor.process(juce::dsp::AudioBlock<float>(audio), parameters);
        fallbackProcessor.process(juce::dsp::AudioBlock<float>(reference), fallbackParameters);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                REQUIRE(audio.getSample(channel, sample) == reference.getSample(channel, sample));
        REQUIRE(processor.getInputLevelDb() == fallbackProcessor.getInputLevelDb());
        REQUIRE(processor.getGainChangeDb() == fallbackProcessor.getGainChangeDb());
        REQUIRE(processor.getDynamicsActivityDb() == fallbackProcessor.getDynamicsActivityDb());
    }
}
