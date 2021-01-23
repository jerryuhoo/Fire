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
    g.setColour(getColour());
    g.fillEllipse(0, 0, getWidth(), getHeight());
    g.setColour(COLOUR7);
    g.fillEllipse(1, 1, getWidth() - 2, getHeight() - 2);
    g.setColour(getColour());
    g.fillEllipse(getWidth() / 4.f, getHeight() / 4.f, getWidth() / 2.f, getHeight() / 2.f);
}

void EnableButton::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}

void EnableButton::mouseUp(const juce::MouseEvent &e)
{
    if (mState)
    {
        mState = false;
    }
    else
    {
        mState = true;
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

bool EnableButton::getState()
{
    return mState;
}

void EnableButton::setState(bool state)
{
    mState = state;
}

juce::Colour EnableButton::getColour()
{
    if (isEntered)
    {
        if (!mState)
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
        if (!mState)
        {
            return juce::Colours::grey;
        }
        else
        {
            return COLOUR1;
        }
    }
}
