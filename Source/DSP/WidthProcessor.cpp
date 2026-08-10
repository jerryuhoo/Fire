/*
  ==============================================================================

    WidthProcessor.cpp
    Created: 21 Dec 2020 6:05:29pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "WidthProcessor.h"
#include <cmath>

void WidthProcessor::process(float* channeldataL, float* channeldataR, float width, float pan, int numSamples) const noexcept
{
    if (channeldataL == nullptr || channeldataR == nullptr || numSamples <= 0)
        return;

    const float safeWidth = std::isfinite(width) ? juce::jlimit(0.0f, 1.0f, width) : 0.5f;
    const float safePan = std::isfinite(pan) ? juce::jlimit(-1.0f, 1.0f, pan) : 0.0f;
    constexpr float inverseSqrtTwo = 0.7071067811865475244f;

    const float midGain = 2.0f * (1.0f - safeWidth);
    const float sideGain = 2.0f * safeWidth;
    const float panLeftGain = safePan > 0.0f ? 1.0f - safePan : 1.0f;
    const float panRightGain = safePan < 0.0f ? 1.0f + safePan : 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
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
