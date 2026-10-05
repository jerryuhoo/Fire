#include "helpers/ProcessingLatency.h"
#include "../Source/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int warmupSamples = 8192;
constexpr int bypassIntervalSamples = 3701;
constexpr int stageTransitionSamples = static_cast<int>(sampleRate * 0.01);

enum class FilterStage
{
    lowCut,
    peak,
    highCut
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

const char* bypassParameterFor(FilterStage stage)
{
    switch (stage)
    {
        case FilterStage::lowCut: return LOWCUT_BYPASSED_ID;
        case FilterStage::peak: return PEAK_BYPASSED_ID;
        case FilterStage::highCut: return HIGHCUT_BYPASSED_ID;
    }

    jassertfalse;
    return PEAK_BYPASSED_ID;
}

void configureStage(FireAudioProcessor& processor,
                    FilterStage stage,
                    bool stageBypassed,
                    bool prepare = true)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      0.0f);

    // Exactly one chain stage is audible. The cut stages include both their
    // Butterworth chain and the same-frequency Q/gain biquad, matching the UI's
    // LOW/HIGH bypass semantics in updateLowCutFilters/updateHighCutFilters.
    setPlainParameter(processor, LOWCUT_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, PEAK_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 1.0f);

    switch (stage)
    {
        case FilterStage::lowCut:
            setPlainParameter(processor, LOWCUT_FREQ_ID, 1900.0f);
            setPlainParameter(processor, LOWCUT_GAIN_ID, 18.0f);
            setPlainParameter(processor, LOWCUT_Q_ID, 5.0f);
            setPlainParameter(processor, LOWCUT_SLOPE_ID, 3.0f);
            break;
        case FilterStage::peak:
            setPlainParameter(processor, PEAK_FREQ_ID, 1837.0f);
            setPlainParameter(processor, PEAK_GAIN_ID, 24.0f);
            setPlainParameter(processor, PEAK_Q_ID, 5.0f);
            break;
        case FilterStage::highCut:
            setPlainParameter(processor, HIGHCUT_FREQ_ID, 2100.0f);
            setPlainParameter(processor, HIGHCUT_GAIN_ID, 18.0f);
            setPlainParameter(processor, HIGHCUT_Q_ID, 5.0f);
            setPlainParameter(processor, HIGHCUT_SLOPE_ID, 3.0f);
            break;
    }

    setPlainParameter(processor,
                      bypassParameterFor(stage),
                      stageBypassed ? 1.0f : 0.0f);
    if (prepare)
        processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float primaryFrequencyFor(FilterStage stage)
{
    switch (stage)
    {
        case FilterStage::lowCut: return 1450.0f;
        case FilterStage::peak: return 1837.0f;
        case FilterStage::highCut: return 2750.0f;
    }

    return 1837.0f;
}

juce::AudioBuffer<float> makeProbeInput(FilterStage stage,
                                        int firstStreamSample,
                                        int numSamples)
{
    juce::AudioBuffer<float> input(2, numSamples);
    const auto primaryFrequency = static_cast<double>(primaryFrequencyFor(stage));
    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto absoluteSample = static_cast<double>(firstStreamSample + sample);
            const auto channelPhase = static_cast<double>(channel) * 0.31;
            const auto primaryPhase = juce::MathConstants<double>::twoPi
                                      * primaryFrequency * absoluteSample / sampleRate;
            const auto secondaryPhase = juce::MathConstants<double>::twoPi
                                        * 389.0 * absoluteSample / sampleRate;
            input.setSample(
                channel,
                sample,
                0.23f * static_cast<float>(
                            std::sin(primaryPhase + 0.21 + channelPhase))
                    + 0.11f * static_cast<float>(
                            std::cos(secondaryPhase + 0.58 + channelPhase)));
        }
    }

    return input;
}

juce::AudioBuffer<float> processCopy(FireAudioProcessor& processor,
                                     const juce::AudioBuffer<float>& input)
{
    auto output = input;
    juce::MidiBuffer midi;
    processor.processBlock(output, midi);
    return output;
}

float maximumAbsoluteDifference(const juce::AudioBuffer<float>& first,
                                const juce::AudioBuffer<float>& second)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());

    float maximumDifference = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
        {
            const float firstValue = first.getSample(channel, sample);
            const float secondValue = second.getSample(channel, sample);
            REQUIRE(std::isfinite(firstValue));
            REQUIRE(std::isfinite(secondValue));
            maximumDifference = std::max(maximumDifference,
                                         std::abs(firstValue - secondValue));
        }
    }

    return maximumDifference;
}

void prepareCanonicalChain(MonoChain& chain,
                           const ChainSettings& settings)
{
    chain.setBypassed<ChainPositions::LowCut>(false);
    chain.setBypassed<ChainPositions::Peak>(false);
    chain.setBypassed<ChainPositions::HighCut>(false);
    chain.setBypassed<ChainPositions::LowCutQ>(false);
    chain.setBypassed<ChainPositions::HighCutQ>(false);

    updateCutFilter(chain.get<ChainPositions::LowCut>(),
                    makeLowCutFilter(settings, sampleRate),
                    settings.lowCutSlope);
    updateCoefficients(chain.get<ChainPositions::Peak>().coefficients,
                       makePeakFilter(settings, sampleRate));
    updateCutFilter(chain.get<ChainPositions::HighCut>(),
                    makeHighCutFilter(settings, sampleRate),
                    settings.highCutSlope);
    updateCoefficients(chain.get<ChainPositions::LowCutQ>().coefficients,
                       makeLowcutQFilter(settings, sampleRate));
    updateCoefficients(chain.get<ChainPositions::HighCutQ>().coefficients,
                       makeHighcutQFilter(settings, sampleRate));

    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32>(preparedBlockSize),
                                        1 };
    chain.prepare(spec);
    chain.reset();
}

void processCanonicalStereo(MonoChain& left,
                            MonoChain& right,
                            juce::AudioBuffer<float>& buffer)
{
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    left.process(juce::dsp::ProcessContextReplacing<float>(leftBlock));
    if (buffer.getNumChannels() > 1)
    {
        auto rightBlock = block.getSingleChannelBlock(1);
        right.process(juce::dsp::ProcessContextReplacing<float>(rightBlock));
    }
}

float boundaryStep(const juce::AudioBuffer<float>& before,
                   const juce::AudioBuffer<float>& after)
{
    REQUIRE(before.getNumChannels() > 0);
    REQUIRE(after.getNumChannels() > 0);
    REQUIRE(before.getNumSamples() > 0);
    REQUIRE(after.getNumSamples() > 0);
    return std::abs(after.getSample(0, 0)
                    - before.getSample(0, before.getNumSamples() - 1));
}

float audibleBoundaryStep(const juce::AudioBuffer<float>& before,
                          const juce::AudioBuffer<float>& after,
                          int latencySamples)
{
    REQUIRE(latencySamples >= 0);
    REQUIRE(latencySamples < after.getNumSamples());
    if (latencySamples == 0)
        return boundaryStep(before, after);
    return std::abs(after.getSample(0, latencySamples)
                    - after.getSample(0, latencySamples - 1));
}

float maximumAdjacentStep(const juce::AudioBuffer<float>& before,
                          const juce::AudioBuffer<float>& during)
{
    REQUIRE(before.getNumChannels() == during.getNumChannels());
    REQUIRE(before.getNumSamples() > 0);
    REQUIRE(during.getNumSamples() > 0);

    float maximumStep = 0.0f;
    for (int channel = 0; channel < during.getNumChannels(); ++channel)
    {
        float previous = before.getSample(channel,
                                          before.getNumSamples() - 1);
        for (int sample = 0; sample < during.getNumSamples(); ++sample)
        {
            const float current = during.getSample(channel, sample);
            REQUIRE(std::isfinite(current));
            maximumStep = std::max(maximumStep,
                                   std::abs(current - previous));
            previous = current;
        }
    }

    return maximumStep;
}

float minimumDistanceFromEitherEndpoint(
    const juce::AudioBuffer<float>& subject,
    const juce::AudioBuffer<float>& wet,
    const juce::AudioBuffer<float>& dry,
    int firstSample,
    int numSamples)
{
    REQUIRE(subject.getNumChannels() == wet.getNumChannels());
    REQUIRE(subject.getNumChannels() == dry.getNumChannels());
    REQUIRE(subject.getNumSamples() == wet.getNumSamples());
    REQUIRE(subject.getNumSamples() == dry.getNumSamples());
    REQUIRE(firstSample >= 0);
    REQUIRE(numSamples > 0);
    REQUIRE(firstSample + numSamples <= subject.getNumSamples());

    float minimumDistance = std::numeric_limits<float>::max();
    for (int channel = 0; channel < subject.getNumChannels(); ++channel)
    {
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
        {
            const float value = subject.getSample(channel, sample);
            const float fromWet = std::abs(value - wet.getSample(channel, sample));
            const float fromDry = std::abs(value - dry.getSample(channel, sample));
            minimumDistance = std::min(minimumDistance,
                                       std::min(fromWet, fromDry));
        }
    }

    return minimumDistance;
}

struct StageBypassMetrics
{
    float wetDrySeparation = 0.0f;
    float fadeOutFirstSampleFromWet = 0.0f;
    float fadeOutFirstSampleFromDry = 0.0f;
    float fadeOutMaximumFromWet = 0.0f;
    float fadeOutMaximumFromDry = 0.0f;
    float fadeOutBoundaryStep = 0.0f;
    float fadeOutMaximumAdjacentStep = 0.0f;
    float fadeOutInteriorEndpointDistance = 0.0f;
    float settledDryError = 0.0f;
    float fadeInFirstSampleFromDry = 0.0f;
    float fadeInFirstSampleFromWet = 0.0f;
    float fadeInBoundaryStep = 0.0f;
    float fadeInMaximumAdjacentStep = 0.0f;
    float fadeInInteriorEndpointDistance = 0.0f;
    float finalWetStateError = 0.0f;
};

StageBypassMetrics runStageBypassProbe(FilterStage stage)
{
    FireAudioProcessor subject;
    FireAudioProcessor alwaysWet;
    FireAudioProcessor alwaysDry;
    configureStage(subject, stage, false);
    configureStage(alwaysWet, stage, false);
    configureStage(alwaysDry, stage, true);

    int streamPosition = 0;
    const auto processAll = [&] (int numSamples)
    {
        const auto input = makeProbeInput(stage, streamPosition, numSamples);
        streamPosition += numSamples;
        return std::array<juce::AudioBuffer<float>, 3> {
            processCopy(subject, input),
            processCopy(alwaysWet, input),
            processCopy(alwaysDry, input)
        };
    };

    const auto warmup = processAll(warmupSamples);
    REQUIRE(maximumAbsoluteDifference(warmup[0], warmup[1]) < 1.0e-6f);

    StageBypassMetrics metrics;
    setPlainParameter(subject, bypassParameterFor(stage), 1.0f);
    const auto bypassFirstBlock = processAll(preparedBlockSize);
    const int audibleTransitionSample = subject.getLatencySamples();
    REQUIRE(audibleTransitionSample >= 0);
    REQUIRE(audibleTransitionSample < preparedBlockSize);
    metrics.wetDrySeparation = maximumAbsoluteDifference(bypassFirstBlock[1],
                                                          bypassFirstBlock[2]);
    metrics.fadeOutFirstSampleFromWet = std::abs(
        bypassFirstBlock[0].getSample(0, audibleTransitionSample)
        - bypassFirstBlock[1].getSample(0, audibleTransitionSample));
    metrics.fadeOutFirstSampleFromDry = std::abs(
        bypassFirstBlock[0].getSample(0, audibleTransitionSample)
        - bypassFirstBlock[2].getSample(0, audibleTransitionSample));
    metrics.fadeOutMaximumFromWet = maximumAbsoluteDifference(
        bypassFirstBlock[0], bypassFirstBlock[1]);
    metrics.fadeOutMaximumFromDry = maximumAbsoluteDifference(
        bypassFirstBlock[0], bypassFirstBlock[2]);
    metrics.fadeOutBoundaryStep = audibleBoundaryStep(warmup[0],
                                                       bypassFirstBlock[0],
                                                       audibleTransitionSample);

    // The first block is 257 samples, shorter than the intended 10 ms stage
    // transition. Sample 64 sits well inside the crossfade and must not equal
    // either independently-running endpoint.
    metrics.fadeOutInteriorEndpointDistance = minimumDistanceFromEitherEndpoint(
        bypassFirstBlock[0],
        bypassFirstBlock[1],
        bypassFirstBlock[2],
        64 + audibleTransitionSample,
        1);
    metrics.fadeOutMaximumAdjacentStep = maximumAdjacentStep(warmup[0],
                                                              bypassFirstBlock[0]);

    const auto fadeOutRemainder = processAll(stageTransitionSamples
                                              - preparedBlockSize);
    metrics.fadeOutMaximumAdjacentStep = std::max(
        metrics.fadeOutMaximumAdjacentStep,
        maximumAdjacentStep(bypassFirstBlock[0], fadeOutRemainder[0]));
    processAll(subject.getLatencySamples());
    const auto drySettled = processAll(193);
    metrics.settledDryError = maximumAbsoluteDifference(drySettled[0],
                                                         drySettled[2]);

    // The longer observation drain must not move the re-enable event to
    // a different input phase; preserve the original bypass interval.
    processAll(bypassIntervalSamples - 193 - fire::tests::legacyInsertOutputReserve());
    const auto beforeEnable = processAll(31);
    setPlainParameter(subject, bypassParameterFor(stage), 0.0f);
    const auto enableFirstBlock = processAll(preparedBlockSize);
    metrics.fadeInFirstSampleFromDry = std::abs(
        enableFirstBlock[0].getSample(0, audibleTransitionSample)
        - enableFirstBlock[2].getSample(0, audibleTransitionSample));
    metrics.fadeInFirstSampleFromWet = std::abs(
        enableFirstBlock[0].getSample(0, audibleTransitionSample)
        - enableFirstBlock[1].getSample(0, audibleTransitionSample));
    metrics.fadeInBoundaryStep = audibleBoundaryStep(beforeEnable[0],
                                                      enableFirstBlock[0],
                                                      audibleTransitionSample);

    metrics.fadeInInteriorEndpointDistance = minimumDistanceFromEitherEndpoint(
        enableFirstBlock[0],
        enableFirstBlock[1],
        enableFirstBlock[2],
        64 + audibleTransitionSample,
        1);
    metrics.fadeInMaximumAdjacentStep = maximumAdjacentStep(beforeEnable[0],
                                                             enableFirstBlock[0]);

    const auto fadeInRemainder = processAll(stageTransitionSamples
                                             - preparedBlockSize);
    metrics.fadeInMaximumAdjacentStep = std::max(
        metrics.fadeInMaximumAdjacentStep,
        maximumAdjacentStep(enableFirstBlock[0], fadeInRemainder[0]));

    // Consume the remaining transition plus ample recursive settling. The
    // Low/High logical stages comprise five serial processors whose private
    // wet histories differ from the always-wet reference while preceding
    // stages crossfade; after all five are fully wet their IIR states converge.
    processAll(4096);
    const auto afterEnable = processAll(1024);
    metrics.finalWetStateError = maximumAbsoluteDifference(afterEnable[0],
                                                            afterEnable[1]);
    return metrics;
}

const char* stageName(FilterStage stage)
{
    switch (stage)
    {
        case FilterStage::lowCut: return "LowCut";
        case FilterStage::peak: return "Peak";
        case FilterStage::highCut: return "HighCut";
    }

    return "Unknown";
}
} // namespace

TEST_CASE("Individual global-filter stage bypasses transition continuously and stay warm",
          "[processor][filter][stage-bypass][smoothing]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const auto stage : { FilterStage::lowCut,
                              FilterStage::peak,
                              FilterStage::highCut })
    {
        DYNAMIC_SECTION(stageName(stage))
        {
            const auto metrics = runStageBypassProbe(stage);
            INFO("wet/dry separation = " << metrics.wetDrySeparation);
            INFO("fade-out first sample from wet = "
                 << metrics.fadeOutFirstSampleFromWet);
            INFO("fade-out first sample from dry = "
                 << metrics.fadeOutFirstSampleFromDry);
            INFO("fade-out maximum from wet = "
                 << metrics.fadeOutMaximumFromWet);
            INFO("fade-out maximum from dry = "
                 << metrics.fadeOutMaximumFromDry);
            INFO("fade-out boundary step = " << metrics.fadeOutBoundaryStep);
            INFO("fade-out maximum adjacent step = "
                 << metrics.fadeOutMaximumAdjacentStep);
            INFO("fade-out interior endpoint distance = "
                 << metrics.fadeOutInteriorEndpointDistance);
            INFO("settled dry error = " << metrics.settledDryError);
            INFO("fade-in first sample from dry = "
                 << metrics.fadeInFirstSampleFromDry);
            INFO("fade-in first sample from wet = "
                 << metrics.fadeInFirstSampleFromWet);
            INFO("fade-in boundary step = " << metrics.fadeInBoundaryStep);
            INFO("fade-in maximum adjacent step = "
                 << metrics.fadeInMaximumAdjacentStep);
            INFO("fade-in interior endpoint distance = "
                 << metrics.fadeInInteriorEndpointDistance);
            INFO("post-enable wet-state error = "
                 << metrics.finalWetStateError);

            REQUIRE(metrics.wetDrySeparation > 0.15f);

            // The first sample after either switch must begin at the previous
            // audible recipe, not hard-select the new path at the callback
            // boundary. This permits any smooth transition shape/duration.
            REQUIRE(metrics.fadeOutFirstSampleFromWet
                    < metrics.fadeOutFirstSampleFromDry * 0.1f);
            REQUIRE(metrics.fadeInFirstSampleFromDry
                    < metrics.fadeInFirstSampleFromWet * 0.1f);

            // At least one sample well inside each 10 ms transition must be a
            // true blend, not a delayed hard switch between endpoints. Once
            // the window completes, the result must be exactly bypassed.
            REQUIRE(metrics.fadeOutInteriorEndpointDistance > 1.0e-5f);
            REQUIRE(metrics.fadeInInteriorEndpointDistance > 1.0e-5f);
            REQUIRE(metrics.settledDryError < 1.0e-6f);

            // Five physical sections participate in a Low/High stage (four
            // Butterworth biquads plus the Q/gain section), while Peak has one.
            // A transition must not add a discontinuity larger than the probe's
            // natural adjacent-sample movement plus a modest headroom margin.
            REQUIRE(metrics.fadeOutMaximumAdjacentStep < 1.75f);
            REQUIRE(metrics.fadeInMaximumAdjacentStep < 1.75f);

            // A bypassed recursive stage must continue consuming audio. Exact
            // agreement after its re-enable transition proves no frozen state
            // or stale tail was resumed.
            REQUIRE(metrics.finalWetStateError < 1.0e-5f);
        }
    }
}

TEST_CASE("All enabled global-filter stages retain the canonical chain response",
          "[processor][filter][stage-bypass][compatibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    FireAudioProcessor subject;
    configureStage(subject, FilterStage::peak, false, false);
    setPlainParameter(subject, LOWCUT_BYPASSED_ID, 0.0f);
    setPlainParameter(subject, PEAK_BYPASSED_ID, 0.0f);
    setPlainParameter(subject, HIGHCUT_BYPASSED_ID, 0.0f);
    setPlainParameter(subject, LOWCUT_FREQ_ID, 1900.0f);
    setPlainParameter(subject, LOWCUT_GAIN_ID, 18.0f);
    setPlainParameter(subject, LOWCUT_Q_ID, 5.0f);
    setPlainParameter(subject, LOWCUT_SLOPE_ID, 3.0f);
    setPlainParameter(subject, HIGHCUT_FREQ_ID, 5900.0f);
    setPlainParameter(subject, HIGHCUT_GAIN_ID, -12.0f);
    setPlainParameter(subject, HIGHCUT_Q_ID, 4.4f);
    setPlainParameter(subject, HIGHCUT_SLOPE_ID, 2.0f);
    subject.prepareToPlay(sampleRate, preparedBlockSize);

    // This reference is deliberately independent of FireAudioProcessor's
    // manual stage dispatcher. FiltersUtil builds the original canonical
    // MonoChain and ProcessorChain itself enforces the historical order:
    // LowCut -> Peak -> HighCut -> LowCutQ -> HighCutQ.
    ChainSettings settings;
    settings.lowCutFreq = 1900.0f;
    settings.lowCutGainInDecibels = 18.0f;
    settings.lowCutQuality = 5.0f;
    settings.lowCutSlope = Slope_48;
    settings.peakFreq = 1837.0f;
    settings.peakGainInDecibels = 24.0f;
    settings.peakQuality = 5.0f;
    settings.highCutFreq = 5900.0f;
    settings.highCutGainInDecibels = -12.0f;
    settings.highCutQuality = 4.4f;
    settings.highCutSlope = Slope_36;
    settings.lowCutBypassed = false;
    settings.peakBypassed = false;
    settings.highCutBypassed = false;

    MonoChain canonicalLeft;
    MonoChain canonicalRight;
    prepareCanonicalChain(canonicalLeft, settings);
    prepareCanonicalChain(canonicalRight, settings);

    juce::dsp::DelayLine<
        float,
        juce::dsp::DelayLineInterpolationTypes::None> canonicalLatency(juce::jmax(64, subject.getLatencySamples() + 2));
    canonicalLatency.prepare({ sampleRate,
                               static_cast<juce::uint32>(preparedBlockSize),
                               2 });
    canonicalLatency.setDelay(
        static_cast<float>(subject.getLatencySamples()));
    canonicalLatency.reset();

    int streamPosition = 0;
    for (int block = 0; block < 48; ++block)
    {
        const auto input = makeProbeInput(FilterStage::peak,
                                          streamPosition,
                                          preparedBlockSize);
        streamPosition += preparedBlockSize;
        const auto subjectOutput = processCopy(subject, input);
        auto canonicalOutput = input;
        processCanonicalStereo(canonicalLeft, canonicalRight, canonicalOutput);
        auto canonicalBlock = juce::dsp::AudioBlock<float>(canonicalOutput);
        canonicalLatency.process(
            juce::dsp::ProcessContextReplacing<float>(canonicalBlock));
        REQUIRE(maximumAbsoluteDifference(subjectOutput, canonicalOutput)
                < 2.0e-5f);
    }
}
