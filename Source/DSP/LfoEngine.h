/*
  ==============================================================================

    LfoEngine.h
    Created: 4 Aug 2025 6:15:46pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../Utility/Parameters.h" // Make sure LfoWaveform enum is here
#include "LfoData.h"
#include "juce_dsp/juce_dsp.h"
#include <vector>

//==============================================================================
/**
    LfoEngine
 
    Generates an LFO signal from either a built-in shape (Sine, Saw, etc.)
    or a user-defined shape, using a high-performance wavetable lookup.
*/
class LfoEngine
{
public:
    LfoEngine();

    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);

    /** Updates the LFO shape from user-drawn points and curvatures. 
        This automatically sets the waveform type to User.
    */
    void updateShape(const LfoData& shapeData);

    /** Processes one sample of the LFO. Returns a unipolar [0, 1] signal. */
    float process();

    /** Sets the phase increment per sample, controlling the LFO's speed. */
    void setPhaseDelta(float newPhaseDelta);

    float getPhase() const;
    float getLastOutput() const;

private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
    float phaseDelta = 0.0f;
    float lastOutput = 0.0f;

    juce::dsp::LookupTable<float> wavetable;
};
