/*
  ==============================================================================

    VerticalLine.cpp
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "VerticalLine.h"
#include "Multiband.h"

namespace
{
bool isPrimaryPointerDown(const juce::MouseEvent& event) noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isPopupMenu()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown();
}
} // namespace

//==============================================================================
VerticalLine::VerticalLine()
{
    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);
}

VerticalLine::~VerticalLine()
{
    if (parameterGestureDepth > 0)
    {
        parameterGestureDepth = 0;
        if (parameterGestureEnd)
            parameterGestureEnd();
    }
}

void VerticalLine::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto hover = juce::jlimit(0.0f, 1.0f, hoverAnimation.current);
    const auto press = juce::jlimit(0.0f, 1.0f, pressAnimation.current);
    const auto emphasis = juce::jlimit(0.0f, 1.0f, hover + press * 0.35f);
    const float physicalScale = juce::jmax(1.0f,
        g.getInternalContext().getPhysicalPixelScaleFactor());
    const float centreX = fire::ui::pixelAligned(bounds.getCentreX(), physicalScale);
    const float lineWidth = (1.0f + emphasis * 1.25f + press * 0.35f) / physicalScale;

    if (emphasis > 0.001f)
    {
        const auto glowWidth = (5.0f + press * 3.0f) / physicalScale;
        g.setColour(fire::ui::colours::flame.withAlpha(0.10f * emphasis));
        g.fillRoundedRectangle(centreX - glowWidth * 0.5f,
                               bounds.getY(),
                               glowWidth,
                               bounds.getHeight(),
                               glowWidth * 0.5f);
    }

    g.setColour(fire::ui::colours::flame.withAlpha(0.46f + 0.50f * emphasis));
    g.fillRect(centreX - lineWidth * 0.5f,
               bounds.getY(),
               lineWidth,
               bounds.getHeight());

    const float handleRadius = 2.6f + 1.1f * hover + 0.55f * press;
    const juce::Rectangle<float> handle(centreX - handleRadius,
                                         bounds.getY() + 3.0f,
                                         handleRadius * 2.0f,
                                         handleRadius * 2.0f);
    if (emphasis > 0.001f)
    {
        const auto halo = handle.expanded((1.5f + press) / physicalScale);
        g.setColour(fire::ui::colours::flame.withAlpha(0.12f * emphasis));
        g.fillEllipse(halo);
    }
    g.setColour(fire::ui::colours::surface1.withAlpha(0.96f));
    g.fillEllipse(handle);
    g.setColour(fire::ui::colours::flame.withAlpha(0.72f + 0.28f * emphasis));
    g.drawEllipse(handle.reduced(0.5f / physicalScale), lineWidth);
}

void VerticalLine::resized()
{
}

void VerticalLine::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (! primaryDragActive)
        return;

    primaryDragActive = false;
    endParameterGesture();
    updateAnimationTargets();
}

void VerticalLine::mouseDoubleClick (const juce::MouseEvent& e)
{
    // do nothing, override the silder function, which will reset value.
}

void VerticalLine::mouseEnter(const juce::MouseEvent& e)
{
    juce::Slider::mouseEnter(e);
    isEntered = true;
    updateAnimationTargets();
    repaint();
}

void VerticalLine::mouseExit(const juce::MouseEvent& e)
{
    juce::Slider::mouseExit(e);
    isEntered = false;
    updateAnimationTargets();
    repaint();
}

void VerticalLine::mouseDrag (const juce::MouseEvent& e)
{
    // this will call multiband mouseDrag
}

void VerticalLine::mouseDown (const juce::MouseEvent& e)
{
    if (! isPrimaryPointerDown(e))
        return;

    primaryDragActive = true;
    beginParameterGesture();
    updateAnimationTargets();
}

bool VerticalLine::advanceAnimation(float deltaSeconds) noexcept
{
    const auto hoverChanged = hoverAnimation.advance(deltaSeconds, 0.10f);
    const auto pressChanged = pressAnimation.advance(deltaSeconds, 0.065f);
    return hoverChanged || pressChanged;
}

void VerticalLine::dismissTransientInteraction()
{
    isEntered = false;
    primaryDragActive = false;

    // Hosts may keep an editor object alive after hiding its window. If that
    // happens during a drag, no later mouseUp is guaranteed, so close the
    // parameter gesture here just as the destructor would.
    if (parameterGestureDepth > 0)
    {
        parameterGestureDepth = 0;
        if (parameterGestureEnd)
            parameterGestureEnd();
    }

    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);
}

void VerticalLine::updateAnimationTargets() noexcept
{
    const bool isPressed = parameterGestureDepth > 0;
    hoverAnimation.setTarget(isEntered || isPressed ? 1.0f : 0.0f);
    pressAnimation.setTarget(isPressed ? 1.0f : 0.0f);
}

void VerticalLine::setParameterGestureCallbacks(ParameterGestureCallback gestureBegin,
                                                ParameterGestureCallback change,
                                                ParameterGestureCallback gestureEnd)
{
    if (parameterGestureDepth > 0)
    {
        parameterGestureDepth = 0;
        if (parameterGestureEnd)
            parameterGestureEnd();
    }

    parameterGestureBegin = std::move(gestureBegin);
    parameterChange = std::move(change);
    parameterGestureEnd = std::move(gestureEnd);
}

void VerticalLine::beginParameterGesture()
{
    if (parameterGestureDepth++ == 0 && parameterGestureBegin)
        parameterGestureBegin();
}

void VerticalLine::endParameterGesture()
{
    if (parameterGestureDepth <= 0)
        return;

    if (--parameterGestureDepth == 0 && parameterGestureEnd)
        parameterGestureEnd();
}

void VerticalLine::setValueAsPartOfGesture(double newValue,
                                           juce::NotificationType notification)
{
    const double constrainedValue = getNormalisableRange().snapToLegalValue(newValue);
    if (! juce::approximatelyEqual(constrainedValue, getValue()) && parameterChange)
        parameterChange();

    setValue(newValue, notification);
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
