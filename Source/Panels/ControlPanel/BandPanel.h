/*
  ==============================================================================

    BandPanel.h
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "../ControlPanel/Graph Components/DistortionGraph.h"
#include "../ControlPanel/Graph Components/Oscilloscope.h"
#include "../ControlPanel/Graph Components/VUPanel.h"
#include "../ControlPanel/Graph Components/WidthGraph.h"
#include "PanelBase.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <vector>

//==============================================================================
class BandPanel : public PanelBase,
                  public juce::AudioProcessorValueTreeState::Listener,
                  public juce::AsyncUpdater,
                  public juce::Button::Listener,
                  public juce::ComboBox::Listener // Add ComboBox::Listener
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
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    void setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass);

    juce::ToggleButton shapeBypassButton, compressorBypassButton, widthBypassButton, driveBypassButton;
    juce::ToggleButton dcFilterButton;

    int getFocusBandNum() const { return focusBandNum; }
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();
    void updateDriveMeter();
    void saveBypassStatesToMemory();

    // Public getters for graphs so PluginEditor can update them
    DistortionGraph* getDistortionGraph() { return &distortionGraph; }
    void updateRealtimeThreshold(float newThreshold);

    float scale = 1.0f;

private:
    void updateAttachments();
    bool canEnableSubKnob(juce::Component& component);
    void buttonClicked(juce::Button* clickedButton) override;

    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);
    void setMenu(juce::ComboBox* combobox);
    void updateDistortionModeVisibility();

    void createSliders();
    void createLabels();
    void createButtons();
    void createComboBoxes(); // New function
    void setupComponentGroups();

    void setVisibility(juce::Array<juce::Component*>& components, bool isVisible);
    void updateLinkedValue();

    juce::Rectangle<int> bandKnobArea, driveKnobArea, outputKnobArea, bottomArea;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::map<juce::String, std::unique_ptr<juce::Label>> labels;
    juce::Label shapePanelLabel, compressorPanelLabel, widthPanelLabel;
    juce::Label dcFilterLabel;

    juce::TextButton linkedButton, safeButton, extremeButton;

    std::unique_ptr<ButtonAttachment> linkedAttachment, safeAttachment, extremeAttachment,
        shapeBypassAttachment, compressorBypassAttachment, widthBypassAttachment, dcFilterAttachment, driveBypassAttachment;

    juce::TextButton oscSwitch, shapeSwitch, widthSwitch, compressorSwitch;
    enum RadioButtonIds
    {
        switchButtons = 1004
    };

    // Groups for visibility
    juce::Array<juce::Component*> shapeComponents;
    juce::Array<juce::Component*> widthComponents;
    juce::Array<juce::Component*> compressorComponents;
    juce::Array<juce::Component*> driveComponents; // New group for drive
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
    bool driveBypassTemp[4] = { false };
    bool dcFilterBypassTemp[4] = { false };

    juce::Rectangle<int> knobsAreaRect;
    juce::Rectangle<int> outputAreaRect;
    juce::Rectangle<int> tabAreaRect;
    juce::Rectangle<int> graphAreaRect; // Area for the graphs
    juce::Colour activeTabColour;

    // Graphs moved from GraphPanel
    Oscilloscope oscilloscope { processor };
    DistortionGraph distortionGraph { processor };
    VUPanel vuPanel { processor };
    WidthGraph widthGraph { processor };

    // Distortion modes moved from PluginEditor
    std::array<juce::ComboBox, 4> distortionModes;
    std::array<std::unique_ptr<ComboBoxAttachment>, 4> modeAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPanel)
};