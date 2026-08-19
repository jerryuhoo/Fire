#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int transitionSamples = 480;
constexpr int warmupSamples = 4096;
constexpr int responseGuardSamples = 32;
constexpr int capturedSamples = transitionSamples + responseGuardSamples + 1;
constexpr float comparisonTolerance = 2.0e-4f;
constexpr std::array<float, 2> highInputs { 1.0f, 0.85f };
constexpr std::array<float, 2> lowInputs { 0.20f, 0.25f };
const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

float logarithmicRamp(float initialValue,
                      float targetValue,
                      int sample)
{
    const float progress = juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(sample) / static_cast<float>(transitionSamples));
    return initialValue
           * std::pow(targetValue / initialValue, progress);
}

BandProcessingParameters makeParameters(float releaseMs, bool useHq)
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
    params.shapeMixVal = 0.0f;
    params.shapeMixValProvider.baseValue = 0.0f;
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.compThreshold = -6.0f;
    params.compRatio = 20.0f;
    params.compAttack = 0.1f;
    params.compRelease = releaseMs;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    return params;
}

BandProcessingParameters makeCanonicalParameters(bool useHq)
{
    auto params = makeParameters(10.0f, useHq);
    // A stable unipolar route over the linear 10..2000 ms range maps
    // LFO=(desired-10)/1990 directly to the desired Release. Its signature and
    // base never change, so it is an independent sample-accurate oracle for the
    // ordinary parameter smoother under test.
    params.compReleaseValProvider.baseValue = 10.0f;
    params.compReleaseValProvider.modulationDepth = 1.0f;
    params.compReleaseValProvider.isBipolar = false;
    params.compReleaseValProvider.range = { 10.0f, 2000.0f };
    params.compReleaseLfoSourceIndex = 0;
    return params;
}

BandProcessingParameters makeRoutedParameters(float baseReleaseMs,
                                              bool useHq)
{
    auto params = makeParameters(baseReleaseMs, useHq);
    params.compReleaseValProvider.baseValue = baseReleaseMs;
    params.compReleaseValProvider.modulationDepth = 0.25f;
    params.compReleaseValProvider.isBipolar = true;
    params.compReleaseValProvider.range = { 10.0f, 2000.0f };
    params.compReleaseLfoSourceIndex = 0;
    return params;
}

bool isSparseProbe(int eventSample)
{
    return eventSample == 0
           || eventSample == transitionSamples / 2
           || eventSample == transitionSamples;
}

float routedLfoSample(int absoluteSample)
{
    constexpr int periodSamples = 32;
    const float phase = static_cast<float>(absoluteSample % periodSamples)
                        / static_cast<float>(periodSamples);
    return 0.5f + 0.45f
                    * std::sin(juce::MathConstants<float>::twoPi * phase);
}

float modulatedRelease(float baseReleaseMs, float lfoSample)
{
    ModulatedValueProvider provider;
    provider.lfoSignal = &lfoSample;
    provider.baseValue = baseReleaseMs;
    provider.modulationDepth = 0.25f;
    provider.isBipolar = true;
    provider.range = { 10.0f, 2000.0f };
    return provider.get(0);
}

juce::AudioBuffer<float> makeInput(int numChannels,
                                   int streamPosition,
                                   int numSamples,
                                   bool continuousReleaseExercise)
{
    juce::AudioBuffer<float> result(numChannels, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const int absoluteSample = streamPosition + sample;
        const int eventSample = absoluteSample - warmupSamples;
        const bool lowPhase = continuousReleaseExercise
                                  ? absoluteSample % 64 >= 8
                                  : isSparseProbe(eventSample);
        for (int channel = 0; channel < numChannels; ++channel)
            result.setSample(channel,
                             sample,
                             lowPhase
                                 ? lowInputs[static_cast<size_t>(channel)]
                                 : highInputs[static_cast<size_t>(channel)]);
    }
    return result;
}

juce::AudioBuffer<float> makeLfo(int numSamples)
{
    juce::AudioBuffer<float> result(1, numSamples);
    result.clear();
    return result;
}

struct CanonicalRender
{
    std::array<std::vector<float>, 2> subject;
    int numChannels = 0;
    bool finite = true;
    float preEventError = 0.0f;
    float eventWindowError = 0.0f;
    std::array<float, 3> checkpointErrors {};
};

void captureComparison(CanonicalRender& result,
                       const juce::AudioBuffer<float>& subject,
                       const juce::AudioBuffer<float>& canonical,
                       int streamPosition)
{
    REQUIRE(subject.getNumChannels() == canonical.getNumChannels());
    REQUIRE(subject.getNumSamples() == canonical.getNumSamples());
    for (int sample = 0; sample < subject.getNumSamples(); ++sample)
    {
        const int eventSample = streamPosition + sample - warmupSamples;
        for (int channel = 0; channel < subject.getNumChannels(); ++channel)
        {
            const float actual = subject.getSample(channel, sample);
            const float expected = canonical.getSample(channel, sample);
            result.finite = result.finite
                            && std::isfinite(actual)
                            && std::isfinite(expected);
            const float error = std::abs(actual - expected);
            if (eventSample < 0)
            {
                result.preEventError = std::max(result.preEventError, error);
                continue;
            }

            result.eventWindowError = std::max(result.eventWindowError, error);
            result.subject[static_cast<size_t>(channel)].push_back(actual);
            for (size_t checkpoint = 0;
                 checkpoint < result.checkpointErrors.size();
                 ++checkpoint)
            {
                if (eventSample
                    == static_cast<int>(checkpoint)
                           * transitionSamples / 2)
                {
                    result.checkpointErrors[checkpoint] = std::max(
                        result.checkpointErrors[checkpoint],
                        error);
                }
            }
        }
    }
}

CanonicalRender renderOrdinaryTransition(
    float initialReleaseMs,
    float targetReleaseMs,
    bool useHq,
    int numChannels,
    int preparedBlockSize,
    const std::vector<int>& hostPattern)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostPattern.empty());
    BandProcessor subject;
    BandProcessor canonical;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        static_cast<juce::uint32>(numChannels)
    };
    subject.prepare(spec);
    canonical.prepare(spec);
    const auto initialParams = makeParameters(initialReleaseMs, useHq);
    const auto targetParams = makeParameters(targetReleaseMs, useHq);
    const auto canonicalParams = makeCanonicalParameters(useHq);

    CanonicalRender result;
    result.numChannels = numChannels;
    for (int channel = 0; channel < numChannels; ++channel)
        result.subject[static_cast<size_t>(channel)].reserve(capturedSamples);

    const int totalSamples = warmupSamples + capturedSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int blockSize = hostPattern[callbackIndex++ % hostPattern.size()];
        REQUIRE(blockSize > 0);
        blockSize = std::min(blockSize, totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            blockSize = std::min(blockSize, warmupSamples - streamPosition);

        const auto input = makeInput(numChannels,
                                     streamPosition,
                                     blockSize,
                                     false);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> canonicalBuffer;
        subjectBuffer.makeCopyOf(input);
        canonicalBuffer.makeCopyOf(input);
        auto subjectLfo = makeLfo(blockSize);
        auto canonicalLfo = makeLfo(blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int eventSample = streamPosition + sample - warmupSamples;
            const float desiredRelease = logarithmicRamp(initialReleaseMs,
                                                         targetReleaseMs,
                                                         eventSample);
            canonicalLfo.setSample(
                0,
                sample,
                juce::jlimit(0.0f,
                             1.0f,
                             (desiredRelease - 10.0f) / 1990.0f));
        }

        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? initialParams
                                                       : targetParams,
                        subjectLfo);
        canonical.process(canonicalBuffer, canonicalParams, canonicalLfo);
        captureComparison(result,
                          subjectBuffer,
                          canonicalBuffer,
                          streamPosition);
        streamPosition += blockSize;
    }
    return result;
}

CanonicalRender renderRoutedBaseTransition(
    bool useHq,
    int preparedBlockSize,
    const std::vector<int>& hostPattern)
{
    constexpr int numChannels = 2;
    constexpr float initialBaseMs = 600.0f;
    constexpr float targetBaseMs = 1400.0f;
    BandProcessor subject;
    BandProcessor canonical;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        numChannels
    };
    subject.prepare(spec);
    canonical.prepare(spec);
    const auto initialParams = makeRoutedParameters(initialBaseMs, useHq);
    const auto targetParams = makeRoutedParameters(targetBaseMs, useHq);
    const auto canonicalParams = makeCanonicalParameters(useHq);

    CanonicalRender result;
    result.numChannels = numChannels;
    for (auto& channel : result.subject)
        channel.reserve(capturedSamples);
    const int totalSamples = warmupSamples + capturedSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int blockSize = hostPattern[callbackIndex++ % hostPattern.size()];
        blockSize = std::min(blockSize, totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            blockSize = std::min(blockSize, warmupSamples - streamPosition);
        const auto input = makeInput(numChannels,
                                     streamPosition,
                                     blockSize,
                                     true);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> canonicalBuffer;
        subjectBuffer.makeCopyOf(input);
        canonicalBuffer.makeCopyOf(input);
        auto subjectLfo = makeLfo(blockSize);
        auto canonicalLfo = makeLfo(blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            const int eventSample = absoluteSample - warmupSamples;
            const float lfo = routedLfoSample(absoluteSample);
            subjectLfo.setSample(0, sample, lfo);
            const float desiredBase = logarithmicRamp(initialBaseMs,
                                                      targetBaseMs,
                                                      eventSample);
            const float desiredRelease = modulatedRelease(desiredBase, lfo);
            canonicalLfo.setSample(
                0,
                sample,
                juce::jlimit(0.0f,
                             1.0f,
                             (desiredRelease - 10.0f) / 1990.0f));
        }
        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? initialParams
                                                       : targetParams,
                        subjectLfo);
        canonical.process(canonicalBuffer, canonicalParams, canonicalLfo);
        captureComparison(result,
                          subjectBuffer,
                          canonicalBuffer,
                          streamPosition);
        streamPosition += blockSize;
    }
    return result;
}

CanonicalRender renderRapidRetarget()
{
    constexpr int numChannels = 2;
    constexpr float initialReleaseMs = 2000.0f;
    constexpr float firstTargetMs = 10.0f;
    constexpr float latestTargetMs = 100.0f;
    constexpr int retargetSample = 160;
    constexpr int rapidCapturedSamples = retargetSample
                                         + transitionSamples
                                         + responseGuardSamples + 1;
    const float retargetAnchor = logarithmicRamp(initialReleaseMs,
                                                 firstTargetMs,
                                                 retargetSample);
    BandProcessor subject;
    BandProcessor canonical;
    const juce::dsp::ProcessSpec spec { sampleRate, 257, numChannels };
    subject.prepare(spec);
    canonical.prepare(spec);
    const auto initialParams = makeParameters(initialReleaseMs, false);
    const auto firstTargetParams = makeParameters(firstTargetMs, false);
    const auto latestTargetParams = makeParameters(latestTargetMs, false);
    const auto canonicalParams = makeCanonicalParameters(false);

    CanonicalRender result;
    result.numChannels = numChannels;
    for (auto& channel : result.subject)
        channel.reserve(rapidCapturedSamples);
    const int totalSamples = warmupSamples + rapidCapturedSamples;
    int streamPosition = 0;
    while (streamPosition < totalSamples)
    {
        int blockSize = std::min(257, totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            blockSize = std::min(blockSize, warmupSamples - streamPosition);
        else if (streamPosition < warmupSamples + retargetSample)
            blockSize = std::min(blockSize,
                                 warmupSamples + retargetSample
                                     - streamPosition);

        const auto input = makeInput(numChannels,
                                     streamPosition,
                                     blockSize,
                                     true);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> canonicalBuffer;
        subjectBuffer.makeCopyOf(input);
        canonicalBuffer.makeCopyOf(input);
        auto subjectLfo = makeLfo(blockSize);
        auto canonicalLfo = makeLfo(blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int eventSample = streamPosition + sample - warmupSamples;
            const float desiredRelease = eventSample < retargetSample
                                             ? logarithmicRamp(initialReleaseMs,
                                                               firstTargetMs,
                                                               eventSample)
                                             : logarithmicRamp(
                                                   retargetAnchor,
                                                   latestTargetMs,
                                                   eventSample - retargetSample);
            canonicalLfo.setSample(
                0,
                sample,
                juce::jlimit(0.0f,
                             1.0f,
                             (desiredRelease - 10.0f) / 1990.0f));
        }
        const BandProcessingParameters* subjectParams = &initialParams;
        if (streamPosition >= warmupSamples + retargetSample)
            subjectParams = &latestTargetParams;
        else if (streamPosition >= warmupSamples)
            subjectParams = &firstTargetParams;
        subject.process(subjectBuffer, *subjectParams, subjectLfo);
        canonical.process(canonicalBuffer, canonicalParams, canonicalLfo);
        captureComparison(result,
                          subjectBuffer,
                          canonicalBuffer,
                          streamPosition);
        streamPosition += blockSize;
    }
    return result;
}

float maximumSubjectDifference(const CanonicalRender& lhs,
                               const CanonicalRender& rhs)
{
    REQUIRE(lhs.numChannels == rhs.numChannels);
    float error = 0.0f;
    for (int channel = 0; channel < lhs.numChannels; ++channel)
    {
        const auto index = static_cast<size_t>(channel);
        REQUIRE(lhs.subject[index].size() == rhs.subject[index].size());
        for (size_t sample = 0; sample < lhs.subject[index].size(); ++sample)
            error = std::max(error,
                             std::abs(lhs.subject[index][sample]
                                      - rhs.subject[index][sample]));
    }
    return error;
}

void checkCanonical(const CanonicalRender& result)
{
    CAPTURE(result.numChannels,
            result.preEventError,
            result.eventWindowError,
            result.checkpointErrors[0],
            result.checkpointErrors[1],
            result.checkpointErrors[2]);
    REQUIRE(result.finite);
    CHECK(result.preEventError <= 1.0e-6f);
    CHECK(result.eventWindowError <= comparisonTolerance);
    CHECK(result.checkpointErrors[0] <= comparisonTolerance);
    CHECK(result.checkpointErrors[1] <= comparisonTolerance);
    CHECK(result.checkpointErrors[2] <= comparisonTolerance);
}

juce::AudioBuffer<float> processResetBlock(
    BandProcessor& band,
    const BandProcessingParameters& params,
    int numSamples,
    int streamPosition)
{
    auto buffer = makeInput(2,
                            streamPosition,
                            numSamples,
                            true);
    auto noLfo = makeLfo(numSamples);
    band.process(buffer, params, noLfo);
    return buffer;
}

float maximumBufferDifference(const juce::AudioBuffer<float>& lhs,
                              const juce::AudioBuffer<float>& rhs)
{
    REQUIRE(lhs.getNumChannels() == rhs.getNumChannels());
    REQUIRE(lhs.getNumSamples() == rhs.getNumSamples());
    float error = 0.0f;
    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
        for (int sample = 0; sample < lhs.getNumSamples(); ++sample)
            error = std::max(error,
                             std::abs(lhs.getSample(channel, sample)
                                      - rhs.getSample(channel, sample)));
    return error;
}
} // namespace

TEST_CASE("Ordinary Compressor Release automation ramps logarithmically for ten milliseconds",
          "[band][compressor][release][automation][transition]")
{
    constexpr std::array<std::array<float, 2>, 3> directions {
        std::array<float, 2> { 10.0f, 2000.0f },
        std::array<float, 2> { 2000.0f, 10.0f },
        std::array<float, 2> { 100.0f, 10.0f }
    };
    for (const auto& direction : directions)
        for (const bool useHq : { false, true })
            for (const int numChannels : { 1, 2 })
            {
                DYNAMIC_SECTION(direction[0] << " -> " << direction[1]
                                                 << " ms, HQ=" << useHq
                                                 << ", channels=" << numChannels)
                {
                    const auto result = renderOrdinaryTransition(
                        direction[0],
                        direction[1],
                        useHq,
                        numChannels,
                        257,
                        fixedCallbacks);
                    checkCanonical(result);
                }
            }
}

TEST_CASE("Compressor Release automation ignores host and internal partitions",
          "[band][compressor][release][automation][transition][block-size][internal-chunk]")
{
    for (const bool useHq : { false, true })
    {
        const auto fixed = renderOrdinaryTransition(2000.0f,
                                                    10.0f,
                                                    useHq,
                                                    2,
                                                    257,
                                                    fixedCallbacks);
        const auto irregular = renderOrdinaryTransition(2000.0f,
                                                        10.0f,
                                                        useHq,
                                                        2,
                                                        257,
                                                        irregularCallbacks);
        const auto internallyChunked = renderOrdinaryTransition(
            2000.0f,
            10.0f,
            useHq,
            2,
            64,
            fixedCallbacks);
        const float partitionError = maximumSubjectDifference(fixed,
                                                               irregular);
        const float chunkError = maximumSubjectDifference(fixed,
                                                           internallyChunked);
        CAPTURE(useHq, partitionError, chunkError);
        CHECK(partitionError <= comparisonTolerance);
        CHECK(chunkError <= comparisonTolerance);
    }
}

TEST_CASE("Routed Compressor Release keeps its LFO trajectory while the base ramps",
          "[band][compressor][release][automation][transition][lfo][sample-accurate][block-size]")
{
    for (const bool useHq : { false, true })
    {
        const auto fixed = renderRoutedBaseTransition(useHq,
                                                      257,
                                                      fixedCallbacks);
        const auto irregular = renderRoutedBaseTransition(useHq,
                                                          257,
                                                          irregularCallbacks);
        checkCanonical(fixed);
        checkCanonical(irregular);
        const float partitionError = maximumSubjectDifference(fixed,
                                                               irregular);
        CAPTURE(useHq, partitionError);
        CHECK(partitionError <= comparisonTolerance);
    }
}

TEST_CASE("Rapid Compressor Release automation is continuous and latest wins",
          "[band][compressor][release][automation][transition][rapid][latest]")
{
    const auto result = renderRapidRetarget();
    checkCanonical(result);
}

TEST_CASE("Reset discards a partial Compressor Release automation ramp",
          "[band][compressor][release][automation][transition][reset]")
{
    const auto oldParams = makeParameters(2000.0f, false);
    const auto targetParams = makeParameters(10.0f, false);
    const juce::dsp::ProcessSpec spec { sampleRate, 257, 2 };
    BandProcessor subject;
    BandProcessor sameResetReference;
    subject.prepare(spec);
    sameResetReference.prepare(spec);
    processResetBlock(subject, oldParams, warmupSamples, 0);
    processResetBlock(subject, targetParams, 160, warmupSamples);
    processResetBlock(sameResetReference, targetParams, 1, 0);
    subject.reset();
    sameResetReference.reset();
    const auto afterReset = processResetBlock(subject, targetParams, 64, 0);
    const auto canonical = processResetBlock(sameResetReference,
                                             targetParams,
                                             64,
                                             0);
    const float resetError = maximumBufferDifference(afterReset, canonical);
    CAPTURE(resetError);
    CHECK(resetError <= 1.0e-6f);
}
