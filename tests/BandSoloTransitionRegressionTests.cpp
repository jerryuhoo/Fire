#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int hostBlockSize = 257;
constexpr int warmupSamples = 12000;
constexpr int transitionSamples = 480; // 10 ms
constexpr int topologyRampSamples = 240; // 5 ms
constexpr int topologyWarmupSamples = 48; // 1 ms
constexpr int topologyTransitionSamples = 2 * topologyRampSamples
                                        + topologyWarmupSamples;
constexpr int finalStateSamples = 4096;

using SoloState = std::array<bool, 2>;

constexpr SoloState noSolo { false, false };
constexpr SoloState firstBandSolo { true, false };
constexpr SoloState secondBandSolo { false, true };
constexpr SoloState bothBandsSolo { true, true };

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String bandParameter(const juce::String& baseID, int bandIndex)
{
    return ParameterIDAndName::getIDString(baseID, bandIndex);
}

int configureProcessor(FireAudioProcessor& processor,
                       SoloState soloState,
                       bool useHq,
                       float globalMix)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, globalMix);
    setPlainParameter(processor, NUM_BANDS_ID, 2.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(FREQ_ID, 0),
                      1000.0f);

    for (int band = 0; band < 4; ++band)
    {
        setPlainParameter(processor,
                          bandParameter(BAND_ENABLE_ID, band),
                          1.0f);
        setPlainParameter(processor,
                          bandParameter(BAND_SOLO_ID, band),
                          band < static_cast<int>(soloState.size())
                                  && soloState[static_cast<size_t>(band)]
                              ? 1.0f
                              : 0.0f);
        setPlainParameter(processor, bandParameter(LINKED_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(COMP_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DC_FILTER_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(OUTPUT_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(MIX_ID, band), 1.0f);
    }

    processor.prepareToPlay(sampleRate, hostBlockSize);
    // The host-facing PDC is fixed at round(natural HQ latency), even while
    // HQ is off. The Solo control envelope itself is delayed inside the HQ
    // topology only; in base mode the complete output delay moves the audible
    // transition by the fixed integer PDC instead.
    return useHq
               ? static_cast<int>(std::ceil(processor.getTotalLatency()))
               : processor.getLatencySamples();
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double phase = channel == 0 ? 0.31 : 0.79;
    return 0.48f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 173.0 * time + phase))
         + 0.21f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 487.0 * time + 0.42 - phase))
         + 0.17f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 2303.0 * time + 0.18));
}

struct Timeline
{
    std::array<std::vector<float>, 2> output;
    bool finite = true;
    int soloEnvelopeDelaySamples = 0;
};

struct SoloEvent
{
    int sample = 0;
    SoloState state {};
};

void setSoloState(FireAudioProcessor& processor, SoloState state)
{
    for (int band = 0; band < 2; ++band)
    {
        setPlainParameter(processor,
                          bandParameter(BAND_SOLO_ID, band),
                          state[static_cast<size_t>(band)] ? 1.0f : 0.0f);
    }
}

Timeline renderSoloEventsPartitioned(SoloState initialState,
                                     const std::vector<SoloEvent>& events,
                                     bool useHq,
                                     float globalMix,
                                     int totalSamples,
                                     const std::vector<int>& blockPattern)
{
    REQUIRE(! blockPattern.empty());
    for (const int blockSize : blockPattern)
    {
        REQUIRE(blockSize > 0);
        REQUIRE(blockSize <= hostBlockSize);
    }

    FireAudioProcessor processor;
    const int soloEnvelopeDelaySamples = configureProcessor(processor,
                                                            initialState,
                                                            useHq,
                                                            globalMix);
    totalSamples += soloEnvelopeDelaySamples + 2;

    Timeline timeline;
    timeline.soloEnvelopeDelaySamples = soloEnvelopeDelaySamples;
    for (auto& output : timeline.output)
        output.reserve(static_cast<size_t>(totalSamples));

    int streamPosition = 0;
    size_t eventIndex = 0;
    size_t blockIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].sample == streamPosition)
        {
            setSoloState(processor, events[eventIndex].state);
            ++eventIndex;
        }

        const int nextEventSample = eventIndex < events.size()
                                        ? events[eventIndex].sample
                                        : totalSamples;
        REQUIRE(nextEventSample > streamPosition);
        const int requestedBlockSize = blockPattern[
            blockIndex % blockPattern.size()];
        ++blockIndex;
        const int samplesThisBlock = std::min(
            requestedBlockSize,
            std::min(totalSamples - streamPosition,
                     nextEventSample - streamPosition));
        REQUIRE(samplesThisBlock > 0);

        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel, streamPosition + sample));

        processor.processBlock(buffer, midi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                timeline.finite = timeline.finite && std::isfinite(value);
                timeline.output[static_cast<size_t>(channel)].push_back(value);
            }
        }

        streamPosition += samplesThisBlock;
    }

    return timeline;
}

Timeline renderSoloEvents(SoloState initialState,
                          const std::vector<SoloEvent>& events,
                          bool useHq,
                          float globalMix,
                          int totalSamples)
{
    return renderSoloEventsPartitioned(initialState,
                                       events,
                                       useHq,
                                       globalMix,
                                       totalSamples,
                                       { hostBlockSize });
}

Timeline renderTimeline(bool startSoloed,
                        bool changeSolo,
                        bool useHq,
                        float globalMix)
{
    FireAudioProcessor processor;
    const int soloEnvelopeDelaySamples = configureProcessor(
        processor,
        startSoloed ? secondBandSolo : noSolo,
        useHq,
        globalMix);

    Timeline timeline;
    timeline.soloEnvelopeDelaySamples = soloEnvelopeDelaySamples;
    const int totalSamples = warmupSamples
                           + soloEnvelopeDelaySamples
                           + transitionSamples
                           + finalStateSamples
                           + 2;
    for (auto& output : timeline.output)
        output.reserve(static_cast<size_t>(totalSamples));

    int streamPosition = 0;
    bool changed = false;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        if (! changed && streamPosition >= warmupSamples)
        {
            if (changeSolo)
            {
                setPlainParameter(processor,
                                  bandParameter(BAND_SOLO_ID, 1),
                                  startSoloed ? 0.0f : 1.0f);
            }
            changed = true;
        }

        const int nextBoundary = ! changed ? warmupSamples : totalSamples;
        const int samplesThisBlock = std::min(
            hostBlockSize,
            std::min(totalSamples - streamPosition,
                     nextBoundary - streamPosition));
        REQUIRE(samplesThisBlock > 0);

        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel, streamPosition + sample));

        processor.processBlock(buffer, midi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                timeline.finite = timeline.finite && std::isfinite(value);
                timeline.output[static_cast<size_t>(channel)].push_back(value);
            }
        }

        streamPosition += samplesThisBlock;
    }

    return timeline;
}

float maximumDifference(const Timeline& first,
                        const Timeline& second,
                        int startSample,
                        int numSamples)
{
    float maximumError = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        REQUIRE(startSample >= 0);
        REQUIRE(startSample + numSamples
                <= static_cast<int>(first.output[channel].size()));
        for (int sample = startSample; sample < startSample + numSamples; ++sample)
        {
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][static_cast<size_t>(sample)]
                         - second.output[channel][static_cast<size_t>(sample)]));
        }
    }
    return maximumError;
}

float maximumAdjacentStep(const Timeline& timeline,
                          int startSample,
                          int numSamples)
{
    float maximumStep = 0.0f;
    for (const auto& channel : timeline.output)
    {
        const int endSample = std::min(static_cast<int>(channel.size()),
                                       startSample + numSamples);
        for (int sample = std::max(1, startSample); sample < endSample; ++sample)
        {
            maximumStep = std::max(
                maximumStep,
                std::abs(channel[static_cast<size_t>(sample)]
                         - channel[static_cast<size_t>(sample - 1)]));
        }
    }
    return maximumStep;
}

void checkSoloTransition(bool soloOn, bool useHq, float globalMix)
{
    const auto subject = renderTimeline(! soloOn, true, useHq, globalMix);
    const auto alwaysFull = renderTimeline(false, false, useHq, globalMix);
    const auto alwaysSolo = renderTimeline(true, false, useHq, globalMix);
    CHECK(subject.finite);
    CHECK(alwaysFull.finite);
    CHECK(alwaysSolo.finite);
    REQUIRE(subject.soloEnvelopeDelaySamples
            == alwaysFull.soloEnvelopeDelaySamples);
    REQUIRE(subject.soloEnvelopeDelaySamples
            == alwaysSolo.soloEnvelopeDelaySamples);

    const auto& fromReference = soloOn ? alwaysFull : alwaysSolo;
    const auto& toReference = soloOn ? alwaysSolo : alwaysFull;
    const int audibleTransitionStart = warmupSamples
                                     + subject.soloEnvelopeDelaySamples;
    const float firstSampleFromError = maximumDifference(subject,
                                                         fromReference,
                                                         warmupSamples,
                                                         1);
    const float firstSampleToError = maximumDifference(subject,
                                                       toReference,
                                                       warmupSamples,
                                                       1);
    const float middleFromError = maximumDifference(subject,
                                                    fromReference,
                                                    audibleTransitionStart
                                                        + transitionSamples / 2,
                                                    1);
    const float middleToError = maximumDifference(subject,
                                                  toReference,
                                                  audibleTransitionStart
                                                      + transitionSamples / 2,
                                                  1);
    const float finalStateError = maximumDifference(subject,
                                                    toReference,
                                                    audibleTransitionStart
                                                        + transitionSamples
                                                        + 2,
                                                    finalStateSamples);
    const float endpointSeparation = maximumDifference(alwaysFull,
                                                       alwaysSolo,
                                                       warmupSamples,
                                                       transitionSamples
                                                           + finalStateSamples);
    const float maximumStep = maximumAdjacentStep(subject,
                                                  warmupSamples - 1,
                                                  transitionSamples + 2);
    CAPTURE(soloOn,
            useHq,
            globalMix,
            subject.soloEnvelopeDelaySamples,
            firstSampleFromError,
            firstSampleToError,
            middleFromError,
            middleToError,
            finalStateError,
            endpointSeparation,
            maximumStep);

    REQUIRE(endpointSeparation > 0.2f);
    // A 10 ms crossfade should begin at the audible pre-switch state, occupy
    // neither endpoint halfway through, and settle exactly on the requested
    // solo state after the ramp.
    CHECK(firstSampleFromError < 2.0e-4f);
    CHECK(firstSampleToError > endpointSeparation * 0.1f);
    CHECK(middleFromError > endpointSeparation * 0.05f);
    CHECK(middleToError > endpointSeparation * 0.05f);
    CHECK(finalStateError < 2.0e-4f);
    CHECK(maximumStep < endpointSeparation * 0.2f);
}

void checkRapidSoloRedirection(bool useHq, float globalMix)
{
    constexpr int redirectOffset = 160;
    constexpr int redirectSample = warmupSamples + redirectOffset;
    constexpr int totalSamples = redirectSample
                               + transitionSamples
                               + finalStateSamples;

    const std::vector<SoloEvent> redirectedEvents {
        { warmupSamples, secondBandSolo },
        { redirectSample, bothBandsSolo }
    };
    const std::vector<SoloEvent> controlEvents {
        { warmupSamples, secondBandSolo }
    };
    const auto subject = renderSoloEvents(firstBandSolo,
                                          redirectedEvents,
                                          useHq,
                                          globalMix,
                                          totalSamples);
    const auto firstBandReference = renderSoloEvents(firstBandSolo,
                                                     {},
                                                     useHq,
                                                     globalMix,
                                                     totalSamples);
    const auto secondBandControl = renderSoloEvents(firstBandSolo,
                                                    controlEvents,
                                                    useHq,
                                                    globalMix,
                                                    totalSamples);
    const auto bothBandsReference = renderSoloEvents(bothBandsSolo,
                                                     {},
                                                     useHq,
                                                     globalMix,
                                                     totalSamples);
    CHECK(subject.finite);
    CHECK(firstBandReference.finite);
    CHECK(secondBandControl.finite);
    CHECK(bothBandsReference.finite);
    REQUIRE(subject.soloEnvelopeDelaySamples
            == firstBandReference.soloEnvelopeDelaySamples);
    REQUIRE(subject.soloEnvelopeDelaySamples
            == secondBandControl.soloEnvelopeDelaySamples);
    REQUIRE(subject.soloEnvelopeDelaySamples
            == bothBandsReference.soloEnvelopeDelaySamples);

    const int audibleRedirectStart = redirectSample
                                   + subject.soloEnvelopeDelaySamples;

    const float firstSwitchError = maximumDifference(subject,
                                                      firstBandReference,
                                                      warmupSamples,
                                                      1);
    const float redirectFirstSampleError = maximumDifference(subject,
                                                             secondBandControl,
                                                             redirectSample,
                                                             1);
    const float middleFromControlError = maximumDifference(
        subject,
        secondBandControl,
        audibleRedirectStart + transitionSamples / 2,
        1);
    const float middleToBothError = maximumDifference(
        subject,
        bothBandsReference,
        audibleRedirectStart + transitionSamples / 2,
        1);
    const float finalBothError = maximumDifference(subject,
                                                   bothBandsReference,
                                                   audibleRedirectStart
                                                       + transitionSamples,
                                                   finalStateSamples);
    const float aToBSeparation = maximumDifference(firstBandReference,
                                                   secondBandControl,
                                                   warmupSamples
                                                       + transitionSamples,
                                                   finalStateSamples);
    CAPTURE(useHq,
            globalMix,
            subject.soloEnvelopeDelaySamples,
            firstSwitchError,
            redirectFirstSampleError,
            middleFromControlError,
            middleToBothError,
            finalBothError,
            aToBSeparation);

    REQUIRE(aToBSeparation > 0.2f);
    // The first A->B switch and the later retarget to a multi-solo state both
    // emit the currently audible gains before advancing their new ramps.
    CHECK(firstSwitchError < 2.0e-4f);
    CHECK(redirectFirstSampleError < 2.0e-4f);
    CHECK(middleFromControlError > aToBSeparation * 0.02f);
    CHECK(middleToBothError > aToBSeparation * 0.02f);
    CHECK(finalBothError < 2.0e-4f);
}

void checkHqGlobalMixEnvelopeAlignment(bool soloOn)
{
    constexpr std::array<float, 3> globalMixValues { 0.0f, 0.5f, 1.0f };
    std::array<Timeline, globalMixValues.size()> subjects;
    std::array<Timeline, globalMixValues.size()> fromReferences;
    std::array<Timeline, globalMixValues.size()> toReferences;

    for (size_t mix = 0; mix < globalMixValues.size(); ++mix)
    {
        const float globalMix = globalMixValues[mix];
        subjects[mix] = renderTimeline(! soloOn, true, true, globalMix);
        const auto alwaysFull = renderTimeline(false, false, true, globalMix);
        const auto alwaysSolo = renderTimeline(true, false, true, globalMix);
        fromReferences[mix] = soloOn ? alwaysFull : alwaysSolo;
        toReferences[mix] = soloOn ? alwaysSolo : alwaysFull;

        REQUIRE(subjects[mix].finite);
        REQUIRE(fromReferences[mix].finite);
        REQUIRE(toReferences[mix].finite);
        REQUIRE(subjects[mix].soloEnvelopeDelaySamples
                == subjects.front().soloEnvelopeDelaySamples);
    }

    // Solo-on changes only the low band's contribution (and Solo-off restores
    // it).  Relative to the two static references, the residual therefore is
    // the exact audible Solo gain.  That normalized gain must be identical on
    // the Global Mix dry, half-mix, and wet paths, including HQ latency.
    const int audibleTransitionStart = warmupSamples
                                     + subjects.front().soloEnvelopeDelaySamples;
    constexpr int edgeGuardSamples = 64;
    constexpr float minimumDenominator = 0.02f;
    float maximumNormalizedGainMismatch = 0.0f;
    int comparedSamples = 0;
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int sample = audibleTransitionStart + edgeGuardSamples;
             sample < audibleTransitionStart
                          + transitionSamples
                          - edgeGuardSamples;
             ++sample)
        {
            std::array<float, globalMixValues.size()> residuals {};
            bool usable = true;
            for (size_t mix = 0; mix < globalMixValues.size(); ++mix)
            {
                const float from = fromReferences[mix].output[
                    static_cast<size_t>(channel)][static_cast<size_t>(sample)];
                const float to = toReferences[mix].output[
                    static_cast<size_t>(channel)][static_cast<size_t>(sample)];
                const float denominator = from - to;
                if (std::abs(denominator) < minimumDenominator)
                {
                    usable = false;
                    break;
                }

                const float subject = subjects[mix].output[
                    static_cast<size_t>(channel)][static_cast<size_t>(sample)];
                residuals[mix] = (subject - to) / denominator;
                usable = usable && std::isfinite(residuals[mix]);
            }

            if (! usable)
                continue;

            ++comparedSamples;
            maximumNormalizedGainMismatch = std::max(
                maximumNormalizedGainMismatch,
                std::max(std::abs(residuals[0] - residuals[1]),
                         std::abs(residuals[0] - residuals[2])));
        }
    }

    CAPTURE(soloOn,
            subjects.front().soloEnvelopeDelaySamples,
            comparedSamples,
            maximumNormalizedGainMismatch);
    REQUIRE(comparedSamples > 100);
    CHECK(maximumNormalizedGainMismatch < 2.0e-4f);
}

void checkSoloHostPartitionInvariance(bool useHq, float globalMix)
{
    constexpr int secondEventSample = warmupSamples + 197;
    constexpr int thirdEventSample = secondEventSample + 331;
    constexpr int totalSamples = thirdEventSample
                               + transitionSamples
                               + finalStateSamples;
    const std::vector<SoloEvent> events {
        { warmupSamples, secondBandSolo },
        { secondEventSample, firstBandSolo },
        { thirdEventSample, noSolo }
    };
    const auto fixedBlocks = renderSoloEventsPartitioned(noSolo,
                                                         events,
                                                         useHq,
                                                         globalMix,
                                                         totalSamples,
                                                         { hostBlockSize });
    const auto irregularBlocks = renderSoloEventsPartitioned(
        noSolo,
        events,
        useHq,
        globalMix,
        totalSamples,
        { 7, 113, 19, 251, 37, 83, 2, 173 });

    REQUIRE(fixedBlocks.finite);
    REQUIRE(irregularBlocks.finite);
    const float wholeStreamError = maximumDifference(
        fixedBlocks,
        irregularBlocks,
        0,
        static_cast<int>(fixedBlocks.output.front().size()));
    const float transitionError = maximumDifference(
        fixedBlocks,
        irregularBlocks,
        warmupSamples,
        thirdEventSample - warmupSamples
            + transitionSamples
            + fixedBlocks.soloEnvelopeDelaySamples
            + 2);
    CAPTURE(useHq,
            globalMix,
            fixedBlocks.soloEnvelopeDelaySamples,
            wholeStreamError,
            transitionError);
    CHECK(wholeStreamError < 2.0e-4f);
    CHECK(transitionError < 2.0e-4f);
}

void processInputRange(FireAudioProcessor& processor,
                       int firstSample,
                       int numSamples,
                       Timeline* capture)
{
    juce::MidiBuffer midi;
    int processed = 0;
    while (processed < numSamples)
    {
        const int samplesThisBlock = std::min(hostBlockSize,
                                              numSamples - processed);
        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             firstSample
                                                 + processed
                                                 + sample));

        processor.processBlock(buffer, midi);
        if (capture != nullptr)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto& output = capture->output[static_cast<size_t>(channel)];
                for (int sample = 0; sample < samplesThisBlock; ++sample)
                {
                    const float value = buffer.getSample(channel, sample);
                    capture->finite = capture->finite && std::isfinite(value);
                    output.push_back(value);
                }
            }
        }
        processed += samplesThisBlock;
    }
}

Timeline renderAfterHqTopologyReset(int targetBandCount,
                                    bool requestGenerationReset,
                                    bool beginWithSoloHistory)
{
    REQUIRE(targetBandCount >= 1);
    REQUIRE(targetBandCount <= 4);
    FireAudioProcessor processor;
    const int latency = configureProcessor(processor,
                                           beginWithSoloHistory
                                               ? secondBandSolo
                                               : noSolo,
                                           true,
                                           1.0f);
    processInputRange(processor, 0, warmupSamples, nullptr);

    if (beginWithSoloHistory)
        setSoloState(processor, noSolo);

    setPlainParameter(processor,
                      NUM_BANDS_ID,
                      static_cast<float>(targetBandCount));
    if (targetBandCount > 1)
    {
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, 0),
                          600.0f);
    }
    if (targetBandCount > 2)
    {
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, 1),
                          3200.0f);
    }
    if (requestGenerationReset)
        processor.requestMultibandTopologyReset();

    Timeline result;
    result.soloEnvelopeDelaySamples = latency;
    processInputRange(processor,
                      warmupSamples,
                      topologyTransitionSamples + finalStateSamples,
                      &result);
    return result;
}

void checkHqTopologyResetSoloEnvelopePriming(int targetBandCount,
                                             bool requestGenerationReset)
{
    const auto subject = renderAfterHqTopologyReset(targetBandCount,
                                                    requestGenerationReset,
                                                    true);
    const auto cleanNoSoloReference = renderAfterHqTopologyReset(
        targetBandCount,
        requestGenerationReset,
        false);
    REQUIRE(subject.finite);
    REQUIRE(cleanNoSoloReference.finite);

    const int latency = subject.soloEnvelopeDelaySamples;
    const int fadeInStart = topologyRampSamples
                          + topologyWarmupSamples;
    const int settledStart = topologyTransitionSamples + latency + 2;
    const int renderedSamples = static_cast<int>(subject.output.front().size());
    REQUIRE(settledStart < renderedSamples);

    // Before commit the subject is allowed to fade out its deliberately
    // soloed old topology. At zero the replacement snapshot is committed and
    // its Solo delay is primed from no-Solo; the complete fade-in and settled
    // output must then be indistinguishable from a clean no-Solo processor.
    const float fadeInError = maximumDifference(subject,
                                                cleanNoSoloReference,
                                                fadeInStart,
                                                topologyRampSamples);
    const float settledError = maximumDifference(
        subject,
        cleanNoSoloReference,
        settledStart,
        renderedSamples - settledStart);
    float subjectWarmMagnitude = 0.0f;
    float referenceWarmMagnitude = 0.0f;
    float referenceMagnitude = 0.0f;
    float subjectMagnitude = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int sample = topologyRampSamples;
             sample < fadeInStart;
             ++sample)
        {
            subjectWarmMagnitude = std::max(
                subjectWarmMagnitude,
                std::abs(subject.output[static_cast<size_t>(channel)]
                                       [static_cast<size_t>(sample)]));
            referenceWarmMagnitude = std::max(
                referenceWarmMagnitude,
                std::abs(cleanNoSoloReference.output[
                    static_cast<size_t>(channel)][static_cast<size_t>(sample)]));
        }
        for (int sample = fadeInStart;
             sample < fadeInStart + topologyRampSamples;
             ++sample)
        {
            referenceMagnitude = std::max(
                referenceMagnitude,
                std::abs(cleanNoSoloReference.output[
                    static_cast<size_t>(channel)][static_cast<size_t>(sample)]));
            subjectMagnitude = std::max(
                subjectMagnitude,
                std::abs(subject.output[static_cast<size_t>(channel)]
                                           [static_cast<size_t>(sample)]));
        }
    }

    CAPTURE(targetBandCount,
            requestGenerationReset,
            latency,
            fadeInError,
            settledError,
            subjectWarmMagnitude,
            referenceWarmMagnitude,
            referenceMagnitude,
            subjectMagnitude);
    REQUIRE(latency > 0);
    REQUIRE(referenceMagnitude > 0.1f);
    CHECK(subjectWarmMagnitude < 1.0e-7f);
    CHECK(referenceWarmMagnitude < 1.0e-7f);
    CHECK(fadeInError < 2.0e-4f);
    CHECK(settledError < 2.0e-4f);
    CHECK(subjectMagnitude > referenceMagnitude * 0.9f);
}
} // namespace

TEST_CASE("Band Solo transitions are click-free",
          "[processor][band][solo][transition]")
{
    SECTION("Solo on")
    {
        for (const bool useHq : { false, true })
            for (const float globalMix : { 0.0f, 0.5f, 1.0f })
                checkSoloTransition(true, useHq, globalMix);
    }

    SECTION("Solo off")
    {
        for (const bool useHq : { false, true })
            for (const float globalMix : { 0.0f, 0.5f, 1.0f })
                checkSoloTransition(false, useHq, globalMix);
    }
}

TEST_CASE("Rapid Solo changes redirect the active gain ramps continuously",
          "[processor][band][solo][transition][rapid][multiple]")
{
    for (const bool useHq : { false, true })
        for (const float globalMix : { 0.0f, 0.5f, 1.0f })
            checkRapidSoloRedirection(useHq, globalMix);
}

TEST_CASE("HQ Solo gain stays aligned across the Global Mix paths",
          "[processor][band][solo][transition][hq][mix]")
{
    SECTION("Solo on")
    {
        checkHqGlobalMixEnvelopeAlignment(true);
    }

    SECTION("Solo off")
    {
        checkHqGlobalMixEnvelopeAlignment(false);
    }
}

TEST_CASE("Solo transitions follow absolute samples across host partitions",
          "[processor][band][solo][transition][block-size]")
{
    for (const bool useHq : { false, true })
        for (const float globalMix : { 0.0f, 0.5f, 1.0f })
            checkSoloHostPartitionInvariance(useHq, globalMix);
}

TEST_CASE("HQ topology resets prime the no-Solo gain delay",
          "[processor][band][solo][hq][topology][priming]")
{
    SECTION("NUM_BANDS change")
    {
        checkHqTopologyResetSoloEnvelopePriming(3, false);
        checkHqTopologyResetSoloEnvelopePriming(1, false);
    }

    SECTION("explicit topology generation")
    {
        checkHqTopologyResetSoloEnvelopePriming(3, true);
    }
}

TEST_CASE("An unprepared processor can release its resources",
          "[processor][lifecycle][release]")
{
    FireAudioProcessor processor;
    REQUIRE_NOTHROW(processor.releaseResources());
}
