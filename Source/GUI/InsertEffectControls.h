#pragma once
#include "EffectRackNavigation.h"
#include "ContextAwareComboBox.h"
#include "EqControlsPanel.h"
#include "HardwareColourPanel.h"
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
    explicit InsertEffectControls(FireAudioProcessor& p) : processor(p), eqControls(p, false)
    {
        addChildComponent(freezeButton);
        freezeButton.setButtonText("Freeze");
        freezeButton.setClickingTogglesState(true);
        freezeButton.setTitle("Freeze granular recording");
        freezeButton.setTooltip("Hold the recorded audio while grains keep playing. Turn off to record new audio. "
                                "Frozen recordings are saved in presets, projects and A/B states. "
                                "With an empty recording, Freeze first captures about one second of new audio.");
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
        addChildComponent(coreMode);
        addChildComponent(reverbModelMenu);
        reverbModelMenu.setTitle("Reverb algorithm"); reverbModelMenu.setTooltip("Choose Classic, Room, Hall, Plate, Spring or Chamber.");
        for (int model = 0; model < fire::space::count; ++model) reverbModelMenu.addItem(fire::space::names[static_cast<size_t>(model)], model + 1);
        addChildComponent(hardwareColour);
        coreMode.setTitle("Shape mode");
        coreMode.setTooltip("Choose the waveshaping algorithm.");
        const juce::StringArray modes {"Arctan", "Exp", "Tanh", "Cubic", "Hard", "Sausage", "Sin", "Linear", "Limit", "Single Sin", "Logic", "Pit"};
        for (int index = 0; index < modes.size(); ++index) coreMode.addItem(modes[index], index + 1);
        coreMode.addSectionHeading("Analog Hardware");
        for (int index = 0; index < fire::analog::count; ++index) coreMode.addItem(fire::analog::names[static_cast<size_t>(index)], fire::analog::legacyCount + index + 1);
        for (size_t index = 0; index < coreSwitches.size(); ++index)
        {
            auto& button = coreSwitches[index];
            addChildComponent(button); button.setClickingTogglesState(true);
            button.setButtonText(index == 0 ? "Safe" : index == 1 ? "Extreme" : "Comp");
            button.setTitle("Drive " + button.getButtonText()); button.setTooltip(button.getTitle());
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
    void setEqControls(const EqControlsPanel::Knobs& controls)
    {
        eqKnobs = controls;
        addChildComponent(eqControls);
        eqControls.setKnobs(controls);
    }
    void setAnalogDriveControl(ModulatableSlider& control) {analogDriveKnob = &control; addChildComponent(control);}
    bool usesExpandedLayout() const noexcept
    { return type == fire::effects::Type::granular; }
    bool usesFullWidthLayout() const noexcept
    { return usesExpandedLayout() || type == fire::effects::Type::lofi || type == fire::effects::Type::eq || usesAnalogLayout(); }
    bool usesAnalogLayout() const noexcept {return type == fire::effects::Type::shape && processor.getShapeMode(scope, slot) >= fire::analog::legacyCount;}
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
        coreModeAttachment.reset();
        shapeModelAttachment.reset();
        analogDriveAttachment.reset();
        reverbModelAttachment.reset();
        for (auto& attachment : coreSwitchAttachments) attachment.reset();
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
        else if (type == fire::effects::Type::lofi && cloudsSliders[0])
        {
            auto& slider = *cloudsSliders[0];
            const auto id = fire::core_modules::parameterID(targetScope, targetSlot, fire::core_modules::jitterField);
            slider.parameterID = id; slider.setComponentID(id);
            cloudsAttachments[0] = std::make_unique<SliderAttachment>(processor.treeState, id, slider);
            if (! isCurrent()) return;
            configureSlider(slider, {"Jitter", " %", 0, 100, 0}, colour, false, false);
            if (! isCurrent()) return;
            for (size_t i = 1; i < cloudsSliders.size(); ++i) if (cloudsSliders[i]) cloudsSliders[i]->parameterID.clear();
        }
        else
        {
            for (auto* slider : cloudsSliders) if (slider) slider->parameterID.clear();
        }
        if (type == fire::effects::Type::eq)
        {
            eqControls.bind(targetScope, targetSlot);
            if (! isCurrent()) return;
            for (int node = 0; node < fire::eq::maxNodes; ++node)
                for (int control = 0; control < 3; ++control)
                {
                    auto* slider = eqKnobs[static_cast<size_t>(node)][static_cast<size_t>(control)];
                    if (! slider) continue;
                    const auto id = fire::core_modules::eqParameterID(targetScope, targetSlot, node, static_cast<fire::eq::Field>(control));
                    slider->parameterID = id; slider->setComponentID(id);
                    eqAttachments[static_cast<size_t>(node)][static_cast<size_t>(control)] = std::make_unique<SliderAttachment>(processor.treeState, id, *slider);
                    if (! isCurrent()) return;
                    slider->textFromValueFunction = {}; slider->valueFromTextFunction = {};
                    slider->setTextValueSuffix(control == 0 ? " Hz" : control == 1 ? " dB" : "");
                    slider->setLabel(control == 0 ? "Frequency" : control == 1 ? "Gain" : "Q", colour);
                    slider->setTitle("EQ point " + juce::String(node + 1) + (control == 0 ? " Frequency" : control == 1 ? " Gain" : " Q"));
                    slider->setDoubleClickReturnValue(true, control == 0 ? fire::eq::defaultNode(node).frequency : control == 1 ? 0 : fire::eq::defaultNode(node).q);
                    slider->updateText();
                    if (! isCurrent()) return;
                }
        }
        else for (auto& node : eqAttachments) for (auto& attachment : node) attachment.reset();

        if (usesChordSelectors())
        {
            bindChordSelectors(request);
            if (! isCurrent()) return;
        }
        if (type == fire::effects::Type::drive || type == fire::effects::Type::shape)
        {
            for (size_t index = 0; index < coreSwitches.size(); ++index)
            {
                const int field = type == fire::effects::Type::shape ? 3 : static_cast<int>(index + 1);
                if (type == fire::effects::Type::shape && index != 0) continue;
                auto& button = coreSwitches[index];
                const auto id = fire::effects::parameterID(scope, slot, field);
                button.setButtonText(type == fire::effects::Type::shape ? "DC Filter" : index == 0 ? "Safe" : index == 1 ? "Extreme" : "Comp");
                button.setTitle(juce::String(fire::effects::name(type)) + " " + button.getButtonText());
                button.setTooltip(button.getTitle());
                button.setComponentID(id);
                coreSwitchAttachments[index] = std::make_unique<ButtonAttachment>(processor.treeState, id, button);
                if (!isCurrent()) return;
            }
        }
        if (type == fire::effects::Type::shape)
        {
            if (analogDriveKnob)
            {
                const auto driveID = fire::analog_params::driveID(scope, slot);
                analogDriveKnob->parameterID = driveID; analogDriveKnob->setComponentID(driveID);
                analogDriveKnob->textFromValueFunction = {}; analogDriveKnob->valueFromTextFunction = {};
                analogDriveKnob->setTextValueSuffix({}); analogDriveKnob->setLabel("Drive", fire::ui::colours::drive);
                analogDriveKnob->setTitle("Analog Drive"); analogDriveKnob->setTooltip("Input gain into the analog model; filament exposure follows Drive and signal energy.");
                analogDriveAttachment = std::make_unique<SliderAttachment>(processor.treeState, driveID, *analogDriveKnob);
                if (!isCurrent()) return;
            }
            const auto id = fire::effects::parameterID(scope, slot, 0);
            coreMode.setComponentID(id);
            auto* parameter = processor.treeState.getParameter(id);
            const auto valid = [safe, request]
            {return safe && safe->bindingGeneration == request && safe->isShowing() && safe->isEnabled()
                && safe->processor.getInsertEffectType(safe->scope, safe->slot) == fire::effects::Type::shape;};
            coreMode.configurePopupSession([safe] {return safe ? safe->choiceGeneration.load(std::memory_order_acquire) : 0;}, valid,
                [safe, valid](int item)
                {if (valid() && item >= 1 && item <= fire::analog::modeCount) safe->processor.setShapeMode(safe->scope, safe->slot, item - 1);});
            const auto update = [safe, request](float)
            {
                if (!safe || safe->bindingGeneration != request) return;
                safe->coreMode.setSelectedId(safe->processor.getShapeMode(safe->scope, safe->slot) + 1, juce::dontSendNotification);
                if (!safe || safe->bindingGeneration != request) return;
                safe->updateVisibility(); if (!safe) return;
                safe->resized(); if (!safe) return;
                auto callback = safe->onLayoutChanged; if (callback) callback();
            };
            coreModeAttachment = std::make_unique<juce::ParameterAttachment>(*parameter, update, nullptr);
            shapeModelAttachment = std::make_unique<juce::ParameterAttachment>(*processor.treeState.getParameter(fire::analog_params::parameterID(scope, slot)), update, nullptr);
            coreModeAttachment->sendInitialUpdate();
            if (!isCurrent()) return;
        }
        if (type == fire::effects::Type::reverb)
        {
            const auto id = fire::reverb_params::parameterID(scope, slot);
            reverbModelMenu.setComponentID(id);
            reverbModelMenu.configurePopupSession([safe] {return safe ? safe->choiceGeneration.load(std::memory_order_acquire) : 0;},
                [safe, request] {return safe && safe->bindingGeneration == request && safe->isShowing() && safe->isEnabled()
                    && safe->processor.getInsertEffectType(safe->scope, safe->slot) == fire::effects::Type::reverb;},
                processor.treeState.getParameter(id));
            reverbModelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, id, reverbModelMenu);
            if (!isCurrent()) return;
        }
        updateVisibility();
        if (! isCurrent()) return;
        resized();
        if (! isCurrent()) return;
        auto callback = onLayoutChanged;
        if (callback) callback();
    }
    void refresh()
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        if (scope >= 0 && slot >= 0) bind(scope, slot);
        if (!safe) return;
        if (type == fire::effects::Type::eq) eqControls.animationTick(1.0f / 60.0f);
        if (!safe) return;
        if (hardwareColour.isShowing())
        {
            const auto peak = scope == 0 ? juce::jmax(processor.getGlobalInputPeakLevel(0), processor.getGlobalInputPeakLevel(1))
                : juce::jmax(processor.getBandInputPeakLevel(scope - 1, 0), processor.getBandInputPeakLevel(scope - 1, 1));
            hardwareColour.setState(processor.getShapeMode(scope, slot) - fire::analog::legacyCount,
                analogDriveKnob ? static_cast<float>(analogDriveKnob->getValue()) : 0, peak, 1.0f / 60.0f, processor.getAudioActivitySequence());
        }
    }
    void dismissButtons()
    {
        const juce::Component::SafePointer<InsertEffectControls> safe(this);
        choiceGeneration.fetch_add(1, std::memory_order_release);
        rootMenu.dismissTransientInteraction();
        if (! safe) return;
        chordMenu.dismissTransientInteraction();
        if (! safe) return;
        eqControls.dismiss();
        if (! safe) return;
        coreMode.dismissTransientInteraction();
        if (!safe) return;
        reverbModelMenu.dismissTransientInteraction(); if (!safe) return;
        for (auto& button : coreSwitches) {button.dismissPointerGesture(); if (!safe) return;}
        if (analogDriveKnob) analogDriveKnob->dismissTransientInteraction();
        if (!safe) return;
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
        if (usesAnalogLayout())
        {
            auto area = getLocalBounds().reduced(juce::roundToInt(7 * scale));
            hardwareColour.setBounds(area.removeFromLeft(area.getWidth() * 48 / 100));
            area.removeFromLeft(juce::roundToInt(16 * scale));
            coreMode.setBounds(area.removeFromTop(juce::roundToInt(32 * scale)));
            area.removeFromTop(juce::roundToInt(8 * scale));
            const auto footer = area.removeFromBottom(juce::roundToInt(28 * scale));
            coreSwitches[0].setBounds(footer.withSizeKeepingCentre(juce::roundToInt(96 * scale), footer.getHeight()));
            const int gap = juce::roundToInt(10 * scale);
            const auto size = fire::ui::ordinaryKnobWidth(scale, {(area.getWidth() - gap * 3) / 4,
                area.getHeight() - juce::roundToInt(fire::ui::Metrics::knobValueHeight * scale)});
            const auto height = fire::ui::ordinaryKnobHeight(size, scale);
            auto strip = area.withSizeKeepingCentre(size * 4 + gap * 3, height);
            for (auto* knob : {analogDriveKnob, sliders[1], sliders[2], sliders[5]})
            {if (knob) knob->setBounds(strip.removeFromLeft(size)); strip.removeFromLeft(gap);}
            return;
        }
        if (type == fire::effects::Type::eq)
        {
            auto area = getLocalBounds();
            const auto size = fire::ui::ordinaryKnobWidth(scale, {knobWidth > 0 ? knobWidth : fire::ui::ordinaryKnobWidth(scale), area.getWidth() / 5});
            auto mix = area.removeFromRight(size + juce::roundToInt(8 * scale));
            if (sliders[5]) sliders[5]->setBounds(mix.withSizeKeepingCentre(size, fire::ui::ordinaryKnobHeight(size, scale)));
            eqControls.setScale(scale); eqControls.setKnobWidth(knobWidth); eqControls.setBounds(area); return;
        }
        const auto gap = juce::roundToInt(8.0f * scale);
        const auto footer = juce::roundToInt(fire::ui::Metrics::knobValueHeight * scale);
        auto bounds = getLocalBounds();
        if (type == fire::effects::Type::reverb)
        {
            auto header = bounds.removeFromTop(juce::roundToInt(28 * scale));
            reverbModelMenu.setBounds(header.withSizeKeepingCentre(juce::jmin(header.getWidth(), juce::roundToInt(174 * scale)), header.getHeight()));
            bounds.removeFromTop(juce::roundToInt(5 * scale));
        }
        const int columns = usesExpandedLayout() ? 6 : type == fire::effects::Type::lofi ? 4 : 3;
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
        else if (type == fire::effects::Type::lofi)
        {
            auto top = area.removeFromTop(height); area.removeFromTop(gap);
            for (auto index : {0, 1}) {if (sliders[static_cast<size_t>(index)]) sliders[static_cast<size_t>(index)]->setBounds(top.removeFromLeft(size)); top.removeFromLeft(gap);}
            if (cloudsSliders[0]) cloudsSliders[0]->setBounds(top.removeFromLeft(size)); top.removeFromLeft(gap);
            if (sliders[5]) sliders[5]->setBounds(top.removeFromLeft(size));
            auto bottom = area.withSizeKeepingCentre(size * 3 + gap * 2, height);
            for (auto index : {2, 3, 4}) {if (sliders[static_cast<size_t>(index)]) sliders[static_cast<size_t>(index)]->setBounds(bottom.removeFromLeft(size)); bottom.removeFromLeft(gap);}
        }
        else
        {
            for (size_t row = 0; row < 2; ++row)
            {
                auto strip = area.removeFromTop(height); area.removeFromTop(gap);
                for (size_t column = 0; column < 3; ++column)
                {
                    if (auto* slider = sliders[row * 3 + column]) slider->setBounds(strip.removeFromLeft(size));
                    strip.removeFromLeft(gap);
                }
            }
        }
        if (type == fire::effects::Type::shape && sliders[0])
            coreMode.setBounds(sliders[0]->getBounds().withSizeKeepingCentre(juce::jmax(size, juce::roundToInt(98 * scale)), juce::roundToInt(30 * scale)));
        for (size_t index = 0; index < coreSwitches.size(); ++index)
        {
            const auto field = type == fire::effects::Type::shape ? size_t{3} : index + 1;
            if (sliders[field]) coreSwitches[index].setBounds(sliders[field]->getBounds().withSizeKeepingCentre(juce::roundToInt(72 * scale), juce::roundToInt(28 * scale)));
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
            : slider.getTitle());
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
            const bool switchControl = (type == fire::effects::Type::drive && index >= 1 && index <= 3)
                || (type == fire::effects::Type::shape && (index == 0 || index == 3));
            if (auto* slider = sliders[index]) slider->setVisible(active && !switchControl && (type != fire::effects::Type::eq || index == 5)
                && fire::effects::controls(type)[index].name[0] != 0 && (! usesChordSelectors() || index >= 2));
            if (! current()) return;
        }
        for (auto* component : std::array<juce::Component*, 5> {&rootMenu, &chordMenu, &rootLabel, &chordLabel, &chordNotes})
        {
            component->setVisible(active && usesChordSelectors());
            if (! current()) return;
        }
        for (size_t i = 0; i < cloudsSliders.size(); ++i)
        {
            if (auto* slider = cloudsSliders[i]) slider->setVisible(active && (usesExpandedLayout() || (type == fire::effects::Type::lofi && i == 0)));
            if (! current()) return;
        }
        freezeButton.setVisible(active && usesExpandedLayout());
        if (current()) eqControls.setVisible(active && type == fire::effects::Type::eq);
        if (!current()) return;
        coreMode.setVisible(active && type == fire::effects::Type::shape);
        if (!current()) return;
        reverbModelMenu.setVisible(active && type == fire::effects::Type::reverb);
        if (!current()) return;
        hardwareColour.setVisible(active && usesAnalogLayout());
        if (!current()) return;
        if (analogDriveKnob) analogDriveKnob->setVisible(active && usesAnalogLayout());
        if (!current()) return;
        for (size_t index = 0; index < coreSwitches.size(); ++index)
        {coreSwitches[index].setVisible(active && (type == fire::effects::Type::drive || (type == fire::effects::Type::shape && index == 0))); if (!current()) return;}
    }
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    FireAudioProcessor& processor;
    EqControlsPanel eqControls;
    EqControlsPanel::Knobs eqKnobs {};
    std::array<std::array<std::unique_ptr<SliderAttachment>, 3>, fire::eq::maxNodes> eqAttachments;
    ContextAwareComboBox coreMode;
    ContextAwareComboBox reverbModelMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> reverbModelAttachment;
    fire::ui::HardwareColourPanel hardwareColour;
    ModulatableSlider* analogDriveKnob = nullptr;
    std::unique_ptr<SliderAttachment> analogDriveAttachment;
    std::array<PrimaryTextButton, 3> coreSwitches;
    std::array<std::unique_ptr<ButtonAttachment>, 3> coreSwitchAttachments;
    std::unique_ptr<juce::ParameterAttachment> coreModeAttachment, shapeModelAttachment;
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
