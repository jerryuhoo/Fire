/*
  ==============================================================================

    SampleAccurateCompressor.h

    A JUCE-compatible compressor whose parameter caches can be updated
    independently. This avoids recalculating every coefficient when a single
    sample-accurate modulation target changes.

  ==============================================================================
*/

#pragma once

#include "juce_dsp/juce_dsp.h"

class SampleAccurateCompressor
{
public:
    void setThreshold(float newThreshold);
    void setRatio(float newRatio);
    void setAttack(float newAttack);
    void setRelease(float newRelease);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        const auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        const auto numChannels = outputBlock.getNumChannels();
        const auto numSamples = outputBlock.getNumSamples();

        jassert(inputBlock.getNumChannels() == numChannels);
        jassert(inputBlock.getNumSamples() == numSamples);

        if (context.isBypassed)
        {
            outputBlock.copyFrom(inputBlock);
            return;
        }

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            const auto* inputSamples = inputBlock.getChannelPointer(channel);
            auto* outputSamples = outputBlock.getChannelPointer(channel);
            for (size_t sample = 0; sample < numSamples; ++sample)
                outputSamples[sample] = processSample(static_cast<int>(channel),
                                                       inputSamples[sample]);
        }
    }

    float processSample(int channel, float inputValue);

private:
    juce::dsp::BallisticsFilter<float> envelopeFilter;
    float threshold = 1.0f;
    float thresholdInverse = 1.0f;
    float ratioInverse = 1.0f;
    float thresholdDb = 0.0f;
    float ratio = 1.0f;
    float attackTime = 1.0f;
    float releaseTime = 100.0f;
};
