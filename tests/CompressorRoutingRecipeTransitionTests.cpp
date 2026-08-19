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
constexpr int transitionSamples = 480;
constexpr int warmupSamples = 4096;
constexpr int responseGuardSamples = 32;
constexpr int capturedSamples = transitionSamples + responseGuardSamples + 1;
constexpr float comparisonTolerance = 2.0e-4f;
constexpr std::array<float, 4> constantLfoValues { 0.0f, 1.0f, 0.25f, 0.5f };
constexpr std::array<float, 2> steadyInputs { 0.80f, 0.67f };
constexpr std::array<float, 2> attackPulses { 0.90f, 0.72f };
constexpr std::array<float, 2> releaseHighInputs { 1.00f, 0.85f };
constexpr std::array<float, 2> releaseLowInputs { 0.20f, 0.25f };
const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

enum class CompressorCoreTarget
{
    threshold,
    ratio,
    attack,
    release
};

struct Recipe
{
    bool routed = false;
    float scalarValue = 0.0f;
    float baseValue = 0.0f;
    float depth = 0.0f;
    bool bipolar = true;
    int sourceIndex = -1;
};

struct Scenario
{
    const char* name = "";
    CompressorCoreTarget target = CompressorCoreTarget::threshold;
    Recipe oldRecipe;
    Recipe newRecipe;
    float minimumEndpointSeparation = 0.0f;
    bool hasSteadyCarrier = false;
};

juce::NormalisableRange<float> targetRange(CompressorCoreTarget target)
{
    switch (target)
    {
        case CompressorCoreTarget::threshold: return { -48.0f, 0.0f };
        case CompressorCoreTarget::ratio:     return { 1.0f, 20.0f };
        case CompressorCoreTarget::attack:    return { 0.1f, 200.0f };
        case CompressorCoreTarget::release:   return { 10.0f, 2000.0f };
    }
    return {};
}

float effectiveValue(CompressorCoreTarget target, const Recipe& recipe)
{
    if (! recipe.routed)
        return recipe.scalarValue;

    REQUIRE(juce::isPositiveAndBelow(recipe.sourceIndex,
                                     static_cast<int>(constantLfoValues.size())));
    const float lfo = constantLfoValues[static_cast<size_t>(recipe.sourceIndex)];
    ModulatedValueProvider provider;
    provider.lfoSignal = &lfo;
    provider.baseValue = recipe.baseValue;
    provider.modulationDepth = recipe.depth;
    provider.isBipolar = recipe.bipolar;
    provider.range = targetRange(target);
    return provider.get(0);
}

float transitionMidpoint(CompressorCoreTarget target,
                         float oldValue,
                         float newValue)
{
    if (target == CompressorCoreTarget::attack
        || target == CompressorCoreTarget::release)
        return std::sqrt(oldValue * newValue);

    return 0.5f * (oldValue + newValue);
}

const std::array<Scenario, 5>& scenarios()
{
    static const std::array<Scenario, 5> result {
        Scenario {
            "Threshold source 0 -> 1",
            CompressorCoreTarget::threshold,
            { true, -24.0f, -24.0f, 1.0f, true, 0 },
            { true, -24.0f, -24.0f, 1.0f, true, 1 },
            0.50f,
            true
        },
        Scenario {
            "Ratio depth -1 -> +1",
            CompressorCoreTarget::ratio,
            { true, 1.0f, 10.5f, -1.0f, true, 1 },
            { true, 20.0f, 10.5f, 1.0f, true, 1 },
            0.50f,
            true
        },
        Scenario {
            "Attack attach",
            CompressorCoreTarget::attack,
            { false, 200.0f, 200.0f, 0.0f, true, -1 },
            // Keep the routed base equal to the old scalar. The event changes
            // only the route recipe; it must not be mistaken for ordinary base
            // automation by the existing Attack smoother.
            { true, 0.1f, 200.0f, -1.0f, false, 1 },
            0.05f,
            false
        },
        Scenario {
            "Release bipolar -> unipolar",
            CompressorCoreTarget::release,
            // At LFO=0, bipolar depth -1 maps the 10 ms base to 1005 ms;
            // unipolar maps the same complete recipe back to 10 ms.
            { true, 1005.0f, 10.0f, -1.0f, true, 0 },
            { true, 10.0f, 10.0f, -1.0f, false, 0 },
            5.0e-4f,
            false
        },
        Scenario {
            "Threshold detach",
            CompressorCoreTarget::threshold,
            { true, -48.0f, -24.0f, 1.0f, true, 0 },
            { false, -24.0f, -24.0f, 0.0f, true, -1 },
            0.04f,
            true
        }
    };
    return result;
}

void applyRecipe(BandProcessingParameters& params,
                 CompressorCoreTarget target,
                 const Recipe& recipe)
{
    ModulatedValueProvider* provider = nullptr;
    int* sourceIndex = nullptr;
    float* scalar = nullptr;
    switch (target)
    {
        case CompressorCoreTarget::threshold:
            provider = &params.compThresholdValProvider;
            sourceIndex = &params.compThresholdLfoSourceIndex;
            scalar = &params.compThreshold;
            break;
        case CompressorCoreTarget::ratio:
            provider = &params.compRatioValProvider;
            sourceIndex = &params.compRatioLfoSourceIndex;
            scalar = &params.compRatio;
            break;
        case CompressorCoreTarget::attack:
            provider = &params.compAttackValProvider;
            sourceIndex = &params.compAttackLfoSourceIndex;
            scalar = &params.compAttack;
            break;
        case CompressorCoreTarget::release:
            provider = &params.compReleaseValProvider;
            sourceIndex = &params.compReleaseLfoSourceIndex;
            scalar = &params.compRelease;
            break;
    }

    REQUIRE(provider != nullptr);
    REQUIRE(sourceIndex != nullptr);
    REQUIRE(scalar != nullptr);
    *scalar = recipe.scalarValue;
    provider->baseValue = recipe.baseValue;
    provider->modulationDepth = recipe.depth;
    provider->isBipolar = recipe.bipolar;
    provider->range = targetRange(target);
    *sourceIndex = recipe.routed ? recipe.sourceIndex : -1;
}

BandProcessingParameters makeParameters(const Scenario& scenario,
                                        const Recipe& recipe,
                                        bool useHq)
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
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;

    switch (scenario.target)
    {
        case CompressorCoreTarget::threshold:
            params.compThreshold = -24.0f;
            params.compRatio = 20.0f;
            params.compAttack = 0.1f;
            params.compRelease = 10.0f;
            break;
        case CompressorCoreTarget::ratio:
            params.compThreshold = -24.0f;
            params.compRatio = 10.5f;
            params.compAttack = 0.1f;
            params.compRelease = 10.0f;
            break;
        case CompressorCoreTarget::attack:
            params.compThreshold = -42.0f;
            params.compRatio = 20.0f;
            params.compAttack = 200.0f;
            params.compRelease = 0.1f;
            break;
        case CompressorCoreTarget::release:
            params.compThreshold = -6.0f;
            params.compRatio = 20.0f;
            params.compAttack = 0.1f;
            params.compRelease = 10.0f;
            break;
    }

    applyRecipe(params, scenario.target, recipe);
    return params;
}

bool isProbeSample(int eventSample)
{
    return eventSample == 0
           || eventSample == transitionSamples / 2
           || eventSample == transitionSamples;
}

juce::AudioBuffer<float> makeInput(const Scenario& scenario,
                                   int numChannels,
                                   int streamPosition,
                                   int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const int eventSample = streamPosition + sample - warmupSamples;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float value = steadyInputs[static_cast<size_t>(channel)];
            if (scenario.target == CompressorCoreTarget::attack)
                value = isProbeSample(eventSample)
                            ? attackPulses[static_cast<size_t>(channel)]
                            : 0.0f;
            else if (scenario.target == CompressorCoreTarget::release)
                value = isProbeSample(eventSample)
                            ? releaseLowInputs[static_cast<size_t>(channel)]
                            : releaseHighInputs[static_cast<size_t>(channel)];
            buffer.setSample(channel, sample, value);
        }
    }
    return buffer;
}

juce::AudioBuffer<float> makeConstantLfoBlock(int numSamples)
{
    juce::AudioBuffer<float> result(
        static_cast<int>(constantLfoValues.size()), numSamples);
    for (int channel = 0; channel < result.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(
            result.getWritePointer(channel),
            constantLfoValues[static_cast<size_t>(channel)],
            numSamples);
    return result;
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
    std::array<float, 2> lastPreEventSubject {};
};

TransitionRender renderTransition(const Scenario& scenario,
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

    const float oldValue = effectiveValue(scenario.target,
                                          scenario.oldRecipe);
    const float newValue = effectiveValue(scenario.target,
                                          scenario.newRecipe);
    const float midpointValue = transitionMidpoint(scenario.target,
                                                   oldValue,
                                                   newValue);
    Recipe midpointRecipe;
    midpointRecipe.scalarValue = midpointValue;
    midpointRecipe.baseValue = midpointValue;
    const auto oldParams = makeParameters(scenario, scenario.oldRecipe, useHq);
    const auto midpointParams = makeParameters(scenario,
                                               midpointRecipe,
                                               useHq);
    const auto newParams = makeParameters(scenario, scenario.newRecipe, useHq);

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

        const auto input = makeInput(scenario,
                                     numChannels,
                                     streamPosition,
                                     blockSize);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> oldBuffer;
        juce::AudioBuffer<float> midpointBuffer;
        juce::AudioBuffer<float> newBuffer;
        subjectBuffer.makeCopyOf(input);
        oldBuffer.makeCopyOf(input);
        midpointBuffer.makeCopyOf(input);
        newBuffer.makeCopyOf(input);
        auto lfo = makeConstantLfoBlock(blockSize);

        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? oldParams : newParams,
                        lfo);
        alwaysOld.process(oldBuffer, oldParams, lfo);
        alwaysMidpoint.process(midpointBuffer, midpointParams, lfo);
        alwaysNew.process(newBuffer, newParams, lfo);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float oldReference = oldBuffer.getSample(channel, sample);
                const float midpointReference = midpointBuffer.getSample(channel,
                                                                          sample);
                const float newReference = newBuffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(oldReference)
                                && std::isfinite(midpointReference)
                                && std::isfinite(newReference);
                if (absoluteSample < warmupSamples)
                {
                    result.preEventError = std::max(
                        result.preEventError,
                        std::abs(actual - oldReference));
                    result.lastPreEventSubject[static_cast<size_t>(channel)] = actual;
                    continue;
                }

                const auto index = static_cast<size_t>(channel);
                result.subject[index].push_back(actual);
                result.oldReference[index].push_back(oldReference);
                result.midpointReference[index].push_back(midpointReference);
                result.newReference[index].push_back(newReference);
            }
        }
        streamPosition += blockSize;
    }
    return result;
}

int findProbeOffset(const TransitionRender& result)
{
    int bestSample = 0;
    float bestSeparation = 0.0f;
    const int searchSamples = std::min(
        responseGuardSamples,
        static_cast<int>(result.subject[0].size()));
    REQUIRE(searchSamples > 0);
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

struct HqReleaseCanonicalResult
{
    bool finite = true;
    int numChannels = 0;
    float preEventError = 0.0f;
    float eventWindowError = 0.0f;
    std::array<float, 3> checkpointErrors {};
};

HqReleaseCanonicalResult renderHqReleaseDynamicCanonical(
    const Scenario& scenario,
    int numChannels)
{
    REQUIRE(scenario.target == CompressorCoreTarget::release);
    REQUIRE((numChannels == 1 || numChannels == 2));
    BandProcessor subject;
    BandProcessor dynamicReference;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        257,
        static_cast<juce::uint32>(numChannels)
    };
    subject.prepare(spec);
    dynamicReference.prepare(spec);

    const float oldRelease = effectiveValue(scenario.target,
                                            scenario.oldRecipe);
    const float newRelease = effectiveValue(scenario.target,
                                            scenario.newRecipe);
    // Keep this route signature fixed for the reference's complete lifetime.
    // With a linear 10..2000 ms range, base 1005 and unipolar depth -0.5,
    // provider output is exactly 1005 - 995 * LFO. The generated LFO therefore
    // reproduces the subject's logarithmic recipe bridge sample by sample while
    // both HQ oversamplers and detector envelopes see identical history.
    const Recipe referenceRecipe {
        true,
        oldRelease,
        oldRelease,
        -0.5f,
        false,
        0
    };
    const auto oldParams = makeParameters(scenario,
                                          scenario.oldRecipe,
                                          true);
    const auto newParams = makeParameters(scenario,
                                          scenario.newRecipe,
                                          true);
    const auto referenceParams = makeParameters(scenario,
                                                referenceRecipe,
                                                true);

    HqReleaseCanonicalResult result;
    result.numChannels = numChannels;
    const int totalSamples = warmupSamples + capturedSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int blockSize = fixedCallbacks[callbackIndex++ % fixedCallbacks.size()];
        blockSize = std::min(blockSize, totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            blockSize = std::min(blockSize, warmupSamples - streamPosition);

        const auto input = makeInput(scenario,
                                     numChannels,
                                     streamPosition,
                                     blockSize);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> referenceBuffer;
        subjectBuffer.makeCopyOf(input);
        referenceBuffer.makeCopyOf(input);
        auto subjectLfo = makeConstantLfoBlock(blockSize);
        auto referenceLfo = makeConstantLfoBlock(blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int eventSample = streamPosition + sample - warmupSamples;
            const float progress = juce::jlimit(
                0.0f,
                1.0f,
                static_cast<float>(eventSample)
                    / static_cast<float>(transitionSamples));
            const float desiredRelease = oldRelease
                                         * std::pow(newRelease / oldRelease,
                                                    progress);
            const float lfoValue = (oldRelease - desiredRelease)
                                   / (oldRelease - newRelease);
            referenceLfo.setSample(0,
                                   sample,
                                   juce::jlimit(0.0f, 1.0f, lfoValue));
        }

        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? oldParams : newParams,
                        subjectLfo);
        dynamicReference.process(referenceBuffer,
                                 referenceParams,
                                 referenceLfo);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            const int eventSample = absoluteSample - warmupSamples;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float expected = referenceBuffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(expected);
                const float error = std::abs(actual - expected);
                if (eventSample < 0)
                    result.preEventError = std::max(result.preEventError,
                                                    error);
                else
                    result.eventWindowError = std::max(
                        result.eventWindowError,
                        error);

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
        streamPosition += blockSize;
    }
    return result;
}

void checkHqReleaseDynamicCanonical(const Scenario& scenario,
                                    const HqReleaseCanonicalResult& result)
{
    CAPTURE(scenario.name,
            result.numChannels,
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

void checkTransition(const Scenario& scenario,
                     const TransitionRender& result,
                     bool useHq)
{
    REQUIRE(result.finite);
    CHECK(result.preEventError <= 1.0e-6f);
    // Threshold and Ratio use a steady carrier, so their audible event is the
    // callback's first sample even in HQ mode (the compressor is downstream of
    // the oversampler). Attack/Release need the short search because their
    // isolated probe is filtered before it reaches the compressor.
    const int probeOffset = scenario.hasSteadyCarrier
                                ? 0
                                : findProbeOffset(result);
    const int midpointSample = probeOffset + transitionSamples / 2;
    const int endpointSample = probeOffset + transitionSamples;
    float minimumSeparation = std::numeric_limits<float>::max();
    float firstOldError = 0.0f;
    float firstNewDistance = std::numeric_limits<float>::max();
    float midpointError = 0.0f;
    float endpointError = 0.0f;
    float eventStep = 0.0f;
    for (int channel = 0; channel < result.numChannels; ++channel)
    {
        const auto channelIndex = static_cast<size_t>(channel);
        const auto firstIndex = static_cast<size_t>(probeOffset);
        const auto midpointIndex = static_cast<size_t>(midpointSample);
        const auto endpointIndex = static_cast<size_t>(endpointSample);
        REQUIRE(endpointIndex < result.subject[channelIndex].size());
        const float separation = std::abs(
            result.oldReference[channelIndex][firstIndex]
            - result.newReference[channelIndex][firstIndex]);
        minimumSeparation = std::min(minimumSeparation, separation);
        firstOldError = std::max(
            firstOldError,
            std::abs(result.subject[channelIndex][firstIndex]
                     - result.oldReference[channelIndex][firstIndex]));
        firstNewDistance = std::min(
            firstNewDistance,
            std::abs(result.subject[channelIndex][firstIndex]
                     - result.newReference[channelIndex][firstIndex]));
        midpointError = std::max(
            midpointError,
            std::abs(result.subject[channelIndex][midpointIndex]
                     - result.midpointReference[channelIndex][midpointIndex]));
        endpointError = std::max(
            endpointError,
            std::abs(result.subject[channelIndex][endpointIndex]
                     - result.newReference[channelIndex][endpointIndex]));
        if (scenario.hasSteadyCarrier)
            eventStep = std::max(
                eventStep,
                std::abs(result.subject[channelIndex][0]
                         - result.lastPreEventSubject[channelIndex]));
    }

    const bool filteredProbe = scenario.target == CompressorCoreTarget::attack
                               || scenario.target
                                      == CompressorCoreTarget::release;
    const float timingTolerance = std::max(
        comparisonTolerance,
        useHq && filteredProbe ? minimumSeparation * 0.05f : 0.0f);
    CAPTURE(scenario.name,
            useHq,
            result.numChannels,
            probeOffset,
            minimumSeparation,
            firstOldError,
            firstNewDistance,
            midpointError,
            endpointError,
            eventStep,
            timingTolerance);
    REQUIRE(minimumSeparation >= scenario.minimumEndpointSeparation);
    CHECK(firstOldError <= timingTolerance);
    CHECK(firstNewDistance >= minimumSeparation * 0.75f);
    CHECK(midpointError <= timingTolerance);
    CHECK(endpointError <= comparisonTolerance);
    if (scenario.hasSteadyCarrier)
        CHECK(eventStep <= minimumSeparation * 0.08f);
}

float maximumSubjectDifference(const TransitionRender& lhs,
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

juce::AudioBuffer<float> processSteadyBlock(
    BandProcessor& band,
    const BandProcessingParameters& params,
    int numSamples,
    int numChannels = 2)
{
    REQUIRE(numSamples > 0);
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
        juce::FloatVectorOperations::fill(
            buffer.getWritePointer(channel),
            steadyInputs[static_cast<size_t>(channel)],
            numSamples);
    auto lfo = makeConstantLfoBlock(numSamples);
    band.process(buffer, params, lfo);
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

struct DynamicRender
{
    std::array<std::vector<float>, 2> output;
    float canonicalError = 0.0f;
    bool finite = true;
};

float dynamicLfoSample(int absoluteSample)
{
    constexpr int periodSamples = 32;
    const float phase = static_cast<float>(absoluteSample % periodSamples)
                        / static_cast<float>(periodSamples);
    return 0.5f + 0.45f
                    * std::sin(juce::MathConstants<float>::twoPi * phase);
}

DynamicRender renderStableDynamicThreshold(int preparedBlockSize,
                                           const std::vector<int>& hostPattern)
{
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostPattern.empty());
    constexpr int numChannels = 2;
    constexpr int measuredSamples = 1024;

    const auto& scenario = scenarios()[0];
    Recipe dynamicRecipe = scenario.oldRecipe;
    dynamicRecipe.baseValue = -24.0f;
    dynamicRecipe.depth = 0.8f;
    dynamicRecipe.sourceIndex = 0;
    const auto params = makeParameters(scenario, dynamicRecipe, false);
    BandProcessor band;
    SampleAccurateCompressor canonical;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        numChannels
    };
    band.prepare(spec);
    canonical.prepare(spec);
    canonical.setRatio(params.compRatio);
    canonical.setAttack(params.compAttack);
    canonical.setRelease(params.compRelease);

    DynamicRender result;
    for (auto& channel : result.output)
        channel.reserve(measuredSamples);

    const int totalSamples = warmupSamples + measuredSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int blockSize = hostPattern[callbackIndex++ % hostPattern.size()];
        REQUIRE(blockSize > 0);
        blockSize = std::min(blockSize, totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            blockSize = std::min(blockSize, warmupSamples - streamPosition);

        juce::AudioBuffer<float> input(numChannels, blockSize);
        for (int channel = 0; channel < numChannels; ++channel)
            juce::FloatVectorOperations::fill(
                input.getWritePointer(channel),
                steadyInputs[static_cast<size_t>(channel)],
                blockSize);
        juce::AudioBuffer<float> buffer;
        buffer.makeCopyOf(input);
        juce::AudioBuffer<float> lfo(1, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
            lfo.setSample(0,
                          sample,
                          dynamicLfoSample(streamPosition + sample));
        band.process(buffer, params, lfo);

        auto provider = params.compThresholdValProvider;
        provider.lfoSignal = lfo.getReadPointer(0);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            canonical.setThreshold(provider.get(sample));
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float expected = canonical.processSample(
                    channel,
                    input.getSample(channel, sample));
                const float actual = buffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(expected);
                if (streamPosition + sample >= warmupSamples)
                {
                    result.canonicalError = std::max(
                        result.canonicalError,
                        std::abs(actual - expected));
                    result.output[static_cast<size_t>(channel)].push_back(actual);
                }
            }
        }
        streamPosition += blockSize;
    }
    return result;
}

float maximumDynamicDifference(const DynamicRender& lhs,
                               const DynamicRender& rhs)
{
    float error = 0.0f;
    for (size_t channel = 0; channel < lhs.output.size(); ++channel)
    {
        REQUIRE(lhs.output[channel].size() == rhs.output[channel].size());
        for (size_t sample = 0; sample < lhs.output[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(lhs.output[channel][sample]
                                      - rhs.output[channel][sample]));
    }
    return error;
}
} // namespace

TEST_CASE("Compressor routed-recipe changes bridge for ten milliseconds",
          "[band][compressor][lfo][routing][recipe][transition]")
{
    for (const auto& scenario : scenarios())
        for (const bool useHq : { false, true })
            for (const int numChannels : { 1, 2 })
            {
                DYNAMIC_SECTION(scenario.name
                                << ", HQ=" << useHq
                                << ", channels=" << numChannels)
                {
                    if (scenario.target == CompressorCoreTarget::release
                        && useHq)
                    {
                        const auto result = renderHqReleaseDynamicCanonical(
                            scenario,
                            numChannels);
                        checkHqReleaseDynamicCanonical(scenario, result);
                        continue;
                    }

                    const auto result = renderTransition(scenario,
                                                         useHq,
                                                         numChannels,
                                                         257,
                                                         fixedCallbacks);
                    checkTransition(scenario, result, useHq);
                }
            }
}

TEST_CASE("Compressor recipe transitions ignore host and internal partitions",
          "[band][compressor][lfo][routing][recipe][transition][block-size][internal-chunk]")
{
    const auto& scenario = scenarios()[0];
    for (const bool useHq : { false, true })
    {
        const auto fixed = renderTransition(scenario,
                                            useHq,
                                            2,
                                            257,
                                            fixedCallbacks);
        const auto irregular = renderTransition(scenario,
                                                useHq,
                                                2,
                                                257,
                                                irregularCallbacks);
        const auto internallyChunked = renderTransition(scenario,
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

TEST_CASE("Rapid Compressor recipe retargeting is continuous and latest wins",
          "[band][compressor][lfo][routing][recipe][transition][rapid][latest]")
{
    const auto& scenario = scenarios()[0];
    Recipe latestRecipe { true, -36.0f, -24.0f, 1.0f, true, 2 };
    const auto oldParams = makeParameters(scenario,
                                          scenario.oldRecipe,
                                          false);
    const auto firstTargetParams = makeParameters(scenario,
                                                  scenario.newRecipe,
                                                  false);
    const auto latestParams = makeParameters(scenario, latestRecipe, false);
    BandProcessor subject;
    BandProcessor latestReference;
    const juce::dsp::ProcessSpec spec { sampleRate, 257, 2 };
    subject.prepare(spec);
    latestReference.prepare(spec);

    processSteadyBlock(subject, oldParams, warmupSamples);
    const auto firstTransition = processSteadyBlock(subject,
                                                    firstTargetParams,
                                                    160);
    const float anchorThreshold =
        subject.compressorThresholdRecipeTransition.lastAppliedValue;
    const auto retargetEvent = processSteadyBlock(subject, latestParams, 1);
    float retargetStep = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        retargetStep = std::max(
            retargetStep,
            std::abs(retargetEvent.getSample(channel, 0)
                     - firstTransition.getSample(channel,
                                                 firstTransition.getNumSamples()
                                                     - 1)));

    processSteadyBlock(subject, latestParams, transitionSamples - 1);
    const auto settled = processSteadyBlock(subject, latestParams, 1);
    processSteadyBlock(latestReference, latestParams, warmupSamples);
    const auto canonical = processSteadyBlock(latestReference,
                                              latestParams,
                                              1);
    const float endpointError = maximumBufferDifference(settled, canonical);
    const float expectedLatest = effectiveValue(scenario.target, latestRecipe);
    CAPTURE(anchorThreshold,
            retargetStep,
            endpointError,
            expectedLatest,
            subject.compressorThresholdRecipeTransition.lastAppliedValue,
            subject.compressorThresholdRecipeTransition.lastRecipe.sourceIndex);
    CHECK(retargetStep <= 1.0e-6f);
    CHECK(endpointError <= comparisonTolerance);
    CHECK(std::abs(subject.compressorThresholdRecipeTransition.lastAppliedValue
                   - expectedLatest)
          <= 1.0e-6f);
    CHECK(subject.compressorThresholdRecipeTransition.lastRecipe.sourceIndex
          == latestRecipe.sourceIndex);
    CHECK_FALSE(subject.compressorThresholdRecipeTransition
                    .routeTransitionMix.isSmoothing());
}

TEST_CASE("Reset discards a partial Compressor recipe transition",
          "[band][compressor][lfo][routing][recipe][transition][reset]")
{
    const auto& scenario = scenarios()[0];
    const auto oldParams = makeParameters(scenario,
                                          scenario.oldRecipe,
                                          false);
    const auto targetParams = makeParameters(scenario,
                                             scenario.newRecipe,
                                             false);
    const juce::dsp::ProcessSpec spec { sampleRate, 257, 2 };
    BandProcessor subject;
    BandProcessor sameResetReference;
    subject.prepare(spec);
    sameResetReference.prepare(spec);

    processSteadyBlock(subject, oldParams, warmupSamples);
    processSteadyBlock(subject, targetParams, 160);
    // Match the reset lifecycle of processors whose JUCE submodules preserve
    // their most recent target across reset(), while leaving the reference free
    // of any partial route bridge.
    processSteadyBlock(sameResetReference, targetParams, 1);
    subject.reset();
    sameResetReference.reset();

    const auto afterReset = processSteadyBlock(subject, targetParams, 64);
    const auto canonical = processSteadyBlock(sameResetReference,
                                              targetParams,
                                              64);
    const float resetError = maximumBufferDifference(afterReset, canonical);
    const float expectedTarget = effectiveValue(scenario.target,
                                                scenario.newRecipe);
    CAPTURE(resetError,
            expectedTarget,
            subject.compressorThresholdRecipeTransition.lastAppliedValue);
    CHECK(resetError <= 1.0e-6f);
    CHECK(std::abs(subject.compressorThresholdRecipeTransition.lastAppliedValue
                   - expectedTarget)
          <= 1.0e-6f);
    CHECK_FALSE(subject.compressorThresholdRecipeTransition
                    .routeTransitionMix.isSmoothing());
}

TEST_CASE("Stable Compressor routing preserves the full dynamic LFO trajectory",
          "[band][compressor][lfo][routing][recipe][sample-accurate][block-size][internal-chunk]")
{
    const auto fixed = renderStableDynamicThreshold(257, fixedCallbacks);
    const auto irregular = renderStableDynamicThreshold(257,
                                                        irregularCallbacks);
    const auto internallyChunked = renderStableDynamicThreshold(64,
                                                                fixedCallbacks);
    const float partitionError = maximumDynamicDifference(fixed, irregular);
    const float chunkError = maximumDynamicDifference(fixed,
                                                       internallyChunked);
    CAPTURE(fixed.canonicalError,
            irregular.canonicalError,
            internallyChunked.canonicalError,
            partitionError,
            chunkError);
    REQUIRE(fixed.finite);
    REQUIRE(irregular.finite);
    REQUIRE(internallyChunked.finite);
    CHECK(fixed.canonicalError <= 2.0e-5f);
    CHECK(irregular.canonicalError <= 2.0e-5f);
    CHECK(internallyChunked.canonicalError <= 2.0e-5f);
    CHECK(partitionError <= comparisonTolerance);
    CHECK(chunkError <= comparisonTolerance);
}
