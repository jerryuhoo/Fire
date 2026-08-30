/*
  ==============================================================================

    DraggableButton.cpp
    Created: 7 Oct 2021 11:42:21pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "DraggableButton.h"

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
        auto frequency = newValue.getDoubleValue();
        if (newValue.containsIgnoreCase("khz"))
            frequency *= 1000.0;
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
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setTitle("Filter node frequency");
    setHelpText("Use the arrow keys to adjust frequency and gain. "
                "Use Page Up and Page Down to adjust resonance.");
}

DraggableButton::~DraggableButton()
{
}

void DraggableButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const auto accent = getColour();
    const bool hasVisibleFocus = mState && isEnabled()
                              && hasKeyboardFocus(true);

    if (mState)
    {
        g.setColour(accent.withAlpha(hasVisibleFocus ? 0.25f
                                                    : isEntered ? 0.20f : 0.11f));
        g.fillEllipse(bounds.expanded((isEntered || hasVisibleFocus) ? 0.0f
                                                                     : -0.5f));
    }

    juce::ColourGradient metal(fire::ui::colours::raised.brighter(isEntered ? 0.10f : 0.04f),
                               bounds.getCentreX(), bounds.getY(),
                               fire::ui::colours::surface0, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(metal);
    g.fillEllipse(bounds.reduced(bounds.getWidth() * 0.16f));

    g.setColour(accent.withAlpha(mState ? (isEntered ? 0.95f : 0.76f) : 0.38f));
    g.drawEllipse(bounds.reduced(bounds.getWidth() * 0.16f), 1.0f);

    const auto core = bounds.reduced(bounds.getWidth() * (isEntered ? 0.34f : 0.38f));
    g.setColour(mState ? fire::ui::colours::whiteHot : fire::ui::colours::disabled);
    g.fillEllipse(core);

    if (hasVisibleFocus)
    {
        g.setColour(fire::ui::colours::ember.withAlpha(0.92f));
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
    repaint();

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
    repaint();

    recoverMissingPointerUp(e);
}

juce::Colour DraggableButton::getColour()
{
    if (mState && isEntered)
        return fire::ui::colours::filter.brighter(0.12f);
    else if (mState && ! isEntered)
        return fire::ui::colours::filter;

    return fire::ui::colours::disabled;
}

void DraggableButton::setState(const bool state)
{
    if (mState == state)
        return;

    mState = state;
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

        // A fresh down from the owner is a lifecycle boundary when a host
        // omitted the previous mouseUp.
        dismissTransientInteraction();

        if (safeThis == nullptr)
            return;
    }

    if (! mState || ! isEnabled() || ! onDrag
        || ! isPrimaryPointerDown(event))
        return;

    primaryDragActive = true;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();
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

void DraggableButton::focusGained(FocusChangeType cause)
{
    juce::Component::focusGained(cause);
    repaint();
}

void DraggableButton::focusLost(FocusChangeType cause)
{
    juce::Component::focusLost(cause);
    repaint();
}

void DraggableButton::enablementChanged()
{
    juce::Component::enablementChanged();
    isEntered = isEntered && isEnabled();
    repaint();

    if (! isEnabled())
        dismissTransientInteraction();
}

void DraggableButton::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (isVisible())
    {
        repaint();
        return;
    }

    isEntered = false;
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

    if (mState && isEnabled() && onQValueChanged)
    {
        auto qValueCallback = onQValueChanged;
        qValueCallback(wheel.deltaY);
    }
}
