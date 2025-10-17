/*
  ==============================================================================

    DraggableButton.cpp
    Created: 7 Oct 2021 11:42:21pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "DraggableButton.h"

//==============================================================================
DraggableButton::DraggableButton()
{
}

DraggableButton::~DraggableButton()
{
}

void DraggableButton::paint(juce::Graphics& g)
{
    g.setColour(getColour().darker().darker());
    g.fillEllipse(0, 0, getWidth(), getHeight());

    g.setColour(getColour());
    g.fillEllipse(getWidth() / 5.0f, getHeight() / 5.0f, getWidth() / 5.0f * 3, getHeight() / 5.0f * 3);
}

void DraggableButton::resized()
{
}

void DraggableButton::mouseEnter(const juce::MouseEvent& e)
{
    isEntered = true;
}

void DraggableButton::mouseExit(const juce::MouseEvent& e)
{
    isEntered = false;
}

juce::Colour DraggableButton::getColour()
{
    if (mState && isEntered)
        return juce::Colours::hotpink.brighter();
    else if (mState && ! isEntered)
        return juce::Colours::hotpink;

    return juce::Colours::dimgrey;
}

void DraggableButton::setState(const bool state)
{
    mState = state;
}

void DraggableButton::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (onQValueChanged)
    {
        // The wheel.deltaY value can be used to control the Q value.
        // A positive value means scrolling up, and a negative value means scrolling down.
        // You can adjust the sensitivity by multiplying by a factor.
        onQValueChanged(wheel.deltaY);
    }
}