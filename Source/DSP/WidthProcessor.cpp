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
    if (channeldataL == nullptr || channeldataR == nullptr || numSamples <= 0)
        return;

    const float safeWidth = std::isfinite(width) ? juce::jlimit(0.0f, 1.0f, width) : 0.5f;
    const float safePan = std::isfinite(pan) ? juce::jlimit(-1.0f, 1.0f, pan) : 0.0f;
    constexpr float inverseSqrtTwo = 0.7071067811865475244f;

    if (! parametersPrimed)
    {
        // The first block must preserve the old immediate-start behaviour.
        // Subsequent changes are ramped to avoid block-boundary zipper noise.
        widthSmoother.setCurrentAndTargetValue(safeWidth);
        panSmoother.setCurrentAndTargetValue(safePan);
        parametersPrimed = true;
    }
    else
    {
        widthSmoother.setTargetValue(safeWidth);
        panSmoother.setTargetValue(safePan);
    }

    for (int i = 0; i < numSamples; ++i)
    {
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
