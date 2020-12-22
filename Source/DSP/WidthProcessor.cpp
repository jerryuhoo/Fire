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

