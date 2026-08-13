#include "../Source/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 256;
constexpr int bypassTransitionSamples = static_cast<int>(testSampleRate * 0.05);
constexpr int holdLength = 11;
// DryWetMixer advances two float SmoothedValues independently for 2400
// samples. Their accumulated rounding differs slightly from the ideal blend
// calculated below (about 3.1e-5 at 48 kHz), while a hard switch misses it by
// roughly 0.87. Keep a small cross-platform margin around the float ramp.
constexpr float linearBlendTolerance = 5.0e-5f;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(
        parameter->getNormalisableRange().convertTo0to1(plainValue));
}

void configureIdentitySignalPath(FireAudioProcessor& processor)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    // Exercise Lo-Fi through the complete processor while making the single
    // multiband slot an identity path.
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      0.0f);
}

void configureLoFi(FireAudioProcessor& processor, bool enabled)
{
    configureIdentitySignalPath(processor);
    setPlainParameter(processor, DOWNSAMPLE_ID, static_cast<float>(holdLength));
    setPlainParameter(processor, BIT_DEPTH_ID, 32.0f);
    setPlainParameter(processor, JITTER_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_MIX_ID, 1.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, enabled ? 1.0f : 0.0f);
    processor.prepareToPlay(testSampleRate, testBlockSize);
}

juce::AudioBuffer<float> makeProbeInput(int firstStreamSample, int numSamples)
{
    juce::AudioBuffer<float> input(2, numSamples);

    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto streamSample = static_cast<float>(firstStreamSample + sample);
            const float channelOffset = static_cast<float>(channel) * 0.37f;
            const float value = 0.47f * std::sin(0.173f * streamSample + channelOffset)
                              + 0.29f * std::cos(0.071f * streamSample
                                                 + channelOffset * 1.7f);
            input.setSample(channel, sample, value);
        }
    }

    return input;
}

juce::AudioBuffer<float> processCopy(FireAudioProcessor& processor,
                                     const juce::AudioBuffer<float>& input)
{
    auto output = input;
    juce::MidiBuffer midi;
    processor.processBlock(output, midi);
    return output;
}

float maximumAbsoluteDifference(const juce::AudioBuffer<float>& first,
                                const juce::AudioBuffer<float>& second)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());

    float maximumDifference = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            maximumDifference = std::max(
                maximumDifference,
                std::abs(first.getSample(channel, sample)
                         - second.getSample(channel, sample)));

    return maximumDifference;
}

juce::AudioBuffer<float> integerDelayedCopy(
    const juce::AudioBuffer<float>& input,
    int latencySamples)
{
    REQUIRE(latencySamples >= 0);
    juce::AudioBuffer<float> delayed(input.getNumChannels(),
                                     input.getNumSamples());
    delayed.clear();
    const int samplesToCopy = input.getNumSamples() - latencySamples;
    if (samplesToCopy > 0)
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
            delayed.copyFrom(channel,
                             latencySamples,
                             input,
                             channel,
                             0,
                             samplesToCopy);
    return delayed;
}

float maximumLinearBlendError(const juce::AudioBuffer<float>& actual,
                              const juce::AudioBuffer<float>& dry,
                              const juce::AudioBuffer<float>& wet,
                              bool rampingToWet,
                              int latencySamples)
{
    REQUIRE(actual.getNumChannels() == dry.getNumChannels());
    REQUIRE(actual.getNumChannels() == wet.getNumChannels());
    REQUIRE(actual.getNumSamples() == dry.getNumSamples());
    REQUIRE(actual.getNumSamples() == wet.getNumSamples());

    float maximumError = 0.0f;
    for (int channel = 0; channel < actual.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < actual.getNumSamples(); ++sample)
        {
            const int preDelaySample = sample - latencySamples;
            const float ramp = preDelaySample < 0
                                   ? 0.0f
                                   : static_cast<float>(preDelaySample + 1)
                                         / static_cast<float>(bypassTransitionSamples);
            const float wetProportion = rampingToWet ? ramp : 1.0f - ramp;
            const float expected = dry.getSample(channel, sample)
                                 + wetProportion
                                       * (wet.getSample(channel, sample)
                                          - dry.getSample(channel, sample));
            maximumError = std::max(maximumError,
                                    std::abs(actual.getSample(channel, sample)
                                             - expected));
        }
    }

    return maximumError;
}
} // namespace

TEST_CASE("Lo-Fi has exact dry and sample-and-hold steady states",
          "[processor][lofi][bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("disabled is transparent")
    {
        FireAudioProcessor processor;
        configureLoFi(processor, false);

        const auto input = makeProbeInput(0, 97);
        const auto output = processCopy(processor, input);
        const auto delayedInput = integerDelayedCopy(
            input, processor.getLatencySamples());
        CHECK(maximumAbsoluteDifference(output, delayedInput) < 1.0e-6f);
    }

    SECTION("enabled holds each captured sample until the next rate interval")
    {
        FireAudioProcessor processor;
        configureLoFi(processor, true);

        juce::AudioBuffer<float> input(2, holdLength * 4);
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
            for (int sample = 0; sample < input.getNumSamples(); ++sample)
                input.setSample(channel, sample,
                                0.01f * static_cast<float>(sample + 1)
                                    + 0.2f * static_cast<float>(channel));

        const auto output = processCopy(processor, input);
        const int latencySamples = processor.getLatencySamples();
        float maximumHoldError = 0.0f;
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < output.getNumSamples(); ++sample)
            {
                const int preDelaySample = sample - latencySamples;
                const int capturedSample = preDelaySample < 0
                                               ? -1
                                               : preDelaySample
                                                     - preDelaySample % holdLength;
                const float expected = capturedSample < 0
                                           ? 0.0f
                                           : input.getSample(channel,
                                                             capturedSample);
                maximumHoldError = std::max(
                    maximumHoldError,
                    std::abs(output.getSample(channel, sample)
                             - expected));
            }
        }
        CHECK(maximumHoldError < 1.0e-6f);
    }
}

TEST_CASE("Lo-Fi enable transitions crossfade for 50 ms without resetting its wet path",
          "[processor][lofi][bypass][smoothing]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    FireAudioProcessor subject;
    FireAudioProcessor alwaysWet;
    FireAudioProcessor alwaysDry;
    configureLoFi(subject, true);
    configureLoFi(alwaysWet, true);
    configureLoFi(alwaysDry, false);
    const int latencySamples = subject.getLatencySamples();
    REQUIRE(latencySamples > 0);

    int streamPosition = 0;
    const auto processAll = [&](int numSamples)
    {
        const auto input = makeProbeInput(streamPosition, numSamples);
        streamPosition += numSamples;
        return std::array<juce::AudioBuffer<float>, 3> {
            processCopy(subject, input),
            processCopy(alwaysWet, input),
            processCopy(alwaysDry, input)
        };
    };

    // Align the subject's sample-and-hold state with the always-wet reference,
    // then switch only the subject to its dry state.
    const auto warmup = processAll(257);
    CHECK(maximumAbsoluteDifference(warmup[0], warmup[1]) < 1.0e-6f);

    setPlainParameter(subject, DOWNSAMPLE_BYPASS_ID, 0.0f);
    const auto fadeOut = processAll(bypassTransitionSamples);
    CHECK(maximumAbsoluteDifference(fadeOut[1], fadeOut[2]) > 0.25f);
    CHECK(maximumLinearBlendError(fadeOut[0],
                                  fadeOut[2],
                                  fadeOut[1],
                                  false,
                                  latencySamples)
          < linearBlendTolerance);

    // Let the fixed output delay emit the last samples of the transition
    // before checking the exact endpoint.
    processAll(latencySamples);

    // Once the 50 ms transition is complete the bypassed result is exactly dry.
    const auto drySteadyState = processAll(97);
    CHECK(maximumAbsoluteDifference(drySteadyState[0], drySteadyState[2])
          < 1.0e-6f);

    setPlainParameter(subject, DOWNSAMPLE_BYPASS_ID, 1.0f);
    const auto fadeIn = processAll(bypassTransitionSamples);
    CHECK(maximumAbsoluteDifference(fadeIn[1], fadeIn[2]) > 0.25f);
    CHECK(maximumLinearBlendError(fadeIn[0],
                                  fadeIn[2],
                                  fadeIn[1],
                                  true,
                                  latencySamples)
          < linearBlendTolerance);

    processAll(latencySamples);

    // The wet processor kept advancing while bypassed, so re-enabling cannot
    // restart the hold counter or capture a different sample.
    const auto wetSteadyState = processAll(113);
    CHECK(maximumAbsoluteDifference(wetSteadyState[0], wetSteadyState[1])
          < 1.0e-6f);
}
