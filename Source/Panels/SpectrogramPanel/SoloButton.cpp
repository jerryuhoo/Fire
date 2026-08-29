/*
  ==============================================================================

    SoloButton.cpp
    Created: 3 Dec 2020 8:18:19pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "SoloButton.h"

namespace
{
bool isPrimaryPointerDown(const juce::MouseEvent& event) noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isPopupMenu()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown();
}

bool isVisibleInHierarchy(const juce::Component& component) noexcept
{
    for (auto* current = &component;
         current != nullptr;
         current = current->getParentComponent())
        if (! current->isVisible())
            return false;

    return true;
}
} // namespace

//==============================================================================
SoloButton::SoloButton()
{
}

SoloButton::~SoloButton()
{
}

void SoloButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.75f);
    const bool active = getToggleState();
    const auto accent = getColour();

    juce::ColourGradient metal(fire::ui::colours::raised.brighter(isEntered ? 0.10f : 0.03f),
                               bounds.getCentreX(), bounds.getY(),
                               fire::ui::colours::surface0, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(metal);
    g.fillEllipse(bounds);
    g.setColour((active ? accent : fire::ui::colours::hairline)
                    .withAlpha(active ? 0.88f : (isEntered ? 0.78f : 0.58f)));
    g.drawEllipse(bounds, 1.0f);

    g.setFont(fire::ui::labelFont(juce::jlimit(8.0f, 13.0f, bounds.getHeight() * 0.52f)));
    g.setColour(active ? fire::ui::colours::whiteHot
                       : fire::ui::colours::textMuted.brighter(isEntered ? 0.18f : 0.0f));
    g.drawText("S", bounds, juce::Justification::centred);
}

void SoloButton::resized()
{
}

void SoloButton::mouseEnter(const juce::MouseEvent& e)
{
    if (primaryPointerDown && ! isPointerSource(e))
        return;

    const juce::Component::SafePointer<SoloButton> safeThis(this);
    juce::ToggleButton::mouseEnter(e);

    if (safeThis == nullptr)
        return;

    isEntered = true;
    repaint();
    recoverMissingPointerUp(e);
}

void SoloButton::mouseMove(const juce::MouseEvent& e)
{
    if (primaryPointerDown && ! isPointerSource(e))
        return;

    const juce::Component::SafePointer<SoloButton> safeThis(this);
    juce::ToggleButton::mouseMove(e);

    if (safeThis != nullptr)
        recoverMissingPointerUp(e);
}

void SoloButton::mouseExit(const juce::MouseEvent& e)
{
    if (primaryPointerDown && ! isPointerSource(e))
        return;

    const juce::Component::SafePointer<SoloButton> safeThis(this);
    juce::ToggleButton::mouseExit(e);

    if (safeThis == nullptr)
        return;

    isEntered = false;
    repaint();
    recoverMissingPointerUp(e);
}

void SoloButton::mouseDown(const juce::MouseEvent& event)
{
    if (primaryPointerDown && ! isPointerSource(event))
        return;

    // A missing mouseUp (for example, while a host hides the editor) must not
    // let a later secondary-button event complete an old toggle gesture.
    const juce::Component::SafePointer<SoloButton> safeThis(this);
    dismissPointerGesture();

    if (safeThis == nullptr)
        return;

    primaryPointerDown = isPrimaryPointerDown(event);

    if (primaryPointerDown)
    {
        pointerSourceType = event.source.getType();
        pointerSourceIndex = event.source.getIndex();
        juce::ToggleButton::mouseDown(event);
    }
}

void SoloButton::mouseDrag(const juce::MouseEvent& event)
{
    if (primaryPointerDown && isPointerSource(event))
        juce::ToggleButton::mouseDrag(event);
}

void SoloButton::mouseUp(const juce::MouseEvent& event)
{
    if (! primaryPointerDown)
    {
        dismissPointerGesture();
        return;
    }

    if (! isPointerSource(event))
        return;

    primaryPointerDown = false;
    pointerSourceIndex = -1;
    juce::ToggleButton::mouseUp(event);
}

bool SoloButton::keyPressed(const juce::KeyPress& key)
{
    if (key.isKeyCode(juce::KeyPress::returnKey)
        || key.isKeyCode(juce::KeyPress::spaceKey))
    {
        if (! isEnabled() || ! isVisibleInHierarchy(*this))
            return false;

        // Button::keyPressed queues triggerClick(). Commit while this visible
        // band still owns the request, because topology changes can hide the
        // same fixed-index button before the message queue is serviced.
        internalClickCallback(key.getModifiers());
        return true;
    }

    return juce::ToggleButton::keyPressed(key);
}

void SoloButton::triggerClick()
{
    if (! isEnabled() || ! isVisibleInHierarchy(*this))
        return;

    // Accessibility press actions also arrive through triggerClick(). Commit
    // at invocation time instead of replaying the request after a topology
    // change. The callback may delete this button, so it is final.
    internalClickCallback(juce::ModifierKeys::currentModifiers);
}

void SoloButton::visibilityChanged()
{
    const juce::Component::SafePointer<SoloButton> safeThis(this);
    juce::ToggleButton::visibilityChanged();

    if (safeThis != nullptr && ! isVisible())
        dismissPointerGesture();
}

void SoloButton::enablementChanged()
{
    const juce::Component::SafePointer<SoloButton> safeThis(this);
    juce::ToggleButton::enablementChanged();

    if (safeThis != nullptr && ! isEnabled())
        dismissPointerGesture();
}

void SoloButton::dismissPointerGesture() noexcept
{
    primaryPointerDown = false;
    pointerSourceIndex = -1;

    if (isDown())
        setState(juce::Button::buttonNormal);
}

void SoloButton::recoverMissingPointerUp(const juce::MouseEvent& event)
{
    if (primaryPointerDown
        && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
        dismissPointerGesture();
}

bool SoloButton::isPointerSource(const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

juce::Colour SoloButton::getColour()
{
    if (isEntered)
    {
        if (! getToggleState())
        {
            return fire::ui::colours::disabled.brighter(0.10f);
        }
        else
        {
            return fire::ui::colours::gold.brighter(0.08f);
        }
    }
    else
    {
        if (! getToggleState())
        {
            return fire::ui::colours::disabled;
        }
        else
        {
            return fire::ui::colours::gold;
        }
    }
}
