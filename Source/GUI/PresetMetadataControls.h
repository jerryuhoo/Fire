#pragma once
#include "LookAndFeel.h"
#include "../Panels/TopPanel/Preset.h"
#include "../Utility/PresetMetadata.h"

struct PresetExperienceUiTestAccess;
namespace fire::ui
{
class PresetMetadataControls final : public juce::Component
{
public:
    PresetMetadataControls(::state::StatePresets& owner, juce::String presetKey, juce::String presetName)
        : library(owner), key(std::move(presetKey)), name(std::move(presetName))
    {
        setLookAndFeel(&look);
        tags.setTitle("Preset tags"); tags.setComponentID("preset_metadata_tags");
        tags.setTooltip("Separate tags with commas. Quote a tag containing a comma. Up to 16 tags, 32 characters each.");
        tags.setInputRestrictions(2048); tags.setMultiLine(false); addAndMakeVisible(tags);
        description.setTitle("Preset description"); description.setComponentID("preset_metadata_description");
        description.setMultiLine(true); description.setReturnKeyStartsNewLine(true);
        description.setInputRestrictions(preset_metadata::maximumDescriptionLength); addAndMakeVisible(description);
        save.setComponentID("preset_metadata_save"); save.onClick = [this] { saveChanges(); }; addAndMakeVisible(save);
        cancel.setComponentID("preset_metadata_cancel"); cancel.onClick = [this] { if (onClose) onClose(); }; addAndMakeVisible(cancel);
        reset.setComponentID("preset_metadata_reset"); reset.setTooltip("Remove your personal Factory tags and description and show the originals.");
        reset.onClick = [this] { resetChanges(); }; addAndMakeVisible(reset);
        status.setJustificationType(juce::Justification::topLeft); status.setFont(bodyFont(11)); addAndMakeVisible(status);
        load(); setSize(560, 410); lookAndFeelChanged();
    }
    ~PresetMetadataControls() override { setLookAndFeel(nullptr); }
    std::function<void()> onSaved, onClose;
    void lookAndFeelChanged() override
    {
        look.setSkin(skinFor(*this)); const auto& p = paletteFor(*this);
        for (auto* editor : {&tags, &description})
        {
            editor->setColour(juce::TextEditor::backgroundColourId,p.surface0); editor->setColour(juce::TextEditor::textColourId,p.textPrimary);
            editor->setColour(juce::TextEditor::outlineColourId,p.hairline); editor->setColour(juce::TextEditor::focusedOutlineColourId,p.accent);
            editor->setColour(juce::TextEditor::highlightColourId,p.accent.withAlpha(.25f));
        }
        status.setColour(juce::Label::textColourId,p.textSecondary); repaint();
    }
    void resized() override
    {
        auto area=getLocalBounds().reduced(20); area.removeFromTop(72);
        tagsLabel=area.removeFromTop(20); tags.setBounds(area.removeFromTop(32));
        area.removeFromTop(14); descriptionLabel=area.removeFromTop(20);
        auto actions=area.removeFromBottom(32);
        save.setBounds(actions.removeFromRight(72)); actions.removeFromRight(10);
        cancel.setBounds(actions.removeFromRight(72)); reset.setBounds(actions.removeFromLeft(128));
        area.removeFromBottom(10); status.setBounds(area.removeFromBottom(36)); area.removeFromBottom(8); description.setBounds(area);
    }
    void paint(juce::Graphics& g) override
    {
        drawCanvas(g,getLocalBounds().toFloat(),skinFor(*this)); auto area=getLocalBounds().reduced(20);
        g.setFont(displayFont(19)); g.setColour(paletteFor(*this).textPrimary);
        g.drawText(name,area.removeFromTop(28),juce::Justification::centredLeft);
        g.setFont(bodyFont(11.5f)); g.setColour(paletteFor(*this).textSecondary);
        g.drawFittedText(metadata.factory ? "Personal notes for a Factory sound. The original sound and description remain available."
            : "Tags and description travel with this user preset. Editing details does not change the sound.",area.removeFromTop(36),juce::Justification::topLeft,2);
        g.setFont(labelFont(10)); g.setColour(paletteFor(*this).textMuted);
        g.drawText("TAGS / COMMA SEPARATED",tagsLabel,juce::Justification::centredLeft);
        g.drawText("DESCRIPTION",descriptionLabel,juce::Justification::centredLeft);
    }
private:
    friend struct ::PresetExperienceUiTestAccess;
    static juce::String formatTags(const juce::StringArray& values)
    {
        juce::StringArray formatted;
        for (const auto& value : values)
            formatted.add(value.containsAnyOf(",\"") ? "\"" + value.replace("\"", "\"\"") + "\"" : value);
        return formatted.joinIntoString(", ");
    }
    static juce::Result parseTags(const juce::String& text, juce::StringArray& result)
    {
        bool quoted = false; juce::String current;
        auto p = text.getCharPointer();
        while (!p.isEmpty())
        {
            const auto c = p.getAndAdvance();
            if (c == '"')
            {
                if (quoted && *p == '"') { current += '"'; ++p; }
                else quoted = !quoted;
            }
            else if (c == ',' && !quoted) { if (current.trim().isNotEmpty()) result.add(current.trim()); current.clear(); }
            else current += juce::String::charToString(c);
        }
        if (quoted) return juce::Result::fail("Close the quote in the tags field before saving.");
        if (current.trim().isNotEmpty()) result.add(current.trim());
        return juce::Result::ok();
    }
    void showError(const juce::Result& result) { status.setText(result.getErrorMessage(),juce::dontSendNotification); }
    void load()
    {
        const juce::Component::SafePointer<PresetMetadataControls> safe(this);
        auto next=metadata;const auto targetKey=key;
        const auto result = library.readPresetMetadata(targetKey,next);
        if(!safe) return;
        metadata=std::move(next);
        save.setEnabled(result.wasOk());if(!safe) return;
        reset.setVisible(result.wasOk() && metadata.factory);if(!safe) return;
        reset.setEnabled(metadata.hasOverride);if(!safe) return;
        if (result.failed()) { showError(result); return; }
        tags.setText(formatTags(metadata.tags),false);if(!safe) return;
        description.setText(metadata.description,false);if(!safe) return;
        status.setText("Up to 16 tags; 32 characters per tag. Description: 2048 characters.",juce::dontSendNotification);
    }
    void saveChanges()
    {
        if (!isShowing() || !isEnabled() || !save.isEnabled()) return;
        auto edited=metadata; edited.tags.clear();
        auto parsed=parseTags(tags.getText(),edited.tags); if(parsed.failed()) {showError(parsed); return;}
        edited.description=description.getText();
        const juce::Component::SafePointer<PresetMetadataControls> safe(this);
        const auto targetKey=key, expectedRevision=metadata.revision;
        const auto result=library.writePresetMetadata(targetKey,edited,expectedRevision);
        if(!safe) return;
        if(result.failed()) {showError(result); return;}
        auto callback=onSaved; if(callback) callback();
        if(safe) {auto close=safe->onClose;if(close) close();}
    }
    void resetChanges()
    {
        if(!isShowing() || !isEnabled() || !metadata.factory || !metadata.hasOverride) return;
        const juce::Component::SafePointer<PresetMetadataControls> safe(this);
        const auto targetKey=key, expectedRevision=metadata.revision;
        const auto result=library.resetFactoryMetadata(targetKey,expectedRevision);
        if(!safe) return;
        if(result.failed()) {showError(result); return;}
        load();
        if(!safe) return;
        auto callback=onSaved; if(callback) callback();
    }
    ::state::StatePresets& library;
    juce::String key,name;
    ::state::StatePresets::PresetMetadata metadata;
    FireLookAndFeel look;
    juce::TextEditor tags,description;
    juce::Label status;
    PrimaryTextButton save{"Save"},cancel{"Cancel"},reset{"Restore original"};
    juce::Rectangle<int> tagsLabel,descriptionLabel;
};
}
