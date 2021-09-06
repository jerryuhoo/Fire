/*
  ==============================================================================

    VerticalLine.cpp
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "VerticalLine.h"
#include "Multiband.h"

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
    
    g.fillRect(getWidth() / 10.f * 4.f, 0.f, getWidth() / 10.f * 2.f, static_cast<float> (getHeight()));

    if (isMouseOverOrDragging()) {
        g.setColour(COLOUR1.withAlpha(0.2f));
        g.fillAll();
    }
    
    if (isEntered)
    {
        //g.setColour(COLOUR1.withAlpha(0.2f));
        //g.fillAll();
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

void VerticalLine::mouseDown(const juce::MouseEvent &e)
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

void VerticalLine::setMoving(bool move)
{
    this->move = move;
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
        // reset
        setVisible(false);
        xPercent = 0;
        Multiband::lineDeleted();
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

void VerticalLine::setIndex(int index)
{
    this->index = index;
}

int VerticalLine::getIndex()
{
    return index;
}

void VerticalLine::setLeft(int leftIndex)
{
    this->leftIndex = leftIndex;
}

int VerticalLine::getLeft()
{
    return leftIndex;
}

void VerticalLine::setRight(int rightIndex)
{
    this->rightIndex = rightIndex;
}

int VerticalLine::getRight()
{
    return rightIndex;
}

void VerticalLine::moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<VerticalLine> verticalLines[], int sortedIndex[])
{
    float leftLimit;
    float rightLimit;

//    int index = leftIndex + 1;
    leftLimit = (index + 1) * margin;
    rightLimit = 1 - (lineNum - index) * margin;

    if (newXPercent < leftLimit) newXPercent = leftLimit;
    if (newXPercent > rightLimit) newXPercent = rightLimit;

    int idx = sortedIndex[leftIndex];

    if (leftIndex >= 0 && newXPercent - verticalLines[idx]->getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        verticalLines[sortedIndex[leftIndex]]->moveToX(lineNum, newXPercent - margin, margin, verticalLines, sortedIndex);
    }
    if (rightIndex < lineNum && verticalLines[sortedIndex[rightIndex]]->getXPercent() - newXPercent - margin < -0.00001f)
    {
        verticalLines[sortedIndex[rightIndex]]->moveToX(lineNum, newXPercent + margin, margin, verticalLines, sortedIndex);
    }
    xPercent = newXPercent;
}
