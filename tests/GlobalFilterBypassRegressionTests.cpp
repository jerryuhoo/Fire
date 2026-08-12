#include "../Source/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int transitionSamples = static_cast<int>(sampleRate * 0.05);
// JUCE's DryWetMixer advances independent float SmoothedValues for its two
// gains. Float accumulation over 2400 samples differs slightly from an ideal
// analytical ramp, so leave the same small cross-platform margin used by the
// Lo-Fi bypass regression.
constexpr float blendTolerance = 2.0e-4f;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(
        parameter->getNormalisableRange().convertTo0to1(plainValue));
}

void configureFilter(FireAudioProcessor& processor, bool enabled)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      0.0f);

    // A high-Q, high-gain recipe gives the wet/dry references clear separation
    // and a long enough recursive state to expose a frozen-tail restart.
    setPlainParameter(processor, LOWCUT_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, PEAK_BYPASSED_ID, 0.0f);
    setPlainParameter(processor, PEAK_FREQ_ID, 1837.0f);
    setPlainParameter(processor, PEAK_GAIN_ID, 24.0f);
    setPlainParameter(processor, PEAK_Q_ID, 5.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, enabled ? 1.0f : 0.0f);
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

juce::AudioBuffer<float> makeProbeInput(int firstStreamSample, int numSamples)
{
    juce::AudioBuffer<float> input(2, numSamples);
    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < input.getNumSamples(); ++sample)
        {
            const auto absoluteSample = static_cast<double>(firstStreamSample + sample);
            const auto channelPhase = static_cast<double>(channel) * 0.37;
            const auto resonantPhase = juce::MathConstants<double>::twoPi
                                       * 1837.0 * absoluteSample / sampleRate;
            const auto secondaryPhase = juce::MathConstants<double>::twoPi
                                        * 421.0 * absoluteSample / sampleRate;
            const float value = 0.24f * static_cast<float>(
                                      std::sin(resonantPhase + 0.19 + channelPhase))
                              + 0.13f * static_cast<float>(
                                      std::cos(secondaryPhase + 0.67 + channelPhase));
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

juce::AudioBuffer<float> processBypassedCopy(
    FireAudioProcessor& processor,
    const juce::AudioBuffer<float>& input)
{
    auto output = input;
    juce::MidiBuffer midi;
    processor.processBlockBypassed(output, midi);
    return output;
}

float maximumAbsoluteDifference(const juce::AudioBuffer<float>& first,
                                const juce::AudioBuffer<float>& second)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());

    float maximumDifference = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
        {
            const float firstValue = first.getSample(channel, sample);
            const float secondValue = second.getSample(channel, sample);
            REQUIRE(std::isfinite(firstValue));
            REQUIRE(std::isfinite(secondValue));
            maximumDifference = std::max(maximumDifference,
                                         std::abs(firstValue - secondValue));
        }
    }

    return maximumDifference;
}

float maximumLinearBlendError(const juce::AudioBuffer<float>& actual,
                              const juce::AudioBuffer<float>& dry,
                              const juce::AudioBuffer<float>& wet,
                              bool rampingToWet)
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
            const float ramp = static_cast<float>(sample + 1)
                             / static_cast<float>(transitionSamples);
            const float wetProportion = rampingToWet ? ramp : 1.0f - ramp;
            const float expected = dry.getSample(channel, sample)
                                 + wetProportion
                                       * (wet.getSample(channel, sample)
                                          - dry.getSample(channel, sample));
            const float value = actual.getSample(channel, sample);
            REQUIRE(std::isfinite(value));
            REQUIRE(std::isfinite(expected));
            maximumError = std::max(maximumError, std::abs(value - expected));
        }
    }

    return maximumError;
}
} // namespace

TEST_CASE("Global filter enable crossfades while its wet state keeps advancing",
          "[processor][filter][bypass][smoothing]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    FireAudioProcessor subject;
    FireAudioProcessor alwaysWet;
    FireAudioProcessor alwaysDry;
    configureFilter(subject, true);
    configureFilter(alwaysWet, true);
    configureFilter(alwaysDry, false);

    int streamPosition = 0;
    const auto processAll = [&] (int numSamples)
    {
        const auto input = makeProbeInput(streamPosition, numSamples);
        streamPosition += numSamples;
        return std::array<juce::AudioBuffer<float>, 3> {
            processCopy(subject, input),
            processCopy(alwaysWet, input),
            processCopy(alwaysDry, input)
        };
    };

    // All wet filter histories begin aligned before only the subject is
    // switched off.
    const auto warmup = processAll(4096);
    CHECK(maximumAbsoluteDifference(warmup[0], warmup[1]) < 1.0e-6f);

    setPlainParameter(subject, FILTER_BYPASS_ID, 0.0f);
    const auto fadeOut = processAll(transitionSamples);
    const float wetDrySeparationOut = maximumAbsoluteDifference(fadeOut[1],
                                                                 fadeOut[2]);
    const float fadeOutError = maximumLinearBlendError(fadeOut[0],
                                                        fadeOut[2],
                                                        fadeOut[1],
                                                        false);
    INFO("fade-out wet/dry separation = " << wetDrySeparationOut);
    INFO("fade-out maximum linear blend error = " << fadeOutError);
    CHECK(wetDrySeparationOut > 0.5f);
    CHECK(fadeOutError < blendTolerance);

    // The disabled result becomes exactly dry after the transition.
    const auto drySteadyState = processAll(317);
    CHECK(maximumAbsoluteDifference(drySteadyState[0], drySteadyState[2])
          < 1.0e-6f);

    // Keep the bypass interval deliberately non-periodic relative to both
    // probe tones. If the recursive filter is skipped, its frozen history will
    // no longer match the always-wet reference at re-enable.
    const auto bypassInterval = processAll(1739);
    CHECK(maximumAbsoluteDifference(bypassInterval[0], bypassInterval[2])
          < 1.0e-6f);

    setPlainParameter(subject, FILTER_BYPASS_ID, 1.0f);
    const auto fadeIn = processAll(transitionSamples);
    const float wetDrySeparationIn = maximumAbsoluteDifference(fadeIn[1],
                                                                fadeIn[2]);
    const float fadeInError = maximumLinearBlendError(fadeIn[0],
                                                       fadeIn[2],
                                                       fadeIn[1],
                                                       true);
    INFO("fade-in wet/dry separation = " << wetDrySeparationIn);
    INFO("fade-in maximum linear blend error = " << fadeInError);
    CHECK(wetDrySeparationIn > 0.5f);
    CHECK(fadeInError < blendTolerance);

    // Once fully enabled, exact agreement proves the wet filter was processed
    // continuously during bypass. This also rejects a stale recursive tail
    // being emitted when the switch comes back on.
    const auto wetSteadyState = processAll(521);
    const float finalWetStateError = maximumAbsoluteDifference(
        wetSteadyState[0], wetSteadyState[1]);
    INFO("post-enable wet-state maximum error = " << finalWetStateError);
    CHECK(finalWetStateError < 1.0e-6f);
}

TEST_CASE("Global filter mixer primes to its prepared enable state",
          "[processor][filter][bypass][priming]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("enabled first callback is fully wet")
    {
        FireAudioProcessor enabled;
        FireAudioProcessor disabled;
        configureFilter(enabled, true);
        configureFilter(disabled, false);

        const auto input = makeProbeInput(0, preparedBlockSize);
        const auto wetOutput = processCopy(enabled, input);
        const auto dryOutput = processCopy(disabled, input);
        const float wetDrySeparation = maximumAbsoluteDifference(wetOutput,
                                                                  dryOutput);
        const float enabledInputSeparation = maximumAbsoluteDifference(
            wetOutput, input);
        INFO("first-block wet/dry separation = " << wetDrySeparation);
        INFO("enabled first-block input separation = "
             << enabledInputSeparation);

        CHECK(wetDrySeparation > 0.5f);
        CHECK(enabledInputSeparation > 0.5f);
    }

    SECTION("disabled first callback is exactly dry")
    {
        FireAudioProcessor disabled;
        configureFilter(disabled, false);

        const auto input = makeProbeInput(0, preparedBlockSize);
        const auto output = processCopy(disabled, input);
        CHECK(maximumAbsoluteDifference(output, input) < 1.0e-6f);
    }
}

TEST_CASE("Host bypass before first processing callback does not poison filter priming",
          "[processor][filter][bypass][priming][host-bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    FireAudioProcessor subject;
    FireAudioProcessor enabledReference;
    FireAudioProcessor dryReference;
    configureFilter(subject, true);
    configureFilter(enabledReference, true);
    configureFilter(dryReference, false);

    // Exercise the lifecycle where prepare/reset is consumed by the host's
    // bypass callback. No global-filter dry samples enter its FIFO here.
    const auto bypassInput = makeProbeInput(0, 193);
    const auto subjectBypass = processBypassedCopy(subject, bypassInput);
    const auto referenceBypass = processBypassedCopy(enabledReference,
                                                     bypassInput);
    CHECK(maximumAbsoluteDifference(subjectBypass, bypassInput) < 1.0e-6f);
    CHECK(maximumAbsoluteDifference(subjectBypass, referenceBypass) < 1.0e-6f);

    // The first subsequent normal callback must still prime directly to the
    // prepared enabled state. A stale/empty mixer FIFO must not introduce a
    // dry fade or uninitialised history.
    const auto normalInput = makeProbeInput(bypassInput.getNumSamples(),
                                           preparedBlockSize);
    const auto subjectOutput = processCopy(subject, normalInput);
    const auto wetReference = processCopy(enabledReference, normalInput);
    const auto dryOutput = processCopy(dryReference, normalInput);
    const float subjectWetError = maximumAbsoluteDifference(subjectOutput,
                                                             wetReference);
    const float wetDrySeparation = maximumAbsoluteDifference(wetReference,
                                                              dryOutput);
    INFO("post-host-bypass first wet error = " << subjectWetError);
    INFO("post-host-bypass wet/dry separation = " << wetDrySeparation);

    CHECK(wetDrySeparation > 0.5f);
    CHECK(subjectWetError < 1.0e-6f);

    // The following callback also remains coherent, guarding the FIFO read/
    // write lifecycle beyond the first recovered block.
    const auto continuationInput = makeProbeInput(
        bypassInput.getNumSamples() + normalInput.getNumSamples(), 311);
    const auto subjectContinuation = processCopy(subject, continuationInput);
    const auto wetContinuation = processCopy(enabledReference,
                                             continuationInput);
    CHECK(maximumAbsoluteDifference(subjectContinuation, wetContinuation)
          < 1.0e-6f);
}
