/*
 ==============================================================================
 
 Delay.h
 Created: 18 Jul 2020 7:51:15pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

class Delay
{
public:
    explicit Delay(int initialDelayTime)
    {
        delayBuffer.setSize(numberOfChannels, bufferSize);
        reset(initialDelayTime);
    }
    
    void reset(int newDelayTime)
    {
        delayBuffer.clear();
        delayTime = juce::jlimit(0, juce::jmax(0, delayBuffer.getNumSamples()), newDelayTime);
        writeIndices.fill(0);
    }
    
    void setLatency(int latency)
    {
        delayTime = juce::jlimit(0, juce::jmax(0, delayBuffer.getNumSamples()), latency);
    }
    
    float process(float insample, int channel, int numSamples)
    {
        const float safeInput = std::isfinite(insample) ? insample : 0.0f;

        if (! state || delayTime == 0 || numSamples <= 0
            || ! juce::isPositiveAndBelow(channel, numberOfChannels)
            || ! juce::isPositiveAndBelow(channel, delayBuffer.getNumChannels()))
            return safeInput;

        const int currentBufferSize = delayBuffer.getNumSamples();
        if (currentBufferSize <= 0)
            return safeInput;

        auto& writeIndex = writeIndices[static_cast<size_t>(channel)];
        writeIndex = juce::jlimit(0, currentBufferSize - 1, writeIndex);

        int readIndex = writeIndex - juce::jmin(delayTime, currentBufferSize);
        if (readIndex < 0)
            readIndex += currentBufferSize;

        const float output = delayBuffer.getSample(channel, readIndex);
        delayBuffer.setSample(channel, writeIndex, safeInput);
        writeIndex = (writeIndex + 1) % currentBufferSize;

        return output;
    }
    
    void setState(bool currentState)
    {
        state = currentState;
    }
    
    juce::AudioBuffer<float> delayBuffer;
    
private:
    static constexpr int numberOfChannels = 2;
    static constexpr int bufferSize = 44100;

    std::array<int, numberOfChannels> writeIndices {};
    int delayTime = 0;
    bool state = false;
};
