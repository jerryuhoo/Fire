/*
  ==============================================================================

    WidthProcessor.cpp
    Created: 21 Dec 2020 6:05:29pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "WidthProcessor.h"
#include <cmath>

void WidthProcessor::prepare(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 44100.0;
    widthSmoother.reset(safeSampleRate, 0.01);
    panSmoother.reset(safeSampleRate, 0.01);
    reset();
}

void WidthProcessor::reset() noexcept
{
    widthSmoother.setCurrentAndTargetValue(0.5f);
    panSmoother.setCurrentAndTargetValue(0.0f);
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
                    numSamples);
}

void WidthProcessor::process(float* channeldataL,
                             float* channeldataR,
                             const ModulatedValueProvider& width,
                             const ModulatedValueProvider& pan,
                             int numSamples) noexcept
{
    processInternal(channeldataL,
                    channeldataR,
                    width.baseValue,
                    pan.baseValue,
                    width.lfoSignal != nullptr ? &width : nullptr,
                    pan.lfoSignal != nullptr ? &pan : nullptr,
                    numSamples);
}

void WidthProcessor::processInternal(float* channeldataL,
                                     float* channeldataR,
                                     float width,
                                     float pan,
                                     const ModulatedValueProvider* widthProvider,
                                     const ModulatedValueProvider* panProvider,
                                     int numSamples) noexcept
{
    if (channeldataL == nullptr || channeldataR == nullptr || numSamples <= 0)
        return;

    const auto getSafeWidth = [widthProvider, width](int sample)
    {
        const float value = widthProvider != nullptr ? widthProvider->get(sample) : width;
        return std::isfinite(value) ? juce::jlimit(0.0f, 1.0f, value) : 0.5f;
    };
    const auto getSafePan = [panProvider, pan](int sample)
    {
        const float value = panProvider != nullptr ? panProvider->get(sample) : pan;
        return std::isfinite(value) ? juce::jlimit(-1.0f, 1.0f, value) : 0.0f;
    };
    const float initialWidth = getSafeWidth(0);
    const float initialPan = getSafePan(0);
    constexpr float inverseSqrtTwo = 0.7071067811865475244f;

    if (! parametersPrimed)
    {
        // The first block must preserve the old immediate-start behaviour.
        // Subsequent changes are ramped to avoid block-boundary zipper noise.
        widthSmoother.setCurrentAndTargetValue(initialWidth);
        panSmoother.setCurrentAndTargetValue(initialPan);
        parametersPrimed = true;
    }
    else
    {
        widthSmoother.setTargetValue(initialWidth);
        panSmoother.setTargetValue(initialPan);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        if (i > 0 && widthProvider != nullptr)
            widthSmoother.setTargetValue(getSafeWidth(i));
        if (i > 0 && panProvider != nullptr)
            panSmoother.setTargetValue(getSafePan(i));

        const float currentWidth = widthSmoother.getNextValue();
        const float currentPan = panSmoother.getNextValue();
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
    }
}
