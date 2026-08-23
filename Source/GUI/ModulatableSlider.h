/*
  ==============================================================================

    ModulatableSlider.h
    Created: 2 Sep 2025 1:42:12am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "InterfaceDefines.h"
#include "juce_gui_basics/juce_gui_basics.h"

struct ModulatableSliderTestAccess;

//==============================================================================
/**
    A custom slider that can be modulated by an LFO.
*/
class ModulatableSlider : public juce::Slider,
                          public juce::Timer
{
public:
    enum class ModulationMenuCommand
    {
        setValue = 1,
        clearModulation,
        invertDepth,
        togglePolarity,
        toggleBypass
    };

    ModulatableSlider();

    // void paint(juce::Graphics& g) override;
    bool hitTest(int x, int y) override;

    juce::Rectangle<float> getModulationHandleBounds() const;

    // LFO properties
    int lfoSource;
    double lfoAmount;
    double lfoValue; // Current LFO output (-1 to 1)

    // Mouse interaction states for the handle
    bool isModHandleMouseOver;
    bool isModHandleMouseDown;

    // Is the mouse over the main body of the slider, and not the handle?
    bool isMouseOverMainSlider() const;
    bool advanceAnimation(float deltaSeconds) noexcept;
    float getHoverAnimation() const noexcept { return hoverAnimation; }
    float getPressAnimation() const noexcept { return pressAnimation; }
    bool isModulated = false;
    bool isBipolar = true;
    bool isBypassed = false;
    juce::String parameterID;

    float assignModeGlowAlpha = 0.0f;

    // Callback to notify when the modulation amount changes via UI drag
    std::function<void(double)> onModAmountSetValue;
    std::function<void(ModulatableSlider*, const juce::String&)> onSetValueRequested;
    std::function<void(double)> onModAmountChanged;
    std::function<void(const juce::String&)> onBipolarModeToggled;
    std::function<void()> onModulationReset;
    std::function<void(const juce::String&)> onClickInAssignMode;
    std::function<void(int, const juce::String&)> onLfoAssignmentRequested;
    std::function<void(const juce::String&)> onModulationCleared;
    std::function<void(const juce::String&)> onModulationInverted;
    std::function<void(const juce::String&)> onBypassToggled;

    // Override mouse events to update handle states and control dragging
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    std::function<void(ModulatableSlider*)> onModDragStart;
    std::function<void(ModulatableSlider*)> onModDragMove;
    std::function<void(ModulatableSlider*)> onModDragEnd;

    std::function<void(ModulatableSlider*)> onMainDragStart;
    std::function<void(ModulatableSlider*)> onMainDragMove;
    std::function<void(ModulatableSlider*)> onMainDragEnd;
    std::function<void(ModulatableSlider*)> onHoverStart;
    std::function<void(ModulatableSlider*)> onHoverEnd;

    const juce::String& getParamID() const { return parameterID; }
    double getLfoValue() const { return lfoValue; }

    // Add a public method to set up the label.
    void setLabel(const juce::String& text, juce::Colour colour);

    // Override component methods for layout and mouse events.
    void resized() override;
    void timerCallback() override;

private:
    friend struct ModulatableSliderTestAccess;

    std::function<void(int)> createModulationMenuResultHandler();
    std::function<void(int)> createLfoAssignmentMenuResultHandler();
    void executeModulationMenuCommand(ModulationMenuCommand command,
                                      const juce::String& targetParameterID);

    float getUiScale() const noexcept;
    juce::Rectangle<float> getRotarySliderBounds() const;
    juce::Rectangle<int> getHeaderBounds() const;

    juce::Label label;
    bool isDraggingMainSlider;
    float hoverAnimation = 0.0f;
    float pressAnimation = 0.0f;

    // Store the initial LFO amount when a drag starts for smoother interaction
    double initialLfoAmount = 0.0;
};
