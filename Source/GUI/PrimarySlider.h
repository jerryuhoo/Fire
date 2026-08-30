/*
  ==============================================================================

    PrimarySlider.h
    Created: 24 Aug 2026

  ==============================================================================
*/

#pragma once

#include "FireTheme.h"
#include "PrimaryButton.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <memory>
#include <optional>

struct PrimarySliderTestAccess;

namespace fire::ui
{
class GuardedSliderValueInterface final
    : public juce::AccessibilityValueInterface
{
public:
    explicit GuardedSliderValueInterface(juce::Slider& sliderToWrap)
        : slider(sliderToWrap), useMaxValue(sliderToWrap.isTwoValue())
    {
    }

    bool isReadOnly() const override { return false; }

    double getCurrentValue() const override
    {
        return useMaxValue ? slider.getMaximum() : slider.getValue();
    }

    void setValue(double newValue) override
    {
        if (! canInteract())
            return;

        juce::Slider::ScopedDragNotification drag(slider);
        if (useMaxValue)
            slider.setMaxValue(newValue, juce::sendNotificationSync);
        else
            slider.setValue(newValue, juce::sendNotificationSync);
    }

    juce::String getCurrentValueAsString() const override
    {
        return slider.getTextFromValue(getCurrentValue());
    }

    void setValueAsString(const juce::String& newValue) override
    {
        if (canInteract())
            setValue(slider.getValueFromText(newValue));
    }

    juce::AccessibilityValueInterface::AccessibleValueRange
    getRange() const override
    {
        const auto interval = slider.getInterval();
        const auto stepSize = ! juce::approximatelyEqual(interval, 0.0)
                                  ? interval
                                  : slider.getRange().getLength() * 0.01;
        return { { slider.getMinimum(), slider.getMaximum() },
                 stepSize };
    }

private:
    bool canInteract() const noexcept
    {
        return slider.isEnabled() && slider.isShowing();
    }

    juce::Slider& slider;
    const bool useMaxValue;
};

class GuardedSliderAccessibilityHandler final
    : public juce::AccessibilityHandler
{
public:
    explicit GuardedSliderAccessibilityHandler(juce::Slider& sliderToWrap)
        : juce::AccessibilityHandler(
              sliderToWrap,
              juce::AccessibilityRole::slider,
              {},
              { std::make_unique<GuardedSliderValueInterface>(sliderToWrap) }),
          slider(sliderToWrap)
    {
    }

    juce::String getHelp() const override { return slider.getTooltip(); }

private:
    juce::Slider& slider;
};
} // namespace fire::ui

class PrimarySliderAnimationState
{
public:
    virtual ~PrimarySliderAnimationState() = default;
    virtual float getHoverAnimation() const noexcept = 0;
    virtual float getPressAnimation() const noexcept = 0;
    virtual float getFocusAnimation() const noexcept = 0;
    virtual float getDisabledAnimation() const noexcept = 0;
};

/** A Slider that accepts pointer drags and double-clicks only from a complete
    primary-button gesture owned by one MouseInputSource.

    JUCE's Slider normally treats right and middle mouse buttons as drags when
    its optional popup menu is disabled. This wrapper keeps keyboard and wheel
    behaviour unchanged while making pointer input deterministic.
*/
class PrimarySlider : public juce::Slider,
                      private juce::Timer,
                      public PrimarySliderAnimationState
{
public:
    using juce::Slider::Slider;

    ~PrimarySlider() override
    {
        stopTimer();
        dismissTransientInteraction();
    }

    float getHoverAnimation() const noexcept override { return hoverAnimation.current; }
    float getPressAnimation() const noexcept override { return pressAnimation.current; }
    float getFocusAnimation() const noexcept override { return focusAnimation.current; }
    float getDisabledAnimation() const noexcept override { return disabledAnimation.current; }

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

        if (safeThis)
            updateAnimationTargets();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (pointerGesture != PointerGesture::primary
            || ! isPointerSource(event))
            return;

        lastAcceptedPointerEvent.emplace(event);
        const auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::mouseDrag(event);
        if (safeThis)
            updateAnimationTargets();
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        const auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::mouseEnter(event);
        if (! safeThis)
            return;
        updateAnimationTargets();
        recoverMissingPointerUp(event);
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        const auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::mouseMove(event);
        if (! safeThis)
            return;
        updateAnimationTargets();
        recoverMissingPointerUp(event);
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        const auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::mouseExit(event);
        if (! safeThis)
            return;
        updateAnimationTargets();
        recoverMissingPointerUp(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (pointerGesture == PointerGesture::none
            || ! isPointerSource(event))
            return;

        const auto completedGesture = pointerGesture;
        clearPointerState();
        const auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);

        // Release modifiers are deliberately ignored after a pure primary
        // mouseDown; the matching source must always close Slider's drag.
        if (completedGesture == PointerGesture::primary)
            juce::Slider::mouseUp(event);

        if (safeThis)
            updateAnimationTargets();
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
        if (safeThis)
        {
            if (! isShowing())
                dismissTransientInteraction();
            if (safeThis)
                updateAnimationTargets();
        }
    }

    void enablementChanged() override
    {
        auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::enablementChanged();
        if (safeThis)
        {
            dismissTransientInteraction();
            if (safeThis)
                updateAnimationTargets();
        }
    }

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        const auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::focusGained(cause);
        if (safeThis)
            updateAnimationTargets();
    }

    void focusLost(juce::Component::FocusChangeType cause) override
    {
        const auto safeThis = juce::Component::SafePointer<PrimarySlider>(this);
        juce::Slider::focusLost(cause);
        if (safeThis)
            updateAnimationTargets();
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

    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override
    {
        return std::make_unique<fire::ui::GuardedSliderAccessibilityHandler>(
            *this);
    }

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

    void recoverMissingPointerUp(const juce::MouseEvent& event)
    {
        if (pointerGesture == PointerGesture::primary
            && isPointerSource(event)
            && ! event.mods.isLeftButtonDown())
        {
            // Slider::mouseUp notifies listeners and may synchronously delete
            // this control, so gesture completion is the final operation.
            finishActivePointerGesture();
        }
    }

    void clearPointerState() noexcept
    {
        pointerGesture = PointerGesture::none;
        pointerSourceIndex = -1;
        lastAcceptedPointerEvent.reset();
        pressAnimation.setTarget(0.0f);
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

    void updateAnimationTargets() noexcept
    {
        if (! isShowing())
        {
            stopTimer();
            hoverAnimation.snapTo(0.0f);
            pressAnimation.snapTo(0.0f);
            focusAnimation.snapTo(0.0f);
            disabledAnimation.snapTo(isEnabled() ? 0.0f : 1.0f);
            repaint();
            return;
        }

        const auto interactive = isEnabled();
        hoverAnimation.setTarget(interactive && isMouseOver(true) ? 1.0f : 0.0f);
        pressAnimation.setTarget(interactive && hasActivePointerGesture() ? 1.0f : 0.0f);
        focusAnimation.setTarget(interactive && hasKeyboardFocus(true) ? 1.0f : 0.0f);
        disabledAnimation.setTarget(interactive ? 0.0f : 1.0f);
        if (! animationsSettled() && ! isTimerRunning())
            startTimerHz(60);
        repaint();
    }

    bool animationsSettled() const noexcept
    {
        return hoverAnimation.isSettled() && pressAnimation.isSettled()
            && focusAnimation.isSettled() && disabledAnimation.isSettled();
    }

    bool advanceAnimation(float deltaSeconds) noexcept
    {
        auto changed = hoverAnimation.advance(deltaSeconds, 0.10f);
        changed = pressAnimation.advance(deltaSeconds, 0.065f) || changed;
        changed = focusAnimation.advance(deltaSeconds, 0.11f) || changed;
        changed = disabledAnimation.advance(deltaSeconds, 0.13f) || changed;
        return changed;
    }

    void timerCallback() override
    {
        if (! isShowing())
        {
            updateAnimationTargets();
            return;
        }

        updateAnimationTargets();
        const auto changed = advanceAnimation(1.0f / 60.0f);
        if (changed)
            repaint();
        if (animationsSettled())
            stopTimer();
    }

    PointerGesture pointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    std::optional<juce::MouseEvent> lastAcceptedPointerEvent;
    fire::ui::DampedValue hoverAnimation;
    fire::ui::DampedValue pressAnimation;
    fire::ui::DampedValue focusAnimation;
    fire::ui::DampedValue disabledAnimation;
};
