/*
 ==============================================================================
 
 Delay.h
 Created: 18 Jul 2020 7:51:15pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#pragma once

class Delay
{
public:
    Delay(int delayTime_) : delayTime(delayTime_)
    {
        delayBuffer.setSize(2, size);
        delayBuffer.clear();
        writeIndex = delayTime;
    }
    
    void reset(int newDelayTime)
    {
        delayBuffer.clear();
        writeIndex = newDelayTime;
        readIndex = 0;
    }
    
    void setLatency(int latency)
    {
        writeIndex = latency;
    }
    
    float process(float insample, int channel, int numSamples)
    {
        if (state)
        {
            float output = delayBuffer.getSample(channel, readIndex);
            readIndex = (readIndex + 1) % numSamples;
            delayBuffer.setSample(channel, writeIndex, insample);
            writeIndex = (writeIndex + 1) % numSamples;
            
            return output;
        }
        else
        {
            return insample;
        }
    }
    
    void setState(bool currentState)
    {
        state = currentState;
    }
    
    juce::AudioBuffer<float> delayBuffer;
    
private:
    int size = 44100;
    int readIndex = 0;
    int writeIndex = 0;
    int delayTime = 0;
    bool state = false;
};
