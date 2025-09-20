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
    void setDataToDisplay(LfoData* dataToDisplay);

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

    std::function<void()> onDataChanged;
    void setOnDataChangedCallback(std::function<void()> callback);

private:
    // This pointer holds the currently active LFO data. It does not own the data.
    LfoData* activeLfoData = nullptr;

    // Internal helper methods that now operate on the activeLfoData pointer.
    void addPoint(juce::Point<float> newPoint);
    void removePoint(int index);
    void updateAndSortPoints();
    void rebuildCurvatures();

    // Helper methods for brush mode
    int getOrCreatePointAtX(float targetX);
    void applyBrushShape(const juce::Point<int>& clickPosition);
    int findSegmentIndexAt(const juce::Point<int>& position) const;

    juce::Point<float> toNormalized(juce::Point<int> localPoint);
    juce::Point<float> fromNormalized(juce::Point<float> normalizedPoint);

    void deleteSelectedPoints();

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

    const int maxPoints = 16;
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
};

//
//  The LfoPanel is the main "Controller" component.
//  It owns all the LFO data and all the UI controls.
//
class LfoPanel : public juce::Component,
                 public juce::Button::Listener,
                 public juce::Slider::Listener,
                 public juce::Timer,
                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    LfoPanel(FireAudioProcessor& p);
    ~LfoPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void timerCallback() override;

    void setScale(float newScale);
    void setOnDataChangedCallback(std::function<void()> callback);

    std::function<void(int lfoIndex)> onAssignButtonClicked;
    juce::TextButton assignButton;

private:
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void setEditMode(LfoEditMode newMode);
    void styleButton(juce::Button& button, bool isToggle);

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

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateSliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncButtonAttachment;
    bool isUpdatingRateSlider = false;

    void updateRateSlider();

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPanel)
};
