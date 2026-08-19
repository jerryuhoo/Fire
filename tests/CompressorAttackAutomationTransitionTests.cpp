#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr float oldAttackMs = 10.0f;
constexpr float newAttackMs = 0.1f;
constexpr int transitionSamples = 480;
constexpr int warmupSamples = 4096;
constexpr int responseGuardSamples = 24;
constexpr int capturedSamples = transitionSamples + responseGuardSamples;
constexpr float partitionTolerance = 2.0e-4f;
constexpr std::array<float, 2> pulseValues { 0.90f, 0.72f };
const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

BandProcessingParameters makeParameters(float attackMs, bool useHq)
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

    // Shape Mix 0 gives the compressor a transparent carrier. Isolated pulses
    // then expose only its detector's Attack coefficient.
    params.shapeMixVal = 0.0f;
    params.shapeMixValProvider.baseValue = 0.0f;
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.compThreshold = -42.0f;
    params.compRatio = 20.0f;
    params.compAttack = attackMs;
    params.compRelease = 0.1f;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    return params;
}

bool isProbeSample(int eventSample)
{
    return eventSample == 0
           || eventSample == transitionSamples / 2
           || eventSample == transitionSamples;
}

juce::AudioBuffer<float> makeInput(int numChannels,
                                   int streamPosition,
                                   int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    buffer.clear();
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const int eventSample = streamPosition + sample - warmupSamples;
        if (! isProbeSample(eventSample))
            continue;

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.setSample(channel,
                             sample,
                             pulseValues[static_cast<size_t>(channel)]);
    }
    return buffer;
}

struct TransitionRender
{
    std::array<std::vector<float>, 2> subject;
    std::array<std::vector<float>, 2> oldReference;
    std::array<std::vector<float>, 2> midpointReference;
    std::array<std::vector<float>, 2> newReference;
    int numChannels = 0;
    bool finite = true;
    float preEventError = 0.0f;
};

TransitionRender renderTransition(float initialAttackMs,
                                  float targetAttackMs,
                                  bool useHq,
                                  int numChannels,
                                  int preparedBlockSize,
                                  const std::vector<int>& hostPattern)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostPattern.empty());

    BandProcessor subject;
    BandProcessor alwaysOld;
    BandProcessor alwaysMidpoint;
    BandProcessor alwaysNew;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        static_cast<juce::uint32>(numChannels)
    };
    subject.prepare(spec);
    alwaysOld.prepare(spec);
    alwaysMidpoint.prepare(spec);
    alwaysNew.prepare(spec);
    // Attack is a logarithmic time control. Its 10 ms bridge advances
    // multiplicatively, so sample 240 uses the geometric rather than the
    // arithmetic midpoint. This oracle rejects a linear-ms implementation.
    const float midpointAttackMs = std::sqrt(initialAttackMs * targetAttackMs);
    const auto oldParams = makeParameters(initialAttackMs, useHq);
    const auto midpointParams = makeParameters(midpointAttackMs, useHq);
    const auto newParams = makeParameters(targetAttackMs, useHq);

    TransitionRender result;
    result.numChannels = numChannels;
    for (auto* output : { &result.subject,
                          &result.oldReference,
                          &result.midpointReference,
                          &result.newReference })
        for (int channel = 0; channel < numChannels; ++channel)
            (*output)[static_cast<size_t>(channel)].reserve(capturedSamples);

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

        const auto input = makeInput(numChannels, streamPosition, blockSize);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> oldBuffer;
        juce::AudioBuffer<float> midpointBuffer;
        juce::AudioBuffer<float> newBuffer;
        subjectBuffer.makeCopyOf(input);
        oldBuffer.makeCopyOf(input);
        midpointBuffer.makeCopyOf(input);
        newBuffer.makeCopyOf(input);
        juce::AudioBuffer<float> noLfo(1, blockSize);
        noLfo.clear();

        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? oldParams : newParams,
                        noLfo);
        alwaysOld.process(oldBuffer, oldParams, noLfo);
        // Warmup is all zero, so each detector and the HQ oversampler remain
        // in the same zero state. Keep these endpoint oracles static for their
        // complete lifetime; otherwise the smoother under test would also
        // turn the references themselves into automation trajectories.
        alwaysMidpoint.process(midpointBuffer, midpointParams, noLfo);
        alwaysNew.process(newBuffer, newParams, noLfo);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float oldValue = oldBuffer.getSample(channel, sample);
                const float midpointValue = midpointBuffer.getSample(channel, sample);
                const float newValue = newBuffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(oldValue)
                                && std::isfinite(midpointValue)
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
                result.midpointReference[index].push_back(midpointValue);
                result.newReference[index].push_back(newValue);
            }
        }
        streamPosition += blockSize;
    }
    return result;
}

int findProbeOffset(const TransitionRender& result)
{
    const int searchSamples = std::min(responseGuardSamples,
                                       static_cast<int>(result.subject[0].size()));
    int bestSample = 0;
    float bestSeparation = 0.0f;
    for (int sample = 0; sample < searchSamples; ++sample)
    {
        const auto index = static_cast<size_t>(sample);
        const float separation = std::abs(result.oldReference[0][index]
                                          - result.newReference[0][index]);
        if (separation > bestSeparation)
        {
            bestSeparation = separation;
            bestSample = sample;
        }
    }
    return bestSample;
}

void checkTransition(const TransitionRender& result,
                     float initialAttackMs,
                     float targetAttackMs,
                     bool useHq)
{
    REQUIRE(result.finite);
    CHECK(result.preEventError <= 1.0e-6f);
    const int probeOffset = findProbeOffset(result);
    const int midpoint = probeOffset + transitionSamples / 2;
    const int endpoint = probeOffset + transitionSamples;
    float minimumSeparation = std::numeric_limits<float>::max();
    float firstOldError = 0.0f;
    float firstNewDistance = std::numeric_limits<float>::max();
    float midpointError = 0.0f;
    float endpointError = 0.0f;
    for (int channel = 0; channel < result.numChannels; ++channel)
    {
        const auto index = static_cast<size_t>(channel);
        const auto& subject = result.subject[index];
        const auto& oldReference = result.oldReference[index];
        const auto& midpointReference = result.midpointReference[index];
        const auto& newReference = result.newReference[index];
        REQUIRE(endpoint < static_cast<int>(subject.size()));
        const auto firstIndex = static_cast<size_t>(probeOffset);
        const auto midpointIndex = static_cast<size_t>(midpoint);
        const auto endpointIndex = static_cast<size_t>(endpoint);
        const float separation = std::abs(oldReference[firstIndex]
                                          - newReference[firstIndex]);
        minimumSeparation = std::min(minimumSeparation, separation);
        firstOldError = std::max(firstOldError,
                                 std::abs(subject[firstIndex]
                                          - oldReference[firstIndex]));
        firstNewDistance = std::min(firstNewDistance,
                                    std::abs(subject[firstIndex]
                                             - newReference[firstIndex]));
        midpointError = std::max(midpointError,
                                 std::abs(subject[midpointIndex]
                                          - midpointReference[midpointIndex]));
        endpointError = std::max(endpointError,
                                 std::abs(subject[endpointIndex]
                                          - newReference[endpointIndex]));
    }

    const float timingTolerance = useHq
                                      ? minimumSeparation * 0.04f
                                      : 2.0e-4f;
    CAPTURE(useHq,
            result.numChannels,
            initialAttackMs,
            targetAttackMs,
            probeOffset,
            minimumSeparation,
            firstOldError,
            firstNewDistance,
            midpointError,
            endpointError,
            timingTolerance);
    REQUIRE(minimumSeparation >= 0.05f);
    CHECK(firstOldError <= timingTolerance);
    CHECK(firstNewDistance >= minimumSeparation * 0.80f);
    CHECK(midpointError <= timingTolerance);
    CHECK(endpointError <= 2.0e-4f);
}

float maximumDifference(const TransitionRender& lhs,
                        const TransitionRender& rhs)
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

juce::AudioBuffer<float> processSilence(BandProcessor& band,
                                        const BandProcessingParameters& params,
                                        int numSamples,
                                        int numChannels = 2)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    buffer.clear();
    juce::AudioBuffer<float> noLfo(1, numSamples);
    noLfo.clear();
    band.process(buffer, params, noLfo);
    return buffer;
}

juce::AudioBuffer<float> processPulse(BandProcessor& band,
                                      const BandProcessingParameters& params,
                                      int numChannels = 2)
{
    juce::AudioBuffer<float> buffer(numChannels, 1);
    for (int channel = 0; channel < numChannels; ++channel)
        buffer.setSample(channel,
                         0,
                         pulseValues[static_cast<size_t>(channel)]);
    juce::AudioBuffer<float> noLfo(1, 1);
    noLfo.clear();
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

TEST_CASE("Ordinary Compressor Attack automation ramps for ten milliseconds",
          "[band][compressor][attack][automation][transition]")
{
    const std::array<std::pair<float, float>, 3> directions {
        std::pair { oldAttackMs, newAttackMs },
        std::pair { newAttackMs, oldAttackMs },
        std::pair { 200.0f, newAttackMs }
    };
    for (const auto [initialAttackMs, targetAttackMs] : directions)
        for (const bool useHq : { false, true })
            for (const int numChannels : { 1, 2 })
            {
                DYNAMIC_SECTION(initialAttackMs << " -> " << targetAttackMs
                                                << " ms, HQ=" << useHq
                                                << ", channels=" << numChannels)
                {
                    const auto result = renderTransition(initialAttackMs,
                                                         targetAttackMs,
                                                         useHq,
                                                         numChannels,
                                                         257,
                                                         fixedCallbacks);
                    checkTransition(result,
                                    initialAttackMs,
                                    targetAttackMs,
                                    useHq);
                }
            }
}

TEST_CASE("Compressor Attack automation ignores host and internal partitions",
          "[band][compressor][attack][automation][transition][block-size][internal-chunk]")
{
    for (const bool useHq : { false, true })
    {
        const auto fixed = renderTransition(oldAttackMs,
                                            newAttackMs,
                                            useHq,
                                            2,
                                            257,
                                            fixedCallbacks);
        const auto irregular = renderTransition(oldAttackMs,
                                                newAttackMs,
                                                useHq,
                                                2,
                                                257,
                                                irregularCallbacks);
        const auto internallyChunked = renderTransition(oldAttackMs,
                                                        newAttackMs,
                                                        useHq,
                                                        2,
                                                        64,
                                                        fixedCallbacks);
        const float partitionError = maximumDifference(fixed, irregular);
        const float chunkError = maximumDifference(fixed, internallyChunked);
        CAPTURE(useHq, partitionError, chunkError);
        CHECK(partitionError <= partitionTolerance);
        CHECK(chunkError <= partitionTolerance);
    }
}

TEST_CASE("Rapid Compressor Attack automation is continuous and latest wins",
          "[band][compressor][attack][automation][transition][rapid][latest]")
{
    constexpr float thirdAttackMs = 200.0f;
    const auto initialParams = makeParameters(oldAttackMs, false);
    const auto firstTargetParams = makeParameters(newAttackMs, false);
    const auto latestParams = makeParameters(thirdAttackMs, false);
    BandProcessor subject;
    subject.prepare({ sampleRate, 257, 2 });
    processSilence(subject, initialParams, warmupSamples);
    processSilence(subject, firstTargetParams, 160);
    const float anchorAttackMs =
        subject.compressorAttackBaseSmoother.getCurrentValue();

    BandProcessor anchorReference;
    anchorReference.prepare({ sampleRate, 257, 2 });
    const auto anchorParams = makeParameters(anchorAttackMs, false);
    processSilence(anchorReference, anchorParams, warmupSamples);
    const auto event = processPulse(subject, latestParams);
    const auto canonicalEvent = processPulse(anchorReference, anchorParams);
    const float eventError = maximumBufferDifference(event, canonicalEvent);

    const float expectedAfterOne = anchorAttackMs
                                   * std::pow(thirdAttackMs / anchorAttackMs,
                                              1.0f
                                                  / static_cast<float>(transitionSamples));
    const float actualAfterOne =
        subject.compressorAttackBaseSmoother.getCurrentValue();
    processSilence(subject, latestParams, transitionSamples - 1);
    const float finalCurrent =
        subject.compressorAttackBaseSmoother.getCurrentValue();
    const float finalTarget =
        subject.compressorAttackBaseSmoother.getTargetValue();
    CAPTURE(anchorAttackMs,
            eventError,
            expectedAfterOne,
            actualAfterOne,
            finalCurrent,
            finalTarget);
    CHECK(eventError <= 1.0e-6f);
    CHECK(actualAfterOne == Catch::Approx(expectedAfterOne).margin(1.0e-5f));
    CHECK(finalCurrent == Catch::Approx(thirdAttackMs).margin(1.0e-4f));
    CHECK(finalTarget == Catch::Approx(thirdAttackMs).margin(1.0e-6f));
}

TEST_CASE("Reset discards a partial Compressor Attack transition",
          "[band][compressor][attack][automation][transition][reset]")
{
    const auto initialParams = makeParameters(oldAttackMs, false);
    const auto targetParams = makeParameters(newAttackMs, false);
    const juce::dsp::ProcessSpec spec { sampleRate, 257, 2 };
    BandProcessor subject;
    BandProcessor sameResetReference;
    subject.prepare(spec);
    sameResetReference.prepare(spec);
    processSilence(subject, initialParams, warmupSamples);
    processSilence(subject, targetParams, 160);

    // Prime the fresh reference's persistent non-compressor targets (notably
    // direct Band Gain) before both objects enter the same reset lifecycle.
    processSilence(sameResetReference, targetParams, 1);
    subject.reset();
    sameResetReference.reset();
    const auto afterReset = processPulse(subject, targetParams);
    const auto canonical = processPulse(sameResetReference, targetParams);
    const float resetError = maximumBufferDifference(afterReset, canonical);
    CAPTURE(resetError,
            subject.compressorAttackBaseSmoother.getCurrentValue(),
            sameResetReference.compressorAttackBaseSmoother.getCurrentValue());
    CHECK(resetError <= 1.0e-6f);
    CHECK(subject.compressorAttackBaseSmoother.getCurrentValue()
          == Catch::Approx(newAttackMs).margin(1.0e-6f));
}
