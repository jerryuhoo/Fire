/*
  ==============================================================================

    DistortionLogic.h
    Created: 6 Sep 2025 6:46:18am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once
#include "ClippingFunctions.h"
#include <functional>

namespace DistortionLogic
{
    // A simple struct to hold the state for our processing function.
    // This makes the function signature clean and easy to manage.
    struct State
    {
        float drive = 1.0f;
        float bias = 0.0f;
        float rec = 0.0f;
        int mode = 0;
    };

    // Helper function to get the correct waveshaper based on the mode.
    // This is moved directly from your BandProcessor.
    inline std::function<float(float)> getWaveshaperForMode(int mode)
    {
        switch (mode)
        {
            case 0:
                return waveshaping::arctanSoftClipping<float>;
            case 1:
                return waveshaping::expSoftClipping<float>;
            case 2:
                return waveshaping::tanhSoftClipping<float>;
            case 3:
                return waveshaping::cubicSoftClipping<float>;
            case 4:
                return waveshaping::hardClipping<float>;
            case 5:
                return waveshaping::sausageFattener<float>;
            case 6:
                return waveshaping::sinFoldback<float>;
            case 7:
                return waveshaping::linFoldback<float>;
            case 8:
                return waveshaping::limitClip<float>;
            case 9:
                return waveshaping::singleSinClip<float>;
            case 10:
                return waveshaping::logicClip<float>;
            case 11:
                return waveshaping::tanclip<float>;
            default:
                return waveshaping::cubicSoftClipping<float>; // Fallback
        }
    }

    // This is the core, shared processing function for a single sample.
    // It takes an input sample and the current state, and returns the processed (wet) sample.
    inline float processSample(float inputSample, const State& state)
    {
        auto waveshaperFunction = getWaveshaperForMode(state.mode);

        // This is the exact processing chain from your BandProcessor, now in one place.
        float currentSample = inputSample;
        currentSample *= state.drive; // 1. Drive Gain
        currentSample += state.bias; // 2. Pre-Bias
        currentSample = waveshaperFunction(currentSample); // 3. Waveshaper
        if (currentSample < 0.0f)
            currentSample *= (0.5f - state.rec) * 2.0f; // 4. Rectifier
        currentSample -= state.bias; // 5. Post-Bias

        return currentSample;
    }
} // namespace DistortionLogic
