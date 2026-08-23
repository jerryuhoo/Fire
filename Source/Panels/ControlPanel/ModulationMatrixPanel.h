/*
  ==============================================================================

    ModulationMatrixPanel.h
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "../../PluginProcessor.h"
#include "../../Utility/Parameters.h"
#include "juce_gui_basics/juce_gui_basics.h"

//
//  A header component to display titles for the matrix columns.
//
class ModulationMatrixHeader : public juce::Component
{
public:
    ModulationMatrixHeader();
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label sourceLabel;
    juce::Label amountLabel;
    juce::Label polarityLabel;
    juce::Label bypassLabel;
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
    ModulationMatrixRow(FireAudioProcessor& p,
                        int routingIndex,
                        const ModulationRouting& routing,
                        std::function<void()> onDelete);
    ~ModulationMatrixRow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class PrimaryButtonSlider final : public juce::Slider
    {
    public:
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        bool primaryGestureInProgress = false;
    };

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    bool isParentRebuildPending();
    void requestParentRebuild();

    FireAudioProcessor& processor;
    FireLookAndFeel fireLookAndFeel;
    int index; // The index of the routing this row represents in the processor's array
    const juce::String targetParameterIDAtBuild;
    std::function<void()> onDeleteCallback; // The function to call when the delete button is pressed.

    juce::ComboBox sourceMenu;
    PrimaryButtonSlider amountSlider;
    juce::TextButton bipolarButton;
    juce::TextButton bypassButton;
    juce::ComboBox destinationMenu;
    juce::TextButton removeButton;

    std::vector<ModulationTarget> allPossibleTargets;
};

//
//  The main panel that holds all the modulation routing rows.
//
class ModulationMatrixPanel : public juce::Component,
                              public juce::Button::Listener,
                              private juce::ChangeListener,
                              private juce::AsyncUpdater
{
public:
    ModulationMatrixPanel(FireAudioProcessor& p);
    ~ModulationMatrixPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Rebuilds the UI from the processor's data model
    void buildUiFromProcessorState();
    void requestUiRebuild();
    bool isUiRebuildPending() const noexcept;

private:
    void buttonClicked(juce::Button* button) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void handleAsyncUpdate() override;
    FireAudioProcessor& processor;
    FireLookAndFeel fireLookAndFeel;

    ModulationMatrixHeader header;
    std::vector<std::unique_ptr<ModulationMatrixRow>> rows;
    juce::TextButton addButton { "+" };
    juce::TextButton closeButton { "Close" };

    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::Rectangle<int> titleArea;
};
