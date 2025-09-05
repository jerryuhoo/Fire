/*
  ==============================================================================

    LfoEngine.h
    Created: 4 Aug 2025 6:15:46pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "LfoData.h"
#include "juce_dsp/juce_dsp.h"
#include <vector>

//==============================================================================
/**
    LfoEngine
 
    This class is responsible for generating an LFO signal based on a user-defined shape.
    It runs on the audio thread and is optimized for real-time performance.
*/
class LfoEngine
{
public:
    LfoEngine()
    {
        // Initialize the lookup table with a default sine wave.
        // The table size (1024) is a good balance between precision and memory usage.
        wavetable.initialise([](size_t i)
                             {
                                 const auto numPoints = 1024.0;
                                 return std::sin((double)i / numPoints * juce::MathConstants<double>::twoPi); },
                             1024);
    }

    void reset()
    {
        phase = 0.0f;
    }

    // Call this in your processor's prepareToPlay method.
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
    }

    // Call this from the message thread whenever the LFO shape changes in the UI.
    // This pre-calculates the LFO shape into a high-performance wavetable.
    void updateShape(const LfoData& shapeData)
    {
        const auto& points = shapeData.points;
        const auto& curvatures = shapeData.curvatures;

        // Make copies to safely capture in the lambda for the audio thread.
        auto pointsCopy = points;
        auto curvaturesCopy = curvatures;
        const auto numPointsInTable = wavetable.getNumPoints();

        wavetable.initialise([pointsCopy, curvaturesCopy, numPointsInTable](size_t i) -> float
                             {
            const float phase = (float)i / (float)(numPointsInTable > 1 ? numPointsInTable - 1 : 1);
            
            if (pointsCopy.size() < 2)
                return 0.5f; // Return middle value if shape is invalid

            for (size_t p = 0; p < pointsCopy.size() - 1; ++p)
            {
                const auto& p1 = pointsCopy[p];
                const auto& p2 = pointsCopy[p + 1];

                if (phase >= p1.x && phase <= p2.x)
                {
                    // Fallback to linear interpolation if curvatures data is missing
                    if (p >= curvaturesCopy.size())
                        return p1.y + (p2.y - p1.y) * ((phase - p1.x) / (p2.x - p1.x));

                    const float curvature = curvaturesCopy[p];
                    
                    const float segmentWidth = p2.x - p1.x;
                    const float tx = (segmentWidth > 0.0f) ? (phase - p1.x) / segmentWidth : 0.0f;
                    
                    const float absExp = std::pow(4.0f, std::abs(curvature));
                    float ty;

                    if (curvature >= 0.0f)
                        ty = std::pow(tx, absExp);
                    else
                        ty = 1.0f - std::pow(juce::jmax(0.0f, 1.0f - tx), absExp);
                    
                    return p1.y + (p2.y - p1.y) * ty;
                }
            }
            return pointsCopy.back().y; },
                             numPointsInTable);
    }

    // Call this on every sample in processBlock. Returns a bipolar [-1, 1] signal.
    float process()
    {
        // Get the unipolar [0, 1] value from the pre-calculated wavetable
        const float unipolarOutput = wavetable.getUnchecked(phase * (wavetable.getNumPoints() - 1));

        // Advance the phase using the externally calculated delta
        phase += phaseDelta;

        if (phase >= 1.0f)
            phase -= 1.0f;

        // Convert the output to bipolar [-1, 1] for modulation
        lastOutput = unipolarOutput;
        return unipolarOutput;
    }

    // Call this from your processor before each block to set the LFO speed.
    // The processor is responsible for calculating the correct delta for Hz or BPM sync.
    void setPhaseDelta(float newPhaseDelta)
    {
        phaseDelta = newPhaseDelta;
    }

    float getPhase() const
    {
        return phase;
    }

    float getLastOutput() const
    {
        return lastOutput;
    }

private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
    float phaseDelta = 0.0f;
    float lastOutput = 0.0f;

    juce::dsp::LookupTable<float> wavetable;
};
