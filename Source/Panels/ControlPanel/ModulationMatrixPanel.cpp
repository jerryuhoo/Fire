/*
  ==============================================================================

    ModulationMatrixPanel.cpp
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulationMatrixPanel.h"
#include "../../Utility/AudioHelpers.h"

#include <utility>

namespace
{
bool haveSameRoutingState(const ModulationRouting& lhs,
                          const ModulationRouting& rhs) noexcept
{
    return lhs.sourceLfoIndex == rhs.sourceLfoIndex
        && lhs.targetParameterID == rhs.targetParameterID
        && juce::exactlyEqual(lhs.depth, rhs.depth)
        && lhs.isBipolar == rhs.isBipolar
        && lhs.isBypassed == rhs.isBypassed;
}
} // namespace

void ModulationMatrixRoutingComboBox::configurePopupSession(
    EditContextProvider contextProvider,
    EditContextValidator contextValidator,
    SelectionCommitter selectionCommitter)
{
    getCurrentEditContext = std::move(contextProvider);
    isEditContextValid = std::move(contextValidator);
    commitSelection = std::move(selectionCommitter);
}

bool ModulationMatrixRoutingComboBox::capturePopupRequest()
{
    if (getCurrentEditContext == nullptr)
        return false;

    popupRequestContext = getCurrentEditContext();
    popupRequestInteractionGeneration = interactionGeneration;
    popupRequestArmed = true;
    return true;
}

bool ModulationMatrixRoutingComboBox::isContextCurrent(
    const EditContext& context) const
{
    return isEnabled()
        && isShowing()
        && isEditContextValid != nullptr
        && isEditContextValid(context);
}

bool ModulationMatrixRoutingComboBox::isPopupContextCurrent(
    const EditContext& context,
    std::uint64_t expectedInteractionGeneration) const
{
    return interactionGeneration == expectedInteractionGeneration
        && isContextCurrent(context);
}

bool ModulationMatrixRoutingComboBox::keyPressed(
    const juce::KeyPress& key)
{
    const bool movesBackward = key == juce::KeyPress::upKey
                               || key == juce::KeyPress::leftKey;
    const bool movesForward = key == juce::KeyPress::downKey
                              || key == juce::KeyPress::rightKey;
    if (movesBackward || movesForward)
    {
        if (isPopupActive() || popupRequestArmed)
            return true;

        if (getCurrentEditContext == nullptr)
            return true;

        const auto context = getCurrentEditContext();
        if (! isContextCurrent(context))
            return true;

        const auto delta = movesBackward ? -1 : 1;
        for (int itemIndex = getSelectedItemIndex() + delta;
             juce::isPositiveAndBelow(itemIndex, getNumItems());
             itemIndex += delta)
        {
            const auto itemId = getItemId(itemIndex);
            if (itemId == 0 || ! isItemEnabled(itemId))
                continue;

            return commitKeyboardSelection(itemId, context);
        }

        return true;
    }

    if (key == juce::KeyPress::returnKey && ! isPopupActive())
    {
        if (popupRequestArmed)
            return true;

        if (! capturePopupRequest())
            return true;
    }

    return juce::ComboBox::keyPressed(key);
}

bool ModulationMatrixRoutingComboBox::commitKeyboardSelection(
    int itemId,
    const EditContext& context)
{
    if (! isContextCurrent(context)
        || itemId == 0
        || ! isItemEnabled(itemId))
        return true;

    popupSessionActive = false;
    ++popupSessionRevision;
    setSelectedId(itemId, juce::dontSendNotification);

    auto selectionCommitter = commitSelection;
    if (selectionCommitter != nullptr)
        selectionCommitter(*this, itemId, context);

    return true;
}

void ModulationMatrixRoutingComboBox::mouseDown(
    const juce::MouseEvent& event)
{
    if (! isEnabled()
        || ! isShowing()
        || ! isCompletePrimaryDown(event))
        return;

    if (popupRequestArmed && ! isPopupActive())
        return;

    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    if (pointerInteractionActive)
    {
        if (! isPointerSource(event))
            return;

        releasePointerInteractionWithoutSelection(event);
        if (safeThis == nullptr)
            return;
    }

    pointerInteractionActive = true;
    cancelPendingPointerRelease = false;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();

    if (! isPopupActive() && ! capturePopupRequest())
    {
        clearPointerInteraction();
        return;
    }

    juce::ComboBox::mouseDown(event);
    if (safeThis != nullptr && ! isPopupActive())
        popupRequestArmed = false;
}

void ModulationMatrixRoutingComboBox::mouseDrag(
    const juce::MouseEvent& event)
{
    if (! pointerInteractionActive
        || ! isPointerSource(event)
        || cancelPendingPointerRelease)
        return;

    const bool mayQueuePopup = ! isPopupActive();
    if (mayQueuePopup && popupRequestArmed)
        return;

    if (mayQueuePopup)
    {
        if (! capturePopupRequest())
            return;
    }

    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseDrag(event);
    if (safeThis != nullptr && mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;
}

void ModulationMatrixRoutingComboBox::mouseEnter(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseEnter(event);
    if (safeThis != nullptr)
        recoverMissingPointerUp(event);
}

void ModulationMatrixRoutingComboBox::mouseMove(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseMove(event);
    if (safeThis != nullptr)
        recoverMissingPointerUp(event);
}

void ModulationMatrixRoutingComboBox::mouseExit(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseExit(event);
    if (safeThis != nullptr)
        recoverMissingPointerUp(event);
}

void ModulationMatrixRoutingComboBox::mouseUp(
    const juce::MouseEvent& event)
{
    if (! pointerInteractionActive || ! isPointerSource(event))
        return;

    if (cancelPendingPointerRelease)
    {
        releasePointerInteractionWithoutSelection(event);
        return;
    }

    const bool mayQueuePopup = ! isPopupActive();
    if (mayQueuePopup && popupRequestArmed)
    {
        releasePointerInteractionWithoutSelection(event);
        return;
    }

    if (mayQueuePopup && ! capturePopupRequest())
    {
        releasePointerInteractionWithoutSelection(event);
        return;
    }

    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseUp(event);
    if (safeThis == nullptr)
        return;

    if (mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;

    clearPointerInteraction();
}

void ModulationMatrixRoutingComboBox::mouseWheelMove(
    const juce::MouseEvent& event,
    const juce::MouseWheelDetails& wheel)
{
    juce::Component::mouseWheelMove(event, wheel);
}

void ModulationMatrixRoutingComboBox::visibilityChanged()
{
    juce::ComboBox::visibilityChanged();
    if (! isShowing())
        dismissTransientInteraction();
}

void ModulationMatrixRoutingComboBox::enablementChanged()
{
    dismissTransientInteraction();
    juce::ComboBox::enablementChanged();
}

void ModulationMatrixRoutingComboBox::parentHierarchyChanged()
{
    juce::ComboBox::parentHierarchyChanged();
    dismissTransientInteraction();
}

bool ModulationMatrixRoutingComboBox::isCompletePrimaryDown(
    const juce::MouseEvent& event) const noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown()
        && ! event.mods.isPopupMenu();
}

bool ModulationMatrixRoutingComboBox::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void ModulationMatrixRoutingComboBox::recoverMissingPointerUp(
    const juce::MouseEvent& event)
{
    if (pointerInteractionActive
        && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
        releasePointerInteractionWithoutSelection(event);
}

void ModulationMatrixRoutingComboBox::releasePointerInteractionWithoutSelection(
    const juce::MouseEvent& event)
{
    clearPointerInteraction();
    juce::ComboBox::mouseUp(
        event.getEventRelativeTo(this).withNewPosition(
            juce::Point<float> { -1.0f, -1.0f }));
}

void ModulationMatrixRoutingComboBox::clearPointerInteraction() noexcept
{
    pointerInteractionActive = false;
    cancelPendingPointerRelease = false;
    pointerSourceIndex = -1;
}

void ModulationMatrixRoutingComboBox::closePopupWindow() noexcept
{
    juce::ComboBox::hidePopup();
}

void ModulationMatrixRoutingComboBox::dismissTransientInteraction() noexcept
{
    popupSessionActive = false;
    ++popupSessionRevision;
    ++interactionGeneration;
    cancelPendingPointerRelease = cancelPendingPointerRelease
                                  || pointerInteractionActive;
    closePopupWindow();
}

std::function<void(int)>
ModulationMatrixRoutingComboBox::createPopupResultHandler()
{
    if (getCurrentEditContext == nullptr)
        return [] (int) {};

    return createPopupResultHandler(getCurrentEditContext(),
                                    interactionGeneration);
}

std::function<void(int)>
ModulationMatrixRoutingComboBox::createPopupResultHandler(
    EditContext context,
    std::uint64_t expectedInteractionGeneration)
{
    popupSessionActive = true;
    const auto sessionRevision = ++popupSessionRevision;

    return [safeThis =
                juce::Component::SafePointer<ModulationMatrixRoutingComboBox>(this),
            capturedContext = std::move(context),
            expectedInteractionGeneration,
            sessionRevision](int result)
    {
        if (safeThis == nullptr
            || ! safeThis->popupSessionActive
            || safeThis->popupSessionRevision != sessionRevision)
            return;

        const bool mayCommit = result != 0
                               && safeThis->isPopupContextCurrent(
                                   capturedContext,
                                   expectedInteractionGeneration)
                               && safeThis->indexOfItemId(result) >= 0
                               && safeThis->isItemEnabled(result);

        safeThis->popupSessionActive = false;
        ++safeThis->popupSessionRevision;
        safeThis->cancelPendingPointerRelease =
            safeThis->cancelPendingPointerRelease
            || safeThis->pointerInteractionActive;
        safeThis->closePopupWindow();

        if (! mayCommit
            || safeThis == nullptr
            || ! safeThis->isPopupContextCurrent(
                capturedContext,
                expectedInteractionGeneration))
            return;

        safeThis->setSelectedId(result, juce::dontSendNotification);
        auto selectionCommitter = safeThis->commitSelection;
        if (selectionCommitter != nullptr)
            selectionCommitter(*safeThis, result, capturedContext);
    };
}

void ModulationMatrixRoutingComboBox::showPopup()
{
    if (! popupRequestArmed)
    {
        if (isPopupActive() || ! capturePopupRequest())
            return;

        juce::ComboBox::keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey });
        return;
    }

    auto requestContext = popupRequestContext;
    const auto requestInteractionGeneration =
        popupRequestInteractionGeneration;
    popupRequestArmed = false;

    if (! isPopupContextCurrent(requestContext,
                                requestInteractionGeneration))
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
            options = lookAndFeel.getOptionsForComboBoxPopupMenu(*this,
                                                                 *label);
            break;
        }
    }

    menu.showMenuAsync(options,
                       createPopupResultHandler(
                           std::move(requestContext),
                           requestInteractionGeneration));
}

//==============================================================================
// ModulationMatrixHeader Implementation
//==============================================================================
ModulationMatrixHeader::ModulationMatrixHeader()
{
    // Initialize and add labels for the column titles.
    addAndMakeVisible(sourceLabel);
    sourceLabel.setText("Source", juce::dontSendNotification);
    sourceLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(amountLabel);
    amountLabel.setText("Amount", juce::dontSendNotification);
    amountLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(polarityLabel);
    polarityLabel.setText("Polarity", juce::dontSendNotification);
    polarityLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(bypassLabel);
    bypassLabel.setText("Bypass", juce::dontSendNotification);
    bypassLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(destinationLabel);
    destinationLabel.setText("Destination", juce::dontSendNotification);
    destinationLabel.setJustificationType(juce::Justification::centred);

    for (auto* label : { &sourceLabel, &amountLabel, &polarityLabel, &bypassLabel, &destinationLabel })
    {
        label->setColour(juce::Label::textColourId, fire::ui::colours::textMuted);
        label->setFont(fire::ui::labelFont(10.0f));
    }
}

void ModulationMatrixHeader::paint(juce::Graphics& g)
{
    g.setColour(fire::ui::colours::surface1.withAlpha(0.86f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), fire::ui::Metrics::radiusSmall);
    g.setColour(fire::ui::colours::hairline.withAlpha(0.72f));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void ModulationMatrixHeader::resized()
{
    // Use a FlexBox to lay out the header labels, matching the row layout.
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.items.add(juce::FlexItem(sourceLabel).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(amountLabel).withFlex(2.0f).withMargin(2));
    flex.items.add(juce::FlexItem(polarityLabel).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(bypassLabel).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(destinationLabel).withFlex(1.6f).withMargin(2));
    flex.items.add(juce::FlexItem().withWidth(35).withMargin(2));
    flex.performLayout(getLocalBounds());
}

//==============================================================================
// ModulationMatrixRow Implementation
//==============================================================================
void ModulationMatrixRow::PrimaryButtonSlider::mouseDown(
    const juce::MouseEvent& event)
{
    if (primaryGestureInProgress
        || ! event.mods.isLeftButtonDown()
        || event.mods.isMiddleButtonDown()
        || event.mods.isPopupMenu())
        return;

    primaryGestureInProgress = true;
    juce::Slider::mouseDown(event);
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseDrag(
    const juce::MouseEvent& event)
{
    if (primaryGestureInProgress)
        juce::Slider::mouseDrag(event);
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseUp(
    const juce::MouseEvent& event)
{
    if (! primaryGestureInProgress)
        return;

    primaryGestureInProgress = false;
    juce::Slider::mouseUp(event);
}

ModulationMatrixRow::ModulationMatrixRow(FireAudioProcessor& p,
                                         int routingIndex,
                                         const ModulationRouting& routing,
                                         std::shared_ptr<ModulationRoutingEditSession> editSession,
                                         std::function<void(std::uint64_t,
                                                            ModulationRouting)> onDelete)
    : processor(p),
      index(routingIndex),
      expectedRouting(routing),
      routingEditSession(std::move(editSession)),
      onDeleteCallback(std::move(onDelete))
{
    setOpaque(false);
    setLookAndFeel(&fireLookAndFeel);
    // SOURCE MENU
    addAndMakeVisible(sourceMenu);
    for (int i = 1; i <= 4; ++i)
        sourceMenu.addItem("LFO " + juce::String(i), i);
    sourceMenu.setSelectedId(routing.sourceLfoIndex + 1, juce::dontSendNotification);
    sourceMenu.addListener(this);
    sourceMenu.setColour(juce::ComboBox::backgroundColourId, fire::ui::colours::surface0);
    sourceMenu.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::modulation.withAlpha(0.45f));
    sourceMenu.setColour(juce::ComboBox::textColourId, fire::ui::colours::modulation);

    // AMOUNT SLIDER
    addAndMakeVisible(amountSlider);
    amountSlider.setRange(-1.0, 1.0, 0.01);
    amountSlider.setValue(routing.depth, juce::dontSendNotification);
    amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    amountSlider.setColour(juce::Slider::trackColourId, fire::ui::colours::modulation);
    amountSlider.setScrollWheelEnabled(false);
    amountSlider.addListener(this);

    // BIPOLAR BUTTON
    addAndMakeVisible(bipolarButton);
    bipolarButton.setComponentID("rounded");
    bipolarButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface0);
    bipolarButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::surface2);
    bipolarButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::modulation);
    bipolarButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    bipolarButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    bipolarButton.setClickingTogglesState(true);
    bipolarButton.setToggleState(routing.isBipolar, juce::dontSendNotification);
    bipolarButton.setButtonText(bipolarButton.getToggleState() ? "Bi" : "Uni");
    bipolarButton.addListener(this);

    // BYPASS BUTTON
    addAndMakeVisible(bypassButton);
    bypassButton.setComponentID("rounded");
    bypassButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface0);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::surface2);
    bypassButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::danger);
    bypassButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::positive);
    bypassButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    bypassButton.setClickingTogglesState(true);
    bypassButton.setToggleState(routing.isBypassed, juce::dontSendNotification);
    bypassButton.setButtonText(bypassButton.getToggleState() ? "On" : "Off");
    bypassButton.addListener(this);

    // === DESTINATION MENU ===
    addAndMakeVisible(destinationMenu);

    // 1. get all possible modulation targets
    allPossibleTargets = ParameterIDAndName::getAllModulatableTargets();

    // 2. Populate the destination menu
    destinationMenu.addItem("None", 1);
    for (size_t i = 0; i < allPossibleTargets.size(); ++i)
    {
        // Use displayText as the menu item, and the menu ID is the index + 2 (because "None" is 1)
        destinationMenu.addItem(allPossibleTargets[i].displayText, static_cast<int>(i) + 2);
    }

    // 3. Set the currently selected destination
    const auto& currentTargetId = routing.targetParameterID;
    if (currentTargetId.isNotEmpty())
    {
        for (size_t i = 0; i < allPossibleTargets.size(); ++i)
        {
            if (allPossibleTargets[i].parameterID == currentTargetId)
            {
                destinationMenu.setSelectedId(static_cast<int>(i) + 2, juce::dontSendNotification);
                break;
            }
        }
    }
    else
    {
        destinationMenu.setSelectedId(1, juce::dontSendNotification); // "None"
    }

    destinationMenu.addListener(this);
    destinationMenu.setColour(juce::ComboBox::backgroundColourId, fire::ui::colours::surface0);
    destinationMenu.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    destinationMenu.setColour(juce::ComboBox::textColourId, fire::ui::colours::textPrimary);

    const auto configureRoutingMenu = [this](auto& menu)
    {
        menu.configurePopupSession(
            [this]
            {
                return captureComboBoxEditContext();
            },
            [this](const auto& context)
            {
                return isComboBoxEditContextCurrent(context);
            },
            [this](auto& comboBox, int selectedId, const auto& context)
            {
                commitComboBoxSelection(comboBox, selectedId, context);
            });
    };
    configureRoutingMenu(sourceMenu);
    configureRoutingMenu(destinationMenu);

    // REMOVE BUTTON
    addAndMakeVisible(removeButton);
    removeButton.setComponentID("remove_button");
    removeButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface0);
    removeButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::danger);
    removeButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::danger);
    removeButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    removeButton.addListener(this);
}

void ModulationMatrixRow::paint(juce::Graphics& g)
{
    fire::ui::drawPanel(g, getLocalBounds().toFloat().reduced(1.0f),
                        fire::ui::colours::modulation, false,
                        fire::ui::Metrics::radiusSmall);
}

ModulationMatrixRow::~ModulationMatrixRow()
{
    sourceMenu.dismissTransientInteraction();
    destinationMenu.dismissTransientInteraction();
    sourceMenu.removeListener(this);
    amountSlider.removeListener(this);
    bipolarButton.removeListener(this);
    bypassButton.removeListener(this);
    destinationMenu.removeListener(this);
    removeButton.removeListener(this);
    setLookAndFeel(nullptr);
}

void ModulationMatrixRow::resized()
{
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.items.add(juce::FlexItem(sourceMenu).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(amountSlider).withFlex(2.0f).withMargin(2));
    flex.items.add(juce::FlexItem(bipolarButton).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(bypassButton).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(destinationMenu).withFlex(1.6f).withMargin(2));
    flex.items.add(juce::FlexItem(removeButton).withWidth(35).withMargin(2));
    flex.performLayout(getLocalBounds());
}

void ModulationMatrixRow::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (! isShowing())
    {
        sourceMenu.dismissTransientInteraction();
        destinationMenu.dismissTransientInteraction();
    }
}

void ModulationMatrixRow::enablementChanged()
{
    juce::Component::enablementChanged();
    if (! isEnabled())
    {
        sourceMenu.dismissTransientInteraction();
        destinationMenu.dismissTransientInteraction();
    }
}

void ModulationMatrixRow::buttonClicked(juce::Button* button)
{
    if (button == &bipolarButton || button == &bypassButton)
    {
        bipolarButton.setButtonText(bipolarButton.getToggleState() ? "Bi" : "Uni");
        bypassButton.setButtonText(bypassButton.getToggleState() ? "On" : "Off");

        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        auto editSession = routingEditSession;
        if (editSession == nullptr)
        {
            requestParentRebuild();
            return;
        }

        auto replacementRouting = expectedRouting;
        if (button == &bipolarButton)
            replacementRouting.isBipolar = bipolarButton.getToggleState();
        else
            replacementRouting.isBypassed = bypassButton.getToggleState();

        auto& manager = processor.getLfoManager();
        const auto result = manager.updateModulationRoutingIfRevisionMatches(
            index,
            editSession->revision,
            expectedRouting,
            replacementRouting);
        if (! result.accepted)
        {
            requestParentRebuild();
            return;
        }

        editSession->revision = result.revision;
        expectedRouting = result.routing;
        if (result.changed)
        {
            auto& processorToNotify = processor;
            processorToNotify.lfoDataHasChanged();
        }

        return;
    }

    if (button == &removeButton)
    {
        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        auto callback = onDeleteCallback;
        auto editSession = routingEditSession;
        const auto routingToDelete = expectedRouting;
        if (callback && editSession != nullptr)
            callback(editSession->revision, routingToDelete);
        return;
    }
}

void ModulationMatrixRow::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &amountSlider)
    {
        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        auto editSession = routingEditSession;
        if (editSession == nullptr)
        {
            requestParentRebuild();
            return;
        }

        auto replacementRouting = expectedRouting;
        replacementRouting.depth = static_cast<float>(amountSlider.getValue());
        auto& manager = processor.getLfoManager();
        const auto result = manager.updateModulationRoutingIfRevisionMatches(
            index,
            editSession->revision,
            expectedRouting,
            replacementRouting);
        if (! result.accepted)
        {
            requestParentRebuild();
            return;
        }

        editSession->revision = result.revision;
        expectedRouting = result.routing;
        if (result.changed)
        {
            auto& processorToNotify = processor;
            processorToNotify.lfoDataHasChanged();
        }
    }
}

void ModulationMatrixRow::comboBoxChanged(juce::ComboBox* comboBox)
{
    auto* routingMenu = dynamic_cast<ModulationMatrixRoutingComboBox*>(comboBox);
    if (routingMenu == &sourceMenu || routingMenu == &destinationMenu)
        commitComboBoxSelection(*routingMenu,
                                routingMenu->getSelectedId(),
                                captureComboBoxEditContext());
}

ModulationMatrixRoutingComboBox::EditContext
ModulationMatrixRow::captureComboBoxEditContext() const
{
    return { routingEditSession,
             routingEditSession != nullptr ? routingEditSession->revision : 0,
             expectedRouting };
}

bool ModulationMatrixRow::isComboBoxEditContextCurrent(
    const ModulationMatrixRoutingComboBox::EditContext& context)
{
    return context.editSession != nullptr
        && context.editSession == routingEditSession
        && routingEditSession != nullptr
        && context.revision == routingEditSession->revision
        && haveSameRoutingState(context.expectedRouting, expectedRouting)
        && ! isParentRebuildPending();
}

void ModulationMatrixRow::commitComboBoxSelection(
    ModulationMatrixRoutingComboBox& comboBox,
    int selectedId,
    const ModulationMatrixRoutingComboBox::EditContext& context)
{
    if (! isComboBoxEditContextCurrent(context))
    {
        requestParentRebuild();
        return;
    }

    auto selectedSourceIndex = context.expectedRouting.sourceLfoIndex;
    auto selectedTargetId = context.expectedRouting.targetParameterID;

    if (&comboBox == &sourceMenu)
    {
        if (! juce::isPositiveAndBelow(selectedId - 1, 4))
        {
            requestParentRebuild();
            return;
        }

        selectedSourceIndex = selectedId - 1;
    }
    else if (&comboBox == &destinationMenu)
    {
        if (selectedId == 1)
        {
            selectedTargetId.clear();
        }
        else
        {
            const auto targetIndex = selectedId - 2;
            if (! juce::isPositiveAndBelow(
                    targetIndex,
                    static_cast<int>(allPossibleTargets.size())))
            {
                requestParentRebuild();
                return;
            }

            selectedTargetId =
                allPossibleTargets[static_cast<size_t>(targetIndex)]
                    .parameterID;
        }
    }
    else
    {
        return;
    }

    auto& processorToNotify = processor;
    auto& manager = processorToNotify.getLfoManager();
    const auto result = manager.assignModulationRoutingIfRevisionMatches(
        index,
        context.revision,
        context.expectedRouting,
        selectedSourceIndex,
        selectedTargetId);
    if (! result.accepted)
    {
        requestParentRebuild();
        return;
    }

    // Source/destination edits may clear another row. Invalidate all rows
    // before the host can synchronously destroy or re-enter the editor.
    requestParentRebuild();
    if (result.changed)
        processorToNotify.lfoDataHasChanged();
}

bool ModulationMatrixRow::isParentRebuildPending()
{
    if (auto* panel = findParentComponentOfClass<ModulationMatrixPanel>())
        return panel->isUiRebuildPending();

    return false;
}

void ModulationMatrixRow::requestParentRebuild()
{
    if (auto* panel = findParentComponentOfClass<ModulationMatrixPanel>())
        panel->requestUiRebuild();
}

//==============================================================================
// ModulationMatrixPanel Implementation
//==============================================================================
ModulationMatrixPanel::ModulationMatrixPanel(FireAudioProcessor& p) : processor(p)
{
    setOpaque(true);
    setLookAndFeel(&fireLookAndFeel);
    processor.addChangeListener(this);
    addAndMakeVisible(header);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&contentComponent, false);
    addAndMakeVisible(addButton);
    addButton.addListener(this);
    addButton.setButtonText("+ ADD ROUTE");
    addButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface1);
    addButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::modulation);
    addButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::modulation);
    addButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    addAndMakeVisible(closeButton);
    closeButton.addListener(this);
    closeButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface1);
    closeButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::textPrimary);
    closeButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
    closeButton.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    buildUiFromProcessorState();
}

ModulationMatrixPanel::~ModulationMatrixPanel()
{
    dismissTransientInteractions();
    processor.removeChangeListener(this);
    cancelPendingUpdate();
    addButton.removeListener(this);
    closeButton.removeListener(this);
    setLookAndFeel(nullptr);
}

void ModulationMatrixPanel::paint(juce::Graphics& g)
{
    fire::ui::drawCanvas(g, getLocalBounds().toFloat());
    fire::ui::drawTechGrid(g, getLocalBounds().toFloat(), 28.0f, 0.05f);
    g.setFont(fire::ui::displayFont(17.0f));
    g.setColour(fire::ui::colours::textPrimary);
    g.drawText("MODULATION MATRIX", titleArea, juce::Justification::centredLeft);
    auto subtitle = titleArea.withTrimmedLeft(205);
    g.setFont(fire::ui::labelFont(9.0f));
    g.setColour(fire::ui::colours::textMuted);
    g.drawText("SIGNAL ROUTING / DEPTH / POLARITY", subtitle, juce::Justification::centredLeft);
}

void ModulationMatrixPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    titleArea = bounds.removeFromTop(34);
    auto bottomArea = bounds.removeFromBottom(44);
    closeButton.setBounds(bottomArea.removeFromRight(110).reduced(4));
    addButton.setBounds(bottomArea.removeFromLeft(130).reduced(4));

    // Position the header at the top.
    header.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(4);
    viewport.setBounds(bounds);

    // Set the size of the content that will be scrolled.
    contentComponent.setBounds(0, 0, viewport.getMaximumVisibleWidth(),
                               juce::jmax(viewport.getHeight(), static_cast<int>(rows.size()) * 44));

    // Layout the rows inside the content component.
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::column;
    for (auto& row : rows)
        flex.items.add(juce::FlexItem(*row).withHeight(40).withMargin(2));
    flex.performLayout(contentComponent.getLocalBounds());
}

void ModulationMatrixPanel::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (! isShowing())
        dismissTransientInteractions();
}

void ModulationMatrixPanel::enablementChanged()
{
    juce::Component::enablementChanged();
    if (! isEnabled())
        dismissTransientInteractions();
}

void ModulationMatrixPanel::dismissTransientInteractions() noexcept
{
    for (auto& row : rows)
    {
        if (row == nullptr)
            continue;

        for (int childIndex = 0;
             childIndex < row->getNumChildComponents();
             ++childIndex)
        {
            if (auto* comboBox = dynamic_cast<ModulationMatrixRoutingComboBox*>(
                    row->getChildComponent(childIndex)))
                comboBox->dismissTransientInteraction();
        }
    }
}

void ModulationMatrixPanel::buttonClicked(juce::Button* button)
{
    if (button == &addButton)
    {
        if (isUiRebuildPending())
            return;

        auto editSession = routingEditSession;
        if (editSession == nullptr)
        {
            requestUiRebuild();
            return;
        }

        auto& manager = processor.getLfoManager();
        const auto result =
            manager.addEmptyModulationRoutingIfRevisionMatches(
                editSession->revision);
        if (! result.accepted)
        {
            requestUiRebuild();
            return;
        }

        // Adding changes the visible routing set. Mark every old row stale
        // before a synchronous host listener can re-enter this panel.
        requestUiRebuild();
        auto& processorToNotify = processor;
        if (result.changed)
            processorToNotify.lfoDataHasChanged();
        return;
    }

    if (button == &closeButton)
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }
}

void ModulationMatrixPanel::buildUiFromProcessorState()
{
    dismissTransientInteractions();
    rows.clear();
    contentComponent.removeAllChildren();

    // Build from one coherent snapshot. Component construction and callbacks
    // must not happen while the shared routing lock is held.
    const auto routingState =
        processor.getLfoManager().getModulationRoutingStateSnapshot();
    routingEditSession = std::make_shared<ModulationRoutingEditSession>();
    routingEditSession->revision = routingState.revision;
    const auto editSession = routingEditSession;
    for (int i = 0; i < routingState.routings.size(); ++i)
    {
        // When creating a row, pass a lambda function that captures the index 'i'.
        // This lambda will be called when the row's remove button is clicked.
        auto onDelete = [this,
                         index = i](std::uint64_t expectedRevision,
                                    ModulationRouting expectedRouting)
        {
            juce::Component::SafePointer<ModulationMatrixPanel> safeThis(this);
            auto& processorToNotify = processor;

            auto& manager = processorToNotify.getLfoManager();
            const auto result =
                manager.removeModulationRoutingIfRevisionMatches(
                    index,
                    expectedRevision,
                    expectedRouting);
            if (! result.accepted)
            {
                if (safeThis != nullptr)
                    safeThis->requestUiRebuild();
                return;
            }

            // Removal shifts every following index. Do not advance the old
            // rows' shared session to the new model revision: invalidate and
            // queue their rebuild before notifying any re-entrant listener.
            if (safeThis != nullptr)
                safeThis->requestUiRebuild();
            if (result.changed)
                processorToNotify.lfoDataHasChanged();
        };

        rows.push_back(std::make_unique<ModulationMatrixRow>(
            processor,
            i,
            routingState.routings.getReference(i),
            editSession,
            std::move(onDelete)));
        contentComponent.addAndMakeVisible(*rows.back());
    }

    resized();
}

void ModulationMatrixPanel::requestUiRebuild()
{
    triggerAsyncUpdate();
}

bool ModulationMatrixPanel::isUiRebuildPending() const noexcept
{
    return isUpdatePending();
}

void ModulationMatrixPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &processor)
        requestUiRebuild();
}

void ModulationMatrixPanel::handleAsyncUpdate()
{
    buildUiFromProcessorState();
}
