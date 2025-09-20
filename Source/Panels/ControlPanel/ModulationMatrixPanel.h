/*
  ==============================================================================

    ModulationMatrixPanel.h
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../../PluginProcessor.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "../../Utility/Parameters.h"

//
//  A header component to display titles for the matrix columns.
//
class ModulationMatrixHeader : public juce::Component
{
public:
    ModulationMatrixHeader();
    void resized() override;

private:
    juce::Label sourceLabel;
    juce::Label amountLabel;
    juce::Label polarityLabel;
    juce::Label destinationLabel;
};

//
//  A single row in our modulation matrix UI.
//
class ModulationMatrixRow : public juce::Component,
                            public juce::Button::Listener,
                            public juce::Slider::Listener,
                            public juce::ComboBox::Listener
{
public:
    // The constructor now accepts a callback function to handle its deletion.
    ModulationMatrixRow(FireAudioProcessor& p, int routingIndex, std::function<void()> onDelete);
    ~ModulationMatrixRow() override;

    void resized() override;

private:
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    FireAudioProcessor& processor;
    int index; // The index of the routing this row represents in the processor's array
    std::function<void()> onDeleteCallback; // The function to call when the delete button is pressed.

    juce::ComboBox sourceMenu;
    juce::Slider amountSlider;
    juce::TextButton bipolarButton;
    juce::ComboBox destinationMenu;
    juce::TextButton removeButton { "X" };

    std::vector<ModulationTarget> allPossibleTargets;
};

//
//  The main panel that holds all the modulation routing rows.
//
class ModulationMatrixPanel : public juce::Component,
                              public juce::Button::Listener
{
public:
    ModulationMatrixPanel(FireAudioProcessor& p);
    ~ModulationMatrixPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Rebuilds the UI from the processor's data model
    void buildUiFromProcessorState();

private:
    void buttonClicked(juce::Button* button) override;
    FireAudioProcessor& processor;

    ModulationMatrixHeader header;
    std::vector<std::unique_ptr<ModulationMatrixRow>> rows;
    juce::TextButton addButton { "Add New" };
    juce::TextButton closeButton { "Close" };

    juce::Viewport viewport;
    juce::Component contentComponent;
};
