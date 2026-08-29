/*
  ==============================================================================

    AudioHelpers.h
    Created: 11 Apr 2021 1:07:30pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once
#include "../GUI/InterfaceDefines.h"
#include "juce_dsp/juce_dsp.h"

#include <cmath>

static inline float dBToNormalizedGain(float inValue)
{
    if (! std::isfinite(inValue) || inValue <= 0.0f)
        return 0.0f;

    constexpr float meterFloorDb = -96.0f;
    const auto valueDb = juce::Decibels::gainToDecibels(inValue,
                                                        meterFloorDb);
    if (! std::isfinite(valueDb))
        return 0.0f;

    return juce::jlimit(0.0f,
                        1.0f,
                        (valueDb - meterFloorDb) / -meterFloorDb);
}

inline float helper_denormalize(float inValue)
{
    float absValue = fabs(inValue);
    if (absValue < 1e-15)
    {
        return 0.0f;
    }
    else
    {
        return inValue;
    }
}

static inline float transformToLog(double valueToTransform)
{
    auto value = juce::mapFromLog10(valueToTransform, 20.0, 20000.0);
    return static_cast<float>(value);
}

static inline float transformFromLog(double between0and1)
{
    auto value = juce::mapToLog10(between0and1, 20.0, 20000.0);
    return static_cast<float>(value);
}
