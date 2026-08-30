/*
  ==============================================================================

    CloseButton.cpp
    Created: 8 Nov 2020 7:57:32pm
    Author:  羽翼深蓝Wings

 ==============================================================================
*/

#include "CloseButton.h"

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
CloseButton::CloseButton()
    : juce::Button("Delete band")
{
    setTitle("Delete band");
    setDescription("Remove this frequency band");
    setHelpText("Deletes this band and keeps the remaining crossover bands in order.");
    setTooltip("Delete band");
    setAccessible(true);
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    visibilityAnimation.snapTo(0.0f);
    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);
    focusAnimation.snapTo(0.0f);
    setInterceptsMouseClicks(false, false);
    juce::Component::setVisible(false);
}

void CloseButton::paintButton(juce::Graphics& g, bool, bool)
{
    const auto visibility = juce::jlimit(0.0f, 1.0f, visibilityAnimation.current);
    if (visibility <= 0.001f || getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto hover = juce::jlimit(0.0f, 1.0f, hoverAnimation.current);
    const auto press = juce::jlimit(0.0f, 1.0f, pressAnimation.current);
    const auto focus = juce::jlimit(0.0f, 1.0f, focusAnimation.current);
    const auto shortestSide = static_cast<float>(juce::jmin(getWidth(), getHeight()));
    if (shortestSide < 4.0f)
        return;

    const auto visualSide = juce::jlimit(2.0f, 20.0f, shortestSide * 0.75f);
    const auto pressScale = 1.0f - 0.045f * press;
    auto surface = juce::Rectangle<float>(visualSide * pressScale,
                                          visualSide * pressScale)
                       .withCentre(getLocalBounds().toFloat().getCentre());
    const auto radius = juce::jmin(fire::ui::Metrics::radiusSmall,
                                   surface.getWidth() * 0.23f);
    const auto physicalScale = juce::jmax(
        1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

    // The old control was a conspicuous outlined circle. This compact raised
    // tile keeps the destructive affordance clear without competing with the
    // spectrum and crossover rails.
    g.setColour(juce::Colours::black.withAlpha(
        visibility * (0.10f + 0.08f * hover)));
    g.fillRoundedRectangle(surface.translated(0.0f, 0.75f / physicalScale),
                           radius);

    const auto surfaceColour = fire::ui::colours::surface2.interpolatedWith(
        fire::ui::colours::danger, 0.06f + 0.10f * hover + 0.05f * press);
    g.setColour(surfaceColour.withAlpha(
        visibility * (0.54f + 0.26f * hover + 0.10f * press)));
    g.fillRoundedRectangle(surface, radius);

    const auto edgeEmphasis = juce::jlimit(0.0f, 1.0f,
                                           0.15f + hover * 0.70f + focus * 0.50f);
    g.setColour(fire::ui::colours::danger.withAlpha(
        visibility * (0.12f + 0.43f * edgeEmphasis)));
    g.drawRoundedRectangle(surface.reduced(0.5f / physicalScale),
                           radius,
                           1.0f / physicalScale);

    if (focus > 0.0f)
    {
        g.setColour(fire::ui::colours::gold.withAlpha(visibility * 0.58f));
        g.drawRoundedRectangle(surface.expanded(1.5f / physicalScale),
                               radius + 1.0f / physicalScale,
                               1.0f / physicalScale);
    }

    const auto halfCross = surface.getWidth() * (0.185f + 0.045f * hover);
    const auto centre = surface.getCentre();
    juce::Path cross;
    cross.startNewSubPath(centre.x - halfCross, centre.y - halfCross);
    cross.lineTo(centre.x + halfCross, centre.y + halfCross);
    cross.startNewSubPath(centre.x + halfCross, centre.y - halfCross);
    cross.lineTo(centre.x - halfCross, centre.y + halfCross);

    const auto iconColour = fire::ui::colours::textSecondary.interpolatedWith(
        fire::ui::colours::danger, 0.34f + 0.58f * hover);
    g.setColour(iconColour.withAlpha(
        visibility * (0.74f + 0.24f * hover)));
    g.strokePath(cross,
                 juce::PathStrokeType(juce::jlimit(1.1f,
                                                   1.8f,
                                                   surface.getWidth() * 0.085f),
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
}

void CloseButton::setPresented(bool shouldBePresented, bool animate)
{
    const juce::Component::SafePointer<CloseButton> safeThis(this);
    presentationTarget = shouldBePresented;
    visibilityAnimation.setTarget(shouldBePresented ? 1.0f : 0.0f);

    if (! animate)
    {
        visibilityAnimation.snapTo(shouldBePresented ? 1.0f : 0.0f);
        hoverAnimation.snapTo(0.0f);
        pressAnimation.snapTo(0.0f);
        focusAnimation.snapTo(0.0f);
    }

    if (shouldBePresented)
    {
        if (! isVisible())
        {
            juce::Component::setVisible(true);

            if (safeThis == nullptr)
                return;
        }

        setInterceptsMouseClicks(true, false);
    }
    else
    {
        // A fading control must not steal the next click from the newly
        // hovered band or an overlapping divider. It must also relinquish
        // keyboard focus immediately: the tile remains visible during its
        // fade, but Space/Return must no longer be able to delete a band.
        setInterceptsMouseClicks(false, false);
        keyboardFocusVisible = false;
        dismissPointerGesture();

        if (safeThis == nullptr)
            return;

        if (hasKeyboardFocus(true))
        {
            giveAwayKeyboardFocus();

            if (safeThis == nullptr)
                return;
        }

        if (! animate || visibilityAnimation.current <= 0.001f)
        {
            juce::Component::setVisible(false);

            if (safeThis == nullptr)
                return;
        }
    }

    updateInteractionTargets();
    repaint();
}

bool CloseButton::advanceAnimation(float deltaSeconds)
{
    const auto visibilityChanged = visibilityAnimation.advance(deltaSeconds, 0.11f);
    const auto hoverChanged = hoverAnimation.advance(deltaSeconds, 0.10f);
    const auto pressChanged = pressAnimation.advance(deltaSeconds, 0.065f);
    const auto focusChanged = focusAnimation.advance(deltaSeconds, 0.11f);

    if (! presentationTarget
        && visibilityAnimation.isSettled(0.001f, 0.01f)
        && visibilityAnimation.current <= 0.001f
        && isVisible())
    {
        visibilityAnimation.snapTo(0.0f);
        const juce::Component::SafePointer<CloseButton> safeThis(this);
        juce::Component::setVisible(false);

        if (safeThis == nullptr)
            return true;
    }

    return visibilityChanged || hoverChanged || pressChanged || focusChanged;
}

void CloseButton::mouseEnter(const juce::MouseEvent& event)
{
    if (primaryPointerDown && ! isPointerSource(event))
        return;

    const juce::Component::SafePointer<CloseButton> safeThis(this);
    juce::Button::mouseEnter(event);

    if (safeThis != nullptr)
        recoverMissingPointerUp(event);
}

void CloseButton::mouseMove(const juce::MouseEvent& event)
{
    if (primaryPointerDown && ! isPointerSource(event))
        return;

    const juce::Component::SafePointer<CloseButton> safeThis(this);
    juce::Button::mouseMove(event);

    if (safeThis != nullptr)
        recoverMissingPointerUp(event);
}

void CloseButton::mouseExit(const juce::MouseEvent& event)
{
    if (primaryPointerDown && ! isPointerSource(event))
        return;

    const juce::Component::SafePointer<CloseButton> safeThis(this);
    juce::Button::mouseExit(event);

    if (safeThis != nullptr)
        recoverMissingPointerUp(event);
}

void CloseButton::mouseDown(const juce::MouseEvent& event)
{
    if (primaryPointerDown)
    {
        if (! isPointerSource(event))
            return;

        // A fresh down from the owner closes a gesture whose mouseUp was lost.
        const juce::Component::SafePointer<CloseButton> safeThis(this);
        dismissPointerGesture();

        if (safeThis == nullptr)
            return;
    }

    // Pointer focus is intentionally not rendered as a persistent keyboard
    // outline. This also handles a click while the tile already owns focus.
    keyboardFocusVisible = false;
    updateInteractionTargets();

    // JUCE Button accepts every mouse button by default. A secondary click on
    // a destructive affordance must never delete a band; on macOS this also
    // covers Ctrl-click through isPopupMenu().
    if (! presentationTarget || ! isEnabled() || ! isPrimaryPointerDown(event))
        return;

    primaryPointerDown = true;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();
    juce::Button::mouseDown(event);
}

void CloseButton::mouseDrag(const juce::MouseEvent& event)
{
    if (primaryPointerDown && isPointerSource(event))
        juce::Button::mouseDrag(event);
}

void CloseButton::mouseUp(const juce::MouseEvent& event)
{
    if (! primaryPointerDown || ! isPointerSource(event))
        return;

    primaryPointerDown = false;
    pointerSourceIndex = -1;

    // Releasing over the tile can invoke onClick and synchronously remove it,
    // so the JUCE dispatch must remain the final operation in this path.
    juce::Button::mouseUp(event);
}

bool CloseButton::keyPressed(const juce::KeyPress& key)
{
    if (hasKeyboardFocus(true) && ! keyboardFocusVisible)
    {
        keyboardFocusVisible = true;
        updateInteractionTargets();
    }

    if (key.isKeyCode(juce::KeyPress::returnKey)
        || key.isKeyCode(juce::KeyPress::spaceKey))
    {
        if (! presentationTarget
            || ! fire::ui::canActivateButton(*this))
            return false;

        // Button::keyPressed queues triggerClick(). Delete the band while this
        // transient tile still identifies it; moving the pointer can present a
        // different tile before a queued message is delivered.
        internalClickCallback(key.getModifiers());
        return true;
    }

    return juce::Button::keyPressed(key);
}

void CloseButton::triggerClick()
{
    if (! presentationTarget
        || ! fire::ui::canTriggerButtonProgrammatically(*this))
        return;

    // Accessibility presses are committed at invocation time rather than
    // being replayed after the hover target changes. The deletion callback
    // may destroy this button, so this is final.
    internalClickCallback(juce::ModifierKeys::currentModifiers);
}

std::unique_ptr<juce::AccessibilityHandler>
CloseButton::createAccessibilityHandler()
{
    return fire::ui::createGuardedButtonAccessibilityHandler(
        *this, juce::AccessibilityRole::button);
}

void CloseButton::visibilityChanged()
{
    const juce::Component::SafePointer<CloseButton> safeThis(this);
    juce::Button::visibilityChanged();

    if (safeThis == nullptr)
        return;

    if (! isVisible())
    {
        keyboardFocusVisible = false;
        focusAnimation.snapTo(0.0f);
        dismissPointerGesture();
    }

    if (safeThis != nullptr)
        updateInteractionTargets();
}

void CloseButton::enablementChanged()
{
    const juce::Component::SafePointer<CloseButton> safeThis(this);
    juce::Button::enablementChanged();

    if (safeThis == nullptr)
        return;

    if (! isEnabled())
    {
        keyboardFocusVisible = false;
        dismissPointerGesture();
    }

    if (safeThis != nullptr)
        updateInteractionTargets();
}

void CloseButton::focusGained(FocusChangeType cause)
{
    const juce::Component::SafePointer<CloseButton> safeThis(this);
    juce::Button::focusGained(cause);
    if (safeThis == nullptr)
        return;

    keyboardFocusVisible = cause != focusChangedByMouseClick;
    updateInteractionTargets();
}

void CloseButton::focusLost(FocusChangeType cause)
{
    const juce::Component::SafePointer<CloseButton> safeThis(this);
    juce::Button::focusLost(cause);
    if (safeThis == nullptr)
        return;

    keyboardFocusVisible = false;
    updateInteractionTargets();
}

void CloseButton::dismissPointerGesture() noexcept
{
    primaryPointerDown = false;
    pointerSourceIndex = -1;

    if (isDown())
        setState(juce::Button::buttonNormal);
}

void CloseButton::recoverMissingPointerUp(const juce::MouseEvent& event)
{
    if (primaryPointerDown
        && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
        dismissPointerGesture();
}

bool CloseButton::isPointerSource(const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void CloseButton::buttonStateChanged()
{
    updateInteractionTargets();
}

void CloseButton::updateInteractionTargets() noexcept
{
    const bool pressed = presentationTarget && isEnabled() && isDown();
    const bool hovered = presentationTarget && isEnabled() && isOver();
    const bool focused = presentationTarget && isEnabled()
                         && keyboardFocusVisible && hasKeyboardFocus(true);
    hoverAnimation.setTarget(hovered || pressed ? 1.0f : 0.0f);
    pressAnimation.setTarget(pressed ? 1.0f : 0.0f);
    focusAnimation.setTarget(focused ? 1.0f : 0.0f);
}
