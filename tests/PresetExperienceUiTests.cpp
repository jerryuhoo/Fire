#include <PluginEditor.h>
#include <GUI/PresetLockControls.h>
#include <GUI/PresetMetadataControls.h>
#include <GUI/PresetBrowserPanel.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

struct PresetExperienceUiTestAccess
{
    static auto lockMenu(ModulatableSlider& slider) { return slider.createLfoAssignmentMenu(slider.getLfoSourceMenuState()); }
    static auto lockResult(ModulatableSlider& slider) { return slider.createLfoAssignmentMenuResultHandler(); }
    static auto& indicator(state::StateComponent& component) {return component.presetLocksButton;}
    static void search(fire::ui::PresetLockControls& control,const juce::String& query) {control.search.setText(query,false);control.rebuild();}
    static void unlockAll(fire::ui::PresetLockControls& control) {control.clear.onClick();}
    static void onlyLocked(fire::ui::PresetLockControls& control)
    {control.lockedOnly.setToggleState(true,juce::dontSendNotification);control.rebuild();}
    static void pollLocks(fire::ui::PresetLockControls& control) {control.timerCallback();}
    static void setDetails(fire::ui::PresetMetadataControls& control,const juce::String& tags,const juce::String& description)
    {control.tags.setText(tags,false);control.description.setText(description,false);}
    static void save(fire::ui::PresetMetadataControls& control) {control.saveChanges();}
    static void reset(fire::ui::PresetMetadataControls& control) {control.resetChanges();}
    static juce::String status(fire::ui::PresetMetadataControls& control) {return control.status.getText();}
    static juce::String tags(fire::ui::PresetMetadataControls& control) {return control.tags.getText();}
    static void edit(fire::ui::PresetBrowserPanel& browser) {browser.showMetadataEditor();}
    static auto dialog(fire::ui::PresetBrowserPanel& browser) {return browser.metadataDialog;}
    static juce::Rectangle<int> detailButton(fire::ui::PresetBrowserPanel& browser) {return browser.editDetails.getBounds();}
    static juce::Rectangle<int> locateButton(fire::ui::PresetBrowserPanel& browser) {return browser.locateButton.getBounds();}
};
namespace
{
using Access=PresetExperienceUiTestAccess;
struct Library
{
    juce::File directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("FirePresetExperience-"+juce::Uuid().toString());
    explicit Library(FireAudioProcessor& processor)
    {REQUIRE(directory.createDirectory().wasOk());processor.statePresets.setPresetDirectoryForTesting(directory);processor.statePresets.enableFactoryPresets();}
    ~Library(){directory.deleteRecursively();}
};
void show(juce::Component& component) {component.addToDesktop(juce::ComponentPeer::windowIsTemporary);component.setVisible(true);}
template<class T> T* find(juce::Component& root,const juce::String& id={})
{
    if(auto* value=dynamic_cast<T*>(&root);value && (id.isEmpty() || root.getComponentID()==id)) return value;
    for(auto* child:root.getChildren()) if(auto* value=find<T>(*child,id)) return value;
    return nullptr;
}
void preview(juce::Component& control,const juce::String& name)
{
    if(const auto* path=std::getenv("FIRE_PRESET_EXPERIENCE_PREVIEW_DIR"))
    {
        const auto file=juce::File(path).getChildFile(name+".png");REQUIRE(file.getParentDirectory().createDirectory().wasOk());
        auto stream=file.createOutputStream();REQUIRE(stream);stream->setPosition(0);stream->truncate();
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(control.createComponentSnapshot(control.getLocalBounds()),*stream));
    }
}
}

TEST_CASE("Knob lock menus preserve base-value intent and expose global unlock access", "[preset-experience-ui][preset-locks][ui]")
{
    FireAudioProcessor processor;processor.hasUpdateCheckBeenPerformed=true;Library library(processor);
    FireAudioProcessorEditor editor(processor);editor.stopTimer();show(editor);
    auto* slider=find<ModulatableSlider>(editor,"drive1");
    if(!slider)
    {
        std::function<ModulatableSlider*(juce::Component&)> locate=[&](auto& root)->ModulatableSlider*
        {if(auto* s=dynamic_cast<ModulatableSlider*>(&root);s && s->getParamID()=="drive1")return s;for(auto* c:root.getChildren())if(auto* s=locate(*c))return s;return nullptr;};
        slider=locate(editor);
    }
    REQUIRE(slider);auto menu=Access::lockMenu(*slider);bool found=false;
    for(juce::PopupMenu::MenuItemIterator item(menu);item.next();)if(item.getItem().itemID==1003){found=true;CHECK_FALSE(item.getItem().isTicked);}
    REQUIRE(found);
    const auto value=processor.treeState.getParameter("drive1")->getValue();
    auto handler=Access::lockResult(*slider);handler(1003);
    CHECK(processor.isPresetParameterLocked("drive1"));CHECK(slider->isPresetLocked);
    CHECK(processor.treeState.getParameter("drive1")->getValue()==value);
    handler(1003);CHECK(processor.isPresetParameterLocked("drive1"));
    auto* header=find<state::StateComponent>(editor);REQUIRE(header);header->updatePresetLockIndicator();
    CHECK(Access::indicator(*header).isVisible());CHECK(Access::indicator(*header).getButtonText()=="1");
    CHECK(Access::indicator(*header).getTooltip().contains("modulation"));
    fire::ui::PresetLockControls controls(processor);show(controls);Access::search(controls,"drive1");
    auto* toggle=find<PrimaryToggleButton>(controls,"presetLock:drive1");REQUIRE(toggle);CHECK(toggle->getToggleState());
    Access::unlockAll(controls);CHECK_FALSE(processor.isPresetParameterLocked("drive1"));
    header->updatePresetLockIndicator();CHECK_FALSE(Access::indicator(*header).isVisible());
}

TEST_CASE("Metadata editor saves Unicode quoted tags and keeps Factory edits personal", "[preset-experience-ui][preset-metadata][ui]")
{
    FireAudioProcessor processor;Library folder(processor);auto& library=processor.statePresets;
    const auto entry=library.getBrowserEntries().front();
    const auto before=processor.treeState.copyState().toXmlString();
    fire::ui::PresetMetadataControls editor(library,entry.key,entry.name);show(editor);
    Access::setDetails(editor,juce::String::fromUTF8("warm, \"a,b\", \"a\"\"b\", 人声"),"Personal studio note");
    Access::save(editor);
    state::StatePresets::PresetMetadata saved;REQUIRE(library.readPresetMetadata(entry.key,saved).wasOk());
    CHECK(saved.factory);CHECK(saved.hasOverride);CHECK(saved.tags.contains("a,b"));CHECK(saved.tags.contains("a\"b"));
    CHECK(saved.tags.contains(juce::String::fromUTF8("人声")));CHECK(saved.description=="Personal studio note");
    CHECK(processor.treeState.copyState().toXmlString()==before);
    fire::ui::PresetMetadataControls restored(library,entry.key,entry.name);show(restored);
    CHECK(Access::tags(restored).contains("\"a,b\""));
    Access::reset(restored);REQUIRE(library.readPresetMetadata(entry.key,saved).wasOk());CHECK_FALSE(saved.hasOverride);
    CHECK(saved.description==entry.description);
}

TEST_CASE("Metadata save rejects an obsolete file revision without silently overwriting newer notes", "[preset-experience-ui][preset-metadata][ui][lifecycle]")
{
    FireAudioProcessor processor;Library folder(processor);auto& library=processor.statePresets;
    const auto entry=library.getBrowserEntries().front();
    fire::ui::PresetMetadataControls first(library,entry.key,entry.name),stale(library,entry.key,entry.name);show(first);show(stale);
    Access::setDetails(first,"first","Current notes");Access::save(first);
    Access::setDetails(stale,"old","Obsolete notes");Access::save(stale);
    CHECK(Access::status(stale).isNotEmpty());
    state::StatePresets::PresetMetadata saved;REQUIRE(library.readPresetMetadata(entry.key,saved).wasOk());CHECK(saved.description=="Current notes");
    fire::ui::PresetMetadataControls invalid(library,entry.key,entry.name);show(invalid);
    Access::setDetails(invalid,"\"unterminated","Another note");Access::save(invalid);
    CHECK(Access::status(invalid).contains("quote"));
    REQUIRE(library.readPresetMetadata(entry.key,saved).wasOk());CHECK(saved.description=="Current notes");
}

TEST_CASE("Library tags are searchable and detail editor closes with the browser", "[preset-experience-ui][preset-browser][ui][lifecycle]")
{
    FireAudioProcessor processor;Library folder(processor);auto& library=processor.statePresets;
    const auto entry=library.getBrowserEntries().front();state::StatePresets::PresetMetadata metadata;
    REQUIRE(library.readPresetMetadata(entry.key,metadata).wasOk());metadata.tags.add("uncommon-special-tag");
    REQUIRE(library.writePresetMetadata(entry.key,metadata,metadata.revision).wasOk());
    library.setCurrentPresetKey(entry.key);
    fire::ui::PresetBrowserPanel browser(library);browser.setSize(1000,500);show(browser);browser.open();
    browser.setSearchText("uncommon-special-tag");CHECK(browser.getVisiblePresetCount()==1);
    CHECK_FALSE(Access::detailButton(browser).intersects(Access::locateButton(browser)));
    Access::edit(browser);auto dialog=Access::dialog(browser);REQUIRE(dialog);
    browser.setVisible(false);CHECK(Access::dialog(browser)==nullptr);CHECK(dialog==nullptr);
}

TEST_CASE("Preset details and lock controls fit the four skins without changing Library footer height", "[preset-experience-ui][ui][skin][layout]")
{
    FireAudioProcessor processor;Library folder(processor);processor.setPresetParameterLocked("drive1",true);
    const auto entry=processor.statePresets.getBrowserEntries().front();processor.statePresets.setCurrentPresetKey(entry.key);
    fire::ui::PresetLockControls locks(processor);
    fire::ui::PresetMetadataControls metadata(processor.statePresets,entry.key,entry.name);
    fire::ui::PresetBrowserPanel browser(processor.statePresets);browser.setSize(1000,500);browser.open();
    for(const auto skin:fire::ui::skins)
    {
        for(auto* control:std::array<juce::Component*,3>{&locks,&metadata,&browser})
        {
            fire::ui::setSkin(*control,skin);control->sendLookAndFeelChange();
            for(auto* child:control->getChildren())if(child->isVisible())CHECK(control->getLocalBounds().contains(child->getBounds()));
            preview(*control,juce::String(fire::ui::skinName(skin)).toLowerCase()+(control==&locks?"-locks":control==&metadata?"-details":"-library"));
        }
        CHECK_FALSE(Access::detailButton(browser).intersects(Access::locateButton(browser)));
    }
}

TEST_CASE("Locked parameter rows cancel pointer ownership before reusing a visible row", "[preset-experience-ui][preset-locks][ui][lifecycle]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.setPresetParameterLocked("drive1",true));
    REQUIRE(processor.setPresetParameterLocked("drive2",true));
    fire::ui::PresetLockControls control(processor); show(control); Access::onlyLocked(control);
    auto* first=find<PrimaryToggleButton>(control,"presetLock:drive1");
    auto* second=find<PrimaryToggleButton>(control,"presetLock:drive2");
    REQUIRE(first); REQUIRE(second);
    // Sort/catalog order is not an identity: drive1 need not be the first row.
    auto* pressed=first->getScreenY()<second->getScreenY()?first:second;
    const auto oldID=pressed->getComponentID().fromFirstOccurrenceOf("presetLock:",false,false);
    const auto survivor=oldID=="drive1"?juce::String("drive2"):juce::String("drive1");
    const juce::Component::SafePointer<PrimaryToggleButton> pending(pressed);
    const auto event=[](juce::Component& component,int modifiers)
    {
        const auto now=juce::Time::getCurrentTime();const auto point=component.getLocalBounds().toFloat().getCentre();
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(modifiers),
            0,0,0,0,0,&component,&component,now,point,now,1,false);
    };
    pressed->mouseDown(event(*pressed,juce::ModifierKeys::leftButtonModifier));REQUIRE(pressed->isDown());
    REQUIRE(processor.setPresetParameterLocked(oldID,false)); Access::pollLocks(control);
    auto* reused=find<PrimaryToggleButton>(control,"presetLock:"+survivor);REQUIRE(reused);
    CHECK_FALSE(reused->isDown());
    if(pending) pending->mouseUp(event(*pending,0));
    CHECK(processor.isPresetParameterLocked(survivor));
    CHECK_FALSE(processor.isPresetParameterLocked(oldID));
}
