/*
  ==============================================================================

    EnableButton.cpp
    Created: 3 Dec 2020 8:18:45pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "EnableButton.h"

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
EnableButton::EnableButton()
{
}

EnableButton::~EnableButton()
{
}

void EnableButton::paint(juce::Graphics& g)
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
                    .withAlpha(active ? 0.84f : (isEntered ? 0.78f : 0.58f)));
    g.drawEllipse(bounds, 1.0f);

    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto lineW = juce::jmax(1.0f, radius * 0.13f);
    const auto arcRadius = radius * 0.48f;

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius,
                                0.0f,
                                juce::MathConstants<float>::twoPi * 0.12f,
                                juce::MathConstants<float>::twoPi * 0.88f,
                                true);

    g.setColour(accent.withAlpha(active ? 0.95f : 0.40f));
    g.strokePath(backgroundArc,
                 juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    g.drawLine(bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.22f,
               bounds.getCentreX(), bounds.getCentreY(), lineW);
}

void EnableButton::resized()
{
}

void EnableButton::mouseEnter(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseEnter(e);
    isEntered = true;
    repaint();
    recoverMissingPointerUp(e);
}

void EnableButton::mouseMove(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseMove(e);
    recoverMissingPointerUp(e);
}

void EnableButton::mouseExit(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseExit(e);
    isEntered = false;
    repaint();
    recoverMissingPointerUp(e);
}

void EnableButton::mouseDown(const juce::MouseEvent& event)
{
    if (primaryPointerDown && ! isPointerSource(event))
        return;

    // A missing mouseUp (for example, while a host hides the editor) must not
    // let a later secondary-button event complete an old toggle gesture.
    dismissPointerGesture();
    primaryPointerDown = isPrimaryPointerDown(event);

    if (primaryPointerDown)
    {
        pointerSourceType = event.source.getType();
        pointerSourceIndex = event.source.getIndex();
        juce::ToggleButton::mouseDown(event);
    }
}

void EnableButton::mouseDrag(const juce::MouseEvent& event)
{
    if (primaryPointerDown && isPointerSource(event))
        juce::ToggleButton::mouseDrag(event);
}

void EnableButton::mouseUp(const juce::MouseEvent& event)
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

void EnableButton::visibilityChanged()
{
    juce::ToggleButton::visibilityChanged();

    if (! isVisible())
        dismissPointerGesture();
}

void EnableButton::enablementChanged()
{
    juce::ToggleButton::enablementChanged();

    if (! isEnabled())
        dismissPointerGesture();
}

void EnableButton::dismissPointerGesture() noexcept
{
    primaryPointerDown = false;
    pointerSourceIndex = -1;

    if (isDown())
        setState(juce::Button::buttonNormal);
}

void EnableButton::recoverMissingPointerUp(const juce::MouseEvent& event)
{
    if (primaryPointerDown
        && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
        dismissPointerGesture();
}

bool EnableButton::isPointerSource(const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

juce::Colour EnableButton::getColour()
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
