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
constexpr int rapidReenableSample = warmupSamples + 160;
constexpr int dcChangeSample = warmupSamples + 800;
constexpr int reenableSample = warmupSamples + 4800;
constexpr int finalStateSamples = 4096;
constexpr int totalSamples = reenableSample
                           + transitionSamples
                           + finalStateSamples
                           + 4;

enum class ToggleTarget
{
    dcFilter,
    shape
};

enum class RenderState
{
    alwaysEnabled,
    alwaysDisabled,
    enabledDisabledEnabled,
    enabledThenDisabled,
    rapidEnabledDisabledEnabled
};

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool finite = true;
    int numChannels = 2;
};

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const float dc = absoluteSample < dcChangeSample ? 0.62f : -0.47f;
    const double channelPhase = channel == 0 ? 0.19 : 0.73;
    return dc
         + 0.07f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 401.0 * time
               + channelPhase))
         + 0.035f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 977.0 * time
               - channelPhase));
}

BandProcessingParameters makeParameters(bool useHq,
                                        ToggleTarget target,
                                        bool enabled)
{
    BandProcessingParameters params;
    params.mode = 4; // Hard clip is exactly linear for this sub-unity fixture.
    params.isHQ = useHq;
    params.isDriveEnabled = false;
    params.isShapeEnabled = target == ToggleTarget::shape ? enabled : true;
    params.isDcFilterEnabled = enabled && params.isShapeEnabled;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.isSafeModeOn = false;
    params.driveVal.baseValue = 0.0f;
    params.biasVal.baseValue = 0.0f;
    params.recVal.baseValue = 0.0f;
    params.outputVal.baseValue = 0.0f;
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

bool enabledAtSample(RenderState state, int absoluteSample)
{
    switch (state)
    {
        case RenderState::alwaysEnabled:
            return true;
        case RenderState::alwaysDisabled:
            return false;
        case RenderState::enabledDisabledEnabled:
            return absoluteSample < warmupSamples
                || absoluteSample >= reenableSample;
        case RenderState::enabledThenDisabled:
            return absoluteSample < warmupSamples;
        case RenderState::rapidEnabledDisabledEnabled:
            return absoluteSample < warmupSamples
                || absoluteSample >= rapidReenableSample;
    }

    return false;
}

int nextStateBoundary(RenderState state, int absoluteSample)
{
    if (state == RenderState::enabledDisabledEnabled)
    {
        if (absoluteSample < warmupSamples)
            return warmupSamples;
        if (absoluteSample < reenableSample)
            return reenableSample;
    }
    else if (state == RenderState::enabledThenDisabled)
    {
        if (absoluteSample < warmupSamples)
            return warmupSamples;
    }
    else if (state == RenderState::rapidEnabledDisabledEnabled)
    {
        if (absoluteSample < warmupSamples)
            return warmupSamples;
        if (absoluteSample < rapidReenableSample)
            return rapidReenableSample;
    }
    return totalSamples;
}

RenderResult render(ToggleTarget target,
                    RenderState state,
                    bool useHq,
                    const std::vector<int>& hostBlockPattern,
                    int numChannels = 2,
                    int processorBlockSize = preparedBlockSize,
                    bool resetBeforeProcessing = true,
                    bool dirtyBeforeReset = false)
{
    REQUIRE(numChannels >= 1);
    REQUIRE(numChannels <= 2);
    REQUIRE(processorBlockSize > 0);
    REQUIRE((! dirtyBeforeReset || resetBeforeProcessing));
    REQUIRE(! hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    BandProcessor processor;
    processor.prepare({ sampleRate,
                        static_cast<juce::uint32>(processorBlockSize),
                        static_cast<juce::uint32>(numChannels) });
    // FireAudioProcessor primes this public per-band gain immediately after
    // prepare. Mirror the real call site so this test isolates DC/Shape state
    // rather than juce::dsp::Gain's direct-construction startup ramp.
    processor.gain.setRampDurationSeconds(0.0);
    processor.gain.setGainDecibels(0.0f);
    processor.gain.setRampDurationSeconds(0.05);
    if (dirtyBeforeReset)
    {
        const bool dirtyEnabled = ! enabledAtSample(state, 0);
        for (int blockIndex = 0; blockIndex < 4; ++blockIndex)
        {
            juce::AudioBuffer<float> dirtyBuffer(numChannels,
                                                 processorBlockSize);
            juce::AudioBuffer<float> dirtyLfo(4, processorBlockSize);
            dirtyLfo.clear();
            for (int channel = 0; channel < numChannels; ++channel)
                for (int sample = 0; sample < processorBlockSize; ++sample)
                    dirtyBuffer.setSample(
                        channel,
                        sample,
                        inputSample(channel,
                                    warmupSamples
                                        + blockIndex * processorBlockSize
                                        + sample));
            processor.process(dirtyBuffer,
                              makeParameters(useHq, target, dirtyEnabled),
                              dirtyLfo);
        }
    }
    if (resetBeforeProcessing)
        processor.reset();

    RenderResult result;
    result.numChannels = numChannels;
    for (int channel = 0; channel < numChannels; ++channel)
        result.output[static_cast<size_t>(channel)].reserve(totalSamples);

    size_t patternIndex = 0;
    int streamPosition = 0;
    while (streamPosition < totalSamples)
    {
        const int requestedBlock = hostBlockPattern[
            patternIndex % hostBlockPattern.size()];
        ++patternIndex;
        const int samplesThisBlock = std::min(
            requestedBlock,
            std::min(totalSamples - streamPosition,
                     nextStateBoundary(state, streamPosition)
                         - streamPosition));
        REQUIRE(samplesThisBlock > 0);

        juce::AudioBuffer<float> buffer(numChannels, samplesThisBlock);
        juce::AudioBuffer<float> lfoOutputs(4, samplesThisBlock);
        lfoOutputs.clear();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));

        const auto params = makeParameters(
            useHq,
            target,
            enabledAtSample(state, streamPosition));
        processor.process(buffer, params, lfoOutputs);

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

float maximumDifference(const RenderResult& first,
                        const RenderResult& second,
                        int firstSample,
                        int numSamples)
{
    float maximumError = 0.0f;
    REQUIRE(first.numChannels == second.numChannels);
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
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][static_cast<size_t>(sample)]
                         - second.output[channel][static_cast<size_t>(sample)]));
        }
    }
    return maximumError;
}

void checkTransition(ToggleTarget target,
                     bool useHq,
                     int numChannels = 2)
{
    const auto subject = render(target,
                                RenderState::enabledDisabledEnabled,
                                useHq,
                                { preparedBlockSize },
                                numChannels);
    const auto alwaysEnabled = render(target,
                                      RenderState::alwaysEnabled,
                                      useHq,
                                      { preparedBlockSize },
                                      numChannels);
    const auto alwaysDisabled = render(target,
                                       RenderState::alwaysDisabled,
                                       useHq,
                                       { preparedBlockSize },
                                       numChannels);
    REQUIRE(subject.finite);
    REQUIRE(alwaysEnabled.finite);
    REQUIRE(alwaysDisabled.finite);

    const float offEndpointSeparation = maximumDifference(alwaysEnabled,
                                                          alwaysDisabled,
                                                          warmupSamples,
                                                          transitionSamples);
    const float offFirstFromError = maximumDifference(subject,
                                                      alwaysEnabled,
                                                      warmupSamples,
                                                      1);
    const float offMiddleFromError = maximumDifference(
        subject,
        alwaysEnabled,
        warmupSamples + transitionSamples / 2,
        1);
    const float offMiddleToError = maximumDifference(
        subject,
        alwaysDisabled,
        warmupSamples + transitionSamples / 2,
        1);
    const float offFinalError = maximumDifference(subject,
                                                  alwaysDisabled,
                                                  warmupSamples
                                                      + transitionSamples
                                                      + 2,
                                                  reenableSample
                                                      - warmupSamples
                                                      - transitionSamples
                                                      - 2);

    const float onEndpointSeparation = maximumDifference(alwaysEnabled,
                                                         alwaysDisabled,
                                                         reenableSample,
                                                         transitionSamples);
    const float onFirstFromError = maximumDifference(subject,
                                                     alwaysDisabled,
                                                     reenableSample,
                                                     1);
    const float onMiddleFromError = maximumDifference(
        subject,
        alwaysDisabled,
        reenableSample + transitionSamples / 2,
        1);
    const float onMiddleToError = maximumDifference(
        subject,
        alwaysEnabled,
        reenableSample + transitionSamples / 2,
        1);
    const float onFinalError = maximumDifference(subject,
                                                 alwaysEnabled,
                                                 reenableSample
                                                     + transitionSamples
                                                     + 2,
                                                 finalStateSamples);
    CAPTURE(target == ToggleTarget::dcFilter,
            useHq,
            numChannels,
            offEndpointSeparation,
            offFirstFromError,
            offMiddleFromError,
            offMiddleToError,
            offFinalError,
            onEndpointSeparation,
            onFirstFromError,
            onMiddleFromError,
            onMiddleToError,
            onFinalError);

    REQUIRE(offEndpointSeparation > 0.35f);
    REQUIRE(onEndpointSeparation > 0.30f);

    // Each direction starts on the currently audible endpoint, spends the
    // 10 ms transition at neither endpoint, and then reaches the requested
    // state. The final enabled comparison also proves that the high-pass wet
    // state followed the DC change while its output was bypassed.
    CHECK(offFirstFromError < 2.0e-4f);
    CHECK(offMiddleFromError > offEndpointSeparation * 0.05f);
    CHECK(offMiddleToError > offEndpointSeparation * 0.05f);
    CHECK(offFinalError < 2.0e-4f);
    CHECK(onFirstFromError < 2.0e-4f);
    CHECK(onMiddleFromError > onEndpointSeparation * 0.05f);
    CHECK(onMiddleToError > onEndpointSeparation * 0.05f);
    CHECK(onFinalError < 2.0e-4f);
}

void checkHostPartitionInvariance(ToggleTarget target, bool useHq)
{
    const auto fixedBlocks = render(target,
                                    RenderState::enabledDisabledEnabled,
                                    useHq,
                                    { preparedBlockSize });
    const auto irregularBlocks = render(target,
                                        RenderState::enabledDisabledEnabled,
                                        useHq,
                                        { 7, 113, 19, 251, 37, 83, 2, 173 });
    REQUIRE(fixedBlocks.finite);
    REQUIRE(irregularBlocks.finite);
    const float maximumError = maximumDifference(fixedBlocks,
                                                 irregularBlocks,
                                                 0,
                                                 totalSamples);
    CAPTURE(target == ToggleTarget::dcFilter, useHq, maximumError);
    CHECK(maximumError < 2.0e-4f);
}

RenderResult applyCanonicalDcFilter(const RenderResult& dry)
{
    REQUIRE(dry.numChannels >= 1);
    REQUIRE(dry.numChannels <= 2);
    const int numSamples = static_cast<int>(dry.output.front().size());
    REQUIRE(numSamples > 0);

    juce::AudioBuffer<float> buffer(dry.numChannels, numSamples);
    for (int channel = 0; channel < dry.numChannels; ++channel)
    {
        const auto& source = dry.output[static_cast<size_t>(channel)];
        REQUIRE(static_cast<int>(source.size()) == numSamples);
        buffer.copyFrom(channel, 0, source.data(), numSamples);
    }

    BandProcessor::DCFilter filter;
    filter.prepare({ sampleRate,
                     static_cast<juce::uint32>(numSamples),
                     static_cast<juce::uint32>(dry.numChannels) });
    *filter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate, 20.0f);
    auto block = juce::dsp::AudioBlock<float>(buffer);
    filter.process(juce::dsp::ProcessContextReplacing<float>(block));

    RenderResult result = dry;
    for (int channel = 0; channel < dry.numChannels; ++channel)
    {
        auto& destination = result.output[static_cast<size_t>(channel)];
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float value = buffer.getSample(channel, sample);
            result.finite = result.finite && std::isfinite(value);
            destination[static_cast<size_t>(sample)] = value;
        }
    }
    return result;
}

float maximumInputDifference(const RenderResult& result,
                             int firstSample,
                             int numSamples)
{
    float maximumError = 0.0f;
    for (int channel = 0; channel < result.numChannels; ++channel)
    {
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            maximumError = std::max(
                maximumError,
                std::abs(result.output[static_cast<size_t>(channel)]
                                      [static_cast<size_t>(sample)]
                         - inputSample(channel, sample)));
        }
    }
    return maximumError;
}

void checkInitialEndpointCompatibility(ToggleTarget target,
                                       bool useHq,
                                       int numChannels)
{
    const auto preparedEnabled = render(target,
                                        RenderState::alwaysEnabled,
                                        useHq,
                                        { preparedBlockSize },
                                        numChannels,
                                        preparedBlockSize,
                                        false);
    const auto preparedDisabled = render(target,
                                         RenderState::alwaysDisabled,
                                         useHq,
                                         { preparedBlockSize },
                                         numChannels,
                                         preparedBlockSize,
                                         false);
    const auto resetEnabled = render(target,
                                     RenderState::alwaysEnabled,
                                     useHq,
                                     { preparedBlockSize },
                                     numChannels,
                                     preparedBlockSize,
                                     true,
                                     true);
    const auto resetDisabled = render(target,
                                      RenderState::alwaysDisabled,
                                      useHq,
                                      { preparedBlockSize },
                                      numChannels,
                                      preparedBlockSize,
                                      true,
                                      true);
    const auto canonicalEnabled = applyCanonicalDcFilter(preparedDisabled);
    REQUIRE(preparedEnabled.finite);
    REQUIRE(preparedDisabled.finite);
    REQUIRE(resetEnabled.finite);
    REQUIRE(resetDisabled.finite);
    REQUIRE(canonicalEnabled.finite);

    const float enabledCanonicalError = maximumDifference(
        preparedEnabled,
        canonicalEnabled,
        0,
        preparedBlockSize);
    const float prepareResetEnabledError = maximumDifference(preparedEnabled,
                                                             resetEnabled,
                                                             0,
                                                             totalSamples);
    const float prepareResetDisabledError = maximumDifference(preparedDisabled,
                                                              resetDisabled,
                                                              0,
                                                              totalSamples);
    const float endpointSeparation = maximumDifference(preparedEnabled,
                                                       preparedDisabled,
                                                       0,
                                                       preparedBlockSize);
    const float disabledInputError = ! useHq
                                         ? maximumInputDifference(
                                             preparedDisabled,
                                             0,
                                             preparedBlockSize)
                                         : 0.0f;
    CAPTURE(target == ToggleTarget::dcFilter,
            useHq,
            numChannels,
            enabledCanonicalError,
            prepareResetEnabledError,
            prepareResetDisabledError,
            endpointSeparation,
            disabledInputError,
            preparedDisabled.output[0][0],
            preparedEnabled.output[0][0],
            inputSample(0, 0));

    REQUIRE(endpointSeparation > 0.15f);
    CHECK(enabledCanonicalError < 2.0e-4f);
    CHECK(prepareResetEnabledError < 2.0e-4f);
    CHECK(prepareResetDisabledError < 2.0e-4f);
    if (! useHq)
        CHECK(disabledInputError < 2.0e-4f);
}

void checkRapidReverse(ToggleTarget target, bool useHq, int numChannels)
{
    const auto subject = render(target,
                                RenderState::rapidEnabledDisabledEnabled,
                                useHq,
                                { preparedBlockSize },
                                numChannels);
    const auto continuingOff = render(target,
                                      RenderState::enabledThenDisabled,
                                      useHq,
                                      { preparedBlockSize },
                                      numChannels);
    const auto alwaysEnabled = render(target,
                                      RenderState::alwaysEnabled,
                                      useHq,
                                      { preparedBlockSize },
                                      numChannels);
    REQUIRE(subject.finite);
    REQUIRE(continuingOff.finite);
    REQUIRE(alwaysEnabled.finite);

    const float endpointSeparation = maximumDifference(continuingOff,
                                                       alwaysEnabled,
                                                       rapidReenableSample,
                                                       transitionSamples);
    const float firstDisableError = maximumDifference(subject,
                                                      alwaysEnabled,
                                                      warmupSamples,
                                                      1);
    const float firstReverseError = maximumDifference(subject,
                                                      continuingOff,
                                                      rapidReenableSample,
                                                      1);
    const float middleFromError = maximumDifference(
        subject,
        continuingOff,
        rapidReenableSample + transitionSamples / 2,
        1);
    const float middleToError = maximumDifference(
        subject,
        alwaysEnabled,
        rapidReenableSample + transitionSamples / 2,
        1);
    const float finalError = maximumDifference(subject,
                                               alwaysEnabled,
                                               rapidReenableSample
                                                   + transitionSamples
                                                   + 2,
                                               finalStateSamples);
    CAPTURE(target == ToggleTarget::dcFilter,
            useHq,
            numChannels,
            endpointSeparation,
            firstDisableError,
            firstReverseError,
            middleFromError,
            middleToError,
            finalError);

    REQUIRE(endpointSeparation > 0.25f);
    CHECK(firstDisableError < 2.0e-4f);
    CHECK(firstReverseError < 2.0e-4f);
    CHECK(middleFromError > endpointSeparation * 0.05f);
    CHECK(middleToError > endpointSeparation * 0.02f);
    CHECK(finalError < 2.0e-4f);
}

void checkOversizedInternalChunkInvariance(ToggleTarget target, bool useHq)
{
    constexpr int hostCallbackSize = 480;
    const auto nativeCapacity = render(target,
                                       RenderState::enabledDisabledEnabled,
                                       useHq,
                                       { hostCallbackSize },
                                       2,
                                       hostCallbackSize);
    const auto smallCapacity = render(target,
                                      RenderState::enabledDisabledEnabled,
                                      useHq,
                                      { hostCallbackSize },
                                      2,
                                      64);
    REQUIRE(nativeCapacity.finite);
    REQUIRE(smallCapacity.finite);
    const float maximumError = maximumDifference(nativeCapacity,
                                                 smallCapacity,
                                                 0,
                                                 totalSamples);
    CAPTURE(target == ToggleTarget::dcFilter, useHq, maximumError);
    CHECK(maximumError < 2.0e-4f);
}
} // namespace

TEST_CASE("Per-band DC and Shape bypasses transition without clicks",
          "[band][dc-filter][shape][transition]")
{
    for (const bool useHq : { false, true })
    {
        SECTION(useHq ? "DC Filter HQ" : "DC Filter non-HQ")
        {
            checkTransition(ToggleTarget::dcFilter, useHq);
        }

        SECTION(useHq ? "Shape HQ" : "Shape non-HQ")
        {
            checkTransition(ToggleTarget::shape, useHq);
        }
    }
}

TEST_CASE("Per-band DC and Shape transitions ignore host block partitioning",
          "[band][dc-filter][shape][transition][block-size]")
{
    for (const bool useHq : { false, true })
    {
        checkHostPartitionInvariance(ToggleTarget::dcFilter, useHq);
        checkHostPartitionInvariance(ToggleTarget::shape, useHq);
    }
}

TEST_CASE("Per-band DC endpoints snap on prepare and reset",
          "[band][dc-filter][shape][transition][priming][reset]")
{
    for (const int numChannels : { 1, 2 })
        for (const bool useHq : { false, true })
        {
            checkInitialEndpointCompatibility(ToggleTarget::dcFilter,
                                              useHq,
                                              numChannels);
            checkInitialEndpointCompatibility(ToggleTarget::shape,
                                              useHq,
                                              numChannels);
        }
}

TEST_CASE("Rapid per-band DC reversals retarget the active ramp",
          "[band][dc-filter][shape][transition][rapid]")
{
    for (const bool useHq : { false, true })
    {
        checkRapidReverse(ToggleTarget::dcFilter, useHq, 2);
        checkRapidReverse(ToggleTarget::shape, useHq, 2);
    }
}

TEST_CASE("Per-band DC and Shape transitions support mono",
          "[band][dc-filter][shape][transition][mono]")
{
    for (const bool useHq : { false, true })
    {
        checkTransition(ToggleTarget::dcFilter, useHq, 1);
        checkTransition(ToggleTarget::shape, useHq, 1);
    }
}

TEST_CASE("Per-band DC transitions survive oversized internal chunks",
          "[band][dc-filter][shape][transition][oversized][chunk]")
{
    for (const bool useHq : { false, true })
    {
        checkOversizedInternalChunkInvariance(ToggleTarget::dcFilter, useHq);
        checkOversizedInternalChunkInvariance(ToggleTarget::shape, useHq);
    }
}
