#pragma once

#include "PrimaryButton.h"
#include "FireTheme.h"
#include "../Panels/TopPanel/Preset.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace fire::ui
{
class PresetBrowserPanel final : public juce::Component
{
    struct Palette {juce::Colour base, accent;};
    static Palette palette(const juce::String& category)
    {
        if (category == "Analog Drive") return {juce::Colour(0xff2b1d17), juce::Colour(0xffe4ae7a)};
        if (category == "Vocals") return {juce::Colour(0xff271920), juce::Colour(0xffe5a0ae)};
        if (category == "Synths") return {juce::Colour(0xff201c30), juce::Colour(0xffb6a4e5)};
        if (category == "Drums") return {juce::Colour(0xff2b2419), juce::Colour(0xffe6b16b)};
        if (category == "Bass") return {juce::Colour(0xff182332), juce::Colour(0xff96afd9)};
        if (category == "Guitar") return {juce::Colour(0xff2b1e19), juce::Colour(0xffdfa17d)};
        if (category == "Keys") return {juce::Colour(0xff192b25), juce::Colour(0xffa3cdbb)};
        if (category == "Spaces") return {juce::Colour(0xff182735), juce::Colour(0xff99c4e2)};
        if (category == "Lo-Fi") return {juce::Colour(0xff2a271c), juce::Colour(0xffcfbc84)};
        if (category == "Rhythm") return {juce::Colour(0xff182a20), juce::Colour(0xff90c2a4)};
        if (category == "Effects") return {juce::Colour(0xff2c1b2d), juce::Colour(0xffd39acb)};
        if (category == "User") return {juce::Colour(0xff1e2631), juce::Colour(0xffb3c0d3)};
        return {juce::Colour(0xff22231f), colours::gold};
    }
    static Palette categoryPalette(const juce::Component& owner, const juce::String& category)
    {
        const auto tones = palette(category);
        if (!isVintage(owner)) return tones;
        return {skinPalette(Skin::vintage).surface0.interpolatedWith(tones.base,.42f),
                tones.accent.interpolatedWith(skinPalette(Skin::vintage).textPrimary,.12f)};
    }
    enum class Icon {star, remove, restore, back};
    class ActionButton final : public PrimaryTextButton
    {
    public:
        Icon icon = Icon::star;
        juce::Colour accent = colours::gold;
        float scale = 1;
        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().reduced(.5f);
            const bool back = icon == Icon::back;
            const bool hover = isMouseOver() || hasKeyboardFocus(false);
            g.setColour(back ? (isVintage(*this)?paletteFor(*this).surface2:juce::Colour(0xff26251f)).interpolatedWith(accent, hover ? .14f : .04f)
                             : accent.withAlpha(hover ? .13f : getToggleState() ? .08f : 0.0f));
            g.fillRoundedRectangle(area, 8 * scale);
            g.setColour(accent.withAlpha(back ? (hover ? .55f : .28f) : hover ? .38f : .0f));
            g.drawRoundedRectangle(area, 8 * scale, 1);
            const auto ink = getToggleState() || hover || back ? accent : paletteFor(*this).textSecondary.withAlpha(.75f);
            g.setColour(ink);
            auto centre = area.getCentre();
            if (back) centre.x = area.getX() + 20 * scale;
            const float r = 7 * scale;
            juce::Path path;
            if (icon == Icon::star)
            {
                path.addStar(centre, 5, r * .47f, r);
                if (getToggleState()) g.fillPath(path); else g.strokePath(path, juce::PathStrokeType(1.3f * scale));
            }
            else if (icon == Icon::remove)
            {
                g.drawRoundedRectangle({centre.x-r*.65f, centre.y-r*.48f, r*1.3f, r*1.45f}, 1.5f*scale, 1.2f*scale);
                g.drawLine(centre.x-r, centre.y-r*.70f, centre.x+r, centre.y-r*.70f, 1.2f*scale);
                g.drawLine(centre.x-r*.35f, centre.y-r, centre.x+r*.35f, centre.y-r, 1.2f*scale);
                for (float x : {-.25f, .25f}) g.drawLine(centre.x+r*x, centre.y-r*.16f, centre.x+r*x, centre.y+r*.63f, scale);
            }
            else
            {
                path.startNewSubPath(centre.x+r*.35f, centre.y-r*.7f);
                path.lineTo(centre.x-r*.5f, centre.y); path.lineTo(centre.x+r*.35f, centre.y+r*.7f);
                g.strokePath(path, juce::PathStrokeType(1.5f*scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.drawLine(centre.x-r*.45f, centre.y, centre.x+r, centre.y, 1.5f*scale);
            }
            if (back)
            {
                g.setFont(labelFont(12 * scale));
                g.drawText(getButtonText(), area.withTrimmedLeft(37 * scale).withTrimmedRight(12 * scale), juce::Justification::centredLeft);
            }
        }
    };
    class SearchLook final : public juce::LookAndFeel_V4
    {
        void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
        {
            g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
            g.fillRoundedRectangle(juce::Rectangle<float>(0,0,static_cast<float>(width),static_cast<float>(height)).reduced(.5f), height * .26f);
        }
        void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
        {
            const float scale = height / 36.0f;
            g.setColour(editor.findColour(editor.hasKeyboardFocus(true) ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
            g.drawRoundedRectangle(juce::Rectangle<float>(0,0,static_cast<float>(width),static_cast<float>(height)).reduced(.5f), height*.26f, 1);
            const juce::Point<float> centre {16*scale, height*.47f};
            g.setColour(paletteFor(editor).textMuted); g.drawEllipse(centre.x-4.5f*scale, centre.y-4.5f*scale, 9*scale, 9*scale, 1.2f*scale);
            g.drawLine(centre.x+3.3f*scale, centre.y+3.3f*scale, centre.x+7*scale, centre.y+7*scale, 1.2f*scale);
        }
    };
    class CategoryButton final : public PrimaryTextButton
    {
    public:
        juce::String name;
        int count = 0;
        float scale = 1;
        void paint(juce::Graphics& g) override
        {
            const auto colour = categoryPalette(*this,name); auto area = getLocalBounds().toFloat().reduced(.5f);
            g.setColour(colour.base.withAlpha(getToggleState() ? 1.0f : isMouseOver() ? .65f : .30f));
            g.fillRoundedRectangle(area, 7 * scale);
            if (getToggleState()) {g.setColour(colour.accent.withAlpha(.5f)); g.fillRoundedRectangle(area.getX(),area.getY()+5*scale,2*scale,area.getHeight()-10*scale,scale);}
            auto label = area.reduced(13*scale, 0);
            auto counter = label.removeFromRight(33*scale);
            g.setColour(getToggleState() ? colour.accent : paletteFor(*this).textSecondary); g.setFont(labelFont(11*scale));
            g.drawText(name, label, juce::Justification::centredLeft);
            g.setColour(paletteFor(*this).textMuted); g.setFont(valueFont(10*scale));
            g.drawText(juce::String(count), counter, juce::Justification::centredRight);
        }
    };
    class Row final : public PrimaryTextButton
    {
    public:
        Row()
        {
            addAndMakeVisible(favourite); favourite.icon = Icon::star; favourite.setClickingTogglesState(true);
            addAndMakeVisible(remove); remove.icon = Icon::remove;
        }
        ::state::StatePresets::BrowserEntry entry;
        ActionButton favourite, remove;
        bool selected = false;
        float scale = 1;
        std::function<void()> onDoubleClick;
        void mouseDoubleClick(const juce::MouseEvent& event) override
        {if (event.mods.isLeftButtonDown() && isShowing() && isEnabled() && onDoubleClick) onDoubleClick();}
        void resized() override
        {
            auto area = getLocalBounds().toFloat().reduced(10*scale, 0);
            const float size = 29*scale;
            remove.scale = favourite.scale = scale;
            remove.setBounds(area.removeFromRight(size).withSizeKeepingCentre(size,size).toNearestInt());
            area.removeFromRight(5*scale);
            favourite.setBounds(area.removeFromRight(size).withSizeKeepingCentre(size,size).toNearestInt());
        }
        void paint(juce::Graphics& g) override
        {
            const auto colour = categoryPalette(*this,entry.factory ? entry.category : "User");
            auto area = getLocalBounds().toFloat().reduced(.5f);
            g.setColour((isVintage(*this)?colour.base.interpolatedWith(paletteFor(*this).surface1,.4f):colour.base).interpolatedWith(colour.accent, selected ? .11f : isMouseOver(true) ? .045f : 0.0f));
            g.fillRoundedRectangle(area, 8*scale);
            g.setColour(colour.accent.withAlpha(selected ? .5f : .13f)); g.drawRoundedRectangle(area,8*scale,1);
            if (selected) {g.setColour(colour.accent); g.fillRoundedRectangle(0,area.getY()+6*scale,3*scale,area.getHeight()-12*scale,scale);}
            auto label = area.reduced(17*scale, 7*scale).withTrimmedRight(71*scale);
            auto badge = label.removeFromRight(78*scale); label.removeFromRight(10*scale);
            g.setColour(selected ? colour.accent.brighter(.12f) : paletteFor(*this).textPrimary); g.setFont(bodyFont(14*scale));
            g.drawText(entry.name,label.removeFromTop(21*scale),juce::Justification::centredLeft,true);
            g.setColour(paletteFor(*this).textSecondary.withAlpha(.72f)); g.setFont(bodyFont(9.5f*scale));
            g.drawText(entry.removed ? (entry.factory ? "Hidden factory sound" : "Archived user sound") : entry.description.isEmpty() ? "User sound" : entry.description,
                label,juce::Justification::centredLeft,true);
            g.setColour(colour.accent.withAlpha(.75f)); g.setFont(labelFont(9*scale));
            g.drawText(entry.factory ? entry.category.toUpperCase() : "USER", badge,juce::Justification::centredRight);
            if (hasKeyboardFocus(false)) {g.setColour(colour.accent); g.drawRoundedRectangle(area.reduced(2),7*scale,1);}
        }
    };
public:
    explicit PresetBrowserPanel(::state::StatePresets& library) : presets(library)
    {
        setOpaque(true); setTitle("Sound library"); setWantsKeyboardFocus(true);
        addAndMakeVisible(closeButton); closeButton.icon=Icon::back;
        closeButton.setButtonText("Back to Reactor"); closeButton.setTitle("Close preset browser"); closeButton.setComponentID("preset_browser_close");
        closeButton.setTooltip("Return to the processing page (Escape)");
        closeButton.onClick=[safe=juce::Component::SafePointer<PresetBrowserPanel>(this)] {if(safe && safe->onClose) safe->onClose();};
        addAndMakeVisible(search); search.setLookAndFeel(&searchLook);
        search.setTextToShowWhenEmpty("Search sounds, colour or space...",paletteFor(*this).textMuted);
        search.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff141a22));
        search.setColour(juce::TextEditor::textColourId,paletteFor(*this).textPrimary);
        search.setColour(juce::TextEditor::outlineColourId,paletteFor(*this).textMuted.withAlpha(.24f));
        search.setColour(juce::TextEditor::focusedOutlineColourId,colours::gold.withAlpha(.6f));
        search.setOpaque(false);
        search.setTitle("Search presets"); search.setComponentID("preset_search"); search.setSelectAllWhenFocused(true);
        search.onTextChange=[safe=juce::Component::SafePointer<PresetBrowserPanel>(this)] {if(safe) safe->rebuildRows();};
        viewport.setViewedComponent(&content,false); viewport.setScrollBarsShown(true,false);
        viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,paletteFor(*this).textMuted.withAlpha(.3f)); addAndMakeVisible(viewport);
        sidebar.setViewedComponent(&categoriesContent,false); sidebar.setScrollBarsShown(true,false);
        sidebar.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,paletteFor(*this).textMuted.withAlpha(.2f)); addAndMakeVisible(sidebar);
    }
    ~PresetBrowserPanel() override
    {search.setLookAndFeel(nullptr); viewport.setViewedComponent(nullptr,false); sidebar.setViewedComponent(nullptr,false);}
    std::function<void(const juce::String&)> onPresetSelected;
    std::function<void()> onClose, onLibraryChanged;
    void open()
    {
        category="All Sounds"; search.setText({},false); message.clear(); refreshLibrary();
    }
    void refreshSelection()
    {
        const auto current=presets.getCurrentPresetKey(); if(current==selectedKey) return;
        selectedKey=current;
        for(auto& row:rows) {row->selected=row->entry.key==selectedKey; row->repaint();}
        repaint(footer.toNearestInt());
    }
    void lookAndFeelChanged() override
    {
        const auto& tones = paletteFor(*this);
        search.setColour(juce::TextEditor::backgroundColourId,isVintage(*this)?tones.surface0:juce::Colour(0xff141a22));
        search.setColour(juce::TextEditor::textColourId,tones.textPrimary);
        search.applyColourToAllText(tones.textPrimary);
        search.setColour(juce::TextEditor::outlineColourId,tones.textMuted.withAlpha(.24f));
        search.setTextToShowWhenEmpty("Search sounds, colour or space...",tones.textMuted);
        search.setOpaque(false);
        for (auto& row : rows)
        {
            row->favourite.accent = isVintage(*this) ? tones.accent : colours::gold;
            row->remove.accent = row->entry.removed ? categoryPalette(*this,row->entry.category).accent
                : isVintage(*this) ? juce::Colour(0xffd5a08c) : juce::Colour(0xffd59191);
        }
        refreshCategorySelection(); repaint();
    }
    juce::String getSelectedCategory() const {return category;}
    int getVisiblePresetCount() const noexcept {return static_cast<int>(rows.size());}
    void selectCategory(const juce::String& name) {category=name; message.clear(); rebuildRows(); refreshCategorySelection();}
    void setSearchText(const juce::String& text) {search.setText(text,false); rebuildRows();}
    void resized() override
    {
        scale=juce::jlimit(.8f,2.2f,static_cast<float>(getWidth())/1000.0f);
        auto area=getLocalBounds().toFloat().reduced(24*scale,18*scale);
        auto header=area.removeFromTop(67*scale);
        closeButton.scale=scale; closeButton.accent=categoryPalette(*this,category).accent;
        closeButton.setBounds(header.removeFromRight(160*scale).withTrimmedTop(9*scale).withHeight(36*scale).toNearestInt());
        header.removeFromRight(16*scale);
        auto searchArea=header.removeFromRight(juce::jmin(285*scale,header.getWidth()*.46f));
        search.setBounds(searchArea.withTrimmedTop(9*scale).withHeight(36*scale).toNearestInt());
        search.setFont(bodyFont(12*scale)); search.setIndents(juce::roundToInt(33*scale), juce::roundToInt(9*scale));
        headerTitle=header; area.removeFromTop(10*scale);
        auto left=area.removeFromLeft(185*scale); area.removeFromLeft(24*scale);
        categoryTitle=left.removeFromTop(27*scale);
        sidebar.setBounds(left.toNearestInt()); sidebar.setScrollBarThickness(juce::roundToInt(4*scale));
        const int pitch=juce::jmax(juce::roundToInt(20*scale), juce::jmin(juce::roundToInt(28*scale),
            sidebar.getHeight()/juce::jmax(1,static_cast<int>(categoryButtons.size()))));
        categoriesContent.setSize(sidebar.getWidth()-juce::roundToInt(5*scale),juce::jmax(sidebar.getHeight(),pitch*static_cast<int>(categoryButtons.size())));
        for(size_t i=0;i<categoryButtons.size();++i)
        {categoryButtons[i]->scale=scale; categoryButtons[i]->setBounds(0,static_cast<int>(i)*pitch,categoriesContent.getWidth(),pitch-juce::roundToInt(3*scale));}
        footer=area.removeFromBottom(69*scale); area.removeFromBottom(15*scale); listTitle=area.removeFromTop(27*scale);
        viewport.setBounds(area.toNearestInt()); viewport.setScrollBarThickness(juce::roundToInt(4*scale));
        const int height=juce::roundToInt(60*scale);
        content.setSize(viewport.getWidth()-juce::roundToInt(7*scale),juce::jmax(viewport.getHeight(),height*static_cast<int>(rows.size())));
        for(size_t i=0;i<rows.size();++i) {rows[i]->scale=scale; rows[i]->setBounds(0,static_cast<int>(i)*height,content.getWidth(),height-juce::roundToInt(6*scale)); rows[i]->resized();}
    }
    void paint(juce::Graphics& g) override
    {
        const auto theme=categoryPalette(*this,category);
        g.fillAll(isVintage(*this)?paletteFor(*this).surface0.interpolatedWith(theme.base,.22f)
                                 :juce::Colour(0xff0d1118).interpolatedWith(theme.base,.4f));
        g.setGradientFill(juce::ColourGradient(theme.accent.withAlpha(.055f),0,0,juce::Colours::transparentBlack,static_cast<float>(getWidth()),static_cast<float>(getHeight()),false)); g.fillAll();
        auto title=headerTitle; g.setColour(theme.accent); g.setFont(labelFont(11*scale));
        g.drawText("FIRE  /  SOUND LIBRARY",title.removeFromTop(20*scale),juce::Justification::centredLeft);
        g.setColour(paletteFor(*this).textPrimary); g.setFont(bodyFont(25*scale)); g.drawText("Find your next colour.",title,juce::Justification::centredLeft);
        g.setColour(paletteFor(*this).textMuted); g.setFont(labelFont(10*scale)); g.drawText("COLLECTIONS",categoryTitle,juce::Justification::centredLeft);
        g.drawText(category.toUpperCase()+"  /  "+juce::String(rows.size())+(rows.size()==1?" SOUND":" SOUNDS"),listTitle,juce::Justification::centredLeft);
        g.setColour(theme.accent.withAlpha(.18f)); g.drawLine(footer.getX(),footer.getY(),footer.getRight(),footer.getY(),1);
        const ::state::StatePresets::BrowserEntry* selected=nullptr;
        for(const auto& entry:entries) if(entry.key==selectedKey && !entry.removed) {selected=&entry;break;}
        auto detail=footer.withTrimmedTop(10*scale); g.setColour(theme.accent); g.setFont(labelFont(12*scale));
        g.drawText(message.isNotEmpty()?message:selected?selected->name:"Click a preset to audition",detail.removeFromTop(20*scale),juce::Justification::centredLeft);
        g.setColour(paletteFor(*this).textSecondary); g.setFont(bodyFont(10.5f*scale));
        g.drawFittedText(message.isNotEmpty()?"Browse Favourites or restore removed sounds from Recycle Bin on the left.":selected?selected->description:
            category=="Recycle Bin"?"Restore a sound to its original collection. Your current sound keeps playing.":"Single-click to audition. Double-click or Escape returns to the reactor.",detail.toNearestInt(),juce::Justification::centredLeft,2);
        if(rows.empty()) {g.setColour(paletteFor(*this).textSecondary);g.setFont(bodyFont(15*scale));g.drawText(category=="Favourites"?"Star a sound to add it here":category=="Recycle Bin"?"Your recycle bin is empty":"No sounds match this search",viewport.getBounds().toFloat(),juce::Justification::centred);}
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(key.isKeyCode(juce::KeyPress::escapeKey)) {if(onClose) onClose();return true;}
        if(search.hasKeyboardFocus(true)||rows.empty()||category=="Recycle Bin") return false;
        if(key.isKeyCode(juce::KeyPress::upKey)||key.isKeyCode(juce::KeyPress::downKey))
        {
            int current=-1;for(size_t i=0;i<rows.size();++i) if(rows[i]->entry.key==selectedKey) current=static_cast<int>(i);
            const int next=juce::jlimit(0,static_cast<int>(rows.size())-1,current+(key.isKeyCode(juce::KeyPress::downKey)?1:-1));
            const auto tag=rows[static_cast<size_t>(next)]->entry.tag;viewport.setViewPosition(0,rows[static_cast<size_t>(next)]->getY());
            if(onPresetSelected) onPresetSelected(tag);return true;
        }
        return false;
    }
private:
    static bool matchesCategory(const ::state::StatePresets::BrowserEntry& entry,const juce::String& group)
    {
        if(group=="Recycle Bin") return entry.removed;
        if(entry.removed) return false;
        return group=="All Sounds"||(group=="Favourites"?entry.favourite:group=="User"?!entry.factory:entry.category==group);
    }
    void refreshCategorySelection()
    {for(auto& button:categoryButtons) button->setToggleState(button->name==category,juce::dontSendNotification);closeButton.accent=categoryPalette(*this,category).accent;repaint();}
    void refreshLibrary()
    {
        ++libraryGeneration; entries=presets.getBrowserEntries(true); selectedKey=presets.getCurrentPresetKey();
        const auto position=sidebar.getViewPosition();
        std::vector<juce::String> names {"All Sounds","Favourites"};
        for(const auto& entry:entries) if(entry.factory && std::find(names.begin(),names.end(),entry.category)==names.end()) names.push_back(entry.category);
        std::sort(names.begin()+2,names.end());names.push_back("User");names.push_back("Recycle Bin");categoryButtons.clear();
        const auto epoch=libraryGeneration;const juce::Component::SafePointer<PresetBrowserPanel> safe(this);
        for(const auto& name:names)
        {
            auto button=std::make_unique<CategoryButton>();button->name=name;
            for(const auto& entry:entries) if(matchesCategory(entry,name)) ++button->count;
            button->setButtonText(name);button->setTitle("Preset category "+name);button->setComponentID("presetCategory:"+name);
            button->onClick=[safe,epoch,name] {if(safe && safe->libraryGeneration==epoch && safe->isShowing() && safe->isEnabled()) safe->selectCategory(name);};
            categoriesContent.addAndMakeVisible(*button);categoryButtons.push_back(std::move(button));
        }
        rebuildRows(false);refreshCategorySelection();resized();sidebar.setViewPosition(position);
    }
    void performAction(const juce::String& key,bool favourite,bool value=false)
    {
        const juce::Component::SafePointer<PresetBrowserPanel> safe(this);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& entry){return entry.key==key && entry.removed==(category=="Recycle Bin");});
        if(found==entries.end()) return;const auto entry=*found;
        const bool result=favourite?presets.setPresetFavourite(key,value):entry.removed?presets.restoreBrowserPreset(key):presets.removeBrowserPreset(key);
        if(!safe) return;
        message=result?(favourite?(value?"Added to favourites":"Removed from favourites"):entry.removed?"Restored "+entry.name:"Moved "+entry.name+" to Recycle Bin")
            :"This action could not be completed. Your current sound is unchanged.";
        refreshLibrary(); if(!safe) return;
        if(result && !favourite && onLibraryChanged) onLibraryChanged();
    }
    void rebuildRows(bool resetScroll=true)
    {
        const auto position=resetScroll?juce::Point<int>{}:viewport.getViewPosition();++generation;rows.clear();
        const auto query=search.getText().trim();const auto epoch=generation;const juce::Component::SafePointer<PresetBrowserPanel> safe(this);
        for(const auto& entry:entries)
        {
            if(!matchesCategory(entry,category)) continue;
            if(query.isNotEmpty() && !(entry.name+" "+entry.description+" "+entry.category).containsIgnoreCase(query)) continue;
            auto row=std::make_unique<Row>();row->entry=entry;row->selected=entry.key==selectedKey;
            row->setTitle(entry.removed?"Archived preset "+entry.name:"Load preset "+entry.name);row->setTooltip(entry.description);row->setComponentID("presetSound:"+entry.key);
            row->favourite.setToggleState(entry.favourite,juce::dontSendNotification);row->favourite.setEnabled(!entry.removed);
            row->favourite.accent = isVintage(*this) ? paletteFor(*this).accent : colours::gold;
            row->favourite.setTitle((entry.favourite?"Unfavourite ":"Favourite ")+entry.name);row->favourite.setComponentID("presetFavourite:"+entry.key);
            row->favourite.setTooltip(entry.favourite?"Remove from favourites":"Add to favourites");
            row->remove.icon=entry.removed?Icon::restore:Icon::remove;row->remove.accent=entry.removed?categoryPalette(*this,entry.category).accent:
                isVintage(*this)?juce::Colour(0xffd5a08c):juce::Colour(0xffd59191);
            row->remove.setTitle((entry.removed?"Restore ":"Delete ")+entry.name);row->remove.setComponentID((entry.removed?"presetRestore:":"presetDelete:")+entry.key);
            row->remove.setTooltip(entry.removed?"Restore to the original collection":entry.factory?"Hide this factory sound. Restore it from Recycle Bin.":"Move this user preset into Recycle Bin. Restore it at any time.");
            const auto live=[safe,epoch] {return safe && safe->generation==epoch && safe->isShowing() && safe->isEnabled();};
            row->onClick=[safe,live,tag=entry.tag,removed=entry.removed] {if(live() && !removed && safe->onPresetSelected) {safe->message.clear();safe->onPresetSelected(tag);}};
            row->onDoubleClick=[safe,live,removed=entry.removed] {if(live() && !removed && safe->onClose) safe->onClose();};
            row->favourite.onClick=[safe,live,key=entry.key,value=!entry.favourite] {if(live()) safe->performAction(key,true,value);};
            row->remove.onClick=[safe,live,key=entry.key] {if(live()) safe->performAction(key,false);};
            content.addAndMakeVisible(*row);rows.push_back(std::move(row));
        }
        resized();viewport.setViewPosition(position);repaint();
    }
    ::state::StatePresets& presets;
    SearchLook searchLook;
    juce::Viewport viewport,sidebar;
    juce::Component content,categoriesContent;
    juce::TextEditor search;
    ActionButton closeButton;
    std::vector<::state::StatePresets::BrowserEntry> entries;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<std::unique_ptr<CategoryButton>> categoryButtons;
    juce::String category {"All Sounds"},selectedKey,message;
    juce::Rectangle<float> headerTitle,categoryTitle,listTitle,footer;
    float scale=1;
    std::uint64_t generation=0,libraryGeneration=0;
};
}
