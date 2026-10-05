#include "helpers/ProcessingLatency.h"
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int routeTransitionSamples = 480;
constexpr int legacyDetachSamples = 2400;
constexpr int warmupSamples = 8192;
constexpr int endpointWindowSamples = 64;
constexpr int baseAutomationSamples = 1536;
constexpr float tolerance = 2.0e-4f;
constexpr std::array<float, 2> inputValues { 0.63f, 0.47f };
const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

enum class RecipeChange
{
    attach,
    activeRecipe
};

const char* changeName(RecipeChange change)
{
    return change == RecipeChange::attach ? "attach" : "active recipe";
}

struct OutputRecipe
{
    bool routed = false;
    float baseDb = 0.0f;
    int sourceIndex = -1;
    float depth = 0.5f;
    bool bipolar = true;
};

std::pair<OutputRecipe, OutputRecipe> recipes(RecipeChange change)
{
    if (change == RecipeChange::attach)
        return { { false, -12.0f, -1, 0.5f, true },
                 { true, -12.0f, 0, 0.6f, true } };

    // One transition deliberately changes every audible routed-recipe field.
    return { { true, -12.0f, 0, 0.4f, true },
             { true, -24.0f, 1, 0.3f, false } };
}

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterId,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterId);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String bandParameter(const juce::String& baseId)
{
    return ParameterIDAndName::getIDString(baseId, 0);
}

LfoData constantLfo(float value)
{
    LfoData shape;
    shape.points = { { 0.0f, value }, { 1.0f, value } };
    shape.curvatures = { 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

void applyGlobalRecipe(FireAudioProcessor& processor,
                       const OutputRecipe& recipe)
{
    setPlainParameter(processor, OUTPUT_ID, recipe.baseDb);
    if (! recipe.routed)
    {
        processor.clearModulationForParameter(OUTPUT_ID);
        return;
    }

    processor.assignLfoToTarget(recipe.sourceIndex, OUTPUT_ID);
    processor.setModulationDepth(OUTPUT_ID, recipe.depth);
    const auto info = processor.getModulationInfoForParameter(OUTPUT_ID);
    REQUIRE(info.isModulated);
    if (info.isBipolar != recipe.bipolar)
        processor.toggleBipolarMode(OUTPUT_ID);
}

void configureGlobalProcessor(FireAudioProcessor& processor,
                              int preparedBlockSize,
                              bool useHq,
                              const OutputRecipe& recipe)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    setPlainParameter(processor, bandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, bandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, bandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, bandParameter(MODE_ID), 4.0f); // identity below 1
    setPlainParameter(processor, bandParameter(OUTPUT_ID), 0.0f);
    setPlainParameter(processor, bandParameter(MIX_ID), 1.0f);

    for (int lfo = 0; lfo < 2; ++lfo)
    {
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, lfo),
                          0.0f);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, lfo),
                          100.0f);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(LFO_PHASE_ID, lfo),
                          0.0f);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(LFO_SMOOTH_ID, lfo),
                          0.0f);
    }
    processor.getLfoManager().setLfoData(0, constantLfo(0.25f));
    processor.getLfoManager().setLfoData(1, constantLfo(0.75f));
    applyGlobalRecipe(processor, recipe);
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

juce::AudioBuffer<float> makeInput(int numSamples)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel),
                                          inputValues[static_cast<size_t>(channel)],
                                          numSamples);
    return buffer;
}

struct TransitionRender
{
    std::array<std::vector<float>, 2> subject;
    std::array<std::vector<float>, 2> oldReference;
    std::array<std::vector<float>, 2> newReference;
    int audibleEventOffset = 0;
    float preEventError = 0.0f;
    bool finite = true;
};

TransitionRender renderGlobalTransition(RecipeChange change,
                                        bool useHq,
                                        const std::vector<int>& pattern)
{
    REQUIRE_FALSE(pattern.empty());
    const int preparedBlockSize = *std::max_element(pattern.begin(), pattern.end());
    const auto [oldRecipe, newRecipe] = recipes(change);
    FireAudioProcessor subject;
    FireAudioProcessor alwaysOld;
    FireAudioProcessor alwaysNew;
    configureGlobalProcessor(subject, preparedBlockSize, useHq, oldRecipe);
    configureGlobalProcessor(alwaysOld, preparedBlockSize, useHq, oldRecipe);
    configureGlobalProcessor(alwaysNew, preparedBlockSize, useHq, newRecipe);

    TransitionRender result;
    result.audibleEventOffset = fire::tests::legacyControlOutputDelay(useHq, subject.getLatencySamples());
    REQUIRE(result.audibleEventOffset >= 0);
    REQUIRE(alwaysOld.getLatencySamples() == subject.getLatencySamples());
    REQUIRE(alwaysNew.getLatencySamples() == subject.getLatencySamples());
    const int capturedSamples = result.audibleEventOffset
                                + routeTransitionSamples
                                + endpointWindowSamples;
    for (auto* output : { &result.subject,
                          &result.oldReference,
                          &result.newReference })
        for (auto& channel : *output)
            channel.reserve(static_cast<size_t>(capturedSamples));

    int streamPosition = 0;
    size_t callbackIndex = 0;
    bool eventApplied = false;
    juce::MidiBuffer midi;
    const int totalSamples = warmupSamples + capturedSamples;
    while (streamPosition < totalSamples)
    {
        int blockSize = pattern[callbackIndex++ % pattern.size()];
        REQUIRE(blockSize > 0);
        blockSize = std::min(blockSize, totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            blockSize = std::min(blockSize, warmupSamples - streamPosition);
        if (streamPosition == warmupSamples && ! eventApplied)
        {
            applyGlobalRecipe(subject, newRecipe);
            eventApplied = true;
        }

        const auto input = makeInput(blockSize);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> oldBuffer;
        juce::AudioBuffer<float> newBuffer;
        subjectBuffer.makeCopyOf(input);
        oldBuffer.makeCopyOf(input);
        newBuffer.makeCopyOf(input);
        subject.processBlock(subjectBuffer, midi);
        alwaysOld.processBlock(oldBuffer, midi);
        alwaysNew.processBlock(newBuffer, midi);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float oldValue = oldBuffer.getSample(channel, sample);
                const float newValue = newBuffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(oldValue)
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
                result.newReference[index].push_back(newValue);
            }
        }
        streamPosition += blockSize;
    }
    REQUIRE(eventApplied);
    return result;
}

float maximumSubjectDifference(const TransitionRender& lhs,
                               const TransitionRender& rhs)
{
    float error = 0.0f;
    for (size_t channel = 0; channel < lhs.subject.size(); ++channel)
    {
        REQUIRE(lhs.subject[channel].size() == rhs.subject[channel].size());
        for (size_t sample = 0; sample < lhs.subject[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(lhs.subject[channel][sample]
                                      - rhs.subject[channel][sample]));
    }
    return error;
}

void checkGlobalTransition(const TransitionRender& render,
                           RecipeChange change,
                           bool useHq)
{
    REQUIRE(render.finite);
    CHECK(render.preEventError <= 1.0e-6f);
    const int first = render.audibleEventOffset;
    const int midpoint = first + routeTransitionSamples / 2;
    const int settled = first + routeTransitionSamples;
    float minimumSeparation = std::numeric_limits<float>::max();
    float firstError = 0.0f;
    float midpointError = 0.0f;
    float settledError = 0.0f;
    float maximumStep = 0.0f;
    for (size_t channel = 0; channel < render.subject.size(); ++channel)
    {
        const auto& subject = render.subject[channel];
        const auto& oldReference = render.oldReference[channel];
        const auto& newReference = render.newReference[channel];
        REQUIRE(static_cast<int>(subject.size())
                >= settled + endpointWindowSamples);
        const auto firstIndex = static_cast<size_t>(first);
        const auto midpointIndex = static_cast<size_t>(midpoint);
        const float separation = std::abs(oldReference[firstIndex]
                                          - newReference[firstIndex]);
        minimumSeparation = std::min(minimumSeparation, separation);
        firstError = std::max(firstError,
                              std::abs(subject[firstIndex]
                                       - oldReference[firstIndex]));
        const float expectedMidpoint = oldReference[midpointIndex]
                                     + 0.5f
                                           * (newReference[midpointIndex]
                                              - oldReference[midpointIndex]);
        midpointError = std::max(midpointError,
                                 std::abs(subject[midpointIndex]
                                          - expectedMidpoint));
        for (int sample = std::max(1, first);
             sample < settled;
             ++sample)
            maximumStep = std::max(
                maximumStep,
                std::abs(subject[static_cast<size_t>(sample)]
                         - subject[static_cast<size_t>(sample - 1)]));
        for (int sample = settled;
             sample < settled + endpointWindowSamples;
             ++sample)
            settledError = std::max(
                settledError,
                std::abs(subject[static_cast<size_t>(sample)]
                         - newReference[static_cast<size_t>(sample)]));
    }

    CAPTURE(changeName(change),
            useHq,
            render.audibleEventOffset,
            minimumSeparation,
            firstError,
            midpointError,
            maximumStep,
            settledError);
    REQUIRE(minimumSeparation >= 0.02f);
    CHECK(firstError <= tolerance);
    CHECK(midpointError <= tolerance);
    CHECK(maximumStep <= minimumSeparation * 0.05f);
    CHECK(settledError <= tolerance);
}

struct GlobalBaseAutomationRender
{
    float maximumReferenceError = 0.0f;
    float expectedGainRange = 0.0f;
    bool finite = true;
};

float globalAutomatedBaseTarget(int eventSample, int callbackSamples)
{
    const float progress = juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(eventSample + callbackSamples)
            / static_cast<float>(baseAutomationSamples));
    return -24.0f + progress * 18.0f;
}

float routedGainForBase(float baseDb)
{
    ModulatedValueProvider provider;
    provider.baseValue = baseDb;
    provider.range = { -48.0f, 6.0f };
    provider.modulationDepth = 0.6f;
    provider.isBipolar = true;
    constexpr float lfoValue = 0.25f;
    provider.lfoSignal = &lfoValue;
    return juce::Decibels::decibelsToGain(provider.get(0));
}

GlobalBaseAutomationRender renderGlobalRoutedBaseAutomation(
    bool useHq,
    const std::vector<int>& callbackPattern)
{
    REQUIRE_FALSE(callbackPattern.empty());
    for (const int blockSize : callbackPattern)
        REQUIRE(blockSize > 0);

    const OutputRecipe initialRecipe { true, -24.0f, 0, 0.6f, true };
    const OutputRecipe carrierRecipe { false, 0.0f, -1, 0.0f, true };
    FireAudioProcessor subject;
    FireAudioProcessor carrier;
    const int preparedBlockSize = std::max(
        512,
        *std::max_element(callbackPattern.begin(), callbackPattern.end()));
    configureGlobalProcessor(subject,
                             preparedBlockSize,
                             useHq,
                             initialRecipe);
    configureGlobalProcessor(carrier,
                             preparedBlockSize,
                             useHq,
                             carrierRecipe);
    REQUIRE(subject.getLatencySamples() == carrier.getLatencySamples());
    const auto* subjectOutputValue = subject.treeState.getRawParameterValue(
        OUTPUT_ID);
    REQUIRE(subjectOutputValue != nullptr);
    const int audibleControlDelay = fire::tests::legacyControlOutputDelay(useHq, subject.getLatencySamples());

    juce::MidiBuffer midi;
    for (int processed = 0; processed < warmupSamples; processed += 512)
    {
        const int blockSize = std::min(512, warmupSamples - processed);
        auto subjectBuffer = makeInput(blockSize);
        auto carrierBuffer = makeInput(blockSize);
        subject.processBlock(subjectBuffer, midi);
        carrier.processBlock(carrierBuffer, midi);
    }

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        expectedBaseGain;
    expectedBaseGain.reset(sampleRate, 0.05);
    expectedBaseGain.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(initialRecipe.baseDb));

    GlobalBaseAutomationRender result;
    std::vector<float> expectedControlGains;
    const int eventSamples = baseAutomationSamples + audibleControlDelay;
    expectedControlGains.reserve(static_cast<size_t>(eventSamples));
    float minimumExpectedGain = std::numeric_limits<float>::max();
    float maximumExpectedGain = 0.0f;
    int eventPosition = 0;
    size_t callbackIndex = 0;
    while (eventPosition < eventSamples)
    {
        int blockSize = callbackPattern[
            callbackIndex++ % callbackPattern.size()];
        blockSize = std::min(blockSize, eventSamples - eventPosition);
        const int automationPosition = std::min(eventPosition,
                                                baseAutomationSamples);
        const int automatedSamplesThisCallback = std::min(
            blockSize,
            baseAutomationSamples - automationPosition);
        const float targetDb = automationPosition < baseAutomationSamples
                                   ? globalAutomatedBaseTarget(
                                         automationPosition,
                                         automatedSamplesThisCallback)
                                   : -6.0f;
        setPlainParameter(subject, OUTPUT_ID, targetDb);
        expectedBaseGain.setTargetValue(
            juce::Decibels::decibelsToGain(
                subjectOutputValue->load(std::memory_order_relaxed)));

        auto subjectBuffer = makeInput(blockSize);
        auto carrierBuffer = makeInput(blockSize);
        subject.processBlock(subjectBuffer, midi);
        carrier.processBlock(carrierBuffer, midi);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float smoothedBaseDb = juce::Decibels::gainToDecibels(
                expectedBaseGain.getNextValue());
            const float controlGain = routedGainForBase(smoothedBaseDb);
            expectedControlGains.push_back(controlGain);
            minimumExpectedGain = std::min(minimumExpectedGain, controlGain);
            maximumExpectedGain = std::max(maximumExpectedGain, controlGain);

            const int outputSample = eventPosition + sample;
            const int controlSample = outputSample - audibleControlDelay;
            const float audibleGain = controlSample >= 0
                                          ? expectedControlGains[
                                                static_cast<size_t>(controlSample)]
                                          : routedGainForBase(
                                                initialRecipe.baseDb);
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float expected = carrierBuffer.getSample(channel, sample)
                                     * audibleGain;
                result.finite = result.finite && std::isfinite(actual)
                                && std::isfinite(expected);
                result.maximumReferenceError = std::max(
                    result.maximumReferenceError,
                    std::abs(actual - expected));
            }
        }

        eventPosition += blockSize;
    }

    result.expectedGainRange = maximumExpectedGain - minimumExpectedGain;
    return result;
}

BandProcessingParameters makeDetachParameters(bool routed)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 8;
    params.isSafeModeOn = false;
    params.isShapeEnabled = true;
    params.shapeMixVal = 0.0f;
    params.mixVal = 1.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.outputVal.baseValue = routed ? -12.0f : -6.0f;
    if (routed)
    {
        params.outputVal.modulationDepth = 0.6f;
        params.outputVal.isBipolar = true;
        params.outputLfoSourceIndex = 0;
    }
    return params;
}
} // namespace

TEST_CASE("Global Output LFO recipe transitions are continuous",
          "[processor][global-output][lfo][transition][block-size]")
{
    for (const auto change : { RecipeChange::attach,
                               RecipeChange::activeRecipe })
        for (const bool useHq : { false, true })
        {
            DYNAMIC_SECTION(changeName(change) << ", HQ=" << useHq)
            {
                const auto fixed = renderGlobalTransition(change,
                                                          useHq,
                                                          fixedCallbacks);
                const auto irregular = renderGlobalTransition(change,
                                                              useHq,
                                                              irregularCallbacks);
                checkGlobalTransition(fixed, change, useHq);
                checkGlobalTransition(irregular, change, useHq);
                const float partitionError = maximumSubjectDifference(fixed,
                                                                      irregular);
                CAPTURE(partitionError);
                CHECK(partitionError <= tolerance);
            }
        }
}

TEST_CASE("Global Output continuous routed-base automation does not restart the route bridge",
          "[processor][global-output][lfo][base-automation][block-size]")
{
    const std::array<std::vector<int>, 3> callbackPatterns {
        fixedCallbacks,
        std::vector<int> { 1 },
        irregularCallbacks
    };

    for (const bool useHq : { false, true })
        for (const auto& pattern : callbackPatterns)
        {
            const auto result = renderGlobalRoutedBaseAutomation(useHq,
                                                                 pattern);
            CAPTURE(useHq,
                    pattern.front(),
                    pattern.size(),
                    result.maximumReferenceError,
                    result.expectedGainRange);
            REQUIRE(result.finite);
            REQUIRE(result.expectedGainRange >= 0.015f);
            CHECK(result.maximumReferenceError <= tolerance);
        }
}

TEST_CASE("Detaching Band Output LFO retains the legacy fifty millisecond ramp",
          "[band][output-gain][lfo][transition][detach]")
{
    BandProcessor band;
    band.prepare({ sampleRate, 257, 2 });
    const auto routed = makeDetachParameters(true);
    const auto detached = makeDetachParameters(false);
    juce::AudioBuffer<float> lfo(1, warmupSamples);
    juce::FloatVectorOperations::fill(lfo.getWritePointer(0),
                                      0.75f,
                                      warmupSamples);
    auto warmup = makeInput(warmupSamples);
    band.process(warmup, routed, lfo);

    constexpr int capture = legacyDetachSamples + endpointWindowSamples;
    juce::AudioBuffer<float> eventLfo(1, capture);
    juce::FloatVectorOperations::fill(eventLfo.getWritePointer(0),
                                      0.75f,
                                      capture);
    auto output = makeInput(capture);
    band.process(output, detached, eventLfo);

    auto oldProvider = routed.outputVal;
    const float lfoValue = 0.75f;
    oldProvider.lfoSignal = &lfoValue;
    const float oldGain = juce::Decibels::decibelsToGain(oldProvider.get(0));
    const float newGain = juce::Decibels::decibelsToGain(
        detached.outputVal.baseValue);
    float maximumError = 0.0f;
    bool finite = true;
    for (int sample = 0; sample < capture; ++sample)
    {
        const float mix = std::min(1.0f,
                                   static_cast<float>(sample + 1)
                                       / static_cast<float>(legacyDetachSamples));
        const float expectedGain = oldGain + mix * (newGain - oldGain);
        for (int channel = 0; channel < 2; ++channel)
        {
            const float actual = output.getSample(channel, sample);
            const float expected = inputValues[static_cast<size_t>(channel)]
                                 * expectedGain;
            finite = finite && std::isfinite(actual);
            maximumError = std::max(maximumError,
                                    std::abs(actual - expected));
        }
    }
    CAPTURE(oldGain, newGain, maximumError);
    REQUIRE(finite);
    CHECK(maximumError <= 3.0e-5f);
}
