/*
  ==============================================================================

    VerticalLine.cpp
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "VerticalLine.h"
#include "Multiband.h"

namespace
{
bool isPrimaryPointerDown(const juce::MouseEvent& event) noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isPopupMenu()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown();
}

class CallbackSliderValueInterface final
    : public juce::AccessibilityValueInterface
{
public:
    using Getter = std::function<double()>;
    using TextGetter = std::function<juce::String()>;
    using Setter = std::function<void(double)>;
    using RangeGetter = std::function<AccessibleValueRange()>;
    using ReadOnlyGetter = std::function<bool()>;

    CallbackSliderValueInterface(Getter getterToUse,
                                 TextGetter textGetterToUse,
                                 Setter setterToUse,
                                 RangeGetter rangeGetterToUse,
                                 ReadOnlyGetter readOnlyGetterToUse)
        : getter(std::move(getterToUse)),
          textGetter(std::move(textGetterToUse)),
          setter(std::move(setterToUse)),
          rangeGetter(std::move(rangeGetterToUse)),
          readOnlyGetter(std::move(readOnlyGetterToUse))
    {
    }

    bool isReadOnly() const override
    {
        return readOnlyGetter == nullptr || readOnlyGetter();
    }

    double getCurrentValue() const override
    {
        return getter != nullptr ? getter() : 0.0;
    }

    juce::String getCurrentValueAsString() const override
    {
        return textGetter != nullptr ? textGetter()
                                     : juce::String(getCurrentValue());
    }

    void setValue(double newValue) override
    {
        if (! isReadOnly() && setter != nullptr)
            setter(newValue);
    }

    void setValueAsString(const juce::String& newValue) override
    {
        auto value = newValue.getDoubleValue();
        if (newValue.containsIgnoreCase("khz"))
            value *= 1000.0;
        setValue(value);
    }

    AccessibleValueRange getRange() const override
    {
        return rangeGetter != nullptr ? rangeGetter()
                                      : AccessibleValueRange {};
    }

private:
    Getter getter;
    TextGetter textGetter;
    Setter setter;
    RangeGetter rangeGetter;
    ReadOnlyGetter readOnlyGetter;
};
} // namespace

//==============================================================================
VerticalLine::VerticalLine()
{
    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setTitle("Crossover frequency");
    setHelpText("Use the arrow keys to move this crossover frequency. "
                "Hold Shift for fine adjustment.");
    setTextValueSuffix(" Hz");
}

VerticalLine::~VerticalLine()
{
    auto endCallback = parameterGestureDepth > 0
                         ? std::move(parameterGestureEnd)
                         : ParameterGestureCallback {};
    parameterGestureDepth = 0;
    parameterGestureBegin = nullptr;
    parameterChange = nullptr;
    parameterGestureEnd = nullptr;

    // This callback can synchronously destroy the owning editor. Keep it as
    // the final operation that depends on the component's lifetime.
    if (endCallback)
        endCallback();
}

void VerticalLine::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto hover = juce::jlimit(0.0f, 1.0f, hoverAnimation.current);
    const auto press = juce::jlimit(0.0f, 1.0f, pressAnimation.current);
    const auto focus = isEnabled() && hasKeyboardFocus(true) ? 1.0f : 0.0f;
    const auto emphasis = juce::jlimit(0.0f, 1.0f,
                                       juce::jmax(hover + press * 0.35f,
                                                  focus * 0.86f));
    const float physicalScale = juce::jmax(1.0f,
        g.getInternalContext().getPhysicalPixelScaleFactor());
    const float centreX = fire::ui::pixelAligned(bounds.getCentreX(), physicalScale);
    const float lineWidth = (1.0f + emphasis * 1.25f + press * 0.35f) / physicalScale;

    if (emphasis > 0.001f)
    {
        const auto glowWidth = (5.0f + press * 3.0f) / physicalScale;
        g.setColour(fire::ui::colours::flame.withAlpha(0.10f * emphasis));
        g.fillRoundedRectangle(centreX - glowWidth * 0.5f,
                               bounds.getY(),
                               glowWidth,
                               bounds.getHeight(),
                               glowWidth * 0.5f);
    }

    g.setColour(fire::ui::colours::flame.withAlpha(0.46f + 0.50f * emphasis));
    g.fillRect(centreX - lineWidth * 0.5f,
               bounds.getY(),
               lineWidth,
               bounds.getHeight());

    const float handleRadius = 2.6f + 1.1f * hover + 0.55f * press;
    const juce::Rectangle<float> handle(centreX - handleRadius,
                                         bounds.getY() + 3.0f,
                                         handleRadius * 2.0f,
                                         handleRadius * 2.0f);
    if (emphasis > 0.001f)
    {
        const auto halo = handle.expanded((1.5f + press) / physicalScale);
        g.setColour(fire::ui::colours::flame.withAlpha(0.12f * emphasis));
        g.fillEllipse(halo);
    }
    g.setColour(fire::ui::colours::surface1.withAlpha(0.96f));
    g.fillEllipse(handle);
    g.setColour(fire::ui::colours::flame.withAlpha(0.72f + 0.28f * emphasis));
    g.drawEllipse(handle.reduced(0.5f / physicalScale), lineWidth);

    if (focus > 0.0f)
    {
        g.setColour(fire::ui::colours::ember.withAlpha(0.78f));
        g.drawRoundedRectangle(bounds.reduced(0.75f / physicalScale),
                               2.0f / physicalScale,
                               1.25f / physicalScale);
    }
}

void VerticalLine::resized()
{
}

void VerticalLine::mouseUp (const juce::MouseEvent& e)
{
    if (! primaryDragActive || ! isPointerSource(e))
        return;

    // The matching host notification may synchronously delete this slider.
    dismissPrimaryPointerGesture();
}

void VerticalLine::mouseDoubleClick (const juce::MouseEvent& e)
{
    // do nothing, override the silder function, which will reset value.
}

void VerticalLine::mouseEnter(const juce::MouseEvent& e)
{
    juce::Slider::mouseEnter(e);
    isEntered = true;
    updateAnimationTargets();
    repaint();
}

void VerticalLine::mouseExit(const juce::MouseEvent& e)
{
    juce::Slider::mouseExit(e);
    isEntered = false;
    updateAnimationTargets();
    repaint();
}

void VerticalLine::mouseDrag (const juce::MouseEvent& e)
{
    // this will call multiband mouseDrag
}

void VerticalLine::mouseDown (const juce::MouseEvent& e)
{
    juce::Component::SafePointer<VerticalLine> safeThis(this);
    if (primaryDragActive)
    {
        // Interleaved touch/pen streams cannot steal the active gesture. A
        // fresh down from its owner is the only reliable boundary when a host
        // or window manager omitted the previous mouseUp.
        if (! isPointerSource(e))
            return;

        dismissPrimaryPointerGesture();
        if (safeThis == nullptr)
            return;
    }

    if (! isEnabled() || ! isPrimaryPointerDown(e))
        return;

    if (pointerGestureAdmission)
    {
        auto admissionCallback = pointerGestureAdmission;
        const bool wasAccepted = admissionCallback(
            e.source.getType(), e.source.getIndex());
        if (safeThis == nullptr || ! wasAccepted)
            return;
    }

    primaryDragActive = true;
    pointerSourceType = e.source.getType();
    pointerSourceIndex = e.source.getIndex();
    updateAnimationTargets();

    // The callback is deliberately last so a synchronous owner teardown does
    // not leave a continuation that accesses this component.
    beginParameterGesture();
}

bool VerticalLine::keyPressed(const juce::KeyPress& key)
{
    if (! canAcceptKeyboardOrAccessibilityInput())
        return juce::Component::keyPressed(key);

    const auto modifiers = key.getModifiers();
    if (modifiers.isCommandDown() || modifiers.isCtrlDown()
        || modifiers.isAltDown())
        return juce::Component::keyPressed(key);

    const bool increase = key.isKeyCode(juce::KeyPress::rightKey)
                       || key.isKeyCode(juce::KeyPress::upKey);
    const bool decrease = key.isKeyCode(juce::KeyPress::leftKey)
                       || key.isKeyCode(juce::KeyPress::downKey);
    if (! increase && ! decrease)
        return juce::Component::keyPressed(key);

    const auto step = modifiers.isShiftDown() ? 0.001f : 0.01f;
    const auto targetX = juce::jlimit(0.0f,
                                      1.0f,
                                      xPercent + (increase ? step : -step));
    return setValueFromUserInput(transformFromLog(targetX));
}

bool VerticalLine::advanceAnimation(float deltaSeconds) noexcept
{
    const auto hoverChanged = hoverAnimation.advance(deltaSeconds, 0.10f);
    const auto pressChanged = pressAnimation.advance(deltaSeconds, 0.065f);
    return hoverChanged || pressChanged;
}

bool VerticalLine::isPointerSource(const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void VerticalLine::dismissPrimaryPointerGesture()
{
    if (! primaryDragActive)
        return;

    // Clear ownership before the callback so re-entrant teardown cannot end
    // the same pointer contribution twice. Text editing may hold a separate
    // nested contribution, which endParameterGesture deliberately preserves.
    primaryDragActive = false;
    pointerSourceIndex = -1;
    updateAnimationTargets();

    // The matching host notification may synchronously delete this slider.
    endParameterGesture();
}

void VerticalLine::dismissTransientInteraction()
{
    isEntered = false;
    primaryDragActive = false;
    pointerSourceIndex = -1;

    // Hosts may keep an editor object alive after hiding its window. If that
    // happens during a drag, no later mouseUp is guaranteed, so close the
    // parameter gesture here just as the destructor would.
    auto endCallback = parameterGestureDepth > 0
                         ? parameterGestureEnd
                         : ParameterGestureCallback {};
    parameterGestureDepth = 0;

    hoverAnimation.snapTo(0.0f);
    pressAnimation.snapTo(0.0f);
    repaint();

    // Ending a host gesture can synchronously close the editor. Nothing below
    // this call may depend on the VerticalLine still existing.
    if (endCallback)
        endCallback();
}

void VerticalLine::updateAnimationTargets() noexcept
{
    const bool isPressed = parameterGestureDepth > 0;
    hoverAnimation.setTarget(isEntered || isPressed || hasKeyboardFocus(true)
                                 ? 1.0f : 0.0f);
    pressAnimation.setTarget(isPressed ? 1.0f : 0.0f);
}

bool VerticalLine::canAcceptKeyboardOrAccessibilityInput() const noexcept
{
    return isEnabled() && isShowing() && ! primaryDragActive;
}

bool VerticalLine::setValueFromUserInput(double newValue)
{
    if (! canAcceptKeyboardOrAccessibilityInput() || ! std::isfinite(newValue))
        return false;

    const auto constrained = getNormalisableRange().snapToLegalValue(newValue);
    if (juce::approximatelyEqual(constrained, getValue()))
        return true;

    const auto targetX = juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(transformToLog(constrained)));
    xPercent = targetX;
    juce::Component::SafePointer<VerticalLine> safeThis(this);
    beginParameterGesture();

    if (safeThis == nullptr)
        return true;

    if (auto positionCallback = safeThis->userPositionChange)
        positionCallback(targetX);
    else
        safeThis->setValueAsPartOfGesture(constrained,
                                          juce::sendNotificationSync);

    if (safeThis == nullptr)
        return true;

    // A host may reject or clamp the publication while keeping the editor
    // alive. Keep hit-testing and painting tied to the authoritative value
    // rather than leaving the optimistic keyboard position behind.
    safeThis->xPercent = juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(transformToLog(safeThis->getValue())));

    // The matching host end callback may synchronously remove this divider.
    safeThis->endParameterGesture();
    return true;
}

void VerticalLine::focusGained(FocusChangeType cause)
{
    juce::Slider::focusGained(cause);
    updateAnimationTargets();
    repaint();
}

void VerticalLine::focusLost(FocusChangeType cause)
{
    juce::Slider::focusLost(cause);
    updateAnimationTargets();
    repaint();
}

void VerticalLine::enablementChanged()
{
    juce::Slider::enablementChanged();
    updateAnimationTargets();
    repaint();

    if (! isEnabled())
        dismissTransientInteraction();
}

void VerticalLine::visibilityChanged()
{
    juce::Slider::visibilityChanged();
    if (isVisible())
    {
        updateAnimationTargets();
        repaint();
        return;
    }

    // A hidden divider cannot receive the matching release/key notification.
    dismissTransientInteraction();
}

std::unique_ptr<juce::AccessibilityHandler>
VerticalLine::createAccessibilityHandler()
{
    const juce::Component::SafePointer<VerticalLine> safeThis(this);
    auto valueInterface = std::make_unique<CallbackSliderValueInterface>(
        [safeThis]
        {
            return safeThis != nullptr ? safeThis->getValue() : 0.0;
        },
        [safeThis]
        {
            return safeThis != nullptr
                     ? safeThis->getTextFromValue(safeThis->getValue())
                     : juce::String {};
        },
        [safeThis](double newValue)
        {
            if (safeThis != nullptr)
                safeThis->setValueFromUserInput(newValue);
        },
        [safeThis]
        {
            if (safeThis == nullptr)
                return juce::AccessibilityValueInterface::AccessibleValueRange {};

            const auto range = safeThis->getRange();
            const auto interval = ! juce::approximatelyEqual(
                                      safeThis->getInterval(), 0.0)
                                    ? safeThis->getInterval()
                                    : range.getLength() * 0.01;
            return juce::AccessibilityValueInterface::AccessibleValueRange {
                { range.getStart(), range.getEnd() }, interval
            };
        },
        [safeThis]
        {
            return safeThis == nullptr
                || ! safeThis->canAcceptKeyboardOrAccessibilityInput();
        });

    return std::make_unique<juce::AccessibilityHandler>(
        *this,
        juce::AccessibilityRole::slider,
        juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces {
            std::move(valueInterface)
        });
}

void VerticalLine::setParameterGestureCallbacks(ParameterGestureCallback gestureBegin,
                                                ParameterChangeCallback change,
                                                ParameterGestureCallback gestureEnd)
{
    auto previousEnd = parameterGestureDepth > 0
                         ? parameterGestureEnd
                         : ParameterGestureCallback {};
    parameterGestureDepth = 0;
    primaryDragActive = false;
    pointerSourceIndex = -1;
    updateAnimationTargets();

    parameterGestureBegin = std::move(gestureBegin);
    parameterChange = std::move(change);
    parameterGestureEnd = std::move(gestureEnd);

    // Install the complete replacement before closing the previous gesture.
    // The old callback is allowed to synchronously destroy this component.
    if (previousEnd)
        previousEnd();
}

void VerticalLine::setPointerGestureAdmissionCallback(
    PointerGestureAdmissionCallback callback)
{
    pointerGestureAdmission = std::move(callback);
}

void VerticalLine::setUserPositionChangeCallback(
    UserPositionChangeCallback callback)
{
    userPositionChange = std::move(callback);
}

void VerticalLine::beginParameterGesture()
{
    const bool shouldNotify = parameterGestureDepth++ == 0;
    updateAnimationTargets();
    auto beginCallback = shouldNotify ? parameterGestureBegin
                                      : ParameterGestureCallback {};

    if (beginCallback)
        beginCallback();
}

void VerticalLine::endParameterGesture()
{
    if (parameterGestureDepth <= 0)
        return;

    const bool shouldNotify = --parameterGestureDepth == 0;
    updateAnimationTargets();
    auto endCallback = shouldNotify ? parameterGestureEnd
                                    : ParameterGestureCallback {};

    if (endCallback)
        endCallback();
}

void VerticalLine::setValueAsPartOfGesture(double newValue,
                                           juce::NotificationType notification)
{
    const double constrainedValue = getNormalisableRange().snapToLegalValue(newValue);
    if (! juce::approximatelyEqual(constrainedValue, getValue()) && parameterChange)
    {
        auto changeCallback = parameterChange;
        juce::Component::SafePointer<VerticalLine> safeThis(this);
        auto gestureToken = changeCallback();

        if (safeThis == nullptr || gestureToken == nullptr)
            return;

        // Keep the processor-owned gesture session alive until every
        // synchronous value callback has returned. setValue is intentionally
        // the final component access in this branch.
        setValue(newValue, notification);
        return;
    }

    setValue(newValue, notification);
}

void VerticalLine::setDeleteState (bool deleteState)
{
    mDeleteState = deleteState;
}

void VerticalLine::setXPercent (float x)
{
    xPercent = std::isfinite(x) ? juce::jlimit(0.0f, 1.0f, x) : 0.0f;
}

float VerticalLine::getXPercent()
{
    return xPercent;
}

void VerticalLine::setIndex (int index)
{
    this->index = index;
    setTitle(index >= 0
                 ? "Crossover " + juce::String(index + 1) + " frequency"
                 : "Crossover frequency");
}

int VerticalLine::getIndex()
{
    return index;
}

int VerticalLine::getLeft()
{
    return index - 1;
}

int VerticalLine::getRight()
{
    return index + 1;
}

void VerticalLine::moveToX (int lineNum, float newXPercent, float margin, std::unique_ptr<VerticalLine> verticalLines[])
{
    float leftLimit;
    float rightLimit;

    //    int index = leftIndex + 1;
    leftLimit = (index + 1) * margin;
    rightLimit = 1 - (lineNum - index) * margin;

    if (newXPercent < leftLimit)
        newXPercent = leftLimit;
    if (newXPercent > rightLimit)
        newXPercent = rightLimit;

    if (leftIndex >= 0 && newXPercent - verticalLines[leftIndex]->getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        verticalLines[leftIndex]->moveToX (lineNum, newXPercent - margin, margin, verticalLines);
    }
    if (rightIndex < lineNum && verticalLines[rightIndex]->getXPercent() - newXPercent - margin < -0.00001f)
    {
        verticalLines[rightIndex]->moveToX (lineNum, newXPercent + margin, margin, verticalLines);
    }
    xPercent = newXPercent;
}
