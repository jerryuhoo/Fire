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

    //    void changeSliderState(int bandNum, bool isPresetChanged);
    void setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass);
    juce::ToggleButton compressorBypassButton, widthBypassButton;
    int getFocusBandNum() const { return focusBandNum; }
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();
    void updateDriveMeter();

    // Handles the logic for the sub-module bypass buttons (Compressor, Width).
    void saveBypassStatesToMemory();

    ModulatableSlider recKnob, biasKnob;

private:
    // Re-attaches all UI components to the parameters of the current focusBandNum.
    void updateAttachments();
    // Helper to generate parameter IDs based on a base string and the band index.
    juce::String getParamID(const juce::String& base, int bandIndex);

    void updateLinkedValue();
    bool canEnableSubKnob(juce::Component& component);

    void buttonClicked(juce::Button* clickedButton) override;
    // Initialization helpers
    void initRotarySlider(juce::Slider& slider, juce::Colour colour);
    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);

    // Sets visibility for a group of components.
    void setVisibility(juce::Array<juce::Component*>& components, bool isVisible);

    FireAudioProcessor& processor;

    // UI layout areas
    juce::Rectangle<int> bandKnobArea;
    juce::Rectangle<int> driveKnobArea;
    juce::Rectangle<int> outputKnobArea;
    juce::Rectangle<int> bottomArea;

    juce::Slider driveKnob, outputKnob, mixKnob,
        compRatioKnob, compThreshKnob, widthKnob;

    juce::Label driveLabel, CompRatioLabel, CompThreshLabel, widthLabel,
        outputLabel, recLabel, mixLabel, biasLabel,
        shapePanelLabel, compressorPanelLabel, widthPanelLabel;

    juce::TextButton linkedButton, safeButton, extremeButton;

    // Panel switching buttons
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

    // A single set of attachments, dynamically reassigned.
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> driveAttachment, outputAttachment, mixAttachment,
        recAttachment, biasAttachment, compRatioAttachment,
        compThreshAttachment, widthAttachment;

    std::unique_ptr<ButtonAttachment> linkedAttachment, safeAttachment, extremeAttachment,
        compressorBypassAttachment, widthBypassAttachment;

    int focusBandNum;

    // Store bypass states for all 4 bands, since the UI only shows one at a time.
    bool compBypassTemp[4] = { false };
    bool widthBypassTemp[4] = { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPanel)
};
