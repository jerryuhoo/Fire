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

    Keyboard activation and Button::triggerClick are deliberately left unchanged.
    The template is shared by text and toggle buttons so controls with different
    drawing implementations use the same pointer ownership rules.
*/
template <typename ButtonType>
class PrimaryPointerButton : public ButtonType
{
    static_assert(std::is_base_of_v<juce::Button, ButtonType>);

public:
    using ButtonType::ButtonType;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::primary
            && ! isPointerSource(event))
            return;

        // Hosts can remove or hide an editor without delivering mouseUp. A
        // fresh down from the owning source starts a new gesture boundary.
        dismissPointerGesture();

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

    void visibilityChanged() override
    {
        ButtonType::visibilityChanged();

        if (! this->isShowing())
            dismissPointerGesture();
    }

    void enablementChanged() override
    {
        ButtonType::enablementChanged();

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

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
};

using PrimaryTextButton = PrimaryPointerButton<juce::TextButton>;
using PrimaryToggleButton = PrimaryPointerButton<juce::ToggleButton>;
