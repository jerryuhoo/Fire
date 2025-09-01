/*
  ==============================================================================

    ModulatableSlider.h
    Created: 2 Sep 2025 1:42:12am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

//==============================================================================
/**
    A custom slider that can be modulated by an LFO.
*/
class ModulatableSlider : public juce::Slider
{
public:
    ModulatableSlider();

    // Overridden to restrict hit testing to the slider's rotary path
    bool hitTest(int x, int y) override;

    juce::Rectangle<float> getModulationHandleBounds();

    // LFO properties
    int lfoSource;
    double lfoAmount;
    double lfoValue; // Current LFO output (-1 to 1)

    // Mouse interaction states for the handle
    bool isModHandleMouseOver;
    bool isModHandleMouseDown;

    // Is the mouse over the main body of the slider, and not the handle?
    bool isMouseOverMainSlider() const;

    // Override mouse events to update handle states and control dragging
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override; // Override mouseDrag
    void mouseUp(const juce::MouseEvent& event) override;

private:
    bool isDraggingMainSlider; // New: Track if the drag is for the main slider
};