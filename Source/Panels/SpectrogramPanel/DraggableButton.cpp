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
}

void DraggableButton::mouseExit(const juce::MouseEvent& e)
{
    juce::Component::mouseExit(e);
    isEntered = false;
    repaint();
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

    if (! state)
        dismissTransientInteraction();

    mState = state;
    repaint();
}

void DraggableButton::mouseDown(const juce::MouseEvent& event)
{
    juce::Component::mouseDown(event);

    // A host can hide the editor before JUCE delivers mouseUp. Close that
    // stale ownership before deciding whether this new pointer is eligible.
    dismissTransientInteraction();

    if (! mState || ! onDrag || ! isPrimaryPointerDown(event))
        return;

    primaryDragActive = true;
    onDrag(*this, event);
}

void DraggableButton::mouseDrag(const juce::MouseEvent& event)
{
    juce::Component::mouseDrag(event);

    if (primaryDragActive && mState && onDrag)
        onDrag(*this, event);
}

void DraggableButton::mouseUp(const juce::MouseEvent& event)
{
    juce::Component::mouseUp(event);

    if (! primaryDragActive)
        return;

    dismissTransientInteraction();
}

void DraggableButton::dismissTransientInteraction()
{
    if (! primaryDragActive)
        return;

    primaryDragActive = false;
    if (onDragFinished)
        onDragFinished();
}

void DraggableButton::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);

    if (mState && onQValueChanged)
    {
        onQValueChanged(wheel.deltaY);
    }
}
