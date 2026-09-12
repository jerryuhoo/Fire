/*
  ==============================================================================

    GlobalPanel.h
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/ContextAwareComboBox.h"
#include "../../GUI/LookAndFeel.h"
#include "Graph Components/Oscilloscope.h"
#include "Graph Components/VUPanel.h"
#include "Graph Components/WidthGraph.h"
#include "PanelBase.h"
#include "../../GUI/InsertEffectControls.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <cstdint>
#include <vector>

struct GlobalPanelSlopeTestAccess;

//==============================================================================
/*
*/
class GlobalPanel : public PanelBase,
                    public juce::Button::Listener
{
public:
    GlobalPanel(FireAudioProcessor& p,
                std::function<void(ModulatableSlider*)> onModDragStart,
                std::function<void(ModulatableSlider*)> onModDragMove,
                std::function<void(ModulatableSlider*)> onModDragEnd,
                std::function<void(ModulatableSlider*)> onHoverStart,
                std::function<void(ModulatableSlider*)> onHoverEnd);
    ~GlobalPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void animationTick(float deltaSeconds);
    void setScale(float newScale);
    void dismissTransientInteraction() noexcept;

    ModulatableSlider& getLowcutFreqKnob();
    ModulatableSlider& getPeakFreqKnob();
    ModulatableSlider& getHighcutFreqKnob();
    ModulatableSlider& getLowcutGainKnob();
    ModulatableSlider& getPeakGainKnob();
    ModulatableSlider& getHighcutGainKnob();

    void setToggleButtonState(juce::String toggleButton);
    void presentMeterValues(const MeterValues& values, std::uint64_t generation);
    float scale = 1.0f;

private:
    friend struct GlobalPanelSlopeTestAccess;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Initialization helpers
    void createSliders();
    void selectInsertEffect(int slot);
    void createLabels();
    void createButtons();
    void createComboBoxes();
    void setupComponentGroups();

    // initRotarySlider is now in PanelBase.
    void initFlatButton(juce::TextButton& button, juce::String buttonName); // This seems to be missing, keeping for consistency.
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);
    void setRoundButton(juce::TextButton& button, juce::String, juce::String buttonName);

    // Re-attaches all UI components to their parameters.
    void updateAttachments();

    // UI update helpers
    void updateFilterKnobVisibility();
    void rebuildChromeCache(float displayScale);
    void invalidateChromeCache();
    void configureGraphInteractions();
    void toggleGraphZoom(GraphTemplate* graph);
    void clearGraphZoom() noexcept;
    void hideComponentsObscuredByZoom(const GraphTemplate& graph);
    void restoreComponentsObscuredByZoom() noexcept;
    void updateSelectionTarget(bool snap);
    juce::TextButton* getSelectedSwitch() noexcept;
    void invalidateSlopeInteractions() noexcept;
    bool canOpenSlopePopup(bool lowCut) const noexcept;

    void buttonClicked(juce::Button* clickedButton) override;
    void visibilityChanged() override;
    void enablementChanged() override;

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

    // UI layout areas to match BandPanel style
    juce::Rectangle<int> tabAreaRect;
    juce::Rectangle<int> controlsAreaRect;
    juce::Rectangle<int> outputAreaRect;

    // --- Buttons ---
    PrimaryTextButton filterLowCutButton, filterPeakButton, filterHighCutButton;
    // Changed ToggleButton to TextButton for tab-like functionality
    PrimaryTextButton filterSwitch, downsampleSwitch, graphSwitch;
    std::unique_ptr<PrimaryToggleButton> filterBypassButton,
        downsampleBypassButton;

    std::unique_ptr<ButtonAttachment> filterLowAttachment, filterBandAttachment, filterHighAttachment,
        filterBypassAttachment, downsampleBypassAttachment;

    // --- ComboBoxes ---
    ContextAwareComboBox lowcutSlopeMode, highcutSlopeMode;
    std::unique_ptr<ComboBoxAttachment> lowcutModeAttachment, highcutModeAttachment;
    std::uint64_t slopeInteractionGeneration = 0;

    // --- Labels ---
    // Removed panel labels, as the switches now serve as titles.
    juce::Label filterTypeLabel, lowcutSlopeLabel, highcutSlopeLabel;

    // Groups of components for easy visibility toggling.
    juce::Array<juce::Component*> filterComponents;
    juce::Array<juce::Component*> downsampleComponents;
    juce::Array<juce::Component*> graphComponents;
    juce::Array<juce::Component*> lowcutKnobs;
    juce::Array<juce::Component*> peakKnobs;
    juce::Array<juce::Component*> highcutKnobs;
    juce::Array<juce::Component*> allControls;

    juce::Image chromeCache;
    float chromeCacheDisplayScale = 0.0f;
    bool chromeCacheDirty = true;

    fire::ui::SpringValue selectionY;
    bool selectionAnimationInitialised = false;

    Oscilloscope oscilloscope { processor };
    VUPanel vuPanel { processor };
    WidthGraph widthGraph { processor };
    GraphTemplate* zoomedGraph = nullptr;
    std::vector<juce::Component::SafePointer<juce::Component>>
        componentsHiddenForGraphZoom;
    fire::ui::EffectRackNavigation effectNavigation {processor, 0};
    InsertEffectControls insertControls {processor};
    int selectedInsert = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalPanel)
};
