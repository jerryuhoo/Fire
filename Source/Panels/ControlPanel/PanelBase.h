/*
  ==============================================================================

    PanelBase.h
    Created: 12 Oct 2025 9:51:17am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../../GUI/ModulatableSlider.h"
#include "../../PluginProcessor.h"
#include <map>
#include <vector>

//==============================================================================
/*
    This base class holds all the common functionality for panels that
    contain groups of ModulatableSliders, like BandPanel and GlobalPanel.
*/
class PanelBase : public juce::Component
{
public:
    std::vector<ModulatableSlider*>& getModulatableSliders() { return modulatableSliders; }

protected: // Use protected so derived classes can access these members
    // Make constructor protected so only derived classes can call it.
    explicit PanelBase(FireAudioProcessor& p);
    ~PanelBase() override = default;

    // This helper function, promoted from BandPanel, will be shared by all panels.
    void createAndConfigureSlider(const juce::String& paramName,
                                  const juce::String& labelText,
                                  juce::Colour sliderColour,
                                  const juce::String& suffix = "");

    // This helper is also common.
    void initRotarySlider(juce::Slider& slider, juce::Colour colour);

    // This helper is useful for setting up modulation callbacks on sliders.
    // Promoted from BandPanel::setupSliderCallbacks
    void setupModulationCallbacks(ModulatableSlider& slider);

    FireAudioProcessor& processor;

    // Common data structures for managing sliders.
    // These are now owned by the base class.
    std::map<juce::String, std::unique_ptr<ModulatableSlider>> modulatableSliderComponents;
    std::vector<ModulatableSlider*> modulatableSliders;
    std::map<juce::String, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;

private:
    // Callbacks can be passed in from the editor and handled here if needed,
    // for this example, we keep the simple callback assignment in derived classes.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelBase)
};