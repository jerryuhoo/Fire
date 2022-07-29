/*
  ==============================================================================

    WidthGraph.cpp
    Created: 14 Dec 2020 3:40:40pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "../../../GUI/InterfaceDefines.h"
#include "WidthGraph.h"
#include <JuceHeader.h>

//==============================================================================
WidthGraph::WidthGraph (FireAudioProcessor& p) : processor (p)
{
    startTimerHz (60);
}

WidthGraph::~WidthGraph()
{
}

void WidthGraph::paint (juce::Graphics& g)
{
    float pi = juce::MathConstants<float>::pi;
    float rotateAngle = pi / 4.0f;
    // draw outline
    g.setColour (COLOUR6);
    g.drawRect (getLocalBounds(), 1);

    // get history array values
    historyL = processor.getHistoryArrayL();
    if (processor.getTotalNumInputChannels() == 2)
    {
        historyR = processor.getHistoryArrayR();
    }
    else if (processor.getTotalNumInputChannels() == 1)
    {
        historyR = processor.getHistoryArrayL();
    }

    // This is Lissajous Graph

    // revert xy coordinates
    g.addTransform (juce::AffineTransform::scale (-1, -1, getWidth() / 2.0f, getHeight() / 2.0f));
    // rotate 45 degrees
    g.addTransform (juce::AffineTransform::rotation (rotateAngle, getWidth() / 2.0f, getHeight() / 2.0f));

    // find max value
    float maxValue = 0.0f;

    for (int i = 0; i < historyL.size(); i++)
    {
        if (historyL[i] > maxValue || historyR[i] > maxValue)
        {
            maxValue = historyL[i] > historyR[i] ? historyL[i] : historyR[i];
        }
    }

    g.setColour (juce::Colours::skyblue.withAlpha (0.2f));
    for (int i = 0; i < historyL.size(); i += 2)
    {
        float x = historyL[i] * getHeight() / 4.0f;
        float y = historyR[i] * getHeight() / 4.0f;

        // normalize
        if (maxValue > 0.00001f)
        {
            x = x / maxValue;
            y = y / maxValue;
        }

        //        g.fillEllipse(getWidth() / 2.0f + x, getHeight() / 2.0f + y, 1.0f, 1.0f);
        g.fillEllipse (getWidth() / 2.0f + x, getHeight() / 2.0f + y, 2.0f * getScale(), 2.0f * getScale());
    }

    if (isMouseOn && ! mZoomState)
    {
        g.setColour (WIDTH_COLOUR.withAlpha (0.05f));
        g.fillAll();
    }
}

void WidthGraph::timerCallback()
{
    repaint();
}

void WidthGraph::resized()
{
    // TODO: resize
}
