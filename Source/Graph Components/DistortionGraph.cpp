/*
  ==============================================================================

    DistortionGraph.cpp
    Created: 29 Nov 2020 10:31:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "DistortionGraph.h"

//==============================================================================
DistortionGraph::DistortionGraph()
{

}

DistortionGraph::~DistortionGraph()
{
}

void DistortionGraph::paint (juce::Graphics& g)
{
    // paint distortion function
    
    float functionValue = 0;
    float mixValue;

    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;

    auto frameRight = getLocalBounds();
    
    if (mode < 9)
    {
        const int numPix = frameRight.getWidth(); // you might experiment here, if you want less steps to speed up

        float driveScale = 1;
        float maxValue = 2.0f * driveScale * mix + 2.0f * (1 - mix);
        float value = -maxValue; // minimum (leftmost)  value for your graph
        float valInc = (maxValue - value) / numPix;
        float xPos = frameRight.getX();
        const float posInc = frameRight.getWidth() / numPix;

        juce::Path p;

        bool edgePointL = false;
        bool edgePointR = false;

        for (int i = 1; i < numPix; ++i)
        {
            value += valInc;
            xPos += posInc;

            // symmetrical distortion
            if (mode < 9)
            {
                functionValue = distortionProcessor.distortionProcess(value);
            }

            if (rateDivide > 1)
            {
                functionValue = ceilf(functionValue * (64.f / rateDivide)) / (64.f / rateDivide);
            }

            // retification
            functionValue = distortionProcessor.rectificationProcess(functionValue);

            // mix
            functionValue = (1.f - mix) * value + mix * functionValue;
            mixValue = (2.0f / 3.0f) * functionValue;
            float yPos = frameRight.getCentreY() - frameRight.getHeight() * mixValue / 2.0f;

            // draw points
            if (yPos < frameRight.getY())
            {
                if (edgePointR == false)
                {
                    yPos = frameRight.getY();
                    edgePointR = true;
                }
                else
                {
                    continue;
                }
            }

            if (yPos > frameRight.getBottom())
            {
                if (edgePointL == false)
                {
                    continue;
                }
                else
                {
                    yPos = frameRight.getBottom();
                }
            }
            else if (edgePointL == false)
            {
                if (mode == 0)
                {
                    p.startNewSubPath(xPos, frameRight.getBottom());
                    p.lineTo(xPos, yPos);
                }
                else
                {
                    p.startNewSubPath(xPos, yPos);
                }
                edgePointL = true;
            }
            p.lineTo(xPos, yPos);
        }

        int colour_r = 244;
        int colour_g = (208 - drive * 2 < 0) ? 0 : (208 - drive * 2);
        int colour_b = 63;

        juce::ColourGradient grad(juce::Colour(colour_r, colour_g, colour_b), frameRight.getX() + frameRight.getWidth() / 2, frameRight.getY() + frameRight.getHeight() / 2,
                                  COLOUR6, frameRight.getX(), frameRight.getY() + frameRight.getHeight() / 2, true);
        g.setGradientFill(grad);
        g.strokePath(p, juce::PathStrokeType(2.0));
    }
    


}

void DistortionGraph::resized()
{
 
}

void DistortionGraph::setState(int mode, float color, float rec, float mix, float bias, float drive, float rateDivide)
{
    this->mode = mode;
    this->color = color;
    this->rec = rec;
    this->mix = mix;
    this->bias = bias;
    this->drive = drive;
    this->rateDivide = rateDivide;
}
