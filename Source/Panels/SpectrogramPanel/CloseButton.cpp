/*
  ==============================================================================

    CloseButton.cpp
    Created: 8 Nov 2020 7:57:32pm
    Author:  羽翼深蓝Wings

 ==============================================================================
*/

#include <JuceHeader.h>
#include "CloseButton.h"



//==============================================================================
CloseButton::CloseButton(VerticalLine &v) : verticalLine(v)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
}

CloseButton::~CloseButton()
{
}

void CloseButton::paint(juce::Graphics &g)
{
    if (isEntered)
    {
        g.setColour(COLOUR1);
        g.drawLine(getWidth() / 4.f, getHeight() / 4.f, getWidth() / 4.f * 3.f, getHeight() / 4.f * 3.f, 2.f);
        g.drawLine(getWidth() / 4.f, getHeight() / 4.f * 3.f, getWidth() / 4.f * 3.f, getHeight() / 4.f, 2.f);
    }
    else
    {
        g.setColour(COLOUR1.withBrightness(0.4f));
        g.drawLine(getWidth() / 4.f, getHeight() / 4.f, getWidth() / 4.f * 3.f, getHeight() / 4.f * 3.f, 2.f);
        g.drawLine(getWidth() / 4.f, getHeight() / 4.f * 3.f, getWidth() / 4.f * 3.f, getHeight() / 4.f, 2.f);
    }
}

void CloseButton::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}

void CloseButton::mouseDown(const juce::MouseEvent &e)
{
    setToggleState(false, juce::sendNotification);
}

void CloseButton::mouseEnter(const juce::MouseEvent &e)
{
    isEntered = true;
}

void CloseButton::mouseExit(const juce::MouseEvent &e)
{
    isEntered = false;
}
