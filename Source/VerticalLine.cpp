/*
  ==============================================================================

    VerticalLine.cpp
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "VerticalLine.h"
#include "LookAndFeel.h"

//==============================================================================
VerticalLine::VerticalLine()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
}

VerticalLine::~VerticalLine()
{
}

void VerticalLine::paint (juce::Graphics& g)
{
    // draw line that will be added next
    g.setColour(COLOUR1);
//    g.drawLine(x, y, x, y + height, 2);
    
    g.fillRect(getWidth() / 10.f * 4.f, 0.f, getWidth() / 10.f * 2.f, static_cast<float> (getHeight()));

    
    if (isEntered)
    {
        g.setColour(COLOUR1.withAlpha(0.2f));
        g.fillAll();
    }
    
    
    
    
}

void VerticalLine::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    
}


void VerticalLine::mouseUp(const juce::MouseEvent &e)
{
    move = false;
}

void VerticalLine::mouseEnter(const juce::MouseEvent &e)
{
    isEntered = true;
}

void VerticalLine::mouseExit(const juce::MouseEvent &e)
{
    isEntered = false;
}

void VerticalLine::mouseDrag(const juce::MouseEvent &e)
{
    if (e.mods.isLeftButtonDown())
    {
        move = true;
    }
    else if (e.mods.isRightButtonDown())
    {
        move = false;
    }
}

bool VerticalLine::isMoving()
{
    return move;
}

bool VerticalLine::getState()
{
    return state;
}

void VerticalLine::setState(bool state)
{
    this->state = state;
    if (state == false)
    {
        setVisible(false);
    }
    else
    {
        setVisible(true);
    }
}

void VerticalLine::setXPercent(float x)
{
    xPercent = x;
}

float VerticalLine::getXPercent()
{
    return xPercent;
}

void VerticalLine::setLeft(int leftIndex)
{
    this->leftIndex = leftIndex;
}

float VerticalLine::getLeft()
{
    return leftIndex;
}

void VerticalLine::setRight(int rightIndex)
{
    this->rightIndex = rightIndex;
}

float VerticalLine::getRight()
{
    return rightIndex;
}

void VerticalLine::moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<VerticalLine> verticalLines[])
{
    float leftLimit;
    float rightLimit;

    int index = leftIndex + 1;
    leftLimit = (index + 1) * margin;
    rightLimit = 1 - (lineNum - index) * margin;

    
    if (newXPercent < leftLimit) newXPercent = leftLimit;
    if (newXPercent > rightLimit) newXPercent = rightLimit;
    if (leftIndex >= 0 && newXPercent - verticalLines[leftIndex]->getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        verticalLines[leftIndex]->moveToX(lineNum, newXPercent - margin, margin, verticalLines);
//        verticalLines[leftIndex]->setXPercent(newXPercent - margin);
    }
    if (rightIndex < lineNum && verticalLines[rightIndex]->getXPercent() - newXPercent - margin < -0.00001f)
    {
        verticalLines[rightIndex]->moveToX(lineNum, newXPercent + margin, margin, verticalLines);
//        verticalLines[rightIndex]->setXPercent(newXPercent + margin);
    }
    xPercent = newXPercent;

}
