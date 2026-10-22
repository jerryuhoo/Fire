#pragma once
#include "PrimarySlider.h"
#include "LookAndFeel.h"
#include "ContextAwareComboBox.h"
#include "../PluginProcessor.h"

namespace fire::ui
{
// ParameterAttachment is used only for host-to-view updates. User writes are
// guarded before host callbacks so an old text edit cannot change a recycled
// source slot. The notification stack keeps only the parameter listener alive;
// its SafePointer callback does not retain the UI component.
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
            auto owner=safe;
            if (!owner || !owner->mayEdit() || owner->gesturing) return;
            auto* target = owner->parameter;
            const auto keepListenerAlive=owner->attachment;
            owner->gesturing = true; owner->startingGesture = true;
            target->beginChangeGesture();
            const bool finishNow=!owner || !owner->gesturing;
            if(owner) owner->startingGesture=false;
            if(finishNow) target->endChangeGesture();
        };
        onDragEnd = [safe] { if (safe) safe->finishGesture(); };
        onValueChange = [safe]
        {
            auto owner=safe;
            if (!owner || owner->updating || !owner->parameter) return;
            if (owner->startingGesture) return;
            auto* target = owner->parameter;
            // JUCE's parameter broadcaster iterates a mutable listener array.
            // Keep this listener installed while a host callback may delete
            // the control, until both sides of the notification complete.
            const auto keepListenerAlive=owner->attachment;
            if (!owner->mayEdit())
            {
                owner->setValue(target->convertFrom0to1(target->getValue()), juce::dontSendNotification);
                return;
            }
            const float normalised = target->convertTo0to1(static_cast<float>(owner->getValue()));
            if (juce::approximatelyEqual(normalised, target->getValue())) return;
            const bool gesture = owner->gesturing;
            auto beforeWrite = owner->onBeforeWrite;
            if (beforeWrite) beforeWrite();
            if (!owner || !owner->mayEdit()) return;
            if (!gesture)
            {
                target->beginChangeGesture();
                const juce::ScopeGuard finish{[target]{target->endChangeGesture();}};
                if (!owner || owner->parameter != target || !owner->mayEdit())
                {
                    if (owner && owner->parameter == target)
                        owner->setValue(target->convertFrom0to1(target->getValue()),juce::dontSendNotification);
                    return;
                }
                target->setValueNotifyingHost(normalised);
            }
            else target->setValueNotifyingHost(normalised);
        };
        attachment = std::make_shared<juce::ParameterAttachment>(target, [safe](float value)
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
        if (startingGesture) return; // The begin caller pairs it after broadcasting returns.
        const auto keepListenerAlive=attachment;
        if (parameter) parameter->endChangeGesture();
    }
    juce::RangedAudioParameter* parameter = nullptr;
    std::function<bool()> valid;
    std::shared_ptr<juce::ParameterAttachment> attachment;
    bool updating = false, gesturing = false, startingGesture = false;
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
