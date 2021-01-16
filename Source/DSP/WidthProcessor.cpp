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
    /*
    M = (L+R)/sqrt(2);   // obtain mid-signal from left and right
    S = (L-R)/sqrt(2);   // obtain side-signal from left and right

    // amplify mid and side signal seperately:
    M *= 2*(1-width);
    S *= 2*width;

    L = (M+S)/sqrt(2);   // obtain left signal from mid and side
    R = (M-S)/sqrt(2);   // obtain right signal from mid and side
    
    mSignal = 0.5 * (left + right);
    sSignal = left - right;

    float pan; // [-1; +1]
    left  = 0.5 * (1.0 + pan) * mSignal + sSignal;
    right = 0.5 * (1.0 - pan) * mSignal - sSignal;
    */
    for (int i = 0; i < numSamples; ++i)
    {
        float tmp = 1 / fmax(1 + width, 2);
        float coef_M = 1 * tmp;
        float coef_S = width * tmp;

        float mid = (channeldataL[i] + channeldataR[i]) * coef_M;
        float sides = (channeldataR[i] - channeldataL[i]) * coef_S;

        channeldataL[i] = mid - sides;
        channeldataR[i] = mid + sides;
    }
}

