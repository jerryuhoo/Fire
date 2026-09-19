#pragma once
#include "EffectRackNavigation.h"
#include "../Utility/CloudsParameters.h"

class InsertEffectControls final : public juce::Component
{
public:
    static constexpr size_t cloudsExtraCount = 3;
    explicit InsertEffectControls(FireAudioProcessor& p) : processor(p)
    {
        addChildComponent(freezeButton);
        freezeButton.setButtonText("Freeze");
        freezeButton.setClickingTogglesState(true);
        freezeButton.setTitle("Freeze granular recording");
        freezeButton.setTooltip("Hold the recorded audio while grains keep playing. Turn off to record new audio. "
                                "The recording is not saved with the project; after reopening or resetting, "
                                "Freeze first captures about one second of new audio.");
        freezeButton.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface1);
        freezeButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::granular.withAlpha(0.25f));
        freezeButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
        freezeButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::whiteHot);
    }
    ~InsertEffectControls() override { onLayoutChanged = nullptr; dismiss(); }
    void setControls(const std::array<ModulatableSlider*, fire::effects::controlCount>& controls)
    {
        sliders = controls;
        for (auto* slider : sliders) if (slider) addChildComponent(slider);
    }
    void setCloudsControls(const std::array<ModulatableSlider*, cloudsExtraCount>& controls)
    {
        cloudsSliders = controls;
        for (auto* slider : cloudsSliders) if (slider) addChildComponent(slider);
    }
    bool usesExpandedLayout() const noexcept
    { return type == fire::effects::Type::granular; }
    void setActive(bool shouldBeActive)
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        const auto request = ++visibilityGeneration;
        active = shouldBeActive;
        if (! active) dismiss();
        if (! safe || request != visibilityGeneration) return;
        updateVisibility();
        if (safe && request == visibilityGeneration) setVisible(active);
    }
    void bind(int targetScope, int targetSlot)
    {
        const auto targetType = processor.getInsertEffectType(targetScope, targetSlot);
        if (targetScope == scope && targetSlot == slot && targetType == type) return;
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        const auto request = ++bindingGeneration;
        dismiss();
        // Closing a host gesture may re-enter bind for another band/slot.
        // Let that newer request win even when this component is still alive.
        if (! safe || request != bindingGeneration) return;
        for (auto& attachment : attachments) attachment.reset();
        for (auto& attachment : cloudsAttachments) attachment.reset();
        freezeAttachment.reset();
        scope = targetScope; slot = targetSlot; type = targetType;
        const auto colour = fire::ui::effectColour(type);
        const auto& definitions = fire::effects::controls(type);
        const auto isCurrent = [&] { return safe != nullptr && request == safe->bindingGeneration; };
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto* slider = sliders[i];
            if (! slider) continue;
            const auto id = fire::effects::parameterID(targetScope, targetSlot, static_cast<int>(i));
            slider->parameterID = id;
            auto attachment = std::make_unique<SliderAttachment>(processor.treeState, id, *slider);
            if (! isCurrent()) return;
            attachments[i] = std::move(attachment);
            configureSlider(*slider, definitions[i], colour,
                            type == fire::effects::Type::delay && i == 4,
                            usesExpandedLayout() && i == 1);
            if (! isCurrent()) return;
            if (usesExpandedLayout() && i == 3)
                slider->setTooltip("Position in the recorded buffer: turn clockwise to select older audio.");
            if (usesExpandedLayout() && i == 4)
                slider->setTooltip("Grain envelope: square, triangle, then Hann. High Texture adds diffusion.");
            if (! isCurrent()) return;
        }
        if (type == fire::effects::Type::granular)
        {
            const auto freezeID = fire::clouds_params::parameterID(targetScope, targetSlot, fire::clouds_params::freezeField);
            freezeButton.setComponentID(freezeID);
            auto freeze = std::make_unique<ButtonAttachment>(processor.treeState, freezeID, freezeButton);
            if (! isCurrent()) return;
            freezeAttachment = std::move(freeze);
            const std::array<const char*, cloudsExtraCount> names { "Spread", "Feedback", "Reverb" };
            for (size_t i = 0; i < cloudsSliders.size(); ++i)
            {
                auto* slider = cloudsSliders[i];
                if (! slider) continue;
                const auto id = fire::clouds_params::parameterID(targetScope, targetSlot,
                    fire::clouds_params::spreadField + static_cast<int>(i));
                slider->parameterID = id;
                slider->setComponentID(id);
                auto extra = std::make_unique<SliderAttachment>(processor.treeState, id, *slider);
                if (! isCurrent()) return;
                cloudsAttachments[i] = std::move(extra);
                configureSlider(*slider, { names[i], " %", 0.0f, 100.0f, i == 0 ? 50.0f : 0.0f }, colour, false, false);
                if (! isCurrent()) return;
            }
        }
        else
        {
            for (auto* slider : cloudsSliders) if (slider) slider->parameterID.clear();
        }
        updateVisibility();
        if (! isCurrent()) return;
        resized();
        if (! isCurrent()) return;
        auto callback = onLayoutChanged;
        if (callback) callback();
    }
    void refresh() { if (scope >= 0 && slot >= 0) bind(scope, slot); }
    void dismissButtons()
    {
        freezeButton.dismissPointerGesture();
    }
    void dismiss()
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        dismissButtons();
        if (! safe) return;
        for (auto* slider : sliders)
        {
            if (slider) slider->dismissTransientInteraction();
            if (! safe) return;
        }
        for (auto* slider : cloudsSliders)
        {
            if (slider) slider->dismissTransientInteraction();
            if (! safe) return;
        }
    }
    void setScale(float value)
    {
        scale = std::isfinite(value) && value > 0.0f ? value : 1.0f;
        resized();
    }
    void resized() override
    {
        const auto gap = juce::roundToInt(8.0f * scale);
        const auto footer = juce::roundToInt(fire::ui::Metrics::knobValueHeight * scale);
        auto bounds = getLocalBounds();
        const int columns = usesExpandedLayout() ? 6 : 3;
        const auto size = juce::jmax(1, std::min({juce::roundToInt(fire::ui::Metrics::knobWidth * scale),
            (bounds.getWidth() - gap * (columns - 1)) / columns,
            (bounds.getHeight() - gap) / 2 - footer}));
        auto area = bounds.withSizeKeepingCentre(size * columns + gap * (columns - 1), (size + footer) * 2 + gap);
        if (usesExpandedLayout())
        {
            const std::array<size_t, 6> order { 3, 0, 2, 1, 4, 5 };
            auto top = area.removeFromTop(size + footer);
            area.removeFromTop(gap);
            for (auto index : order)
            {
                if (auto* slider = sliders[index]) slider->setBounds(top.removeFromLeft(size));
                top.removeFromLeft(gap);
            }
            for (auto* slider : cloudsSliders)
            {
                if (slider) slider->setBounds(area.removeFromLeft(size));
                area.removeFromLeft(gap);
            }
            const auto buttonHeight = juce::jmin(size, juce::roundToInt(28.0f * scale));
            freezeButton.setBounds(area.withSizeKeepingCentre(
                juce::jmin(area.getWidth(), size * 2 + gap), buttonHeight));
        }
        else
        {
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
    }
    std::function<void()> onLayoutChanged;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void configureSlider(ModulatableSlider& slider, fire::effects::Control definition,
                         juce::Colour colour, bool delaySync, bool cloudsDensity)
    {
        const juce::Component::SafePointer<ModulatableSlider> safeSlider(&slider);
        slider.setLabel(definition.name, colour);
        if (! safeSlider) return;
        slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
        slider.setTextValueSuffix({});
        slider.setTitle(juce::String(fire::effects::name(type)) + " " + definition.name);
        slider.setTooltip(cloudsDensity
            ? "Density: left is Regular and right is Random. The centre (-6% to +6%) is Off: no new grains."
            : slider.getTitle() + " - insert slot " + juce::String(slot + 1));
        slider.textFromValueFunction = [definition, delaySync, cloudsDensity](double value)
        {
            const auto physical = definition.fromNormalised(static_cast<float>(value));
            if (delaySync)
            {
                const juce::StringArray divisions { "Free", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1 Bar" };
                return divisions[juce::jlimit(0, 7, juce::roundToInt(physical))];
            }
            if (cloudsDensity && std::abs(physical) <= 6.0f) return juce::String("Off");
            return juce::String(physical, physical >= 100.0f ? 0 : 1) + definition.unit;
        };
        slider.valueFromTextFunction = [definition, delaySync, cloudsDensity](const juce::String& text)
        {
            if (delaySync)
            {
                const juce::StringArray divisions { "Free", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1 Bar" };
                const auto index = divisions.indexOf(text.trim(), true);
                if (index >= 0) return static_cast<double>(definition.toNormalised(static_cast<float>(index)));
            }
            if (cloudsDensity && text.trim().equalsIgnoreCase("Off")) return 0.5;
            return static_cast<double>(definition.toNormalised(text.getFloatValue()));
        };
        slider.setDoubleClickReturnValue(true, definition.toNormalised(definition.initial));
        slider.updateText();
        if (safeSlider) slider.invalidateAccessibilityHandler();
    }
    void updateVisibility()
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        const auto request = visibilityGeneration;
        const auto binding = bindingGeneration;
        const auto current = [&] { return safe && request == safe->visibilityGeneration && binding == safe->bindingGeneration; };
        for (auto* slider : sliders)
        {
            if (slider) slider->setVisible(active);
            if (! current()) return;
        }
        for (auto* slider : cloudsSliders)
        {
            if (slider) slider->setVisible(active && usesExpandedLayout());
            if (! current()) return;
        }
        freezeButton.setVisible(active && usesExpandedLayout());
    }
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    FireAudioProcessor& processor;
    std::array<ModulatableSlider*, fire::effects::controlCount> sliders {};
    std::array<ModulatableSlider*, cloudsExtraCount> cloudsSliders {};
    PrimaryTextButton freezeButton;
    // Destroy attachments while their local and parent-owned controls still exist.
    std::array<std::unique_ptr<SliderAttachment>, fire::effects::controlCount> attachments;
    std::array<std::unique_ptr<SliderAttachment>, cloudsExtraCount> cloudsAttachments;
    std::unique_ptr<ButtonAttachment> freezeAttachment;
    std::uint64_t bindingGeneration = 0, visibilityGeneration = 0;
    int scope = -1, slot = -1;
    fire::effects::Type type = fire::effects::Type::none;
    bool active = false;
    float scale = 1.0f;
};
