#pragma once
#include "PrimarySlider.h"
#include "LookAndFeel.h"
#include "ContextAwareComboBox.h"
#include "../PluginProcessor.h"

namespace fire::ui
{
// ParameterAttachment is used only for host-to-view updates. User writes are
// guarded before host callbacks so an old text edit cannot change a recycled
// source slot, and the notifying stack never owns a UI attachment.
class PerformanceIntegerControl final : public PrimarySlider
{
public:
    PerformanceIntegerControl() { setSliderStyle(juce::Slider::IncDecButtons); setTextBoxStyle(juce::Slider::TextBoxLeft, false, 88, 28); }
    ~PerformanceIntegerControl() override { dismissTransientInteraction(); finishGesture(); }
    void bind(juce::RangedAudioParameter& target, std::function<bool()> validator)
    {
        dismissTransientInteraction(); finishGesture(); attachment.reset();
        parameter = &target; valid = std::move(validator);
        const auto range = target.getNormalisableRange();
        setRange(range.start, range.end, range.interval);
        const juce::Component::SafePointer<PerformanceIntegerControl> safe(this);
        onDragStart = [safe]
        {
            if (!safe || !safe->mayEdit() || safe->gesturing) return;
            auto* target = safe->parameter;
            safe->gesturing = true;
            target->beginChangeGesture();
        };
        onDragEnd = [safe] { if (safe) safe->finishGesture(); };
        onValueChange = [safe]
        {
            if (!safe || safe->updating || !safe->parameter) return;
            auto* target = safe->parameter;
            if (!safe->mayEdit())
            {
                safe->setValue(target->convertFrom0to1(target->getValue()), juce::dontSendNotification);
                return;
            }
            const float normalised = target->convertTo0to1(static_cast<float>(safe->getValue()));
            if (juce::approximatelyEqual(normalised, target->getValue())) return;
            const bool gesture = safe->gesturing;
            auto beforeWrite = safe->onBeforeWrite;
            if (beforeWrite) beforeWrite();
            if (!safe || !safe->mayEdit()) return;
            if (!gesture) target->beginChangeGesture();
            target->setValueNotifyingHost(normalised);
            if (!gesture) target->endChangeGesture();
        };
        attachment = std::make_unique<juce::ParameterAttachment>(target, [safe](float value)
        {
            if (!safe) return;
            safe->updating = true;
            safe->setValue(value, juce::dontSendNotification);
            if (safe) safe->updating = false;
        }, nullptr);
        attachment->sendInitialUpdate();
    }
    std::function<void()> onBeforeWrite;
private:
    bool mayEdit() const { return parameter && isEnabled() && isShowing() && valid && valid(); }
    void finishGesture()
    {
        if (!gesturing) return;
        gesturing = false;
        if (parameter) parameter->endChangeGesture();
    }
    juce::RangedAudioParameter* parameter = nullptr;
    std::function<bool()> valid;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool updating = false, gesturing = false;
};
inline void performanceComboColours(juce::ComboBox& box, const SkinPalette& palette)
{
    box.setColour(juce::ComboBox::backgroundColourId, palette.surface0);
    box.setColour(juce::ComboBox::textColourId, palette.textPrimary);
    box.setColour(juce::ComboBox::outlineColourId, palette.hairline);
    box.setColour(juce::ComboBox::arrowColourId, palette.textSecondary);
    box.setColour(juce::ComboBox::focusedOutlineColourId, palette.accent);
}
inline bool performanceUndo(FireAudioProcessor& processor, const juce::KeyPress& key)
{
    if (!key.getModifiers().isCommandDown() && !key.getModifiers().isCtrlDown()) return false;
    const auto letter = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (letter == 'z') return key.getModifiers().isShiftDown() ? processor.redoEdit() : processor.undoEdit();
    if (letter == 'y') return processor.redoEdit();
    return false;
}
}
