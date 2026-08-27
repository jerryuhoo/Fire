/*
  ==============================================================================

    PrimarySlider.h
    Created: 24 Aug 2026

  ==============================================================================
*/

#pragma once

#include "PrimaryButton.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <optional>

struct PrimarySliderTestAccess;

/** A Slider that accepts pointer drags and double-clicks only from a complete
    primary-button gesture owned by one MouseInputSource.

    JUCE's Slider normally treats right and middle mouse buttons as drags when
    its optional popup menu is disabled. This wrapper keeps keyboard and wheel
    behaviour unchanged while making pointer input deterministic.
*/
class PrimarySlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    ~PrimarySlider() override
    {
        dismissTransientInteraction();
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        if (pointerGesture != PointerGesture::none)
        {
            if (! isPointerSource(event))
                return;

            // A fresh down from the owning source is a lifecycle boundary if
            // the host omitted the previous mouseUp.
            finishActivePointerGesture();
            if (! safeThis)
                return;
        }

        if (! isEnabled())
            return;

        pointerGesture = isCompletePrimaryDown(event)
                             ? PointerGesture::primary
                             : PointerGesture::rejected;
        pointerSourceType = event.source.getType();
        pointerSourceIndex = event.source.getIndex();
        lastAcceptedPointerEvent.emplace(event);

        if (pointerGesture == PointerGesture::primary)
            juce::Slider::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (pointerGesture != PointerGesture::primary
            || ! isPointerSource(event))
            return;

        lastAcceptedPointerEvent.emplace(event);
        juce::Slider::mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::none
            || ! isPointerSource(event))
            return;

        const auto completedGesture = pointerGesture;
        clearPointerState();

        // Release modifiers are deliberately ignored after a pure primary
        // mouseDown; the matching source must always close Slider's drag.
        if (completedGesture == PointerGesture::primary)
            juce::Slider::mouseUp(event);
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        if (! isEnabled() || ! isCompletePrimaryDown(event))
            return;

        if (pointerGesture != PointerGesture::none
            && (pointerGesture != PointerGesture::primary
                || ! isPointerSource(event)))
            return;

        juce::Slider::mouseDoubleClick(event);
    }

    void visibilityChanged() override
    {
        auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::visibilityChanged();
        if (safeThis && ! isShowing())
            dismissTransientInteraction();
    }

    void enablementChanged() override
    {
        auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::enablementChanged();
        if (safeThis)
            dismissTransientInteraction();
    }

    bool hasActivePointerGesture() const noexcept
    {
        return pointerGesture == PointerGesture::primary;
    }

    /** Ends the accepted Slider drag while its attachment/listeners are alive,
        and returns any Inc/Dec child buttons to their resting state. */
    void dismissTransientInteraction()
    {
        auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);

        // Text entered for one attached parameter must never be committed
        // after the Slider has been rebound to another parameter. Avoid an
        // unconditional hideTextBox() here because it refreshes display text
        // even during member destruction, when a conversion lambda may refer
        // to parent state that has already been torn down.
        for (auto* child : getChildren())
            if (auto* valueLabel = dynamic_cast<juce::Label*>(child);
                valueLabel != nullptr && valueLabel->isBeingEdited())
            {
                hideTextBox(true);
                if (! safeThis)
                    return;
                break;
            }

        for (auto* child : getChildren())
        {
            if (auto* button = dynamic_cast<PrimaryTextButton*>(child))
                button->dismissPointerGesture();

            if (! safeThis)
                return;
        }

        finishActivePointerGesture();
    }

private:
    friend struct PrimarySliderTestAccess;

    enum class PointerGesture
    {
        none,
        rejected,
        primary
    };

    bool isCompletePrimaryDown(
        const juce::MouseEvent& event) const noexcept
    {
        return event.mods.isLeftButtonDown()
            && ! event.mods.isRightButtonDown()
            && ! event.mods.isMiddleButtonDown()
            && ! event.mods.isPopupMenu();
    }

    bool isPointerSource(const juce::MouseEvent& event) const noexcept
    {
        return event.source.getType() == pointerSourceType
            && event.source.getIndex() == pointerSourceIndex;
    }

    void clearPointerState() noexcept
    {
        pointerGesture = PointerGesture::none;
        pointerSourceIndex = -1;
        lastAcceptedPointerEvent.reset();
    }

    void finishActivePointerGesture()
    {
        std::optional<juce::MouseEvent> releaseEvent;
        if (lastAcceptedPointerEvent.has_value())
            releaseEvent.emplace(*lastAcceptedPointerEvent);

        const auto completedGesture = pointerGesture;
        clearPointerState();

        if (completedGesture == PointerGesture::primary)
        {
            jassert(releaseEvent.has_value());
            if (releaseEvent.has_value())
                juce::Slider::mouseUp(*releaseEvent);
        }
    }

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    std::optional<juce::MouseEvent> lastAcceptedPointerEvent;
};
