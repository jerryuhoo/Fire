/*
  ==============================================================================

    ModulationMatrixPanel.cpp
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulationMatrixPanel.h"
#include "../../Utility/AudioHelpers.h" // Include AudioHelpers to access ParameterIDAndName

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

    addAndMakeVisible(destinationLabel);
    destinationLabel.setText("Destination", juce::dontSendNotification);
    destinationLabel.setJustificationType(juce::Justification::centred);
}

void ModulationMatrixHeader::resized()
{
    // Use a FlexBox to lay out the header labels, matching the row layout.
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.items.add(juce::FlexItem(sourceLabel).withFlex(1.0f));
    flex.items.add(juce::FlexItem(amountLabel).withFlex(2.0f));
    flex.items.add(juce::FlexItem(polarityLabel).withFlex(1.0f));
    flex.items.add(juce::FlexItem(destinationLabel).withFlex(1.0f));
    flex.items.add(juce::FlexItem().withWidth(35));
    flex.performLayout(getLocalBounds());
}

//==============================================================================
// ModulationMatrixRow Implementation
//==============================================================================
ModulationMatrixRow::ModulationMatrixRow(FireAudioProcessor& p, int routingIndex, std::function<void()> onDelete)
    : processor(p), index(routingIndex), onDeleteCallback(onDelete)
{
    // SOURCE MENU
    addAndMakeVisible(sourceMenu);
    for (int i = 1; i <= 4; ++i)
        sourceMenu.addItem("LFO " + juce::String(i), i);
    sourceMenu.setSelectedId(processor.modulationRoutings[index].sourceLfoIndex + 1, juce::dontSendNotification);
    sourceMenu.addListener(this);
    sourceMenu.setColour(juce::ComboBox::backgroundColourId, COLOUR6);
    sourceMenu.setColour(juce::ComboBox::outlineColourId, COLOUR7);
    sourceMenu.setColour(juce::ComboBox::textColourId, COLOUR1);

    // AMOUNT SLIDER
    addAndMakeVisible(amountSlider);
    amountSlider.setRange(-1.0, 1.0, 0.01);
    amountSlider.setValue(processor.modulationRoutings[index].depth, juce::dontSendNotification);
    amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    amountSlider.setScrollWheelEnabled(false);
    amountSlider.addListener(this);

    // BIPOLAR BUTTON
    addAndMakeVisible(bipolarButton);
    bipolarButton.setClickingTogglesState(true);
    bipolarButton.setToggleState(processor.modulationRoutings[index].isBipolar, juce::dontSendNotification);
    bipolarButton.setButtonText(bipolarButton.getToggleState() ? "Bi" : "Uni");
    bipolarButton.onStateChange = [this]
    {
        bipolarButton.setButtonText(bipolarButton.getToggleState() ? "Bi" : "Uni");
        processor.modulationRoutings.getReference(index).isBipolar = bipolarButton.getToggleState();
    };

    // === DESTINATION MENU ===
    addAndMakeVisible(destinationMenu);

    // 1. get all possible modulation targets
    allPossibleTargets = ParameterIDAndName::getAllModulatableTargets();

    // 2. Populate the destination menu
    destinationMenu.addItem("None", 1);
    for (int i = 0; i < allPossibleTargets.size(); ++i)
    {
        // Use displayText as the menu item, and the menu ID is the index + 2 (because "None" is 1)
        destinationMenu.addItem(allPossibleTargets[i].displayText, i + 2);
    }

    // 3. Set the currently selected destination
    const auto& currentTargetId = processor.modulationRoutings[index].targetParameterID;
    if (currentTargetId.isNotEmpty())
    {
        for (int i = 0; i < allPossibleTargets.size(); ++i)
        {
            if (allPossibleTargets[i].parameterID == currentTargetId)
            {
                destinationMenu.setSelectedId(i + 2, juce::dontSendNotification);
                break;
            }
        }
    }
    else
    {
        destinationMenu.setSelectedId(1, juce::dontSendNotification); // "None"
    }

    destinationMenu.addListener(this);
    destinationMenu.setColour(juce::ComboBox::backgroundColourId, COLOUR6);
    destinationMenu.setColour(juce::ComboBox::outlineColourId, COLOUR7);
    destinationMenu.setColour(juce::ComboBox::textColourId, COLOUR1);

    // REMOVE BUTTON
    addAndMakeVisible(removeButton);
    removeButton.addListener(this);
}

ModulationMatrixRow::~ModulationMatrixRow() {}

void ModulationMatrixRow::resized()
{
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.items.add(juce::FlexItem(sourceMenu).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(amountSlider).withFlex(2.0f).withMargin(2));
    flex.items.add(juce::FlexItem(bipolarButton).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(destinationMenu).withFlex(1.0f).withMargin(2));
    flex.items.add(juce::FlexItem(removeButton).withWidth(35).withMargin(2));
    flex.performLayout(getLocalBounds());
}

void ModulationMatrixRow::buttonClicked(juce::Button* button)
{
    if (button == &removeButton)
    {
        if (onDeleteCallback)
            onDeleteCallback();
    }
}

void ModulationMatrixRow::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &amountSlider)
        processor.modulationRoutings.getReference(index).depth = (float) amountSlider.getValue();
}

void ModulationMatrixRow::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &sourceMenu)
    {
        processor.modulationRoutings.getReference(index).sourceLfoIndex = sourceMenu.getSelectedId() - 1;
    }

    if (comboBox == &destinationMenu)
    {
        int selectedId = destinationMenu.getSelectedId();
        if (selectedId == 1) // "None" was selected
        {
            processor.modulationRoutings.getReference(index).targetParameterID = "";
        }
        else
        {
            int listIndex = selectedId - 2;

            if (juce::isPositiveAndBelow(listIndex, allPossibleTargets.size()))
            {
                processor.modulationRoutings.getReference(index).targetParameterID = allPossibleTargets[listIndex].parameterID;
            }
        }
    }
}

//==============================================================================
// ModulationMatrixPanel Implementation
//==============================================================================
ModulationMatrixPanel::ModulationMatrixPanel(FireAudioProcessor& p) : processor(p)
{
    while (processor.modulationRoutings.size() < 4)
        processor.modulationRoutings.add({});

    addAndMakeVisible(header);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&contentComponent, false);
    addAndMakeVisible(addButton);
    addButton.addListener(this);
    addAndMakeVisible(closeButton);
    closeButton.addListener(this);
    buildUiFromProcessorState();
    setSize(500, 300);
}

ModulationMatrixPanel::~ModulationMatrixPanel() {}

void ModulationMatrixPanel::paint(juce::Graphics& g)
{
    g.fillAll(COLOUR7);
}

void ModulationMatrixPanel::resized()
{
    auto bounds = getLocalBounds();
    auto bottomArea = bounds.removeFromBottom(40);
    closeButton.setBounds(bottomArea.removeFromRight(100).reduced(5));
    addButton.setBounds(bottomArea.removeFromLeft(100).reduced(5));

    // Position the header at the top.
    header.setBounds(bounds.removeFromTop(30));
    viewport.setBounds(bounds);

    // Set the size of the content that will be scrolled.
    contentComponent.setBounds(0, 0, viewport.getMaximumVisibleWidth(), static_cast<int>(rows.size()) * 40);

    // Layout the rows inside the content component.
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::column;
    for (auto& row : rows)
        flex.items.add(juce::FlexItem(*row).withHeight(35).withMargin(2));
    flex.performLayout(contentComponent.getLocalBounds());
}

void ModulationMatrixPanel::buttonClicked(juce::Button* button)
{
    if (button == &addButton)
    {
        processor.modulationRoutings.add({});
        buildUiFromProcessorState();
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

    for (int i = 0; i < processor.modulationRoutings.size(); ++i)
    {
        // When creating a row, pass a lambda function that captures the index 'i'.
        // This lambda will be called when the row's remove button is clicked.
        auto onDelete = [this, index = i]()
        {
            // Remove the routing from the processor's data model.
            processor.modulationRoutings.remove(index);
            // Rebuild the entire UI to reflect the change.
            buildUiFromProcessorState();
        };

        rows.push_back(std::make_unique<ModulationMatrixRow>(processor, i, onDelete));
        contentComponent.addAndMakeVisible(*rows.back());
    }

    resized();
}
