/*
  ==============================================================================

    SoloButton.cpp
    Created: 3 Dec 2020 8:18:19pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SoloButton.h"

//==============================================================================
SoloButton::SoloButton()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

SoloButton::~SoloButton()
{
}

void SoloButton::paint (juce::Graphics& g)
{
    if (isEntered)
    {
        g.setColour(COLOUR1.withAlpha(0.2f));
        g.fillEllipse(0, 0, getWidth(), getHeight());
        g.setColour(COLOUR7);
        g.drawText("S", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
    }
    else
    {
        if (!state)
        {
            g.setColour(juce::Colours::grey);
            g.fillEllipse(0, 0, getWidth(), getHeight());
            g.setColour(COLOUR7);
            g.drawText("S", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
        }
        else
        {
            g.setColour(COLOUR1);
            g.fillEllipse(0, 0, getWidth(), getHeight());
            g.setColour(COLOUR7);
            g.drawText("S", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
        }
    }
}

void SoloButton::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}

void SoloButton::mouseUp(const juce::MouseEvent &e)
{
    if (state)
    {
        state = false;
    }
    else
    {
        state = true;
    }
}

void SoloButton::mouseEnter(const juce::MouseEvent &e)
{
    isEntered = true;
}

void SoloButton::mouseExit(const juce::MouseEvent &e)
{
    isEntered = false;
}
