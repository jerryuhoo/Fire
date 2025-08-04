/*
  ==============================================================================

    ModulationMatrixPanel.h
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../PluginProcessor.h"

//
//  A single row in our modulation matrix UI.
//  It contains controls for source, amount, and destination.
//
class ModulationMatrixRow : public juce::Component,
                            public juce::Slider::Listener,
                            public juce::ComboBox::Listener
{
public:
    ModulationMatrixRow (FireAudioProcessor& p, int routingIndex);
    ~ModulationMatrixRow() override;
    
    void resized() override;
    
private:
    void sliderValueChanged (juce::Slider* slider) override;
    void comboBoxChanged (juce::ComboBox* comboBox) override;
    
    FireAudioProcessor& processor;
    int index; // The index of the routing this row represents in the processor's array
    
    juce::ComboBox sourceMenu;
    juce::Slider amountSlider;
    juce::ComboBox destinationMenu;
};

//
//  The main panel that holds all the modulation routing rows.
//
class ModulationMatrixPanel : public juce::Component,
                              public juce::Button::Listener
{
public:
    ModulationMatrixPanel (FireAudioProcessor& p);
    ~ModulationMatrixPanel() override;
    
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked (juce::Button* button) override;
    
    // Rebuilds the UI from the processor's data model
    void buildUiFromProcessorState();
    
    FireAudioProcessor& processor;
    
    std::vector<std::unique_ptr<ModulationMatrixRow>> rows;
    juce::TextButton addButton { "Add New" };
    juce::TextButton closeButton { "Close" };
    
    juce::Viewport viewport;
    juce::Component contentComponent;
};
