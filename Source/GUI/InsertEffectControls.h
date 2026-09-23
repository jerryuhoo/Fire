#pragma once
#include "EffectRackNavigation.h"
#include "ContextAwareComboBox.h"
#include "../DSP/ChordResonator.h"
#include <atomic>
#include "../Utility/CloudsParameters.h"
#include "../Utility/ModulationEffectParameters.h"
#include "../Utility/ResonatorParameters.h"

class InsertEffectControls final : public juce::Component,
                                   private juce::AudioProcessorParameter::Listener
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
        for (auto* menu : {&rootMenu, &chordMenu})
        {
            addChildComponent(menu);
            menu->setColour(juce::ComboBox::backgroundColourId, fire::ui::colours::surface0);
            menu->setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
            menu->setColour(juce::ComboBox::textColourId, fire::ui::colours::textPrimary);
            menu->setColour(juce::ComboBox::arrowColourId, fire::ui::colours::chordResonator);
        }
        for (int midi = fire::chord_resonator::minimumRoot; midi <= fire::chord_resonator::maximumRoot; ++midi)
            rootMenu.addItem(fire::chord_resonator::rootName(midi), midi - fire::chord_resonator::minimumRoot + 1);
        for (size_t chord = 0; chord < fire::chord_resonator::chordNames.size(); ++chord)
            chordMenu.addItem(fire::chord_resonator::chordNames[chord], static_cast<int>(chord + 1));
        rootMenu.setTitle("Chord Resonator root note");
        rootMenu.setTooltip("Root note of the resonating chord. C3 is MIDI note 48.");
        chordMenu.setTitle("Chord Resonator chord");
        chordMenu.setTooltip("Choose the intervals that resonate above the root note.");
        rootMenu.setHelpText(rootMenu.getTooltip());
        chordMenu.setHelpText(chordMenu.getTooltip());
        rootMenu.setExplicitFocusOrder(1);
        chordMenu.setExplicitFocusOrder(2);
        rootLabel.setText("Root", juce::dontSendNotification);
        chordLabel.setText("Chord", juce::dontSendNotification);
        chordNotes.setTitle("Selected chord notes");
        chordNotes.setTooltip("Notes in the selected chord before LFO modulation.");
        chordNotes.setHelpText(chordNotes.getTooltip());
        for (auto* label : {&rootLabel, &chordLabel, &chordNotes})
        {
            addChildComponent(label);
            label->setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
            label->setInterceptsMouseClicks(false, false);
            label->setJustificationType(juce::Justification::centredLeft);
        }
    }
    ~InsertEffectControls() override
    {
        onLayoutChanged = nullptr;
        unobserveType();
        dismiss();
    }
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
    bool usesFullWidthLayout() const noexcept
    { return usesExpandedLayout() || type == fire::effects::Type::lofi; }
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
        unobserveType();
        for (auto& attachment : chordAttachments) attachment.reset();
        for (auto& attachment : attachments) attachment.reset();
        for (auto& attachment : cloudsAttachments) attachment.reset();
        freezeAttachment.reset();
        scope = targetScope; slot = targetSlot; type = targetType;
        const auto colour = fire::ui::effectColour(type);
        const auto& definitions = fire::effects::controls(type);
        const bool modulationEffect = type == fire::effects::Type::flanger || type == fire::effects::Type::phaser;
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
                            usesExpandedLayout() && i == 1,
                            (modulationEffect && i == 0) || (usesChordSelectors() && i == 3));
            if (! isCurrent()) return;
            if (modulationEffect)
            {
                const std::array<const char*, 6> help {
                    "Speed of the sweep in cycles per second.",
                    type == fire::effects::Type::flanger ? "Amount of delay-time movement."
                                                        : "Range of the filter sweep.",
                    type == fire::effects::Type::flanger ? "Base delay time around which the sweep moves."
                                                        : "Centre frequency around which the phaser notches sweep.",
                    "Recirculates the effected signal for stronger resonances. Negative values reverse the feedback polarity.",
                    "Spread of the movement between the left and right channels.",
                    "Blend of the original signal and the effect."
                };
                slider->setTooltip(juce::String(fire::effects::name(type)) + " " + definitions[i].name
                                   + ": " + help[i]);
            }
            if (usesChordSelectors() && i >= 2)
            {
                const std::array<const char*, 4> help {
                    "Balance the fundamental body and brighter overtones of the chord.",
                    "How long the resonances ring after the incoming sound excites them.",
                    "Stereo spread of the chord's resonances.",
                    "Blend of the incoming sound and the ringing chord."
                };
                slider->setTooltip(juce::String("Chord Resonator ") + definitions[i].name + ": " + help[i - 2]);
            }
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
        if (usesChordSelectors())
        {
            bindChordSelectors(request);
            if (! isCurrent()) return;
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
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        choiceGeneration.fetch_add(1, std::memory_order_release);
        rootMenu.dismissTransientInteraction();
        if (! safe) return;
        chordMenu.dismissTransientInteraction();
        if (! safe) return;
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
    void setKnobWidth(int width)
    {
        if (knobWidth == width) return;
        knobWidth = width;
        resized();
    }
    void resized() override
    {
        const auto gap = juce::roundToInt(8.0f * scale);
        const auto footer = juce::roundToInt(fire::ui::Metrics::knobValueHeight * scale);
        auto bounds = getLocalBounds();
        const int columns = usesExpandedLayout() ? 6 : 3;
        const auto size = fire::ui::ordinaryKnobWidth(scale, {
            knobWidth > 0 ? knobWidth : fire::ui::ordinaryKnobWidth(scale),
            (bounds.getWidth() - gap * (columns - 1)) / columns,
            (bounds.getHeight() - gap) / 2 - footer});
        const auto height = fire::ui::ordinaryKnobHeight(size, scale);
        auto area = bounds.withSizeKeepingCentre(size * columns + gap * (columns - 1), height * 2 + gap);
        if (usesChordSelectors())
        {
            const auto selectorWidth = juce::jmax(1, juce::jmin(juce::roundToInt(112.0f * scale),
                bounds.getWidth() - size * 2 - gap * 2));
            area = bounds.withSizeKeepingCentre(selectorWidth + size * 2 + gap * 2, height * 2 + gap);
            auto choices = area.removeFromLeft(selectorWidth);
            area.removeFromLeft(gap);
            const auto titleHeight = juce::roundToInt(20.0f * scale);
            const auto menuHeight = juce::roundToInt(30.0f * scale);
            rootLabel.setBounds(choices.removeFromTop(titleHeight));
            rootMenu.setBounds(choices.removeFromTop(menuHeight));
            choices.removeFromTop(gap);
            chordLabel.setBounds(choices.removeFromTop(titleHeight));
            chordMenu.setBounds(choices.removeFromTop(menuHeight));
            choices.removeFromTop(gap);
            chordNotes.setBounds(choices);
            rootLabel.setFont(fire::ui::labelFont(11.0f * scale));
            chordLabel.setFont(fire::ui::labelFont(11.0f * scale));
            chordNotes.setFont(fire::ui::bodyFont(10.5f * scale));
            for (size_t row = 0; row < 2; ++row)
            {
                auto strip = area.removeFromTop(height);
                area.removeFromTop(gap);
                for (size_t column = 0; column < 2; ++column)
                {
                    if (auto* slider = sliders[2 + row * 2 + column]) slider->setBounds(strip.removeFromLeft(size));
                    strip.removeFromLeft(gap);
                }
            }
        }
        else if (usesExpandedLayout())
        {
            const std::array<size_t, 6> order { 3, 0, 2, 1, 4, 5 };
            auto top = area.removeFromTop(height);
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
            // Lo-Fi shares Master's two functional groups: reduction/mix
            // above, tape motion below. Old insert slots have no Jitter ID.
            const std::array<size_t, 6> order = type == fire::effects::Type::lofi
                ? std::array<size_t, 6> { 0, 1, 5, 2, 3, 4 }
                : std::array<size_t, 6> { 0, 1, 2, 3, 4, 5 };
            for (size_t row = 0; row < 2; ++row)
            {
                auto strip = area.removeFromTop(height); area.removeFromTop(gap);
                for (size_t column = 0; column < 3; ++column)
                {
                    if (auto* slider = sliders[order[row * 3 + column]]) slider->setBounds(strip.removeFromLeft(size));
                    strip.removeFromLeft(gap);
                }
            }
        }
    }
    std::function<void()> onLayoutChanged;

private:
    bool usesChordSelectors() const noexcept { return type == fire::effects::Type::chordResonator; }
    void unobserveType()
    {
        for (auto*& parameter : observedTypes)
        {
            if (parameter != nullptr) parameter->removeListener(this);
            parameter = nullptr;
        }
    }
    void parameterValueChanged(int, float) override
    {
        // Type notifications may arrive on the audio thread. Invalidate only
        // the interaction epoch here; the normal panel clock performs rebinds.
        choiceGeneration.fetch_add(1, std::memory_order_release);
    }
    void parameterGestureChanged(int, bool) override {}
    void updateChordChoice(size_t index, float normalized, std::uint64_t binding)
    {
        if (binding != bindingGeneration || ! usesChordSelectors()) return;
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        auto& menu = index == 0 ? rootMenu : chordMenu;
        const auto selected = std::isfinite(normalized)
            ? juce::jlimit(0, menu.getNumItems() - 1,
                juce::roundToInt(juce::jlimit(0.0f, 1.0f, normalized) * (menu.getNumItems() - 1)))
            : index == 0 ? fire::chord_resonator::defaultRoot - fire::chord_resonator::minimumRoot
                         : fire::chord_resonator::defaultChord;
        menu.setSelectedItemIndex(selected, juce::dontSendNotification);
        if (safe && binding == bindingGeneration) updateChordNotes();
    }
    void updateChordNotes()
    {
        // The root and chord helper data are shared with the audio engine.
        const auto root = fire::chord_resonator::minimumRoot + juce::jmax(0, rootMenu.getSelectedItemIndex());
        const auto chord = juce::jlimit(0, static_cast<int>(fire::chord_resonator::chordDefinitions.size() - 1),
                                      chordMenu.getSelectedItemIndex());
        const auto& definition = fire::chord_resonator::chordDefinitions[static_cast<size_t>(chord)];
        juce::String notes;
        for (int note = 0; note < definition.count; ++note)
        {
            if (note != 0) notes += note == 2 ? "\n" : "  ";
            notes += fire::chord_resonator::rootName(root + definition.intervals[static_cast<size_t>(note)]);
        }
        chordNotes.setText(notes, juce::dontSendNotification);
    }
    void bindChordSelectors(std::uint64_t binding)
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        observedTypes = {
            processor.treeState.getParameter(fire::effects::parameterID(scope, slot, fire::effects::typeField)),
            processor.treeState.getParameter(fire::modulation_fx::parameterID(scope, slot)),
            processor.treeState.getParameter(fire::resonator_params::parameterID(scope, slot))
        };
        for (auto* parameter : observedTypes) if (parameter) parameter->addListener(this);
        for (size_t index = 0; index < chordAttachments.size(); ++index)
        {
            auto& menu = index == 0 ? rootMenu : chordMenu;
            const auto id = fire::effects::parameterID(scope, slot, static_cast<int>(index));
            auto* parameter = processor.treeState.getParameter(id);
            menu.setComponentID(id);
            const auto valid = [safe, binding, index]
            {
                if (! safe || safe->bindingGeneration != binding || ! safe->active || ! safe->isEnabled()
                    || ! safe->isShowing() || ! safe->usesChordSelectors()) return false;
                const auto& selector = index == 0 ? safe->rootMenu : safe->chordMenu;
                return selector.isShowing() && selector.isEnabled()
                    && safe->processor.getInsertEffectType(safe->scope, safe->slot) == safe->type;
            };
            menu.configurePopupSession([safe] { return safe ? safe->choiceGeneration.load(std::memory_order_acquire) : 0; },
                valid, [safe, parameter, binding, index, valid](int item)
                {
                    if (! parameter || ! valid()) return;
                    const auto count = index == 0 ? fire::chord_resonator::maximumRoot - fire::chord_resonator::minimumRoot + 1
                                                  : static_cast<int>(fire::chord_resonator::chordNames.size());
                    if (! juce::isPositiveAndBelow(item - 1, count)) return;
                    const auto value = static_cast<float>(item - 1) / static_cast<float>(count - 1);
                    if (juce::approximatelyEqual(value, parameter->getValue())) return;
                    const auto generation = safe->choiceGeneration.load(std::memory_order_acquire);
                    parameter->beginChangeGesture();
                    const juce::ScopeGuard end {[parameter] { parameter->endChangeGesture(); }};
                    if (! valid() || safe->choiceGeneration.load(std::memory_order_acquire) != generation)
                    {
                        if (safe) safe->updateChordChoice(index, parameter->getValue(), binding);
                        return;
                    }
                    // No UI state is touched after this host notification.
                    parameter->setValueNotifyingHost(value);
                });
            if (! safe || binding != bindingGeneration) return;
            if (parameter != nullptr)
            {
                chordAttachments[index] = std::make_unique<juce::ParameterAttachment>(*parameter,
                    [safe, binding, index](float value)
                    { if (safe) safe->updateChordChoice(index, value, binding); }, nullptr);
                chordAttachments[index]->sendInitialUpdate();
                if (! safe || binding != bindingGeneration) return;
            }
        }
    }
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void configureSlider(ModulatableSlider& slider, fire::effects::Control definition,
                         juce::Colour colour, bool delaySync, bool cloudsDensity, bool fineLowValues = false)
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
        slider.textFromValueFunction = [definition, delaySync, cloudsDensity, fineLowValues](double value)
        {
            const auto physical = definition.fromNormalised(static_cast<float>(value));
            if (delaySync)
            {
                const juce::StringArray divisions { "Free", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1 Bar" };
                return divisions[juce::jlimit(0, 7, juce::roundToInt(physical))];
            }
            if (cloudsDensity && std::abs(physical) <= 6.0f) return juce::String("Off");
            const int decimals = fineLowValues && physical < 1.0f ? 2 : physical >= 100.0f ? 0 : 1;
            return juce::String(physical, decimals) + definition.unit;
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
        for (size_t index = 0; index < sliders.size(); ++index)
        {
            if (auto* slider = sliders[index]) slider->setVisible(active && (! usesChordSelectors() || index >= 2));
            if (! current()) return;
        }
        for (auto* component : std::array<juce::Component*, 5> {&rootMenu, &chordMenu, &rootLabel, &chordLabel, &chordNotes})
        {
            component->setVisible(active && usesChordSelectors());
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
    ContextAwareComboBox rootMenu, chordMenu;
    juce::Label rootLabel, chordLabel, chordNotes;
    std::array<juce::RangedAudioParameter*, 3> observedTypes {};
    std::atomic<std::uint64_t> choiceGeneration {0};
    std::array<std::unique_ptr<juce::ParameterAttachment>, 2> chordAttachments;
    // Destroy attachments while their local and parent-owned controls still exist.
    std::array<std::unique_ptr<SliderAttachment>, fire::effects::controlCount> attachments;
    std::array<std::unique_ptr<SliderAttachment>, cloudsExtraCount> cloudsAttachments;
    std::unique_ptr<ButtonAttachment> freezeAttachment;
    std::uint64_t bindingGeneration = 0, visibilityGeneration = 0;
    int scope = -1, slot = -1;
    fire::effects::Type type = fire::effects::Type::none;
    bool active = false;
    float scale = 1.0f;
    int knobWidth = 0;
};
