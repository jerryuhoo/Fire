/*
  ==============================================================================

    WidthProcessor.cpp
    Created: 21 Dec 2020 6:05:29pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/


#include "WidthProcessor.h"

WidthProcessor::WidthProcessor()
{  
}

WidthProcessor::~WidthProcessor()
{  
}

void WidthProcessor::process(float* channeldataL, float* channeldataR, float width, int numSamples)
{
    // width
    for (int i = 0; i < numSamples; ++i)
    {
        float mid = (channeldataL[i] + channeldataR[i]) / sqrt(2);   // obtain mid-signal from left and right
        float sides = (channeldataL[i] - channeldataR[i]) / sqrt(2);   // obtain side-signal from left and right

        // amplify mid and side signal seperately:
        mid *= 2.0f * (1.0f - width);
        sides *= 2.0f * width;

        channeldataL[i] = (mid + sides) / sqrt(2);   // obtain left signal from mid and side
        channeldataR[i] = (mid - sides) / sqrt(2);   // obtain right signal from mid and side
    }
    
//    mSignal = 0.5 * (left + right);
//    sSignal = left - right;
//
//    float pan; // [-1; +1]
//    left  = 0.5 * (1.0 + pan) * mSignal + sSignal;
//    right = 0.5 * (1.0 - pan) * mSignal - sSignal;
}

