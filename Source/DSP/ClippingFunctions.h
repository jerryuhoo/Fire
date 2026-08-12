/*
  ==============================================================================

    ClippingFunctions.h
    Created: 9 Sep 2021 9:21:12am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
namespace waveshaping
{

using JMath = juce::dsp::FastMathApproximations;

template<typename T>
T arctanSoftClipping (T x) noexcept
{
    if (std::isnan(x))
        return T {};
    return std::atan(x) / static_cast<T>(2);
}

template<typename T>
T expSoftClipping (T x) noexcept
{
    if (std::isnan(x))
        return T {};

    if (x > 0)
    {
        x = static_cast<T>(1) - std::exp(-x);
    }
    else
    {
        x = static_cast<T>(-1) + std::exp(x);
    }
    return x;
}

template<typename T>
T tanhSoftClipping (T x) noexcept
{
    return std::isnan(x) ? T {} : std::tanh(x);
}

template<typename T>
T cubicSoftClipping (T x) noexcept
{
    if (std::isnan(x))
        return T {};

    if (x > static_cast<T>(1))
    {
        x = static_cast<T>(2) / static_cast<T>(3);
    }
    else if (x < static_cast<T>(-1))
    {
        x = static_cast<T>(-2) / static_cast<T>(3);
    }
    else
    {
        // Keep this hot waveshaper path efficient without relying on
        // fast-math, whose non-finite assumptions invalidate the guards used
        // throughout the DSP. This multiplication is the same optimisation
        // that fast-math previously applied to pow(x, 3).
        x = x - (x * x * x / static_cast<T>(3));
    }
    return x * static_cast<T>(3) / static_cast<T>(2);
}

template<typename T>
T hardClipping (T x) noexcept
{
    if (std::isnan(x))
        return T {};

    if (x > static_cast<T>(1))
    {
        x = static_cast<T>(1);
    }
    else if (x < static_cast<T>(-1))
    {
        x = static_cast<T>(-1);
    }
    return x;
}

template<typename T>
T sausageFattener (T x) noexcept
{
    if (std::isnan(x))
        return T {};

    x = x * static_cast<T>(1.1);
    if (x >= static_cast<T>(1.1))
    {
        x = static_cast<T>(1);
    }
    else if (x <= static_cast<T>(-1.1))
    {
        x = static_cast<T>(-1);
    }
    else if (x > static_cast<T>(0.9) && x < static_cast<T>(1.1))
    {
        x = static_cast<T>(-2.5) * x * x + static_cast<T>(5.5) * x - static_cast<T>(2.025);
    }
    else if (x < static_cast<T>(-0.9) && x > static_cast<T>(-1.1))
    {
        x = static_cast<T>(2.5) * x * x + static_cast<T>(5.5) * x + static_cast<T>(2.025);
    }
    return x;
}

template<typename T>
T sinFoldback (T x) noexcept
{
    return std::isfinite(x) ? std::sin(x) : T {};
}

template<typename T>
T linFoldback (T x) noexcept
{
    if (! std::isfinite(x))
        return T {};

    if (x > static_cast<T>(1) || x < static_cast<T>(-1))
    {
        x = std::abs(std::abs(std::fmod(x - static_cast<T>(1), static_cast<T>(4)))
                     - static_cast<T>(2))
            - static_cast<T>(1);
    }
    return x;
}

template<typename T>
T limitClip (T x) noexcept
{
    return std::isnan(x) ? T {} : juce::jlimit(static_cast<T>(-0.1), static_cast<T>(0.1), x);
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
    if (std::isnan(x))
        return T {};

    // 2 / (1 + exp(-2x)) - 1 is exactly tanh(x). The former implementation
    // used JUCE's small-range Padé exp approximation with unbounded drive
    // values, making the curve fold back towards zero at large magnitudes.
    return std::tanh(x);
}

template<typename T>
T tanclip (T x) noexcept
{
    if (! std::isfinite(x))
        return T {};

    constexpr T soft {};
    // JUCE's Padé tanh approximation is only accurate over a small input
    // interval. Keep its input inside that interval while preserving the
    // unbounded linear term that gives this waveshaper its intended high-level
    // foldback. Passing Extreme-drive samples directly to the approximation
    // can otherwise reverse the curve and even produce the wrong polarity.
    const T nonlinearInput = juce::jlimit(static_cast<T>(-5), static_cast<T>(5),
                                          (static_cast<T>(1) - static_cast<T>(0.5) * soft) * x);
    const T result = JMath::tanh(nonlinearInput)
                     - static_cast<T>(0.02) * x;
    return std::isfinite(result)
               ? juce::jlimit(static_cast<T>(-1), static_cast<T>(1), result)
               : T {};
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
    if (! std::isfinite(x))
        return T {};
    if (! std::isfinite(rectification))
        rectification = T {};

    if (x < 0)
    {
        x *= (static_cast<T>(0.5) - rectification) * static_cast<T>(2);
    }
    return x;
}

}
