/*
  ==============================================================================

    LfoEngine.h
    Created: 4 Aug 2025 6:15:46pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

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
                                 return std::sin((double)i / numPoints * juce::MathConstants<double>::twoPi);
                             }, 1024);
    }

    // Call this in your processor's prepareToPlay method.
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
    }

    // Call this from the message thread whenever the LFO shape changes in the UI.
    void updateShape(const std::vector<juce::Point<float>>& points, const std::vector<float>& curvatures)
    {
        auto pointsCopy = points;
        auto curvaturesCopy = curvatures;
        const auto numPoints = wavetable.getNumPoints();

        // Add 'numPoints' to the lambda's capture list so it can be used inside.
        wavetable.initialise([pointsCopy, curvaturesCopy, numPoints](size_t i) -> float
        {
            const float phase = (float)i / (float)numPoints; // Now this is valid
            
            int pointIndex1 = -1, pointIndex2 = -1;
            if (!pointsCopy.empty())
            {
                for (int p = 0; p < (int)pointsCopy.size() - 1; ++p)
                {
                    if (phase >= pointsCopy[p].x && phase <= pointsCopy[p + 1].x)
                    {
                        pointIndex1 = p;
                        pointIndex2 = p + 1;
                        break;
                    }
                }
            }

            if (pointIndex1 != -1 && pointIndex1 < curvaturesCopy.size())
            {
                const auto p1 = pointsCopy[pointIndex1];
                const auto p2 = pointsCopy[pointIndex2];
                const float curvature = curvaturesCopy[pointIndex1];
                
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

            return 0.0f; // Fallback value
        },
        numPoints);
    }

    // Sets the LFO frequency in Hertz. Call this when the "Rate" parameter changes.
    void setFrequency(float frequency)
    {
        if (sampleRate > 0)
            phaseDelta = frequency / (float)sampleRate;
    }

    // Call this for every single audio sample in your processBlock.
    float getNextSample()
    {
        // The index for getUnchecked should be between 0 and getNumPoints().
        // We calculate this from our normalized phase [0, 1].
        const float tableIndex = phase * wavetable.getNumPoints();
        const float output = wavetable.getUnchecked(tableIndex);
        
        // Advance the phase for the next sample.
        phase += phaseDelta;
        if (phase >= 1.0f)
            phase -= 1.0f;
            
        return output;
    }
    
private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
    float phaseDelta = 0.0f;

    juce::dsp::LookupTable<float> wavetable;
};
