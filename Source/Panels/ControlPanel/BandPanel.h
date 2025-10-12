/*
  ==============================================================================

    BandPanel.h
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "PanelBase.h" // [MODIFIED] Include the new base class
#include "juce_gui_basics/juce_gui_basics.h"
#include <vector>

//==============================================================================
/*
*/
class BandPanel : public PanelBase, // [MODIFIED] Inherit from PanelBase
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
    juce::ToggleButton compressorBypassButton, widthBypassButton;
    int getFocusBandNum() const { return focusBandNum; }
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();
    void updateDriveMeter();
    void saveBypassStatesToMemory();

    // [REMOVED] The public list of sliders is now in PanelBase.
    // The editor can access it via a public getter in PanelBase if needed,
    // or by casting the component to PanelBase*. For now we assume internal access is sufficient.

private:
    // Re-attaches all UI components to the parameters of the current focusBandNum.
    void updateAttachments();

    // [REMOVED] setupSliderCallbacks is now in PanelBase as setupModulationCallbacks

    void updateLinkedValue();
    bool canEnableSubKnob(juce::Component& component);

    void buttonClicked(juce::Button* clickedButton) override;
    // Initialization helpers
    // [REMOVED] initRotarySlider is now in PanelBase.
    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);

    void createSliders();
    void createLabels();
    void createButtons();
    void setupComponentGroups();

    // Sets visibility for a group of components.
    void setVisibility(juce::Array<juce::Component*>& components, bool isVisible);

    // UI layout areas
    juce::Rectangle<int> bandKnobArea;
    juce::Rectangle<int> driveKnobArea;
    juce::Rectangle<int> outputKnobArea;
    juce::Rectangle<int> bottomArea;

    // === Refactored UI Component Management ===
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // A map to own and manage all labels, keyed by the parameter name they are associated with.
    std::map<juce::String, std::unique_ptr<juce::Label>> labels;

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