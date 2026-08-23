/*
  ==============================================================================

    ModulationMatrixPanel.cpp
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulationMatrixPanel.h"
#include "../../Utility/AudioHelpers.h"

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
ModulationMatrixRow::ModulationMatrixRow(FireAudioProcessor& p,
                                         int routingIndex,
                                         const ModulationRouting& routing,
                                         std::function<void()> onDelete)
    : processor(p),
      index(routingIndex),
      targetParameterIDAtBuild(routing.targetParameterID),
      onDeleteCallback(onDelete)
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
    bipolarButton.onStateChange = [this]
    {
        bipolarButton.setButtonText(bipolarButton.getToggleState() ? "Bi" : "Uni");
        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        auto& manager = processor.getLfoManager();
        bool didUpdate = false;
        {
            const juce::ScopedLock lock(manager.getLfoDataLock());
            auto& routings = manager.getModulationRoutings();
            if (juce::isPositiveAndBelow(index, routings.size())
                && routings.getReference(index).targetParameterID == targetParameterIDAtBuild)
            {
                routings.getReference(index).isBipolar = bipolarButton.getToggleState();
                didUpdate = true;
            }
        }

        if (didUpdate)
            processor.lfoDataHasChanged();
        else
            requestParentRebuild();
    };

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
    bypassButton.onStateChange = [this]
    {
        bypassButton.setButtonText(bypassButton.getToggleState() ? "On" : "Off");
        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        auto& manager = processor.getLfoManager();
        bool didUpdate = false;
        {
            const juce::ScopedLock lock(manager.getLfoDataLock());
            auto& routings = manager.getModulationRoutings();
            if (juce::isPositiveAndBelow(index, routings.size())
                && routings.getReference(index).targetParameterID == targetParameterIDAtBuild)
            {
                routings.getReference(index).isBypassed = bypassButton.getToggleState();
                didUpdate = true;
            }
        }

        if (didUpdate)
            processor.lfoDataHasChanged();
        else
            requestParentRebuild();
    };

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
    sourceMenu.removeListener(this);
    amountSlider.removeListener(this);
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

void ModulationMatrixRow::buttonClicked(juce::Button* button)
{
    if (button == &removeButton)
    {
        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        if (onDeleteCallback)
            onDeleteCallback();
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

        auto& manager = processor.getLfoManager();
        bool didUpdate = false;
        {
            const juce::ScopedLock lock(manager.getLfoDataLock());
            auto& routings = manager.getModulationRoutings();
            if (juce::isPositiveAndBelow(index, routings.size())
                && routings.getReference(index).targetParameterID == targetParameterIDAtBuild)
            {
                routings.getReference(index).depth = (float) amountSlider.getValue();
                didUpdate = true;
            }
        }

        if (didUpdate)
            processor.lfoDataHasChanged();
        else
            requestParentRebuild();
    }
}

void ModulationMatrixRow::comboBoxChanged(juce::ComboBox* comboBox)
{
    // This function now handles changes from BOTH combo boxes.
    if (comboBox == &sourceMenu || comboBox == &destinationMenu)
    {
        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        // 1. Get the current selections from both menus.
        int selectedSourceIndex = sourceMenu.getSelectedId() - 1;
        juce::String selectedTargetID = "";

        int selectedDestinationId = destinationMenu.getSelectedId();
        if (selectedDestinationId > 1) // i.e., not "None"
        {
            int listIndex = selectedDestinationId - 2;
            if (juce::isPositiveAndBelow(listIndex, static_cast<int>(allPossibleTargets.size())))
            {
                selectedTargetID = allPossibleTargets[static_cast<size_t>(listIndex)].parameterID;
            }
        }

        // 2. Call the new, safe method in the processor to apply the changes.
        processor.assignModulation(index,
                                   selectedSourceIndex,
                                   selectedTargetID,
                                   targetParameterIDAtBuild);

        // 3. IMPORTANT: Tell the parent panel to rebuild its UI.
        // This ensures that if another row was cleared, it will visually update to "None".
        requestParentRebuild();
    }
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

void ModulationMatrixPanel::buttonClicked(juce::Button* button)
{
    if (button == &addButton)
    {
        if (isUiRebuildPending())
            return;

        auto& manager = processor.getLfoManager();
        {
            const juce::ScopedLock lock(manager.getLfoDataLock());
            manager.getModulationRoutings().add({});
        }
        processor.lfoDataHasChanged();
        requestUiRebuild();
    }

    if (button == &closeButton)
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }
}

void ModulationMatrixPanel::buildUiFromProcessorState()
{
    rows.clear();
    contentComponent.removeAllChildren();

    // Build from one coherent snapshot. Component construction and callbacks
    // must not happen while the shared routing lock is held.
    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    for (int i = 0; i < routings.size(); ++i)
    {
        // When creating a row, pass a lambda function that captures the index 'i'.
        // This lambda will be called when the row's remove button is clicked.
        auto onDelete = [this,
                         index = i,
                         expectedTargetParameterID = routings.getReference(i).targetParameterID]()
        {
            // Remove the routing from the processor's data model.
            auto& manager = processor.getLfoManager();
            bool didRemove = false;
            {
                const juce::ScopedLock lock(manager.getLfoDataLock());
                auto& mutableRoutings = manager.getModulationRoutings();
                if (juce::isPositiveAndBelow(index, mutableRoutings.size())
                    && mutableRoutings.getReference(index).targetParameterID
                           == expectedTargetParameterID)
                {
                    mutableRoutings.remove(index);
                    didRemove = true;
                }
            }

            if (didRemove)
                processor.lfoDataHasChanged();

            // Defer rebuilding until the current button callback has returned.
            // Clearing rows synchronously here would destroy the row that is
            // currently executing this callback.
            requestUiRebuild();
        };

        rows.push_back(std::make_unique<ModulationMatrixRow>(processor, i, routings.getReference(i), onDelete));
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
