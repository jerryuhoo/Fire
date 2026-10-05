#include "helpers/ProcessingLatency.h"
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int lfoCycleSamples = 480;
constexpr int warmupSamples = 12 * lfoCycleSamples;
constexpr int captureSamples = 8 * lfoCycleSamples;
constexpr std::array<float, 2> inputValues { 0.34f, -0.29f };

enum class MixState
{
    modulated,
    alwaysDry,
    alwaysWet
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

LfoData makeConstantLfo(float value)
{
    LfoData shape;
    shape.points = { { 0.0f, value }, { 1.0f, value } };
    shape.curvatures = { 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

float triangleSample(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoCycleSamples)
                        / static_cast<float>(lfoCycleSamples);
    return phase < 0.5f ? 2.0f * phase : 2.0f * (1.0f - phase);
}

void configureProcessor(FireAudioProcessor& processor,
                        int preparedBlockSize,
                        bool useHq,
                        MixState state)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    // A disabled single band is the latency-correct identity path. LoFi uses
    // a deterministic sample-and-hold/quantisation fixture: Jitter is zero so
    // independent processors have identical wet state, while Rate=17 and
    // Bits=5 make the constant dry/wet endpoints audibly distinct.
    setPlainParameter(processor, firstBandParameter(BAND_ENABLE_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 1.0f);
    setPlainParameter(processor, DOWNSAMPLE_ID, 17.0f);
    setPlainParameter(processor, BIT_DEPTH_ID, 5.0f);
    setPlainParameter(processor, JITTER_ID, 0.0f);
    setPlainParameter(processor,
                      DOWNSAMPLE_MIX_ID,
                      state == MixState::alwaysDry ? 0.0f
                      : state == MixState::alwaysWet ? 1.0f
                                                     : 0.5f);

    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0),
                      100.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_PHASE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0),
                      0.0f);
    processor.getLfoManager().setLfoData(0, makeTriangleLfo());
    if (state == MixState::modulated)
    {
        processor.assignLfoToTarget(0, DOWNSAMPLE_MIX_ID);
        processor.setModulationDepth(DOWNSAMPLE_MIX_ID, 1.0f);
    }

    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    float analyticError = 0.0f;
    float endpointSeparation = 0.0f;
    bool finite = true;
};

RenderResult render(bool useHq,
                    int preparedBlockSize,
                    const std::vector<int>& callbackPattern)
{
    REQUIRE_FALSE(callbackPattern.empty());
    for (const int blockSize : callbackPattern)
        REQUIRE(blockSize > 0);

    FireAudioProcessor subject;
    FireAudioProcessor alwaysDry;
    FireAudioProcessor alwaysWet;
    configureProcessor(subject,
                       preparedBlockSize,
                       useHq,
                       MixState::modulated);
    configureProcessor(alwaysDry,
                       preparedBlockSize,
                       useHq,
                       MixState::alwaysDry);
    configureProcessor(alwaysWet,
                       preparedBlockSize,
                       useHq,
                       MixState::alwaysWet);
    const int controlDelay = fire::tests::legacyControlOutputDelay(useHq, subject.getLatencySamples());

    RenderResult result;
    for (auto& channel : result.output)
        channel.reserve(captureSamples);

    constexpr int totalSamples = warmupSamples + captureSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        const int requested = callbackPattern[callbackIndex
                                              % callbackPattern.size()];
        ++callbackIndex;
        const int samplesThisCallback = std::min(requested,
                                                 totalSamples - streamPosition);
        juce::AudioBuffer<float> subjectAudio(2, samplesThisCallback);
        for (int channel = 0; channel < 2; ++channel)
        {
            juce::FloatVectorOperations::fill(
                subjectAudio.getWritePointer(channel),
                inputValues[static_cast<size_t>(channel)],
                samplesThisCallback);
        }
        auto dryAudio = subjectAudio;
        auto wetAudio = subjectAudio;
        subject.processBlock(subjectAudio, midi);
        alwaysDry.processBlock(dryAudio, midi);
        alwaysWet.processBlock(wetAudio, midi);

        for (int sample = 0; sample < samplesThisCallback; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            if (absoluteSample < warmupSamples)
                continue;

            // Base mode delays the complete post-LoFi result by the fixed host
            // PDC D; HQ's natural latency is upstream of LoFi, so its control
            // trajectory is audible at the current output index.
            const float mix = triangleSample(absoluteSample - controlDelay);
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectAudio.getSample(channel, sample);
                const float dry = dryAudio.getSample(channel, sample);
                const float wet = wetAudio.getSample(channel, sample);
                const float expected = dry + mix * (wet - dry);
                result.finite = result.finite && std::isfinite(actual)
                                && std::isfinite(dry) && std::isfinite(wet);
                result.analyticError = std::max(result.analyticError,
                                                std::abs(actual - expected));
                result.endpointSeparation = std::max(
                    result.endpointSeparation,
                    std::abs(wet - dry));
                result.output[static_cast<size_t>(channel)].push_back(actual);
            }
        }
        streamPosition += samplesThisCallback;
    }
    return result;
}

float maximumDifference(const RenderResult& first,
                        const RenderResult& second)
{
    float error = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (size_t sample = 0; sample < first.output[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(first.output[channel][sample]
                                      - second.output[channel][sample]));
    }
    return error;
}
} // namespace

TEST_CASE("LoFi Mix preserves its analytic LFO trajectory across host ranges",
          "[processor][lofi][mix][lfo][trajectory][block-size][oversized]")
{
    for (const bool useHq : std::array { false, true })
    {
        const auto fixed = render(useHq, lfoCycleSamples, { lfoCycleSamples });
        const auto irregular = render(useHq,
                                      257,
                                      { 17, 31, 43, 29, 53, 37 });
        const auto oversized = render(useHq, 257, { 8193 });
        const float irregularError = maximumDifference(fixed, irregular);
        const float oversizedError = maximumDifference(fixed, oversized);
        CAPTURE(useHq,
                fixed.analyticError,
                irregular.analyticError,
                oversized.analyticError,
                fixed.endpointSeparation,
                irregularError,
                oversizedError);
        REQUIRE(fixed.finite);
        REQUIRE(irregular.finite);
        REQUIRE(oversized.finite);
        REQUIRE(fixed.endpointSeparation > 0.015f);
        CHECK(fixed.analyticError < 2.0e-4f);
        CHECK(irregular.analyticError < 2.0e-4f);
        CHECK(oversized.analyticError < 2.0e-4f);
        CHECK(irregularError < 2.0e-4f);
        CHECK(oversizedError < 2.0e-4f);
    }
}

TEST_CASE("LoFi Mix bridges an active LFO source change in ten milliseconds",
          "[processor][lofi][mix][lfo][recipe-transition][source]")
{
    constexpr int bridgeSamples = 480;
    constexpr float oldMix = 0.2f;
    constexpr float newMix = 0.8f;
    constexpr int warmSamples = 4096;

    for (const bool useHq : std::array { false, true })
    {
        FireAudioProcessor subject;
        FireAudioProcessor alwaysDry;
        FireAudioProcessor alwaysWet;
        configureProcessor(subject, 512, useHq, MixState::modulated);
        configureProcessor(alwaysDry, 512, useHq, MixState::alwaysDry);
        configureProcessor(alwaysWet, 512, useHq, MixState::alwaysWet);

        // Keep the modulation range independent from the stable-trajectory
        // case: bipolar base .5/depth .6 maps constant sources 0 and 1 to
        // exact mix endpoints .2 and .8.
        for (auto* processor : std::array { &subject, &alwaysDry, &alwaysWet })
        {
            processor->getLfoManager().setLfoData(0, makeConstantLfo(0.0f));
            processor->getLfoManager().setLfoData(1, makeConstantLfo(1.0f));
        }
        subject.assignLfoToTarget(0, DOWNSAMPLE_MIX_ID);
        subject.setModulationDepth(DOWNSAMPLE_MIX_ID, 0.6f);

        juce::MidiBuffer midi;
        int streamPosition = 0;
        for (int processed = 0; processed < warmSamples; processed += 256)
        {
            juce::AudioBuffer<float> subjectAudio(2, 256);
            for (int channel = 0; channel < 2; ++channel)
            {
                juce::FloatVectorOperations::fill(
                    subjectAudio.getWritePointer(channel),
                    inputValues[static_cast<size_t>(channel)],
                    256);
            }
            auto dryAudio = subjectAudio;
            auto wetAudio = subjectAudio;
            subject.processBlock(subjectAudio, midi);
            alwaysDry.processBlock(dryAudio, midi);
            alwaysWet.processBlock(wetAudio, midi);
            streamPosition += 256;
        }

        subject.assignLfoToTarget(1, DOWNSAMPLE_MIX_ID);
        const int audibleOffset = fire::tests::legacyControlOutputDelay(useHq, subject.getLatencySamples());
        const int eventSamples = audibleOffset + bridgeSamples + 1;
        juce::AudioBuffer<float> subjectAudio(2, eventSamples);
        for (int channel = 0; channel < 2; ++channel)
        {
            juce::FloatVectorOperations::fill(
                subjectAudio.getWritePointer(channel),
                inputValues[static_cast<size_t>(channel)],
                eventSamples);
        }
        auto dryAudio = subjectAudio;
        auto wetAudio = subjectAudio;
        subject.processBlock(subjectAudio, midi);
        alwaysDry.processBlock(dryAudio, midi);
        alwaysWet.processBlock(wetAudio, midi);

        float endpointSeparation = 0.0f;
        bool finite = true;
        for (int sample = 0; sample < eventSamples; ++sample)
        {
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectAudio.getSample(channel, sample);
                const float dry = dryAudio.getSample(channel, sample);
                const float wet = wetAudio.getSample(channel, sample);
                finite = finite && std::isfinite(actual)
                         && std::isfinite(dry) && std::isfinite(wet);
                endpointSeparation = std::max(endpointSeparation,
                                              std::abs(wet - dry));
            }
        }

        const auto projectionError = [&](int bridgeSample)
        {
            const int sample = audibleOffset + bridgeSample;
            const float progress = static_cast<float>(bridgeSample)
                                   / static_cast<float>(bridgeSamples);
            const float mix = oldMix + progress * (newMix - oldMix);
            float error = 0.0f;
            for (int channel = 0; channel < 2; ++channel)
            {
                const float dry = dryAudio.getSample(channel, sample);
                const float wet = wetAudio.getSample(channel, sample);
                const float expected = dry + mix * (wet - dry);
                error = std::max(
                    error,
                    std::abs(subjectAudio.getSample(channel, sample)
                             - expected));
            }
            return error;
        };
        const float firstError = projectionError(0);
        const float midpointError = projectionError(240);
        const float endpointError = projectionError(480);
        CAPTURE(useHq,
                audibleOffset,
                streamPosition,
                endpointSeparation,
                firstError,
                midpointError,
                endpointError);
        REQUIRE(finite);
        REQUIRE(endpointSeparation > 0.015f);
        CHECK(firstError < 2.0e-4f);
        CHECK(midpointError < 2.0e-4f);
        CHECK(endpointError < 2.0e-4f);
    }
}
