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
constexpr int routeRampSamples = 480;
constexpr int warmupSamples = 12 * lfoCycleSamples;
constexpr int captureSamples = 8 * lfoCycleSamples;

enum class GlobalMixState
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

float idealTriangleAtPhase(float phase)
{
    return phase < 0.5f ? 2.0f * phase : 2.0f * (1.0f - phase);
}

float triangleWavetableLookup(float phase)
{
    // LfoEngine samples shapes into 1024 entries spanning 1023 intervals,
    // then linearly interpolates that table. The triangle cusp at phase .5 is
    // halfway between entries 511 and 512, so the real engine peak is about
    // .999022 rather than the ideal mathematical value 1. Reproducing that
    // independent table construction keeps this test about Global Mix rather
    // than falsely attributing the LFO's documented discretisation to it.
    constexpr size_t tableSize = 1024;
    constexpr float tableIntervals = static_cast<float>(tableSize - 1);
    const float safePhase = juce::jlimit(0.0f, 1.0f, phase);
    const float tablePosition = safePhase * tableIntervals;
    const size_t firstIndex = juce::jmin(
        static_cast<size_t>(tablePosition), tableSize - 1);
    const size_t secondIndex = juce::jmin(firstIndex + 1, tableSize - 1);
    const float fraction = tablePosition - static_cast<float>(firstIndex);
    const auto tableValue = [](size_t index)
    {
        return idealTriangleAtPhase(
            static_cast<float>(index) / tableIntervals);
    };
    const float first = tableValue(firstIndex);
    const float second = tableValue(secondIndex);
    return first + fraction * (second - first);
}

std::vector<float> makeExpectedTriangleTrajectory(int numSamples)
{
    REQUIRE(numSamples >= 0);
    std::vector<float> result(static_cast<size_t>(numSamples));
    float phase = 0.0f;
    const float phaseDelta = 100.0f / static_cast<float>(sampleRate);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        result[static_cast<size_t>(sample)] = triangleWavetableLookup(phase);
        phase += phaseDelta;
        if (phase >= 1.0f || phase < 0.0f)
            phase -= std::floor(phase);
    }
    return result;
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = static_cast<double>(channel) * 0.31;
    return 0.47f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 731.0 * time
               + channelPhase))
         + 0.19f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 1817.0 * time
               + 0.2 - channelPhase));
}

void configureProcessor(FireAudioProcessor& processor,
                        int preparedBlockSize,
                        bool useHq,
                        GlobalMixState state)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor,
                      MIX_ID,
                      state == GlobalMixState::alwaysDry ? 0.0f
                      : state == GlobalMixState::alwaysWet ? 1.0f
                                                            : 0.5f);

    // The wet band is deliberately 18 dB quieter than the latency-matched
    // global dry path. All other optional stages are neutral, keeping the
    // dry/wet endpoint relationship linear in both Base and HQ.
    setPlainParameter(processor, firstBandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(SHAPE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(SAFE_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(EXTREME_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(MODE_ID), 4.0f);
    setPlainParameter(processor, firstBandParameter(OUTPUT_ID), -18.0f);
    setPlainParameter(processor, firstBandParameter(MIX_ID), 1.0f);

    for (int lfo = 0; lfo < 2; ++lfo)
    {
        setPlainParameter(
            processor,
            ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, lfo),
            0.0f);
        setPlainParameter(
            processor,
            ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, lfo),
            100.0f);
        setPlainParameter(
            processor,
            ParameterIDAndName::getIDString(LFO_PHASE_ID, lfo),
            0.0f);
        setPlainParameter(
            processor,
            ParameterIDAndName::getIDString(LFO_SMOOTH_ID, lfo),
            0.0f);
        processor.getLfoManager().setLfoData(lfo, makeTriangleLfo());
    }

    if (state == GlobalMixState::modulated)
    {
        processor.assignLfoToTarget(0, MIX_ID);
        processor.setModulationDepth(MIX_ID, 1.0f);
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

RenderResult renderStable(bool useHq,
                          const std::vector<int>& callbackPattern)
{
    REQUIRE_FALSE(callbackPattern.empty());
    for (const int blockSize : callbackPattern)
        REQUIRE(blockSize > 0);

    constexpr int preparedBlockSize = lfoCycleSamples;
    FireAudioProcessor subject;
    FireAudioProcessor alwaysDry;
    FireAudioProcessor alwaysWet;
    configureProcessor(subject,
                       preparedBlockSize,
                       useHq,
                       GlobalMixState::modulated);
    configureProcessor(alwaysDry,
                       preparedBlockSize,
                       useHq,
                       GlobalMixState::alwaysDry);
    configureProcessor(alwaysWet,
                       preparedBlockSize,
                       useHq,
                       GlobalMixState::alwaysWet);

    const int controlDelay = fire::tests::legacyControlOutputDelay(useHq, subject.getLatencySamples());
    REQUIRE(alwaysDry.getLatencySamples() == subject.getLatencySamples());
    REQUIRE(alwaysWet.getLatencySamples() == subject.getLatencySamples());

    RenderResult result;
    for (auto& channel : result.output)
        channel.reserve(captureSamples);

    constexpr int totalSamples = warmupSamples + captureSamples;
    const auto expectedLfo = makeExpectedTriangleTrajectory(totalSamples);
    int streamPosition = 0;
    size_t callbackIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        const int requested = callbackPattern[callbackIndex
                                              % callbackPattern.size()];
        ++callbackIndex;
        const int blockSize = std::min(requested,
                                       totalSamples - streamPosition);
        juce::AudioBuffer<float> subjectAudio(2, blockSize);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
                subjectAudio.setSample(channel,
                                       sample,
                                       inputSample(channel,
                                                   streamPosition + sample));
        auto dryAudio = subjectAudio;
        auto wetAudio = subjectAudio;
        subject.processBlock(subjectAudio, midi);
        alwaysDry.processBlock(dryAudio, midi);
        alwaysWet.processBlock(wetAudio, midi);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            if (absoluteSample < warmupSamples)
                continue;

            // Base mode delays the complete Global Mix result by fixed PDC D.
            // HQ's natural latency is already upstream of this coefficient.
            const float mix = expectedLfo[static_cast<size_t>(
                absoluteSample - controlDelay)];
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectAudio.getSample(channel, sample);
                const float dry = dryAudio.getSample(channel, sample);
                const float wet = wetAudio.getSample(channel, sample);
                const float expected = dry + mix * (wet - dry);
                result.finite = result.finite && std::isfinite(actual)
                                && std::isfinite(dry) && std::isfinite(wet);
                result.analyticError = std::max(
                    result.analyticError, std::abs(actual - expected));
                result.endpointSeparation = std::max(
                    result.endpointSeparation, std::abs(wet - dry));
                result.output[static_cast<size_t>(channel)].push_back(actual);
            }
        }
        streamPosition += blockSize;
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

void checkSourceBridge(bool useHq)
{
    constexpr float oldMix = 0.2f;
    constexpr float newMix = 0.8f;
    FireAudioProcessor subject;
    FireAudioProcessor alwaysDry;
    FireAudioProcessor alwaysWet;
    configureProcessor(subject, 512, useHq, GlobalMixState::modulated);
    configureProcessor(alwaysDry, 512, useHq, GlobalMixState::alwaysDry);
    configureProcessor(alwaysWet, 512, useHq, GlobalMixState::alwaysWet);

    for (auto* processor : std::array { &subject, &alwaysDry, &alwaysWet })
    {
        processor->getLfoManager().setLfoData(0, makeConstantLfo(0.0f));
        processor->getLfoManager().setLfoData(1, makeConstantLfo(1.0f));
    }
    subject.assignLfoToTarget(0, MIX_ID);
    subject.setModulationDepth(MIX_ID, 0.6f);

    juce::MidiBuffer midi;
    constexpr int warmBlockSize = 256;
    constexpr std::array<float, 2> inputValues { 0.34f, -0.29f };
    for (int processed = 0; processed < 4096; processed += warmBlockSize)
    {
        juce::AudioBuffer<float> subjectAudio(2, warmBlockSize);
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(
                subjectAudio.getWritePointer(channel),
                inputValues[static_cast<size_t>(channel)],
                warmBlockSize);
        auto dryAudio = subjectAudio;
        auto wetAudio = subjectAudio;
        subject.processBlock(subjectAudio, midi);
        alwaysDry.processBlock(dryAudio, midi);
        alwaysWet.processBlock(wetAudio, midi);
    }

    subject.assignLfoToTarget(1, MIX_ID);
    const int audibleOffset = fire::tests::legacyControlOutputDelay(useHq, subject.getLatencySamples());
    const int eventSamples = audibleOffset + routeRampSamples + 1;
    juce::AudioBuffer<float> subjectAudio(2, eventSamples);
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::fill(
            subjectAudio.getWritePointer(channel),
            inputValues[static_cast<size_t>(channel)],
            eventSamples);
    auto dryAudio = subjectAudio;
    auto wetAudio = subjectAudio;
    subject.processBlock(subjectAudio, midi);
    alwaysDry.processBlock(dryAudio, midi);
    alwaysWet.processBlock(wetAudio, midi);

    const auto projectionError = [&](int transitionSample, float mix)
    {
        const int sample = audibleOffset + transitionSample;
        float error = 0.0f;
        for (int channel = 0; channel < 2; ++channel)
        {
            const float dry = dryAudio.getSample(channel, sample);
            const float wet = wetAudio.getSample(channel, sample);
            const float expected = dry + mix * (wet - dry);
            error = std::max(error,
                             std::abs(subjectAudio.getSample(channel, sample)
                                      - expected));
        }
        return error;
    };

    float endpointSeparation = 0.0f;
    bool finite = true;
    for (int sample = 0; sample < eventSamples; ++sample)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            finite = finite
                     && std::isfinite(subjectAudio.getSample(channel, sample));
            endpointSeparation = std::max(
                endpointSeparation,
                std::abs(wetAudio.getSample(channel, sample)
                         - dryAudio.getSample(channel, sample)));
        }
    }

    const float firstError = projectionError(0, oldMix);
    const float midpointError = projectionError(240, 0.5f);
    const float endpointError = projectionError(480, newMix);
    CAPTURE(useHq,
            audibleOffset,
            endpointSeparation,
            firstError,
            midpointError,
            endpointError);
    REQUIRE(finite);
    REQUIRE(endpointSeparation > 0.05f);
    CHECK(firstError < 2.0e-4f);
    CHECK(midpointError < 2.0e-4f);
    CHECK(endpointError < 2.0e-4f);
}
} // namespace

TEST_CASE("Global Mix preserves its analytic LFO trajectory in Base and HQ",
          "[processor][global-mix][lfo][trajectory][block-size]")
{
    const std::vector<int> fixed { lfoCycleSamples };
    const std::vector<int> irregular { 17, 31, 43, 29, 53, 37 };
    for (const bool useHq : std::array { false, true })
    {
        const auto fixedRender = renderStable(useHq, fixed);
        const auto irregularRender = renderStable(useHq, irregular);
        const float partitionError = maximumDifference(fixedRender,
                                                       irregularRender);
        CAPTURE(useHq,
                fixedRender.analyticError,
                irregularRender.analyticError,
                fixedRender.endpointSeparation,
                partitionError);
        REQUIRE(fixedRender.finite);
        REQUIRE(irregularRender.finite);
        REQUIRE(fixedRender.endpointSeparation > 0.05f);
        CHECK(fixedRender.analyticError < 2.0e-4f);
        CHECK(irregularRender.analyticError < 2.0e-4f);
        CHECK(partitionError < 2.0e-4f);
    }
}

TEST_CASE("Global Mix bridges an active LFO source change in ten milliseconds",
          "[processor][global-mix][lfo][recipe-transition][source]")
{
    checkSourceBridge(false);
    checkSourceBridge(true);
}
