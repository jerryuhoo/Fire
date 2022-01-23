/*
  ==============================================================================

    ClippingFunctions.h
    Created: 9 Sep 2021 9:21:12am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <math.h>
namespace waveshaping
{

using JMath = juce::dsp::FastMathApproximations;

template<typename T>
T arctanSoftClipping (T x) noexcept
{
    return atan(x) / 2.0f;
}

template<typename T>
T expSoftClipping (T x) noexcept
{
    if (x > 0)
    {
        x = 1.0f - expf(-x);
    }
    else
    {
        x = -1.0f + expf(x);
    }
    return x;
}

template<typename T>
T tanhSoftClipping (T x) noexcept
{
    return tanh(x);
}

template<typename T>
T cubicSoftClipping (T x) noexcept
{
    if (x > 1.0f)
    {
        x = 1.0f * 2.0f / 3.0f;
    }
    else if (x < -1.0f)
    {
        x = -1.0f * 2.0f / 3.0f;
    }
    else
    {
        x = x - (pow(x, 3.0f) / 3.0f);
    }
    return x * 3.0f / 2.0f;
}

template<typename T>
T hardClipping (T x) noexcept
{
    if (x > 1.0f)
    {
        x = 1.0f;
    }
    else if (x < -1.0f)
    {
        x = -1.0f;
    }
    return x;
}

template<typename T>
T sausageFattener (T x) noexcept
{
    x = x * 1.1f;
    if (x >= 1.1f)
    {
        x = 1.0f;
    }
    else if (x <= -1.1f)
    {
        x = -1.0f;
    }
    else if (x > 0.9f && x < 1.1f)
    {
        x = -2.5f * x * x + 5.5f * x - 2.025f;
    }
    else if (x < -0.9f && x > -1.1f)
    {
        x = 2.5f * x * x + 5.5f * x + 2.025f;
    }
    return x;
}

template<typename T>
T sinFoldback (T x) noexcept
{
    return std::sin(x);
}

template<typename T>
T linFoldback (T x) noexcept
{
    if (x > 1.0f || x < -1.0f)
    {
        x = fabs(fabs(fmod(x - 1.0f, 1.0f * 4)) - 1.0f * 2) - 1.0f;
    }
    return x;
}

template<typename T>
T limitClip (T x) noexcept
{
    return juce::jlimit (-0.1f, 0.1f, x);
}

template<typename T>
T singleSinClip (T x) noexcept
{
    if (std::fabs (x) < juce::MathConstants<T>::pi)
    {
        return JMath::sin (x);
    }
    else
    {
        return 0;//signbit (x) * 1.0f;
    }
}

template<typename T>
T logicClip (T x) noexcept
{
    return 2.0f / (1.0f + JMath::exp (-2.0f * x)) - 1.0f;
}

template<typename T>
T tanclip (T x) noexcept
{
    float soft = 0.0f;
    return juce::jlimit (-1.0f, 1.0f, static_cast<float>(JMath::tanh ((1.0f - 0.5f * soft) * x) - 0.02 * x));
}

//
//template<typename T>
//T algClip (T x) noexcept
//{
//    float soft = 0.0f;
//    return x / std::sqrtf ((1.0f + 2.0f * soft + std::powf (x, 2.0f)));
//}

//template<typename T>
//T arcClip (T x) noexcept
//{
//    float soft = 0.0f;
//    return (2.0f / juce::MathConstants<T>::pi) * std::atanf ((1.6f - soft * 0.6f) * x);
//}



template<typename T>
T rectificationProcess (T x, T rectification) noexcept
{
    if (x < 0)
    {
        x *= (0.5f - rectification) * 2.0f;
    }
    return x;
}

}
