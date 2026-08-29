/*
  ==============================================================================

    DraggableButton.cpp
    Created: 7 Oct 2021 11:42:21pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "DraggableButton.h"

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
DraggableButton::DraggableButton()
{
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

DraggableButton::~DraggableButton()
{
}

void DraggableButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const auto accent = getColour();

    if (mState)
    {
        g.setColour(accent.withAlpha(isEntered ? 0.20f : 0.11f));
        g.fillEllipse(bounds.expanded(isEntered ? 0.0f : -0.5f));
    }

    juce::ColourGradient metal(fire::ui::colours::raised.brighter(isEntered ? 0.10f : 0.04f),
                               bounds.getCentreX(), bounds.getY(),
                               fire::ui::colours::surface0, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(metal);
    g.fillEllipse(bounds.reduced(bounds.getWidth() * 0.16f));

    g.setColour(accent.withAlpha(mState ? (isEntered ? 0.95f : 0.76f) : 0.38f));
    g.drawEllipse(bounds.reduced(bounds.getWidth() * 0.16f), 1.0f);

    const auto core = bounds.reduced(bounds.getWidth() * (isEntered ? 0.34f : 0.38f));
    g.setColour(mState ? fire::ui::colours::whiteHot : fire::ui::colours::disabled);
    g.fillEllipse(core);
}

void DraggableButton::resized()
{
}

void DraggableButton::mouseEnter(const juce::MouseEvent& e)
{
    juce::Component::mouseEnter(e);
    isEntered = true;
    repaint();

    // If the owner re-enters without its primary button, the preceding up was
    // lost while the host or window manager changed pointer capture.
    recoverMissingPointerUp(e);
}

void DraggableButton::mouseMove(const juce::MouseEvent& e)
{
    juce::Component::mouseMove(e);
    recoverMissingPointerUp(e);
}

void DraggableButton::mouseExit(const juce::MouseEvent& e)
{
    juce::Component::mouseExit(e);
    isEntered = false;
    repaint();

    recoverMissingPointerUp(e);
}

juce::Colour DraggableButton::getColour()
{
    if (mState && isEntered)
        return fire::ui::colours::filter.brighter(0.12f);
    else if (mState && ! isEntered)
        return fire::ui::colours::filter;

    return fire::ui::colours::disabled;
}

void DraggableButton::setState(const bool state)
{
    if (mState == state)
        return;

    mState = state;
    repaint();

    // Ending an active drag can synchronously remove this component through a
    // host parameter callback, so it must be the final access in this path.
    if (! state)
        dismissTransientInteraction();
}

void DraggableButton::mouseDown(const juce::MouseEvent& event)
{
    juce::Component::mouseDown(event);

    juce::Component::SafePointer<DraggableButton> safeThis(this);
    if (primaryDragActive)
    {
        // Interleaved touch or pen streams cannot steal an active filter drag.
        if (! isPointerSource(event))
            return;

        // A fresh down from the owner is a lifecycle boundary when a host
        // omitted the previous mouseUp.
        dismissTransientInteraction();

        if (safeThis == nullptr)
            return;
    }

    if (! mState || ! onDrag || ! isPrimaryPointerDown(event))
        return;

    primaryDragActive = true;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();
    auto dragCallback = onDrag;

    // Keep the callable alive if it removes its owning component. No member is
    // accessed after the callback begins.
    dragCallback(*this, event);
}

void DraggableButton::mouseDrag(const juce::MouseEvent& event)
{
    juce::Component::mouseDrag(event);

    if (primaryDragActive && isPointerSource(event) && mState && onDrag)
    {
        auto dragCallback = onDrag;
        dragCallback(*this, event);
    }
}

void DraggableButton::mouseUp(const juce::MouseEvent& event)
{
    juce::Component::mouseUp(event);

    if (! primaryDragActive || ! isPointerSource(event))
        return;

    dismissTransientInteraction();
}

void DraggableButton::dismissTransientInteraction()
{
    if (! primaryDragActive)
        return;

    primaryDragActive = false;
    pointerSourceIndex = -1;
    if (onDragFinished)
    {
        auto finishedCallback = onDragFinished;
        finishedCallback();
    }
}

bool DraggableButton::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void DraggableButton::recoverMissingPointerUp(
    const juce::MouseEvent& event)
{
    if (! primaryDragActive
        || ! isPointerSource(event)
        || event.mods.isLeftButtonDown())
        return;

    // The gesture-end callback may synchronously remove this button, so it is
    // deliberately the final operation in this path.
    dismissTransientInteraction();
}

void DraggableButton::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);

    if (mState && onQValueChanged)
    {
        auto qValueCallback = onQValueChanged;
        qValueCallback(wheel.deltaY);
    }
}
