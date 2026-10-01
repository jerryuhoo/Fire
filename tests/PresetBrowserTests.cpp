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

TEST_CASE("Factory catalogue is lightweight and every collection contains twenty sounds", "[preset-browser][factory-presets][catalogue]")
{
    FireAudioProcessor processor;
    LibraryFolder folder(processor);
    const auto entries = processor.statePresets.getBrowserEntries();
    int factoryCount = 0;
    for (const auto& entry : entries) if (entry.factory) {++factoryCount; CHECK(entry.key.startsWith("@factory/"));}
    CHECK(factoryCount == 200);
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
    {browser.selectCategory(category); CHECK(browser.getVisiblePresetCount() == 20);}
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
