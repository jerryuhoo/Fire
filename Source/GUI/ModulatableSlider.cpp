/*
  ==============================================================================

    ModulatableSlider.cpp
    Created: 2 Sep 2025 1:42:12am
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulatableSlider.h"
#include "LookAndFeel.h"
#include <cmath>

ModulatableSlider::ModulatableSlider()
{
    // Default values
    parameterID = "";
    lfoSource = 0;
    lfoAmount = 0.0;
    lfoValue = 0.0;
    isModHandleMouseOver = false;
    isModHandleMouseDown = false;
    isDraggingMainSlider = false;

    addAndMakeVisible(label);
    // The title is presentation only. The slider deliberately counts the
    // header as part of its hit target, so allowing this child to intercept
    // events makes the visible title strip a dead zone.
    label.setInterceptsMouseClicks(false, false);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(KNOB_FONT_SIZE).withStyle("Plain") });
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}

ModulatableSlider::~ModulatableSlider()
{
    // SliderParameterAttachment does not close an outstanding host gesture in
    // its destructor. Finish our accepted pointer gesture while Slider's
    // implementation and all listeners are still alive.
    dismissTransientInteraction();
    detachValueLabelPopupForwarder();
}

std::unique_ptr<juce::AccessibilityHandler>
ModulatableSlider::createAccessibilityHandler()
{
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    ++accessibilityHandlerCreationCountForTesting;
#endif
    return std::make_unique<fire::ui::GuardedSliderAccessibilityHandler>(
        *this);
}

void ModulatableSlider::ValueLabelPopupForwarder::mouseDown(
    const juce::MouseEvent& event)
{
    if (! owner.isStandalonePopupDown(event))
        return;

    const auto sliderEvent = event.getEventRelativeTo(&owner);
    const auto safeOwner =
        juce::Component::SafePointer<ModulatableSlider>(&owner);
    owner.mouseDown(sliderEvent);
    if (! safeOwner)
        return;

    ownsPopupGesture = owner.activePointerGesture == PointerGesture::popupMenu
                       && owner.isPointerSource(sliderEvent);
}

void ModulatableSlider::ValueLabelPopupForwarder::mouseUp(
    const juce::MouseEvent& event)
{
    if (! ownsPopupGesture)
        return;

    const auto sliderEvent = event.getEventRelativeTo(&owner);
    if (owner.activePointerGesture != PointerGesture::popupMenu)
    {
        ownsPopupGesture = false;
        return;
    }

    if (! owner.isPointerSource(sliderEvent))
        return;

    ownsPopupGesture = false;
    owner.mouseUp(sliderEvent);
}

void ModulatableSlider::ValueLabelPopupForwarder::cancelGesture()
{
    if (! ownsPopupGesture)
        return;

    ownsPopupGesture = false;
    if (owner.activePointerGesture == PointerGesture::popupMenu
        && owner.lastAcceptedPointerEvent.has_value())
        owner.finishActivePointerGesture(*owner.lastAcceptedPointerEvent);
}

void ModulatableSlider::lookAndFeelChanged()
{
    const auto safeThis =
        juce::Component::SafePointer<ModulatableSlider>(this);
    detachValueLabelPopupForwarder();
    juce::Slider::lookAndFeelChanged();
    if (! safeThis)
        return;

    attachValueLabelPopupForwarder();
}

void ModulatableSlider::attachValueLabelPopupForwarder()
{
    for (auto* child : getChildren())
    {
        auto* candidate = dynamic_cast<juce::Label*>(child);
        if (candidate == nullptr || candidate == &label)
            continue;

        forwardedValueLabel = candidate;
        candidate->addMouseListener(&valueLabelPopupForwarder, false);
        break;
    }
}

void ModulatableSlider::detachValueLabelPopupForwarder()
{
    valueLabelPopupForwarder.cancelGesture();
    if (auto* valueLabel = forwardedValueLabel.getComponent())
        valueLabel->removeMouseListener(&valueLabelPopupForwarder);
    forwardedValueLabel = nullptr;
}

bool ModulatableSlider::hitTest(int x, int y)
{
    const auto point = juce::Point<float>(static_cast<float>(x), static_cast<float>(y));
    if (getHeaderBounds().toFloat().contains(point))
        return true;

    auto outerBounds = getRotarySliderBounds().reduced(juce::jmax(5.0f, 7.0f * getUiScale()));
    auto center = outerBounds.getCentre();
    auto distFromCentre = point.getDistanceFrom(center);
    auto outerRadius = juce::jmin(outerBounds.getWidth(), outerBounds.getHeight()) / 2.0f;
    float maxSliderClickableRadius = outerRadius * 1.1f;

    if (distFromCentre <= maxSliderClickableRadius)
    {
        return true;
    }

    if (isModulated && getModulationHandleBounds().contains((float) x, (float) y))
    {
        return true;
    }

    return false;
}

juce::Rectangle<float> ModulatableSlider::getModulationHandleBounds() const
{
    auto bounds = getRotarySliderBounds().reduced(juce::jmax(5.0f, 7.0f * getUiScale()));
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    float handleSize = radius * 0.4f;

    float handleCenterX = bounds.getCentreX() + (radius * 0.85f);
    float handleCenterY = bounds.getCentreY() + (radius * 0.85f);

    return juce::Rectangle<float>(handleSize, handleSize).withCentre({ handleCenterX, handleCenterY });
}

float ModulatableSlider::getUiScale() const noexcept
{
    if (const auto* fireLookAndFeel = dynamic_cast<const FireLookAndFeel*>(&getLookAndFeel()))
        return juce::jmax(0.1f, fireLookAndFeel->scale);

    return 1.0f;
}

juce::Rectangle<float> ModulatableSlider::getRotarySliderBounds() const
{
    auto& mutableSlider = const_cast<ModulatableSlider&>(*this);
    return getLookAndFeel().getSliderLayout(mutableSlider).sliderBounds.toFloat();
}

juce::Rectangle<int> ModulatableSlider::getHeaderBounds() const
{
    auto header = getLocalBounds();
    const auto rotaryBounds = getRotarySliderBounds();
    header.setBottom(juce::jlimit(0, getHeight(), juce::roundToInt(rotaryBounds.getY())));
    return header;
}

// New helper function
bool ModulatableSlider::isMouseOverMainSlider() const
{
    // The main slider is highlighted only if the mouse is over the component
    // in general, but NOT specifically over the modulation handle.
    return isMouseOver() && ! isModHandleMouseOver;
}

bool ModulatableSlider::advanceAnimation(float deltaSeconds) noexcept
{
    deltaSeconds = juce::jlimit(0.0f, 0.05f, deltaSeconds);
    const auto oldHover = hoverAnimation;
    const auto oldPress = pressAnimation;
    const auto oldHandleHover = modulationHandleHoverAnimation;
    const auto oldHandlePress = modulationHandlePressAnimation;
    const auto hoverTarget = isMouseOverMainSlider() && isEnabled() ? 1.0f : 0.0f;
    const auto pressTarget = (isDraggingMainSlider || (isMouseButtonDown() && ! isModHandleMouseDown))
                                 && isEnabled()
                             ? 1.0f
                             : 0.0f;
    const auto handleHoverTarget = isModulated && isModHandleMouseOver
                                       && isEnabled()
                                   ? 1.0f
                                   : 0.0f;
    const auto handlePressTarget = isModulated && isModHandleMouseDown
                                       && isEnabled()
                                   ? 1.0f
                                   : 0.0f;
    const auto hoverStep = juce::jmin(1.0f, deltaSeconds * 10.0f);
    const auto pressStep = juce::jmin(1.0f, deltaSeconds * 16.0f);
    const auto handleHoverStep = juce::jmin(1.0f, deltaSeconds * 12.0f);
    const auto handlePressStep = juce::jmin(1.0f, deltaSeconds * 18.0f);
    hoverAnimation += (hoverTarget - hoverAnimation) * hoverStep;
    pressAnimation += (pressTarget - pressAnimation) * pressStep;
    modulationHandleHoverAnimation +=
        (handleHoverTarget - modulationHandleHoverAnimation)
        * handleHoverStep;
    modulationHandlePressAnimation +=
        (handlePressTarget - modulationHandlePressAnimation)
        * handlePressStep;

    if (std::abs(hoverAnimation - hoverTarget) < 0.002f)
        hoverAnimation = hoverTarget;
    if (std::abs(pressAnimation - pressTarget) < 0.002f)
        pressAnimation = pressTarget;
    if (std::abs(modulationHandleHoverAnimation - handleHoverTarget) < 0.002f)
        modulationHandleHoverAnimation = handleHoverTarget;
    if (std::abs(modulationHandlePressAnimation - handlePressTarget) < 0.002f)
        modulationHandlePressAnimation = handlePressTarget;

    return std::abs(oldHover - hoverAnimation) > 0.001f
           || std::abs(oldPress - pressAnimation) > 0.001f
           || std::abs(oldHandleHover - modulationHandleHoverAnimation) > 0.001f
           || std::abs(oldHandlePress - modulationHandlePressAnimation) > 0.001f;
}

// New/updated mouse handlers
void ModulatableSlider::mouseMove(const juce::MouseEvent& event)
{
    auto safeThis = juce::Component::SafePointer<ModulatableSlider>(this);
    juce::Slider::mouseMove(event);
    if (! safeThis)
        return;

    if (! recoverMissingPointerUp(event) || ! safeThis)
        return;

    const bool isOverHandleNow = isModulated
        && getModulationHandleBounds().contains(event.getPosition().toFloat());

    if (isOverHandleNow != isModHandleMouseOver)
    {
        isModHandleMouseOver = isOverHandleNow;
        repaint();

        auto callback = isModHandleMouseOver ? onHoverStart : onHoverEnd;
        if (callback)
            callback(this);
    }
}

void ModulatableSlider::mouseEnter(const juce::MouseEvent& event)
{
    auto safeThis = juce::Component::SafePointer<ModulatableSlider>(this);
    stopTimer();
    juce::Slider::mouseEnter(event);
    if (! safeThis)
        return;

    mouseMove(event);
    if (! safeThis)
        return;

    label.setVisible(false);
    if (! safeThis)
        return;

    const auto uiScale = getUiScale();
    setTextBoxStyle(juce::Slider::TextBoxAbove, false,
                    juce::roundToInt(TEXTBOX_WIDTH * uiScale),
                    juce::roundToInt(TEXTBOX_HEIGHT * uiScale));
}

void ModulatableSlider::mouseExit(const juce::MouseEvent& event)
{
    auto safeThis = juce::Component::SafePointer<ModulatableSlider>(this);
    juce::Slider::mouseExit(event);
    if (! safeThis)
        return;

    if (! recoverMissingPointerUp(event) || ! safeThis)
        return;

    const bool endedHandleHover = isModHandleMouseOver;
    if (isModHandleMouseOver)
    {
        isModHandleMouseOver = false;
        repaint();
    }

    startTimer(100);

    if (endedHandleHover)
    {
        auto callback = onHoverEnd;
        if (callback)
            callback(this);
    }
}

void ModulatableSlider::mouseDoubleClick(const juce::MouseEvent& event)
{
    // Slider's base implementation rejects double-clicks after a component is
    // disabled. Preserve that guarantee for the custom modulation-handle path
    // too: a mouseUp callback may disable the control before JUCE dispatches
    // the trailing mouseDoubleClick event.
    if (! isEnabled())
        return;

    if (shouldSuppressAssignmentDoubleClick(event))
    {
        clearAssignmentDoubleClickSuppression();
        return;
    }

    if (! isCompletePrimaryDown(event)
        || (activePointerGesture != PointerGesture::none
            && (! isPointerSource(event)
                || (activePointerGesture != PointerGesture::mainSlider
                    && activePointerGesture != PointerGesture::modulationHandle))))
        return;

    // Check if the double-click is on the modulation handle
    if (isModulated && getModulationHandleBounds().contains(event.getPosition().toFloat()))
    {
        repaint();
        auto callback = onModulationReset;
        if (callback)
            callback();
    }
    else
    {
        // Otherwise, perform the default slider action (resetting the slider's value)
        juce::Slider::mouseDoubleClick(event);
    }
}

void ModulatableSlider::mouseDown(const juce::MouseEvent& event)
{
    auto safeThis = juce::Component::SafePointer<ModulatableSlider>(this);

    if (activePointerGesture != PointerGesture::none)
    {
        // A second input source cannot steal a gesture. A fresh down from the
        // owning source is a reliable lifecycle boundary when a host omitted
        // the previous mouseUp.
        if (! isPointerSource(event))
            return;

        if (activePointerGesture == PointerGesture::assignment
            && event.getNumberOfClicks() > 1)
        {
            // The first click may synchronously clear assign mode. Keep the
            // remainder of that physical double-click owned by the assignment
            // sequence so its second down cannot start a parameter drag.
            lastAcceptedPointerEvent.emplace(event);
            return;
        }

        finishActivePointerGesture(event);
        if (! safeThis)
            return;
    }

    if (suppressAssignmentDoubleClick)
    {
        const bool suppressThisDown = shouldSuppressAssignmentDoubleClick(event);
        if (suppressThisDown)
        {
            // JUCE delivers the second mouseUp before mouseDoubleClick. Keep
            // this suppression alive while the rejected down/up pair closes,
            // then let mouseDoubleClick consume it.
            beginPointerGesture(PointerGesture::rejected, event);
            return;
        }

        clearAssignmentDoubleClickSuppression();
    }

    if (! isEnabled())
        return;

    if (! isCompletePrimaryDown(event))
    {
        const bool isPopupDown = isStandalonePopupDown(event);
        beginPointerGesture(isPopupDown ? PointerGesture::popupMenu
                                        : PointerGesture::rejected,
                            event);
        if (isPopupDown)
        {
            popupMenuTarget = isModulated
                                      && getModulationHandleBounds().contains(
                                          event.getPosition().toFloat())
                                  ? PopupMenuTarget::modulationHandle
                                  : PopupMenuTarget::mainSlider;
            popupTargetParameterID = parameterID;
        }
        return;
    }

    if (onClickInAssignMode)
    {
        // Assigning a target exits assign mode, which clears the callback on
        // every slider (including this one).  Keep the callable alive until
        // it returns instead of destroying the std::function target while it
        // is still executing.
        beginPointerGesture(PointerGesture::assignment, event);
        auto assignCallback = onClickInAssignMode;
        const auto targetParameterID = parameterID;
        assignCallback(targetParameterID);
        return;
    }

    const bool isOnModulationHandle = isModulated
        && getModulationHandleBounds().contains(event.getPosition().toFloat());

    // Test the actual mouse-down position rather than relying on the last
    // mouseMove event. A fast click can otherwise start a main-slider drag
    // while the pointer is already over the modulation handle.
    if (isOnModulationHandle)
    {
        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            auto callback = onBipolarModeToggled;
            const auto targetParameterID = parameterID;
            if (callback)
                callback(targetParameterID);

            return;
        }

        beginPointerGesture(PointerGesture::modulationHandle, event);
        initialLfoAmount = lfoAmount;
        repaint();

        auto callback = onModDragStart;
        if (callback)
            callback(this);

        return;
    }

    beginPointerGesture(PointerGesture::mainSlider, event);
    juce::Slider::mouseDown(event);
    if (! safeThis)
        return;

    auto callback = onMainDragStart;
    if (callback)
        callback(this);
}

void ModulatableSlider::mouseDrag(const juce::MouseEvent& event)
{
    if (! hasActiveInteraction() || ! isPointerSource(event))
        return;

    lastAcceptedPointerEvent.emplace(event);
    auto safeThis = juce::Component::SafePointer<ModulatableSlider>(this);

    if (activePointerGesture == PointerGesture::mainSlider)
    {
        juce::Slider::mouseDrag(event);
        if (! safeThis)
            return;

        auto callback = onMainDragMove;
        if (callback)
            callback(this);
    }
    else if (activePointerGesture == PointerGesture::modulationHandle)
    {
        auto diff = event.getPosition() - event.getMouseDownPosition();

        // Use the initial amount for a more stable drag interaction
        auto newAmount = initialLfoAmount - diff.y * 0.005;

        // Allow amount to be from -1.0 to 1.0
        lfoAmount = juce::jlimit(-1.0, 1.0, newAmount);
        repaint();

        // If the callback is set, call it to notify the processor
        auto amountChangedCallback = onModAmountChanged;
        if (amountChangedCallback)
        {
            amountChangedCallback(lfoAmount);
            if (! safeThis)
                return;
        }

        auto dragCallback = onModDragMove;
        if (dragCallback)
            dragCallback(this);
    }
}

void ModulatableSlider::mouseUp(const juce::MouseEvent& event)
{
    if (activePointerGesture != PointerGesture::none)
    {
        // Once a primary gesture has been accepted, only its source can close
        // it. Modifier flags on the release are intentionally ignored: macOS
        // can report a popup modifier if Control is pressed before mouseUp.
        if (! isPointerSource(event))
            return;

        const auto completedGesture = activePointerGesture;
        const auto completedPopupTarget = popupMenuTarget;
        const auto completedPopupParameterID = popupTargetParameterID;
        const auto completedSourceType = pointerSourceType;
        const auto completedSourceIndex = pointerSourceIndex;
        if (! finishActivePointerGesture(event))
            return;

        if (completedGesture == PointerGesture::assignment)
        {
            suppressAssignmentDoubleClick = true;
            assignmentSourceType = completedSourceType;
            assignmentSourceIndex = completedSourceIndex;
            assignmentDoubleClickDeadlineMs = event.eventTime.toMilliseconds()
                                              + juce::MouseEvent::getDoubleClickTimeout();
            return;
        }

        if (completedGesture != PointerGesture::popupMenu)
            return;

        juce::PopupMenu menu;
        if (completedPopupTarget == PopupMenuTarget::modulationHandle)
        {
            menu.addItem(1, "Set Value");
            menu.addItem(2, "Clear LFO");
            menu.addItem(3, "Invert Depth");

            juce::String bipolarToggleText = isBipolar ? "Switch to Unipolar" : "Switch to Bipolar";
            menu.addItem(4, bipolarToggleText);

            juce::String bypassToggleText = isBypassed ? "Enable modulation" : "Bypass modulation";
            menu.addItem(5, bypassToggleText);

            menu.showMenuAsync(fire::ui::prepareContextMenu(
                                   menu, *this, event.getScreenPosition()),
                               createModulationMenuResultHandler(
                                   completedPopupParameterID));
        }
        else if (completedPopupTarget == PopupMenuTarget::mainSlider
                 && completedPopupParameterID.isNotEmpty())
        {
            menu.addSectionHeader("Assign modulation");
            for (int lfoIndex = 0; lfoIndex < 4; ++lfoIndex)
                menu.addItem(lfoIndex + 1,
                             "LFO " + juce::String(lfoIndex + 1),
                             true,
                             isModulated && lfoSource == lfoIndex + 1);

            menu.showMenuAsync(fire::ui::prepareContextMenu(
                                   menu, *this, event.getScreenPosition()),
                               createLfoAssignmentMenuResultHandler(
                                   completedPopupParameterID));
        }

        return;
    }
}

bool ModulatableSlider::isCompletePrimaryDown(
    const juce::MouseEvent& event) const noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown()
        && ! event.mods.isPopupMenu();
}

bool ModulatableSlider::isStandalonePopupDown(
    const juce::MouseEvent& event) noexcept
{
    return event.mods.isPopupMenu()
        && ! event.mods.isMiddleButtonDown()
        && ! (event.mods.isLeftButtonDown()
              && event.mods.isRightButtonDown());
}

bool ModulatableSlider::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

bool ModulatableSlider::recoverMissingPointerUp(
    const juce::MouseEvent& event)
{
    if (activePointerGesture == PointerGesture::none
        || ! isPointerSource(event))
        return true;

    const bool ownsPrimaryButton =
        activePointerGesture == PointerGesture::assignment
        || activePointerGesture == PointerGesture::mainSlider
        || activePointerGesture == PointerGesture::modulationHandle;
    const bool owningButtonStillDown = ownsPrimaryButton
                                           ? event.mods.isLeftButtonDown()
                                           : event.mods.isAnyMouseButtonDown();
    if (owningButtonStillDown)
        return true;

    // A host or window manager may omit mouseUp while pointer capture changes.
    // Treat the first same-source hover event without the gesture's owning
    // button as a cancelled release. An unrelated button must not leave a
    // primary SliderAttachment gesture open, and this path never opens a menu.
    return finishActivePointerGesture(event);
}

bool ModulatableSlider::shouldSuppressAssignmentDoubleClick(
    const juce::MouseEvent& event) const noexcept
{
    return suppressAssignmentDoubleClick
        && event.getNumberOfClicks() > 1
        && event.source.getType() == assignmentSourceType
        && event.source.getIndex() == assignmentSourceIndex
        && event.eventTime.toMilliseconds() <= assignmentDoubleClickDeadlineMs;
}

void ModulatableSlider::clearAssignmentDoubleClickSuppression() noexcept
{
    suppressAssignmentDoubleClick = false;
    assignmentSourceIndex = -1;
    assignmentDoubleClickDeadlineMs = 0;
}

void ModulatableSlider::beginPointerGesture(
    PointerGesture gesture,
    const juce::MouseEvent& event)
{
    activePointerGesture = gesture;
    isDraggingMainSlider = gesture == PointerGesture::mainSlider;
    isModHandleMouseDown = gesture == PointerGesture::modulationHandle;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();
    lastAcceptedPointerEvent.emplace(event);
}

bool ModulatableSlider::finishActivePointerGesture(
    const juce::MouseEvent& releaseEvent)
{
    const auto completedGesture = activePointerGesture;
    if (completedGesture == PointerGesture::none)
        return true;

    // Reset ownership before invoking JUCE or user callbacks. Any re-entrant
    // cleanup is therefore idempotent and cannot emit a second drag end.
    activePointerGesture = PointerGesture::none;
    isDraggingMainSlider = false;
    isModHandleMouseDown = false;
    pointerSourceIndex = -1;
    lastAcceptedPointerEvent.reset();
    popupMenuTarget = PopupMenuTarget::none;
    popupTargetParameterID.clear();
    repaint();

    auto safeThis = juce::Component::SafePointer<ModulatableSlider>(this);
    if (completedGesture == PointerGesture::mainSlider)
    {
        // This must precede onMainDragEnd: SliderAttachment closes the host
        // beginGesture from mouseDown in Slider::mouseUp.
        juce::Slider::mouseUp(releaseEvent);
        if (! safeThis)
            return false;

        auto callback = onMainDragEnd;
        if (callback)
        {
            callback(this);
            if (! safeThis)
                return false;
        }
    }
    else if (completedGesture == PointerGesture::modulationHandle)
    {
        auto callback = onModDragEnd;
        if (callback)
        {
            callback(this);
            if (! safeThis)
                return false;
        }
    }

    if (completedGesture == PointerGesture::mainSlider
        || completedGesture == PointerGesture::modulationHandle)
    {
        auto interactionCallback = onInteractionEnded;
        if (interactionCallback)
        {
            interactionCallback();
            if (! safeThis)
                return false;
        }
    }

    return true;
}

bool ModulatableSlider::resetTransientPresentation()
{
    const auto safeThis =
        juce::Component::SafePointer<ModulatableSlider>(this);
    const bool timerWasRunning = isTimerRunning();
    const bool presentationChanged = timerWasRunning
                                     || isModHandleMouseOver
                                     || hoverAnimation != 0.0f
                                     || pressAnimation != 0.0f
                                     || modulationHandleHoverAnimation != 0.0f
                                     || modulationHandlePressAnimation != 0.0f
                                     || ! label.isVisible()
                                     || getTextBoxPosition() != juce::Slider::NoTextBox;
    if (timerWasRunning)
        stopTimer();
    isModHandleMouseOver = false;
    hoverAnimation = 0.0f;
    pressAnimation = 0.0f;
    modulationHandleHoverAnimation = 0.0f;
    modulationHandlePressAnimation = 0.0f;
    if (! label.isVisible())
    {
        label.setVisible(true);
        if (! safeThis)
            return false;
    }

    if (getTextBoxPosition() != juce::Slider::NoTextBox)
    {
        // Slider::hideTextBox updates its private value label after asking the
        // label to hide its editor. Hide the editor explicitly first so an
        // onEditorHide callback may delete this Slider before Pimpl continues.
        if (auto* valueLabel = forwardedValueLabel.getComponent();
            valueLabel != nullptr && valueLabel->isBeingEdited())
        {
            valueLabel->hideEditor(true);
            if (! safeThis)
                return false;
        }

        hideTextBox(true);
        if (! safeThis)
            return false;

        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        if (! safeThis)
            return false;
    }
    if (presentationChanged)
        repaint();

    return safeThis != nullptr;
}

void ModulatableSlider::dismissTransientInteraction()
{
    dismissTransientInteractionImpl(true);
}

void ModulatableSlider::dismissTransientInteractionPreservingContextMenu()
{
    dismissTransientInteractionImpl(false);
}

void ModulatableSlider::dismissTransientInteractionImpl(
    bool invalidateContextMenu)
{
    const auto safeThis =
        juce::Component::SafePointer<ModulatableSlider>(this);

    // Invalidate before any drag-end or hover callback. Those callbacks may
    // re-enter UI code, and an abandoned menu must already be inert there.
    if (invalidateContextMenu)
        ++contextMenuRevision;

    const bool shouldNotifyHoverEnd = isModHandleMouseOver;
    std::optional<juce::MouseEvent> releaseEvent;
    if (lastAcceptedPointerEvent.has_value())
        releaseEvent.emplace(*lastAcceptedPointerEvent);

    if (! resetTransientPresentation() || ! safeThis)
        return;

    clearAssignmentDoubleClickSuppression();

    if (activePointerGesture != PointerGesture::none)
    {
        // Every accepted gesture stores its most recent event. Keeping a copy
        // here lets finishActivePointerGesture clear its state before calling
        // the base Slider::mouseUp.
        jassert(releaseEvent.has_value());
        if (releaseEvent.has_value())
            finishActivePointerGesture(*releaseEvent);

        if (! safeThis)
            return;
    }

    if (shouldNotifyHoverEnd)
    {
        auto callback = onHoverEnd;
        if (callback)
            callback(this);
    }
}

std::function<void(int)> ModulatableSlider::createModulationMenuResultHandler(
    juce::String targetParameterIDAtOpen)
{
    if (targetParameterIDAtOpen.isEmpty())
        targetParameterIDAtOpen = parameterID;

    const auto sessionRevision = ++contextMenuRevision;
    return [safeThis = juce::Component::SafePointer<ModulatableSlider>(this),
            sessionRevision,
            frozenTargetParameterID = std::move(targetParameterIDAtOpen)](int result)
    {
        if (! safeThis
            || safeThis->contextMenuRevision != sessionRevision)
            return;

        // Consume before dispatching user code. A processor/host callback may
        // synchronously delete this slider, and duplicate PopupMenu results
        // must never execute the same command twice.
        ++safeThis->contextMenuRevision;
        if (result <= 0)
            return;

        safeThis->executeModulationMenuCommand(
            static_cast<ModulationMenuCommand>(result),
            frozenTargetParameterID);
    };
}

std::function<void(int)> ModulatableSlider::createLfoAssignmentMenuResultHandler(
    juce::String targetParameterIDAtOpen)
{
    if (targetParameterIDAtOpen.isEmpty())
        targetParameterIDAtOpen = parameterID;

    const auto sessionRevision = ++contextMenuRevision;
    return [safeThis = juce::Component::SafePointer<ModulatableSlider>(this),
            sessionRevision,
            frozenTargetParameterID = std::move(targetParameterIDAtOpen)](int result)
    {
        if (! safeThis
            || safeThis->contextMenuRevision != sessionRevision)
            return;

        ++safeThis->contextMenuRevision;
        if (! juce::isPositiveAndBelow(result - 1, 4)
            || frozenTargetParameterID.isEmpty())
            return;

        auto callback = safeThis->onLfoAssignmentRequested;
        if (callback)
            callback(result - 1, frozenTargetParameterID);
    };
}

void ModulatableSlider::executeModulationMenuCommand(
    ModulationMenuCommand command,
    const juce::String& targetParameterID)
{
    if (targetParameterID.isEmpty())
        return;

    switch (command)
    {
        case ModulationMenuCommand::setValue:
        {
            auto callback = onSetValueRequested;
            if (callback)
                callback(this, targetParameterID);
            break;
        }
        case ModulationMenuCommand::clearModulation:
        {
            auto callback = onModulationCleared;
            if (callback)
                callback(targetParameterID);
            break;
        }
        case ModulationMenuCommand::invertDepth:
        {
            auto callback = onModulationInverted;
            if (callback)
                callback(targetParameterID);
            break;
        }
        case ModulationMenuCommand::togglePolarity:
        {
            auto callback = onBipolarModeToggled;
            if (callback)
                callback(targetParameterID);
            break;
        }
        case ModulationMenuCommand::toggleBypass:
        {
            auto callback = onBypassToggled;
            if (callback)
                callback(targetParameterID);
            break;
        }
        default:
            break;
    }
}

void ModulatableSlider::setLabel(const juce::String& text, juce::Colour colour)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, colour);
    setTitle(text);
}

void ModulatableSlider::resized()
{
    // First, call the base class's resized() to let it draw the slider itself.
    juce::Slider::resized();

    // Match the exact header reserved by FireLookAndFeel. This keeps the static
    // label, hover text editor and rotary drawing area aligned at every zoom.
    const auto headerBounds = getHeaderBounds();
    label.setBounds(headerBounds);
    label.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(juce::jmax(1.0f,
                                   juce::jmin(KNOB_FONT_SIZE * getUiScale(),
                                              static_cast<float>(headerBounds.getHeight()) * 0.68f)))
            .withStyle("Plain") });
}

void ModulatableSlider::timerCallback()
{
    const auto safeThis =
        juce::Component::SafePointer<ModulatableSlider>(this);
    // This function is called when the timer finishes.
    stopTimer();

    // Check if the mouse is truly outside the slider and its children (like the textbox).
    if (! isMouseOver(true))
    {
        label.setVisible(true);
        if (! safeThis)
            return;

        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }
}

void ModulatableSlider::visibilityChanged()
{
    const auto safeThis =
        juce::Component::SafePointer<ModulatableSlider>(this);
    juce::Slider::visibilityChanged();
    if (! safeThis)
        return;

    if (! isShowing())
        dismissTransientInteraction();
}

void ModulatableSlider::enablementChanged()
{
    const auto safeThis =
        juce::Component::SafePointer<ModulatableSlider>(this);
    juce::Slider::enablementChanged();
    if (! safeThis)
        return;

    // Enabling is also a lifecycle boundary. A physical button may still be
    // held after the control was disabled, and must not revive that gesture.
    dismissTransientInteraction();
}
