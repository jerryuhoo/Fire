/*
  ==============================================================================

    GraphTemplate.cpp
    Created: 5 Oct 2021 11:42:57am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "GraphTemplate.h"

//==============================================================================
GraphTemplate::GraphTemplate()
{

}

GraphTemplate::~GraphTemplate()
{
}

void GraphTemplate::paint (juce::Graphics& g)
{
    // draw outline
    g.setColour (COLOUR6);
    g.drawRect (getLocalBounds(), 1);
}

void GraphTemplate::resized()
{

}

void GraphTemplate::setScale(float scale)
{
    this->scale = scale;
}

float GraphTemplate::getScale()
{
    return scale;
}

bool GraphTemplate::getZoomState()
{
    return mZoomState;
}

void GraphTemplate::setZoomState(bool zoomState)
{
    mZoomState = zoomState;
}

void GraphTemplate::mouseDown(const juce::MouseEvent &e)
{
    if (mZoomState)
    {
        mZoomState = false;
        scale = 1;
    }
    else
    {
        mZoomState = true;
        scale = 2;
    }
}

void GraphTemplate::mouseEnter(const juce::MouseEvent &e)
{
    isMouseOn = true;
}

void GraphTemplate::mouseExit(const juce::MouseEvent &e)
{
    isMouseOn = false;
}
