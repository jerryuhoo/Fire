/*
  ==============================================================================

    SampleAccurateCompressor.cpp

  ==============================================================================
*/

#include "SampleAccurateCompressor.h"

#include <cmath>

void SampleAccurateCompressor::setThreshold(float newThreshold)
{
    if (juce::exactlyEqual(newThreshold, thresholdDb))
        return;

    thresholdDb = newThreshold;
    threshold = juce::Decibels::decibelsToGain(thresholdDb, -200.0f);
    thresholdInverse = 1.0f / threshold;
}

void SampleAccurateCompressor::setRatio(float newRatio)
{
    jassert(newRatio >= 1.0f);
    if (juce::exactlyEqual(newRatio, ratio))
        return;

    ratio = newRatio;
    ratioInverse = 1.0f / ratio;
}

void SampleAccurateCompressor::setAttack(float newAttack)
{
    if (juce::exactlyEqual(newAttack, attackTime))
        return;

    attackTime = newAttack;
    envelopeFilter.setAttackTime(attackTime);
}

void SampleAccurateCompressor::setRelease(float newRelease)
{
    if (juce::exactlyEqual(newRelease, releaseTime))
        return;

    releaseTime = newRelease;
    envelopeFilter.setReleaseTime(releaseTime);
}

void SampleAccurateCompressor::prepare(const juce::dsp::ProcessSpec& spec)
{
    envelopeFilter.prepare(spec);
    reset();
}

void SampleAccurateCompressor::reset()
{
    envelopeFilter.reset();
}

float SampleAccurateCompressor::processSample(int channel, float inputValue)
{
    const float envelope = envelopeFilter.processSample(channel, inputValue);
    const float gain = envelope < threshold
                           ? 1.0f
                           : std::pow(envelope * thresholdInverse,
                                      ratioInverse - 1.0f);
    return gain * inputValue;
}
