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

static inline float dBToNormalizedGain(float inValue)
{
    float inValuedB = juce::Decibels::gainToDecibels(inValue + 0.00001f);
    inValuedB = (inValuedB + 96.0f) / 96.0f;
    return inValuedB;
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