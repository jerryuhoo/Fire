/*
  ==============================================================================

    GlobalPanel.h
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "../../GUI/ModulatableSlider.h"
#include "../../PluginProcessor.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <map>
#include <vector>

//==============================================================================
/*
*/
class GlobalPanel : public juce::Component,
                    public juce::ComboBox::Listener,
                    public juce::Button::Listener
{
public:
    GlobalPanel(FireAudioProcessor& p);
    ~GlobalPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // A public list of all modulatable sliders for the editor to access and update.
    std::vector<ModulatableSlider*> modulatableSliders;

    ModulatableSlider& getLowcutFreqKnob();
    ModulatableSlider& getPeakFreqKnob();
    ModulatableSlider& getHighcutFreqKnob();
    ModulatableSlider& getLowcutGainKnob();
    ModulatableSlider& getPeakGainKnob();
    ModulatableSlider& getHighcutGainKnob();

    void setToggleButtonState(juce::String toggleButton);

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

    void initRotarySlider(juce::Slider& slider, juce::Colour colour);
    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);
    void setRoundButton(juce::TextButton& button, juce::String paramId, juce::String buttonName);

    // Re-attaches all UI components to their parameters.
    void updateAttachments();
    void configureModulatableSlider(ModulatableSlider& slider, const juce::String& paramID);

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

    FireAudioProcessor& processor;

    // UI layout areas
    juce::Rectangle<int> globalEffectArea;
    juce::Rectangle<int> outputKnobArea;

    // === UI Component Management ===
    std::map<juce::String, std::unique_ptr<ModulatableSlider>> modulatableSliderComponents;
    std::map<juce::String, std::unique_ptr<juce::Label>> labels;
    std::map<juce::String, std::unique_ptr<SliderAttachment>> sliderAttachments;

    // --- Buttons ---
    juce::TextButton filterLowCutButton, filterPeakButton, filterHighCutButton;
    juce::ToggleButton filterSwitch, downsampleSwitch;
    std::unique_ptr<juce::ToggleButton> filterBypassButton, downsampleBypassButton;

    std::unique_ptr<ButtonAttachment> filterLowAttachment, filterBandAttachment, filterHighAttachment,
        filterBypassAttachment, downsampleBypassAttachment;

    // --- ComboBoxes ---
    juce::ComboBox lowcutSlopeMode, highcutSlopeMode;
    std::unique_ptr<ComboBoxAttachment> lowcutModeAttachment, highcutModeAttachment;

    // --- Labels ---
    juce::Label filterTypeLabel, lowcutSlopeLabel, highcutSlopeLabel, postFilterPanelLabel, downSamplePanelLabel;

    // Groups of components for easy visibility toggling.
    juce::Array<juce::Component*> filterComponents;
    juce::Array<juce::Component*> downsampleComponents;
    juce::Array<juce::Component*> lowcutKnobs;
    juce::Array<juce::Component*> peakKnobs;
    juce::Array<juce::Component*> highcutKnobs;
    juce::Array<juce::Component*> allControls;

    juce::Rectangle<int> bottomArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalPanel)
};