/*
  ==============================================================================

    PrimaryButton.h
    Created: 24 Aug 2026

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <cmath>
#include <type_traits>

struct PrimaryButtonTestAccess;

class PrimaryButtonAnimationState
{
public:
    virtual ~PrimaryButtonAnimationState() = default;
    virtual float getHoverAnimation() const noexcept = 0;
    virtual float getPressAnimation() const noexcept = 0;
    virtual float getFocusAnimation() const noexcept = 0;
    virtual float getDisabledAnimation() const noexcept = 0;
};

/** A JUCE button that accepts pointer clicks only from an owned primary gesture.

    Keyboard and accessibility/programmatic activation are synchronous so a
    command cannot be replayed after a shared control has been rebound or
    hidden. The template is shared by text, toggle, and hyperlink buttons so
    controls with different drawing implementations use the same ownership
    rules.
*/
template <typename ButtonType>
class PrimaryPointerButton : public ButtonType,
                             private juce::Timer,
                             public PrimaryButtonAnimationState
{
    static_assert(std::is_base_of_v<juce::Button, ButtonType>);

public:
    using ButtonType::ButtonType;

    ~PrimaryPointerButton() override { stopTimer(); }

    float getHoverAnimation() const noexcept override { return hoverAnimation; }
    float getPressAnimation() const noexcept override { return pressAnimation; }
    float getFocusAnimation() const noexcept override { return focusAnimation; }
    float getDisabledAnimation() const noexcept override { return disabledAnimation; }

    void triggerClick() override
    {
        if (this->isEnabled())
        {
            // Accessibility press actions route through triggerClick(). Submit
            // now so the command cannot land on a later band/LFO binding. This
            // must remain the final operation because it may delete the button.
            this->internalClickCallback(
                juce::ModifierKeys::currentModifiers);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        // Hosts can remove or hide an editor without delivering mouseUp. A
        // fresh down from the owning source starts a new gesture boundary.
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        dismissPointerGesture();

        if (safeThis == nullptr)
            return;

        const auto isPrimaryButton = event.mods.isLeftButtonDown()
                                     && ! event.mods.isRightButtonDown()
                                     && ! event.mods.isMiddleButtonDown()
                                     && ! event.mods.isPopupMenu();
        pointerGesture = isPrimaryButton ? PointerGesture::primary
                                         : PointerGesture::rejected;

        if (pointerGesture == PointerGesture::primary)
        {
            pointerSourceType = event.source.getType();
            pointerSourceIndex = event.source.getIndex();
            ButtonType::mouseDown(event);
            if (safeThis != nullptr)
                updateAnimationTargets();
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && isPointerSource(event))
        {
            const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
            ButtonType::mouseDrag(event);
            if (safeThis != nullptr)
                updateAnimationTargets();
        }
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::mouseEnter(event);

        if (safeThis != nullptr)
        {
            recoverMissingPointerUp(event);
            updateAnimationTargets();
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::mouseMove(event);

        if (safeThis != nullptr)
        {
            recoverMissingPointerUp(event);
            updateAnimationTargets();
        }
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::mouseExit(event);

        if (safeThis != nullptr)
        {
            recoverMissingPointerUp(event);
            updateAnimationTargets();
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const auto completedGesture = pointerGesture;
        pointerGesture = PointerGesture::none;
        pointerSourceIndex = -1;
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);

        if (completedGesture == PointerGesture::primary)
            ButtonType::mouseUp(event);
        else
            dismissPointerGesture();

        if (safeThis != nullptr)
            updateAnimationTargets();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const bool isActivationKey =
            key.isKeyCode(juce::KeyPress::returnKey)
            || key.isKeyCode(juce::KeyPress::spaceKey);

        if (this->isEnabled()
            && isActivationKey)
        {
            // Button::keyPressed queues triggerClick(). Invoke the normal
            // callback now so it cannot land on a later attachment target.
            // This must remain the final operation because it may delete this
            // button.
            this->internalClickCallback(key.getModifiers());
            return true;
        }

        return ButtonType::keyPressed(key);
    }

    void visibilityChanged() override
    {
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::visibilityChanged();

        if (safeThis != nullptr && ! this->isShowing())
            dismissPointerGesture();

        if (safeThis != nullptr)
            updateAnimationTargets();
    }

    void enablementChanged() override
    {
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::enablementChanged();

        if (safeThis == nullptr)
            return;

        // Re-enabling while a physical button remains held must not revive an
        // abandoned gesture through Button::updateState().
        dismissPointerGesture();
        updateAnimationTargets();
    }

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::focusGained(cause);
        if (safeThis != nullptr)
            updateAnimationTargets();
    }

    void focusLost(juce::Component::FocusChangeType cause) override
    {
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::focusLost(cause);
        if (safeThis != nullptr)
            updateAnimationTargets();
    }

    void dismissPointerGesture() noexcept
    {
        pointerGesture = PointerGesture::none;
        pointerSourceIndex = -1;

        const auto restingState = this->isEnabled()
                                      && this->isShowing()
                                      && this->isMouseOver(true)
                                  ? juce::Button::buttonOver
                                  : juce::Button::buttonNormal;
        this->setState(restingState);
        updateAnimationTargets();
    }

private:
    friend struct PrimaryButtonTestAccess;

    enum class PointerGesture
    {
        none,
        rejected,
        primary
    };

    bool isPointerSource(const juce::MouseEvent& event) const noexcept
    {
        return event.source.getType() == pointerSourceType
            && event.source.getIndex() == pointerSourceIndex;
    }

    void recoverMissingPointerUp(const juce::MouseEvent& event)
    {
        if (pointerGesture == PointerGesture::primary
            && isPointerSource(event)
            && ! event.mods.isLeftButtonDown())
            dismissPointerGesture();
    }

    void updateAnimationTargets()
    {
        if (! this->isShowing())
        {
            stopTimer();
            hoverAnimation = pressAnimation = focusAnimation = 0.0f;
            disabledAnimation = this->isEnabled() ? 0.0f : 1.0f;
            this->repaint();
            return;
        }

        if (! animationsAtRest())
            startTimerHz(60);
        this->repaint();
    }

    bool animationsAtRest() const noexcept
    {
        const auto hoverTarget = this->isEnabled() && this->isMouseOver(true) ? 1.0f : 0.0f;
        const auto pressTarget = this->isEnabled() && this->getState() == juce::Button::buttonDown ? 1.0f : 0.0f;
        const auto focusTarget = this->isEnabled() && this->hasKeyboardFocus(true) ? 1.0f : 0.0f;
        const auto disabledTarget = this->isEnabled() ? 0.0f : 1.0f;
        return std::abs(hoverAnimation - hoverTarget) < 0.0001f
            && std::abs(pressAnimation - pressTarget) < 0.0001f
            && std::abs(focusAnimation - focusTarget) < 0.0001f
            && std::abs(disabledAnimation - disabledTarget) < 0.0001f;
    }

    static bool approach(float& value, float target, float amount) noexcept
    {
        const auto old = value;
        value += (target - value) * amount;
        if (std::abs(value - target) < 0.002f)
            value = target;
        return std::abs(old - value) > 0.001f;
    }

    void timerCallback() override
    {
        if (! this->isShowing())
        {
            updateAnimationTargets();
            return;
        }

        const auto enabled = this->isEnabled();
        auto changed = approach(hoverAnimation, enabled && this->isMouseOver(true) ? 1.0f : 0.0f, 0.22f);
        changed = approach(pressAnimation, enabled && this->getState() == juce::Button::buttonDown ? 1.0f : 0.0f, 0.32f) || changed;
        changed = approach(focusAnimation, enabled && this->hasKeyboardFocus(true) ? 1.0f : 0.0f, 0.20f) || changed;
        changed = approach(disabledAnimation, enabled ? 0.0f : 1.0f, 0.18f) || changed;

        if (changed)
            this->repaint();
        if (animationsAtRest())
            stopTimer();
    }

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    float hoverAnimation = 0.0f;
    float pressAnimation = 0.0f;
    float focusAnimation = 0.0f;
    float disabledAnimation = 0.0f;
};

using PrimaryTextButton = PrimaryPointerButton<juce::TextButton>;
using PrimaryToggleButton = PrimaryPointerButton<juce::ToggleButton>;
using PrimaryHyperlinkButton = PrimaryPointerButton<juce::HyperlinkButton>;
