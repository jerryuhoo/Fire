#include "helpers/ProcessingLatency.h"
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 64;
// End after the first half of a 2/3 pair, leaving a 0.5 residual dirty so the
// reset case can detect an implementation that clears the countdown only.
constexpr int renderedSamples = 1022;
constexpr float exactPathTolerance = 2.0e-6f;
constexpr float partitionTolerance = 2.0e-4f;

using Timeline = std::vector<std::vector<float>>;

enum class HoldRule
{
    fractionalResidual,
    legacyFloor
};

struct CallbackPattern
{
    const char* name;
    std::vector<int> sizes;
};

struct PairRender
{
    Timeline subject;
    Timeline upstreamReference;
    int reportedLatency = 0;
    bool finite = true;
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void setLayout(FireAudioProcessor& processor, int numChannels)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    const auto channelSet = numChannels == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    REQUIRE(processor.setBusesLayout(layout));
}

void configureProcessor(FireAudioProcessor& processor,
                        bool useHq,
                        int numChannels,
                        float rate,
                        bool enableLoFi)
{
    setLayout(processor, numChannels);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    // Keep the complete latency-bearing upstream path, but remove nonlinear
    // processing so a LoFi-disabled twin reveals exactly what LoFi receives.
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_SOLO_ID, 0),
                      0.0f);

    setPlainParameter(processor, DOWNSAMPLE_ID, rate);
    setPlainParameter(processor, BIT_DEPTH_ID, 32.0f);
    setPlainParameter(processor, JITTER_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_MIX_ID, 1.0f);
    setPlainParameter(processor,
                      DOWNSAMPLE_BYPASS_ID,
                      enableLoFi ? 1.0f : 0.0f);
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float probeSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = channel == 0 ? 0.17 : 0.71;
    return 0.43f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 5237.0 * time
                   + channelPhase))
         + 0.21f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 11939.0 * time
                   - 0.37 * channelPhase));
}

Timeline render(FireAudioProcessor& processor,
                int numChannels,
                const std::vector<int>& callbackPattern,
                bool& finite)
{
    REQUIRE_FALSE(callbackPattern.empty());
    for (const int callbackSize : callbackPattern)
    {
        REQUIRE(callbackSize > 0);
        REQUIRE(callbackSize <= preparedBlockSize);
    }

    Timeline output(static_cast<size_t>(numChannels));
    for (auto& channel : output)
        channel.reserve(renderedSamples);

    int streamPosition = 0;
    size_t callbackIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < renderedSamples)
    {
        const int requested = callbackPattern[
            callbackIndex % callbackPattern.size()];
        ++callbackIndex;
        const int samplesThisCallback = std::min(requested,
                                                 renderedSamples
                                                     - streamPosition);
        juce::AudioBuffer<float> audio(numChannels, samplesThisCallback);
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisCallback; ++sample)
                audio.setSample(channel,
                                sample,
                                probeSample(channel,
                                            streamPosition + sample));

        processor.processBlock(audio, midi);
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& destination = output[static_cast<size_t>(channel)];
            for (int sample = 0; sample < samplesThisCallback; ++sample)
            {
                const float value = audio.getSample(channel, sample);
                finite = finite && std::isfinite(value);
                destination.push_back(value);
            }
        }
        streamPosition += samplesThisCallback;
    }
    return output;
}

PairRender renderPair(bool useHq,
                      int numChannels,
                      float rate,
                      const std::vector<int>& callbackPattern)
{
    FireAudioProcessor subject;
    FireAudioProcessor upstreamReference;
    configureProcessor(subject, useHq, numChannels, rate, true);
    configureProcessor(upstreamReference, useHq, numChannels, rate, false);
    REQUIRE(subject.getLatencySamples()
            == upstreamReference.getLatencySamples());

    PairRender result;
    result.reportedLatency = subject.getLatencySamples();
    result.subject = render(subject,
                            numChannels,
                            callbackPattern,
                            result.finite);
    result.upstreamReference = render(upstreamReference,
                                     numChannels,
                                     callbackPattern,
                                     result.finite);
    return result;
}

int allocateHold(float rate, double& residual, HoldRule rule)
{
    if (rule == HoldRule::legacyFloor)
        return std::max(1, static_cast<int>(rate));

    const double accumulated = std::max(1.0, static_cast<double>(rate))
                               + residual;
    const int duration = std::max(1, static_cast<int>(std::floor(accumulated)));
    residual = accumulated - static_cast<double>(duration);
    return duration;
}

std::vector<int> firstHoldDurations(float rate, int count)
{
    std::vector<int> durations;
    durations.reserve(static_cast<size_t>(count));
    double residual = 0.0;
    for (int index = 0; index < count; ++index)
        durations.push_back(allocateHold(rate,
                                         residual,
                                         HoldRule::fractionalResidual));
    return durations;
}

Timeline offlineHold(const Timeline& upstreamReference,
                     bool useHq,
                     int reportedLatency,
                     float rate,
                     HoldRule rule)
{
    REQUIRE_FALSE(upstreamReference.empty());
    const int numSamples = static_cast<int>(upstreamReference.front().size());
    const int baseOutputDelay = fire::tests::legacyControlOutputDelay(useHq, reportedLatency);
    REQUIRE(baseOutputDelay >= 0);
    REQUIRE(baseOutputDelay < numSamples);

    Timeline expected(upstreamReference.size(),
                      std::vector<float>(static_cast<size_t>(numSamples),
                                         0.0f));
    for (size_t channel = 0; channel < upstreamReference.size(); ++channel)
    {
        REQUIRE(static_cast<int>(upstreamReference[channel].size())
                == numSamples);
        int remaining = 0;
        float held = 0.0f;
        double residual = 0.0;
        for (int sample = 0; sample < numSamples - baseOutputDelay; ++sample)
        {
            if (remaining <= 0)
            {
                // Base adds its fixed host-PDC delay after LoFi. Remove that
                // delay from the disabled twin before applying the canonical
                // hold, then restore it at the output. HQ latency is upstream
                // of LoFi, so its disabled output is already on the right clock.
                held = upstreamReference[channel][static_cast<size_t>(
                    sample + baseOutputDelay)];
                remaining = allocateHold(rate, residual, rule);
            }
            expected[channel][static_cast<size_t>(sample + baseOutputDelay)] = held;
            --remaining;
        }
    }
    return expected;
}

float maximumDifference(const Timeline& first, const Timeline& second)
{
    REQUIRE(first.size() == second.size());
    float error = 0.0f;
    for (size_t channel = 0; channel < first.size(); ++channel)
    {
        REQUIRE(first[channel].size() == second[channel].size());
        for (size_t sample = 0; sample < first[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(first[channel][sample]
                                      - second[channel][sample]));
    }
    return error;
}
} // namespace

TEST_CASE("LoFi Rate 2.5 alternates two- and three-sample captures",
          "[processor][lofi][rate][fractional][block-size]")
{
    CHECK(firstHoldDurations(2.5f, 8)
          == std::vector<int> { 2, 3, 2, 3, 2, 3, 2, 3 });

    const std::array<CallbackPattern, 3> patterns {{
        { "fixed", { 64 } },
        { "one-sample", { 1 } },
        { "irregular", { 17, 1, 31, 7, 3, 5 } }
    }};

    for (const bool useHq : { false, true })
    {
        for (const int numChannels : { 1, 2 })
        {
            const auto baseline = renderPair(useHq,
                                             numChannels,
                                             2.5f,
                                             patterns.front().sizes);
            for (const auto& pattern : patterns)
            {
                DYNAMIC_SECTION((useHq ? "HQ " : "Base ")
                                << numChannels << "ch " << pattern.name)
                {
                    const auto rendered = pattern.sizes == patterns.front().sizes
                                              ? baseline
                                              : renderPair(useHq,
                                                           numChannels,
                                                           2.5f,
                                                           pattern.sizes);
                    const auto fractional = offlineHold(
                        rendered.upstreamReference,
                        useHq,
                        rendered.reportedLatency,
                        2.5f,
                        HoldRule::fractionalResidual);
                    const auto legacyFloor = offlineHold(
                        rendered.upstreamReference,
                        useHq,
                        rendered.reportedLatency,
                        2.5f,
                        HoldRule::legacyFloor);
                    const float fractionalError = maximumDifference(
                        rendered.subject, fractional);
                    const float legacyFloorError = maximumDifference(
                        rendered.subject, legacyFloor);
                    const float oracleSeparation = maximumDifference(
                        fractional, legacyFloor);
                    const float subjectPartitionError = maximumDifference(
                        rendered.subject, baseline.subject);
                    const float referencePartitionError = maximumDifference(
                        rendered.upstreamReference,
                        baseline.upstreamReference);
                    CAPTURE(useHq,
                            numChannels,
                            pattern.name,
                            rendered.reportedLatency,
                            fractionalError,
                            legacyFloorError,
                            oracleSeparation,
                            subjectPartitionError,
                            referencePartitionError);
                    REQUIRE(rendered.finite);
                    REQUIRE(oracleSeparation > 0.05f);
                    CHECK(fractionalError < exactPathTolerance);
                    CHECK(legacyFloorError > 0.05f);
                    CHECK(subjectPartitionError < partitionTolerance);
                    CHECK(referencePartitionError < partitionTolerance);
                }
            }
        }
    }
}

TEST_CASE("LoFi integer Rate 17 retains the legacy floor schedule",
          "[processor][lofi][rate][integer][compatibility]")
{
    for (const bool useHq : { false, true })
    {
        for (const int numChannels : { 1, 2 })
        {
            DYNAMIC_SECTION((useHq ? "HQ " : "Base ")
                            << numChannels << "ch")
            {
                const auto rendered = renderPair(useHq,
                                                 numChannels,
                                                 17.0f,
                                                 { 17, 1, 31, 7, 3, 5 });
                const auto legacy = offlineHold(rendered.upstreamReference,
                                                useHq,
                                                rendered.reportedLatency,
                                                17.0f,
                                                HoldRule::legacyFloor);
                const float error = maximumDifference(rendered.subject, legacy);
                const float audibleSeparation = maximumDifference(
                    legacy, rendered.upstreamReference);
                CAPTURE(useHq,
                        numChannels,
                        rendered.reportedLatency,
                        error,
                        audibleSeparation);
                REQUIRE(rendered.finite);
                REQUIRE(audibleSeparation > 0.05f);
                CHECK(error < exactPathTolerance);
            }
        }
    }
}

TEST_CASE("LoFi reset restarts the fractional capture clock",
          "[processor][lofi][rate][fractional][reset]")
{
    for (const bool useHq : { false, true })
    {
        for (const int numChannels : { 1, 2 })
        {
            DYNAMIC_SECTION((useHq ? "HQ " : "Base ")
                            << numChannels << "ch")
            {
                FireAudioProcessor processor;
                configureProcessor(processor,
                                   useHq,
                                   numChannels,
                                   2.5f,
                                   true);
                bool finite = true;
                const auto first = render(processor,
                                          numChannels,
                                          { 17, 1, 31, 7, 3, 5 },
                                          finite);
                processor.reset();
                const auto repeated = render(processor,
                                             numChannels,
                                             { 17, 1, 31, 7, 3, 5 },
                                             finite);
                const float repeatError = maximumDifference(first, repeated);
                CAPTURE(useHq, numChannels, repeatError);
                REQUIRE(finite);
                CHECK(repeatError < exactPathTolerance);
            }
        }
    }
}
