/*
  ==============================================================================

    VerticalLine.cpp
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

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

void VerticalLine::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool engaged = isMouseOverOrDragging() || isEntered;
    const float physicalScale = juce::jmax(1.0f,
        g.getInternalContext().getPhysicalPixelScaleFactor());
    const float centreX = fire::ui::pixelAligned(bounds.getCentreX(), physicalScale);
    const float lineWidth = (engaged ? 2.0f : 1.0f) / physicalScale;

    g.setColour(fire::ui::colours::flame.withAlpha(engaged ? 0.92f : 0.58f));
    g.fillRect(centreX - lineWidth * 0.5f,
               bounds.getY(),
               lineWidth,
               bounds.getHeight());

    const float handleRadius = engaged ? 3.5f : 2.75f;
    const juce::Rectangle<float> handle(centreX - handleRadius,
                                         bounds.getY() + 3.0f,
                                         handleRadius * 2.0f,
                                         handleRadius * 2.0f);
    g.setColour(fire::ui::colours::surface1.withAlpha(0.96f));
    g.fillEllipse(handle);
    g.setColour(fire::ui::colours::flame.withAlpha(engaged ? 1.0f : 0.78f));
    g.drawEllipse(handle.reduced(0.5f / physicalScale), lineWidth);
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

void VerticalLine::mouseEnter(const juce::MouseEvent& e)
{
    juce::Slider::mouseEnter(e);
    isEntered = true;
    repaint();
}

void VerticalLine::mouseExit(const juce::MouseEvent& e)
{
    juce::Slider::mouseExit(e);
    isEntered = false;
    repaint();
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
    xPercent = std::isfinite(x) ? juce::jlimit(0.0f, 1.0f, x) : 0.0f;
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
