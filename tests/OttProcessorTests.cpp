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
