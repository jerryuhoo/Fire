#pragma once
#include "EffectRackNavigation.h"

class InsertEffectControls final : public juce::Component
{
public:
    explicit InsertEffectControls(FireAudioProcessor& p) : processor(p) {}
    ~InsertEffectControls() override { dismiss(); }
    void setControls(const std::array<ModulatableSlider*, fire::effects::controlCount>& controls)
    {
        sliders = controls;
        for (auto* slider : sliders) addChildComponent(slider);
    }
    void setActive(bool active)
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        if (! active) dismiss();
        if (! safe) return;
        for (auto* slider : sliders)
        {
            if (slider) slider->setVisible(active);
            if (! safe) return;
        }
        setVisible(active);
    }
    void bind(int targetScope, int targetSlot)
    {
        const auto targetType = processor.getInsertEffectType(targetScope, targetSlot);
        if (targetScope == scope && targetSlot == slot && targetType == type) return;
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        dismiss();
        if (! safe) return;
        scope = targetScope; slot = targetSlot; type = targetType;
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto* slider = sliders[i];
            if (! slider) continue;
            attachments[i].reset();
            const auto id = fire::effects::parameterID(scope, slot, static_cast<int>(i));
            slider->parameterID = id;
            attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, id, *slider);
            const auto definition = fire::effects::controls(type)[i];
            const auto colour = fire::ui::effectColour(type);
            slider->setLabel(definition.name, colour);
            slider->setColour(juce::Slider::rotarySliderFillColourId, colour);
            slider->setTextValueSuffix({});
            slider->setTitle(juce::String(fire::effects::name(type)) + " " + definition.name);
            slider->setTooltip(slider->getTitle() + " · insert slot " + juce::String(slot + 1));
            const bool sync = type == fire::effects::Type::delay && i == 4;
            slider->textFromValueFunction = [definition, sync](double value) {
                const auto physical = definition.fromNormalised(static_cast<float>(value));
                if (sync)
                {
                    const juce::StringArray divisions {"Free", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1 Bar"};
                    return divisions[juce::jlimit(0, 7, juce::roundToInt(physical))];
                }
                return juce::String(physical, physical >= 100.0f ? 0 : 1) + definition.unit;
            };
            slider->valueFromTextFunction = [definition, sync](const juce::String& text) {
                if (sync)
                {
                    const juce::StringArray divisions {"Free", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1 Bar"};
                    const auto index = divisions.indexOf(text.trim(), true);
                    if (index >= 0) return static_cast<double>(definition.toNormalised(static_cast<float>(index)));
                }
                return static_cast<double>(definition.toNormalised(text.getFloatValue()));
            };
            slider->setDoubleClickReturnValue(true, definition.toNormalised(definition.initial));
            slider->updateText();
            slider->invalidateAccessibilityHandler();
        }
    }
    void refresh() { if (scope >= 0 && slot >= 0) bind(scope, slot); }
    void dismiss()
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        for (auto* slider : sliders)
        {
            if (slider) slider->dismissTransientInteraction();
            if (! safe) return;
        }
    }
    void setScale(float value) { scale = value; resized(); }
    void resized() override
    {
        const auto gap = juce::roundToInt(8.0f * scale);
        const auto footer = juce::roundToInt(fire::ui::Metrics::knobValueHeight * scale);
        const auto size = juce::jmax(1, std::min({juce::roundToInt(fire::ui::Metrics::knobWidth * scale),
            (getWidth() - gap * 2) / 3, (getHeight() - gap) / 2 - footer}));
        auto area = getLocalBounds().withSizeKeepingCentre(size * 3 + gap * 2, (size + footer) * 2 + gap);
        for (size_t row = 0; row < 2; ++row)
        {
            auto strip = area.removeFromTop(size + footer); area.removeFromTop(gap);
            for (size_t column = 0; column < 3; ++column)
            {
                if (auto* slider = sliders[row * 3 + column]) slider->setBounds(strip.removeFromLeft(size));
                strip.removeFromLeft(gap);
            }
        }
    }
private:
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    FireAudioProcessor& processor;
    std::array<ModulatableSlider*, fire::effects::controlCount> sliders {};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, fire::effects::controlCount> attachments;
    int scope = -1, slot = -1;
    fire::effects::Type type = fire::effects::Type::none;
    float scale = 1;
};
