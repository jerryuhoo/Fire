/*
  ==============================================================================

    PanelBase.cpp
    Created: 12 Oct 2025 9:51:17am
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "PanelBase.h"

PanelBase::PanelBase(FireAudioProcessor& p) : processor(p)
{
    // The processor reference is initialized here.
}

void PanelBase::createAndConfigureSlider(const juce::String& paramName,
                                         const juce::String& labelText,
                                         juce::Colour sliderColour,
                                         const juce::String& suffix)
{
    // Create the slider instance and store it in the map.
    modulatableSliderComponents[paramName] = std::make_unique<ModulatableSlider>();
    auto* slider = modulatableSliderComponents.at(paramName).get();

    // Configure its appearance.
    initRotarySlider(*slider, sliderColour);

    // Set the label text which the ModulatableSlider will draw itself.
    slider->setLabel(labelText, sliderColour.withBrightness(0.9f));

    // Set the value suffix if one is provided.
    if (suffix.isNotEmpty())
        slider->setTextValueSuffix(suffix);

    // Setup modulation-related callbacks for the slider.
    setupModulationCallbacks(*slider);

    // Add the raw pointer to our non-owning vector for easy access by the editor.
    modulatableSliders.push_back(slider);
}

void PanelBase::initRotarySlider(juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    // Let the ModulatableSlider handle its own label drawing, so no textbox needed here.
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
}

void PanelBase::setupModulationCallbacks(ModulatableSlider& slider)
{
    auto safeSlider = juce::Component::SafePointer<ModulatableSlider>(&slider);

    // This logic is generic enough to be in the base class.
    // It uses the slider's `parameterID` which is set later during `updateAttachments`.
    safeSlider->onModAmountSetValue = [this, safeSlider](double newValue)
    {
        if (safeSlider)
            processor.setModulationValue(safeSlider->parameterID, (float) newValue);
    };

    safeSlider->onModAmountChanged = [this, safeSlider](double newAmount)
    {
        if (safeSlider)
            processor.setModulationDepth(safeSlider->parameterID, (float) newAmount);
    };

    safeSlider->onBipolarModeToggled = [this, safeSlider]()
    {
        if (safeSlider)
            processor.toggleBipolarMode(safeSlider->parameterID);
    };

    safeSlider->onModulationReset = [this, safeSlider]()
    {
        if (safeSlider)
            processor.resetModulation(safeSlider->parameterID);
    };

    safeSlider->onModulationCleared = [this, safeSlider]()
    {
        if (safeSlider)
            processor.clearModulationForParameter(safeSlider->parameterID);
    };

    safeSlider->onModulationInverted = [this, safeSlider]()
    {
        if (safeSlider)
            processor.invertModulationDepthForParameter(safeSlider->parameterID);
    };
}