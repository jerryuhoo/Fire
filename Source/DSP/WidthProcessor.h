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

private:
    void processInternal(float* channeldataL,
                         float* channeldataR,
                         float width,
                         float pan,
                         const ModulatedValueProvider* widthProvider,
                         const ModulatedValueProvider* panProvider,
                         int numSamples) noexcept;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoother { 0.5f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> panSmoother { 0.0f };
    bool parametersPrimed = false;
};
