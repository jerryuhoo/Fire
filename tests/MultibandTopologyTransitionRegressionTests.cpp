#include "helpers/ProcessingLatency.h"
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int warmupSamples = 12000;
constexpr int transitionSamples = 480; // Proposed 10 ms topology crossfade.
constexpr int topologyRampSamples = 240; // 5 ms at 48 kHz.
constexpr int topologyWarmupSamples = 48; // At least 1 ms completely muted.
constexpr int topologyTotalTransitionSamples = 2 * topologyRampSamples
                                             + topologyWarmupSamples;
constexpr int transitionGuardSamples = 128;
constexpr int finalComparisonSamples = 2048;
constexpr float endpointTolerance = 2.0e-4f;

struct TopologyEvent
{
    int sample = 0;
    int numBands = 1;
    int hqState = -1;
};

struct Timeline
{
    std::array<std::vector<float>, 2> output;
    bool finite = true;
    bool latencyInvariant = true;
    int reportedLatency = 0;
};

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

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double phase = channel == 0 ? 0.13 : 0.61;
    const float dc = channel == 0 ? 0.105f : -0.082f;
    return dc
         + 0.16f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 73.0 * time + phase))
         + 0.13f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 619.0 * time
                   + 0.37 - phase))
         + 0.105f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 3101.0 * time
                   + 0.19 + phase))
         + 0.072f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 8701.0 * time
                   + 0.43 - 0.4 * phase));
}

void configureProcessor(FireAudioProcessor& processor,
                        int initialBandCount,
                        bool useHq,
                        int prepareCapacity = preparedBlockSize)
{
    REQUIRE(initialBandCount >= 1);
    REQUIRE(initialBandCount <= 4);
    REQUIRE(prepareCapacity > 0);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor,
                      NUM_BANDS_ID,
                      static_cast<float>(initialBandCount));
    constexpr std::array<float, 3> crossoverFrequencies {
        500.0f, 2500.0f, 7000.0f
    };
    for (int divider = 0; divider < 3; ++divider)
    {
        setPlainParameter(processor,
                          bandParameter(FREQ_ID, divider),
                          crossoverFrequencies[static_cast<size_t>(divider)]);
        setPlainParameter(processor,
                          bandParameter(LINE_STATE_ID, divider),
                          divider < initialBandCount - 1 ? 1.0f : 0.0f);
    }

    constexpr std::array<float, 4> bandOutputDb {
        0.0f, -6.0f, 3.0f, -3.0f
    };
    for (int band = 0; band < 4; ++band)
    {
        setPlainParameter(processor, bandParameter(BAND_ENABLE_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(BAND_SOLO_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(LINKED_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(COMP_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DC_FILTER_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(SAFE_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(EXTREME_ID, band), 0.0f);
        // Hard clip is exactly linear for this fixture's sub-unity signal.
        setPlainParameter(processor, bandParameter(MODE_ID, band), 4.0f);
        setPlainParameter(processor, bandParameter(DRIVE_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(BIAS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(REC_ID, band), 0.0f);
        setPlainParameter(processor,
                          bandParameter(OUTPUT_ID, band),
                          bandOutputDb[static_cast<size_t>(band)]);
        setPlainParameter(processor, bandParameter(MIX_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(SHAPE_MIX_ID, band), 1.0f);
    }
    processor.prepareToPlay(sampleRate, prepareCapacity);
}

Timeline render(int initialBandCount,
                bool useHq,
                const std::vector<TopologyEvent>& events,
                const std::vector<int>& blockPattern,
                int totalSamples,
                int numChannels = 2,
                int prepareCapacity = preparedBlockSize)
{
    REQUIRE_FALSE(blockPattern.empty());
    REQUIRE((numChannels == 1 || numChannels == 2));
    for (const int blockSize : blockPattern)
        REQUIRE(blockSize > 0);

    FireAudioProcessor processor;
    setLayout(processor, numChannels);
    configureProcessor(processor,
                       initialBandCount,
                       useHq,
                       prepareCapacity);
    Timeline result;
    result.reportedLatency = processor.getLatencySamples();
    REQUIRE(result.reportedLatency > 0);
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(totalSamples));

    int streamPosition = 0;
    size_t eventIndex = 0;
    size_t blockIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].sample == streamPosition)
        {
            REQUIRE(events[eventIndex].numBands >= 1);
            REQUIRE(events[eventIndex].numBands <= 4);
            setPlainParameter(processor,
                              NUM_BANDS_ID,
                              static_cast<float>(events[eventIndex].numBands));
            if (events[eventIndex].hqState >= 0)
            {
                REQUIRE(events[eventIndex].hqState <= 1);
                setPlainParameter(
                    processor,
                    HQ_ID,
                    static_cast<float>(events[eventIndex].hqState));
            }
            ++eventIndex;
        }

        const int nextEvent = eventIndex < events.size()
                                  ? events[eventIndex].sample
                                  : totalSamples;
        REQUIRE(nextEvent > streamPosition);
        const int samplesThisBlock = std::min(
            blockPattern[blockIndex % blockPattern.size()],
            std::min(totalSamples - streamPosition,
                     nextEvent - streamPosition));
        ++blockIndex;
        juce::AudioBuffer<float> buffer(numChannels, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
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
        if (first.output[channel].empty())
            continue;
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

float maximumAdjacentStep(const Timeline& timeline,
                          int firstSample,
                          int numSamples)
{
    REQUIRE(firstSample > 0);
    float maximum = 0.0f;
    for (const auto& channel : timeline.output)
    {
        REQUIRE(firstSample + numSamples <= static_cast<int>(channel.size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
            maximum = std::max(
                maximum,
                std::abs(channel[static_cast<size_t>(sample)]
                         - channel[static_cast<size_t>(sample - 1)]));
    }
    return maximum;
}

float maximumMagnitude(const Timeline& timeline,
                       int firstSample,
                       int numSamples)
{
    REQUIRE(firstSample >= 0);
    float maximum = 0.0f;
    for (const auto& channel : timeline.output)
    {
        if (channel.empty())
            continue;
        REQUIRE(firstSample + numSamples <= static_cast<int>(channel.size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
            maximum = std::max(
                maximum,
                std::abs(channel[static_cast<size_t>(sample)]));
    }
    return maximum;
}

float maximumBufferDifference(const juce::AudioBuffer<float>& first,
                              const juce::AudioBuffer<float>& second)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());
    float maximum = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            maximum = std::max(
                maximum,
                std::abs(first.getSample(channel, sample)
                         - second.getSample(channel, sample)));
    return maximum;
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite(buffer.getSample(channel, sample)))
                return false;
    return true;
}

void checkTopologyChange(int targetBandCount, bool useHq)
{
    constexpr int initialBandCount = 2;
    const int totalSamples = warmupSamples
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 32;
    const std::vector<int> pattern { preparedBlockSize };
    const auto subject = render(initialBandCount,
                                useHq,
                                { { warmupSamples, targetBandCount } },
                                pattern,
                                totalSamples);
    const auto fromReference = render(initialBandCount,
                                      useHq,
                                      {},
                                      pattern,
                                      totalSamples);
    const auto toReference = render(targetBandCount,
                                    useHq,
                                    {},
                                    pattern,
                                    totalSamples);
    REQUIRE(subject.finite);
    REQUIRE(subject.latencyInvariant);
    REQUIRE(fromReference.finite);
    REQUIRE(fromReference.latencyInvariant);
    REQUIRE(toReference.finite);
    REQUIRE(toReference.latencyInvariant);
    REQUIRE(subject.reportedLatency == fromReference.reportedLatency);
    REQUIRE(subject.reportedLatency == toReference.reportedLatency);

    // HQ's natural band latency is upstream of the topology sum, while Base
    // receives the fixed integer PDC pad after the complete processing path.
    const int eventOutputOffset = fire::tests::legacyTransitionEnvelopeDelay(useHq, subject.reportedLatency);
    const int firstAudibleSample = warmupSamples + eventOutputOffset;
    const float initialError = maximumDifference(subject,
                                                 fromReference,
                                                 subject.reportedLatency + 64,
                                                 warmupSamples
                                                     - subject.reportedLatency
                                                     - 128);
    const float firstOldError = maximumDifference(subject,
                                                  fromReference,
                                                  firstAudibleSample,
                                                  1);
    const float firstEndpointSeparation = maximumDifference(fromReference,
                                                            toReference,
                                                            firstAudibleSample,
                                                            1);
    constexpr int stepWindowSamples = 96;
    const float subjectMaximumStep = maximumAdjacentStep(subject,
                                                         firstAudibleSample,
                                                         stepWindowSamples);
    const float endpointMaximumStep = std::max(
        maximumAdjacentStep(fromReference,
                            firstAudibleSample,
                            stepWindowSamples),
        maximumAdjacentStep(toReference,
                            firstAudibleSample,
                            stepWindowSamples));
    const int finalStateStart = firstAudibleSample
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
    CAPTURE(initialBandCount,
            targetBandCount,
            useHq,
            subject.reportedLatency,
            initialError,
            firstOldError,
            firstEndpointSeparation,
            subjectMaximumStep,
            endpointMaximumStep,
            finalEndpointSeparation,
            finalError);
    CHECK(initialError < endpointTolerance);
    REQUIRE(firstEndpointSeparation > 1.0e-3f);
    CHECK(firstOldError < endpointTolerance);
    CHECK(subjectMaximumStep <= endpointMaximumStep * 1.25f + 0.01f);
    REQUIRE(finalEndpointSeparation > 0.02f);
    CHECK(finalError < endpointTolerance);
}
} // namespace

TEST_CASE("Live multiband topology add/remove starts from the audible topology",
          "[processor][multiband][topology][transition]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const bool useHq : { false, true })
        for (const int targetBandCount : { 1, 3 })
        {
            DYNAMIC_SECTION("2->" << targetBandCount << ", HQ=" << useHq)
            {
                checkTopologyChange(targetBandCount, useHq);
            }
        }
}

TEST_CASE("A topology transition completes inside one 8192-sample callback",
          "[processor][multiband][topology][transition][large-block]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int oversizedCallbackSamples = 8192;
    constexpr int targetBandCount = 3;
    const int totalSamples = warmupSamples + oversizedCallbackSamples;
    for (const bool useHq : { false, true })
        for (const int numChannels : { 1, 2 })
        {
            DYNAMIC_SECTION("HQ=" << useHq
                            << ", channels=" << numChannels)
            {
                const std::vector<int> callback { oversizedCallbackSamples };
                const auto subject = render(2,
                                            useHq,
                                            { { warmupSamples,
                                                targetBandCount } },
                                            callback,
                                            totalSamples,
                                            numChannels,
                                            oversizedCallbackSamples);
                // No-op events align the reference callback boundary with the
                // real edit without changing either endpoint topology.
                const auto oldReference = render(2,
                                                 useHq,
                                                 { { warmupSamples, 2 } },
                                                 callback,
                                                 totalSamples,
                                                 numChannels,
                                                 oversizedCallbackSamples);
                const auto finalReference = render(
                    targetBandCount,
                    useHq,
                    { { warmupSamples, targetBandCount } },
                    callback,
                    totalSamples,
                    numChannels,
                    oversizedCallbackSamples);
                const int outputOffset = fire::tests::legacyTransitionEnvelopeDelay(useHq, subject.reportedLatency);
                const int firstAudibleSample = warmupSamples + outputOffset;
                const int warmWindowStart = firstAudibleSample
                                          + topologyRampSamples;
                const int finalWindowStart = firstAudibleSample
                                           + topologyTotalTransitionSamples
                                           + transitionGuardSamples;
                const float firstOldError = maximumDifference(
                    subject, oldReference, firstAudibleSample, 1);
                const float firstEndpointSeparation = maximumDifference(
                    oldReference, finalReference, firstAudibleSample, 1);
                const float finalEndpointSeparation = maximumDifference(
                    oldReference,
                    finalReference,
                    finalWindowStart,
                    finalComparisonSamples);
                const float finalError = maximumDifference(
                    subject,
                    finalReference,
                    finalWindowStart,
                    finalComparisonSamples);
                const float preWarmMagnitude = maximumMagnitude(
                    subject,
                    warmWindowStart - 64,
                    64);
                const float warmMagnitude = maximumMagnitude(
                    subject,
                    warmWindowStart,
                    topologyWarmupSamples);
                const float postWarmMagnitude = maximumMagnitude(
                    subject,
                    warmWindowStart + topologyWarmupSamples,
                    64);
                CAPTURE(useHq,
                        numChannels,
                        subject.reportedLatency,
                        firstOldError,
                        firstEndpointSeparation,
                        finalEndpointSeparation,
                        finalError,
                        preWarmMagnitude,
                        warmMagnitude,
                        postWarmMagnitude);
                CHECK(subject.finite);
                CHECK(subject.latencyInvariant);
                CHECK(subject.reportedLatency == oldReference.reportedLatency);
                CHECK(subject.reportedLatency
                      == finalReference.reportedLatency);
                REQUIRE(firstEndpointSeparation > 1.0e-3f);
                CHECK(firstOldError < endpointTolerance);
                REQUIRE(preWarmMagnitude > 1.0e-3f);
                CHECK(warmMagnitude < 1.0e-7f);
                REQUIRE(postWarmMagnitude > 1.0e-3f);
                REQUIRE(finalEndpointSeparation > 0.02f);
                CHECK(finalError < endpointTolerance);
            }
        }
}

TEST_CASE("Topology transitions are independent of host callback partition",
          "[processor][multiband][topology][transition][partition]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int targetBandCount = 3;
    const int totalSamples = warmupSamples
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 64;
    const std::vector<TopologyEvent> events {
        { warmupSamples, targetBandCount }
    };
    const std::vector<int> fixedBlocks { preparedBlockSize };
    const std::vector<int> irregularBlocks {
        13, 251, 7, 97, 2, 173, 61
    };

    for (const bool useHq : { false, true })
    {
        DYNAMIC_SECTION("HQ=" << useHq)
        {
            const auto fixed = render(2,
                                      useHq,
                                      events,
                                      fixedBlocks,
                                      totalSamples);
            const auto irregular = render(2,
                                          useHq,
                                          events,
                                          irregularBlocks,
                                          totalSamples);
            const float partitionError = maximumDifference(
                fixed,
                irregular,
                warmupSamples,
                totalSamples - warmupSamples);
            CAPTURE(useHq,
                    fixed.reportedLatency,
                    irregular.reportedLatency,
                    partitionError);
            CHECK(fixed.finite);
            CHECK(irregular.finite);
            CHECK(fixed.latencyInvariant);
            CHECK(irregular.latencyInvariant);
            CHECK(fixed.reportedLatency == irregular.reportedLatency);
            CHECK(partitionError < endpointTolerance);
        }
    }
}

TEST_CASE("Steady topology processing remains bit-exact without an edit",
          "[processor][multiband][topology][transition][steady]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int noOpEventSample = 16 * preparedBlockSize;
    constexpr int totalSamples = 8192;
    const std::vector<int> blocks { preparedBlockSize };
    for (const bool useHq : { false, true })
    {
        DYNAMIC_SECTION("HQ=" << useHq)
        {
            const auto eventless = render(2,
                                          useHq,
                                          {},
                                          blocks,
                                          totalSamples);
            const auto sameCountNotification = render(
                2,
                useHq,
                { { noOpEventSample, 2 } },
                blocks,
                totalSamples);
            const float compatibilityError = maximumDifference(
                eventless,
                sameCountNotification,
                0,
                totalSamples);
            CAPTURE(useHq,
                    eventless.reportedLatency,
                    compatibilityError);
            CHECK(eventless.finite);
            CHECK(sameCountNotification.finite);
            CHECK(eventless.latencyInvariant);
            CHECK(sameCountNotification.latencyInvariant);
            CHECK(eventless.reportedLatency
                  == sameCountNotification.reportedLatency);
            CHECK(compatibilityError == 0.0f);
        }
    }
}

TEST_CASE("Simultaneous HQ and topology requests serialize to the final target",
          "[processor][multiband][topology][transition][hq]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int targetBandCount = 3;
    // Each state machine owns a 5 ms out + 1 ms warm + 5 ms in sequence.
    // Two callback boundaries cover the hand-off because topology must not
    // reset the graph inside an active HQ reset.
    constexpr int serialTransitionBudget = 2 * topologyTotalTransitionSamples
                                         + 2 * preparedBlockSize
                                         + transitionGuardSamples;
    const int totalSamples = warmupSamples
                           + serialTransitionBudget
                           + finalComparisonSamples
                           + 128;
    const std::vector<int> blocks { preparedBlockSize };
    const auto subject = render(2,
                                false,
                                { { warmupSamples, targetBandCount, 1 } },
                                blocks,
                                totalSamples);
    const auto finalReference = render(
        targetBandCount,
        true,
        { { warmupSamples, targetBandCount, 1 } },
        blocks,
        totalSamples);
    const auto oldTopologyReference = render(
        2,
        true,
        { { warmupSamples, 2, 1 } },
        blocks,
        totalSamples);
    const int finalWindowStart = warmupSamples
                               + serialTransitionBudget
                               + subject.reportedLatency;
    const float finalError = maximumDifference(subject,
                                                finalReference,
                                                finalWindowStart,
                                                finalComparisonSamples);
    const float topologyEndpointSeparation = maximumDifference(
        finalReference,
        oldTopologyReference,
        finalWindowStart,
        finalComparisonSamples);
    CAPTURE(subject.reportedLatency,
            finalReference.reportedLatency,
            finalError,
            topologyEndpointSeparation);
    CHECK(subject.finite);
    CHECK(subject.latencyInvariant);
    CHECK(subject.reportedLatency == finalReference.reportedLatency);
    REQUIRE(topologyEndpointSeparation > 0.02f);
    CHECK(finalError < endpointTolerance);
}

TEST_CASE("A topology request completes behind the transparent host-bypass tap",
          "[processor][multiband][topology][transition][bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int bypassBlocks = 4;
    constexpr int targetBandCount = 3;

    for (const bool useHq : { false, true })
    {
        DYNAMIC_SECTION("HQ=" << useHq)
        {
            FireAudioProcessor subject;
            FireAudioProcessor bypassReference;
            FireAudioProcessor continuousReference;
            for (auto* processor : { &subject,
                                     &bypassReference,
                                     &continuousReference })
                setLayout(*processor, 2);
            configureProcessor(subject, 2, useHq);
            configureProcessor(bypassReference, 2, useHq);
            configureProcessor(continuousReference, 2, useHq);

            const int fixedLatency = subject.getLatencySamples();
            int streamPosition = 0;
            float warmupError = 0.0f;
            float bypassError = 0.0f;
            float bypassMagnitude = 0.0f;
            float firstResumeError = 0.0f;
            float finalError = 0.0f;
            float finalEndpointSeparation = 0.0f;
            bool allFinite = true;
            int resumeSample = -1;

            const auto processThree = [&] (int numSamples,
                                           bool bypassed,
                                           bool compareBypass,
                                           bool compareFinal)
            {
                auto subjectOutput = juce::AudioBuffer<float>(2, numSamples);
                auto bypassOutput = juce::AudioBuffer<float>(2, numSamples);
                auto continuousOutput = juce::AudioBuffer<float>(2,
                                                                  numSamples);
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < numSamples; ++sample)
                    {
                        const float value = inputSample(
                            channel, streamPosition + sample);
                        subjectOutput.setSample(channel, sample, value);
                        bypassOutput.setSample(channel, sample, value);
                        continuousOutput.setSample(channel, sample, value);
                    }

                juce::MidiBuffer subjectMidi;
                juce::MidiBuffer bypassMidi;
                juce::MidiBuffer continuousMidi;
                if (bypassed)
                {
                    subject.processBlockBypassed(subjectOutput, subjectMidi);
                    bypassReference.processBlockBypassed(bypassOutput,
                                                         bypassMidi);
                    continuousReference.processBlock(continuousOutput,
                                                     continuousMidi);
                }
                else
                {
                    subject.processBlock(subjectOutput, subjectMidi);
                    bypassReference.processBlock(bypassOutput, bypassMidi);
                    continuousReference.processBlock(continuousOutput,
                                                     continuousMidi);
                }

                allFinite = allFinite && isFinite(subjectOutput)
                                      && isFinite(bypassOutput)
                                      && isFinite(continuousOutput);
                if (streamPosition < warmupSamples)
                    warmupError = std::max(
                        warmupError,
                        maximumBufferDifference(subjectOutput,
                                                bypassOutput));
                if (compareBypass)
                {
                    bypassError = std::max(
                        bypassError,
                        maximumBufferDifference(subjectOutput,
                                                bypassOutput));
                    bypassMagnitude = std::max(
                        bypassMagnitude,
                        subjectOutput.getMagnitude(0,
                                                   subjectOutput.getNumSamples()));
                }
                if (compareFinal)
                {
                    const float callbackError = maximumBufferDifference(
                        subjectOutput, continuousOutput);
                    finalError = std::max(finalError, callbackError);
                    if (streamPosition == resumeSample)
                        firstResumeError = callbackError;
                    finalEndpointSeparation = std::max(
                        finalEndpointSeparation,
                        maximumBufferDifference(continuousOutput,
                                                bypassOutput));
                }
                streamPosition += numSamples;
            };

            while (streamPosition < warmupSamples)
                processThree(std::min(preparedBlockSize,
                                      warmupSamples - streamPosition),
                             false,
                             false,
                             false);

            setPlainParameter(subject,
                              NUM_BANDS_ID,
                              static_cast<float>(targetBandCount));
            setPlainParameter(continuousReference,
                              NUM_BANDS_ID,
                              static_cast<float>(targetBandCount));
            for (int block = 0; block < bypassBlocks; ++block)
                processThree(preparedBlockSize, true, true, false);

            resumeSample = streamPosition;
            const int endSample = resumeSample + finalComparisonSamples;
            while (streamPosition < endSample)
            {
                processThree(std::min(preparedBlockSize,
                                      endSample - streamPosition),
                             false,
                             false,
                             true);
            }

            CAPTURE(useHq,
                    fixedLatency,
                    warmupError,
                    bypassError,
                    bypassMagnitude,
                    firstResumeError,
                    finalError,
                    finalEndpointSeparation);
            CHECK(allFinite);
            CHECK(subject.getLatencySamples() == fixedLatency);
            CHECK(warmupError < 1.0e-6f);
            CHECK(bypassError < 1.0e-6f);
            REQUIRE(bypassMagnitude > 0.01f);
            REQUIRE(finalEndpointSeparation > 0.02f);
            CHECK(firstResumeError < endpointTolerance);
            CHECK(finalError < endpointTolerance);
        }
    }
}

TEST_CASE("Rapid topology automation keeps the latest complete target",
          "[processor][multiband][topology][transition][rapid]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int firstTarget = 3;
    constexpr int rapidOffsetSamples = 160;
    const int secondEventSample = warmupSamples + rapidOffsetSamples;
    const int totalSamples = secondEventSample
                           + transitionSamples
                           + transitionGuardSamples
                           + finalComparisonSamples
                           + 64;
    const std::vector<int> blocks { preparedBlockSize };

    for (const bool useHq : { false, true })
        for (const int latestTarget : { 2, 1 })
        {
            DYNAMIC_SECTION("HQ=" << useHq
                            << ", latest=" << latestTarget)
            {
                const std::vector<TopologyEvent> events {
                    { warmupSamples, firstTarget },
                    { secondEventSample, latestTarget }
                };
                const std::vector<TopologyEvent> oldReferenceEvents {
                    { warmupSamples, 2 },
                    { secondEventSample, 2 }
                };
                const std::vector<TopologyEvent> finalReferenceEvents {
                    { warmupSamples, latestTarget },
                    { secondEventSample, latestTarget }
                };
                const auto subject = render(2,
                                            useHq,
                                            events,
                                            blocks,
                                            totalSamples);
                const auto oldReference = render(2,
                                                 useHq,
                                                 oldReferenceEvents,
                                                 blocks,
                                                 totalSamples);
                const auto finalReference = render(
                    latestTarget,
                    useHq,
                    finalReferenceEvents,
                    blocks,
                    totalSamples);
                const int outputOffset = fire::tests::legacyTransitionEnvelopeDelay(useHq, subject.reportedLatency);
                const int firstAudibleSample = warmupSamples + outputOffset;
                const int finalWindowStart = secondEventSample
                                           + outputOffset
                                           + transitionSamples
                                           + transitionGuardSamples;
                const float firstOldError = maximumDifference(
                    subject, oldReference, firstAudibleSample, 1);
                const float initialEndpointSeparation = maximumDifference(
                    oldReference,
                    render(firstTarget,
                           useHq,
                           { { warmupSamples, firstTarget },
                             { secondEventSample, firstTarget } },
                           blocks,
                           totalSamples),
                    firstAudibleSample,
                    1);
                const float finalError = maximumDifference(
                    subject,
                    finalReference,
                    finalWindowStart,
                    finalComparisonSamples);
                const float finalOldSeparation = maximumDifference(
                    finalReference,
                    oldReference,
                    finalWindowStart,
                    finalComparisonSamples);
                CAPTURE(useHq,
                        latestTarget,
                        subject.reportedLatency,
                        firstOldError,
                        initialEndpointSeparation,
                        finalError,
                        finalOldSeparation);
                CHECK(subject.finite);
                CHECK(subject.latencyInvariant);
                REQUIRE(initialEndpointSeparation > 1.0e-3f);
                CHECK(firstOldError < endpointTolerance);
                CHECK(finalError < endpointTolerance);
                if (latestTarget != 2)
                    REQUIRE(finalOldSeparation > 0.02f);
            }
        }
}
