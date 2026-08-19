#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int transitionSamples = 480; // 10 ms at the base sample rate.
constexpr int warmupSamples = 4096;
constexpr int endpointWindowSamples = 64;
constexpr int capturedSamples = transitionSamples + endpointWindowSamples;
constexpr float endpointTolerance = 2.0e-4f;

constexpr std::array<float, 2> inputValues { 0.82f, 0.67f };
const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

enum class AutomationTarget
{
    threshold,
    ratio
};

const char* targetName(AutomationTarget target)
{
    return target == AutomationTarget::threshold ? "Threshold" : "Ratio";
}

BandProcessingParameters makeParameters(AutomationTarget target,
                                        bool useHq,
                                        bool highCompressionEndpoint)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 8;
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isDriveEnabled = false;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = true;
    params.isWidthEnabled = false;

    // Shape Mix 0 presents a transparent, steady signal to the compressor and
    // prevents waveshaper behaviour from contaminating the automation probe.
    params.shapeMixVal = 0.0f;
    params.shapeMixValProvider.baseValue = 0.0f;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    params.compAttack = 0.1f;
    params.compRelease = 10.0f;

    if (target == AutomationTarget::threshold)
    {
        params.compThreshold = highCompressionEndpoint ? -30.0f : 0.0f;
        params.compRatio = 10.0f;
    }
    else
    {
        params.compThreshold = -24.0f;
        params.compRatio = highCompressionEndpoint ? 20.0f : 1.0f;
    }
    return params;
}

BandProcessingParameters makeRoutedParameters(AutomationTarget target,
                                              float baseValue)
{
    auto params = makeParameters(target, false, true);
    ModulatedValueProvider* provider = nullptr;
    if (target == AutomationTarget::threshold)
    {
        params.compThreshold = baseValue;
        params.compRatio = 10.0f;
        provider = &params.compThresholdValProvider;
        provider->range = { -48.0f, 0.0f };
        params.compThresholdLfoSourceIndex = 0;
    }
    else
    {
        params.compThreshold = -24.0f;
        params.compRatio = baseValue;
        provider = &params.compRatioValProvider;
        provider->range = { 1.0f, 20.0f };
        params.compRatioLfoSourceIndex = 0;
    }

    provider->baseValue = baseValue;
    provider->modulationDepth = 0.6f;
    provider->isBipolar = true;
    return params;
}

juce::AudioBuffer<float> makeInput(int numChannels, int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
        juce::FloatVectorOperations::fill(
            buffer.getWritePointer(channel),
            inputValues[static_cast<size_t>(channel)],
            numSamples);
    return buffer;
}

struct TransitionResult
{
    std::array<std::vector<float>, 2> subject;
    std::array<std::vector<float>, 2> oldReference;
    std::array<std::vector<float>, 2> newReference;
    float preEventError = 0.0f;
    bool finite = true;
};

float predictableLfoSample(int absoluteSample)
{
    constexpr int periodSamples = 32;
    const float phase = static_cast<float>(absoluteSample % periodSamples)
                        / static_cast<float>(periodSamples);
    return 0.5f + 0.45f * std::sin(juce::MathConstants<float>::twoPi * phase);
}

TransitionResult renderParameterTransition(
    const BandProcessingParameters& oldParams,
    const BandProcessingParameters& newParams,
    int numChannels,
    int preparedBlockSize,
    const std::vector<int>& hostBlockPattern,
    bool usePredictableLfo)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostBlockPattern.empty());

    BandProcessor subject;
    BandProcessor alwaysOld;
    BandProcessor alwaysNew;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        static_cast<juce::uint32>(numChannels)
    };
    subject.prepare(spec);
    alwaysOld.prepare(spec);
    alwaysNew.prepare(spec);

    TransitionResult result;
    for (auto* channels : { &result.subject,
                            &result.oldReference,
                            &result.newReference })
        for (auto& output : *channels)
            output.reserve(capturedSamples);

    constexpr int totalSamples = warmupSamples + capturedSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int samplesThisCallback = hostBlockPattern[
            callbackIndex % hostBlockPattern.size()];
        ++callbackIndex;
        REQUIRE(samplesThisCallback > 0);
        samplesThisCallback = std::min(samplesThisCallback,
                                       totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            samplesThisCallback = std::min(samplesThisCallback,
                                           warmupSamples - streamPosition);

        const auto input = makeInput(numChannels, samplesThisCallback);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> oldBuffer;
        juce::AudioBuffer<float> newBuffer;
        subjectBuffer.makeCopyOf(input);
        oldBuffer.makeCopyOf(input);
        newBuffer.makeCopyOf(input);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisCallback);
        if (usePredictableLfo)
        {
            for (int sample = 0; sample < samplesThisCallback; ++sample)
                lfoOutputs.setSample(0,
                                     sample,
                                     predictableLfoSample(streamPosition + sample));
        }
        else
        {
            lfoOutputs.clear();
        }

        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? oldParams : newParams,
                        lfoOutputs);
        alwaysOld.process(oldBuffer, oldParams, lfoOutputs);
        alwaysNew.process(newBuffer, newParams, lfoOutputs);

        for (int sample = 0; sample < samplesThisCallback; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float subjectSample = subjectBuffer.getSample(channel, sample);
                const float oldSample = oldBuffer.getSample(channel, sample);
                const float newSample = newBuffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(subjectSample)
                                && std::isfinite(oldSample)
                                && std::isfinite(newSample);
                if (absoluteSample < warmupSamples)
                {
                    result.preEventError = std::max(
                        result.preEventError,
                        std::abs(subjectSample - oldSample));
                    continue;
                }

                const auto channelIndex = static_cast<size_t>(channel);
                result.subject[channelIndex].push_back(subjectSample);
                result.oldReference[channelIndex].push_back(oldSample);
                result.newReference[channelIndex].push_back(newSample);
            }
        }
        streamPosition += samplesThisCallback;
    }

    for (int channel = 0; channel < numChannels; ++channel)
        REQUIRE(result.subject[static_cast<size_t>(channel)].size()
                == capturedSamples);
    return result;
}

TransitionResult renderTransition(AutomationTarget target,
                                  bool initialHighCompression,
                                  bool useHq,
                                  int numChannels,
                                  int preparedBlockSize,
                                  const std::vector<int>& hostBlockPattern)
{
    return renderParameterTransition(
        makeParameters(target, useHq, initialHighCompression),
        makeParameters(target, useHq, ! initialHighCompression),
        numChannels,
        preparedBlockSize,
        hostBlockPattern,
        false);
}

float maximumDifference(const TransitionResult& first,
                        const TransitionResult& second,
                        int numChannels)
{
    float result = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto channelIndex = static_cast<size_t>(channel);
        REQUIRE(first.subject[channelIndex].size()
                == second.subject[channelIndex].size());
        for (size_t sample = 0; sample < first.subject[channelIndex].size(); ++sample)
            result = std::max(result,
                              std::abs(first.subject[channelIndex][sample]
                                       - second.subject[channelIndex][sample]));
    }
    return result;
}

void checkTransitionContract(const TransitionResult& result,
                             AutomationTarget target,
                             bool initialHighCompression,
                             bool useHq,
                             int numChannels)
{
    REQUIRE(result.finite);
    CHECK(result.preEventError <= 1.0e-6f);

    float minimumEndpointSeparation = std::numeric_limits<float>::max();
    float maximumEndpointSeparation = 0.0f;
    float firstSampleError = 0.0f;
    float midpointDistanceFromOld = std::numeric_limits<float>::max();
    float midpointDistanceFromNew = std::numeric_limits<float>::max();
    float settledError = 0.0f;
    float maximumStep = 0.0f;
    constexpr int midpointSample = transitionSamples / 2;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto channelIndex = static_cast<size_t>(channel);
        const auto& subject = result.subject[channelIndex];
        const auto& oldReference = result.oldReference[channelIndex];
        const auto& newReference = result.newReference[channelIndex];
        const float endpointSeparation = std::abs(oldReference[0]
                                                  - newReference[0]);
        minimumEndpointSeparation = std::min(minimumEndpointSeparation,
                                             endpointSeparation);
        maximumEndpointSeparation = std::max(maximumEndpointSeparation,
                                             endpointSeparation);
        firstSampleError = std::max(firstSampleError,
                                    std::abs(subject[0] - oldReference[0]));
        midpointDistanceFromOld = std::min(
            midpointDistanceFromOld,
            std::abs(subject[midpointSample] - oldReference[midpointSample]));
        midpointDistanceFromNew = std::min(
            midpointDistanceFromNew,
            std::abs(subject[midpointSample] - newReference[midpointSample]));

        for (int sample = 1; sample < transitionSamples; ++sample)
            maximumStep = std::max(maximumStep,
                                   std::abs(subject[static_cast<size_t>(sample)]
                                            - subject[static_cast<size_t>(sample - 1)]));
        for (int sample = transitionSamples; sample < capturedSamples; ++sample)
            settledError = std::max(
                settledError,
                std::abs(subject[static_cast<size_t>(sample)]
                         - newReference[static_cast<size_t>(sample)]));
    }

    CAPTURE(targetName(target),
            initialHighCompression,
            useHq,
            numChannels,
            minimumEndpointSeparation,
            firstSampleError,
            midpointDistanceFromOld,
            midpointDistanceFromNew,
            maximumStep,
            settledError);
    REQUIRE(minimumEndpointSeparation >= 0.10f);
    CHECK(firstSampleError <= endpointTolerance);
    CHECK(midpointDistanceFromOld >= minimumEndpointSeparation * 0.005f);
    CHECK(midpointDistanceFromNew >= minimumEndpointSeparation * 0.005f);
    CHECK(maximumStep <= maximumEndpointSeparation * 0.20f);
    CHECK(settledError <= endpointTolerance);
}

void checkRoutedBaseTransition(AutomationTarget target)
{
    const float oldBase = target == AutomationTarget::threshold ? -12.0f : 4.0f;
    const float newBase = target == AutomationTarget::threshold ? -30.0f : 16.0f;
    const auto oldParams = makeRoutedParameters(target, oldBase);
    const auto newParams = makeRoutedParameters(target, newBase);
    constexpr int numChannels = 2;
    const auto fixed = renderParameterTransition(oldParams,
                                                 newParams,
                                                 numChannels,
                                                 257,
                                                 fixedCallbacks,
                                                 true);
    const auto irregular = renderParameterTransition(oldParams,
                                                     newParams,
                                                     numChannels,
                                                     257,
                                                     irregularCallbacks,
                                                     true);

    float minimumEndpointSeparation = std::numeric_limits<float>::max();
    float firstSampleError = 0.0f;
    float midpointDistanceFromOld = std::numeric_limits<float>::max();
    float midpointDistanceFromNew = std::numeric_limits<float>::max();
    float settledError = 0.0f;
    float referenceLfoActivity = 0.0f;
    constexpr int midpointSample = transitionSamples / 2;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto channelIndex = static_cast<size_t>(channel);
        const auto& subject = fixed.subject[channelIndex];
        const auto& oldReference = fixed.oldReference[channelIndex];
        const auto& newReference = fixed.newReference[channelIndex];
        minimumEndpointSeparation = std::min(
            minimumEndpointSeparation,
            std::abs(oldReference[0] - newReference[0]));
        firstSampleError = std::max(firstSampleError,
                                    std::abs(subject[0] - oldReference[0]));
        midpointDistanceFromOld = std::min(
            midpointDistanceFromOld,
            std::abs(subject[midpointSample] - oldReference[midpointSample]));
        midpointDistanceFromNew = std::min(
            midpointDistanceFromNew,
            std::abs(subject[midpointSample] - newReference[midpointSample]));

        for (int sample = transitionSamples; sample < capturedSamples; ++sample)
        {
            settledError = std::max(
                settledError,
                std::abs(subject[static_cast<size_t>(sample)]
                         - newReference[static_cast<size_t>(sample)]));
            if (sample > transitionSamples)
                referenceLfoActivity = std::max(
                    referenceLfoActivity,
                    std::abs(newReference[static_cast<size_t>(sample)]
                             - newReference[static_cast<size_t>(sample - 1)]));
        }
    }

    const float partitionError = maximumDifference(fixed,
                                                   irregular,
                                                   numChannels);
    CAPTURE(targetName(target),
            minimumEndpointSeparation,
            firstSampleError,
            midpointDistanceFromOld,
            midpointDistanceFromNew,
            settledError,
            referenceLfoActivity,
            partitionError);
    REQUIRE(fixed.finite);
    REQUIRE(irregular.finite);
    REQUIRE(minimumEndpointSeparation >= 0.02f);
    CHECK(firstSampleError <= endpointTolerance);
    CHECK(midpointDistanceFromOld >= minimumEndpointSeparation * 0.005f);
    CHECK(midpointDistanceFromNew >= minimumEndpointSeparation * 0.005f);
    CHECK(settledError <= endpointTolerance);

    // The no-base-event target reference retains the 32-sample LFO ripple.
    // Matching it after the base ramp prevents an implementation from putting
    // the complete modulated value through the 10 ms base smoother.
    REQUIRE(referenceLfoActivity >= minimumEndpointSeparation * 0.002f);
    CHECK(partitionError <= endpointTolerance);
}

juce::AudioBuffer<float> processConstantBlock(BandProcessor& band,
                                              const BandProcessingParameters& params,
                                              int numSamples)
{
    auto buffer = makeInput(2, numSamples);
    juce::AudioBuffer<float> lfoOutputs(1, numSamples);
    lfoOutputs.clear();
    band.process(buffer, params, lfoOutputs);
    return buffer;
}

float maximumBufferDifference(const juce::AudioBuffer<float>& first,
                              const juce::AudioBuffer<float>& second)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());
    float result = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            result = std::max(result,
                              std::abs(first.getSample(channel, sample)
                                       - second.getSample(channel, sample)));
    return result;
}
} // namespace

TEST_CASE("Ordinary compressor Threshold and Ratio automation ramps for ten milliseconds",
          "[band][compressor][automation][transition]")
{
    for (const auto target : { AutomationTarget::threshold,
                               AutomationTarget::ratio })
        for (const bool initialHighCompression : { false, true })
            for (const bool useHq : { false, true })
                for (const int numChannels : { 1, 2 })
                {
                    DYNAMIC_SECTION(targetName(target)
                                    << ", high->low=" << initialHighCompression
                                    << ", HQ=" << useHq
                                    << ", channels=" << numChannels)
                    {
                        const auto result = renderTransition(target,
                                                             initialHighCompression,
                                                             useHq,
                                                             numChannels,
                                                             257,
                                                             fixedCallbacks);
                        checkTransitionContract(result,
                                                target,
                                                initialHighCompression,
                                                useHq,
                                                numChannels);
                    }
                }
}

TEST_CASE("Compressor automation ramps ignore host partitions and internal chunks",
          "[band][compressor][automation][transition][block-size][internal-chunk]")
{
    for (const auto target : { AutomationTarget::threshold,
                               AutomationTarget::ratio })
        for (const bool useHq : { false, true })
        {
            DYNAMIC_SECTION(targetName(target) << ", HQ=" << useHq)
            {
                constexpr int numChannels = 2;
                const auto fixed = renderTransition(target,
                                                    false,
                                                    useHq,
                                                    numChannels,
                                                    257,
                                                    fixedCallbacks);
                const auto irregular = renderTransition(target,
                                                        false,
                                                        useHq,
                                                        numChannels,
                                                        257,
                                                        irregularCallbacks);
                const auto internallyChunked = renderTransition(target,
                                                                false,
                                                                useHq,
                                                                numChannels,
                                                                64,
                                                                fixedCallbacks);
                const float partitionError = maximumDifference(fixed,
                                                               irregular,
                                                               numChannels);
                const float chunkError = maximumDifference(fixed,
                                                           internallyChunked,
                                                           numChannels);
                CAPTURE(targetName(target),
                        useHq,
                        partitionError,
                        chunkError);
                CHECK(partitionError <= endpointTolerance);
                CHECK(chunkError <= endpointTolerance);
            }
        }
}

TEST_CASE("Compressor LFO remains sample-accurate while its base value ramps",
          "[band][compressor][automation][transition][lfo][block-size]")
{
    for (const auto target : { AutomationTarget::threshold,
                               AutomationTarget::ratio })
    {
        DYNAMIC_SECTION(targetName(target))
        {
            checkRoutedBaseTransition(target);
        }
    }
}

TEST_CASE("Compressor reset clears a partial ordinary-automation ramp",
          "[band][compressor][automation][transition][reset]")
{
    constexpr int samplesBeforeReset = 160;
    constexpr int firstBlockAfterReset = 64;
    const auto oldParams = makeParameters(AutomationTarget::threshold,
                                          false,
                                          false);
    const auto targetParams = makeParameters(AutomationTarget::threshold,
                                             false,
                                             true);
    const juce::dsp::ProcessSpec spec { sampleRate, 4096, 2 };
    BandProcessor subject;
    BandProcessor sameLifecycleReference;
    subject.prepare(spec);
    sameLifecycleReference.prepare(spec);

    processConstantBlock(subject, oldParams, warmupSamples);
    processConstantBlock(subject, targetParams, samplesBeforeReset);
    // JUCE Gain retains its most recent target across reset(). Prime the
    // otherwise-clean reference once so this test isolates compressor ramp
    // history instead of the band's pre-existing Output-gain lifecycle.
    processConstantBlock(sameLifecycleReference, targetParams, 1);
    subject.reset();
    sameLifecycleReference.reset();

    const auto afterReset = processConstantBlock(subject,
                                                 targetParams,
                                                 firstBlockAfterReset);
    const auto sameLifecycleCanonical = processConstantBlock(
        sameLifecycleReference,
        targetParams,
        firstBlockAfterReset);
    const float resetError = maximumBufferDifference(afterReset,
                                                     sameLifecycleCanonical);
    CAPTURE(resetError);
    CHECK(resetError <= 1.0e-6f);
}
