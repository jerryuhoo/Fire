#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int warmupSamples = 12000;
constexpr int fadeSamples = 240; // 5 ms at 48 kHz
constexpr int oneMillisecondSamples = 48;
constexpr int transitionGuardSamples = 128;
constexpr int finalStateSamples = 4096;
constexpr float endpointTolerance = 2.0e-4f;
constexpr float silentTolerance = 2.0e-4f;
constexpr float complexLfoRateHz = 97.0f;

struct HqEvent
{
    int sample = 0;
    bool enabled = false;
};

struct Timeline
{
    std::vector<std::vector<float>> output;
    bool finite = true;
    bool latencyInvariant = true;
    int reportedLatency = 0;
};

enum class RenderPath
{
    normal,
    hostBypass
};

struct EndpointPoint
{
    int sample = 0;
    int channel = 0;
    float referenceMagnitude = 0.0f;
    float subjectMagnitude = 0.0f;
    float distanceToReference = 0.0f;
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String bandParameter(const juce::String& baseID)
{
    return ParameterIDAndName::getIDString(baseID, 0);
}

void setLayout(FireAudioProcessor& processor, int numChannels)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    juce::AudioProcessor::BusesLayout layout;
    const auto channelSet = numChannels == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    REQUIRE(processor.setBusesLayout(layout));
}

void configureProcessor(FireAudioProcessor& processor,
                        bool useHq,
                        int numChannels = 2,
                        int maximumBlockSize = preparedBlockSize)
{
    setLayout(processor, numChannels);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    setPlainParameter(processor, bandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, bandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, bandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, bandParameter(SAFE_ID), 0.0f);
    setPlainParameter(processor, bandParameter(EXTREME_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID), 1.0f);
    setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID), 1.0f);
    setPlainParameter(processor, bandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, bandParameter(MODE_ID), 2.0f); // tanh
    setPlainParameter(processor, bandParameter(DRIVE_ID), 76.0f);
    setPlainParameter(processor, bandParameter(BIAS_ID), 0.09f);
    setPlainParameter(processor, bandParameter(REC_ID), 0.18f);
    setPlainParameter(processor, bandParameter(SHAPE_MIX_ID), 1.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID), -6.0f);
    setPlainParameter(processor, bandParameter(MIX_ID), 1.0f);
    processor.prepareToPlay(sampleRate, maximumBlockSize);
}

LfoData makeComplexTriangleLfo()
{
    LfoData shape;
    shape.points = { { 0.0f, 0.0f },
                     { 0.5f, 1.0f },
                     { 1.0f, 0.0f } };
    shape.curvatures = { 0.0f, 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

void configureComplexProcessor(FireAudioProcessor& processor,
                               bool useHq,
                               int maximumBlockSize,
                               bool enableLfoRouting)
{
    setLayout(processor, 2);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, -1.5f);
    setPlainParameter(processor, MIX_ID, 0.5f);
    setPlainParameter(processor, NUM_BANDS_ID, 4.0f);

    constexpr float crossoverFrequencies[] { 260.0f, 1850.0f, 7350.0f };
    for (int divider = 0; divider < 3; ++divider)
    {
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, divider),
                          crossoverFrequencies[divider]);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(LINE_STATE_ID,
                                                         divider),
                          1.0f);
    }

    constexpr float bandMixes[] { 0.72f, 0.83f, 0.64f, 0.91f };
    constexpr float driveValues[] { 72.0f, 79.0f, 68.0f, 75.0f };
    for (int band = 0; band < 4; ++band)
    {
        const auto parameter = [band](const juce::String& baseID)
        {
            return ParameterIDAndName::getIDString(baseID, band);
        };
        setPlainParameter(processor, parameter(BAND_ENABLE_ID), 1.0f);
        setPlainParameter(processor, parameter(BAND_SOLO_ID), 0.0f);
        setPlainParameter(processor, parameter(LINKED_ID), 0.0f);
        setPlainParameter(processor, parameter(SAFE_ID), 1.0f);
        setPlainParameter(processor, parameter(EXTREME_ID), 0.0f);
        setPlainParameter(processor, parameter(DRIVE_BYPASS_ID), 1.0f);
        setPlainParameter(processor, parameter(SHAPE_BYPASS_ID), 1.0f);
        setPlainParameter(processor, parameter(COMP_BYPASS_ID), 0.0f);
        setPlainParameter(processor, parameter(WIDTH_BYPASS_ID), 0.0f);
        setPlainParameter(processor, parameter(DC_FILTER_ID), 0.0f);
        setPlainParameter(processor,
                          parameter(MODE_ID),
                          static_cast<float>(2 + band % 2));
        setPlainParameter(processor,
                          parameter(DRIVE_ID),
                          driveValues[band]);
        setPlainParameter(processor,
                          parameter(BIAS_ID),
                          0.04f * static_cast<float>(band + 1));
        setPlainParameter(processor,
                          parameter(REC_ID),
                          0.08f + 0.03f * static_cast<float>(band));
        setPlainParameter(processor, parameter(SHAPE_MIX_ID), 0.86f);
        setPlainParameter(processor, parameter(OUTPUT_ID), -4.0f);
        setPlainParameter(processor,
                          parameter(MIX_ID),
                          bandMixes[band]);
    }

    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0),
                      complexLfoRateHz);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_PHASE_ID, 0),
                      0.17f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0),
                      0.0f);
    processor.getLfoManager().setLfoData(0, makeComplexTriangleLfo());
    if (enableLfoRouting)
    {
        processor.assignLfoToTarget(0, MIX_ID);
        processor.setModulationDepth(MIX_ID, 1.0f);
    }

    processor.prepareToPlay(sampleRate, maximumBlockSize);
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = channel == 0 ? 0.21 : 0.83;
    return 0.31f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 997.0 * time
                   + channelPhase))
         + 0.27f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 7139.0 * time
                   + 0.37 - channelPhase))
         + 0.22f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 12011.0 * time
                   + 0.19 + 0.3 * channelPhase))
         + 0.09f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 173.0 * time + 0.61));
}

Timeline render(bool initialHq,
                const std::vector<HqEvent>& events,
                const std::vector<int>& blockPattern,
                int totalSamples,
                int numChannels = 2,
                int maximumBlockSize = preparedBlockSize,
                int bypassFirstSample = -1,
                int bypassEndSample = -1,
                RenderPath defaultPath = RenderPath::normal,
                bool useComplexRouting = false,
                bool enableComplexLfoRouting = true)
{
    REQUIRE(! blockPattern.empty());
    for (const int blockSize : blockPattern)
        REQUIRE(blockSize > 0);
    REQUIRE((bypassFirstSample < 0
             || (bypassFirstSample < bypassEndSample
                 && bypassEndSample <= totalSamples)));

    FireAudioProcessor processor;
    if (useComplexRouting)
    {
        REQUIRE(numChannels == 2);
        configureComplexProcessor(processor,
                                  initialHq,
                                  maximumBlockSize,
                                  enableComplexLfoRouting);
    }
    else
    {
        configureProcessor(processor,
                           initialHq,
                           numChannels,
                           maximumBlockSize);
    }

    Timeline result;
    result.reportedLatency = processor.getLatencySamples();
    REQUIRE(result.reportedLatency > 0);
    result.output.resize(static_cast<size_t>(numChannels));
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(totalSamples));

    size_t eventIndex = 0;
    size_t blockIndex = 0;
    int streamPosition = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].sample == streamPosition)
        {
            setPlainParameter(processor,
                              HQ_ID,
                              events[eventIndex].enabled ? 1.0f : 0.0f);
            ++eventIndex;
        }

        int nextBoundary = eventIndex < events.size()
                               ? events[eventIndex].sample
                               : totalSamples;
        const int pathIntervalStart = defaultPath == RenderPath::hostBypass
                                          ? bypassEndSample
                                          : bypassFirstSample;
        const int pathIntervalEnd = defaultPath == RenderPath::hostBypass
                                        ? totalSamples
                                        : bypassEndSample;
        if (pathIntervalStart > streamPosition)
            nextBoundary = std::min(nextBoundary, pathIntervalStart);
        if (pathIntervalEnd > streamPosition)
            nextBoundary = std::min(nextBoundary, pathIntervalEnd);
        REQUIRE(nextBoundary > streamPosition);
        const int samplesThisBlock = std::min(
            blockPattern[blockIndex % blockPattern.size()],
            std::min(totalSamples - streamPosition,
                     nextBoundary - streamPosition));
        ++blockIndex;
        REQUIRE(samplesThisBlock > 0);

        juce::AudioBuffer<float> buffer(numChannels, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));
        const bool useHostBypass = defaultPath == RenderPath::hostBypass
                                       ? streamPosition < bypassEndSample
                                       : bypassFirstSample >= 0
                                             && streamPosition
                                                    >= bypassFirstSample
                                             && streamPosition
                                                    < bypassEndSample;
        if (useHostBypass)
            processor.processBlockBypassed(buffer, midi);
        else
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

    return result;
}

Timeline renderComplex(bool initialHq,
                       const std::vector<HqEvent>& events,
                       const std::vector<int>& blockPattern,
                       int totalSamples,
                       bool enableLfoRouting = true)
{
    return render(initialHq,
                  events,
                  blockPattern,
                  totalSamples,
                  2,
                  preparedBlockSize,
                  -1,
                  -1,
                  RenderPath::normal,
                  true,
                  enableLfoRouting);
}

float maximumDifference(const Timeline& first,
                        const Timeline& second,
                        int firstSample,
                        int numSamples)
{
    float maximumError = 0.0f;
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
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][static_cast<size_t>(sample)]
                         - second.output[channel][static_cast<size_t>(sample)]));
        }
    }
    return maximumError;
}

float maximumMagnitude(const Timeline& timeline,
                       int firstSample,
                       int numSamples)
{
    float maximum = 0.0f;
    for (const auto& channel : timeline.output)
    {
        REQUIRE(firstSample >= 0);
        REQUIRE(firstSample + numSamples
                <= static_cast<int>(channel.size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
            maximum = std::max(maximum,
                               std::abs(channel[static_cast<size_t>(sample)]));
    }
    return maximum;
}

EndpointPoint strongestReferencePoint(const Timeline& subject,
                                      const Timeline& reference,
                                      int firstSample,
                                      int numSamples)
{
    EndpointPoint result;
    for (size_t channel = 0; channel < subject.output.size(); ++channel)
    {
        REQUIRE(subject.output[channel].size()
                == reference.output[channel].size());
        REQUIRE(firstSample >= 0);
        REQUIRE(firstSample + numSamples
                <= static_cast<int>(subject.output[channel].size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            const float endpoint = reference.output[channel][
                static_cast<size_t>(sample)];
            if (std::abs(endpoint) <= result.referenceMagnitude)
                continue;

            const float value = subject.output[channel][
                static_cast<size_t>(sample)];
            result.sample = sample;
            result.channel = static_cast<int>(channel);
            result.referenceMagnitude = std::abs(endpoint);
            result.subjectMagnitude = std::abs(value);
            result.distanceToReference = std::abs(value - endpoint);
        }
    }
    return result;
}

int findFirstSilentWindow(const Timeline& timeline,
                          int firstCandidate,
                          int lastCandidate,
                          int windowSamples)
{
    for (int sample = firstCandidate; sample <= lastCandidate; ++sample)
        if (maximumMagnitude(timeline, sample, windowSamples)
            < silentTolerance)
            return sample;
    return -1;
}

int findFirstActiveWindow(const Timeline& timeline,
                          int firstCandidate,
                          int lastCandidate,
                          int windowSamples)
{
    for (int sample = firstCandidate; sample <= lastCandidate; ++sample)
        if (maximumMagnitude(timeline, sample, windowSamples)
            > silentTolerance * 10.0f)
            return sample;
    return -1;
}

int findFirstSettledWindow(const Timeline& subject,
                           const Timeline& reference,
                           int firstCandidate,
                           int lastCandidate,
                           int windowSamples)
{
    for (int sample = firstCandidate; sample <= lastCandidate; ++sample)
        if (maximumDifference(subject,
                              reference,
                              sample,
                              windowSamples)
            < endpointTolerance)
            return sample;
    return -1;
}

void checkFiniteAndBounded(const Timeline& subject,
                           const Timeline& alwaysBase,
                           const Timeline& alwaysHq)
{
    REQUIRE(subject.finite);
    REQUIRE(subject.latencyInvariant);
    REQUIRE(alwaysBase.finite);
    REQUIRE(alwaysBase.latencyInvariant);
    REQUIRE(alwaysHq.finite);
    REQUIRE(alwaysHq.latencyInvariant);
    REQUIRE(subject.reportedLatency == alwaysBase.reportedLatency);
    REQUIRE(subject.reportedLatency == alwaysHq.reportedLatency);

    const int totalSamples = static_cast<int>(subject.output.front().size());
    const float subjectPeak = maximumMagnitude(subject, 0, totalSamples);
    const float endpointPeak = std::max(
        maximumMagnitude(alwaysBase, 0, totalSamples),
        maximumMagnitude(alwaysHq, 0, totalSamples));
    CAPTURE(subjectPeak, endpointPeak);
    CHECK(subjectPeak <= endpointPeak * 1.10f + 0.02f);
    CHECK(subjectPeak < 1.5f);
}

void checkStaticModeIdentity(bool useHq, int numChannels)
{
    constexpr int totalSamples = 2 * preparedBlockSize
                               + finalStateSamples;
    const std::vector<int> subjectPattern {
        1, 31, 257, 7, 113, 19, 251, 43
    };
    const auto subject = render(useHq,
                                {},
                                subjectPattern,
                                totalSamples,
                                numChannels);
    const auto reference = render(useHq,
                                  {},
                                  subjectPattern,
                                  totalSamples,
                                  numChannels);
    const auto alternateReference = render(! useHq,
                                           {},
                                           subjectPattern,
                                           totalSamples,
                                           numChannels);
    REQUIRE(subject.finite);
    REQUIRE(subject.latencyInvariant);
    REQUIRE(reference.finite);
    REQUIRE(reference.latencyInvariant);
    REQUIRE(alternateReference.finite);
    REQUIRE(alternateReference.latencyInvariant);
    REQUIRE(subject.reportedLatency == reference.reportedLatency);

    const float firstCallbackError = maximumDifference(subject,
                                                       reference,
                                                       0,
                                                       subjectPattern.front());
    const float firstAudibleError = maximumDifference(
        subject,
        reference,
        subject.reportedLatency,
        preparedBlockSize);
    const float steadyError = maximumDifference(subject,
                                                reference,
                                                2 * preparedBlockSize,
                                                finalStateSamples);
    const float steadyModeSeparation = maximumDifference(
        reference,
        alternateReference,
        2 * preparedBlockSize,
        finalStateSamples);
    CAPTURE(useHq,
            numChannels,
            subject.reportedLatency,
            firstCallbackError,
            firstAudibleError,
            steadyError,
            steadyModeSeparation);
    CHECK(firstCallbackError == 0.0f);
    CHECK(firstAudibleError == 0.0f);
    CHECK(steadyError == 0.0f);
    REQUIRE(steadyModeSeparation > 0.05f);
}

void checkSafeTransition(bool startHq)
{
    const int totalSamples = warmupSamples
                           + 2 * fadeSamples
                           + preparedBlockSize
                           + 2 * transitionGuardSamples
                           + finalStateSamples
                           + 16;
    const std::vector<HqEvent> event {
        { warmupSamples, ! startHq }
    };
    const auto subject = render(startHq,
                                event,
                                { preparedBlockSize },
                                totalSamples);
    const auto alwaysBase = render(false,
                                   {},
                                   { preparedBlockSize },
                                   totalSamples);
    const auto alwaysHq = render(true,
                                 {},
                                 { preparedBlockSize },
                                 totalSamples);
    const auto& fromReference = startHq ? alwaysHq : alwaysBase;
    const auto& toReference = startHq ? alwaysBase : alwaysHq;
    checkFiniteAndBounded(subject, alwaysBase, alwaysHq);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const int fadeOutEnd = warmupSamples + fadeSamples;
    const int earliestFadeInStart = fadeOutEnd + mutedWarmSamples;
    const int latestFadeInStart = earliestFadeInStart + preparedBlockSize;
    const int detectedFadeInStart = findFirstActiveWindow(
        subject,
        earliestFadeInStart,
        latestFadeInStart,
        16);
    REQUIRE(detectedFadeInStart >= 0);
    const int steadyStateStart = latestFadeInStart
                               + fadeSamples
                               + transitionGuardSamples;
    const float initialError = maximumDifference(
        subject,
        fromReference,
        subject.reportedLatency + 64,
        warmupSamples - subject.reportedLatency - 128);
    const float firstFadeSampleError = maximumDifference(
        subject,
        fromReference,
        warmupSamples,
        1);
    const auto fadeOutMiddle = strongestReferencePoint(
        subject,
        fromReference,
        warmupSamples + fadeSamples / 2 - 24,
        49);
    const float fadeOutEndPeak = maximumMagnitude(subject,
                                                  fadeOutEnd,
                                                  1);
    const float mutedPeak = maximumMagnitude(subject,
                                             fadeOutEnd,
                                             mutedWarmSamples + 1);
    const float prematureFadeInPeak = maximumMagnitude(
        subject,
        fadeOutEnd,
        mutedWarmSamples);
    const auto fadeInMiddle = strongestReferencePoint(
        subject,
        toReference,
        detectedFadeInStart + fadeSamples / 2 - 24,
        49);
    const float finalEndpointSeparation = maximumDifference(
        fromReference,
        toReference,
        steadyStateStart,
        finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                toReference,
                                                steadyStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            subject.reportedLatency,
            mutedWarmSamples,
            initialError,
            firstFadeSampleError,
            fadeOutMiddle.sample,
            fadeOutMiddle.channel,
            fadeOutMiddle.referenceMagnitude,
            fadeOutMiddle.subjectMagnitude,
            fadeOutMiddle.distanceToReference,
            fadeOutEndPeak,
            mutedPeak,
            prematureFadeInPeak,
            earliestFadeInStart,
            detectedFadeInStart,
            fadeInMiddle.sample,
            fadeInMiddle.channel,
            fadeInMiddle.referenceMagnitude,
            fadeInMiddle.subjectMagnitude,
            fadeInMiddle.distanceToReference,
            finalEndpointSeparation,
            finalError);

    CHECK(initialError < endpointTolerance);
    CHECK(firstFadeSampleError < endpointTolerance);
    REQUIRE(fadeOutMiddle.referenceMagnitude > 0.04f);
    CHECK(fadeOutMiddle.subjectMagnitude
          > fadeOutMiddle.referenceMagnitude * 0.10f);
    CHECK(fadeOutMiddle.subjectMagnitude
          < fadeOutMiddle.referenceMagnitude * 0.85f);
    CHECK(fadeOutMiddle.distanceToReference
          > fadeOutMiddle.referenceMagnitude * 0.10f);
    CHECK(fadeOutEndPeak < silentTolerance);
    CHECK(mutedPeak < silentTolerance);
    CHECK(prematureFadeInPeak < silentTolerance);
    REQUIRE(fadeInMiddle.referenceMagnitude > 0.04f);
    CHECK(fadeInMiddle.subjectMagnitude
          > fadeInMiddle.referenceMagnitude * 0.10f);
    CHECK(fadeInMiddle.subjectMagnitude
          < fadeInMiddle.referenceMagnitude * 0.85f);
    CHECK(fadeInMiddle.distanceToReference
          > fadeInMiddle.referenceMagnitude * 0.10f);
    REQUIRE(finalEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkMonoSafeTransition(bool startHq)
{
    const int totalSamples = warmupSamples
                           + 2 * fadeSamples
                           + preparedBlockSize
                           + 2 * transitionGuardSamples
                           + finalStateSamples;
    const std::vector<HqEvent> events {
        { warmupSamples, ! startHq }
    };
    const std::vector<int> pattern { 31, 257, 7, 113, 19, 251 };
    const auto subject = render(startHq,
                                events,
                                pattern,
                                totalSamples,
                                1);
    const auto fromReference = render(startHq,
                                      {},
                                      pattern,
                                      totalSamples,
                                      1);
    const auto toReference = render(! startHq,
                                    {},
                                    pattern,
                                    totalSamples,
                                    1);
    checkFiniteAndBounded(subject,
                         startHq ? toReference : fromReference,
                         startHq ? fromReference : toReference);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const int fadeOutEnd = warmupSamples + fadeSamples;
    const int finalStateStart = fadeOutEnd
                              + mutedWarmSamples
                              + preparedBlockSize
                              + fadeSamples
                              + transitionGuardSamples;
    const float firstFadeSampleError = maximumDifference(subject,
                                                         fromReference,
                                                         warmupSamples,
                                                         1);
    const int firstSilent = findFirstSilentWindow(
        subject,
        fadeOutEnd - 1,
        fadeOutEnd + preparedBlockSize,
        16);
    const float finalEndpointSeparation = maximumDifference(
        fromReference,
        toReference,
        finalStateStart,
        finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                toReference,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            subject.reportedLatency,
            mutedWarmSamples,
            firstFadeSampleError,
            firstSilent,
            finalEndpointSeparation,
            finalError);
    CHECK(firstFadeSampleError < endpointTolerance);
    REQUIRE(firstSilent >= 0);
    CHECK(maximumMagnitude(subject,
                           firstSilent,
                           mutedWarmSamples)
          < silentTolerance);
    REQUIRE(finalEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkFadeOutCancellation(bool startHq)
{
    constexpr int cancelSample = warmupSamples + 96;
    const int totalSamples = cancelSample
                           + fadeSamples
                           + transitionGuardSamples
                           + finalStateSamples;
    const std::vector<HqEvent> redirectedEvents {
        { warmupSamples, ! startHq },
        { cancelSample, startHq }
    };
    const std::vector<HqEvent> continuingEvents {
        { warmupSamples, ! startHq }
    };
    const auto subject = render(startHq,
                                redirectedEvents,
                                { preparedBlockSize },
                                totalSamples);
    const auto continuing = render(startHq,
                                   continuingEvents,
                                   { preparedBlockSize },
                                   totalSamples);
    const auto alwaysBase = render(false,
                                   {},
                                   { preparedBlockSize },
                                   totalSamples);
    const auto alwaysHq = render(true,
                                 {},
                                 { preparedBlockSize },
                                 totalSamples);
    const auto& restoredReference = startHq ? alwaysHq : alwaysBase;
    const auto& abandonedReference = startHq ? alwaysBase : alwaysHq;
    checkFiniteAndBounded(subject, alwaysBase, alwaysHq);
    REQUIRE(continuing.finite);

    const float firstFadeSampleError = maximumDifference(subject,
                                                         restoredReference,
                                                         warmupSamples,
                                                         1);
    const float firstCancelSampleError = maximumDifference(subject,
                                                           continuing,
                                                           cancelSample,
                                                           1);
    const auto recoveredBeforeMute = strongestReferencePoint(
        subject,
        restoredReference,
        warmupSamples + fadeSamples - 24,
        49);
    const int finalStateStart = cancelSample
                              + fadeSamples
                              + transitionGuardSamples;
    const float finalEndpointSeparation = maximumDifference(
        restoredReference,
        abandonedReference,
        finalStateStart,
        finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                restoredReference,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            firstFadeSampleError,
            firstCancelSampleError,
            recoveredBeforeMute.sample,
            recoveredBeforeMute.channel,
            recoveredBeforeMute.referenceMagnitude,
            recoveredBeforeMute.subjectMagnitude,
            finalEndpointSeparation,
            finalError);

    CHECK(firstFadeSampleError < endpointTolerance);
    CHECK(firstCancelSampleError < endpointTolerance);
    REQUIRE(recoveredBeforeMute.referenceMagnitude > 0.04f);
    CHECK(recoveredBeforeMute.subjectMagnitude
          > recoveredBeforeMute.referenceMagnitude * 0.20f);
    REQUIRE(finalEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkMutedWarmRetarget(bool startHq)
{
    constexpr int retargetSample = warmupSamples + fadeSamples + 24;
    const int totalSamples = retargetSample
                           + oneMillisecondSamples
                           + fadeSamples
                           + preparedBlockSize
                           + transitionGuardSamples
                           + finalStateSamples;
    const std::vector<HqEvent> events {
        { warmupSamples, ! startHq },
        { retargetSample, startHq }
    };
    const auto subject = render(startHq,
                                events,
                                { preparedBlockSize },
                                totalSamples);
    const auto alwaysBase = render(false,
                                   {},
                                   { preparedBlockSize },
                                   totalSamples);
    const auto alwaysHq = render(true,
                                 {},
                                 { preparedBlockSize },
                                 totalSamples);
    const auto& latestReference = startHq ? alwaysHq : alwaysBase;
    const auto& abandonedReference = startHq ? alwaysBase : alwaysHq;
    checkFiniteAndBounded(subject, alwaysBase, alwaysHq);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const float firstFadeSampleError = maximumDifference(subject,
                                                         latestReference,
                                                         warmupSamples,
                                                         1);
    const float retargetMutedPeak = maximumMagnitude(subject,
                                                     retargetSample,
                                                     mutedWarmSamples);
    const int finalStateStart = retargetSample
                              + mutedWarmSamples
                              + fadeSamples
                              + preparedBlockSize
                              + transitionGuardSamples;
    const float finalEndpointSeparation = maximumDifference(
        latestReference,
        abandonedReference,
        finalStateStart,
        finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                latestReference,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            subject.reportedLatency,
            mutedWarmSamples,
            firstFadeSampleError,
            retargetMutedPeak,
            finalEndpointSeparation,
            finalError);

    CHECK(firstFadeSampleError < endpointTolerance);
    CHECK(retargetMutedPeak < silentTolerance);
    REQUIRE(finalEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkFadeInReversal(bool startHq)
{
    constexpr int reverseSample = warmupSamples
                                + fadeSamples
                                + oneMillisecondSamples
                                + preparedBlockSize
                                + 96;
    const int totalSamples = reverseSample
                           + fadeSamples
                           + oneMillisecondSamples
                           + fadeSamples
                           + preparedBlockSize
                           + transitionGuardSamples
                           + finalStateSamples;
    const std::vector<HqEvent> redirectedEvents {
        { warmupSamples, ! startHq },
        { reverseSample, startHq }
    };
    const std::vector<HqEvent> continuingEvents {
        { warmupSamples, ! startHq }
    };
    const auto subject = render(startHq,
                                redirectedEvents,
                                { preparedBlockSize },
                                totalSamples);
    const auto continuing = render(startHq,
                                   continuingEvents,
                                   { preparedBlockSize },
                                   totalSamples);
    const auto alwaysBase = render(false,
                                   {},
                                   { preparedBlockSize },
                                   totalSamples);
    const auto alwaysHq = render(true,
                                 {},
                                 { preparedBlockSize },
                                 totalSamples);
    const auto& restoredReference = startHq ? alwaysHq : alwaysBase;
    const auto& abandonedReference = startHq ? alwaysBase : alwaysHq;
    checkFiniteAndBounded(subject, alwaysBase, alwaysHq);
    REQUIRE(continuing.finite);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const float firstReverseSampleError = maximumDifference(subject,
                                                            continuing,
                                                            reverseSample,
                                                            1);
    const float activeFadeInPeak = maximumMagnitude(subject,
                                                    reverseSample,
                                                    16);
    const int secondFadeOutEnd = reverseSample + fadeSamples;
    const float secondMutedPeak = maximumMagnitude(subject,
                                                   secondFadeOutEnd,
                                                   mutedWarmSamples + 1);
    const int finalStateStart = secondFadeOutEnd
                              + mutedWarmSamples
                              + fadeSamples
                              + preparedBlockSize
                              + transitionGuardSamples;
    const float finalEndpointSeparation = maximumDifference(
        restoredReference,
        abandonedReference,
        finalStateStart,
        finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                restoredReference,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            subject.reportedLatency,
            mutedWarmSamples,
            firstReverseSampleError,
            activeFadeInPeak,
            secondMutedPeak,
            finalEndpointSeparation,
            finalError);

    CHECK(firstReverseSampleError < endpointTolerance);
    CHECK(activeFadeInPeak > silentTolerance * 10.0f);
    CHECK(secondMutedPeak < silentTolerance);
    REQUIRE(finalEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkHostPartitionTolerance(bool startHq)
{
    const int totalSamples = warmupSamples
                           + 2 * fadeSamples
                           + 4 * preparedBlockSize
                           + 2 * transitionGuardSamples
                           + finalStateSamples;
    const std::vector<HqEvent> events {
        { warmupSamples, ! startHq }
    };
    const std::vector<int> fixedPattern { preparedBlockSize };
    const std::vector<int> irregularPattern {
        7, 113, 19, 251, 37, 83, 2, 173
    };
    const auto fixed = render(startHq,
                              events,
                              fixedPattern,
                              totalSamples);
    const auto irregular = render(startHq,
                                  events,
                                  irregularPattern,
                                  totalSamples);
    const auto fixedFrom = render(startHq,
                                  {},
                                  fixedPattern,
                                  totalSamples);
    const auto irregularFrom = render(startHq,
                                      {},
                                      irregularPattern,
                                      totalSamples);
    const auto fixedTarget = render(! startHq,
                                    {},
                                    fixedPattern,
                                    totalSamples);
    const auto irregularTarget = render(! startHq,
                                        {},
                                        irregularPattern,
                                        totalSamples);
    checkFiniteAndBounded(fixed, fixedFrom, fixedTarget);
    checkFiniteAndBounded(irregular, irregularFrom, irregularTarget);
    REQUIRE(fixed.reportedLatency == irregular.reportedLatency);

    constexpr int silenceWindow = 16;
    constexpr int settledWindow = 64;
    const int firstSilentCandidate = warmupSamples + fadeSamples - 1;
    const int lastSilentCandidate = warmupSamples
                                  + fadeSamples
                                  + preparedBlockSize;
    const int fixedSilent = findFirstSilentWindow(fixed,
                                                  firstSilentCandidate,
                                                  lastSilentCandidate,
                                                  silenceWindow);
    const int irregularSilent = findFirstSilentWindow(irregular,
                                                      firstSilentCandidate,
                                                      lastSilentCandidate,
                                                      silenceWindow);
    const int firstSettledCandidate = warmupSamples
                                    + 2 * fadeSamples
                                    + oneMillisecondSamples;
    const int lastSettledCandidate = firstSettledCandidate
                                   + 2 * preparedBlockSize
                                   + transitionGuardSamples;
    const int fixedSettled = findFirstSettledWindow(fixed,
                                                    fixedTarget,
                                                    firstSettledCandidate,
                                                    lastSettledCandidate,
                                                    settledWindow);
    const int irregularSettled = findFirstSettledWindow(
        irregular,
        irregularTarget,
        firstSettledCandidate,
        lastSettledCandidate,
        settledWindow);
    const float firstFixedError = maximumDifference(fixed,
                                                    fixedFrom,
                                                    warmupSamples,
                                                    1);
    const float firstIrregularError = maximumDifference(
        irregular,
        irregularFrom,
        warmupSamples,
        1);
    const int conservativeFinalStart = lastSettledCandidate
                                     + settledWindow;
    const float fixedFinalError = maximumDifference(fixed,
                                                    fixedTarget,
                                                    conservativeFinalStart,
                                                    finalStateSamples);
    const float irregularFinalError = maximumDifference(
        irregular,
        irregularTarget,
        conservativeFinalStart,
        finalStateSamples);
    const float finalPartitionError = maximumDifference(
        fixed,
        irregular,
        conservativeFinalStart,
        finalStateSamples);
    CAPTURE(startHq,
            fixed.reportedLatency,
            firstFixedError,
            firstIrregularError,
            fixedSilent,
            irregularSilent,
            fixedSettled,
            irregularSettled,
            fixedFinalError,
            irregularFinalError,
            finalPartitionError);

    CHECK(firstFixedError < endpointTolerance);
    CHECK(firstIrregularError < endpointTolerance);
    REQUIRE(fixedSilent >= 0);
    REQUIRE(irregularSilent >= 0);
    CHECK(std::abs(fixedSilent - irregularSilent) <= preparedBlockSize);
    CHECK(maximumMagnitude(fixed,
                           fixedSilent,
                           oneMillisecondSamples)
          < silentTolerance);
    CHECK(maximumMagnitude(irregular,
                           irregularSilent,
                           oneMillisecondSamples)
          < silentTolerance);
    REQUIRE(fixedSettled >= 0);
    REQUIRE(irregularSettled >= 0);
    CHECK(std::abs(fixedSettled - irregularSettled) <= preparedBlockSize);
    CHECK(fixedSettled - fixedSilent
          >= oneMillisecondSamples + fadeSamples - 32);
    CHECK(irregularSettled - irregularSilent
          >= oneMillisecondSamples + fadeSamples - 32);
    CHECK(fixedFinalError < endpointTolerance);
    CHECK(irregularFinalError < endpointTolerance);
    CHECK(finalPartitionError < endpointTolerance);
}

void checkOversizedCallback(bool startHq)
{
    constexpr int oversizedSamples = 8192;
    constexpr int eventSample = preparedBlockSize;
    constexpr int totalSamples = eventSample
                               + oversizedSamples
                               + finalStateSamples;
    const std::vector<HqEvent> events {
        { eventSample, ! startHq }
    };
    const std::vector<int> pattern {
        eventSample, oversizedSamples, preparedBlockSize
    };
    const auto subject = render(startHq,
                                events,
                                pattern,
                                totalSamples,
                                2,
                                preparedBlockSize);
    const auto fromReference = render(startHq,
                                      {},
                                      pattern,
                                      totalSamples,
                                      2,
                                      preparedBlockSize);
    const auto toReference = render(! startHq,
                                    {},
                                    pattern,
                                    totalSamples,
                                    2,
                                    preparedBlockSize);
    checkFiniteAndBounded(subject,
                         startHq ? toReference : fromReference,
                         startHq ? fromReference : toReference);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const int fadeOutEnd = eventSample + fadeSamples;
    const float firstFadeSampleError = maximumDifference(subject,
                                                         fromReference,
                                                         eventSample,
                                                         1);
    const int firstSilent = findFirstSilentWindow(subject,
                                                  fadeOutEnd - 1,
                                                  fadeOutEnd + 32,
                                                  16);
    REQUIRE(firstSilent >= 0);
    const float mutedPeak = maximumMagnitude(subject,
                                             firstSilent,
                                             mutedWarmSamples);
    const int earliestFadeIn = firstSilent + mutedWarmSamples;
    const int firstActive = findFirstActiveWindow(subject,
                                                  earliestFadeIn,
                                                  earliestFadeIn
                                                      + preparedBlockSize,
                                                  16);
    REQUIRE(firstActive >= 0);
    constexpr int settledWindow = 64;
    const int firstSettledCandidate = firstActive + fadeSamples - 32;
    const int lastSettledCandidate = firstActive
                                   + fadeSamples
                                   + transitionGuardSamples;
    const int firstSettled = findFirstSettledWindow(subject,
                                                    toReference,
                                                    firstSettledCandidate,
                                                    lastSettledCandidate,
                                                    settledWindow);
    REQUIRE(firstSettled >= 0);
    const int latestCompleteTransition = eventSample
                                       + 2 * fadeSamples
                                       + mutedWarmSamples
                                       + preparedBlockSize;
    const int finalStateStart = eventSample + 2048;
    const float endpointSeparation = maximumDifference(fromReference,
                                                       toReference,
                                                       finalStateStart,
                                                       finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                toReference,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            subject.reportedLatency,
            firstFadeSampleError,
            firstSilent,
            mutedPeak,
            earliestFadeIn,
            firstActive,
            firstSettled,
            latestCompleteTransition,
            endpointSeparation,
            finalError);
    CHECK(firstFadeSampleError < endpointTolerance);
    CHECK(mutedPeak < silentTolerance);
    CHECK(firstActive >= earliestFadeIn);
    CHECK(firstSettled <= latestCompleteTransition);
    CHECK(firstSettled < eventSample + oversizedSamples);
    REQUIRE(endpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkComplexOversizedTransition(bool startHq)
{
    constexpr int eventSample = 12288;
    constexpr int oversizedSamples = 8192;
    constexpr int postCallbackSamples = 4096;
    constexpr int totalSamples = eventSample
                               + oversizedSamples
                               + postCallbackSamples;
    const std::vector<HqEvent> events {
        { eventSample, ! startHq }
    };
    const std::vector<int> oversizedPattern {
        eventSample, oversizedSamples, preparedBlockSize
    };

    const auto subject = renderComplex(startHq,
                                       events,
                                       oversizedPattern,
                                       totalSamples);
    const auto alwaysBase = renderComplex(false,
                                          {},
                                          oversizedPattern,
                                          totalSamples);
    const auto alwaysHq = renderComplex(true,
                                        {},
                                        oversizedPattern,
                                        totalSamples);
    const auto& fromReference = startHq ? alwaysHq : alwaysBase;
    const auto& toReference = startHq ? alwaysBase : alwaysHq;
    const auto targetWithoutLfo = renderComplex(! startHq,
                                                {},
                                                oversizedPattern,
                                                totalSamples,
                                                false);
    checkFiniteAndBounded(subject, alwaysBase, alwaysHq);
    REQUIRE(targetWithoutLfo.finite);
    REQUIRE(targetWithoutLfo.latencyInvariant);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const float firstFadeSampleError = maximumDifference(subject,
                                                         fromReference,
                                                         eventSample,
                                                         1);
    const int firstSilent = findFirstSilentWindow(
        subject,
        eventSample + fadeSamples - 1,
        eventSample + fadeSamples + 32,
        16);
    REQUIRE(firstSilent >= 0);
    const float mutedPeak = maximumMagnitude(subject,
                                             firstSilent,
                                             mutedWarmSamples);
    const int firstActive = findFirstActiveWindow(
        subject,
        firstSilent + mutedWarmSamples,
        firstSilent + mutedWarmSamples + preparedBlockSize,
        16);
    REQUIRE(firstActive >= 0);

    // All samples in this window are still inside the one 8192-sample host
    // callback. Matching the static target here proves that every internal HQ
    // phase range consumed the LFO at its absolute callback offset instead of
    // replaying lfoOutputs[0] after each fade/reset/warm segmentation point.
    constexpr int lfoProbeOffset = 4096;
    constexpr int lfoProbeSamples = 2048;
    const int lfoProbeStart = eventSample + lfoProbeOffset;
    REQUIRE(lfoProbeStart + lfoProbeSamples
            < eventSample + oversizedSamples);
    const float lfoAbsoluteSampleError = maximumDifference(
        subject,
        toReference,
        lfoProbeStart,
        lfoProbeSamples);
    const float lfoFixtureSeparation = maximumDifference(
        toReference,
        targetWithoutLfo,
        lfoProbeStart,
        lfoProbeSamples);
    const float endpointSeparation = maximumDifference(fromReference,
                                                       toReference,
                                                       lfoProbeStart,
                                                       lfoProbeSamples);
    const float subjectPeak = maximumMagnitude(subject,
                                               eventSample,
                                               oversizedSamples);
    const float endpointPeak = std::max(
        maximumMagnitude(alwaysBase, eventSample, oversizedSamples),
        maximumMagnitude(alwaysHq, eventSample, oversizedSamples));
    CAPTURE(startHq,
            subject.reportedLatency,
            mutedWarmSamples,
            firstFadeSampleError,
            firstSilent,
            mutedPeak,
            firstActive,
            lfoAbsoluteSampleError,
            lfoFixtureSeparation,
            endpointSeparation,
            subjectPeak,
            endpointPeak);
    CHECK(firstFadeSampleError < endpointTolerance);
    CHECK(mutedPeak < silentTolerance);
    REQUIRE(firstActive >= firstSilent + mutedWarmSamples);
    CHECK(firstActive < eventSample + oversizedSamples);
    // The complex wet and dry paths are deliberately correlated, so the
    // routed Global Mix delta is modest but still comfortably above numerical
    // noise and large enough to reject replaying a segment's first LFO sample.
    REQUIRE(lfoFixtureSeparation > 0.005f);
    REQUIRE(endpointSeparation > 0.02f);
    CHECK(lfoAbsoluteSampleError < endpointTolerance);
    CHECK(subjectPeak <= endpointPeak * 1.10f + 0.02f);
    CHECK(subjectPeak < 1.5f);
}

void checkHostBypassToggle(bool startHq, int numChannels)
{
    constexpr int eventSample = warmupSamples;
    constexpr int bypassEndSample = eventSample
                                  + 2 * fadeSamples
                                  + preparedBlockSize;
    constexpr int totalSamples = bypassEndSample
                               + 2 * fadeSamples
                               + 3 * preparedBlockSize
                               + 2 * transitionGuardSamples
                               + finalStateSamples;
    const std::vector<HqEvent> events {
        { eventSample, ! startHq }
    };
    const std::vector<int> pattern { 73, 257, 11, 149, 37, 251 };
    const auto subject = render(startHq,
                                events,
                                pattern,
                                totalSamples,
                                numChannels,
                                preparedBlockSize,
                                eventSample,
                                bypassEndSample);
    const auto bypassReference = render(startHq,
                                        {},
                                        pattern,
                                        totalSamples,
                                        numChannels,
                                        preparedBlockSize,
                                        eventSample,
                                        bypassEndSample);
    const auto alwaysBase = render(false,
                                   {},
                                   pattern,
                                   totalSamples,
                                   numChannels);
    const auto alwaysHq = render(true,
                                 {},
                                 pattern,
                                 totalSamples,
                                 numChannels);
    const auto& fromReference = startHq ? alwaysHq : alwaysBase;
    const auto& toReference = startHq ? alwaysBase : alwaysHq;
    REQUIRE(subject.finite);
    REQUIRE(subject.latencyInvariant);
    REQUIRE(bypassReference.finite);
    REQUIRE(bypassReference.latencyInvariant);
    REQUIRE(alwaysBase.finite);
    REQUIRE(alwaysHq.finite);
    REQUIRE(subject.reportedLatency == bypassReference.reportedLatency);
    REQUIRE(subject.reportedLatency == alwaysBase.reportedLatency);
    REQUIRE(subject.reportedLatency == alwaysHq.reportedLatency);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const int preEventProbe = eventSample - subject.reportedLatency - 1;
    const float preEventEndpointError = maximumDifference(subject,
                                                          fromReference,
                                                          preEventProbe,
                                                          1);
    const float bypassTransparencyError = maximumDifference(
        subject,
        bypassReference,
        eventSample,
        bypassEndSample - eventSample);
    const float bypassPeak = maximumMagnitude(subject,
                                              eventSample,
                                              bypassEndSample - eventSample);
    const int expectedSilent = bypassEndSample + fadeSamples;
    const int firstSilent = findFirstSilentWindow(
        subject,
        expectedSilent - 1,
        expectedSilent + preparedBlockSize,
        16);
    REQUIRE(firstSilent >= 0);
    const float mutedPeak = maximumMagnitude(subject,
                                             firstSilent,
                                             mutedWarmSamples);
    constexpr int settledWindow = 64;
    const int firstSettledCandidate = firstSilent
                                    + mutedWarmSamples
                                    + fadeSamples
                                    - 32;
    const int lastSettledCandidate = firstSettledCandidate
                                   + 2 * preparedBlockSize
                                   + transitionGuardSamples;
    const int firstSettled = findFirstSettledWindow(subject,
                                                    toReference,
                                                    firstSettledCandidate,
                                                    lastSettledCandidate,
                                                    settledWindow);
    REQUIRE(firstSettled >= 0);
    const int finalStateStart = lastSettledCandidate + settledWindow;
    const float endpointSeparation = maximumDifference(fromReference,
                                                       toReference,
                                                       finalStateStart,
                                                       finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                toReference,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            numChannels,
            subject.reportedLatency,
            subject.latencyInvariant,
            preEventEndpointError,
            bypassTransparencyError,
            bypassPeak,
            firstSilent,
            mutedPeak,
            firstSettled,
            endpointSeparation,
            finalError);
    CHECK(preEventEndpointError < endpointTolerance);
    CHECK(bypassTransparencyError < 1.0e-6f);
    REQUIRE(bypassPeak > 0.20f);
    CHECK(mutedPeak < silentTolerance);
    REQUIRE(endpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}

void checkToggleEntirelyInsideHostBypass(bool startHq, int numChannels)
{
    constexpr int eventSample = warmupSamples;
    constexpr int returnToNormalSample = eventSample
                                      + 2 * fadeSamples
                                      + preparedBlockSize;
    constexpr int totalSamples = returnToNormalSample
                               + 2 * fadeSamples
                               + 3 * preparedBlockSize
                               + 2 * transitionGuardSamples
                               + finalStateSamples;
    const std::vector<HqEvent> events {
        { eventSample, ! startHq }
    };
    const std::vector<int> pattern { 257, 29, 181, 7, 113, 251 };
    const auto subject = render(startHq,
                                events,
                                pattern,
                                totalSamples,
                                numChannels,
                                preparedBlockSize,
                                0,
                                returnToNormalSample,
                                RenderPath::hostBypass);
    const auto bypassFromReference = render(startHq,
                                            {},
                                            pattern,
                                            totalSamples,
                                            numChannels,
                                            preparedBlockSize,
                                            0,
                                            returnToNormalSample,
                                            RenderPath::hostBypass);
    const auto target = render(! startHq,
                               {},
                               pattern,
                               totalSamples,
                               numChannels);
    REQUIRE(subject.finite);
    REQUIRE(subject.latencyInvariant);
    REQUIRE(bypassFromReference.finite);
    REQUIRE(bypassFromReference.latencyInvariant);
    REQUIRE(target.finite);
    REQUIRE(target.latencyInvariant);
    REQUIRE(subject.reportedLatency == target.reportedLatency);

    const float bypassTransparencyError = maximumDifference(
        subject,
        bypassFromReference,
        0,
        returnToNormalSample);
    const float bypassPeak = maximumMagnitude(subject,
                                              eventSample,
                                              returnToNormalSample
                                                  - eventSample);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const int expectedSilent = returnToNormalSample + fadeSamples;
    const int firstSilent = findFirstSilentWindow(
        subject,
        expectedSilent - 1,
        expectedSilent + preparedBlockSize,
        16);
    REQUIRE(firstSilent >= 0);
    const float mutedPeak = maximumMagnitude(subject,
                                             firstSilent,
                                             mutedWarmSamples);
    constexpr int settledWindow = 64;
    const int firstSettledCandidate = firstSilent
                                    + mutedWarmSamples
                                    + fadeSamples
                                    - 32;
    const int lastSettledCandidate = firstSettledCandidate
                                   + 2 * preparedBlockSize
                                   + transitionGuardSamples;
    const int firstSettled = findFirstSettledWindow(subject,
                                                    target,
                                                    firstSettledCandidate,
                                                    lastSettledCandidate,
                                                    settledWindow);
    REQUIRE(firstSettled >= 0);
    const int finalStateStart = lastSettledCandidate + settledWindow;
    const float finalError = maximumDifference(subject,
                                                target,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            numChannels,
            subject.reportedLatency,
            subject.latencyInvariant,
            bypassTransparencyError,
            bypassPeak,
            firstSilent,
            mutedPeak,
            firstSettled,
            finalError);
    CHECK(bypassTransparencyError < 1.0e-6f);
    REQUIRE(bypassPeak > 0.20f);
    CHECK(mutedPeak < silentTolerance);
    CHECK(finalError < endpointTolerance);
}

void checkHostBypassImmediatelyAfterQualitySwitch(bool startHq,
                                                  int numChannels)
{
    constexpr int eventSample = warmupSamples;
    constexpr int bypassFirstSample = eventSample + fadeSamples;
    constexpr int bypassEndSample = bypassFirstSample + 512;
    constexpr int totalSamples = bypassEndSample + 512;
    const std::vector<HqEvent> events {
        { eventSample, ! startHq }
    };

    // A 240-sample callback makes the fade-out finish exactly at its final
    // sample. The very next callback is host-bypassed, so no normal-path
    // warm-up range can hide a cleared raw-delay history.
    const std::vector<int> pattern { fadeSamples };
    const auto subject = render(startHq,
                                events,
                                pattern,
                                totalSamples,
                                numChannels,
                                preparedBlockSize,
                                bypassFirstSample,
                                bypassEndSample);
    const auto targetBypassReference = render(! startHq,
                                              {},
                                              pattern,
                                              totalSamples,
                                              numChannels,
                                              preparedBlockSize,
                                              bypassFirstSample,
                                              bypassEndSample);
    REQUIRE(subject.finite);
    REQUIRE(subject.latencyInvariant);
    REQUIRE(targetBypassReference.finite);
    REQUIRE(targetBypassReference.latencyInvariant);
    REQUIRE(subject.reportedLatency == targetBypassReference.reportedLatency);

    constexpr int tapSettlingSamples = 64;
    const float settledBypassError = maximumDifference(
        subject,
        targetBypassReference,
        bypassFirstSample + tapSettlingSamples,
        bypassEndSample - bypassFirstSample - tapSettlingSamples);
    const float initialHistoryPeak = maximumMagnitude(
        subject,
        bypassFirstSample,
        subject.reportedLatency);
    const float bypassPeak = maximumMagnitude(
        subject,
        bypassFirstSample,
        bypassEndSample - bypassFirstSample);
    CAPTURE(startHq,
            numChannels,
            settledBypassError,
            initialHistoryPeak,
            bypassPeak);
    // Switching the fractional tap may have a short all-pass settling tail,
    // but it must retain real raw history rather than emit an empty delay line.
    REQUIRE(initialHistoryPeak > 0.05f);
    CHECK(settledBypassError < 1.0e-6f);
    REQUIRE(bypassPeak > 0.20f);
}

void checkTinyCallbackResetStorm(bool startHq)
{
    const std::vector<int> tinyCallbacks { 1, 7, 13 };
    // 12012 is exactly 572 complete 1/7/13 cycles, so every automation event
    // below lands on a real host callback boundary without render() splitting
    // or otherwise perturbing the requested callback sequence.
    constexpr int stormStart = 12012;
    std::vector<HqEvent> events {
        { stormStart, ! startHq }
    };

    int callbackPosition = stormStart;
    size_t callbackIndex = 0;
    while (callbackPosition < stormStart + fadeSamples)
    {
        callbackPosition += tinyCallbacks[
            callbackIndex % tinyCallbacks.size()];
        ++callbackIndex;
    }
    const int alternatingStart = callbackPosition;

    // Once the output is muted, reverse the request at every 1/7/13-sample
    // callback. A correct implementation coalesces this reset storm and lets
    // only the last request own the topology that eventually fades back in.
    constexpr int alternatingCallbacks = 91;
    for (int callback = 0; callback < alternatingCallbacks; ++callback)
    {
        const bool requestedHq = callback % 2 == 0
                                     ? ! startHq
                                     : startHq;
        events.push_back({ callbackPosition, requestedHq });
        callbackPosition += tinyCallbacks[
            callbackIndex % tinyCallbacks.size()];
        ++callbackIndex;
    }
    const int stormEnd = callbackPosition;
    const bool finalHq = ! startHq;
    REQUIRE(events.back().enabled == finalHq);

    const int totalSamples = stormEnd
                           + oneMillisecondSamples
                           + fadeSamples
                           + transitionGuardSamples
                           + finalStateSamples;
    const auto subject = render(startHq,
                                events,
                                tinyCallbacks,
                                totalSamples);
    const auto alwaysBase = render(false,
                                   {},
                                   tinyCallbacks,
                                   totalSamples);
    const auto alwaysHq = render(true,
                                 {},
                                 tinyCallbacks,
                                 totalSamples);
    const auto& fromReference = startHq ? alwaysHq : alwaysBase;
    const auto& latestReference = finalHq ? alwaysHq : alwaysBase;
    checkFiniteAndBounded(subject, alwaysBase, alwaysHq);

    const int mutedWarmSamples = std::max(subject.reportedLatency,
                                          oneMillisecondSamples);
    const float firstFadeSampleError = maximumDifference(subject,
                                                         fromReference,
                                                         stormStart,
                                                         1);
    const int firstSilent = findFirstSilentWindow(
        subject,
        stormStart + fadeSamples - 1,
        alternatingStart,
        8);
    REQUIRE(firstSilent >= 0);
    const float resetStormPeak = maximumMagnitude(
        subject,
        alternatingStart,
        stormEnd - alternatingStart);
    const int finalStateStart = stormEnd
                              + mutedWarmSamples
                              + fadeSamples
                              + transitionGuardSamples;
    const float staleEndpointSeparation = maximumDifference(
        latestReference,
        fromReference,
        finalStateStart,
        finalStateSamples);
    const float finalError = maximumDifference(subject,
                                                latestReference,
                                                finalStateStart,
                                                finalStateSamples);
    CAPTURE(startHq,
            finalHq,
            subject.reportedLatency,
            events.size(),
            alternatingStart,
            stormEnd,
            firstFadeSampleError,
            firstSilent,
            resetStormPeak,
            staleEndpointSeparation,
            finalError);
    CHECK(firstFadeSampleError < endpointTolerance);
    CHECK(resetStormPeak < silentTolerance);
    REQUIRE(staleEndpointSeparation > 0.05f);
    CHECK(finalError < endpointTolerance);
}
} // namespace

TEST_CASE("Static HQ selection is primed directly at its canonical endpoint",
          "[processor][hq][transition][static]")
{
    for (const int numChannels : { 1, 2 })
    {
        checkStaticModeIdentity(false, numChannels);
        checkStaticModeIdentity(true, numChannels);
    }
}

TEST_CASE("Live HQ changes use a bounded fade-mute-reset-warm-fade state machine",
          "[processor][hq][transition]")
{
    checkSafeTransition(false);
    checkSafeTransition(true);

    SECTION("Mono uses the same transition contract")
    {
        checkMonoSafeTransition(false);
        checkMonoSafeTransition(true);
    }
}

TEST_CASE("Rapid live HQ changes obey latest-target-wins in every transition state",
          "[processor][hq][transition][rapid]")
{
    SECTION("A fade-out can be cancelled before the topology is muted")
    {
        checkFadeOutCancellation(false);
        checkFadeOutCancellation(true);
    }

    SECTION("A target change during muted warm-up restarts on the latest topology")
    {
        checkMutedWarmRetarget(false);
        checkMutedWarmRetarget(true);
    }

    SECTION("A reversal during fade-in safely fades down before restoring")
    {
        checkFadeInReversal(false);
        checkFadeInReversal(true);
    }

    SECTION("A 1/7/13-sample muted reset storm settles on only the last request")
    {
        checkTinyCallbackResetStorm(false);
        checkTinyCallbackResetStorm(true);
    }
}

TEST_CASE("Live HQ transition timing is bounded by host callback partitioning",
          "[processor][hq][transition][block-size]")
{
    checkHostPartitionTolerance(false);
    checkHostPartitionTolerance(true);
}

TEST_CASE("An oversized callback can contain a complete HQ transition",
          "[processor][hq][transition][oversized]")
{
    checkOversizedCallback(false);
    checkOversizedCallback(true);
}

TEST_CASE("A complex modulated four-band graph survives an oversized HQ transition",
          "[processor][hq][transition][oversized][multiband][lfo]")
{
    checkComplexOversizedTransition(false);
    checkComplexOversizedTransition(true);
}

TEST_CASE("Host bypass defers HQ changes without muting its transparent raw path",
          "[processor][hq][transition][host-bypass]")
{
    for (const int numChannels : { 1, 2 })
    {
        checkHostBypassToggle(false, numChannels);
        checkHostBypassToggle(true, numChannels);
        checkToggleEntirelyInsideHostBypass(false, numChannels);
        checkToggleEntirelyInsideHostBypass(true, numChannels);
        checkHostBypassImmediatelyAfterQualitySwitch(false, numChannels);
        checkHostBypassImmediatelyAfterQualitySwitch(true, numChannels);
    }
}
