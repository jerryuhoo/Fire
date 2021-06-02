/*
  ==============================================================================

    AudioHelpers.h
    Created: 11 Apr 2021 1:07:30pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

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
