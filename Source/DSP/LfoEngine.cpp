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
    setWaveform(LfoWaveform::Sine); // Default to Sine wave on startup
}

void LfoEngine::reset()
{
    phase = 0.0f;
}

void LfoEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
}

void LfoEngine::setWaveform(LfoWaveform waveformType)
{
    currentWaveform = waveformType;
    const auto numPoints = wavetable.getNumPoints();

    // A lambda function to pass to wavetable.initialise
    std::function<float(size_t)> wave_function;

    switch (currentWaveform)
    {
        case LfoWaveform::Sine:
            wave_function = [numPoints](size_t i)
            {
                // Generates a sine wave and maps its range from [-1, 1] to [0, 1]
                return 0.5f * (std::sin((double) i / (double) numPoints * juce::MathConstants<double>::twoPi) + 1.0f);
            };
            break;

        case LfoWaveform::Triangle:
            wave_function = [numPoints](size_t i)
            {
                double phase = (double) i / (double) numPoints;
                if (phase < 0.5)
                    return (float) (2.0 * phase); // Ramp up 0 -> 1
                else
                    return (float) (2.0 * (1.0 - phase)); // Ramp down 1 -> 0
            };
            break;

        case LfoWaveform::SawtoothDown: // "Saw" - 斜下
            wave_function = [numPoints](size_t i)
            {
                return 1.0f - ((float) i / (float) numPoints);
            };
            break;

        case LfoWaveform::SawtoothUp: // "Ramp" - 斜上
            wave_function = [numPoints](size_t i)
            {
                return (float) i / (float) numPoints;
            };
            break;

        case LfoWaveform::Square:
            wave_function = [numPoints](size_t i)
            {
                return ((double) i / (double) numPoints < 0.5) ? 1.0f : 0.0f;
            };
            break;

        case LfoWaveform::User:
            // For User mode, the wavetable is updated via updateUserShape(), so we do nothing here.
            return;
    }

    wavetable.initialise(wave_function, numPoints);
}

void LfoEngine::updateShape(const LfoData& shapeData)
{
    // When this function is called, we know the user is editing the shape.
    currentWaveform = LfoWaveform::User;

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