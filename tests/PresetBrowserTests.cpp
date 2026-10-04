#include <PluginEditor.h>
#include <GUI/PresetBrowserPanel.h>
#include <Utility/FactoryPresets.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

namespace
{
struct LibraryFolder
{
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile("TestTemp")
        .getChildFile("PresetBrowser-" + juce::Uuid().toString());
    explicit LibraryFolder(FireAudioProcessor& processor)
    {
        REQUIRE(file.createDirectory().wasOk());
        processor.statePresets.setPresetDirectoryForTesting(file);
        processor.statePresets.enableFactoryPresets();
    }
    ~LibraryFolder() {file.deleteRecursively();}
};
template <typename T, typename Predicate> T* find(juce::Component& root, Predicate predicate)
{
    if (auto* item = dynamic_cast<T*>(&root); item && predicate(*item)) return item;
    for (auto* child : root.getChildren()) if (auto* item = find<T>(*child, predicate)) return item;
    return nullptr;
}
void preview(juce::Component& root, const juce::String& name)
{
    if (const auto* path = std::getenv("FIRE_PRESET_PREVIEW_DIR"))
    {
        juce::File file = juce::File(path).getChildFile(name + ".png"); file.getParentDirectory().createDirectory();
        auto stream = file.createOutputStream(); REQUIRE(stream); stream->setPosition(0); stream->truncate();
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(root.createComponentSnapshot(root.getLocalBounds()), *stream));
    }
}
}

TEST_CASE("Factory catalogue stays lightweight with twenty base sounds per collection and 36 analog drive scenes", "[preset-browser][factory-presets][catalogue]")
{
    FireAudioProcessor processor;
    LibraryFolder folder(processor);
    const auto entries = processor.statePresets.getBrowserEntries();
    int factoryCount = 0;
    for (const auto& entry : entries) if (entry.factory) {++factoryCount; CHECK(entry.key.startsWith("@factory/"));}
    CHECK(factoryCount == fire::factory::presetCount);
    CHECK(processor.statePresets.getPresetXml().toString().length() < 160000);
    std::function<void(const juce::XmlElement&)> checkMetadata = [&](const auto& node)
    {
        if (node.getBoolAttribute("factoryPreset"))
        {
            CHECK(node.getBoolAttribute("factoryVirtual"));
            CHECK_FALSE(node.hasAttribute("drive1"));
            CHECK(node.getNumChildElements() == 0);
        }
        for (auto* child : node.getChildIterator()) checkMetadata(*child);
    };
    checkMetadata(processor.statePresets.getPresetXml());
    juce::ScopedJuceInitialiser_GUI gui;
    fire::ui::PresetBrowserPanel browser(processor.statePresets); browser.setSize(1000, 500); browser.open();
    for (const auto& category : fire::factory::categories)
    {browser.selectCategory(category); CHECK(browser.getVisiblePresetCount() == (juce::String(category) == "Analog Drive" ? 36 : 20));}
    browser.setSearchText("zz-no-such-sound"); CHECK(browser.getVisiblePresetCount() == 0);
}

TEST_CASE("Clicking the preset header opens a full page and loading a sound keeps audition mode open", "[preset-browser][ui][selection][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    LibraryFolder folder(processor);
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setSize(1000, 500); editor->addToDesktop(juce::ComponentPeer::windowIsTemporary); editor->setVisible(true);
    auto* opener = find<juce::Button>(*editor, [](auto& button) {return button.getComponentID() == "header_preset_browser";}); REQUIRE(opener);
    opener->triggerClick();
    auto* browser = find<fire::ui::PresetBrowserPanel>(*editor, [](auto&) {return true;}); REQUIRE(browser); REQUIRE(browser->isShowing());
    CHECK(browser->getBounds() == editor->getLocalBounds());
    browser->selectCategory("Vocals"); REQUIRE(browser->getVisiblePresetCount() == 20);
    auto* row = find<juce::Button>(*browser, [](auto& button) {return button.getComponentID().startsWith("presetSound:@factory/");}); REQUIRE(row);
    const auto key = row->getComponentID().fromFirstOccurrenceOf("presetSound:", false, false);
    row->triggerClick();
    CHECK(processor.statePresets.getCurrentPresetKey() == key); CHECK(browser->isShowing());
    preview(*editor, "preset-library-vocals");
    browser->selectCategory("Guitar");
    browser->setSearchText("Valve"); REQUIRE(browser->getVisiblePresetCount() == 1);
    row = find<juce::Button>(*browser, [](auto& button) {return button.getComponentID().startsWith("presetSound:@factory/");}); REQUIRE(row);
    row->triggerClick(); browser->refreshSelection();
    preview(*editor, "preset-library-guitar-search");
    editor->setSize(2000, 1000);
    CHECK(browser->getBounds() == editor->getLocalBounds());
    preview(*editor, "preset-library-large");
    for (auto* child : editor->getChildren())
        if (dynamic_cast<juce::ResizableCornerComponent*>(child)) CHECK(child->isShowing());
        else if (child != browser) {CAPTURE(child->getTitle(), child->getComponentID()); CHECK_FALSE(child->isShowing());}
    REQUIRE(browser->keyPressed(juce::KeyPress{juce::KeyPress::escapeKey})); CHECK_FALSE(browser->isShowing()); CHECK(opener->isShowing());
}

TEST_CASE("Preset rows cannot replay commands after a different category or a closed page", "[preset-browser][ui][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; fire::ui::PresetBrowserPanel browser(processor.statePresets);
    LibraryFolder folder(processor);
    browser.setSize(1000, 500); browser.addToDesktop(juce::ComponentPeer::windowIsTemporary); browser.setVisible(true); browser.open();
    auto* row = find<PrimaryTextButton>(browser, [](auto& button) {return button.getComponentID().startsWith("presetSound:");}); REQUIRE(row);
    auto stale = row->onClick; int calls = 0; browser.onPresetSelected = [&](const auto&) {++calls;};
    browser.selectCategory("Guitar"); stale(); CHECK(calls == 0);
    row = find<PrimaryTextButton>(browser, [](auto& button) {return button.getComponentID().startsWith("presetSound:");}); REQUIRE(row);
    stale = row->onClick; browser.setVisible(false); stale(); CHECK(calls == 0);
}

TEST_CASE("Library favourites and hidden factory scenes persist without changing the sound", "[preset-browser][library][preferences][state]")
{
    FireAudioProcessor processor; LibraryFolder folder(processor);
    auto& library=processor.statePresets;
    const auto key=library.getBrowserEntries().front().key;
    juce::MemoryBlock before,after;processor.getStateInformation(before);
    REQUIRE(library.setPresetFavourite(key,true));
    REQUIRE(library.removeBrowserPreset(key));
    CHECK(library.getBrowserEntries().size()==fire::factory::presetCount-1);CHECK(library.getNumFactoryPresets()==fire::factory::presetCount);
    library.scanAllPresets();CHECK(library.getBrowserEntries().size()==fire::factory::presetCount-1);
    FireAudioProcessor another;
    another.statePresets.setPresetDirectoryForTesting(folder.file);another.statePresets.enableFactoryPresets();
    auto entries=another.statePresets.getBrowserEntries(true);
    auto entry=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.key==key;});
    REQUIRE(entry!=entries.end());CHECK(entry->removed);CHECK(entry->favourite);
    REQUIRE(another.statePresets.restoreBrowserPreset(key));
    entries=library.getBrowserEntries();CHECK(entries.size()==fire::factory::presetCount);
    entry=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.key==key;});
    REQUIRE(entry!=entries.end());CHECK(entry->favourite);CHECK_FALSE(entry->removed);
    processor.getStateInformation(after);CHECK(before==after);
}

TEST_CASE("User presets recycle and restore their exact files while preserving live processing", "[preset-browser][library][filesystem][restore]")
{
    FireAudioProcessor processor;LibraryFolder folder(processor);auto& library=processor.statePresets;
    const auto file=folder.file.getChildFile("User/My voice.fire");REQUIRE(library.savePreset(file).isNotEmpty());
    const auto key=library.getCurrentPresetKey();REQUIRE(key=="User/My voice.fire");
    REQUIRE(library.setPresetFavourite(key,true));
    const auto parameters=processor.treeState.copyState().toXmlString();
    const auto bytes=file.loadFileAsString();
    REQUIRE(library.removeBrowserPreset(key));CHECK_FALSE(file.exists());CHECK(library.getCurrentPresetKey().isEmpty());
    CHECK(processor.treeState.copyState().toXmlString()==parameters);
    library.scanAllPresets();CHECK(library.getBrowserEntries().size()==fire::factory::presetCount);
    const auto entries=library.getBrowserEntries(true);
    const auto entry=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.key==key;});
    REQUIRE(entry!=entries.end());CHECK(entry->removed);CHECK(entry->favourite);
    REQUIRE(file.replaceWithText("A replacement file"));CHECK_FALSE(library.restoreBrowserPreset(key));
    CHECK(file.loadFileAsString()=="A replacement file");REQUIRE(file.deleteFile());
    REQUIRE(library.restoreBrowserPreset(key));CHECK(file.loadFileAsString()==bytes);
    CHECK(library.getBrowserEntries().size()==fire::factory::presetCount+1);CHECK(processor.treeState.copyState().toXmlString()==parameters);
    CHECK_FALSE(library.removeBrowserPreset("../escape.fire"));CHECK_FALSE(library.restoreBrowserPreset("../escape.fire"));
    REQUIRE(folder.file.getChildFile(".fire-library.xml").replaceWithText("<FIRE_LIBRARY version=\"99\"/>"));
    CHECK_FALSE(library.setPresetFavourite(key,false));CHECK(file.loadFileAsString()==bytes);
}

TEST_CASE("Preset row favourites deletion and restore never audition a different sound", "[preset-browser][ui][library][actions]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;LibraryFolder folder(processor);fire::ui::PresetBrowserPanel browser(processor.statePresets);
    browser.setSize(1000,500);browser.addToDesktop(juce::ComponentPeer::windowIsTemporary);browser.setVisible(true);browser.open();
    int auditions=0;browser.onPresetSelected=[&](const auto&){++auditions;};browser.selectCategory("Vocals");
    auto* favourite=find<juce::Button>(browser,[](auto& b){return b.getComponentID().startsWith("presetFavourite:");});REQUIRE(favourite);
    const auto key=favourite->getComponentID().fromFirstOccurrenceOf("presetFavourite:",false,false);favourite->triggerClick();
    CHECK(auditions==0);browser.selectCategory("Favourites");CHECK(browser.getVisiblePresetCount()==1);preview(browser,"library-favourites");
    auto* remove=find<PrimaryTextButton>(browser,[](auto& b){return b.getComponentID().startsWith("presetDelete:");});REQUIRE(remove);
    auto stale=remove->onClick;remove->triggerClick();CHECK(auditions==0);CHECK(browser.getVisiblePresetCount()==0);
    browser.selectCategory("Recycle Bin");CHECK(browser.getVisiblePresetCount()==1);preview(browser,"library-recycle-bin");
    auto* restore=find<juce::Button>(browser,[](auto& b){return b.getComponentID().startsWith("presetRestore:");});REQUIRE(restore);restore->triggerClick();
    CHECK(browser.getVisiblePresetCount()==0);CHECK(auditions==0);stale();
    browser.selectCategory("Favourites");CHECK(browser.getVisiblePresetCount()==1);
    const auto entries=processor.statePresets.getBrowserEntries();
    CHECK(std::any_of(entries.begin(),entries.end(),[&](const auto& e){return e.key==key && e.favourite;}));
    browser.setVisible(false);remove=find<PrimaryTextButton>(browser,[](auto& b){return b.getComponentID().startsWith("presetDelete:");});REQUIRE(remove);
    stale=remove->onClick;stale();CHECK(processor.statePresets.getBrowserEntries().size()==fire::factory::presetCount);
}

TEST_CASE("Library search and return button remain separate and collections have distinct dark palettes", "[preset-browser][ui][layout][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;LibraryFolder folder(processor);fire::ui::PresetBrowserPanel browser(processor.statePresets);
    browser.setSize(1000,500);browser.open();
    auto* search=find<juce::TextEditor>(browser,[](auto& e){return e.getComponentID()=="preset_search";});REQUIRE(search);
    auto* back=find<juce::Button>(browser,[](auto& b){return b.getComponentID()=="preset_browser_close";});REQUIRE(back);
    for(int width:{1000,1400,2000})
    {
        browser.setSize(width,width/2);CHECK(back->getX()-search->getRight()>=juce::roundToInt(16.0f*width/1000));
        CHECK(browser.getLocalBounds().contains(search->getBounds()));CHECK(browser.getLocalBounds().contains(back->getBounds()));
        auto* recycle=find<juce::Button>(browser,[](auto& b){return b.getComponentID()=="presetCategory:Recycle Bin";});REQUIRE(recycle);
        CHECK(browser.getLocalBounds().contains(browser.getLocalArea(recycle,recycle->getLocalBounds())));
    }
    const auto rounded=search->createComponentSnapshot(search->getLocalBounds());CHECK(rounded.getPixelAt(0,0).getAlpha()<30);
    browser.setSize(1000,500);browser.selectCategory("Vocals");
    const auto red=browser.createComponentSnapshot(browser.getLocalBounds()).getPixelAt(995,250);preview(browser,"library-vocals-colour");
    browser.selectCategory("Drums");const auto amber=browser.createComponentSnapshot(browser.getLocalBounds()).getPixelAt(995,250);preview(browser,"library-drums-colour");
    CHECK(red!=amber);browser.setSize(2000,1000);preview(browser,"library-drums-large");
}

TEST_CASE("Analog Drive collection filters auditions and restores its new factory entries",
          "[factory-presets][analog-presets][preset-browser][ui][library]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; LibraryFolder folder(processor);
    fire::ui::PresetBrowserPanel browser(processor.statePresets);
    browser.setSize(1000, 500); browser.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    browser.setVisible(true); browser.open(); browser.selectCategory("Analog Drive");
    CHECK(browser.getVisiblePresetCount() == 36);
    browser.setSearchText("Tape"); CHECK(browser.getVisiblePresetCount() == 3);
    auto* row = find<juce::Button>(browser, [](auto& b) {
        return b.getComponentID() == "presetSound:@factory/analog-tape-studio-print";
    });
    REQUIRE(row);
    browser.onPresetSelected = [&](const auto& tag) { REQUIRE(processor.statePresets.loadPreset(tag)); };
    row->triggerClick(); CHECK(processor.getShapeMode(0, 0) == 23);
    const auto key = processor.statePresets.getCurrentPresetKey();
    CHECK(key == "@factory/analog-tape-studio-print");
    REQUIRE(processor.statePresets.setPresetFavourite(key, true));
    REQUIRE(processor.statePresets.removeBrowserPreset(key));
    browser.open(); browser.selectCategory("Analog Drive"); browser.setSearchText("Tape");
    CHECK(browser.getVisiblePresetCount() == 2);
    REQUIRE(processor.statePresets.restoreBrowserPreset(key));
    browser.open(); browser.selectCategory("Analog Drive"); browser.setSearchText("Tape");
    CHECK(browser.getVisiblePresetCount() == 3);
    browser.setSearchText({}); preview(browser, "analog-drive-collection");
}

TEST_CASE("Analog factory audition opens the active colour module after leaving the library",
          "[preset-browser][analog-presets][ui][preset-focus][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    LibraryFolder folder(processor);
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setSize(1000, 500); editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    auto* opener = find<juce::Button>(*editor, [](auto& b) { return b.getComponentID() == "header_preset_browser"; });
    auto* browser = find<fire::ui::PresetBrowserPanel>(*editor, [](auto&) { return true; });
    REQUIRE(opener); REQUIRE(browser);
    const auto entries = processor.statePresets.getBrowserEntries();
    for (const auto& scene : fire::factory::analog_drive::scenes)
    {
        CAPTURE(scene.name);
        if (scene.layout == fire::factory::analog_drive::Layout::master)
        {
            // A removed selection in the old chain must not enqueue a fallback
            // that later overrides the requested Master colour module.
            int slot = -1;
            while (slot < 2) {slot = processor.addInsertEffect(0, fire::effects::Type::reverb); REQUIRE(slot >= 0);}
            auto* panel = find<GlobalPanel>(*editor, [](auto&) { return true; });
            REQUIRE(panel); panel->focusInsertEffect(slot);
        }
        opener->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        REQUIRE(browser->isShowing());
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& item)
        { return item.key == "@factory/" + juce::String(scene.key); });
        REQUIRE(entry != entries.end());
        browser->onPresetSelected(entry->tag);
        const auto parameters = processor.treeState.copyState().toXmlString();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
        REQUIRE(browser->isShowing());
        for (auto* child : editor->getChildren())
            if (child != browser && dynamic_cast<juce::ResizableCornerComponent*>(child) == nullptr)
                CHECK_FALSE(child->isShowing());
        REQUIRE(browser->keyPressed(juce::KeyPress{juce::KeyPress::escapeKey}));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        const bool master = scene.layout == fire::factory::analog_drive::Layout::master;
        auto* tab = find<juce::Button>(*editor, [master](auto& b)
        { return b.getTitle() == (master ? "Master processing workspace" : "Band processing workspace"); });
        REQUIRE(tab); CHECK(tab->getToggleState());
        auto* mode = find<juce::ComboBox>(*editor, [&](auto& box)
        { return box.isShowing() && box.getText() == fire::analog::names[static_cast<size_t>(scene.model)]; });
        REQUIRE(mode);
        if (! master)
        {
            const int band = scene.layout == fire::factory::analog_drive::Layout::upperBand ? 1 : 0;
            auto* panel = find<BandPanel>(*editor, [](auto&) { return true; });
            REQUIRE(panel); CHECK(panel->getFocusBandNum() == band);
            CHECK(processor.getShapeMode(band + 1, -1) == scene.model + 12);
        }
        CHECK(processor.treeState.copyState().toXmlString() == parameters);
        CHECK(processor.statePresets.getCurrentPresetKey() == entry->key);
        if (juce::String(scene.key) == "analog-tape-studio-print") preview(*editor, "tape-studio-print-focused");
    }
}

TEST_CASE("Reopening an editor on Tape Studio Print displays its Master tape instead of bypassed Band Cubic",
          "[preset-browser][analog-presets][ui][preset-focus][state][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; processor.hasUpdateCheckBeenPerformed = true;
    LibraryFolder folder(processor);
    const auto entry = fire::factory::create(processor, fire::factory::presetCount - 3);
    REQUIRE(entry.front()->getStringAttribute("presetKey") == "@factory/analog-tape-studio-print");
    REQUIRE(state::loadStateFromXml(*entry.front(), processor));
    processor.statePresets.setCurrentPresetKey("@factory/analog-tape-studio-print");
    const auto parameters = processor.treeState.copyState().toXmlString();
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary); editor.setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    auto* mode = find<juce::ComboBox>(editor, [](auto& box)
    { return box.isShowing() && box.getText() == "Tape Saturation"; });
    REQUIRE(mode); CHECK(processor.getShapeMode(0, 0) == 23);
    CHECK(processor.treeState.copyState().toXmlString() == parameters);
}
