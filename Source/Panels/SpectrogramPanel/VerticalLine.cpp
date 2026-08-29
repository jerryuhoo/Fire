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
    auto endCallback = parameterGestureDepth > 0
                         ? std::move(parameterGestureEnd)
                         : ParameterGestureCallback {};
    parameterGestureDepth = 0;
    parameterGestureBegin = nullptr;
    parameterChange = nullptr;
    parameterGestureEnd = nullptr;

    // This callback can synchronously destroy the owning editor. Keep it as
    // the final operation that depends on the component's lifetime.
    if (endCallback)
        endCallback();
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
    updateAnimationTargets();

    // The matching host notification may synchronously delete this slider.
    endParameterGesture();
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
    updateAnimationTargets();

    // The callback is deliberately last so a synchronous owner teardown does
    // not leave a continuation that accesses this component.
    beginParameterGesture();
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
    auto endCallback = parameterGestureDepth > 0
                         ? parameterGestureEnd
                         : ParameterGestureCallback {};
    parameterGestureDepth = 0;

    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);

    // Ending a host gesture can synchronously close the editor. Nothing below
    // this call may depend on the VerticalLine still existing.
    if (endCallback)
        endCallback();
}

void VerticalLine::updateAnimationTargets() noexcept
{
    const bool isPressed = parameterGestureDepth > 0;
    hoverAnimation.setTarget(isEntered || isPressed ? 1.0f : 0.0f);
    pressAnimation.setTarget(isPressed ? 1.0f : 0.0f);
}

void VerticalLine::setParameterGestureCallbacks(ParameterGestureCallback gestureBegin,
                                                ParameterChangeCallback change,
                                                ParameterGestureCallback gestureEnd)
{
    auto previousEnd = parameterGestureDepth > 0
                         ? parameterGestureEnd
                         : ParameterGestureCallback {};
    parameterGestureDepth = 0;
    primaryDragActive = false;
    updateAnimationTargets();

    parameterGestureBegin = std::move(gestureBegin);
    parameterChange = std::move(change);
    parameterGestureEnd = std::move(gestureEnd);

    // Install the complete replacement before closing the previous gesture.
    // The old callback is allowed to synchronously destroy this component.
    if (previousEnd)
        previousEnd();
}

void VerticalLine::beginParameterGesture()
{
    const bool shouldNotify = parameterGestureDepth++ == 0;
    updateAnimationTargets();
    auto beginCallback = shouldNotify ? parameterGestureBegin
                                      : ParameterGestureCallback {};

    if (beginCallback)
        beginCallback();
}

void VerticalLine::endParameterGesture()
{
    if (parameterGestureDepth <= 0)
        return;

    const bool shouldNotify = --parameterGestureDepth == 0;
    updateAnimationTargets();
    auto endCallback = shouldNotify ? parameterGestureEnd
                                    : ParameterGestureCallback {};

    if (endCallback)
        endCallback();
}

void VerticalLine::setValueAsPartOfGesture(double newValue,
                                           juce::NotificationType notification)
{
    const double constrainedValue = getNormalisableRange().snapToLegalValue(newValue);
    if (! juce::approximatelyEqual(constrainedValue, getValue()) && parameterChange)
    {
        auto changeCallback = parameterChange;
        juce::Component::SafePointer<VerticalLine> safeThis(this);
        auto gestureToken = changeCallback();

        if (safeThis == nullptr || gestureToken == nullptr)
            return;

        // Keep the processor-owned gesture session alive until every
        // synchronous value callback has returned. setValue is intentionally
        // the final component access in this branch.
        setValue(newValue, notification);
        return;
    }

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
