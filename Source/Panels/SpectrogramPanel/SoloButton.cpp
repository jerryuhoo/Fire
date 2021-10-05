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
    g.setColour(getColour().darker().darker());
    g.fillEllipse(0, 0, getWidth(), getHeight());
    g.setColour(getColour());
    g.drawText("S", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
}

void SoloButton::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}

void SoloButton::mouseEnter(const juce::MouseEvent &e)
{
    isEntered = true;
}

void SoloButton::mouseExit(const juce::MouseEvent &e)
{
    isEntered = false;
}

juce::Colour SoloButton::getColour()
{
    if (isEntered)
    {
        if (!getToggleState())
        {
            return juce::Colours::grey.withAlpha(0.8f);
        }
        else
        {
            return COLOUR1.withAlpha(0.8f);
        }
    }
    else
    {
        if (!getToggleState())
        {
            return juce::Colours::grey;
        }
        else
        {
            return COLOUR1;
        }
    }
}
