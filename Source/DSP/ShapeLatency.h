#pragma once

#include <juce_dsp/juce_dsp.h>

namespace fire::effects
{
// Reserve a constant, integer delay for every independently insertable slot.
// Adding a Shape or automating HQ must not change the host's PDC or the timing
// of a parallel dry path. Only prepare/lifecycle code constructs this probe.
inline int independentShapeLatency(size_t channels)
{
    juce::dsp::Oversampling<float> probe(channels, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);
    probe.initProcessing(1);
    return juce::jmax(1, juce::roundToInt(probe.getLatencyInSamples()));
}
}
