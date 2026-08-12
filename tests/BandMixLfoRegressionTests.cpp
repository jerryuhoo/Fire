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

enum class MixTarget
{
    band,
    shape
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

juce::String targetParameter(MixTarget target)
{
    return firstBandParameter(target == MixTarget::band ? MIX_ID : SHAPE_MIX_ID);
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
                        MixTarget target,
                        bool modulateTarget,
                        float targetMix,
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
    setPlainParameter(processor, firstBandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(EXTREME_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(SAFE_ID), 0.0f);

    if (target == MixTarget::band)
    {
        // Band Mix=0 is the unprocessed band, while Mix=1 includes this
        // deliberately quiet -18 dB wet path.
        setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 0.0f);
        setPlainParameter(processor, firstBandParameter(SHAPE_BYPASS_ID), 0.0f);
        setPlainParameter(processor, firstBandParameter(OUTPUT_ID), -18.0f);
        setPlainParameter(processor, firstBandParameter(MIX_ID), targetMix);
        setPlainParameter(processor, firstBandParameter(SHAPE_MIX_ID), 1.0f);
    }
    else
    {
        // Shape Mix=0 is the signal before the distortion stage. At Mix=1,
        // high Drive plus tanh makes a strongly separated nonlinear reference.
        setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 1.0f);
        setPlainParameter(processor, firstBandParameter(SHAPE_BYPASS_ID), 1.0f);
        setPlainParameter(processor, firstBandParameter(MODE_ID), 2.0f); // tanh
        setPlainParameter(processor, firstBandParameter(DRIVE_ID), 82.0f);
        setPlainParameter(processor, firstBandParameter(BIAS_ID), 0.17f);
        setPlainParameter(processor, firstBandParameter(REC_ID), 0.28f);
        setPlainParameter(processor, firstBandParameter(OUTPUT_ID), 0.0f);
        setPlainParameter(processor, firstBandParameter(MIX_ID), 1.0f);
        setPlainParameter(processor, firstBandParameter(SHAPE_MIX_ID), targetMix);
    }

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

    // The default bipolar routing combines base 0.5 and depth 1.0 into a
    // complete 0..1 sweep of the selected mix parameter.
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float inputSample(int channel, int absoluteSample)
{
    const auto time = static_cast<double>(absoluteSample) / sampleRate;
    const auto channelPhase = static_cast<double>(channel) * 0.27;
    return 0.31f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 617.0 * time + channelPhase))
         + 0.12f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 1901.0 * time + 0.4 - channelPhase));
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool isFinite = true;
};

RenderResult render(MixTarget target,
                    const std::vector<int>& hostBlockPattern,
                    bool modulateTarget = true,
                    float targetMix = 0.5f,
                    bool useHq = false)
{
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    FireAudioProcessor processor;
    configureProcessor(processor,
                       *std::max_element(hostBlockPattern.begin(), hostBlockPattern.end()),
                       target,
                       modulateTarget,
                       targetMix,
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

BandProcessingParameters makeDirectBandParameters(MixTarget target,
                                                   bool useHq)
{
    BandProcessingParameters params;
    params.mode = 2; // tanh
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = target == MixTarget::band ? 0.5f : 1.0f;
    params.shapeMixVal = target == MixTarget::shape ? 0.5f : 1.0f;

    if (target == MixTarget::band)
    {
        params.outputVal.baseValue = -18.0f;
        params.mixValProvider.baseValue = 0.5f;
        params.mixValProvider.modulationDepth = 1.0f;
        params.mixValProvider.range = { 0.0f, 1.0f };
        params.mixLfoSourceIndex = 0;
    }
    else
    {
        params.isDriveEnabled = true;
        params.isShapeEnabled = true;
        params.driveVal.baseValue = 82.0f;
        params.driveVal.range = { 0.0f, 100.0f };
        params.biasVal.baseValue = 0.17f;
        params.biasVal.range = { -1.0f, 1.0f };
        params.recVal.baseValue = 0.28f;
        params.recVal.range = { 0.0f, 1.0f };
        params.shapeMixValProvider.baseValue = 0.5f;
        params.shapeMixValProvider.modulationDepth = 1.0f;
        params.shapeMixValProvider.range = { 0.0f, 1.0f };
        params.shapeMixLfoSourceIndex = 0;
    }

    return params;
}

float triangleLfoSample(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoCycleSamples)
                        / static_cast<float>(lfoCycleSamples);
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

RenderResult renderDirectBandProcessor(MixTarget target,
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
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][sample]
                         - second.output[channel][sample]));
    }
    return maximumError;
}

void checkHostBlockIndependence(MixTarget target, bool useHq)
{
    const auto fullCycleBlocks = render(target,
                                        { lfoCycleSamples },
                                        true,
                                        0.5f,
                                        useHq);
    const auto irregularSmallBlocks = render(target,
                                             { 17, 31, 43, 29, 53, 37 },
                                             true,
                                             0.5f,
                                             useHq);
    const auto alwaysDry = render(target,
                                  { lfoCycleSamples },
                                  false,
                                  0.0f,
                                  useHq);
    const auto alwaysWet = render(target,
                                  { lfoCycleSamples },
                                  false,
                                  1.0f,
                                  useHq);

    REQUIRE(fullCycleBlocks.output[0].size() == comparisonSamples);
    REQUIRE(irregularSmallBlocks.output[0].size() == comparisonSamples);
    CHECK(fullCycleBlocks.isFinite);
    CHECK(irregularSmallBlocks.isFinite);
    CHECK(alwaysDry.isFinite);
    CHECK(alwaysWet.isFinite);

    const float maximumError = maximumDifference(fullCycleBlocks,
                                                  irregularSmallBlocks);
    const float dryWetSeparation = maximumDifference(alwaysDry, alwaysWet);
    CAPTURE(target == MixTarget::band ? "Band Mix" : "Shape Mix",
            useHq,
            maximumError,
            dryWetSeparation);

    // The two endpoint references prove this routing controls audibly distinct
    // paths after the startup ramps have settled.
    REQUIRE(dryWetSeparation > 0.15f);

    // A 480-sample host callback is exactly one LFO period. Sampling only the
    // first LFO value would freeze it; sample-accurate processing is invariant.
    CHECK(maximumError < 2.0e-4f);
}

void checkInternalChunkOffset(MixTarget target, bool useHq)
{
    // FireAudioProcessor reserves a minimum safety capacity, so exercise its
    // public BandProcessor directly to make this genuinely oversized. Both
    // renders receive identical 480-sample callbacks; only the prepared
    // maximum changes, forcing the second band to use 64-sample chunks.
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
    CAPTURE(target == MixTarget::band ? "Band Mix" : "Shape Mix",
            useHq,
            maximumError);

    // Internal chunk boundaries must advance the LFO sample offset instead of
    // restarting modulation from sample zero for every sub-block.
    CHECK(maximumError < 2.0e-4f);
}
} // namespace

TEST_CASE("Per-band mix LFOs are independent of host callback partitioning",
          "[processor][band][mix][lfo][block-size]")
{
    SECTION("Band Mix non-HQ")
    {
        checkHostBlockIndependence(MixTarget::band, false);
    }

    SECTION("Shape Mix non-HQ")
    {
        checkHostBlockIndependence(MixTarget::shape, false);
    }

    SECTION("Band Mix HQ")
    {
        checkHostBlockIndependence(MixTarget::band, true);
    }

    SECTION("Shape Mix HQ")
    {
        checkHostBlockIndependence(MixTarget::shape, true);
    }
}

TEST_CASE("Per-band mix LFOs preserve their offset across internal chunks",
          "[processor][band][mix][lfo][block-size][internal-chunk]")
{
    SECTION("Band Mix")
    {
        checkInternalChunkOffset(MixTarget::band, false);
    }

    SECTION("Shape Mix non-HQ")
    {
        checkInternalChunkOffset(MixTarget::shape, false);
    }

    SECTION("Band Mix HQ")
    {
        checkInternalChunkOffset(MixTarget::band, true);
    }

    SECTION("Shape Mix HQ")
    {
        checkInternalChunkOffset(MixTarget::shape, true);
    }
}
