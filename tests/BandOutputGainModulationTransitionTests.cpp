#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int transitionSamples = 480;
constexpr int warmupSamples = 4096;
constexpr int endpointWindowSamples = 64;
constexpr int capturedSamples = transitionSamples + endpointWindowSamples;
constexpr int baseAutomationSamples = 1536;
constexpr float transitionTolerance = 2.0e-4f;
constexpr std::array<float, 2> inputValues { 0.63f, 0.47f };

const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

enum class RecipeChange
{
    attach,
    activeRecipe
};

const char* changeName(RecipeChange change)
{
    return change == RecipeChange::attach ? "attach" : "active recipe";
}

BandProcessingParameters makeTransparentParameters(bool useHq)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 8;
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isDriveEnabled = false;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.shapeMixVal = 0.0f;
    params.shapeMixValProvider.baseValue = 0.0f;
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    return params;
}

std::pair<BandProcessingParameters, BandProcessingParameters>
makeTransitionParameters(RecipeChange change, bool useHq)
{
    auto oldParams = makeTransparentParameters(useHq);
    auto newParams = makeTransparentParameters(useHq);

    if (change == RecipeChange::attach)
    {
        oldParams.outputVal.baseValue = -12.0f;
        newParams.outputVal.baseValue = -12.0f;
        newParams.outputVal.modulationDepth = 0.6f;
        newParams.outputVal.isBipolar = true;
        newParams.outputLfoSourceIndex = 0;
    }
    else
    {
        oldParams.outputVal.baseValue = -12.0f;
        oldParams.outputVal.modulationDepth = 0.4f;
        oldParams.outputVal.isBipolar = true;
        oldParams.outputLfoSourceIndex = 0;

        // Change every audible part of the routed recipe at once. Source 1 is
        // deliberately a different constant from source 0, so a transition
        // keyed to the buffer pointer or only the base value cannot pass.
        newParams.outputVal.baseValue = -24.0f;
        newParams.outputVal.modulationDepth = 0.3f;
        newParams.outputVal.isBipolar = false;
        newParams.outputLfoSourceIndex = 1;
    }

    return { oldParams, newParams };
}

juce::AudioBuffer<float> makeInput(int numSamples)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel),
                                          inputValues[static_cast<size_t>(channel)],
                                          numSamples);
    return buffer;
}

void fillConstantLfos(juce::AudioBuffer<float>& lfoOutputs)
{
    REQUIRE(lfoOutputs.getNumChannels() >= 2);
    juce::FloatVectorOperations::fill(lfoOutputs.getWritePointer(0),
                                      0.25f,
                                      lfoOutputs.getNumSamples());
    juce::FloatVectorOperations::fill(lfoOutputs.getWritePointer(1),
                                      0.75f,
                                      lfoOutputs.getNumSamples());
}

struct TransitionRender
{
    std::array<std::vector<float>, 2> subject;
    std::array<std::vector<float>, 2> oldReference;
    std::array<std::vector<float>, 2> newReference;
    float preEventError = 0.0f;
    bool finite = true;
};

TransitionRender renderTransition(RecipeChange change,
                                  bool useHq,
                                  int preparedBlockSize,
                                  const std::vector<int>& hostBlockPattern)
{
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    BandProcessor subject;
    BandProcessor alwaysOld;
    BandProcessor alwaysNew;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        2
    };
    subject.prepare(spec);
    alwaysOld.prepare(spec);
    alwaysNew.prepare(spec);
    const auto [oldParams, newParams] = makeTransitionParameters(change, useHq);

    TransitionRender result;
    for (auto* output : { &result.subject,
                          &result.oldReference,
                          &result.newReference })
        for (auto& channel : *output)
            channel.reserve(capturedSamples);

    constexpr int totalSamples = warmupSamples + capturedSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int samplesThisCallback = hostBlockPattern[
            callbackIndex % hostBlockPattern.size()];
        ++callbackIndex;
        samplesThisCallback = std::min(samplesThisCallback,
                                       totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            samplesThisCallback = std::min(samplesThisCallback,
                                           warmupSamples - streamPosition);

        const auto input = makeInput(samplesThisCallback);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> oldBuffer;
        juce::AudioBuffer<float> newBuffer;
        subjectBuffer.makeCopyOf(input);
        oldBuffer.makeCopyOf(input);
        newBuffer.makeCopyOf(input);
        juce::AudioBuffer<float> lfoOutputs(2, samplesThisCallback);
        fillConstantLfos(lfoOutputs);

        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? oldParams : newParams,
                        lfoOutputs);
        alwaysOld.process(oldBuffer, oldParams, lfoOutputs);
        alwaysNew.process(newBuffer, newParams, lfoOutputs);

        for (int sample = 0; sample < samplesThisCallback; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float oldValue = oldBuffer.getSample(channel, sample);
                const float newValue = newBuffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(oldValue)
                                && std::isfinite(newValue);
                if (absoluteSample < warmupSamples)
                {
                    result.preEventError = std::max(
                        result.preEventError,
                        std::abs(actual - oldValue));
                    continue;
                }

                const auto index = static_cast<size_t>(channel);
                result.subject[index].push_back(actual);
                result.oldReference[index].push_back(oldValue);
                result.newReference[index].push_back(newValue);
            }
        }
        streamPosition += samplesThisCallback;
    }

    return result;
}

float maximumDifference(const TransitionRender& first,
                        const TransitionRender& second)
{
    float error = 0.0f;
    for (size_t channel = 0; channel < first.subject.size(); ++channel)
    {
        REQUIRE(first.subject[channel].size() == second.subject[channel].size());
        for (size_t sample = 0; sample < first.subject[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(first.subject[channel][sample]
                                      - second.subject[channel][sample]));
    }
    return error;
}

void checkTransition(const TransitionRender& result,
                     RecipeChange change,
                     bool useHq)
{
    REQUIRE(result.finite);
    CHECK(result.preEventError <= 1.0e-6f);

    float minimumSeparation = std::numeric_limits<float>::max();
    float maximumSeparation = 0.0f;
    float firstOldError = 0.0f;
    float midpointOldDistance = std::numeric_limits<float>::max();
    float midpointNewDistance = std::numeric_limits<float>::max();
    float settledError = 0.0f;
    float maximumStep = 0.0f;
    constexpr int midpoint = transitionSamples / 2;
    for (size_t channel = 0; channel < result.subject.size(); ++channel)
    {
        const auto& subject = result.subject[channel];
        const auto& oldReference = result.oldReference[channel];
        const auto& newReference = result.newReference[channel];
        REQUIRE(subject.size() == capturedSamples);
        const float separation = std::abs(oldReference[0] - newReference[0]);
        minimumSeparation = std::min(minimumSeparation, separation);
        maximumSeparation = std::max(maximumSeparation, separation);
        firstOldError = std::max(firstOldError,
                                 std::abs(subject[0] - oldReference[0]));
        midpointOldDistance = std::min(
            midpointOldDistance,
            std::abs(subject[midpoint] - oldReference[midpoint]));
        midpointNewDistance = std::min(
            midpointNewDistance,
            std::abs(subject[midpoint] - newReference[midpoint]));
        for (int sample = 1; sample < transitionSamples; ++sample)
            maximumStep = std::max(
                maximumStep,
                std::abs(subject[static_cast<size_t>(sample)]
                         - subject[static_cast<size_t>(sample - 1)]));
        for (int sample = transitionSamples; sample < capturedSamples; ++sample)
            settledError = std::max(
                settledError,
                std::abs(subject[static_cast<size_t>(sample)]
                         - newReference[static_cast<size_t>(sample)]));
    }

    CAPTURE(changeName(change),
            useHq,
            minimumSeparation,
            firstOldError,
            midpointOldDistance,
            midpointNewDistance,
            maximumStep,
            settledError);
    REQUIRE(minimumSeparation >= 0.05f);
    CHECK(firstOldError <= transitionTolerance);
    CHECK(midpointOldDistance >= minimumSeparation * 0.05f);
    CHECK(midpointNewDistance >= minimumSeparation * 0.05f);
    CHECK(maximumStep <= maximumSeparation * 0.05f);
    CHECK(settledError <= transitionTolerance);
}

float triangleSample(int absoluteSample)
{
    constexpr int cycleSamples = 480;
    const float phase = static_cast<float>(absoluteSample % cycleSamples)
                        / static_cast<float>(cycleSamples);
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

struct DynamicRender
{
    std::array<std::vector<float>, 2> output;
    std::array<std::vector<float>, 2> expected;
    bool finite = true;
};

DynamicRender renderDynamicLfo(bool useHq,
                              int preparedBlockSize,
                              const std::vector<int>& hostBlockPattern)
{
    BandProcessor subject;
    BandProcessor carrier;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        2
    };
    subject.prepare(spec);
    carrier.prepare(spec);

    auto routedParams = makeTransparentParameters(useHq);
    routedParams.outputVal.baseValue = -21.0f;
    routedParams.outputVal.modulationDepth = 0.6f;
    routedParams.outputVal.isBipolar = true;
    routedParams.outputLfoSourceIndex = 0;
    auto carrierParams = makeTransparentParameters(useHq);
    carrierParams.outputVal.baseValue = 0.0f;

    DynamicRender result;
    for (auto* output : { &result.output, &result.expected })
        for (auto& channel : *output)
            channel.reserve(capturedSamples);

    constexpr int totalSamples = warmupSamples + capturedSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int samplesThisCallback = hostBlockPattern[
            callbackIndex % hostBlockPattern.size()];
        ++callbackIndex;
        samplesThisCallback = std::min(samplesThisCallback,
                                       totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            samplesThisCallback = std::min(samplesThisCallback,
                                           warmupSamples - streamPosition);

        auto subjectBuffer = makeInput(samplesThisCallback);
        auto carrierBuffer = makeInput(samplesThisCallback);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisCallback);
        for (int sample = 0; sample < samplesThisCallback; ++sample)
            lfoOutputs.setSample(0,
                                 sample,
                                 triangleSample(streamPosition + sample));

        subject.process(subjectBuffer, routedParams, lfoOutputs);
        carrier.process(carrierBuffer, carrierParams, lfoOutputs);
        auto provider = routedParams.outputVal;
        provider.lfoSignal = lfoOutputs.getReadPointer(0);
        for (int sample = 0; sample < samplesThisCallback; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            if (absoluteSample < warmupSamples)
                continue;

            const float linearGain = juce::Decibels::decibelsToGain(
                provider.get(sample));
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float expected = carrierBuffer.getSample(channel, sample)
                                     * linearGain;
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(expected);
                const auto index = static_cast<size_t>(channel);
                result.output[index].push_back(actual);
                result.expected[index].push_back(expected);
            }
        }
        streamPosition += samplesThisCallback;
    }
    return result;
}

float dynamicReferenceError(const DynamicRender& result)
{
    float error = 0.0f;
    for (size_t channel = 0; channel < result.output.size(); ++channel)
    {
        REQUIRE(result.output[channel].size() == result.expected[channel].size());
        for (size_t sample = 0; sample < result.output[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(result.output[channel][sample]
                                      - result.expected[channel][sample]));
    }
    return error;
}

struct BaseAutomationRender
{
    float maximumReferenceError = 0.0f;
    float expectedGainRange = 0.0f;
    bool finite = true;
};

float automatedBaseTarget(int eventSample, int callbackSamples)
{
    const float progress = juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(eventSample + callbackSamples)
            / static_cast<float>(baseAutomationSamples));
    return -24.0f + progress * 18.0f;
}

BaseAutomationRender renderRoutedBaseAutomation(
    bool useHq,
    const std::vector<int>& hostBlockPattern)
{
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    BandProcessor subject;
    BandProcessor carrier;
    const auto preparedBlockSize = *std::max_element(hostBlockPattern.begin(),
                                                      hostBlockPattern.end());
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        2
    };
    subject.prepare(spec);
    carrier.prepare(spec);

    auto routedParams = makeTransparentParameters(useHq);
    routedParams.outputVal.baseValue = -24.0f;
    routedParams.outputVal.modulationDepth = 0.6f;
    routedParams.outputVal.isBipolar = true;
    routedParams.outputLfoSourceIndex = 0;
    auto carrierParams = makeTransparentParameters(useHq);
    carrierParams.outputVal.baseValue = 0.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        expectedBaseGain;
    expectedBaseGain.reset(sampleRate, 0.05);
    expectedBaseGain.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(-24.0f));

    BaseAutomationRender result;
    float minimumExpectedGain = std::numeric_limits<float>::max();
    float maximumExpectedGain = 0.0f;
    constexpr int totalSamples = warmupSamples + baseAutomationSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int samplesThisCallback = hostBlockPattern[
            callbackIndex++ % hostBlockPattern.size()];
        samplesThisCallback = std::min(samplesThisCallback,
                                       totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            samplesThisCallback = std::min(samplesThisCallback,
                                           warmupSamples - streamPosition);

        const bool automating = streamPosition >= warmupSamples;
        if (automating)
        {
            const int eventSample = streamPosition - warmupSamples;
            routedParams.outputVal.baseValue = automatedBaseTarget(
                eventSample,
                samplesThisCallback);
            expectedBaseGain.setTargetValue(
                juce::Decibels::decibelsToGain(
                    routedParams.outputVal.baseValue));
        }

        auto subjectBuffer = makeInput(samplesThisCallback);
        auto carrierBuffer = makeInput(samplesThisCallback);
        juce::AudioBuffer<float> lfoOutputs(2, samplesThisCallback);
        fillConstantLfos(lfoOutputs);
        subject.process(subjectBuffer, routedParams, lfoOutputs);
        carrier.process(carrierBuffer, carrierParams, lfoOutputs);

        if (automating)
        {
            auto provider = routedParams.outputVal;
            provider.lfoSignal = lfoOutputs.getReadPointer(0);
            for (int sample = 0; sample < samplesThisCallback; ++sample)
            {
                // Output's legacy automation ramp is linear in gain. A stable
                // route must use that smoothed base as an override while the
                // LFO itself remains a direct, per-sample dB modulation.
                const float smoothedBaseDb = juce::Decibels::gainToDecibels(
                    expectedBaseGain.getNextValue());
                const float expectedGain = juce::Decibels::decibelsToGain(
                    provider.get(sample, smoothedBaseDb));
                minimumExpectedGain = std::min(minimumExpectedGain,
                                               expectedGain);
                maximumExpectedGain = std::max(maximumExpectedGain,
                                               expectedGain);

                for (int channel = 0; channel < 2; ++channel)
                {
                    const float actual = subjectBuffer.getSample(channel,
                                                                  sample);
                    const float expected = carrierBuffer.getSample(channel,
                                                                    sample)
                                         * expectedGain;
                    result.finite = result.finite && std::isfinite(actual)
                                    && std::isfinite(expected);
                    result.maximumReferenceError = std::max(
                        result.maximumReferenceError,
                        std::abs(actual - expected));
                }
            }
        }

        streamPosition += samplesThisCallback;
    }

    result.expectedGainRange = maximumExpectedGain - minimumExpectedGain;
    return result;
}

BandProcessingParameters makeRoutedRecipe(float baseDb,
                                          float depth,
                                          bool bipolar,
                                          int sourceIndex,
                                          bool enabled = true)
{
    auto params = makeTransparentParameters(false);
    params.isBandEnabled = enabled;
    params.outputVal.baseValue = baseDb;
    params.outputVal.modulationDepth = depth;
    params.outputVal.isBipolar = bipolar;
    params.outputLfoSourceIndex = sourceIndex;
    return params;
}

juce::AudioBuffer<float> processConstant(BandProcessor& band,
                                         const BandProcessingParameters& params,
                                         int numSamples)
{
    auto buffer = makeInput(numSamples);
    juce::AudioBuffer<float> lfoOutputs(2, numSamples);
    fillConstantLfos(lfoOutputs);
    band.process(buffer, params, lfoOutputs);
    return buffer;
}

float recipeLinearGain(const BandProcessingParameters& params)
{
    auto provider = params.outputVal;
    constexpr std::array<float, 2> values { 0.25f, 0.75f };
    REQUIRE(juce::isPositiveAndBelow(params.outputLfoSourceIndex,
                                     static_cast<int>(values.size())));
    provider.lfoSignal = &values[static_cast<size_t>(params.outputLfoSourceIndex)];
    return juce::Decibels::decibelsToGain(provider.get(0));
}

float maximumBufferDifference(const juce::AudioBuffer<float>& first,
                              const juce::AudioBuffer<float>& second,
                              int startSample = 0)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());
    float error = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = startSample; sample < first.getNumSamples(); ++sample)
            error = std::max(error,
                             std::abs(first.getSample(channel, sample)
                                      - second.getSample(channel, sample)));
    return error;
}

bool bufferIsFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite(buffer.getSample(channel, sample)))
                return false;

    return true;
}
} // namespace

TEST_CASE("Band Output LFO recipe changes bridge for ten milliseconds",
          "[band][output-gain][lfo][transition]")
{
    for (const auto change : { RecipeChange::attach,
                               RecipeChange::activeRecipe })
        for (const bool useHq : { false, true })
        {
            DYNAMIC_SECTION(changeName(change) << ", HQ=" << useHq)
            {
                const auto result = renderTransition(change,
                                                     useHq,
                                                     257,
                                                     fixedCallbacks);
                checkTransition(result, change, useHq);
            }
        }
}

TEST_CASE("Band Output LFO transitions ignore host and internal partitions",
          "[band][output-gain][lfo][transition][block-size][internal-chunk]")
{
    for (const bool useHq : { false, true })
    {
        const auto fixed = renderTransition(RecipeChange::activeRecipe,
                                            useHq,
                                            257,
                                            fixedCallbacks);
        const auto irregular = renderTransition(RecipeChange::activeRecipe,
                                                useHq,
                                                257,
                                                irregularCallbacks);
        const auto internallyChunked = renderTransition(
            RecipeChange::activeRecipe,
            useHq,
            64,
            fixedCallbacks);
        const float partitionError = maximumDifference(fixed, irregular);
        const float chunkError = maximumDifference(fixed, internallyChunked);
        CAPTURE(useHq, partitionError, chunkError);
        CHECK(partitionError <= transitionTolerance);
        CHECK(chunkError <= transitionTolerance);
    }
}

TEST_CASE("Stable Band Output LFO remains sample accurate",
          "[band][output-gain][lfo][transition][dynamic][block-size]")
{
    for (const bool useHq : { false, true })
    {
        const auto fixed = renderDynamicLfo(useHq, 257, fixedCallbacks);
        const auto irregular = renderDynamicLfo(useHq, 257, irregularCallbacks);
        const auto internallyChunked = renderDynamicLfo(useHq,
                                                        64,
                                                        fixedCallbacks);
        REQUIRE(fixed.finite);
        REQUIRE(irregular.finite);
        REQUIRE(internallyChunked.finite);
        const float fixedReferenceError = dynamicReferenceError(fixed);
        const float irregularReferenceError = dynamicReferenceError(irregular);
        const float chunkReferenceError = dynamicReferenceError(internallyChunked);

        TransitionRender fixedAsTransition;
        TransitionRender irregularAsTransition;
        TransitionRender chunkedAsTransition;
        fixedAsTransition.subject = fixed.output;
        irregularAsTransition.subject = irregular.output;
        chunkedAsTransition.subject = internallyChunked.output;
        const float partitionError = maximumDifference(fixedAsTransition,
                                                       irregularAsTransition);
        const float chunkError = maximumDifference(fixedAsTransition,
                                                   chunkedAsTransition);
        CAPTURE(useHq,
                fixedReferenceError,
                irregularReferenceError,
                chunkReferenceError,
                partitionError,
                chunkError);
        CHECK(fixedReferenceError <= 2.0e-5f);
        CHECK(irregularReferenceError <= 2.0e-5f);
        CHECK(chunkReferenceError <= 2.0e-5f);
        CHECK(partitionError <= transitionTolerance);
        CHECK(chunkError <= transitionTolerance);
    }
}

TEST_CASE("Band Output continuous routed-base automation does not restart the route bridge",
          "[band][output-gain][lfo][base-automation][block-size]")
{
    const std::array<std::vector<int>, 3> callbackPatterns {
        fixedCallbacks,
        std::vector<int> { 1 },
        irregularCallbacks
    };

    for (const bool useHq : { false, true })
        for (const auto& pattern : callbackPatterns)
        {
            const auto result = renderRoutedBaseAutomation(useHq, pattern);
            CAPTURE(useHq,
                    pattern.front(),
                    pattern.size(),
                    result.maximumReferenceError,
                    result.expectedGainRange);
            REQUIRE(result.finite);
            REQUIRE(result.expectedGainRange >= 0.015f);
            CHECK(result.maximumReferenceError <= 2.0e-5f);
        }
}

TEST_CASE("Band Output Linked toggles bridge only when the effective routed base changes",
          "[band][output-gain][lfo][transition][linked]")
{
    BandProcessor subject;
    subject.prepare({ sampleRate, 257, 2 });

    auto unlinked = makeRoutedRecipe(-18.0f, 0.4f, true, 0);
    auto linked = unlinked;
    linked.isOutputLinked = true;
    linked.outputVal.baseValue = -6.0f; // Linked Drive=60 -> -0.1 * 60 dB.

    processConstant(subject, unlinked, warmupSamples);
    const auto transition = processConstant(subject,
                                            linked,
                                            transitionSamples
                                                + endpointWindowSamples);
    REQUIRE(bufferIsFinite(transition));

    const float oldGain = recipeLinearGain(unlinked);
    const float newGain = recipeLinearGain(linked);
    REQUIRE(std::abs(newGain - oldGain) >= 0.05f);
    for (const int sample : { 0, transitionSamples / 2, transitionSamples })
    {
        const float mix = static_cast<float>(sample)
                          / static_cast<float>(transitionSamples);
        const float expectedGain = oldGain + mix * (newGain - oldGain);
        for (int channel = 0; channel < 2; ++channel)
        {
            CAPTURE(sample, channel, oldGain, newGain, expectedGain);
            CHECK(transition.getSample(channel, sample)
                  == Catch::Approx(inputValues[static_cast<size_t>(channel)]
                                   * expectedGain)
                         .margin(transitionTolerance));
        }
    }

    // If Output already equals the Linked effective base, toggling the mode is
    // audibly a no-op and must not freeze a moving LFO behind a needless bridge.
    auto sameEffectiveBase = linked;
    sameEffectiveBase.isOutputLinked = false;
    const auto noOp = processConstant(subject,
                                      sameEffectiveBase,
                                      endpointWindowSamples);
    REQUIRE(bufferIsFinite(noOp));
    CHECK_FALSE(subject.outputGainTransition.routeTransitionMix.isSmoothing());
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < noOp.getNumSamples(); ++sample)
            CHECK(noOp.getSample(channel, sample)
                  == Catch::Approx(inputValues[static_cast<size_t>(channel)]
                                   * newGain)
                         .margin(transitionTolerance));

    // APVTS range reconstruction and Linked arithmetic can produce values
    // that differ by a few ULPs while representing the same user-visible base.
    // Treat them as one parameter value so this toggle cannot unnecessarily
    // hold a moving LFO behind a 10 ms bridge.
    constexpr float apvtsEquivalentBase = -48.0f + 0.1f * 386.0f;
    constexpr float linkedEquivalentBase = -0.1f * 94.0f;
    const float apvtsEquivalentGain = juce::Decibels::decibelsToGain(
        apvtsEquivalentBase);
    const float linkedEquivalentGain = juce::Decibels::decibelsToGain(
        linkedEquivalentBase);
    REQUIRE_FALSE(juce::exactlyEqual(apvtsEquivalentGain,
                                     linkedEquivalentGain));
    REQUIRE(std::abs(apvtsEquivalentBase - linkedEquivalentBase) <= 1.0e-5f);

    BandProcessor ulpSubject;
    ulpSubject.prepare({ sampleRate, 257, 2 });
    auto linkedEquivalent = makeRoutedRecipe(linkedEquivalentBase,
                                             0.4f,
                                             true,
                                             0);
    linkedEquivalent.isOutputLinked = true;
    processConstant(ulpSubject, linkedEquivalent, warmupSamples);
    auto unlinkedEquivalent = linkedEquivalent;
    unlinkedEquivalent.isOutputLinked = false;
    unlinkedEquivalent.outputVal.baseValue = apvtsEquivalentBase;

    constexpr int movingSamples = 257;
    auto movingOutput = makeInput(movingSamples);
    juce::AudioBuffer<float> movingLfos(2, movingSamples);
    for (int sample = 0; sample < movingSamples; ++sample)
    {
        movingLfos.setSample(0, sample, triangleSample(sample));
        movingLfos.setSample(1, sample, 0.75f);
    }
    ulpSubject.process(movingOutput, unlinkedEquivalent, movingLfos);
    REQUIRE(bufferIsFinite(movingOutput));
    CHECK_FALSE(ulpSubject.outputGainTransition.routeTransitionMix.isSmoothing());

    auto expectedProvider = unlinkedEquivalent.outputVal;
    expectedProvider.lfoSignal = movingLfos.getReadPointer(0);
    float movingReferenceError = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < movingSamples; ++sample)
        {
            const float expectedGain = juce::Decibels::decibelsToGain(
                expectedProvider.get(sample));
            movingReferenceError = std::max(
                movingReferenceError,
                std::abs(movingOutput.getSample(channel, sample)
                         - inputValues[static_cast<size_t>(channel)]
                               * expectedGain));
        }
    CAPTURE(apvtsEquivalentBase,
            linkedEquivalentBase,
            apvtsEquivalentGain,
            linkedEquivalentGain,
            movingReferenceError);
    CHECK(movingReferenceError <= 2.0e-5f);
}

TEST_CASE("Attaching Band Output LFO anchors to an in-flight legacy gain ramp",
          "[band][output-gain][lfo][transition][legacy-ramp]")
{
    BandProcessor band;
    band.prepare({ sampleRate, 257, 2 });
    auto initial = makeTransparentParameters(false);
    initial.outputVal.baseValue = -24.0f;
    auto legacyTarget = initial;
    legacyTarget.outputVal.baseValue = 0.0f;
    const auto attached = makeRoutedRecipe(-12.0f, 0.6f, true, 0);

    processConstant(band, initial, warmupSamples);
    const auto partialRamp = processConstant(band, legacyTarget, 160);
    const auto event = processConstant(band,
                                       attached,
                                       transitionSamples + endpointWindowSamples);
    REQUIRE(bufferIsFinite(partialRamp));
    REQUIRE(bufferIsFinite(event));
    float firstError = 0.0f;
    float settledError = 0.0f;
    const float targetGain = recipeLinearGain(attached);
    for (int channel = 0; channel < 2; ++channel)
    {
        firstError = std::max(
            firstError,
            std::abs(event.getSample(channel, 0)
                     - partialRamp.getSample(channel,
                                             partialRamp.getNumSamples() - 1)));
        for (int sample = transitionSamples; sample < event.getNumSamples(); ++sample)
            settledError = std::max(
                settledError,
                std::abs(event.getSample(channel, sample)
                         - inputValues[static_cast<size_t>(channel)] * targetGain));
    }
    CAPTURE(firstError, settledError, targetGain);
    CHECK(firstError <= 1.0e-6f);
    CHECK(settledError <= transitionTolerance);
}

TEST_CASE("Rapid Band Output recipe retargeting is continuous and latest wins",
          "[band][output-gain][lfo][transition][rapid][latest]")
{
    const auto recipeA = makeRoutedRecipe(-12.0f, 0.4f, true, 0);
    const auto recipeB = makeRoutedRecipe(-24.0f, 0.3f, false, 1);
    // This is neither endpoint: it reverses the source while changing the
    // base, depth and polarity again, exercising the queued-third path.
    const auto recipeC = makeRoutedRecipe(-8.0f, -0.25f, true, 0);
    BandProcessor band;
    band.prepare({ sampleRate, 257, 2 });
    processConstant(band, recipeA, warmupSamples);
    const auto firstTransition = processConstant(band, recipeB, 160);
    const auto latest = processConstant(band,
                                        recipeC,
                                        transitionSamples + endpointWindowSamples);
    REQUIRE(bufferIsFinite(firstTransition));
    REQUIRE(bufferIsFinite(latest));

    float retargetError = 0.0f;
    float settledError = 0.0f;
    const float targetGain = recipeLinearGain(recipeC);
    for (int channel = 0; channel < 2; ++channel)
    {
        retargetError = std::max(
            retargetError,
            std::abs(latest.getSample(channel, 0)
                     - firstTransition.getSample(channel,
                                                 firstTransition.getNumSamples() - 1)));
        for (int sample = transitionSamples; sample < latest.getNumSamples(); ++sample)
            settledError = std::max(
                settledError,
                std::abs(latest.getSample(channel, sample)
                         - inputValues[static_cast<size_t>(channel)] * targetGain));
    }
    CAPTURE(retargetError, settledError, targetGain);
    CHECK(retargetError <= 1.0e-6f);
    CHECK(settledError <= transitionTolerance);
}

TEST_CASE("Reset discards a partial Band Output recipe transition",
          "[band][output-gain][lfo][transition][reset]")
{
    const auto oldRecipe = makeRoutedRecipe(-12.0f, 0.4f, true, 0);
    const auto targetRecipe = makeRoutedRecipe(-24.0f, 0.3f, false, 1);
    BandProcessor subject;
    BandProcessor sameResetReference;
    const juce::dsp::ProcessSpec spec { sampleRate, 257, 2 };
    subject.prepare(spec);
    sameResetReference.prepare(spec);
    processConstant(subject, oldRecipe, warmupSamples);
    processConstant(subject, targetRecipe, 160);
    subject.reset();
    sameResetReference.reset();

    const auto afterReset = processConstant(subject, targetRecipe, 64);
    const auto canonical = processConstant(sameResetReference, targetRecipe, 64);
    REQUIRE(bufferIsFinite(afterReset));
    REQUIRE(bufferIsFinite(canonical));
    const float resetError = maximumBufferDifference(afterReset, canonical);
    CAPTURE(resetError);
    CHECK(resetError <= 1.0e-6f);
}

TEST_CASE("Band Output recipe transition advances while the band is disabled",
          "[band][output-gain][lfo][transition][hidden][band-enable]")
{
    const auto oldRecipe = makeRoutedRecipe(-12.0f, 0.4f, true, 0);
    const auto newRecipe = makeRoutedRecipe(-24.0f, 0.3f, false, 1);
    auto hiddenRecipe = newRecipe;
    hiddenRecipe.isBandEnabled = false;
    BandProcessor subject;
    subject.prepare({ sampleRate, 257, 2 });
    processConstant(subject, oldRecipe, warmupSamples);
    processConstant(subject,
                    hiddenRecipe,
                    transitionSamples + endpointWindowSamples);

    const float targetGain = recipeLinearGain(newRecipe);
    const float hiddenStateError = std::abs(
        subject.outputGainTransition.lastAppliedLinearGain - targetGain);
    CHECK_FALSE(subject.outputGainTransition.routeTransitionMix.isSmoothing());

    const auto reenabled = processConstant(
        subject,
        newRecipe,
        transitionSamples + endpointWindowSamples);
    REQUIRE(bufferIsFinite(reenabled));
    float endpointError = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = transitionSamples; sample < reenabled.getNumSamples(); ++sample)
            endpointError = std::max(
                endpointError,
                std::abs(reenabled.getSample(channel, sample)
                         - inputValues[static_cast<size_t>(channel)] * targetGain));
    CAPTURE(hiddenStateError, endpointError, targetGain);
    CHECK(hiddenStateError <= 1.0e-6f);
    CHECK(endpointError <= transitionTolerance);
}
