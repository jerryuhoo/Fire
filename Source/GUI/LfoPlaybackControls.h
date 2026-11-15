#pragma once
#include "PerformanceControlHelpers.h"
#include "../Utility/LfoPlaybackParameters.h"

struct PerformanceControlsTestAccess;
namespace fire::ui
{
class LfoPlaybackControls final : public juce::Component, private juce::Timer
{
public:
    LfoPlaybackControls(FireAudioProcessor& owner, int selected,
                        std::function<bool()> ownerValid = {})
        : processor(owner), slot(selected), slotGeneration(owner.getLfoSlotGeneration(selected)),
          validOwner(std::move(ownerValid))
    {
        setLookAndFeel(&look);
        const juce::Component::SafePointer<LfoPlaybackControls> safe(this);
        for (const auto& name : lfo_playback::playModeNames()) playback.addItem(name, playback.getNumItems() + 1);
        for (const auto& name : lfo_playback::waveModeNames()) wave.addItem(name, wave.getNumItems() + 1);
        channel.addItem("Omni", 1);
        for (int i = 1; i <= 16; ++i) channel.addItem("Channel " + juce::String(i), i + 1);
        const auto bindChoice = [&](ContextAwareComboBox& box, int field, const char* title, const char* help)
        {
            const auto id = lfo_playback::parameterID(slot, field);
            box.setComponentID(id); box.setTitle(title); box.setTooltip(help); box.setHelpText(help);
            box.configurePopupSession([safe] { return safe ? safe->epoch() : 0; },
                [safe] { return safe && safe->contextCurrent(); }, processor.treeState.getParameter(id));
            addAndMakeVisible(box);
            combos.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, id, box));
        };
        bindChoice(playback, lfo_playback::playMode, "LFO playback mode",
            "Loop runs continuously. Retrigger restarts on a matching MIDI Note On or Trigger and then loops. "
            "One-shot plays one full cycle per trigger and holds its final value.");
        bindChoice(wave, lfo_playback::waveMode, "LFO waveform mode",
            "Shape uses the drawn curve. Random Hold creates stepped values; Random Glide interpolates between values. "
            "Shape and Smooth only affect Shape output. Random modes use the Seed and trigger sequence, independently of the drawn curve.");
        bindChoice(channel, lfo_playback::triggerChannel, "LFO trigger MIDI channel", "Omni accepts matching Note On messages on all MIDI channels.");
        const auto bindNumber = [&](PerformanceIntegerControl& control, int field, const char* title, const char* help)
        {
            const auto id = lfo_playback::parameterID(slot, field);
            control.setComponentID(id); control.setTitle(title); control.setTooltip(help);
            control.bind(*processor.treeState.getParameter(id), [safe] { return safe && safe->contextCurrent(); });
            addAndMakeVisible(control);
        };
        bindNumber(seed, lfo_playback::seed, "Random seed", "Seeds 0 to 65535 reproduce a random sequence. Each trigger advances to a new value; the same seed and trigger sequence reproduce it.");
        bindNumber(note, lfo_playback::triggerNote, "LFO trigger note", "Any accepts all MIDI Note On messages; otherwise match the MIDI note number 0 to 127. Note Off does not trigger playback.");
        note.textFromValueFunction = [](double value) { return value < 0 ? juce::String("Any") : "Note " + juce::String(juce::roundToInt(value)); };
        note.valueFromTextFunction = [](const juce::String& text) { return text.trim().equalsIgnoreCase("Any") ? -1.0 : text.retainCharacters("0123456789").getDoubleValue(); };
        note.updateText();
        trigger.setComponentID("lfo_manual_trigger"); trigger.setTitle("Trigger LFO " + juce::String(slot + 1));
        trigger.setTooltip("Start this LFO now. Available in Retrigger and One-shot modes. This gesture is not saved in presets.");
        trigger.onClick = [safe]
        {
            if (!safe || !safe->isShowing() || !safe->contextCurrent() || safe->value(lfo_playback::playMode) == lfo_playback::loop) return;
            safe->processor.triggerLfo(safe->slot, safe->slotGeneration);
        };
        addAndMakeVisible(trigger);
        for (int i = 0; i < 5; ++i)
        {
            labels[static_cast<size_t>(i)].setText(juce::StringArray{"PLAYBACK", "WAVE", "SEED", "TRIGGER NOTE", "CHANNEL"}[i], juce::dontSendNotification);
            labels[static_cast<size_t>(i)].setFont(labelFont(10));
            addAndMakeVisible(labels[static_cast<size_t>(i)]);
        }
        noteText.setJustificationType(juce::Justification::topLeft);
        noteText.setFont(bodyFont(12)); addAndMakeVisible(noteText);
        setSize(520, 310); refresh(); lookAndFeelChanged();
    }
    ~LfoPlaybackControls() override { stopTimer(); dismiss(); setLookAndFeel(nullptr); }
    void dismiss()
    {
        const juce::Component::SafePointer<LfoPlaybackControls> safe(this);
        ++revision;
        for (auto* combo : {&playback, &wave, &channel}) { combo->dismissTransientInteraction(); if (!safe) return; }
        seed.dismissTransientInteraction(); if (!safe) return;
        note.dismissTransientInteraction(); if (!safe) return;
        trigger.dismissPointerGesture();
    }
    void visibilityChanged() override
    { if (isShowing()) { refresh(); startTimerHz(30); } else { stopTimer(); dismiss(); } }
    void enablementChanged() override { if (!isEnabled()) dismiss(); }
    void lookAndFeelChanged() override
    {
        look.setSkin(skinFor(*this));
        const auto& palette = paletteFor(*this);
        for (auto* combo : {&playback, &wave, &channel}) performanceComboColours(*combo, palette);
        for (auto& label : labels) label.setColour(juce::Label::textColourId, palette.textMuted);
        noteText.setColour(juce::Label::textColourId, palette.textSecondary);
        for (auto* control : {&seed, &note})
        {
            control->setColour(juce::Slider::textBoxTextColourId, palette.textPrimary);
            control->setColour(juce::Slider::textBoxBackgroundColourId, palette.surface0);
            control->setColour(juce::Slider::textBoxOutlineColourId, palette.hairline);
        }
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override { return performanceUndo(processor, key); }
    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        area.removeFromTop(46);
        const int gap = 12;
        auto upper = area.removeFromTop(56);
        auto left = upper.removeFromLeft((upper.getWidth() - gap) / 2); upper.removeFromLeft(gap);
        labels[0].setBounds(left.removeFromTop(20)); playback.setBounds(left);
        labels[1].setBounds(upper.removeFromTop(20)); wave.setBounds(upper);
        area.removeFromTop(14);
        auto lower = area.removeFromTop(56);
        const int width = (lower.getWidth() - gap * 2) / 3;
        for (int i = 0; i < 3; ++i)
        {
            auto cell = i < 2 ? lower.removeFromLeft(width) : lower;
            labels[static_cast<size_t>(i + 2)].setBounds(cell.removeFromTop(20));
            if (i == 0) seed.setBounds(cell);
            else if (i == 1) note.setBounds(cell);
            else channel.setBounds(cell);
            lower.removeFromLeft(gap);
        }
        area.removeFromTop(18);
        trigger.setBounds(area.removeFromRight(88).withHeight(34));
        area.removeFromRight(16); noteText.setBounds(area);
    }
    void paint(juce::Graphics& g) override
    {
        drawCanvas(g, getLocalBounds().toFloat(), skinFor(*this));
        const auto& palette = paletteFor(*this);
        g.setColour(palette.textPrimary); g.setFont(displayFont(19));
        g.drawText("LFO " + juce::String(slot + 1) + " / Playback", getLocalBounds().reduced(20).withHeight(28), juce::Justification::centredLeft);
    }
private:
    friend struct ::PerformanceControlsTestAccess;
    std::uint64_t epoch() const { return revision + processor.getLfoSlotGeneration(slot); }
    bool contextCurrent() const
    { return processor.isLfoPresent(slot) && processor.getLfoSlotGeneration(slot) == slotGeneration && (!validOwner || validOwner()); }
    int value(int field) const { return juce::roundToInt(processor.treeState.getRawParameterValue(lfo_playback::parameterID(slot, field))->load()); }
    void timerCallback() override { refresh(); }
    void refresh()
    {
        const juce::Component::SafePointer<LfoPlaybackControls> safe(this);
        if (!contextCurrent())
        {
            dismiss();
            if (!safe) return;
            for (auto* component : std::array<juce::Component*, 6>{&playback, &wave, &channel, &seed, &note, &trigger}) { component->setEnabled(false); if (!safe) return; }
            noteText.setText("This LFO has changed. Reopen Playback to edit its current settings.", juce::dontSendNotification);
            stopTimer(); return;
        }
        const bool random = value(lfo_playback::waveMode) != lfo_playback::shape;
        const bool triggered = value(lfo_playback::playMode) != lfo_playback::loop;
        seed.setEnabled(random); if (!safe) return;
        note.setEnabled(triggered); if (!safe) return;
        channel.setEnabled(triggered); if (!safe) return;
        trigger.setEnabled(triggered); if (!safe) return;
        const auto help = random
            ? "Shape/Smooth affect Shape output only. Random One-shot ignores Phase: Hold keeps one value; Glide completes one transition."
            : "Shape One-shot reads from Phase and always plays a complete cycle.";
        noteText.setText(help, juce::dontSendNotification);
    }
    FireAudioProcessor& processor;
    int slot;
    std::uint32_t slotGeneration;
    std::function<bool()> validOwner;
    std::uint64_t revision = 0;
    FireLookAndFeel look;
    ContextAwareComboBox playback, wave, channel;
    PerformanceIntegerControl seed, note;
    PrimaryTextButton trigger {"Trigger"};
    std::array<juce::Label, 5> labels;
    juce::Label noteText;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> combos;
};
}
