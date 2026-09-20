/*
  ==============================================================================

    DraggableButton.cpp
    Created: 7 Oct 2021 11:42:21pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "DraggableButton.h"
#include "../../Utility/StrictNumberParser.h"

namespace
{
constexpr double minimumFilterFrequency = 20.0;
constexpr double maximumFilterFrequency = 20000.0;

bool isPrimaryPointerDown(const juce::MouseEvent& event) noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isPopupMenu()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown();
}

class CallbackFrequencyValueInterface final
    : public juce::AccessibilityValueInterface
{
public:
    using Getter = std::function<double()>;
    using Setter = std::function<void(double)>;
    using ReadOnlyGetter = std::function<bool()>;

    CallbackFrequencyValueInterface(Getter getterToUse,
                                    Setter setterToUse,
                                    ReadOnlyGetter readOnlyGetterToUse)
        : getter(std::move(getterToUse)),
          setter(std::move(setterToUse)),
          readOnlyGetter(std::move(readOnlyGetterToUse))
    {
    }

    bool isReadOnly() const override
    {
        return readOnlyGetter == nullptr || readOnlyGetter();
    }

    double getCurrentValue() const override
    {
        return getter != nullptr ? getter() : minimumFilterFrequency;
    }

    juce::String getCurrentValueAsString() const override
    {
        return juce::String(juce::roundToInt(getCurrentValue())) + " Hz";
    }

    void setValue(double newValue) override
    {
        if (! isReadOnly() && setter != nullptr)
            setter(newValue);
    }

    void setValueAsString(const juce::String& newValue) override
    {
        double frequency = 0.0;
        if (fire::utility::parseStrictFrequency(newValue, frequency))
            setValue(frequency);
    }

    AccessibleValueRange getRange() const override
    {
        return { { minimumFilterFrequency, maximumFilterFrequency }, 1.0 };
    }

private:
    Getter getter;
    Setter setter;
    ReadOnlyGetter readOnlyGetter;
};
} // namespace

//==============================================================================
DraggableButton::DraggableButton()
{
    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);
    focusAnimation.snapTo(0.0f);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setTitle("Filter node frequency");
    setHelpText("Use the arrow keys to adjust frequency and gain. "
                "Use Page Up and Page Down to adjust resonance.");
}

DraggableButton::~DraggableButton()
{
    stopTimer();
}

void DraggableButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const auto hover = juce::jlimit(0.0f, 1.0f, hoverAnimation.current);
    const auto press = juce::jlimit(0.0f, 1.0f, pressAnimation.current);
    const auto focus = juce::jlimit(0.0f, 1.0f, focusAnimation.current);
    const auto accent = getColour();

    bounds = bounds.reduced(press * 0.45f);

    if (mState)
    {
        const auto emphasis = juce::jmax(hover, focus * 0.92f);
        g.setColour(accent.withAlpha(0.11f + hover * 0.09f
                                           + focus * 0.14f));
        g.fillEllipse(bounds.expanded(-0.5f + emphasis * 0.5f));
    }

    juce::ColourGradient metal(fire::ui::colours::raised.brighter(0.04f + hover * 0.06f),
                               bounds.getCentreX(), bounds.getY(),
                               fire::ui::colours::surface0, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(metal);
    g.fillEllipse(bounds.reduced(bounds.getWidth() * 0.16f));

    g.setColour(accent.withAlpha(mState ? 0.76f + hover * 0.19f
                                      : 0.38f));
    g.drawEllipse(bounds.reduced(bounds.getWidth() * 0.16f), 1.0f);

    const auto ordinal = static_cast<int>(getProperties().getWithDefault("eqOrdinal", 0));
    if (ordinal > 0)
    {
        const bool selected = getProperties().getWithDefault("eqSelected", false);
        const bool bypassed = getProperties().getWithDefault("eqBypassed", false);
        if (selected)
        {
            g.setColour(accent.withAlpha(0.65f));
            g.drawEllipse(bounds.reduced(1.5f), 1.2f);
        }
        g.setColour(! bypassed && mState ? fire::ui::colours::whiteHot : fire::ui::colours::textMuted);
        g.setFont(fire::ui::labelFont(bounds.getWidth() * 0.43f));
        g.drawText(juce::String(ordinal), bounds, juce::Justification::centred);
    }
    else
    {
        const auto coreInset = 0.38f - hover * 0.04f + press * 0.015f;
        const auto core = bounds.reduced(bounds.getWidth() * coreInset);
        g.setColour(mState ? fire::ui::colours::whiteHot : fire::ui::colours::disabled);
        g.fillEllipse(core);
    }

    if (focus > 0.001f)
    {
        g.setColour(fire::ui::colours::ember.withAlpha(0.92f * focus));
        g.drawEllipse(bounds.reduced(1.0f), 1.5f);
    }
}

void DraggableButton::resized()
{
}

void DraggableButton::mouseEnter(const juce::MouseEvent& e)
{
    juce::Component::mouseEnter(e);
    isEntered = true;
    updateAnimationTargets();

    // If the owner re-enters without its primary button, the preceding up was
    // lost while the host or window manager changed pointer capture.
    recoverMissingPointerUp(e);
}

void DraggableButton::mouseMove(const juce::MouseEvent& e)
{
    juce::Component::mouseMove(e);
    recoverMissingPointerUp(e);
}

void DraggableButton::mouseExit(const juce::MouseEvent& e)
{
    juce::Component::mouseExit(e);
    isEntered = false;
    updateAnimationTargets();

    recoverMissingPointerUp(e);
}

juce::Colour DraggableButton::getColour() const
{
    if (mState)
        return fire::ui::colours::filter.brighter(
            juce::jlimit(0.0f, 1.0f, hoverAnimation.current) * 0.12f);

    return fire::ui::colours::disabled;
}

void DraggableButton::setState(const bool state)
{
    if (mState == state)
        return;

    mState = state;
    if (! state)
        clearInteractionPresentation();
    else
        updateAnimationTargets();
    repaint();

    // Ending an active drag can synchronously remove this component through a
    // host parameter callback, so it must be the final access in this path.
    if (! state)
        dismissTransientInteraction();
}

void DraggableButton::mouseDown(const juce::MouseEvent& event)
{
    juce::Component::mouseDown(event);

    juce::Component::SafePointer<DraggableButton> safeThis(this);
    if (primaryDragActive)
    {
        // Interleaved touch or pen streams cannot steal an active filter drag.
        if (! isPointerSource(event))
            return;

        keyboardFocusVisible = false;

        // A fresh down from the owner is a lifecycle boundary when a host
        // omitted the previous mouseUp.
        dismissTransientInteraction();

        if (safeThis == nullptr)
            return;
    }
    else
    {
        keyboardFocusVisible = false;
    }

    updateAnimationTargets();

    if (! mState || ! isEnabled() || ! onDrag
        || ! isPrimaryPointerDown(event))
        return;

    primaryDragActive = true;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();
    updateAnimationTargets();
    auto dragCallback = onDrag;

    // Keep the callable alive if it removes its owning component. No member is
    // accessed after the callback begins.
    dragCallback(*this, event);
}

void DraggableButton::mouseDrag(const juce::MouseEvent& event)
{
    juce::Component::mouseDrag(event);

    if (primaryDragActive && isPointerSource(event) && mState && onDrag)
    {
        auto dragCallback = onDrag;
        dragCallback(*this, event);
    }
}

void DraggableButton::mouseUp(const juce::MouseEvent& event)
{
    juce::Component::mouseUp(event);

    if (! primaryDragActive || ! isPointerSource(event))
        return;

    dismissTransientInteraction();
}

bool DraggableButton::keyPressed(const juce::KeyPress& key)
{
    if (hasKeyboardFocus(true) && ! keyboardFocusVisible)
    {
        keyboardFocusVisible = true;
        updateAnimationTargets();
    }

    if (! canAcceptKeyboardOrAccessibilityInput())
        return juce::Component::keyPressed(key);

    const auto modifiers = key.getModifiers();
    if (modifiers.isCommandDown() || modifiers.isCtrlDown()
        || modifiers.isAltDown())
        return juce::Component::keyPressed(key);

    if (key.isKeyCode(juce::KeyPress::pageUpKey)
        || key.isKeyCode(juce::KeyPress::pageDownKey))
    {
        if (! onQValueChanged)
            return juce::Component::keyPressed(key);

        const auto qDelta = key.isKeyCode(juce::KeyPress::pageUpKey)
                              ? 0.04f : -0.04f;
        auto qValueCallback = onQValueChanged;

        // The parameter callback may synchronously remove this control.
        qValueCallback(qDelta);
        return true;
    }

    const auto step = modifiers.isShiftDown() ? 1.0f : 4.0f;
    auto position = getLocalBounds().toFloat().getCentre();
    if (key.isKeyCode(juce::KeyPress::leftKey))
        position.x -= step;
    else if (key.isKeyCode(juce::KeyPress::rightKey))
        position.x += step;
    else if (key.isKeyCode(juce::KeyPress::upKey))
        position.y -= step;
    else if (key.isKeyCode(juce::KeyPress::downKey))
        position.y += step;
    else
        return juce::Component::keyPressed(key);

    return performKeyboardMove(position);
}

void DraggableButton::dismissTransientInteraction()
{
    if (! primaryDragActive)
        return;

    primaryDragActive = false;
    pointerSourceIndex = -1;
    updateAnimationTargets();
    if (onDragFinished)
    {
        auto finishedCallback = onDragFinished;
        finishedCallback();
    }
}

bool DraggableButton::performKeyboardMove(juce::Point<float> localPosition)
{
    if (! canAcceptKeyboardOrAccessibilityInput() || ! onDrag)
        return false;

    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent keyboardMove(source,
                                        localPosition,
                                        {},
                                        0.0f,
                                        0.0f,
                                        0.0f,
                                        0.0f,
                                        0.0f,
                                        this,
                                        this,
                                        now,
                                        localPosition,
                                        now,
                                        1,
                                        true);
    auto dragCallback = onDrag;
    juce::Component::SafePointer<DraggableButton> safeThis(this);
    dragCallback(*this, keyboardMove);

    if (safeThis == nullptr)
        return true;

    auto finishedCallback = safeThis->onDragFinished;

    // A keyboard nudge is one complete host gesture. The finish callback may
    // synchronously delete the control, so it is the final component action.
    if (finishedCallback)
        finishedCallback();

    return true;
}

bool DraggableButton::setAccessibleFrequency(double frequency)
{
    if (! canAcceptKeyboardOrAccessibilityInput() || getParentComponent() == nullptr
        || getParentWidth() <= 0 || ! std::isfinite(frequency))
        return false;

    frequency = juce::jlimit(minimumFilterFrequency,
                             maximumFilterFrequency,
                             frequency);
    if (juce::approximatelyEqual(frequency, getAccessibleFrequency()))
        return true;

    const auto normalisedX = static_cast<float>(
        juce::mapFromLog10(frequency,
                           minimumFilterFrequency,
                           maximumFilterFrequency));
    const auto targetInParent = juce::Point<float> {
        normalisedX * static_cast<float>(getParentWidth()),
        getBounds().toFloat().getCentreY()
    };
    return performKeyboardMove(targetInParent - getPosition().toFloat());
}

double DraggableButton::getAccessibleFrequency() const noexcept
{
    if (getParentComponent() == nullptr || getParentWidth() <= 0)
        return minimumFilterFrequency;

    const auto normalisedX = juce::jlimit(
        0.0f,
        1.0f,
        getBounds().toFloat().getCentreX()
            / static_cast<float>(getParentWidth()));
    return juce::mapToLog10(static_cast<double>(normalisedX),
                            minimumFilterFrequency,
                            maximumFilterFrequency);
}

bool DraggableButton::canAcceptKeyboardOrAccessibilityInput() const noexcept
{
    return mState && isEnabled() && isShowing() && ! primaryDragActive;
}

void DraggableButton::timerCallback()
{
    if (! mState || ! isEnabled() || ! isShowing())
    {
        isEntered = false;
        clearInteractionPresentation();
        repaint();

        // A detached peer cannot deliver its matching pointer release. The
        // finish callback can synchronously delete the owning editor, so this
        // remains the final operation in the lifecycle path.
        dismissTransientInteraction();
        return;
    }

    updateAnimationTargets();
    if (advanceAnimation(1.0f / 60.0f))
        repaint();

    if (animationsSettled())
    {
        if (hasPresentedInteraction())
            startTimer(100);
        else
            stopTimer();
    }
}

void DraggableButton::updateAnimationTargets() noexcept
{
    if (! mState || ! isEnabled() || ! isShowing())
    {
        clearInteractionPresentation();
        return;
    }

    hoverAnimation.setTarget(isEntered || primaryDragActive ? 1.0f : 0.0f);
    pressAnimation.setTarget(primaryDragActive ? 1.0f : 0.0f);
    focusAnimation.setTarget(keyboardFocusVisible && hasKeyboardFocus(true)
                                 ? 1.0f
                                 : 0.0f);
    startAnimationIfNeeded();
}

void DraggableButton::startAnimationIfNeeded() noexcept
{
    if (! animationsSettled())
    {
        startTimerHz(60);
        return;
    }

    // Keep a low-rate lifecycle watch while an interaction is visibly held.
    // Component::removeFromDesktop() does not emit visibilityChanged(), so a
    // fully settled hover/focus would otherwise remain painted after a host
    // detaches the editor peer.
    if (hasPresentedInteraction() && ! isTimerRunning())
        startTimer(100);
}

bool DraggableButton::advanceAnimation(float deltaSeconds) noexcept
{
    auto changed = hoverAnimation.advance(deltaSeconds, 0.10f);
    changed = pressAnimation.advance(deltaSeconds, 0.065f) || changed;
    changed = focusAnimation.advance(deltaSeconds, 0.11f) || changed;
    return changed;
}

bool DraggableButton::animationsSettled() const noexcept
{
    return hoverAnimation.isSettled() && pressAnimation.isSettled()
        && focusAnimation.isSettled();
}

bool DraggableButton::hasPresentedInteraction() const noexcept
{
    return hoverAnimation.current > 0.001f
        || pressAnimation.current > 0.001f
        || focusAnimation.current > 0.001f
        || hoverAnimation.target > 0.001f
        || pressAnimation.target > 0.001f
        || focusAnimation.target > 0.001f;
}

void DraggableButton::clearInteractionPresentation() noexcept
{
    stopTimer();
    keyboardFocusVisible = false;
    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);
    focusAnimation.snapTo(0.0f);
}

void DraggableButton::focusGained(FocusChangeType cause)
{
    const juce::Component::SafePointer<DraggableButton> safeThis(this);
    juce::Component::focusGained(cause);
    if (safeThis == nullptr)
        return;

    keyboardFocusVisible = cause != focusChangedByMouseClick;
    updateAnimationTargets();
}

void DraggableButton::focusLost(FocusChangeType cause)
{
    const juce::Component::SafePointer<DraggableButton> safeThis(this);
    juce::Component::focusLost(cause);
    if (safeThis == nullptr)
        return;

    keyboardFocusVisible = false;
    updateAnimationTargets();
}

void DraggableButton::enablementChanged()
{
    juce::Component::enablementChanged();
    isEntered = isEntered && isEnabled();

    if (isEnabled())
        updateAnimationTargets();
    else
        clearInteractionPresentation();

    repaint();

    if (! isEnabled())
        dismissTransientInteraction();
}

void DraggableButton::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (isShowing())
    {
        updateAnimationTargets();
        repaint();
        return;
    }

    isEntered = false;
    clearInteractionPresentation();
    repaint();

    // The drag-finished callback may synchronously destroy the owner.
    dismissTransientInteraction();
}

std::unique_ptr<juce::AccessibilityHandler>
DraggableButton::createAccessibilityHandler()
{
    const juce::Component::SafePointer<DraggableButton> safeThis(this);
    auto valueInterface = std::make_unique<CallbackFrequencyValueInterface>(
        [safeThis]
        {
            return safeThis != nullptr
                     ? safeThis->getAccessibleFrequency()
                     : minimumFilterFrequency;
        },
        [safeThis](double frequency)
        {
            if (safeThis != nullptr)
                safeThis->setAccessibleFrequency(frequency);
        },
        [safeThis]
        {
            return safeThis == nullptr
                || ! safeThis->canAcceptKeyboardOrAccessibilityInput()
                || ! safeThis->onDrag;
        });

    return std::make_unique<juce::AccessibilityHandler>(
        *this,
        juce::AccessibilityRole::slider,
        juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces {
            std::move(valueInterface)
        });
}

bool DraggableButton::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void DraggableButton::recoverMissingPointerUp(
    const juce::MouseEvent& event)
{
    if (! primaryDragActive
        || ! isPointerSource(event)
        || event.mods.isLeftButtonDown())
        return;

    // The gesture-end callback may synchronously remove this button, so it is
    // deliberately the final operation in this path.
    dismissTransientInteraction();
}

void DraggableButton::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);

    if (canAcceptKeyboardOrAccessibilityInput() && onQValueChanged)
    {
        auto qValueCallback = onQValueChanged;
        qValueCallback(wheel.deltaY);
    }
}
