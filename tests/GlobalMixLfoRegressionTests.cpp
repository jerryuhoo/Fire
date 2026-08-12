#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr float lfoRateHz = 100.0f;
constexpr int lfoCycleSamples = static_cast<int>(sampleRate / lfoRateHz);
constexpr int warmupSamples = lfoCycleSamples * 12;
constexpr int comparisonSamples = lfoCycleSamples * 12;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

LfoData makeTriangleLfo()
{
    LfoData shape;
    shape.points = { { 0.0f, 0.0f },
                     { 0.5f, 1.0f },
                     { 1.0f, 0.0f } };
    shape.curvatures = { 0.0f, 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

void configureProcessor(FireAudioProcessor& processor,
                        int preparedBlockSize,
                        bool modulateGlobalMix,
                        float globalMix)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, globalMix);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    // Make the single wet band deliberately different from the global dry
    // path, while avoiding distortion, compression and HQ latency.
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      1.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(COMP_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LINKED_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(OUTPUT_ID, 0),
                      -18.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(MIX_ID, 0),
                      1.0f);

    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0),
                      lfoRateHz);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_PHASE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0),
                      0.0f);
    processor.getLfoManager().setLfoData(0, makeTriangleLfo());
    if (modulateGlobalMix)
    {
        processor.assignLfoToTarget(0, MIX_ID);
        processor.setModulationDepth(MIX_ID, 1.0f);
    }

    // assignLfoToTarget defaults to bipolar. At a 0.5 base value and full
    // depth, the triangle sweeps the complete [0, 1] global Mix range.
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float inputSample(int channel, int absoluteSample)
{
    const auto time = static_cast<double>(absoluteSample) / sampleRate;
    const auto channelPhase = static_cast<double>(channel) * 0.31;
    return 0.47f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 731.0 * time + channelPhase))
         + 0.19f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 1817.0 * time + 0.2 - channelPhase));
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool isFinite = true;
};

RenderResult render(const std::vector<int>& hostBlockPattern,
                    bool modulateGlobalMix = true,
                    float globalMix = 0.5f)
{
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    FireAudioProcessor processor;
    configureProcessor(processor,
                       *std::max_element(hostBlockPattern.begin(), hostBlockPattern.end()),
                       modulateGlobalMix,
                       globalMix);
    RenderResult result;
    for (auto& channel : result.output)
        channel.reserve(comparisonSamples);

    const int totalSamples = warmupSamples + comparisonSamples;
    int streamPosition = 0;
    size_t blockIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        const int requested = hostBlockPattern[blockIndex % hostBlockPattern.size()];
        ++blockIndex;
        const int samplesThisBlock = std::min(requested,
                                              totalSamples - streamPosition);
        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel, streamPosition + sample));

        processor.processBlock(buffer, midi);

        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            if (absoluteSample < warmupSamples)
                continue;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const float value = buffer.getSample(channel, sample);
                result.isFinite = result.isFinite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        }

        streamPosition += samplesThisBlock;
    }

    return result;
}

float maximumDifference(const RenderResult& first,
                        const RenderResult& second)
{
    float maximumError = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (size_t sample = 0; sample < first.output[channel].size(); ++sample)
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][sample]
                         - second.output[channel][sample]));
    }
    return maximumError;
}

float outputRange(const RenderResult& render, int channel)
{
    const auto& output = render.output[static_cast<size_t>(channel)];
    REQUIRE_FALSE(output.empty());
    const auto [minimum, maximum] = std::minmax_element(output.begin(), output.end());
    return *maximum - *minimum;
}
} // namespace

TEST_CASE("LFO-modulated Global Mix is independent of host callback partitioning",
          "[processor][global-mix][lfo][block-size]")
{
    // At 100 Hz a 480-sample callback is exactly one LFO cycle. Sampling only
    // index zero would therefore freeze every large callback at the same Mix.
    const auto fullCycleBlocks = render({ lfoCycleSamples });
    const auto irregularSmallBlocks = render({ 17, 31, 43, 29, 53, 37 });
    const auto alwaysDry = render({ lfoCycleSamples }, false, 0.0f);
    const auto alwaysWet = render({ lfoCycleSamples }, false, 1.0f);

    REQUIRE(fullCycleBlocks.output[0].size() == comparisonSamples);
    REQUIRE(irregularSmallBlocks.output[0].size() == comparisonSamples);
    CHECK(fullCycleBlocks.isFinite);
    CHECK(irregularSmallBlocks.isFinite);
    CHECK(alwaysDry.isFinite);
    CHECK(alwaysWet.isFinite);

    const float maximumError = maximumDifference(fullCycleBlocks,
                                                  irregularSmallBlocks);
    const float largeBlockRange = outputRange(fullCycleBlocks, 0);
    const float smallBlockRange = outputRange(irregularSmallBlocks, 0);
    const float dryWetSeparation = maximumDifference(alwaysDry, alwaysWet);
    CAPTURE(maximumError,
            largeBlockRange,
            smallBlockRange,
            dryWetSeparation);

    // A sample-accurate target produces identical audio regardless of where a
    // host divides callbacks. Allow a small floating-point/mixer margin.
    CHECK(maximumError < 2.0e-4f);

    // Guard the fixture directly: after startup is skipped, global Mix=0 and
    // Mix=1 references must remain audibly distinct. Output ranges additionally
    // reject a silent or constant test signal.
    REQUIRE(dryWetSeparation > 0.2f);
    CHECK(largeBlockRange > 0.2f);
    CHECK(smallBlockRange > 0.2f);
}
