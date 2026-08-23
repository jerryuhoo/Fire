/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include "PluginProcessor.h"
#include "DSP/DistortionLogic.h"
#include "PluginEditor.h"
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace
{
bool parseStrictFiniteDouble(const juce::String& textToParse, double& result) noexcept
{
    const auto trimmed = textToParse.trim();
    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(utf8, &end);
    if (end == utf8 || end == nullptr || *end != '\0'
        || errno == ERANGE || ! std::isfinite(parsed))
        return false;

    result = parsed;
    return true;
}

bool arePresetFloatsEquivalent(float lhs, float rhs) noexcept
{
    constexpr float tolerance = 1.0e-6f;
    return std::isfinite(lhs) && std::isfinite(rhs)
           && std::abs(lhs - rhs) <= tolerance;
}

bool areLfoShapesEquivalent(const LfoData& current, const LfoData& expected) noexcept
{
    if (current.points.size() != expected.points.size()
        || current.curvatures.size() != expected.curvatures.size()
        || ! arePresetFloatsEquivalent(current.smoothness, expected.smoothness))
        return false;

    for (size_t i = 0; i < current.points.size(); ++i)
        if (! arePresetFloatsEquivalent(current.points[i].x, expected.points[i].x)
            || ! arePresetFloatsEquivalent(current.points[i].y, expected.points[i].y))
            return false;

    for (size_t i = 0; i < current.curvatures.size(); ++i)
        if (! arePresetFloatsEquivalent(current.curvatures[i], expected.curvatures[i]))
            return false;

    return true;
}

bool areModulationRoutingsEquivalent(const ModulationRouting& current,
                                     const ModulationRouting& expected) noexcept
{
    return current.sourceLfoIndex == expected.sourceLfoIndex
           && current.targetParameterID == expected.targetParameterID
           && arePresetFloatsEquivalent(current.depth, expected.depth)
           && current.isBipolar == expected.isBipolar
           && current.isBypassed == expected.isBypassed;
}

struct FilterFrequencyRange
{
    float minimum;
    float maximum;
};

FilterFrequencyRange getSafeFilterFrequencyRange(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    const float nyquist = static_cast<float>(safeSampleRate * 0.5);
    const float maximum = std::nextafter(nyquist, 0.0f);
    return { juce::jmin(20.0f, maximum), maximum };
}

Slope getSlopeParameterValue(const std::atomic<float>* parameter) noexcept
{
    const int value = parameter != nullptr ? juce::roundToInt(parameter->load(std::memory_order_relaxed)) : 0;
    return static_cast<Slope>(juce::jlimit(static_cast<int>(Slope_12),
                                           static_cast<int>(Slope_48),
                                           value));
}

// A few hosts occasionally exceed the block-size hint passed to prepareToPlay.
// Reserving a practical safety capacity keeps JUCE mixers/oversamplers inside
// their prepared bounds without allocating on the audio thread.
constexpr int minimumProcessingBlockCapacity = 8192;
constexpr double safePeakHoldSeconds = 0.05;
constexpr double safePeakReleaseSeconds = 0.05;

template <typename FloatType>
bool sameCachedValue(FloatType lhs, FloatType rhs) noexcept
{
    const auto scale = juce::jmax(static_cast<FloatType>(1),
                                  juce::jmax(std::abs(lhs), std::abs(rhs)));
    return std::abs(lhs - rhs) <= std::numeric_limits<FloatType>::epsilon() * scale;
}

bool sameLowCutSettings(const ChainSettings& lhs, const ChainSettings& rhs) noexcept
{
    return sameCachedValue(lhs.lowCutFreq, rhs.lowCutFreq)
        && sameCachedValue(lhs.lowCutGainInDecibels, rhs.lowCutGainInDecibels)
        && sameCachedValue(lhs.lowCutQuality, rhs.lowCutQuality)
        && lhs.lowCutSlope == rhs.lowCutSlope
        && lhs.lowCutBypassed == rhs.lowCutBypassed;
}

bool samePeakSettings(const ChainSettings& lhs, const ChainSettings& rhs) noexcept
{
    return sameCachedValue(lhs.peakFreq, rhs.peakFreq)
        && sameCachedValue(lhs.peakGainInDecibels, rhs.peakGainInDecibels)
        && sameCachedValue(lhs.peakQuality, rhs.peakQuality)
        && lhs.peakBypassed == rhs.peakBypassed;
}

bool sameHighCutSettings(const ChainSettings& lhs, const ChainSettings& rhs) noexcept
{
    return sameCachedValue(lhs.highCutFreq, rhs.highCutFreq)
        && sameCachedValue(lhs.highCutGainInDecibels, rhs.highCutGainInDecibels)
        && sameCachedValue(lhs.highCutQuality, rhs.highCutQuality)
        && lhs.highCutSlope == rhs.highCutSlope
        && lhs.highCutBypassed == rhs.highCutBypassed;
}

struct BandParameterAddress
{
    const ModulatableParameterInfo* parameter = nullptr;
    int bandIndex = -1;
};

BandParameterAddress findBandParameterAddress(const juce::String& parameterID)
{
    for (const auto& parameter : ParameterIDAndName::getBandParameterInfo())
    {
        for (int bandIndex = 0; bandIndex < 4; ++bandIndex)
        {
            if (parameterID == ParameterIDAndName::getIDString(parameter.idBase, bandIndex))
                return { &parameter, bandIndex };
        }
    }

    return {};
}

void resetModulationRouting(ModulationRouting& routing)
{
    routing = {};
}

using BiquadCoefficients = std::array<float, 6>;

void assignCutStage(CutFilter& chain,
                    int stage,
                    const BiquadCoefficients& coefficients) noexcept
{
    switch (stage)
    {
        case 0: *chain.get<0>().coefficients = coefficients; break;
        case 1: *chain.get<1>().coefficients = coefficients; break;
        case 2: *chain.get<2>().coefficients = coefficients; break;
        case 3: *chain.get<3>().coefficients = coefficients; break;
        default: jassertfalse; break;
    }
}

void setCutStageBypassed(CutFilter& chain, int stageCount) noexcept
{
    chain.setBypassed<0>(stageCount < 1);
    chain.setBypassed<1>(stageCount < 2);
    chain.setBypassed<2>(stageCount < 3);
    chain.setBypassed<3>(stageCount < 4);
}

void updateButterworthCutFilterPair(CutFilter& leftChain,
                                    CutFilter& rightChain,
                                    float frequency,
                                    double sampleRate,
                                    Slope slope,
                                    bool highPass) noexcept
{
    const int stageCount = juce::jlimit(1, 4, static_cast<int>(slope) + 1);
    const int order = stageCount * 2;
    constexpr BiquadCoefficients identityBiquad { 1.0f, 0.0f, 0.0f,
                                                   1.0f, 0.0f, 0.0f };

    for (int stage = 0; stage < 4; ++stage)
    {
        if (stage >= stageCount)
        {
            // Keep even bypassed stages second-order. Otherwise a later slope
            // increase changes JUCE IIR::Filter's order from one to two and
            // makes its internal state allocate/reset on the audio thread.
            assignCutStage(leftChain, stage, identityBiquad);
            assignCutStage(rightChain, stage, identityBiquad);
            continue;
        }

        // This is the same Q sequence used by JUCE's high-order Butterworth
        // designer, but it writes into the filters' existing coefficient
        // storage instead of allocating ReferenceCountedObjects per block.
        const auto angle = (2.0 * static_cast<double>(stage) + 1.0)
                         * juce::MathConstants<double>::pi
                         / (2.0 * static_cast<double>(order));
        const float q = static_cast<float>(1.0 / (2.0 * std::cos(angle)));
        const auto coefficients = highPass
                                      ? juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate,
                                                                                                frequency,
                                                                                                q)
                                      : juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sampleRate,
                                                                                               frequency,
                                                                                               q);
        assignCutStage(leftChain, stage, coefficients);
        assignCutStage(rightChain, stage, coefficients);
    }

    setCutStageBypassed(leftChain, stageCount);
    setCutStageBypassed(rightChain, stageCount);
}

float processCutFilterSample(CutFilter& chain, float sample) noexcept
{
    const auto processStage = [] (Filter& filter, bool bypassed, float input)
    {
        const auto filtered = filter.processSample(input);
        return bypassed ? input : filtered;
    };
    sample = processStage(chain.get<0>(), chain.isBypassed<0>(), sample);
    sample = processStage(chain.get<1>(), chain.isBypassed<1>(), sample);
    sample = processStage(chain.get<2>(), chain.isBypassed<2>(), sample);
    sample = processStage(chain.get<3>(), chain.isBypassed<3>(), sample);
    return sample;
}

void snapCutFilterToZero(CutFilter& chain) noexcept
{
    chain.get<0>().snapToZero();
    chain.get<1>().snapToZero();
    chain.get<2>().snapToZero();
    chain.get<3>().snapToZero();
}

template <typename StageProcessor>
void processGlobalFilterStage(StageProcessor& leftProcessor,
                              StageProcessor& rightProcessor,
                              juce::dsp::AudioBlock<float>& fullBlock,
                              juce::AudioBuffer<float>& scratchBuffer,
                              juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& wetMix,
                              int startSample,
                              int numSamples) noexcept
{
    if (numSamples <= 0 || fullBlock.getNumChannels() == 0)
        return;

    // The fully-enabled steady state retains the original in-place processing
    // path and its exact processor ordering. During a transition or while a
    // stage is bypassed, keep a private wet shadow running and blend (or
    // discard) it without disturbing the audible dry signal.
    if (! wetMix.isSmoothing() && wetMix.getCurrentValue() == 1.0f)
    {
        auto stageBlock = fullBlock.getSubBlock(static_cast<size_t>(startSample),
                                                static_cast<size_t>(numSamples));
        auto leftBlock = stageBlock.getSingleChannelBlock(0);
        leftProcessor.process(juce::dsp::ProcessContextReplacing<float>(leftBlock));

        if (stageBlock.getNumChannels() > 1)
        {
            auto rightBlock = stageBlock.getSingleChannelBlock(1);
            rightProcessor.process(juce::dsp::ProcessContextReplacing<float>(rightBlock));
        }
        return;
    }

    const int scratchCapacity = scratchBuffer.getNumSamples();
    jassert(scratchCapacity > 0);
    if (scratchCapacity <= 0)
        return;

    int processed = 0;
    while (processed < numSamples)
    {
        const int chunkSamples = juce::jmin(scratchCapacity, numSamples - processed);
        auto stageBlock = fullBlock.getSubBlock(static_cast<size_t>(startSample + processed),
                                                static_cast<size_t>(chunkSamples));
        const int processedChannels = stageBlock.getNumChannels() > 1 ? 2 : 1;
        auto scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer)
                                .getSubsetChannelBlock(0, static_cast<size_t>(processedChannels))
                                .getSubBlock(0, static_cast<size_t>(chunkSamples));
        scratchBlock.copyFrom(stageBlock.getSubsetChannelBlock(
            0, static_cast<size_t>(processedChannels)));

        auto scratchLeft = scratchBlock.getSingleChannelBlock(0);
        leftProcessor.process(juce::dsp::ProcessContextReplacing<float>(scratchLeft));

        if (processedChannels > 1)
        {
            auto scratchRight = scratchBlock.getSingleChannelBlock(1);
            rightProcessor.process(juce::dsp::ProcessContextReplacing<float>(scratchRight));
        }

        if (wetMix.isSmoothing() || wetMix.getCurrentValue() != 0.0f)
        {
            for (int sample = 0; sample < chunkSamples; ++sample)
            {
                const auto mix = wetMix.getNextValue();
                for (int channel = 0; channel < processedChannels; ++channel)
                {
                    const auto dry = stageBlock.getSample(channel, sample);
                    const auto wet = scratchBlock.getSample(channel, sample);
                    const auto output = mix <= 0.0f ? dry
                                      : mix >= 1.0f ? wet
                                                    : dry + mix * (wet - dry);
                    stageBlock.setSample(channel, sample, output);
                }
            }
        }

        processed += chunkSamples;
    }
}
} // namespace

void OutputGainTransitionState::prepare(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    routeTransitionMix.reset(safeSampleRate, 0.01);
    legacyGainTracker.reset(safeSampleRate, 0.05);
    reset();
}

void OutputGainTransitionState::reset() noexcept
{
    routeTransitionMix.setCurrentAndTargetValue(1.0f);
    legacyGainTracker.setCurrentAndTargetValue(0.0f);
    lastRecipe = {};
    anchorLinearGain = 1.0f;
    lastAppliedLinearGain = 1.0f;
    initialised = false;
}

void CompressorRecipeTransitionState::prepare(double sampleRate,
                                               float initialValue) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    routeTransitionMix.reset(safeSampleRate, 0.01);
    reset(initialValue);
}

void CompressorRecipeTransitionState::reset(float initialValue) noexcept
{
    const float safeInitialValue = std::isfinite(initialValue)
                                       ? initialValue
                                       : 0.0f;
    routeTransitionMix.setCurrentAndTargetValue(1.0f);
    lastRecipe = {};
    anchorValue = safeInitialValue;
    lastAppliedValue = safeInitialValue;
    initialised = false;
}

static CompressorRecipeTransitionState::RecipeSignature
makeCompressorRecipe(const ModulatedValueProvider& provider,
                     int sourceIndex) noexcept
{
    CompressorRecipeTransitionState::RecipeSignature recipe;
    recipe.routed = provider.lfoSignal != nullptr;
    recipe.sourceIndex = recipe.routed ? sourceIndex : -1;
    recipe.modulationDepth = recipe.routed
                                 && std::isfinite(provider.modulationDepth)
                                 ? juce::jlimit(-1.0f,
                                                1.0f,
                                                provider.modulationDepth)
                                 : 0.0f;
    recipe.isBipolar = recipe.routed ? provider.isBipolar : true;
    return recipe;
}

static bool sameCompressorRecipe(
    const CompressorRecipeTransitionState::RecipeSignature& lhs,
    const CompressorRecipeTransitionState::RecipeSignature& rhs) noexcept
{
    if (lhs.routed != rhs.routed)
        return false;
    if (! lhs.routed)
        return true;

    return lhs.sourceIndex == rhs.sourceIndex
           && juce::exactlyEqual(lhs.modulationDepth, rhs.modulationDepth)
           && lhs.isBipolar == rhs.isBipolar;
}

static void serviceCompressorRecipeTransition(
    CompressorRecipeTransitionState& transition,
    const CompressorRecipeTransitionState::RecipeSignature& recipe,
    float currentTarget) noexcept
{
    if (! transition.initialised)
    {
        transition.initialised = true;
        transition.lastRecipe = recipe;
        transition.routeTransitionMix.setCurrentAndTargetValue(1.0f);
        transition.anchorValue = currentTarget;
        transition.lastAppliedValue = currentTarget;
        return;
    }

    if (! sameCompressorRecipe(recipe, transition.lastRecipe))
    {
        // Anchor to the value that the detector actually consumed on the
        // preceding sample.  Rapid edits therefore retarget continuously and
        // never rewrite a partly audible trajectory.
        transition.anchorValue = transition.lastAppliedValue;
        transition.routeTransitionMix.setCurrentAndTargetValue(0.0f);
        transition.routeTransitionMix.setTargetValue(1.0f);
        transition.lastRecipe = recipe;
    }
}

static float applyCompressorRecipeTransition(
    const CompressorRecipeTransitionState& transition,
    float targetValue,
    bool useLogarithmicDomain) noexcept
{
    const float mix = transition.routeTransitionMix.getCurrentValue();
    if (mix <= 0.0f)
        return transition.anchorValue;
    if (mix >= 1.0f)
        return targetValue;

    if (! useLogarithmicDomain)
        return transition.anchorValue
               + mix * (targetValue - transition.anchorValue);

    // Attack and release control detector poles.  Interpolating milliseconds
    // geometrically avoids recreating a large coefficient step at the short
    // end of an otherwise linear time ramp.
    constexpr float minimumTimeMs = 0.01f;
    const float safeAnchor = std::isfinite(transition.anchorValue)
                                 ? juce::jmax(minimumTimeMs,
                                              transition.anchorValue)
                                 : minimumTimeMs;
    const float safeTarget = std::isfinite(targetValue)
                                 ? juce::jmax(minimumTimeMs, targetValue)
                                 : minimumTimeMs;
    return std::exp(std::log(safeAnchor)
                    + mix * (std::log(safeTarget) - std::log(safeAnchor)));
}

static OutputGainTransitionState::RecipeSignature makeOutputGainRecipe(
    const ModulatedValueProvider& provider,
    int sourceIndex) noexcept
{
    OutputGainTransitionState::RecipeSignature recipe;
    recipe.routed = provider.lfoSignal != nullptr;
    recipe.sourceIndex = recipe.routed ? sourceIndex : -1;
    recipe.baseValue = std::isfinite(provider.baseValue)
                           ? provider.baseValue
                           : 0.0f;
    recipe.modulationDepth = std::isfinite(provider.modulationDepth)
                                 ? juce::jlimit(-1.0f,
                                                1.0f,
                                                provider.modulationDepth)
                                 : 0.0f;
    recipe.isBipolar = provider.isBipolar;
    return recipe;
}

static bool sameOutputGainRecipe(
    const OutputGainTransitionState::RecipeSignature& lhs,
    const OutputGainTransitionState::RecipeSignature& rhs) noexcept
{
    if (lhs.routed != rhs.routed)
        return false;
    if (! lhs.routed)
        return true;

    return lhs.sourceIndex == rhs.sourceIndex
           && juce::exactlyEqual(lhs.baseValue, rhs.baseValue)
           && juce::exactlyEqual(lhs.modulationDepth, rhs.modulationDepth)
           && lhs.isBipolar == rhs.isBipolar;
}

static void synchroniseLegacyGain(juce::dsp::Gain<float>& gain,
                                  OutputGainTransitionState& transition,
                                  float linearGain) noexcept
{
    transition.lastAppliedLinearGain = linearGain;
    transition.legacyGainTracker.setCurrentAndTargetValue(linearGain);
    gain.setRampDurationSeconds(0.0);
    gain.setGainLinear(linearGain);
    gain.setRampDurationSeconds(0.05);
}

static void applyGain(juce::AudioBuffer<float>& buffer,
                      const ModulatedValueProvider& gainProvider,
                      juce::dsp::Gain<float>& gain,
                      OutputGainTransitionState& transition,
                      int sourceIndex)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    const auto recipe = makeOutputGainRecipe(gainProvider, sourceIndex);
    if (gainProvider.lfoSignal == nullptr)
    {
        if (! transition.initialised)
        {
            // Gain exposes its target rather than its current smoothed value.
            // Immediately after prepare/reset those are equal; from there this
            // tracker advances in exact lockstep with Gain's 50 ms ramp.
            transition.legacyGainTracker.setCurrentAndTargetValue(
                gain.getGainLinear());
            transition.initialised = true;
        }

        transition.lastRecipe = recipe;
        transition.routeTransitionMix.setCurrentAndTargetValue(1.0f);
        const float safeBaseDb = std::isfinite(gainProvider.baseValue)
                                     ? gainProvider.baseValue
                                     : 0.0f;
        gain.setGainDecibels(safeBaseDb);
        const float targetLinearGain = juce::Decibels::decibelsToGain(
            safeBaseDb);
        transition.legacyGainTracker.setTargetValue(targetLinearGain);

        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto context = juce::dsp::ProcessContextReplacing<float>(block);
        gain.process(context);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            transition.lastAppliedLinearGain =
                transition.legacyGainTracker.getNextValue();
        return;
    }

    if (! transition.initialised)
    {
        transition.initialised = true;
        transition.lastRecipe = recipe;
        transition.routeTransitionMix.setCurrentAndTargetValue(1.0f);
    }
    else if (! sameOutputGainRecipe(recipe, transition.lastRecipe))
    {
        // Keep the target LFO fully sample-accurate. Only the discrete recipe
        // boundary is bridged from the gain that was actually audible.
        transition.anchorLinearGain = transition.lastAppliedLinearGain;
        transition.routeTransitionMix.setCurrentAndTargetValue(0.0f);
        transition.routeTransitionMix.setTargetValue(1.0f);
        transition.lastRecipe = recipe;
    }

    if (! transition.routeTransitionMix.isSmoothing())
    {
        // Preserve the historical steady routed path exactly: no smoother is
        // placed in front of the LFO, and each channel follows the same direct
        // provider evaluation it used before recipe bridging was added.
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                // The provider does all the complex calculation for us!
                const float gainDb = gainProvider.get(sample);
                channelData[sample] *= juce::Decibels::decibelsToGain(gainDb);
            }
        }

        const float lastGainDb = gainProvider.get(buffer.getNumSamples() - 1);
        const float lastLinearGain = juce::Decibels::decibelsToGain(lastGainDb);
        synchroniseLegacyGain(gain,
                              transition,
                              lastLinearGain);
        return;
    }

    auto* const* channelData = buffer.getArrayOfWritePointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float targetLinearGain = juce::Decibels::decibelsToGain(
            gainProvider.get(sample));
        const float mix = transition.routeTransitionMix.getCurrentValue();
        const float effectiveLinearGain = mix <= 0.0f
                                              ? transition.anchorLinearGain
                                          : mix >= 1.0f
                                              ? targetLinearGain
                                              : transition.anchorLinearGain
                                                    + mix
                                                          * (targetLinearGain
                                                             - transition.anchorLinearGain);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            channelData[channel][sample] *= effectiveLinearGain;

        transition.lastAppliedLinearGain = effectiveLinearGain;
        transition.routeTransitionMix.getNextValue();
    }

    synchroniseLegacyGain(
        gain,
        transition,
        transition.lastAppliedLinearGain);
}

template <typename ValueType>
static bool pushToFifo(juce::AbstractFifo& fifo,
                       std::vector<ValueType>& storage,
                       const ValueType& value)
{
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    fifo.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0)
        storage[static_cast<size_t>(start1)] = value;
    else if (size2 > 0)
        storage[static_cast<size_t>(start2)] = value;
    else
        return false;

    fifo.finishedWrite(1);
    return true;
}

//==============================================================================
// BandProcessor Implementation
//==============================================================================

// This is where we tell JUCE what to do when prepareToPlay is called for a single band.
void BandProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    // Prepare all the DSP modules with the sample rate and block size.
    compressor.prepare(spec);
    widthProcessor.prepare(spec.sampleRate);
    gain.setRampDurationSeconds(0.05);
    gain.prepare(spec);
    outputGainTransition.prepare(spec.sampleRate);
    juce::dsp::ProcessSpec mixerSpec = spec;
    mixerSpec.maximumBlockSize = spec.maximumBlockSize * 4 + 64;
    bandMixer.prepare(mixerSpec);
    compressorMixer.prepare(mixerSpec);
    widthMixer.prepare(mixerSpec);
    sharedBandDryDelay.prepare(spec);

    // The DC filter needs its coefficients to be calculated.
    dcFilter.prepare(spec);
    *dcFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);

    const auto numChannels = static_cast<int>(spec.numChannels);
    maximumPreparedBlockSize = juce::jmax(
        1,
        static_cast<int>(juce::jmin<juce::uint64>(
            spec.maximumBlockSize,
            static_cast<juce::uint64>(std::numeric_limits<int>::max()))));
    const auto maximumBlockSize = maximumPreparedBlockSize;

    // The oversampling object also needs to be prepared.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels, oversampleFactor, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    oversampling->initProcessing(static_cast<size_t>(maximumPreparedBlockSize));

    dryBuffer.setSize(numChannels, maximumBlockSize);
    dcFilterDryBuffer.setSize(numChannels, maximumBlockSize);
    upsampledLfoOutputs.setSize(4, maximumBlockSize * 4);
    safePeakEnvelopeBuffer.setSize(1, maximumBlockSize);
    safePeakEnvelopeBuffer.clear();

    const double safeSampleRate = std::isfinite(spec.sampleRate)
                                      && spec.sampleRate > 0.0
                                      ? spec.sampleRate
                                      : 48000.0;
    safePeakHoldSamples = juce::jmax(
        1,
        juce::roundToInt(safeSampleRate * safePeakHoldSeconds));
    safePeakReleaseCoefficient = static_cast<float>(
        std::exp(-1.0 / (safeSampleRate * safePeakReleaseSeconds)));
    safePeakEnvelope = 0.0f;
    safePeakHoldRemaining = 0;

    // Reset all smoothed values with the current sample rate and a ramp time.
    driveSmoother.reset(spec.sampleRate, 0.05);
    biasSmoother.reset(spec.sampleRate, 0.05);
    recSmoother.reset(spec.sampleRate, 0.05);
    compressorThresholdBaseSmoother.reset(spec.sampleRate, 0.01);
    compressorThresholdBaseSmoother.setCurrentAndTargetValue(0.0f);
    compressorRatioBaseSmoother.reset(spec.sampleRate, 0.01);
    compressorRatioBaseSmoother.setCurrentAndTargetValue(1.0f);
    compressorAttackBaseSmoother.reset(spec.sampleRate, 0.01);
    compressorAttackBaseSmoother.setCurrentAndTargetValue(10.0f);
    compressorReleaseBaseSmoother.reset(spec.sampleRate, 0.01);
    compressorReleaseBaseSmoother.setCurrentAndTargetValue(100.0f);
    compressorThresholdRecipeTransition.prepare(spec.sampleRate, 0.0f);
    compressorRatioRecipeTransition.prepare(spec.sampleRate, 1.0f);
    compressorAttackRecipeTransition.prepare(spec.sampleRate, 10.0f);
    compressorReleaseRecipeTransition.prepare(spec.sampleRate, 100.0f);
    shapeMixSmoother.reset(spec.sampleRate, 0.05);
    shapeMixSmoother.setCurrentAndTargetValue(1.0f);
    waveshaperModeMixSmoother.reset(spec.sampleRate, 0.01);
    waveshaperModeMixSmoother.setCurrentAndTargetValue(0.0f);

    // prepare() may be called again on an existing processor. Force the first
    // post-prepare callback to snap each primed control to the newly supplied
    // parameters instead of ramping from the previous playback configuration.
    isFirstBlock = true;
    shapeMixSmootherPrimed = false;
    compressorBaseSmoothersPrimed = false;
    waveshaperModeMixPrimed = false;
    bandEnableMixSmoother.reset(spec.sampleRate, 0.01);
    bandEnableMixSmoother.setCurrentAndTargetValue(1.0f);
    bandEnableMixPrimed = false;
    dcFilterMixSmoother.reset(spec.sampleRate, 0.01);
    dcFilterMixSmoother.setCurrentAndTargetValue(0.0f);
    dcFilterMixPrimed = false;
}

// This is what happens when we need to clear the internal state of a band's processors.
void BandProcessor::reset()
{
    isFirstBlock = true;
    shapeMixSmootherPrimed = false;
    compressorBaseSmoothersPrimed = false;
    waveshaperModeMixPrimed = false;
    bandEnableMixPrimed = false;
    dcFilterMixPrimed = false;
    compressor.reset();
    widthProcessor.reset();
    gain.reset();
    outputGainTransition.reset();
    bandMixer.reset();
    compressorMixer.reset();
    widthMixer.reset();
    sharedBandDryDelay.reset();
    dcFilter.reset();
    bandEnableMixSmoother.setCurrentAndTargetValue(1.0f);
    dcFilterMixSmoother.setCurrentAndTargetValue(0.0f);
    compressorThresholdBaseSmoother.setCurrentAndTargetValue(0.0f);
    compressorRatioBaseSmoother.setCurrentAndTargetValue(1.0f);
    compressorAttackBaseSmoother.setCurrentAndTargetValue(10.0f);
    compressorReleaseBaseSmoother.setCurrentAndTargetValue(100.0f);
    compressorThresholdRecipeTransition.reset(0.0f);
    compressorRatioRecipeTransition.reset(1.0f);
    compressorAttackRecipeTransition.reset(10.0f);
    compressorReleaseRecipeTransition.reset(100.0f);
    shapeMixSmoother.setCurrentAndTargetValue(1.0f);
    waveshaperModeMixSmoother.setCurrentAndTargetValue(0.0f);
    safePeakEnvelope = 0.0f;
    safePeakHoldRemaining = 0;
    safePeakEnvelopeBuffer.clear();

    if (oversampling)
        oversampling->reset();

    mInputLeftRMS.store(0.0f, std::memory_order_relaxed);
    mInputRightRMS.store(0.0f, std::memory_order_relaxed);
    mOutputLeftRMS.store(0.0f, std::memory_order_relaxed);
    mOutputRightRMS.store(0.0f, std::memory_order_relaxed);
    mInputLeftPeak.store(0.0f, std::memory_order_relaxed);
    mInputRightPeak.store(0.0f, std::memory_order_relaxed);
    mOutputLeftPeak.store(0.0f, std::memory_order_relaxed);
    mOutputRightPeak.store(0.0f, std::memory_order_relaxed);
    mReductionPercent.store(1.0f, std::memory_order_relaxed);
    mSampleMaxValue.store(0.0f, std::memory_order_relaxed);
}

void BandProcessor::resetQualityTransitionState() noexcept
{
    // The oversampler is the only per-band state that stops advancing in base
    // mode. Band/Enable mixers, the Shape Mix smoother, and their delay lines
    // consume every base-rate frame in both modes, so retaining them preserves
    // LFO smoother history and a continuous raw timeline while latency taps
    // change.
    if (oversampling != nullptr)
        oversampling->reset();
}

void BandProcessor::fillSafePeakEnvelope(
    const juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    jassert(numSamples <= safePeakEnvelopeBuffer.getNumSamples());
    if (numSamples <= 0 || safePeakEnvelopeBuffer.getNumSamples() <= 0)
        return;

    auto* envelopeValues = safePeakEnvelopeBuffer.getWritePointer(0);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float instantaneousPeak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float value = buffer.getSample(channel, sample);
            if (std::isfinite(value))
                instantaneousPeak = juce::jmax(instantaneousPeak,
                                                std::abs(value));
        }

        if (instantaneousPeak > 0.0f
            && instantaneousPeak >= safePeakEnvelope)
        {
            // Safe must react to a new linked-channel peak on the same sample.
            // Holding the peak avoids gain breathing between ordinary waveform
            // crests and keeps steady periodic material on the established
            // block-peak recipe.
            safePeakEnvelope = instantaneousPeak;
            safePeakHoldRemaining = juce::jmax(0,
                                                safePeakHoldSamples - 1);
        }
        else if (safePeakHoldRemaining > 0)
        {
            --safePeakHoldRemaining;
        }
        else
        {
            const float releasedPeak = safePeakEnvelope
                                       * safePeakReleaseCoefficient;
            if (instantaneousPeak >= releasedPeak)
            {
                safePeakEnvelope = instantaneousPeak;
                if (instantaneousPeak > 0.0f)
                    safePeakHoldRemaining = juce::jmax(
                        0,
                        safePeakHoldSamples - 1);
            }
            else
            {
                safePeakEnvelope = releasedPeak;
            }

            if (safePeakEnvelope < 1.0e-12f)
                safePeakEnvelope = 0.0f;
        }

        envelopeValues[sample] = safePeakEnvelope;
    }
}

//==============================================================================
// This is the new, self-contained processing function for a single band.
// It replaces the old `processOneBand` and `processDistortion` functions.
//==============================================================================
void BandProcessor::process(juce::AudioBuffer<float>& buffer,
                            const BandProcessingParameters& params,
                            const juce::AudioBuffer<float>& lfoOutputs)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    process(buffer,
            params,
            lfoOutputs,
            buffer.getMagnitude(0, buffer.getNumSamples()),
            true);
}

void BandProcessor::process(juce::AudioBuffer<float>& buffer,
                            const BandProcessingParameters& params,
                            const juce::AudioBuffer<float>& lfoOutputs,
                            float callbackInputPeak,
                            bool updateReductionMeter)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    const int totalNumSamples = buffer.getNumSamples();
    const float inputPeak = std::isfinite(callbackInputPeak)
                                ? juce::jmax(0.0f, callbackInputPeak)
                                : 0.0f;
    const int chunkCapacity = juce::jmax(1, maximumPreparedBlockSize);

    if (totalNumSamples <= chunkCapacity)
    {
        processChunk(buffer,
                     params,
                     lfoOutputs,
                     0,
                     inputPeak,
                     updateReductionMeter);
        return;
    }

    // JUCE's Oversampling stages keep fixed-size internal buffers allocated by
    // initProcessing(). Some hosts occasionally exceed their prepareToPlay()
    // block-size hint; handing such a block to processSamplesUp() writes beyond
    // those buffers in release builds. Process non-owning views instead so all
    // DSP state remains continuous without allocating or re-preparing on the
    // audio thread.
    for (int sampleOffset = 0; sampleOffset < totalNumSamples; sampleOffset += chunkCapacity)
    {
        const int samplesInChunk = juce::jmin(chunkCapacity, totalNumSamples - sampleOffset);
        juce::AudioBuffer<float> chunk(buffer.getArrayOfWritePointers(),
                                       buffer.getNumChannels(),
                                       sampleOffset,
                                       samplesInChunk);
        processChunk(chunk,
                     params,
                     lfoOutputs,
                     sampleOffset,
                     inputPeak,
                     updateReductionMeter);
    }
}

void BandProcessor::processChunk(juce::AudioBuffer<float>& buffer,
                                 const BandProcessingParameters& params,
                                 const juce::AudioBuffer<float>& lfoOutputs,
                                 int lfoSampleOffset,
                                 float inputPeak,
                                 bool updateReductionMeter)
{
    // 1. Preparation
    fillSafePeakEnvelope(buffer);
    dryBuffer.makeCopyOf(buffer, true);
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto paramsForProcessing = params; // Create a mutable copy
    const bool useHQ = params.isHQ && oversampling != nullptr;
    paramsForProcessing.isHQ = useHQ;

    const bool hasCompleteBaseRateLfoChunk = lfoSampleOffset + buffer.getNumSamples()
                                              <= lfoOutputs.getNumSamples();
    if (hasCompleteBaseRateLfoChunk
        && std::abs(paramsForProcessing.mixValProvider.modulationDepth) > 1.0e-6f
        && juce::isPositiveAndBelow(params.mixLfoSourceIndex,
                                    lfoOutputs.getNumChannels()))
    {
        paramsForProcessing.mixValProvider.lfoSignal =
            lfoOutputs.getReadPointer(params.mixLfoSourceIndex, lfoSampleOffset);
    }
    if (hasCompleteBaseRateLfoChunk
        && std::abs(paramsForProcessing.widthValProvider.modulationDepth) > 1.0e-6f
        && juce::isPositiveAndBelow(params.widthLfoSourceIndex,
                                    lfoOutputs.getNumChannels()))
    {
        paramsForProcessing.widthValProvider.lfoSignal =
            lfoOutputs.getReadPointer(params.widthLfoSourceIndex, lfoSampleOffset);
    }
    if (hasCompleteBaseRateLfoChunk
        && std::abs(paramsForProcessing.panValProvider.modulationDepth) > 1.0e-6f
        && juce::isPositiveAndBelow(params.panLfoSourceIndex,
                                    lfoOutputs.getNumChannels()))
    {
        paramsForProcessing.panValProvider.lfoSignal =
            lfoOutputs.getReadPointer(params.panLfoSourceIndex, lfoSampleOffset);
    }
    if (hasCompleteBaseRateLfoChunk
        && std::abs(paramsForProcessing.widthMixValProvider.modulationDepth) > 1.0e-6f
        && juce::isPositiveAndBelow(params.widthMixLfoSourceIndex,
                                    lfoOutputs.getNumChannels()))
    {
        paramsForProcessing.widthMixValProvider.lfoSignal =
            lfoOutputs.getReadPointer(params.widthMixLfoSourceIndex, lfoSampleOffset);
    }
    const auto bindCompressorProvider = [&](ModulatedValueProvider& provider,
                                            int sourceIndex)
    {
        if (hasCompleteBaseRateLfoChunk
            && std::abs(provider.modulationDepth) > 1.0e-6f
            && juce::isPositiveAndBelow(sourceIndex,
                                        lfoOutputs.getNumChannels()))
        {
            provider.lfoSignal = lfoOutputs.getReadPointer(sourceIndex,
                                                            lfoSampleOffset);
        }
    };
    bindCompressorProvider(paramsForProcessing.compThresholdValProvider,
                           params.compThresholdLfoSourceIndex);
    bindCompressorProvider(paramsForProcessing.compRatioValProvider,
                           params.compRatioLfoSourceIndex);
    bindCompressorProvider(paramsForProcessing.compAttackValProvider,
                           params.compAttackLfoSourceIndex);
    bindCompressorProvider(paramsForProcessing.compReleaseValProvider,
                           params.compReleaseLfoSourceIndex);
    bindCompressorProvider(paramsForProcessing.compMixValProvider,
                           params.compMixLfoSourceIndex);

    // Band Enable bypasses the complete processed band, including the user's
    // own Band Mix. Align the shared dry path to the oversampled wet path once
    // so both coefficient stages retain the established Thiran phase response.
    sharedBandDryDelay.setDelay(useHQ ? oversampling->getLatencyInSamples()
                                      : 0.0f);
    auto bandEnableDryBlock = juce::dsp::AudioBlock<float>(dryBuffer);
    sharedBandDryDelay.process(
        juce::dsp::ProcessContextReplacing<float>(bandEnableDryBlock));
    bandMixer.pushDrySamples(bandEnableDryBlock);

    // 2. Core Distortion Processing
    if (useHQ)
    {
        auto oversampledBlock = oversampling->processSamplesUp(block);

        // --- LFO Upsampling ---
        upsampledLfoOutputs.setSize(lfoOutputs.getNumChannels(),
                                    static_cast<int>(oversampledBlock.getNumSamples()),
                                    false,
                                    false,
                                    true);
        upsampledLfoOutputs.clear();
        if (lfoOutputs.getNumSamples() > 0 && upsampledLfoOutputs.getNumSamples() > 0)
        {
            const int baseSamplesInChunk = buffer.getNumSamples();
            const int oversamplingRatio = baseSamplesInChunk > 0
                                              ? upsampledLfoOutputs.getNumSamples() / baseSamplesInChunk
                                              : 1;
            const int safeOversamplingRatio = juce::jmax(1, oversamplingRatio);

            for (int channel = 0; channel < lfoOutputs.getNumChannels(); ++channel)
            {
                auto* dest = upsampledLfoOutputs.getWritePointer(channel);
                const auto* src = lfoOutputs.getReadPointer(channel);

                if (lfoOutputs.getNumSamples() == 1)
                {
                    juce::FloatVectorOperations::fill(dest, src[0], upsampledLfoOutputs.getNumSamples());
                    continue;
                }

                // LFO samples describe base-rate instants. Repeat each value
                // for the corresponding oversampled interval so the mapping is
                // independent of host callback and internal chunk boundaries.
                // The previous endpoint-normalised interpolation changed its
                // step with every callback length, producing different audio
                // for the same timeline when a host repartitioned its blocks.
                for (int i = 0; i < upsampledLfoOutputs.getNumSamples(); ++i)
                {
                    const int sourceIndex = juce::jlimit(
                        0,
                        lfoOutputs.getNumSamples() - 1,
                        lfoSampleOffset + i / safeOversamplingRatio);
                    dest[i] = src[sourceIndex];
                }
            }
        }

        // Bind the upsampled LFO signals to the providers
        if (juce::isPositiveAndBelow(params.driveLfoSourceIndex, upsampledLfoOutputs.getNumChannels()))
            paramsForProcessing.driveVal.lfoSignal = upsampledLfoOutputs.getReadPointer(params.driveLfoSourceIndex);
        if (juce::isPositiveAndBelow(params.biasLfoSourceIndex, upsampledLfoOutputs.getNumChannels()))
            paramsForProcessing.biasVal.lfoSignal = upsampledLfoOutputs.getReadPointer(params.biasLfoSourceIndex);
        if (juce::isPositiveAndBelow(params.recLfoSourceIndex, upsampledLfoOutputs.getNumChannels()))
            paramsForProcessing.recVal.lfoSignal = upsampledLfoOutputs.getReadPointer(params.recLfoSourceIndex);
        if (std::abs(paramsForProcessing.shapeMixValProvider.modulationDepth) > 1.0e-6f
            && juce::isPositiveAndBelow(params.shapeMixLfoSourceIndex,
                                        upsampledLfoOutputs.getNumChannels()))
        {
            paramsForProcessing.shapeMixValProvider.lfoSignal =
                upsampledLfoOutputs.getReadPointer(params.shapeMixLfoSourceIndex);
        }
        // Output gain runs after downsampling, so it must use the original-rate
        // LFO. Using the oversampled signal here consumed only its first quarter.
        if (juce::isPositiveAndBelow(params.outputLfoSourceIndex, lfoOutputs.getNumChannels())
            && lfoSampleOffset + buffer.getNumSamples() <= lfoOutputs.getNumSamples())
            paramsForProcessing.outputVal.lfoSignal = lfoOutputs.getReadPointer(params.outputLfoSourceIndex,
                                                                                 lfoSampleOffset);

        processDistortion(oversampledBlock,
                          paramsForProcessing,
                          safePeakEnvelopeBuffer.getReadPointer(0),
                          buffer.getNumSamples(),
                          inputPeak,
                          updateReductionMeter);
        oversampling->processSamplesDown(block);
    }
    else
    {
        // Bind the original LFO signals to the providers
        if (hasCompleteBaseRateLfoChunk
            && juce::isPositiveAndBelow(params.driveLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.driveVal.lfoSignal = lfoOutputs.getReadPointer(params.driveLfoSourceIndex,
                                                                                lfoSampleOffset);
        if (hasCompleteBaseRateLfoChunk
            && juce::isPositiveAndBelow(params.biasLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.biasVal.lfoSignal = lfoOutputs.getReadPointer(params.biasLfoSourceIndex,
                                                                               lfoSampleOffset);
        if (hasCompleteBaseRateLfoChunk
            && juce::isPositiveAndBelow(params.recLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.recVal.lfoSignal = lfoOutputs.getReadPointer(params.recLfoSourceIndex,
                                                                              lfoSampleOffset);
        if (hasCompleteBaseRateLfoChunk
            && juce::isPositiveAndBelow(params.outputLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.outputVal.lfoSignal = lfoOutputs.getReadPointer(params.outputLfoSourceIndex,
                                                                                 lfoSampleOffset);
        if (hasCompleteBaseRateLfoChunk
            && std::abs(paramsForProcessing.shapeMixValProvider.modulationDepth) > 1.0e-6f
            && juce::isPositiveAndBelow(params.shapeMixLfoSourceIndex,
                                        lfoOutputs.getNumChannels()))
        {
            paramsForProcessing.shapeMixValProvider.lfoSignal =
                lfoOutputs.getReadPointer(params.shapeMixLfoSourceIndex, lfoSampleOffset);
        }

        processDistortion(block,
                          paramsForProcessing,
                          safePeakEnvelopeBuffer.getReadPointer(0),
                          buffer.getNumSamples(),
                          inputPeak,
                          updateReductionMeter);
    }

    // The DC filter is designed at the base sample rate, so both normal and HQ
    // paths meet here after any downsampling. Keep its hidden wet path running
    // while bypassed and crossfade the audible result; skipping the IIR froze
    // its state and hard-switched up to a large DC offset at block boundaries.
    processDcFilter(buffer, params.isDcFilterEnabled);

    // 3. Block-wise Compressor and Width
    // These operate on the downsampled block, so their mixers are safe.
    auto postDistortionContext = juce::dsp::ProcessContextReplacing<float>(block);
    // Keep the compressor detector and dry path warm while bypassed. Gating the
    // complete branch hard-switched between compressed and dry audio at a block
    // boundary; using the mixer's existing 50 ms ramp removes that click.
    const bool hasThresholdModulation = paramsForProcessing.compThresholdValProvider.lfoSignal
                                        != nullptr;
    const bool hasRatioModulation = paramsForProcessing.compRatioValProvider.lfoSignal
                                    != nullptr;
    const bool hasAttackModulation = paramsForProcessing.compAttackValProvider.lfoSignal
                                     != nullptr;
    const bool hasReleaseModulation = paramsForProcessing.compReleaseValProvider.lfoSignal
                                      != nullptr;
    const bool hasModulatedCompressorCore = hasThresholdModulation
                                            || hasRatioModulation
                                            || hasAttackModulation
                                            || hasReleaseModulation;
    compressorMixer.pushDrySamples(postDistortionContext.getOutputBlock());
    const auto safeThreshold = [](float value)
    {
        // Keep the public BandProcessor's legacy finite-value contract.  APVTS
        // values are already constrained to the UI range, but direct callers
        // historically could use a wider threshold.
        return std::isfinite(value) ? value : 0.0f;
    };
    const auto safeRatio = [](float value)
    {
        return std::isfinite(value) ? juce::jmax(1.0f, value) : 1.0f;
    };
    const auto safeAttack = [](float value)
    {
        return std::isfinite(value) ? juce::jmax(0.01f, value) : 10.0f;
    };
    const auto safeRelease = [](float value)
    {
        return std::isfinite(value) ? juce::jmax(0.01f, value) : 100.0f;
    };

    // Smooth only the user-controlled base values. A routed LFO is still
    // evaluated directly for every sample after that base has moved, so its
    // waveform, depth, and timing remain sample-accurate.
    const float thresholdBaseTarget = safeThreshold(
        hasThresholdModulation
            ? paramsForProcessing.compThresholdValProvider.baseValue
            : params.compThreshold);
    const float ratioBaseTarget = safeRatio(
        hasRatioModulation
            ? paramsForProcessing.compRatioValProvider.baseValue
            : params.compRatio);
    const float attackBaseTarget = safeAttack(
        hasAttackModulation
            ? paramsForProcessing.compAttackValProvider.baseValue
            : params.compAttack);
    const float releaseBaseTarget = safeRelease(
        hasReleaseModulation
            ? paramsForProcessing.compReleaseValProvider.baseValue
            : params.compRelease);
    if (! compressorBaseSmoothersPrimed)
    {
        compressorThresholdBaseSmoother.setCurrentAndTargetValue(
            thresholdBaseTarget);
        compressorRatioBaseSmoother.setCurrentAndTargetValue(ratioBaseTarget);
        compressorAttackBaseSmoother.setCurrentAndTargetValue(attackBaseTarget);
        compressorReleaseBaseSmoother.setCurrentAndTargetValue(
            releaseBaseTarget);
        compressorBaseSmoothersPrimed = true;
    }
    else
    {
        compressorThresholdBaseSmoother.setTargetValue(thresholdBaseTarget);
        compressorRatioBaseSmoother.setTargetValue(ratioBaseTarget);
        compressorAttackBaseSmoother.setTargetValue(attackBaseTarget);
        compressorReleaseBaseSmoother.setTargetValue(releaseBaseTarget);
    }

    auto thresholdProvider = paramsForProcessing.compThresholdValProvider;
    auto ratioProvider = paramsForProcessing.compRatioValProvider;
    auto attackProvider = paramsForProcessing.compAttackValProvider;
    auto releaseProvider = paramsForProcessing.compReleaseValProvider;

    const float initialThresholdBase =
        compressorThresholdBaseSmoother.getCurrentValue();
    const float initialRatioBase =
        compressorRatioBaseSmoother.getCurrentValue();
    const float initialAttackBase =
        compressorAttackBaseSmoother.getCurrentValue();
    const float initialReleaseBase =
        compressorReleaseBaseSmoother.getCurrentValue();
    thresholdProvider.baseValue = initialThresholdBase;
    ratioProvider.baseValue = initialRatioBase;
    attackProvider.baseValue = initialAttackBase;
    releaseProvider.baseValue = initialReleaseBase;

    const float initialThresholdTarget = safeThreshold(
        hasThresholdModulation ? thresholdProvider.get(0)
                               : initialThresholdBase);
    const float initialRatioTarget = safeRatio(
        hasRatioModulation ? ratioProvider.get(0) : initialRatioBase);
    const float initialAttackTarget = safeAttack(
        hasAttackModulation ? attackProvider.get(0) : initialAttackBase);
    const float initialReleaseTarget = safeRelease(
        hasReleaseModulation ? releaseProvider.get(0)
                             : initialReleaseBase);

    serviceCompressorRecipeTransition(
        compressorThresholdRecipeTransition,
        makeCompressorRecipe(thresholdProvider,
                             params.compThresholdLfoSourceIndex),
        initialThresholdTarget);
    serviceCompressorRecipeTransition(
        compressorRatioRecipeTransition,
        makeCompressorRecipe(ratioProvider, params.compRatioLfoSourceIndex),
        initialRatioTarget);
    serviceCompressorRecipeTransition(
        compressorAttackRecipeTransition,
        makeCompressorRecipe(attackProvider, params.compAttackLfoSourceIndex),
        initialAttackTarget);
    serviceCompressorRecipeTransition(
        compressorReleaseRecipeTransition,
        makeCompressorRecipe(releaseProvider,
                             params.compReleaseLfoSourceIndex),
        initialReleaseTarget);

    const bool compressorBaseIsSmoothing =
        compressorThresholdBaseSmoother.isSmoothing()
        || compressorRatioBaseSmoother.isSmoothing()
        || compressorAttackBaseSmoother.isSmoothing()
        || compressorReleaseBaseSmoother.isSmoothing();
    const bool compressorRecipeIsSmoothing =
        compressorThresholdRecipeTransition.routeTransitionMix.isSmoothing()
        || compressorRatioRecipeTransition.routeTransitionMix.isSmoothing()
        || compressorAttackRecipeTransition.routeTransitionMix.isSmoothing()
        || compressorReleaseRecipeTransition.routeTransitionMix.isSmoothing();
    const bool needsSampleAccurateCompressorCore =
        hasModulatedCompressorCore || compressorBaseIsSmoothing
        || compressorRecipeIsSmoothing;

    if (! needsSampleAccurateCompressorCore)
    {
        const float threshold =
            compressorThresholdBaseSmoother.getTargetValue();
        const float ratio = compressorRatioBaseSmoother.getTargetValue();
        const float attack = compressorAttackBaseSmoother.getTargetValue();
        const float release = compressorReleaseBaseSmoother.getTargetValue();
        this->compressor.setThreshold(threshold);
        this->compressor.setRatio(ratio);
        this->compressor.setAttack(attack);
        this->compressor.setRelease(release);
        this->compressor.process(postDistortionContext);

        compressorThresholdRecipeTransition.lastAppliedValue = threshold;
        compressorRatioRecipeTransition.lastAppliedValue = ratio;
        compressorAttackRecipeTransition.lastAppliedValue = attack;
        compressorReleaseRecipeTransition.lastAppliedValue = release;
    }
    else
    {
        auto* const* channelData = buffer.getArrayOfWritePointers();
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float thresholdBase =
                compressorThresholdBaseSmoother.getCurrentValue();
            const float ratioBase =
                compressorRatioBaseSmoother.getCurrentValue();
            const float attackBase =
                compressorAttackBaseSmoother.getCurrentValue();
            const float releaseBase =
                compressorReleaseBaseSmoother.getCurrentValue();
            thresholdProvider.baseValue = thresholdBase;
            ratioProvider.baseValue = ratioBase;
            attackProvider.baseValue = attackBase;
            releaseProvider.baseValue = releaseBase;

            const float thresholdTarget = safeThreshold(
                hasThresholdModulation ? thresholdProvider.get(sample)
                                       : thresholdBase);
            const float ratioTarget = safeRatio(
                hasRatioModulation ? ratioProvider.get(sample) : ratioBase);
            const float attackTarget = safeAttack(
                hasAttackModulation ? attackProvider.get(sample)
                                    : attackBase);
            const float releaseTarget = safeRelease(
                hasReleaseModulation ? releaseProvider.get(sample)
                                     : releaseBase);

            const float threshold = safeThreshold(
                applyCompressorRecipeTransition(
                    compressorThresholdRecipeTransition,
                    thresholdTarget,
                    false));
            const float ratio = safeRatio(applyCompressorRecipeTransition(
                compressorRatioRecipeTransition,
                ratioTarget,
                false));
            const float attack = safeAttack(applyCompressorRecipeTransition(
                compressorAttackRecipeTransition,
                attackTarget,
                true));
            const float release = safeRelease(applyCompressorRecipeTransition(
                compressorReleaseRecipeTransition,
                releaseTarget,
                true));

            this->compressor.setThreshold(threshold);
            this->compressor.setRatio(ratio);
            this->compressor.setAttack(attack);
            this->compressor.setRelease(release);

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                channelData[channel][sample] = this->compressor.processSample(
                    channel, channelData[channel][sample]);
            }

            compressorThresholdRecipeTransition.lastAppliedValue = threshold;
            compressorRatioRecipeTransition.lastAppliedValue = ratio;
            compressorAttackRecipeTransition.lastAppliedValue = attack;
            compressorReleaseRecipeTransition.lastAppliedValue = release;

            // The event sample itself uses the previous audible base, then the
            // base and route bridges advance once after every channel has
            // consumed it.
            compressorThresholdRecipeTransition.routeTransitionMix.getNextValue();
            compressorRatioRecipeTransition.routeTransitionMix.getNextValue();
            compressorAttackRecipeTransition.routeTransitionMix.getNextValue();
            compressorReleaseRecipeTransition.routeTransitionMix.getNextValue();
            compressorThresholdBaseSmoother.getNextValue();
            compressorRatioBaseSmoother.getNextValue();
            compressorAttackBaseSmoother.getNextValue();
            compressorReleaseBaseSmoother.getNextValue();
        }
    }

    compressorMixer.mixWetSamples(
        postDistortionContext.getOutputBlock(),
        paramsForProcessing.compMixValProvider,
        params.compMixVal,
        params.compMixLfoSourceIndex,
        params.isCompEnabled);
    if (buffer.getNumChannels() == 2)
    {
        // Keep the width path warm and use the mixer's existing 50 ms ramp for
        // bypass transitions. Skipping the complete branch when Stereo was
        // disabled hard-switched between wet and dry samples at a block
        // boundary, which could produce an audible click at extreme Width/Pan
        // settings.
        const bool hasSampleAccurateWidth = paramsForProcessing.widthValProvider.lfoSignal != nullptr
                                            || paramsForProcessing.panValProvider.lfoSignal != nullptr;
        widthMixer.pushDrySamples(postDistortionContext.getOutputBlock());
        if (hasSampleAccurateWidth)
        {
            auto widthProvider = paramsForProcessing.widthValProvider;
            auto panProvider = paramsForProcessing.panValProvider;
            if (widthProvider.lfoSignal == nullptr)
                widthProvider.baseValue = params.width;
            if (panProvider.lfoSignal == nullptr)
                panProvider.baseValue = params.pan;
            this->widthProcessor.process(buffer.getWritePointer(0),
                                         buffer.getWritePointer(1),
                                         widthProvider,
                                         panProvider,
                                         params.widthLfoSourceIndex,
                                         params.panLfoSourceIndex,
                                         buffer.getNumSamples());
        }
        else
        {
            this->widthProcessor.process(buffer.getWritePointer(0),
                                         buffer.getWritePointer(1),
                                         params.width,
                                         params.pan,
                                         buffer.getNumSamples());
        }

        widthMixer.mixWetSamples(
            postDistortionContext.getOutputBlock(),
            paramsForProcessing.widthMixValProvider,
            params.widthMixVal,
            params.widthMixLfoSourceIndex,
            params.isWidthEnabled);
    }

    // 4. Post-Distortion Effects
    // Per-sample Output Gain
    applyGain(buffer,
              paramsForProcessing.outputVal,
              gain,
              outputGainTransition,
              params.outputLfoSourceIndex);

    // 5. Final Dry/Wet Mix. The dry samples supplied above already contain
    // JUCE's original HQ Thiran latency compensation. Keep the coefficient
    // stage zero-latency so stable routed modulation follows every LFO sample;
    // scalar automation still uses the legacy 50 ms ramp.
    bandMixer.mixWetSamples(block,
                            paramsForProcessing.mixValProvider,
                            params.mixVal,
                            params.mixLfoSourceIndex,
                            true);

    processBandEnable(buffer, params.isBandEnabled);
}

void BandProcessor::processBandEnable(juce::AudioBuffer<float>& buffer,
                                      bool enabled)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    const float targetMix = enabled ? 1.0f : 0.0f;
    if (! bandEnableMixPrimed)
    {
        bandEnableMixSmoother.setCurrentAndTargetValue(targetMix);
        bandEnableMixPrimed = true;
    }
    else
    {
        bandEnableMixSmoother.setTargetValue(targetMix);
    }

    if (! bandEnableMixSmoother.isSmoothing())
    {
        if (bandEnableMixSmoother.getCurrentValue() <= 0.0f)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.copyFrom(channel, 0,
                                dryBuffer, channel, 0,
                                buffer.getNumSamples());
        }
        return;
    }

    auto* const* wetChannels = buffer.getArrayOfWritePointers();
    const auto* const* dryChannels = dryBuffer.getArrayOfReadPointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float mix = bandEnableMixSmoother.getCurrentValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float dry = dryChannels[channel][sample];
            wetChannels[channel][sample] = dry
                                          + mix * (wetChannels[channel][sample]
                                                   - dry);
        }
        bandEnableMixSmoother.getNextValue();
    }
}

void BandProcessor::processDistortion(juce::dsp::AudioBlock<float>& blockToProcess,
                                      const BandProcessingParameters& params,
                                      const float* safePeakEnvelopeValues,
                                      int safePeakEnvelopeSamples,
                                      float inputPeak,
                                      bool updateReductionMeter)
{
    const int numSamples = static_cast<int>(blockToProcess.getNumSamples());
    const int numChannels = static_cast<int>(blockToProcess.getNumChannels());
    const int smoothingStride = params.isHQ ? (1 << oversampleFactor) : 1;
    const auto getSafePeak = [&](int sample) noexcept
    {
        if (safePeakEnvelopeValues == nullptr || safePeakEnvelopeSamples <= 0)
            return 0.0f;

        const int baseSample = juce::jlimit(0,
                                            safePeakEnvelopeSamples - 1,
                                            sample / smoothingStride);
        const float peak = safePeakEnvelopeValues[baseSample];
        return std::isfinite(peak) ? juce::jmax(0.0f, peak) : 0.0f;
    };

    // Shape Mix historically wraps the complete distortion stage, including
    // Drive. Once Shape is disabled its controls are frozen in the UI, so a
    // stale Mix value must not silently blend Drive back to the dry signal.
    // Keep the existing Shape-on sound, but make Shape-off bypass only the
    // Shape controls (Bias/Rectification/DC) rather than the Drive module.
    const bool hasSampleAccurateShapeMix = params.isShapeEnabled
                                           && params.shapeMixValProvider.lfoSignal != nullptr;
    const float requestedShapeMix = hasSampleAccurateShapeMix
                                        ? params.shapeMixValProvider.get(0)
                                        : params.shapeMixVal;
    const float effectiveShapeMix = params.isShapeEnabled
                                      ? juce::jlimit(0.0f, 1.0f, requestedShapeMix)
                                      : 1.0f;
    if (! shapeMixSmootherPrimed)
    {
        shapeMixSmoother.setCurrentAndTargetValue(effectiveShapeMix);
        shapeMixSmootherPrimed = true;
    }
    else
    {
        shapeMixSmoother.setTargetValue(effectiveShapeMix);
    }

    // Keep the callback maximum for UI telemetry only. The audible Safe path
    // uses the causal per-sample envelope above; allowing this future-looking
    // maximum to drive the audio made identical timelines sound different at
    // different host callback sizes.
    const float sampleMaxValue = std::isfinite(inputPeak) ? juce::jmax(0.0f, inputPeak) : 0.0f;
    mSampleMaxValue.store(sampleMaxValue, std::memory_order_relaxed);

    const auto normaliseMode = [] (int mode) noexcept
    {
        return juce::isPositiveAndBelow(mode, 12) ? mode : 3;
    };
    const auto serviceModeRequest = [&] (int rawRequestedMode)
    {
        const int requestedMode = normaliseMode(rawRequestedMode);
        requestedWaveshaperMode = requestedMode;

        if (! waveshaperModeMixPrimed)
        {
            waveshaperModeSlots = { requestedMode, requestedMode };
            waveshaperModeMixSmoother.setCurrentAndTargetValue(0.0f);
            waveshaperModeMixPrimed = true;
            return;
        }

        if (waveshaperModeMixSmoother.isSmoothing())
        {
            const int targetSlot = waveshaperModeMixSmoother.getTargetValue() >= 0.5f ? 1 : 0;
            const int sourceSlot = 1 - targetSlot;

            // A request for the mode we are fading away from reverses the
            // current transition without changing either audible function.
            // A third mode remains queued in requestedWaveshaperMode until an
            // endpoint makes one slot fully inaudible.
            if (requestedMode == waveshaperModeSlots[static_cast<size_t>(sourceSlot)])
                waveshaperModeMixSmoother.setTargetValue(sourceSlot == 1 ? 1.0f : 0.0f);
            return;
        }

        const int activeSlot = waveshaperModeMixSmoother.getCurrentValue() >= 0.5f ? 1 : 0;
        if (requestedMode == waveshaperModeSlots[static_cast<size_t>(activeSlot)])
            return;

        const int targetSlot = 1 - activeSlot;
        waveshaperModeSlots[static_cast<size_t>(targetSlot)] = requestedMode;
        waveshaperModeMixSmoother.setTargetValue(targetSlot == 1 ? 1.0f : 0.0f);
    };

    serviceModeRequest(params.mode);
    auto mode0Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[0]);
    auto mode1Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[1]);

    // The providers are now correctly prepared with LFO signals (if any)
    auto driveProvider = params.driveVal;
    auto biasProvider = params.biasVal;
    auto recProvider = params.recVal;

    if (! params.isShapeEnabled)
    {
        // Disable LFO modulation for Bias and Rectification
        biasProvider.baseValue = 0.0f;
        recProvider.baseValue = 0.0f;
        biasProvider.lfoSignal = nullptr;
        recProvider.lfoSignal = nullptr;
    }

    DistortionLogic::State currentState;
    currentState.mode = params.mode;

    if (isFirstBlock)
    {
        // Prime the ordinary Drive dezipper from the requested first-sample
        // gain. The causal Safe ceiling is applied below on that same sample,
        // so startup has neither a fade from unity nor callback lookahead.
        float initialRequestedDriveGain;

        if (! params.isDriveEnabled)
        {
            // If bypassed at startup, initialize the smoother to a gain of 1.0.
            initialRequestedDriveGain = 1.0f;
        }
        else
        {
            float initialDrive = driveProvider.get(0); // Get LFO-modulated value for sample 0
            if (params.isExtremeModeOn)
                initialDrive = log2f(10.0f) * initialDrive;
            const float initialDriveForCalc = initialDrive * 6.5f / 100.0f;
            initialRequestedDriveGain = std::pow(2.0f,
                                                  initialDriveForCalc);
        }

        driveSmoother.setCurrentAndTargetValue(initialRequestedDriveGain);

        // Initialize Bias and Rec smoothers with their final modulated value for sample 0
        biasSmoother.setCurrentAndTargetValue(biasProvider.get(0));
        recSmoother.setCurrentAndTargetValue(recProvider.get(0));

        isFirstBlock = false;
    }

    float currentShapeMix = shapeMixSmoother.getCurrentValue();
    float finalReductionDriveForCalc = 0.0f;
    float finalReductionDriveGain = 1.0f;
    bool hasReductionForRange = false;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        if ((sample % smoothingStride) == 0)
        {
            const float targetShapeMix = hasSampleAccurateShapeMix
                                             ? juce::jlimit(
                                                   0.0f,
                                                   1.0f,
                                                   params.shapeMixValProvider.get(sample))
                                             : effectiveShapeMix;
            shapeMixSmoother.setTargetValue(targetShapeMix);
            currentShapeMix = shapeMixSmoother.getNextValue();
        }

        // 1. Get the final, LFO-modulated value for each parameter for the CURRENT sample.
        float currentDrive = driveProvider.get(sample);
        const float currentBias = biasProvider.get(sample);
        const float currentRec = recProvider.get(sample);

        // 2. Calculate the requested Drive gain. User/LFO changes retain the
        // established 50 ms dezipper; Safe applies an independent causal
        // ceiling after that smoother.
        if (params.isExtremeModeOn)
            currentDrive = log2f(10.0f) * currentDrive;

        const float driveForCalc = currentDrive * 6.5f / 100.0f;
        const float requestedDriveGain = params.isDriveEnabled
                                             ? std::pow(2.0f, driveForCalc)
                                             : 1.0f;

        // 3. Smooth the requested controls. A Safe reduction is allowed to
        // move down immediately, but releasing that reduction still returns
        // through this same 50 ms Drive ramp.
        driveSmoother.setTargetValue(requestedDriveGain);
        biasSmoother.setTargetValue(currentBias);
        recSmoother.setTargetValue(currentRec);

        // In HQ mode the block contains 4x as many samples, while these
        // smoothers were prepared at the base sample rate. Advance them once
        // per base-rate sample so their time constants do not become 4x faster.
        if ((sample % smoothingStride) == 0)
        {
            currentState.drive = driveSmoother.getNextValue();
            currentState.bias = biasSmoother.getNextValue();
            currentState.rec = recSmoother.getNextValue();

            if (params.isDriveEnabled && params.isSafeModeOn)
            {
                const float causalPeak = getSafePeak(sample);
                if (causalPeak > 0.0001f)
                {
                    const float safeCeiling = 2.0f / causalPeak
                                              + 0.1f * driveForCalc;
                    if (std::isfinite(safeCeiling)
                        && safeCeiling < currentState.drive)
                    {
                        currentState.drive = safeCeiling;
                        driveSmoother.setCurrentAndTargetValue(safeCeiling);
                    }
                }
            }
        }
        else
        {
            currentState.drive = driveSmoother.getCurrentValue();
            currentState.bias = biasSmoother.getCurrentValue();
            currentState.rec = recSmoother.getCurrentValue();
        }

        // Publish the causal Safe result at base rate. Internal oversized
        // chunks share this state, so the meter and the audio follow the same
        // absolute timeline rather than whichever callback peak arrived first.
        if ((sample % smoothingStride) == 0 && updateReductionMeter)
        {
            finalReductionDriveForCalc = driveForCalc;
            finalReductionDriveGain = currentState.drive;
            hasReductionForRange = true;
        }

        // 5. Apply audio processing using the correctly smoothed values
        const float modeMix = waveshaperModeMixSmoother.getCurrentValue();
        const float negativeScale = (0.5f - currentState.rec) * 2.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float currentSample = blockToProcess.getSample(channel, sample);
            const float drySample = currentSample;

            currentSample *= currentState.drive;
            currentSample += currentState.bias;

            const auto shapeAndRectify = [&] (DistortionLogic::WaveshaperFunction function)
            {
                auto shaped = function(currentSample);
                if (shaped < 0.0f)
                    shaped *= negativeScale;
                return shaped;
            };

            if (modeMix <= 0.0f)
            {
                currentSample = shapeAndRectify(mode0Function);
            }
            else if (modeMix >= 1.0f)
            {
                currentSample = shapeAndRectify(mode1Function);
            }
            else
            {
                const auto mode0Sample = shapeAndRectify(mode0Function);
                const auto mode1Sample = shapeAndRectify(mode1Function);
                currentSample = mode0Sample
                              + modeMix * (mode1Sample - mode0Sample);
            }

            currentSample -= currentState.bias;

            // Shape Mix is a base-rate control even though the distortion is
            // evaluated at 4x in HQ mode. Reuse one weight for the complete
            // oversampled frame so its 50 ms ramp has the same wall-clock
            // duration in both quality modes.
            currentSample *= currentShapeMix;
            currentSample += drySample * (1.0f - currentShapeMix);

            blockToProcess.setSample(channel, sample, currentSample);
        }

        // The HQ distortion loop contains four oversampled frames for each
        // base-rate frame.  Hold the same crossfade weight for that whole
        // group and advance only at its end, so the transition remains 10 ms
        // in both modes and across internal chunks.
        if (((sample + 1) % smoothingStride) == 0
            && waveshaperModeMixSmoother.isSmoothing())
        {
            waveshaperModeMixSmoother.getNextValue();
            if (! waveshaperModeMixSmoother.isSmoothing())
            {
                serviceModeRequest(requestedWaveshaperMode);
                mode0Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[0]);
                mode1Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[1]);
            }
        }
    }

    if (hasReductionForRange)
    {
        float reduction = 1.0f;
        if (params.isDriveEnabled && params.isSafeModeOn
            && std::abs(finalReductionDriveForCalc) > 1.0e-8f)
        {
            reduction = juce::jlimit(
                0.0f,
                1.0f,
                std::log2(juce::jmax(finalReductionDriveGain, 1.0e-12f))
                    / finalReductionDriveForCalc);
        }
        mReductionPercent.store(reduction, std::memory_order_relaxed);
    }
}

void BandProcessor::processDcFilter(juce::AudioBuffer<float>& buffer,
                                    bool enabled)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    dcFilterDryBuffer.makeCopyOf(buffer, true);

    const float targetMix = enabled ? 1.0f : 0.0f;
    if (! dcFilterMixPrimed)
    {
        dcFilterMixSmoother.setCurrentAndTargetValue(targetMix);
        dcFilterMixPrimed = true;
    }
    else
    {
        dcFilterMixSmoother.setTargetValue(targetMix);
    }

    auto block = juce::dsp::AudioBlock<float>(buffer);
    dcFilter.process(juce::dsp::ProcessContextReplacing<float>(block));

    // Preserve both legacy endpoints exactly. Only an active transition needs
    // sample-by-sample blending, and the one shared mix value is advanced once
    // per base-rate sample for identical timing in mono, stereo and HQ modes.
    if (! dcFilterMixSmoother.isSmoothing())
    {
        if (dcFilterMixSmoother.getCurrentValue() <= 0.0f)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.copyFrom(channel, 0,
                                dcFilterDryBuffer, channel, 0,
                                buffer.getNumSamples());
        }
        return;
    }

    auto* const* wetChannels = buffer.getArrayOfWritePointers();
    const auto* const* dryChannels = dcFilterDryBuffer.getArrayOfReadPointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float mix = dcFilterMixSmoother.getCurrentValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float dry = dryChannels[channel][sample];
            wetChannels[channel][sample] = dry
                                          + mix * (wetChannels[channel][sample]
                                                   - dry);
        }
        dcFilterMixSmoother.getNextValue();
    }
}

//==============================================================================
FireAudioProcessor::CachedParameter FireAudioProcessor::cacheParameter(const juce::String& parameterID)
{
    return { treeState.getRawParameterValue(parameterID),
             treeState.getParameter(parameterID) };
}

float FireAudioProcessor::loadCachedParameter(const CachedParameter& parameter, float fallback) noexcept
{
    if (parameter.raw == nullptr)
        return fallback;

    const float value = parameter.raw->load(std::memory_order_relaxed);
    return std::isfinite(value) ? value : fallback;
}

float FireAudioProcessor::getBlockModulatedValue(const CachedParameter& parameter,
                                                 const juce::AudioBuffer<float>& lfoOutputs) const noexcept
{
    return getModulatedValueAtSample(parameter, lfoOutputs, 0);
}

float FireAudioProcessor::getModulatedValueAtSample(const CachedParameter& parameter,
                                                    const juce::AudioBuffer<float>& lfoOutputs,
                                                    int sampleIndex) const noexcept
{
    const float defaultValue = parameter.ranged != nullptr
                                   ? parameter.ranged->convertFrom0to1(parameter.ranged->getDefaultValue())
                                   : 0.0f;
    const float baseValue = loadCachedParameter(parameter, defaultValue);
    if (parameter.ranged == nullptr)
        return baseValue;

    LfoManager::AudioThreadRoutingInfo routingInfo;
    if (! lfoManager->getAudioThreadRoutingInfo(parameter.ranged, routingInfo)
        || ! juce::isPositiveAndBelow(routingInfo.sourceLfoIndex, lfoOutputs.getNumChannels())
        || ! juce::isPositiveAndBelow(sampleIndex, lfoOutputs.getNumSamples()))
        return baseValue;

    float lfoValue = lfoOutputs.getSample(routingInfo.sourceLfoIndex, sampleIndex);
    if (! std::isfinite(lfoValue))
        return baseValue;

    const float rawNormalisedBase = parameter.ranged->getValue();
    const float normalisedBase = std::isfinite(rawNormalisedBase)
                                     ? juce::jlimit(0.0f, 1.0f, rawNormalisedBase)
                                     : parameter.ranged->getDefaultValue();

    float effectiveDepth = routingInfo.depth;
    if (routingInfo.isBipolar)
    {
        lfoValue = lfoValue * 2.0f - 1.0f;
        effectiveDepth *= 0.5f;
    }

    const float normalisedValue = juce::jlimit(0.0f, 1.0f,
                                               normalisedBase + lfoValue * effectiveDepth);
    const float value = parameter.ranged->convertFrom0to1(normalisedValue);
    return std::isfinite(value) ? value : baseValue;
}

ChainSettings FireAudioProcessor::getCachedChainSettings(const juce::AudioBuffer<float>* lfoOutputs) const noexcept
{
    const auto getValue = [this, lfoOutputs](const CachedParameter& parameter)
    {
        return lfoOutputs != nullptr
                   ? getBlockModulatedValue(parameter, *lfoOutputs)
                   : loadCachedParameter(parameter);
    };

    ChainSettings settings;
    settings.lowCutFreq = getValue(filterParameterCache.lowCutFrequency);
    settings.lowCutGainInDecibels = getValue(filterParameterCache.lowCutGain);
    settings.lowCutQuality = getValue(filterParameterCache.lowCutQuality);
    settings.lowCutSlope = getSlopeParameterValue(filterParameterCache.lowCutSlope.raw);
    settings.lowCutBypassed = loadCachedParameter(filterParameterCache.lowCutBypassed) > 0.5f;

    settings.peakFreq = getValue(filterParameterCache.peakFrequency);
    settings.peakGainInDecibels = getValue(filterParameterCache.peakGain);
    settings.peakQuality = getValue(filterParameterCache.peakQuality);
    settings.peakBypassed = loadCachedParameter(filterParameterCache.peakBypassed) > 0.5f;

    settings.highCutFreq = getValue(filterParameterCache.highCutFrequency);
    settings.highCutGainInDecibels = getValue(filterParameterCache.highCutGain);
    settings.highCutQuality = getValue(filterParameterCache.highCutQuality);
    settings.highCutSlope = getSlopeParameterValue(filterParameterCache.highCutSlope.raw);
    settings.highCutBypassed = loadCachedParameter(filterParameterCache.highCutBypassed) > 0.5f;
    return settings;
}

ChainSettings FireAudioProcessor::getCachedChainSettingsAtSample(
    const juce::AudioBuffer<float>& lfoOutputs,
    int sampleIndex) const noexcept
{
    const auto getValue = [this, &lfoOutputs, sampleIndex](const CachedParameter& parameter)
    {
        return getModulatedValueAtSample(parameter, lfoOutputs, sampleIndex);
    };

    ChainSettings settings;
    settings.lowCutFreq = getValue(filterParameterCache.lowCutFrequency);
    settings.lowCutGainInDecibels = getValue(filterParameterCache.lowCutGain);
    settings.lowCutQuality = getValue(filterParameterCache.lowCutQuality);
    settings.lowCutSlope = getSlopeParameterValue(filterParameterCache.lowCutSlope.raw);
    settings.lowCutBypassed = loadCachedParameter(filterParameterCache.lowCutBypassed) > 0.5f;
    settings.peakFreq = getValue(filterParameterCache.peakFrequency);
    settings.peakGainInDecibels = getValue(filterParameterCache.peakGain);
    settings.peakQuality = getValue(filterParameterCache.peakQuality);
    settings.peakBypassed = loadCachedParameter(filterParameterCache.peakBypassed) > 0.5f;
    settings.highCutFreq = getValue(filterParameterCache.highCutFrequency);
    settings.highCutGainInDecibels = getValue(filterParameterCache.highCutGain);
    settings.highCutQuality = getValue(filterParameterCache.highCutQuality);
    settings.highCutSlope = getSlopeParameterValue(filterParameterCache.highCutSlope.raw);
    settings.highCutBypassed = loadCachedParameter(filterParameterCache.highCutBypassed) > 0.5f;
    return settings;
}

bool FireAudioProcessor::hasActiveFilterModulation() const noexcept
{
    if (lfoOutputBuffer.getNumSamples() <= 0)
        return false;

    const std::array<const CachedParameter*, 9> parameters {
        &filterParameterCache.lowCutFrequency,
        &filterParameterCache.lowCutGain,
        &filterParameterCache.lowCutQuality,
        &filterParameterCache.peakFrequency,
        &filterParameterCache.peakGain,
        &filterParameterCache.peakQuality,
        &filterParameterCache.highCutFrequency,
        &filterParameterCache.highCutGain,
        &filterParameterCache.highCutQuality
    };

    for (const auto* parameter : parameters)
    {
        LfoManager::AudioThreadRoutingInfo routingInfo;
        if (parameter->ranged != nullptr
            && lfoManager->getAudioThreadRoutingInfo(parameter->ranged, routingInfo)
            && juce::isPositiveAndBelow(routingInfo.sourceLfoIndex,
                                        lfoOutputBuffer.getNumChannels())
            && std::isfinite(routingInfo.depth)
            && std::abs(routingInfo.depth) > std::numeric_limits<float>::epsilon())
            return true;
    }

    return false;
}

void FireAudioProcessor::initialiseParameterCache()
{
    const auto indexed = [this](const juce::String& base, int index)
    {
        return cacheParameter(ParameterIDAndName::getIDString(base, index));
    };

    for (int i = 0; i < static_cast<int>(bandParameterCache.size()); ++i)
    {
        auto& parameters = bandParameterCache[static_cast<size_t>(i)];
        parameters.enabled = indexed(BAND_ENABLE_ID, i);
        parameters.solo = indexed(BAND_SOLO_ID, i);
        parameters.mode = indexed(MODE_ID, i);
        parameters.linked = indexed(LINKED_ID, i);
        parameters.safe = indexed(SAFE_ID, i);
        parameters.extreme = indexed(EXTREME_ID, i);
        parameters.driveEnabled = indexed(DRIVE_BYPASS_ID, i);
        parameters.shapeEnabled = indexed(SHAPE_BYPASS_ID, i);
        parameters.compressorEnabled = indexed(COMP_BYPASS_ID, i);
        parameters.widthEnabled = indexed(WIDTH_BYPASS_ID, i);
        parameters.dcFilterEnabled = indexed(DC_FILTER_ID, i);
        parameters.drive = indexed(DRIVE_ID, i);
        parameters.bias = indexed(BIAS_ID, i);
        parameters.rec = indexed(REC_ID, i);
        parameters.output = indexed(OUTPUT_ID, i);
        parameters.compressorRatio = indexed(COMP_RATIO_ID, i);
        parameters.compressorThreshold = indexed(COMP_THRESH_ID, i);
        parameters.compressorAttack = indexed(COMP_ATTACK_ID, i);
        parameters.compressorRelease = indexed(COMP_RELEASE_ID, i);
        parameters.compressorMix = indexed(COMP_MIX_ID, i);
        parameters.width = indexed(WIDTH_ID, i);
        parameters.pan = indexed(PAN_ID, i);
        parameters.widthMix = indexed(WIDTH_MIX_ID, i);
        parameters.mix = indexed(MIX_ID, i);
        parameters.shapeMix = indexed(SHAPE_MIX_ID, i);

        lfoSmoothParameters[static_cast<size_t>(i)] = indexed(LFO_SMOOTH_ID, i).raw;
    }

    for (int i = 0; i < static_cast<int>(crossoverFrequencyParameters.size()); ++i)
        crossoverFrequencyParameters[static_cast<size_t>(i)] = indexed(FREQ_ID, i);

    numBandsParameter = cacheParameter(NUM_BANDS_ID);
    hqParameter = cacheParameter(HQ_ID);
    globalOutputParameter = cacheParameter(OUTPUT_ID);
    globalMixParameter = cacheParameter(MIX_ID);
    filterEnabledParameter = cacheParameter(FILTER_BYPASS_ID);
    downsampleEnabledParameter = cacheParameter(DOWNSAMPLE_BYPASS_ID);
    downsampleRateParameter = cacheParameter(DOWNSAMPLE_ID);
    bitDepthParameter = cacheParameter(BIT_DEPTH_ID);
    jitterParameter = cacheParameter(JITTER_ID);
    downsampleMixParameter = cacheParameter(DOWNSAMPLE_MIX_ID);

    filterParameterCache.lowCutFrequency = cacheParameter(LOWCUT_FREQ_ID);
    filterParameterCache.lowCutGain = cacheParameter(LOWCUT_GAIN_ID);
    filterParameterCache.lowCutQuality = cacheParameter(LOWCUT_Q_ID);
    filterParameterCache.lowCutSlope = cacheParameter(LOWCUT_SLOPE_ID);
    filterParameterCache.lowCutBypassed = cacheParameter(LOWCUT_BYPASSED_ID);
    filterParameterCache.peakFrequency = cacheParameter(PEAK_FREQ_ID);
    filterParameterCache.peakGain = cacheParameter(PEAK_GAIN_ID);
    filterParameterCache.peakQuality = cacheParameter(PEAK_Q_ID);
    filterParameterCache.peakBypassed = cacheParameter(PEAK_BYPASSED_ID);
    filterParameterCache.highCutFrequency = cacheParameter(HIGHCUT_FREQ_ID);
    filterParameterCache.highCutGain = cacheParameter(HIGHCUT_GAIN_ID);
    filterParameterCache.highCutQuality = cacheParameter(HIGHCUT_Q_ID);
    filterParameterCache.highCutSlope = cacheParameter(HIGHCUT_SLOPE_ID);
    filterParameterCache.highCutBypassed = cacheParameter(HIGHCUT_BYPASSED_ID);
}

//==============================================================================
FireAudioProcessor::FireAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      treeState(*this, nullptr, "PARAMETERS", createParameters()),
      lfoManager(std::make_unique<LfoManager>(treeState)),
      stateAB {
          *this
      }
#if JUCE_MAC
      ,
      statePresets {
          *this,
          "Audio/Presets/Wings/Fire"
      }
#else
      ,
      statePresets {
          *this,
          "Wings/Fire"
      } // AppData/Roaming/...
#endif
#endif
{
    initialiseParameterCache();

    // Initialize the band processors in a loop.
    for (int i = 0; i < 4; ++i)
        bands.push_back(std::make_unique<BandProcessor>());

    for (int i = 0; i < 4; ++i)
        realtimeModulatedThresholds[i].store(-48.0f);

    filterFifoBuffer.resize(filterFifo.getTotalSize());
    meterFifoBuffer.resize(meterFifo.getTotalSize());
    graphFifoBuffer.resize(graphFifo.getTotalSize());

    // Set up the properties file options.
    juce::PropertiesFile::Options options;
    options.applicationName = JucePlugin_Name;
    options.filenameSuffix = ".settings";
    options.folderName = "Wings";
    options.osxLibrarySubFolder = "Application Support";
    options.commonToAllUsers = false;

    // Create the properties file object.
    appProperties = std::make_unique<juce::PropertiesFile>(options);

    // Initialize all 9 smoothers with default parameter values
    auto chainSettings = getCachedChainSettings(nullptr);

    const auto frequencyRange = getSafeFilterFrequencyRange(getSampleRate());

    chainSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.highCutFreq);

    lowcutFreqSmoother.setCurrentAndTargetValue(chainSettings.lowCutFreq);
    lowcutGainSmoother.setCurrentAndTargetValue(chainSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.setCurrentAndTargetValue(chainSettings.lowCutQuality);

    peakFreqSmoother.setCurrentAndTargetValue(chainSettings.peakFreq);
    peakGainSmoother.setCurrentAndTargetValue(chainSettings.peakGainInDecibels);
    peakQualitySmoother.setCurrentAndTargetValue(chainSettings.peakQuality);

    highcutFreqSmoother.setCurrentAndTargetValue(chainSettings.highCutFreq);
    highcutGainSmoother.setCurrentAndTargetValue(chainSettings.highCutGainInDecibels);
    highcutQualitySmoother.setCurrentAndTargetValue(chainSettings.highCutQuality);
    startTimerHz(30);
}

FireAudioProcessor::~FireAudioProcessor()
{
    stopTimer();
}

//==============================================================================
const juce::String FireAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FireAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool FireAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool FireAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double FireAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FireAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int FireAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FireAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String FireAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void FireAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void FireAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    const int maximumBlockSize = juce::jmax(minimumProcessingBlockCapacity,
                                             juce::jmax(1, samplesPerBlock));
    preparedProcessingBlockCapacity = maximumBlockSize;
    const int outputChannels = juce::jmax(1, getMainBusNumOutputChannels());
    juce::dsp::ProcessSpec spec { safeSampleRate,
                                  static_cast<juce::uint32>(maximumBlockSize),
                                  static_cast<juce::uint32>(outputChannels) };

    globalFilterCacheValid = false;

    for (int i = 0; i < 4; ++i)
    {
        if (auto* band = bands[i].get())
        {
            const auto& parameters = bandParameterCache[static_cast<size_t>(i)];
            band->prepare(spec);
            band->recSmoother.setCurrentAndTargetValue(loadCachedParameter(parameters.rec));
            band->biasSmoother.setCurrentAndTargetValue(loadCachedParameter(parameters.bias));

            const float initialOutput = loadCachedParameter(parameters.linked) > 0.5f
                                            ? -0.1f * loadCachedParameter(parameters.drive)
                                            : loadCachedParameter(parameters.output);
            band->gain.setRampDurationSeconds(0.0);
            band->gain.setGainDecibels(initialOutput);
            band->gain.setRampDurationSeconds(0.05);
        }
    }

    float hqLatency = 0.0f;
    if (! bands.empty() && bands.front() != nullptr && bands.front()->oversampling != nullptr)
        hqLatency = bands.front()->oversampling->getLatencyInSamples();
    if (! std::isfinite(hqLatency) || hqLatency < 0.0f)
        hqLatency = 0.0f;
    preparedHqLatency.store(hqLatency, std::memory_order_release);
    // Host PDC cannot safely follow an automatable quality switch. Report the
    // prepared HQ latency for both modes; base processing is delayed by the
    // same integer number of samples at the end of the callback.
    totalLatency.store(hqLatency, std::memory_order_release);
    nonHqOutputDelay.prepare(spec);
    nonHqOutputDelay.setDelay(static_cast<float>(juce::roundToInt(hqLatency)));
    hqTransitionRampSamples = juce::jmax(
        1, juce::roundToInt(static_cast<float>(safeSampleRate) * 0.005f));
    hqTransitionGain = 1.0f;
    hqTransitionGainStep = 0.0f;
    hqTransitionRampRemaining = 0;
    hqTransitionWarmupSamples = juce::jmax(
        juce::roundToInt(static_cast<float>(safeSampleRate) * 0.001f),
        juce::roundToInt(std::ceil(hqLatency))
            + juce::roundToInt(hqLatency)
            + 2);
    topologyTransitionRampSamples = juce::jmax(
        1, juce::roundToInt(static_cast<float>(safeSampleRate) * 0.005f));
    topologyTransitionWarmupSamples = juce::jmax(
        48,
        juce::roundToInt(static_cast<float>(safeSampleRate) * 0.001f),
        juce::roundToInt(std::ceil(hqLatency))
            + juce::roundToInt(hqLatency)
            + 2);

    lfoManager->prepare(spec);

    for (int i = 0; i < historyLength; ++i)
    {
        historyArrayL[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
        historyArrayR[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
    }
    historyWritePosition.store(0, std::memory_order_relaxed);
    historySamplesAvailable.store(historyLength, std::memory_order_release);
    historyGeneration.fetch_add(1, std::memory_order_release);

    delayMatchedDryBuffer.setSize(outputChannels, maximumBlockSize);
    delayMatchedDryBuffer.clear();

    mWetBuffer.setSize(outputChannels, maximumBlockSize);
    mWetBuffer.clear();
    hostBypassWetBuffer.setSize(outputChannels, maximumBlockSize);
    hostBypassWetBuffer.clear();
    lfoOutputBuffer.setSize(4, maximumBlockSize);
    lfoOutputBuffer.clear();
    lofiDryBuffer.setSize(outputChannels, maximumBlockSize);
    lofiDryBuffer.clear();

    const float rampTimeSeconds = 0.0005f;
    auto initialFilterSettings = getCachedChainSettings(nullptr);
    const auto initialFilterFrequencyRange = getSafeFilterFrequencyRange(safeSampleRate);
    initialFilterSettings.lowCutFreq = juce::jlimit(initialFilterFrequencyRange.minimum,
                                                    initialFilterFrequencyRange.maximum,
                                                    initialFilterSettings.lowCutFreq);
    initialFilterSettings.peakFreq = juce::jlimit(initialFilterFrequencyRange.minimum,
                                                  initialFilterFrequencyRange.maximum,
                                                  initialFilterSettings.peakFreq);
    initialFilterSettings.highCutFreq = juce::jlimit(initialFilterFrequencyRange.minimum,
                                                     initialFilterFrequencyRange.maximum,
                                                     initialFilterSettings.highCutFreq);

    lowcutFreqSmoother.reset(safeSampleRate, rampTimeSeconds);
    lowcutFreqSmoother.setCurrentAndTargetValue(initialFilterSettings.lowCutFreq);
    lowcutGainSmoother.reset(safeSampleRate, rampTimeSeconds);
    lowcutGainSmoother.setCurrentAndTargetValue(initialFilterSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.reset(safeSampleRate, rampTimeSeconds);
    lowcutQualitySmoother.setCurrentAndTargetValue(initialFilterSettings.lowCutQuality);

    peakFreqSmoother.reset(safeSampleRate, rampTimeSeconds);
    peakFreqSmoother.setCurrentAndTargetValue(initialFilterSettings.peakFreq);
    peakGainSmoother.reset(safeSampleRate, rampTimeSeconds);
    peakGainSmoother.setCurrentAndTargetValue(initialFilterSettings.peakGainInDecibels);
    peakQualitySmoother.reset(safeSampleRate, rampTimeSeconds);
    peakQualitySmoother.setCurrentAndTargetValue(initialFilterSettings.peakQuality);

    highcutFreqSmoother.reset(safeSampleRate, rampTimeSeconds);
    highcutFreqSmoother.setCurrentAndTargetValue(initialFilterSettings.highCutFreq);
    highcutGainSmoother.reset(safeSampleRate, rampTimeSeconds);
    highcutGainSmoother.setCurrentAndTargetValue(initialFilterSettings.highCutGainInDecibels);
    highcutQualitySmoother.reset(safeSampleRate, rampTimeSeconds);
    highcutQualitySmoother.setCurrentAndTargetValue(initialFilterSettings.highCutQuality);

    smoothedFreq1.reset(safeSampleRate, rampTimeSeconds * 2);
    smoothedFreq2.reset(safeSampleRate, rampTimeSeconds * 2);
    smoothedFreq3.reset(safeSampleRate, rampTimeSeconds * 2);
    for (auto& smoother : bandSoloGainSmoothers)
        smoother.reset(safeSampleRate, 0.01);
    juce::dsp::ProcessSpec soloEnvelopeSpec {
        safeSampleRate,
        static_cast<juce::uint32>(maximumBlockSize),
        1
    };
    const int maximumSoloEnvelopeDelay = juce::jmax(1, juce::roundToInt(
        std::ceil(preparedHqLatency.load(std::memory_order_acquire))) + 2);
    bandSoloGainDelayLinesPrepared = false;
    for (auto& delayLine : bandSoloGainDelayLines)
    {
        delayLine.setMaximumDelayInSamples(maximumSoloEnvelopeDelay);
        delayLine.prepare(soloEnvelopeSpec);
        delayLine.setDelay(0.0f);
    }
    bandSoloGainDelayLinesPrepared = true;
    synchroniseMultibandTopologyResetState();

    // filter init
    constexpr double cutSlopeRampSeconds = 0.01;
    const auto initialiseCutSlopeTransition = [&] (CutSlopeTransitionState& transition,
                                                    Slope initialSlope,
                                                    float initialFrequency,
                                                    bool highPass)
    {
        transition.standbyMix.reset(safeSampleRate, cutSlopeRampSeconds);
        transition.standbyMix.setCurrentAndTargetValue(0.0f);
        transition.slotSlopes = { initialSlope, initialSlope };
        transition.requestedSlope = initialSlope;
        transition.lastFrequency = initialFrequency;
        transition.lastSampleRate = safeSampleRate;
        transition.highPass = highPass;
        transition.initialised = false;
    };
    initialiseCutSlopeTransition(lowCutSlopeTransition,
                                 initialFilterSettings.lowCutSlope,
                                 initialFilterSettings.lowCutFreq,
                                 true);
    initialiseCutSlopeTransition(highCutSlopeTransition,
                                 initialFilterSettings.highCutSlope,
                                 initialFilterSettings.highCutFreq,
                                 false);
    updateFilter(safeSampleRate);
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    lowCutSlopeTransition.leftStandby.prepare(spec);
    lowCutSlopeTransition.rightStandby.prepare(spec);
    highCutSlopeTransition.leftStandby.prepare(spec);
    highCutSlopeTransition.rightStandby.prepare(spec);
    globalFilterStageDryBuffer.setSize(outputChannels, maximumBlockSize);
    globalFilterStageDryBuffer.clear();
    globalCutSlopeShadowBuffer.setSize(outputChannels, maximumBlockSize);
    globalCutSlopeShadowBuffer.clear();

    constexpr double stageBypassRampSeconds = 0.01;
    const std::array<float, numGlobalFilterStages> initialStageMix {
        initialFilterSettings.lowCutBypassed ? 0.0f : 1.0f,
        initialFilterSettings.peakBypassed ? 0.0f : 1.0f,
        initialFilterSettings.highCutBypassed ? 0.0f : 1.0f,
        initialFilterSettings.lowCutBypassed ? 0.0f : 1.0f,
        initialFilterSettings.highCutBypassed ? 0.0f : 1.0f
    };
    for (size_t stage = 0; stage < globalFilterStageMix.size(); ++stage)
    {
        globalFilterStageMix[stage].reset(safeSampleRate, stageBypassRampSeconds);
        globalFilterStageMix[stage].setCurrentAndTargetValue(initialStageMix[stage]);
    }

    // multiband filters
    mBuffer1.setSize(outputChannels, maximumBlockSize);
    mBuffer2.setSize(outputChannels, maximumBlockSize);
    mBuffer3.setSize(outputChannels, maximumBlockSize);
    mBuffer4.setSize(outputChannels, maximumBlockSize);
    mSplitTemp1.setSize(outputChannels, maximumBlockSize);
    mSplitTemp2.setSize(outputChannels, maximumBlockSize);
    mSplitTemp3.setSize(outputChannels, maximumBlockSize);
    bandSoloGainEnvelope.setSize(4, maximumBlockSize);
    delayedBandSoloGainEnvelope.setSize(4, maximumBlockSize);
    mBuffer1.clear();
    mBuffer2.clear();
    mBuffer3.clear();
    mBuffer4.clear();

    lowpass1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowpass2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowpass3.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    highpass1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    highpass2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    highpass3.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    lowpass1.prepare(spec);
    lowpass2.prepare(spec);
    lowpass3.prepare(spec);
    highpass1.prepare(spec);
    highpass2.prepare(spec);
    highpass3.prepare(spec);

    compensatorLP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    compensatorHP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    compensatorLP.prepare(spec);
    compensatorHP.prepare(spec);
    secondCompensatorLP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    secondCompensatorHP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    secondCompensatorLP.prepare(spec);
    secondCompensatorHP.prepare(spec);

    // gain
    gainProcessorGlobal.setRampDurationSeconds(0.05);
    gainProcessorGlobal.prepare(spec);
    gainProcessorGlobal.setRampDurationSeconds(0.0);
    gainProcessorGlobal.setGainDecibels(loadCachedParameter(globalOutputParameter));
    gainProcessorGlobal.setRampDurationSeconds(0.05);
    globalOutputGainTransition.prepare(safeSampleRate);

    // dry wet
    juce::dsp::ProcessSpec globalMixerSpec = spec;
    globalMixerSpec.maximumBlockSize = spec.maximumBlockSize * 20; // set 20 to pass PluginVal
    dryWetMixerGlobal.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    dryWetMixerGlobal.setWetMixProportion(juce::jlimit(0.0f, 1.0f,
                                                       loadCachedParameter(globalMixParameter)));
    dryWetMixerGlobal.prepare(globalMixerSpec);

    globalFilterMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    globalFilterMixer.setWetMixProportion(loadCachedParameter(filterEnabledParameter) > 0.5f
                                              ? 1.0f
                                              : 0.0f);
    globalFilterMixer.prepare(globalMixerSpec);

    bypassDelayMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    // Set the target before prepare/reset so the first bypassed sample is not
    // blended with the undelayed wet input by DryWetMixer's 50 ms ramp.
    bypassDelayMixer.setWetMixProportion(0.0f);
    bypassDelayMixer.prepare(globalMixerSpec);

    lofiMixer.prepare(globalMixerSpec);
    publishLatencyToHost();
    reset();
}

void FireAudioProcessor::reset()
{
    needsReset = true;
}

void FireAudioProcessor::beginMultibandTopologyEdit() noexcept
{
    // Keep one recursive writer-lock level alive until the matching publish.
    // This serialises editor, preset and host-state writers without ever
    // involving the audio thread. Nested edits on the same thread coalesce
    // into one odd/even publication.
    multibandTopologyWriterLock.enter();
    if (multibandTopologyEditDepth++ == 0)
    {
        const auto previous = multibandTopologyResetGeneration.fetch_add(
            1u, std::memory_order_acq_rel);
        jassert((previous & 1u) == 0u);
    }
}

void FireAudioProcessor::requestMultibandTopologyReset() noexcept
{
    // CriticalSection is recursive. The extra level acquired here lets the
    // owner finish its transaction while competing writers wait; the second
    // exit balances the level deliberately retained by begin().
    multibandTopologyWriterLock.enter();

    if (multibandTopologyEditDepth > 0)
    {
        --multibandTopologyEditDepth;
        if (multibandTopologyEditDepth == 0)
        {
            const auto previous = multibandTopologyResetGeneration.fetch_add(
                1u, std::memory_order_release);
            jassert((previous & 1u) != 0u);
        }

        multibandTopologyWriterLock.exit();
        multibandTopologyWriterLock.exit();
        return;
    }

    // A standalone request represents a complete same-count publication.
    const auto previous = multibandTopologyResetGeneration.fetch_add(
        2u, std::memory_order_release);
    jassert((previous & 1u) == 0u);
    multibandTopologyWriterLock.exit();
}

void FireAudioProcessor::resetMultibandProcessingState(
    const HqCallbackContext* callbackContext) noexcept
{
    lowpass1.reset();
    lowpass2.reset();
    lowpass3.reset();
    highpass1.reset();
    highpass2.reset();
    highpass3.reset();
    compensatorLP.reset();
    compensatorHP.reset();
    secondCompensatorLP.reset();
    secondCompensatorHP.reset();

    for (size_t bandIndex = 0; bandIndex < bands.size(); ++bandIndex)
    {
        auto& band = bands[bandIndex];
        if (band == nullptr)
            continue;

        band->reset();

        // juce::dsp::Gain::reset() snaps its smoother to the physical slot's
        // previous target.  After an add/remove that slot can represent a
        // different logical band, so initialise it from the completed
        // parameter migration instead of ramping from stale output gain for
        // the next 50 ms.
        const float initialOutput = callbackContext != nullptr
                                        ? callbackContext->bandParameters[bandIndex]
                                              .outputVal.baseValue
                                        : loadCachedParameter(
                                              bandParameterCache[bandIndex].output);
        band->gain.setRampDurationSeconds(0.0);
        band->gain.setGainDecibels(initialOutput);
        band->gain.setRampDurationSeconds(0.05);
    }
}

std::array<float, 3> FireAudioProcessor::getEffectiveCrossoverFrequencies(
    int crossoversToValidate) const noexcept
{
    constexpr std::array<float, 3> defaultCrossoverFrequencies { 200.0f, 1000.0f, 5000.0f };
    constexpr float minimumParameterFrequency = 40.0f;
    constexpr float maximumParameterFrequency = 10024.0f;

    std::array<float, 3> frequencies {
        loadCachedParameter(crossoverFrequencyParameters[0]),
        loadCachedParameter(crossoverFrequencyParameters[1]),
        loadCachedParameter(crossoverFrequencyParameters[2])
    };

    bool activeFrequenciesAreValid = true;
    crossoversToValidate = juce::jlimit(0, 3, crossoversToValidate);
    for (int index = 0; index < crossoversToValidate; ++index)
    {
        const auto frequency = frequencies[static_cast<size_t>(index)];
        activeFrequenciesAreValid = activeFrequenciesAreValid
                                     && std::isfinite(frequency)
                                     && frequency >= minimumParameterFrequency
                                     && frequency <= maximumParameterFrequency
                                     && (index == 0
                                         || frequency > frequencies[static_cast<size_t>(index - 1)]);
    }

    // Old sessions use 21 Hz as the inactive-slot sentinel. If a host exposes
    // NUM_BANDS on its generic parameter surface and enables those slots by
    // itself, use safe local crossover values without rewriting the session's
    // APVTS state.
    if (! activeFrequenciesAreValid)
    {
        for (int index = 0; index < crossoversToValidate; ++index)
            frequencies[static_cast<size_t>(index)] = defaultCrossoverFrequencies[static_cast<size_t>(index)];
    }

    return frequencies;
}

void FireAudioProcessor::snapCrossoverSmoothers(
    const std::array<float, 3>& frequencies) noexcept
{
    smoothedFreq1.setCurrentAndTargetValue(frequencies[0]);
    smoothedFreq2.setCurrentAndTargetValue(frequencies[1]);
    smoothedFreq3.setCurrentAndTargetValue(frequencies[2]);
}

void FireAudioProcessor::snapBandSoloGains(
    int snapshotNumBands,
    const HqCallbackContext& callbackContext) noexcept
{
    for (size_t band = 0; band < bandSoloGainSmoothers.size(); ++band)
    {
        const bool isActiveBand = static_cast<int>(band) < snapshotNumBands;
        const float target = isActiveBand
                                 && (! callbackContext.anySoloActive
                                     || callbackContext.soloState[band])
                             ? 1.0f
                             : 0.0f;
        bandSoloGainSmoothers[band].setCurrentAndTargetValue(target);

        // Topology changes can reuse a physical DSP slot for another logical
        // band. Prime the latency-matched control delay with the new snapped
        // gain so the first HQ samples cannot inherit the previous slot's Solo
        // history (or fall through an all-zero reset buffer).
        if (bandSoloGainDelayLinesPrepared)
        {
            auto& delayLine = bandSoloGainDelayLines[band];
            delayLine.reset();
            const int historySamples = delayLine.getMaximumDelayInSamples() + 2;
            for (int sample = 0; sample < historySamples; ++sample)
                delayLine.pushSample(0, target);
        }
    }
}

void FireAudioProcessor::updateBandSoloGainEnvelope(
    int numSamples,
    bool useHQ,
    const std::array<bool, 4>& soloState,
    bool anySoloActive) noexcept
{
    if (numSamples <= 0)
        return;

    bandSoloGainEnvelope.setSize(4, numSamples, false, false, true);
    delayedBandSoloGainEnvelope.setSize(4, numSamples, false, false, true);
    const float wetPathLatency = useHQ
                                     ? preparedHqLatency.load(std::memory_order_acquire)
                                     : 0.0f;
    for (size_t band = 0; band < bandSoloGainSmoothers.size(); ++band)
    {
        const bool isActiveBand = static_cast<int>(band) < numBands;
        const float target = isActiveBand
                                 && (! anySoloActive || soloState[band])
                             ? 1.0f
                             : 0.0f;
        auto& smoother = bandSoloGainSmoothers[band];
        smoother.setTargetValue(target);

        auto* gains = bandSoloGainEnvelope.getWritePointer(static_cast<int>(band));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Emit the old audible value first, then advance. This keeps the
            // switching sample continuous and reaches the target after exactly
            // the configured 10 ms ramp.
            gains[sample] = smoother.getCurrentValue();
            smoother.getNextValue();
        }

        // In HQ mode the wet bands already carry the oversampler latency,
        // while the dry bands are delayed later by Global Mix. Delay the wet
        // contribution gain by the same amount so both arrive at the output
        // with one identical Solo envelope.
        auto& delayLine = bandSoloGainDelayLines[band];
        delayLine.setDelay(wetPathLatency);
        const juce::AudioBuffer<float> sourceView(
            bandSoloGainEnvelope.getArrayOfWritePointers()
                + static_cast<int>(band),
            1,
            numSamples);
        juce::AudioBuffer<float> destinationView(
            delayedBandSoloGainEnvelope.getArrayOfWritePointers()
                + static_cast<int>(band),
            1,
            numSamples);
        const auto inputBlock = juce::dsp::AudioBlock<const float>(sourceView);
        auto outputBlock = juce::dsp::AudioBlock<float>(destinationView);
        delayLine.process(juce::dsp::ProcessContextNonReplacing<float>(
            inputBlock, outputBlock));
    }
}

bool FireAudioProcessor::tryCaptureMultibandTopologySnapshot(
    const juce::AudioBuffer<float>& lfoOutputs,
    std::uint32_t sequenceAtCallbackStart,
    bool routingSnapshotWasRefreshed,
    MultibandTopologySnapshot& snapshot)
{
    const auto sequenceBefore = multibandTopologyResetGeneration.load(
        std::memory_order_acquire);
    if (sequenceBefore != sequenceAtCallbackStart
        || (sequenceBefore & 1u) != 0u)
        return false;

    MultibandTopologySnapshot candidate;
    candidate.publicationSequence = sequenceBefore;
    candidate.numBands = juce::jlimit(
        1,
        4,
        juce::roundToInt(loadCachedParameter(numBandsParameter, 1.0f)));
    candidate.crossoverFrequencies = getEffectiveCrossoverFrequencies(
        candidate.numBands - 1);
    prepareHqCallbackContext(lfoOutputs,
                             candidate.numBands,
                             candidate.callbackContext);

    const auto sequenceAfter = multibandTopologyResetGeneration.load(
        std::memory_order_acquire);
    if (sequenceAfter != sequenceBefore || (sequenceAfter & 1u) != 0u)
        return false;

    // Topology migrations can move LFO targets between physical band slots.
    // LfoManager publishes those routes with a non-blocking try-lock at the
    // callback boundary. If that refresh missed, keep the complete old audio
    // snapshot instead of combining new APVTS slots with stale runtime routes.
    const bool topologyIdentityChanged =
        ! activeMultibandTopologySnapshotInitialised
        || candidate.numBands != numBands
        || candidate.publicationSequence
               != appliedMultibandTopologyResetGeneration;
    if (topologyIdentityChanged && ! routingSnapshotWasRefreshed)
        return false;

    snapshot = std::move(candidate);
    return true;
}

void FireAudioProcessor::publishMultibandTelemetry(
    const HqCallbackContext& callbackContext,
    int snapshotNumBands,
    const juce::AudioBuffer<float>& lfoOutputs) noexcept
{
    snapshotNumBands = juce::jlimit(0, 4, snapshotNumBands);
    for (int band = 0; band < snapshotNumBands; ++band)
    {
        auto provider = callbackContext.bandParameters[static_cast<size_t>(band)]
                            .compThresholdValProvider;
        const int sourceIndex = callbackContext.bandParameters[static_cast<size_t>(band)]
                                    .compThresholdLfoSourceIndex;
        if (juce::isPositiveAndBelow(sourceIndex, lfoOutputs.getNumChannels())
            && lfoOutputs.getNumSamples() > 0)
            provider.lfoSignal = lfoOutputs.getReadPointer(sourceIndex);

        realtimeModulatedThresholds[band].store(
            provider.get(0), std::memory_order_relaxed);
    }
}

void FireAudioProcessor::synchroniseMultibandTopologyResetState() noexcept
{
    juce::AudioBuffer<float> noLfoOutputs;
    MultibandTopologySnapshot requestedSnapshot;
    const auto sequenceAtReset = multibandTopologyResetGeneration.load(
        std::memory_order_acquire);
    // A lifecycle reset runs before the callback's non-blocking LFO routing
    // refresh. Do not mark a newly published topology as applied here: when an
    // older audible snapshot exists, the first successful audio-thread refresh
    // must validate its migrated routing at the same time.
    if (tryCaptureMultibandTopologySnapshot(noLfoOutputs,
                                            sequenceAtReset,
                                            false,
                                            requestedSnapshot))
    {
        activeMultibandTopologySnapshot = std::move(requestedSnapshot);
        activeMultibandTopologySnapshotInitialised = true;
    }
    else if (! activeMultibandTopologySnapshotInitialised)
    {
        // prepare/reset normally runs while parameter publication is quiescent.
        // Keep a safe fallback for hosts that violate that lifecycle contract;
        // the next stable callback will replace it atomically.
        activeMultibandTopologySnapshot.numBands = juce::jlimit(
            1,
            4,
            juce::roundToInt(loadCachedParameter(numBandsParameter, 1.0f)));
        activeMultibandTopologySnapshot.crossoverFrequencies =
            getEffectiveCrossoverFrequencies(
                activeMultibandTopologySnapshot.numBands - 1);
        prepareHqCallbackContext(
            noLfoOutputs,
            activeMultibandTopologySnapshot.numBands,
            activeMultibandTopologySnapshot.callbackContext);
        activeMultibandTopologySnapshot.publicationSequence =
            multibandTopologyResetGeneration.load(std::memory_order_relaxed)
            & ~std::uint32_t { 1 };
        activeMultibandTopologySnapshotInitialised = true;
    }

    numBands = activeMultibandTopologySnapshot.numBands;
    activeCrossovers = numBands - 1;
    snapCrossoverSmoothers(
        activeMultibandTopologySnapshot.crossoverFrequencies);
    snapBandSoloGains(
        numBands,
        activeMultibandTopologySnapshot.callbackContext);
    appliedMultibandTopologyResetGeneration =
        activeMultibandTopologySnapshot.publicationSequence;
}

void FireAudioProcessor::performReset()
{
    synchroniseMultibandTopologyResetState();
    resetMultibandProcessingState(
        &activeMultibandTopologySnapshot.callbackContext);
    leftChain.reset();
    rightChain.reset();
    lowCutSlopeTransition.leftStandby.reset();
    lowCutSlopeTransition.rightStandby.reset();
    highCutSlopeTransition.leftStandby.reset();
    highCutSlopeTransition.rightStandby.reset();
    const auto snapCutSlopeTransition = [&] (CutSlopeTransitionState& transition,
                                             CutFilter& leftPrimary,
                                             CutFilter& rightPrimary,
                                             Slope requestedSlope)
    {
        transition.requestedSlope = requestedSlope;
        transition.slotSlopes = { requestedSlope, requestedSlope };
        transition.standbyMix.setCurrentAndTargetValue(0.0f);
        if (! transition.initialised)
            return;

        updateButterworthCutFilterPair(leftPrimary,
                                       rightPrimary,
                                       transition.lastFrequency,
                                       transition.lastSampleRate,
                                       requestedSlope,
                                       transition.highPass);
        updateButterworthCutFilterPair(transition.leftStandby,
                                       transition.rightStandby,
                                       transition.lastFrequency,
                                       transition.lastSampleRate,
                                       requestedSlope,
                                       transition.highPass);
    };
    snapCutSlopeTransition(lowCutSlopeTransition,
                           leftChain.get<ChainPositions::LowCut>(),
                           rightChain.get<ChainPositions::LowCut>(),
                           getSlopeParameterValue(filterParameterCache.lowCutSlope.raw));
    snapCutSlopeTransition(highCutSlopeTransition,
                           leftChain.get<ChainPositions::HighCut>(),
                           rightChain.get<ChainPositions::HighCut>(),
                           getSlopeParameterValue(filterParameterCache.highCutSlope.raw));
    globalFilterCacheValid = false;
    const std::array<float, numGlobalFilterStages> initialStageMix {
        loadCachedParameter(filterParameterCache.lowCutBypassed) > 0.5f ? 0.0f : 1.0f,
        loadCachedParameter(filterParameterCache.peakBypassed) > 0.5f ? 0.0f : 1.0f,
        loadCachedParameter(filterParameterCache.highCutBypassed) > 0.5f ? 0.0f : 1.0f,
        loadCachedParameter(filterParameterCache.lowCutBypassed) > 0.5f ? 0.0f : 1.0f,
        loadCachedParameter(filterParameterCache.highCutBypassed) > 0.5f ? 0.0f : 1.0f
    };
    for (size_t stage = 0; stage < globalFilterStageMix.size(); ++stage)
        globalFilterStageMix[stage].setCurrentAndTargetValue(initialStageMix[stage]);
    globalFilterMixer.reset();
    globalFilterMixerPrimed = false;
    dryWetMixerGlobal.reset();
    globalMixerPrimed = false;
    bypassDelayMixer.reset();
    nonHqOutputDelay.reset();
    lofiMixer.reset();
    resetDownsamplingState();
    gainProcessorGlobal.reset();
    globalOutputGainTransition.reset();
    lfoManager->reset();
    snapHqTransitionToParameter();
    snapTopologyTransitionToActive();
    hostBypassSessionActive = false;
    hostBypassSessionHqMode = activeHqMode;
}

void FireAudioProcessor::releaseResources()
{
    needsReset.store(false, std::memory_order_release);
    performReset();
}

void FireAudioProcessor::publishLatencyToHost()
{
    const int latencyInSamples = juce::roundToInt(totalLatency.load(std::memory_order_acquire));
    if (latencyInSamples != getLatencySamples())
        setLatencySamples(latencyInSamples);
}

void FireAudioProcessor::timerCallback()
{
    // setLatencySamples synchronously notifies the host, so keep it on the
    // message thread. The prepared maximum is invariant across HQ automation.
    publishLatencyToHost();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FireAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void FireAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    isBypassed.store(true, std::memory_order_relaxed);

    if (needsReset.exchange(false, std::memory_order_acq_rel))
        performReset();

    calculateAndStoreLevels(buffer,
                            mInputLeftRMSGlobal,
                            mInputRightRMSGlobal,
                            mInputLeftPeakGlobal,
                            mInputRightPeakGlobal);
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
    {
        calculateAndStoreLevels(buffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);
        return;
    }

    if (! hostBypassSessionActive)
    {
        // Keep the audible raw tap fixed for the complete host-bypass session.
        // The hidden wet graph may finish an HQ transition meanwhile, but a
        // delayed dry signal must never expose that fractional-tap switch.
        hostBypassSessionHqMode = activeHqMode;
        hostBypassSessionActive = true;
    }

    // The host hears only latency-matched raw audio. In parallel, render a
    // discarded copy through the complete wet graph so recursive filters,
    // compressors, Lo-Fi, LFOs and live HQ/topology state machines remain on
    // the same timeline they would have followed without host bypass.
    hostBypassWetBuffer.makeCopyOf(buffer, true);
    processLatencyMatchedBypass(buffer, hostBypassSessionHqMode);
    processWetBlock(hostBypassWetBuffer, midiMessages, true);
    calculateAndStoreLevels(buffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);
}

void FireAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    isBypassed.store(false, std::memory_order_relaxed);
    hostBypassSessionActive = false;

    if (needsReset.exchange(false, std::memory_order_acq_rel))
        performReset();

    processWetBlock(buffer, midiMessages, false);
}

void FireAudioProcessor::processWetBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages,
    bool hostBypassShadow)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int numBufferChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    auto sampleRate = getSampleRate();

    if (! std::isfinite(sampleRate) || sampleRate <= 0)
    {
        sampleRate = 48000;
    }

    for (int channel = juce::jlimit(0, numBufferChannels, totalNumInputChannels);
         channel < numBufferChannels;
         ++channel)
        buffer.clear(channel, 0, numSamples);

    if (! hostBypassShadow)
        calculateAndStoreLevels(buffer,
                                mInputLeftRMSGlobal,
                                mInputRightRMSGlobal,
                                mInputLeftPeakGlobal,
                                mInputRightPeakGlobal);

    if (numBufferChannels == 0 || numSamples == 0)
    {
        calculateAndStoreLevels(buffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);
        return;
    }

    // Read the requested quality once. A live transition may deliberately use
    // two internally consistent quality ranges in this callback, but a
    // concurrent automation write cannot change the plan halfway through it.
    const bool requestedHq = loadCachedParameter(hqParameter) > 0.5f;
    const auto topologySequenceAtCallbackStart =
        multibandTopologyResetGeneration.load(std::memory_order_acquire);

    lfoOutputBuffer.setSize(4, numSamples, false, false, true);
    lfoOutputBuffer.clear();
    lfoManager->processBlock(lfoOutputBuffer, static_cast<float>(sampleRate), getPlayHead(), numSamples);
    const bool routingSnapshotWasRefreshed =
        lfoManager->wasRoutingSnapshotRefreshedThisBlock();

    HqCallbackContext callbackContext;
    updateParameters(lfoOutputBuffer,
                     topologySequenceAtCallbackStart,
                     routingSnapshotWasRefreshed,
                     callbackContext);

    mBuffer1.setSize(numBufferChannels, numSamples, false, false, true);
    mBuffer2.setSize(numBufferChannels, numSamples, false, false, true);
    mBuffer3.setSize(numBufferChannels, numSamples, false, false, true);
    mBuffer4.setSize(numBufferChannels, numSamples, false, false, true);
    delayMatchedDryBuffer.setSize(numBufferChannels, numSamples, false, false, true);
    mWetBuffer.setSize(numBufferChannels, numSamples, false, false, true);
    lofiDryBuffer.setSize(numBufferChannels, numSamples, false, false, true);

    const std::array<juce::AudioBuffer<float>*, 4> fullBandBuffers {
        &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4
    };
    delayMatchedDryBuffer.clear();

    const bool topologyTransitionActive =
        topologyTransitionPhase != TopologyTransitionPhase::steady;
    const bool hqTransitionNeedsService =
        hqTransitionPhase != HqTransitionPhase::steady
        || requestedHq != activeHqMode;
    const bool startTopologyTransition =
        ! topologyTransitionActive
        && hasPendingTopologyChange()
        && ! hqTransitionNeedsService;

    if (topologyTransitionActive || startTopologyTransition)
    {
        processTopologyTransitionBlock(buffer,
                                       lfoOutputBuffer,
                                       sampleRate,
                                       requestedHq,
                                       ! hostBypassShadow);
    }
    else
    {
        // The steady/HQ-only fast path retains the historical full-callback
        // crossover and meter snapshots. Audible Safe control is deliberately
        // causal and advances inside each BandProcessor range.
        splitBands(buffer, sampleRate);
        for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
        {
            auto* band = bands[static_cast<size_t>(bandIndex)].get();
            auto* bandBuffer = fullBandBuffers[static_cast<size_t>(bandIndex)];
            if (band == nullptr || bandBuffer == nullptr)
                continue;

            callbackContext.bandInputPeaks[static_cast<size_t>(bandIndex)] =
                bandBuffer->getMagnitude(0, bandBuffer->getNumSamples());
            calculateAndStoreLevels(*bandBuffer,
                                    band->mInputLeftRMS,
                                    band->mInputRightRMS,
                                    band->mInputLeftPeak,
                                    band->mInputRightPeak);
        }

        processHqTransitionBlock(buffer,
                                 lfoOutputBuffer,
                                 sampleRate,
                                 requestedHq,
                                 callbackContext,
                                 ! hostBypassShadow);
    }

    // Shadow rendering exists only to advance audible DSP state. Preserve the
    // established host-bypass UI/analysis contract: the wrapper publishes the
    // actual delayed-raw output meters, while wet history/FFT/graph FIFOs stay
    // untouched until normal processing resumes.
    if (hostBypassShadow)
        return;

    for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto* band = bands[static_cast<size_t>(bandIndex)].get();
        auto* bandBuffer = fullBandBuffers[static_cast<size_t>(bandIndex)];
        if (band == nullptr || bandBuffer == nullptr)
            continue;

        calculateAndStoreLevels(*bandBuffer,
                                band->mOutputLeftRMS,
                                band->mOutputRightRMS,
                                band->mOutputLeftPeak,
                                band->mOutputRightPeak);
    }

    mWetBuffer.makeCopyOf(buffer, true);
    captureHistorySamples();
    pushDataPairToFFT(mWetBuffer, delayMatchedDryBuffer);
    calculateAndStoreLevels(mWetBuffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);

    // --- 1. Push Modulated Filter Data to its FIFO ---
    if (filterFifo.getFreeSpace() >= 1)
    {
        // Get the final, modulated values for this block
        auto chainSettings = getCachedChainSettings(&lfoOutputBuffer);

        ModulatedFilterValues filterVals;
        filterVals.lowCutFreq = chainSettings.lowCutFreq;
        filterVals.lowCutGain = chainSettings.lowCutGainInDecibels;
        filterVals.lowCutQ = chainSettings.lowCutQuality;
        filterVals.highCutFreq = chainSettings.highCutFreq;
        filterVals.highCutGain = chainSettings.highCutGainInDecibels;
        filterVals.highCutQ = chainSettings.highCutQuality;
        filterVals.peakFreq = chainSettings.peakFreq;
        filterVals.peakGain = chainSettings.peakGainInDecibels;
        filterVals.peakQ = chainSettings.peakQuality;

        pushToFifo(filterFifo, filterFifoBuffer, filterVals);
    }

    if (graphFifo.getFreeSpace() >= 1)
    {
        DistortionGraphValues vals;
        const int bandIndex = juce::jlimit(0, 3, uiFocusBand.load(std::memory_order_relaxed));
        const auto& parameters = bandParameterCache[static_cast<size_t>(bandIndex)];

        const bool shapeEnabled = loadCachedParameter(parameters.shapeEnabled) > 0.5f;
        vals.rec = shapeEnabled
                       ? getBlockModulatedValue(parameters.rec, lfoOutputBuffer)
                       : 0.0f;
        const float bandMix = getBlockModulatedValue(parameters.mix, lfoOutputBuffer);
        const float shapeMix = shapeEnabled
                                   ? getBlockModulatedValue(parameters.shapeMix, lfoOutputBuffer)
                                   : 1.0f;
        vals.mix = bandMix * shapeMix;
        vals.bias = shapeEnabled
                        ? getBlockModulatedValue(parameters.bias, lfoOutputBuffer)
                        : 0.0f;
        const bool driveEnabled = loadCachedParameter(parameters.driveEnabled) > 0.5f;
        float driveBase = driveEnabled
                              ? getBlockModulatedValue(parameters.drive, lfoOutputBuffer)
                              : 0.0f;
        if (driveEnabled && loadCachedParameter(parameters.extreme) > 0.5f)
            driveBase *= std::log2(10.0f);

        vals.mode = juce::roundToInt(loadCachedParameter(parameters.mode));
        const bool isSafeModeOn = loadCachedParameter(parameters.safe) > 0.5f;

        float driveForCalc = driveBase * 6.5f / 100.0f;
        float powerDrive = powf(2, driveForCalc);
        float sampleMaxValue = getSampleMaxValue(bandIndex);

        if (driveEnabled && isSafeModeOn
            && sampleMaxValue > 0.0001f && sampleMaxValue * powerDrive > 2.0f)
            vals.drive = 2.0f / sampleMaxValue + 0.1f * driveForCalc;
        else
            vals.drive = powerDrive;

        vals.rateDivide = getBlockModulatedValue(downsampleRateParameter, lfoOutputBuffer);
        if (loadCachedParameter(downsampleEnabledParameter) > 0.5f)
            vals.rateDivide = 1.0f;

        pushToFifo(graphFifo, graphFifoBuffer, vals);
    }
    if (meterFifo.getFreeSpace() >= 1)
    {
        MeterValues values;

        // Global Meters
        values.inputRMS_L = mInputLeftRMSGlobal.load();
        values.inputRMS_R = mInputRightRMSGlobal.load();
        values.inputPeak_L = mInputLeftPeakGlobal.load();
        values.inputPeak_R = mInputRightPeakGlobal.load();
        values.outputRMS_L = mOutputLeftRMSGlobal.load();
        values.outputRMS_R = mOutputRightRMSGlobal.load();
        values.outputPeak_L = mOutputLeftPeakGlobal.load();
        values.outputPeak_R = mOutputRightPeakGlobal.load();

        // Per-Band Meters
        for (int i = 0; i < 4; ++i)
        {
            if (auto* band = bands[i].get())
            {
                values.bandInputRMS_L[i] = band->mInputLeftRMS.load();
                values.bandInputRMS_R[i] = band->mInputRightRMS.load();
                values.bandInputPeak_L[i] = band->mInputLeftPeak.load();
                values.bandInputPeak_R[i] = band->mInputRightPeak.load();

                values.bandOutputRMS_L[i] = band->mOutputLeftRMS.load();
                values.bandOutputRMS_R[i] = band->mOutputRightRMS.load();
                values.bandOutputPeak_L[i] = band->mOutputLeftPeak.load();
                values.bandOutputPeak_R[i] = band->mOutputRightPeak.load();
            }
        }

        pushToFifo(meterFifo, meterFifoBuffer, values);
    }

}

//==============================================================================
bool FireAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FireAudioProcessor::createEditor()
{
    return new FireAudioProcessorEditor(*this);
}

//==============================================================================
void FireAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    int xmlIndex = 0;
    juce::XmlElement xmlState { "state" };

    // 1. save treestate (parameters)
    auto state = treeState.copyState();
    std::unique_ptr<juce::XmlElement> treeStateXml(state.createXml());
    xmlState.insertChildElement(treeStateXml.release(), xmlIndex++);

    // 2. save current preset ID, width and height
    auto currentStateXml = std::make_unique<juce::XmlElement>("otherState");
    currentStateXml->setAttribute("currentPresetID", statePresets.getCurrentPresetId());
    currentStateXml->setAttribute("currentPresetKey", statePresets.getCurrentPresetKey());
    currentStateXml->setAttribute("editorWidth", editorWidth.load(std::memory_order_relaxed));
    currentStateXml->setAttribute("editorHeight", editorHeight.load(std::memory_order_relaxed));

    xmlState.insertChildElement(currentStateXml.release(), xmlIndex++);

    const auto lfoDataToSave = lfoManager->getLfoDataCopy();
    const auto routingsToSave = lfoManager->getModulationRoutingsCopy();

    // 3. Save LFO Shapes
    auto lfoState = std::make_unique<juce::XmlElement>("LFO_STATE");
    for (int i = 0; i < static_cast<int>(lfoDataToSave.size()); ++i)
    {
        auto lfoXml = std::make_unique<juce::XmlElement>("LFO");
        lfoXml->setAttribute("index", i);
        lfoDataToSave[static_cast<size_t>(i)].writeToXml(*lfoXml);
        lfoState->addChildElement(lfoXml.release());
    }
    xmlState.insertChildElement(lfoState.release(), xmlIndex++);

    // 4. Save Modulation Matrix Routings
    auto modMatrixState = std::make_unique<juce::XmlElement>("MODULATION_STATE");
    for (const auto& routing : routingsToSave)
    {
        auto routingXml = std::make_unique<juce::XmlElement>("ROUTING");
        routing.writeToXml(*routingXml);
        modMatrixState->addChildElement(routingXml.release());
    }
    xmlState.insertChildElement(modMatrixState.release(), xmlIndex++);

    // Persist the inactive A/B snapshot alongside the currently active state.
    stateAB.writeToXml(xmlState);

    copyXmlToBinary(xmlState, destData);
}

void FireAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName("state"))
        return;

    // Parse and validate every section before mutating live state. Hosts may
    // retain a damaged or truncated chunk for years; applying only its UI/LFO
    // tail while leaving the old parameters in place creates a state that was
    // never saved and is much harder to recover from than rejecting the chunk.
    const auto* xmlTreeState = xmlState->getChildByName(treeState.state.getType().toString());
    if (xmlTreeState == nullptr)
        xmlTreeState = xmlState->getChildElement(0); // Legacy chunks relied on ordering.

    if (xmlTreeState == nullptr)
        return;

    const auto incomingParameterState = juce::ValueTree::fromXml(*xmlTreeState);
    if (! incomingParameterState.isValid()
        || ! incomingParameterState.hasType(treeState.state.getType()))
        return;

    const auto parameterStateTemplate = treeState.copyState();
    juce::ValueTree treeToLoad(parameterStateTemplate.getType());
    juce::StringArray loadedParameterIDs;
    std::array<bool, 4> smoothnessPresentInParameterState {};

    const auto findParameterState = [](juce::ValueTree& state, const juce::String& parameterID)
    {
        for (auto child : state)
            if (child.getProperty("id").toString() == parameterID)
                return child;

        return juce::ValueTree {};
    };

    // Begin with one canonical child for every current parameter. Missing
    // parameters in an older state therefore migrate to their defaults instead
    // of inheriting whatever value happened to be live before the load.
    for (const auto& templateChild : parameterStateTemplate)
    {
        const auto parameterID = templateChild.getProperty("id").toString();
        auto* parameter = treeState.getParameter(parameterID);
        if (parameter == nullptr)
            continue;

        auto child = templateChild.createCopy();
        child.setProperty("value",
                          parameter->convertFrom0to1(parameter->getDefaultValue()),
                          nullptr);
        treeToLoad.addChild(child, -1, nullptr);
    }

    int recognisedParameterCount = 0;
    for (const auto& incomingChild : incomingParameterState)
    {
        const auto parameterID = incomingChild.getProperty("id").toString();
        auto* parameter = treeState.getParameter(parameterID);
        if (parameterID.isEmpty() || parameter == nullptr
            || loadedParameterIDs.contains(parameterID))
            continue;

        if (! incomingChild.hasProperty("value"))
            return;

        double parsedValue = 0.0;
        if (! parseStrictFiniteDouble(incomingChild.getProperty("value").toString(), parsedValue))
            return;

        // APVTS child values are denormalised. Clamp before snapping so a
        // damaged out-of-range chunk does not trip JUCE's debug assertion in
        // convertTo0to1(), while integer/choice parameters remain legal.
        const auto& range = parameter->getNormalisableRange();
        const float boundedValue = juce::jlimit(range.start,
                                                range.end,
                                                static_cast<float>(parsedValue));
        const float safeValue = range.snapToLegalValue(boundedValue);
        auto targetChild = findParameterState(treeToLoad, parameterID);
        if (! targetChild.isValid())
            return;

        targetChild.setProperty("value", safeValue, nullptr);
        loadedParameterIDs.add(parameterID);
        ++recognisedParameterCount;

        for (int i = 0; i < static_cast<int>(smoothnessPresentInParameterState.size()); ++i)
            if (parameterID == ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i))
                smoothnessPresentInParameterState[static_cast<size_t>(i)] = true;
    }

    // A correctly named but empty/foreign PARAMETERS node is not a usable Fire
    // state. Reject it transactionally rather than interpreting it as Init.
    if (recognisedParameterCount == 0)
        return;

    // Shape was always active before its explicit enable parameter existed.
    // Preserve that sound for old host chunks. The root-property lookup retains
    // compatibility with the brief legacy representation used by some builds.
    for (int i = 0; i < 4; ++i)
    {
        const auto shapeID = ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, i);
        if (loadedParameterIDs.contains(shapeID))
            continue;

        float legacyShapeValue = 1.0f;
        if (incomingParameterState.hasProperty(shapeID))
        {
            double parsedValue = 0.0;
            if (! parseStrictFiniteDouble(incomingParameterState.getProperty(shapeID).toString(), parsedValue))
                return;
            legacyShapeValue = parsedValue > 0.5 ? 1.0f : 0.0f;
        }

        if (auto shapeState = findParameterState(treeToLoad, shapeID); shapeState.isValid())
            shapeState.setProperty("value", legacyShapeValue, nullptr);
    }

    std::array<LfoData, 4> loadedLfoData;
    std::array<bool, 4> loadedLfoSmoothnessFromXml {};
    std::array<bool, 4> loadedLfoIndices {};
    if (auto* lfoState = xmlState->getChildByName("LFO_STATE"))
    {
        for (auto* lfoXml : lfoState->getChildIterator())
        {
            if (! lfoXml->hasTagName("LFO"))
                continue;

            const int index = lfoXml->getIntAttribute("index", -1);
            if (juce::isPositiveAndBelow(index, static_cast<int>(loadedLfoData.size()))
                && ! loadedLfoIndices[static_cast<size_t>(index)])
            {
                loadedLfoData[static_cast<size_t>(index)] = LfoData::readFromXml(*lfoXml);
                loadedLfoSmoothnessFromXml[static_cast<size_t>(index)] = lfoXml->hasAttribute("smoothness");
                loadedLfoIndices[static_cast<size_t>(index)] = true;
            }
        }
    }

    // The staged tree, rather than the still-live APVTS atomics, is authoritative
    // while this transaction is being assembled.
    for (size_t i = 0; i < loadedLfoData.size(); ++i)
    {
        if (! smoothnessPresentInParameterState[i] && loadedLfoSmoothnessFromXml[i])
            continue;

        const auto smoothnessID = ParameterIDAndName::getIDString(
            LFO_SMOOTH_ID, static_cast<int>(i));
        const auto smoothnessState = findParameterState(treeToLoad, smoothnessID);
        if (smoothnessState.isValid())
        {
            const float smoothness = static_cast<float>(smoothnessState.getProperty("value"));
            if (std::isfinite(smoothness))
                loadedLfoData[i].smoothness = juce::jlimit(0.0f, 1.0f, smoothness);
        }
    }

    constexpr int maximumStateModulationRoutings = 128;
    juce::Array<ModulationRouting> loadedRoutings;
    juce::StringArray loadedRoutingTargets;
    if (auto* modMatrixState = xmlState->getChildByName("MODULATION_STATE"))
    {
        for (auto* routingXml : modMatrixState->getChildIterator())
        {
            if (loadedRoutings.size() >= maximumStateModulationRoutings)
                break;
            if (! routingXml->hasTagName("ROUTING"))
                continue;

            auto routing = ModulationRouting::readFromXml(*routingXml);
            if (routing.targetParameterID.isEmpty()
                || treeState.getParameter(routing.targetParameterID) == nullptr
                || loadedRoutingTargets.contains(routing.targetParameterID))
                continue;

            loadedRoutingTargets.add(routing.targetParameterID);
            loadedRoutings.add(std::move(routing));
        }
    }

    const auto* xmlCurrentState = xmlState->getChildByName("otherState");
    const auto presetKey = xmlCurrentState != nullptr
                               ? xmlCurrentState->getStringAttribute("currentPresetKey").trim()
                               : juce::String {};
    const int legacyPresetID = xmlCurrentState != nullptr
                                   ? juce::jlimit(0,
                                                  statePresets.getNumPresets(),
                                                  xmlCurrentState->getIntAttribute("currentPresetID", 0))
                                   : 0;
    const int restoredEditorWidth = xmlCurrentState != nullptr
                                        ? juce::jlimit(static_cast<int>(INIT_WIDTH),
                                                       2000,
                                                       xmlCurrentState->getIntAttribute(
                                                           "editorWidth", static_cast<int>(INIT_WIDTH)))
                                        : editorWidth.load(std::memory_order_relaxed);
    const int restoredEditorHeight = xmlCurrentState != nullptr
                                         ? juce::jlimit(static_cast<int>(INIT_HEIGHT),
                                                        1000,
                                                        xmlCurrentState->getIntAttribute(
                                                            "editorHeight", static_cast<int>(INIT_HEIGHT)))
                                         : editorHeight.load(std::memory_order_relaxed);

    // Commit only after the complete chunk has passed validation. Keep the
    // audio thread on its previous coherent multiband snapshot until the APVTS
    // state and modulation routings have both been replaced.
    beginMultibandTopologyEdit();
    treeState.replaceState(treeToLoad);
    if (xmlCurrentState != nullptr)
    {
        if (presetKey.isNotEmpty())
            statePresets.setCurrentPresetKey(presetKey);
        else
            statePresets.setCurrentPresetId(legacyPresetID);
        editorWidth.store(restoredEditorWidth, std::memory_order_relaxed);
        editorHeight.store(restoredEditorHeight, std::memory_order_relaxed);
    }

    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        lfoManager->clearAllLfoData();
        for (int i = 0; i < static_cast<int>(loadedLfoData.size()); ++i)
            lfoManager->setLfoData(i, loadedLfoData[static_cast<size_t>(i)]);
        lfoManager->getModulationRoutings() = std::move(loadedRoutings);
    }

    // A/B restoration mutates its internal snapshot, so keep it inside the
    // commit phase after the complete host chunk has passed validation.
    stateAB.readFromXml(xmlState->getChildByName("AB_STATE"));

    // State replacement can migrate band parameters while NUM_BANDS stays
    // unchanged. Publish an explicit topology generation so the next audio
    // block cannot reuse DSP history from the previous logical band layout.
    requestMultibandTopologyReset();
    sendChangeMessage();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FireAudioProcessor();
}

// Filter selection
void updateCoefficients(CoefficientsPtr& old, const CoefficientsPtr& replacements)
{
    *old = *replacements;
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowCutFreq = apvts.getRawParameterValue(LOWCUT_FREQ_ID)->load();
    settings.lowCutQuality = apvts.getRawParameterValue(LOWCUT_Q_ID)->load();
    settings.lowCutGainInDecibels = apvts.getRawParameterValue(LOWCUT_GAIN_ID)->load();
    settings.highCutFreq = apvts.getRawParameterValue(HIGHCUT_FREQ_ID)->load();
    settings.highCutQuality = apvts.getRawParameterValue(HIGHCUT_Q_ID)->load();
    settings.highCutGainInDecibels = apvts.getRawParameterValue(HIGHCUT_GAIN_ID)->load();
    settings.peakFreq = apvts.getRawParameterValue(PEAK_FREQ_ID)->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue(PEAK_GAIN_ID)->load();
    settings.peakQuality = apvts.getRawParameterValue(PEAK_Q_ID)->load();
    settings.lowCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(LOWCUT_SLOPE_ID));
    settings.highCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID));

    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load() > 0.5f;

    return settings;
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts, LfoManager& lfoManager)
{
    ChainSettings settings;

    // For each filter parameter, get its final value from the LfoManager.
    settings.lowCutFreq = lfoManager.getModulatedValue(LOWCUT_FREQ_ID);
    settings.lowCutQuality = lfoManager.getModulatedValue(LOWCUT_Q_ID);
    settings.lowCutGainInDecibels = lfoManager.getModulatedValue(LOWCUT_GAIN_ID);
    settings.highCutFreq = lfoManager.getModulatedValue(HIGHCUT_FREQ_ID);
    settings.highCutQuality = lfoManager.getModulatedValue(HIGHCUT_Q_ID);
    settings.highCutGainInDecibels = lfoManager.getModulatedValue(HIGHCUT_GAIN_ID);
    settings.peakFreq = lfoManager.getModulatedValue(PEAK_FREQ_ID);
    settings.peakGainInDecibels = lfoManager.getModulatedValue(PEAK_GAIN_ID);
    settings.peakQuality = lfoManager.getModulatedValue(PEAK_Q_ID);

    // These parameters are not modulatable, so we get them directly from the apvts.
    settings.lowCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(LOWCUT_SLOPE_ID));
    settings.highCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID));
    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load() > 0.5f;

    return settings;
}

CoefficientsPtr makePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.peakFreq,
                                                               chainSettings.peakQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
}

CoefficientsPtr makeLowcutQFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.lowCutFreq,
                                                               chainSettings.lowCutQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.lowCutGainInDecibels));
}

CoefficientsPtr makeHighcutQFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.highCutFreq,
                                                               chainSettings.highCutQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.highCutGainInDecibels));
}

void FireAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    const auto peakCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate,
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    // Logical stage bypassing is crossfaded after the processor. Keep the wet
    // filter running so its recursive state is ready when the stage returns.
    leftChain.setBypassed<ChainPositions::Peak>(false);
    rightChain.setBypassed<ChainPositions::Peak>(false);
    *leftChain.get<ChainPositions::Peak>().coefficients = peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = peakCoefficients;
}

void FireAudioProcessor::updateCutSlopeTransition(
    CutSlopeTransitionState& transition,
    CutFilter& leftPrimary,
    CutFilter& rightPrimary,
    float frequency,
    double sampleRate,
    Slope requestedSlope,
    bool highPass)
{
    transition.lastFrequency = frequency;
    transition.lastSampleRate = sampleRate;
    transition.highPass = highPass;

    if (! transition.initialised)
    {
        transition.slotSlopes = { requestedSlope, requestedSlope };
        transition.requestedSlope = requestedSlope;
        transition.standbyMix.setCurrentAndTargetValue(0.0f);
        transition.initialised = true;
    }
    else
    {
        transition.requestedSlope = requestedSlope;
        if (transition.standbyMix.isSmoothing())
        {
            const int targetSlot = transition.standbyMix.getTargetValue() >= 0.5f ? 1 : 0;
            const int sourceSlot = 1 - targetSlot;

            // A request for the chain we are fading away from can reverse the
            // same two warm recursive states without rewriting either one.
            if (requestedSlope == transition.slotSlopes[static_cast<size_t>(sourceSlot)])
                transition.standbyMix.setTargetValue(sourceSlot == 1 ? 1.0f : 0.0f);
            // A third recipe is remembered in requestedSlope and starts after
            // the current pair reaches an endpoint. Rewriting a partially
            // audible IIR bank here would recreate the coefficient hard cut.
        }
        else
        {
            const int activeSlot = transition.standbyMix.getCurrentValue() >= 0.5f ? 1 : 0;
            if (requestedSlope != transition.slotSlopes[static_cast<size_t>(activeSlot)])
            {
                const int targetSlot = 1 - activeSlot;
                if (transition.slotSlopes[static_cast<size_t>(targetSlot)] != requestedSlope)
                    transition.slotSlopes[static_cast<size_t>(targetSlot)] = requestedSlope;
                transition.standbyMix.setTargetValue(targetSlot == 1 ? 1.0f : 0.0f);
            }
        }
    }

    updateButterworthCutFilterPair(leftPrimary,
                                   rightPrimary,
                                   frequency,
                                   sampleRate,
                                   transition.slotSlopes[0],
                                   highPass);
    updateButterworthCutFilterPair(transition.leftStandby,
                                   transition.rightStandby,
                                   frequency,
                                   sampleRate,
                                   transition.slotSlopes[1],
                                   highPass);
}

void FireAudioProcessor::processCutFilterStage(
    CutFilter& leftPrimary,
    CutFilter& rightPrimary,
    CutSlopeTransitionState& transition,
    juce::dsp::AudioBlock<float>& fullBlock,
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& wetMix,
    int startSample,
    int numSamples) noexcept
{
    if (numSamples <= 0 || fullBlock.getNumChannels() == 0)
        return;

    const int shadowCapacity = globalCutSlopeShadowBuffer.getNumSamples();
    jassert(shadowCapacity > 0);
    if (shadowCapacity <= 0)
        return;

    const auto processShadowRange = [&] (CutFilter& leftProcessor,
                                         CutFilter& rightProcessor,
                                         int rangeStart,
                                         int rangeSamples)
    {
        int processed = 0;
        while (processed < rangeSamples)
        {
            const int chunkSamples = juce::jmin(shadowCapacity, rangeSamples - processed);
            auto sourceBlock = fullBlock.getSubBlock(
                static_cast<size_t>(rangeStart + processed),
                static_cast<size_t>(chunkSamples));
            const int processedChannels = sourceBlock.getNumChannels() > 1 ? 2 : 1;
            auto shadowBlock = juce::dsp::AudioBlock<float>(globalCutSlopeShadowBuffer)
                                   .getSubsetChannelBlock(0, static_cast<size_t>(processedChannels))
                                   .getSubBlock(0, static_cast<size_t>(chunkSamples));
            shadowBlock.copyFrom(sourceBlock.getSubsetChannelBlock(
                0, static_cast<size_t>(processedChannels)));

            auto shadowLeft = shadowBlock.getSingleChannelBlock(0);
            leftProcessor.process(juce::dsp::ProcessContextReplacing<float>(shadowLeft));
            if (processedChannels > 1)
            {
                auto shadowRight = shadowBlock.getSingleChannelBlock(1);
                rightProcessor.process(juce::dsp::ProcessContextReplacing<float>(shadowRight));
            }
            processed += chunkSamples;
        }
    };

    int processedSamples = 0;
    while (processedSamples < numSamples)
    {
        if (! transition.standbyMix.isSmoothing())
        {
            const bool standbyIsActive = transition.standbyMix.getCurrentValue() >= 0.5f;
            auto& activeLeft = standbyIsActive ? transition.leftStandby : leftPrimary;
            auto& activeRight = standbyIsActive ? transition.rightStandby : rightPrimary;
            auto& inactiveLeft = standbyIsActive ? leftPrimary : transition.leftStandby;
            auto& inactiveRight = standbyIsActive ? rightPrimary : transition.rightStandby;
            const int remainingSamples = numSamples - processedSamples;
            const int rangeStart = startSample + processedSamples;

            // Keep the other slope recipe on the same input timeline so a
            // quick reversal can reuse its recursive history. Its output is
            // discarded and cannot perturb the legacy steady-state path.
            processShadowRange(inactiveLeft,
                               inactiveRight,
                               rangeStart,
                               remainingSamples);
            processGlobalFilterStage(activeLeft,
                                     activeRight,
                                     fullBlock,
                                     globalFilterStageDryBuffer,
                                     wetMix,
                                     rangeStart,
                                     remainingSamples);
            return;
        }

        const int sampleIndex = startSample + processedSamples;
        const auto slopeMix = transition.standbyMix.getCurrentValue();
        auto stageMix = wetMix.getCurrentValue();
        if (wetMix.isSmoothing())
            stageMix = wetMix.getNextValue();

        const int processedChannels = fullBlock.getNumChannels() > 1 ? 2 : 1;
        for (int channel = 0; channel < processedChannels; ++channel)
        {
            auto& primary = channel == 0 ? leftPrimary : rightPrimary;
            auto& standby = channel == 0 ? transition.leftStandby
                                         : transition.rightStandby;
            auto* channelData = fullBlock.getChannelPointer(static_cast<size_t>(channel));
            const auto dry = channelData[sampleIndex];
            const auto primaryWet = processCutFilterSample(primary, dry);
            const auto standbyWet = processCutFilterSample(standby, dry);
            const auto slopeWet = slopeMix <= 0.0f ? primaryWet
                                : slopeMix >= 1.0f ? standbyWet
                                                   : primaryWet
                                                       + slopeMix * (standbyWet - primaryWet);
            channelData[sampleIndex] = stageMix <= 0.0f ? dry
                                     : stageMix >= 1.0f ? slopeWet
                                                        : dry + stageMix * (slopeWet - dry);
        }

        transition.standbyMix.getNextValue();
        ++processedSamples;

        if (! transition.standbyMix.isSmoothing())
        {
            snapCutFilterToZero(leftPrimary);
            snapCutFilterToZero(rightPrimary);
            snapCutFilterToZero(transition.leftStandby);
            snapCutFilterToZero(transition.rightStandby);

            // A third recipe requested during the crossfade starts only after
            // one bank is fully inaudible. This preserves continuity while
            // still making the latest automation value win.
            updateCutSlopeTransition(transition,
                                     leftPrimary,
                                     rightPrimary,
                                     transition.lastFrequency,
                                     transition.lastSampleRate,
                                     transition.requestedSlope,
                                     transition.highPass);
        }
    }
}

void FireAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings, double sampleRate)
{
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();

    const auto lowcutQCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate,
        chainSettings.lowCutFreq,
        chainSettings.lowCutQuality,
        juce::Decibels::decibelsToGain(chainSettings.lowCutGainInDecibels));

    leftChain.setBypassed<ChainPositions::LowCut>(false);
    rightChain.setBypassed<ChainPositions::LowCut>(false);
    leftChain.setBypassed<ChainPositions::LowCutQ>(false);
    rightChain.setBypassed<ChainPositions::LowCutQ>(false);

    updateCutSlopeTransition(lowCutSlopeTransition,
                             leftLowCut,
                             rightLowCut,
                             chainSettings.lowCutFreq,
                             sampleRate,
                             chainSettings.lowCutSlope,
                             true);

    *leftChain.get<ChainPositions::LowCutQ>().coefficients = lowcutQCoefficients;
    *rightChain.get<ChainPositions::LowCutQ>().coefficients = lowcutQCoefficients;
}

void FireAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings, double sampleRate)
{
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

    const auto highcutQCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate,
        chainSettings.highCutFreq,
        chainSettings.highCutQuality,
        juce::Decibels::decibelsToGain(chainSettings.highCutGainInDecibels));

    leftChain.setBypassed<ChainPositions::HighCut>(false);
    rightChain.setBypassed<ChainPositions::HighCut>(false);
    leftChain.setBypassed<ChainPositions::HighCutQ>(false);
    rightChain.setBypassed<ChainPositions::HighCutQ>(false);

    updateCutSlopeTransition(highCutSlopeTransition,
                             leftHighCut,
                             rightHighCut,
                             chainSettings.highCutFreq,
                             sampleRate,
                             chainSettings.highCutSlope,
                             false);

    *leftChain.get<ChainPositions::HighCutQ>().coefficients = highcutQCoefficients;
    *rightChain.get<ChainPositions::HighCutQ>().coefficients = highcutQCoefficients;
}

void FireAudioProcessor::updateFilter(double sampleRate)
{
    auto chainSettings = getChainSettings(treeState);
    if (! std::isfinite(sampleRate) || sampleRate <= 0.0)
        return;

    const auto frequencyRange = getSafeFilterFrequencyRange(sampleRate);

    chainSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.highCutFreq);

    // Set the target values for all 9 smoothers
    lowcutFreqSmoother.setTargetValue(chainSettings.lowCutFreq);
    lowcutGainSmoother.setTargetValue(chainSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.setTargetValue(chainSettings.lowCutQuality);

    peakFreqSmoother.setTargetValue(chainSettings.peakFreq);
    peakGainSmoother.setTargetValue(chainSettings.peakGainInDecibels);
    peakQualitySmoother.setTargetValue(chainSettings.peakQuality);

    highcutFreqSmoother.setTargetValue(chainSettings.highCutFreq);
    highcutGainSmoother.setTargetValue(chainSettings.highCutGainInDecibels);
    highcutQualitySmoother.setTargetValue(chainSettings.highCutQuality);

    // Create a new settings object with the smoothed values for this audio block
    ChainSettings smoothedSettings;
    smoothedSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum,
                                               frequencyRange.maximum,
                                               lowcutFreqSmoother.getNextValue());
    smoothedSettings.lowCutGainInDecibels = lowcutGainSmoother.getNextValue();
    smoothedSettings.lowCutQuality = lowcutQualitySmoother.getNextValue();

    smoothedSettings.peakFreq = juce::jlimit(frequencyRange.minimum,
                                            frequencyRange.maximum,
                                            peakFreqSmoother.getNextValue());
    smoothedSettings.peakGainInDecibels = peakGainSmoother.getNextValue();
    smoothedSettings.peakQuality = peakQualitySmoother.getNextValue();

    smoothedSettings.highCutFreq = juce::jlimit(frequencyRange.minimum,
                                                frequencyRange.maximum,
                                                highcutFreqSmoother.getNextValue());

    smoothedSettings.highCutGainInDecibels = highcutGainSmoother.getNextValue();
    smoothedSettings.highCutQuality = highcutQualitySmoother.getNextValue();

    // Slopes and bypass states don't need smoothing, use them directly
    smoothedSettings.lowCutSlope = chainSettings.lowCutSlope;
    smoothedSettings.highCutSlope = chainSettings.highCutSlope;
    smoothedSettings.lowCutBypassed = chainSettings.lowCutBypassed;
    smoothedSettings.peakBypassed = chainSettings.peakBypassed;
    smoothedSettings.highCutBypassed = chainSettings.highCutBypassed;

    // Call the modular update functions
    updateLowCutFilters(smoothedSettings, sampleRate);
    updatePeakFilter(smoothedSettings, sampleRate);
    updateHighCutFilters(smoothedSettings, sampleRate);

    cachedGlobalFilterSettings = smoothedSettings;
    cachedGlobalFilterSampleRate = sampleRate;
    globalFilterCacheValid = true;
}

bool FireAudioProcessor::isSlient(const juce::AudioBuffer<float>& buffer)
{
    return buffer.getNumChannels() == 0
        || buffer.getNumSamples() == 0
        || buffer.getMagnitude(0, buffer.getNumSamples()) == 0.0f;
}

void FireAudioProcessor::setHistoryArray(int bandIndex)
{
    historySourceBand.store(juce::isPositiveAndBelow(bandIndex, 4) ? bandIndex : 4,
                            std::memory_order_relaxed);
}

void FireAudioProcessor::captureHistorySamples()
{
    const std::array<const juce::AudioBuffer<float>*, 5> sourceBuffers {
        &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4, &mWetBuffer
    };
    const int sourceIndex = juce::jlimit(0, 4, historySourceBand.load(std::memory_order_relaxed));
    const auto* sourceBuffer = sourceBuffers[static_cast<size_t>(sourceIndex)];

    if (sourceBuffer == nullptr || sourceBuffer->getNumChannels() == 0 || sourceBuffer->getNumSamples() == 0)
        return;

    const auto* left = sourceBuffer->getReadPointer(0);
    const auto* right = sourceBuffer->getNumChannels() > 1 ? sourceBuffer->getReadPointer(1) : left;
    int writePosition = historyWritePosition.load(std::memory_order_relaxed);

    for (int sample = 0; sample < sourceBuffer->getNumSamples(); sample += 10)
    {
        const auto index = static_cast<size_t>(writePosition);
        historyArrayL[index].store(left[sample], std::memory_order_relaxed);
        historyArrayR[index].store(right[sample], std::memory_order_relaxed);
        writePosition = (writePosition + 1) % historyLength;
    }

    historySamplesAvailable.store(historyLength, std::memory_order_relaxed);
    historyWritePosition.store(writePosition, std::memory_order_release);
    historyGeneration.fetch_add(1, std::memory_order_release);
}

std::uint64_t FireAudioProcessor::getHistoryGeneration() const noexcept
{
    return historyGeneration.load(std::memory_order_acquire);
}

void FireAudioProcessor::copyHistoryArrays(juce::Array<float>& leftDestination,
                                           juce::Array<float>& rightDestination) const
{
    const int writePosition = historyWritePosition.load(std::memory_order_acquire);
    const int count = juce::jlimit(0, historyLength, historySamplesAvailable.load(std::memory_order_relaxed));
    const int start = (writePosition - count + historyLength) % historyLength;

    leftDestination.resize(count);
    rightDestination.resize(count);
    for (int i = 0; i < count; ++i)
    {
        const auto index = static_cast<size_t>((start + i) % historyLength);
        leftDestination.setUnchecked(i, historyArrayL[index].load(std::memory_order_relaxed));
        rightDestination.setUnchecked(i, historyArrayR[index].load(std::memory_order_relaxed));
    }
}

juce::Array<float> FireAudioProcessor::getHistoryArrayL()
{
    juce::Array<float> left;
    juce::Array<float> right;
    copyHistoryArrays(left, right);
    return left;
}

juce::Array<float> FireAudioProcessor::getHistoryArrayR()
{
    juce::Array<float> left;
    juce::Array<float> right;
    copyHistoryArrays(left, right);
    return right;
}

int FireAudioProcessor::getNumBins() const noexcept
{
    return SpectrumProcessor::numBins;
}

int FireAudioProcessor::getFFTSize() const noexcept
{
    return SpectrumProcessor::fftSize;
}

bool FireAudioProcessor::popLatestFFTFrames(float* processedDestination,
                                            int processedDestinationSize,
                                            float* originalDestination,
                                            int originalDestinationSize) noexcept
{
    return spectrumProcessor.popLatestFramePair(processedDestination,
                                                 processedDestinationSize,
                                                 originalDestination,
                                                 originalDestinationSize);
}

void FireAudioProcessor::pushDataPairToFFT(const juce::AudioBuffer<float>& processedBuffer,
                                           const juce::AudioBuffer<float>& originalBuffer)
{
    if (processedBuffer.getNumChannels() <= 0 || originalBuffer.getNumChannels() <= 0)
        return;

    const auto* processedData = processedBuffer.getReadPointer(0);
    const auto* originalData = originalBuffer.getReadPointer(0);
    const int samplesToPush = juce::jmin(processedBuffer.getNumSamples(),
                                          originalBuffer.getNumSamples());

    for (int sample = 0; sample < samplesToPush; ++sample)
        spectrumProcessor.pushNextSamplePairIntoFifo(processedData[sample], originalData[sample]);
}

bool FireAudioProcessor::processFFT(float* tempFFTData, int bufferSize)
{
    return spectrumProcessor.doProcessing(tempFFTData, bufferSize);
}

int FireAudioProcessor::getSavedWidth() const
{
    return editorWidth.load(std::memory_order_relaxed);
}

int FireAudioProcessor::getSavedHeight() const
{
    return editorHeight.load(std::memory_order_relaxed);
}

void FireAudioProcessor::setSavedWidth(const int width)
{
    editorWidth.store(width, std::memory_order_relaxed);
}

void FireAudioProcessor::setSavedHeight(const int height)
{
    editorHeight.store(height, std::memory_order_relaxed);
}

bool FireAudioProcessor::getBypassedState() const
{
    return isBypassed.load(std::memory_order_relaxed);
}

// drive lookandfeel
float FireAudioProcessor::getReductionPrecent(int bandIndex)
{
    // Safely check if the provided index is valid for our bands vector.
    if (juce::isPositiveAndBelow(bandIndex, bands.size()))
        if (auto* band = bands[bandIndex].get())
            return band->mReductionPercent.load(std::memory_order_relaxed);

    // If the index is invalid, assert in debug mode and return a safe default.
    jassertfalse;
    return 0.0f;
}

float FireAudioProcessor::getSampleMaxValue(int bandIndex)
{
    if (juce::isPositiveAndBelow(bandIndex, bands.size()))
        if (auto* band = bands[bandIndex].get())
            return band->mSampleMaxValue.load(std::memory_order_relaxed);

    jassertfalse;
    return 0.0f;
}

juce::AudioProcessorValueTreeState::ParameterLayout FireAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    using PBool = juce::AudioParameterBool;
    using PInt = juce::AudioParameterInt;
    using PFloat = juce::AudioParameterFloat;
    using PChoice = juce::AudioParameterChoice;

    // --- Global Parameters ---
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(HQ_ID), HQ_NAME, false));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(OUTPUT_ID), GLOBAL_OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(MIX_ID), GLOBAL_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(NUM_BANDS_ID), NUM_BANDS_NAME, 1, 4, 1));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(FILTER_BYPASS_ID), FILTER_BYPASS_NAME, false));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(DOWNSAMPLE_ID), DOWNSAMPLE_NAME, juce::NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(BIT_DEPTH_ID), BIT_DEPTH_NAME, 4, 32, 32));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(JITTER_ID), JITTER_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(DOWNSAMPLE_MIX_ID), DOWNSAMPLE_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(DOWNSAMPLE_BYPASS_ID), DOWNSAMPLE_BYPASS_NAME, false));

    // --- Per-Band Parameters (created in a loop) ---
    for (int i = 0; i < 4; ++i)
    {
        parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(MODE_ID, i), MODE_NAME, 0, 11, 3));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LINKED_ID, i), LINKED_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(SAFE_ID, i), SAFE_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(EXTREME_ID, i), EXTREME_NAME, false));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(DRIVE_ID, i), DRIVE_NAME, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_RATIO_ID, i), COMP_RATIO_NAME, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_THRESH_ID, i), COMP_THRESH_NAME, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_ATTACK_ID, i), COMP_ATTACK_NAME, juce::NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.3f), 10.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_RELEASE_ID, i), COMP_RELEASE_NAME, juce::NormalisableRange<float>(10.0f, 2000.0f, 1.f, 0.3f), 100.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_MIX_ID, i), COMP_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(WIDTH_ID, i), WIDTH_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PAN_ID, i), PAN_NAME, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(WIDTH_MIX_ID, i), WIDTH_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(OUTPUT_ID, i), OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(MIX_ID, i), MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(BIAS_ID, i), BIAS_NAME, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(REC_ID, i), REC_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(SHAPE_MIX_ID, i), SHAPE_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_ENABLE_ID, i), BAND_ENABLE_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_SOLO_ID, i), BAND_SOLO_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(DRIVE_BYPASS_ID, i), DRIVE_BYPASS_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(COMP_BYPASS_ID, i), COMP_BYPASS_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(WIDTH_BYPASS_ID, i), WIDTH_BYPASS_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(SHAPE_BYPASS_ID, i), SHAPE_BYPASS_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(DC_FILTER_ID, i), DC_FILTER_NAME, false));
    }

    // --- Crossover Parameters ---
    juce::NormalisableRange<float> freqRange(40.0f, 10024.0f, 1.0f);
    freqRange.setSkewForCentre(651.0f);

    for (int i = 0; i < 3; ++i)
    {
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(FREQ_ID, i), ParameterIDAndName::getName(FREQ_NAME, i), freqRange, 21.0f));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LINE_STATE_ID, i), ParameterIDAndName::getName(LINE_STATE_NAME, i), false));
    }

    // --- Global Filter Parameters ---
    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.0f);
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(LOWCUT_FREQ_ID), LOWCUT_FREQ_NAME, cutoffRange, 20.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(LOWCUT_Q_ID), LOWCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(LOWCUT_GAIN_ID), LOWCUT_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(HIGHCUT_FREQ_ID), HIGHCUT_FREQ_NAME, cutoffRange, 20000.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(HIGHCUT_Q_ID), HIGHCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(HIGHCUT_GAIN_ID), HIGHCUT_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PEAK_FREQ_ID), PEAK_FREQ_NAME, cutoffRange, 1000.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PEAK_Q_ID), PEAK_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PEAK_GAIN_ID), PEAK_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(LOWCUT_SLOPE_ID), LOWCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(HIGHCUT_SLOPE_ID), HIGHCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LOWCUT_BYPASSED_ID), LOWCUT_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(PEAK_BYPASSED_ID), PEAK_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(HIGHCUT_BYPASSED_ID), HIGHCUT_BYPASSED_NAME, false));

    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(OFF_ID), OFF_NAME, true));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(PRE_ID), PRE_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(POST_ID), POST_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LOW_ID), LOW_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_ID), BAND_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(HIGH_ID), HIGH_NAME, true));

    juce::StringArray lfoRateSyncDivisions = {
        "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T", "1/2", "1 Bar", "2 Bars", "4 Bars"
    };

    // --- Per-LFO Parameters (created in a loop using the new structure) ---
    for (int i = 0; i < 4; ++i)
    {
        parameters.push_back(std::make_unique<PBool>(
            ParameterIDAndName::getID(LFO_SYNC_MODE_ID, i),
            LFO_SYNC_MODE_NAME,
            true));

        parameters.push_back(std::make_unique<PChoice>(
            ParameterIDAndName::getID(LFO_RATE_SYNC_ID, i),
            LFO_RATE_SYNC_NAME,
            lfoRateSyncDivisions,
            8 // Default index for "1/4"
            ));

        parameters.push_back(std::make_unique<PFloat>(
            ParameterIDAndName::getID(LFO_RATE_HZ_ID, i),
            LFO_RATE_HZ_NAME,
            juce::NormalisableRange<float>(0.01f, 100.0f, 0.01f, 0.3f),
            1.0f,
            "Hz"));

        parameters.push_back(std::make_unique<PFloat>(
            ParameterIDAndName::getID(LFO_SMOOTH_ID, i),
            LFO_SMOOTH_NAME,
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.0f));

        parameters.push_back(std::make_unique<PFloat>(
            ParameterIDAndName::getID(LFO_PHASE_ID, i),
            LFO_PHASE_NAME,
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.0f));
    }

    return { parameters.begin(), parameters.end() };
}

bool FireAudioProcessor::isDawPlaying() const
{
    return lfoManager->isDawPlaying();
}

float FireAudioProcessor::getLfoPhase(int lfoIndex) const
{
    return lfoManager->getLfoPhase(lfoIndex);
}

void FireAudioProcessor::updateParameters(
    const juce::AudioBuffer<float>& lfoOutputs,
    std::uint32_t topologySequenceAtCallbackStart,
    bool routingSnapshotWasRefreshed,
    HqCallbackContext& callbackContext)
{
    //==============================================================================
    // 1. Update Global and Crossover Parameters
    //==============================================================================

    MultibandTopologySnapshot requestedSnapshot;
    const bool hasStablePublication = tryCaptureMultibandTopologySnapshot(
        lfoOutputs,
        topologySequenceAtCallbackStart,
        routingSnapshotWasRefreshed,
        requestedSnapshot);
    topologyPendingChangedThisCallback = false;

    if (! activeMultibandTopologySnapshotInitialised)
        synchroniseMultibandTopologyResetState();

    if (! pendingMultibandTopologySnapshotInitialised)
    {
        pendingMultibandTopologySnapshot = activeMultibandTopologySnapshot;
        pendingMultibandTopologySnapshotInitialised = true;
    }

    if (hasStablePublication)
    {
        topologyPendingChangedThisCallback =
            ! sameTopologyIdentity(requestedSnapshot,
                                    pendingMultibandTopologySnapshot);
        pendingMultibandTopologySnapshot = requestedSnapshot;

        if (sameTopologyIdentity(requestedSnapshot,
                                 activeMultibandTopologySnapshot))
        {
            // Ordinary parameter/divider edits keep updating the audible
            // snapshot immediately. A true topology identity change remains
            // pending until the output bus has faded to zero.
            activeMultibandTopologySnapshot = requestedSnapshot;
            numBands = activeMultibandTopologySnapshot.numBands;
            activeCrossovers = numBands - 1;

            // Ordinary divider dragging remains smoothly interpolated.
            const auto& frequencies =
                activeMultibandTopologySnapshot.crossoverFrequencies;
            smoothedFreq1.setTargetValue(frequencies[0]);
            smoothedFreq2.setTargetValue(frequencies[1]);
            smoothedFreq3.setTargetValue(frequencies[2]);
            for (int bandIndex = 0; bandIndex < 4; ++bandIndex)
            {
                if (auto* band = bands[static_cast<size_t>(bandIndex)].get())
                {
                    const auto& params = activeMultibandTopologySnapshot
                                             .callbackContext
                                             .bandParameters[static_cast<size_t>(bandIndex)];
                    band->recSmoother.setTargetValue(params.recVal.baseValue);
                    band->biasSmoother.setTargetValue(params.biasVal.baseValue);
                }
            }
        }
    }

    callbackContext = activeMultibandTopologySnapshot.callbackContext;
    publishMultibandTelemetry(callbackContext,
                              numBands,
                              lfoOutputs);
}

void FireAudioProcessor::prepareHqCallbackContext(
    const juce::AudioBuffer<float>& lfoOutputs,
    int snapshotNumBands,
    HqCallbackContext& callbackContext)
{
    juce::ignoreUnused(lfoOutputs);
    callbackContext = HqCallbackContext {};
    callbackContext.anySoloActive = false;

    snapshotNumBands = juce::jlimit(1, 4, snapshotNumBands);
    for (int i = 0; i < 4; ++i)
    {
        const auto index = static_cast<size_t>(i);
        const auto& parameters = bandParameterCache[index];
        callbackContext.soloState[index] =
            i < snapshotNumBands
            && loadCachedParameter(parameters.solo) > 0.5f;
        if (i < snapshotNumBands)
            callbackContext.anySoloActive = callbackContext.anySoloActive
                                         || callbackContext.soloState[index];

        BandProcessingParameters params;
        params.isBandEnabled = loadCachedParameter(parameters.enabled) > 0.5f;
        params.mode = juce::roundToInt(loadCachedParameter(parameters.mode));
        params.isDriveEnabled = loadCachedParameter(parameters.driveEnabled) > 0.5f;
        params.isShapeEnabled = loadCachedParameter(parameters.shapeEnabled) > 0.5f;
        params.isCompEnabled = loadCachedParameter(parameters.compressorEnabled) > 0.5f;
        params.isWidthEnabled = loadCachedParameter(parameters.widthEnabled) > 0.5f;
        params.isSafeModeOn = loadCachedParameter(parameters.safe) > 0.5f;
        params.isExtremeModeOn = loadCachedParameter(parameters.extreme) > 0.5f;
        params.isDcFilterEnabled = params.isShapeEnabled
                                && loadCachedParameter(parameters.dcFilterEnabled) > 0.5f;

        const auto setupProvider = [this](ModulatedValueProvider& provider,
                                          int& lfoIndex,
                                          const CachedParameter& parameter)
        {
            if (parameter.ranged == nullptr)
                return;

            provider.baseValue = loadCachedParameter(parameter);
            provider.range = parameter.ranged->getNormalisableRange();

            LfoManager::AudioThreadRoutingInfo routingInfo;
            if (lfoManager->getAudioThreadRoutingInfo(parameter.ranged,
                                                      routingInfo))
            {
                provider.modulationDepth = routingInfo.depth;
                provider.isBipolar = routingInfo.isBipolar;
                lfoIndex = routingInfo.sourceLfoIndex;
            }
        };

        setupProvider(params.driveVal, params.driveLfoSourceIndex, parameters.drive);
        setupProvider(params.biasVal, params.biasLfoSourceIndex, parameters.bias);
        setupProvider(params.recVal, params.recLfoSourceIndex, parameters.rec);
        setupProvider(params.outputVal, params.outputLfoSourceIndex, parameters.output);
        setupProvider(params.mixValProvider,
                      params.mixLfoSourceIndex,
                      parameters.mix);
        setupProvider(params.shapeMixValProvider,
                      params.shapeMixLfoSourceIndex,
                      parameters.shapeMix);
        setupProvider(params.widthValProvider,
                      params.widthLfoSourceIndex,
                      parameters.width);
        setupProvider(params.panValProvider,
                      params.panLfoSourceIndex,
                      parameters.pan);
        setupProvider(params.widthMixValProvider,
                      params.widthMixLfoSourceIndex,
                      parameters.widthMix);
        setupProvider(params.compThresholdValProvider,
                      params.compThresholdLfoSourceIndex,
                      parameters.compressorThreshold);
        setupProvider(params.compRatioValProvider,
                      params.compRatioLfoSourceIndex,
                      parameters.compressorRatio);
        setupProvider(params.compAttackValProvider,
                      params.compAttackLfoSourceIndex,
                      parameters.compressorAttack);
        setupProvider(params.compReleaseValProvider,
                      params.compReleaseLfoSourceIndex,
                      parameters.compressorRelease);
        setupProvider(params.compMixValProvider,
                      params.compMixLfoSourceIndex,
                      parameters.compressorMix);

        if (loadCachedParameter(parameters.linked) > 0.5f)
            params.outputVal.baseValue = -0.1f
                                       * loadCachedParameter(parameters.drive);

        params.compRatio = params.compRatioValProvider.baseValue;
        params.compThreshold = params.compThresholdValProvider.baseValue;
        params.compAttack = params.compAttackValProvider.baseValue;
        params.compRelease = params.compReleaseValProvider.baseValue;
        params.compMixVal = params.compMixValProvider.baseValue;
        params.width = params.widthValProvider.baseValue;
        params.pan = params.panValProvider.baseValue;
        params.widthMixVal = params.widthMixValProvider.baseValue;
        params.mixVal = params.mixValProvider.baseValue;
        params.shapeMixVal = params.shapeMixValProvider.baseValue;

        callbackContext.bandParameters[index] = params;
    }
}

void FireAudioProcessor::sumBands(juce::AudioBuffer<float>& outputBuffer,
                                  const std::array<juce::AudioBuffer<float>*, 4>& sourceBandBuffers,
                                  bool ignoreSoloLogic,
                                  bool useDelayedSoloEnvelope)
{
    outputBuffer.clear();

    // The main summing loop now operates on the provided sourceBandBuffers.
    for (int i = 0; i < numBands; ++i)
    {
        // Get a pointer to the current source buffer for this band.
        auto* currentBandBuffer = sourceBandBuffers[i];

        // Skip if the buffer pointer is invalid for any reason.
        if (currentBandBuffer == nullptr)
            continue;

        // The number of samples to process for this operation.
        const int numSamples = juce::jmin(currentBandBuffer->getNumSamples(),
                                          outputBuffer.getNumSamples());
        const auto& gainBuffer = useDelayedSoloEnvelope
                                     ? delayedBandSoloGainEnvelope
                                     : bandSoloGainEnvelope;
        const bool hasGainEnvelope = ! ignoreSoloLogic
                                     && i < gainBuffer.getNumChannels()
                                     && numSamples <= gainBuffer.getNumSamples();
        const float* gainEnvelope = hasGainEnvelope
                                        ? gainBuffer.getReadPointer(i)
                                        : nullptr;
        if (numSamples <= 0)
            continue;

        // A fast Solo retarget can make the first and last delayed gains equal
        // while a transition still exists between them. Inspect the complete
        // range before taking a constant-gain fast path.
        const auto gainRange = gainEnvelope != nullptr
                                   ? juce::FloatVectorOperations::findMinAndMax(
                                       gainEnvelope, numSamples)
                                   : juce::Range<float>(1.0f, 1.0f);
        const bool isConstantZero = gainEnvelope != nullptr
                                    && juce::exactlyEqual(gainRange.getStart(), 0.0f)
                                    && juce::exactlyEqual(gainRange.getEnd(), 0.0f);
        const bool isConstantUnity = gainEnvelope == nullptr
                                     || (juce::exactlyEqual(gainRange.getStart(), 1.0f)
                                         && juce::exactlyEqual(gainRange.getEnd(), 1.0f));

        if (isConstantZero)
            continue;

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            if (channel < currentBandBuffer->getNumChannels())
            {
                if (isConstantUnity)
                    outputBuffer.addFrom(channel, 0, *currentBandBuffer, channel, 0, numSamples);
                else
                    juce::FloatVectorOperations::addWithMultiply(
                        outputBuffer.getWritePointer(channel),
                        currentBandBuffer->getReadPointer(channel),
                        gainEnvelope,
                        numSamples);
            }
        }
    }
}

void FireAudioProcessor::splitBands(const juce::AudioBuffer<float>& inputBuffer, double sampleRate)
{
    splitBandsRange(inputBuffer, 0, inputBuffer.getNumSamples(), sampleRate);
}

void FireAudioProcessor::splitBandsRange(
    const juce::AudioBuffer<float>& inputBuffer,
    int rangeStartSample,
    int rangeNumSamples,
    double sampleRate)
{
    const int totalNumOutputChannels = inputBuffer.getNumChannels();
    const int totalSamples = inputBuffer.getNumSamples();

    if (! std::isfinite(sampleRate) || sampleRate <= 0.0
        || totalSamples <= 0 || rangeNumSamples <= 0)
        return;
    rangeStartSample = juce::jlimit(0, totalSamples, rangeStartSample);
    const int rangeEndSample = juce::jlimit(
        rangeStartSample,
        totalSamples,
        rangeStartSample + rangeNumSamples);
    if (rangeEndSample <= rangeStartSample)
        return;

    const int lineNum = activeCrossovers;

    const auto frequencyRange = getSafeFilterFrequencyRange(sampleRate);
    if (lineNum >= 2)
    {
        mSplitTemp1.setSize(totalNumOutputChannels, totalSamples, false, false, true);
        mSplitTemp2.setSize(totalNumOutputChannels, totalSamples, false, false, true);
        if (lineNum == 3)
            mSplitTemp3.setSize(totalNumOutputChannels, totalSamples, false, false, true);
    }

    const auto copyRange = [totalNumOutputChannels](juce::AudioBuffer<float>& destination,
                                                     const juce::AudioBuffer<float>& source,
                                                     int startSample,
                                                     int samplesInRange)
    {
        const int channels = juce::jmin(totalNumOutputChannels,
                                        juce::jmin(destination.getNumChannels(), source.getNumChannels()));
        for (int channel = 0; channel < channels; ++channel)
            destination.copyFrom(channel, startSample, source, channel, startSample, samplesInRange);
    };

    const auto addRange = [](juce::AudioBuffer<float>& destination,
                             const juce::AudioBuffer<float>& source,
                             int startSample,
                             int samplesInRange)
    {
        const int channels = juce::jmin(destination.getNumChannels(), source.getNumChannels());
        for (int channel = 0; channel < channels; ++channel)
            destination.addFrom(channel, startSample, source, channel, startSample, samplesInRange);
    };

    const auto processRange = [&](int startSample,
                                  int samplesInRange,
                                  float freqValue1,
                                  float freqValue2,
                                  float freqValue3)
    {
        freqValue1 = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, freqValue1);
        freqValue2 = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, freqValue2);
        freqValue3 = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, freqValue3);

        lowpass1.setCutoffFrequency(freqValue1);
        highpass1.setCutoffFrequency(freqValue1);
        lowpass2.setCutoffFrequency(freqValue2);
        highpass2.setCutoffFrequency(freqValue2);
        lowpass3.setCutoffFrequency(freqValue3);
        highpass3.setCutoffFrequency(freqValue3);

        if (lineNum == 2)
        {
            compensatorLP.setCutoffFrequency(freqValue2);
            compensatorHP.setCutoffFrequency(freqValue2);
        }
        else if (lineNum == 3)
        {
            compensatorLP.setCutoffFrequency(freqValue3);
            compensatorHP.setCutoffFrequency(freqValue3);
            secondCompensatorLP.setCutoffFrequency(freqValue1);
            secondCompensatorHP.setCutoffFrequency(freqValue1);
        }

        const auto blockRange = [startSample, samplesInRange](juce::AudioBuffer<float>& buffer)
        {
            return juce::dsp::AudioBlock<float>(buffer).getSubBlock(
                static_cast<size_t>(startSample), static_cast<size_t>(samplesInRange));
        };

        if (lineNum == 0)
        {
            copyRange(mBuffer1, inputBuffer, startSample, samplesInRange);
            return;
        }

        if (lineNum == 1)
        {
            copyRange(mBuffer1, inputBuffer, startSample, samplesInRange);
            copyRange(mBuffer2, inputBuffer, startSample, samplesInRange);
            auto block1 = blockRange(mBuffer1);
            auto block2 = blockRange(mBuffer2);
            lowpass1.process(juce::dsp::ProcessContextReplacing<float>(block1));
            highpass1.process(juce::dsp::ProcessContextReplacing<float>(block2));
            return;
        }

        if (lineNum == 2)
        {
            auto& highPassBuffer = mSplitTemp1;
            auto& compensatorHighBuffer = mSplitTemp2;
            copyRange(mBuffer1, inputBuffer, startSample, samplesInRange);
            copyRange(highPassBuffer, inputBuffer, startSample, samplesInRange);

            auto lowBranchBlock = blockRange(mBuffer1);
            lowpass1.process(juce::dsp::ProcessContextReplacing<float>(lowBranchBlock));
            copyRange(compensatorHighBuffer, mBuffer1, startSample, samplesInRange);

            auto compLpBlock = blockRange(mBuffer1);
            auto compHpBlock = blockRange(compensatorHighBuffer);
            compensatorLP.process(juce::dsp::ProcessContextReplacing<float>(compLpBlock));
            compensatorHP.process(juce::dsp::ProcessContextReplacing<float>(compHpBlock));
            addRange(mBuffer1, compensatorHighBuffer, startSample, samplesInRange);

            auto highPassBlock = blockRange(highPassBuffer);
            highpass1.process(juce::dsp::ProcessContextReplacing<float>(highPassBlock));
            copyRange(mBuffer2, highPassBuffer, startSample, samplesInRange);
            copyRange(mBuffer3, highPassBuffer, startSample, samplesInRange);

            auto block2 = blockRange(mBuffer2);
            auto block3 = blockRange(mBuffer3);
            lowpass2.process(juce::dsp::ProcessContextReplacing<float>(block2));
            highpass2.process(juce::dsp::ProcessContextReplacing<float>(block3));
            return;
        }

        auto& lowMidBuffer = mSplitTemp1;
        auto& highMidBuffer = mSplitTemp2;
        auto& allPassScratch = mSplitTemp3;
        copyRange(lowMidBuffer, inputBuffer, startSample, samplesInRange);
        copyRange(highMidBuffer, inputBuffer, startSample, samplesInRange);

        auto lowMidBlock = blockRange(lowMidBuffer);
        auto highMidBlock = blockRange(highMidBuffer);
        lowpass2.process(juce::dsp::ProcessContextReplacing<float>(lowMidBlock));
        highpass2.process(juce::dsp::ProcessContextReplacing<float>(highMidBlock));

        copyRange(allPassScratch, lowMidBuffer, startSample, samplesInRange);
        auto lowHalfCompLpBlock = blockRange(lowMidBuffer);
        auto lowHalfCompHpBlock = blockRange(allPassScratch);
        compensatorLP.process(juce::dsp::ProcessContextReplacing<float>(lowHalfCompLpBlock));
        compensatorHP.process(juce::dsp::ProcessContextReplacing<float>(lowHalfCompHpBlock));
        addRange(lowMidBuffer, allPassScratch, startSample, samplesInRange);

        copyRange(mBuffer1, lowMidBuffer, startSample, samplesInRange);
        copyRange(mBuffer2, lowMidBuffer, startSample, samplesInRange);
        auto block1 = blockRange(mBuffer1);
        auto block2 = blockRange(mBuffer2);
        lowpass1.process(juce::dsp::ProcessContextReplacing<float>(block1));
        highpass1.process(juce::dsp::ProcessContextReplacing<float>(block2));

        copyRange(allPassScratch, highMidBuffer, startSample, samplesInRange);
        auto highHalfCompLpBlock = blockRange(highMidBuffer);
        auto highHalfCompHpBlock = blockRange(allPassScratch);
        secondCompensatorLP.process(juce::dsp::ProcessContextReplacing<float>(highHalfCompLpBlock));
        secondCompensatorHP.process(juce::dsp::ProcessContextReplacing<float>(highHalfCompHpBlock));
        addRange(highMidBuffer, allPassScratch, startSample, samplesInRange);

        copyRange(mBuffer3, highMidBuffer, startSample, samplesInRange);
        copyRange(mBuffer4, highMidBuffer, startSample, samplesInRange);
        auto block3 = blockRange(mBuffer3);
        auto block4 = blockRange(mBuffer4);
        lowpass3.process(juce::dsp::ProcessContextReplacing<float>(block3));
        highpass3.process(juce::dsp::ProcessContextReplacing<float>(block4));
    };

    // SmoothedValue advances in samples, not callbacks. The old code used the
    // first ramp value for the complete host block and then skipped to the end,
    // turning a 1 ms divider glide into a full block-size-dependent jump. Only
    // the short active ramp uses single-sample ranges; steady state retains the
    // original whole-block processing path.
    for (int startSample = rangeStartSample;
         startSample < rangeEndSample;)
    {
        const bool crossoverIsSmoothing = (lineNum >= 1 && smoothedFreq1.isSmoothing())
                                          || (lineNum >= 2 && smoothedFreq2.isSmoothing())
                                          || (lineNum >= 3 && smoothedFreq3.isSmoothing());
        int samplesInRange = crossoverIsSmoothing
                                 ? 1
                                 : rangeEndSample - startSample;
        const float freqValue1 = smoothedFreq1.getNextValue();
        const float freqValue2 = smoothedFreq2.getNextValue();
        const float freqValue3 = smoothedFreq3.getNextValue();
        if (samplesInRange > 1)
        {
            smoothedFreq1.skip(samplesInRange - 1);
            smoothedFreq2.skip(samplesInRange - 1);
            smoothedFreq3.skip(samplesInRange - 1);
        }

        processRange(startSample, samplesInRange, freqValue1, freqValue2, freqValue3);
        startSample += samplesInRange;
    }
}

bool FireAudioProcessor::updateGlobalFilters(
    double sampleRate,
    const juce::AudioBuffer<float>& lfoOutputs,
    int lfoSampleIndex)
{
    // Get the final, modulated settings for the entire filter chain.
    auto chainSettings = getCachedChainSettingsAtSample(lfoOutputs,
                                                         lfoSampleIndex);

    // It's good practice to ensure frequencies are within a valid range.
    if (! std::isfinite(sampleRate) || sampleRate <= 0.0)
        return false;

    const auto frequencyRange = getSafeFilterFrequencyRange(sampleRate);

    // Clamp frequencies to be safe.
    chainSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.highCutFreq);

    lowcutFreqSmoother.setTargetValue(chainSettings.lowCutFreq);
    lowcutGainSmoother.setTargetValue(chainSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.setTargetValue(chainSettings.lowCutQuality);
    peakFreqSmoother.setTargetValue(chainSettings.peakFreq);
    peakGainSmoother.setTargetValue(chainSettings.peakGainInDecibels);
    peakQualitySmoother.setTargetValue(chainSettings.peakQuality);
    highcutFreqSmoother.setTargetValue(chainSettings.highCutFreq);
    highcutGainSmoother.setTargetValue(chainSettings.highCutGainInDecibels);
    highcutQualitySmoother.setTargetValue(chainSettings.highCutQuality);

    ChainSettings smoothedSettings = chainSettings;
    smoothedSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum,
                                               frequencyRange.maximum,
                                               lowcutFreqSmoother.getNextValue());
    smoothedSettings.lowCutGainInDecibels = lowcutGainSmoother.getNextValue();
    smoothedSettings.lowCutQuality = lowcutQualitySmoother.getNextValue();
    smoothedSettings.peakFreq = juce::jlimit(frequencyRange.minimum,
                                            frequencyRange.maximum,
                                            peakFreqSmoother.getNextValue());
    smoothedSettings.peakGainInDecibels = peakGainSmoother.getNextValue();
    smoothedSettings.peakQuality = peakQualitySmoother.getNextValue();
    smoothedSettings.highCutFreq = juce::jlimit(frequencyRange.minimum,
                                                frequencyRange.maximum,
                                                highcutFreqSmoother.getNextValue());
    smoothedSettings.highCutGainInDecibels = highcutGainSmoother.getNextValue();
    smoothedSettings.highCutQuality = highcutQualitySmoother.getNextValue();

    const bool sampleRateChanged = ! globalFilterCacheValid
                                || ! sameCachedValue(cachedGlobalFilterSampleRate, sampleRate);
    const bool lowCutChanged = sampleRateChanged
                            || ! sameLowCutSettings(cachedGlobalFilterSettings, smoothedSettings);
    const bool peakChanged = sampleRateChanged
                          || ! samePeakSettings(cachedGlobalFilterSettings, smoothedSettings);
    const bool highCutChanged = sampleRateChanged
                             || ! sameHighCutSettings(cachedGlobalFilterSettings, smoothedSettings);

    if (! lowCutChanged && ! peakChanged && ! highCutChanged)
        return false;

    // During a parameter ramp this function is called once per processed
    // sample. All coefficient factories below use fixed-size arrays and the
    // filters have already been prepared as biquads, so this remains
    // allocation-free on the audio thread.
    if (lowCutChanged)
        updateLowCutFilters(smoothedSettings, sampleRate);
    if (peakChanged)
        updatePeakFilter(smoothedSettings, sampleRate);
    if (highCutChanged)
        updateHighCutFilters(smoothedSettings, sampleRate);

    if (lowCutChanged)
    {
        cachedGlobalFilterSettings.lowCutFreq = smoothedSettings.lowCutFreq;
        cachedGlobalFilterSettings.lowCutGainInDecibels = smoothedSettings.lowCutGainInDecibels;
        cachedGlobalFilterSettings.lowCutQuality = smoothedSettings.lowCutQuality;
        cachedGlobalFilterSettings.lowCutSlope = smoothedSettings.lowCutSlope;
        cachedGlobalFilterSettings.lowCutBypassed = smoothedSettings.lowCutBypassed;
    }
    if (peakChanged)
    {
        cachedGlobalFilterSettings.peakFreq = smoothedSettings.peakFreq;
        cachedGlobalFilterSettings.peakGainInDecibels = smoothedSettings.peakGainInDecibels;
        cachedGlobalFilterSettings.peakQuality = smoothedSettings.peakQuality;
        cachedGlobalFilterSettings.peakBypassed = smoothedSettings.peakBypassed;
    }
    if (highCutChanged)
    {
        cachedGlobalFilterSettings.highCutFreq = smoothedSettings.highCutFreq;
        cachedGlobalFilterSettings.highCutGainInDecibels = smoothedSettings.highCutGainInDecibels;
        cachedGlobalFilterSettings.highCutQuality = smoothedSettings.highCutQuality;
        cachedGlobalFilterSettings.highCutSlope = smoothedSettings.highCutSlope;
        cachedGlobalFilterSettings.highCutBypassed = smoothedSettings.highCutBypassed;
    }
    cachedGlobalFilterSampleRate = sampleRate;
    globalFilterCacheValid = true;
    return true;
}

float FireAudioProcessor::getTotalLatency() const
{
    return totalLatency.load(std::memory_order_relaxed);
}

void FireAudioProcessor::processMultiBandRange(
    juce::AudioBuffer<float>& wetBuffer,
    juce::AudioBuffer<float>& delayMatchedDryBufferForRange,
    const std::array<juce::AudioBuffer<float>*, 4>& bandBuffers,
    const juce::AudioBuffer<float>& lfoOutputs,
    const HqCallbackContext& callbackContext,
    bool useHQ,
    bool updateReductionMeter)
{
    updateBandSoloGainEnvelope(wetBuffer.getNumSamples(),
                               useHQ,
                               callbackContext.soloState,
                               callbackContext.anySoloActive);

    // Solo must affect both sides of the global dry/wet mix. Otherwise the
    // un-soloed bands leak back through the dry side whenever Mix is below 1.
    sumBands(delayMatchedDryBufferForRange, bandBuffers, false, false);

    for (int i = 0; i < numBands; ++i)
    {
        auto* band = bands[static_cast<size_t>(i)].get();
        auto* bandBuffer = bandBuffers[static_cast<size_t>(i)];
        if (band == nullptr || bandBuffer == nullptr)
            continue;

        auto params = callbackContext.bandParameters[static_cast<size_t>(i)];
        params.isHQ = useHQ;
        band->process(*bandBuffer,
                      params,
                      lfoOutputs,
                      callbackContext.bandInputPeaks[static_cast<size_t>(i)],
                      updateReductionMeter);
    }

    sumBands(wetBuffer, bandBuffers, false, true);
}

void FireAudioProcessor::applyGlobalEffects(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& lfoOutputs, double sampleRate)
{
    // ==============================================================================
    // 1. Global Filter Processing (Block-based)
    // ==============================================================================
    {
        const bool filterEnabled = loadCachedParameter(filterEnabledParameter) > 0.5f;
        globalFilterMixer.setWetMixProportion(filterEnabled ? 1.0f : 0.0f);
        if (! globalFilterMixerPrimed)
            globalFilterMixer.reset();
        globalFilterMixerPrimed = true;
        globalFilterMixer.pushDrySamples(juce::dsp::AudioBlock<float>(buffer));

        auto block = juce::dsp::AudioBlock<float>(buffer);

        const float lowCutMix = loadCachedParameter(filterParameterCache.lowCutBypassed) > 0.5f
                                    ? 0.0f
                                    : 1.0f;
        const float peakMix = loadCachedParameter(filterParameterCache.peakBypassed) > 0.5f
                                  ? 0.0f
                                  : 1.0f;
        const float highCutMix = loadCachedParameter(filterParameterCache.highCutBypassed) > 0.5f
                                     ? 0.0f
                                     : 1.0f;
        globalFilterStageMix[lowCutStage].setTargetValue(lowCutMix);
        globalFilterStageMix[peakStage].setTargetValue(peakMix);
        globalFilterStageMix[highCutStage].setTargetValue(highCutMix);
        globalFilterStageMix[lowCutQStage].setTargetValue(lowCutMix);
        globalFilterStageMix[highCutQStage].setTargetValue(highCutMix);

        const auto processRange = [&] (int startSample, int numSamples)
        {
            processCutFilterStage(leftChain.get<ChainPositions::LowCut>(),
                                  rightChain.get<ChainPositions::LowCut>(),
                                  lowCutSlopeTransition,
                                  block,
                                  globalFilterStageMix[lowCutStage],
                                  startSample,
                                  numSamples);
            processGlobalFilterStage(leftChain.get<ChainPositions::Peak>(),
                                     rightChain.get<ChainPositions::Peak>(),
                                     block,
                                     globalFilterStageDryBuffer,
                                     globalFilterStageMix[peakStage],
                                     startSample,
                                     numSamples);
            processCutFilterStage(leftChain.get<ChainPositions::HighCut>(),
                                  rightChain.get<ChainPositions::HighCut>(),
                                  highCutSlopeTransition,
                                  block,
                                  globalFilterStageMix[highCutStage],
                                  startSample,
                                  numSamples);
            processGlobalFilterStage(leftChain.get<ChainPositions::LowCutQ>(),
                                     rightChain.get<ChainPositions::LowCutQ>(),
                                     block,
                                     globalFilterStageDryBuffer,
                                     globalFilterStageMix[lowCutQStage],
                                     startSample,
                                     numSamples);
            processGlobalFilterStage(leftChain.get<ChainPositions::HighCutQ>(),
                                     rightChain.get<ChainPositions::HighCutQ>(),
                                     block,
                                     globalFilterStageDryBuffer,
                                     globalFilterStageMix[highCutQStage],
                                     startSample,
                                     numSamples);
        };

        const auto isFilterSmoothing = [&]
        {
            return lowcutFreqSmoother.isSmoothing()
                || lowcutGainSmoother.isSmoothing()
                || lowcutQualitySmoother.isSmoothing()
                || peakFreqSmoother.isSmoothing()
                || peakGainSmoother.isSmoothing()
                || peakQualitySmoother.isSmoothing()
                || highcutFreqSmoother.isSmoothing()
                || highcutGainSmoother.isSmoothing()
                || highcutQualitySmoother.isSmoothing();
        };

        int processedSamples = 0;
        const bool filterHasModulation = hasActiveFilterModulation();
        const bool coefficientsChanged = updateGlobalFilters(sampleRate,
                                                              lfoOutputs,
                                                              0);
        const bool needsPerSampleUpdates = filterHasModulation || isFilterSmoothing();

        if ((coefficientsChanged || needsPerSampleUpdates) && buffer.getNumSamples() > 0)
        {
            processRange(0, 1);
            processedSamples = 1;

            while (processedSamples < buffer.getNumSamples()
                   && (filterHasModulation || isFilterSmoothing()))
            {
                updateGlobalFilters(sampleRate,
                                    lfoOutputs,
                                    processedSamples);
                processRange(processedSamples, 1);
                ++processedSamples;
            }
        }

        if (processedSamples < buffer.getNumSamples())
            processRange(processedSamples, buffer.getNumSamples() - processedSamples);

        globalFilterMixer.mixWetSamples(block);
    }

    // ==============================================================================
    // 2. Global Gain Processing
    // ==============================================================================

    // a. Prepare the "recipe" for the global output gain.
    ModulatedValueProvider globalGainProvider;
    if (globalOutputParameter.ranged == nullptr)
        return;

    globalGainProvider.baseValue = loadCachedParameter(globalOutputParameter);
    globalGainProvider.range = globalOutputParameter.ranged->getNormalisableRange();

    int globalGainLfoSourceIndex = -1;
    LfoManager::AudioThreadRoutingInfo routingInfo;
    if (lfoManager->getAudioThreadRoutingInfo(globalOutputParameter.ranged, routingInfo))
    {
        const int sourceIndex = routingInfo.sourceLfoIndex;
        if (juce::isPositiveAndBelow(sourceIndex, lfoOutputs.getNumChannels()))
        {
            globalGainProvider.lfoSignal = lfoOutputs.getReadPointer(sourceIndex);
            globalGainProvider.modulationDepth = routingInfo.depth;
            globalGainProvider.isBipolar = routingInfo.isBipolar;
            globalGainLfoSourceIndex = sourceIndex;
        }
    }

    // b. Apply the gain using our new, clean helper function.
    applyGain(buffer,
              globalGainProvider,
              gainProcessorGlobal,
              globalOutputGainTransition,
              globalGainLfoSourceIndex);
}

void FireAudioProcessor::applyDownsamplingEffect(
    juce::AudioBuffer<float>& buffer,
    const juce::AudioBuffer<float>& lfoOutputs)
{
    const bool isActive = loadCachedParameter(downsampleEnabledParameter) > 0.5f;
    if (! downsamplingWasActive)
    {
        downsampleSamplesRemaining.fill(0);
        downsampleHeldSamples.fill(0.0f);
        downsamplingWasActive = true;
    }

    // --- 1. Prepare Dry Signal & Mixer ---
    // A copy of the original signal is needed for the dry/wet mix.
    lofiDryBuffer.makeCopyOf(buffer, true);

    const auto configureProvider = [this, &buffer, &lfoOutputs](
                                       const CachedParameter& parameter,
                                       ModulatedValueProvider& provider,
                                       int* sourceIndex)
    {
        if (sourceIndex != nullptr)
            *sourceIndex = -1;

        const float defaultValue = parameter.ranged != nullptr
                                       ? parameter.ranged->convertFrom0to1(
                                             parameter.ranged->getDefaultValue())
                                       : 0.0f;
        provider.baseValue = loadCachedParameter(parameter, defaultValue);
        if (parameter.ranged == nullptr)
            return false;

        provider.range = parameter.ranged->getNormalisableRange();
        LfoManager::AudioThreadRoutingInfo routingInfo;
        const bool hasCompleteLfoBlock = lfoOutputs.getNumSamples()
                                         >= buffer.getNumSamples();
        if (! hasCompleteLfoBlock
            || ! lfoManager->getAudioThreadRoutingInfo(parameter.ranged,
                                                        routingInfo)
            || std::abs(routingInfo.depth) <= 1.0e-6f
            || ! juce::isPositiveAndBelow(routingInfo.sourceLfoIndex,
                                           lfoOutputs.getNumChannels()))
        {
            return false;
        }

        provider.lfoSignal = lfoOutputs.getReadPointer(
            routingInfo.sourceLfoIndex);
        provider.modulationDepth = routingInfo.depth;
        provider.isBipolar = routingInfo.isBipolar;
        if (sourceIndex != nullptr)
            *sourceIndex = routingInfo.sourceLfoIndex;
        return true;
    };

    ModulatedValueProvider rateProvider;
    ModulatedValueProvider bitsProvider;
    ModulatedValueProvider jitterProvider;
    ModulatedValueProvider mixProvider;
    const bool hasRateModulation = configureProvider(downsampleRateParameter,
                                                      rateProvider,
                                                      nullptr);
    const bool hasBitsModulation = configureProvider(bitDepthParameter,
                                                      bitsProvider,
                                                      nullptr);
    const bool hasJitterModulation = configureProvider(jitterParameter,
                                                        jitterProvider,
                                                        nullptr);
    int mixLfoSourceIndex = -1;
    configureProvider(downsampleMixParameter,
                      mixProvider,
                      &mixLfoSourceIndex);

    // Keep the sample-and-hold path and all mixer control state advancing while
    // bypassed. Static mix/enable changes retain the legacy 50 ms ramp, while a
    // routed LFO follows its trajectory without being low-pass filtered.
    lofiMixer.pushDrySamples(juce::dsp::AudioBlock<float>(lofiDryBuffer));

    // --- 2. Process Audio ---
    const int channelsToProcess = juce::jmin(buffer.getNumChannels(),
                                              static_cast<int>(downsamplingStateChannels));
    const int staticBits = juce::jlimit(4, 32,
                                       juce::roundToInt(bitsProvider.baseValue));
    const float staticRate = std::isfinite(rateProvider.baseValue)
                                 ? juce::jlimit(1.0f, 64.0f,
                                                rateProvider.baseValue)
                                 : 1.0f;
    const float staticJitter = std::isfinite(jitterProvider.baseValue)
                                   ? juce::jlimit(0.0f, 1.0f,
                                                  jitterProvider.baseValue)
                                   : 0.0f;
    int cachedBits = -1;
    float quantisationStep = 0.0f;

    // Iterate samples first so jitter consumes the same random sequence
    // regardless of how the host partitions the stream into blocks.
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const int bits = hasBitsModulation
                             ? juce::jlimit(4, 32,
                                            juce::roundToInt(bitsProvider.get(sample)))
                             : staticBits;
        if (bits != cachedBits)
        {
            cachedBits = bits;
            quantisationStep = bits < 32
                                   ? 2.0f / std::ldexp(1.0f, bits)
                                   : 0.0f;
        }

        bool needsNewHeldSample = false;
        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            if (downsampleSamplesRemaining[static_cast<size_t>(channel)] <= 0)
            {
                needsNewHeldSample = true;
                break;
            }
        }

        // Rate and Jitter define the duration of a newly captured sample. A
        // parameter change must not retroactively resize the hold already in
        // progress, so sample their LFOs only at capture instants.
        float currentRate = staticRate;
        float currentJitter = staticJitter;
        if (needsNewHeldSample)
        {
            if (hasRateModulation)
            {
                const float value = rateProvider.get(sample);
                currentRate = std::isfinite(value)
                                  ? juce::jlimit(1.0f, 64.0f, value)
                                  : staticRate;
            }
            if (hasJitterModulation)
            {
                const float value = jitterProvider.get(sample);
                currentJitter = std::isfinite(value)
                                    ? juce::jlimit(0.0f, 1.0f, value)
                                    : staticJitter;
            }
        }

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            const auto stateIndex = static_cast<size_t>(channel);

            // --- Rate Reduction (Sample & Hold) ---
            if (downsampleSamplesRemaining[stateIndex] <= 0)
            {
                // It's time to grab a new sample.
                downsampleHeldSamples[stateIndex] = channelData[sample];

                // Determine the hold duration for this new sample.
                float currentRateReduce = currentRate;

                // Apply Jitter if the parameter is active.
                if (currentJitter > 0.0f)
                {
                    // Introduce a random variation to the hold time.
                    // random.nextFloat() returns [0, 1]. We map it to [-1, 1].
                    float randomFactor = 1.0f
                                         + (random.nextFloat() * 2.0f - 1.0f)
                                               * currentJitter;
                    currentRateReduce *= randomFactor;
                }

                // Set how many samples we need to hold for. Must be at least 1.
                downsampleSamplesRemaining[stateIndex] = juce::jmax(1, static_cast<int>(currentRateReduce));
            }

            // Output the held sample.
            channelData[sample] = downsampleHeldSamples[stateIndex];
            --downsampleSamplesRemaining[stateIndex];

            // --- Bit Crushing ---
            // Apply this effect after the sample has been selected (or held).
            if (bits < 32)
                channelData[sample] = quantisationStep
                                      * std::floor(channelData[sample] / quantisationStep + 0.5f);
        }
    }

    // --- 3. Mix with Dry Signal ---
    // Finally, mix the processed (wet) buffer with the original (dry) buffer.
    auto wetBlock = juce::dsp::AudioBlock<float>(buffer);
    lofiMixer.mixWetSamples(wetBlock,
                            mixProvider,
                            mixProvider.baseValue,
                            mixLfoSourceIndex,
                            isActive);
}

void FireAudioProcessor::resetDownsamplingState() noexcept
{
    downsampleSamplesRemaining.fill(0);
    downsampleHeldSamples.fill(0.0f);
    downsamplingWasActive = false;
}

void FireAudioProcessor::primeLatencyMatchedBypass(
    juce::AudioBuffer<float>& inputBuffer,
    bool useHQ)
{
    if (inputBuffer.getNumChannels() == 0 || inputBuffer.getNumSamples() == 0)
        return;

    mWetBuffer.makeCopyOf(inputBuffer, true);
    const float bypassLatency = useHQ
                                    ? preparedHqLatency.load(std::memory_order_acquire)
                                    : static_cast<float>(juce::roundToInt(
                                          preparedHqLatency.load(
                                              std::memory_order_acquire)));
    bypassDelayMixer.setWetLatency(bypassLatency);
    bypassDelayMixer.setWetMixProportion(0.0f);
    bypassDelayMixer.pushDrySamples(juce::dsp::AudioBlock<float>(inputBuffer));
    bypassDelayMixer.mixWetSamples(juce::dsp::AudioBlock<float>(mWetBuffer));
}

void FireAudioProcessor::processLatencyMatchedBypass(
    juce::AudioBuffer<float>& buffer,
    bool useHQ)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    const float bypassLatency = useHQ
                                    ? preparedHqLatency.load(std::memory_order_acquire)
                                    : static_cast<float>(juce::roundToInt(
                                          preparedHqLatency.load(
                                              std::memory_order_acquire)));
    bypassDelayMixer.setWetLatency(bypassLatency);
    bypassDelayMixer.setWetMixProportion(0.0f);
    auto block = juce::dsp::AudioBlock<float>(buffer);
    const int rangeCapacity = juce::jmax(1,
                                         preparedProcessingBlockCapacity);
    int sampleOffset = 0;
    while (sampleOffset < buffer.getNumSamples())
    {
        const int samplesInRange = juce::jmin(
            rangeCapacity, buffer.getNumSamples() - sampleOffset);
        auto range = block.getSubBlock(static_cast<size_t>(sampleOffset),
                                       static_cast<size_t>(samplesInRange));
        bypassDelayMixer.pushDrySamples(range);
        bypassDelayMixer.mixWetSamples(range);
        sampleOffset += samplesInRange;
    }
}

void FireAudioProcessor::advanceNonHqOutputDelay(
    const juce::AudioBuffer<float>& inputBuffer)
{
    for (int sample = 0; sample < inputBuffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < inputBuffer.getNumChannels(); ++channel)
        {
            nonHqOutputDelay.pushSample(channel,
                                       inputBuffer.getSample(channel, sample));
            juce::ignoreUnused(nonHqOutputDelay.popSample(channel));
        }
    }
}

void FireAudioProcessor::applyNonHqOutputDelay(juce::AudioBuffer<float>& buffer)
{
    auto block = juce::dsp::AudioBlock<float>(buffer);
    nonHqOutputDelay.process(juce::dsp::ProcessContextReplacing<float>(block));
}

bool FireAudioProcessor::sameTopologyIdentity(
    const MultibandTopologySnapshot& first,
    const MultibandTopologySnapshot& second) noexcept
{
    return first.numBands == second.numBands
        && first.publicationSequence == second.publicationSequence;
}

bool FireAudioProcessor::hasPendingTopologyChange() const noexcept
{
    return pendingMultibandTopologySnapshotInitialised
        && activeMultibandTopologySnapshotInitialised
        && ! sameTopologyIdentity(pendingMultibandTopologySnapshot,
                                  activeMultibandTopologySnapshot);
}

void FireAudioProcessor::snapTopologyTransitionToActive() noexcept
{
    if (activeMultibandTopologySnapshotInitialised)
    {
        pendingMultibandTopologySnapshot = activeMultibandTopologySnapshot;
        pendingMultibandTopologySnapshotInitialised = true;
    }

    topologyTransitionPhase = TopologyTransitionPhase::steady;
    topologyTransitionGain = 1.0f;
    topologyTransitionGainStep = 0.0f;
    topologyTransitionRampRemaining = 0;
    topologyTransitionWarmupRemaining = 0;
    topologyPendingChangedThisCallback = false;
}

void FireAudioProcessor::startTopologyTransitionRamp(
    float target,
    TopologyTransitionPhase phase) noexcept
{
    target = juce::jlimit(0.0f, 1.0f, target);
    const float difference = target - topologyTransitionGain;
    if (std::abs(difference) <= std::numeric_limits<float>::epsilon())
    {
        topologyTransitionGain = target;
        topologyTransitionGainStep = 0.0f;
        topologyTransitionRampRemaining = 0;
        topologyTransitionPhase = target >= 1.0f
                                      ? TopologyTransitionPhase::steady
                                      : phase;
        return;
    }

    topologyTransitionRampRemaining = juce::jmax(
        1, topologyTransitionRampSamples);
    topologyTransitionGainStep = difference
                               / static_cast<float>(
                                     topologyTransitionRampRemaining);
    topologyTransitionPhase = phase;
}

void FireAudioProcessor::beginTopologyTransitionCallback() noexcept
{
    if (! pendingMultibandTopologySnapshotInitialised)
    {
        snapTopologyTransitionToActive();
        return;
    }

    switch (topologyTransitionPhase)
    {
        case TopologyTransitionPhase::steady:
            if (hasPendingTopologyChange())
                startTopologyTransitionRamp(
                    0.0f, TopologyTransitionPhase::fadingOut);
            break;

        case TopologyTransitionPhase::fadingOut:
            if (! hasPendingTopologyChange())
                startTopologyTransitionRamp(
                    1.0f, TopologyTransitionPhase::fadingIn);
            break;

        case TopologyTransitionPhase::warmingUp:
            // Require one stable muted window after the latest complete
            // publication, but never repeatedly reset the large DSP graph for
            // rapid add/delete automation.
            if (topologyPendingChangedThisCallback)
                topologyTransitionWarmupRemaining =
                    topologyTransitionWarmupSamples;
            break;

        case TopologyTransitionPhase::fadingIn:
            if (hasPendingTopologyChange())
            {
                if (topologyTransitionGain <= 0.0f)
                {
                    topologyTransitionWarmupRemaining =
                        topologyTransitionWarmupSamples;
                    topologyTransitionPhase =
                        TopologyTransitionPhase::warmingUp;
                }
                else
                {
                    startTopologyTransitionRamp(
                        0.0f, TopologyTransitionPhase::fadingOut);
                }
            }
            break;
    }
}

void FireAudioProcessor::applyTopologyTransitionRamp(
    juce::AudioBuffer<float>& buffer) noexcept
{
    auto* const* channels = buffer.getArrayOfWritePointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float gain = juce::jlimit(
            0.0f, 1.0f, topologyTransitionGain);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            channels[channel][sample] *= gain;

        topologyTransitionGain += topologyTransitionGainStep;
        if (topologyTransitionRampRemaining > 0)
            --topologyTransitionRampRemaining;
    }
}

bool FireAudioProcessor::commitPendingTopologySnapshot() noexcept
{
    // pendingMultibandTopologySnapshot is written only after a stable even
    // publication and a successful routing refresh. It remains a coherent,
    // consumable target if a later callback temporarily misses the LFO
    // try-lock; tying commit to that later refresh could mute indefinitely.
    if (! hasPendingTopologyChange())
        return false;

    activeMultibandTopologySnapshot = pendingMultibandTopologySnapshot;
    numBands = activeMultibandTopologySnapshot.numBands;
    activeCrossovers = numBands - 1;

    resetMultibandProcessingState(
        &activeMultibandTopologySnapshot.callbackContext);
    snapCrossoverSmoothers(
        activeMultibandTopologySnapshot.crossoverFrequencies);
    snapBandSoloGains(
        numBands,
        activeMultibandTopologySnapshot.callbackContext);
    appliedMultibandTopologyResetGeneration =
        activeMultibandTopologySnapshot.publicationSequence;

    pendingMultibandTopologySnapshot = activeMultibandTopologySnapshot;
    topologyPendingChangedThisCallback = false;
    return true;
}

void FireAudioProcessor::snapHqTransitionToParameter() noexcept
{
    activeHqMode = loadCachedParameter(hqParameter) > 0.5f;
    pendingHqMode = activeHqMode;
    hqTransitionInitialised = true;
    hqTransitionPhase = HqTransitionPhase::steady;
    hqTransitionGain = 1.0f;
    hqTransitionGainStep = 0.0f;
    hqTransitionRampRemaining = 0;
    hqTransitionWarmupRemaining = 0;
}

void FireAudioProcessor::resetHqQualityPathState() noexcept
{
    for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
        if (auto* band = bands[static_cast<size_t>(bandIndex)].get())
            band->resetQualityTransitionState();

    // Global/Band/Solo/bypass mixers all remain balanced and warm in both
    // modes. Only the non-HQ output delay changes what signal feeds its
    // history (raw shadow in HQ, processed output in base), so rebuild that
    // path under the muted warm-up instead of reviving incompatible samples.
    nonHqOutputDelay.reset();
}

void FireAudioProcessor::startHqTransitionRamp(
    float target,
    HqTransitionPhase phase) noexcept
{
    target = juce::jlimit(0.0f, 1.0f, target);
    const float difference = target - hqTransitionGain;
    if (std::abs(difference) <= std::numeric_limits<float>::epsilon())
    {
        hqTransitionGain = target;
        hqTransitionGainStep = 0.0f;
        hqTransitionRampRemaining = 0;
        hqTransitionPhase = target >= 1.0f
                                ? HqTransitionPhase::steady
                                : phase;
        return;
    }

    hqTransitionRampRemaining = juce::jmax(1, hqTransitionRampSamples);
    hqTransitionGainStep = difference
                         / static_cast<float>(hqTransitionRampRemaining);
    hqTransitionPhase = phase;
}

void FireAudioProcessor::beginHqTransitionCallback(bool requestedHq) noexcept
{
    if (! hqTransitionInitialised)
    {
        activeHqMode = requestedHq;
        pendingHqMode = requestedHq;
        hqTransitionInitialised = true;
        hqTransitionPhase = HqTransitionPhase::steady;
        hqTransitionGain = 1.0f;
        hqTransitionGainStep = 0.0f;
        hqTransitionRampRemaining = 0;
        hqTransitionWarmupRemaining = 0;
        return;
    }

    const bool pendingRequestChanged = requestedHq != pendingHqMode;
    pendingHqMode = requestedHq;
    switch (hqTransitionPhase)
    {
        case HqTransitionPhase::steady:
            if (requestedHq != activeHqMode)
                startHqTransitionRamp(0.0f,
                                      HqTransitionPhase::fadingOut);
            break;

        case HqTransitionPhase::fadingOut:
            // A request returning to the audible mode before silence cancels
            // the switch from the current gain instead of introducing a jump.
            if (requestedHq == activeHqMode)
                startHqTransitionRamp(1.0f,
                                      HqTransitionPhase::fadingIn);
            break;

        case HqTransitionPhase::warmingUp:
            // A rapidly changing request must not briefly escape the muted
            // warm-up merely because the pending value happens to equal the
            // active mode when the current warm-up counter reaches zero.
            // Re-arm only the cheap stability window here; the quality path
            // itself is reset at most once per completed window below.
            if (pendingRequestChanged)
                hqTransitionWarmupRemaining = hqTransitionWarmupSamples;
            break;

        case HqTransitionPhase::fadingIn:
            if (requestedHq != activeHqMode)
            {
                if (hqTransitionGain <= 0.0f)
                {
                    activeHqMode = pendingHqMode;
                    resetHqQualityPathState();
                    hqTransitionWarmupRemaining = hqTransitionWarmupSamples;
                    hqTransitionPhase = HqTransitionPhase::warmingUp;
                }
                else
                {
                    startHqTransitionRamp(0.0f,
                                          HqTransitionPhase::fadingOut);
                }
            }
            break;
    }
}

void FireAudioProcessor::applyHqTransitionRamp(
    juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    auto* const* channels = buffer.getArrayOfWritePointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float gain = juce::jlimit(0.0f, 1.0f, hqTransitionGain);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            channels[channel][sample] *= gain;

        hqTransitionGain += hqTransitionGainStep;
        if (hqTransitionRampRemaining > 0)
            --hqTransitionRampRemaining;
    }
}

void FireAudioProcessor::processActiveHqRange(
    juce::AudioBuffer<float>& buffer,
    juce::AudioBuffer<float>& delayMatchedDryBufferForRange,
    const std::array<juce::AudioBuffer<float>*, 4>& bandBuffers,
    const juce::AudioBuffer<float>& lfoOutputs,
    double sampleRate,
    bool useHQ,
    bool updateReductionMeter,
    const HqCallbackContext& callbackContext,
    bool applyFinalNonHqDelay,
    bool primeBypassDelay)
{
    if (useHQ)
        advanceNonHqOutputDelay(buffer);

    // Keep the host-bypass delay line on the same raw timeline as the audible
    // path before this range is overwritten in place.
    if (primeBypassDelay)
        primeLatencyMatchedBypass(buffer, useHQ);

    processMultiBandRange(buffer,
                          delayMatchedDryBufferForRange,
                          bandBuffers,
                          lfoOutputs,
                          callbackContext,
                          useHQ,
                          updateReductionMeter);
    applyDownsamplingEffect(buffer, lfoOutputs);
    applyGlobalEffects(buffer, lfoOutputs, sampleRate);
    applyGlobalMix(buffer,
                   delayMatchedDryBufferForRange,
                   lfoOutputs,
                   useHQ);

    if (! useHQ && applyFinalNonHqDelay)
        applyNonHqOutputDelay(buffer);
}

void FireAudioProcessor::processHqTransitionBlock(
    juce::AudioBuffer<float>& buffer,
    const juce::AudioBuffer<float>& lfoOutputs,
    double sampleRate,
    bool requestedHq,
    const HqCallbackContext& callbackContext,
    bool primeBypassDelay)
{
    beginHqTransitionCallback(requestedHq);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    int sampleOffset = 0;

    while (sampleOffset < numSamples)
    {
        const auto phaseForRange = hqTransitionPhase;
        int samplesInRange = numSamples - sampleOffset;
        if (phaseForRange == HqTransitionPhase::fadingOut
            || phaseForRange == HqTransitionPhase::fadingIn)
        {
            samplesInRange = juce::jmin(samplesInRange,
                                        hqTransitionRampRemaining);
        }
        else if (phaseForRange == HqTransitionPhase::warmingUp)
        {
            samplesInRange = juce::jmin(samplesInRange,
                                        hqTransitionWarmupRemaining);
        }

        // Every phase is entered with a positive counter. Guarding here keeps
        // malformed restored state from turning the audio thread into a loop.
        if (samplesInRange <= 0)
        {
            if (phaseForRange == HqTransitionPhase::warmingUp)
            {
                hqTransitionWarmupRemaining = hqTransitionWarmupSamples;
                samplesInRange = juce::jmin(numSamples - sampleOffset,
                                            hqTransitionWarmupRemaining);
            }
            else
            {
                startHqTransitionRamp(
                    phaseForRange == HqTransitionPhase::fadingOut ? 0.0f : 1.0f,
                    phaseForRange);
                samplesInRange = juce::jmin(numSamples - sampleOffset,
                                            hqTransitionRampRemaining);
            }
        }

        // JUCE DryWetMixer owns a finite FIFO sized during prepare(). Hosts
        // may still deliver a callback larger than their advertised maximum,
        // so keep every paired global mixer push/mix within prepared capacity.
        samplesInRange = juce::jmin(
            samplesInRange,
            juce::jmax(1, preparedProcessingBlockCapacity));

        juce::AudioBuffer<float> audioRange(buffer.getArrayOfWritePointers(),
                                            numChannels,
                                            sampleOffset,
                                            samplesInRange);
        juce::AudioBuffer<float> dryRange(
            delayMatchedDryBuffer.getArrayOfWritePointers(),
            numChannels,
            sampleOffset,
            samplesInRange);
        juce::AudioBuffer<float> lfoRange(
            lfoOutputBuffer.getArrayOfWritePointers(),
            lfoOutputs.getNumChannels(),
            sampleOffset,
            samplesInRange);
        juce::AudioBuffer<float> band1(mBuffer1.getArrayOfWritePointers(),
                                       numChannels,
                                       sampleOffset,
                                       samplesInRange);
        juce::AudioBuffer<float> band2(mBuffer2.getArrayOfWritePointers(),
                                       numChannels,
                                       sampleOffset,
                                       samplesInRange);
        juce::AudioBuffer<float> band3(mBuffer3.getArrayOfWritePointers(),
                                       numChannels,
                                       sampleOffset,
                                       samplesInRange);
        juce::AudioBuffer<float> band4(mBuffer4.getArrayOfWritePointers(),
                                       numChannels,
                                       sampleOffset,
                                       samplesInRange);
        const std::array<juce::AudioBuffer<float>*, 4> bandRanges {
            &band1, &band2, &band3, &band4
        };

        processActiveHqRange(audioRange,
                             dryRange,
                             bandRanges,
                             lfoRange,
                             sampleRate,
                             activeHqMode,
                             true,
                             callbackContext,
                             true,
                             primeBypassDelay);

        if (phaseForRange == HqTransitionPhase::fadingOut
            || phaseForRange == HqTransitionPhase::fadingIn)
        {
            applyHqTransitionRamp(audioRange);
        }
        else if (phaseForRange == HqTransitionPhase::warmingUp)
        {
            audioRange.clear();
            hqTransitionWarmupRemaining -= samplesInRange;
        }

        sampleOffset += samplesInRange;

        if (phaseForRange == HqTransitionPhase::fadingOut
            && hqTransitionRampRemaining == 0)
        {
            hqTransitionGain = 0.0f;
            hqTransitionGainStep = 0.0f;
            activeHqMode = pendingHqMode;
            resetHqQualityPathState();
            hqTransitionWarmupRemaining = hqTransitionWarmupSamples;
            hqTransitionPhase = HqTransitionPhase::warmingUp;
        }
        else if (phaseForRange == HqTransitionPhase::fadingIn
                 && hqTransitionRampRemaining == 0)
        {
            hqTransitionGain = 1.0f;
            hqTransitionGainStep = 0.0f;
            hqTransitionPhase = HqTransitionPhase::steady;
        }
        else if (phaseForRange == HqTransitionPhase::warmingUp
                 && hqTransitionWarmupRemaining <= 0)
        {
            hqTransitionWarmupRemaining = 0;
            if (pendingHqMode != activeHqMode)
            {
                // Rapid automation is coalesced while silent. The expensive
                // oversampling reset can happen at most once per completed
                // warm-up window, never once per tiny host callback.
                activeHqMode = pendingHqMode;
                resetHqQualityPathState();
                hqTransitionWarmupRemaining = hqTransitionWarmupSamples;
            }
            else
            {
                hqTransitionGain = 0.0f;
                startHqTransitionRamp(1.0f,
                                      HqTransitionPhase::fadingIn);
            }
        }
    }
}

void FireAudioProcessor::processTopologyTransitionBlock(
    juce::AudioBuffer<float>& buffer,
    const juce::AudioBuffer<float>& lfoOutputs,
    double sampleRate,
    bool requestedHq,
    bool primeBypassDelay)
{
    beginTopologyTransitionCallback();

    // A topology reset and an HQ reset must never be nested. Keep the current
    // quality graph coherent for this complete transition; the latest HQ
    // request is serviced by the existing state machine on a later callback.
    pendingHqMode = requestedHq;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    mBuffer1.clear();
    mBuffer2.clear();
    mBuffer3.clear();
    mBuffer4.clear();

    int sampleOffset = 0;
    while (sampleOffset < numSamples)
    {
        // Complete state boundaries before rendering the next sample. The
        // destructive topology reset can therefore only occur at exact zero.
        if (topologyTransitionPhase == TopologyTransitionPhase::fadingOut
            && topologyTransitionRampRemaining <= 0)
        {
            topologyTransitionGain = 0.0f;
            topologyTransitionGainStep = 0.0f;
            if (hasPendingTopologyChange())
            {
                commitPendingTopologySnapshot();
                topologyTransitionWarmupRemaining =
                    topologyTransitionWarmupSamples;
                topologyTransitionPhase =
                    TopologyTransitionPhase::warmingUp;
            }
            else
            {
                startTopologyTransitionRamp(
                    1.0f, TopologyTransitionPhase::fadingIn);
            }
        }
        else if (topologyTransitionPhase == TopologyTransitionPhase::warmingUp
                 && topologyTransitionWarmupRemaining <= 0)
        {
            if (hasPendingTopologyChange())
            {
                commitPendingTopologySnapshot();
                topologyTransitionWarmupRemaining =
                    topologyTransitionWarmupSamples;
            }
            else
            {
                topologyTransitionGain = 0.0f;
                startTopologyTransitionRamp(
                    1.0f, TopologyTransitionPhase::fadingIn);
            }
        }
        else if (topologyTransitionPhase == TopologyTransitionPhase::fadingIn
                 && topologyTransitionRampRemaining <= 0)
        {
            topologyTransitionGain = 1.0f;
            topologyTransitionGainStep = 0.0f;
            topologyTransitionPhase = TopologyTransitionPhase::steady;
        }

        const auto phaseForRange = topologyTransitionPhase;
        int samplesInRange = numSamples - sampleOffset;
        if (phaseForRange == TopologyTransitionPhase::fadingOut
            || phaseForRange == TopologyTransitionPhase::fadingIn)
        {
            samplesInRange = juce::jmin(
                samplesInRange,
                juce::jmax(1, topologyTransitionRampRemaining));
        }
        else if (phaseForRange == TopologyTransitionPhase::warmingUp)
        {
            samplesInRange = juce::jmin(
                samplesInRange,
                juce::jmax(1, topologyTransitionWarmupRemaining));
        }

        // See processHqTransitionBlock(): all global DryWetMixer calls below
        // must receive a range that fits the FIFO allocated at prepare time.
        samplesInRange = juce::jmin(
            samplesInRange,
            juce::jmax(1, preparedProcessingBlockCapacity));

        splitBandsRange(buffer,
                        sampleOffset,
                        samplesInRange,
                        sampleRate);

        HqCallbackContext rangeContext =
            activeMultibandTopologySnapshot.callbackContext;
        const std::array<juce::AudioBuffer<float>*, 4> fullBandBuffers {
            &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4
        };
        std::array<juce::AudioBuffer<float>, 4> bandRanges;
        std::array<juce::AudioBuffer<float>*, 4> bandRangePointers {};
        for (int bandIndex = 0; bandIndex < 4; ++bandIndex)
        {
            auto* owner = fullBandBuffers[static_cast<size_t>(bandIndex)];
            bandRanges[static_cast<size_t>(bandIndex)] =
                juce::AudioBuffer<float>(owner->getArrayOfWritePointers(),
                                         numChannels,
                                         sampleOffset,
                                         samplesInRange);
            bandRangePointers[static_cast<size_t>(bandIndex)] =
                &bandRanges[static_cast<size_t>(bandIndex)];

            if (bandIndex < numBands)
            {
                rangeContext.bandInputPeaks[static_cast<size_t>(bandIndex)] =
                    owner->getMagnitude(sampleOffset, samplesInRange);
                if (auto* band = bands[static_cast<size_t>(bandIndex)].get())
                    calculateAndStoreLevels(
                        bandRanges[static_cast<size_t>(bandIndex)],
                        band->mInputLeftRMS,
                        band->mInputRightRMS,
                        band->mInputLeftPeak,
                        band->mInputRightPeak);
            }
        }

        juce::AudioBuffer<float> audioRange(buffer.getArrayOfWritePointers(),
                                            numChannels,
                                            sampleOffset,
                                            samplesInRange);
        juce::AudioBuffer<float> dryRange(
            delayMatchedDryBuffer.getArrayOfWritePointers(),
            numChannels,
            sampleOffset,
            samplesInRange);
        juce::AudioBuffer<float> lfoRange(
            lfoOutputBuffer.getArrayOfWritePointers(),
            lfoOutputs.getNumChannels(),
            sampleOffset,
            samplesInRange);

        processActiveHqRange(audioRange,
                             dryRange,
                             bandRangePointers,
                             lfoRange,
                             sampleRate,
                             activeHqMode,
                             true,
                             rangeContext,
                             false,
                             primeBypassDelay);

        if (phaseForRange == TopologyTransitionPhase::fadingOut
            || phaseForRange == TopologyTransitionPhase::fadingIn)
        {
            applyTopologyTransitionRamp(audioRange);
        }
        else if (phaseForRange == TopologyTransitionPhase::warmingUp)
        {
            audioRange.clear();
            topologyTransitionWarmupRemaining -= samplesInRange;
        }

        // The topology envelope is part of the complete Base signal and must
        // reach the host through the same fixed PDC pad. HQ already carries
        // its natural upstream latency.
        if (! activeHqMode)
            applyNonHqOutputDelay(audioRange);

        sampleOffset += samplesInRange;
    }
}

void FireAudioProcessor::applyGlobalMix(
    juce::AudioBuffer<float>& buffer,
    juce::AudioBuffer<float>& delayMatchedDryBufferForRange,
    const juce::AudioBuffer<float>& lfoOutputs,
    bool useHQ)
{
    if (useHQ)
    {
        dryWetMixerGlobal.setWetLatency(
            preparedHqLatency.load(std::memory_order_acquire));
    }
    else
    {
        dryWetMixerGlobal.setWetLatency(0);
    }

    ModulatedValueProvider mixProvider;
    mixProvider.baseValue = loadCachedParameter(globalMixParameter, 1.0f);
    if (globalMixParameter.ranged != nullptr)
        mixProvider.range = globalMixParameter.ranged->getNormalisableRange();

    LfoManager::AudioThreadRoutingInfo routingInfo;
    const bool hasSampleAccurateModulation = globalMixParameter.ranged != nullptr
                                          && lfoManager->getAudioThreadRoutingInfo(
                                              globalMixParameter.ranged, routingInfo)
                                          && std::abs(routingInfo.depth) > 1.0e-6f
                                          && juce::isPositiveAndBelow(
                                              routingInfo.sourceLfoIndex,
                                              lfoOutputs.getNumChannels())
                                          && lfoOutputs.getNumSamples()
                                                 >= buffer.getNumSamples();
    if (hasSampleAccurateModulation)
    {
        mixProvider.lfoSignal = lfoOutputs.getReadPointer(
            routingInfo.sourceLfoIndex);
        mixProvider.modulationDepth = routingInfo.depth;
        mixProvider.isBipolar = routingInfo.isBipolar;
    }

    // Set the first target before reset so the initial callback still snaps to
    // the restored value. Later callbacks retain the mixer's existing 50 ms
    // smoothing while following every LFO sample instead of only sample zero.
    dryWetMixerGlobal.setWetMixProportion(
        juce::jlimit(0.0f, 1.0f, mixProvider.get(0)));
    if (! globalMixerPrimed)
        dryWetMixerGlobal.reset();
    globalMixerPrimed = true;

    auto wetBlock = juce::dsp::AudioBlock<float>(buffer);
    dryWetMixerGlobal.pushDrySamples(
        juce::dsp::AudioBlock<float>(delayMatchedDryBufferForRange));

    if (! hasSampleAccurateModulation)
    {
        dryWetMixerGlobal.mixWetSamples(wetBlock);
        return;
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        dryWetMixerGlobal.setWetMixProportion(
            juce::jlimit(0.0f, 1.0f, mixProvider.get(sample)));
        dryWetMixerGlobal.mixWetSamples(
            wetBlock.getSubBlock(static_cast<size_t>(sample), 1));
    }
}

FireAudioProcessor::ModulationInfo FireAudioProcessor::getModulationInfoForParameter(const juce::String& parameterID) const
{
    const juce::ScopedLock sl(lfoManager->getLfoDataLock());
    // Find the routing in the manager's list
    for (const auto& routing : lfoManager->getModulationRoutings())
    {
        if (routing.targetParameterID == parameterID)
        {
            if (juce::isPositiveAndBelow(routing.sourceLfoIndex, 4))
            {
                const float unipolarLfoValue = lfoManager->getLfoOutput(routing.sourceLfoIndex);
                float finalLfoValue = routing.isBipolar ? (unipolarLfoValue * 2.0f - 1.0f) : unipolarLfoValue;
                return { true, routing.sourceLfoIndex + 1, routing.depth, finalLfoValue, routing.isBipolar, routing.isBypassed };
            }
        }
    }
    return { false, 0, 0.0f, 0.0f, true, false }; // Default "not modulated" state
}

void FireAudioProcessor::setModulationValue(const juce::String& targetParameterID, float newValue)
{
    auto* parameter = treeState.getParameter(targetParameterID);
    auto* rawParameter = treeState.getRawParameterValue(targetParameterID);
    if (parameter == nullptr || rawParameter == nullptr || ! std::isfinite(newValue))
        return;

    const auto range = parameter->getNormalisableRange();
    const float valueNormalized = range.convertTo0to1(newValue);
    const float baseNormalized = range.convertTo0to1(rawParameter->load(std::memory_order_relaxed));
    bool didUpdate = false;

    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID != targetParameterID)
                continue;

            // The new depth is the difference between the target value's normalized position
            // and the base value's normalized position.
            float newDepth = valueNormalized - baseNormalized;

            // In Bipolar mode, the DSP logic effectively halves the depth's impact
            // to create a symmetrical swing. To make our `newValue` the actual extreme
            // of that swing, we must pre-emptively double the calculated depth.
            if (routing.isBipolar)
            {
                newDepth *= 2.0f;
            }

            // Clamp the final depth to the valid range [-1.0, 1.0] and update the routing.
            routing.depth = juce::jlimit(-1.0f, 1.0f, newDepth);
            didUpdate = true;
            break;
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::setModulationDepth(const juce::String& targetParameterID, float newDepth)
{
    if (! std::isfinite(newDepth))
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID == targetParameterID)
            {
                routing.depth = juce::jlimit(-1.0f, 1.0f, newDepth);
                didUpdate = true;
                break;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::toggleBipolarMode(const juce::String& targetParameterID)
{
    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID == targetParameterID)
            {
                routing.isBipolar = ! routing.isBipolar;
                didUpdate = true;
                break;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::resetModulation(const juce::String& targetParameterID)
{
    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID == targetParameterID)
            {
                routing.depth = 0.5f;
                routing.isBipolar = true;
                didUpdate = true;
                break;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::assignModulation(int routingIndex, int sourceLfoIndex, const juce::String& targetParameterID)
{
    if (targetParameterID.isNotEmpty()
        && (! juce::isPositiveAndBelow(sourceLfoIndex, 4) || treeState.getParameter(targetParameterID) == nullptr))
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        auto& routings = lfoManager->getModulationRoutings();
        if (! juce::isPositiveAndBelow(routingIndex, routings.size()))
            return;

        if (targetParameterID.isNotEmpty())
        {
            for (int i = 0; i < routings.size(); ++i)
            {
                if (i != routingIndex && routings.getReference(i).targetParameterID == targetParameterID)
                    routings.getReference(i).targetParameterID.clear();
            }
        }

        auto& currentRouting = routings.getReference(routingIndex);
        currentRouting.sourceLfoIndex = juce::jlimit(0, 3, sourceLfoIndex);
        currentRouting.targetParameterID = targetParameterID;
        didUpdate = true;
    }

    if (didUpdate)
        lfoDataHasChanged();
}

float FireAudioProcessor::getRealtimeModulatedThreshold(int bandIndex) const
{
    if (juce::isPositiveAndBelow(bandIndex, 4))
        return realtimeModulatedThresholds[bandIndex].load();

    return -48.0f;
}

const juce::StringArray& FireAudioProcessor::getLfoRateSyncDivisions() const
{
    return lfoManager->getLfoRateSyncDivisions();
}

void FireAudioProcessor::lfoDataHasChanged()
{
    updateHostDisplay(
        juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));

    if (auto* editor = dynamic_cast<FireAudioProcessorEditor*>(getActiveEditor()))
    {
        editor->markPresetAsDirty();
    }
}

bool FireAudioProcessor::getLatestMeterValues(MeterValues& values)
{
    // Check how many complete data packets are ready to be read.
    int numAvailable = meterFifo.getNumReady();

    // If there's at least one, we can proceed.
    if (numAvailable > 0)
    {
        // We want to get the most recent value. If there are multiple values
        // queued up, we'll read them all but only copy the last one.
        // This prevents the GUI from lagging behind the audio thread.

        int start1, size1, start2, size2;
        meterFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

        // The fifo might wrap around, so it gives us two contiguous blocks.
        // We just need to find the last element across these two blocks.
        if (size2 > 0)
        {
            // The latest element is at the end of the second block.
            values = meterFifoBuffer[start2 + size2 - 1];
        }
        else
        {
            // The latest element is at the end of the first block.
            values = meterFifoBuffer[start1 + size1 - 1];
        }

        // Tell the fifo that we've now "consumed" all the available data.
        meterFifo.finishedRead(numAvailable);

        // Return true to indicate that we successfully updated the 'values' object.
        return true;
    }

    // If no new data was available, return false.
    return false;
}

bool FireAudioProcessor::getLatestModulatedFilterValues(ModulatedFilterValues& values)
{
    int numAvailable = filterFifo.getNumReady();

    if (numAvailable > 0)
    {
        int start1, size1, start2, size2;
        filterFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

        if (size2 > 0)
        {
            values = filterFifoBuffer[start2 + size2 - 1];
        }
        else
        {
            values = filterFifoBuffer[start1 + size1 - 1];
        }
        filterFifo.finishedRead(numAvailable);
        return true;
    }
    return false;
}

float FireAudioProcessor::getGlobalInputRMSLevel(int channel) const
{
    return channel == 0 ? mInputLeftRMSGlobal.load() : mInputRightRMSGlobal.load();
}

float FireAudioProcessor::getGlobalOutputRMSLevel(int channel) const
{
    return channel == 0 ? mOutputLeftRMSGlobal.load() : mOutputRightRMSGlobal.load();
}

float FireAudioProcessor::getGlobalInputPeakLevel(int channel) const
{
    return channel == 0 ? mInputLeftPeakGlobal.load() : mInputRightPeakGlobal.load();
}

float FireAudioProcessor::getGlobalOutputPeakLevel(int channel) const
{
    return channel == 0 ? mOutputLeftPeakGlobal.load() : mOutputRightPeakGlobal.load();
}

float FireAudioProcessor::getBandInputRMSLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mInputLeftRMS.load() : bandProcessor->mInputRightRMS.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

float FireAudioProcessor::getBandOutputRMSLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mOutputLeftRMS.load() : bandProcessor->mOutputRightRMS.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

float FireAudioProcessor::getBandInputPeakLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mInputLeftPeak.load() : bandProcessor->mInputRightPeak.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

float FireAudioProcessor::getBandOutputPeakLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mOutputLeftPeak.load() : bandProcessor->mOutputRightPeak.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

void FireAudioProcessor::assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID)
{
    lfoManager->assignLfoToTarget(sourceLfoIndex, targetParameterID);

    // When the modulation assignment changes, notify the active editor to update its UI
    if (auto* editor = getActiveEditor())
    {
        // We must cast the generic editor pointer to our specific FireAudioProcessorEditor
        // type to access the triggerAsyncUpdate() method from its AsyncUpdater base class.
        // A dynamic_cast is used for type safety.
        if (auto* fireEditor = dynamic_cast<FireAudioProcessorEditor*>(editor))
        {
            fireEditor->triggerAsyncUpdate();
        }
    }
    lfoDataHasChanged();
}

void FireAudioProcessor::clearModulationForParameter(const juce::String& targetParameterID)
{
    lfoManager->clearModulationForTarget(targetParameterID);

    if (auto* editor = getActiveEditor())
    {
        if (auto* fireEditor = dynamic_cast<FireAudioProcessorEditor*>(editor))
        {
            fireEditor->triggerAsyncUpdate();
        }
    }
    lfoDataHasChanged();
}

void FireAudioProcessor::invertModulationDepthForParameter(const juce::String& targetParameterID)
{
    lfoManager->invertModulationDepth(targetParameterID);
    lfoDataHasChanged();
}

bool FireAudioProcessor::isCurrentStateEquivalentToPreset(const juce::XmlElement& presetXml)
{
    constexpr float comparisonTolerance = 1.0e-6f;

    // Recreate the exact model produced by loadStateFromXml rather than
    // comparing XML text. Older presets omit attributes whose loader defaults
    // are well-defined, and decimal formatting is not part of the preset's
    // audible state.
    std::array<LfoData, 4> expectedLfoData;
    std::array<bool, 4> expectedSmoothnessFromLfoXml {};
    std::array<bool, 4> expectedSmoothnessFromParameter {};
    std::array<bool, 4> loadedLfoIndices {};
    for (int i = 0; i < static_cast<int>(expectedSmoothnessFromParameter.size()); ++i)
        expectedSmoothnessFromParameter[static_cast<size_t>(i)] = presetXml.hasAttribute(
            ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i));

    // getChildByName intentionally selects the first section, matching the
    // loader when a malformed/legacy document contains duplicate sections.
    if (const auto* lfoState = presetXml.getChildByName("LFO_STATE"))
    {
        for (auto* lfoXml : lfoState->getChildIterator())
        {
            if (! lfoXml->hasTagName("LFO"))
                continue;

            const int index = lfoXml->getIntAttribute("index", -1);
            if (juce::isPositiveAndBelow(index, static_cast<int>(expectedLfoData.size()))
                && ! loadedLfoIndices[static_cast<size_t>(index)])
            {
                expectedLfoData[static_cast<size_t>(index)] = LfoData::readFromXml(*lfoXml);
                expectedSmoothnessFromLfoXml[static_cast<size_t>(index)] =
                    lfoXml->hasAttribute("smoothness");
                loadedLfoIndices[static_cast<size_t>(index)] = true;
            }
        }
    }

    for (const auto& parameter : getParameters())
    {
        auto* parameterWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        if (parameterWithID == nullptr)
            continue;

        float presetValue = parameterWithID->getDefaultValue();
        if (presetXml.hasAttribute(parameterWithID->paramID))
        {
            double parsedValue = 0.0;
            if (parseStrictFiniteDouble(presetXml.getStringAttribute(parameterWithID->paramID),
                                        parsedValue))
                presetValue = juce::jlimit(0.0f, 1.0f, static_cast<float>(parsedValue));
        }
        else if (parameterWithID->paramID.startsWith(SHAPE_BYPASS_ID))
        {
            presetValue = 1.0f;
        }
        else
        {
            // Before Smoothness became an APVTS parameter it lived only on the
            // LFO element. loadStateFromXml promotes that legacy value back to
            // the parameter through setLfoData(), so compare against the same
            // effective normalised parameter value.
            for (int i = 0; i < static_cast<int>(expectedLfoData.size()); ++i)
                if (parameterWithID->paramID
                        == ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i)
                    && expectedSmoothnessFromLfoXml[static_cast<size_t>(i)])
                {
                    if (const auto* ranged = treeState.getParameter(parameterWithID->paramID))
                        presetValue = ranged->convertTo0to1(
                            expectedLfoData[static_cast<size_t>(i)].smoothness);
                    break;
                }
        }

        if (std::abs(parameterWithID->getValue() - presetValue) > comparisonTolerance)
            return false;
    }

    for (size_t i = 0; i < expectedLfoData.size(); ++i)
    {
        if (! expectedSmoothnessFromParameter[i] && expectedSmoothnessFromLfoXml[i])
            continue;

        if (const auto* smoothness = treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(LFO_SMOOTH_ID, static_cast<int>(i))))
            expectedLfoData[i].smoothness = juce::jlimit(
                0.0f, 1.0f, smoothness->load(std::memory_order_relaxed));
    }

    const auto currentLfoData = lfoManager->getLfoDataCopy();
    if (currentLfoData.size() != expectedLfoData.size())
        return false;

    for (size_t i = 0; i < expectedLfoData.size(); ++i)
        if (! areLfoShapesEquivalent(currentLfoData[i], expectedLfoData[i]))
            return false;

    constexpr int maximumPresetRoutings = 128;
    juce::Array<ModulationRouting> expectedRoutings;
    if (const auto* routingState = presetXml.getChildByName("MODULATION_STATE"))
    {
        for (auto* routingXml : routingState->getChildIterator())
        {
            if (! routingXml->hasTagName("ROUTING")
                || expectedRoutings.size() >= maximumPresetRoutings)
                continue;

            auto routing = ModulationRouting::readFromXml(*routingXml);
            bool targetAlreadyUsed = false;
            for (const auto& existing : expectedRoutings)
                if (existing.targetParameterID == routing.targetParameterID)
                {
                    targetAlreadyUsed = true;
                    break;
                }

            if (! targetAlreadyUsed
                && routing.targetParameterID.isNotEmpty()
                && treeState.getParameter(routing.targetParameterID) != nullptr)
                expectedRoutings.add(std::move(routing));
        }
    }

    // Empty routing slots are an implementation detail of the live manager and
    // are neither written nor restored by presets. Compare unique targets as a
    // set because their array order has no DSP meaning.
    juce::Array<ModulationRouting> currentRoutings;
    juce::StringArray currentRoutingTargets;
    for (const auto& routing : lfoManager->getModulationRoutingsCopy())
    {
        if (routing.targetParameterID.isEmpty())
            continue;
        if (currentRoutingTargets.contains(routing.targetParameterID))
            return false;

        currentRoutingTargets.add(routing.targetParameterID);
        currentRoutings.add(routing);
    }

    if (currentRoutings.size() != expectedRoutings.size())
        return false;

    for (const auto& expected : expectedRoutings)
    {
        bool foundEquivalentRouting = false;
        for (const auto& candidate : currentRoutings)
            if (candidate.targetParameterID == expected.targetParameterID)
            {
                foundEquivalentRouting = areModulationRoutingsEquivalent(candidate, expected);
                break;
            }

        if (! foundEquivalentRouting)
            return false;
    }

    return true;
}

/**
 * @brief Shifts the band index for any LFO modulation routing target within a specified range.
 *
 * This function iterates through all modulation routings. If a routing's target parameter
 * has a band index between startIndex and endIndex (inclusive), it adjusts that index
 * by shiftAmount. This is crucial for keeping modulation assignments correct when bands
 * are added or removed.
 *
 * @param startIndex The starting band index of the range to affect.
 * @param endIndex The ending band index of the range to affect.
 * @param shiftAmount The amount to add to the band index (can be positive or negative).
 */
void FireAudioProcessor::shiftLfoModulationTargets(int startIndex, int endIndex, int shiftAmount)
{
    if (! juce::isPositiveAndBelow(startIndex, 4)
        || ! juce::isPositiveAndBelow(endIndex, 4)
        || startIndex > endIndex
        || shiftAmount == 0)
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        auto& routings = lfoManager->getModulationRoutings();
        juce::StringArray remappedTargets;
        remappedTargets.ensureStorageAllocated(routings.size());

        // Calculate the complete move from an immutable snapshot first. A band
        // shift has memmove semantics: every destination slot is replaced even
        // when its source band has no routing. This is especially important on
        // deletion, where the routing on the deleted band must not survive and
        // collide with the routing shifted in from its right-hand neighbour.
        for (const auto& routing : routings)
        {
            auto newTarget = routing.targetParameterID;
            const auto address = findBandParameterAddress(routing.targetParameterID);

            if (address.parameter != nullptr)
            {
                if (address.bandIndex >= startIndex && address.bandIndex <= endIndex)
                {
                    const int newBandIndex = address.bandIndex + shiftAmount;
                    newTarget = juce::isPositiveAndBelow(newBandIndex, 4)
                                    ? ParameterIDAndName::getIDString(address.parameter->idBase, newBandIndex)
                                    : juce::String();
                }
                else
                {
                    // If this band is a destination of the requested move, its
                    // old routing is overwritten even if the corresponding
                    // source has no routing of its own.
                    const int sourceBandIndex = address.bandIndex - shiftAmount;
                    if (sourceBandIndex >= startIndex && sourceBandIndex <= endIndex)
                        newTarget.clear();
                }
            }

            remappedTargets.add(newTarget);
        }

        // A band target is intentionally unique throughout the UI/API. Corrupt
        // or legacy states can contain duplicate source routings; keep the
        // first one produced by this move and reset the remaining slots. Leave
        // unrelated (including global) routings exactly as they were.
        for (int routingIndex = 0; routingIndex < remappedTargets.size(); ++routingIndex)
        {
            auto& target = remappedTargets.getReference(routingIndex);
            if (target.isEmpty()
                || target == routings.getReference(routingIndex).targetParameterID)
                continue;

            for (int earlierIndex = 0; earlierIndex < routingIndex; ++earlierIndex)
            {
                if (remappedTargets[earlierIndex] == target)
                {
                    target.clear();
                    break;
                }
            }
        }

        for (int routingIndex = 0; routingIndex < routings.size(); ++routingIndex)
        {
            auto& routing = routings.getReference(routingIndex);
            const auto& newTarget = remappedTargets[routingIndex];

            if (newTarget == routing.targetParameterID)
                continue;

            if (newTarget.isEmpty())
                resetModulationRouting(routing);
            else
                routing.targetParameterID = newTarget;

            didUpdate = true;
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

/**
 * @brief Clears (un-assigns) any LFO modulation that targets a specific band.
 *
 * This is used to clean up modulation routings when a band is being reset to its
 * default state, for example, when a new band is created.
 *
 * @param bandIndex The 0-based index of the band whose modulation targets should be cleared.
 */
void FireAudioProcessor::clearLfoModulationForBand(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, 4))
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        auto& routings = lfoManager->getModulationRoutings();

        for (auto& routing : routings)
        {
            const auto address = findBandParameterAddress(routing.targetParameterID);
            if (address.parameter != nullptr && address.bandIndex == bandIndex)
            {
                resetModulationRouting(routing);
                didUpdate = true;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

bool FireAudioProcessor::getLatestDistortionGraphValues(DistortionGraphValues& values)
{
    int numAvailable = graphFifo.getNumReady();
    if (numAvailable > 0)
    {
        int start1, size1, start2, size2;
        graphFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

        if (size2 > 0)
            values = graphFifoBuffer[start2 + size2 - 1];
        else
            values = graphFifoBuffer[start1 + size1 - 1];

        graphFifo.finishedRead(numAvailable);
        return true;
    }
    return false;
}

void FireAudioProcessor::setUiFocusBand(int bandIndex)
{
    if (juce::isPositiveAndBelow(bandIndex, 4))
    {
        uiFocusBand.store(bandIndex);
    }
}

void FireAudioProcessor::calculateAndStoreLevels(const juce::AudioBuffer<float>& buffer,
                                                 std::atomic<float>& rmsLeft,
                                                 std::atomic<float>& rmsRight,
                                                 std::atomic<float>& peakLeft,
                                                 std::atomic<float>& peakRight)
{
    // This function calculates RMS and Peak levels for a given buffer and stores them
    // in the provided atomic float variables for thread-safe access from the UI.

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // If there's no audio, reset levels to zero to prevent stale values.
    if (numChannels <= 0 || numSamples <= 0)
    {
        rmsLeft.store(0.0f);
        rmsRight.store(0.0f);
        peakLeft.store(0.0f);
        peakRight.store(0.0f);
        return;
    }

    // Use JUCE's built-in functions for efficient calculation.
    // getRMSLevel() returns linear RMS amplitude.
    // getMagnitude() with arguments (0, numSamples) finds the peak absolute value.

    // Calculate for Left Channel (or Mono)
    rmsLeft.store(buffer.getRMSLevel(0, 0, numSamples));
    peakLeft.store(buffer.getMagnitude(0, 0, numSamples));

    // Calculate for Right Channel if it exists, otherwise mirror the left channel.
    if (numChannels > 1)
    {
        rmsRight.store(buffer.getRMSLevel(1, 0, numSamples));
        peakRight.store(buffer.getMagnitude(1, 0, numSamples));
    }
    else
    {
        rmsRight.store(rmsLeft.load());
        peakRight.store(peakLeft.load());
    }
}
