#pragma once
#include <juce_dsp/juce_dsp.h>
#include <Utility/InsertParameters.h>

namespace fire::tests
{
// Independent reference for the unused insertion budget in legacy-only
// recipes. The reserve is at the output so the old DSP/sample clocks remain
// unchanged; HQ no longer implies that post-band control events are immediate.
inline int legacyInsertOutputReserve()
{
    static const int reserve = []
    {
        juce::dsp::Oversampling<float> shape(2, 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);
        shape.initProcessing(1);
        return 2 * effects::slotCount * juce::roundToInt(shape.getLatencyInSamples());
    }();
    return reserve;
}
inline int legacyControlOutputDelay(bool hq, int reportedLatency)
{return hq ? legacyInsertOutputReserve() : reportedLatency;}
inline int legacyTransitionEnvelopeDelay(bool hq, int reportedLatency)
{return hq ? 0 : reportedLatency - legacyInsertOutputReserve();}
}
