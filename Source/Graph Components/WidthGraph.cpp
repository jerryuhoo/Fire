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
WidthGraph::WidthGraph()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
}

WidthGraph::~WidthGraph()
{
}

void WidthGraph::paint (juce::Graphics& g)
{
    g.setColour(COLOUR6);
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 25);
}

void WidthGraph::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}
