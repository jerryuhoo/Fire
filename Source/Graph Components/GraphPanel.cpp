/*
  ==============================================================================

    GraphPanel.cpp
    Created: 30 Aug 2021 11:52:16pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "GraphPanel.h"

//==============================================================================
GraphPanel::GraphPanel(FireAudioProcessor &p) : processor(p)
{
    // Visualiser
    addAndMakeVisible(oscilloscope);
    
    // VU Meters
    addAndMakeVisible(vuPanel);
    
    // Distortion Graph
    addAndMakeVisible(distortionGraph);
    
    // Width Graph
    addAndMakeVisible(widthGraph);

}

GraphPanel::~GraphPanel()
{
}

void GraphPanel::paint (juce::Graphics& g)
{
}

void GraphPanel::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    oscilloscope.setBounds(0, 0, getWidth() / 2, getHeight() / 2);
    distortionGraph.setBounds(getWidth() / 2, 0, getWidth() / 2, getHeight() / 2);
    vuPanel.setBounds(0, getHeight() / 2, getWidth() / 2, getHeight() / 2);
    widthGraph.setBounds(getWidth() / 2, getHeight() / 2, getWidth() / 2, getHeight() / 2);
}

void GraphPanel::setDistortionState(int mode, float color, float rec, float mix,
                                    float bias, float drive, float rateDivide)
{
    distortionGraph.setState(mode, color, rec, mix, bias, drive, rateDivide);
}
