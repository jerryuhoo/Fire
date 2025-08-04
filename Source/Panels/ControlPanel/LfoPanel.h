/*
  ==============================================================================

    LfoPanel.h
    Created: 2 Aug 2025 9:24:01pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../PluginProcessor.h"
#include "../../GUI/LookAndFeel.h"
#include "ModulationMatrixPanel.h"

//
//  Represents the data model for a single LFO shape.
//  This ensures that every LFO always starts with a valid default state.
//
struct LfoData
{
    std::vector<juce::Point<float>> points;
    std::vector<float> curvatures;

    // Default constructor to initialize a valid shape
    LfoData()
    {
        points.push_back({ 0.0f, 0.5f });
        points.push_back({ 1.0f, 0.5f });
        curvatures.push_back(0.0f);
    }
};


//
//  The LfoEditor is now a pure "View" component.
//  It holds a pointer to the data it should display and modify.
//
class LfoEditor : public juce::Component
{
public:
    LfoEditor();
    ~LfoEditor() override;
    
    // Sets the data model for the editor to point to. This is the safe way to switch LFOs.
    void setDataToDisplay(LfoData* dataToDisplay);

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

    void setGridDivisions(int horizontal, int vertical);
    void setPlayheadPosition(float position);

private:
    // This pointer holds the currently active LFO data. It does not own the data.
    LfoData* activeLfoData = nullptr;

    // Internal helper methods that now operate on the activeLfoData pointer.
    void addPoint(juce::Point<float> newPoint);
    void removePoint(int index);
    void updateAndSortPoints();
    
    juce::Point<float> toNormalized(juce::Point<int> localPoint);
    juce::Point<float> fromNormalized(juce::Point<float> normalizedPoint);

    // State variables for interaction
    int draggingPointIndex = -1;
    int editingCurveIndex = -1;
    
    int hGridDivs = 4;
    int vGridDivs = 4;
    float playheadPos = -1.0f;

    const int maxPoints = 16;
    const float pointRadius = 6.0f;
};


//
//  The LfoPanel is the main "Controller" component.
//  It owns all the LFO data and all the UI controls.
//
class LfoPanel  : public juce::Component,
                  public juce::Button::Listener,
                  public juce::Slider::Listener,
                  public juce::Timer
{
public:
    LfoPanel(FireAudioProcessor& p);
    ~LfoPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    void timerCallback() override;
    
    void setScale(float newScale);

private:
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    
    FireAudioProcessor& processor;
    
    float scale = 1.0f;

    // --- Data Model ---
    // The LfoPanel owns the data for all 4 LFOs.
    std::vector<LfoData> lfoData;
    int currentLfoIndex = 0;

    // --- UI Components ---
    LfoEditor lfoEditor;

    std::array<std::unique_ptr<juce::TextButton>, 4> lfoSelectButtons;
    
    juce::TextButton matrixButton { "Matrix" };
    juce::TextButton syncButton;

    juce::Slider rateSlider;
    juce::Label  rateLabel;
    
    juce::Slider gridXSlider;
    juce::Label  gridXLabel;
    juce::Slider gridYSlider;
    juce::Label  gridYLabel;
    
    FlatLnf flatLnf;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LfoPanel)
};
