/*
  ==============================================================================

    GlobalPanel.cpp
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "GlobalPanel.h"

//==============================================================================
GlobalPanel::GlobalPanel()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

GlobalPanel::~GlobalPanel()
{
}

void GlobalPanel::paint (juce::Graphics& g)
{
    /* This demo code just fills the component's background and
       draws some placeholder text to get you started.

       You should replace everything in this method with your own
       drawing code..
    */

    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText ("GlobalPanel", getLocalBounds(),
                juce::Justification::centred, true);   // draw some placeholder text
}

void GlobalPanel::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}
