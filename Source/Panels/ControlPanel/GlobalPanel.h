/*
  ==============================================================================

    GlobalPanel.h
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "PanelBase.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <vector>

//==============================================================================
/*
*/
class GlobalPanel : public PanelBase,
                    public juce::ComboBox::Listener,
                    public juce::Button::Listener
{
public:
    GlobalPanel(FireAudioProcessor& p,
                std::function<void(ModulatableSlider*)> onDragStart,
                std::function<void(ModulatableSlider*)> onDragMove,
                std::function<void(ModulatableSlider*)> onDragEnd,
                std::function<void(ModulatableSlider*)> onHoverStart,
                std::function<void(ModulatableSlider*)> onHoverEnd);
    ~GlobalPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    ModulatableSlider& getLowcutFreqKnob();
    ModulatableSlider& getPeakFreqKnob();
    ModulatableSlider& getHighcutFreqKnob();
    ModulatableSlider& getLowcutGainKnob();
    ModulatableSlider& getPeakGainKnob();
    ModulatableSlider& getHighcutGainKnob();

    void setToggleButtonState(juce::String toggleButton);
    float scale = 1.0f;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Initialization helpers
    void createSliders();
    void createLabels();
    void createButtons();
    void createComboBoxes();
    void setupComponentGroups();

    // initRotarySlider is now in PanelBase.
    void initFlatButton(juce::TextButton& button, juce::String buttonName); // This seems to be missing, keeping for consistency.
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);
    void setRoundButton(juce::TextButton& button, juce::String paramId, juce::String buttonName);

    // Re-attaches all UI components to their parameters.
    void updateAttachments();

    // UI update helpers
    void updateFilterKnobVisibility();

    void buttonClicked(juce::Button* clickedButton) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override {}

    void setBypassState(int index, bool state);
    void setVisibility(juce::Array<juce::Component*>& array, bool isVisible);

    enum RadioButtonIds
    {
        // filter state: off, pre, post
        filterStateButtons = 1001,
        // filter mode: low, band, high
        filterModeButtons = 1002,
        // switches global
        switchButtonsGlobal = 1005
    };

    // [MODIFIED] UI layout areas to match BandPanel style
    juce::Rectangle<int> tabAreaRect;
    juce::Rectangle<int> outputAreaRect;

    // --- Buttons ---
    juce::TextButton filterLowCutButton, filterPeakButton, filterHighCutButton;
    // [MODIFIED] Changed ToggleButton to TextButton for tab-like functionality
    juce::TextButton filterSwitch, downsampleSwitch;
    std::unique_ptr<juce::ToggleButton> filterBypassButton, downsampleBypassButton;

    std::unique_ptr<ButtonAttachment> filterLowAttachment, filterBandAttachment, filterHighAttachment,
        filterBypassAttachment, downsampleBypassAttachment;

    // --- ComboBoxes ---
    juce::ComboBox lowcutSlopeMode, highcutSlopeMode;
    std::unique_ptr<ComboBoxAttachment> lowcutModeAttachment, highcutModeAttachment;

    // --- Labels ---
    // [MODIFIED] Removed panel labels, as the switches now serve as titles.
    juce::Label filterTypeLabel, lowcutSlopeLabel, highcutSlopeLabel;

    // Groups of components for easy visibility toggling.
    juce::Array<juce::Component*> filterComponents;
    juce::Array<juce::Component*> downsampleComponents;
    juce::Array<juce::Component*> lowcutKnobs;
    juce::Array<juce::Component*> peakKnobs;
    juce::Array<juce::Component*> highcutKnobs;
    juce::Array<juce::Component*> allControls;

    // [REMOVED] juce::Rectangle<int> bottomArea;

    // [NEW] Added to store active tab colour for painting, like in BandPanel
    juce::Colour activeTabColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalPanel)
};