/*
  ==============================================================================

    EnableButton.cpp
    Created: 3 Dec 2020 8:18:45pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "EnableButton.h"

//==============================================================================
EnableButton::EnableButton()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

EnableButton::~EnableButton()
{
}

void EnableButton::paint (juce::Graphics& g)
{
    if (isEntered)
    {
        g.setColour(COLOUR1.withAlpha(0.2f));
        g.fillEllipse(0, 0, getWidth(), getHeight());
        g.setColour(COLOUR7);
        g.drawText(".", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colours::grey);
        g.fillEllipse(0, 0, getWidth(), getHeight());
        g.setColour(COLOUR7);
        g.drawText(".", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
    }
}

void EnableButton::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}

void EnableButton::mouseUp(const juce::MouseEvent &e)
{
    if (state = true)
    {
        state = false;
    }
    else
    {
        state = true;
    }
}

void EnableButton::mouseEnter(const juce::MouseEvent &e)
{
    isEntered = true;
}

void EnableButton::mouseExit(const juce::MouseEvent &e)
{
    isEntered = false;
}
