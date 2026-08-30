/*
  ==============================================================================

    GuardedSliderAccessibility.h
    Created: 30 Aug 2026

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <memory>
#include <new>

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

        // Drag-start, value, and drag-end notifications are synchronous. Any
        // one of them may close the editor and delete both the Slider and this
        // value interface. Keep every input needed below in locals before the
        // first callback, and never access this again after it.
        auto* const target = &slider;
        const auto writesMaximum = useMaxValue;
        setValueGuarded(*target, writesMaximum, newValue);
    }

    juce::String getCurrentValueAsString() const override
    {
        return slider.getTextFromValue(getCurrentValue());
    }

    void setValueAsString(const juce::String& newValue) override
    {
        if (! canInteract())
            return;

        auto* const target = &slider;
        const auto writesMaximum = useMaxValue;
        const juce::Component::SafePointer<juce::Slider> safeTarget(target);
        const auto suffix = target->getTextValueSuffix();
        auto valueFromText = target->valueFromTextFunction;
        auto text = newValue.trimStart();
        if (text.endsWith(suffix))
            text = text.substring(0, text.length() - suffix.length());

        // Copy the user conversion before invoking it. It is stored inside the
        // Slider, so deleting the Slider from the conversion must not destroy
        // the std::function target while that target is still executing.
        const auto parsedValue = valueFromText != nullptr
                                     ? valueFromText(text)
                                     : parseDefaultSliderText(text);
        if (safeTarget == nullptr)
            return;

        // Do not call back through this interface: a custom text conversion
        // may have synchronously removed it along with the Slider.
        setValueGuarded(*target, writesMaximum, parsedValue);
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
    static double parseDefaultSliderText(juce::String text)
    {
        while (text.startsWithChar('+'))
            text = text.substring(1).trimStart();

        return text.initialSectionContainingOnly("0123456789.,-")
            .getDoubleValue();
    }

    static void setValueGuarded(juce::Slider& target,
                                bool writesMaximum,
                                double newValue)
    {
        const juce::Component::SafePointer<juce::Slider> safeTarget(&target);
        using Drag = juce::Slider::ScopedDragNotification;
        alignas(Drag) unsigned char dragStorage[sizeof(Drag)];

        // Placement storage lets us omit the destructor when sendDragStart()
        // synchronously deletes the Slider. ScopedDragNotification's normal
        // automatic destructor would otherwise dereference the dead Slider.
        auto* const drag = new (&dragStorage) Drag(target);
        if (safeTarget == nullptr)
            return;

        if (writesMaximum)
            target.setMaxValue(newValue, juce::sendNotificationSync);
        else
            target.setValue(newValue, juce::sendNotificationSync);

        if (safeTarget == nullptr)
            return;

        // This remains the final operation: sendDragEnd() may also delete the
        // Slider and its accessibility handler.
        drag->~Drag();
    }

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
