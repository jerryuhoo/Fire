/*
  ==============================================================================

    WidthGraph.cpp
    Created: 14 Dec 2020 3:40:40pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "WidthGraph.h"
#include "../GUI/InterfaceDefines.h"

//==============================================================================
WidthGraph::WidthGraph(FireAudioProcessor &p) : processor(p)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
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
    
    // draw rotated rectangle
//    float rectWidth = getWidth() / sqrt(2);
//    juce::Rectangle<float> rect = juce::Rectangle<float>(getWidth() / 2.0f - rectWidth / 2.0f,
//                                                         getHeight() - rectWidth / 2.0f, getWidth() / sqrt(2), getWidth() / sqrt(2));
//
//    juce::AffineTransform t = juce::AffineTransform::rotation(rotateAngle, rect.getX() + rect.getWidth() / 2.0f,
//                                                              rect.getY() + rect.getWidth() / 2.0f);
//
//    juce::Path p;
//    p.addRectangle(rect);
//    g.strokePath(p, juce::PathStrokeType(1), t);
    
    historyL = processor.getHistoryArrayL();
    historyR = processor.getHistoryArrayR();
    
    // This is Lissajous Graph
    
    // revert xy coordinates
    g.addTransform(juce::AffineTransform::scale(-1, -1, getWidth() / 2.0f, getHeight() / 2.0f));
    // rotate 45 degrees
    g.addTransform(juce::AffineTransform::rotation(rotateAngle, getWidth() / 2.0f, getHeight() / 2.0f));
    
    // find max value
    float maxValue = 0.0f;
    
    for (int i = 0; i < historyL.size(); i++)
    {
        if (historyL[i] > maxValue || historyR[i] > maxValue)
        {
            maxValue = historyL[i] > historyR[i] ? historyL[i] : historyR[i];
        }
    }
    
    for (int i = 0; i < historyL.size(); i++)
    {
        float x = historyL[i] * getHeight() / 4.0f;
        float y = historyR[i] * getHeight() / 4.0f;
        
        // normalize
        if (maxValue > 0.00001f)
        {
            x = x / maxValue;
            y = y / maxValue;
        }
        
        g.setColour(COLOUR1);
//        g.drawEllipse(getWidth() / 2.0f + x, getHeight() + y, 0.5f, 0.5f, 0.5f);
        g.drawEllipse(getWidth() / 2.0f + x, getHeight() / 2.0f + y, 0.5f, 0.5f, 0.5f);
    }
}

void WidthGraph::resized()
{
}
