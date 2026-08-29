/*
  ==============================================================================

    ContextAwareComboBox.cpp

  ==============================================================================
*/

#include "ContextAwareComboBox.h"

#include <utility>

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
    return getCurrentGeneration != nullptr
           && isPopupContextValid != nullptr
           && getCurrentGeneration() == contextGeneration
           && isPopupContextValid();
}

bool ContextAwareComboBox::keyPressed(const juce::KeyPress& key)
{
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
    // Match ComboBox's own popup eligibility. Capturing a superset here is
    // harmless, while failing to capture an input that JUCE queues would lose
    // the context identity needed by showPopup().
    const bool mayStartPopup = isEnabled() && ! event.mods.isPopupMenu();

    if (mayStartPopup && popupRequestArmed && ! isPopupActive())
        return;

    if (mayStartPopup)
    {
        pointerInteractionGeneration = getCurrentGeneration != nullptr
                                           ? getCurrentGeneration()
                                           : 0;
        pointerInteractionActive = true;
        cancelPendingPointerRelease = false;

        if (! isPopupActive())
            capturePopupRequest();
    }

    juce::ComboBox::mouseDown(event);

    // Editable labels can decline to start a popup. Avoid leaving a request
    // armed when JUCE did not actually queue showPopup().
    if (mayStartPopup && ! isPopupActive())
        popupRequestArmed = false;
}

void ContextAwareComboBox::mouseDrag(const juce::MouseEvent& event)
{
    if (cancelPendingPointerRelease)
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

    juce::ComboBox::mouseDrag(event);

    if (mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;
}

void ContextAwareComboBox::mouseUp(const juce::MouseEvent& event)
{
    if (cancelPendingPointerRelease)
    {
        // ComboBox keeps its button-down bit private. Give it an outside
        // release so that bit is cleared without showPopupIfNotActive().
        juce::ComboBox::mouseUp(
            event.getEventRelativeTo(this).withNewPosition(
                juce::Point<float> { -1.0f, -1.0f }));
        cancelPendingPointerRelease = false;
        pointerInteractionActive = false;
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
            juce::ComboBox::mouseUp(
                event.getEventRelativeTo(this).withNewPosition(
                    juce::Point<float> { -1.0f, -1.0f }));
            pointerInteractionActive = false;
            return;
        }

        popupRequestGeneration = pointerInteractionGeneration;
        popupRequestArmed = true;
    }

    juce::ComboBox::mouseUp(event);

    if (mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;

    pointerInteractionActive = false;
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
        if (! juce::approximatelyEqual(parameter->getValue(), normalizedValue))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(normalizedValue);
            parameter->endChangeGesture();
        }
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
