#pragma once

#include "../PluginProcessor.h"
#include "../Utility/EqParameters.h"
#include "ContextAwareComboBox.h"
#include "ModulatableSlider.h"
#include "PrimaryButton.h"

// Only the selected point's controls are presented. Slots keep their parameter
// identity when other points are removed; the navigation uses visible ordinals.
class EqControlsPanel final : public juce::Component,
                              private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr int capacity = 12;
    using Knobs = std::array<std::array<ModulatableSlider*, 3>, capacity>;

    explicit EqControlsPanel(FireAudioProcessor& p, bool bindLegacy = true) : processor(p)
    {
        setTitle("EQ point controls");
        addAndMakeVisible(addButton);
        addAndMakeVisible(removeButton);
        addAndMakeVisible(powerButton);
        addAndMakeVisible(typeMenu);
        addAndMakeVisible(slopeMenu);
        addButton.setButtonText("+");
        addButton.setTitle("Add EQ point");
        addButton.setTooltip("Add a bell point. You can also double-click the spectrum.");
        removeButton.setButtonText("-");
        removeButton.setTitle("Remove selected EQ point");
        removeButton.setTooltip("Remove the selected EQ point");
        powerButton.getProperties().set("iconType", "power");
        powerButton.setTitle("Selected EQ point power");
        powerButton.setTooltip("Enable or bypass the selected EQ point");
        powerButton.setColour(juce::ToggleButton::tickColourId, fire::ui::colours::filter);
        for (auto* menu : {&typeMenu, &slopeMenu})
        {
            menu->setColour(juce::ComboBox::backgroundColourId, fire::ui::paletteFor(*this).surface1);
            menu->setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
            menu->setColour(juce::ComboBox::textColourId, fire::ui::paletteFor(*this).textPrimary);
            menu->setColour(juce::ComboBox::arrowColourId, fire::ui::colours::filter);
        }
        const char* types[] {"Bell", "Low cut", "High cut", "Low shelf", "High shelf", "Notch", "Band pass"};
        for (int i = 0; i < 7; ++i) typeMenu.addItem(types[i], i + 1);
        for (int i = 0; i < 4; ++i) slopeMenu.addItem(juce::String((i + 1) * 12) + " dB/oct", i + 1);
        typeMenu.setTitle("EQ point filter type");
        slopeMenu.setTitle("EQ point slope");
        typeMenu.setTooltip("Select the filter type for the selected EQ point");
        slopeMenu.setTooltip("Select the slope of the selected low-cut or high-cut point");
        const juce::Component::SafePointer<EqControlsPanel> safe(this);
        addButton.onClick = [safe]
        {
            if (! safe) return;
            const auto slot = safe->processor.addEqNode(1000.0f, 0.0f, fire::eq::Type::bell, safe->fxScope, safe->fxSlot);
            if (safe && slot >= 0) safe->selectNode(slot);
        };
        removeButton.onClick = [safe]
        {
            if (! safe || safe->selected < 0) return;
            safe->processor.removeEqNode(safe->selected, safe->fxScope, safe->fxSlot);
            if (safe) safe->refresh();
        };
        powerButton.onClick = [safe]
        {
            if (! safe || safe->selected < 0) return;
            auto* parameter = safe->processor.treeState.getParameter(
                safe->nodeParameterID(safe->selected, fire::eq::Field::bypassed));
            const auto value = safe->powerButton.getToggleState() ? 0.0f : 1.0f;
            if (parameter != nullptr)
            {
                const auto expectedGeneration = safe->interactionGeneration();
                parameter->beginChangeGesture();
                const juce::ScopeGuard end {[parameter] { parameter->endChangeGesture(); }};
                if (! safe || safe->interactionGeneration() != expectedGeneration) return;
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            }
        };
        for (int slot = 0; slot < capacity; ++slot)
        {
            auto& point = navigation[static_cast<size_t>(slot)];
            addChildComponent(point);
            point.setComponentID((bindLegacy ? "eqPointNavigation" : "insertEqPointNavigation") + juce::String(slot + 1));
            point.setTitle("EQ point " + juce::String(slot + 1));
            point.setTooltip("Select EQ point " + juce::String(slot + 1));
            point.onClick = [safe, slot] { if (safe) safe->selectNode(slot); };
        }
        if (bindLegacy) bind(-1, -1);
    }

    ~EqControlsPanel() override
    {
        for (auto* parameter : presentParameterObjects) if (parameter) parameter->removeListener(this);
        dismiss();
    }
    std::function<void(int)> onSelectionChanged;

    void bind(int scope, int slot)
    {
        if (bound && fxScope == scope && fxSlot == slot) return;
        const juce::Component::SafePointer<EqControlsPanel> safe(this);
        const auto request = ++bindingGeneration;
        dismiss();
        if (!safe || request != bindingGeneration) return;
        for (auto* parameter : presentParameterObjects) if (parameter) parameter->removeListener(this);
        fxScope = scope; fxSlot = slot; bound = true;
        selected = -1; presentedSelection = -2; paintSignature = -1; present.fill(false);
        for (int node = 0; node < capacity; ++node)
        {
            const auto i = static_cast<size_t>(node);
            navigation[i].setComponentID(slot < 0 ? "eqPointNavigation" + juce::String(node + 1)
                : fire::effects::parameterID(scope, slot, fire::effects::typeField) + "EqPoint" + juce::String(node + 1));
            presentParameters[i] = processor.treeState.getRawParameterValue(nodeParameterID(node, fire::eq::Field::present));
            presentParameterObjects[i] = processor.treeState.getParameter(nodeParameterID(node, fire::eq::Field::present));
            typeParameters[i] = processor.treeState.getRawParameterValue(nodeParameterID(node, fire::eq::Field::type));
            bypassParameters[i] = processor.treeState.getRawParameterValue(nodeParameterID(node, fire::eq::Field::bypassed));
            if (presentParameterObjects[i]) presentParameterObjects[i]->addListener(this);
        }
        refresh();
    }
    void setKnobs(Knobs next)
    {
        knobs = next;
        presentedSelection = -2;
        for (auto& node : knobs)
            for (auto* knob : node)
                if (knob != nullptr) addChildComponent(knob);
        refresh();
    }
    int getSelectedNode() const noexcept { return selected; }
    void setScale(float next)
    {
        if (juce::approximatelyEqual(scale, next)) return;
        scale = next; resized();
    }
    void setKnobWidth(int width)
    {
        if (knobWidth == width) return;
        knobWidth = width;
        resized();
    }
    void selectNode(int slot)
    {
        if (slot < 0 || slot >= capacity || ! processor.getEqNodeState(slot, fxScope, fxSlot).present) return;
        if (selected == slot) { refresh(); return; }
        const juce::Component::SafePointer<EqControlsPanel> safe(this);
        const auto expectedGeneration = interactionGeneration() + 1;
        dismiss();
        if (! safe || interactionGeneration() != expectedGeneration) return;
        selected = slot;
        bindMenus();
        if (! safe) return;
        refresh();
        if (safe && onSelectionChanged) onSelectionChanged(selected);
    }
    void dismiss()
    {
        ++generation;
        const juce::Component::SafePointer<EqControlsPanel> safe(this);
        typeMenu.dismissTransientInteraction();
        if (! safe) return;
        slopeMenu.dismissTransientInteraction();
        if (! safe) return;
        for (auto& node : knobs)
            for (auto* knob : node)
            {
                if (knob != nullptr) knob->dismissTransientInteraction();
                if (! safe) return;
            }
        addButton.dismissPointerGesture();
        if (! safe) return;
        removeButton.dismissPointerGesture();
        if (! safe) return;
        powerButton.dismissPointerGesture();
        if (! safe) return;
        for (auto& dot : navigation)
        {
            dot.dismissPointerGesture();
            if (! safe) return;
        }
    }
    void animationTick(float seconds)
    {
        if (! isShowing()) return;
        const juce::Component::SafePointer<EqControlsPanel> safe(this);
        refresh();
        if (! safe) return;
        for (auto& dot : navigation) dot.advance(seconds);
    }
    void refresh()
    {
        const juce::Component::SafePointer<EqControlsPanel> safe(this);
        const auto structureEpoch = presenceEpoch.load(std::memory_order_acquire);
        if (structureEpoch != presentedPresenceEpoch)
        {
            presentedPresenceEpoch = structureEpoch;
            const auto expectedGeneration = interactionGeneration() + 1;
            dismiss();
            if (! safe || interactionGeneration() != expectedGeneration) return;
        }
        std::array<bool, capacity> nextPresent {};
        int count = 0;
        for (int i = 0; i < capacity; ++i)
        {
            const auto* value = presentParameters[static_cast<size_t>(i)];
            nextPresent[static_cast<size_t>(i)] = value != nullptr && value->load(std::memory_order_relaxed) > 0.5f;
            if (nextPresent[static_cast<size_t>(i)]) ++count;
        }
        const bool structureChanged = present != nextPresent;
        present = nextPresent;
        if (selected < 0 || ! present[static_cast<size_t>(selected)])
        {
            int replacement = -1;
            for (int i = juce::jmax(0, selected); i < capacity; ++i)
                if (present[static_cast<size_t>(i)]) { replacement = i; break; }
            if (replacement < 0)
                for (int i = capacity - 1; i >= 0; --i)
                    if (present[static_cast<size_t>(i)]) { replacement = i; break; }
            if (replacement != selected)
            {
                const auto expectedGeneration = interactionGeneration() + 1;
                dismiss();
                if (! safe || interactionGeneration() != expectedGeneration) return;
                selected = replacement;
                bindMenus();
                if (! safe) return;
                if (onSelectionChanged) onSelectionChanged(selected);
                if (! safe) return;
            }
        }
        const bool selectionChanged = presentedSelection != selected;
        presentedSelection = selected;
        int ordinal = 0;
        for (int i = 0; (structureChanged || selectionChanged) && i < capacity; ++i)
        {
            auto& dot = navigation[static_cast<size_t>(i)];
            dot.setVisible(present[static_cast<size_t>(i)]);
            if (! safe) return;
            if (present[static_cast<size_t>(i)])
            {
                dot.ordinal = ++ordinal;
                dot.setTitle("EQ point " + juce::String(ordinal));
                dot.setTooltip("Select EQ point " + juce::String(ordinal));
            }
            dot.setSelectedState(selected == i);
            for (auto* knob : knobs[static_cast<size_t>(i)])
            {
                if (knob != nullptr && knob->isVisible() != (selected == i))
                {
                    const auto expectedGeneration = interactionGeneration();
                    knob->dismissTransientInteraction();
                    if (! safe || interactionGeneration() != expectedGeneration) return;
                }
                if (knob != nullptr) knob->setVisible(selected == i);
                if (! safe) return;
            }
        }
        addButton.setEnabled(count < capacity);
        if (! safe) return;
        if (structureChanged)
            addButton.setTooltip(count == capacity ? "Maximum 12 EQ points. Remove a point to add another."
                                                   : "Add a bell point. You can also double-click the spectrum.");
        removeButton.setEnabled(selected >= 0);
        if (! safe) return;
        powerButton.setVisible(selected >= 0);
        if (! safe) return;
        typeMenu.setVisible(selected >= 0);
        if (! safe) return;
        slopeMenu.setVisible(selected >= 0);
        if (! safe) return;
        if (selected >= 0)
        {
            const auto index = static_cast<size_t>(selected);
            const bool bypassed = bypassParameters[index] != nullptr
                               && bypassParameters[index]->load(std::memory_order_relaxed) > 0.5f;
            const auto rawType = typeParameters[index] != nullptr ? typeParameters[index]->load(std::memory_order_relaxed) : 0.0f;
            const int type = std::isfinite(rawType) ? juce::jlimit(0, 6, juce::roundToInt(rawType)) : 0;
            powerButton.setToggleState(! bypassed, juce::dontSendNotification);
            powerButton.setTitle("EQ point " + juce::String(navigation[static_cast<size_t>(selected)].ordinal) + " power");
            const bool cut = type == 1 || type == 2;
            slopeMenu.setEnabled(cut);
            if (! safe) return;
            auto* gain = knobs[static_cast<size_t>(selected)][1];
            const bool gainEnabled = type != 5 && type != 6;
            if (gain != nullptr && gain->isEnabled() != gainEnabled)
            {
                // JUCE setEnabled continues accessing the component after its
                // enablement callback. End host gestures before entering it.
                const auto expectedGeneration = interactionGeneration();
                gain->dismissTransientInteraction();
                if (! safe || interactionGeneration() != expectedGeneration) return;
                gain->setEnabled(gainEnabled);
            }
            if (! safe) return;
            const int signature = selected * 32 + type * 2 + (bypassed ? 1 : 0);
            if (signature != paintSignature) { paintSignature = signature; repaint(); }
        }
        if (structureChanged || selectionChanged) { resized(); repaint(); }
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(juce::roundToInt(8 * scale), 0);
        header = area.removeFromTop(juce::roundToInt(32 * scale));
        auto footer = area.removeFromBottom(juce::roundToInt(38 * scale));
        emptyBounds = area;

        const int gap = juce::roundToInt(10 * scale);
        const int columnGap = juce::roundToInt(16 * scale);
        const int utilityWidth = juce::jmin(juce::roundToInt(145 * scale), area.getWidth() / 3);
        const int width = fire::ui::ordinaryKnobWidth(scale, {
            knobWidth > 0 ? knobWidth : fire::ui::ordinaryKnobWidth(scale),
            (area.getWidth() - utilityWidth - columnGap - 2 * gap) / 3,
            area.getHeight() - juce::roundToInt(fire::ui::Metrics::knobValueHeight * scale)});
        const int height = fire::ui::ordinaryKnobHeight(width, scale);
        const int groupWidth = width * 3 + gap * 2 + columnGap + utilityWidth;

        // Keep the point actions, dials and selectors on one shared grid.
        // Centre the complete group so selectors never drift to the far edge.
        header = header.withSizeKeepingCentre(groupWidth, header.getHeight());
        auto actions = header.withHeight(juce::roundToInt(28 * scale))
            .withCentre(header.getCentre());
        const int buttonWidth = juce::roundToInt(28 * scale);
        removeButton.setBounds(actions.removeFromRight(buttonWidth));
        actions.removeFromRight(juce::roundToInt(6 * scale));
        addButton.setBounds(actions.removeFromRight(buttonWidth));
        actions.removeFromLeft(juce::roundToInt(78 * scale));
        powerButton.setBounds(actions.removeFromLeft(buttonWidth));

        int count = 0;
        for (bool enabled : present) if (enabled) ++count;
        const auto pitch = juce::jmin(juce::roundToInt(29 * scale), footer.getWidth() / juce::jmax(1, count));
        auto strip = footer.withSizeKeepingCentre(count * pitch, footer.getHeight());
        for (int i = 0; i < capacity; ++i)
            if (present[static_cast<size_t>(i)]) navigation[static_cast<size_t>(i)].setBounds(strip.removeFromLeft(pitch));

        auto row = area.withSizeKeepingCentre(groupWidth,
            juce::jmax(height, juce::roundToInt(112 * scale)));
        auto utility = row.removeFromRight(utilityWidth);
        utilityDivider = juce::Rectangle<float>(
            static_cast<float>(utility.getX() - columnGap / 2),
            static_cast<float>(row.getY()), juce::jmax(0.7f, scale),
            static_cast<float>(row.getHeight()));
        typeLabel = utility.removeFromTop(juce::roundToInt(20 * scale));
        typeMenu.setBounds(utility.removeFromTop(juce::roundToInt(30 * scale)));
        utility.removeFromTop(juce::roundToInt(9 * scale));
        slopeLabel = utility.removeFromTop(juce::roundToInt(20 * scale));
        slopeMenu.setBounds(utility.removeFromTop(juce::roundToInt(30 * scale)));
        row.removeFromRight(columnGap);
        row.setHeight(height);
        for (int control = 0; control < 3; ++control)
        {
            const auto bounds = row.removeFromLeft(width);
            row.removeFromLeft(gap);
            for (auto& node : knobs) if (node[static_cast<size_t>(control)] != nullptr) node[static_cast<size_t>(control)]->setBounds(bounds);
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(fire::ui::paletteFor(*this).textSecondary);
        g.setFont(fire::ui::labelFont(11 * scale));
        auto title = selected >= 0 ? header.withWidth(juce::roundToInt(70 * scale))
                                  : header.withTrimmedRight(juce::roundToInt(76 * scale));
        if (selected >= 0)
        {
            g.setColour(fire::ui::paletteFor(*this).hairline.withAlpha(0.24f));
            g.fillRect(utilityDivider);
            g.setColour(fire::ui::paletteFor(*this).textSecondary);
            const auto ordinal = navigation[static_cast<size_t>(selected)].ordinal;
            g.drawText("POINT " + juce::String(ordinal).paddedLeft('0', 2), title, juce::Justification::centredLeft);
            g.drawText("FILTER TYPE", typeLabel, juce::Justification::centredLeft);
            if (! slopeMenu.isEnabled()) g.setColour(fire::ui::paletteFor(*this).textMuted.withAlpha(0.55f));
            g.drawText("SLOPE", slopeLabel, juce::Justification::centredLeft);
        }
        else
        {
            g.drawText("NO EQ POINTS", title, juce::Justification::centredLeft);
            g.setFont(fire::ui::bodyFont(13 * scale));
            g.drawFittedText("Double-click the spectrum or use + to add a point", emptyBounds,
                             juce::Justification::centred, 2);
        }
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress::leftKey && key != juce::KeyPress::rightKey) return false;
        bool navigationFocused = false;
        for (auto& dot : navigation) navigationFocused = navigationFocused || dot.hasKeyboardFocus(false);
        if (! navigationFocused || selected < 0) return false;
        const int direction = key == juce::KeyPress::rightKey ? 1 : -1;
        for (int distance = 1; distance < capacity; ++distance)
        {
            const int next = (selected + direction * distance + capacity) % capacity;
            if (present[static_cast<size_t>(next)])
            {
                const juce::Component::SafePointer<EqControlsPanel> safe(this);
                selectNode(next);
                if (safe) navigation[static_cast<size_t>(next)].grabKeyboardFocus();
                return true;
            }
        }
        return true;
    }
private:
    class PointButton final : public PrimaryTextButton
    {
    public:
        int ordinal = 0;
        void setSelectedState(bool value)
        {
            setToggleState(value, juce::dontSendNotification);
            expansion.setTarget(value ? 1.0f : 0.0f);
            if (! isShowing()) expansion.snapTo(value ? 1.0f : 0.0f);
        }
        void advance(float seconds)
        {
            expansion.setTarget(getToggleState() ? 1.0f : 0.0f);
            if (expansion.advance(seconds, 0.13f)) repaint();
        }
        void paint(juce::Graphics& g) override
        {
            const float size = juce::jmin(getWidth(), getHeight()) * (0.29f + 0.48f * expansion.current);
            auto dot = getLocalBounds().toFloat().withSizeKeepingCentre(size, size);
            const auto colour = getToggleState() ? fire::ui::colours::filter : fire::ui::paletteFor(*this).textMuted;
            g.setColour(colour.withAlpha(getToggleState() ? 0.20f : (isMouseOver() ? 0.7f : 0.32f)));
            g.fillEllipse(dot);
            if (getToggleState() || hasKeyboardFocus(false))
            {
                g.setColour(colour.withAlpha(0.9f)); g.drawEllipse(dot, 1.0f);
            }
            if (expansion.current > 0.45f)
            {
                g.setColour(fire::ui::paletteFor(*this).textPrimary.withAlpha(expansion.current));
                g.setFont(fire::ui::labelFont(juce::jmax(9.0f, size * 0.51f)));
                g.drawText(juce::String(ordinal), dot, juce::Justification::centred);
            }
        }
    private:
        fire::ui::DampedValue expansion;
    };
    void bindMenus()
    {
        typeAttachment.reset(); slopeAttachment.reset();
        if (selected < 0) return;
        const juce::Component::SafePointer<EqControlsPanel> safe(this);
        for (auto pair : {std::pair{&typeMenu, fire::eq::Field::type}, std::pair{&slopeMenu, fire::eq::Field::slope}})
        {
            auto id = nodeParameterID(selected, pair.second);
            pair.first->setComponentID(id);
            pair.first->configurePopupSession([safe] { return safe ? safe->interactionGeneration() : 0; },
                [safe] { return safe && safe->isShowing() && safe->isEnabled() && safe->selected >= 0
                    && (safe->fxSlot < 0 || safe->processor.getInsertEffectType(safe->fxScope, safe->fxSlot) == fire::effects::Type::eq)
                    && safe->presentParameters[static_cast<size_t>(safe->selected)]->load(std::memory_order_relaxed) > 0.5f; },
                processor.treeState.getParameter(id));
            if (! safe) return;
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, id, *pair.first);
            if (! safe) return;
            if (pair.second == fire::eq::Field::type) typeAttachment = std::move(attachment);
            else slopeAttachment = std::move(attachment);
        }
    }
    void visibilityChanged() override
    {
        if (! isShowing()) dismiss();
        else refresh();
    }
    void enablementChanged() override { dismiss(); }
    std::uint64_t interactionGeneration() const noexcept
    { return generation + presenceEpoch.load(std::memory_order_acquire); }
    void parameterValueChanged(int, float) override
    { presenceEpoch.fetch_add(1, std::memory_order_release); }
    void parameterGestureChanged(int, bool) override {}
    juce::String nodeParameterID(int node, fire::eq::Field field) const
    {return fxSlot < 0 ? fire::eq::parameterID(node, field) : fire::core_modules::eqParameterID(fxScope, fxSlot, node, field);}
    int fxScope = -1, fxSlot = -1;
    bool bound = false;
    std::uint64_t bindingGeneration = 0;
    FireAudioProcessor& processor;
    Knobs knobs {};
    std::array<PointButton, capacity> navigation;
    std::array<bool, capacity> present {};
    std::array<std::atomic<float>*, capacity> presentParameters {}, typeParameters {}, bypassParameters {};
    std::array<juce::RangedAudioParameter*, capacity> presentParameterObjects {};
    PrimaryTextButton addButton, removeButton;
    PrimaryToggleButton powerButton;
    ContextAwareComboBox typeMenu, slopeMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment, slopeAttachment;
    int selected = -1, presentedSelection = -2, paintSignature = -1;
    float scale = 1.0f;
    int knobWidth = 0;
    std::uint64_t generation = 0;
    std::atomic<std::uint64_t> presenceEpoch {0};
    std::uint64_t presentedPresenceEpoch = 0;
    juce::Rectangle<int> header, typeLabel, slopeLabel, emptyBounds;
    juce::Rectangle<float> utilityDivider;
};
