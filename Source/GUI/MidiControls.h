#pragma once
#include "PerformanceControlHelpers.h"
#include "../Utility/MidiModulationParameters.h"

struct PerformanceControlsTestAccess;
namespace fire::ui
{
class MidiControls final : public juce::Component, private juce::Timer
{
public:
    explicit MidiControls(FireAudioProcessor& owner) : processor(owner), stateRevision(owner.getChainEditRevision())
    {
        setLookAndFeel(&look);
        const juce::Component::SafePointer<MidiControls> safe(this);
        for (int slot = 0; slot < midi_mod::count; ++slot)
        {
            auto& row = rows[static_cast<size_t>(slot)];
            const auto numberID = midi_mod::parameterID(slot, midi_mod::number);
            const auto channelID = midi_mod::parameterID(slot, midi_mod::channel);
            row.number.setComponentID(numberID); row.number.setTitle("MIDI " + juce::String(slot + 1) + " CC number");
            row.number.setTooltip("MIDI CC 0 to 127. Off disables this modulation source. Assign MIDI CC " + juce::String(slot + 1) + " to a destination in the modulation matrix.");
            row.number.bind(*processor.treeState.getParameter(numberID), [safe] { return safe && safe->contextCurrent(); });
            row.number.onBeforeWrite = [safe] { if (safe) safe->cancelLearn(); };
            row.number.textFromValueFunction = [](double value) { return value < 0 ? juce::String("Off") : "CC " + juce::String(juce::roundToInt(value)); };
            row.number.valueFromTextFunction = [](const juce::String& text) { return text.trim().equalsIgnoreCase("Off") ? -1.0 : text.retainCharacters("0123456789").getDoubleValue(); };
            row.number.updateText();
            row.channel.addItem("Omni", 1);
            for (int i = 1; i <= 16; ++i) row.channel.addItem("Channel " + juce::String(i), i + 1);
            row.channel.setComponentID(channelID); row.channel.setTitle("MIDI " + juce::String(slot + 1) + " channel");
            row.channel.setTooltip("Omni accepts this controller on any MIDI channel; otherwise use channel 1 to 16.");
            auto* target = processor.treeState.getParameter(channelID);
            row.channel.configurePopupSession([safe] { return safe ? safe->revision + safe->processor.getChainEditRevision() : 0; },
                [safe] { return safe && safe->contextCurrent(); },
                [safe, target](int id)
                {
                    if (!safe || !safe->contextCurrent()) return;
                    safe->cancelLearn();
                    const auto value = target->convertTo0to1(static_cast<float>(id - 1));
                    if (juce::approximatelyEqual(target->getValue(), value)) return;
                    target->beginChangeGesture(); target->setValueNotifyingHost(value); target->endChangeGesture();
                });
            row.channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.treeState, channelID, row.channel);
            row.learn.setComponentID("midiLearn" + juce::String(slot + 1));
            row.learn.setTitle("Learn MIDI " + juce::String(slot + 1));
            row.learn.setTooltip("Listen for the next MIDI CC and capture its controller number and channel. Click again to cancel. Closing this panel cancels Learn.");
            row.learn.onClick = [safe, slot]
            {
                if (!safe || !safe->isShowing() || !safe->contextCurrent()) return;
                if (safe->learningSlot == slot) { safe->cancelLearn(); return; }
                safe->cancelLearn();
                safe->learnToken = safe->processor.beginMidiLearn(slot);
                safe->learningSlot = safe->learnToken != 0 ? slot : -1;
                safe->refreshLearnButtons();
            };
            addAndMakeVisible(row.number); addAndMakeVisible(row.channel); addAndMakeVisible(row.learn);
        }
        setSize(660, 354); lookAndFeelChanged();
    }
    ~MidiControls() override { stopTimer(); dismiss(); setLookAndFeel(nullptr); }
    void dismiss()
    {
        const juce::Component::SafePointer<MidiControls> safe(this);
        ++revision; cancelLearn();
        for (auto& row : rows)
        {
            row.number.dismissTransientInteraction(); if (!safe) return;
            row.channel.dismissTransientInteraction(); if (!safe) return;
            row.learn.dismissPointerGesture(); if (!safe) return;
        }
    }
    void visibilityChanged() override
    { if (isShowing()) { poll(); startTimerHz(30); } else { stopTimer(); dismiss(); } }
    void enablementChanged() override { if (!isEnabled()) dismiss(); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.isKeyCode(juce::KeyPress::escapeKey)) { cancelLearn(); return false; }
        return performanceUndo(processor, key);
    }
    void lookAndFeelChanged() override
    {
        look.setSkin(skinFor(*this));
        const auto& palette = paletteFor(*this);
        for (auto& row : rows)
        {
            performanceComboColours(row.channel, palette);
            row.number.setColour(juce::Slider::textBoxTextColourId, palette.textPrimary);
            row.number.setColour(juce::Slider::textBoxBackgroundColourId, palette.surface0);
            row.number.setColour(juce::Slider::textBoxOutlineColourId, palette.hairline);
        }
        repaint();
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        area.removeFromTop(72);
        for (auto& row : rows)
        {
            row.area = area.removeFromTop(48);
            auto line = row.area.reduced(0, 7); line.removeFromLeft(62);
            const int gap = 10;
            const int meterWidth = juce::jmax(60, juce::roundToInt(line.getWidth() * .17f));
            const int numberWidth = juce::jmax(116, juce::roundToInt(line.getWidth() * .27f));
            const int channelWidth = juce::jmax(102, juce::roundToInt(line.getWidth() * .24f));
            row.number.setBounds(line.removeFromLeft(numberWidth)); line.removeFromLeft(gap);
            row.channel.setBounds(line.removeFromLeft(channelWidth)); line.removeFromLeft(gap);
            row.meter = line.removeFromRight(meterWidth); line.removeFromRight(gap);
            row.learn.setBounds(line);
        }
        footer = area;
    }
    void paint(juce::Graphics& g) override
    {
        drawCanvas(g, getLocalBounds().toFloat(), skinFor(*this));
        const auto& palette = paletteFor(*this);
        auto title = getLocalBounds().reduced(20);
        g.setColour(palette.textPrimary); g.setFont(displayFont(19));
        g.drawText("MIDI / Four control sources", title.removeFromTop(28), juce::Justification::centredLeft);
        g.setColour(palette.textSecondary); g.setFont(bodyFont(11.5f));
        g.drawText("Route MIDI CC 1-4 in the matrix, then move a controller or use Learn.", title.removeFromTop(24), juce::Justification::centredLeft);
        for (int i = 0; i < midi_mod::count; ++i)
        {
            const auto& row = rows[static_cast<size_t>(i)];
            g.setColour(palette.hairline); g.drawHorizontalLine(row.area.getBottom(), float(row.area.getX()), float(row.area.getRight()));
            g.setColour(palette.textSecondary); g.setFont(labelFont(11));
            g.drawText("MIDI " + juce::String(i + 1), row.area.withWidth(56), juce::Justification::centredLeft);
            auto bar = row.meter.withHeight(4).translated(0, 5).toFloat();
            g.setColour(palette.hairline); g.fillRoundedRectangle(bar, 2);
            g.setColour(palette.accent); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * row.value), 2);
            g.setFont(portfolioMonoFont(10)); g.setColour(palette.textSecondary);
            g.drawText(juce::String(juce::roundToInt(row.value * 127)) + " / 127", row.meter.withTrimmedTop(12), juce::Justification::centredLeft);
        }
        g.setColour(palette.textMuted); g.setFont(bodyFont(10.5f));
        g.drawFittedText(contextCurrent() ? "Route MIDI to Fire's VST3 or CLAP instance. If your AU host cannot send MIDI to effects, use automation."
                                          : "The preset or project state changed. Reopen MIDI controls to edit the current settings.",
                        footer.withTrimmedTop(12), juce::Justification::topLeft, 2);
    }
private:
    friend struct ::PerformanceControlsTestAccess;
    struct Row
    {
        PerformanceIntegerControl number;
        ContextAwareComboBox channel;
        PrimaryTextButton learn {"Learn"};
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;
        juce::Rectangle<int> area, meter;
        float value = 0;
    };
    bool contextCurrent() const { return processor.getChainEditRevision() == stateRevision; }
    void cancelLearn()
    {
        const auto old = learnToken; learnToken = 0; learningSlot = -1;
        if (old) processor.endMidiLearn(old);
        refreshLearnButtons();
    }
    void refreshLearnButtons()
    {
        for (int i = 0; i < midi_mod::count; ++i)
        {
            auto& button = rows[static_cast<size_t>(i)].learn;
            button.setButtonText(i == learningSlot ? "Listening..." : "Learn");
            button.setToggleState(i == learningSlot, juce::dontSendNotification);
        }
    }
    void timerCallback() override { poll(); }
    void poll()
    {
        if (!contextCurrent())
        {
            const juce::Component::SafePointer<MidiControls> safe(this);
            dismiss();
            if (!safe) return;
            for (auto& row : rows)
            {
                row.number.setEnabled(false); if (!safe) return;
                row.channel.setEnabled(false); if (!safe) return;
                row.learn.setEnabled(false); if (!safe) return;
            }
            stopTimer(); repaint(); return;
        }
        if (learnToken)
        {
            const auto token = learnToken;
            const juce::Component::SafePointer<MidiControls> safe(this);
            const bool applied = processor.applyMidiLearn(token);
            if (!safe) return;
            if (applied) stateRevision = processor.getChainEditRevision();
            if (applied || !processor.isMidiLearnActive(token)) cancelLearn();
        }
        for (int i = 0; i < midi_mod::count; ++i)
        {
            auto& row = rows[static_cast<size_t>(i)];
            row.value = juce::jlimit(0.0f, 1.0f, processor.getMidiCCValue(i));
            repaint(row.meter);
        }
    }
    FireAudioProcessor& processor;
    std::uint64_t stateRevision, revision = 0, learnToken = 0;
    int learningSlot = -1;
    FireLookAndFeel look;
    std::array<Row, midi_mod::count> rows;
    juce::Rectangle<int> footer;
};
}
