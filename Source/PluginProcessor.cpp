/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include "PluginProcessor.h"
#include <map>
#include <set>
#include "DSP/DistortionLogic.h"
#include "PluginEditor.h"
#include "Utility/StrictNumberParser.h"
#include "Utility/EditHistory.h"
#include "Utility/FrozenAudioState.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
using HostBypassDenormalStateHook = void (*) (bool) noexcept;
void setHostBypassDenormalStateHookForTesting(
    HostBypassDenormalStateHook hook) noexcept;
#endif

namespace
{
bool parseStrictFiniteDouble(const juce::String& textToParse, double& result) noexcept
{
    return fire::utility::parseStrictFiniteDouble(textToParse, result);
}

bool parseStrictNonNegativeIntegerAttribute(const juce::XmlElement& xml,
                                            const char* attributeName,
                                            int& result) noexcept
{
    double parsed = 0.0;
    if (! xml.hasAttribute(attributeName)
        || ! parseStrictFiniteDouble(xml.getStringAttribute(attributeName), parsed)
        || parsed < 0.0
        || parsed > static_cast<double>(std::numeric_limits<int>::max())
        || parsed != std::floor(parsed))
    {
        return false;
    }

    result = static_cast<int>(parsed);
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
constexpr int hostStateFormatVersion = 1;
constexpr int oldestWrappedHostParameterCount = 19;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
std::atomic<HostBypassDenormalStateHook>
    hostBypassDenormalStateHookForTesting { nullptr };
#endif
constexpr std::array<const char*, 8> legacyHostStateParameterAnchors {
    HQ_ID,
    DOWNSAMPLE_ID,
    OFF_ID,
    PRE_ID,
    POST_ID,
    LOW_ID,
    BAND_ID,
    HIGH_ID,
};

bool hasStrictFiniteAttributeInRange(const juce::XmlElement& xml,
                                     const char* attributeName,
                                     double minimum,
                                     double maximum) noexcept
{
    double parsed = 0.0;
    return xml.hasAttribute(attributeName)
           && parseStrictFiniteDouble(xml.getStringAttribute(attributeName),
                                      parsed)
           && parsed >= minimum && parsed <= maximum;
}

bool hasStrictBooleanAttribute(const juce::XmlElement& xml,
                               const char* attributeName) noexcept
{
    if (! xml.hasAttribute(attributeName))
        return false;

    const auto value = xml.getStringAttribute(attributeName).trim();
    return value == "0" || value == "1";
}

bool readLoudnessMatchSettings(const juce::XmlElement* xml,
                              fire::dsp::LoudnessMatchState::Settings& settings)
{
    settings = {};
    if (xml == nullptr)
        return true;
    constexpr std::array<const char*, 6> attributes { "loudnessMatchVersion", "loudnessMatchEnabled",
        "loudnessMatchReadyA", "loudnessMatchReadyB", "loudnessMatchGainA", "loudnessMatchGainB" };
    const bool hasAny = std::any_of(attributes.begin(), attributes.end(),
        [xml](const auto* name) { return xml->hasAttribute(name); });
    if (! hasAny)
        return true;
    int version = 0;
    if (! parseStrictNonNegativeIntegerAttribute(*xml, attributes[0], version) || version != 1
        || ! hasStrictBooleanAttribute(*xml, attributes[1])
        || ! hasStrictBooleanAttribute(*xml, attributes[2])
        || ! hasStrictBooleanAttribute(*xml, attributes[3])
        || ! hasStrictFiniteAttributeInRange(*xml, attributes[4], -18.0, 18.0)
        || ! hasStrictFiniteAttributeInRange(*xml, attributes[5], -18.0, 18.0))
        return false;
    settings.enabled = xml->getBoolAttribute(attributes[1]);
    for (size_t side = 0; side < 2; ++side)
    {
        settings.ready[side] = xml->getBoolAttribute(attributes[side + 2]);
        double gainDb = 0.0;
        parseStrictFiniteDouble(xml->getStringAttribute(attributes[side + 4]), gainDb);
        settings.gainDb[side] = settings.ready[side] ? static_cast<float>(gainDb) : 0.0f;
        settings.limited[side] = settings.ready[side] && std::abs(settings.gainDb[side]) >= 18.0f;
    }
    return true;
}

int countDirectChildrenWithTagName(const juce::XmlElement& parent,
                                   const char* tagName) noexcept
{
    int count = 0;
    for (auto* child : parent.getChildIterator())
        if (child->hasTagName(tagName))
            ++count;
    return count;
}

bool isValidVersionedHostLfoState(const juce::XmlElement& lfoState, int expectedCount) noexcept
{
    if (! lfoState.hasTagName("LFO_STATE")
        || lfoState.getNumChildElements() != expectedCount)
        return false;

    std::array<bool, fire::lfo_bank::capacity> seenIndices {};
    for (auto* lfo : lfoState.getChildIterator())
    {
        int index = -1;
        if (! lfo->hasTagName("LFO")
            || ! parseStrictNonNegativeIntegerAttribute(*lfo, "index", index)
            || ! juce::isPositiveAndBelow(index, expectedCount)
            || seenIndices[static_cast<size_t>(index)])
        {
            return false;
        }

        seenIndices[static_cast<size_t>(index)] = true;
        if (lfo->hasAttribute("smoothness")
            && ! hasStrictFiniteAttributeInRange(*lfo,
                                                 "smoothness",
                                                 0.0,
                                                 1.0))
        {
            return false;
        }

        const juce::XmlElement* points = nullptr;
        const juce::XmlElement* curvatures = nullptr;
        for (auto* payload : lfo->getChildIterator())
        {
            if (payload->hasTagName("POINTS") && points == nullptr)
                points = payload;
            else if (payload->hasTagName("CURVATURES")
                     && curvatures == nullptr)
                curvatures = payload;
            else
                return false;
        }

        if (points == nullptr || curvatures == nullptr
            || lfo->getNumChildElements() != 2
            || points->getNumChildElements() < 2
            || points->getNumChildElements()
                   > static_cast<int>(LfoData::maximumNumberOfPoints)
            || curvatures->getNumChildElements()
                   != points->getNumChildElements() - 1)
        {
            return false;
        }

        for (auto* point : points->getChildIterator())
            if (! point->hasTagName("P")
                || point->getNumChildElements() != 0
                || ! hasStrictFiniteAttributeInRange(*point, "x", 0.0, 1.0)
                || ! hasStrictFiniteAttributeInRange(*point, "y", 0.0, 1.0))
            {
                return false;
            }

        for (auto* curvature : curvatures->getChildIterator())
            if (! curvature->hasTagName("C")
                || curvature->getNumChildElements() != 0
                || ! hasStrictFiniteAttributeInRange(*curvature,
                                                     "v",
                                                     -2.0,
                                                     2.0))
            {
                return false;
            }
    }

    return true;
}

bool isValidVersionedHostRoutingState(
    const juce::XmlElement& routingState,
    juce::AudioProcessorValueTreeState& parameterState,
    int sourceCount) noexcept
{
    if (! routingState.hasTagName("MODULATION_STATE")
        || routingState.getNumChildElements()
               > LfoManager::maximumModulationRoutings)
    {
        return false;
    }

    juce::StringArray seenTargets;
    for (auto* routing : routingState.getChildIterator())
    {
        int source = -1;
        if (! routing->hasTagName("ROUTING")
            || routing->getNumChildElements() != 0
            || ! parseStrictNonNegativeIntegerAttribute(*routing,
                                                        "source",
                                                        source)
            || ! juce::isPositiveAndBelow(source, sourceCount)
            || ! routing->hasAttribute("target")
            || ! hasStrictFiniteAttributeInRange(*routing,
                                                 "depth",
                                                 -1.0,
                                                 1.0)
            || ! hasStrictBooleanAttribute(*routing, "bipolar")
            || ! hasStrictBooleanAttribute(*routing, "bypassed"))
        {
            return false;
        }

        // Current Init snapshots contain empty preallocated slots. They have no
        // semantic routing and may repeat, but all populated targets must be
        // known and unique.
        const auto target = routing->getStringAttribute("target");
        if (fire::mod_sources::isParameterID(target)) return false;
        if (target.isEmpty())
            continue;
        if (parameterState.getParameter(target) == nullptr
            || seenTargets.contains(target))
        {
            return false;
        }

        seenTargets.add(target);
    }

    return true;
}

void replaceNonFiniteSamplesWithSilence(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* channelData = buffer.getReadPointer(channel);
        float* writableChannelData = nullptr;
        for (int sample = 0; sample < numSamples; ++sample)
            if (! std::isfinite(channelData[sample]))
            {
                if (writableChannelData == nullptr)
                    writableChannelData = buffer.getWritePointer(channel);
                writableChannelData[sample] = 0.0f;
            }
    }
}

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

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
void setHostBypassDenormalStateHookForTesting(
    HostBypassDenormalStateHook hook) noexcept
{
    hostBypassDenormalStateHookForTesting.store(hook,
                                                std::memory_order_release);
}
#endif

void OutputGainTransitionState::prepare(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    routeRampSamples = static_cast<int>(std::floor(safeSampleRate * 0.01));
    driveCompHandoffSamples = static_cast<int>(std::floor(safeSampleRate * 0.05));
    routeTransitionMix.reset(safeSampleRate, 0.01);
    routedBaseGainSmoother.reset(safeSampleRate, 0.05);
    legacyGainTracker.reset(safeSampleRate, 0.05);
    reset();
}

void OutputGainTransitionState::reset() noexcept
{
    routeTransitionMix.setCurrentAndTargetValue(1.0f);
    routedBaseGainSmoother.setCurrentAndTargetValue(1.0f);
    legacyGainTracker.setCurrentAndTargetValue(0.0f);
    lastRecipe = {};
    anchorLinearGain = 1.0f;
    lastAppliedLinearGain = 1.0f;
    routedBaseTargetDb = 0.0f;
    driveCompHandoffRemaining = 0;
    lastModernDriveComp = false;
    scalarDriveCompHandoff = false;
    routedBaseGainPrimed = false;
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

void ShapeControlRecipeTransitionState::prepare(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    routeTransitionMix.reset(safeSampleRate, 0.01);
    reset();
}

void ShapeControlRecipeTransitionState::reset() noexcept
{
    routeTransitionMix.setCurrentAndTargetValue(1.0f);
    lastRecipe = {};
    anchorValue = 0.0f;
    lastAppliedValue = 0.0f;
    initialised = false;
}

static ShapeControlRecipeTransitionState::RecipeSignature
makeShapeControlRecipe(const ModulatedValueProvider& provider,
                       int sourceIndex) noexcept
{
    ShapeControlRecipeTransitionState::RecipeSignature recipe;
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

static bool sameShapeControlRecipe(
    const ShapeControlRecipeTransitionState::RecipeSignature& lhs,
    const ShapeControlRecipeTransitionState::RecipeSignature& rhs) noexcept
{
    if (lhs.routed != rhs.routed)
        return false;
    if (! lhs.routed)
        return true;

    return lhs.sourceIndex == rhs.sourceIndex
           && juce::exactlyEqual(lhs.modulationDepth, rhs.modulationDepth)
           && lhs.isBipolar == rhs.isBipolar;
}

static void serviceShapeControlRecipeTransition(
    ShapeControlRecipeTransitionState& transition,
    const ShapeControlRecipeTransitionState::RecipeSignature& recipe,
    float currentTarget) noexcept
{
    const float safeTarget = std::isfinite(currentTarget)
                                 ? currentTarget
                                 : 0.0f;
    if (! transition.initialised)
    {
        transition.initialised = true;
        transition.lastRecipe = recipe;
        transition.routeTransitionMix.setCurrentAndTargetValue(1.0f);
        transition.anchorValue = safeTarget;
        transition.lastAppliedValue = safeTarget;
        return;
    }

    if (! sameShapeControlRecipe(recipe, transition.lastRecipe))
    {
        transition.anchorValue = transition.lastAppliedValue;
        transition.routeTransitionMix.setCurrentAndTargetValue(0.0f);
        transition.routeTransitionMix.setTargetValue(1.0f);
        transition.lastRecipe = recipe;
    }
}

static float applyShapeControlRecipeTransition(
    const ShapeControlRecipeTransitionState& transition,
    float targetValue) noexcept
{
    const float safeTarget = std::isfinite(targetValue)
                                 ? targetValue
                                 : transition.anchorValue;
    const float mix = transition.routeTransitionMix.getCurrentValue();
    if (mix <= 0.0f)
        return transition.anchorValue;
    if (mix >= 1.0f)
        return safeTarget;

    return transition.anchorValue
           + mix * (safeTarget - transition.anchorValue);
}

void DriveControlTransitionState::prepare(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    routeTransitionMix.reset(safeSampleRate, 0.01);
    enableTransitionMix.reset(safeSampleRate, 0.05);
    safeRecoveryMix.reset(safeSampleRate, 0.05);
    reset();
}

void DriveControlTransitionState::reset() noexcept
{
    routeTransitionMix.setCurrentAndTargetValue(1.0f);
    enableTransitionMix.setCurrentAndTargetValue(1.0f);
    safeRecoveryMix.setCurrentAndTargetValue(1.0f);
    lastRecipe = {};
    routeAnchorGain = 1.0f;
    lastAppliedRouteGain = 1.0f;
    enableAnchorGain = 1.0f;
    lastAppliedFinalGain = 1.0f;
    safeRecoveryAnchorGain = 1.0f;
    recipeInitialised = false;
    enableInitialised = false;
    lastDriveEnabled = true;
}

static DriveControlTransitionState::RecipeSignature makeDriveRecipe(
    const ModulatedValueProvider& provider,
    int sourceIndex,
    bool isExtremeModeOn) noexcept
{
    DriveControlTransitionState::RecipeSignature recipe;
    recipe.routed = provider.lfoSignal != nullptr;
    recipe.sourceIndex = recipe.routed ? sourceIndex : -1;
    recipe.modulationDepth = recipe.routed
                                 && std::isfinite(provider.modulationDepth)
                                 ? juce::jlimit(-1.0f,
                                                1.0f,
                                                provider.modulationDepth)
                                 : 0.0f;
    recipe.isBipolar = recipe.routed ? provider.isBipolar : true;
    recipe.isExtremeModeOn = recipe.routed && isExtremeModeOn;
    return recipe;
}

static bool sameDriveRecipe(
    const DriveControlTransitionState::RecipeSignature& lhs,
    const DriveControlTransitionState::RecipeSignature& rhs) noexcept
{
    if (lhs.routed != rhs.routed)
        return false;
    if (! lhs.routed)
        return true;

    return lhs.sourceIndex == rhs.sourceIndex
           && juce::exactlyEqual(lhs.modulationDepth, rhs.modulationDepth)
           && lhs.isBipolar == rhs.isBipolar
           && lhs.isExtremeModeOn == rhs.isExtremeModeOn;
}

static void serviceDriveRecipeTransition(
    DriveControlTransitionState& transition,
    const DriveControlTransitionState::RecipeSignature& recipe,
    float currentTargetGain) noexcept
{
    const float safeTargetGain = std::isfinite(currentTargetGain)
                                     ? juce::jmax(0.0f, currentTargetGain)
                                     : 1.0f;
    if (! transition.recipeInitialised)
    {
        transition.recipeInitialised = true;
        transition.lastRecipe = recipe;
        transition.routeTransitionMix.setCurrentAndTargetValue(1.0f);
        transition.routeAnchorGain = safeTargetGain;
        transition.lastAppliedRouteGain = safeTargetGain;
        return;
    }

    if (! sameDriveRecipe(recipe, transition.lastRecipe))
    {
        // Hold the gain that was actually requested on the preceding base-rate
        // frame. Rapid recipe edits therefore retarget without an endpoint jump.
        transition.routeAnchorGain = transition.lastAppliedRouteGain;
        transition.routeTransitionMix.setCurrentAndTargetValue(0.0f);
        transition.routeTransitionMix.setTargetValue(1.0f);
        transition.lastRecipe = recipe;
    }
}

static float applyDriveRecipeTransition(
    const DriveControlTransitionState& transition,
    float targetGain) noexcept
{
    const float safeTargetGain = std::isfinite(targetGain)
                                     ? juce::jmax(0.0f, targetGain)
                                     : transition.routeAnchorGain;
    const float mix = transition.routeTransitionMix.getCurrentValue();
    if (mix <= 0.0f)
        return transition.routeAnchorGain;
    if (mix >= 1.0f)
        return safeTargetGain;

    return transition.routeAnchorGain
           + mix * (safeTargetGain - transition.routeAnchorGain);
}

static void resetSafeDriveRecovery(
    DriveControlTransitionState& transition) noexcept
{
    transition.safeRecoveryMix.setCurrentAndTargetValue(1.0f);
    transition.safeRecoveryAnchorGain = 1.0f;
}

static void serviceDriveEnableTransition(
    DriveControlTransitionState& transition,
    bool isDriveEnabled) noexcept
{
    if (! transition.enableInitialised)
    {
        transition.enableInitialised = true;
        transition.lastDriveEnabled = isDriveEnabled;
        transition.enableTransitionMix.setCurrentAndTargetValue(1.0f);
        return;
    }

    if (transition.lastDriveEnabled != isDriveEnabled)
    {
        // Safe may have pulled the last audible gain below the routed request.
        // Anchor the power transition after Safe, then clear the old recovery
        // bridge so the two 50 ms ramps cannot multiply into a gain rebound.
        transition.enableAnchorGain = transition.lastAppliedFinalGain;
        transition.enableTransitionMix.setCurrentAndTargetValue(0.0f);
        transition.enableTransitionMix.setTargetValue(1.0f);
        resetSafeDriveRecovery(transition);
        transition.lastDriveEnabled = isDriveEnabled;
    }
}

static float applyDriveEnableTransition(
    DriveControlTransitionState& transition,
    float targetGain) noexcept
{
    const float safeTargetGain = std::isfinite(targetGain)
                                     ? juce::jmax(0.0f, targetGain)
                                     : transition.enableAnchorGain;
    const float mix = transition.enableTransitionMix.getNextValue();
    if (mix <= 0.0f)
        return transition.enableAnchorGain;
    if (mix >= 1.0f)
        return safeTargetGain;

    return transition.enableAnchorGain
           + mix * (safeTargetGain - transition.enableAnchorGain);
}

static float applySafeDriveRecovery(
    DriveControlTransitionState& transition,
    float requestedGain) noexcept
{
    const float safeRequestedGain = std::isfinite(requestedGain)
                                        ? juce::jmax(0.0f, requestedGain)
                                        : transition.safeRecoveryAnchorGain;
    const float mix = transition.safeRecoveryMix.getNextValue();
    if (mix >= 1.0f)
        return safeRequestedGain;

    const float recoveryCap = transition.safeRecoveryAnchorGain
                              + mix
                                    * (safeRequestedGain
                                       - transition.safeRecoveryAnchorGain);
    return juce::jmin(safeRequestedGain, recoveryCap);
}

static float driveGainScale(bool isExtremeModeOn) noexcept
{
    const float extremeScale = isExtremeModeOn ? std::log2(10.0f) : 1.0f;
    return extremeScale * 6.5f / 100.0f;
}

static float driveValueToExponent(float driveValue,
                                  bool isExtremeModeOn) noexcept
{
    float driveForCalculation = std::isfinite(driveValue) ? driveValue : 0.0f;
    if (isExtremeModeOn)
        driveForCalculation = log2f(10.0f) * driveForCalculation;
    return driveForCalculation * 6.5f / 100.0f;
}

static float driveValueToGain(float driveValue,
                              bool isExtremeModeOn) noexcept
{
    return std::pow(2.0f,
                    driveValueToExponent(driveValue, isExtremeModeOn));
}

static float driveGainToValue(float driveGain,
                              bool isExtremeModeOn) noexcept
{
    const float safeDriveGain = std::isfinite(driveGain)
                                    ? juce::jmax(1.0e-12f, driveGain)
                                    : 1.0f;
    return std::log2(safeDriveGain) / driveGainScale(isExtremeModeOn);
}

static float getDriveRouteTargetGain(
    const ModulatedValueProvider& provider,
    int sample,
    float smoothedBaseGain,
    bool isExtremeModeOn) noexcept
{
    const float safeBaseGain = std::isfinite(smoothedBaseGain)
                                   ? juce::jmax(0.0f, smoothedBaseGain)
                                   : 1.0f;

    // Preserve the established static/no-route trajectory exactly. In
    // particular, avoid an unnecessary log2/exp2 round trip in Golden paths.
    if (provider.lfoSignal == nullptr)
        return safeBaseGain;

    const float smoothedBaseValue = driveGainToValue(safeBaseGain,
                                                     isExtremeModeOn);
    return driveValueToGain(provider.get(sample, smoothedBaseValue),
                            isExtremeModeOn);
}

static OutputGainTransitionState::RecipeSignature makeOutputGainRecipe(
    const ModulatedValueProvider& provider,
    int sourceIndex,
    bool isLinked) noexcept
{
    OutputGainTransitionState::RecipeSignature recipe;
    recipe.routed = provider.lfoSignal != nullptr;
    recipe.sourceIndex = recipe.routed ? sourceIndex : -1;
    recipe.modulationDepth = std::isfinite(provider.modulationDepth)
                                 ? juce::jlimit(-1.0f,
                                                1.0f,
                                                provider.modulationDepth)
                                 : 0.0f;
    recipe.isBipolar = provider.isBipolar;
    recipe.isLinked = recipe.routed && isLinked;
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
                      int sourceIndex,
                      bool isLinked,
                      bool modernDriveComp = false)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    // Moving compensation between Drive and Output uses the same 50 ms
    // handoff at both locations. A routed Output normally bridges recipe
    // changes in 10 ms, which would otherwise finish far ahead of Drive.
    // Retargets retain the handoff's deadline; only a mode reversal starts a
    // new 50 ms window. Count base-rate samples on every path, including the
    // scalar and steady routed early returns below.
    const bool driveCompModeChanged = transition.initialised
                                     && modernDriveComp != transition.lastModernDriveComp;
    if (driveCompModeChanged)
        transition.driveCompHandoffRemaining = transition.driveCompHandoffSamples;
    transition.lastModernDriveComp = modernDriveComp;
    const int recipeRampSamples = juce::jmax(transition.routeRampSamples,
                                            transition.driveCompHandoffRemaining);
    if (gainProvider.lfoSignal != nullptr)
        transition.scalarDriveCompHandoff = false;
    else if (transition.driveCompHandoffRemaining > 0 && transition.lastRecipe.routed)
        transition.scalarDriveCompHandoff = true;
    transition.driveCompHandoffRemaining = juce::jmax(
        0, transition.driveCompHandoffRemaining - buffer.getNumSamples());

    const auto recipe = makeOutputGainRecipe(gainProvider,
                                             sourceIndex,
                                             isLinked);
    // A route removed during the handoff must also retain its deadline.
    // Temporarily let the recipe bridge below target a scalar value; handing
    // it straight back to Gain here would restart a full 50 ms Output ramp.
    if (gainProvider.lfoSignal == nullptr && ! transition.scalarDriveCompHandoff)
    {
        transition.routedBaseGainPrimed = false;
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

    const float safeBaseDb = std::isfinite(gainProvider.baseValue)
                                 ? gainProvider.baseValue
                                 : 0.0f;
    const float routedBaseTargetGain = juce::Decibels::decibelsToGain(
        safeBaseDb);
    // Output's smallest effective base step is 0.001 dB (Linked Drive at its
    // 0.01 step). APVTS range reconstruction and -0.1 * Drive can differ by a
    // few float ULPs for the same displayed value, so compare in the parameter
    // domain with a tolerance far below any real automation step.
    constexpr float baseTargetEqualityToleranceDb = 1.0e-5f;
    const bool routedBaseTargetChanged =
        ! transition.routedBaseGainPrimed
        || std::abs(safeBaseDb - transition.routedBaseTargetDb)
               > baseTargetEqualityToleranceDb;
    const bool linkedModeChanged = transition.initialised
                                   && recipe.isLinked
                                          != transition.lastRecipe.isLinked;
    const bool recipeChanged = transition.initialised
                               && (! sameOutputGainRecipe(
                                       recipe,
                                       transition.lastRecipe)
                                   || (linkedModeChanged
                                       && routedBaseTargetChanged)
                                   || (driveCompModeChanged
                                       && routedBaseTargetChanged));

    // A newly attached route starts from its complete current recipe. A true
    // discrete recipe edit also snaps a simultaneously changed base so the
    // held-anchor bridge remains its only transition. When just
    // source/depth/polarity changes during an in-flight base ramp, retain that
    // independent 50 ms trajectory instead of fast-forwarding it.
    if (! transition.routedBaseGainPrimed
        || (recipeChanged && routedBaseTargetChanged))
    {
        transition.routedBaseGainSmoother.setCurrentAndTargetValue(
            routedBaseTargetGain);
    }
    else if (routedBaseTargetChanged)
    {
        transition.routedBaseGainSmoother.setTargetValue(
            routedBaseTargetGain);
    }
    transition.routedBaseTargetDb = safeBaseDb;
    transition.routedBaseGainPrimed = true;

    if (! transition.initialised)
    {
        transition.initialised = true;
        transition.lastRecipe = recipe;
        transition.routeTransitionMix.setCurrentAndTargetValue(1.0f);
    }
    else if (recipeChanged)
    {
        // Keep the target LFO fully sample-accurate. Only the discrete recipe
        // boundary is bridged from the gain that was actually audible.
        transition.anchorLinearGain = transition.lastAppliedLinearGain;
        transition.routeTransitionMix.reset(recipeRampSamples);
        transition.routeTransitionMix.setCurrentAndTargetValue(0.0f);
        transition.routeTransitionMix.setTargetValue(1.0f);
        transition.lastRecipe = recipe;
    }
    else if (linkedModeChanged)
    {
        // A Linked toggle that produces the same effective base is audibly a
        // no-op. Record the mode without restarting the route bridge; a later
        // Linked toggle with a different effective base is handled above.
        transition.lastRecipe = recipe;
    }

    if (! transition.routeTransitionMix.isSmoothing()
        && ! transition.routedBaseGainSmoother.isSmoothing())
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
        transition.scalarDriveCompHandoff = false;
        return;
    }

    auto* const* channelData = buffer.getArrayOfWritePointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float gainDb = transition.routedBaseGainSmoother.isSmoothing()
                                 ? gainProvider.get(
                                     sample,
                                     juce::Decibels::gainToDecibels(
                                         transition.routedBaseGainSmoother
                                             .getNextValue()))
                                 : gainProvider.get(sample);
        const float targetLinearGain = juce::Decibels::decibelsToGain(
            gainDb);
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
void BandProcessor::prepare(const juce::dsp::ProcessSpec& spec, bool withInserts, bool independentHq)
{
    analogShapeBaseRate = spec.sampleRate;
    for (auto& bank : analogShapeBanks) for (auto& stage : bank) stage.prepare(spec.sampleRate);
    // Prepare all the DSP modules with the sample rate and block size.
    compressor.prepare(spec);
    ott.prepare(spec);
    if (withInserts) inserts.prepare(spec, independentHq);
    orderTransition.prepare(spec.sampleRate);
    mOttInputLevelDb.store(-120.0f, std::memory_order_relaxed);
    mOttGainChangeDb.store(0.0f, std::memory_order_relaxed);
    mOttDynamicsActivityDb.store(0.0f, std::memory_order_relaxed);
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
        static_cast<int>(std::min<juce::uint64>(
            spec.maximumBlockSize,
            static_cast<juce::uint64>(std::numeric_limits<int>::max()))));
    const auto maximumBlockSize = maximumPreparedBlockSize;

    // The oversampling object also needs to be prepared.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels, oversampleFactor, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    oversampling->initProcessing(static_cast<size_t>(maximumPreparedBlockSize));

    dryBuffer.setSize(numChannels, maximumBlockSize);
    dcFilterDryBuffer.setSize(numChannels, maximumBlockSize);
    // Preallocate the entire source bank, including Envelope and all Macros,
    // so binding an auxiliary route cannot grow this buffer on the audio thread.
    upsampledLfoOutputs.setSize(fire::mod_sources::sourceCount, maximumBlockSize * 4);
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
    driveControlTransition.prepare(spec.sampleRate);
    driveCompensation.prepare(spec.sampleRate);
    mDriveCompensationDb.store(0.0f, std::memory_order_relaxed);
    mDriveCompensationSequence.store(0, std::memory_order_release);
    biasSmoother.reset(spec.sampleRate, 0.05);
    recSmoother.reset(spec.sampleRate, 0.05);
    biasRecipeTransition.prepare(spec.sampleRate);
    recRecipeTransition.prepare(spec.sampleRate);
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
    shapeControlsPrimed = false;
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
    shapeControlsPrimed = false;
    shapeMixSmootherPrimed = false;
    compressorBaseSmoothersPrimed = false;
    waveshaperModeMixPrimed = false;
    for (auto& bank : analogShapeBanks) for (auto& stage : bank) stage.reset();
    bandEnableMixPrimed = false;
    dcFilterMixPrimed = false;
    compressor.reset();
    ott.reset();
    inserts.reset();
    orderTransition.reset();
    mOttInputLevelDb.store(-120.0f, std::memory_order_relaxed);
    mOttGainChangeDb.store(0.0f, std::memory_order_relaxed);
    mOttDynamicsActivityDb.store(0.0f, std::memory_order_relaxed);
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
    driveControlTransition.reset();
    driveCompensation.reset();
    mDriveCompensationDb.store(0.0f, std::memory_order_relaxed);
    mDriveCompensationSequence.store(0, std::memory_order_release);
    biasRecipeTransition.reset();
    recRecipeTransition.reset();
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
    for (auto& bank : analogShapeBanks) for (auto& stage : bank) stage.reset();
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

    for (size_t i = 0; i < paramsForProcessing.ott.controls.size(); ++i)
        bindCompressorProvider(paramsForProcessing.ott.controls[i], params.ott.sources[i]);

    if (hasCompleteBaseRateLfoChunk && juce::isPositiveAndBelow(params.outputLfoSourceIndex, lfoOutputs.getNumChannels()))
        paramsForProcessing.outputVal.lfoSignal = lfoOutputs.getReadPointer(params.outputLfoSourceIndex, lfoSampleOffset);

    // Band Enable bypasses the complete processed band, including the user's
    // own Band Mix. Align the shared dry path to the oversampled wet path once
    // so both coefficient stages retain the established Thiran phase response.
    sharedBandDryDelay.setDelay(static_cast<float>(inserts.getReservedLatency())
                               + (useHQ ? oversampling->getLatencyInSamples() : 0.0f));
    auto bandEnableDryBlock = juce::dsp::AudioBlock<float>(dryBuffer);
    sharedBandDryDelay.process(
        juce::dsp::ProcessContextReplacing<float>(bandEnableDryBlock));
    bandMixer.pushDrySamples(bandEnableDryBlock);

    const auto& order = orderTransition.begin(params.moduleOrder);
    for (size_t position = 0; position < order.size(); ++position)
    {
        const int node = order[position];
        if (node == 0)
        {
            const bool joined = position + 1 < order.size() && order[position + 1] == 1;
            const auto peak = position == 0 ? inputPeak : buffer.getMagnitude(0, buffer.getNumSamples());
            processDriveShapeStage(buffer, params, lfoOutputs, lfoSampleOffset, peak, updateReductionMeter, true, joined);
            if (joined) { processDcFilter(buffer, params.isDcFilterEnabled); ++position; }
        }
        else if (node == 1)
        {
            processDriveShapeStage(buffer, params, lfoOutputs, lfoSampleOffset, inputPeak, false, false, true);
            processDcFilter(buffer, params.isDcFilterEnabled);
        }
        else if (node == 2) processCompressorStage(buffer, paramsForProcessing);
        else if (node == 3) processStereoStage(buffer, paramsForProcessing);
        else if (node == 4)
        {
            ott.process(block, paramsForProcessing.ott);
            mOttInputLevelDb.store(params.isBandEnabled ? ott.getInputLevelDb() : -120.0f, std::memory_order_relaxed);
            mOttGainChangeDb.store(params.isBandEnabled ? ott.getGainChangeDb() : 0.0f, std::memory_order_relaxed);
            mOttDynamicsActivityDb.store(params.isBandEnabled ? ott.getDynamicsActivityDb() : 0.0f, std::memory_order_relaxed);
        }
        else if (node >= fire::module_order::firstInsert)
            inserts.processSlot(block, node - fire::module_order::firstInsert, params.inserts, lfoOutputs, lfoSampleOffset, useHQ);
    }
    orderTransition.apply(block, dryBuffer);

    // Per-sample Output Gain
    applyGain(buffer,
              paramsForProcessing.outputVal,
              gain,
              outputGainTransition,
              params.outputLfoSourceIndex,
              params.isOutputLinked && ! params.useModernDriveComp,
              params.useModernDriveComp);

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

void BandProcessor::processDriveShapeStage(juce::AudioBuffer<float>& buffer,
    const BandProcessingParameters& params, const juce::AudioBuffer<float>& lfoOutputs,
    int lfoSampleOffset, float inputPeak, bool updateReductionMeter, bool processDrive, bool processShape)
{
    if (processDrive) fillSafePeakEnvelope(buffer);
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto paramsForProcessing = params;
    const bool useHQ = processShape && params.isHQ && oversampling != nullptr;
    paramsForProcessing.isHQ = useHQ;
    const bool hasCompleteBaseRateLfoChunk = lfoSampleOffset + buffer.getNumSamples() <= lfoOutputs.getNumSamples();
    // 2. Core Distortion Processing
    if (useHQ)
    {
        auto oversampledBlock = oversampling->processSamplesUp(block);

        // --- Modulation source upsampling ---
        upsampledLfoOutputs.setSize(juce::jmin(fire::mod_sources::sourceCount, lfoOutputs.getNumChannels()),
                                    static_cast<int>(oversampledBlock.getNumSamples()),
                                    false,
                                    false,
                                    true);
        if (upsampledLfoOutputs.getNumSamples() > 0)
        {
            std::array<bool, fire::mod_sources::sourceCount> neededSources {};
            for (const int source : {params.driveLfoSourceIndex, params.biasLfoSourceIndex,
                                     params.recLfoSourceIndex, params.shapeMixLfoSourceIndex})
                if (juce::isPositiveAndBelow(source, upsampledLfoOutputs.getNumChannels()))
                    neededSources[static_cast<size_t>(source)] = true;
            const int baseSamplesInChunk = buffer.getNumSamples();
            const int oversamplingRatio = baseSamplesInChunk > 0
                                              ? upsampledLfoOutputs.getNumSamples() / baseSamplesInChunk
                                              : 1;
            const int safeOversamplingRatio = juce::jmax(1, oversamplingRatio);

            for (int channel = 0; channel < upsampledLfoOutputs.getNumChannels(); ++channel)
            {
                // Only these four providers run at the oversampled rate.
                // Unassigned/dormant bank channels are never read by this stage.
                if (! neededSources[static_cast<size_t>(channel)]) continue;
                auto* dest = upsampledLfoOutputs.getWritePointer(channel);
                if (lfoOutputs.getNumSamples() <= 0)
                {
                    juce::FloatVectorOperations::clear(dest, upsampledLfoOutputs.getNumSamples());
                    continue;
                }
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
                          updateReductionMeter && processDrive, processDrive, processShape);
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
                          updateReductionMeter && processDrive, processDrive, processShape);
    }

}

void BandProcessor::processCompressorStage(juce::AudioBuffer<float>& buffer, const BandProcessingParameters& params)
{
    auto block = juce::dsp::AudioBlock<float>(buffer);
    const auto& paramsForProcessing = params;
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
}

void BandProcessor::processStereoStage(juce::AudioBuffer<float>& buffer, const BandProcessingParameters& params)
{
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto postDistortionContext = juce::dsp::ProcessContextReplacing<float>(block);
    const auto& paramsForProcessing = params;
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

void BandProcessor::processDistortion(juce::dsp::AudioBlock<float> &blockToProcess,
                                      const BandProcessingParameters &params,
                                      const float *safePeakEnvelopeValues, int safePeakEnvelopeSamples,
                                      float inputPeak, bool updateReductionMeter, bool processDrive,
                                      bool processShape)
{
    const int numSamples = static_cast<int>(blockToProcess.getNumSamples());
    const int numChannels = static_cast<int>(blockToProcess.getNumChannels());
    const int smoothingStride = params.isHQ ? (1 << oversampleFactor) : 1;
    const auto getSafePeak = [&](int sample) noexcept
    {
        if (safePeakEnvelopeValues == nullptr || safePeakEnvelopeSamples <= 0)
            return 0.0f;

        const int baseSample = juce::jlimit(0, safePeakEnvelopeSamples - 1, sample / smoothingStride);
        const float peak = safePeakEnvelopeValues[baseSample];
        return std::isfinite(peak) ? juce::jmax(0.0f, peak) : 0.0f;
    };

    // Shape Mix historically wraps the complete distortion stage, including
    // Drive. Once Shape is disabled its controls are frozen in the UI, so a
    // stale Mix value must not silently blend Drive back to the dry signal.
    // Keep the existing Shape-on sound, but make Shape-off bypass only the
    // Shape controls (Bias/Rectification/DC) rather than the Drive module.
    const bool hasSampleAccurateShapeMix =
        params.isShapeEnabled && params.shapeMixValProvider.lfoSignal != nullptr;
    const float requestedShapeMix =
        hasSampleAccurateShapeMix ? params.shapeMixValProvider.get(0) : params.shapeMixVal;
    const float effectiveShapeMix =
        params.isShapeEnabled ? juce::jlimit(0.0f, 1.0f, requestedShapeMix) : 1.0f;
    if (processShape && !shapeMixSmootherPrimed)
    {
        shapeMixSmoother.setCurrentAndTargetValue(effectiveShapeMix);
        shapeMixSmootherPrimed = true;
    }
    else if (processShape)
    {
        shapeMixSmoother.setTargetValue(effectiveShapeMix);
    }

    // Keep the callback maximum for UI telemetry only. The audible Safe path
    // uses the causal per-sample envelope above; allowing this future-looking
    // maximum to drive the audio made identical timelines sound different at
    // different host callback sizes.
    const float sampleMaxValue = std::isfinite(inputPeak) ? juce::jmax(0.0f, inputPeak) : 0.0f;
    if (processDrive)
        mSampleMaxValue.store(sampleMaxValue, std::memory_order_relaxed);

    const auto normaliseMode = [](int mode) noexcept
    { return juce::isPositiveAndBelow(mode, fire::analog::modeCount) ? mode : 3; };
    const auto serviceModeRequest = [&](int rawRequestedMode)
    {
        const int requestedMode = normaliseMode(rawRequestedMode);
        requestedWaveshaperMode = requestedMode;

        if (!waveshaperModeMixPrimed)
        {
            waveshaperModeSlots = {requestedMode, requestedMode};
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

    if (processShape)
        serviceModeRequest(params.mode);
    auto mode0Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[0]);
    auto mode1Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[1]);

    // The providers are now correctly prepared with LFO signals (if any)
    auto driveProvider = params.driveVal;
    auto biasProvider = params.biasVal;
    auto recProvider = params.recVal;

    if (!params.isShapeEnabled)
    {
        // Disable LFO modulation for Bias and Rectification
        biasProvider.baseValue = 0.0f;
        recProvider.baseValue = 0.0f;
        biasProvider.lfoSignal = nullptr;
        recProvider.lfoSignal = nullptr;
    }

    const auto safeBaseValue = [](float value) noexcept { return std::isfinite(value) ? value : 0.0f; };
    const float driveBaseTargetGain =
        driveValueToGain(safeBaseValue(driveProvider.baseValue), params.isExtremeModeOn);
    const float biasBaseTarget = safeBaseValue(biasProvider.baseValue);
    const float recBaseTarget = safeBaseValue(recProvider.baseValue);

    DistortionLogic::State currentState;
    currentState.mode = params.mode;

    if (processDrive && isFirstBlock)
    {
        driveSmoother.setCurrentAndTargetValue(driveBaseTargetGain);
        isFirstBlock = false;
    }
    if (processShape && !shapeControlsPrimed)
    {
        biasSmoother.setCurrentAndTargetValue(biasBaseTarget);
        recSmoother.setCurrentAndTargetValue(recBaseTarget);
        shapeControlsPrimed = true;
    }
    if (processDrive)
        driveSmoother.setTargetValue(driveBaseTargetGain);
    if (processShape)
    {
        biasSmoother.setTargetValue(biasBaseTarget);
        recSmoother.setTargetValue(recBaseTarget);
    }

    if (processDrive)
    {
        // The legacy disabled path held its only Drive smoother at unity. If the
        // base was edited shortly before re-enabling, playback therefore began a
        // single 50 ms unity-to-current ramp rather than cascading two ramps. Keep
        // hidden route state warm, but snap its base to the latest target at that
        // same enable edge so the independent power bridge retains that contract.
        if (driveControlTransition.enableInitialised && !driveControlTransition.lastDriveEnabled &&
            params.isDriveEnabled)
        {
            driveSmoother.setCurrentAndTargetValue(driveBaseTargetGain);
        }

        serviceDriveRecipeTransition(
            driveControlTransition,
            makeDriveRecipe(driveProvider, params.driveLfoSourceIndex, params.isExtremeModeOn),
            getDriveRouteTargetGain(driveProvider, 0, driveSmoother.getCurrentValue(),
                                    params.isExtremeModeOn));
        serviceDriveEnableTransition(driveControlTransition, params.isDriveEnabled);
        driveCompensation.setMode(params.useModernDriveComp, params.isOutputLinked);
    }

    if (processShape)
    {
        serviceShapeControlRecipeTransition(biasRecipeTransition,
                                            makeShapeControlRecipe(biasProvider, params.biasLfoSourceIndex),
                                            biasProvider.get(0, biasSmoother.getCurrentValue()));
        serviceShapeControlRecipeTransition(recRecipeTransition,
                                            makeShapeControlRecipe(recProvider, params.recLfoSourceIndex),
                                            recProvider.get(0, recSmoother.getCurrentValue()));
    }

    float currentShapeMix = shapeMixSmoother.getCurrentValue();
    float currentRouteDriveGain = driveControlTransition.lastAppliedRouteGain;
    const float legacyDriveForCalc =
        driveValueToExponent(safeBaseValue(driveProvider.baseValue), params.isExtremeModeOn);
    float currentDriveForCalc = legacyDriveForCalc;
    float finalReductionDriveForCalc = 0.0f;
    float finalReductionDriveGain = 1.0f;
    float currentCompensationGain = 1.0f;
    const bool applyDriveCompensation = processDrive
        && (params.useModernDriveComp || driveCompensation.isActive());
    bool hasReductionForRange = false;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (processShape && (sample % smoothingStride) == 0)
        {
            const float targetShapeMix =
                hasSampleAccurateShapeMix ? juce::jlimit(0.0f, 1.0f, params.shapeMixValProvider.get(sample))
                                          : effectiveShapeMix;
            shapeMixSmoother.setTargetValue(targetShapeMix);
            currentShapeMix = shapeMixSmoother.getNextValue();
        }

        // In HQ mode the block contains 4x as many samples, while these
        // smoothers were prepared at the base sample rate. Advance them once
        // per base-rate sample so their time constants do not become 4x faster.
        if ((sample % smoothingStride) == 0)
        {
            if (processDrive)
            {
                // 1. Advance only the ordinary base gain through the established
                // 50 ms linear-gain dezipper. Apply a stable routed LFO after it.
                const float smoothedBaseDriveGain = driveSmoother.getNextValue();
                const float routeTargetGain = getDriveRouteTargetGain(
                    driveProvider, sample, smoothedBaseDriveGain, params.isExtremeModeOn);
                currentRouteDriveGain = applyDriveRecipeTransition(driveControlTransition, routeTargetGain);
                const bool useLegacyDriveForCalc = !driveControlTransition.lastRecipe.routed &&
                                                   !driveControlTransition.routeTransitionMix.isSmoothing();
                currentDriveForCalc = useLegacyDriveForCalc
                                          ? legacyDriveForCalc
                                          : std::log2(juce::jmax(1.0e-12f, currentRouteDriveGain));

                // 2. Power changes retain a 50 ms transition, anchored to the
                // preceding post-Safe audible gain. Its target remains live so a
                // hidden routed LFO is ready when Drive is enabled again.
                const float enabledTargetGain = params.isDriveEnabled ? currentRouteDriveGain : 1.0f;
                const float enabledDriveGain =
                    applyDriveEnableTransition(driveControlTransition, enabledTargetGain);

                // 3. Safe recovery is a held-anchor gain-domain bridge, not a
                // multiplier. If the live request falls below its recovering cap,
                // min() passes that request exactly instead of over-attenuating a
                // downward LFO excursion.
                currentState.drive = applySafeDriveRecovery(driveControlTransition, enabledDriveGain);

                if (params.isDriveEnabled && params.isSafeModeOn)
                {
                    const float causalPeak = getSafePeak(sample);
                    if (causalPeak > 0.0001f)
                    {
                        const float safeCeiling = 2.0f / causalPeak + 0.1f * currentDriveForCalc;
                        if (std::isfinite(safeCeiling) && safeCeiling < currentState.drive)
                        {
                            currentState.drive = safeCeiling;
                            driveControlTransition.safeRecoveryAnchorGain = safeCeiling;
                            driveControlTransition.safeRecoveryMix.setCurrentAndTargetValue(0.0f);
                            driveControlTransition.safeRecoveryMix.setTargetValue(1.0f);
                        }
                    }
                }
                if (applyDriveCompensation)
                    currentCompensationGain = driveCompensation.next(currentState.drive,
                        processShape ? currentShapeMix : 1.0f);
            }
            if (processShape)
            {
                const float biasBase = biasSmoother.getNextValue();
                const float recBase = recSmoother.getNextValue();
                currentState.bias = applyShapeControlRecipeTransition(biasRecipeTransition,
                                                                      biasProvider.get(sample, biasBase));
                currentState.rec =
                    applyShapeControlRecipeTransition(recRecipeTransition, recProvider.get(sample, recBase));
            }
        }

        // Publish the causal Safe result at base rate. Internal oversized
        // chunks share this state, so the meter and the audio follow the same
        // absolute timeline rather than whichever callback peak arrived first.
        if (processDrive && (sample % smoothingStride) == 0 && updateReductionMeter)
        {
            finalReductionDriveForCalc = currentDriveForCalc;
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

            if (processDrive)
                currentSample *= currentState.drive;
            if (processShape)
            {
                currentSample += currentState.bias;

                const auto shapeAndRectify = [&](DistortionLogic::WaveshaperFunction function, size_t bank)
                {
                    auto shaped = currentSample;
                    const auto mode = waveshaperModeSlots[bank];
                    if (mode >= fire::analog::legacyCount)
                    {
                        auto& stage = analogShapeBanks[bank][static_cast<size_t>(juce::jmin(channel, 1))];
                        stage.setProfile(mode, analogShapeBaseRate * smoothingStride);
                        shaped = stage.process(currentSample);
                    }
                    else shaped = function(currentSample);
                    if (shaped < 0.0f)
                        shaped *= negativeScale;
                    return shaped;
                };

                if (modeMix <= 0.0f)
                {
                    currentSample = shapeAndRectify(mode0Function, 0);
                }
                else if (modeMix >= 1.0f)
                {
                    currentSample = shapeAndRectify(mode1Function, 1);
                }
                else
                {
                    const auto mode0Sample = shapeAndRectify(mode0Function, 0);
                    const auto mode1Sample = shapeAndRectify(mode1Function, 1);
                    currentSample = mode0Sample + modeMix * (mode1Sample - mode0Sample);
                }

                currentSample -= currentState.bias;

                // Shape Mix is a base-rate control even though the distortion is
                // evaluated at 4x in HQ mode. Reuse one weight for the complete
                // oversampled frame so its 50 ms ramp has the same wall-clock
                // duration in both quality modes.
                currentSample *= currentShapeMix;
                currentSample += drySample * (1.0f - currentShapeMix);
            }
            if (processDrive && ! juce::exactlyEqual(currentCompensationGain, 1.0f))
                currentSample *= currentCompensationGain;
            blockToProcess.setSample(channel, sample, currentSample);
        }

        // The HQ distortion loop contains four oversampled frames for each
        // base-rate frame.  Hold the same crossfade weight for that whole
        // group and advance only at its end, so the transition remains 10 ms
        // in both modes and across internal chunks.
        const bool completesBaseFrame = ((sample + 1) % smoothingStride) == 0 || sample + 1 == numSamples;
        if (completesBaseFrame)
        {
            if (processDrive)
            {
                driveControlTransition.lastAppliedRouteGain = currentRouteDriveGain;
                driveControlTransition.lastAppliedFinalGain = currentState.drive;
                driveControlTransition.routeTransitionMix.getNextValue();
            }
            if (processShape)
            {
                biasRecipeTransition.lastAppliedValue = currentState.bias;
                recRecipeTransition.lastAppliedValue = currentState.rec;
                biasRecipeTransition.routeTransitionMix.getNextValue();
                recRecipeTransition.routeTransitionMix.getNextValue();
            }
        }

        if (processShape && completesBaseFrame && waveshaperModeMixSmoother.isSmoothing())
        {
            waveshaperModeMixSmoother.getNextValue();
            if (!waveshaperModeMixSmoother.isSmoothing())
            {
                serviceModeRequest(requestedWaveshaperMode);
                mode0Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[0]);
                mode1Function = DistortionLogic::getWaveshaperForMode(waveshaperModeSlots[1]);
            }
        }
    }

    if (processDrive)
    {
        mDriveCompensationDb.store(params.useModernDriveComp ? driveCompensation.getLastGainDb() : 0.0f,
                                  std::memory_order_relaxed);
        if (params.useModernDriveComp)
            mDriveCompensationSequence.fetch_add(1, std::memory_order_release);
        else
            mDriveCompensationSequence.store(0, std::memory_order_release);
    }

    if (hasReductionForRange)
    {
        float reduction = 1.0f;
        if (params.isDriveEnabled && params.isSafeModeOn && std::abs(finalReductionDriveForCalc) > 1.0e-8f)
        {
            reduction = juce::jlimit(0.0f, 1.0f,
                                     std::log2(juce::jmax(finalReductionDriveGain, 1.0e-12f)) /
                                         finalReductionDriveForCalc);
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

float FireAudioProcessor::getSnapshotModulatedValueAtSample(
    const ModulatedParameterSnapshot& parameter,
    const juce::AudioBuffer<float>& lfoOutputs,
    int sampleIndex) const noexcept
{
    const auto& provider = parameter.provider;
    const float baseValue = std::isfinite(provider.baseValue)
                                ? provider.baseValue
                                : 0.0f;
    if (! juce::isPositiveAndBelow(parameter.lfoSourceIndex,
                                    lfoOutputs.getNumChannels())
        || ! juce::isPositiveAndBelow(sampleIndex,
                                      lfoOutputs.getNumSamples()))
        return baseValue;

    const float rawLfoValue = lfoOutputs.getSample(parameter.lfoSourceIndex,
                                                    sampleIndex);
    if (! std::isfinite(rawLfoValue)
        || ! std::isfinite(provider.range.start)
        || ! std::isfinite(provider.range.end)
        || provider.range.end <= provider.range.start)
        return baseValue;

    const float lfoValue = juce::jlimit(0.0f, 1.0f, rawLfoValue);
    const float mappedLfo = provider.isBipolar
                                ? lfoValue * 2.0f - 1.0f
                                : lfoValue;
    const float safeDepth = std::isfinite(provider.modulationDepth)
                                ? juce::jlimit(-1.0f,
                                              1.0f,
                                              provider.modulationDepth)
                                : 0.0f;
    const float effectiveDepth = provider.isBipolar
                                     ? safeDepth * 0.5f
                                     : safeDepth;
    const float rawNormalisedBase = provider.range.convertTo0to1(baseValue);
    const float normalisedBase = std::isfinite(rawNormalisedBase)
                                     ? juce::jlimit(0.0f,
                                                   1.0f,
                                                   rawNormalisedBase)
                                     : 0.0f;
    const float normalisedValue = juce::jlimit(
        0.0f,
        1.0f,
        normalisedBase + mappedLfo * effectiveDepth);
    const float value = provider.range.convertFrom0to1(normalisedValue);
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
    settings.lowCutBypassed = settings.lowCutBypassed || ! isLegacyEqNodeActive(0);
    settings.peakBypassed = settings.peakBypassed || ! isLegacyEqNodeActive(1);
    settings.highCutBypassed = settings.highCutBypassed || ! isLegacyEqNodeActive(2);
    return settings;
}

bool FireAudioProcessor::isLegacyEqNodeActive(int slot) const noexcept
{
    using namespace fire::eq;
    const auto& parameters = eqParameterCache[static_cast<size_t>(slot)];
    return loadCachedParameter(parameters[static_cast<size_t>(Field::present)], 1.0f) > 0.5f
           && juce::roundToInt(loadCachedParameter(parameters[static_cast<size_t>(Field::type)],
                                                   static_cast<float>(defaultType(slot))))
                  == static_cast<int>(defaultType(slot));
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
    settings.lowCutBypassed = settings.lowCutBypassed || ! isLegacyEqNodeActive(0);
    settings.peakBypassed = settings.peakBypassed || ! isLegacyEqNodeActive(1);
    settings.highCutBypassed = settings.highCutBypassed || ! isLegacyEqNodeActive(2);
    return settings;
}

ChainSettings FireAudioProcessor::getSnapshotChainSettingsAtSample(
    const juce::AudioBuffer<float>& lfoOutputs,
    int sampleIndex) const noexcept
{
    const auto& snapshot = activeAudioCallbackParameterSnapshot.globalFilter;
    ChainSettings settings = snapshot.baseSettings;
    settings.lowCutFreq = getSnapshotModulatedValueAtSample(
        snapshot.lowCutFrequency, lfoOutputs, sampleIndex);
    settings.lowCutGainInDecibels = getSnapshotModulatedValueAtSample(
        snapshot.lowCutGain, lfoOutputs, sampleIndex);
    settings.lowCutQuality = getSnapshotModulatedValueAtSample(
        snapshot.lowCutQuality, lfoOutputs, sampleIndex);
    settings.peakFreq = getSnapshotModulatedValueAtSample(
        snapshot.peakFrequency, lfoOutputs, sampleIndex);
    settings.peakGainInDecibels = getSnapshotModulatedValueAtSample(
        snapshot.peakGain, lfoOutputs, sampleIndex);
    settings.peakQuality = getSnapshotModulatedValueAtSample(
        snapshot.peakQuality, lfoOutputs, sampleIndex);
    settings.highCutFreq = getSnapshotModulatedValueAtSample(
        snapshot.highCutFrequency, lfoOutputs, sampleIndex);
    settings.highCutGainInDecibels = getSnapshotModulatedValueAtSample(
        snapshot.highCutGain, lfoOutputs, sampleIndex);
    settings.highCutQuality = getSnapshotModulatedValueAtSample(
        snapshot.highCutQuality, lfoOutputs, sampleIndex);
    return settings;
}

bool FireAudioProcessor::hasActiveFilterModulation() const noexcept
{
    if (lfoOutputBuffer.getNumSamples() <= 0)
        return false;

    const auto& filter = activeAudioCallbackParameterSnapshot.globalFilter;
    const std::array<const ModulatedParameterSnapshot*, 9> parameters {
        &filter.lowCutFrequency,
        &filter.lowCutGain,
        &filter.lowCutQuality,
        &filter.peakFrequency,
        &filter.peakGain,
        &filter.peakQuality,
        &filter.highCutFrequency,
        &filter.highCutGain,
        &filter.highCutQuality
    };

    for (const auto* parameter : parameters)
    {
        if (juce::isPositiveAndBelow(parameter->lfoSourceIndex,
                                        lfoOutputBuffer.getNumChannels())
            && std::isfinite(parameter->provider.modulationDepth)
            && std::abs(parameter->provider.modulationDepth)
                   > std::numeric_limits<float>::epsilon())
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
        parameters.shapeModel = cacheParameter(fire::analog_params::bandID(i));
        parameters.linked = indexed(LINKED_ID, i);
        parameters.modernDriveComp = cacheParameter(fire::drive_comp::parameterID(i));
        parameters.safe = indexed(SAFE_ID, i);
        parameters.extreme = indexed(EXTREME_ID, i);
        parameters.driveEnabled = indexed(DRIVE_BYPASS_ID, i);
        parameters.shapeEnabled = indexed(SHAPE_BYPASS_ID, i);
        parameters.compressorEnabled = indexed(COMP_BYPASS_ID, i);
        parameters.ottEnabled = indexed(OTT_ENABLED_ID, i);
        for (size_t control = 0; control < parameters.ottControls.size(); ++control)
            parameters.ottControls[control] = indexed(ParameterIDAndName::ottControlIDs[control], i);
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

    }
    for (int i = 0; i < fire::lfo_bank::capacity; ++i)
        lfoSmoothParameters[static_cast<size_t>(i)] = indexed(LFO_SMOOTH_ID, i).raw;

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
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int node = 0; node < fire::module_order::capacity; ++node)
            if (fire::module_order::valid(scope, node))
                moduleOrderParameters[static_cast<size_t>(scope)][static_cast<size_t>(node)] = cacheParameter(fire::module_order::parameterID(scope, node));
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
    {
        for (int node = 0; node < (scope == 0 ? 3 : 5); ++node)
            modulePresenceParameters[static_cast<size_t>(scope)][static_cast<size_t>(node)] = cacheParameter(fire::core_modules::presenceID(scope, node));
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
            for (int field = 0; field < fire::core_modules::slotFieldCount; ++field)
                coreModuleParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)][static_cast<size_t>(field)]
                    = cacheParameter(fire::core_modules::parameterID(scope, slot, field));
    }
    for (size_t i = 0; i < tapeParameters.size(); ++i) tapeParameters[i] = cacheParameter(fire::effects::tapeIDs[i]);
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
            for (int field = 0; field < fire::effects::fieldCount; ++field)
                insertParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)][static_cast<size_t>(field)]
                    = cacheParameter(fire::effects::parameterID(scope, slot, field));
    for (int scope = 0; scope < fire::clouds_params::scopeCount; ++scope)
        for (int slot = 0; slot < fire::clouds_params::slotCount; ++slot)
            for (int field = 0; field < fire::clouds_params::fieldCount; ++field)
                cloudsParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)][static_cast<size_t>(field)]
                    = cacheParameter(fire::clouds_params::parameterID(scope, slot, field));
    for (int scope = 0; scope < fire::modulation_fx::scopeCount; ++scope)
        for (int slot = 0; slot < fire::modulation_fx::slotCount; ++slot)
            modulationEffectParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)]
                = cacheParameter(fire::modulation_fx::parameterID(scope, slot));
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            shapeModelParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)] = cacheParameter(fire::analog_params::parameterID(scope, slot));
            analogDriveParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)] = cacheParameter(fire::analog_params::driveID(scope, slot));
            reverbModelParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)] = cacheParameter(fire::reverb_params::parameterID(scope, slot));
        }
    for (int scope = 0; scope < fire::resonator_params::scopeCount; ++scope)
        for (int slot = 0; slot < fire::resonator_params::slotCount; ++slot)
            resonatorParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)]
                = cacheParameter(fire::resonator_params::parameterID(scope, slot));

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
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
        for (int field = 0; field < fire::eq::fieldCount; ++field)
            eqParameterCache[static_cast<size_t>(slot)][static_cast<size_t>(field)] =
                cacheParameter(fire::eq::parameterID(slot, static_cast<fire::eq::Field>(field)));
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
    for (auto& generation : eqNodeGenerations) generation.store(0u, std::memory_order_relaxed);
    serializableMainStateReady.store(true, std::memory_order_release);
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
    statePresets.enableFactoryPresets();
    editHistory = std::make_unique<fire::state::EditHistory>(*this,
        [this]
        {
            juce::MemoryBlock snapshot;
            getStateInformation(snapshot);
            return snapshot;
        },
        [this](const juce::MemoryBlock& snapshot)
        {
            auto xml = getXmlFromBinary(snapshot.getData(), static_cast<int>(snapshot.getSize()));
            if (xml == nullptr) return;
            if (auto* other = xml->getChildByName("otherState"))
            {
                const auto size = getSavedEditorSize();
                other->setAttribute("editorWidth", size.width);
                other->setAttribute("editorHeight", size.height);
            }
            juce::MemoryBlock restored;
            copyXmlToBinary(*xml, restored);
            setStateInformation(restored.getData(), static_cast<int>(restored.getSize()));
        });
    lfoManager->onStateEdited = [this] { if (editHistory) editHistory->changed(); };
    startTimerHz(30);
}

FireAudioProcessor::~FireAudioProcessor()
{
    stopTimer();
    lfoManager->onStateEdited = {};
    editHistory.reset();
}

bool FireAudioProcessor::canUndoEdit() const noexcept { return editHistory && editHistory->canUndo(); }
bool FireAudioProcessor::canRedoEdit() const noexcept { return editHistory && editHistory->canRedo(); }
bool FireAudioProcessor::undoEdit()
{
    const bool changed = editHistory && editHistory->undo();
    if (changed)
    {
        editHistory->ignoreNextHostDisplayChange();
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
    }
    return changed; // The host notification may destroy the processor.
}
bool FireAudioProcessor::redoEdit()
{
    const bool changed = editHistory && editHistory->redo();
    if (changed)
    {
        editHistory->ignoreNextHostDisplayChange();
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
    }
    return changed;
}
void FireAudioProcessor::checkpointEditHistory() noexcept { if (editHistory) editHistory->checkpoint(); }

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
    double master = 0, bandsTail = 0;
    const auto activeBands = juce::jlimit(1, 4, juce::roundToInt(loadCachedParameter(numBandsParameter, 1.0f)));
    bool anySolo = false;
    for (int band = 0; band < activeBands; ++band)
        anySolo = anySolo || loadCachedParameter(bandParameterCache[static_cast<size_t>(band)].solo) > 0.5f;
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
    {
        double tail = 0;
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            const auto type = getInsertEffectType(scope, slot);
            if (type == fire::effects::Type::delay) tail += 180.0;
            else if (type == fire::effects::Type::reverb) tail += 60.0;
            else if (type == fire::effects::Type::granular)
            {
                const auto& clouds = cloudsParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)];
                const auto& insert = insertParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)];
                const bool cloudsEnabled = loadCachedParameter(insert[fire::effects::enabledField]) > 0.5f;
                const bool audibleScope = scope == 0 || (scope <= activeBands
                    && loadCachedParameter(bandParameterCache[static_cast<size_t>(scope - 1)].enabled) > 0.5f
                    && (! anySolo || loadCachedParameter(bandParameterCache[static_cast<size_t>(scope - 1)].solo) > 0.5f));
                if (cloudsEnabled && audibleScope
                    && (loadCachedParameter(clouds[fire::clouds_params::freezeField]) > 0.5f
                        || loadCachedParameter(clouds[fire::clouds_params::feedbackField]) >= 1.0f))
                    return std::numeric_limits<double>::infinity();
                // Keep a conservative finite tail while a disabled slot's
                // existing wet state completes its short bypass fade.
                tail += 180.0;
            }
            else if (type == fire::effects::Type::chorus) tail += 2.0;
            else if (type == fire::effects::Type::flanger) tail += 2.0;
            else if (type == fire::effects::Type::phaser) tail += 8.0;
            else if (type == fire::effects::Type::chordResonator) tail += 8.0;
            else if (type == fire::effects::Type::lofi) tail += 0.05;
        }
        if (scope == 0) master = tail; else bandsTail = juce::jmax(bandsTail, tail);
    }
    return master + bandsTail;
}

fire::effects::Type FireAudioProcessor::getInsertEffectType(int scope, int slot) const
{
    if (! juce::isPositiveAndBelow(scope, fire::effects::scopeCount) || ! juce::isPositiveAndBelow(slot, fire::effects::slotCount))
        return fire::effects::Type::none;
    const auto core = fire::core_modules::decodeType(juce::roundToInt(loadCachedParameter(
        coreModuleParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)][fire::core_modules::typeField])));
    if (core != fire::effects::Type::none) return core;
    if (loadCachedParameter(resonatorParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)]) > 0.5f)
        return fire::effects::Type::chordResonator;
    const auto extension = juce::roundToInt(loadCachedParameter(
        modulationEffectParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)]));
    if (extension == 1) return fire::effects::Type::flanger;
    if (extension == 2) return fire::effects::Type::phaser;
    return static_cast<fire::effects::Type>(juce::jlimit(0, 5, juce::roundToInt(loadCachedParameter(
        insertParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)][fire::effects::typeField]))));
}

fire::eq::NodeState FireAudioProcessor::getEqNodeState(int slot, int fxScope, int fxSlot) const
{
    if (fxSlot < 0) return fire::eq::readNode(treeState, slot);
    auto node = fire::eq::defaultNode(slot);
    if (!fire::eq::validSlot(slot) || getInsertEffectType(fxScope, fxSlot) != fire::effects::Type::eq)
    {node.present = false; return node;}
    const auto read = [&](fire::eq::Field field)
    {
        auto* value = treeState.getRawParameterValue(fire::core_modules::eqParameterID(fxScope, fxSlot, slot, field));
        return value ? value->load(std::memory_order_relaxed) : 0.0f;
    };
    node.present = read(fire::eq::Field::present) > 0.5f;
    node.bypassed = read(fire::eq::Field::bypassed) > 0.5f;
    node.type = static_cast<fire::eq::Type>(juce::jlimit(0, 6, juce::roundToInt(read(fire::eq::Field::type))));
    node.slope = juce::jlimit(0, 3, juce::roundToInt(read(fire::eq::Field::slope)));
    node.frequency = read(fire::eq::Field::frequency); node.gainDb = read(fire::eq::Field::gain); node.q = read(fire::eq::Field::q);
    return node;
}

int FireAudioProcessor::addEqNode(float frequency, float gainDb, fire::eq::Type type, int fxScope, int fxSlot)
{
    using namespace fire::eq;
    if (fxSlot >= 0 && getInsertEffectType(fxScope, fxSlot) != fire::effects::Type::eq) return -1;
    const auto id = [&](int node, Field field) {return fxSlot < 0 ? parameterID(node, field) : fire::core_modules::eqParameterID(fxScope, fxSlot, node, field);};
    if (! std::isfinite(frequency) || ! std::isfinite(gainDb)
        || static_cast<int>(type) < 0 || static_cast<int>(type) >= static_cast<int>(typeNames.size())) return -1;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish { [this] { finishMainStateEdit(false); } };
    int slot = -1;
    for (int index = 0; index < maxNodes; ++index)
        if (! getEqNodeState(index, fxScope, fxSlot).present) { slot = index; break; }
    if (slot < 0) return -1;
    const auto write = [&](Field field, float value)
    {
        auto* parameter = treeState.getParameter(id(slot, field));
        if (parameter == nullptr) return;
        const auto& range = parameter->getNormalisableRange();
        const auto safeValue = range.snapToLegalValue(juce::jlimit(range.start, range.end, value));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(safeValue));
        parameter->endChangeGesture();
    };
    // A deleted stable slot can carry historical automation values. Clear only
    // its old LFO assignments before publishing its new active identity.
    for (auto field : {Field::frequency, Field::gain, Field::q})
        clearModulationForParameter(id(slot, field));
    write(Field::frequency, frequency);
    write(Field::gain, gainDb);
    write(Field::q, slot < 3 ? 1.0f : 0.70710678f);
    write(Field::slope, 0.0f);
    write(Field::type, static_cast<float>(type));
    write(Field::bypassed, 0.0f);
    if (fxSlot < 0) eqNodeGenerations[static_cast<size_t>(slot)].fetch_add(1u, std::memory_order_relaxed);
    write(Field::present, 1.0f);
    if (auto* enabled = treeState.getParameter(fxSlot < 0 ? juce::String(FILTER_BYPASS_ID) : fire::effects::parameterID(fxScope, fxSlot, fire::effects::enabledField)))
    {
        enabled->beginChangeGesture();
        enabled->setValueNotifyingHost(1.0f);
        enabled->endChangeGesture();
    }
    return slot;
}

bool FireAudioProcessor::removeEqNode(int slot, int fxScope, int fxSlot)
{
    using namespace fire::eq;
    if (fxSlot >= 0 && getInsertEffectType(fxScope, fxSlot) != fire::effects::Type::eq) return false;
    const auto id = [&](int node, Field field) {return fxSlot < 0 ? parameterID(node, field) : fire::core_modules::eqParameterID(fxScope, fxSlot, node, field);};
    if (! validSlot(slot)) return false;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish { [this] { finishMainStateEdit(false); } };
    if (! getEqNodeState(slot, fxScope, fxSlot).present) return false;
    auto* present = treeState.getParameter(id(slot, Field::present));
    if (present == nullptr) return false;
    present->beginChangeGesture();
    present->setValueNotifyingHost(0.0f);
    present->endChangeGesture();
    for (auto field : {Field::frequency, Field::gain, Field::q})
        clearModulationForParameter(id(slot, field));
    return true;
}

int FireAudioProcessor::getInsertEffectOrder(int scope, int slot) const
{
    if (! juce::isPositiveAndBelow(scope, fire::effects::scopeCount) || ! juce::isPositiveAndBelow(slot, fire::effects::slotCount)) return 0;
    return juce::roundToInt(loadCachedParameter(insertParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)][fire::effects::orderField]));
}

namespace
{
juce::StringArray legacyModuleParameters(int scope, int node)
{
    juce::StringArray ids;
    if (! fire::core_modules::validLegacy(scope, node)) return ids;
    if (scope == 0)
    {
        if (node == 0)
        {
            ids.add(FILTER_BYPASS_ID);
            for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
                for (int field = 0; field < fire::eq::fieldCount; ++field)
                    ids.add(fire::eq::parameterID(slot, static_cast<fire::eq::Field>(field)));
        }
        else if (node == 1)
        {
            for (const auto* id : {DOWNSAMPLE_BYPASS_ID, DOWNSAMPLE_ID, BIT_DEPTH_ID, JITTER_ID, DOWNSAMPLE_MIX_ID}) ids.add(id);
            for (const auto* id : fire::effects::tapeIDs) ids.add(id);
        }
        return ids;
    }
    const auto add = [&](const char* id) {ids.add(ParameterIDAndName::getIDString(id, scope - 1));};
    if (node == 0)
    {
        for (const auto* id : {DRIVE_BYPASS_ID, DRIVE_ID, SAFE_ID, EXTREME_ID, LINKED_ID}) add(id);
        ids.add(fire::drive_comp::parameterID(scope - 1));
    }
    else if (node == 1)
    {
        for (const auto* id : {SHAPE_BYPASS_ID, MODE_ID, BIAS_ID, REC_ID, SHAPE_MIX_ID, DC_FILTER_ID}) add(id);
        ids.add(fire::analog_params::bandID(scope - 1));
    }
    else if (node == 2)
        for (const auto* id : {COMP_BYPASS_ID, COMP_THRESH_ID, COMP_RATIO_ID, COMP_ATTACK_ID, COMP_RELEASE_ID, COMP_MIX_ID}) add(id);
    else if (node == 3)
        for (const auto* id : {WIDTH_BYPASS_ID, WIDTH_ID, PAN_ID, WIDTH_MIX_ID}) add(id);
    else
    {
        add(OTT_ENABLED_ID);
        for (const auto* id : ParameterIDAndName::ottControlIDs) add(id);
    }
    return ids;
}
}

int FireAudioProcessor::getShapeMode(int scope, int slot) const
{
    const auto read = [](const CachedParameter& parameter)
    {return parameter.ranged ? parameter.ranged->convertFrom0to1(parameter.ranged->getValue()) : 0.0f;};
    if (slot < 0)
    {
        if (scope < 1 || scope > 4) return 3;
        const auto& cache = bandParameterCache[static_cast<size_t>(scope - 1)];
        return fire::analog::resolve(juce::roundToInt(read(cache.mode)), juce::roundToInt(read(cache.shapeModel)));
    }
    if (getInsertEffectType(scope, slot) != fire::effects::Type::shape) return 3;
    const auto legacy = juce::roundToInt(read(insertParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)][0]) * 11);
    return fire::analog::resolve(legacy, juce::roundToInt(read(shapeModelParameters[static_cast<size_t>(scope)][static_cast<size_t>(slot)])));
}
bool FireAudioProcessor::setShapeMode(int scope, int slot, int mode)
{
    if (mode < 0 || mode >= fire::analog::modeCount || (slot < 0 ? scope < 1 || scope > 4 : getInsertEffectType(scope, slot) != fire::effects::Type::shape)) return false;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish {[this] {finishMainStateEdit(false);}};
    auto* legacy = treeState.getParameter(slot < 0 ? ParameterIDAndName::getIDString(MODE_ID, scope - 1) : fire::effects::parameterID(scope, slot, 0));
    auto* model = treeState.getParameter(slot < 0 ? fire::analog_params::bandID(scope - 1) : fire::analog_params::parameterID(scope, slot));
    const auto write = [](juce::RangedAudioParameter* parameter, float value)
    {parameter->beginChangeGesture(); parameter->setValueNotifyingHost(value); parameter->endChangeGesture();};
    if (mode < fire::analog::legacyCount)
        write(legacy, slot < 0 ? legacy->convertTo0to1(static_cast<float>(mode)) : static_cast<float>(mode) / 11);
    write(model, model->convertTo0to1(mode < fire::analog::legacyCount ? 0.0f : static_cast<float>(mode - fire::analog::legacyCount + 1)));
    return true;
}

bool FireAudioProcessor::isModulePresent(int scope, int node) const
{
    if (! fire::module_order::valid(scope, node)) return false;
    if (node >= fire::module_order::firstInsert)
        return getInsertEffectType(scope, node - fire::module_order::firstInsert) != fire::effects::Type::none;
    return fire::core_modules::validLegacy(scope, node)
        && loadCachedParameter(modulePresenceParameters[static_cast<size_t>(scope)][static_cast<size_t>(node)], 1) > 0.5f;
}
void FireAudioProcessor::removeModule(int scope, int node)
{
    if (! isModulePresent(scope, node)) return;
    if (node >= fire::module_order::firstInsert) {removeInsertEffect(scope, node - fire::module_order::firstInsert); return;}
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish {[this] {requestMultibandTopologyReset();}};
    const auto ids = legacyModuleParameters(scope, node);
    for (const auto& id : ids) clearModulationForParameter(id);
    if (!ids.isEmpty())
        if (auto* enabled = treeState.getParameter(ids[0]))
        {enabled->beginChangeGesture(); enabled->setValueNotifyingHost(0); enabled->endChangeGesture();}
    if (scope > 0 && node == 0)
        if (auto* linked = treeState.getParameter(ParameterIDAndName::getIDString(LINKED_ID, scope - 1)))
        {linked->beginChangeGesture(); linked->setValueNotifyingHost(0); linked->endChangeGesture();}
    auto* parameter = treeState.getParameter(fire::core_modules::presenceID(scope, node));
    parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();
}
bool FireAudioProcessor::restoreLegacyModule(int scope, int node)
{
    if (! fire::core_modules::validLegacy(scope, node)) return false;
    if (isModulePresent(scope, node)) return true;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish {[this] {requestMultibandTopologyReset();}};
    for (const auto& id : legacyModuleParameters(scope, node))
    {
        clearModulationForParameter(id);
        if (auto* parameter = treeState.getParameter(id))
        {
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->getDefaultValue()); parameter->endChangeGesture();
        }
    }
    auto* parameter = treeState.getParameter(fire::core_modules::presenceID(scope, node));
    parameter->beginChangeGesture(); parameter->setValueNotifyingHost(1); parameter->endChangeGesture();
    auto nodes = visibleModuleOrder(scope);
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end()); nodes.push_back(node);
    for (int id : getModuleOrder(scope)) if (id >= 0 && std::find(nodes.begin(), nodes.end(), id) == nodes.end()) nodes.push_back(id);
    fire::module_order::Order order; order.fill(-1);
    std::copy(nodes.begin(), nodes.end(), order.begin()); writeModuleOrder(scope, order);
    return true;
}

fire::module_order::Order FireAudioProcessor::getModuleOrder(int scope) const
{
    using namespace fire;
    auto order = module_order::defaults(scope);
    if (! juce::isPositiveAndBelow(scope, effects::scopeCount)) { order.fill(-1); return order; }
    const int builtins = scope == 0 ? 3 : 5;
    // Read each atomic parameter once. Besides avoiding repeated loads in the
    // sort comparator, this keeps its ordering consistent during automation.
    std::array<int, effects::slotCount> insertPositions;
    for (int slot = 0; slot < effects::slotCount; ++slot)
        insertPositions[static_cast<size_t>(slot)] = getInsertEffectOrder(scope, slot);
    std::sort(order.begin() + builtins, order.begin() + builtins + effects::slotCount, [&](int a, int b) {
        const auto av = insertPositions[static_cast<size_t>(a - module_order::firstInsert)];
        const auto bv = insertPositions[static_cast<size_t>(b - module_order::firstInsert)];
        return av == bv ? a < b : av < bv;
    });
    std::array<int, module_order::capacity> legacyPositions {}, positions {};
    for (int index = 0; index < builtins + effects::slotCount; ++index)
    {
        const auto node = static_cast<size_t>(order[static_cast<size_t>(index)]);
        legacyPositions[node] = index;
        const auto stored = juce::roundToInt(juce::jlimit(-1.0f, 12.0f,
            loadCachedParameter(moduleOrderParameters[static_cast<size_t>(scope)][node], -1)));
        positions[node] = stored < 0 ? index : stored;
    }
    std::sort(order.begin(), order.begin() + builtins + effects::slotCount, [&](int a, int b) {
        const auto ai = static_cast<size_t>(a), bi = static_cast<size_t>(b);
        return positions[ai] == positions[bi] ? legacyPositions[ai] < legacyPositions[bi] : positions[ai] < positions[bi];
    });
    return order;
}

std::vector<int> FireAudioProcessor::visibleModuleOrder(int scope) const
{
    std::vector<int> result;
    for (int node : getModuleOrder(scope))
        if (isModulePresent(scope, node)) result.push_back(node);
    return result;
}

void FireAudioProcessor::writeModuleOrder(int scope, const fire::module_order::Order& order)
{
    int insertPosition = 0;
    for (size_t position = 0; position < order.size(); ++position)
    {
        const int node = order[position];
        if (! fire::module_order::valid(scope, node)) continue;
        const auto write = [](juce::RangedAudioParameter* parameter, float value) {
            if (! parameter || juce::approximatelyEqual(parameter->getValue(), parameter->convertTo0to1(value))) return;
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); parameter->endChangeGesture();
        };
        write(treeState.getParameter(fire::module_order::parameterID(scope, node)), static_cast<float>(position));
        if (node >= fire::module_order::firstInsert && getInsertEffectType(scope, node - fire::module_order::firstInsert) != fire::effects::Type::none)
            write(treeState.getParameter(fire::effects::parameterID(scope, node - fire::module_order::firstInsert, fire::effects::orderField)), static_cast<float>(++insertPosition));
    }
}

void FireAudioProcessor::moveModuleBefore(int scope, int node, int beforeNode)
{
    if (! fire::module_order::valid(scope, node) || beforeNode == node) return;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish {[this] {requestMultibandTopologyReset();}};
    auto visible = visibleModuleOrder(scope);
    const auto previous = visible;
    const auto source = std::find(visible.begin(), visible.end(), node);
    if (source == visible.end()) return;
    visible.erase(source);
    const auto target = std::find(visible.begin(), visible.end(), beforeNode);
    if (beforeNode >= 0 && target == visible.end()) return;
    visible.insert(target, node);
    if (visible == previous) return;
    for (int id : getModuleOrder(scope))
        if (id >= 0 && std::find(visible.begin(), visible.end(), id) == visible.end()) visible.push_back(id);
    fire::module_order::Order order; order.fill(-1);
    std::copy(visible.begin(), visible.end(), order.begin());
    writeModuleOrder(scope, order);
}

void FireAudioProcessor::moveModuleBy(int scope, int node, int direction)
{
    if (! fire::module_order::valid(scope, node) || direction == 0) return;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish {[this] {requestMultibandTopologyReset();}};
    const auto visible = visibleModuleOrder(scope);
    const auto found = std::find(visible.begin(), visible.end(), node);
    if (found == visible.end()) return;
    const int index = static_cast<int>(std::distance(visible.begin(), found));
    if (direction < 0 && index > 0) moveModuleBefore(scope, node, visible[static_cast<size_t>(index - 1)]);
    else if (direction > 0 && index + 1 < static_cast<int>(visible.size()))
        moveModuleBefore(scope, node, index + 2 < static_cast<int>(visible.size()) ? visible[static_cast<size_t>(index + 2)] : -1);
}

int FireAudioProcessor::addInsertEffect(int scope, fire::effects::Type type)
{
    using namespace fire::effects;
    if (! juce::isPositiveAndBelow(scope, scopeCount) || type <= Type::none || type >= Type::count) return -1;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish { [this] { requestMultibandTopologyReset(); } };
    int freeSlot = -1;
    std::vector<int> active;
    for (int slot = 0; slot < slotCount; ++slot)
        if (getInsertEffectType(scope, slot) == Type::none) { if (freeSlot < 0) freeSlot = slot; }
        else active.push_back(slot);
    if (freeSlot < 0) return -1;
    auto& rack = scope == 0 ? masterInserts : bands[static_cast<size_t>(scope - 1)]->inserts;
    rack.stageFrozenRecording(freeSlot, {}, multibandTopologyResetGeneration.load(std::memory_order_seq_cst) + 1u);
    std::stable_sort(active.begin(), active.end(), [&](int a, int b) {return getInsertEffectOrder(scope, a) < getInsertEffectOrder(scope, b);});
    const auto write = [&](int slot, int field, float value) {
        auto* parameter = treeState.getParameter(parameterID(scope, slot, field));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    };
    for (size_t i = 0; i < active.size(); ++i) write(active[i], orderField, static_cast<float>(i + 1));
    for (int control = 0; control < static_cast<int>(controlCount); ++control)
    {
        clearModulationForParameter(parameterID(scope, freeSlot, control));
        const auto& descriptor = controls(type)[static_cast<size_t>(control)];
        write(freeSlot, control, descriptor.toNormalised(descriptor.initial));
    }
    for (int field = 0; field < fire::clouds_params::fieldCount; ++field)
    {
        const auto id = fire::clouds_params::parameterID(scope, freeSlot, field);
        clearModulationForParameter(id);
        auto* parameter = treeState.getParameter(id);
        const auto value = fire::clouds_params::defaults[static_cast<size_t>(field)];
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
    write(freeSlot, orderField, static_cast<float>(active.size() + 1));
    write(freeSlot, enabledField, 1);
    const int extension = type == Type::flanger ? 1 : type == Type::phaser ? 2 : 0;
    const bool isResonator = type == Type::chordResonator;
    write(freeSlot, typeField, extension == 0 && ! isResonator && ! isCore(type) ? static_cast<float>(type) : 0.0f);
    auto* extendedType = treeState.getParameter(fire::modulation_fx::parameterID(scope, freeSlot));
    extendedType->beginChangeGesture();
    extendedType->setValueNotifyingHost(extendedType->convertTo0to1(static_cast<float>(extension)));
    extendedType->endChangeGesture();
    auto* resonator = treeState.getParameter(fire::resonator_params::parameterID(scope, freeSlot));
    resonator->beginChangeGesture();
    resonator->setValueNotifyingHost(isResonator ? 1.0f : 0.0f);
    resonator->endChangeGesture();
    for (int field = 0; field < fire::core_modules::slotFieldCount; ++field)
    {
        const auto id = fire::core_modules::parameterID(scope, freeSlot, field);
        clearModulationForParameter(id);
        auto* parameter = treeState.getParameter(id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(field == fire::core_modules::typeField
            ? parameter->convertTo0to1(static_cast<float>(fire::core_modules::encodeType(type))) : parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
    const auto driveID = fire::analog_params::driveID(scope, freeSlot);
    clearModulationForParameter(driveID);
    auto* analogDrive = treeState.getParameter(driveID);
    analogDrive->beginChangeGesture(); analogDrive->setValueNotifyingHost(0); analogDrive->endChangeGesture();
    auto* reverbModel = treeState.getParameter(fire::reverb_params::parameterID(scope, freeSlot));
    reverbModel->beginChangeGesture(); reverbModel->setValueNotifyingHost(0); reverbModel->endChangeGesture();
    auto* shapeModel = treeState.getParameter(fire::analog_params::parameterID(scope, freeSlot));
    shapeModel->beginChangeGesture(); shapeModel->setValueNotifyingHost(0); shapeModel->endChangeGesture();
    // Reused storage slots still append to the visible chain, irrespective of
    // where their previous instance was located.
    auto nodes = visibleModuleOrder(scope);
    const int node = fire::module_order::firstInsert + freeSlot;
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
    nodes.push_back(node);
    for (int id : getModuleOrder(scope))
        if (id >= 0 && std::find(nodes.begin(), nodes.end(), id) == nodes.end()) nodes.push_back(id);
    fire::module_order::Order order; order.fill(-1);
    std::copy(nodes.begin(), nodes.end(), order.begin());
    writeModuleOrder(scope, order);
    return freeSlot;
}

void FireAudioProcessor::removeInsertEffect(int scope, int slot)
{
    using namespace fire::effects;
    if (getInsertEffectType(scope, slot) == Type::none) return;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish { [this] { requestMultibandTopologyReset(); } };
    auto& rack = scope == 0 ? masterInserts : bands[static_cast<size_t>(scope - 1)]->inserts;
    rack.stageFrozenRecording(slot, {}, multibandTopologyResetGeneration.load(std::memory_order_seq_cst) + 1u);
    for (int control = 0; control < static_cast<int>(controlCount); ++control)
        clearModulationForParameter(parameterID(scope, slot, control));
    for (int field = 0; field < fire::clouds_params::fieldCount; ++field)
    {
        const auto id = fire::clouds_params::parameterID(scope, slot, field);
        clearModulationForParameter(id);
        auto* extension = treeState.getParameter(id);
        extension->beginChangeGesture();
        extension->setValueNotifyingHost(extension->getDefaultValue());
        extension->endChangeGesture();
    }
    for (int field = 0; field < fire::core_modules::slotFieldCount; ++field)
    {
        const auto id = fire::core_modules::parameterID(scope, slot, field);
        clearModulationForParameter(id);
        auto* extra = treeState.getParameter(id);
        extra->beginChangeGesture(); extra->setValueNotifyingHost(extra->getDefaultValue()); extra->endChangeGesture();
    }
    const auto driveID = fire::analog_params::driveID(scope, slot);
    clearModulationForParameter(driveID);
    auto* analogDrive = treeState.getParameter(driveID);
    analogDrive->beginChangeGesture(); analogDrive->setValueNotifyingHost(0); analogDrive->endChangeGesture();
    auto* reverbModel = treeState.getParameter(fire::reverb_params::parameterID(scope, slot));
    reverbModel->beginChangeGesture(); reverbModel->setValueNotifyingHost(0); reverbModel->endChangeGesture();
    auto* shapeModel = treeState.getParameter(fire::analog_params::parameterID(scope, slot));
    shapeModel->beginChangeGesture(); shapeModel->setValueNotifyingHost(0); shapeModel->endChangeGesture();
    auto* parameter = treeState.getParameter(parameterID(scope, slot, typeField));
    parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();
    auto* extendedType = treeState.getParameter(fire::modulation_fx::parameterID(scope, slot));
    extendedType->beginChangeGesture(); extendedType->setValueNotifyingHost(0); extendedType->endChangeGesture();
    auto* resonator = treeState.getParameter(fire::resonator_params::parameterID(scope, slot));
    resonator->beginChangeGesture(); resonator->setValueNotifyingHost(0); resonator->endChangeGesture();
}

void FireAudioProcessor::moveInsertEffect(int scope, int slot, int direction)
{
    if (direction != 0) moveInsertEffectInternal(scope, slot, direction > 0 ? 1 : -1, true);
}

fire::dsp::LoudnessMatchState::View FireAudioProcessor::getLoudnessMatchState() const noexcept
{
    return loudnessMatch.view(stateAB.isCurrentA() ? 0 : 1, isBypassed.load(std::memory_order_acquire));
}

float FireAudioProcessor::getBandDriveCompensationDb(int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(bandIndex, static_cast<int>(bands.size())))
        return std::numeric_limits<float>::quiet_NaN();
    const auto* band = bands[static_cast<size_t>(bandIndex)].get();
    if (band == nullptr || band->mDriveCompensationSequence.load(std::memory_order_acquire) == 0)
        return std::numeric_limits<float>::quiet_NaN();
    return band->mDriveCompensationDb.load(std::memory_order_relaxed);
}

std::uint64_t FireAudioProcessor::getBandDriveCompensationSequence(int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(bandIndex, static_cast<int>(bands.size()))) return 0;
    const auto* band = bands[static_cast<size_t>(bandIndex)].get();
    return band != nullptr ? band->mDriveCompensationSequence.load(std::memory_order_acquire) : 0;
}

void FireAudioProcessor::upgradeBandDriveCompensation(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, 4)) return;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish { [this] { finishMainStateEdit(false); } };
    for (const auto& id : {fire::drive_comp::parameterID(bandIndex),
                          ParameterIDAndName::getIDString(LINKED_ID, bandIndex)})
        if (auto* parameter = treeState.getParameter(id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(1.0f);
            parameter->endChangeGesture();
        }
}

void FireAudioProcessor::setLoudnessMatchEnabled(bool enabled)
{
    loudnessMatch.setEnabled(enabled, stateAB.isCurrentA() ? 0 : 1);
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
}

void FireAudioProcessor::learnLoudnessMatch()
{
    const auto current = getLoudnessMatchState();
    if (current.measuring)
        loudnessMatch.cancelMeasurement(current.side);
    else
        loudnessMatch.requestLearn(current.side);
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
}

void FireAudioProcessor::clearCurrentLoudnessMatch() noexcept
{
    loudnessMatch.cancelMeasurements();
    loudnessMatch.clearSide(stateAB.isCurrentA() ? 0 : 1);
}

void FireAudioProcessor::cancelLoudnessMatchMeasurement() noexcept
{
    loudnessMatch.cancelMeasurements();
}

void FireAudioProcessor::copyLoudnessMatchToOtherSide() noexcept
{
    loudnessMatch.copyToOtherSide(stateAB.isCurrentA() ? 0 : 1);
}

void FireAudioProcessor::moveInsertEffectToPosition(int scope, int slot, int position)
{
    moveInsertEffectInternal(scope, slot, position, false);
}

void FireAudioProcessor::moveInsertEffectInternal(int scope, int slot, int position, bool relative)
{
    using namespace fire::effects;
    if (getInsertEffectType(scope, slot) == Type::none) return;
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish { [this] { requestMultibandTopologyReset(); } };
    std::vector<int> active;
    for (int i = 0; i < slotCount; ++i) if (getInsertEffectType(scope, i) != Type::none) active.push_back(i);
    std::stable_sort(active.begin(), active.end(), [&](int a, int b) {return getInsertEffectOrder(scope, a) < getInsertEffectOrder(scope, b);});
    const auto from = std::find(active.begin(), active.end(), slot);
    if (from == active.end()) return;
    const int index = static_cast<int>(std::distance(active.begin(), from));
    const int target = relative ? index + position : position;
    if (! juce::isPositiveAndBelow(target, static_cast<int>(active.size())) || target == index) return;
    active.erase(active.begin() + index);
    active.insert(active.begin() + target, slot);
    auto order = getModuleOrder(scope);
    size_t next = 0;
    for (auto& node : order)
        if (node >= fire::module_order::firstInsert && getInsertEffectType(scope, node - fire::module_order::firstInsert) != Type::none)
            node = fire::module_order::firstInsert + active[next++];
    writeModuleOrder(scope, order);
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
            band->prepare(spec, true, true);
            band->recSmoother.setCurrentAndTargetValue(loadCachedParameter(parameters.rec));
            band->biasSmoother.setCurrentAndTargetValue(loadCachedParameter(parameters.bias));

            const float initialOutput = loadCachedParameter(parameters.linked) > 0.5f
                                            && loadCachedParameter(parameters.modernDriveComp) < 0.5f
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
    const auto insertGraphLatency = static_cast<float>(2 * fire::effects::slotCount
        * fire::effects::independentShapeLatency(spec.numChannels));
    const auto graphHqLatency = hqLatency + insertGraphLatency;
    loudnessMatch.prepare(safeSampleRate);
    loudnessReference.setSize(outputChannels, maximumBlockSize);
    loudnessReferenceDelay.setMaximumDelayInSamples(juce::jmax(1, static_cast<int>(std::ceil(graphHqLatency)) + 2));
    loudnessReferenceDelay.prepare(spec);
    loudnessReferenceWasActive = false;
    // Host PDC cannot safely follow an automatable quality switch. Report the
    // prepared HQ latency for both modes; base processing is delayed by the
    // same integer number of samples at the end of the callback.
    totalLatency.store(graphHqLatency, std::memory_order_release);
    nonHqOutputDelay.prepare(spec);
    nonHqOutputDelay.setDelay(static_cast<float>(juce::roundToInt(hqLatency)));
    hqTransitionRampSamples = juce::jmax(
        1, juce::roundToInt(static_cast<float>(safeSampleRate) * 0.005f));
    hqTransitionGain = 1.0f;
    hqTransitionGainStep = 0.0f;
    hqTransitionRampRemaining = 0;
    hqTransitionWarmupSamples = juce::jmax(
        juce::roundToInt(static_cast<float>(safeSampleRate) * 0.001f),
        juce::roundToInt(std::ceil(graphHqLatency))
            + juce::roundToInt(hqLatency)
            + 2);
    topologyTransitionRampSamples = juce::jmax(
        1, juce::roundToInt(static_cast<float>(safeSampleRate) * 0.005f));
    topologyTransitionWarmupSamples = juce::jmax(
        48,
        juce::roundToInt(static_cast<float>(safeSampleRate) * 0.001f),
        juce::roundToInt(std::ceil(graphHqLatency))
            + juce::roundToInt(hqLatency)
            + 2);

    lfoManager->prepare(spec);
    spectrumProcessor.reset();

    historyPublicationSequence.fetch_add(1, std::memory_order_acq_rel);
    for (int i = 0; i < historyLength; ++i)
    {
        historyArrayL[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
        historyArrayR[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
    }
    historyWritePosition.store(0, std::memory_order_relaxed);
    historySamplesAvailable.store(0, std::memory_order_relaxed);
    historySamplesUntilCapture = 0;
    activeHistorySourceToken = historySourceRequestToken.load(
        std::memory_order_acquire);
    publishedHistorySourceToken.store(activeHistorySourceToken,
                                      std::memory_order_relaxed);
    historyGeneration.fetch_add(1, std::memory_order_release);
    historyPublicationSequence.fetch_add(1, std::memory_order_release);

    delayMatchedDryBuffer.setSize(outputChannels, maximumBlockSize);
    delayMatchedDryBuffer.clear();

    mWetBuffer.setSize(outputChannels, maximumBlockSize);
    mWetBuffer.clear();
    hostBypassWetBuffer.setSize(outputChannels, maximumBlockSize);
    hostBypassWetBuffer.clear();
    lfoOutputBuffer.setSize(fire::mod_sources::sourceCount, maximumBlockSize);
    lfoOutputBuffer.clear();
    lofiDryBuffer.setSize(outputChannels, maximumBlockSize);
    lofiDryBuffer.clear();
    globalMixAlignedDryBuffer.setSize(outputChannels, maximumBlockSize);
    globalMixAlignedDryBuffer.clear();

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
        std::ceil(hqLatency + insertGraphLatency * .5f)) + 2);
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
    eqProcessor.prepare(safeSampleRate);
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
    // This mixer now only supplies the legacy HQ dry-path alignment. The
    // coefficient stage below owns static and routed Global Mix control.
    dryWetMixerGlobal.setWetMixProportion(0.0f);
    dryWetMixerGlobal.prepare(globalMixerSpec);
    globalMixMixer.prepare(spec);

    globalFilterMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    globalFilterMixer.setWetMixProportion(isModulePresent(0, 0) && loadCachedParameter(filterEnabledParameter) > 0.5f
                                              ? 1.0f
                                              : 0.0f);
    globalFilterMixer.prepare(globalMixerSpec);

    bypassDelayMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    // Set the target before prepare/reset so the first bypassed sample is not
    // blended with the undelayed wet input by DryWetMixer's 50 ms ramp.
    bypassDelayMixer.setWetMixProportion(0.0f);
    bypassDelayMixer.prepare(globalMixerSpec);

    lofiMixer.prepare(globalMixerSpec);
    masterInserts.prepare(spec, true);
    masterOrderTransition.prepare(safeSampleRate);
    masterOrderDry.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    masterOrderDryDelay.prepare(spec);
    masterOrderDryDelay.setDelay(static_cast<float>(masterInserts.getReservedLatency()));
    masterTape.prepare(safeSampleRate);
    for (size_t i = 0; i < tapeSmoothers.size(); ++i)
    {
        tapeSmoothers[i].reset(safeSampleRate, 0.025);
        tapeSmoothers[i].setCurrentAndTargetValue(loadCachedParameter(tapeParameters[i]));
    }
    publishLatencyToHost();
    reset();
}

void FireAudioProcessor::reset()
{
    needsReset = true;
}

FireAudioProcessor::SerializableMainStateSnapshot
FireAudioProcessor::captureSerializableMainStateSnapshot() const
{
    auto lfoSnapshot = lfoManager->captureSerializableStateSnapshot();
    auto frozenAudio = captureFrozenAudio(lfoSnapshot.parameterState);
    auto presetIdentity = statePresets.getCurrentPresetIdentity();
    const auto currentEditorSize = getSavedEditorSize();
    const auto savedEditorSize = normaliseEditorSize(currentEditorSize.width,
                                                     currentEditorSize.height);
    // Copy A/B does not change the audible topology generation. Hold its lock
    // across both the alternate sound and the associated comparison gains.
    const juce::ScopedLock abLock(stateAB.stateLock);
    return {
        std::move(lfoSnapshot.parameterState),
        std::move(lfoSnapshot.lfoData),
        std::move(lfoSnapshot.routings),
        presetIdentity.id,
        std::move(presetIdentity.key),
        savedEditorSize.width,
        savedEditorSize.height,
        stateAB.captureSerializableStateSnapshot(),
        loudnessMatch.settings(),
        std::move(frozenAudio)
    };
}

fire::effects::FrozenRecordings FireAudioProcessor::captureFrozenAudio(const juce::ValueTree& parameters) const
{
    using namespace fire::effects;
    FrozenRecordings recordings{};
    const auto read = [&](const juce::String& id, float fallback = 0.0f)
    {
        const auto child = parameters.getChildWithProperty("id", id);
        return child.isValid() ? static_cast<float>(child.getProperty("value", fallback)) : fallback;
    };
    for (int scope = 0; scope < scopeCount; ++scope)
        for (int slot = 0; slot < slotCount; ++slot)
        {
            if (read(parameterID(scope, slot, typeField)) != 4.0f
                || read(fire::modulation_fx::parameterID(scope, slot)) != 0.0f
                || read(fire::resonator_params::parameterID(scope, slot)) > 0.5f
                || read(fire::core_modules::parameterID(scope, slot, fire::core_modules::typeField)) != 0.0f
                || read(fire::clouds_params::parameterID(scope, slot, fire::clouds_params::freezeField)) <= 0.5f)
                continue;
            const auto* rack = scope == 0 ? &masterInserts
                : static_cast<size_t>(scope) <= bands.size() ? &bands[static_cast<size_t>(scope - 1)]->inserts : nullptr;
            if (rack) recordings[static_cast<size_t>(scope)][static_cast<size_t>(slot)] = rack->copyFrozenRecording(slot);
        }
    return recordings;
}

void FireAudioProcessor::restoreFrozenAudio(const fire::effects::FrozenRecordings& recordings)
{
    const auto sequence = multibandTopologyResetGeneration.load(std::memory_order_seq_cst);
    const auto publication = (sequence & 1u) != 0u ? sequence + 1u : sequence;
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            auto* rack = scope == 0 ? &masterInserts
                : static_cast<size_t>(scope) <= bands.size() ? &bands[static_cast<size_t>(scope - 1)]->inserts : nullptr;
            if (rack) rack->stageFrozenRecording(slot, recordings[static_cast<size_t>(scope)][static_cast<size_t>(slot)], publication);
        }
    if (editHistory) editHistory->changed();
}

FireAudioProcessor::SerializableMainStateSnapshot
FireAudioProcessor::captureCoherentSerializableMainStateSnapshot() const
{
    for (;;)
    {
        const auto publishedGeneration =
            multibandTopologyResetGeneration.load(std::memory_order_seq_cst);
        if ((publishedGeneration & 1u) != 0u)
        {
            // Once odd is already visible, no registration is necessary: the
            // writer published this immutable pre-edit state before the odd
            // sequence and will never mutate that object.
            const auto publishedPreEditState = std::atomic_load_explicit(
                &mainStateBeforeTopologyEdit,
                std::memory_order_acquire);
            jassert(publishedPreEditState != nullptr);
            if (publishedPreEditState != nullptr)
                return *publishedPreEditState;

            continue;
        }

        std::shared_ptr<const SerializableMainStateSnapshot> preEditState;
        SerializableMainStateSnapshot candidate;
        bool candidateWasAccepted = false;

        {
            const auto previousReaders = activeSerializableStateReaders.fetch_add(
                1u, std::memory_order_seq_cst);
            jassert(previousReaders
                    != std::numeric_limits<unsigned int>::max());
            const juce::ScopeGuard unregisterReader { [this]
            {
                const auto previous = activeSerializableStateReaders.fetch_sub(
                    1u, std::memory_order_seq_cst);
                jassert(previous > 0u);
            } };

            const auto generationBefore =
                multibandTopologyResetGeneration.load(
                    std::memory_order_seq_cst);
            if ((generationBefore & 1u) != 0u)
            {
                // Copy the pointer while registered, then copy its immutable
                // payload after unregistering below.
                preEditState = std::atomic_load_explicit(
                    &mainStateBeforeTopologyEdit,
                    std::memory_order_acquire);
                jassert(preEditState != nullptr);
            }
            else
            {
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
                std::function<void()> readerHook;
                {
                    const juce::ScopedLock lock(
                        serializableStateHookLock);
                    readerHook = std::move(
                        serializableStateReaderHookForTesting);
                    serializableStateReaderHookForTesting = {};
                }
                if (readerHook)
                    readerHook();
#endif

                candidate = captureSerializableMainStateSnapshot();
                const auto generationAfter =
                    multibandTopologyResetGeneration.load(
                        std::memory_order_seq_cst);
                candidateWasAccepted = generationBefore == generationAfter
                                       && (generationAfter & 1u) == 0u;
            }
        }

        if (preEditState != nullptr)
            return *preEditState;

        if (candidateWasAccepted)
            return candidate;
    }
}

FireAudioProcessor::SerializablePresetStateSnapshot
FireAudioProcessor::captureSerializablePresetStateSnapshot() const
{
    if (! serializableMainStateReady.load(std::memory_order_acquire))
    {
        auto snapshot = lfoManager->captureSerializableStateSnapshot();
        return {
            std::move(snapshot.parameterState),
            std::move(snapshot.lfoData),
            std::move(snapshot.routings)
        };
    }

    auto snapshot = captureCoherentSerializableMainStateSnapshot();
    return {
        std::move(snapshot.parameterState),
        std::move(snapshot.lfoData),
        std::move(snapshot.routings),
        std::move(snapshot.frozenAudio)
    };
}

FireAudioProcessor::SerializablePresetStateSnapshot
FireAudioProcessor::captureCurrentSerializablePresetStateSnapshotForABFallback()
    const
{
    // StateAB uses this only after a host-state transaction has completely
    // staged its new APVTS/LFO/routing state, while the public generation is
    // intentionally still odd. Its damaged/missing alternate must mirror that
    // new live state rather than the immutable pre-edit snapshot returned to
    // external serializers during the same transaction.
    auto snapshot = lfoManager->captureSerializableStateSnapshot();
    auto frozenAudio = captureFrozenAudio(snapshot.parameterState);
    return {
        std::move(snapshot.parameterState),
        std::move(snapshot.lfoData),
        std::move(snapshot.routings),
        std::move(frozenAudio)
    };
}

bool FireAudioProcessor::addMultibandBand(int splitBandIndex,
                                          int currentBandCount,
                                          bool newBandIsOnLeft,
                                          float crossoverFrequency)
{
    if (currentBandCount < 1 || currentBandCount >= 4
        || ! juce::isPositiveAndBelow(splitBandIndex, currentBandCount)
        || ! std::isfinite(crossoverFrequency))
    {
        jassertfalse;
        return false;
    }

    bool didAdd = false;
    {
        // Validate and capture the source tuple under the same writer lock as
        // the mutation. A preset or host-state writer must not replace it
        // between those two phases.
        const juce::ScopedLock writerLock(multibandTopologyWriterLock);
        didAdd = addMultibandBandLocked(splitBandIndex,
                                        currentBandCount,
                                        newBandIsOnLeft,
                                        crossoverFrequency);
    }

    if (! didAdd)
        return false;

    // The APVTS/LFO tuple and even generation are complete before this
    // externally-calling notification. It may synchronously close the editor.
    lfoDataHasChanged();
    return true;
}

bool FireAudioProcessor::addMultibandBandLocked(int splitBandIndex,
                                                int currentBandCount,
                                                bool newBandIsOnLeft,
                                                float crossoverFrequency)
{
    auto frozenAudio = captureSerializablePresetStateSnapshot().frozenAudio;
    struct ParameterWrite
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float normalisedValue = 0.0f;
    };

    const int oldLastBandIndex = currentBandCount - 1;
    const int newBandCount = currentBandCount + 1;
    const int oldLineCount = currentBandCount - 1;
    const int newLineCount = currentBandCount;
    const int newBandIndex = newBandIsOnLeft ? splitBandIndex
                                             : splitBandIndex + 1;

    std::vector<ParameterWrite> copiedBandParameters;
    std::vector<ParameterWrite> resetBandParameters;
    const auto& bandParameters = ParameterIDAndName::getBandParameterInfo();
    const int firstBandToShift = newBandIsOnLeft ? splitBandIndex
                                                 : splitBandIndex + 1;
    copiedBandParameters.reserve(
        static_cast<size_t>(juce::jmax(0,
                                      oldLastBandIndex - firstBandToShift + 1))
        * bandParameters.size());
    resetBandParameters.reserve(bandParameters.size());

    // Capture every source before publishing the first parameter. This gives
    // the right shift memmove semantics even when synchronous callbacks
    // inspect or replace other state.
    for (int sourceBand = oldLastBandIndex;
         sourceBand >= firstBandToShift;
         --sourceBand)
    {
        for (const auto& parameterInfo : bandParameters)
        {
            auto* source = treeState.getParameter(
                ParameterIDAndName::getIDString(parameterInfo.idBase,
                                                sourceBand));
            auto* target = treeState.getParameter(
                ParameterIDAndName::getIDString(parameterInfo.idBase,
                                                sourceBand + 1));
            if (source == nullptr || target == nullptr)
            {
                jassertfalse;
                return false;
            }

            copiedBandParameters.push_back({ target, source->getValue() });
        }
    }

    for (const auto& parameterInfo : bandParameters)
    {
        auto* parameter = treeState.getParameter(
            ParameterIDAndName::getIDString(parameterInfo.idBase,
                                            newBandIndex));
        if (parameter == nullptr)
        {
            jassertfalse;
            return false;
        }

        resetBandParameters.push_back(
            { parameter, parameter->getDefaultValue() });
    }

    std::array<juce::RangedAudioParameter*, 3> frequencyParameters {};
    std::array<juce::RangedAudioParameter*, 3> lineStateParameters {};
    std::array<float, 3> originalFrequencies {};
    for (int divider = 0; divider < 3; ++divider)
    {
        auto* frequency = treeState.getParameter(
            ParameterIDAndName::getIDString(FREQ_ID, divider));
        auto* lineState = treeState.getParameter(
            ParameterIDAndName::getIDString(LINE_STATE_ID, divider));
        if (frequency == nullptr || lineState == nullptr)
        {
            jassertfalse;
            return false;
        }

        frequencyParameters[static_cast<size_t>(divider)] = frequency;
        lineStateParameters[static_cast<size_t>(divider)] = lineState;
        originalFrequencies[static_cast<size_t>(divider)] =
            frequency->getNormalisableRange().convertFrom0to1(
                frequency->getValue());
    }

    const auto& crossoverRange =
        frequencyParameters[static_cast<size_t>(splitBandIndex)]
            ->getNormalisableRange();
    const float constrainedFrequency = crossoverRange.snapToLegalValue(
        juce::jlimit(crossoverRange.start,
                     crossoverRange.end,
                     crossoverFrequency));
    if (! std::isfinite(constrainedFrequency)
        || (splitBandIndex > 0
            && constrainedFrequency
                   <= originalFrequencies[static_cast<size_t>(splitBandIndex - 1)])
        || (splitBandIndex < oldLineCount
            && constrainedFrequency
                   >= originalFrequencies[static_cast<size_t>(splitBandIndex)]))
        return false;

    std::array<float, 3> finalFrequencies = originalFrequencies;
    for (int divider = 0; divider < newLineCount; ++divider)
    {
        if (divider < splitBandIndex)
            continue;

        finalFrequencies[static_cast<size_t>(divider)] =
            divider == splitBandIndex
                ? constrainedFrequency
                : originalFrequencies[static_cast<size_t>(divider - 1)];
    }

    auto* bandCountParameter = treeState.getParameter(NUM_BANDS_ID);
    if (bandCountParameter == nullptr)
    {
        jassertfalse;
        return false;
    }

    const int publishedBandCount = juce::roundToInt(
        bandCountParameter->getNormalisableRange().convertFrom0to1(
            bandCountParameter->getValue()));
    if (publishedBandCount != currentBandCount)
        return false;

    {
        beginMultibandTopologyEdit();
        const juce::ScopeGuard finishTopologyEdit { [processor = this]
        {
            processor->requestMultibandTopologyReset();
        } };

        for (int divider = 0; divider < 3; ++divider)
            lineStateParameters[static_cast<size_t>(divider)]
                ->setValueNotifyingHost(divider < newLineCount ? 1.0f : 0.0f);

        for (int divider = 0; divider < newLineCount; ++divider)
        {
            auto* parameter = frequencyParameters[static_cast<size_t>(divider)];
            parameter->setValueNotifyingHost(
                parameter->getNormalisableRange().convertTo0to1(
                    finalFrequencies[static_cast<size_t>(divider)]));
        }

        for (const auto& write : copiedBandParameters)
            write.parameter->setValueNotifyingHost(write.normalisedValue);

        if (firstBandToShift <= oldLastBandIndex)
            shiftLfoModulationTargets(firstBandToShift,
                                      oldLastBandIndex,
                                      1,
                                      false);

        for (const auto& write : resetBandParameters)
            write.parameter->setValueNotifyingHost(write.normalisedValue);

        clearLfoModulationForBand(newBandIndex, false);
        for (int source = oldLastBandIndex; source >= firstBandToShift; --source)
            frozenAudio[static_cast<size_t>(source + 2)] = frozenAudio[static_cast<size_t>(source + 1)];
        frozenAudio[static_cast<size_t>(newBandIndex + 1)] = {};
        restoreFrozenAudio(frozenAudio);
        bandCountParameter->setValueNotifyingHost(
            bandCountParameter->getNormalisableRange().convertTo0to1(
                static_cast<float>(newBandCount)));
    }

    return true;
}

bool FireAudioProcessor::deleteMultibandBand(int deletedBandIndex,
                                             int currentBandCount)
{
    if (currentBandCount < 2 || currentBandCount > 4
        || ! juce::isPositiveAndBelow(deletedBandIndex, currentBandCount))
    {
        jassertfalse;
        return false;
    }

    bool didDelete = false;
    {
        // Snapshot validation is part of the writer transaction too. Without
        // this outer level, a preset/host-state writer could publish between
        // individual reads and leave the migration with a mixed-generation
        // source tuple. beginMultibandTopologyEdit() recursively acquires the
        // same lock and retains its own level until publication.
        const juce::ScopedLock writerLock(multibandTopologyWriterLock);
        didDelete = deleteMultibandBandLocked(deletedBandIndex,
                                              currentBandCount);
    }

    if (! didDelete)
        return false;

    // The complete APVTS/LFO tuple is now visible under an even generation and
    // the writer lock has been released. Keep this as the final externally-
    // calling operation: its host callback may synchronously close the editor.
    lfoDataHasChanged();
    return true;
}

bool FireAudioProcessor::deleteMultibandBandLocked(int deletedBandIndex,
                                                   int currentBandCount)
{
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    std::function<void()> snapshotHook;
    {
        const juce::ScopedLock hookLock(serializableStateHookLock);
        snapshotHook = multibandDeleteSnapshotHookForTesting;
    }
    if (snapshotHook)
        snapshotHook();
#endif

    auto frozenAudio = captureSerializablePresetStateSnapshot().frozenAudio;
    struct ParameterWrite
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float normalisedValue = 0.0f;
    };

    const int oldLastBandIndex = currentBandCount - 1;
    const int newBandCount = currentBandCount - 1;
    const int oldLineCount = currentBandCount - 1;
    const int newLineCount = newBandCount - 1;
    const int removedDividerIndex = deletedBandIndex == oldLastBandIndex
                                      ? oldLineCount - 1
                                      : deletedBandIndex;

    std::vector<ParameterWrite> copiedBandParameters;
    std::vector<ParameterWrite> resetBandParameters;
    const auto& bandParameters = ParameterIDAndName::getBandParameterInfo();
    copiedBandParameters.reserve(
        static_cast<size_t>(oldLastBandIndex - deletedBandIndex)
        * bandParameters.size());
    resetBandParameters.reserve(bandParameters.size());

    // Capture every source before the first synchronous notification. A host
    // callback may re-enter state code, and later writes must still have
    // memmove semantics rather than reading already-overwritten slots.
    for (int targetBand = deletedBandIndex;
         targetBand < oldLastBandIndex;
         ++targetBand)
    {
        for (const auto& parameterInfo : bandParameters)
        {
            auto* target = treeState.getParameter(
                ParameterIDAndName::getIDString(parameterInfo.idBase,
                                                targetBand));
            auto* source = treeState.getParameter(
                ParameterIDAndName::getIDString(parameterInfo.idBase,
                                                targetBand + 1));
            if (target == nullptr || source == nullptr)
            {
                jassertfalse;
                return false;
            }

            copiedBandParameters.push_back({ target, source->getValue() });
        }
    }

    for (const auto& parameterInfo : bandParameters)
    {
        auto* parameter = treeState.getParameter(
            ParameterIDAndName::getIDString(parameterInfo.idBase,
                                            oldLastBandIndex));
        if (parameter == nullptr)
        {
            jassertfalse;
            return false;
        }

        resetBandParameters.push_back(
            { parameter, parameter->getDefaultValue() });
    }

    std::array<juce::RangedAudioParameter*, 3> frequencyParameters {};
    std::array<juce::RangedAudioParameter*, 3> lineStateParameters {};
    std::array<float, 3> originalFrequencies {};
    for (int divider = 0; divider < 3; ++divider)
    {
        frequencyParameters[static_cast<size_t>(divider)] =
            treeState.getParameter(
                ParameterIDAndName::getIDString(FREQ_ID, divider));
        lineStateParameters[static_cast<size_t>(divider)] =
            treeState.getParameter(
                ParameterIDAndName::getIDString(LINE_STATE_ID, divider));
        if (frequencyParameters[static_cast<size_t>(divider)] == nullptr
            || lineStateParameters[static_cast<size_t>(divider)] == nullptr)
        {
            jassertfalse;
            return false;
        }

        originalFrequencies[static_cast<size_t>(divider)] =
            frequencyParameters[static_cast<size_t>(divider)]->getValue();
    }

    auto* bandCountParameter = treeState.getParameter(NUM_BANDS_ID);
    if (bandCountParameter == nullptr)
    {
        jassertfalse;
        return false;
    }

    const int publishedBandCount = juce::roundToInt(
        bandCountParameter->getNormalisableRange().convertFrom0to1(
            bandCountParameter->getValue()));
    if (publishedBandCount != currentBandCount)
        return false;

    {
        beginMultibandTopologyEdit();
        const juce::ScopeGuard finishTopologyEdit { [processor = this]
        {
            processor->requestMultibandTopologyReset();
        } };

        // Publish the final canonical divider tuple directly. In particular,
        // deleting the leftmost band compacts FREQ1 into FREQ0 even though the
        // final LINE0 state remains enabled throughout the transaction.
        for (int divider = 0; divider < 3; ++divider)
            lineStateParameters[static_cast<size_t>(divider)]
                ->setValueNotifyingHost(divider < newLineCount ? 1.0f : 0.0f);

        for (int targetDivider = removedDividerIndex;
             targetDivider < newLineCount;
             ++targetDivider)
        {
            frequencyParameters[static_cast<size_t>(targetDivider)]
                ->setValueNotifyingHost(
                    originalFrequencies[static_cast<size_t>(targetDivider + 1)]);
        }

        clearLfoModulationForBand(deletedBandIndex, false);

        for (const auto& write : copiedBandParameters)
            write.parameter->setValueNotifyingHost(write.normalisedValue);

        if (deletedBandIndex < oldLastBandIndex)
            shiftLfoModulationTargets(deletedBandIndex + 1,
                                      oldLastBandIndex,
                                      -1,
                                      false);

        for (const auto& write : resetBandParameters)
            write.parameter->setValueNotifyingHost(write.normalisedValue);

        clearLfoModulationForBand(oldLastBandIndex, false);
        for (int source = deletedBandIndex + 1; source <= oldLastBandIndex; ++source)
            frozenAudio[static_cast<size_t>(source)] = frozenAudio[static_cast<size_t>(source + 1)];
        frozenAudio[static_cast<size_t>(oldLastBandIndex + 1)] = {};
        restoreFrozenAudio(frozenAudio);
        bandCountParameter->setValueNotifyingHost(
            bandCountParameter->getNormalisableRange().convertTo0to1(
                static_cast<float>(newBandCount)));
    }

    return true;
}

void FireAudioProcessor::beginMultibandTopologyEdit()
{
    if (editHistory) editHistory->beginGroup();
    // Keep one recursive writer-lock level alive until the matching publish.
    // This serialises editor, preset and host-state writers without ever
    // involving the audio thread. Nested edits on the same thread coalesce
    // into one odd/even publication.
    multibandTopologyWriterLock.enter();
    if (multibandTopologyEditDepth == 0)
    {
        try
        {
            // Audio uses the odd/even generation below. Host state callbacks
            // can be synchronously re-entered by parameter notifications, so
            // retain the last complete main state for that owning thread too.
            std::atomic_store_explicit(
                &mainStateBeforeTopologyEdit,
                std::make_shared<const SerializableMainStateSnapshot>(
                    captureSerializableMainStateSnapshot()),
                std::memory_order_release);
        }
        catch (...)
        {
            multibandTopologyWriterLock.exit();
            throw;
        }

        mainStateEditRequiresDspReset = false;
        const auto previous = multibandTopologyResetGeneration.fetch_add(
            1u, std::memory_order_seq_cst);
        jassert((previous & 1u) == 0u);

        // A host callback may already hold JUCE's parameter-listener lock and
        // be copying APVTS. Do not let the caller start a V->P state
        // replacement until every reader which observed the previous even
        // generation has unregistered. seq_cst is required because the count
        // and generation are distinct atomics.
        auto readers = activeSerializableStateReaders.load(
            std::memory_order_seq_cst);
        while (readers != 0u)
        {
            // The generated Xcode/Projucer targets still compile as C++17, so
            // use JUCE's portable yield instead of C++20 atomic::wait().
            juce::Thread::yield();
            readers = activeSerializableStateReaders.load(
                std::memory_order_seq_cst);
        }
    }

    ++multibandTopologyEditDepth;
}

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
void FireAudioProcessor::setSerializableStateReaderHookForTesting(
    std::function<void()> hook)
{
    const juce::ScopedLock lock(serializableStateHookLock);
    serializableStateReaderHookForTesting = std::move(hook);
}

void FireAudioProcessor::setHostStateMainCaptureHookForTesting(
    std::function<void()> hook)
{
    const juce::ScopedLock lock(serializableStateHookLock);
    hostStateMainCaptureHookForTesting = std::move(hook);
}

void FireAudioProcessor::setAudioCallbackStateCaptureHookForTesting(
    std::function<void()> hook)
{
    const juce::ScopedLock lock(serializableStateHookLock);
    audioCallbackStateCaptureHookForTesting = std::move(hook);
}

void FireAudioProcessor::setMultibandDeleteSnapshotHookForTesting(
    std::function<void()> hook)
{
    const juce::ScopedLock lock(serializableStateHookLock);
    multibandDeleteSnapshotHookForTesting = std::move(hook);
}

FireAudioProcessor::AudioCallbackRecipeForTesting
FireAudioProcessor::getLastAudioCallbackRecipeForTesting() const noexcept
{
    const auto& band0 = activeMultibandTopologySnapshot.callbackContext
                            .bandParameters[0];
    return {
        lastAudioCallbackGenerationAtStart,
        activeMultibandTopologySnapshot.publicationSequence,
        activeMultibandTopologySnapshot.numBands,
        band0.outputVal.baseValue,
        activeAudioCallbackParameterSnapshot.requestedHq,
        activeAudioCallbackParameterSnapshot.globalOutput.provider.baseValue,
        activeAudioCallbackParameterSnapshot.downsampleEnabled,
        activeAudioCallbackParameterSnapshot.downsampleRate.provider.baseValue,
        activeAudioCallbackParameterSnapshot.lfoParameters.lfos[0].freeRate
    };
}
#endif

void FireAudioProcessor::requestMultibandTopologyReset() noexcept
{
    finishMainStateEdit(true);
}

void FireAudioProcessor::finishMainStateEdit(bool resetDsp) noexcept
{
    // CriticalSection is recursive. The extra level acquired here lets the
    // owner finish its transaction while competing writers wait; the second
    // exit balances the level deliberately retained by begin().
    multibandTopologyWriterLock.enter();

    if (multibandTopologyEditDepth > 0)
    {
        mainStateEditRequiresDspReset = mainStateEditRequiresDspReset || resetDsp;
        --multibandTopologyEditDepth;
        if (multibandTopologyEditDepth == 0)
        {
            if (mainStateEditRequiresDspReset)
                multibandDspResetSequence.fetch_add(1u, std::memory_order_relaxed);
            const auto previous = multibandTopologyResetGeneration.fetch_add(
                1u, std::memory_order_seq_cst);
            jassert((previous & 1u) != 0u);
        }

        multibandTopologyWriterLock.exit();
        multibandTopologyWriterLock.exit();
        if (editHistory) editHistory->endGroup();
        return;
    }

    // A standalone request represents a complete same-count publication.
    if (resetDsp) multibandDspResetSequence.fetch_add(1u, std::memory_order_relaxed);
    const auto previous = multibandTopologyResetGeneration.fetch_add(
        2u, std::memory_order_seq_cst);
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
    const float wetPathLatency = static_cast<float>(masterInserts.getReservedLatency())
        + (useHQ ? preparedHqLatency.load(std::memory_order_acquire) : 0.0f);
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
    MultibandTopologySnapshot& snapshot,
    AudioCallbackParameterSnapshot& callbackParameters)
{
    const auto sequenceBefore = multibandTopologyResetGeneration.load(
        std::memory_order_acquire);
    if (sequenceBefore != sequenceAtCallbackStart
        || (sequenceBefore & 1u) != 0u)
        return false;

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    std::function<void()> captureHook;
    {
        const juce::ScopedLock lock(serializableStateHookLock);
        captureHook = std::move(audioCallbackStateCaptureHookForTesting);
        audioCallbackStateCaptureHookForTesting = {};
    }
    if (captureHook)
        captureHook();
#endif

    auto& candidate = audioCallbackWorkspace->candidateTopology;
    candidate.publicationSequence = sequenceBefore;
    candidate.dspResetSequence = multibandDspResetSequence.load(std::memory_order_relaxed);
    candidate.numBands = juce::jlimit(
        1,
        4,
        juce::roundToInt(loadCachedParameter(numBandsParameter, 1.0f)));
    candidate.crossoverFrequencies = getEffectiveCrossoverFrequencies(
        candidate.numBands - 1);
    prepareHqCallbackContext(lfoOutputs,
                             candidate.numBands,
                             candidate.callbackContext);
    auto& candidateCallbackParameters = audioCallbackWorkspace->candidateParameters;
    prepareAudioCallbackParameterSnapshot(sequenceBefore,
                                          candidateCallbackParameters);

    // APVTS publishes its raw parameter atomics with release/seq_cst stores.
    // If any relaxed payload read above observed a value staged after a writer
    // made the generation odd, this acquire fence imports that writer before
    // the final generation check. Atomic coherence then prevents the check
    // from reading the older even generation and accepting a mixed recipe.
    std::atomic_thread_fence(std::memory_order_acquire);
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
    callbackParameters = std::move(candidateCallbackParameters);
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
    auto& requestedSnapshot = audioCallbackWorkspace->resetTopology;
    auto& requestedCallbackParameters = audioCallbackWorkspace->resetParameters;
    const auto sequenceAtReset = multibandTopologyResetGeneration.load(
        std::memory_order_acquire);
    // A lifecycle reset runs before the callback's non-blocking LFO routing
    // refresh. Do not mark a newly published topology as applied here: when an
    // older audible snapshot exists, the first successful audio-thread refresh
    // must validate its migrated routing at the same time.
    if (tryCaptureMultibandTopologySnapshot(noLfoOutputs,
                                            sequenceAtReset,
                                            false,
                                            requestedSnapshot,
                                            requestedCallbackParameters))
    {
        activeMultibandTopologySnapshot = std::move(requestedSnapshot);
        activeMultibandTopologySnapshotInitialised = true;
        activeAudioCallbackParameterSnapshot =
            std::move(requestedCallbackParameters);
        activeAudioCallbackParameterSnapshotInitialised = true;
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
        activeMultibandTopologySnapshot.dspResetSequence =
            multibandDspResetSequence.load(std::memory_order_relaxed);
        activeMultibandTopologySnapshotInitialised = true;
    }

    if (! activeAudioCallbackParameterSnapshotInitialised)
    {
        prepareAudioCallbackParameterSnapshot(
            activeMultibandTopologySnapshot.publicationSequence,
            activeAudioCallbackParameterSnapshot);
        activeAudioCallbackParameterSnapshotInitialised = true;
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
    loudnessMatch.suspendMeasurement();
    loudnessReferenceWasActive = false;
    spectrumProcessor.reset();
    historySamplesUntilCapture = 0;
    synchroniseMultibandTopologyResetState();
    resetMultibandProcessingState(
        &activeMultibandTopologySnapshot.callbackContext);
    leftChain.reset();
    rightChain.reset();
    eqProcessor.reset();
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
                           activeAudioCallbackParameterSnapshot
                               .globalFilter.baseSettings.lowCutSlope);
    snapCutSlopeTransition(highCutSlopeTransition,
                           leftChain.get<ChainPositions::HighCut>(),
                           rightChain.get<ChainPositions::HighCut>(),
                           activeAudioCallbackParameterSnapshot
                               .globalFilter.baseSettings.highCutSlope);
    globalFilterCacheValid = false;
    const auto& filterSettings =
        activeAudioCallbackParameterSnapshot.globalFilter.baseSettings;
    const std::array<float, numGlobalFilterStages> initialStageMix {
        filterSettings.lowCutBypassed ? 0.0f : 1.0f,
        filterSettings.peakBypassed ? 0.0f : 1.0f,
        filterSettings.highCutBypassed ? 0.0f : 1.0f,
        filterSettings.lowCutBypassed ? 0.0f : 1.0f,
        filterSettings.highCutBypassed ? 0.0f : 1.0f
    };
    for (size_t stage = 0; stage < globalFilterStageMix.size(); ++stage)
        globalFilterStageMix[stage].setCurrentAndTargetValue(initialStageMix[stage]);
    globalFilterMixer.reset();
    globalFilterMixerPrimed = false;
    dryWetMixerGlobal.reset();
    globalMixMixer.reset();
    globalMixAlignedDryBuffer.clear();
    bypassDelayMixer.reset();
    nonHqOutputDelay.reset();
    lofiMixer.reset();
    masterInserts.reset();
    masterOrderDryDelay.reset();
    masterOrderTransition.reset();
    masterTape.reset();
    for (size_t i = 0; i < tapeSmoothers.size(); ++i)
        tapeSmoothers[i].setCurrentAndTargetValue(loadCachedParameter(tapeParameters[i]));
    resetDownsamplingState();
    gainProcessorGlobal.reset();
    globalOutputGainTransition.reset();
    lfoManager->reset();
    snapHqTransitionToParameter();
    snapTopologyTransitionToActive();
    lastPublishedMeterValues = {};
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
    if (editHistory) editHistory->poll();
    // setLatencySamples synchronously notifies the host, so keep it on the
    // message thread. The prepared maximum is invariant across HQ automation.
    if (getLatencySamples() != juce::roundToInt(totalLatency.load(std::memory_order_acquire)))
    {
        publishLatencyToHost();
        return; // A host notification may synchronously delete the processor.
    }
    if (loudnessMatch.takeCompletedNotification())
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
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
    // The audible bypass path runs meters and the latency-matching Thiran delay
    // before processWetBlock() establishes its own denormal guard for the hidden
    // wet render. Protect the complete callback so subnormal tails cannot cause
    // floating-point assists while the plug-in is host-bypassed.
    juce::ScopedNoDenormals noDenormals;
    loudnessMatch.suspendMeasurement();
    loudnessReferenceWasActive = false;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (const auto hook = hostBypassDenormalStateHookForTesting.load(
            std::memory_order_acquire))
        hook(juce::FloatVectorOperations::areDenormalsDisabled());
#endif

    isBypassed.store(true, std::memory_order_release);

    if (needsReset.exchange(false, std::memory_order_acq_rel))
        performReset();

    if (! hostBypassSessionActive)
    {
        // Host bypass does not publish analyser data. Invalidate both the last
        // completed frame and any partially accumulated pre-bypass window at
        // the audio-thread epoch boundary, so the UI cannot mistake either for
        // the first frame after processing resumes. SpectrumProcessor::reset()
        // is allocation-free and its publication protocol already tolerates a
        // concurrent message-thread reader.
        spectrumProcessor.reset();
        hostBypassPresentationEpoch.fetch_add(1,
                                               std::memory_order_release);

        // Keep the audible raw tap fixed for the complete host-bypass session.
        // The hidden wet graph may finish an HQ transition meanwhile, but a
        // delayed dry signal must never expose that fractional-tap switch.
        hostBypassSessionHqMode = activeHqMode;
        hostBypassSessionActive = true;
    }

    // Treat a non-finite host sample as silence before it can enter any
    // recursive filter, detector, oversampler or latency-compensation state.
    // Cleaning only the final output would hide the symptom while leaving the
    // wet graph and the audible host-bypass delay permanently poisoned.
    replaceNonFiniteSamplesWithSilence(buffer);

    calculateAndStoreLevels(buffer,
                            mInputLeftRMSGlobal,
                            mInputRightRMSGlobal,
                            mInputLeftPeakGlobal,
                            mInputRightPeakGlobal);
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
    {
        calculateAndStoreLevels(buffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);
        publishMeterValues(false);
        return;
    }

    // The host hears only latency-matched raw audio. In parallel, render a
    // discarded copy through the complete wet graph so recursive filters,
    // compressors, Lo-Fi, LFOs and live HQ/topology state machines remain on
    // the same timeline they would have followed without host bypass.
    const int rangeCapacity = juce::jmax(1, preparedProcessingBlockCapacity);
    if (buffer.getNumSamples() <= rangeCapacity)
    {
        hostBypassWetBuffer.makeCopyOf(buffer, true);
        processLatencyMatchedBypass(buffer, hostBypassSessionHqMode);
        processWetBlock(hostBypassWetBuffer, midiMessages, true);
    }
    else
    {
        // Keep every scratch buffer within the capacity reserved by
        // prepareToPlay(). AudioBuffer views are non-owning (and use JUCE's
        // inline channel-pointer storage), so a malformed oversized host
        // callback cannot force heap allocation on the audio thread.
        const int numChannels = buffer.getNumChannels();
        int sampleOffset = 0;
        while (sampleOffset < buffer.getNumSamples())
        {
            const int samplesInRange = juce::jmin(
                rangeCapacity, buffer.getNumSamples() - sampleOffset);
            juce::AudioBuffer<float> audibleRange(
                buffer.getArrayOfWritePointers(),
                numChannels,
                sampleOffset,
                samplesInRange);

            hostBypassWetBuffer.setSize(numChannels,
                                        samplesInRange,
                                        false,
                                        false,
                                        true);
            for (int channel = 0; channel < numChannels; ++channel)
                hostBypassWetBuffer.copyFrom(channel,
                                             0,
                                             audibleRange,
                                             channel,
                                             0,
                                             samplesInRange);

            processLatencyMatchedBypass(audibleRange,
                                        hostBypassSessionHqMode);
            processWetBlock(hostBypassWetBuffer,
                            midiMessages,
                            true,
                            sampleOffset,
                            false);
            sampleOffset += samplesInRange;
        }
    }
    calculateAndStoreLevels(buffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);
    publishMeterValues(false);
}

void FireAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0)
        audioActivitySequence.fetch_add(1, std::memory_order_relaxed);
    isBypassed.store(false, std::memory_order_release);
    hostBypassSessionActive = false;

    if (needsReset.exchange(false, std::memory_order_acq_rel))
        performReset();

    replaceNonFiniteSamplesWithSilence(buffer);
    const int rangeCapacity = juce::jmax(1, preparedProcessingBlockCapacity);
    if (buffer.getNumChannels() == 0
        || buffer.getNumSamples() <= rangeCapacity)
    {
        processWetBlock(buffer, midiMessages, false);
        return;
    }

    // Defensive fixed-capacity fallback for hosts which exceed the maximum
    // block size they supplied to prepareToPlay(). Preserve one callback-wide
    // global meter packet while streaming all stateful DSP and analysis in
    // allocation-free non-owning ranges.
    const int numChannels = buffer.getNumChannels();
    const int totalNumInputChannels = getTotalNumInputChannels();
    for (int channel = juce::jlimit(0,
                                   numChannels,
                                   totalNumInputChannels);
         channel < numChannels;
         ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    calculateAndStoreLevels(buffer,
                            mInputLeftRMSGlobal,
                            mInputRightRMSGlobal,
                            mInputLeftPeakGlobal,
                            mInputRightPeakGlobal);
    int sampleOffset = 0;
    while (sampleOffset < buffer.getNumSamples())
    {
        const int samplesInRange = juce::jmin(
            rangeCapacity, buffer.getNumSamples() - sampleOffset);
        juce::AudioBuffer<float> range(buffer.getArrayOfWritePointers(),
                                       numChannels,
                                       sampleOffset,
                                       samplesInRange);
        processWetBlock(range,
                        midiMessages,
                        false,
                        sampleOffset,
                        false);
        sampleOffset += samplesInRange;
    }

    calculateAndStoreLevels(buffer,
                            mOutputLeftRMSGlobal,
                            mOutputRightRMSGlobal,
                            mOutputLeftPeakGlobal,
                            mOutputRightPeakGlobal);
    publishMeterValues(true);
}

void FireAudioProcessor::processWetBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages,
    bool hostBypassShadow,
    int playheadSampleOffset,
    bool publishMeterPacket)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int numBufferChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    jassert(numSamples <= juce::jmax(1, preparedProcessingBlockCapacity));
    auto sampleRate = getSampleRate();

    if (! std::isfinite(sampleRate) || sampleRate <= 0)
    {
        sampleRate = 48000;
    }

    for (int channel = juce::jlimit(0, numBufferChannels, totalNumInputChannels);
         channel < numBufferChannels;
         ++channel)
        buffer.clear(channel, 0, numSamples);

    if (! hostBypassShadow && publishMeterPacket)
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

    const auto topologySequenceAtCallbackStart =
        multibandTopologyResetGeneration.load(std::memory_order_acquire);
    lastAudioCallbackGenerationAtStart = topologySequenceAtCallbackStart;

    lfoOutputBuffer.setSize(fire::mod_sources::sourceCount, numSamples, false, false, true);
    lfoOutputBuffer.clear();

    // Routes, staged shapes and every scalar DSP recipe are selected under
    // one odd/even handshake before the LFO or any audio state is advanced.
    // A failed/odd capture leaves both sides on their last completed frame.
    const bool routingSnapshotWasRefreshed =
        lfoManager->beginAudioThreadStateCapture(
            topologySequenceAtCallbackStart,
            multibandTopologyResetGeneration);
    bool lfoCaptureNeedsAbort = routingSnapshotWasRefreshed;
    const juce::ScopeGuard abortIncompleteLfoCapture { [this,
                                                        &lfoCaptureNeedsAbort]
    {
        if (lfoCaptureNeedsAbort)
            lfoManager->abortAudioThreadStateCapture();
    } };

    auto& callbackContext = audioCallbackWorkspace->blockContext;
    const bool capturedStableCallbackState = updateParameters(
        lfoOutputBuffer,
        topologySequenceAtCallbackStart,
        routingSnapshotWasRefreshed,
        callbackContext);
    lfoManager->finishAudioThreadStateCapture(capturedStableCallbackState);
    lfoCaptureNeedsAbort = false;

    lfoManager->setAudioInput(&buffer);
    const juce::ScopeGuard clearModulationInput{[this] { lfoManager->setAudioInput(nullptr); }};
    lfoManager->processBlock(
        lfoOutputBuffer,
        static_cast<float>(sampleRate),
        getPlayHead(),
        numSamples,
        activeAudioCallbackParameterSnapshot.lfoParameters,
        playheadSampleOffset);
    publishMultibandTelemetry(callbackContext,
                              numBands,
                              lfoOutputBuffer);

    // A live transition may deliberately use two internally consistent
    // quality ranges in this callback, but the requested mode itself belongs
    // to the one accepted callback frame above.
    const bool requestedHq =
        activeAudioCallbackParameterSnapshot.requestedHq;

    // Measure the raw host input against the final output, using a separate
    // latency-matched reference. The crossover/solo dry bus is not raw input.
    const auto& matchFrame = activeAudioCallbackParameterSnapshot.loudnessMatch;
    const bool needsLoudnessReference = ! hostBypassShadow && loudnessMatch.needsReference(matchFrame);
    if (needsLoudnessReference)
    {
        if (! loudnessReferenceWasActive)
            loudnessReferenceDelay.reset();
        loudnessReferenceDelay.setDelay(activeHqMode
            ? totalLatency.load(std::memory_order_relaxed)
            : static_cast<float>(juce::roundToInt(totalLatency.load(std::memory_order_relaxed))));
        loudnessReference.makeCopyOf(buffer, true);
        juce::dsp::AudioBlock<float> referenceBlock(loudnessReference);
        juce::dsp::ProcessContextReplacing<float> referenceContext(referenceBlock);
        loudnessReferenceDelay.process(referenceContext);
    }
    loudnessReferenceWasActive = needsLoudnessReference;

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

    loudnessMatch.process(loudnessReference, buffer, matchFrame,
        capturedStableCallbackState && ! topologyTransitionActive && ! startTopologyTransition
        && ! hqTransitionNeedsService && ! hasPendingTopologyChange());

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
    if (publishMeterPacket)
        calculateAndStoreLevels(mWetBuffer,
                                mOutputLeftRMSGlobal,
                                mOutputRightRMSGlobal,
                                mOutputLeftPeakGlobal,
                                mOutputRightPeakGlobal);

    // --- 1. Push Modulated Filter Data to its FIFO ---
    if (filterFifo.getFreeSpace() >= 1)
    {
        // Get the final, modulated values for this block
        auto chainSettings = getCachedChainSettings(&lfoOutputBuffer);

        ModulatedFilterValues filterVals;
        filterVals.captureEpoch = filterTelemetryCaptureEpoch.load(
            std::memory_order_acquire);
        filterVals.lowCutFreq = chainSettings.lowCutFreq;
        filterVals.lowCutGain = chainSettings.lowCutGainInDecibels;
        filterVals.lowCutQ = chainSettings.lowCutQuality;
        filterVals.highCutFreq = chainSettings.highCutFreq;
        filterVals.highCutGain = chainSettings.highCutGainInDecibels;
        filterVals.highCutQ = chainSettings.highCutQuality;
        filterVals.peakFreq = chainSettings.peakFreq;
        filterVals.peakGain = chainSettings.peakGainInDecibels;
        filterVals.peakQ = chainSettings.peakQuality;
        for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
        {
            auto node = eqProcessor.currentState(slot);
            const auto& state = activeAudioCallbackParameterSnapshot.globalFilter.eqNodes[static_cast<size_t>(slot)].state;
            node.present = state.present;
            node.bypassed = state.bypassed;
            node.type = state.type;
            node.slope = state.slope;
            if (fire::eq::usesLegacyShape(slot, state.type))
            {
                node.frequency = slot == 0 ? cachedGlobalFilterSettings.lowCutFreq
                    : slot == 1 ? cachedGlobalFilterSettings.peakFreq : cachedGlobalFilterSettings.highCutFreq;
                node.gainDb = slot == 0 ? cachedGlobalFilterSettings.lowCutGainInDecibels
                    : slot == 1 ? cachedGlobalFilterSettings.peakGainInDecibels : cachedGlobalFilterSettings.highCutGainInDecibels;
                node.q = slot == 0 ? cachedGlobalFilterSettings.lowCutQuality
                    : slot == 1 ? cachedGlobalFilterSettings.peakQuality : cachedGlobalFilterSettings.highCutQuality;
            }
            filterVals.eqNodes[static_cast<size_t>(slot)] = node;
        }

        pushToFifo(filterFifo, filterFifoBuffer, filterVals);
    }

    if (graphFifo.getFreeSpace() >= 1)
    {
        DistortionGraphValues vals;
        // A single acquire pairs the selected band with the generation copied
        // into this packet. Reading separate band/epoch atomics here would let
        // a focus change label Band A values as Band B (or vice versa).
        vals.sourceToken = distortionGraphSourceToken.load(std::memory_order_acquire);
        const int bandIndex = static_cast<int>(vals.sourceToken
                                               & distortionGraphBandMask);
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

        vals.mode = fire::analog::resolve(juce::roundToInt(loadCachedParameter(parameters.mode)), juce::roundToInt(loadCachedParameter(parameters.shapeModel)));
        const bool isSafeModeOn = loadCachedParameter(parameters.safe) > 0.5f;

        float driveForCalc = driveBase * 6.5f / 100.0f;
        float powerDrive = powf(2, driveForCalc);
        float sampleMaxValue = getSampleMaxValue(bandIndex);

        if (driveEnabled && isSafeModeOn
            && sampleMaxValue > 0.0001f && sampleMaxValue * powerDrive > 2.0f)
            vals.drive = 2.0f / sampleMaxValue + 0.1f * driveForCalc;
        else
            vals.drive = powerDrive;

        const bool downsampleEnabled =
            loadCachedParameter(downsampleEnabledParameter) > 0.5f;
        vals.rateDivide = downsampleEnabled
                              ? getBlockModulatedValue(downsampleRateParameter,
                                                       lfoOutputBuffer)
                              : 1.0f;

        pushToFifo(graphFifo, graphFifoBuffer, vals);
    }
    if (publishMeterPacket)
        publishMeterValues(true);

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

    // The immutable main snapshot includes the inactive A/B state. A/B state
    // replacements use the same topology generation as parameter/LFO
    // replacements; inactive-only copies are serialised by StateAB's lock.
    // Therefore every section below belongs to one valid state generation.
    auto mainState = captureCoherentSerializableMainStateSnapshot();
    xmlState.setAttribute("stateFormatVersion", hostStateFormatVersion);
    xmlState.setAttribute("ottSchemaVersion", 1);
    xmlState.setAttribute("insertEffectsSchemaVersion", 1);
    xmlState.setAttribute("modulationEffectsSchemaVersion", fire::modulation_fx::schemaVersion);
    xmlState.setAttribute("resonatorSchemaVersion", fire::resonator_params::schemaVersion);
    xmlState.setAttribute("driveCompSchemaVersion", fire::drive_comp::schemaVersion);
    xmlState.setAttribute("modulationSourcesSchemaVersion", fire::mod_sources::schemaVersion);
    xmlState.setAttribute("moduleOrderSchemaVersion", 1);
    xmlState.setAttribute("cloudsSchemaVersion", fire::clouds_params::schemaVersion);
    xmlState.setAttribute("eqSchemaVersion", 1);
    xmlState.setAttribute("coreModulesSchemaVersion", fire::core_modules::schemaVersion);
    xmlState.setAttribute("analogShapesSchemaVersion", fire::analog_params::schemaVersion);
    xmlState.setAttribute("reverbModelsSchemaVersion", fire::reverb_params::schemaVersion);
    xmlState.setAttribute("lfoBankSchemaVersion", fire::lfo_bank::schemaVersion);
    xmlState.setAttribute("savedParameterCount",
                          mainState.parameterState.getNumChildren());

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    std::function<void()> mainCaptureHook;
    {
        const juce::ScopedLock lock(serializableStateHookLock);
        mainCaptureHook = std::move(hostStateMainCaptureHookForTesting);
        hostStateMainCaptureHookForTesting = {};
    }
    if (mainCaptureHook)
        mainCaptureHook();
#endif

    // 1. save treestate (parameters)
    std::unique_ptr<juce::XmlElement> treeStateXml(
        mainState.parameterState.createXml());
    for (auto* parameter : treeStateXml->getChildIterator())
        if (fire::clouds_params::isReservedEngineParameterID(parameter->getStringAttribute("id")))
            parameter->setAttribute("value", 1.0f);
    xmlState.insertChildElement(treeStateXml.release(), xmlIndex++);

    // 2. save current preset ID, width and height
    auto currentStateXml = std::make_unique<juce::XmlElement>("otherState");
    currentStateXml->setAttribute("currentPresetID", mainState.currentPresetID);
    currentStateXml->setAttribute("currentPresetKey", mainState.currentPresetKey);
    currentStateXml->setAttribute("editorWidth", mainState.editorWidth);
    currentStateXml->setAttribute("editorHeight", mainState.editorHeight);
    currentStateXml->setAttribute("loudnessMatchVersion", 1);
    currentStateXml->setAttribute("loudnessMatchEnabled", mainState.loudnessMatch.enabled);
    currentStateXml->setAttribute("loudnessMatchReadyA", mainState.loudnessMatch.ready[0]);
    currentStateXml->setAttribute("loudnessMatchReadyB", mainState.loudnessMatch.ready[1]);
    currentStateXml->setAttribute("loudnessMatchGainA", mainState.loudnessMatch.gainDb[0]);
    currentStateXml->setAttribute("loudnessMatchGainB", mainState.loudnessMatch.gainDb[1]);

    xmlState.insertChildElement(currentStateXml.release(), xmlIndex++);

    // 3. Save LFO Shapes
    auto lfoState = std::make_unique<juce::XmlElement>("LFO_STATE");
    for (int i = 0; i < static_cast<int>(mainState.lfoData.size()); ++i)
    {
        auto lfoXml = std::make_unique<juce::XmlElement>("LFO");
        lfoXml->setAttribute("index", i);
        mainState.lfoData[static_cast<size_t>(i)].writeToXml(*lfoXml);
        lfoState->addChildElement(lfoXml.release());
    }
    xmlState.insertChildElement(lfoState.release(), xmlIndex++);

    // 4. Save Modulation Matrix Routings
    auto modMatrixState = std::make_unique<juce::XmlElement>("MODULATION_STATE");
    for (const auto& routing : mainState.routings)
    {
        auto routingXml = std::make_unique<juce::XmlElement>("ROUTING");
        routing.writeToXml(*routingXml);
        modMatrixState->addChildElement(routingXml.release());
    }
    xmlState.insertChildElement(modMatrixState.release(), xmlIndex++);

    // Persist the inactive A/B snapshot alongside the matching active state.
    xmlState.addChildElement(new juce::XmlElement(mainState.abState));
    fire::effects::writeFrozenAudioState(xmlState, mainState.frozenAudio);

    copyXmlToBinary(xmlState, destData);
}

void FireAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName("state"))
        return;
    fire::effects::FrozenRecordings frozenAudio;
    if (!fire::effects::readFrozenAudioState(*xmlState, frozenAudio)) return;

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

    // APVTS always serialises a complete list of unique id/value children.
    // Validate even unknown legacy IDs so duplicates or junk cannot be used to
    // make a sparse, damaged tree look complete. Unknown but well-formed IDs
    // remain available for backward compatibility and are ignored below.
    juce::StringArray incomingParameterIDs;
    std::set<juce::String> incomingParameterIDSet;
    for (const auto& incomingChild : incomingParameterState)
    {
        const auto parameterID = incomingChild.getProperty("id").toString();
        if (parameterID.isEmpty() || incomingParameterIDSet.contains(parameterID)
            || ! incomingChild.hasProperty("value"))
        {
            return;
        }

        double parsedValue = 0.0;
        if (! parseStrictFiniteDouble(
                incomingChild.getProperty("value").toString(), parsedValue))
        {
            return;
        }

        incomingParameterIDs.add(parameterID);
        incomingParameterIDSet.insert(parameterID);
    }

    const bool hasStateFormatVersion =
        xmlState->hasAttribute("stateFormatVersion");
    int ottParameterCount = 0;
    for (const auto& id : incomingParameterIDs)
        if (ParameterIDAndName::isOttParameterID(id))
            ++ottParameterCount;
    if (xmlState->hasAttribute("ottSchemaVersion") || ottParameterCount > 0)
    {
        int version = 1;
        if ((xmlState->hasAttribute("ottSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "ottSchemaVersion", version) || version != 1))
            || ottParameterCount != 4 * (OttProcessor::controlCount + 1))
            return;
    }
    const bool hasSavedParameterCount =
        xmlState->hasAttribute("savedParameterCount");
    int insertParameterCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::effects::isParameterID(id)) ++insertParameterCount;
    if (xmlState->hasAttribute("insertEffectsSchemaVersion") || insertParameterCount > 0)
    {
        int version = 1;
        if ((xmlState->hasAttribute("insertEffectsSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "insertEffectsSchemaVersion", version) || version != 1))
            || insertParameterCount != fire::effects::parameterCount)
            return;
    }
    int modulationEffectCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::modulation_fx::isParameterID(id)) ++modulationEffectCount;
    if (xmlState->hasAttribute("modulationEffectsSchemaVersion") || modulationEffectCount > 0)
    {
        int version = fire::modulation_fx::schemaVersion;
        if ((xmlState->hasAttribute("modulationEffectsSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "modulationEffectsSchemaVersion", version)
                 || version != fire::modulation_fx::schemaVersion))
            || modulationEffectCount != fire::modulation_fx::parameterCount)
            return;
        for (const auto& child : incomingParameterState)
            if (fire::modulation_fx::isParameterID(child.getProperty("id").toString()))
            {
                double value = 0;
                if (! parseStrictFiniteDouble(child.getProperty("value").toString(), value)
                    || value < 0 || value > 2 || value != std::floor(value)) return;
            }
    }
    int resonatorCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::resonator_params::isParameterID(id)) ++resonatorCount;
    if (xmlState->hasAttribute("resonatorSchemaVersion") || resonatorCount > 0)
    {
        int version = fire::resonator_params::schemaVersion;
        if ((xmlState->hasAttribute("resonatorSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "resonatorSchemaVersion", version)
                 || version != fire::resonator_params::schemaVersion))
            || resonatorCount != fire::resonator_params::parameterCount)
            return;
        for (const auto& child : incomingParameterState)
            if (fire::resonator_params::isParameterID(child.getProperty("id").toString()))
            {
                double value = 0;
                if (! parseStrictFiniteDouble(child.getProperty("value").toString(), value)
                    || (value != 0.0 && value != 1.0)) return;
            }
    }
    int driveCompCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::drive_comp::isParameterID(id)) ++driveCompCount;
    if (xmlState->hasAttribute("driveCompSchemaVersion") || driveCompCount > 0)
    {
        int version = fire::drive_comp::schemaVersion;
        if ((xmlState->hasAttribute("driveCompSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "driveCompSchemaVersion", version)
                 || version != fire::drive_comp::schemaVersion))
            || driveCompCount != fire::drive_comp::parameterCount) return;
        for (const auto& child : incomingParameterState)
            if (fire::drive_comp::isParameterID(child.getProperty("id").toString()))
            {
                double value = 0;
                if (! parseStrictFiniteDouble(child.getProperty("value").toString(), value)
                    || (value != 0.0 && value != 1.0)) return;
            }
    }
    int reverbCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::reverb_params::isParameterID(id)) ++reverbCount;
    if (xmlState->hasAttribute("reverbModelsSchemaVersion") || reverbCount > 0)
    {
        int version = 1;
        if ((xmlState->hasAttribute("reverbModelsSchemaVersion") && (!parseStrictNonNegativeIntegerAttribute(*xmlState, "reverbModelsSchemaVersion", version)
                || version != fire::reverb_params::schemaVersion)) || reverbCount != fire::reverb_params::parameterCount) return;
    }
    int analogCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::analog_params::isParameterID(id)) ++analogCount;
    if (xmlState->hasAttribute("analogShapesSchemaVersion") || analogCount > 0)
    {
        int version = 1;
        if ((xmlState->hasAttribute("analogShapesSchemaVersion") && (!parseStrictNonNegativeIntegerAttribute(*xmlState, "analogShapesSchemaVersion", version)
                || version != fire::analog_params::schemaVersion)) || analogCount != fire::analog_params::parameterCount) return;
    }
    int coreModuleCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::core_modules::isParameterID(id)) ++coreModuleCount;
    if (xmlState->hasAttribute("coreModulesSchemaVersion") || coreModuleCount > 0)
    {
        int version = 1;
        if ((xmlState->hasAttribute("coreModulesSchemaVersion")
             && (!parseStrictNonNegativeIntegerAttribute(*xmlState, "coreModulesSchemaVersion", version)
                 || version != fire::core_modules::schemaVersion))
            || coreModuleCount != fire::core_modules::parameterCount) return;
    }
    int moduleOrderCount = 0;
    int auxiliaryCount = 0;
    for (const auto& id : incomingParameterIDs) if (fire::mod_sources::isParameterID(id)) ++auxiliaryCount;
    const bool hasAuxiliaryState = xmlState->hasAttribute("modulationSourcesSchemaVersion") || auxiliaryCount > 0;
    if (hasAuxiliaryState)
    {
        int version = 0;
        if (auxiliaryCount != fire::mod_sources::parameterCount
            || (xmlState->hasAttribute("modulationSourcesSchemaVersion")
                && (!parseStrictNonNegativeIntegerAttribute(*xmlState, "modulationSourcesSchemaVersion", version)
                    || version != fire::mod_sources::schemaVersion))) return;
    }
    for (const auto& id : incomingParameterIDs) if (fire::module_order::isParameterID(id)) ++moduleOrderCount;
    if (xmlState->hasAttribute("moduleOrderSchemaVersion") || moduleOrderCount > 0)
    {
        int version = 1;
        if ((xmlState->hasAttribute("moduleOrderSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "moduleOrderSchemaVersion", version) || version != 1))
            || moduleOrderCount != fire::module_order::parameterCount) return;
    }
    int cloudsParameterCount = 0;
    int cloudsStateVersion = 0;
    for (const auto& id : incomingParameterIDs)
        if (fire::clouds_params::isParameterID(id)) ++cloudsParameterCount;
    if (xmlState->hasAttribute("cloudsSchemaVersion") || cloudsParameterCount > 0)
    {
        cloudsStateVersion = 1;
        if ((xmlState->hasAttribute("cloudsSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "cloudsSchemaVersion", cloudsStateVersion)
                 || cloudsStateVersion < 1 || cloudsStateVersion > fire::clouds_params::schemaVersion))
            || cloudsParameterCount != fire::clouds_params::parameterCount)
            return;
    }
    int eqParameterCount = 0;
    for (const auto& id : incomingParameterIDs)
        if (fire::eq::isAppendedParameterID(id)) ++eqParameterCount;
    if (xmlState->hasAttribute("eqSchemaVersion") || eqParameterCount > 0)
    {
        int version = 1;
        if ((xmlState->hasAttribute("eqSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "eqSchemaVersion", version) || version != 1))
            || eqParameterCount != fire::eq::appendedParameterCount) return;
    }
    int lfoBankParameterCount = 0;
    for (const auto& id : incomingParameterIDs)
        if (fire::lfo_bank::isAppendedParameterID(id)) ++lfoBankParameterCount;
    const bool hasLfoBankState = xmlState->hasAttribute("lfoBankSchemaVersion") || lfoBankParameterCount > 0;
    const int lfoCountInState = hasLfoBankState ? fire::lfo_bank::capacity : fire::lfo_bank::defaultCount;
    if (hasLfoBankState)
    {
        int version = fire::lfo_bank::schemaVersion;
        const auto* lfoState = xmlState->getChildByName("LFO_STATE");
        if ((xmlState->hasAttribute("lfoBankSchemaVersion")
             && (! parseStrictNonNegativeIntegerAttribute(*xmlState, "lfoBankSchemaVersion", version)
                 || version != fire::lfo_bank::schemaVersion))
            || lfoBankParameterCount != fire::lfo_bank::appendedParameterCount
            || lfoState == nullptr || countDirectChildrenWithTagName(*xmlState, "LFO_STATE") != 1
            || ! isValidVersionedHostLfoState(*lfoState, lfoCountInState)) return;
    }
    if (hasStateFormatVersion != hasSavedParameterCount)
        return;

    if (hasStateFormatVersion)
    {
        int incomingFormatVersion = 0;
        int declaredParameterCount = 0;
        const auto* versionedLfoState = xmlState->getChildByName("LFO_STATE");
        const auto* versionedRoutingState =
            xmlState->getChildByName("MODULATION_STATE");
        if (! parseStrictNonNegativeIntegerAttribute(
                *xmlState, "stateFormatVersion", incomingFormatVersion)
            || ! parseStrictNonNegativeIntegerAttribute(
                *xmlState, "savedParameterCount", declaredParameterCount)
            || incomingFormatVersion != hostStateFormatVersion
            || declaredParameterCount != incomingParameterIDs.size()
            || xmlState->getChildByName("otherState") == nullptr
            || versionedLfoState == nullptr
            || versionedRoutingState == nullptr
            || countDirectChildrenWithTagName(*xmlState, "LFO_STATE") != 1
            || countDirectChildrenWithTagName(*xmlState,
                                              "MODULATION_STATE") != 1
            || ! isValidVersionedHostLfoState(*versionedLfoState, lfoCountInState)
            || ! isValidVersionedHostRoutingState(*versionedRoutingState,
                                                  treeState, hasAuxiliaryState ? fire::mod_sources::sourceCount : lfoCountInState)
            || xmlState->getChildByName("AB_STATE") == nullptr)
        {
            return;
        }
    }
    else if (incomingParameterIDs.size()
             < oldestWrappedHostParameterCount)
    {
        // Fire 0.751 introduced the outer <state> wrapper with 19 APVTS
        // children. Older direct-PARAMETERS chunks are a different format and
        // were never accepted by this loader. Fewer children therefore means
        // this wrapped state was truncated, not merely saved by an old build.
        return;
    }

    for (const auto* anchor : legacyHostStateParameterAnchors)
        if (! incomingParameterIDs.contains(anchor))
            return;

    // The first wrapped state used one global "mix" parameter. Early
    // multiband builds temporarily replaced it with mix1..mix4, and later
    // versions restored the global control. Accept either complete historical
    // family without weakening the structural checks above.
    bool hasCompleteMixFamily = incomingParameterIDs.contains(MIX_ID);
    if (! hasCompleteMixFamily)
    {
        hasCompleteMixFamily = true;
        for (int band = 0; band < 4; ++band)
            hasCompleteMixFamily = hasCompleteMixFamily
                                   && incomingParameterIDs.contains(
                                       ParameterIDAndName::getIDString(
                                           MIX_ID, band));
    }
    if (! hasCompleteMixFamily)
        return;

    const auto parameterStateTemplate = treeState.copyState();
    juce::ValueTree treeToLoad(parameterStateTemplate.getType());
    std::set<juce::String> loadedParameterIDs;
    std::array<bool, fire::lfo_bank::capacity> smoothnessPresentInParameterState {};

    std::map<juce::String, juce::ValueTree> stagedParameters;
    const auto findParameterState = [&](const juce::String& parameterID)
    {
        const auto found = stagedParameters.find(parameterID);
        return found == stagedParameters.end() ? juce::ValueTree{} : found->second;
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
                          fire::drive_comp::isParameterID(parameterID) ? 0.0f
                              : parameter->convertFrom0to1(parameter->getDefaultValue()),
                          nullptr);
        treeToLoad.addChild(child, -1, nullptr);
        stagedParameters.emplace(parameterID, child);
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
        auto targetChild = findParameterState(parameterID);
        if (! targetChild.isValid())
            return;

        targetChild.setProperty("value", safeValue, nullptr);
        loadedParameterIDs.insert(parameterID);
        ++recognisedParameterCount;

        for (int i = 0; i < static_cast<int>(smoothnessPresentInParameterState.size()); ++i)
            if (parameterID == ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i))
                smoothnessPresentInParameterState[static_cast<size_t>(i)] = true;
    }

    // Only pre-v2 Granular slots whose old engine was Legacy are remapped.
    // The canonical tree already contains finite, legal normalised insert
    // controls; absence of the old engine field is an explicit Legacy marker.
    bool migratedLegacyGranular = false;
    juce::StringArray migratedCloudsRoutingTargets;
    for (int scope = 0; scope < fire::clouds_params::scopeCount; ++scope)
        for (int slot = 0; slot < fire::clouds_params::slotCount; ++slot)
        {
            const auto engineID = fire::clouds_params::parameterID(scope, slot, fire::clouds_params::engineField);
            auto engineState = findParameterState(engineID);
            const auto typeState = findParameterState(
                fire::effects::parameterID(scope, slot, fire::effects::typeField));
            const bool wasLegacy = ! loadedParameterIDs.contains(engineID)
                               || static_cast<float>(engineState.getProperty("value", 0.0f)) < 0.5f;
            if (cloudsStateVersion < fire::clouds_params::schemaVersion
                && static_cast<int>(typeState.getProperty("value", 0)) == 4 && wasLegacy)
            {
                migratedLegacyGranular = true;
                std::array<float, 6> values;
                std::array<juce::ValueTree, 6> controls;
                for (int control = 0; control < 6; ++control)
                {
                    controls[static_cast<size_t>(control)] = findParameterState(
                        fire::effects::parameterID(scope, slot, control));
                    values[static_cast<size_t>(control)] = static_cast<float>(
                        controls[static_cast<size_t>(control)].getProperty("value", 0.5f));
                }
                values = fire::clouds_params::migrateLegacyGranular(values);
                for (size_t control = 0; control < controls.size(); ++control)
                    controls[control].setProperty("value", values[control], nullptr);
                // These controls were inaudible in Legacy. In particular,
                // do not freeze an empty new engine using an obsolete flag.
                for (int field = fire::clouds_params::freezeField; field < fire::clouds_params::fieldCount; ++field)
                    findParameterState(fire::clouds_params::parameterID(scope, slot, field))
                        .setProperty("value", fire::clouds_params::defaults[static_cast<size_t>(field)], nullptr);
                for (int field = fire::clouds_params::spreadField; field < fire::clouds_params::fieldCount; ++field)
                    migratedCloudsRoutingTargets.add(fire::clouds_params::parameterID(scope, slot, field));
            }
            engineState.setProperty("value", 1.0f, nullptr);
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

        if (auto shapeState = findParameterState(shapeID); shapeState.isValid())
            shapeState.setProperty("value", legacyShapeValue, nullptr);
    }

    std::array<LfoData, fire::lfo_bank::capacity> loadedLfoData;
    std::array<bool, fire::lfo_bank::capacity> loadedLfoSmoothnessFromXml {};
    std::array<bool, fire::lfo_bank::capacity> loadedLfoIndices {};
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
        const auto smoothnessID = ParameterIDAndName::getIDString(
            LFO_SMOOTH_ID, static_cast<int>(i));
        auto smoothnessState = findParameterState(smoothnessID);
        if (! smoothnessState.isValid())
            continue;

        if (! smoothnessPresentInParameterState[i]
            && loadedLfoSmoothnessFromXml[i])
        {
            if (auto* parameter = treeState.getParameter(smoothnessID))
            {
                const auto& range = parameter->getNormalisableRange();
                const float smoothness = range.snapToLegalValue(juce::jlimit(
                    range.start,
                    range.end,
                    loadedLfoData[i].smoothness));
                loadedLfoData[i].smoothness = smoothness;
                smoothnessState.setProperty("value", smoothness, nullptr);
            }
            continue;
        }

        if (smoothnessState.isValid())
        {
            const float smoothness = static_cast<float>(smoothnessState.getProperty("value"));
            if (std::isfinite(smoothness))
                loadedLfoData[i].smoothness = juce::jlimit(0.0f, 1.0f, smoothness);
        }
    }

    juce::Array<ModulationRouting> loadedRoutings;
    juce::StringArray loadedRoutingTargets;
    if (auto* modMatrixState = xmlState->getChildByName("MODULATION_STATE"))
    {
        for (auto* routingXml : modMatrixState->getChildIterator())
        {
            if (loadedRoutings.size()
                >= LfoManager::maximumModulationRoutings)
                break;
            if (! routingXml->hasTagName("ROUTING"))
                continue;

            auto routing = ModulationRouting::readFromXml(*routingXml);
            if (!hasAuxiliaryState) routing.sourceLfoIndex = juce::jmin(routing.sourceLfoIndex, fire::lfo_bank::capacity - 1);
            if (routing.targetParameterID.isEmpty()
                || treeState.getParameter(routing.targetParameterID) == nullptr
                || fire::mod_sources::isParameterID(routing.targetParameterID)
                || migratedCloudsRoutingTargets.contains(routing.targetParameterID)
                || loadedRoutingTargets.contains(routing.targetParameterID))
                continue;

            loadedRoutingTargets.add(routing.targetParameterID);
            loadedRoutings.add(std::move(routing));
        }
    }

    const auto* xmlCurrentState = xmlState->getChildByName("otherState");
    fire::dsp::LoudnessMatchState::Settings restoredLoudnessMatch;
    if (! readLoudnessMatchSettings(xmlCurrentState, restoredLoudnessMatch))
        return;
    const auto presetKey = xmlCurrentState != nullptr
                               ? xmlCurrentState->getStringAttribute("currentPresetKey").trim()
                               : juce::String {};
    const int legacyPresetID = xmlCurrentState != nullptr
                                   ? juce::jlimit(0,
                                                  statePresets.getNumPresets(),
                                                  xmlCurrentState->getIntAttribute("currentPresetID", 0))
                                   : 0;
    const auto currentEditorSize = getSavedEditorSize();
    const auto restoredEditorSize = xmlCurrentState != nullptr
                                        ? normaliseEditorSize(
                                              xmlCurrentState->getIntAttribute(
                                                  "editorWidth", static_cast<int>(INIT_WIDTH)),
                                              xmlCurrentState->getIntAttribute(
                                                  "editorHeight", static_cast<int>(INIT_HEIGHT)))
                                        : normaliseEditorSize(currentEditorSize.width,
                                                              currentEditorSize.height);

    // Commit only after the complete chunk has passed validation. Keep the
    // audio thread on its previous coherent multiband snapshot until the APVTS
    // state and modulation routings have both been replaced.
    {
        beginMultibandTopologyEdit();
        const juce::ScopeGuard finishTopologyEdit { [this]
        {
            requestMultibandTopologyReset();
        } };
        treeState.replaceState(treeToLoad);
        restoreFrozenAudio(frozenAudio);
        if (xmlCurrentState != nullptr)
        {
            if (presetKey.isNotEmpty())
                statePresets.setCurrentPresetKey(presetKey);
            else
                statePresets.setCurrentPresetId(legacyPresetID);
            setSavedEditorSize(restoredEditorSize.width,
                               restoredEditorSize.height);
        }

        lfoManager->replaceLfoDataAndRoutings(loadedLfoData,
                                              std::move(loadedRoutings));

        // A/B restoration mutates its internal snapshot, so keep it inside the
        // commit phase after the complete host chunk has passed validation.
        bool migratedAlternateGranular = false;
        if (! stateAB.readFromXml(xmlState->getChildByName("AB_STATE"), &migratedAlternateGranular)
            || migratedLegacyGranular || migratedAlternateGranular)
            restoredLoudnessMatch = {}; // A fallback or engine migration changed the calibrated sound.
        loudnessMatch.restore(restoredLoudnessMatch);

        // The scope guard publishes even if a foreign synchronous listener
        // throws, so no failed restore can strand the generation odd or retain
        // the recursive writer lock indefinitely.
    }
    if (editHistory) editHistory->requestReset();
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
    const auto normalisedSource = static_cast<std::uint64_t>(
        juce::isPositiveAndBelow(bandIndex, globalHistorySourceIndex)
            ? bandIndex
            : globalHistorySourceIndex);
    auto currentToken = historySourceRequestToken.load(
        std::memory_order_relaxed);

    for (;;)
    {
        if ((currentToken & historySourceMask) == normalisedSource)
            return;

        auto nextEpoch = (currentToken & ~historySourceMask)
                         + (historySourceMask + 1u);
        if ((nextEpoch & ~historySourceMask) == 0)
            nextEpoch = historySourceMask + 1u;

        const auto nextToken = nextEpoch | normalisedSource;
        if (historySourceRequestToken.compare_exchange_weak(
                currentToken,
                nextToken,
                std::memory_order_release,
                std::memory_order_relaxed))
            return;
    }
}

void FireAudioProcessor::captureHistorySamples()
{
    const std::array<const juce::AudioBuffer<float>*, 5> sourceBuffers {
        &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4, &mWetBuffer
    };
    const auto requestedSourceToken = historySourceRequestToken.load(
        std::memory_order_acquire);
    const int sourceIndex = juce::jlimit(
        0,
        4,
        static_cast<int>(requestedSourceToken & historySourceMask));
    const auto* sourceBuffer = sourceBuffers[static_cast<size_t>(sourceIndex)];
    const bool sourceChanged = requestedSourceToken != activeHistorySourceToken;

    const bool hasSamples = sourceBuffer != nullptr
                            && sourceBuffer->getNumChannels() > 0
                            && sourceBuffer->getNumSamples() > 0;
    if (! sourceChanged && ! hasSamples)
        return;

    // This is the sole history writer. The odd/even sequence lets the message
    // thread reject a snapshot that overlaps these relaxed atomic writes,
    // without adding a lock or allocating on the audio thread.
    historyPublicationSequence.fetch_add(1, std::memory_order_acq_rel);

    if (sourceChanged)
    {
        activeHistorySourceToken = requestedSourceToken;
        historyWritePosition.store(0, std::memory_order_relaxed);
        historySamplesAvailable.store(0, std::memory_order_relaxed);
        historySamplesUntilCapture = 0;
    }

    int writePosition = historyWritePosition.load(std::memory_order_relaxed);
    int samplesAvailable = historySamplesAvailable.load(
        std::memory_order_relaxed);
    int capturedSamples = 0;

    if (hasSamples)
    {
        const auto* left = sourceBuffer->getReadPointer(0);
        const auto* right = sourceBuffer->getNumChannels() > 1
                                ? sourceBuffer->getReadPointer(1)
                                : left;

        int sample = historySamplesUntilCapture;
        for (; sample < sourceBuffer->getNumSamples();
             sample += historyDecimationFactor)
        {
            const auto index = static_cast<size_t>(writePosition);
            historyArrayL[index].store(left[sample], std::memory_order_relaxed);
            historyArrayR[index].store(right[sample], std::memory_order_relaxed);
            writePosition = (writePosition + 1) % historyLength;
            ++capturedSamples;
        }

        historySamplesUntilCapture = sample - sourceBuffer->getNumSamples();
    }

    samplesAvailable = juce::jmin(historyLength,
                                  samplesAvailable + capturedSamples);
    historySamplesAvailable.store(samplesAvailable,
                                  std::memory_order_relaxed);
    historyWritePosition.store(writePosition, std::memory_order_relaxed);
    publishedHistorySourceToken.store(activeHistorySourceToken,
                                      std::memory_order_relaxed);
    historyGeneration.fetch_add(1, std::memory_order_relaxed);
    historyPublicationSequence.fetch_add(1, std::memory_order_release);
}

std::uint64_t FireAudioProcessor::getHistorySourceToken() const noexcept
{
    return historySourceRequestToken.load(std::memory_order_acquire);
}

std::uint64_t FireAudioProcessor::getHistoryGeneration() const noexcept
{
    return historyGeneration.load(std::memory_order_acquire);
}

bool FireAudioProcessor::copyHistorySnapshot(HistorySnapshot& destination) const
{
    constexpr int maximumAttempts = 4;
    for (int attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        const auto requestedTokenBefore = historySourceRequestToken.load(
            std::memory_order_acquire);
        const auto sequenceBefore = historyPublicationSequence.load(
            std::memory_order_acquire);
        if ((sequenceBefore & 1u) != 0u)
            continue;

        const auto publishedToken = publishedHistorySourceToken.load(
            std::memory_order_relaxed);
        const auto generation = historyGeneration.load(
            std::memory_order_relaxed);
        const int writePosition = historyWritePosition.load(
            std::memory_order_relaxed);
        const int count = juce::jlimit(
            0,
            historyLength,
            historySamplesAvailable.load(std::memory_order_relaxed));
        const int start = (writePosition - count + historyLength)
                          % historyLength;

        destination.left.resize(count);
        destination.right.resize(count);
        for (int i = 0; i < count; ++i)
        {
            const auto index = static_cast<size_t>(
                (start + i) % historyLength);
            destination.left.setUnchecked(
                i,
                historyArrayL[index].load(std::memory_order_relaxed));
            destination.right.setUnchecked(
                i,
                historyArrayR[index].load(std::memory_order_relaxed));
        }

        // Prevent the copied atomic samples from moving past the validation
        // read on weakly ordered CPUs. A changed/odd sequence then rejects the
        // whole candidate rather than exposing a torn ring-buffer frame.
        std::atomic_thread_fence(std::memory_order_acq_rel);
        const auto sequenceAfter = historyPublicationSequence.load(
            std::memory_order_relaxed);
        const auto requestedTokenAfter = historySourceRequestToken.load(
            std::memory_order_acquire);
        if (sequenceBefore == sequenceAfter
            && (sequenceAfter & 1u) == 0u
            && requestedTokenBefore == requestedTokenAfter
            && publishedToken == requestedTokenAfter)
        {
            destination.sourceToken = publishedToken;
            destination.generation = generation;
            return true;
        }
    }

    destination.left.clearQuick();
    destination.right.clearQuick();
    destination.sourceToken = historySourceRequestToken.load(
        std::memory_order_acquire);
    destination.generation = 0;
    return false;
}

void FireAudioProcessor::copyHistoryArrays(juce::Array<float>& leftDestination,
                                           juce::Array<float>& rightDestination) const
{
    HistorySnapshot snapshot;
    if (! copyHistorySnapshot(snapshot))
    {
        leftDestination.clearQuick();
        rightDestination.clearQuick();
        return;
    }

    leftDestination.swapWith(snapshot.left);
    rightDestination.swapWith(snapshot.right);
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
                                            int originalDestinationSize,
                                            std::uint64_t minimumCaptureEpoch) noexcept
{
    return spectrumProcessor.popLatestFramePair(processedDestination,
                                                 processedDestinationSize,
                                                 originalDestination,
                                                 originalDestinationSize,
                                                 minimumCaptureEpoch);
}

std::uint64_t FireAudioProcessor::requestFreshFFTFrameEpoch() noexcept
{
    return spectrumProcessor.requestFreshCaptureEpoch();
}

void FireAudioProcessor::pushDataPairToFFT(const juce::AudioBuffer<float>& processedBuffer,
                                           const juce::AudioBuffer<float>& originalBuffer)
{
    spectrumProcessor.beginInputBlock();

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

std::uint64_t FireAudioProcessor::packEditorSize(const int width,
                                                 const int height) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(width)) << 32u)
           | static_cast<std::uint32_t>(height);
}

FireAudioProcessor::SavedEditorSize
FireAudioProcessor::unpackEditorSize(const std::uint64_t packedSize) noexcept
{
    return {
        static_cast<int>(static_cast<std::uint32_t>(packedSize >> 32u)),
        static_cast<int>(static_cast<std::uint32_t>(packedSize))
    };
}

FireAudioProcessor::SavedEditorSize
FireAudioProcessor::normaliseEditorSize(const int width, const int height) noexcept
{
    constexpr int minimumWidth = static_cast<int>(INIT_WIDTH);
    constexpr int minimumHeight = static_cast<int>(INIT_HEIGHT);
    constexpr int maximumWidth = 2000;
    constexpr int maximumHeight = 1000;

    const auto constrainedWidth = juce::jlimit(minimumWidth,
                                                maximumWidth,
                                                width);
    const auto constrainedHeight = juce::jlimit(minimumHeight,
                                                 maximumHeight,
                                                 height);

    // Editor layout scale is determined by its smaller axis. Preserve that
    // effective scale when repairing legacy or torn non-2:1 state, rather
    // than unexpectedly enlarging the restored window.
    const auto normalisedHeight = juce::jmin(constrainedHeight,
                                              constrainedWidth / 2);
    return { normalisedHeight * 2, normalisedHeight };
}

void FireAudioProcessor::setSavedEditorSize(const int width,
                                            const int height) noexcept
{
    const auto normalisedSize = normaliseEditorSize(width, height);
    editorSize.store(packEditorSize(normalisedSize.width,
                                    normalisedSize.height),
                     std::memory_order_relaxed);
}

FireAudioProcessor::SavedEditorSize
FireAudioProcessor::getSavedEditorSize() const noexcept
{
    return unpackEditorSize(editorSize.load(std::memory_order_relaxed));
}

void FireAudioProcessor::setSavedWidth(const int width) noexcept
{
    auto current = editorSize.load(std::memory_order_relaxed);
    for (;;)
    {
        const auto currentSize = unpackEditorSize(current);
        const auto desired = packEditorSize(width, currentSize.height);
        if (editorSize.compare_exchange_weak(current,
                                             desired,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed))
            return;
    }
}

void FireAudioProcessor::setSavedHeight(const int height) noexcept
{
    auto current = editorSize.load(std::memory_order_relaxed);
    for (;;)
    {
        const auto currentSize = unpackEditorSize(current);
        const auto desired = packEditorSize(currentSize.width, height);
        if (editorSize.compare_exchange_weak(current,
                                             desired,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed))
            return;
    }
}

int FireAudioProcessor::getSavedWidth() const noexcept
{
    return getSavedEditorSize().width;
}

int FireAudioProcessor::getSavedHeight() const noexcept
{
    return getSavedEditorSize().height;
}

bool FireAudioProcessor::getBypassedState() const noexcept
{
    return isBypassed.load(std::memory_order_acquire);
}

std::uint64_t FireAudioProcessor::getHostBypassPresentationEpoch() const noexcept
{
    return hostBypassPresentationEpoch.load(std::memory_order_acquire);
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

    // Append new parameters and use a newer AU version hint so every existing
    // automation index and ID keeps its original meaning.
    for (int band = 0; band < 4; ++band)
    {
        parameters.push_back(std::make_unique<PBool>(
            juce::ParameterID { ParameterIDAndName::getIDString(OTT_ENABLED_ID, band), 2 },
            "OTT Enable " + juce::String(band + 1), false));
        for (size_t control = 0; control < OttProcessor::defaults.size(); ++control)
            parameters.push_back(std::make_unique<PFloat>(
                juce::ParameterID { ParameterIDAndName::getIDString(ParameterIDAndName::ottControlIDs[control], band), 2 },
                juce::String(ParameterIDAndName::ottControlNames[control]) + " " + juce::String(band + 1),
                juce::NormalisableRange<float>(OttProcessor::minimums[control], OttProcessor::maximums[control],
                    control == OttProcessor::depth || control == OttProcessor::mix ? 0.01f : 0.1f),
                OttProcessor::defaults[control]));
    }

    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            const auto prefix = (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope)) + " FX " + juce::String(slot + 1);
            for (int field = 0; field < fire::effects::fieldCount; ++field)
            {
                const juce::ParameterID id {fire::effects::parameterID(scope, slot, field), 3};
                if (field == fire::effects::typeField)
                    parameters.push_back(std::make_unique<PChoice>(id, prefix + " Type", juce::StringArray {"Empty", "Chorus", "Delay", "Reverb", "Granular", "Lo-Fi"}, 0));
                else if (field == fire::effects::enabledField)
                    parameters.push_back(std::make_unique<PBool>(id, prefix + " Enabled", true));
                else if (field == fire::effects::orderField)
                    parameters.push_back(std::make_unique<PInt>(id, prefix + " Order", 0, fire::effects::slotCount, 0));
                else
                    parameters.push_back(std::make_unique<PFloat>(id, prefix + " Control " + juce::String(field + 1),
                        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
            }
        }
    for (size_t i = 0; i < fire::effects::tapeIDs.size(); ++i)
        parameters.push_back(std::make_unique<PFloat>(juce::ParameterID {fire::effects::tapeIDs[i], 3},
            "Lo-Fi " + juce::String(fire::effects::tapeNames[i]), juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int node = 0; node < fire::module_order::capacity; ++node)
            if (fire::module_order::valid(scope, node))
                parameters.push_back(std::make_unique<PInt>(juce::ParameterID {fire::module_order::parameterID(scope, node), 4},
                    (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope)) + " Module " + juce::String(node + 1) + " Order",
                    -1, fire::module_order::capacity - 1, -1));

    // Append the extension after every historical parameter. The original
    // insert choice/ranges and AU indices retain their saved meaning.
    for (int scope = 0; scope < fire::clouds_params::scopeCount; ++scope)
        for (int slot = 0; slot < fire::clouds_params::slotCount; ++slot)
            for (int field = 0; field < fire::clouds_params::fieldCount; ++field)
            {
                const auto prefix = (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope))
                                  + " FX " + juce::String(slot + 1) + " Clouds ";
                const juce::ParameterID id { fire::clouds_params::parameterID(scope, slot, field), 5 };
                const auto name = prefix + fire::clouds_params::fieldNames[static_cast<size_t>(field)];
                if (field == fire::clouds_params::engineField)
                    parameters.push_back(std::make_unique<PChoice>(id, name + " (Reserved)",
                        juce::StringArray { "Reserved", "Clouds" }, 1,
                        juce::AudioParameterChoiceAttributes().withAutomatable(false).withMeta(true)));
                else if (field == fire::clouds_params::freezeField)
                    parameters.push_back(std::make_unique<PBool>(id, name, false));
                else
                    parameters.push_back(std::make_unique<PFloat>(id, name,
                        juce::NormalisableRange<float>(0.0f, 1.0f),
                        fire::clouds_params::defaults[static_cast<size_t>(field)]));
            }

    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
        for (int fieldIndex = 0; fieldIndex < fire::eq::fieldCount; ++fieldIndex)
        {
            using namespace fire::eq;
            const auto field = static_cast<Field>(fieldIndex);
            if (! isAppendedParameter(slot, field)) continue;
            const juce::ParameterID id { parameterID(slot, field), 6 };
            const auto name = "EQ " + juce::String(slot + 1) + " ";
            if (field == Field::present)
                parameters.push_back(std::make_unique<PBool>(id, name + "Present", slot < 3));
            else if (field == Field::bypassed)
                parameters.push_back(std::make_unique<PBool>(id, name + "Bypassed", false));
            else if (field == Field::type)
            {
                juce::StringArray choices;
                for (const auto* typeName : typeNames) choices.add(typeName);
                parameters.push_back(std::make_unique<PChoice>(id, name + "Type", choices, static_cast<int>(defaultType(slot))));
            }
            else if (field == Field::slope)
                parameters.push_back(std::make_unique<PInt>(id, name + "Slope", 0, 3, 0));
            else if (field == Field::frequency)
                parameters.push_back(std::make_unique<PFloat>(id, name + "Frequency", cutoffRange, 1000.0f));
            else if (field == Field::gain)
                parameters.push_back(std::make_unique<PFloat>(id, name + "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
            else if (field == Field::q)
                parameters.push_back(std::make_unique<PFloat>(id, name + "Q", juce::NormalisableRange<float>(0.1f, 18.0f, 0.01f), 0.70710678f));
        }

    // Keep all historical timing parameter positions intact. New timing groups
    // and presence controls form one append-only AU v7 family.
    for (int index = fire::lfo_bank::defaultCount; index < fire::lfo_bank::capacity; ++index)
    {
        using namespace fire::lfo_bank;
        const auto prefix = "LFO " + juce::String(index + 1) + " ";
        parameters.push_back(std::make_unique<PBool>(juce::ParameterID {parameterID(index, Field::syncMode), 7}, prefix + "Sync", true));
        parameters.push_back(std::make_unique<PChoice>(juce::ParameterID {parameterID(index, Field::rateSync), 7}, prefix + "Synced Rate", lfoRateSyncDivisions, 8));
        parameters.push_back(std::make_unique<PFloat>(juce::ParameterID {parameterID(index, Field::rateHz), 7}, prefix + "Rate",
            juce::NormalisableRange<float>(0.01f, 100.0f, 0.01f, 0.3f), 1.0f, "Hz"));
        parameters.push_back(std::make_unique<PFloat>(juce::ParameterID {parameterID(index, Field::smoothness), 7}, prefix + "Smoothness",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(juce::ParameterID {parameterID(index, Field::phase), 7}, prefix + "Phase",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    }
    for (int index = 0; index < fire::lfo_bank::capacity; ++index)
        parameters.push_back(std::make_unique<PBool>(
            juce::ParameterID {fire::lfo_bank::presentParameterID(index), 7},
            "LFO " + juce::String(index + 1) + " Present", fire::lfo_bank::defaultPresent(index)));

    // Freeze the historical Type choice and its 0/.2/.../1 automation values.
    // This family is appended after AU v7, leaving every existing index intact.
    for (int scope = 0; scope < fire::modulation_fx::scopeCount; ++scope)
        for (int slot = 0; slot < fire::modulation_fx::slotCount; ++slot)
            parameters.push_back(std::make_unique<PChoice>(
                juce::ParameterID {fire::modulation_fx::parameterID(scope, slot), 8},
                (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope))
                    + " FX " + juce::String(slot + 1) + " Modulation Type",
                juce::StringArray {"Standard", "Flanger", "Phaser"}, 0));

    // Independent flags preserve both existing effect-choice ranges.
    for (int scope = 0; scope < fire::resonator_params::scopeCount; ++scope)
        for (int slot = 0; slot < fire::resonator_params::slotCount; ++slot)
            parameters.push_back(std::make_unique<PBool>(
                juce::ParameterID {fire::resonator_params::parameterID(scope, slot), 9},
                (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope))
                    + " FX " + juce::String(slot + 1) + " Chord Resonator", false));

    for (int band = 0; band < fire::drive_comp::parameterCount; ++band)
        parameters.push_back(std::make_unique<PBool>(
            juce::ParameterID {fire::drive_comp::parameterID(band), 10},
            "Drive Comp Modern " + juce::String(band + 1), true));

    for (int index = 0; index < fire::mod_sources::parameterCount; ++index)
    {
        const auto i = static_cast<size_t>(index);
        const auto name = index == 0 ? juce::String("Envelope Attack") : index == 1 ? juce::String("Envelope Release")
            : index == 2 ? juce::String("Envelope Sensitivity") : "Macro " + juce::String(index - 2);
        parameters.push_back(std::make_unique<PFloat>(juce::ParameterID{fire::mod_sources::ids[i], 11}, name,
            juce::NormalisableRange<float>(fire::mod_sources::minimums[i], fire::mod_sources::maximums[i],
                index < 2 ? 0.1f : index == 2 ? 0.1f : 0.001f, index < 2 ? 0.35f : 1.0f), fire::mod_sources::defaults[i]));
    }
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
    {
        const auto prefix = scope == 0 ? juce::String("Master") : "Band " + juce::String(scope);
        for (int node = 0; node < (scope == 0 ? 3 : 5); ++node)
            parameters.push_back(std::make_unique<PBool>(juce::ParameterID{fire::core_modules::presenceID(scope, node), 12},
                prefix + " " + (scope == 0 && node == 2 ? "Analysis" : fire::effects::name(fire::core_modules::legacyType(scope, node))) + " Present", true));
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            const auto fx = prefix + " FX " + juce::String(slot + 1);
            parameters.push_back(std::make_unique<PChoice>(juce::ParameterID{fire::core_modules::parameterID(scope, slot, 0), 12},
                fx + " Core Type", juce::StringArray{"Standard", "Drive", "Shape", "Compressor", "OTT", "Stereo", "EQ"}, 0));
            parameters.push_back(std::make_unique<PFloat>(juce::ParameterID{fire::core_modules::parameterID(scope, slot, 1), 12},
                fx + " Jitter", juce::NormalisableRange<float>{0, 1}, 0));
            for (int node = 0; node < fire::eq::maxNodes; ++node)
            {
                const auto state = fire::eq::defaultNode(node);
                const auto point = fx + " EQ " + juce::String(node + 1);
                const auto id = [&](fire::eq::Field field) {return juce::ParameterID{fire::core_modules::eqParameterID(scope, slot, node, field), 12};};
                parameters.push_back(std::make_unique<PFloat>(id(fire::eq::Field::frequency), point + " Frequency", juce::NormalisableRange<float>{20, 20000, 0, 0.3f}, state.frequency));
                parameters.push_back(std::make_unique<PFloat>(id(fire::eq::Field::gain), point + " Gain", juce::NormalisableRange<float>{-24, 24}, state.gainDb));
                parameters.push_back(std::make_unique<PFloat>(id(fire::eq::Field::q), point + " Q", juce::NormalisableRange<float>{0.1f, 20, 0, 0.35f}, state.q));
                parameters.push_back(std::make_unique<PChoice>(id(fire::eq::Field::slope), point + " Slope", juce::StringArray{"12", "24", "36", "48"}, 0));
                parameters.push_back(std::make_unique<PChoice>(id(fire::eq::Field::type), point + " Type", juce::StringArray{"Bell", "Low cut", "High cut", "Low shelf", "High shelf", "Notch", "Band pass"}, static_cast<int>(state.type)));
                parameters.push_back(std::make_unique<PBool>(id(fire::eq::Field::present), point + " Present", state.present));
                parameters.push_back(std::make_unique<PBool>(id(fire::eq::Field::bypassed), point + " Bypassed", false));
            }
        }
    }
    juce::StringArray shapeModels {"Legacy"};
    for (auto name : fire::analog::names) shapeModels.add(name);
    for (int band = 0; band < 4; ++band)
        parameters.push_back(std::make_unique<PChoice>(juce::ParameterID{fire::analog_params::bandID(band), 13}, "Band " + juce::String(band + 1) + " Shape Model", shapeModels, 0));
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
            parameters.push_back(std::make_unique<PChoice>(juce::ParameterID{fire::analog_params::parameterID(scope, slot), 13},
                (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope)) + " FX " + juce::String(slot + 1) + " Shape Model", shapeModels, 0));
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
            parameters.push_back(std::make_unique<PFloat>(juce::ParameterID{fire::analog_params::driveID(scope, slot), 13},
                (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope)) + " FX " + juce::String(slot + 1) + " Analog Drive",
                juce::NormalisableRange<float>{0, 100}, 0));
    juce::StringArray reverbs;
    for (auto name : fire::space::names) reverbs.add(name);
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
            parameters.push_back(std::make_unique<PChoice>(juce::ParameterID{fire::reverb_params::parameterID(scope, slot), 14},
                (scope == 0 ? juce::String("Master") : "Band " + juce::String(scope)) + " FX " + juce::String(slot + 1) + " Reverb Model", reverbs, 0));
    return { parameters.begin(), parameters.end() };
}

void FireAudioProcessor::prepareInsertParameters(int scope, fire::effects::RackParameters& destination) const
{
    float bpm = 120.0f;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto value = position->getBpm(); value && std::isfinite(*value) && *value > 0) bpm = static_cast<float>(*value);
    for (size_t slot = 0; slot < destination.size(); ++slot)
    {
        const auto& cache = insertParameters[static_cast<size_t>(scope)][slot];
        auto& target = destination[slot];
        target.effect.publicationSequence = multibandTopologyResetGeneration.load(std::memory_order_seq_cst) & ~std::uint32_t{1};
        target.effect.type = getInsertEffectType(scope, static_cast<int>(slot));
        target.effect.reverbModel = juce::roundToInt(loadCachedParameter(reverbModelParameters[static_cast<size_t>(scope)][slot]));
        target.effect.shapeModel = juce::roundToInt(loadCachedParameter(shapeModelParameters[static_cast<size_t>(scope)][slot]));
        const auto& analogDrive = analogDriveParameters[static_cast<size_t>(scope)][slot];
        target.effect.analogDrive.baseValue = loadCachedParameter(analogDrive);
        target.effect.analogDrive.range = {0, 100}; target.effect.analogDrive.modulationDepth = 0; target.effect.analogDrive.lfoSignal = nullptr;
        target.analogDriveSource = -1;
        LfoManager::AudioThreadRoutingInfo analogRouting;
        if (analogDrive.ranged && lfoManager->getAudioThreadRoutingInfo(analogDrive.ranged, analogRouting))
        {target.effect.analogDrive.modulationDepth = analogRouting.depth; target.effect.analogDrive.isBipolar = analogRouting.isBipolar; target.analogDriveSource = analogRouting.sourceLfoIndex;}
        target.effect.analogDriveSource = target.analogDriveSource;

        target.effect.enabled = loadCachedParameter(cache[fire::effects::enabledField]) > 0.5f;
        target.effect.normalised = true;
        target.effect.bpm = bpm;
        target.order = juce::roundToInt(loadCachedParameter(cache[fire::effects::orderField]));
        for (size_t control = 0; control < fire::effects::controlCount; ++control)
        {
            auto& provider = target.effect.values[control];
            provider.baseValue = loadCachedParameter(cache[control], 0.5f);
            provider.range = {0.0f, 1.0f}; provider.lfoSignal = nullptr;
            target.sources[control] = -1;
            LfoManager::AudioThreadRoutingInfo routing;
            if (cache[control].ranged && lfoManager->getAudioThreadRoutingInfo(cache[control].ranged, routing))
            {
                provider.modulationDepth = routing.depth; provider.isBipolar = routing.isBipolar;
                target.sources[control] = routing.sourceLfoIndex;
            }
        }
        const auto& core = coreModuleParameters[static_cast<size_t>(scope)][slot];
        auto& jitter = target.effect.jitter;
        jitter.baseValue = loadCachedParameter(core[fire::core_modules::jitterField]);
        jitter.range = {0, 1}; jitter.lfoSignal = nullptr; jitter.modulationDepth = 0;
        target.jitterSource = -1;
        LfoManager::AudioThreadRoutingInfo jitterRouting;
        if (core[fire::core_modules::jitterField].ranged && lfoManager->getAudioThreadRoutingInfo(core[fire::core_modules::jitterField].ranged, jitterRouting))
        {jitter.modulationDepth = jitterRouting.depth; jitter.isBipolar = jitterRouting.isBipolar; target.jitterSource = jitterRouting.sourceLfoIndex;}
        if (target.effect.type == fire::effects::Type::eq)
            for (int node = 0; node < fire::eq::maxNodes; ++node)
            {
                auto& destination = target.effect.eq[static_cast<size_t>(node)];
                const auto field = [&](fire::eq::Field index) -> const CachedParameter&
                { return core[static_cast<size_t>(2 + node * fire::eq::fieldCount + static_cast<int>(index))]; };
                destination.state.present = loadCachedParameter(field(fire::eq::Field::present)) > 0.5f;
                destination.state.bypassed = loadCachedParameter(field(fire::eq::Field::bypassed)) > 0.5f;
                destination.state.type = static_cast<fire::eq::Type>(juce::jlimit(0, 6, juce::roundToInt(loadCachedParameter(field(fire::eq::Field::type)))));
                destination.state.slope = juce::jlimit(0, 3, juce::roundToInt(loadCachedParameter(field(fire::eq::Field::slope))));
                destination.generation = 0;
                for (int control = 0; control < 3; ++control)
                {
                    const auto& cached = field(static_cast<fire::eq::Field>(control));
                    auto& value = destination.controls[static_cast<size_t>(control)];
                    value.value = loadCachedParameter(cached); value.depth = 0; value.source = -1; value.signal = nullptr;
                    LfoManager::AudioThreadRoutingInfo routing;
                    if (cached.ranged && lfoManager->getAudioThreadRoutingInfo(cached.ranged, routing))
                    { value.depth = routing.depth; value.bipolar = routing.isBipolar; value.source = routing.sourceLfoIndex; }
                }
                destination.state.frequency = destination.controls[0].value;
                destination.state.gainDb = destination.controls[1].value;
                destination.state.q = destination.controls[2].value;
            }
        const auto& clouds = cloudsParameters[static_cast<size_t>(scope)][slot];
        const bool isGranular = target.effect.type == fire::effects::Type::granular;
        target.effect.clouds.freeze = isGranular
            && loadCachedParameter(clouds[fire::clouds_params::freezeField]) > 0.5f;
        for (size_t control = 0; control < target.effect.clouds.values.size(); ++control)
        {
            const auto field = static_cast<size_t>(fire::clouds_params::spreadField) + control;
            auto& provider = target.effect.clouds.values[control];
            provider.lfoSignal = nullptr;
            target.cloudsSources[control] = -1;
            // Empty/non-granular slots have no Clouds modulation work.
            if (! isGranular) continue;
            provider.baseValue = loadCachedParameter(clouds[field], fire::clouds_params::defaults[field]);
            provider.range = { 0.0f, 1.0f };
            LfoManager::AudioThreadRoutingInfo routing;
            if (clouds[field].ranged && lfoManager->getAudioThreadRoutingInfo(clouds[field].ranged, routing))
            {
                provider.modulationDepth = routing.depth;
                provider.isBipolar = routing.isBipolar;
                target.cloudsSources[control] = routing.sourceLfoIndex;
            }
        }
    }
}

bool FireAudioProcessor::isDawPlaying() const
{
    return lfoManager->isDawPlaying();
}

float FireAudioProcessor::getLfoPhase(int lfoIndex) const
{
    return lfoManager->getLfoPhase(lfoIndex);
}

LfoManager::VisualState FireAudioProcessor::getLfoVisualState(int lfoIndex) const noexcept
{
    return lfoManager->getLfoVisualState(lfoIndex);
}

bool FireAudioProcessor::updateParameters(
    const juce::AudioBuffer<float>& lfoOutputs,
    std::uint32_t topologySequenceAtCallbackStart,
    bool routingSnapshotWasRefreshed,
    HqCallbackContext& callbackContext)
{
    //==============================================================================
    // 1. Update Global and Crossover Parameters
    //==============================================================================

    auto& requestedSnapshot = audioCallbackWorkspace->requestedTopology;
    auto& requestedCallbackParameters = audioCallbackWorkspace->requestedParameters;
    const bool hasStablePublication = tryCaptureMultibandTopologySnapshot(
        lfoOutputs,
        topologySequenceAtCallbackStart,
        routingSnapshotWasRefreshed,
        requestedSnapshot,
        requestedCallbackParameters);
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
        // Once the generation is even this is a complete publication. Preserve
        // the established automation semantics: non-structural controls and
        // LFO state become active at this callback boundary, while a destructive
        // band-count change alone waits for the topology fade-to-zero reset.
        activeAudioCallbackParameterSnapshot =
            std::move(requestedCallbackParameters);
        activeAudioCallbackParameterSnapshotInitialised = true;
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
            appliedMultibandTopologyResetGeneration = requestedSnapshot.publicationSequence;
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
    return hasStablePublication;
}

void FireAudioProcessor::prepareHqCallbackContext(
    const juce::AudioBuffer<float>& lfoOutputs,
    int snapshotNumBands,
    HqCallbackContext& callbackContext)
{
    juce::ignoreUnused(lfoOutputs);
    // Reconstruct directly in caller-owned storage. Assigning a default
    // HqCallbackContext creates a six-figure-byte temporary on the audio stack.
    std::destroy_at(std::addressof(callbackContext));
    std::construct_at(std::addressof(callbackContext));
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
        params.mode = fire::analog::resolve(juce::roundToInt(loadCachedParameter(parameters.mode)), juce::roundToInt(loadCachedParameter(parameters.shapeModel)));
        params.isOutputLinked = isModulePresent(i + 1, 0) && loadCachedParameter(parameters.linked) > 0.5f;
        params.useModernDriveComp = loadCachedParameter(parameters.modernDriveComp) > 0.5f;
        params.isDriveEnabled = isModulePresent(i + 1, 0) && loadCachedParameter(parameters.driveEnabled) > 0.5f;
        params.isShapeEnabled = isModulePresent(i + 1, 1) && loadCachedParameter(parameters.shapeEnabled) > 0.5f;
        params.isCompEnabled = isModulePresent(i + 1, 2) && loadCachedParameter(parameters.compressorEnabled) > 0.5f;
        params.ott.enabled = isModulePresent(i + 1, 4) && loadCachedParameter(parameters.ottEnabled) > 0.5f;
        prepareInsertParameters(i + 1, params.inserts);
        params.moduleOrder = getModuleOrder(i + 1);
        params.isWidthEnabled = isModulePresent(i + 1, 3) && loadCachedParameter(parameters.widthEnabled) > 0.5f;
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

        for (size_t control = 0; control < params.ott.controls.size(); ++control)
            setupProvider(params.ott.controls[control], params.ott.sources[control], parameters.ottControls[control]);

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

        if (params.isOutputLinked && ! params.useModernDriveComp)
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

void FireAudioProcessor::prepareAudioCallbackParameterSnapshot(
    std::uint32_t publicationSequence,
    AudioCallbackParameterSnapshot& snapshot) const
{
    std::destroy_at(std::addressof(snapshot));
    std::construct_at(std::addressof(snapshot));
    snapshot.publicationSequence = publicationSequence;
    snapshot.requestedHq = loadCachedParameter(hqParameter) > 0.5f;
    snapshot.downsampleEnabled =
        isModulePresent(0, 1) && loadCachedParameter(downsampleEnabledParameter) > 0.5f;
    snapshot.loudnessMatch = loudnessMatch.capture(stateAB.isCurrentA() ? 0 : 1);
    snapshot.lfoParameters =
        lfoManager->captureAudioThreadParameterSnapshot();

    const auto prepareModulatedParameter =
        [this] (const CachedParameter& parameter,
                ModulatedParameterSnapshot& destination)
    {
        destination = ModulatedParameterSnapshot {};
        const float defaultValue = parameter.ranged != nullptr
                                       ? parameter.ranged->convertFrom0to1(
                                             parameter.ranged->getDefaultValue())
                                       : 0.0f;
        destination.provider.baseValue = loadCachedParameter(parameter,
                                                              defaultValue);
        if (parameter.ranged == nullptr)
            return;

        destination.provider.range =
            parameter.ranged->getNormalisableRange();
        LfoManager::AudioThreadRoutingInfo routingInfo;
        if (lfoManager->getAudioThreadRoutingInfo(parameter.ranged,
                                                  routingInfo))
        {
            destination.provider.modulationDepth = routingInfo.depth;
            destination.provider.isBipolar = routingInfo.isBipolar;
            destination.lfoSourceIndex = routingInfo.sourceLfoIndex;
        }
    };

    auto& filter = snapshot.globalFilter;
    filter.enabled = isModulePresent(0, 0) && loadCachedParameter(filterEnabledParameter) > 0.5f;
    prepareModulatedParameter(filterParameterCache.lowCutFrequency,
                              filter.lowCutFrequency);
    prepareModulatedParameter(filterParameterCache.lowCutGain,
                              filter.lowCutGain);
    prepareModulatedParameter(filterParameterCache.lowCutQuality,
                              filter.lowCutQuality);
    prepareModulatedParameter(filterParameterCache.peakFrequency,
                              filter.peakFrequency);
    prepareModulatedParameter(filterParameterCache.peakGain,
                              filter.peakGain);
    prepareModulatedParameter(filterParameterCache.peakQuality,
                              filter.peakQuality);
    prepareModulatedParameter(filterParameterCache.highCutFrequency,
                              filter.highCutFrequency);
    prepareModulatedParameter(filterParameterCache.highCutGain,
                              filter.highCutGain);
    prepareModulatedParameter(filterParameterCache.highCutQuality,
                              filter.highCutQuality);

    filter.baseSettings.lowCutFreq =
        filter.lowCutFrequency.provider.baseValue;
    filter.baseSettings.lowCutGainInDecibels =
        filter.lowCutGain.provider.baseValue;
    filter.baseSettings.lowCutQuality =
        filter.lowCutQuality.provider.baseValue;
    filter.baseSettings.lowCutSlope =
        getSlopeParameterValue(filterParameterCache.lowCutSlope.raw);
    filter.baseSettings.lowCutBypassed =
        loadCachedParameter(filterParameterCache.lowCutBypassed) > 0.5f;
    filter.baseSettings.peakFreq = filter.peakFrequency.provider.baseValue;
    filter.baseSettings.peakGainInDecibels =
        filter.peakGain.provider.baseValue;
    filter.baseSettings.peakQuality = filter.peakQuality.provider.baseValue;
    filter.baseSettings.peakBypassed =
        loadCachedParameter(filterParameterCache.peakBypassed) > 0.5f;
    filter.baseSettings.highCutFreq =
        filter.highCutFrequency.provider.baseValue;
    filter.baseSettings.highCutGainInDecibels =
        filter.highCutGain.provider.baseValue;
    filter.baseSettings.highCutQuality =
        filter.highCutQuality.provider.baseValue;
    filter.baseSettings.highCutSlope =
        getSlopeParameterValue(filterParameterCache.highCutSlope.raw);
    filter.baseSettings.highCutBypassed =
        loadCachedParameter(filterParameterCache.highCutBypassed) > 0.5f;

    const std::array<const ModulatedParameterSnapshot*, 9> legacyControls {
        &filter.lowCutFrequency, &filter.lowCutGain, &filter.lowCutQuality,
        &filter.peakFrequency, &filter.peakGain, &filter.peakQuality,
        &filter.highCutFrequency, &filter.highCutGain, &filter.highCutQuality
    };
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
    {
        using namespace fire::eq;
        auto& node = filter.eqNodes[static_cast<size_t>(slot)];
        node.generation = eqNodeGenerations[static_cast<size_t>(slot)].load(std::memory_order_relaxed);
        const auto& cached = eqParameterCache[static_cast<size_t>(slot)];
        const auto read = [&](Field field, float fallback)
        { return loadCachedParameter(cached[static_cast<size_t>(field)], fallback); };
        node.state = defaultNode(slot);
        node.state.present = read(Field::present, node.state.present ? 1.0f : 0.0f) > 0.5f;
        node.state.bypassed = read(Field::bypassed, 0.0f) > 0.5f;
        node.state.type = static_cast<Type>(juce::jlimit(0, 6, juce::roundToInt(read(Field::type,
                                                                                static_cast<float>(node.state.type)))));
        node.state.slope = juce::jlimit(0, 3, juce::roundToInt(read(Field::slope, 0.0f)));
        for (int control = 0; control < 3; ++control)
        {
            ModulatedParameterSnapshot value;
            if (slot < 3) value = *legacyControls[static_cast<size_t>(slot * 3 + control)];
            else prepareModulatedParameter(cached[static_cast<size_t>(control)], value);
            node.controls[static_cast<size_t>(control)] = value.provider;
            node.sources[static_cast<size_t>(control)] = value.lfoSourceIndex;
        }
        node.state.frequency = node.controls[0].baseValue;
        node.state.gainDb = node.controls[1].baseValue;
        node.state.q = node.controls[2].baseValue;
        node.enabled = node.state.present && ! node.state.bypassed
                       && ! usesLegacyShape(slot, node.state.type);
    }
    filter.baseSettings.lowCutBypassed = filter.baseSettings.lowCutBypassed
        || ! filter.eqNodes[0].state.present || ! fire::eq::usesLegacyShape(0, filter.eqNodes[0].state.type);
    filter.baseSettings.peakBypassed = filter.baseSettings.peakBypassed
        || ! filter.eqNodes[1].state.present || ! fire::eq::usesLegacyShape(1, filter.eqNodes[1].state.type);
    filter.baseSettings.highCutBypassed = filter.baseSettings.highCutBypassed
        || ! filter.eqNodes[2].state.present || ! fire::eq::usesLegacyShape(2, filter.eqNodes[2].state.type);

    prepareModulatedParameter(globalOutputParameter, snapshot.globalOutput);
    prepareModulatedParameter(globalMixParameter, snapshot.globalMix);
    prepareModulatedParameter(downsampleRateParameter,
                              snapshot.downsampleRate);
    prepareModulatedParameter(bitDepthParameter, snapshot.bitDepth);
    prepareModulatedParameter(jitterParameter, snapshot.jitter);
    prepareModulatedParameter(downsampleMixParameter,
                              snapshot.downsampleMix);
    for (size_t i = 0; i < snapshot.tape.size(); ++i) prepareModulatedParameter(tapeParameters[i], snapshot.tape[i]);
    prepareInsertParameters(0, snapshot.inserts);
    snapshot.moduleOrder = getModuleOrder(0);
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
    auto chainSettings = getSnapshotChainSettingsAtSample(lfoOutputs,
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

void FireAudioProcessor::applyGlobalEffects(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& lfoOutputs, double sampleRate, bool highQuality)
{
    masterOrderDry.makeCopyOf(buffer, true);
    auto alignedOrderDry = juce::dsp::AudioBlock<float>(masterOrderDry);
    masterOrderDryDelay.process(juce::dsp::ProcessContextReplacing<float>(alignedOrderDry));
    const auto& order = masterOrderTransition.begin(activeAudioCallbackParameterSnapshot.moduleOrder);
    auto block = juce::dsp::AudioBlock<float>(buffer);
    for (int node : order)
    {
        if (node == 0) applyMasterFilter(buffer, lfoOutputs, sampleRate);
        else if (node == 1) applyDownsamplingEffect(buffer, lfoOutputs);
        else if (node >= fire::module_order::firstInsert)
            masterInserts.processSlot(block, node - fire::module_order::firstInsert, activeAudioCallbackParameterSnapshot.inserts, lfoOutputs, 0, highQuality);
        // Analysis is a view-only row; its position has no audio operation.
    }
    masterOrderTransition.apply(block, masterOrderDry);
    applyMasterOutput(buffer, lfoOutputs);
}

void FireAudioProcessor::applyMasterFilter(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& lfoOutputs, double sampleRate)
{
    {
        const auto& filterSnapshot =
            activeAudioCallbackParameterSnapshot.globalFilter;
        const bool filterEnabled = filterSnapshot.enabled;
        globalFilterMixer.setWetMixProportion(filterEnabled ? 1.0f : 0.0f);
        if (! globalFilterMixerPrimed)
            globalFilterMixer.reset();
        globalFilterMixerPrimed = true;
        globalFilterMixer.pushDrySamples(juce::dsp::AudioBlock<float>(buffer));

        auto block = juce::dsp::AudioBlock<float>(buffer);
        for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
        {
            auto parameters = filterSnapshot.eqNodes[static_cast<size_t>(slot)];
            for (size_t control = 0; control < parameters.controls.size(); ++control)
                if (juce::isPositiveAndBelow(parameters.sources[control], lfoOutputs.getNumChannels())
                    && lfoOutputs.getNumSamples() >= buffer.getNumSamples())
                    parameters.controls[control].lfoSignal = lfoOutputs.getReadPointer(parameters.sources[control]);
            eqProcessor.begin(slot, parameters);
        }

        const float lowCutMix = filterSnapshot.baseSettings.lowCutBypassed
                                    ? 0.0f
                                    : 1.0f;
        const float peakMix = filterSnapshot.baseSettings.peakBypassed
                                  ? 0.0f
                                  : 1.0f;
        const float highCutMix = filterSnapshot.baseSettings.highCutBypassed
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
            eqProcessor.process(0, block, startSample, numSamples);
            processGlobalFilterStage(leftChain.get<ChainPositions::Peak>(),
                                     rightChain.get<ChainPositions::Peak>(),
                                     block,
                                     globalFilterStageDryBuffer,
                                     globalFilterStageMix[peakStage],
                                     startSample,
                                     numSamples);
            eqProcessor.process(1, block, startSample, numSamples);
            processCutFilterStage(leftChain.get<ChainPositions::HighCut>(),
                                  rightChain.get<ChainPositions::HighCut>(),
                                  highCutSlopeTransition,
                                  block,
                                  globalFilterStageMix[highCutStage],
                                  startSample,
                                  numSamples);
            eqProcessor.process(2, block, startSample, numSamples);
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
            for (int slot = 3; slot < fire::eq::maxNodes; ++slot)
                eqProcessor.process(slot, block, startSample, numSamples);
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

}

void FireAudioProcessor::applyMasterOutput(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& lfoOutputs)
{
    // a. Prepare the "recipe" for the global output gain.
    ModulatedValueProvider globalGainProvider;
    if (globalOutputParameter.ranged == nullptr)
        return;

    const auto& globalOutputSnapshot =
        activeAudioCallbackParameterSnapshot.globalOutput;
    globalGainProvider = globalOutputSnapshot.provider;

    int globalGainLfoSourceIndex = globalOutputSnapshot.lfoSourceIndex;
    if (juce::isPositiveAndBelow(globalGainLfoSourceIndex,
                                  lfoOutputs.getNumChannels()))
    {
        globalGainProvider.lfoSignal = lfoOutputs.getReadPointer(
            globalGainLfoSourceIndex);
    }
    else
        globalGainLfoSourceIndex = -1;

    // b. Apply the gain using our new, clean helper function.
    applyGain(buffer,
              globalGainProvider,
              gainProcessorGlobal,
              globalOutputGainTransition,
              globalGainLfoSourceIndex,
              false);
}

void FireAudioProcessor::applyDownsamplingEffect(
    juce::AudioBuffer<float>& buffer,
    const juce::AudioBuffer<float>& lfoOutputs)
{
    const bool isActive =
        activeAudioCallbackParameterSnapshot.downsampleEnabled;
    if (! downsamplingWasActive)
    {
        downsampleSamplesRemaining.fill(0);
        downsampleHeldSamples.fill(0.0f);
        downsampleHoldResiduals.fill(0.0);
        downsamplingWasActive = true;
    }

    // --- 1. Prepare Dry Signal & Mixer ---
    // A copy of the original signal is needed for the dry/wet mix.
    lofiDryBuffer.makeCopyOf(buffer, true);

    const auto configureProvider = [&buffer, &lfoOutputs](
                                       const ModulatedParameterSnapshot& parameter,
                                       ModulatedValueProvider& provider,
                                       int* sourceIndex)
    {
        if (sourceIndex != nullptr)
            *sourceIndex = -1;

        provider = parameter.provider;
        const bool hasCompleteLfoBlock = lfoOutputs.getNumSamples()
                                         >= buffer.getNumSamples();
        if (! hasCompleteLfoBlock
            || std::abs(provider.modulationDepth) <= 1.0e-6f
            || ! juce::isPositiveAndBelow(parameter.lfoSourceIndex,
                                           lfoOutputs.getNumChannels()))
        {
            return false;
        }

        provider.lfoSignal = lfoOutputs.getReadPointer(
            parameter.lfoSourceIndex);
        if (sourceIndex != nullptr)
            *sourceIndex = parameter.lfoSourceIndex;
        return true;
    };

    ModulatedValueProvider rateProvider;
    ModulatedValueProvider bitsProvider;
    ModulatedValueProvider jitterProvider;
    ModulatedValueProvider mixProvider;
    const auto& callbackParameters = activeAudioCallbackParameterSnapshot;
    const bool hasRateModulation = configureProvider(callbackParameters.downsampleRate,
                                                      rateProvider,
                                                      nullptr);
    const bool hasBitsModulation = configureProvider(callbackParameters.bitDepth,
                                                      bitsProvider,
                                                      nullptr);
    const bool hasJitterModulation = configureProvider(callbackParameters.jitter,
                                                        jitterProvider,
                                                        nullptr);
    int mixLfoSourceIndex = -1;
    configureProvider(callbackParameters.downsampleMix,
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

                // Preserve the Rate parameter's advertised fractional
                // resolution.  A hold duration must be an integer number of
                // samples, so carry the unused fraction into the next capture
                // instead of flooring every request independently.  Integer
                // rates remain sample-for-sample identical, while e.g. 2.5x
                // produces deterministic 2, 3, 2, 3 ... holds.
                const double requestedHold = std::isfinite(currentRateReduce)
                                                 ? juce::jlimit(
                                                       1.0,
                                                       128.0,
                                                       static_cast<double>(
                                                           currentRateReduce))
                                                 : 1.0;
                auto& residual = downsampleHoldResiduals[stateIndex];
                if (! std::isfinite(residual) || residual < 0.0
                    || residual >= 1.0)
                    residual = 0.0;

                const double accumulatedHold = requestedHold + residual;
                const int holdSamples = juce::jmax(
                    1,
                    static_cast<int>(std::floor(accumulatedHold)));
                residual = accumulatedHold
                         - static_cast<double>(holdSamples);
                downsampleSamplesRemaining[stateIndex] = holdSamples;
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

    std::array<ModulatedValueProvider, 3> tapeProviders;
    for (size_t i = 0; i < tapeProviders.size(); ++i)
    {
        configureProvider(callbackParameters.tape[i], tapeProviders[i], nullptr);
        tapeSmoothers[i].setTargetValue(juce::jlimit(0.0f, 1.0f, tapeProviders[i].baseValue));
    }
    if (channelsToProcess > 0)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            std::array<float, 3> values;
            for (size_t i = 0; i < values.size(); ++i) values[i] = tapeProviders[i].get(sample, tapeSmoothers[i].getNextValue());
            auto left = buffer.getSample(0, sample);
            auto right = channelsToProcess > 1 ? buffer.getSample(1, sample) : left;
            masterTape.process(left, right, values[0], values[1], values[2]);
            buffer.setSample(0, sample, left);
            if (channelsToProcess > 1) buffer.setSample(1, sample, right);
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
    downsampleHoldResiduals.fill(0.0);
    downsamplingWasActive = false;
}

void FireAudioProcessor::primeLatencyMatchedBypass(
    juce::AudioBuffer<float>& inputBuffer,
    bool useHQ)
{
    if (inputBuffer.getNumChannels() == 0 || inputBuffer.getNumSamples() == 0)
        return;

    mWetBuffer.makeCopyOf(inputBuffer, true);
    const float bypassLatency = useHQ ? totalLatency.load(std::memory_order_acquire)
        : static_cast<float>(juce::roundToInt(totalLatency.load(std::memory_order_acquire)));
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

    const float bypassLatency = useHQ ? totalLatency.load(std::memory_order_acquire)
        : static_cast<float>(juce::roundToInt(totalLatency.load(std::memory_order_acquire)));
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
        && first.dspResetSequence == second.dspResetSequence;
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
    activeHqMode = activeAudioCallbackParameterSnapshotInitialised
                       ? activeAudioCallbackParameterSnapshot.requestedHq
                       : loadCachedParameter(hqParameter) > 0.5f;
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
    applyGlobalEffects(buffer, lfoOutputs, sampleRate, useHQ);
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

        auto& rangeContext = audioCallbackWorkspace->topologyRangeContext;
        rangeContext = activeMultibandTopologySnapshot.callbackContext;
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
            totalLatency.load(std::memory_order_acquire));
    }
    else
    {
        dryWetMixerGlobal.setWetLatency(static_cast<float>(2 * masterInserts.getReservedLatency()));
    }

    const auto& mixSnapshot =
        activeAudioCallbackParameterSnapshot.globalMix;
    ModulatedValueProvider mixProvider = mixSnapshot.provider;
    const bool hasSampleAccurateModulation =
                                             std::abs(mixProvider.modulationDepth) > 1.0e-6f
                                          && juce::isPositiveAndBelow(
                                              mixSnapshot.lfoSourceIndex,
                                              lfoOutputs.getNumChannels())
                                          && lfoOutputs.getNumSamples()
                                                 >= buffer.getNumSamples();
    if (hasSampleAccurateModulation)
    {
        mixProvider.lfoSignal = lfoOutputs.getReadPointer(
            mixSnapshot.lfoSourceIndex);
    }

    auto wetBlock = juce::dsp::AudioBlock<float>(buffer);
    jassert(wetBlock.getNumChannels()
            <= static_cast<size_t>(globalMixAlignedDryBuffer.getNumChannels()));
    jassert(wetBlock.getNumSamples()
            <= static_cast<size_t>(globalMixAlignedDryBuffer.getNumSamples()));

    auto alignedDryBlock = juce::dsp::AudioBlock<float>(globalMixAlignedDryBuffer)
                               .getSubsetChannelBlock(0, wetBlock.getNumChannels())
                               .getSubBlock(0, wetBlock.getNumSamples());
    alignedDryBlock.clear();

    // Keep JUCE's established Base/HQ dry alignment, but force this stage to
    // output dry only. The second, zero-latency stage owns the Mix envelope.
    dryWetMixerGlobal.pushDrySamples(
        juce::dsp::AudioBlock<float>(delayMatchedDryBufferForRange));
    dryWetMixerGlobal.mixWetSamples(alignedDryBlock);

    globalMixMixer.pushDrySamples(alignedDryBlock);
    globalMixMixer.mixWetSamples(
        wetBlock,
        mixProvider,
        mixProvider.baseValue,
        hasSampleAccurateModulation ? mixSnapshot.lfoSourceIndex : -1,
        true);
}

FireAudioProcessor::ModulationInfo FireAudioProcessor::getModulationInfoForParameter(const juce::String& parameterID) const
{
    const juce::ScopedLock sl(lfoManager->getLfoDataLock());
    // Find the routing in the manager's list
    for (const auto& routing : lfoManager->getModulationRoutings())
    {
        if (routing.targetParameterID == parameterID)
        {
            if (isModulationSourcePresent(routing.sourceLfoIndex))
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
            const float safeDepth = juce::jlimit(-1.0f, 1.0f, newDepth);
            if (! juce::exactlyEqual(routing.depth, safeDepth))
            {
                routing.depth = safeDepth;
                lfoManager->advanceModulationRoutingRevisionLocked();
                didUpdate = true;
            }
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
                const float safeDepth = juce::jlimit(-1.0f, 1.0f,
                                                     newDepth);
                if (! juce::exactlyEqual(routing.depth, safeDepth))
                {
                    routing.depth = safeDepth;
                    lfoManager->advanceModulationRoutingRevisionLocked();
                    didUpdate = true;
                }
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
                lfoManager->advanceModulationRoutingRevisionLocked();
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
                if (! juce::exactlyEqual(routing.depth, 0.5f)
                    || ! routing.isBipolar)
                {
                    routing.depth = 0.5f;
                    routing.isBipolar = true;
                    lfoManager->advanceModulationRoutingRevisionLocked();
                    didUpdate = true;
                }
                break;
            }
        }
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
    modulationUiRevision.fetch_add(1, std::memory_order_release);
    updateHostDisplay(
        juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));
    modulationUiChangeBroadcaster.sendChangeMessage();
}

bool FireAudioProcessor::isLfoPresent(int index) const noexcept
{
    return lfoManager->isLfoPresent(index);
}

int FireAudioProcessor::addLfo()
{
    int slot = -1;
    {
        beginMultibandTopologyEdit();
        const juce::ScopeGuard publish { [this] { finishMainStateEdit(false); } };
        for (int index = 0; index < fire::lfo_bank::capacity; ++index)
            if (! isLfoPresent(index)) { slot = index; break; }
        if (slot < 0) return -1;
        // Invalidate old menus/editor contexts before the first synchronous
        // parameter notification can re-enter the UI for this reused slot.
        lfoManager->resetLfoSlot(slot);
        for (int field = 0; field < fire::lfo_bank::timingFieldCount; ++field)
            if (auto* parameter = treeState.getParameter(fire::lfo_bank::parameterID(
                    slot, static_cast<fire::lfo_bank::Field>(field))))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->getDefaultValue());
                parameter->endChangeGesture();
            }
        if (auto* present = treeState.getParameter(fire::lfo_bank::presentParameterID(slot)))
        {
            present->beginChangeGesture();
            present->setValueNotifyingHost(1.0f);
            present->endChangeGesture();
        }
    }
    lfoDataHasChanged();
    return slot;
}

bool FireAudioProcessor::removeLfo(int index)
{
    if (! fire::lfo_bank::validIndex(index)) return false;
    {
        beginMultibandTopologyEdit();
        const juce::ScopeGuard publish { [this] { finishMainStateEdit(false); } };
        if (! isLfoPresent(index)) return false;
        lfoManager->resetLfoSlot(index);
        if (auto* present = treeState.getParameter(fire::lfo_bank::presentParameterID(index)))
        {
            present->beginChangeGesture();
            present->setValueNotifyingHost(0.0f);
            present->endChangeGesture();
        }
        for (int field = 0; field < fire::lfo_bank::timingFieldCount; ++field)
            if (auto* parameter = treeState.getParameter(fire::lfo_bank::parameterID(
                    index, static_cast<fire::lfo_bank::Field>(field))))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->getDefaultValue());
                parameter->endChangeGesture();
            }
    }
    lfoDataHasChanged();
    return true;
}

std::uint64_t FireAudioProcessor::getModulationUiRevision() const noexcept
{
    return modulationUiRevision.load(std::memory_order_acquire);
}

void FireAudioProcessor::addModulationUiChangeListener(
    juce::ChangeListener* listener)
{
    modulationUiChangeBroadcaster.addChangeListener(listener);
}

void FireAudioProcessor::removeModulationUiChangeListener(
    juce::ChangeListener* listener)
{
    modulationUiChangeBroadcaster.removeChangeListener(listener);
}

bool FireAudioProcessor::isModulationUiChangeSource(
    const juce::ChangeBroadcaster* source) const noexcept
{
    return source == &modulationUiChangeBroadcaster;
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

void FireAudioProcessor::publishMeterValues(bool refreshBandMeters)
{
    if (meterFifo.getFreeSpace() < 1)
        return;

    auto values = lastPublishedMeterValues;
    values.bandLevelsAreFresh = refreshBandMeters;

    values.inputRMS_L = mInputLeftRMSGlobal.load();
    values.inputRMS_R = mInputRightRMSGlobal.load();
    values.inputPeak_L = mInputLeftPeakGlobal.load();
    values.inputPeak_R = mInputRightPeakGlobal.load();
    values.outputRMS_L = mOutputLeftRMSGlobal.load();
    values.outputRMS_R = mOutputRightRMSGlobal.load();
    values.outputPeak_L = mOutputLeftPeakGlobal.load();
    values.outputPeak_R = mOutputRightPeakGlobal.load();

    if (refreshBandMeters)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (auto* band = bands[static_cast<size_t>(i)].get())
            {
                values.bandInputRMS_L[static_cast<size_t>(i)] = band->mInputLeftRMS.load();
                values.bandInputRMS_R[static_cast<size_t>(i)] = band->mInputRightRMS.load();
                values.bandInputPeak_L[static_cast<size_t>(i)] = band->mInputLeftPeak.load();
                values.bandInputPeak_R[static_cast<size_t>(i)] = band->mInputRightPeak.load();

                values.bandOutputRMS_L[static_cast<size_t>(i)] = band->mOutputLeftRMS.load();
                values.bandOutputRMS_R[static_cast<size_t>(i)] = band->mOutputRightRMS.load();
                values.bandOutputPeak_L[static_cast<size_t>(i)] = band->mOutputLeftPeak.load();
                values.bandOutputPeak_R[static_cast<size_t>(i)] = band->mOutputRightPeak.load();
                values.ottInputLevelDb[static_cast<size_t>(i)] = band->mOttInputLevelDb.load(std::memory_order_relaxed);
                values.ottGainChangeDb[static_cast<size_t>(i)] = band->mOttGainChangeDb.load(std::memory_order_relaxed);
                values.ottDynamicsActivityDb[static_cast<size_t>(i)] = band->mOttDynamicsActivityDb.load(std::memory_order_relaxed);
            }
        }
    }

    lastPublishedMeterValues = values;
    pushToFifo(meterFifo, meterFifoBuffer, values);
}

bool FireAudioProcessor::getLatestModulatedFilterValues(
    ModulatedFilterValues& values,
    std::uint64_t requiredCaptureEpoch)
{
    const int numAvailable = filterFifo.getNumReady();
    if (numAvailable <= 0)
        return false;

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    filterFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

    ModulatedFilterValues latestMatchingValues;
    bool foundMatchingPacket = false;
    const auto inspectRange = [&] (int start, int size)
    {
        for (int offset = 0; offset < size; ++offset)
        {
            const auto& candidate =
                filterFifoBuffer[static_cast<size_t>(start + offset)];
            if (requiredCaptureEpoch == 0
                || candidate.captureEpoch == requiredCaptureEpoch)
            {
                latestMatchingValues = candidate;
                foundMatchingPacket = true;
            }
        }
    };

    inspectRange(start1, size1);
    inspectRange(start2, size2);
    // Consume rejected packets too. This keeps the SPSC queue available while
    // the filter page or its owning editor is hidden.
    filterFifo.finishedRead(numAvailable);

    if (! foundMatchingPacket)
        return false;

    // A second editor/session may have advanced the epoch while this drain was
    // in progress. Never publish a packet to the superseded presentation.
    if (requiredCaptureEpoch != 0
        && filterTelemetryCaptureEpoch.load(std::memory_order_acquire)
               != requiredCaptureEpoch)
        return false;

    values = latestMatchingValues;
    return true;
}

std::uint64_t FireAudioProcessor::requestFreshModulatedFilterValuesEpoch() noexcept
{
    auto currentEpoch = filterTelemetryCaptureEpoch.load(
        std::memory_order_relaxed);
    for (;;)
    {
        auto nextEpoch = currentEpoch + 1u;
        if (nextEpoch == 0)
            nextEpoch = 1;

        if (filterTelemetryCaptureEpoch.compare_exchange_weak(
                currentEpoch,
                nextEpoch,
                std::memory_order_release,
                std::memory_order_relaxed))
            return nextEpoch;
    }
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

LfoManager::AssignmentResult FireAudioProcessor::assignLfoToTarget(
    int sourceLfoIndex,
    const juce::String& targetParameterID)
{
    const auto result =
        lfoManager->assignLfoToTarget(sourceLfoIndex, targetParameterID);
    if (result != LfoManager::AssignmentResult::changed)
        return result;

    lfoDataHasChanged();
    return result;
}

bool FireAudioProcessor::clearModulationForParameter(
    const juce::String& targetParameterID)
{
    if (! lfoManager->clearModulationForTarget(targetParameterID))
        return false;

    lfoDataHasChanged();
    return true;
}

bool FireAudioProcessor::invertModulationDepthForParameter(
    const juce::String& targetParameterID)
{
    if (! lfoManager->invertModulationDepth(targetParameterID))
        return false;

    lfoDataHasChanged();
    return true;
}

bool FireAudioProcessor::toggleModulationBypassForParameter(
    const juce::String& targetParameterID)
{
    if (! lfoManager->toggleBypassForRouting(targetParameterID))
        return false;

    lfoDataHasChanged();
    return true;
}

bool FireAudioProcessor::isCurrentStateEquivalentToPreset(const juce::XmlElement& incomingPresetXml)
{
    constexpr float comparisonTolerance = 1.0e-6f;
    if (! state::canLoadStateFromXml(incomingPresetXml, *this))
        return false;
    fire::effects::FrozenRecordings expectedAudio;
    if (!fire::effects::readFrozenAudioState(incomingPresetXml, expectedAudio)) return false;
    const auto currentAudio = captureSerializablePresetStateSnapshot().frozenAudio;
    for (size_t scope = 0; scope < expectedAudio.size(); ++scope)
        for (size_t slot = 0; slot < expectedAudio[scope].size(); ++slot)
        {
            const auto& expected = expectedAudio[scope][slot];
            const auto& current = currentAudio[scope][slot];
            if (static_cast<bool>(expected) != static_cast<bool>(current)) return false;
            if (expected && (expected->head != current->head || expected->validFrames != current->validFrames
                             || expected->samples != current->samples)) return false;
        }
    juce::XmlElement presetXml(incomingPresetXml);
    state::canonicaliseCloudsPresetState(presetXml);

    // Recreate the exact model produced by loadStateFromXml rather than
    // comparing XML text. Older presets omit attributes whose loader defaults
    // are well-defined, and decimal formatting is not part of the preset's
    // audible state.
    std::array<LfoData, fire::lfo_bank::capacity> expectedLfoData;
    std::array<bool, fire::lfo_bank::capacity> expectedSmoothnessFromLfoXml {};
    std::array<bool, fire::lfo_bank::capacity> expectedSmoothnessFromParameter {};
    std::array<bool, fire::lfo_bank::capacity> loadedLfoIndices {};
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
        if (fire::clouds_params::isReservedEngineParameterID(parameterWithID->paramID))
            continue; // Deprecated automation slots do not affect the sound.

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
        else if (fire::drive_comp::isParameterID(parameterWithID->paramID))
        {
            presetValue = 0.0f; // Missing in an older preset means Legacy Link.
        }
        else
        {
            // Before Smoothness became an APVTS parameter it lived only on the
            // LFO element. loadStateFromXml explicitly promotes that legacy
            // value to APVTS before replacing the shape, so compare against
            // the same effective normalised parameter value.
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

        // setValueNotifyingHost applies each parameter's legal step size.
        // Older presets can contain intermediate values (e.g. peak gain before
        // its 0.1 dB step), so compare the value the loader actually produces.
        if (const auto* ranged = treeState.getParameter(parameterWithID->paramID))
            presetValue = ranged->convertTo0to1(ranged->convertFrom0to1(presetValue));

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

    juce::Array<ModulationRouting> expectedRoutings;
    if (const auto* routingState = presetXml.getChildByName("MODULATION_STATE"))
    {
        for (auto* routingXml : routingState->getChildIterator())
        {
            if (! routingXml->hasTagName("ROUTING")
                || expectedRoutings.size()
                       >= LfoManager::maximumModulationRoutings)
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
void FireAudioProcessor::shiftLfoModulationTargets(int startIndex,
                                                   int endIndex,
                                                   int shiftAmount,
                                                   bool notifyHost)
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

        if (didUpdate)
            lfoManager->advanceModulationRoutingRevisionLocked();
    }

    if (didUpdate && notifyHost)
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
void FireAudioProcessor::clearLfoModulationForBand(int bandIndex,
                                                   bool notifyHost)
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
        if (didUpdate)
            lfoManager->advanceModulationRoutingRevisionLocked();
    }

    if (didUpdate && notifyHost)
        lfoDataHasChanged();
}

bool FireAudioProcessor::getLatestDistortionGraphValues(DistortionGraphValues& values)
{
    const int numAvailable = graphFifo.getNumReady();
    if (numAvailable <= 0)
        return false;

    const auto requestedSourceToken = distortionGraphSourceToken.load(
        std::memory_order_acquire);
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    graphFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

    DistortionGraphValues latestMatchingValues;
    bool foundMatchingPacket = false;
    const auto inspectRange = [&] (int start, int size)
    {
        for (int offset = 0; offset < size; ++offset)
        {
            const auto& candidate = graphFifoBuffer[static_cast<size_t>(start + offset)];
            if (candidate.sourceToken == requestedSourceToken)
            {
                latestMatchingValues = candidate;
                foundMatchingPacket = true;
            }
        }
    };

    inspectRange(start1, size1);
    inspectRange(start2, size2);
    // Stale packets are deliberately consumed too. Otherwise a hidden editor
    // or repeated focus changes could leave the FIFO permanently clogged.
    graphFifo.finishedRead(numAvailable);

    // Focus may have changed while the message thread was draining. Never
    // publish a value unless it still belongs to the current source epoch.
    if (! foundMatchingPacket
        || distortionGraphSourceToken.load(std::memory_order_acquire)
               != requestedSourceToken)
        return false;

    values = latestMatchingValues;
    return true;
}

void FireAudioProcessor::setUiFocusBand(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, 4))
        return;

    // Publish band and generation atomically. Advancing the generation on
    // every valid publication (including A -> B -> A and forced A -> A
    // refreshes) prevents a queued packet from an earlier visit to a band from
    // becoming current again.
    auto currentToken = distortionGraphSourceToken.load(std::memory_order_relaxed);
    for (;;)
    {
        auto nextGeneration = (currentToken & ~distortionGraphBandMask)
                              + (distortionGraphBandMask + 1u);
        if ((nextGeneration & ~distortionGraphBandMask) == 0)
            nextGeneration = distortionGraphBandMask + 1u;

        const auto nextToken = nextGeneration
                               | static_cast<std::uint64_t>(bandIndex);
        if (distortionGraphSourceToken.compare_exchange_weak(
                currentToken,
                nextToken,
                std::memory_order_release,
                std::memory_order_relaxed))
            break;
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

    const auto calculateChannelLevels = [&buffer, numSamples](int channel)
    {
        const auto rawPeak = buffer.getMagnitude(channel, 0, numSamples);
        const auto finitePeak = std::isfinite(rawPeak) && rawPeak >= 0.0f
                                    ? rawPeak
                                    : 0.0f;

        // JUCE's float RMS accumulator can overflow while squaring a very
        // large but still finite host sample. The peak remains a conservative
        // finite fallback and prevents one telemetry packet from poisoning the
        // UI's smoothing state.
        const auto rawRms = buffer.getRMSLevel(channel, 0, numSamples);
        const auto finiteRms = std::isfinite(rawRms) && rawRms >= 0.0f
                                   ? rawRms
                                   : finitePeak;
        return std::pair { finiteRms, finitePeak };
    };

    const auto [leftRms, leftPeak] = calculateChannelLevels(0);
    rmsLeft.store(leftRms);
    peakLeft.store(leftPeak);

    // Calculate for Right Channel if it exists, otherwise mirror the left channel.
    if (numChannels > 1)
    {
        const auto [rightRms, rightPeak] = calculateChannelLevels(1);
        rmsRight.store(rightRms);
        peakRight.store(rightPeak);
    }
    else
    {
        rmsRight.store(leftRms);
        peakRight.store(leftPeak);
    }
}
