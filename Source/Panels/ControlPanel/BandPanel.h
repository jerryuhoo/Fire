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
#include <array>
#include <atomic>
#include <vector>

//==============================================================================
class BandPanel : public PanelBase,
                  public juce::AudioProcessorValueTreeState::Listener,
                  private juce::Timer,
                  public juce::Button::Listener,
                  public juce::ComboBox::Listener // Add ComboBox::Listener
{
public:
    BandPanel(FireAudioProcessor&,
              std::function<void(ModulatableSlider*)> onModDragStart,
              std::function<void(ModulatableSlider*)> onModDragMove,
              std::function<void(ModulatableSlider*)> onModDragEnd,
              std::function<void(ModulatableSlider*)> onHoverStart,
              std::function<void(ModulatableSlider*)> onHoverEnd);
    ~BandPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void animationTick(float deltaSeconds);
    void setFocusBandNum(int num, bool forceUpdate = false);

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    void setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass);

    juce::ToggleButton shapeBypassButton, compressorBypassButton, widthBypassButton, driveBypassButton;
    juce::ToggleButton dcFilterButton;

    int getFocusBandNum() const { return focusBandNum; }
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();
    void updateDriveMeter();

    // Public getters for graphs so PluginEditor can update them
    DistortionGraph* getDistortionGraph() { return &distortionGraph; }
    void updateRealtimeThreshold(float newThreshold);

    float scale = 1.0f;

    // A helper to get a direct pointer to the drive knob
    ModulatableSlider* getDriveKnob() { return modulatableSliderComponents.at(DRIVE_NAME).get(); }
    void setGraphVisibilityForDriveDrag(bool isDragging);

private:
    void updateAttachments();
    bool canEnableSubKnob(juce::Component& component);
    void buttonClicked(juce::Button* clickedButton) override;

    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);
    void setMenu(juce::ComboBox* combobox);
    void updateDistortionModeVisibility();
    void rebuildChromeCache(float displayScale);
    void invalidateChromeCache();
    void setAnimatedModuleTarget(int moduleIndex);
    juce::Rectangle<float> getModuleSelectionBounds(float modulePosition) const;
    juce::Colour getModuleSelectionColour() const;

    void createSliders();
    void createLabels();
    void createButtons();
    void createComboBoxes(); // New function
    void setupComponentGroups();

    void setVisibility(juce::Array<juce::Component*>& components, bool isVisible);
    void updateLinkedValue(int bandIndex);
    void updateDistortionGraphFromParameters();
    void timerCallback() override;
    std::atomic<unsigned int> linkedValueDirtyMask { 0 };
    std::atomic<unsigned int> distortionGraphDirtyMask { 0 };
    static constexpr size_t distortionGraphParameterCount = 6;
    std::array<juce::String, 4> driveParameterIds;
    std::array<juce::String, 4> linkedParameterIds;
    std::array<juce::String, 4> outputParameterIds;
    std::array<std::array<juce::String, 4>, distortionGraphParameterCount> distortionGraphParameterIds;

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

    juce::Rectangle<int> knobsAreaRect;
    juce::Rectangle<int> outputAreaRect;
    juce::Rectangle<int> tabAreaRect;
    juce::Rectangle<int> graphAreaRect; // Area for the graphs
    juce::Image chromeCache;
    float chromeCacheDisplayScale = 0.0f;
    bool chromeCacheDirty = true;

    fire::ui::DampedValue moduleSelectionPosition;
    fire::ui::DampedValue moduleSelectionColourMix;
    juce::Colour moduleSelectionColourStart { fire::ui::colours::drive };
    juce::Colour moduleSelectionColourTarget { fire::ui::colours::drive };

    // Graphs moved from GraphPanel
    Oscilloscope oscilloscope { processor };
    DistortionGraph distortionGraph { processor };
    VUPanel vuPanel { processor };
    WidthGraph widthGraph { processor };

    // Distortion modes moved from PluginEditor
    std::array<juce::ComboBox, 4> distortionModes;
    std::array<std::unique_ptr<ComboBoxAttachment>, 4> modeAttachments;

    juce::Component* preDragVisibleGraph = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPanel)
};
