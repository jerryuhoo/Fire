/*
  ==============================================================================

    PrimaryButton.h
    Created: 24 Aug 2026

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <type_traits>

struct PrimaryButtonTestAccess;

/** A JUCE button that accepts pointer clicks only from an owned primary gesture.

    Keyboard and accessibility/programmatic activation are synchronous so a
    command cannot be replayed after a shared control has been rebound or
    hidden. The template is shared by text, toggle, and hyperlink buttons so
    controls with different drawing implementations use the same ownership
    rules.
*/
template <typename ButtonType>
class PrimaryPointerButton : public ButtonType
{
    static_assert(std::is_base_of_v<juce::Button, ButtonType>);

public:
    using ButtonType::ButtonType;

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
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && isPointerSource(event))
            ButtonType::mouseDrag(event);
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::mouseEnter(event);

        if (safeThis != nullptr)
            recoverMissingPointerUp(event);
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::mouseMove(event);

        if (safeThis != nullptr)
            recoverMissingPointerUp(event);
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const juce::Component::SafePointer<PrimaryPointerButton> safeThis(this);
        ButtonType::mouseExit(event);

        if (safeThis != nullptr)
            recoverMissingPointerUp(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        const auto completedGesture = pointerGesture;
        pointerGesture = PointerGesture::none;
        pointerSourceIndex = -1;

        if (completedGesture == PointerGesture::primary)
            ButtonType::mouseUp(event);
        else
            dismissPointerGesture();
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

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
};

using PrimaryTextButton = PrimaryPointerButton<juce::TextButton>;
using PrimaryToggleButton = PrimaryPointerButton<juce::ToggleButton>;
using PrimaryHyperlinkButton = PrimaryPointerButton<juce::HyperlinkButton>;
