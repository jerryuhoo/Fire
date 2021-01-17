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

    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.drive = drive;
    distortionProcessor.controls.color = color;
    distortionProcessor.controls.rectification = rec;
    distortionProcessor.controls.bias = bias;

    auto frameRight = getLocalBounds();
    
    g.setColour(COLOUR6);
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 25);
    
    if (mode < 9)
    {
        float functionValue = 0.0f;
        float mixValue;
        
        const int numPix = frameRight.getWidth(); // you might experiment here, if you want less steps to speed up

        float driveScale = 1.0f;
        float maxValue = 2.0f * driveScale * mix + 2.0f * (1.0f - mix);
        float value = -maxValue; // minimum (leftmost)  value for your graph
        float valInc = (maxValue - value) / numPix;
        float xPos = frameRight.getX();
        const float posInc = frameRight.getWidth() / numPix;

        juce::Path p;

        bool edgePointL = false;
        bool edgePointR = false;

        // step = 0.5f, maybe high cpu but more accurate graph
        float step = 0.5f;
        
        for (float i = 1; i < numPix; i += step)
        {
            value += valInc * step;
            xPos += posInc * step;

            // symmetrical distortion
            if (mode < 9)
            {
                functionValue = distortionProcessor.distortionProcess(value);
            }

            if (rateDivide > 1.0f)
            {
//                functionValue = ceilf(functionValue * (64.0f / rateDivide)) / (64.0f / rateDivide);
                functionValue = ceilf(functionValue * (256.0f / rateDivide)) / (256.0f / rateDivide);
            }

            // retification
            functionValue = distortionProcessor.rectificationProcess(functionValue);

            // mix
            functionValue = (1.0f - mix) * value + mix * functionValue;
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
                                  juce::Colour(colour_r, colour_g, colour_b).withAlpha(0.0f), frameRight.getX(), frameRight.getY() + frameRight.getHeight() / 2, true);
        g.setGradientFill(grad);
        g.strokePath(p, juce::PathStrokeType(2.0f));
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
