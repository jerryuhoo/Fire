/*
  ==============================================================================

    WidthProcessor.h
    Created: 21 Dec 2020 6:05:29pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once
#include "juce_core/juce_core.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "ModulatedValueProvider.h"

class WidthProcessor
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* channeldataL, float* channeldataR, float width, float pan, int numSamples) noexcept;
    void process(float* channeldataL,
                 float* channeldataR,
                 const ModulatedValueProvider& width,
                 const ModulatedValueProvider& pan,
                 int numSamples) noexcept;
    void process(float* channeldataL,
                 float* channeldataR,
                 const ModulatedValueProvider& width,
                 const ModulatedValueProvider& pan,
                 int widthSourceIndex,
                 int panSourceIndex,
                 int numSamples) noexcept;

private:
    struct RouteRecipeSignature
    {
        bool routed = false;
        int sourceIndex = -1;
        float modulationDepth = 0.0f;
        bool isBipolar = true;
    };

    struct RouteTransitionState
    {
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
            transitionMix;
        RouteRecipeSignature lastRecipe;
        float anchorValue = 0.0f;
        float lastAppliedValue = 0.0f;
        bool initialised = false;

        void prepare(double sampleRate, float initialValue) noexcept;
        void reset(float initialValue) noexcept;
    };

    void processInternal(float* channeldataL,
                         float* channeldataR,
                         float width,
                         float pan,
                         const ModulatedValueProvider* widthProvider,
                         const ModulatedValueProvider* panProvider,
                         int widthSourceIndex,
                         int panSourceIndex,
                         int numSamples) noexcept;

    static RouteRecipeSignature makeRouteRecipe(
        const ModulatedValueProvider* provider,
        int sourceIndex) noexcept;
    static void serviceRouteTransition(
        RouteTransitionState& transition,
        const RouteRecipeSignature& recipe,
        float initialTarget) noexcept;
    static float applyRouteTransition(
        const RouteTransitionState& transition,
        float target) noexcept;

    // These smooth only the user-controlled base values. Routed LFO samples
    // remain sample-accurate and use the independent transition states below
    // only when the routing recipe itself changes.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoother { 0.5f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> panSmoother { 0.0f };
    RouteTransitionState widthRouteTransition;
    RouteTransitionState panRouteTransition;
    bool parametersPrimed = false;
};
