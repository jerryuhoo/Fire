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

enum class StereoTarget
{
    width,
    pan,
    widthMix
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

juce::String targetParameter(StereoTarget target)
{
    switch (target)
    {
        case StereoTarget::width:    return firstBandParameter(WIDTH_ID);
        case StereoTarget::pan:      return firstBandParameter(PAN_ID);
        case StereoTarget::widthMix: return firstBandParameter(WIDTH_MIX_ID);
    }

    jassertfalse;
    return {};
}

const char* targetName(StereoTarget target)
{
    switch (target)
    {
        case StereoTarget::width:    return "Width";
        case StereoTarget::pan:      return "Pan";
        case StereoTarget::widthMix: return "Width Mix";
    }

    return "Unknown";
}

float targetBaseValue(StereoTarget target)
{
    return target == StereoTarget::pan ? 0.0f : 0.5f;
}

std::array<float, 2> targetEndpoints(StereoTarget target)
{
    return target == StereoTarget::pan
             ? std::array<float, 2> { -1.0f, 1.0f }
             : std::array<float, 2> { 0.0f, 1.0f };
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
                        StereoTarget target,
                        bool modulateTarget,
                        float targetValue,
                        bool useHq)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    setPlainParameter(processor, firstBandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(SHAPE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(WIDTH_BYPASS_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(OUTPUT_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(MIX_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(SHAPE_MIX_ID), 1.0f);

    // Give every target a deliberately distinct pair of stereo endpoints.
    setPlainParameter(processor,
                      firstBandParameter(WIDTH_ID),
                      target == StereoTarget::width ? targetValue : 0.91f);
    setPlainParameter(processor,
                      firstBandParameter(PAN_ID),
                      target == StereoTarget::pan ? targetValue : 0.58f);
    setPlainParameter(processor,
                      firstBandParameter(WIDTH_MIX_ID),
                      target == StereoTarget::widthMix ? targetValue : 1.0f);

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
    if (channel == 0)
    {
        return 0.29f * static_cast<float>(std::sin(
                   juce::MathConstants<double>::twoPi * 431.0 * time + 0.17))
             + 0.08f * static_cast<float>(std::cos(
                   juce::MathConstants<double>::twoPi * 1777.0 * time));
    }

    return 0.16f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 719.0 * time + 0.63))
         - 0.11f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 1231.0 * time + 0.31));
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool isFinite = true;
};

RenderResult render(StereoTarget target,
                    const std::vector<int>& hostBlockPattern,
                    bool modulateTarget,
                    float targetValue,
                    bool useHq)
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
                       targetValue,
                       useHq);

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

BandProcessingParameters makeDirectBandParameters(StereoTarget target,
                                                   bool useHq)
{
    BandProcessingParameters params;
    params.mode = 2; // tanh
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isWidthEnabled = true;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.shapeMixVal = 1.0f;

    params.width = target == StereoTarget::width ? 0.5f : 0.91f;
    params.widthValProvider.baseValue = params.width;
    params.widthValProvider.range = { 0.0f, 1.0f };
    params.pan = target == StereoTarget::pan ? 0.0f : 0.58f;
    params.panValProvider.baseValue = params.pan;
    params.panValProvider.range = { -1.0f, 1.0f };
    params.widthMixVal = target == StereoTarget::widthMix ? 0.5f : 1.0f;
    params.widthMixValProvider.baseValue = params.widthMixVal;
    params.widthMixValProvider.range = { 0.0f, 1.0f };

    switch (target)
    {
        case StereoTarget::width:
            params.widthValProvider.modulationDepth = 1.0f;
            params.widthLfoSourceIndex = 0;
            break;
        case StereoTarget::pan:
            params.panValProvider.modulationDepth = 1.0f;
            params.panLfoSourceIndex = 0;
            break;
        case StereoTarget::widthMix:
            params.widthMixValProvider.modulationDepth = 1.0f;
            params.widthMixLfoSourceIndex = 0;
            break;
    }

    return params;
}

float triangleLfoSample(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoCycleSamples)
                        / static_cast<float>(lfoCycleSamples);
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

RenderResult renderDirectBandProcessor(StereoTarget target,
                                       int preparedBlockSize,
                                       bool useHq)
{
    REQUIRE(preparedBlockSize > 0);

    BandProcessor band;
    band.prepare({ sampleRate,
                   static_cast<juce::uint32>(preparedBlockSize),
                   2 });
    const auto params = makeDirectBandParameters(target, useHq);

    RenderResult result;
    for (auto& output : result.output)
        output.reserve(comparisonSamples);

    const int totalSamples = warmupSamples + comparisonSamples;
    for (int streamPosition = 0;
         streamPosition < totalSamples;
         streamPosition += lfoCycleSamples)
    {
        const int samplesThisBlock = std::min(lfoCycleSamples,
                                              totalSamples - streamPosition);
        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisBlock);
        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            lfoOutputs.setSample(0, sample, triangleLfoSample(absoluteSample));
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel, absoluteSample));
        }

        band.process(buffer, params, lfoOutputs);

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

void checkBlockSizeIndependence(StereoTarget target, bool useHq)
{
    const auto fullCycleBlocks = render(target,
                                        { lfoCycleSamples },
                                        true,
                                        targetBaseValue(target),
                                        useHq);
    const auto irregularBlocks = render(target,
                                        { 17, 31, 43, 29, 53, 37 },
                                        true,
                                        targetBaseValue(target),
                                        useHq);
    const auto endpoints = targetEndpoints(target);
    const auto firstEndpoint = render(target,
                                      { lfoCycleSamples },
                                      false,
                                      endpoints[0],
                                      useHq);
    const auto secondEndpoint = render(target,
                                       { lfoCycleSamples },
                                       false,
                                       endpoints[1],
                                       useHq);

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
    CAPTURE(targetName(target), useHq, maximumError, endpointSeparation);

    REQUIRE(endpointSeparation > 0.15f);
    CHECK(maximumError < 2.0e-4f);
}

void checkInternalChunkOffset(StereoTarget target, bool useHq)
{
    const auto wholeHostBlock = renderDirectBandProcessor(target,
                                                          lfoCycleSamples,
                                                          useHq);
    const auto internallyChunked = renderDirectBandProcessor(target,
                                                             64,
                                                             useHq);

    REQUIRE(wholeHostBlock.output[0].size() == comparisonSamples);
    REQUIRE(internallyChunked.output[0].size() == comparisonSamples);
    CHECK(wholeHostBlock.isFinite);
    CHECK(internallyChunked.isFinite);

    const float maximumError = maximumDifference(wholeHostBlock,
                                                  internallyChunked);
    CAPTURE(targetName(target), useHq, maximumError);

    // Every callback contains one complete LFO cycle. The capacity-64 band
    // must bind each provider at the chunk's absolute LFO offset instead of
    // replaying the first 64 samples for every internal sub-block.
    CHECK(maximumError < 2.0e-4f);
}
} // namespace

TEST_CASE("Stereo LFO modulation is independent of host callback partitioning",
          "[processor][stereo][lfo][block-size]")
{
    SECTION("Width")
    {
        checkBlockSizeIndependence(StereoTarget::width, false);
        checkBlockSizeIndependence(StereoTarget::width, true);
    }

    SECTION("Pan")
    {
        checkBlockSizeIndependence(StereoTarget::pan, false);
        checkBlockSizeIndependence(StereoTarget::pan, true);
    }

    SECTION("Width Mix")
    {
        checkBlockSizeIndependence(StereoTarget::widthMix, false);
        checkBlockSizeIndependence(StereoTarget::widthMix, true);
    }
}

TEST_CASE("Stereo LFO providers preserve their offset across internal chunks",
          "[processor][stereo][lfo][block-size][internal-chunk]")
{
    SECTION("Width")
    {
        checkInternalChunkOffset(StereoTarget::width, false);
        checkInternalChunkOffset(StereoTarget::width, true);
    }

    SECTION("Pan")
    {
        checkInternalChunkOffset(StereoTarget::pan, false);
        checkInternalChunkOffset(StereoTarget::pan, true);
    }

    SECTION("Width Mix")
    {
        checkInternalChunkOffset(StereoTarget::widthMix, false);
        checkInternalChunkOffset(StereoTarget::widthMix, true);
    }
}
