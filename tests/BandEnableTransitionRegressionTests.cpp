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
constexpr int transitionSamples = 480; // 10 ms
constexpr int dcChangeSample = warmupSamples + 800;
constexpr int reenableSample = warmupSamples + 4800;
constexpr int finalStateSamples = 4096;

struct EnableEvent
{
    int sample = 0;
    bool enabled = true;
};

struct Timeline
{
    std::array<std::vector<float>, 2> output;
    bool finite = true;
    int latencySamples = 0;
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

int configureProcessor(FireAudioProcessor& processor,
                       bool enabled,
                       bool useHq)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    setPlainParameter(processor,
                      bandParameter(BAND_ENABLE_ID),
                      enabled ? 1.0f : 0.0f);
    setPlainParameter(processor, bandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, bandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, bandParameter(SAFE_ID), 0.0f);
    setPlainParameter(processor, bandParameter(EXTREME_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID), 1.0f);
    setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID), 1.0f);
    setPlainParameter(processor, bandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DC_FILTER_ID), 1.0f);
    setPlainParameter(processor, bandParameter(MODE_ID), 2.0f); // tanh
    setPlainParameter(processor, bandParameter(DRIVE_ID), 78.0f);
    setPlainParameter(processor, bandParameter(BIAS_ID), 0.13f);
    setPlainParameter(processor, bandParameter(REC_ID), 0.21f);
    setPlainParameter(processor, bandParameter(SHAPE_MIX_ID), 1.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID), -9.0f);
    setPlainParameter(processor, bandParameter(MIX_ID), 1.0f);

    processor.prepareToPlay(sampleRate, preparedBlockSize);
    // HQ carries its natural fractional oversampler latency through the band;
    // base mode instead receives the fixed integer output delay reported to
    // the host. Both move the audible Enable envelope, but at different sites.
    return useHq
               ? static_cast<int>(std::ceil(processor.getTotalLatency()))
               : processor.getLatencySamples();
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const float dc = absoluteSample < dcChangeSample ? 0.31f : -0.27f;
    const double phase = channel == 0 ? 0.27 : 0.91;
    return dc
         + 0.17f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 283.0 * time + phase))
         + 0.09f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 1291.0 * time
               - 0.4 * phase));
}

void setBandEnabled(FireAudioProcessor& processor, bool enabled)
{
    setPlainParameter(processor,
                      bandParameter(BAND_ENABLE_ID),
                      enabled ? 1.0f : 0.0f);
}

Timeline renderEnableEvents(bool initialEnabled,
                            const std::vector<EnableEvent>& events,
                            bool useHq,
                            const std::vector<int>& hostBlockPattern,
                            int requestedTotalSamples)
{
    REQUIRE(! hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
    {
        REQUIRE(blockSize > 0);
        REQUIRE(blockSize <= preparedBlockSize);
    }

    FireAudioProcessor processor;
    const int latencySamples = configureProcessor(processor,
                                                  initialEnabled,
                                                  useHq);
    const int totalSamples = requestedTotalSamples + latencySamples + 2;

    Timeline result;
    result.latencySamples = latencySamples;
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(totalSamples));

    int streamPosition = 0;
    size_t eventIndex = 0;
    size_t patternIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].sample == streamPosition)
        {
            setBandEnabled(processor, events[eventIndex].enabled);
            ++eventIndex;
        }

        const int nextEventSample = eventIndex < events.size()
                                        ? events[eventIndex].sample
                                        : totalSamples;
        REQUIRE(nextEventSample > streamPosition);
        const int requestedBlock = hostBlockPattern[
            patternIndex % hostBlockPattern.size()];
        ++patternIndex;
        const int samplesThisBlock = std::min(
            requestedBlock,
            std::min(totalSamples - streamPosition,
                     nextEventSample - streamPosition));
        REQUIRE(samplesThisBlock > 0);

        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));

        processor.processBlock(buffer, midi);
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

float maximumAdjacentStep(const Timeline& timeline,
                          int firstSample,
                          int numSamples)
{
    float maximumStep = 0.0f;
    for (const auto& channel : timeline.output)
    {
        for (int sample = std::max(1, firstSample);
             sample < firstSample + numSamples;
             ++sample)
        {
            maximumStep = std::max(
                maximumStep,
                std::abs(channel[static_cast<size_t>(sample)]
                         - channel[static_cast<size_t>(sample - 1)]));
        }
    }
    return maximumStep;
}

void checkEnableRoundTrip(bool useHq)
{
    const int requestedTotalSamples = reenableSample
                                    + transitionSamples
                                    + finalStateSamples;
    const std::vector<EnableEvent> events {
        { warmupSamples, false },
        { reenableSample, true }
    };
    const auto subject = renderEnableEvents(true,
                                            events,
                                            useHq,
                                            { preparedBlockSize },
                                            requestedTotalSamples);
    const auto alwaysEnabled = renderEnableEvents(true,
                                                  {},
                                                  useHq,
                                                  { preparedBlockSize },
                                                  requestedTotalSamples);
    const auto alwaysDisabled = renderEnableEvents(false,
                                                   {},
                                                   useHq,
                                                   { preparedBlockSize },
                                                   requestedTotalSamples);
    REQUIRE(subject.finite);
    REQUIRE(alwaysEnabled.finite);
    REQUIRE(alwaysDisabled.finite);
    REQUIRE(subject.latencySamples == alwaysEnabled.latencySamples);
    REQUIRE(subject.latencySamples == alwaysDisabled.latencySamples);

    const int latency = subject.latencySamples;
    const float disableSeparation = maximumDifference(alwaysEnabled,
                                                      alwaysDisabled,
                                                      warmupSamples,
                                                      transitionSamples
                                                          + latency);
    const float disableFirstError = maximumDifference(subject,
                                                      alwaysEnabled,
                                                      warmupSamples,
                                                      1);
    const float disableMiddleFromError = maximumDifference(
        subject,
        alwaysEnabled,
        warmupSamples + transitionSamples / 2,
        1);
    const float disableMiddleToError = maximumDifference(
        subject,
        alwaysDisabled,
        warmupSamples + transitionSamples / 2,
        1);
    const float disableFinalError = maximumDifference(
        subject,
        alwaysDisabled,
        warmupSamples + transitionSamples + latency + 2,
        reenableSample - warmupSamples - transitionSamples - latency - 2);

    const float enableSeparation = maximumDifference(alwaysEnabled,
                                                     alwaysDisabled,
                                                     reenableSample,
                                                     transitionSamples
                                                         + latency);
    const float enableFirstError = maximumDifference(subject,
                                                     alwaysDisabled,
                                                     reenableSample,
                                                     1);
    const float enableMiddleFromError = maximumDifference(
        subject,
        alwaysDisabled,
        reenableSample + transitionSamples / 2,
        1);
    const float enableMiddleToError = maximumDifference(
        subject,
        alwaysEnabled,
        reenableSample + transitionSamples / 2,
        1);
    const float enableFinalError = maximumDifference(
        subject,
        alwaysEnabled,
        reenableSample + transitionSamples + latency + 2,
        finalStateSamples);
    const float disableMaximumStep = maximumAdjacentStep(subject,
                                                         warmupSamples - 1,
                                                         transitionSamples + 2);
    const float enableMaximumStep = maximumAdjacentStep(subject,
                                                        reenableSample - 1,
                                                        transitionSamples + 2);
    CAPTURE(useHq,
            latency,
            disableSeparation,
            disableFirstError,
            disableMiddleFromError,
            disableMiddleToError,
            disableFinalError,
            enableSeparation,
            enableFirstError,
            enableMiddleFromError,
            enableMiddleToError,
            enableFinalError,
            disableMaximumStep,
            enableMaximumStep);

    REQUIRE(disableSeparation > 0.25f);
    REQUIRE(enableSeparation > 0.25f);
    CHECK(disableFirstError < 2.0e-4f);
    CHECK(disableMiddleFromError > disableSeparation * 0.05f);
    CHECK(disableMiddleToError > disableSeparation * 0.05f);
    CHECK(disableFinalError < 2.0e-4f);
    CHECK(enableFirstError < 2.0e-4f);
    CHECK(enableMiddleFromError > enableSeparation * 0.05f);
    CHECK(enableMiddleToError > enableSeparation * 0.05f);
    CHECK(enableFinalError < 2.0e-4f);
    CHECK(disableMaximumStep < disableSeparation * 0.25f);
    CHECK(enableMaximumStep < enableSeparation * 0.25f);
}

void checkRapidEnableRetarget(bool useHq)
{
    constexpr int reverseSample = warmupSamples + 160;
    const int requestedTotalSamples = reverseSample
                                    + transitionSamples
                                    + finalStateSamples;
    const std::vector<EnableEvent> redirectedEvents {
        { warmupSamples, false },
        { reverseSample, true }
    };
    const std::vector<EnableEvent> continuingDisableEvent {
        { warmupSamples, false }
    };
    const auto subject = renderEnableEvents(true,
                                            redirectedEvents,
                                            useHq,
                                            { preparedBlockSize },
                                            requestedTotalSamples);
    const auto continuingDisable = renderEnableEvents(
        true,
        continuingDisableEvent,
        useHq,
        { preparedBlockSize },
        requestedTotalSamples);
    const auto alwaysEnabled = renderEnableEvents(true,
                                                  {},
                                                  useHq,
                                                  { preparedBlockSize },
                                                  requestedTotalSamples);
    REQUIRE(subject.finite);
    REQUIRE(continuingDisable.finite);
    REQUIRE(alwaysEnabled.finite);

    const int latency = subject.latencySamples;
    const float endpointSeparation = maximumDifference(continuingDisable,
                                                       alwaysEnabled,
                                                       reverseSample,
                                                       transitionSamples
                                                           + latency);
    const float firstDisableError = maximumDifference(subject,
                                                      alwaysEnabled,
                                                      warmupSamples,
                                                      1);
    const float firstReverseError = maximumDifference(subject,
                                                      continuingDisable,
                                                      reverseSample,
                                                      1);
    const float middleFromError = maximumDifference(
        subject,
        continuingDisable,
        reverseSample + transitionSamples / 2,
        1);
    const float middleToError = maximumDifference(
        subject,
        alwaysEnabled,
        reverseSample + transitionSamples / 2,
        1);
    const float finalError = maximumDifference(
        subject,
        alwaysEnabled,
        reverseSample + transitionSamples + latency + 2,
        finalStateSamples);
    CAPTURE(useHq,
            latency,
            endpointSeparation,
            firstDisableError,
            firstReverseError,
            middleFromError,
            middleToError,
            finalError);

    REQUIRE(endpointSeparation > 0.20f);
    CHECK(firstDisableError < 2.0e-4f);
    CHECK(firstReverseError < 2.0e-4f);
    CHECK(middleFromError > endpointSeparation * 0.02f);
    CHECK(middleToError > endpointSeparation * 0.02f);
    CHECK(finalError < 2.0e-4f);
}

void checkHostPartitionInvariance(bool useHq)
{
    const int requestedTotalSamples = reenableSample
                                    + transitionSamples
                                    + finalStateSamples;
    const std::vector<EnableEvent> events {
        { warmupSamples, false },
        { reenableSample, true }
    };
    const auto fixedBlocks = renderEnableEvents(true,
                                                events,
                                                useHq,
                                                { preparedBlockSize },
                                                requestedTotalSamples);
    const auto irregularBlocks = renderEnableEvents(
        true,
        events,
        useHq,
        { 7, 113, 19, 251, 37, 83, 2, 173 },
        requestedTotalSamples);
    REQUIRE(fixedBlocks.finite);
    REQUIRE(irregularBlocks.finite);
    const float maximumError = maximumDifference(
        fixedBlocks,
        irregularBlocks,
        0,
        static_cast<int>(fixedBlocks.output.front().size()));
    CAPTURE(useHq, fixedBlocks.latencySamples, maximumError);
    CHECK(maximumError < 2.0e-4f);
}

struct DirectTimeline
{
    std::vector<std::vector<float>> output;
    bool finite = true;
};

enum class DirectPrimeMode
{
    fresh,
    dirtyThenReset,
    dirtyThenPrepare
};

BandProcessingParameters makeDirectParameters(bool enabled,
                                              bool useHq,
                                              float bandMix = 1.0f)
{
    BandProcessingParameters params;
    params.isBandEnabled = enabled;
    params.mode = 2; // tanh
    params.isHQ = useHq;
    params.isDriveEnabled = true;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = true;
    params.isSafeModeOn = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.driveVal.baseValue = 78.0f;
    params.biasVal.baseValue = 0.13f;
    params.recVal.baseValue = 0.21f;
    params.outputVal.baseValue = -9.0f;
    params.mixVal = bandMix;
    params.mixValProvider.baseValue = bandMix;
    params.shapeMixVal = 1.0f;
    params.shapeMixValProvider.baseValue = 1.0f;
    params.compMixVal = 0.0f;
    params.compMixValProvider.baseValue = 0.0f;
    params.widthMixVal = 0.0f;
    params.widthMixValProvider.baseValue = 0.0f;
    return params;
}

void prepareDirectBand(BandProcessor& processor,
                       int capacity,
                       int numChannels)
{
    processor.prepare({ sampleRate,
                        static_cast<juce::uint32>(capacity),
                        static_cast<juce::uint32>(numChannels) });
    // Match FireAudioProcessor::prepareToPlay: the public gain is snapped to
    // the parameter endpoint before the first audio callback.
    processor.gain.setRampDurationSeconds(0.0);
    processor.gain.setGainDecibels(-9.0f);
    processor.gain.setRampDurationSeconds(0.05);
}

void dirtyDirectBand(BandProcessor& processor,
                     bool dirtyEnabled,
                     bool useHq,
                     int capacity,
                     int numChannels,
                     float dirtyBandMix)
{
    auto params = makeDirectParameters(dirtyEnabled, useHq, dirtyBandMix);
    for (int callback = 0; callback < 4; ++callback)
    {
        juce::AudioBuffer<float> buffer(numChannels, capacity);
        juce::AudioBuffer<float> lfo(4, capacity);
        lfo.clear();
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < capacity; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             warmupSamples
                                                 + callback * capacity
                                                 + sample));
        processor.process(buffer, params, lfo);
    }
}

DirectTimeline renderDirect(bool initialEnabled,
                            const std::vector<EnableEvent>& events,
                            bool useHq,
                            int numChannels,
                            int capacity,
                            int hostBlockSize,
                            int requestedTotalSamples,
                            DirectPrimeMode primeMode = DirectPrimeMode::fresh,
                            float bandMix = 1.0f,
                            bool dirtyEnabled = true,
                            float dirtyBandMix = 1.0f)
{
    REQUIRE(numChannels >= 1);
    REQUIRE(numChannels <= 2);
    REQUIRE(capacity > 0);
    REQUIRE(hostBlockSize > 0);

    BandProcessor processor;
    prepareDirectBand(processor, capacity, numChannels);
    if (primeMode != DirectPrimeMode::fresh)
    {
        dirtyDirectBand(processor,
                        dirtyEnabled,
                        useHq,
                        capacity,
                        numChannels,
                        dirtyBandMix);
        if (primeMode == DirectPrimeMode::dirtyThenReset)
            processor.reset();
        else
            prepareDirectBand(processor, capacity, numChannels);
    }

    DirectTimeline result;
    result.output.resize(static_cast<size_t>(numChannels));
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(requestedTotalSamples));

    bool enabled = initialEnabled;
    size_t eventIndex = 0;
    int streamPosition = 0;
    while (streamPosition < requestedTotalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].sample == streamPosition)
        {
            enabled = events[eventIndex].enabled;
            ++eventIndex;
        }
        const int nextEvent = eventIndex < events.size()
                                  ? events[eventIndex].sample
                                  : requestedTotalSamples;
        REQUIRE(nextEvent > streamPosition);
        const int samplesThisBlock = std::min(
            hostBlockSize,
            std::min(requestedTotalSamples - streamPosition,
                     nextEvent - streamPosition));

        juce::AudioBuffer<float> buffer(numChannels, samplesThisBlock);
        juce::AudioBuffer<float> lfo(4, samplesThisBlock);
        lfo.clear();
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));

        processor.process(buffer,
                          makeDirectParameters(enabled, useHq, bandMix),
                          lfo);
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        streamPosition += samplesThisBlock;
    }
    return result;
}

float maximumDirectDifference(const DirectTimeline& first,
                              const DirectTimeline& second)
{
    REQUIRE(first.output.size() == second.output.size());
    float maximumError = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (size_t sample = 0; sample < first.output[channel].size(); ++sample)
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][sample]
                         - second.output[channel][sample]));
    }
    return maximumError;
}

void checkDirectPriming(bool useHq, int numChannels)
{
    constexpr int probeSamples = 4 * preparedBlockSize;
    for (const bool enabled : { false, true })
    {
        const auto fresh = renderDirect(enabled,
                                        {},
                                        useHq,
                                        numChannels,
                                        preparedBlockSize,
                                        preparedBlockSize,
                                        probeSamples);
        const auto afterReset = renderDirect(
            enabled,
            {},
            useHq,
            numChannels,
            preparedBlockSize,
            preparedBlockSize,
            probeSamples,
            DirectPrimeMode::dirtyThenReset,
            1.0f,
            ! enabled);
        const auto afterPrepareFromOpposite = renderDirect(
            enabled,
            {},
            useHq,
            numChannels,
            preparedBlockSize,
            preparedBlockSize,
            probeSamples,
            DirectPrimeMode::dirtyThenPrepare,
            1.0f,
            ! enabled);
        const auto afterPrepareFromSame = renderDirect(
            enabled,
            {},
            useHq,
            numChannels,
            preparedBlockSize,
            preparedBlockSize,
            probeSamples,
            DirectPrimeMode::dirtyThenPrepare,
            1.0f,
            enabled);
        REQUIRE(fresh.finite);
        REQUIRE(afterReset.finite);
        REQUIRE(afterPrepareFromOpposite.finite);
        REQUIRE(afterPrepareFromSame.finite);
        const float resetError = maximumDirectDifference(fresh, afterReset);
        // Both controls have identical core-DSP history. Only the audible Band
        // Enable endpoint before re-prepare differs, so this isolates whether
        // prepare clears the outer bypass smoother's primed state.
        const float reprepareError = maximumDirectDifference(
            afterPrepareFromOpposite,
            afterPrepareFromSame);
        CAPTURE(useHq, numChannels, enabled, resetError, reprepareError);
        CHECK(resetError < 2.0e-4f);
        CHECK(reprepareError < 2.0e-4f);
    }

    const auto enabled = renderDirect(true,
                                      {},
                                      useHq,
                                      numChannels,
                                      preparedBlockSize,
                                      preparedBlockSize,
                                      probeSamples);
    const auto disabled = renderDirect(false,
                                       {},
                                       useHq,
                                       numChannels,
                                       preparedBlockSize,
                                       preparedBlockSize,
                                       probeSamples);
    const float endpointSeparation = maximumDirectDifference(enabled, disabled);
    CAPTURE(useHq, numChannels, endpointSeparation);
    REQUIRE(endpointSeparation > 0.20f);
}

void checkDirectMono(bool useHq)
{
    const int total = reenableSample + transitionSamples + 1024;
    const std::vector<EnableEvent> events {
        { warmupSamples, false },
        { reenableSample, true }
    };
    const auto mono = renderDirect(true,
                                   events,
                                   useHq,
                                   1,
                                   preparedBlockSize,
                                   preparedBlockSize,
                                   total);
    const auto stereo = renderDirect(true,
                                     events,
                                     useHq,
                                     2,
                                     preparedBlockSize,
                                     preparedBlockSize,
                                     total);
    REQUIRE(mono.finite);
    REQUIRE(stereo.finite);
    REQUIRE(mono.output[0].size() == stereo.output[0].size());
    float maximumError = 0.0f;
    for (size_t sample = 0; sample < mono.output[0].size(); ++sample)
        maximumError = std::max(maximumError,
                                std::abs(mono.output[0][sample]
                                         - stereo.output[0][sample]));
    CAPTURE(useHq, maximumError);
    CHECK(maximumError < 2.0e-4f);
}

void checkDirectOversizedChunk(bool useHq)
{
    constexpr int hostBlockSize = 480;
    const int total = reenableSample + transitionSamples + 1024;
    const std::vector<EnableEvent> events {
        { warmupSamples, false },
        { reenableSample, true }
    };
    const auto native = renderDirect(true,
                                     events,
                                     useHq,
                                     2,
                                     hostBlockSize,
                                     hostBlockSize,
                                     total);
    const auto internallyChunked = renderDirect(true,
                                                events,
                                                useHq,
                                                2,
                                                64,
                                                hostBlockSize,
                                                total);
    REQUIRE(native.finite);
    REQUIRE(internallyChunked.finite);
    const float maximumError = maximumDirectDifference(native,
                                                       internallyChunked);
    CAPTURE(useHq, maximumError);
    CHECK(maximumError < 2.0e-4f);
}

void checkStaticDryDelayEquivalence(bool useHq, int numChannels)
{
    constexpr int probeSamples = 4 * preparedBlockSize;
    for (const auto primeMode : { DirectPrimeMode::fresh,
                                  DirectPrimeMode::dirtyThenReset,
                                  DirectPrimeMode::dirtyThenPrepare })
    {
        // These are intentionally separate processors and separate JUCE delay
        // implementations inside BandProcessor. Band Enable=false selects the
        // outer raw delay, while Enable=true + Band Mix=0 selects the legacy
        // inner dry delay. They must be interchangeable at a static endpoint.
        const auto outerEnableDry = renderDirect(
            false,
            {},
            useHq,
            numChannels,
            preparedBlockSize,
            preparedBlockSize,
            probeSamples,
            primeMode,
            1.0f,
            true,
            1.0f);
        const auto innerBandMixDry = renderDirect(
            true,
            {},
            useHq,
            numChannels,
            preparedBlockSize,
            preparedBlockSize,
            probeSamples,
            primeMode,
            0.0f,
            true,
            1.0f);
        REQUIRE(outerEnableDry.finite);
        REQUIRE(innerBandMixDry.finite);
        const float maximumError = maximumDirectDifference(outerEnableDry,
                                                           innerBandMixDry);
        CAPTURE(useHq,
                numChannels,
                primeMode == DirectPrimeMode::fresh,
                primeMode == DirectPrimeMode::dirtyThenReset,
                maximumError);
        CHECK(maximumError < 1.0e-6f);
    }
}
} // namespace

TEST_CASE("Band Enable transitions are click-free and keep the wet path warm",
          "[processor][band][enable][transition]")
{
    for (const bool useHq : { false, true })
        checkEnableRoundTrip(useHq);
}

TEST_CASE("Rapid Band Enable changes retarget the active ramp",
          "[processor][band][enable][transition][rapid]")
{
    for (const bool useHq : { false, true })
        checkRapidEnableRetarget(useHq);
}

TEST_CASE("Band Enable transitions ignore host callback partitioning",
          "[processor][band][enable][transition][block-size]")
{
    for (const bool useHq : { false, true })
        checkHostPartitionInvariance(useHq);
}

TEST_CASE("Band Enable endpoints snap after prepare, re-prepare, and reset",
          "[processor][band][enable][transition][priming][reset]")
{
    for (const int numChannels : { 1, 2 })
        for (const bool useHq : { false, true })
            checkDirectPriming(useHq, numChannels);
}

TEST_CASE("Band Enable transitions support mono processing",
          "[processor][band][enable][transition][mono]")
{
    for (const bool useHq : { false, true })
        checkDirectMono(useHq);
}

TEST_CASE("Band Enable transitions survive oversized internal chunks",
          "[processor][band][enable][transition][oversized][chunk]")
{
    for (const bool useHq : { false, true })
        checkDirectOversizedChunk(useHq);
}

TEST_CASE("Static Band Enable dry matches the legacy Band Mix dry delay",
          "[processor][band][enable][transition][priming][dry-reference]")
{
    for (const int numChannels : { 1, 2 })
        for (const bool useHq : { false, true })
            checkStaticDryDelayEquivalence(useHq, numChannels);
}
