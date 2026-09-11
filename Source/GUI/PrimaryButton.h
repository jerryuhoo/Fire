/*
  ==============================================================================

    PrimaryButton.h
    Created: 24 Aug 2026

  ==============================================================================
*/

#pragma once

#include "FireTheme.h"
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

namespace fire::ui
{
class KeyboardFocusModalityState
{
public:
    void notePointer() noexcept { lastInputWasKeyboard = false; }
    void noteKeyboard() noexcept { lastInputWasKeyboard = true; }

    void focusGained(juce::Component::FocusChangeType cause) noexcept
    {
        if (cause == juce::Component::focusChangedByMouseClick)
            notePointer();
        else if (cause == juce::Component::focusChangedByTabKey)
            noteKeyboard();
    }

    void resetSession() noexcept { lastInputWasKeyboard = true; }
    bool isKeyboardVisible() const noexcept { return lastInputWasKeyboard; }

private:
    // The first direct/programmatic focus is keyboard-visible. Once an actual
    // input establishes modality, temporary direct focus transfers preserve it.
    bool lastInputWasKeyboard = true;
};

// Keyboard and accessibility are user-input paths, so the button must still
// belong to a live, visible peer when the command is dispatched.
inline bool canActivateButton(const juce::Button& button) noexcept
{
    return button.isEnabled() && button.isShowing();
}

// Preserve the pre-existing explicit-trigger contract of the spectrogram
// controls: a hidden hierarchy is inert, but a test/owner may deliberately
// trigger a visible off-desktop control.
inline bool canTriggerButtonProgrammatically(
    const juce::Button& button) noexcept
{
    if (! button.isEnabled())
        return false;

    for (auto* component = static_cast<const juce::Component*>(&button);
         component != nullptr;
         component = component->getParentComponent())
        if (! component->isVisible())
            return false;

    return true;
}

class GuardedButtonValueInterface final
    : public juce::AccessibilityTextValueInterface
{
public:
    explicit GuardedButtonValueInterface(juce::Button& buttonToWrap)
        : button(buttonToWrap)
    {
    }

    bool isReadOnly() const override { return true; }

    juce::String getCurrentValueAsString() const override
    {
        return button.getToggleState() ? "On" : "Off";
    }

    void setValueAsString(const juce::String&) override {}

private:
    juce::Button& button;
};

class GuardedButtonAccessibilityHandler final
    : public juce::AccessibilityHandler
{
public:
    GuardedButtonAccessibilityHandler(juce::Button& buttonToWrap,
                                      juce::AccessibilityRole role)
        : juce::AccessibilityHandler(
              buttonToWrap,
              buttonToWrap.getRadioGroupId() != 0
                  ? juce::AccessibilityRole::radioButton
                  : role,
              makeActions(buttonToWrap),
              makeInterfaces(buttonToWrap)),
          button(buttonToWrap)
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = juce::AccessibilityHandler::getCurrentState();

        if (button.isToggleable())
        {
            state = state.withCheckable();
            if (button.getToggleState())
                state = state.withChecked();
        }

        return state;
    }

    juce::String getTitle() const override
    {
        const auto title = juce::AccessibilityHandler::getTitle();
        return title.isEmpty() ? button.getButtonText() : title;
    }

    juce::String getHelp() const override
    {
        return button.getTooltip();
    }

private:
    static juce::AccessibilityActions makeActions(juce::Button& button)
    {
        auto actions = juce::AccessibilityActions().addAction(
            juce::AccessibilityActionType::press,
            [&button]
            {
                if (canActivateButton(button))
                    button.triggerClick();
            });

        if (button.isToggleable())
            actions.addAction(
                juce::AccessibilityActionType::toggle,
                [&button]
                {
                    if (! canActivateButton(button))
                        return;

                    if (button.getRadioGroupId() != 0)
                    {
                        // A radio action selects an option; it must never leave
                        // the group with no selected item. Route a new
                        // selection through the normal click path so listeners
                        // and sibling deselection keep their JUCE semantics.
                        // This must remain the final operation because a click
                        // listener may delete the button.
                        if (! button.getToggleState())
                            button.triggerClick();
                        return;
                    }

                    // This matches JUCE's ButtonAccessibilityHandler:
                    // sendNotification also invokes click listeners/onClick.
                    button.setToggleState(! button.getToggleState(),
                                          juce::sendNotification);
                });

        return actions;
    }

    static Interfaces makeInterfaces(juce::Button& button)
    {
        if (button.isToggleable())
            return { std::make_unique<GuardedButtonValueInterface>(button) };

        return {};
    }

    juce::Button& button;
};

inline std::unique_ptr<juce::AccessibilityHandler>
createGuardedButtonAccessibilityHandler(juce::Button& button,
                                        juce::AccessibilityRole role)
{
    return std::make_unique<GuardedButtonAccessibilityHandler>(button, role);
}

template <typename ButtonType>
constexpr juce::AccessibilityRole getButtonAccessibilityRole() noexcept
{
    if constexpr (std::is_base_of_v<juce::ToggleButton, ButtonType>)
        return juce::AccessibilityRole::toggleButton;
    else if constexpr (std::is_base_of_v<juce::HyperlinkButton, ButtonType>)
        return juce::AccessibilityRole::hyperlink;
    else
        return juce::AccessibilityRole::button;
}
} // namespace fire::ui

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
            // Keep JUCE's explicit programmatic trigger semantics: unlike user
            // input, triggerClick() is allowed off-desktop. The guarded
            // accessibility action has already required isShowing(). Submit now
            // so the command cannot land on a later band/LFO binding. This must
            // remain the final operation because it may delete the button.
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
            // A pointer click may give the component keyboard focus, but that
            // must not leave a keyboard-navigation outline behind after the
            // pointer exits.
            focusModality.notePointer();
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

            if (safeThis != nullptr)
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

            if (safeThis != nullptr)
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

            if (safeThis != nullptr)
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
        if (this->hasKeyboardFocus(true)
            && ! focusModality.isKeyboardVisible())
        {
            focusModality.noteKeyboard();
            updateAnimationTargets();
        }

        const bool isActivationKey =
            key.isKeyCode(juce::KeyPress::returnKey)
            || key.isKeyCode(juce::KeyPress::spaceKey);

        if (isActivationKey)
        {
            if (! fire::ui::canActivateButton(*this))
                return false;

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

    void parentHierarchyChanged() override
    {
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::parentHierarchyChanged();

        if (safeThis == nullptr)
            return;

        if (! this->isShowing())
        {
            dismissPointerGesture();
            if (safeThis == nullptr)
                return;
        }

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

        if (safeThis != nullptr)
            updateAnimationTargets();
    }

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::focusGained(cause);
        if (safeThis != nullptr)
        {
            focusModality.focusGained(cause);
            updateAnimationTargets();
        }
    }

    void focusLost(juce::Component::FocusChangeType cause) override
    {
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::focusLost(cause);
        if (safeThis != nullptr)
        {
            updateAnimationTargets();
        }
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
        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        this->setState(restingState);

        if (safeThis != nullptr)
            updateAnimationTargets();
    }

private:
    friend struct PrimaryButtonTestAccess;

    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override
    {
        return fire::ui::createGuardedButtonAccessibilityHandler(
            *this,
            fire::ui::getButtonAccessibilityRole<ButtonType>());
    }

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
            focusModality.resetSession();
            hoverAnimation = pressAnimation = focusAnimation = 0.0f;
            disabledAnimation = this->isEnabled() ? 0.0f : 1.0f;
            this->repaint();
            return;
        }

        if (! this->isEnabled())
            focusModality.resetSession();

        if (! animationsAtRest() || hasPresentedInteraction())
            startTimerHz(60);
        this->repaint();
    }

    bool hasPresentedInteraction() const noexcept
    {
        return pointerGesture == PointerGesture::primary
            || this->getState() != juce::Button::buttonNormal
            || (this->isEnabled()
                && ((focusModality.isKeyboardVisible()
                     && this->hasKeyboardFocus(true))
                    || this->isMouseOver(true)));
    }

    bool animationsAtRest() const noexcept
    {
        const auto hoverTarget = this->isEnabled() && this->isMouseOver(true) ? 1.0f : 0.0f;
        const auto pressTarget = this->isEnabled() && this->getState() == juce::Button::buttonDown ? 1.0f : 0.0f;
        const auto focusTarget = this->isEnabled()
                                     && focusModality.isKeyboardVisible()
                                     && this->hasKeyboardFocus(true)
                                 ? 1.0f
                                 : 0.0f;
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
            const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
            dismissPointerGesture();
            if (safeThis == nullptr)
                return;

            updateAnimationTargets();
            return;
        }

        const auto enabled = this->isEnabled();
        auto changed = approach(hoverAnimation, enabled && this->isMouseOver(true) ? 1.0f : 0.0f, fire::ui::Motion::step(1.0f / 60.0f, fire::ui::Motion::hover));
        changed = approach(pressAnimation, enabled && this->getState() == juce::Button::buttonDown ? 1.0f : 0.0f, fire::ui::Motion::step(1.0f / 60.0f, fire::ui::Motion::press)) || changed;
        changed = approach(focusAnimation,
                           enabled && focusModality.isKeyboardVisible()
                               && this->hasKeyboardFocus(true)
                           ? 1.0f
                           : 0.0f,
                           fire::ui::Motion::step(1.0f / 60.0f, fire::ui::Motion::focus)) || changed;
        changed = approach(disabledAnimation, enabled ? 0.0f : 1.0f, fire::ui::Motion::step(1.0f / 60.0f, fire::ui::Motion::disabled)) || changed;

        if (changed)
            this->repaint();
        if (animationsAtRest() && ! hasPresentedInteraction())
            stopTimer();
    }

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    fire::ui::KeyboardFocusModalityState focusModality;
    float hoverAnimation = 0.0f;
    float pressAnimation = 0.0f;
    float focusAnimation = 0.0f;
    float disabledAnimation = 0.0f;
};

using PrimaryTextButton = PrimaryPointerButton<juce::TextButton>;
using PrimaryToggleButton = PrimaryPointerButton<juce::ToggleButton>;
using PrimaryHyperlinkButton = PrimaryPointerButton<juce::HyperlinkButton>;
