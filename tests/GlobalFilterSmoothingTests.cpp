#include "../Source/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 128;
constexpr int warmupBlocks = 128;
constexpr int settlingBlocks = 128;
constexpr int modulationWarmupSamples = 4096;
constexpr int modulationComparisonSamples = 48000;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(
        parameter->getNormalisableRange().convertTo0to1(plainValue));
}

void configureIdentityPath(FireAudioProcessor& processor)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      0.0f);
}

juce::AudioBuffer<float> makeProbeInput(int firstStreamSample,
                                        float primaryFrequency)
{
    juce::AudioBuffer<float> input(2, blockSize);

    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < input.getNumSamples(); ++sample)
        {
            const auto streamSample = static_cast<double>(firstStreamSample + sample);
            const auto channelPhase = static_cast<double>(channel) * 0.41;
            const auto primaryPhase = juce::MathConstants<double>::twoPi
                                      * static_cast<double>(primaryFrequency)
                                      * streamSample / sampleRate;
            const auto secondaryPhase = juce::MathConstants<double>::twoPi
                                        * 317.0 * streamSample / sampleRate;
            const float value = 0.53f * static_cast<float>(std::sin(primaryPhase + 0.37 + channelPhase))
                              + 0.19f * static_cast<float>(std::cos(secondaryPhase + 0.11 + channelPhase));
            input.setSample(channel, sample, value);
        }
    }

    return input;
}

struct AutomationProbe
{
    int firstAudibleSample = 0;
    float firstDistanceFromHeld = 0.0f;
    float firstHardJumpDistance = 0.0f;
    float firstDistanceFromHardJump = 0.0f;
    float settledMaximumError = 0.0f;
};

template <typename ConfigureFilter>
AutomationProbe runAutomationProbe(const juce::String& parameterID,
                                   float initialValue,
                                   float targetValue,
                                   float primaryFrequency,
                                   ConfigureFilter&& configureFilter)
{
    FireAudioProcessor subject;
    FireAudioProcessor heldReference;
    FireAudioProcessor hardJumpReference;

    const std::array<FireAudioProcessor*, 3> processors {
        &subject, &heldReference, &hardJumpReference
    };

    for (auto* processor : processors)
    {
        configureIdentityPath(*processor);
        configureFilter(*processor);
        setPlainParameter(*processor,
                          parameterID,
                          processor == &hardJumpReference ? targetValue : initialValue);
        processor->prepareToPlay(sampleRate, blockSize);
    }

    const int firstAudibleSample = subject.getLatencySamples();
    REQUIRE(firstAudibleSample > 0);
    REQUIRE(firstAudibleSample < blockSize);
    REQUIRE(heldReference.getLatencySamples() == firstAudibleSample);
    REQUIRE(hardJumpReference.getLatencySamples() == firstAudibleSample);

    juce::MidiBuffer midi;
    int streamPosition = 0;

    for (int block = 0; block < warmupBlocks; ++block)
    {
        const auto input = makeProbeInput(streamPosition, primaryFrequency);
        for (auto* processor : processors)
        {
            auto output = input;
            processor->processBlock(output, midi);
        }
        streamPosition += blockSize;
    }

    setPlainParameter(subject, parameterID, targetValue);

    const auto transitionInput = makeProbeInput(streamPosition, primaryFrequency);
    auto subjectOutput = transitionInput;
    auto heldOutput = transitionInput;
    auto hardOutput = transitionInput;
    subject.processBlock(subjectOutput, midi);
    heldReference.processBlock(heldOutput, midi);
    hardJumpReference.processBlock(hardOutput, midi);
    streamPosition += blockSize;

    AutomationProbe result;
    result.firstAudibleSample = firstAudibleSample;
    // The parameter event occurs at this callback's input sample zero. Base
    // processing now has a fixed integer output delay, so output samples
    // [0, D) still belong to the preceding callback. Compare all three
    // processors at output sample D, where that first automated input sample
    // actually becomes audible.
    const float firstSubject = subjectOutput.getSample(0,
                                                       firstAudibleSample);
    const float firstHeld = heldOutput.getSample(0,
                                                 firstAudibleSample);
    const float firstHard = hardOutput.getSample(0,
                                                 firstAudibleSample);
    result.firstDistanceFromHeld = std::abs(firstSubject - firstHeld);
    result.firstHardJumpDistance = std::abs(firstHard - firstHeld);
    result.firstDistanceFromHardJump = std::abs(firstSubject - firstHard);

    for (int block = 0; block < settlingBlocks; ++block)
    {
        const auto input = makeProbeInput(streamPosition, primaryFrequency);
        subjectOutput = input;
        hardOutput = input;
        subject.processBlock(subjectOutput, midi);
        hardJumpReference.processBlock(hardOutput, midi);
        streamPosition += blockSize;
    }

    for (int channel = 0; channel < subjectOutput.getNumChannels(); ++channel)
        for (int sample = 0; sample < subjectOutput.getNumSamples(); ++sample)
            result.settledMaximumError = std::max(
                result.settledMaximumError,
                std::abs(subjectOutput.getSample(channel, sample)
                         - hardOutput.getSample(channel, sample)));

    return result;
}

void requireSmoothedTransition(const AutomationProbe& result)
{
    INFO("first audible transition sample = " << result.firstAudibleSample);
    INFO("first distance from held response = " << result.firstDistanceFromHeld);
    INFO("hard-jump reference distance = " << result.firstHardJumpDistance);
    INFO("first distance from hard-jump response = " << result.firstDistanceFromHardJump);
    INFO("settled maximum target error = " << result.settledMaximumError);

    // The reference separation makes the ratio assertions meaningful rather
    // than allowing a favourable zero crossing to pass the test accidentally.
    REQUIRE(result.firstHardJumpDistance > 1.0e-4f);

    // A 0.5 ms parameter ramp is 24 samples at 48 kHz. The first output sample
    // must therefore remain much closer to the previous steady response than
    // to a reference filter already settled at the new recipe. This is an
    // observable processor contract and does not prescribe coefficient-ramp,
    // sub-block, or dual-chain crossfade implementation details.
    REQUIRE(result.firstDistanceFromHeld
            < result.firstHardJumpDistance * 0.25f);
    REQUIRE(result.firstDistanceFromHardJump
            > result.firstHardJumpDistance * 0.5f);

    // Smoothing must not leave the filter stranded between recipes.
    REQUIRE(result.settledMaximumError < 2.0e-4f);
}

LfoData makeFilterModulationShape()
{
    LfoData shape;
    shape.points = {
        { 0.0f, 0.13f },
        { 0.19f, 0.92f },
        { 0.47f, 0.31f },
        { 0.71f, 0.84f },
        { 1.0f, 0.13f }
    };
    shape.curvatures = { 0.55f, -0.37f, 0.21f, -0.68f };
    shape.smoothness = 0.0f;
    return shape;
}

void configureFilterModulation(FireAudioProcessor& processor,
                               int maximumBlockSize)
{
    configureIdentityPath(processor);
    setPlainParameter(processor, LOWCUT_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, PEAK_BYPASSED_ID, 0.0f);
    setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, PEAK_FREQ_ID, 1733.0f);
    setPlainParameter(processor, PEAK_Q_ID, 4.7f);
    setPlainParameter(processor, PEAK_GAIN_ID, 0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0),
                      17.37f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_PHASE_ID, 0),
                      0.23f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0),
                      0.0f);

    processor.getLfoManager().setLfoData(0, makeFilterModulationShape());
    processor.assignLfoToTarget(0, PEAK_GAIN_ID);
    processor.setModulationDepth(PEAK_GAIN_ID, 0.88f);
    // assignLfoToTarget defaults to bipolar modulation, which keeps the base
    // gain centred and exercises both boost and cut recipes.
    processor.prepareToPlay(sampleRate, maximumBlockSize);
}

float makeContinuousProbeSample(int streamSample, int channel)
{
    const auto sample = static_cast<double>(streamSample);
    const auto channelPhase = static_cast<double>(channel) * 0.29;
    const auto primary = juce::MathConstants<double>::twoPi * 1733.0
                         * sample / sampleRate;
    const auto secondary = juce::MathConstants<double>::twoPi * 419.0
                           * sample / sampleRate;
    return 0.54f * static_cast<float>(std::sin(primary + 0.17 + channelPhase))
         + 0.16f * static_cast<float>(std::cos(secondary + 0.63 + channelPhase));
}

std::vector<float> renderFilterModulation(int hostBlockSize)
{
    FireAudioProcessor processor;
    configureFilterModulation(processor, hostBlockSize);

    const int totalSamples = modulationWarmupSamples + modulationComparisonSamples;
    std::vector<float> output(static_cast<size_t>(modulationComparisonSamples));
    juce::MidiBuffer midi;

    int streamPosition = 0;
    while (streamPosition < totalSamples)
    {
        const int samplesThisBlock = juce::jmin(hostBlockSize,
                                                totalSamples - streamPosition);
        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 makeContinuousProbeSample(streamPosition + sample,
                                                           channel));

        processor.processBlock(buffer, midi);

        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            if (absoluteSample >= modulationWarmupSamples)
            {
                const auto outputIndex = static_cast<size_t>(
                    absoluteSample - modulationWarmupSamples);
                const float value = buffer.getSample(0, sample);
                REQUIRE(std::isfinite(value));
                output[outputIndex] = value;
            }
        }

        streamPosition += samplesThisBlock;
    }

    return output;
}

float lowSampleRateProbeValue(int streamSample,
                              int channel,
                              double processorSampleRate)
{
    const auto sample = static_cast<double>(streamSample);
    const auto channelPhase = static_cast<double>(channel) * 0.33;
    const auto lowPhase = juce::MathConstants<double>::twoPi * 117.0
                          * sample / processorSampleRate;
    const auto middlePhase = juce::MathConstants<double>::twoPi * 1733.0
                             * sample / processorSampleRate;
    const auto highPhase = juce::MathConstants<double>::twoPi * 0.41
                           * processorSampleRate * sample / processorSampleRate;
    return 0.29f * static_cast<float>(std::sin(lowPhase + 0.15 + channelPhase))
         + 0.31f * static_cast<float>(std::cos(middlePhase + 0.43 + channelPhase))
         + 0.17f * static_cast<float>(std::sin(highPhase + 0.71 + channelPhase));
}

std::vector<float> renderLowSampleRateFilter(double processorSampleRate,
                                             float requestedFrequency,
                                             int comparedTailSamples = 0)
{
    constexpr int lowRateBlockSize = 127;
    constexpr int lowRateBlockCount = 160;
    FireAudioProcessor processor;
    configureIdentityPath(processor);
    setPlainParameter(processor, LOWCUT_BYPASSED_ID, 0.0f);
    setPlainParameter(processor, PEAK_BYPASSED_ID, 0.0f);
    setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 0.0f);
    setPlainParameter(processor, LOWCUT_FREQ_ID, requestedFrequency);
    setPlainParameter(processor, PEAK_FREQ_ID, requestedFrequency);
    setPlainParameter(processor, HIGHCUT_FREQ_ID, requestedFrequency);
    setPlainParameter(processor, LOWCUT_GAIN_ID, 9.0f);
    setPlainParameter(processor, PEAK_GAIN_ID, 15.0f);
    setPlainParameter(processor, HIGHCUT_GAIN_ID, -9.0f);
    setPlainParameter(processor, LOWCUT_Q_ID, 3.7f);
    setPlainParameter(processor, PEAK_Q_ID, 4.8f);
    setPlainParameter(processor, HIGHCUT_Q_ID, 4.2f);
    setPlainParameter(processor, LOWCUT_SLOPE_ID, 3.0f);
    setPlainParameter(processor, HIGHCUT_SLOPE_ID, 3.0f);
    processor.prepareToPlay(processorSampleRate, lowRateBlockSize);

    std::vector<float> output;
    output.reserve(static_cast<size_t>(lowRateBlockSize * lowRateBlockCount));
    juce::MidiBuffer midi;
    int streamPosition = 0;

    for (int block = 0; block < lowRateBlockCount; ++block)
    {
        juce::AudioBuffer<float> buffer(2, lowRateBlockSize);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < lowRateBlockSize; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 lowSampleRateProbeValue(streamPosition + sample,
                                                         channel,
                                                         processorSampleRate));

        processor.processBlock(buffer, midi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                REQUIRE(std::isfinite(buffer.getSample(channel, sample)));

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            output.push_back(buffer.getSample(0, sample));

        streamPosition += lowRateBlockSize;
    }

    if (comparedTailSamples > 0
        && comparedTailSamples < static_cast<int>(output.size()))
    {
        return { output.end() - comparedTailSamples, output.end() };
    }

    return output;
}
} // namespace

TEST_CASE("Global cutoff automation does not replace coefficients at a block boundary",
          "[processor][filter][smoothing]")
{
    const auto result = runAutomationProbe(
        LOWCUT_FREQ_ID,
        120.0f,
        8000.0f,
        1300.0f,
        [] (FireAudioProcessor& processor)
        {
            setPlainParameter(processor, LOWCUT_BYPASSED_ID, 0.0f);
            setPlainParameter(processor, PEAK_BYPASSED_ID, 1.0f);
            setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 1.0f);
            setPlainParameter(processor, LOWCUT_GAIN_ID, 0.0f);
            setPlainParameter(processor, LOWCUT_Q_ID, 1.0f);
            setPlainParameter(processor, LOWCUT_SLOPE_ID, 0.0f);
        });

    requireSmoothedTransition(result);
}

TEST_CASE("Global peak gain automation does not replace coefficients at a block boundary",
          "[processor][filter][smoothing]")
{
    const auto result = runAutomationProbe(
        PEAK_GAIN_ID,
        -18.0f,
        18.0f,
        1237.0f,
        [] (FireAudioProcessor& processor)
        {
            setPlainParameter(processor, LOWCUT_BYPASSED_ID, 1.0f);
            setPlainParameter(processor, PEAK_BYPASSED_ID, 0.0f);
            setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 1.0f);
            setPlainParameter(processor, PEAK_FREQ_ID, 1237.0f);
            setPlainParameter(processor, PEAK_Q_ID, 5.0f);
        });

    requireSmoothedTransition(result);
}

TEST_CASE("Global filter LFO modulation is independent of host block size",
          "[processor][filter][lfo][sample-accurate]")
{
    const auto smallBlocks = renderFilterModulation(32);
    const auto irregularBlocks = renderFilterModulation(257);
    REQUIRE(smallBlocks.size() == irregularBlocks.size());

    float maximumError = 0.0f;
    double squaredError = 0.0;
    for (size_t sample = 0; sample < smallBlocks.size(); ++sample)
    {
        REQUIRE(std::isfinite(smallBlocks[sample]));
        REQUIRE(std::isfinite(irregularBlocks[sample]));
        const float error = std::abs(smallBlocks[sample]
                                     - irregularBlocks[sample]);
        maximumError = std::max(maximumError, error);
        squaredError += static_cast<double>(error) * error;
    }

    const float rmsError = static_cast<float>(
        std::sqrt(squaredError / static_cast<double>(smallBlocks.size())));
    INFO("32-vs-257 maximum sample error = " << maximumError);
    INFO("32-vs-257 RMS sample error = " << rmsError);

    // A sample-accurate modulation path receives the identical LFO value at
    // every absolute sample, regardless of callback boundaries. Allow a small
    // cross-platform margin for recursive IIR arithmetic while rejecting a
    // block-held coefficient recipe.
    REQUIRE(maximumError < 2.0e-4f);
    REQUIRE(rmsError < 2.0e-5f);
}

TEST_CASE("Global filter initialisation clamps restored frequencies below Nyquist",
          "[processor][filter][low-sample-rate][robustness]")
{
    for (const double processorSampleRate : { 22050.0, 32000.0 })
    {
        DYNAMIC_SECTION("sample rate " << processorSampleRate)
        {
            const float nyquist = static_cast<float>(processorSampleRate * 0.5);
            // The public parameter interval is 1 Hz, so this is the closest
            // user/host-settable reference that remains below Nyquist.
            const float safeFrequency = std::ceil(nyquist) - 1.0f;
            const auto restoredOutOfRange = renderLowSampleRateFilter(
                processorSampleRate, 20000.0f, 4096);
            const auto explicitSafe = renderLowSampleRateFilter(
                processorSampleRate, safeFrequency, 4096);
            REQUIRE(restoredOutOfRange.size() == explicitSafe.size());

            float maximumError = 0.0f;
            for (size_t sample = 0; sample < restoredOutOfRange.size(); ++sample)
            {
                REQUIRE(std::isfinite(restoredOutOfRange[sample]));
                REQUIRE(std::isfinite(explicitSafe[sample]));
                maximumError = std::max(
                    maximumError,
                    std::abs(restoredOutOfRange[sample] - explicitSafe[sample]));
            }

            INFO("out-of-range versus nearest host-settable safe max error = "
                 << maximumError);
            // The explicit reference is the closest 1 Hz-stepped host value
            // below Nyquist. Near-Nyquist high-order sections are extremely
            // sensitive to that final fractional Hz, so the reference is a
            // bounded sanity comparison rather than a bit-equivalence check.
            // A missing clamp produces invalid coefficients/non-finite output;
            // the assertions above are the primary safety contract.
            REQUIRE(maximumError < 0.35f);
        }
    }
}
