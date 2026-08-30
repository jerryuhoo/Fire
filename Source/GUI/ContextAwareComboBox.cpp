/*
  ==============================================================================

    ContextAwareComboBox.cpp

  ==============================================================================
*/

#include "ContextAwareComboBox.h"

#include <utility>

namespace
{
void commitNormalizedParameterValue(juce::RangedAudioParameter& parameter,
                                    float normalizedValue)
{
    if (juce::approximatelyEqual(parameter.getValue(), normalizedValue))
        return;

    // The parameter belongs to the processor and outlives this editor. Keep
    // the complete host call stack free of ComboBox/attachment state: any one
    // of these callbacks may synchronously delete the owning panel.
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost(normalizedValue);
    parameter.endChangeGesture();
}
} // namespace

void ContextAwareComboBox::configurePopupSession(
    GenerationProvider generationProvider,
    ContextValidator contextValidator,
    juce::RangedAudioParameter* parameter)
{
    getCurrentGeneration = std::move(generationProvider);
    isPopupContextValid = std::move(contextValidator);
    boundParameter = parameter;
}

void ContextAwareComboBox::capturePopupRequest() noexcept
{
    popupRequestGeneration = getCurrentGeneration != nullptr
                                 ? getCurrentGeneration()
                                 : 0;
    popupRequestArmed = true;
}

bool ContextAwareComboBox::isContextCurrent(
    std::uint64_t contextGeneration) const
{
    return isEnabled()
           && isShowing()
           && getCurrentGeneration != nullptr
           && isPopupContextValid != nullptr
           && getCurrentGeneration() == contextGeneration
           && isPopupContextValid();
}

bool ContextAwareComboBox::keyPressed(const juce::KeyPress& key)
{
    const bool movesBackward = key == juce::KeyPress::upKey
                               || key == juce::KeyPress::leftKey;
    const bool movesForward = key == juce::KeyPress::downKey
                              || key == juce::KeyPress::rightKey;
    if (movesBackward || movesForward)
    {
        // Popup windows own their keyboard navigation. A queued popup request
        // must also consume directions until showPopup() has classified it.
        if (isPopupActive() || popupRequestArmed)
            return true;

        const auto contextGeneration = getCurrentGeneration != nullptr
                                           ? getCurrentGeneration()
                                           : 0;
        if (! isContextCurrent(contextGeneration))
            return true;

        const auto delta = movesBackward ? -1 : 1;
        const auto itemCount = getNumItems();
        for (int itemIndex = getSelectedItemIndex() + delta;
             juce::isPositiveAndBelow(itemIndex, itemCount);
             itemIndex += delta)
        {
            const auto itemId = getItemId(itemIndex);
            if (itemId == 0 || ! isItemEnabled(itemId))
                continue;

            auto* const parameter = boundParameter;
            const auto normalizedValue = itemCount > 1
                                             ? static_cast<float>(itemIndex)
                                                   / static_cast<float>(itemCount - 1)
                                             : 0.0f;
            if (parameter == nullptr
                || ! isContextCurrent(contextGeneration))
                return true;

            // A direction key is a complete decision at this point. Invalidate
            // a result from any menu that has just closed, then submit directly
            // instead of queuing ComboBoxAttachment::comboBoxChanged().
            popupSessionActive = false;
            ++popupSessionRevision;
            commitNormalizedParameterValue(*parameter, normalizedValue);
            return true;
        }

        return true;
    }

    if (key == juce::KeyPress::returnKey && ! isPopupActive())
    {
        // A previous showPopup() may still be queued after its context was
        // invalidated. Do not overwrite its captured generation or enqueue a
        // second call before the first one has consumed the request.
        if (popupRequestArmed)
            return true;

        capturePopupRequest();
    }

    return juce::ComboBox::keyPressed(key);
}

void ContextAwareComboBox::mouseDown(const juce::MouseEvent& event)
{
    if (! isEnabled() || ! isCompletePrimaryDown(event))
        return;

    // A previous showPopup() may still be queued with another generation.
    // Classify that request before accepting any later physical press.
    if (popupRequestArmed && ! isPopupActive())
        return;

    const juce::Component::SafePointer<ContextAwareComboBox> safeThis(this);
    if (pointerInteractionActive)
    {
        if (! isPointerSource(event))
            return;

        // A fresh down from the owning source is a lifecycle boundary when a
        // host omitted the previous release. Clear ComboBox's private pressed
        // bit without selecting or disturbing an already-open popup session.
        releasePointerInteractionWithoutSelection(event);
        if (safeThis == nullptr)
            return;
    }

    pointerInteractionGeneration = getCurrentGeneration != nullptr
                                       ? getCurrentGeneration()
                                       : 0;
    pointerInteractionActive = true;
    cancelPendingPointerRelease = false;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();

    if (! isPopupActive())
        capturePopupRequest();

    juce::ComboBox::mouseDown(event);
    if (safeThis == nullptr)
        return;

    // Editable labels can decline to start a popup. Avoid leaving a request
    // armed when JUCE did not actually queue showPopup().
    if (! isPopupActive())
        popupRequestArmed = false;
}

void ContextAwareComboBox::mouseDrag(const juce::MouseEvent& event)
{
    if (! pointerInteractionActive
        || ! isPointerSource(event)
        || cancelPendingPointerRelease)
        return;

    const bool mayQueuePopup = pointerInteractionActive
                               && ! isPopupActive();

    if (mayQueuePopup)
    {
        if (getCurrentGeneration == nullptr
            || getCurrentGeneration() != pointerInteractionGeneration
            || popupRequestArmed)
            return;

        popupRequestGeneration = pointerInteractionGeneration;
        popupRequestArmed = true;
    }

    const juce::Component::SafePointer<ContextAwareComboBox> safeThis(this);
    juce::ComboBox::mouseDrag(event);
    if (safeThis == nullptr)
        return;

    if (mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;
}

void ContextAwareComboBox::mouseEnter(const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ContextAwareComboBox> safeThis(this);
    juce::ComboBox::mouseEnter(event);

    if (safeThis != nullptr)
    {
        recoverMissingPointerUp(event);

        if (safeThis != nullptr)
            repaint();
    }
}

void ContextAwareComboBox::mouseMove(const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ContextAwareComboBox> safeThis(this);
    juce::ComboBox::mouseMove(event);

    if (safeThis != nullptr)
    {
        recoverMissingPointerUp(event);

        if (safeThis != nullptr)
            repaint();
    }
}

void ContextAwareComboBox::mouseExit(const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ContextAwareComboBox> safeThis(this);
    juce::ComboBox::mouseExit(event);

    if (safeThis != nullptr)
    {
        recoverMissingPointerUp(event);

        if (safeThis != nullptr)
            repaint();
    }
}

void ContextAwareComboBox::mouseUp(const juce::MouseEvent& event)
{
    if (! pointerInteractionActive || ! isPointerSource(event))
        return;

    if (cancelPendingPointerRelease)
    {
        releasePointerInteractionWithoutSelection(event);
        return;
    }

    const bool mayQueuePopup = pointerInteractionActive
                               && ! isPopupActive();

    if (mayQueuePopup)
    {
        if (getCurrentGeneration == nullptr
            || getCurrentGeneration() != pointerInteractionGeneration
            || popupRequestArmed)
        {
            releasePointerInteractionWithoutSelection(event);
            return;
        }

        popupRequestGeneration = pointerInteractionGeneration;
        popupRequestArmed = true;
    }

    const juce::Component::SafePointer<ContextAwareComboBox> safeThis(this);
    juce::ComboBox::mouseUp(event);
    if (safeThis == nullptr)
        return;

    if (mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;

    clearPointerInteraction();
}

bool ContextAwareComboBox::isCompletePrimaryDown(
    const juce::MouseEvent& event) const noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown()
        && ! event.mods.isPopupMenu();
}

bool ContextAwareComboBox::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void ContextAwareComboBox::recoverMissingPointerUp(
    const juce::MouseEvent& event)
{
    if (pointerInteractionActive
        && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
    {
        // Releasing ComboBox's private pressed bit can synchronously invoke UI
        // listeners, so keep it as the final operation in this recovery path.
        releasePointerInteractionWithoutSelection(event);
    }
}

void ContextAwareComboBox::releasePointerInteractionWithoutSelection(
    const juce::MouseEvent& event)
{
    // Clear custom ownership first because ComboBox::mouseUp may delete this
    // control. Popup request/session generations deliberately remain intact.
    clearPointerInteraction();
    juce::ComboBox::mouseUp(
        event.getEventRelativeTo(this).withNewPosition(
            juce::Point<float> { -1.0f, -1.0f }));
}

void ContextAwareComboBox::clearPointerInteraction() noexcept
{
    pointerInteractionActive = false;
    cancelPendingPointerRelease = false;
    pointerSourceIndex = -1;
}

void ContextAwareComboBox::mouseWheelMove(
    const juce::MouseEvent& event,
    const juce::MouseWheelDetails& wheel)
{
    // A ComboBox wheel nudge posts an asynchronous change notification, which
    // cannot retain this control's context identity and lets the stock
    // ComboBoxAttachment own the UI-to-host call stack. Keep slope/mode changes
    // deliberate and pass scrolling to the nearest enabled ancestor instead.
    juce::Component::mouseWheelMove(event, wheel);
}

void ContextAwareComboBox::closePopupWindow() noexcept
{
    juce::ComboBox::hidePopup();
}

void ContextAwareComboBox::dismissTransientInteraction() noexcept
{
    popupSessionActive = false;
    ++popupSessionRevision;
    cancelPendingPointerRelease = cancelPendingPointerRelease
                                  || pointerInteractionActive;

    // Keep popupRequestArmed intact. JUCE may already have queued the virtual
    // showPopup() call; that call must observe its old generation and reject
    // itself instead of being reclassified as a new direct request.
    closePopupWindow();
}

std::function<void(int)> ContextAwareComboBox::createPopupResultHandler()
{
    const auto contextGeneration = getCurrentGeneration != nullptr
                                       ? getCurrentGeneration()
                                       : 0;
    return createPopupResultHandler(contextGeneration);
}

std::function<void(int)> ContextAwareComboBox::createPopupResultHandler(
    std::uint64_t contextGeneration)
{
    popupSessionActive = true;
    const auto sessionRevision = ++popupSessionRevision;

    return [safeThis = juce::Component::SafePointer<ContextAwareComboBox>(this),
            contextGeneration,
            sessionRevision](int result)
    {
        if (safeThis == nullptr
            || ! safeThis->popupSessionActive
            || safeThis->popupSessionRevision != sessionRevision)
            return;

        const auto selectedIndex = safeThis->indexOfItemId(result);
        const auto itemCount = safeThis->getNumItems();
        auto* const parameter = safeThis->boundParameter;
        const auto normalizedValue = itemCount > 1
                                         ? static_cast<float>(selectedIndex)
                                               / static_cast<float>(itemCount - 1)
                                         : 0.0f;
        const bool mayCommit = result != 0
                               && safeThis->isContextCurrent(contextGeneration)
                               && selectedIndex >= 0
                               && safeThis->isItemEnabled(result)
                               && parameter != nullptr;

        // Consume the session before notifying listeners. ComboBoxAttachment
        // synchronously notifies the host and that callback may destroy the UI.
        safeThis->popupSessionActive = false;
        ++safeThis->popupSessionRevision;
        safeThis->cancelPendingPointerRelease =
            safeThis->cancelPendingPointerRelease
            || safeThis->pointerInteractionActive;
        safeThis->closePopupWindow();

        if (! mayCommit
            || safeThis == nullptr
            || ! safeThis->isContextCurrent(contextGeneration))
            return;

        // Submit through the processor-owned parameter, then let the surviving
        // ComboBoxAttachment perform its normal parameter-to-UI update (and
        // accessibility notification). It must not own the UI-to-parameter
        // call stack: a synchronous host callback may delete the panel and its
        // attachment during setValueNotifyingHost().
        commitNormalizedParameterValue(*parameter, normalizedValue);
    };
}

void ContextAwareComboBox::showPopup()
{
    // Accessibility actions call showPopup() directly, whereas mouse and key
    // input arrive through ComboBox::showPopupIfNotActive(). Route direct calls
    // through the latter too so JUCE's private menuActive state remains correct.
    if (! popupRequestArmed)
    {
        if (isPopupActive())
            return;

        capturePopupRequest();
        juce::ComboBox::keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey });
        return;
    }

    const auto requestGeneration = popupRequestGeneration;
    popupRequestArmed = false;

    if (! isContextCurrent(requestGeneration))
    {
        popupSessionActive = false;
        ++popupSessionRevision;
        closePopupWindow();
        return;
    }

    auto menu = *getRootMenu();

    if (menu.getNumItems() > 0)
    {
        const auto selectedId = getSelectedId();

        for (juce::PopupMenu::MenuItemIterator iterator(menu, true);
             iterator.next();)
        {
            auto& item = iterator.getItem();

            if (item.itemID != 0)
                item.isTicked = item.itemID == selectedId;
        }
    }
    else
    {
        menu.addItem(1, getTextWhenNoChoicesAvailable(), false, false);
    }

    auto& lookAndFeel = getLookAndFeel();
    menu.setLookAndFeel(&lookAndFeel);

    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(this)
                       .withItemThatMustBeVisible(getSelectedId())
                       .withInitiallySelectedItem(getSelectedId())
                       .withMinimumWidth(getWidth())
                       .withMaximumNumColumns(1)
                       .withStandardItemHeight(getHeight());

    for (auto* child : getChildren())
    {
        if (auto* label = dynamic_cast<juce::Label*>(child))
        {
            options = lookAndFeel.getOptionsForComboBoxPopupMenu(*this, *label);
            break;
        }
    }

    menu.showMenuAsync(options,
                       createPopupResultHandler(requestGeneration));
}
