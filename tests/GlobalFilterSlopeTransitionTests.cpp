#include "../Source/PluginProcessor.h"

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
constexpr int warmupSamples = 12000;
constexpr int transitionSamples = 480; // 10 ms
constexpr int transitionGuardSamples = 96;
constexpr int finalComparisonSamples = 2048;
constexpr float endpointTolerance = 2.0e-4f;

enum class CutStage
{
    low,
    high
};

struct SlopeEvent
{
    int sample = 0;
    int slopeIndex = 0;
};

struct StageBypassEvent
{
    int sample = 0;
    bool bypassed = false;
};

struct Timeline
{
    std::array<std::vector<float>, 2> output;
    bool finite = true;
    bool latencyInvariant = true;
    int reportedLatency = 0;
};

struct EndpointPoint
{
    int sample = 0;
    int channel = 0;
    float endpointSeparation = 0.0f;
    float distanceFrom = 0.0f;
    float distanceTo = 0.0f;
};

struct BlendFit
{
    float projectedMix = 0.0f;
    float normalisedResidual = std::numeric_limits<float>::infinity();
    float endpointEnergy = 0.0f;
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(
        parameter->getNormalisableRange().convertTo0to1(plainValue));
}

const char* slopeParameter(CutStage stage)
{
    return stage == CutStage::low ? LOWCUT_SLOPE_ID : HIGHCUT_SLOPE_ID;
}

const char* bypassParameter(CutStage stage)
{
    return stage == CutStage::low ? LOWCUT_BYPASSED_ID
                                  : HIGHCUT_BYPASSED_ID;
}

const char* stageName(CutStage stage)
{
    return stage == CutStage::low ? "LowCut" : "HighCut";
}

int slopeDecibelsPerOctave(int slopeIndex)
{
    return 12 * (slopeIndex + 1);
}

void configureProcessor(FireAudioProcessor& processor,
                        CutStage stage,
                        int slopeIndex,
                        bool useHq)
{
    REQUIRE(slopeIndex >= 0);
    REQUIRE(slopeIndex <= 3);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      0.0f);

    setPlainParameter(processor, LOWCUT_BYPASSED_ID,
                      stage == CutStage::low ? 0.0f : 1.0f);
    setPlainParameter(processor, PEAK_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, HIGHCUT_BYPASSED_ID,
                      stage == CutStage::high ? 0.0f : 1.0f);

    // The Q/gain companion stages share the cut bypass flag. Zero gain makes
    // those biquads unity, so the fixture isolates only the Butterworth slope.
    setPlainParameter(processor, LOWCUT_FREQ_ID, 3200.0f);
    setPlainParameter(processor, LOWCUT_GAIN_ID, 0.0f);
    setPlainParameter(processor, LOWCUT_Q_ID, 1.0f);
    setPlainParameter(processor, LOWCUT_SLOPE_ID,
                      stage == CutStage::low
                          ? static_cast<float>(slopeIndex)
                          : 0.0f);
    setPlainParameter(processor, HIGHCUT_FREQ_ID, 2600.0f);
    setPlainParameter(processor, HIGHCUT_GAIN_ID, 0.0f);
    setPlainParameter(processor, HIGHCUT_Q_ID, 1.0f);
    setPlainParameter(processor, HIGHCUT_SLOPE_ID,
                      stage == CutStage::high
                          ? static_cast<float>(slopeIndex)
                          : 0.0f);
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float probeSample(CutStage stage, int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = channel == 0 ? 0.19 : 0.71;
    const double primaryFrequency = stage == CutStage::low ? 1800.0 : 4600.0;
    const double secondaryFrequency = stage == CutStage::low ? 6100.0 : 631.0;
    return 0.46f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * primaryFrequency * time
                   + channelPhase))
         + 0.13f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * secondaryFrequency * time
                   + 0.37 - 0.3 * channelPhase));
}

Timeline render(CutStage stage,
                int initialSlope,
                bool useHq,
                const std::vector<SlopeEvent>& events,
                const std::vector<int>& blockPattern,
                int totalSamples,
                const std::vector<StageBypassEvent>& bypassEvents = {})
{
    REQUIRE_FALSE(blockPattern.empty());
    for (const int blockSize : blockPattern)
    {
        REQUIRE(blockSize > 0);
        REQUIRE(blockSize <= preparedBlockSize);
    }

    FireAudioProcessor processor;
    configureProcessor(processor, stage, initialSlope, useHq);

    Timeline result;
    result.reportedLatency = processor.getLatencySamples();
    REQUIRE(result.reportedLatency > 0);
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(totalSamples));

    int streamPosition = 0;
    size_t blockIndex = 0;
    size_t eventIndex = 0;
    size_t bypassEventIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].sample == streamPosition)
        {
            setPlainParameter(processor,
                              slopeParameter(stage),
                              static_cast<float>(events[eventIndex].slopeIndex));
            ++eventIndex;
        }

        while (bypassEventIndex < bypassEvents.size()
               && bypassEvents[bypassEventIndex].sample == streamPosition)
        {
            setPlainParameter(
                processor,
                bypassParameter(stage),
                bypassEvents[bypassEventIndex].bypassed ? 1.0f : 0.0f);
            ++bypassEventIndex;
        }

        const int nextSlopeEvent = eventIndex < events.size()
                                       ? events[eventIndex].sample
                                       : totalSamples;
        const int nextBypassEvent = bypassEventIndex < bypassEvents.size()
                                        ? bypassEvents[bypassEventIndex].sample
                                        : totalSamples;
        const int nextEvent = std::min(nextSlopeEvent, nextBypassEvent);
        REQUIRE(nextEvent > streamPosition);
        const int samplesThisBlock = std::min(
            blockPattern[blockIndex % blockPattern.size()],
            std::min(totalSamples - streamPosition,
                     nextEvent - streamPosition));
        ++blockIndex;

        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 probeSample(stage,
                                             channel,
                                             streamPosition + sample));
        processor.processBlock(buffer, midi);
        result.latencyInvariant = result.latencyInvariant
                               && processor.getLatencySamples()
                                      == result.reportedLatency;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
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
    REQUIRE(bypassEventIndex == bypassEvents.size());
    return result;
}

Timeline renderExistingProcessor(FireAudioProcessor& processor,
                                 CutStage stage,
                                 int firstStreamSample,
                                 int totalSamples,
                                 const std::vector<int>& blockPattern)
{
    REQUIRE(totalSamples > 0);
    REQUIRE_FALSE(blockPattern.empty());
    for (const int blockSize : blockPattern)
    {
        REQUIRE(blockSize > 0);
        REQUIRE(blockSize <= preparedBlockSize);
    }

    Timeline result;
    result.reportedLatency = processor.getLatencySamples();
    REQUIRE(result.reportedLatency > 0);
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(totalSamples));

    int processedSamples = 0;
    size_t blockIndex = 0;
    juce::MidiBuffer midi;
    while (processedSamples < totalSamples)
    {
        const int samplesThisBlock = std::min(
            blockPattern[blockIndex % blockPattern.size()],
            totalSamples - processedSamples);
        ++blockIndex;

        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(
                    channel,
                    sample,
                    probeSample(stage,
                                channel,
                                firstStreamSample + processedSamples + sample));
        processor.processBlock(buffer, midi);
        result.latencyInvariant = result.latencyInvariant
                               && processor.getLatencySamples()
                                      == result.reportedLatency;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto& destination = result.output[static_cast<size_t>(channel)];
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                destination.push_back(value);
            }
        }
        processedSamples += samplesThisBlock;
    }

    return result;
}

float maximumDifference(const Timeline& first,
                        const Timeline& second,
                        int firstSample,
                        int numSamples)
{
    float maximum = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
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

float maximumMagnitude(const Timeline& timeline,
                       int firstSample,
                       int numSamples)
{
    float maximum = 0.0f;
    for (const auto& channel : timeline.output)
    {
        REQUIRE(firstSample >= 0);
        REQUIRE(firstSample + numSamples <= static_cast<int>(channel.size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
            maximum = std::max(maximum,
                               std::abs(channel[static_cast<size_t>(sample)]));
    }
    return maximum;
}

EndpointPoint strongestEndpointPoint(const Timeline& subject,
                                     const Timeline& fromReference,
                                     const Timeline& toReference,
                                     int firstSample,
                                     int numSamples)
{
    EndpointPoint result;
    result.endpointSeparation = -std::numeric_limits<float>::infinity();
    for (size_t channel = 0; channel < subject.output.size(); ++channel)
    {
        REQUIRE(subject.output[channel].size()
                == fromReference.output[channel].size());
        REQUIRE(subject.output[channel].size()
                == toReference.output[channel].size());
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            const float from = fromReference.output[channel][
                static_cast<size_t>(sample)];
            const float to = toReference.output[channel][
                static_cast<size_t>(sample)];
            const float separation = std::abs(from - to);
            if (separation <= result.endpointSeparation)
                continue;

            const float value = subject.output[channel][
                static_cast<size_t>(sample)];
            result.sample = sample;
            result.channel = static_cast<int>(channel);
            result.endpointSeparation = separation;
            result.distanceFrom = std::abs(value - from);
            result.distanceTo = std::abs(value - to);
        }
    }
    return result;
}

BlendFit fitLinearBlend(const Timeline& subject,
                        const Timeline& fromReference,
                        const Timeline& toReference,
                        int firstSample,
                        int numSamples)
{
    double endpointEnergy = 0.0;
    double projection = 0.0;
    for (size_t channel = 0; channel < subject.output.size(); ++channel)
    {
        REQUIRE(subject.output[channel].size()
                == fromReference.output[channel].size());
        REQUIRE(subject.output[channel].size()
                == toReference.output[channel].size());
        REQUIRE(firstSample >= 0);
        REQUIRE(firstSample + numSamples
                <= static_cast<int>(subject.output[channel].size()));
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
    for (size_t channel = 0; channel < subject.output.size(); ++channel)
    {
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            const auto index = static_cast<size_t>(sample);
            const double endpointDelta = toReference.output[channel][index]
                                       - fromReference.output[channel][index];
            const double expectedDelta = static_cast<double>(result.projectedMix)
                                       * endpointDelta;
            const double subjectDelta = subject.output[channel][index]
                                      - fromReference.output[channel][index];
            const double residual = subjectDelta - expectedDelta;
            residualEnergy += residual * residual;
        }
    }
    result.normalisedResidual = static_cast<float>(
        std::sqrt(residualEnergy / endpointEnergy));
    return result;
}

void requireFiniteAndBounded(const Timeline& subject,
                             const Timeline& firstReference,
                             const Timeline& secondReference)
{
    REQUIRE(subject.finite);
    REQUIRE(subject.latencyInvariant);
    REQUIRE(firstReference.finite);
    REQUIRE(firstReference.latencyInvariant);
    REQUIRE(secondReference.finite);
    REQUIRE(secondReference.latencyInvariant);
    REQUIRE(subject.reportedLatency == firstReference.reportedLatency);
    REQUIRE(subject.reportedLatency == secondReference.reportedLatency);

    const int totalSamples = static_cast<int>(subject.output.front().size());
    const float subjectPeak = maximumMagnitude(subject, 0, totalSamples);
    const float endpointPeak = std::max(
        maximumMagnitude(firstReference, 0, totalSamples),
        maximumMagnitude(secondReference, 0, totalSamples));
    CAPTURE(subjectPeak, endpointPeak);
    CHECK(subjectPeak <= endpointPeak * 1.15f + 0.02f);
    CHECK(subjectPeak < 1.5f);
}

void checkSlopeTransition(CutStage stage,
                          int initialSlope,
                          int targetSlope,
                          bool useHq)
{
    const int totalSamples = warmupSamples
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 32;
    const std::vector<SlopeEvent> events {
        { warmupSamples, targetSlope }
    };
    const std::vector<int> pattern { preparedBlockSize };
    const auto subject = render(stage,
                                initialSlope,
                                useHq,
                                events,
                                pattern,
                                totalSamples);
    const auto fromReference = render(stage,
                                      initialSlope,
                                      useHq,
                                      {},
                                      pattern,
                                      totalSamples);
    const auto toReference = render(stage,
                                    targetSlope,
                                    useHq,
                                    {},
                                    pattern,
                                    totalSamples);
    requireFiniteAndBounded(subject, fromReference, toReference);

    // The global filter is after the HQ oversampler, so an HQ slope event is
    // audible immediately. Base mode receives the fixed host-PDC pad after
    // the filter and therefore exposes the same event D samples later.
    const int eventOutputOffset = useHq ? 0 : subject.reportedLatency;
    const int firstAudibleSample = warmupSamples + eventOutputOffset;
    const float initialError = maximumDifference(
        subject,
        fromReference,
        subject.reportedLatency + 64,
        warmupSamples - subject.reportedLatency - 128);
    const auto first = strongestEndpointPoint(subject,
                                              fromReference,
                                              toReference,
                                              firstAudibleSample,
                                              1);
    const auto middle = strongestEndpointPoint(
        subject,
        fromReference,
        toReference,
        firstAudibleSample + transitionSamples / 2 - 32,
        65);
    const auto middleBlend = fitLinearBlend(
        subject,
        fromReference,
        toReference,
        firstAudibleSample + transitionSamples / 2 - 8,
        17);
    const int finalStateStart = firstAudibleSample
                              + transitionSamples
                              + transitionGuardSamples;
    const float finalEndpointSeparation = maximumDifference(
        fromReference,
        toReference,
        finalStateStart,
        finalComparisonSamples);
    const float finalError = maximumDifference(subject,
                                                toReference,
                                                finalStateStart,
                                                finalComparisonSamples);
    CAPTURE(stageName(stage),
            initialSlope,
            targetSlope,
            useHq,
            subject.reportedLatency,
            initialError,
            first.sample,
            first.channel,
            first.endpointSeparation,
            first.distanceFrom,
            first.distanceTo,
            middle.sample,
            middle.channel,
            middle.endpointSeparation,
            middle.distanceFrom,
            middle.distanceTo,
            middleBlend.endpointEnergy,
            middleBlend.projectedMix,
            middleBlend.normalisedResidual,
            finalEndpointSeparation,
            finalError);
    CHECK(initialError < endpointTolerance);
    REQUIRE(first.endpointSeparation > 1.0e-3f);
    CHECK(first.distanceFrom < endpointTolerance);
    CHECK(first.distanceTo > first.endpointSeparation * 0.50f);
    REQUIRE(middle.endpointSeparation > 0.02f);
    CHECK(middle.distanceFrom > middle.endpointSeparation * 0.05f);
    CHECK(middle.distanceTo > middle.endpointSeparation * 0.05f);
    REQUIRE(middleBlend.endpointEnergy > 1.0e-3f);
    CHECK(std::abs(middleBlend.projectedMix - 0.5f) < 0.22f);
    CHECK(middleBlend.normalisedResidual < 0.30f);
    REQUIRE(finalEndpointSeparation > 0.02f);
    CHECK(finalError < endpointTolerance);
}

void checkRapidReverse(CutStage stage,
                       int initialSlope,
                       int temporarySlope,
                       bool useHq)
{
    constexpr int reverseSample = warmupSamples + 160;
    const int totalSamples = reverseSample
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 32;
    const std::vector<int> pattern { preparedBlockSize };
    const std::vector<SlopeEvent> redirectedEvents {
        { warmupSamples, temporarySlope },
        { reverseSample, initialSlope }
    };
    const std::vector<SlopeEvent> continuingEvents {
        { warmupSamples, temporarySlope }
    };
    const auto subject = render(stage,
                                initialSlope,
                                useHq,
                                redirectedEvents,
                                pattern,
                                totalSamples);
    const auto continuing = render(stage,
                                   initialSlope,
                                   useHq,
                                   continuingEvents,
                                   pattern,
                                   totalSamples);
    const auto restoredReference = render(stage,
                                          initialSlope,
                                          useHq,
                                          {},
                                          pattern,
                                          totalSamples);
    const auto temporaryReference = render(stage,
                                           temporarySlope,
                                           useHq,
                                           {},
                                           pattern,
                                           totalSamples);
    requireFiniteAndBounded(subject,
                            restoredReference,
                            temporaryReference);
    REQUIRE(continuing.finite);
    REQUIRE(continuing.latencyInvariant);

    const int eventOutputOffset = useHq ? 0 : subject.reportedLatency;
    const int firstAudibleReverse = reverseSample + eventOutputOffset;
    const auto first = strongestEndpointPoint(subject,
                                              continuing,
                                              restoredReference,
                                              firstAudibleReverse,
                                              1);
    const int finalStateStart = firstAudibleReverse
                              + transitionSamples
                              + transitionGuardSamples;
    const float finalError = maximumDifference(subject,
                                                restoredReference,
                                                finalStateStart,
                                                finalComparisonSamples);
    const float endpointSeparation = maximumDifference(
        restoredReference,
        temporaryReference,
        finalStateStart,
        finalComparisonSamples);
    CAPTURE(stageName(stage),
            initialSlope,
            temporarySlope,
            useHq,
            subject.reportedLatency,
            first.sample,
            first.channel,
            first.endpointSeparation,
            first.distanceFrom,
            first.distanceTo,
            endpointSeparation,
            finalError);
    REQUIRE(first.endpointSeparation > 1.0e-3f);
    CHECK(first.distanceFrom < endpointTolerance);
    CHECK(first.distanceTo > first.endpointSeparation * 0.25f);
    REQUIRE(endpointSeparation > 0.02f);
    CHECK(finalError < endpointTolerance);
}

void checkHostPartition(CutStage stage,
                        int initialSlope,
                        int targetSlope,
                        bool useHq)
{
    const int totalSamples = warmupSamples
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 32;
    const std::vector<SlopeEvent> events {
        { warmupSamples, targetSlope }
    };
    const std::vector<int> fixedPattern { preparedBlockSize };
    const std::vector<int> irregularPattern {
        7, 113, 19, 251, 37, 83, 2, 173
    };
    const auto fixed = render(stage,
                              initialSlope,
                              useHq,
                              events,
                              fixedPattern,
                              totalSamples);
    const auto irregular = render(stage,
                                  initialSlope,
                                  useHq,
                                  events,
                                  irregularPattern,
                                  totalSamples);
    const auto targetReference = render(stage,
                                        targetSlope,
                                        useHq,
                                        {},
                                        fixedPattern,
                                        totalSamples);
    requireFiniteAndBounded(fixed, irregular, targetReference);

    const float partitionError = maximumDifference(fixed,
                                                   irregular,
                                                   0,
                                                   totalSamples);
    const int eventOutputOffset = useHq ? 0 : fixed.reportedLatency;
    const int finalStateStart = warmupSamples
                              + eventOutputOffset
                              + transitionSamples
                              + transitionGuardSamples;
    const float finalError = maximumDifference(fixed,
                                                targetReference,
                                                finalStateStart,
                                                finalComparisonSamples);
    CAPTURE(stageName(stage),
            initialSlope,
            targetSlope,
            useHq,
            fixed.reportedLatency,
            partitionError,
            finalError);
    CHECK(partitionError < endpointTolerance);
    CHECK(finalError < endpointTolerance);
}

void checkQueuedThirdSlope(CutStage stage, bool useHq)
{
    constexpr int initialSlope = 0;
    constexpr int firstTarget = 3;
    constexpr int queuedTarget = 1;
    constexpr int latestTarget = 2;
    constexpr int queuedSample = warmupSamples + 160;
    constexpr int latestSample = warmupSamples + 240;
    const int totalSamples = warmupSamples
                           + 2 * transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 64;
    const std::vector<SlopeEvent> queuedEvents {
        { warmupSamples, firstTarget },
        { queuedSample, queuedTarget },
        { latestSample, latestTarget }
    };
    const std::vector<SlopeEvent> firstTargetOnly {
        { warmupSamples, firstTarget }
    };
    const std::vector<int> fixedPattern { preparedBlockSize };
    const std::vector<int> irregularPattern {
        7, 113, 19, 251, 37, 83, 2, 173
    };
    const auto fixed = render(stage,
                              initialSlope,
                              useHq,
                              queuedEvents,
                              fixedPattern,
                              totalSamples);
    const auto irregular = render(stage,
                                  initialSlope,
                                  useHq,
                                  queuedEvents,
                                  irregularPattern,
                                  totalSamples);
    const auto continuingFirstTarget = render(stage,
                                              initialSlope,
                                              useHq,
                                              firstTargetOnly,
                                              fixedPattern,
                                              totalSamples);
    const auto latestReference = render(stage,
                                        latestTarget,
                                        useHq,
                                        {},
                                        fixedPattern,
                                        totalSamples);
    const auto staleQueuedReference = render(stage,
                                             queuedTarget,
                                             useHq,
                                             {},
                                             fixedPattern,
                                             totalSamples);
    requireFiniteAndBounded(fixed, irregular, latestReference);
    REQUIRE(continuingFirstTarget.finite);
    REQUIRE(continuingFirstTarget.latencyInvariant);
    REQUIRE(staleQueuedReference.finite);
    REQUIRE(staleQueuedReference.latencyInvariant);

    const int eventOutputOffset = useHq ? 0 : fixed.reportedLatency;
    const float queuedContinuityError = maximumDifference(
        fixed,
        continuingFirstTarget,
        queuedSample + eventOutputOffset,
        1);
    const float latestContinuityError = maximumDifference(
        fixed,
        continuingFirstTarget,
        latestSample + eventOutputOffset,
        1);
    const int firstEndpointSample = warmupSamples
                                  + eventOutputOffset
                                  + transitionSamples;
    const float firstEndpointError = maximumDifference(
        fixed,
        continuingFirstTarget,
        firstEndpointSample,
        1);
    const int finalStateStart = warmupSamples
                              + eventOutputOffset
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
    CAPTURE(stageName(stage),
            useHq,
            fixed.reportedLatency,
            queuedContinuityError,
            latestContinuityError,
            firstEndpointError,
            finalError,
            staleEndpointSeparation,
            partitionError);
    CHECK(queuedContinuityError < endpointTolerance);
    CHECK(latestContinuityError < endpointTolerance);
    CHECK(firstEndpointError < endpointTolerance);
    REQUIRE(staleEndpointSeparation > 5.0e-3f);
    CHECK(finalError < endpointTolerance);
    CHECK(partitionError < endpointTolerance);
}

void checkSlopeChangeWhileStageBypassed(CutStage stage, bool useHq)
{
    constexpr int initialSlope = 0;
    constexpr int targetSlope = 3;
    constexpr int fadeOutSample = warmupSamples;
    constexpr int slopeChangeSample = fadeOutSample
                                    + transitionSamples
                                    + transitionGuardSamples;
    constexpr int reenableSample = slopeChangeSample
                                 + transitionSamples
                                 + transitionGuardSamples;
    const int totalSamples = reenableSample
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 32;
    const std::vector<StageBypassEvent> bypassEvents {
        { fadeOutSample, true },
        { reenableSample, false }
    };
    const std::vector<int> pattern { preparedBlockSize };
    const auto subject = render(stage,
                                initialSlope,
                                useHq,
                                { { slopeChangeSample, targetSlope } },
                                pattern,
                                totalSamples,
                                bypassEvents);
    const auto warmTarget = render(stage,
                                   targetSlope,
                                   useHq,
                                   {},
                                   pattern,
                                   totalSamples,
                                   bypassEvents);
    const auto staleOldSlope = render(stage,
                                      initialSlope,
                                      useHq,
                                      {},
                                      pattern,
                                      totalSamples,
                                      bypassEvents);
    requireFiniteAndBounded(subject, warmTarget, staleOldSlope);

    const int eventOutputOffset = useHq ? 0 : subject.reportedLatency;
    const int reenableAudibleSample = reenableSample + eventOutputOffset;
    const int comparisonSamples = transitionSamples
                                + transitionGuardSamples
                                + finalComparisonSamples;
    const float warmTrajectoryError = maximumDifference(subject,
                                                         warmTarget,
                                                         reenableAudibleSample,
                                                         comparisonSamples);
    const int finalStateStart = reenableAudibleSample
                              + transitionSamples
                              + transitionGuardSamples;
    const float oldSlopeSeparation = maximumDifference(warmTarget,
                                                       staleOldSlope,
                                                       finalStateStart,
                                                       finalComparisonSamples);
    CAPTURE(stageName(stage),
            useHq,
            subject.reportedLatency,
            warmTrajectoryError,
            oldSlopeSeparation);
    REQUIRE(oldSlopeSeparation > 0.02f);
    CHECK(warmTrajectoryError < 5.0e-4f);
}

void checkLifecycleSnapsToSlopeEndpoint(CutStage stage,
                                        bool useHq,
                                        bool reprepare)
{
    constexpr int initialSlope = 0;
    constexpr int targetSlope = 3;
    constexpr int lifecycleWarmup = 2048;
    constexpr int halfTransition = transitionSamples / 2;
    constexpr int comparisonSamples = 2048;
    const std::vector<int> pattern { 73, 257, 19, 101 };

    FireAudioProcessor subject;
    configureProcessor(subject, stage, initialSlope, useHq);
    juce::ignoreUnused(renderExistingProcessor(subject,
                                               stage,
                                               0,
                                               lifecycleWarmup,
                                               pattern));
    setPlainParameter(subject,
                      slopeParameter(stage),
                      static_cast<float>(targetSlope));
    juce::ignoreUnused(renderExistingProcessor(subject,
                                               stage,
                                               lifecycleWarmup,
                                               halfTransition,
                                               pattern));
    if (reprepare)
        subject.prepareToPlay(sampleRate, preparedBlockSize);
    else
        subject.reset();

    FireAudioProcessor targetReferenceProcessor;
    configureProcessor(targetReferenceProcessor,
                       stage,
                       targetSlope,
                       useHq);
    FireAudioProcessor oldReferenceProcessor;
    configureProcessor(oldReferenceProcessor,
                       stage,
                       initialSlope,
                       useHq);
    const int comparisonStart = lifecycleWarmup + halfTransition;
    const auto subjectOutput = renderExistingProcessor(subject,
                                                       stage,
                                                       comparisonStart,
                                                       comparisonSamples,
                                                       pattern);
    const auto targetOutput = renderExistingProcessor(targetReferenceProcessor,
                                                      stage,
                                                      comparisonStart,
                                                      comparisonSamples,
                                                      pattern);
    const auto oldOutput = renderExistingProcessor(oldReferenceProcessor,
                                                   stage,
                                                   comparisonStart,
                                                   comparisonSamples,
                                                   pattern);
    requireFiniteAndBounded(subjectOutput, targetOutput, oldOutput);

    const float endpointError = maximumDifference(subjectOutput,
                                                  targetOutput,
                                                  0,
                                                  comparisonSamples);
    const float endpointSeparation = maximumDifference(targetOutput,
                                                       oldOutput,
                                                       0,
                                                       comparisonSamples);
    CAPTURE(stageName(stage),
            useHq,
            reprepare,
            subjectOutput.reportedLatency,
            endpointError,
            endpointSeparation);
    REQUIRE(endpointSeparation > 0.02f);
    CHECK(endpointError < endpointTolerance);
}
} // namespace

TEST_CASE("Global Low/High Cut slopes crossfade 12 dB against every steeper recipe",
          "[processor][filter][slope][transition]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto stage : { CutStage::low, CutStage::high })
        for (const bool useHq : { false, true })
            for (int steeperSlope = 1; steeperSlope <= 3; ++steeperSlope)
                for (const bool rising : { true, false })
                {
                    const int initialSlope = rising ? 0 : steeperSlope;
                    const int targetSlope = rising ? steeperSlope : 0;
                    DYNAMIC_SECTION(stageName(stage)
                                    << " "
                                    << slopeDecibelsPerOctave(initialSlope)
                                    << "->"
                                    << slopeDecibelsPerOctave(targetSlope)
                                    << " dB/oct, HQ=" << useHq)
                    {
                        checkSlopeTransition(stage,
                                             initialSlope,
                                             targetSlope,
                                             useHq);
                    }
                }
}

TEST_CASE("Rapid cutoff-slope reversals retarget the active transition",
          "[processor][filter][slope][transition][rapid]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto stage : { CutStage::low, CutStage::high })
        for (const bool useHq : { false, true })
            for (const bool startSteep : { false, true })
            {
                const int initialSlope = startSteep ? 3 : 0;
                const int temporarySlope = startSteep ? 0 : 3;
                DYNAMIC_SECTION(stageName(stage)
                                << " "
                                << slopeDecibelsPerOctave(initialSlope)
                                << "->"
                                << slopeDecibelsPerOctave(temporarySlope)
                                << "->"
                                << slopeDecibelsPerOctave(initialSlope)
                                << " dB/oct, HQ=" << useHq)
                {
                    checkRapidReverse(stage,
                                      initialSlope,
                                      temporarySlope,
                                      useHq);
                }
            }
}

TEST_CASE("Cutoff-slope transitions ignore host callback partitioning",
          "[processor][filter][slope][transition][block-size]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto stage : { CutStage::low, CutStage::high })
        for (const bool useHq : { false, true })
            for (const bool rising : { true, false })
            {
                const int initialSlope = rising ? 0 : 3;
                const int targetSlope = rising ? 3 : 0;
                DYNAMIC_SECTION(stageName(stage)
                                << " "
                                << slopeDecibelsPerOctave(initialSlope)
                                << "->"
                                << slopeDecibelsPerOctave(targetSlope)
                                << " dB/oct, HQ=" << useHq)
                {
                    checkHostPartition(stage,
                                       initialSlope,
                                       targetSlope,
                                       useHq);
                }
            }
}

TEST_CASE("Queued cutoff slopes preserve the active pair and latest recipe wins",
          "[processor][filter][slope][transition][queued][block-size]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto stage : { CutStage::low, CutStage::high })
        for (const bool useHq : { false, true })
        {
            DYNAMIC_SECTION(stageName(stage) << ", HQ=" << useHq)
            {
                checkQueuedThirdSlope(stage, useHq);
            }
        }
}

TEST_CASE("Cutoff slopes finish warming while their logical stage is bypassed",
          "[processor][filter][slope][transition][stage-bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto stage : { CutStage::low, CutStage::high })
        for (const bool useHq : { false, true })
        {
            DYNAMIC_SECTION(stageName(stage) << ", HQ=" << useHq)
            {
                checkSlopeChangeWhileStageBypassed(stage, useHq);
            }
        }
}

TEST_CASE("Reset and reprepare snap a partial slope transition to its parameter",
          "[processor][filter][slope][transition][reset][prepare]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto stage : { CutStage::low, CutStage::high })
        for (const bool useHq : { false, true })
            for (const bool reprepare : { false, true })
            {
                DYNAMIC_SECTION(stageName(stage)
                                << ", HQ=" << useHq
                                << (reprepare ? ", reprepare" : ", reset"))
                {
                    checkLifecycleSnapsToSlopeEndpoint(stage,
                                                       useHq,
                                                       reprepare);
                }
            }
}
