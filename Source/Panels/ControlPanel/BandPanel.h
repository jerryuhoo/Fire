/*
  ==============================================================================

    BandPanel.h
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/ContextAwareComboBox.h"
#include "../../GUI/LookAndFeel.h"
#include "../ControlPanel/Graph Components/DistortionGraph.h"
#include "Graph Components/OttGraph.h"
#include "../ControlPanel/Graph Components/Oscilloscope.h"
#include "../ControlPanel/Graph Components/VUPanel.h"
#include "../ControlPanel/Graph Components/WidthGraph.h"
#include "PanelBase.h"
#include "../../GUI/InsertEffectControls.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

struct BandPanelGraphTestAccess;
struct BandPanelModeTestAccess;

//==============================================================================
class BandPanel : public PanelBase,
                  public juce::AudioProcessorValueTreeState::Listener,
                  private juce::Timer,
                  public juce::Button::Listener
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
    void setScale(float newScale);
    void setFocusBandNum(int num, bool forceUpdate = false);
    void dismissTransientInteraction() noexcept;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void presentDistortionGraphValues(const DistortionGraphValues& values);
    bool isOttSelected() const { return ottSwitch.getToggleState(); }
    int getOttPreviewDirection() const;
    void setOttSpectrumVisuals(const fire::ui::OttSpectrumProfile& profile, float centroid, int interactionDirection)
    {
        ottGraph.setSpectrum(profile, centroid);
        spectrumOttInteraction = interactionDirection;
    }
    std::function<void()> onModuleChanged;

    void setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass);

    PrimaryToggleButton shapeBypassButton, compressorBypassButton,
        widthBypassButton, driveBypassButton, ottBypassButton;
    PrimaryToggleButton dcFilterButton;

    int getFocusBandNum() const { return focusBandNum; }
    void setSwitch(const int index, bool state);
    void updateWhenChangingFocus();
    void updateDriveMeter();

    // Public getters for graphs so PluginEditor can update them
    DistortionGraph* getDistortionGraph() { return &distortionGraph; }
    void updateRealtimeThreshold(float newThreshold);
    void presentMeterValues(const MeterValues& values, std::uint64_t generation);

    float scale = 1.0f;

    // A helper to get a direct pointer to the drive knob
    ModulatableSlider* getDriveKnob() { return modulatableSliderComponents.at(DRIVE_NAME).get(); }
    void setGraphVisibilityForDriveDrag(bool isDragging);

private:
    friend struct BandPanelGraphTestAccess;
    friend struct BandPanelModeTestAccess;
    friend struct DriveCompensationUiTestAccess;
    friend struct DistortionGraphSourceEpochTestAccess;

    void updateAttachments();
    void dismissButtonInteractions() noexcept;
    void dismissTransientInteractionForParameterRebind() noexcept;
    void invalidateDistortionModeInteractions() noexcept;
    bool canOpenDistortionModePopup(size_t modeIndex) const noexcept;
    bool hasActiveSliderInteraction() const noexcept;
    void applyPendingFocusChange();
    bool canEnableSubKnob(juce::Component& component);
    void buttonClicked(juce::Button* clickedButton) override;

    void initFlatButton(juce::TextButton& button, juce::String buttonName);
    void initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour);
    void setMenu(juce::ComboBox* combobox);
    void updateDistortionModeVisibility();
    void rebuildChromeCache(float displayScale);
    void invalidateChromeCache();
    void configureGraphInteractions();
    void configureGraphViewMenu();
    void selectGraphView(int itemId);
    void applySelectedGraphView();
    void updateGraphViewMenu();
    void dismissGraphViewMenu() noexcept;
    GraphTemplate* getAutomaticModuleGraph() noexcept;
    void toggleGraphZoom(GraphTemplate* graph);
    void clearGraphZoom() noexcept;
    void hideComponentsObscuredByZoom(const GraphTemplate& graph);
    void restoreComponentsObscuredByZoom() noexcept;
    GraphTemplate* getSelectedModuleGraph() noexcept;
    void restoreDriveGraphPreviewNow() noexcept;
    void setAnimatedModuleTarget(int moduleIndex);
    juce::Rectangle<float> getModuleSelectionBounds(float modulePosition) const;

    void createSliders();
    void selectInsertEffect(int slot);
    void refreshInsertLayout();
    void createLabels();
    void createButtons();
    void createComboBoxes(); // New function
    void setupComponentGroups();
    void updateIconButtonSemantics();
    bool usesModernDriveCompensation() const noexcept;
    void updateDriveCompensationPresentation(bool updateLayout = true);

    void setVisibility(juce::Array<juce::Component*>& components, bool isVisible);
    void updateDistortionGraphFromParameters();
    void timerCallback() override;
    void visibilityChanged() override;
    void enablementChanged() override;
    void parentHierarchyChanged() override;
    std::atomic<unsigned int> distortionGraphDirtyMask { 0 };
    double lastGraphTelemetryTimeMs = -1.0;
    static constexpr double graphTelemetryTimeoutMs = 250.0;
    static constexpr size_t distortionGraphParameterCount = 9;
    std::array<juce::String, 4> driveParameterIds;
    std::array<std::array<juce::String, 4>, distortionGraphParameterCount> distortionGraphParameterIds;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::map<juce::String, std::unique_ptr<juce::Label>> labels;
    juce::Label shapePanelLabel, compressorPanelLabel, widthPanelLabel;
    juce::Label dcFilterLabel;

    PrimaryTextButton linkedButton, safeButton, extremeButton;
    PrimaryTextButton upgradeDriveCompButton;
    juce::Label driveCompReadout;
    struct DriveCompPresentationParameters
    {
        std::atomic<float>* modern = nullptr;
        std::atomic<float>* linked = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* extreme = nullptr;
        std::atomic<float>* driveEnabled = nullptr;
        std::atomic<float>* bandEnabled = nullptr;
    };
    std::array<DriveCompPresentationParameters, 4> driveCompParameters {};
    int displayedDriveCompMode = -1;
    int displayedDriveCompBand = -1;
    std::array<std::uint64_t, 4> driveCompSequences {};
    std::array<double, 4> driveCompReceivedAtMs { -1.0, -1.0, -1.0, -1.0 };
    static constexpr double driveCompTelemetryTimeoutMs = 250.0;

    std::unique_ptr<ButtonAttachment> linkedAttachment, safeAttachment, extremeAttachment,
        shapeBypassAttachment, compressorBypassAttachment, widthBypassAttachment, dcFilterAttachment, driveBypassAttachment;

    fire::ui::ModuleDragButton oscSwitch, shapeSwitch, widthSwitch, compressorSwitch, ottSwitch;
    std::unique_ptr<ButtonAttachment> ottAttachment;
    enum RadioButtonIds
    {
        switchButtons = 1004
    };

    // Groups for visibility
    juce::Array<juce::Component*> shapeComponents;
    juce::Array<juce::Component*> widthComponents;
    juce::Array<juce::Component*> compressorComponents;
    juce::Array<juce::Component*> driveComponents; // New group for drive
    juce::Array<juce::Component*> ottComponents;
    juce::Array<juce::Component*> allControls;

    // Groups for enable/disable logic
    juce::Array<juce::Component*> shapeSubControls;
    juce::Array<juce::Component*> compressorSubControls;
    juce::Array<juce::Component*> widthSubControls;

    int focusBandNum;
    int pendingFocusBandNum = -1;
    bool pendingFocusForceUpdate = false;

    juce::Rectangle<int> knobsAreaRect;
    juce::Rectangle<int> outputAreaRect;
    juce::Rectangle<int> tabAreaRect;
    juce::Rectangle<int> graphAreaRect; // Area for the graphs
    juce::Component graphSelectorStrip;
    juce::Label graphViewLabel;
    ContextAwareComboBox graphViewMenu;
    int selectedGraphView = 1; // Auto, Waveform, Transfer, Meters, Stereo.
    std::uint64_t graphViewGeneration = 0;
    juce::Image chromeCache;
    float chromeCacheDisplayScale = 0.0f;
    bool chromeCacheDirty = true;

    fire::ui::SpringValue moduleSelectionPosition;

    // Graphs moved from GraphPanel
    Oscilloscope oscilloscope { processor };
    DistortionGraph distortionGraph { processor };
    OttGraph ottGraph;
    std::uint64_t lastOttMeterGeneration = 0;
    double lastOttMeterTimeMs = -1.0;
    int spectrumOttInteraction = 0;
    VUPanel vuPanel { processor };
    WidthGraph widthGraph { processor };
    GraphTemplate* zoomedGraph = nullptr;
    std::vector<juce::Component::SafePointer<juce::Component>>
        componentsHiddenForGraphZoom;

    // Distortion modes moved from PluginEditor
    std::array<ContextAwareComboBox, 4> distortionModes;
    std::array<std::unique_ptr<ComboBoxAttachment>, 4> modeAttachments;
    std::uint64_t distortionModeInteractionGeneration = 0;

    enum class DriveGraphPreviewPhase
    {
        idle,
        previewing,
        restoreAfterZoom
    };

    DriveGraphPreviewPhase driveGraphPreviewPhase =
        DriveGraphPreviewPhase::idle;
    GraphTemplate* graphBeforeDrivePreview = nullptr;
    fire::ui::EffectRackNavigation effectNavigation {processor, 1};
    InsertEffectControls insertControls {processor};
    int selectedInsert = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPanel)
};
