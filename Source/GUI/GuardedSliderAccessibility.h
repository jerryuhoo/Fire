/*
  ==============================================================================

    GuardedSliderAccessibility.h
    Created: 30 Aug 2026

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <memory>

namespace fire::ui
{
/** Preserves JUCE Slider's accessible value contract while rejecting commands
    delivered after the control has left its interactive component lifecycle. */
class GuardedSliderValueInterface final
    : public juce::AccessibilityValueInterface
{
public:
    explicit GuardedSliderValueInterface(juce::Slider& sliderToWrap)
        : slider(sliderToWrap), useMaxValue(sliderToWrap.isTwoValue())
    {
    }

    bool isReadOnly() const override { return false; }

    double getCurrentValue() const override
    {
        return useMaxValue ? slider.getMaximum() : slider.getValue();
    }

    void setValue(double newValue) override
    {
        if (! canInteract())
            return;

        juce::Slider::ScopedDragNotification drag(slider);
        if (useMaxValue)
            slider.setMaxValue(newValue, juce::sendNotificationSync);
        else
            slider.setValue(newValue, juce::sendNotificationSync);
    }

    juce::String getCurrentValueAsString() const override
    {
        return slider.getTextFromValue(getCurrentValue());
    }

    void setValueAsString(const juce::String& newValue) override
    {
        if (canInteract())
            setValue(slider.getValueFromText(newValue));
    }

    juce::AccessibilityValueInterface::AccessibleValueRange
    getRange() const override
    {
        const auto interval = slider.getInterval();
        const auto stepSize = ! juce::approximatelyEqual(interval, 0.0)
                                  ? interval
                                  : slider.getRange().getLength() * 0.01;
        return { { slider.getMinimum(), slider.getMaximum() },
                 stepSize };
    }

private:
    bool canInteract() const noexcept
    {
        return slider.isEnabled() && slider.isShowing();
    }

    juce::Slider& slider;
    const bool useMaxValue;
};

class GuardedSliderAccessibilityHandler final
    : public juce::AccessibilityHandler
{
public:
    explicit GuardedSliderAccessibilityHandler(juce::Slider& sliderToWrap)
        : juce::AccessibilityHandler(
              sliderToWrap,
              juce::AccessibilityRole::slider,
              {},
              { std::make_unique<GuardedSliderValueInterface>(sliderToWrap) }),
          slider(sliderToWrap)
    {
    }

    juce::String getHelp() const override { return slider.getTooltip(); }

private:
    juce::Slider& slider;
};
} // namespace fire::ui
