/*
  ==============================================================================

    BandPanel.h
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "PanelBase.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <vector>

//==============================================================================
class BandPanel : public PanelBase,
                  public juce::AudioProcessorValueTreeState::Listener,
                  public juce::AsyncUpdater,
                  public juce::Button::Listener
{
public:
    BandPanel(FireAudioProcessor&,
              std::function<void(ModulatableSlider*)> onDragStart,
              std::function<void(ModulatableSlider*)> onDragMove,
              std::function<void(ModulatableSlider*)> onDragEnd,
              std::function<void(ModulatableSlider*)> onHoverStart,
              std::function<void(ModulatableSlider*)> onHoverEnd);
    ~BandPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setFocusBandNum(int num, bool forceUpdate = false);

    void handleAsyncUpdate() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass);

    juce::ToggleButton shapeBypassButton, compressorBypassButton, widthBypassButton;
    juce::ToggleButton dcFilterButton;

    int getFocusBandNum() const { return focusBandNum; }
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();
    void updateDriveMeter();
    void saveBypassStatesToMemory();

private:
    void updateAttachments();
    bool canEnableSubKnob(juce::Component& component);
    void buttonClicked(juce::Button* clickedButton) override;

    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);

    void createSliders();
    void createLabels();
    void createButtons();
    void setupComponentGroups();

    void setVisibility(juce::Array<juce::Component*>& components, bool isVisible);
    void updateLinkedValue();

    juce::Rectangle<int> bandKnobArea, driveKnobArea, outputKnobArea, bottomArea;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::map<juce::String, std::unique_ptr<juce::Label>> labels;
    juce::Label shapePanelLabel, compressorPanelLabel, widthPanelLabel;
    juce::Label dcFilterLabel;

    juce::TextButton linkedButton, safeButton, extremeButton;

    std::unique_ptr<ButtonAttachment> linkedAttachment, safeAttachment, extremeAttachment,
        shapeBypassAttachment, compressorBypassAttachment, widthBypassAttachment, dcFilterAttachment;

    juce::ToggleButton oscSwitch, shapeSwitch, widthSwitch, compressorSwitch;
    enum RadioButtonIds
    {
        switchButtons = 1004
    };

    // Groups for visibility
    juce::Array<juce::Component*> shapeComponents;
    juce::Array<juce::Component*> widthComponents;
    juce::Array<juce::Component*> compressorComponents;
    juce::Array<juce::Component*> allControls;

    // Groups for enable/disable logic
    juce::Array<juce::Component*> shapeSubControls;
    juce::Array<juce::Component*> compressorSubControls;
    juce::Array<juce::Component*> widthSubControls;

    int focusBandNum;

    // Add temp state for the new button
    bool shapeBypassTemp[4] = { false };
    bool compBypassTemp[4] = { false };
    bool widthBypassTemp[4] = { false };
    bool dcFilterBypassTemp[4] = { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPanel)
};