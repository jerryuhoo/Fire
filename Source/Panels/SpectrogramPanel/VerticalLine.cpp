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
    //    boundsConstrainer.setMinimumHeight(0);
}

VerticalLine::~VerticalLine()
{
}

void VerticalLine::paint (juce::Graphics& g)
{
    // draw line that will be added next
    g.setColour (COLOUR1);

    g.fillRect (getWidth() / 10.f * 4.f, 0.f, getWidth() / 10.f * 2.f, static_cast<float> (getHeight()));

    if (isMouseOverOrDragging())
    {
        g.setColour (COLOUR1.withAlpha (0.2f));
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
}

void VerticalLine::mouseUp (const juce::MouseEvent& e)
{
    //    move = false;
}

void VerticalLine::mouseDoubleClick (const juce::MouseEvent& e)
{
    // do nothing, override the silder function, which will reset value.
}

void VerticalLine::mouseEnter (const juce::MouseEvent& e)
{
    isEntered = true;
}

void VerticalLine::mouseExit (const juce::MouseEvent& e)
{
    isEntered = false;
}

void VerticalLine::mouseDrag (const juce::MouseEvent& e)
{
    // this will call multiband mouseDrag
}

void VerticalLine::mouseDown (const juce::MouseEvent& e)
{
    // call parent mousedown(FreqDividerGroup)
    //    getParentComponent()->mouseDown(e.getEventRelativeTo(getParentComponent()));
    //    dragger.startDraggingComponent (this, e);
}

void VerticalLine::setDeleteState (bool deleteState)
{
    mDeleteState = deleteState;
}

void VerticalLine::setXPercent (float x)
{
    xPercent = x;
}

float VerticalLine::getXPercent()
{
    return xPercent;
}

void VerticalLine::setIndex (int index)
{
    this->index = index;
}

int VerticalLine::getIndex()
{
    return index;
}

int VerticalLine::getLeft()
{
    return index - 1;
}

int VerticalLine::getRight()
{
    return index + 1;
}

void VerticalLine::moveToX (int lineNum, float newXPercent, float margin, std::unique_ptr<VerticalLine> verticalLines[])
{
    float leftLimit;
    float rightLimit;

    //    int index = leftIndex + 1;
    leftLimit = (index + 1) * margin;
    rightLimit = 1 - (lineNum - index) * margin;

    if (newXPercent < leftLimit)
        newXPercent = leftLimit;
    if (newXPercent > rightLimit)
        newXPercent = rightLimit;

    if (leftIndex >= 0 && newXPercent - verticalLines[leftIndex]->getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        verticalLines[leftIndex]->moveToX (lineNum, newXPercent - margin, margin, verticalLines);
    }
    if (rightIndex < lineNum && verticalLines[rightIndex]->getXPercent() - newXPercent - margin < -0.00001f)
    {
        verticalLines[rightIndex]->moveToX (lineNum, newXPercent + margin, margin, verticalLines);
    }
    xPercent = newXPercent;
}
