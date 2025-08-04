/*
  ==============================================================================

    ModulationMatrixPanel.cpp
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulationMatrixPanel.h"

//==============================================================================
// ModulationMatrixRow Implementation
//==============================================================================
ModulationMatrixRow::ModulationMatrixRow (FireAudioProcessor& p, int routingIndex)
    : processor (p), index (routingIndex)
{
    // SOURCE MENU
    addAndMakeVisible (sourceMenu);
    for (int i = 1; i <= 4; ++i)
        sourceMenu.addItem ("LFO " + juce::String (i), i);
    sourceMenu.setSelectedId (processor.modulationRoutings[index].sourceLfoIndex + 1, juce::dontSendNotification);
    sourceMenu.addListener (this);
    
    // AMOUNT SLIDER
    addAndMakeVisible (amountSlider);
    amountSlider.setRange (-1.0, 1.0, 0.01);
    amountSlider.setValue (processor.modulationRoutings[index].depth, juce::dontSendNotification);
    amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    amountSlider.addListener (this);

    // DESTINATION MENU
    addAndMakeVisible (destinationMenu);
    // Populate with all available parameters from your processor
    auto& params = processor.getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        if (auto* paramWithId = dynamic_cast<juce::AudioProcessorParameterWithID*> (params.getUnchecked(i)))
            destinationMenu.addItem (paramWithId->getName(100), i + 1);
    }
    // Find the correct item to select
    for (int i = 0; i < params.size(); ++i)
    {
        if (auto* paramWithId = dynamic_cast<juce::AudioProcessorParameterWithID*> (params.getUnchecked(i)))
        {
            if (paramWithId->getParameterID() == processor.modulationRoutings[index].targetParameterID)
            {
                destinationMenu.setSelectedId (i + 1, juce::dontSendNotification);
                break;
            }
        }
    }
    destinationMenu.addListener (this);
}

ModulationMatrixRow::~ModulationMatrixRow() {}

void ModulationMatrixRow::resized()
{
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.items.add (juce::FlexItem (sourceMenu).withFlex (1.0f).withMargin (2));
    flex.items.add (juce::FlexItem (amountSlider).withFlex (2.0f).withMargin (2));
    flex.items.add (juce::FlexItem (destinationMenu).withFlex (1.0f).withMargin (2));
    flex.performLayout (getLocalBounds());
}

void ModulationMatrixRow::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &amountSlider)
        processor.modulationRoutings.getReference(index).depth = (float) amountSlider.getValue();
}

void ModulationMatrixRow::comboBoxChanged (juce::ComboBox* comboBox)
{
    if (comboBox == &sourceMenu)
        processor.modulationRoutings.getReference(index).sourceLfoIndex = sourceMenu.getSelectedId() - 1;
    
    if (comboBox == &destinationMenu)
    {
        auto* param = processor.getParameters().getUnchecked (destinationMenu.getSelectedId() - 1);
        if (auto* paramWithId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            processor.modulationRoutings.getReference(index).targetParameterID = paramWithId->getParameterID();
    }
}

//==============================================================================
// ModulationMatrixPanel Implementation
//==============================================================================
ModulationMatrixPanel::ModulationMatrixPanel (FireAudioProcessor& p) : processor (p)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&contentComponent, false);
    
    addAndMakeVisible (addButton);
    addButton.addListener (this);
    
    addAndMakeVisible (closeButton);
    closeButton.addListener (this);
    
    buildUiFromProcessorState();
    
    setSize (500, 300);
}

ModulationMatrixPanel::~ModulationMatrixPanel() {}

void ModulationMatrixPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkslategrey);
}

void ModulationMatrixPanel::resized()
{
    auto bounds = getLocalBounds();
    auto bottomArea = bounds.removeFromBottom (40);
    closeButton.setBounds (bottomArea.removeFromRight(100).reduced(5));
    addButton.setBounds (bottomArea.removeFromLeft(100).reduced(5));
    viewport.setBounds (bounds);
    
    // Set the size of the content that will be scrolled
    contentComponent.setBounds (0, 0, viewport.getMaximumVisibleWidth(), static_cast<int>(rows.size()) * 40);
    
    // Layout the rows inside the content component
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::column;
    for (auto& row : rows)
        flex.items.add (juce::FlexItem (*row).withHeight (35).withMargin (2));
    flex.performLayout (contentComponent.getLocalBounds());
}

void ModulationMatrixPanel::buttonClicked (juce::Button* button)
{
    if (button == &addButton)
    {
        processor.modulationRoutings.add ({}); // Add a new default routing
        buildUiFromProcessorState();
    }
    
    if (button == &closeButton)
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    }
}

void ModulationMatrixPanel::buildUiFromProcessorState()
{
    rows.clear();
    contentComponent.removeAllChildren();
    
    for (int i = 0; i < processor.modulationRoutings.size(); ++i)
    {
        rows.push_back (std::make_unique<ModulationMatrixRow> (processor, i));
        contentComponent.addAndMakeVisible (*rows.back());
    }
    
    resized();
}
