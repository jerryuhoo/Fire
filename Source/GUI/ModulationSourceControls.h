#pragma once
#include "PrimarySlider.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

namespace fire::ui
{
class ModulationSourceControls final : public juce::Component, private juce::Timer
{
public:
    explicit ModulationSourceControls(FireAudioProcessor& owner) : processor(owner)
    {
        setLookAndFeel(&lookAndFeel); setOpaque(true); setSize(620, 340);
        for (size_t index = 0; index < sliders.size(); ++index)
        {
            auto& slider = sliders[index]; auto& label = labels[index];
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 22);
            slider.setComponentID(mod_sources::ids[index]);
            const auto title = index == 0 ? juce::String("Attack") : index == 1 ? juce::String("Release")
                : index == 2 ? juce::String("Sensitivity") : "Macro " + juce::String(static_cast<int>(index) - 2);
            slider.setTitle(title); label.setText(title, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setColour(juce::Label::textColourId, colours::textSecondary);
            label.setFont(bodyFont(12));
            slider.setColour(juce::Slider::rotarySliderFillColourId,
                modulationSourceColour(index < 3 ? mod_sources::envelope : mod_sources::firstMacro + static_cast<int>(index) - 3));
            slider.setColour(juce::Slider::textBoxTextColourId, colours::textPrimary);
            slider.setColour(juce::Slider::textBoxBackgroundColourId, colours::surface0);
            slider.setColour(juce::Slider::textBoxOutlineColourId, colours::hairline);
            slider.setTooltip(index < 3 ? "Shape the input envelope before using Envelope as a matrix source."
                : "Assign this macro to one or more destinations in the modulation matrix. Automate it in the DAW.");
            addAndMakeVisible(label); addAndMakeVisible(slider);
            attachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, mod_sources::ids[index], slider);
            slider.textFromValueFunction = [index](double value)
            { return juce::String(index >= 3 ? value * 100.0 : value, 1)
                + (index < 2 ? " ms" : index == 2 ? " dB" : " %"); };
            slider.valueFromTextFunction = [index](const juce::String& text)
            { return text.getDoubleValue() * (index >= 3 ? 0.01 : 1.0); };
            slider.updateText();
            slider.setDoubleClickReturnValue(true, mod_sources::defaults[index]);
        }
    }
    ~ModulationSourceControls() override { stopTimer(); setLookAndFeel(nullptr); }
    void visibilityChanged() override
    {
        if (isShowing()) startTimerHz(30);
        else { stopTimer(); for (auto& slider : sliders) slider.dismissTransientInteraction(); }
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        auto upper = area.removeFromTop((area.getHeight() - 12) / 2); area.removeFromTop(12);
        envelopeArea = upper; macrosArea = area;
        upper.removeFromTop(30); area.removeFromTop(30);
        const int width = upper.getWidth() / 4;
        for (int index = 0; index < 3; ++index)
        {
            auto cell = upper.removeFromLeft(width);
            labels[static_cast<size_t>(index)].setBounds(cell.removeFromTop(20));
            sliders[static_cast<size_t>(index)].setBounds(cell.withSizeKeepingCentre(104, juce::jmin(98, cell.getHeight())));
        }
        meterArea = upper.reduced(12, 20);
        const int macroWidth = area.getWidth() / 4;
        for (int index = 3; index < 7; ++index)
        {
            auto cell = area.removeFromLeft(macroWidth);
            labels[static_cast<size_t>(index)].setBounds(cell.removeFromTop(20));
            sliders[static_cast<size_t>(index)].setBounds(cell.withSizeKeepingCentre(104, juce::jmin(98, cell.getHeight())));
        }
    }
    void paint(juce::Graphics& g) override
    {
        drawCanvas(g, getLocalBounds().toFloat());
        drawPanel(g, envelopeArea.toFloat(), colours::flame, false);
        drawPanel(g, macrosArea.toFloat(), colours::modulation, false);
        g.setFont(labelFont(12)); g.setColour(colours::textPrimary);
        g.drawText("INPUT ENVELOPE", envelopeArea.reduced(12, 0).removeFromTop(28), juce::Justification::centredLeft);
        g.drawText("MACROS  /  ASSIGN IN MATRIX", macrosArea.reduced(12, 0).removeFromTop(28), juce::Justification::centredLeft);
        auto bar = meterArea.withHeight(12).withY(meterArea.getCentreY() - 6).toFloat();
        g.setColour(colours::surface0); g.fillRoundedRectangle(bar, 4);
        g.setColour(colours::flame); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * level), 4);
        g.setFont(bodyFont(11)); g.setColour(colours::textSecondary);
        g.drawText(juce::String(juce::roundToInt(level * 100)) + " %", meterArea.withTrimmedTop(meterArea.getHeight() / 2 + 10), juce::Justification::centred);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (!key.getModifiers().isCommandDown() && !key.getModifiers().isCtrlDown()) return false;
        if (juce::CharacterFunctions::toLowerCase(key.getTextCharacter()) == 'z')
            return key.getModifiers().isShiftDown() ? processor.redoEdit() : processor.undoEdit();
        if (juce::CharacterFunctions::toLowerCase(key.getTextCharacter()) == 'y') return processor.redoEdit();
        return false;
    }
private:
    void timerCallback() override
    { level = processor.getLfoManager().getLfoOutput(mod_sources::envelope); repaint(meterArea); }
    FireAudioProcessor& processor;
    FireLookAndFeel lookAndFeel;
    std::array<PrimarySlider, 7> sliders;
    std::array<juce::Label, 7> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> attachments;
    juce::Rectangle<int> envelopeArea, macrosArea, meterArea;
    float level = 0;
};
}
