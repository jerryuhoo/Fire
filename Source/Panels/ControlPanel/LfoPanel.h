/*
  ==============================================================================

    LfoPanel.h
    Created: 2 Aug 2025 9:24:01pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../../DSP/LfoData.h"
#include "../../Utility/Parameters.h" // Include for LfoEditMode and LfoPresetShape
#include "ModulationMatrixPanel.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"

class FireAudioProcessor;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct LfoEditorTestAccess;
#endif

inline LfoData lfoClipboard;

//
//  The LfoEditor is now a pure "View" component.
//  It holds a pointer to the data it should display and modify.
//  It is told which editing mode to be in by the LfoPanel.
//
class LfoEditor : public juce::Component
{
public:
    LfoEditor();
    ~LfoEditor() override;

    // Sets the data model for the editor to point to. This is the safe way to switch LFOs.
    void setDataToDisplay(const LfoData& dataToDisplay);

    // Called by LfoPanel to set the current interaction mode.
    void setCurrentBrush(LfoPresetShape newBrush);
    void setEditMode(LfoEditMode newMode);

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setGridDivisions(int horizontal, int vertical);
    void setPlayheadPosition(float position);
    void setPhaseOffsetLinePosition(float position);
    void setSmoothness(float smoothness);

    std::function<void(const LfoData&)> onDataChanged;

    enum CommandIDs
    {
        selectAll = 1,
        clear,
        copy,
        paste,
        invertX,
        invertY
    };

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct LfoEditorTestAccess;
#endif

    // This pointer holds the currently active LFO data. It does not own the data.
    LfoData activeLfoData;
    bool dataIsActive = false;

    // Internal helper methods that now operate on the activeLfoData pointer.
    void addPoint(juce::Point<float> newPoint);
    void removePoint(int index);
    void updateAndSortPoints();
    void rebuildCurvatures();
    bool isValidPointIndex(int index) const noexcept;
    bool isValidCurveIndex(int index) const noexcept;
    bool hasValidSelectedPointIndices() const noexcept;
    bool hasValidPointDragState() const noexcept;
    bool validateCurveInteractionOrCancel() noexcept;
    bool validatePointDragInteractionOrCancel() noexcept;
    void cancelPointAndCurveInteraction() noexcept;
    void cancelAllInteraction() noexcept;

    // Helper methods for brush mode
    int getOrCreatePointAtX(float targetX);
    void applyBrushShape(const juce::Point<int>& clickPosition);
    int findSegmentIndexAt(const juce::Point<int>& position) const;

    juce::Point<float> toNormalized(juce::Point<int> localPoint);
    juce::Point<float> fromNormalized(juce::Point<float> normalizedPoint);

    void deleteSelectedPoints();

    void rebuildGridCache(float physicalScale);
    void rebuildWavePath();
    uint64_t getWavePathSignature() const noexcept;
    juce::Image gridCache;
    juce::Path cachedWavePath;
    uint64_t cachedWavePathSignature = 0;
    float cachedGridScale = 0.0f;
    int cachedGridWidth = 0;
    int cachedGridHeight = 0;
    int cachedHorizontalDivisions = 0;
    int cachedVerticalDivisions = 0;

    // --- State variables for interaction ---
    LfoEditMode currentMode = LfoEditMode::PointEdit;
    LfoPresetShape currentBrush = LfoPresetShape::SawUp;

    // State for PointEdit mode
    int draggingPointIndex = -1;
    int editingCurveIndex = -1; // This is the new name for draggedCurvatureIndex
    int hoveredPointIndex = -1;
    float initialCurvature = 0.0f;
    int initialDragY = 0;

    int hGridDivs = 4;
    int vGridDivs = 4;
    float playheadPos = -1.0f;
    float phaseOffsetPosition = -1.0f;

    const int maxPoints = 64;
    const float pointRadius = 6.0f;

    std::vector<int> selectedPointIndices;
    juce::Rectangle<int> selectionRectangle;

    enum class DraggingState
    {
        None,
        Point,
        Selection,
        Marquee
    };
    DraggingState draggingState = DraggingState::None;

    // Used for dragging single or multiple points
    juce::Point<float> dragAnchor; // The normalized position of the main point being dragged
    std::vector<juce::Point<float>> initialDragPositions; // Store original positions of all selected points

    bool isBrushing = false;
    juce::Point<int> lastBrushCell { -1, -1 };

    void selectAllPoints();
    void clearAllPoints();
    void copyShape();
    void pasteShape();
    void invertShape(bool invertX, bool invertY);
};

//
//  The LfoPanel is the main "Controller" component.
//  It owns all the LFO data and all the UI controls.
//
class LfoPanel : public juce::Component,
                 public juce::Button::Listener,
                 public juce::Slider::Listener,
                 public juce::AudioProcessorValueTreeState::Listener,
                 public juce::AsyncUpdater
{
public:
    LfoPanel(FireAudioProcessor& p);
    ~LfoPanel() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    /** Called by the editor's shared UI clock. */
    void animationTick(float deltaSeconds = 1.0f / 60.0f);

    void setScale(float newScale);
    void setOnDataChangedCallback(std::function<void()> callback);

    std::function<void()> onDataChanged;
    std::function<void(int lfoIndex)> onAssignButtonClicked;
    juce::TextButton assignButton;

    void refreshLfoDisplay();
    void handleAsyncUpdate() override;

private:
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted(juce::Slider* slider) override;
    void sliderDragEnded(juce::Slider* slider) override;
    void setEditMode(LfoEditMode newMode);
    void styleButton(juce::Button& button, bool isToggle);
    void styleLfoSelectButton(juce::TextButton& button, juce::Colour colour);
    void setLfo(int newIndex);
    LfoData getLfoDataCopy(int index);

    FireAudioProcessor& processor;

    float scale = 1.0f;

    // --- Data Model ---
    // The LfoPanel owns the data for all 4 LFOs.
    int currentLfoIndex = 0;

    // --- UI Components ---
    LfoEditor lfoEditor;

    std::array<std::unique_ptr<juce::TextButton>, 4> lfoSelectButtons;

    // --- UI Components for mode selection ---
    juce::TextButton editModeButton { "Edit Mode" };
    juce::TextButton brushModeButton { "Brush Mode" };
    juce::ComboBox brushSelector;

    juce::TextButton matrixButton { "Matrix" };
    juce::TextButton syncButton;

    juce::Slider rateSlider;
    juce::Label rateLabel;

    juce::Slider gridXSlider;
    juce::Label gridXLabel;
    juce::Slider gridYSlider;
    juce::Label gridYLabel;

    juce::Slider lfoSmoothSlider;
    juce::Label lfoSmoothLabel;

    juce::Slider lfoPhaseSlider;
    juce::Label lfoPhaseLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateSliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoPhaseAttachment;
    bool isUpdatingRateSlider = false;
    bool isDraggingPhaseSlider = false;
    std::atomic<bool> pendingRateSliderUpdate { false };
    std::atomic<unsigned int> pendingSmoothnessUpdates { 0 };
    std::array<juce::String, 4> syncParameterIDs;
    std::array<juce::String, 4> smoothParameterIDs;

    juce::Component::SafePointer<juce::DialogWindow> modulationMatrixDialog;

    void updateRateSlider();

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::Rectangle<int> leftColumnArea;
    juce::Rectangle<int> centerColumnArea;
    juce::Rectangle<int> rightColumnArea;
    const std::array<juce::Colour, 4> lfoColours {
        fire::ui::colours::modulation,
        fire::ui::colours::signalCool,
        fire::ui::colours::positive,
        fire::ui::colours::gold
    };
    fire::ui::DampedValue lfoSelectionPosition;
    juce::Rectangle<int> separatorLine;
    juce::Rectangle<int> topRowArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPanel)
};
