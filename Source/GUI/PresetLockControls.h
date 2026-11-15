#pragma once
#include "LookAndFeel.h"
#include "PresetLockGlyph.h"
#include "../PluginProcessor.h"

struct PresetExperienceUiTestAccess;
namespace fire::ui
{
class PresetLockControls final : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit PresetLockControls(FireAudioProcessor& owner) : processor(owner)
    {
        setLookAndFeel(&look);
        search.setTitle("Search lockable parameters"); search.setComponentID("preset_lock_search");
        search.setTextToShowWhenEmpty("Search parameters...", paletteFor(*this).textMuted);
        search.onTextChange = [this] { rebuild(); }; addAndMakeVisible(search);
        lockedOnly.setButtonText("Locked only"); lockedOnly.setComponentID("preset_locks_only");
        lockedOnly.onClick = [this] { rebuild(); }; addAndMakeVisible(lockedOnly);
        clear.setTitle("Unlock all preset parameters"); clear.setComponentID("preset_unlock_all");
        clear.onClick = [safe = juce::Component::SafePointer<PresetLockControls>(this)]
        { auto owner=safe; if (!owner || !owner->isShowing()) return; owner->processor.clearPresetParameterLocks(); if (owner) owner->rebuild(); };
        addAndMakeVisible(clear);
        list.setModel(this); list.setRowHeight(34); list.setTitle("Preset parameter locks");
        list.setComponentID("preset_locks_list"); addAndMakeVisible(list);
        entries = processor.getLockableParameters();
        setSize(600, 460); rebuild(); lookAndFeelChanged();
    }
    ~PresetLockControls() override { stopTimer(); list.setModel(nullptr); setLookAndFeel(nullptr); }
    void visibilityChanged() override { if (isShowing()) { rebuild(); startTimerHz(10); } else stopTimer(); }
    void lookAndFeelChanged() override
    {
        look.setSkin(skinFor(*this)); const auto& p = paletteFor(*this);
        search.setColour(juce::TextEditor::backgroundColourId,p.surface0); search.setColour(juce::TextEditor::textColourId,p.textPrimary);
        search.setColour(juce::TextEditor::outlineColourId,p.hairline); search.setColour(juce::TextEditor::focusedOutlineColourId,p.accent);
        list.setColour(juce::ListBox::backgroundColourId, p.canvas); list.setColour(juce::ListBox::outlineColourId,p.hairline);
        repaint();
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(20); area.removeFromTop(72);
        auto tools = area.removeFromTop(30); clear.setBounds(tools.removeFromRight(86)); tools.removeFromRight(10);
        lockedOnly.setBounds(tools.removeFromRight(108)); tools.removeFromRight(10); search.setBounds(tools);
        area.removeFromTop(14); list.setBounds(area);
    }
    void paint(juce::Graphics& g) override
    {
        drawCanvas(g,getLocalBounds().toFloat(),skinFor(*this)); auto area=getLocalBounds().reduced(20);
        g.setColour(paletteFor(*this).textPrimary); g.setFont(displayFont(19));
        g.drawText("Preset locks / " + juce::String(locks.size()),area.removeFromTop(28),juce::Justification::centredLeft);
        g.setColour(paletteFor(*this).textSecondary); g.setFont(bodyFont(11.5f));
        g.drawFittedText("Keep base values while browsing sounds. Manual edits and automation remain available; modulation follows the loaded preset.",area.removeFromTop(36),juce::Justification::topLeft,2);
    }
private:
    friend struct ::PresetExperienceUiTestAccess;
    struct Row final : juce::Component
    {
        PrimaryToggleButton toggle;
        Row() { addAndMakeVisible(toggle); }
        void resized() override { toggle.setBounds(getLocalBounds().reduced(4,1)); }
    };
    int getNumRows() override { return static_cast<int>(shown.size()); }
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow(int index, bool, juce::Component* previous) override
    {
        auto* row = dynamic_cast<Row*>(previous); if (!row) row = new Row;
        // ListBox reuses visible components after filtering or changes from a
        // different editor. A press belongs to the old parameter, never to the
        // new row placed underneath the same pointer before mouseUp arrives.
        const juce::Component::SafePointer<PresetLockControls> owner(this);
        const juce::Component::SafePointer<Row> liveRow(row);
        row->toggle.dismissPointerGesture();
        if (!owner || !liveRow) return nullptr;
        if (!juce::isPositiveAndBelow(index, static_cast<int>(shown.size()))) return row;
        const auto entry = shown[static_cast<size_t>(index)];
        row->toggle.setButtonText(entry.name); row->toggle.setTitle("Keep " + entry.name + " when loading presets");
        row->toggle.setTooltip("Preserve this base value on preset load. This does not stop manual edits, automation or modulation.");
        row->toggle.setComponentID("presetLock:" + entry.id);
        row->toggle.setToggleState(locks.contains(entry.id), juce::dontSendNotification);
        if (!owner || !liveRow) return nullptr;
        row->toggle.onClick = [safe = juce::Component::SafePointer<PresetLockControls>(this), id = entry.id,
                               button = juce::Component::SafePointer<PrimaryToggleButton>(&row->toggle)]
        {
            if (!safe || !button || !safe->isShowing()) return;
            auto owner=safe; const auto parameter=id; const bool requested=button->getToggleState();
            owner->processor.setPresetParameterLocked(parameter, requested);
            if (owner) owner->rebuild();
        };
        return row;
    }
    void rebuild()
    {
        const juce::Component::SafePointer<PresetLockControls> owner(this);
        locks = processor.getPresetParameterLocks(); shown.clear();
        const auto query = search.getText().trim();
        for (const auto& entry : entries)
            if ((!lockedOnly.getToggleState() || locks.contains(entry.id))
                && (query.isEmpty() || (entry.name + " " + entry.id).containsIgnoreCase(query))) shown.push_back(entry);
        clear.setEnabled(!locks.isEmpty()); if (!owner) return;
        list.updateContent(); if (!owner) return;
        list.repaint(); repaint();
    }
    void timerCallback() override { if (locks != processor.getPresetParameterLocks()) rebuild(); }
    FireAudioProcessor& processor;
    FireLookAndFeel look;
    juce::TextEditor search;
    PrimaryToggleButton lockedOnly;
    PrimaryTextButton clear {"Unlock all"};
    juce::ListBox list;
    juce::StringArray locks;
    std::vector<FireAudioProcessor::LockablePresetParameter> entries, shown;
};
}
