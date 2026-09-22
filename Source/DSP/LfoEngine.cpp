/*
  ==============================================================================

    LfoEngine.cpp
    Created: 14 Sep 2025 11:10:19pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "LfoEngine.h"
#include <cmath>

LfoEngine::LfoEngine() = default;

void LfoEngine::reset()
{
    phase = 0.0f;
    phaseCorrection = 0.0f;
    phaseCorrectionStep = 0.0f;
    phaseCorrectionRemaining = 0;
    lastOutput = 0.0f;
    lastRenderedPhase = -1.0f;
    publishedPhase.store(phase, std::memory_order_relaxed);
    publishedOutput.store(lastOutput, std::memory_order_relaxed);
    transitionSamplesProcessed = transitionLengthSamples;
    hasProcessedSample = false;
}

void LfoEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    lastRenderedPhase = -1.0f;
    jassert(std::isfinite(spec.sampleRate) && spec.sampleRate > 0.0);
    const double safeSampleRate = std::isfinite(spec.sampleRate) && spec.sampleRate > 0.0
                                      ? spec.sampleRate
                                      : 44100.0;
    transitionLengthSamples = juce::jmax(1, juce::roundToInt(safeSampleRate * 0.01));
    transitionSamplesProcessed = transitionLengthSamples;
    phaseCorrectionLengthSamples = transitionLengthSamples;
    phaseCorrection = 0.0f;
    phaseCorrectionStep = 0.0f;
    phaseCorrectionRemaining = 0;
    hasProcessedSample = false;
}

void LfoEngine::stageShape(const LfoData& shapeData)
{
    const auto& points = shapeData.points;
    const auto& curvatures = shapeData.curvatures;

    if (points.size() < 2)
    {
        jassertfalse;
        return;
    }

    for (size_t i = 0; i < points.size(); ++i)
    {
        if (! std::isfinite(points[i].x) || ! std::isfinite(points[i].y)
            || (i > 0 && points[i].x < points[i - 1].x))
        {
            jassertfalse;
            return;
        }
    }

    // Step 1: Create a temporary, mutable array to build the waveform.
    Wavetable rawTable {};

    // Step 2: Generate the raw, unsmoothed shape into the temporary array.
    size_t segmentIndex = 0;
    for (size_t i = 0; i < wavetableSize; ++i)
    {
        const float lookupPhase = static_cast<float>(i)
                                  / static_cast<float>(wavetableSize - 1);

        float sampleValue = 0.5f; // Default to middle value

        while (segmentIndex + 1 < points.size() && lookupPhase > points[segmentIndex + 1].x)
            ++segmentIndex;

        if (segmentIndex + 1 < points.size())
        {
            const auto& p1 = points[segmentIndex];
            const auto& p2 = points[segmentIndex + 1];

            if (lookupPhase >= p1.x && lookupPhase <= p2.x)
            {
                const float segmentWidth = p2.x - p1.x;

                // Fallback to linear interpolation if curvature data is missing or the segment is vertical.
                if (segmentIndex >= curvatures.size() || std::abs(segmentWidth) < 1.0e-9f)
                {
                    sampleValue = (std::abs(segmentWidth) < 1.0e-9f)
                                      ? p1.y
                                      : p1.y + (p2.y - p1.y) * ((lookupPhase - p1.x) / segmentWidth);
                }
                else // Apply curvature
                {
                    const float rawCurvature = curvatures[segmentIndex];
                    const float curvature = std::isfinite(rawCurvature)
                                                ? juce::jlimit(-2.0f, 2.0f, rawCurvature)
                                                : 0.0f;
                    const float tx = juce::jlimit(0.0f, 1.0f, (lookupPhase - p1.x) / segmentWidth);
                    const float absExp = std::pow(4.0f, std::abs(curvature));
                    const float ty = curvature >= 0.0f
                                         ? std::pow(tx, absExp)
                                         : 1.0f - std::pow(juce::jmax(0.0f, 1.0f - tx), absExp);

                    sampleValue = p1.y + (p2.y - p1.y) * ty;
                }
            }
        }

        // If phase is somehow outside all segments, hold the last point's value at the endpoint.
        if (i == wavetableSize - 1)
            sampleValue = points.back().y;

        rawTable[i] = std::isfinite(sampleValue) ? juce::jlimit(0.0f, 1.0f, sampleValue) : 0.5f;
    }
    rawTable[wavetableSize] = rawTable[wavetableSize - 1];

    // Build every value exposed by the 0.01-stepped Smooth parameter. This is
    // deliberately done by the producer thread, never by process().
    stagedBank = 1 - activeBank;
    auto& bank = wavetableBanks[static_cast<size_t>(stagedBank)];
    for (size_t step = 0; step < smoothnessStepCount; ++step)
    {
        auto& table = bank[step];
        table = rawTable;

        const float smoothness = static_cast<float>(step)
                                 / static_cast<float>(smoothnessStepCount - 1);
        if (smoothness <= 0.001f)
            continue;

        const float feedbackCoeff = smoothness * 0.95f;
        const float feedforwardCoeff = 1.0f - feedbackCoeff;

        for (int pass = 0; pass < 2; ++pass)
        {
            float smoothingState = table[wavetableSize - 1];
            for (size_t sample = 0; sample < wavetableSize; ++sample)
            {
                auto& currentSample = table[sample];
                const float smoothedSample = (currentSample * feedforwardCoeff) + (smoothingState * feedbackCoeff);
                currentSample = smoothedSample;
                smoothingState = smoothedSample;
            }
        }

        table[wavetableSize] = table[wavetableSize - 1];
    }

    stagedBankReady = true;
}

void LfoEngine::publishStagedShape() noexcept
{
    if (! stagedBankReady)
        return;

    if (! hasProcessedSample || transitionLengthSamples <= 0)
    {
        activeBank = stagedBank;
        stagedBankReady = false;
        transitionSamplesProcessed = transitionLengthSamples;
        return;
    }

    // The producer may immediately reuse the old bank after this hand-off.
    // Capture the currently audible table into audio-thread-owned storage
    // before switching banks, then crossfade to the published shape.
    captureCurrentAudibleTable();
    activeBank = stagedBank;
    stagedBankReady = false;
    beginTableTransition();
}

void LfoEngine::setSmoothness(float newSmoothness) noexcept
{
    const float safeSmoothness = std::isfinite(newSmoothness)
                                     ? juce::jlimit(0.0f, 1.0f, newSmoothness)
                                     : 0.0f;
    const int newStep = juce::jlimit(0,
                                     static_cast<int>(smoothnessStepCount - 1),
                                     juce::roundToInt(safeSmoothness
                                                      * static_cast<float>(smoothnessStepCount - 1)));
    if (newStep == activeSmoothnessStep)
        return;

    if (! hasProcessedSample || transitionLengthSamples <= 0)
    {
        activeSmoothnessStep = newStep;
        transitionSamplesProcessed = transitionLengthSamples;
        return;
    }

    captureCurrentAudibleTable();
    activeSmoothnessStep = newStep;
    beginTableTransition();
}

float LfoEngine::lookupTable(const Wavetable& table, float lookupPhase) const noexcept
{
    const float safePhase = juce::jlimit(0.0f, 1.0f, lookupPhase);
    const float tablePosition = safePhase * static_cast<float>(wavetableSize - 1);
    const size_t tableIndex = juce::jmin(static_cast<size_t>(tablePosition), wavetableSize - 1);
    const float fraction = tablePosition - static_cast<float>(tableIndex);
    return table[tableIndex] + fraction * (table[tableIndex + 1] - table[tableIndex]);
}

void LfoEngine::captureCurrentAudibleTable() noexcept
{
    const auto& currentTarget = wavetableBanks[static_cast<size_t>(activeBank)]
                                              [static_cast<size_t>(activeSmoothnessStep)];
    if (transitionLengthSamples <= 0
        || transitionSamplesProcessed >= transitionLengthSamples)
    {
        transitionSourceTable = currentTarget;
        return;
    }

    const float amount = static_cast<float>(transitionSamplesProcessed)
                         / static_cast<float>(transitionLengthSamples);
    for (size_t sample = 0; sample < transitionSourceTable.size(); ++sample)
        transitionSourceTable[sample] += amount
                                         * (currentTarget[sample] - transitionSourceTable[sample]);
}

void LfoEngine::beginTableTransition() noexcept
{
    transitionSamplesProcessed = transitionLengthSamples > 0 ? 0 : transitionLengthSamples;
}

// Call this on every sample in processBlock. Returns a bipolar [-1, 1] signal.
float LfoEngine::process()
{
    // Get the unipolar [0, 1] value from the pre-calculated wavetable
    if (! std::isfinite(phase))
        phase = 0.0f;

    float safePhase = juce::jlimit(0.0f, 1.0f, phase);
    if (phaseCorrectionRemaining > 0)
    {
        auto audiblePhase = phase + phaseCorrection;
        if (! std::isfinite(audiblePhase))
            audiblePhase = phase;
        audiblePhase -= std::floor(audiblePhase);
        safePhase = juce::jlimit(0.0f, 1.0f, audiblePhase);
    }
    lastRenderedPhase = safePhase;
    const auto& table = wavetableBanks[static_cast<size_t>(activeBank)]
                                      [static_cast<size_t>(activeSmoothnessStep)];
    float unipolarOutput = lookupTable(table, safePhase);
    if (transitionLengthSamples > 0
        && transitionSamplesProcessed < transitionLengthSamples)
    {
        ++transitionSamplesProcessed;
        if (transitionSamplesProcessed < transitionLengthSamples)
        {
            const float amount = static_cast<float>(transitionSamplesProcessed)
                                 / static_cast<float>(transitionLengthSamples);
            const float sourceOutput = lookupTable(transitionSourceTable, safePhase);
            unipolarOutput = sourceOutput + amount * (unipolarOutput - sourceOutput);
        }
    }

    // Advance the phase using the externally calculated delta
    phase += phaseDelta;

    if (phase >= 1.0f || phase < 0.0f)
        phase -= std::floor(phase);

    // Emit the complete correction at the event sample, then advance it once
    // per generated LFO sample. At 48 kHz samples 0..479 transition and sample
    // 480 is exactly the newly anchored canonical phase.
    if (phaseCorrectionRemaining > 0)
    {
        --phaseCorrectionRemaining;
        if (phaseCorrectionRemaining > 0)
            phaseCorrection += phaseCorrectionStep;
        else
        {
            phaseCorrection = 0.0f;
            phaseCorrectionStep = 0.0f;
        }
    }

    // Convert the output to bipolar [-1, 1] for modulation
    lastOutput = unipolarOutput;
    hasProcessedSample = true;
    publishedPhase.store(phase, std::memory_order_relaxed);
    publishedOutput.store(lastOutput, std::memory_order_relaxed);
    return unipolarOutput;
}

// Call this from your processor before each block to set the LFO speed.
// The processor is responsible for calculating the correct delta for Hz or BPM sync.
void LfoEngine::setPhaseDelta(float newPhaseDelta)
{
    phaseDelta = std::isfinite(newPhaseDelta) ? newPhaseDelta : 0.0f;
}

void LfoEngine::setPhase(float newPhase)
{
    // Set only the canonical phase. Any live correction deliberately survives
    // the host's normal absolute re-anchor at each callback boundary.
    phase = std::isfinite(newPhase) ? juce::jlimit(0.0f, 1.0f, newPhase) : 0.0f;
    publishedPhase.store(phase, std::memory_order_relaxed);
}

void LfoEngine::setPhaseWithCorrection(float newPhase)
{
    auto wrapPhase = [] (float value) noexcept
    {
        if (! std::isfinite(value))
            return 0.0f;
        value -= std::floor(value);
        return value;
    };

    const float newCanonicalPhase = wrapPhase(newPhase);
    if (! hasProcessedSample || phaseCorrectionLengthSamples <= 0)
    {
        phase = newCanonicalPhase;
        phaseCorrection = 0.0f;
        phaseCorrectionStep = 0.0f;
        phaseCorrectionRemaining = 0;
        publishedPhase.store(phase, std::memory_order_relaxed);
        return;
    }

    const float previousAudiblePhase = wrapPhase(phase + phaseCorrection);
    phase = newCanonicalPhase;

    // The two phases are circular values. Choose the shortest signed offset so
    // a large Rate/Sync change never makes the transition rotate unnecessarily
    // through almost a complete cycle.
    float wrappedDifference = previousAudiblePhase - newCanonicalPhase;
    wrappedDifference -= std::round(wrappedDifference);
    if (! std::isfinite(wrappedDifference)
        || std::abs(wrappedDifference) <= 1.0e-7f)
    {
        phaseCorrection = 0.0f;
        phaseCorrectionStep = 0.0f;
        phaseCorrectionRemaining = 0;
    }
    else
    {
        phaseCorrection = wrappedDifference;
        phaseCorrectionRemaining = phaseCorrectionLengthSamples;
        phaseCorrectionStep = -phaseCorrection
                              / static_cast<float>(phaseCorrectionRemaining);
    }

    publishedPhase.store(phase, std::memory_order_relaxed);
}

float LfoEngine::getPhase() const
{
    return publishedPhase.load(std::memory_order_relaxed);
}

float LfoEngine::getLastOutput() const
{
    return publishedOutput.load(std::memory_order_relaxed);
}
