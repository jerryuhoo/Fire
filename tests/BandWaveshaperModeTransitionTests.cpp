#include <PluginProcessor.h>
#include <DSP/DistortionLogic.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int warmupSamples = 4800;
constexpr int transitionSamples = 480; // 10 ms at the base sample rate.
constexpr int transitionGuardSamples = 96;
constexpr int finalComparisonSamples = 1024;
constexpr float endpointTolerance = 2.0e-4f;

struct ModeEvent
{
    int sample = 0;
    int mode = 3;
};

struct Timeline
{
    std::array<std::vector<float>, 2> output;
    int numChannels = 2;
    bool finite = true;
};

struct BlendFit
{
    float projectedMix = 0.0f;
    float normalisedResidual = std::numeric_limits<float>::infinity();
    float endpointEnergy = 0.0f;
};

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double phase = channel == 0 ? 0.17 : 0.69;
    const float dc = channel == 0 ? 0.46f : -0.41f;
    return dc
         + 0.16f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 617.0 * time + phase))
         + 0.055f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 1901.0 * time
                   + 0.31 - phase));
}

BandProcessingParameters makeParameters(int mode, bool useHq)
{
    BandProcessingParameters params;
    params.mode = mode;
    params.isHQ = useHq;
    params.isBandEnabled = true;
    params.isDriveEnabled = false;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.isSafeModeOn = false;
    params.isExtremeModeOn = false;
    params.driveVal.baseValue = 0.0f;
    params.driveVal.range = { 0.0f, 100.0f };
    params.biasVal.baseValue = 0.0f;
    params.biasVal.range = { -1.0f, 1.0f };
    params.recVal.baseValue = 0.0f;
    params.recVal.range = { 0.0f, 1.0f };
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.shapeMixVal = 1.0f;
    params.shapeMixValProvider.baseValue = 1.0f;
    params.compMixVal = 0.0f;
    params.compMixValProvider.baseValue = 0.0f;
    params.widthMixVal = 0.0f;
    params.widthMixValProvider.baseValue = 0.0f;
    return params;
}

Timeline render(int initialMode,
                bool useHq,
                const std::vector<ModeEvent>& events,
                const std::vector<int>& hostBlockPattern,
                int totalSamples,
                int numChannels = 2,
                int processorBlockSize = preparedBlockSize)
{
    REQUIRE(initialMode >= 0);
    REQUIRE(initialMode <= 11);
    REQUIRE(numChannels >= 1);
    REQUIRE(numChannels <= 2);
    REQUIRE(processorBlockSize > 0);
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    BandProcessor processor;
    processor.prepare({ sampleRate,
                        static_cast<juce::uint32>(processorBlockSize),
                        static_cast<juce::uint32>(numChannels) });
    processor.gain.setRampDurationSeconds(0.0);
    processor.gain.setGainDecibels(0.0f);
    processor.gain.setRampDurationSeconds(0.05);

    Timeline result;
    result.numChannels = numChannels;
    for (int channel = 0; channel < numChannels; ++channel)
        result.output[static_cast<size_t>(channel)].reserve(
            static_cast<size_t>(totalSamples));

    int currentMode = initialMode;
    int streamPosition = 0;
    size_t eventIndex = 0;
    size_t blockIndex = 0;
    while (streamPosition < totalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].sample == streamPosition)
        {
            currentMode = events[eventIndex].mode;
            REQUIRE(currentMode >= 0);
            REQUIRE(currentMode <= 11);
            ++eventIndex;
        }

        const int nextEvent = eventIndex < events.size()
                                  ? events[eventIndex].sample
                                  : totalSamples;
        REQUIRE(nextEvent > streamPosition);
        const int samplesThisBlock = std::min(
            hostBlockPattern[blockIndex % hostBlockPattern.size()],
            std::min(totalSamples - streamPosition,
                     nextEvent - streamPosition));
        ++blockIndex;
        REQUIRE(samplesThisBlock > 0);

        juce::AudioBuffer<float> buffer(numChannels, samplesThisBlock);
        juce::AudioBuffer<float> lfoOutputs(4, samplesThisBlock);
        lfoOutputs.clear();
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));

        processor.process(buffer,
                          makeParameters(currentMode, useHq),
                          lfoOutputs);
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& destination = result.output[static_cast<size_t>(channel)];
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                destination.push_back(value);
            }
        }
        streamPosition += samplesThisBlock;
    }

    REQUIRE(eventIndex == events.size());
    return result;
}

float maximumDifference(const Timeline& first,
                        const Timeline& second,
                        int firstSample,
                        int numSamples)
{
    REQUIRE(first.numChannels == second.numChannels);
    float maximum = 0.0f;
    for (int channelIndex = 0;
         channelIndex < first.numChannels;
         ++channelIndex)
    {
        const auto channel = static_cast<size_t>(channelIndex);
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        REQUIRE(firstSample >= 0);
        REQUIRE(firstSample + numSamples
                <= static_cast<int>(first.output[channel].size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            maximum = std::max(
                maximum,
                std::abs(first.output[channel][static_cast<size_t>(sample)]
                         - second.output[channel][static_cast<size_t>(sample)]));
        }
    }
    return maximum;
}

float maximumMagnitude(const Timeline& timeline)
{
    float maximum = 0.0f;
    for (int channelIndex = 0;
         channelIndex < timeline.numChannels;
         ++channelIndex)
        for (const float value : timeline.output[static_cast<size_t>(channelIndex)])
            maximum = std::max(maximum, std::abs(value));
    return maximum;
}

BlendFit fitLinearBlend(const Timeline& subject,
                        const Timeline& fromReference,
                        const Timeline& toReference,
                        int firstSample,
                        int numSamples)
{
    REQUIRE(subject.numChannels == fromReference.numChannels);
    REQUIRE(subject.numChannels == toReference.numChannels);
    double endpointEnergy = 0.0;
    double projection = 0.0;
    for (int channelIndex = 0;
         channelIndex < subject.numChannels;
         ++channelIndex)
    {
        const auto channel = static_cast<size_t>(channelIndex);
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            const auto index = static_cast<size_t>(sample);
            const double endpointDelta = toReference.output[channel][index]
                                       - fromReference.output[channel][index];
            const double subjectDelta = subject.output[channel][index]
                                      - fromReference.output[channel][index];
            endpointEnergy += endpointDelta * endpointDelta;
            projection += subjectDelta * endpointDelta;
        }
    }

    BlendFit result;
    result.endpointEnergy = static_cast<float>(endpointEnergy);
    if (endpointEnergy <= std::numeric_limits<double>::epsilon())
        return result;

    result.projectedMix = static_cast<float>(projection / endpointEnergy);
    double residualEnergy = 0.0;
    for (int channelIndex = 0;
         channelIndex < subject.numChannels;
         ++channelIndex)
    {
        const auto channel = static_cast<size_t>(channelIndex);
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            const auto index = static_cast<size_t>(sample);
            const double endpointDelta = toReference.output[channel][index]
                                       - fromReference.output[channel][index];
            const double subjectDelta = subject.output[channel][index]
                                      - fromReference.output[channel][index];
            const double residual = subjectDelta
                                  - static_cast<double>(result.projectedMix)
                                        * endpointDelta;
            residualEnergy += residual * residual;
        }
    }
    result.normalisedResidual = static_cast<float>(
        std::sqrt(residualEnergy / endpointEnergy));
    return result;
}

void requireFiniteAndBounded(const Timeline& timeline)
{
    REQUIRE(timeline.finite);
    const float peak = maximumMagnitude(timeline);
    CAPTURE(peak);
    REQUIRE(peak < 2.0f);
}

void checkTransition(int initialMode,
                     int targetMode,
                     bool useHq,
                     int numChannels)
{
    const int totalSamples = warmupSamples
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 16;
    const std::vector<int> pattern { preparedBlockSize };
    const auto subject = render(initialMode,
                                useHq,
                                { { warmupSamples, targetMode } },
                                pattern,
                                totalSamples,
                                numChannels);
    const auto fromReference = render(initialMode,
                                      useHq,
                                      {},
                                      pattern,
                                      totalSamples,
                                      numChannels);
    const auto toReference = render(targetMode,
                                    useHq,
                                    {},
                                    pattern,
                                    totalSamples,
                                    numChannels);
    requireFiniteAndBounded(subject);
    requireFiniteAndBounded(fromReference);
    requireFiniteAndBounded(toReference);

    const float initialError = maximumDifference(subject,
                                                 fromReference,
                                                 128,
                                                 warmupSamples - 256);
    const float firstOldError = maximumDifference(subject,
                                                  fromReference,
                                                  warmupSamples,
                                                  1);
    const float firstEndpointSeparation = maximumDifference(fromReference,
                                                            toReference,
                                                            warmupSamples,
                                                            1);
    const auto middle = fitLinearBlend(subject,
                                       fromReference,
                                       toReference,
                                       warmupSamples
                                           + transitionSamples / 2 - 8,
                                       17);
    const int finalStateStart = warmupSamples
                              + transitionSamples
                              + transitionGuardSamples;
    const float finalEndpointSeparation = maximumDifference(fromReference,
                                                            toReference,
                                                            finalStateStart,
                                                            finalComparisonSamples);
    const float finalError = maximumDifference(subject,
                                                toReference,
                                                finalStateStart,
                                                finalComparisonSamples);
    CAPTURE(initialMode,
            targetMode,
            useHq,
            numChannels,
            initialError,
            firstOldError,
            firstEndpointSeparation,
            middle.endpointEnergy,
            middle.projectedMix,
            middle.normalisedResidual,
            finalEndpointSeparation,
            finalError);
    CHECK(initialError < endpointTolerance);
    REQUIRE(firstEndpointSeparation > 0.05f);
    CHECK(firstOldError < endpointTolerance);
    REQUIRE(middle.endpointEnergy > 0.01f);
    CHECK(std::abs(middle.projectedMix - 0.5f) < 0.10f);
    CHECK(middle.normalisedResidual < 0.25f);
    REQUIRE(finalEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkHostPartition(int initialMode, int targetMode, bool useHq)
{
    const int totalSamples = warmupSamples
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 16;
    const std::vector<ModeEvent> events {
        { warmupSamples, targetMode }
    };
    const auto fixed = render(initialMode,
                              useHq,
                              events,
                              { preparedBlockSize },
                              totalSamples);
    const auto irregular = render(initialMode,
                                  useHq,
                                  events,
                                  { 7, 113, 19, 251, 37, 83, 2, 173 },
                                  totalSamples);
    requireFiniteAndBounded(fixed);
    requireFiniteAndBounded(irregular);
    const float partitionError = maximumDifference(fixed,
                                                   irregular,
                                                   0,
                                                   totalSamples);
    CAPTURE(initialMode, targetMode, useHq, partitionError);
    CHECK(partitionError < endpointTolerance);
}

void checkRapidReverse(int initialMode, int temporaryMode, bool useHq)
{
    constexpr int reverseSample = warmupSamples + 160;
    const int totalSamples = reverseSample
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 16;
    const std::vector<int> pattern { preparedBlockSize };
    const auto subject = render(initialMode,
                                useHq,
                                { { warmupSamples, temporaryMode },
                                  { reverseSample, initialMode } },
                                pattern,
                                totalSamples);
    const auto continuing = render(initialMode,
                                   useHq,
                                   { { warmupSamples, temporaryMode } },
                                   pattern,
                                   totalSamples);
    const auto restoredReference = render(initialMode,
                                          useHq,
                                          {},
                                          pattern,
                                          totalSamples);
    const auto temporaryReference = render(temporaryMode,
                                           useHq,
                                           {},
                                           pattern,
                                           totalSamples);
    requireFiniteAndBounded(subject);
    requireFiniteAndBounded(continuing);
    requireFiniteAndBounded(restoredReference);

    const float reverseContinuityError = maximumDifference(subject,
                                                           continuing,
                                                           reverseSample,
                                                           1);
    const float reverseSeparation = maximumDifference(continuing,
                                                      restoredReference,
                                                      reverseSample,
                                                      1);
    const int finalStateStart = reverseSample
                              + transitionSamples
                              + transitionGuardSamples;
    const float finalError = maximumDifference(subject,
                                                restoredReference,
                                                finalStateStart,
                                                finalComparisonSamples);
    const float finalEndpointSeparation = maximumDifference(restoredReference,
                                                            temporaryReference,
                                                            finalStateStart,
                                                            finalComparisonSamples);
    CAPTURE(initialMode,
            temporaryMode,
            useHq,
            reverseContinuityError,
            reverseSeparation,
            finalError,
            finalEndpointSeparation);
    REQUIRE(reverseSeparation > 1.0e-3f);
    CHECK(reverseContinuityError < endpointTolerance);
    REQUIRE(finalEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkQueuedLatestMode(bool useHq)
{
    constexpr int initialMode = 3; // Cubic
    constexpr int firstTarget = 8; // Limit
    constexpr int queuedTarget = 2; // Tanh
    constexpr int latestTarget = 4; // Hard clip
    constexpr int queuedSample = warmupSamples + 160;
    constexpr int latestSample = warmupSamples + 240;
    const int totalSamples = warmupSamples
                           + 2 * transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 16;
    const std::vector<ModeEvent> events {
        { warmupSamples, firstTarget },
        { queuedSample, queuedTarget },
        { latestSample, latestTarget }
    };
    const std::vector<int> fixedPattern { preparedBlockSize };
    const std::vector<int> irregularPattern {
        7, 113, 19, 251, 37, 83, 2, 173
    };
    const auto fixed = render(initialMode,
                              useHq,
                              events,
                              fixedPattern,
                              totalSamples);
    const auto irregular = render(initialMode,
                                  useHq,
                                  events,
                                  irregularPattern,
                                  totalSamples);
    const auto continuingFirstTarget = render(
        initialMode,
        useHq,
        { { warmupSamples, firstTarget } },
        fixedPattern,
        totalSamples);
    const auto latestReference = render(latestTarget,
                                        useHq,
                                        {},
                                        fixedPattern,
                                        totalSamples);
    const auto staleQueuedReference = render(queuedTarget,
                                             useHq,
                                             {},
                                             fixedPattern,
                                             totalSamples);
    requireFiniteAndBounded(fixed);
    requireFiniteAndBounded(irregular);
    requireFiniteAndBounded(continuingFirstTarget);

    const float queuedContinuityError = maximumDifference(
        fixed,
        continuingFirstTarget,
        queuedSample,
        1);
    const float latestContinuityError = maximumDifference(
        fixed,
        continuingFirstTarget,
        latestSample,
        1);
    const float firstEndpointError = maximumDifference(
        fixed,
        continuingFirstTarget,
        warmupSamples + transitionSamples,
        1);
    const int finalStateStart = warmupSamples
                              + 2 * transitionSamples
                              + transitionGuardSamples;
    const float finalError = maximumDifference(fixed,
                                                latestReference,
                                                finalStateStart,
                                                finalComparisonSamples);
    const float staleEndpointSeparation = maximumDifference(
        latestReference,
        staleQueuedReference,
        finalStateStart,
        finalComparisonSamples);
    const float partitionError = maximumDifference(fixed,
                                                   irregular,
                                                   0,
                                                   totalSamples);
    CAPTURE(useHq,
            queuedContinuityError,
            latestContinuityError,
            firstEndpointError,
            finalError,
            staleEndpointSeparation,
            partitionError);
    CHECK(queuedContinuityError < endpointTolerance);
    CHECK(latestContinuityError < endpointTolerance);
    CHECK(firstEndpointError < endpointTolerance);
    REQUIRE(staleEndpointSeparation > 0.02f);
    CHECK(finalError < endpointTolerance);
    CHECK(partitionError < endpointTolerance);
}

void checkInternalChunking(bool useHq, int numChannels)
{
    const int totalSamples = warmupSamples
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 16;
    const std::vector<ModeEvent> events {
        { warmupSamples, 8 }
    };
    const auto fullCapacity = render(3,
                                     useHq,
                                     events,
                                     { 480 },
                                     totalSamples,
                                     numChannels,
                                     480);
    const auto chunked = render(3,
                                useHq,
                                events,
                                { 480 },
                                totalSamples,
                                numChannels,
                                64);
    requireFiniteAndBounded(fullCapacity);
    requireFiniteAndBounded(chunked);
    const float chunkError = maximumDifference(fullCapacity,
                                               chunked,
                                               0,
                                               totalSamples);
    CAPTURE(useHq, numChannels, chunkError);
    CHECK(chunkError < endpointTolerance);
}

void checkStaticModeCompatibility(int mode, int numChannels)
{
    constexpr int totalSamples = 2048;
    const auto subject = render(mode,
                                false,
                                {},
                                { preparedBlockSize },
                                totalSamples,
                                numChannels);
    requireFiniteAndBounded(subject);

    DistortionLogic::State state;
    state.drive = 1.0f;
    state.bias = 0.0f;
    state.rec = 0.0f;
    state.mode = mode;
    float maximumError = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto channelIndex = static_cast<size_t>(channel);
        for (int sample = 0; sample < totalSamples; ++sample)
        {
            const float expected = DistortionLogic::processSample(
                inputSample(channel, sample), state);
            maximumError = std::max(
                maximumError,
                std::abs(subject.output[channelIndex][static_cast<size_t>(sample)]
                         - expected));
        }
    }
    CAPTURE(mode, numChannels, maximumError);
    CHECK(maximumError < 1.0e-5f);
}
} // namespace

TEST_CASE("Per-band waveshaper modes crossfade over ten base-rate milliseconds",
          "[band][waveshaper][mode][transition]")
{
    for (const bool useHq : { false, true })
        for (const int numChannels : { 1, 2 })
            for (const bool cubicToLimit : { true, false })
            {
                const int initialMode = cubicToLimit ? 3 : 8;
                const int targetMode = cubicToLimit ? 8 : 3;
                DYNAMIC_SECTION("mode " << initialMode << "->" << targetMode
                                         << ", HQ=" << useHq
                                         << ", channels=" << numChannels)
                {
                    checkTransition(initialMode,
                                    targetMode,
                                    useHq,
                                    numChannels);
                }
            }
}

TEST_CASE("Waveshaper-mode transitions ignore host callback partitioning",
          "[band][waveshaper][mode][transition][block-size]")
{
    for (const bool useHq : { false, true })
        for (const bool cubicToLimit : { true, false })
        {
            const int initialMode = cubicToLimit ? 3 : 8;
            const int targetMode = cubicToLimit ? 8 : 3;
            DYNAMIC_SECTION("mode " << initialMode << "->" << targetMode
                                     << ", HQ=" << useHq)
            {
                checkHostPartition(initialMode, targetMode, useHq);
            }
        }
}

TEST_CASE("Rapid waveshaper-mode reversals continue from the audible blend",
          "[band][waveshaper][mode][transition][rapid]")
{
    for (const bool useHq : { false, true })
        for (const bool cubicToLimit : { true, false })
        {
            const int initialMode = cubicToLimit ? 3 : 8;
            const int temporaryMode = cubicToLimit ? 8 : 3;
            DYNAMIC_SECTION("mode " << initialMode << "->"
                                     << temporaryMode << "->"
                                     << initialMode << ", HQ=" << useHq)
            {
                checkRapidReverse(initialMode, temporaryMode, useHq);
            }
        }
}

TEST_CASE("A third waveshaper mode queues safely and the latest request wins",
          "[band][waveshaper][mode][transition][queued][block-size]")
{
    for (const bool useHq : { false, true })
    {
        DYNAMIC_SECTION("HQ=" << useHq)
        {
            checkQueuedLatestMode(useHq);
        }
    }
}

TEST_CASE("Waveshaper transitions survive oversized internal chunking",
          "[band][waveshaper][mode][transition][oversized][chunk]")
{
    for (const bool useHq : { false, true })
        for (const int numChannels : { 1, 2 })
        {
            DYNAMIC_SECTION("HQ=" << useHq
                                   << ", channels=" << numChannels)
            {
                checkInternalChunking(useHq, numChannels);
            }
        }
}

TEST_CASE("All static waveshaper modes retain their legacy transfer functions",
          "[band][waveshaper][mode][compatibility]")
{
    for (int mode = 0; mode < 12; ++mode)
        for (const int numChannels : { 1, 2 })
        {
            DYNAMIC_SECTION("mode=" << mode
                                     << ", channels=" << numChannels)
            {
                checkStaticModeCompatibility(mode, numChannels);
            }
        }
}
