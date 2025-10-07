/*
  ==============================================================================

    LfoEngine.cpp
    Created: 14 Sep 2025 11:10:19pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "LfoEngine.h"

LfoEngine::LfoEngine()
{
    // The size of the lookup table (1024) is a good balance between precision and memory usage.
    wavetable.initialise([](float x)
                         { return 0.0f; },
                         1024); // Initialize with silence
}

void LfoEngine::reset()
{
    phase = 0.0f;
}

void LfoEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
}

void LfoEngine::updateShape(const LfoData& shapeData)
{
    const auto& points = shapeData.points;
    const auto& curvatures = shapeData.curvatures;
    const auto numPointsInTable = wavetable.getNumPoints();

    // Step 1: Create a temporary, mutable array to build the waveform.
    juce::Array<float> tempTable;
    tempTable.resize(numPointsInTable);

    // Step 2: Generate the raw, unsmoothed shape into the temporary array.
    for (int i = 0; i < numPointsInTable; ++i)
    {
        const float phase = (float) i / (float) (numPointsInTable > 1 ? numPointsInTable - 1 : 1);

        float sampleValue = 0.5f; // Default to middle value

        if (points.size() >= 2)
        {
            // Find the correct segment for the current phase
            for (size_t p = 0; p < points.size() - 1; ++p)
            {
                const auto& p1 = points[p];
                const auto& p2 = points[p + 1];

                if (phase >= p1.x && phase <= p2.x)
                {
                    const float segmentWidth = p2.x - p1.x;

                    // Fallback to linear interpolation if curvatures data is missing or segment is zero-width
                    if (p >= curvatures.size() || std::abs(segmentWidth) < 1e-9f)
                    {
                        sampleValue = (std::abs(segmentWidth) < 1e-9f)
                                          ? p1.y
                                          : p1.y + (p2.y - p1.y) * ((phase - p1.x) / segmentWidth);
                    }
                    else // Apply curvature
                    {
                        const float curvature = curvatures[p];
                        const float tx = (phase - p1.x) / segmentWidth;
                        const float absExp = std::pow(4.0f, std::abs(curvature));
                        float ty;

                        if (curvature >= 0.0f)
                            ty = std::pow(tx, absExp);
                        else
                            ty = 1.0f - std::pow(juce::jmax(0.0f, 1.0f - tx), absExp);

                        sampleValue = p1.y + (p2.y - p1.y) * ty;
                    }
                    break; // Exit segment search once found
                }
            }
            // If phase is somehow outside all segments, hold the last point's value
            if (i == numPointsInTable - 1)
                sampleValue = points.back().y;
        }
        tempTable.set(i, sampleValue);
    }

    // Step 3: Apply smoothing to the temporary array if required.
    const float smoothness = shapeData.smoothness;
    if (smoothness > 0.001f && tempTable.size() > 0)
    {
        // Map smoothness (0-1) to a filter coefficient.
        const float feedbackCoeff = smoothness * 0.95f;
        const float feedforwardCoeff = 1.0f - feedbackCoeff;

        // Apply a one-pole low-pass filter across the temporary table.
        // Run it twice to better handle the wrap-around continuity.
        for (int pass = 0; pass < 2; ++pass)
        {
            float lastOutput = tempTable.getLast();
            for (int i = 0; i < tempTable.size(); ++i)
            {
                const float currentInput = tempTable.getUnchecked(i);
                const float smoothedSample = (currentInput * feedforwardCoeff) + (lastOutput * feedbackCoeff);
                tempTable.set(i, smoothedSample);
                lastOutput = smoothedSample;
            }
        }
    }

    // Step 4: Finally, initialize the actual wavetable from the processed temporary table.
    wavetable.initialise([&tempTable](size_t i)
                         { return tempTable.getUnchecked(i); },
                         tempTable.size());
}

// Call this on every sample in processBlock. Returns a bipolar [-1, 1] signal.
float LfoEngine::process()
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
void LfoEngine::setPhaseDelta(float newPhaseDelta)
{
    phaseDelta = newPhaseDelta;
}

float LfoEngine::getPhase() const
{
    return phase;
}

float LfoEngine::getLastOutput() const
{
    return lastOutput;
}