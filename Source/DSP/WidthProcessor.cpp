/*
  ==============================================================================

    WidthProcessor.cpp
    Created: 21 Dec 2020 6:05:29pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "WidthProcessor.h"
#include <cmath>

void WidthProcessor::RouteTransitionState::prepare(double sampleRate,
                                                   float initialValue) noexcept
{
    transitionMix.reset(sampleRate, 0.01);
    reset(initialValue);
}

void WidthProcessor::RouteTransitionState::reset(float initialValue) noexcept
{
    transitionMix.setCurrentAndTargetValue(1.0f);
    lastRecipe = {};
    anchorValue = initialValue;
    lastAppliedValue = initialValue;
    initialised = false;
}

WidthProcessor::RouteRecipeSignature WidthProcessor::makeRouteRecipe(
    const ModulatedValueProvider* provider,
    int sourceIndex) noexcept
{
    RouteRecipeSignature recipe;
    if (provider == nullptr || provider->lfoSignal == nullptr)
        return recipe;

    recipe.routed = true;
    recipe.sourceIndex = sourceIndex >= 0 ? sourceIndex : 0;
    recipe.modulationDepth = std::isfinite(provider->modulationDepth)
                                 ? juce::jlimit(-1.0f,
                                                1.0f,
                                                provider->modulationDepth)
                                 : 0.0f;
    recipe.isBipolar = provider->isBipolar;
    return recipe;
}

void WidthProcessor::serviceRouteTransition(
    RouteTransitionState& transition,
    const RouteRecipeSignature& recipe,
    float initialTarget) noexcept
{
    const auto recipesMatch = [](const RouteRecipeSignature& first,
                                 const RouteRecipeSignature& second)
    {
        return first.routed == second.routed
               && first.sourceIndex == second.sourceIndex
               && juce::exactlyEqual(first.modulationDepth,
                                     second.modulationDepth)
               && first.isBipolar == second.isBipolar;
    };

    if (! transition.initialised)
    {
        transition.lastRecipe = recipe;
        transition.anchorValue = initialTarget;
        transition.lastAppliedValue = initialTarget;
        transition.transitionMix.setCurrentAndTargetValue(1.0f);
        transition.initialised = true;
        return;
    }

    if (recipesMatch(transition.lastRecipe, recipe))
        return;

    transition.lastRecipe = recipe;
    transition.anchorValue = transition.lastAppliedValue;
    transition.transitionMix.setCurrentAndTargetValue(0.0f);
    transition.transitionMix.setTargetValue(1.0f);
}

float WidthProcessor::applyRouteTransition(
    const RouteTransitionState& transition,
    float target) noexcept
{
    const float mix = transition.transitionMix.getCurrentValue();
    if (mix <= 0.0f)
        return transition.anchorValue;
    if (mix >= 1.0f)
        return target;

    return transition.anchorValue
           + mix * (target - transition.anchorValue);
}

void WidthProcessor::prepare(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 44100.0;
    widthSmoother.reset(safeSampleRate, 0.01);
    panSmoother.reset(safeSampleRate, 0.01);
    widthRouteTransition.prepare(safeSampleRate, 0.5f);
    panRouteTransition.prepare(safeSampleRate, 0.0f);
    reset();
}

void WidthProcessor::reset() noexcept
{
    widthSmoother.setCurrentAndTargetValue(0.5f);
    panSmoother.setCurrentAndTargetValue(0.0f);
    widthRouteTransition.reset(0.5f);
    panRouteTransition.reset(0.0f);
    parametersPrimed = false;
}

void WidthProcessor::process(float* channeldataL, float* channeldataR, float width, float pan, int numSamples) noexcept
{
    processInternal(channeldataL,
                    channeldataR,
                    width,
                    pan,
                    nullptr,
                    nullptr,
                    -1,
                    -1,
                    numSamples);
}

void WidthProcessor::process(float* channeldataL,
                             float* channeldataR,
                             const ModulatedValueProvider& width,
                             const ModulatedValueProvider& pan,
                             int numSamples) noexcept
{
    process(channeldataL,
            channeldataR,
            width,
            pan,
            width.lfoSignal != nullptr ? 0 : -1,
            pan.lfoSignal != nullptr ? 0 : -1,
            numSamples);
}

void WidthProcessor::process(float* channeldataL,
                             float* channeldataR,
                             const ModulatedValueProvider& width,
                             const ModulatedValueProvider& pan,
                             int widthSourceIndex,
                             int panSourceIndex,
                             int numSamples) noexcept
{
    processInternal(channeldataL,
                    channeldataR,
                    width.baseValue,
                    pan.baseValue,
                    width.lfoSignal != nullptr ? &width : nullptr,
                    pan.lfoSignal != nullptr ? &pan : nullptr,
                    widthSourceIndex,
                    panSourceIndex,
                    numSamples);
}

void WidthProcessor::processInternal(float* channeldataL,
                                     float* channeldataR,
                                     float width,
                                     float pan,
                                     const ModulatedValueProvider* widthProvider,
                                     const ModulatedValueProvider* panProvider,
                                     int widthSourceIndex,
                                     int panSourceIndex,
                                     int numSamples) noexcept
{
    if (channeldataL == nullptr || channeldataR == nullptr || numSamples <= 0)
        return;

    const auto getSafeWidth = [](float value)
    {
        return std::isfinite(value) ? juce::jlimit(0.0f, 1.0f, value) : 0.5f;
    };
    const auto getSafePan = [](float value)
    {
        return std::isfinite(value) ? juce::jlimit(-1.0f, 1.0f, value) : 0.0f;
    };

    const float widthBaseTarget = getSafeWidth(
        widthProvider != nullptr ? widthProvider->baseValue : width);
    const float panBaseTarget = getSafePan(
        panProvider != nullptr ? panProvider->baseValue : pan);
    constexpr float inverseSqrtTwo = 0.7071067811865475244f;

    if (! parametersPrimed)
    {
        // The first block must preserve the old immediate-start behaviour.
        // Subsequent changes are ramped to avoid block-boundary zipper noise.
        widthSmoother.setCurrentAndTargetValue(widthBaseTarget);
        panSmoother.setCurrentAndTargetValue(panBaseTarget);
        parametersPrimed = true;
    }
    else
    {
        widthSmoother.setTargetValue(widthBaseTarget);
        panSmoother.setTargetValue(panBaseTarget);
    }

    auto widthRecipeProvider = widthProvider != nullptr
                                   ? *widthProvider
                                   : ModulatedValueProvider {};
    auto panRecipeProvider = panProvider != nullptr
                                 ? *panProvider
                                 : ModulatedValueProvider {};
    const auto getWidthTarget = [&](int sample, float baseValue)
    {
        if (widthProvider == nullptr)
            return getSafeWidth(baseValue);

        widthRecipeProvider.baseValue = baseValue;
        return getSafeWidth(widthRecipeProvider.get(sample));
    };
    const auto getPanTarget = [&](int sample, float baseValue)
    {
        if (panProvider == nullptr)
            return getSafePan(baseValue);

        panRecipeProvider.baseValue = baseValue;
        return getSafePan(panRecipeProvider.get(sample));
    };

    serviceRouteTransition(
        widthRouteTransition,
        makeRouteRecipe(widthProvider, widthSourceIndex),
        getWidthTarget(0, widthSmoother.getCurrentValue()));
    serviceRouteTransition(
        panRouteTransition,
        makeRouteRecipe(panProvider, panSourceIndex),
        getPanTarget(0, panSmoother.getCurrentValue()));

    for (int i = 0; i < numSamples; ++i)
    {
        const float currentWidthBase = widthSmoother.getNextValue();
        const float currentPanBase = panSmoother.getNextValue();
        const float widthTarget = getWidthTarget(i, currentWidthBase);
        const float panTarget = getPanTarget(i, currentPanBase);
        const float currentWidth = applyRouteTransition(widthRouteTransition,
                                                        widthTarget);
        const float currentPan = applyRouteTransition(panRouteTransition,
                                                      panTarget);
        const float midGain = 2.0f * (1.0f - currentWidth);
        const float sideGain = 2.0f * currentWidth;
        const float panLeftGain = currentPan > 0.0f ? 1.0f - currentPan : 1.0f;
        const float panRightGain = currentPan < 0.0f ? 1.0f + currentPan : 1.0f;

        const float leftInput = std::isfinite(channeldataL[i]) ? channeldataL[i] : 0.0f;
        const float rightInput = std::isfinite(channeldataR[i]) ? channeldataR[i] : 0.0f;
        float mid = (leftInput + rightInput) * inverseSqrtTwo;
        float sides = (leftInput - rightInput) * inverseSqrtTwo;

        mid *= midGain;
        sides *= sideGain;

        channeldataL[i] = (mid + sides) * inverseSqrtTwo * panLeftGain;
        channeldataR[i] = (mid - sides) * inverseSqrtTwo * panRightGain;

        widthRouteTransition.lastAppliedValue = currentWidth;
        panRouteTransition.lastAppliedValue = currentPan;
        widthRouteTransition.transitionMix.getNextValue();
        panRouteTransition.transitionMix.getNextValue();
    }
}
