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

void WidthProcessor::process(float* channeldataL, float* channeldataR, float width, float pan, int numSamples)
{
    // width
    for (int i = 0; i < numSamples; ++i)
    {
        float mid = (channeldataL[i] + channeldataR[i]) / sqrt(2); // obtain mid-signal from left and right
        float sides = (channeldataL[i] - channeldataR[i]) / sqrt(2); // obtain side-signal from left and right

        // amplify mid and side signal seperately:
        mid *= 2.0f * (1.0f - width);
        sides *= 2.0f * width;

        channeldataL[i] = (mid + sides) / sqrt(2); // obtain left signal from mid and side
        channeldataR[i] = (mid - sides) / sqrt(2); // obtain right signal from mid and side

        float panLeftGain = 1.0f;
        float panRightGain = 1.0f;

        if (pan > 0.0f) // Panning to the right
        {
            // As pan goes from 0 to 1, left gain goes from 1 to 0.
            panLeftGain = 1.0f - pan;
        }
        else // Panning to the left (pan is 0 or negative)
        {
            // As pan goes from 0 to -1, right gain goes from 1 to 0.
            panRightGain = 1.0f + pan;
        }

        channeldataL[i] *= panLeftGain;
        channeldataR[i] *= panRightGain;
    }
}
