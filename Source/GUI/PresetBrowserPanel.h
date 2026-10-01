#pragma once

#include "PrimaryButton.h"
#include "FireTheme.h"
#include "../Panels/TopPanel/Preset.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace fire::ui
{
class PresetBrowserPanel final : public juce::Component
{
public:
    explicit PresetBrowserPanel(::state::StatePresets& library) : presets(library)
    {
        setOpaque(true); setTitle("Sound library"); setWantsKeyboardFocus(true);
        addAndMakeVisible(closeButton); closeButton.setButtonText("Back to Reactor"); closeButton.setTitle("Close preset browser");
        closeButton.onClick = [safe = juce::Component::SafePointer<PresetBrowserPanel>(this)] {if (safe && safe->onClose) safe->onClose();};
        addAndMakeVisible(search);
        search.setTextToShowWhenEmpty("Search sounds, colour or space...", colours::textMuted);
        search.setColour(juce::TextEditor::backgroundColourId, colours::surface1);
        search.setColour(juce::TextEditor::textColourId, colours::textPrimary);
        search.setColour(juce::TextEditor::outlineColourId, colours::hairline);
        search.setColour(juce::TextEditor::focusedOutlineColourId, colours::gold.withAlpha(.5f));
        search.setTitle("Search presets"); search.setSelectAllWhenFocused(true);
        search.onTextChange = [safe = juce::Component::SafePointer<PresetBrowserPanel>(this)] {if (safe) safe->rebuildRows();};
        viewport.setViewedComponent(&content, false); viewport.setScrollBarsShown(true, false);
        viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, colours::textMuted.withAlpha(.3f));
        addAndMakeVisible(viewport);
        sidebar.setViewedComponent(&categoriesContent, false); sidebar.setScrollBarsShown(true, false);
        sidebar.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, colours::textMuted.withAlpha(.2f));
        addAndMakeVisible(sidebar);
    }
    ~PresetBrowserPanel() override {viewport.setViewedComponent(nullptr, false); sidebar.setViewedComponent(nullptr, false);}
    std::function<void(const juce::String&)> onPresetSelected;
    std::function<void()> onClose;
    void open()
    {
        entries = presets.getBrowserEntries(); selectedKey = presets.getCurrentPresetKey();
        category = "All Sounds"; search.setText({}, false);
        std::vector<juce::String> names {"All Sounds"};
        for (const auto& entry : entries) if (entry.factory && std::find(names.begin(), names.end(), entry.category) == names.end()) names.push_back(entry.category);
        std::sort(names.begin() + 1, names.end()); names.push_back("User");
        categoryButtons.clear();
        const juce::Component::SafePointer<PresetBrowserPanel> safe(this);
        for (const auto& name : names)
        {
            auto button = std::make_unique<PrimaryTextButton>();
            int count = 0; for (const auto& entry : entries) if (matchesCategory(entry, name)) ++count;
            button->setButtonText(name + "   " + juce::String(count));
            button->setTitle("Preset category " + name); button->setComponentID("presetCategory:" + name);
            button->getProperties().set("fireModuleRail", true);
            button->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            button->setColour(juce::TextButton::buttonOnColourId, colours::gold.withAlpha(.10f));
            button->setColour(juce::TextButton::textColourOffId, colours::textSecondary);
            button->setColour(juce::TextButton::textColourOnId, colours::gold);
            button->onClick = [safe, name]
            {
                if (!safe || !safe->isShowing() || !safe->isEnabled()) return;
                safe->category = name; safe->rebuildRows(); safe->refreshCategorySelection();
            };
            categoriesContent.addAndMakeVisible(*button); categoryButtons.push_back(std::move(button));
        }
        rebuildRows(); refreshCategorySelection(); resized();
    }
    void refreshSelection()
    {
        const auto current = presets.getCurrentPresetKey();
        if (current == selectedKey) return;
        selectedKey = current;
        for (auto& row : rows) {row->selected = row->entry.key == selectedKey; row->repaint();}
        repaint(footer.toNearestInt());
    }
    juce::String getSelectedCategory() const {return category;}
    int getVisiblePresetCount() const noexcept {return static_cast<int>(rows.size());}
    void selectCategory(const juce::String& name) {category = name; rebuildRows(); refreshCategorySelection();}
    void setSearchText(const juce::String& text) {search.setText(text, false); rebuildRows();}
    void resized() override
    {
        scale = juce::jlimit(.8f, 2.2f, static_cast<float>(getWidth()) / 1000.0f);
        auto area = getLocalBounds().toFloat().reduced(24 * scale, 18 * scale);
        header = area.removeFromTop(67 * scale);
        closeButton.setBounds(header.removeFromRight(154 * scale).withTrimmedTop(9 * scale).withHeight(34 * scale).toNearestInt());
        auto searchArea = header.removeFromRight(juce::jmin(285 * scale, header.getWidth() * .46f));
        search.setBounds(searchArea.withTrimmedTop(9 * scale).withHeight(34 * scale).toNearestInt()); search.setFont(bodyFont(12 * scale));
        headerTitle = header;
        area.removeFromTop(10 * scale);
        auto left = area.removeFromLeft(185 * scale); area.removeFromLeft(24 * scale);
        categoryTitle = left.removeFromTop(27 * scale);
        sidebar.setBounds(left.toNearestInt()); sidebar.setScrollBarThickness(juce::roundToInt(4 * scale));
        const auto rowHeight = juce::roundToInt(32 * scale);
        categoriesContent.setSize(sidebar.getWidth() - juce::roundToInt(5 * scale), juce::jmax(sidebar.getHeight(), rowHeight * static_cast<int>(categoryButtons.size())));
        for (size_t i = 0; i < categoryButtons.size(); ++i) categoryButtons[i]->setBounds(0, static_cast<int>(i) * rowHeight, categoriesContent.getWidth(), rowHeight - juce::roundToInt(2 * scale));
        footer = area.removeFromBottom(69 * scale); area.removeFromBottom(15 * scale);
        listTitle = area.removeFromTop(27 * scale);
        viewport.setBounds(area.toNearestInt()); viewport.setScrollBarThickness(juce::roundToInt(4 * scale));
        const auto height = juce::roundToInt(58 * scale);
        content.setSize(viewport.getWidth() - juce::roundToInt(7 * scale), juce::jmax(viewport.getHeight(), height * static_cast<int>(rows.size())));
        for (size_t i = 0; i < rows.size(); ++i) {rows[i]->scale = scale; rows[i]->setBounds(0, static_cast<int>(i) * height, content.getWidth(), height - juce::roundToInt(5 * scale));}
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1118));
        g.setGradientFill(juce::ColourGradient(colours::gold.withAlpha(.06f), 0, 0, juce::Colours::transparentBlack, static_cast<float>(getWidth()), static_cast<float>(getHeight()), false));
        g.fillAll();
        g.setColour(colours::gold); g.setFont(labelFont(11 * scale));
        auto titleArea = headerTitle;
        g.drawText("FIRE  /  SOUND LIBRARY", titleArea.removeFromTop(20 * scale), juce::Justification::centredLeft);
        g.setColour(colours::textPrimary); g.setFont(bodyFont(25 * scale));
        g.drawText("Find your next colour.", titleArea, juce::Justification::centredLeft);
        g.setColour(colours::textMuted); g.setFont(labelFont(10 * scale));
        g.drawText("COLLECTIONS", categoryTitle, juce::Justification::centredLeft);
        g.drawText(category.toUpperCase() + "  /  " + juce::String(rows.size()) + (rows.size() == 1 ? " SOUND" : " SOUNDS"), listTitle, juce::Justification::centredLeft);
        g.setColour(colours::hairline); g.drawLine(footer.getX(), footer.getY(), footer.getRight(), footer.getY(), 1);
        const ::state::StatePresets::BrowserEntry* selected = nullptr;
        for (const auto& entry : entries) if (entry.key == selectedKey) {selected = &entry; break;}
        auto detail = footer.withTrimmedTop(10 * scale);
        g.setColour(colours::gold); g.setFont(labelFont(12 * scale));
        g.drawText(selected ? selected->name : "Click a preset to audition", detail.removeFromTop(20 * scale), juce::Justification::centredLeft);
        g.setColour(colours::textSecondary); g.setFont(bodyFont(10.5f * scale));
        g.drawFittedText(selected ? selected->description : "Choose a collection on the left. A single click loads the sound; double-click returns to the reactor.", detail.toNearestInt(), juce::Justification::centredLeft, 2);
        if (rows.empty())
        {
            g.setColour(colours::textSecondary); g.setFont(bodyFont(15 * scale));
            g.drawText("No sounds match this search", viewport.getBounds().toFloat(), juce::Justification::centred);
        }
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.isKeyCode(juce::KeyPress::escapeKey)) {if (onClose) onClose(); return true;}
        if (search.hasKeyboardFocus(true) || rows.empty()) return false;
        if (key.isKeyCode(juce::KeyPress::upKey) || key.isKeyCode(juce::KeyPress::downKey))
        {
            int current = -1; for (size_t i = 0; i < rows.size(); ++i) if (rows[i]->entry.key == selectedKey) current = static_cast<int>(i);
            const int next = juce::jlimit(0, static_cast<int>(rows.size()) - 1, current + (key.isKeyCode(juce::KeyPress::downKey) ? 1 : -1));
            const auto tag = rows[static_cast<size_t>(next)]->entry.tag;
            viewport.setViewPosition(0, rows[static_cast<size_t>(next)]->getY());
            if (onPresetSelected) onPresetSelected(tag); return true;
        }
        return false;
    }
private:
    class Row final : public PrimaryTextButton
    {
    public:
        ::state::StatePresets::BrowserEntry entry;
        bool selected = false;
        float scale = 1;
        std::function<void()> onDoubleClick;
        void mouseDoubleClick(const juce::MouseEvent& event) override
        {if (event.mods.isLeftButtonDown() && isShowing() && isEnabled() && onDoubleClick) onDoubleClick();}
        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().reduced(.5f);
            const auto background = selected ? juce::Colour(0xff272521) : isMouseOver() ? juce::Colour(0xff1e2530) : juce::Colour(0xff141a23);
            g.setColour(background); g.fillRoundedRectangle(area, 6 * scale);
            g.setColour(selected ? colours::gold.withAlpha(.45f) : colours::hairline.withAlpha(.45f)); g.drawRoundedRectangle(area, 6 * scale, 1);
            if (selected) {g.setColour(colours::gold); g.fillRoundedRectangle(0, area.getY() + 6 * scale, 3 * scale, area.getHeight() - 12 * scale, scale);}
            auto label = area.reduced(17 * scale, 7 * scale);
            auto badge = label.removeFromRight(100 * scale);
            g.setColour(selected ? colours::gold : colours::textPrimary); g.setFont(bodyFont(14 * scale));
            g.drawText(entry.name, label.removeFromTop(21 * scale), juce::Justification::centredLeft, true);
            g.setColour(colours::textMuted); g.setFont(bodyFont(9.5f * scale));
            g.drawText(entry.description.isEmpty() ? "User sound" : entry.description, label, juce::Justification::centredLeft, true);
            g.setColour(colours::textSecondary); g.setFont(labelFont(9 * scale));
            g.drawText(entry.factory ? entry.category.toUpperCase() : "USER", badge, juce::Justification::centredRight);
            if (hasKeyboardFocus(false)) {g.setColour(colours::gold); g.drawRoundedRectangle(area.reduced(2), 5 * scale, 1);}
        }
    };
    static bool matchesCategory(const ::state::StatePresets::BrowserEntry& entry, const juce::String& group)
    {return group == "All Sounds" || (group == "User" ? !entry.factory : entry.category == group);}
    void refreshCategorySelection()
    {
        for (auto& button : categoryButtons) button->setToggleState(button->getComponentID() == "presetCategory:" + category, juce::dontSendNotification);
        repaint();
    }
    void rebuildRows()
    {
        ++generation; rows.clear();
        const auto query = search.getText().trim();
        const auto epoch = generation;
        const juce::Component::SafePointer<PresetBrowserPanel> safe(this);
        for (const auto& entry : entries)
        {
            if (!matchesCategory(entry, category)) continue;
            if (query.isNotEmpty() && !(entry.name + " " + entry.description + " " + entry.category).containsIgnoreCase(query)) continue;
            auto row = std::make_unique<Row>(); row->entry = entry; row->selected = entry.key == selectedKey;
            row->setTitle("Load preset " + entry.name); row->setTooltip(entry.description); row->setComponentID("presetSound:" + entry.key);
            row->onClick = [safe, epoch, tag = entry.tag]
            {if (safe && safe->generation == epoch && safe->isShowing() && safe->isEnabled() && safe->onPresetSelected) safe->onPresetSelected(tag);};
            row->onDoubleClick = [safe, epoch]
            {if (safe && safe->generation == epoch && safe->isShowing() && safe->isEnabled() && safe->onClose) safe->onClose();};
            content.addAndMakeVisible(*row); rows.push_back(std::move(row));
        }
        viewport.setViewPosition(0, 0); resized(); repaint();
    }
    ::state::StatePresets& presets;
    juce::Viewport viewport, sidebar;
    juce::Component content, categoriesContent;
    juce::TextEditor search;
    PrimaryTextButton closeButton;
    std::vector<::state::StatePresets::BrowserEntry> entries;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<std::unique_ptr<PrimaryTextButton>> categoryButtons;
    juce::String category {"All Sounds"}, selectedKey;
    juce::Rectangle<float> header, headerTitle, categoryTitle, listTitle, footer;
    float scale = 1;
    std::uint64_t generation = 0;
};
}
