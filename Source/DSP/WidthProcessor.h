/*
  ==============================================================================

    WidthProcessor.h
    Created: 21 Dec 2020 6:05:29pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class WidthProcessor
{
public:
    WidthProcessor();
    ~WidthProcessor();
    void process(float* channeldataL, float* channeldataR, float width, int numSamples);
private:
};
