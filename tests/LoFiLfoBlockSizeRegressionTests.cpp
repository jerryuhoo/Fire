#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr float lfoRateHz = 100.0f;
constexpr int lfoCycleSamples = static_cast<int>(sampleRate / lfoRateHz);
constexpr int warmupSamples = lfoCycleSamples * 12;
constexpr int comparisonSamples = lfoCycleSamples * 12;

enum class LoFiTarget
{
    rate,
    bitDepth,
    mix,
    jitter
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String firstBandParameter(const juce::String& baseID)
{
    return ParameterIDAndName::getIDString(baseID, 0);
}

juce::String targetParameter(LoFiTarget target)
{
    switch (target)
    {
        case LoFiTarget::rate:     return DOWNSAMPLE_ID;
        case LoFiTarget::bitDepth: return BIT_DEPTH_ID;
        case LoFiTarget::mix:      return DOWNSAMPLE_MIX_ID;
        case LoFiTarget::jitter:   return JITTER_ID;
    }

    jassertfalse;
    return {};
}

const char* targetName(LoFiTarget target)
{
    switch (target)
    {
        case LoFiTarget::rate:     return "Rate";
        case LoFiTarget::bitDepth: return "Bit Depth";
        case LoFiTarget::mix:      return "Mix";
        case LoFiTarget::jitter:   return "Jitter";
    }

    return "Unknown";
}

float targetBaseValue(LoFiTarget target)
{
    switch (target)
    {
        case LoFiTarget::rate:     return 32.5f;
        case LoFiTarget::bitDepth: return 18.0f;
        case LoFiTarget::mix:      return 0.5f;
        case LoFiTarget::jitter:   return 0.5f;
    }

    return 0.0f;
}

std::array<float, 2> targetEndpoints(LoFiTarget target)
{
    switch (target)
    {
        case LoFiTarget::rate:     return { 1.0f, 64.0f };
        case LoFiTarget::bitDepth: return { 4.0f, 32.0f };
        case LoFiTarget::mix:
        case LoFiTarget::jitter:   return { 0.0f, 1.0f };
    }

    return { 0.0f, 0.0f };
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
                        LoFiTarget target,
                        bool modulateTarget,
                        float targetValue)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    // A bypassed single band keeps the fixture focused on the global LoFi
    // stage without adding distortion, compression, width, or HQ latency.
    setPlainParameter(processor, firstBandParameter(BAND_ENABLE_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 1.0f);

    setPlainParameter(processor,
                      DOWNSAMPLE_ID,
                      target == LoFiTarget::rate ? targetValue
                                                 : target == LoFiTarget::mix ? 23.0f
                                                                            : target == LoFiTarget::jitter ? 24.0f
                                                                                                           : 1.0f);
    setPlainParameter(processor,
                      BIT_DEPTH_ID,
                      target == LoFiTarget::bitDepth ? targetValue
                                                     : target == LoFiTarget::mix ? 5.0f
                                                                                : 32.0f);
    setPlainParameter(processor,
                      JITTER_ID,
                      target == LoFiTarget::jitter ? targetValue : 0.0f);
    setPlainParameter(processor,
                      DOWNSAMPLE_MIX_ID,
                      target == LoFiTarget::mix ? targetValue : 1.0f);

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
    if (modulateTarget)
    {
        processor.assignLfoToTarget(0, targetParameter(target));
        processor.setModulationDepth(targetParameter(target), 1.0f);
    }

    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = channel == 0 ? 0.19 : 0.73;
    return 0.37f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 7139.0 * time + channelPhase))
         + 0.14f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 11983.0 * time
               + 0.31 - channelPhase));
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool isFinite = true;
};

RenderResult render(LoFiTarget target,
                    const std::vector<int>& hostBlockPattern,
                    bool modulateTarget,
                    float targetValue)
{
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    FireAudioProcessor processor;
    configureProcessor(processor,
                       *std::max_element(hostBlockPattern.begin(),
                                         hostBlockPattern.end()),
                       target,
                       modulateTarget,
                       targetValue);

    RenderResult result;
    for (auto& output : result.output)
        output.reserve(comparisonSamples);

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
        {
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][sample]
                         - second.output[channel][sample]));
        }
    }

    return maximumError;
}

void checkBlockSizeIndependence(LoFiTarget target)
{
    const auto fullCycleBlocks = render(target,
                                        { lfoCycleSamples },
                                        true,
                                        targetBaseValue(target));
    const auto irregularBlocks = render(target,
                                        { 17, 31, 43, 29, 53, 37 },
                                        true,
                                        targetBaseValue(target));
    const auto endpoints = targetEndpoints(target);
    const auto firstEndpoint = render(target,
                                      { lfoCycleSamples },
                                      false,
                                      endpoints[0]);
    const auto secondEndpoint = render(target,
                                       { lfoCycleSamples },
                                       false,
                                       endpoints[1]);

    REQUIRE(fullCycleBlocks.output[0].size() == comparisonSamples);
    REQUIRE(irregularBlocks.output[0].size() == comparisonSamples);
    CHECK(fullCycleBlocks.isFinite);
    CHECK(irregularBlocks.isFinite);
    CHECK(firstEndpoint.isFinite);
    CHECK(secondEndpoint.isFinite);

    const float maximumError = maximumDifference(fullCycleBlocks,
                                                  irregularBlocks);
    const float endpointSeparation = maximumDifference(firstEndpoint,
                                                       secondEndpoint);
    CAPTURE(targetName(target), maximumError, endpointSeparation);

    const float minimumEndpointSeparation = target == LoFiTarget::bitDepth
                                              ? 0.04f
                                              : 0.1f;
    REQUIRE(endpointSeparation > minimumEndpointSeparation);
    CHECK(maximumError < 2.0e-4f);
}

std::set<int> interiorHeldRunLengths(const std::vector<float>& output)
{
    std::vector<int> runs;
    int runLength = 1;
    for (size_t sample = 1; sample < output.size(); ++sample)
    {
        if (std::abs(output[sample] - output[sample - 1]) <= 1.0e-7f)
        {
            ++runLength;
        }
        else
        {
            runs.push_back(runLength);
            runLength = 1;
        }
    }
    runs.push_back(runLength);

    std::set<int> uniqueInteriorRuns;
    if (runs.size() > 2)
        uniqueInteriorRuns.insert(runs.begin() + 1, runs.end() - 1);
    return uniqueInteriorRuns;
}
} // namespace

TEST_CASE("LoFi LFO modulation is independent of host callback partitioning",
          "[processor][lofi][lfo][block-size]")
{
    SECTION("Rate")
    {
        checkBlockSizeIndependence(LoFiTarget::rate);
    }

    SECTION("Bit Depth")
    {
        checkBlockSizeIndependence(LoFiTarget::bitDepth);
    }

    SECTION("Mix")
    {
        checkBlockSizeIndependence(LoFiTarget::mix);
    }
}

TEST_CASE("Jitter LFO varies hold duration within a full-cycle callback",
          "[processor][lofi][lfo][jitter][sample-accurate]")
{
    // Separate processors cannot be compared sample-for-sample because Jitter
    // intentionally owns a private random stream. A single modulated render is
    // sufficient here: at Rate=24, block-sample-zero modulation produces only
    // 24-sample holds, whereas sample-accurate Jitter produces several lengths.
    const auto modulated = render(LoFiTarget::jitter,
                                  { lfoCycleSamples },
                                  true,
                                  targetBaseValue(LoFiTarget::jitter));
    REQUIRE(modulated.output[0].size() == comparisonSamples);
    REQUIRE(modulated.isFinite);

    const auto uniqueRunLengths = interiorHeldRunLengths(modulated.output[0]);
    CAPTURE(uniqueRunLengths);
    CHECK(uniqueRunLengths.size() >= 3);
}
