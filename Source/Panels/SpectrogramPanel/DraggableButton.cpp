/*
  ==============================================================================

    DraggableButton.cpp
    Created: 7 Oct 2021 11:42:21pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "DraggableButton.h"

//==============================================================================
DraggableButton::DraggableButton()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

DraggableButton::~DraggableButton()
{
}

void DraggableButton::paint (juce::Graphics& g)
{
    g.setColour(getColour().darker().darker());
    g.fillEllipse(0, 0, getWidth(), getHeight());

    g.setColour(getColour());
    g.fillEllipse(getWidth() / 5.0f, getHeight() / 5.0f, getWidth() / 5.0f * 3, getHeight() / 5.0f * 3);
}

void DraggableButton::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}

void DraggableButton::mouseEnter(const juce::MouseEvent &e)
{
    isEntered = true;
}

void DraggableButton::mouseExit(const juce::MouseEvent &e)
{
    isEntered = false;
}

juce::Colour DraggableButton::getColour()
{
    if (isEntered)
    {
        if (mState) return juce::Colours::hotpink.withAlpha(0.8f);
        else return juce::Colours::dimgrey.withAlpha(0.8f);
    }
    else
    {
        if (mState) return juce::Colours::hotpink;
        else return juce::Colours::lightgrey;
    }
}

void DraggableButton::setState(const bool state)
{
    mState = state;
}
