#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 512;
constexpr int warmupSamples = 4096;
constexpr int renderedSamples = 8192;
constexpr int transitionWindowSamples = 512;
constexpr int finalWindowSamples = 2048;

struct CrossoverScenario
{
    int numBands;
    std::array<float, 3> initialFrequencies;
    std::array<float, 3> targetFrequencies;
};

constexpr std::array crossoverScenarios {
    CrossoverScenario { 2, { 200.0f, 1000.0f, 5000.0f },
                           { 6000.0f, 1000.0f, 5000.0f } },
    CrossoverScenario { 3, { 200.0f, 8000.0f, 9000.0f },
                           { 6000.0f, 10000.0f, 9000.0f } },
    CrossoverScenario { 4, { 200.0f, 2500.0f, 8000.0f },
                           { 1200.0f, 6000.0f, 10000.0f } }
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

float inputSample(int channel, int absoluteSample)
{
    const auto time = static_cast<double>(absoluteSample) / sampleRate;
    const auto channelPhase = static_cast<double>(channel) * 0.37;
    return 0.13f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 137.0 * time + channelPhase))
         + 0.11f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 1307.0 * time + 0.3 + channelPhase))
         + 0.07f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 5711.0 * time + 0.9 - channelPhase))
         + 0.04f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 11003.0 * time + 1.2 + channelPhase));
}

void configureNeutralProcessor(FireAudioProcessor& processor,
                               const CrossoverScenario& scenario)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, NUM_BANDS_ID, static_cast<float>(scenario.numBands));

    for (int crossover = 0; crossover < scenario.numBands - 1; ++crossover)
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, crossover),
                          scenario.initialFrequencies[static_cast<size_t>(crossover)]);

    for (int band = 0; band < scenario.numBands; ++band)
    {
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(BAND_ENABLE_ID, band),
                          0.0f);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(BAND_SOLO_ID, band),
                          0.0f);
    }

    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool isFinite = true;
};

RenderResult renderCrossoverAutomation(const CrossoverScenario& scenario,
                                       const std::vector<int>& hostBlockPattern)
{
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    FireAudioProcessor processor;
    configureNeutralProcessor(processor, scenario);
    juce::MidiBuffer midi;

    size_t blockIndex = 0;
    const auto processRange = [&](int firstSample,
                                  int numSamples,
                                  RenderResult* capture)
    {
        int processed = 0;
        while (processed < numSamples)
        {
            const int requested = hostBlockPattern[blockIndex % hostBlockPattern.size()];
            ++blockIndex;
            const int samplesThisBlock = std::min(requested, numSamples - processed);
            juce::AudioBuffer<float> block(2, samplesThisBlock);

            for (int channel = 0; channel < block.getNumChannels(); ++channel)
                for (int sample = 0; sample < samplesThisBlock; ++sample)
                    block.setSample(channel,
                                    sample,
                                    inputSample(channel,
                                                firstSample + processed + sample));

            processor.processBlock(block, midi);

            if (capture != nullptr)
            {
                for (int channel = 0; channel < block.getNumChannels(); ++channel)
                {
                    auto& destination = capture->output[static_cast<size_t>(channel)];
                    for (int sample = 0; sample < samplesThisBlock; ++sample)
                    {
                        const float value = block.getSample(channel, sample);
                        capture->isFinite = capture->isFinite && std::isfinite(value);
                        destination.push_back(value);
                    }
                }
            }

            processed += samplesThisBlock;
        }
    };

    // Both renders reach the automation point with the same absolute input
    // history and the same steady crossover coefficients.
    processRange(0, warmupSamples, nullptr);
    for (int crossover = 0; crossover < scenario.numBands - 1; ++crossover)
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, crossover),
                          scenario.targetFrequencies[static_cast<size_t>(crossover)]);

    RenderResult result;
    for (auto& channel : result.output)
        channel.reserve(renderedSamples);
    processRange(warmupSamples, renderedSamples, &result);
    return result;
}

float maximumDifference(const RenderResult& first,
                        const RenderResult& second,
                        int startSample,
                        int numSamples)
{
    float maximumError = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (int sample = startSample; sample < startSample + numSamples; ++sample)
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][static_cast<size_t>(sample)]
                         - second.output[channel][static_cast<size_t>(sample)]));
    }
    return maximumError;
}

double finalMagnitudeRatio(const RenderResult& render, int channel)
{
    const int startSample = renderedSamples - finalWindowSamples;
    double inputEnergy = 0.0;
    double outputEnergy = 0.0;

    for (int sample = startSample; sample < renderedSamples; ++sample)
    {
        const auto input = static_cast<double>(
            inputSample(channel, warmupSamples + sample));
        const auto output = static_cast<double>(
            render.output[static_cast<size_t>(channel)][static_cast<size_t>(sample)]);
        inputEnergy += input * input;
        outputEnergy += output * output;
    }

    REQUIRE(inputEnergy > 0.0);
    return std::sqrt(outputEnergy / inputEnergy);
}
} // namespace

TEST_CASE("Crossover automation is independent of host callback partitioning",
          "[processor][crossover][smoothing][block-size]")
{
    for (const auto& scenario : crossoverScenarios)
    {
        CAPTURE(scenario.numBands,
                scenario.initialFrequencies,
                scenario.targetFrequencies);

        const auto largeBlocks = renderCrossoverAutomation(
            scenario, { preparedBlockSize });
        const auto irregularSmallBlocks = renderCrossoverAutomation(
            scenario, { 7, 31, 11, 43, 17, 29 });

        REQUIRE(largeBlocks.output[0].size() == renderedSamples);
        REQUIRE(irregularSmallBlocks.output[0].size() == renderedSamples);
        CHECK(largeBlocks.isFinite);
        CHECK(irregularSmallBlocks.isFinite);

        const float transitionError = maximumDifference(largeBlocks,
                                                         irregularSmallBlocks,
                                                         0,
                                                         transitionWindowSamples);
        const float finalError = maximumDifference(
            largeBlocks,
            irregularSmallBlocks,
            renderedSamples - finalWindowSamples,
            finalWindowSamples);
        CAPTURE(transitionError, finalError);

        // The 1 ms ramps are properties of the absolute sample stream. Host
        // callback boundaries must not turn them into one held cutoff value
        // per callback, including when several dividers move together.
        CHECK(transitionError < 2.0e-4f);

        // Both paths must settle on the same target crossovers. Every neutral
        // Linkwitz-Riley tree must retain a flat summed magnitude once settled.
        CHECK(finalError < 2.0e-4f);
        for (int channel = 0; channel < 2; ++channel)
        {
            CHECK(finalMagnitudeRatio(largeBlocks, channel)
                  == Catch::Approx(1.0).margin(0.02));
            CHECK(finalMagnitudeRatio(irregularSmallBlocks, channel)
                  == Catch::Approx(1.0).margin(0.02));
        }
    }
}
