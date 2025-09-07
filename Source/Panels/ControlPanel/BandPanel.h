/*
  ==============================================================================

    BandPanel.h
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "../../PluginProcessor.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <map>
#include <vector>

//==============================================================================
/*
*/
class BandPanel : public juce::Component,
                  public juce::AudioProcessorValueTreeState::Listener,
                  public juce::AsyncUpdater,
                  public juce::Button::Listener
{
public:
    BandPanel(FireAudioProcessor&);
    ~BandPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setFocusBandNum(int num, bool forceUpdate = false);

    void handleAsyncUpdate() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass);
    juce::ToggleButton compressorBypassButton, widthBypassButton;
    int getFocusBandNum() const { return focusBandNum; }
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();
    void updateDriveMeter();
    void saveBypassStatesToMemory();

    // A public list of all modulatable sliders for the editor to access and update.
    std::vector<ModulatableSlider*> modulatableSliders;

private:
    // Re-attaches all UI components to the parameters of the current focusBandNum.
    void updateAttachments();

    void configureModulatableSlider(ModulatableSlider& slider, const juce::String& paramIDBase);

    void updateLinkedValue();
    bool canEnableSubKnob(juce::Component& component);

    void buttonClicked(juce::Button* clickedButton) override;
    // Initialization helpers
    void initRotarySlider(juce::Slider& slider, juce::Colour colour);
    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);

    void createSliders();
    void createLabels();
    void createButtons();
    void setupComponentGroups();

    // Sets visibility for a group of components.
    void setVisibility(juce::Array<juce::Component*>& components, bool isVisible);

    FireAudioProcessor& processor;

    // UI layout areas
    juce::Rectangle<int> bandKnobArea;
    juce::Rectangle<int> driveKnobArea;
    juce::Rectangle<int> outputKnobArea;
    juce::Rectangle<int> bottomArea;

    // === Refactored UI Component Management ===
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // A map to own and manage all sliders, keyed by their parameter name.
    std::map<juce::String, std::unique_ptr<ModulatableSlider>> modulatableSliderComponents;

    // A map to own and manage all labels, keyed by the parameter name they are associated with.
    std::map<juce::String, std::unique_ptr<juce::Label>> labels;

    // A map to own the attachments, ensuring they are re-created correctly when the band changes.
    std::map<juce::String, std::unique_ptr<SliderAttachment>> sliderAttachments;

    // --- Unchanged Members ---
    juce::Label shapePanelLabel, compressorPanelLabel, widthPanelLabel;

    juce::TextButton linkedButton, safeButton, extremeButton;

    std::unique_ptr<ButtonAttachment> linkedAttachment, safeAttachment, extremeAttachment,
        compressorBypassAttachment, widthBypassAttachment;

    juce::ToggleButton oscSwitch, shapeSwitch, widthSwitch, compressorSwitch;
    enum RadioButtonIds
    {
        switchButtons = 1004
    };

    // Groups of components for easy visibility toggling.
    juce::Array<juce::Component*> shapeComponents;
    juce::Array<juce::Component*> widthComponents;
    juce::Array<juce::Component*> compressorComponents;
    juce::Array<juce::Component*> allControls;
    juce::Array<juce::Component*> mainControls;
    juce::Array<juce::Slider*> compressorKnobs;
    juce::Array<juce::Slider*> widthKnobs;

    int focusBandNum;

    // Store bypass states for all 4 bands, since the UI only shows one at a time.
    bool compBypassTemp[4] = { false };
    bool widthBypassTemp[4] = { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPanel)
};