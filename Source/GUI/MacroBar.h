#pragma once
#include "PrimarySlider.h"
#include "../PluginProcessor.h"

namespace fire::ui
{
class MacroBar final : public juce::Component, private juce::Timer
{
    class CompactLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        float scale = 1.0f;
        FireLookAndFeel renderer;
        void setSkin(Skin skin) { renderer.setSkin(skin); }
        juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
        {
            juce::Slider::SliderLayout layout;
            layout.sliderBounds = slider.getLocalBounds();
            return layout;
        }
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float value, float start, float end, juce::Slider& slider) override
        {
            // The ordinary dial reserves a wide halo. A toolbar dial needs
            // the same artwork with a two-pixel inset and its full hit area.
            const auto inset = juce::roundToInt(juce::jmax(5.0f, 7.0f * scale) - 2.0f * scale);
            renderer.scale = scale;
            renderer.drawRotarySlider(g, x - inset, y - inset, width + 2 * inset,
                                              height + 2 * inset, value, start, end, slider);
        }
    };
public:
    explicit MacroBar(FireAudioProcessor& owner) : processor(owner)
    {
        setComponentID("performance_macros");
        setTitle("Performance macros");
        const juce::Component::SafePointer<MacroBar> safe(this);
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto& slider = sliders[i];
            slider.setLookAndFeel(&lookAndFeel);
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setComponentID(mod_sources::ids[i + 3]);
            slider.setDoubleClickReturnValue(true, 0);
            addAndMakeVisible(slider);
            attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, mod_sources::ids[i + 3], slider);
            auto& label = names[i];
            label.setComponentID("macro_name_" + juce::String(static_cast<int>(i) + 1));
            label.setEditable(false, true, false);
            label.setJustificationType(juce::Justification::centredLeft);
            label.onEditorShow = [safe, i]
            {
                if (! safe) return;
                safe->editRevisions[i] = safe->processor.getMacroNamesRevision();
                if (auto* editor = safe->names[i].getCurrentTextEditor())
                    editor->setInputRestrictions(macro_names::maximumLength);
            };
            label.onTextChange = [safe, i]
            {
                if (! safe || safe->editRevisions[i] != safe->processor.getMacroNamesRevision()) return;
                const auto text = safe->names[i].getText();
                safe->processor.setMacroDisplayName(static_cast<int>(i), text, safe->editRevisions[i]);
            };
            addAndMakeVisible(label);
            readouts[i].setJustificationType(juce::Justification::centredLeft);
            readouts[i].setInterceptsMouseClicks(false, false);
            addAndMakeVisible(readouts[i]);
        }
        lookAndFeelChanged();
        refresh();
    }
    ~MacroBar() override { stopTimer(); dismissEditing(); for (auto& slider : sliders) slider.setLookAndFeel(nullptr); }
    void resized() override
    {
        auto area = getLocalBounds();
        const auto cellWidth = area.getWidth() / mod_sources::macroCount;
        const auto scale = juce::jmax(0.5f, getHeight() / 30.0f);
        lookAndFeel.scale = scale;
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto cell = area.removeFromLeft(i + 1 == sliders.size() ? area.getWidth() : cellWidth).reduced(2, 0);
            sliders[i].setBounds(cell.removeFromLeft(juce::jmin(cell.getWidth() / 2, getHeight())));
            names[i].setBounds(cell.removeFromTop(juce::roundToInt(getHeight() * .56f)));
            readouts[i].setBounds(cell);
            names[i].setFont(isLineSkin(*this) ? portfolioFont(juce::jmin(18.0f, 11.5f * scale), true) : labelFont(11.5f * scale));
            readouts[i].setFont(isLineSkin(*this) ? portfolioMonoFont(juce::jmin(16.0f, 10.0f * scale)) : valueFont(10.0f * scale));
        }
    }
    void lookAndFeelChanged() override
    {
        lookAndFeel.setSkin(skinFor(*this));
        const auto& palette = paletteFor(*this);
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            names[i].setColour(juce::Label::textColourId, palette.textPrimary);
            names[i].setColour(juce::Label::textWhenEditingColourId, palette.textPrimary);
            names[i].setColour(juce::Label::backgroundWhenEditingColourId, palette.surface1);
            readouts[i].setColour(juce::Label::textColourId, palette.textSecondary);
            sliders[i].setColour(juce::Slider::rotarySliderFillColourId, isLineSkin(*this) ? palette.accent : modulationSourceColour(mod_sources::firstMacro + static_cast<int>(i)));
        }
        resized();
    }
    void visibilityChanged() override
    {
        if (isShowing()) { refresh(); startTimerHz(15); }
        else { stopTimer(); dismissEditing(); }
    }
    void parentHierarchyChanged() override { lookAndFeelChanged(); visibilityChanged(); }
    void dismissEditing()
    {
        const juce::Component::SafePointer<MacroBar> safe(this);
        for (size_t i = 0; i < names.size(); ++i)
        {
            names[i].hideEditor(true);
            if (! safe) return;
            sliders[i].dismissTransientInteraction();
            if (! safe) return;
        }
    }
    void refresh()
    {
        const juce::Component::SafePointer<MacroBar> safe(this);
        const auto revision = processor.getMacroNamesRevision();
        const auto labels = processor.getMacroDisplayNames();
        for (size_t i = 0; i < names.size(); ++i)
        {
            if (names[i].isBeingEdited() && editRevisions[i] != revision) names[i].hideEditor(true);
            if (! safe) return;
            if (! names[i].isBeingEdited()) names[i].setText(labels[i], juce::dontSendNotification);
            const auto identity = "Macro " + juce::String(static_cast<int>(i) + 1);
            names[i].setTitle(identity + " name");
            names[i].setTooltip(identity + ": " + labels[i] + " — double-click the name to rename for this preset.");
            sliders[i].setTitle(identity + ": " + labels[i]);
            sliders[i].setTooltip(identity + ": " + labels[i] + ". Assign destinations in Mod Forge → Matrix.");
            readouts[i].setText(identity.substring(0, 1) + juce::String(static_cast<int>(i) + 1) + "  "
                + juce::String(juce::roundToInt(sliders[i].getValue() * 100.0)) + "%", juce::dontSendNotification);
        }
    }
private:
    void timerCallback() override { refresh(); }
    FireAudioProcessor& processor;
    CompactLookAndFeel lookAndFeel;
    std::array<PrimarySlider, mod_sources::macroCount> sliders;
    std::array<juce::Label, mod_sources::macroCount> names, readouts;
    std::array<std::uint64_t, mod_sources::macroCount> editRevisions {};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, mod_sources::macroCount> attachments;
};
}
