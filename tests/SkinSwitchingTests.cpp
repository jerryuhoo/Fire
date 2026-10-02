#include <PluginEditor.h>
#include <GUI/SettingsComponent.h>
#include <GUI/PresetBrowserPanel.h>
#include <GUI/Skin.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <functional>
#include <memory>

struct SkinSwitchingTestAccess
{
    static void replaceSettings(FireAudioProcessor& processor,
                                std::unique_ptr<juce::PropertiesFile> properties)
    {
        processor.appProperties = std::move(properties);
    }
};

namespace
{
using fire::ui::Skin;

struct SkinTestFiles
{
    juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("FireSkinTests-" + juce::Uuid().toString());

    SkinTestFiles() { REQUIRE(root.createDirectory().wasOk()); }
    ~SkinTestFiles() { root.deleteRecursively(); }

    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options result;
        result.applicationName = "FireSkinTests";
        result.filenameSuffix = ".settings";
        result.millisecondsBeforeSaving = -1;
        result.storageFormat = juce::PropertiesFile::storeAsXML;
        return result;
    }

    juce::File settingsFile(const juce::String& name) const
    { return root.getChildFile(name + ".settings"); }

    void prepare(FireAudioProcessor& processor, const juce::String& name) const
    {
        SkinSwitchingTestAccess::replaceSettings(processor,
            std::make_unique<juce::PropertiesFile>(settingsFile(name), options()));
        processor.hasUpdateCheckBeenPerformed = true;
        const auto presets = root.getChildFile(name + "-presets");
        REQUIRE(presets.createDirectory().wasOk());
        processor.statePresets.setPresetDirectoryForTesting(presets);
        processor.statePresets.enableFactoryPresets();
    }
};

template <typename T, typename Predicate>
T* findSkinControl(juce::Component& parent, Predicate predicate)
{
    if (auto* control = dynamic_cast<T*>(&parent); control != nullptr && predicate(*control))
        return control;
    for (auto* child : parent.getChildren())
        if (auto* control = findSkinControl<T>(*child, predicate))
            return control;
    return nullptr;
}

juce::Button& skinButton(juce::Component& parent, const juce::String& id)
{
    auto* control = findSkinControl<juce::Button>(parent,
        [&](auto& button) { return button.getComponentID() == id; });
    REQUIRE(control != nullptr);
    return *control;
}

void selectWorkspace(FireAudioProcessorEditor& editor, const juce::String& text)
{
    auto* control = findSkinControl<juce::Button>(editor,
        [&](auto& button)
        { return button.getComponentID() == "workspace_tab" && button.getButtonText() == text; });
    REQUIRE(control != nullptr);
    control->triggerClick();
}

juce::Image skinSnapshot(juce::Component& component, const juce::String& name = {})
{
    const auto image = component.createComponentSnapshot(component.getLocalBounds());
    REQUIRE_FALSE(image.isNull());
    if (const auto* directory = std::getenv("FIRE_SKIN_PREVIEW_DIR"); directory && name.isNotEmpty())
    {
        const auto file = juce::File(directory).getChildFile(name + ".png");
        REQUIRE(file.getParentDirectory().createDirectory().wasOk());
        auto stream = file.createOutputStream();
        REQUIRE(stream != nullptr);
        stream->setPosition(0);
        stream->truncate();
        CHECK(juce::PNGImageFormat{}.writeImageToStream(image, *stream));
    }
    return image;
}

// A substantial pixel change catches cached panels retaining their old skin;
// a few anti-aliased glyphs or animated highlights cannot satisfy this check.
double changedPixelFraction(const juce::Image& a, const juce::Image& b)
{
    REQUIRE(a.getBounds() == b.getBounds());
    int changed = 0, count = 0;
    for (int y = 2; y < a.getHeight(); y += 4)
        for (int x = 2; x < a.getWidth(); x += 4)
        {
            const auto first = a.getPixelAt(x, y), second = b.getPixelAt(x, y);
            const auto distance = std::abs(int(first.getRed()) - int(second.getRed()))
                + std::abs(int(first.getGreen()) - int(second.getGreen()))
                + std::abs(int(first.getBlue()) - int(second.getBlue()));
            changed += distance > 18;
            ++count;
        }
    REQUIRE(count > 0);
    return static_cast<double>(changed) / count;
}

juce::String soundXml(FireAudioProcessor& processor)
{
    juce::XmlElement xml("Preset");
    state::saveStateToXml(processor, xml);
    return xml.toString();
}

void prepareEditor(FireAudioProcessorEditor& editor)
{
    // Peerless rendering settles workspace transitions immediately and keeps
    // preview checks independent of the system cursor and native focus.
    editor.stopTimer();
    editor.setVisible(true);
    editor.setSize(1000, 500);
}

class CallbackSkinProperties final : public juce::PropertiesFile
{
public:
    explicit CallbackSkinProperties(const juce::File& file)
        : juce::PropertiesFile(file, SkinTestFiles::options()) {}
    std::function<void()> onChange;
protected:
    void propertyChanged() override
    {
        const auto callback = onChange;
        if (callback) callback();
    }
};
}

TEST_CASE("Skin switching redraws every workspace and returns to Modern without changing the sound",
          "[skin][ui][render][background-cache][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SkinTestFiles files;
    FireAudioProcessor processor;
    files.prepare(processor, "workspaces");
    FireAudioProcessorEditor editor(processor);
    prepareEditor(editor);
    auto* drive = processor.treeState.getParameter("drive1");
    REQUIRE(drive != nullptr);
    const auto originalDrive = drive->getValue();
    drive->beginChangeGesture();
    drive->setValueNotifyingHost(0.61f);
    drive->endChangeGesture();
    REQUIRE(processor.canUndoEdit());
    REQUIRE_FALSE(processor.canRedoEdit());
    const auto liveParameters = processor.treeState.copyState().toXmlString();
    const auto presetBefore = soundXml(processor);
    const auto presetIdentity = processor.statePresets.getCurrentPresetKey();

    for (const auto* workspace : {"BAND LAB", "MASTER LAB", "MOD FORGE"})
    {
        CAPTURE(workspace);
        selectWorkspace(editor, workspace);
        const auto bounds = editor.getBounds();
        const auto label = juce::String(workspace).toLowerCase().replaceCharacter(' ', '-');
        editor.setSkinPreference(Skin::modern);
        const auto modern = skinSnapshot(editor, "modern-" + label);
        editor.setSkinPreference(Skin::vintage);
        CHECK(editor.getBounds() == bounds);
        CHECK(editor.getSkinPreference() == Skin::vintage);
        const auto vintage = skinSnapshot(editor, "vintage-" + label);
        CHECK(changedPixelFraction(modern, vintage) > 0.10);
        editor.setSkinPreference(Skin::modern);
        const auto restored = skinSnapshot(editor);
        CHECK(changedPixelFraction(modern, restored) < 0.005);
        CHECK(processor.treeState.copyState().toXmlString() == liveParameters);
        CHECK(soundXml(processor) == presetBefore);
        CHECK(processor.statePresets.getCurrentPresetKey() == presetIdentity);
    }

    skinButton(editor, "header_preset_browser").triggerClick();
    auto* browser = findSkinControl<fire::ui::PresetBrowserPanel>(editor, [](auto&) { return true; });
    REQUIRE(browser != nullptr);
    REQUIRE(browser->isVisible());
    browser->selectCategory("Vocals");
    browser->setSearchText("vocal");
    const auto category = browser->getSelectedCategory();
    const auto visibleCount = browser->getVisiblePresetCount();
    const auto modernBrowser = skinSnapshot(editor, "modern-preset-library");
    editor.setSkinPreference(Skin::vintage);
    const auto vintageBrowser = skinSnapshot(editor, "vintage-preset-library");
    CHECK(changedPixelFraction(modernBrowser, vintageBrowser) > 0.10);
    CHECK(browser->getSelectedCategory() == category);
    CHECK(browser->getVisiblePresetCount() == visibleCount);
    auto* search = findSkinControl<juce::TextEditor>(*browser,
        [](auto& control) { return control.getComponentID() == "preset_search"; });
    REQUIRE(search != nullptr);
    CHECK(search->getText() == "vocal");
    editor.setSkinPreference(Skin::modern);
    CHECK(changedPixelFraction(modernBrowser, skinSnapshot(editor)) < 0.005);
    CHECK(processor.treeState.copyState().toXmlString() == liveParameters);
    CHECK(soundXml(processor) == presetBefore);
    REQUIRE(processor.canUndoEdit());
    REQUIRE_FALSE(processor.canRedoEdit());
    REQUIRE(processor.undoEdit());
    CHECK(drive->getValue() == Catch::Approx(originalDrive));
    CHECK_FALSE(processor.canUndoEdit());
    REQUIRE(processor.redoEdit());
    CHECK(drive->getValue() == Catch::Approx(0.61f));
}

TEST_CASE("Skin preferences survive new editors while separate instances retain their own appearance",
          "[skin][ui][preferences][isolation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SkinTestFiles files;
    FireAudioProcessor first, second;
    files.prepare(first, "first");
    files.prepare(second, "second");
    auto editor = std::make_unique<FireAudioProcessorEditor>(first);
    prepareEditor(*editor);
    FireAudioProcessorEditor other(second);
    prepareEditor(other);
    const auto otherModern = skinSnapshot(other);
    REQUIRE(editor->getSkinPreference() == Skin::modern);
    editor->setSkinPreference(Skin::vintage);
    CHECK(other.getSkinPreference() == Skin::modern);
    CHECK(changedPixelFraction(otherModern, skinSnapshot(other)) == 0.0);
    CHECK(dynamic_cast<FireLookAndFeel&>(editor->getLookAndFeel()).getSkin() == Skin::vintage);
    CHECK(dynamic_cast<FireLookAndFeel&>(other.getLookAndFeel()).getSkin() == Skin::modern);
    editor.reset();
    editor = std::make_unique<FireAudioProcessorEditor>(first);
    prepareEditor(*editor);
    CHECK(editor->getSkinPreference() == Skin::vintage);
    juce::PropertiesFile diskCopy(files.settingsFile("first"), SkinTestFiles::options());
    CHECK(diskCopy.getIntValue(fire::ui::skinSetting, -1) == static_cast<int>(Skin::vintage));
    FireAudioProcessor reopened;
    files.prepare(reopened, "first");
    FireAudioProcessorEditor reopenedEditor(reopened);
    prepareEditor(reopenedEditor);
    CHECK(reopenedEditor.getSkinPreference() == Skin::vintage);
    other.setSkinPreference(Skin::vintage);
    editor->setSkinPreference(Skin::modern);
    CHECK(other.getSkinPreference() == Skin::vintage);
    CHECK(reopenedEditor.getSkinPreference() == Skin::vintage);
    CHECK(editor->getSkinPreference() == Skin::modern);
}

TEST_CASE("Settings skin buttons persist their choice and repaint the live editor at minimum sizes",
          "[skin][ui][settings][layout][preferences]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SkinTestFiles files;
    FireAudioProcessor processor;
    files.prepare(processor, "settings");
    FireAudioProcessorEditor editor(processor);
    prepareEditor(editor);
    SettingsComponent settings(processor.getAppSettings());
    settings.setSize(400, 300);
    settings.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    settings.setVisible(true);
    auto& modern = skinButton(settings, "skinModern");
    auto& vintage = skinButton(settings, "skinVintage");
    REQUIRE(modern.getToggleState());
    REQUIRE_FALSE(vintage.getToggleState());
    for (const auto size : {juce::Point<int>(SettingsComponent::minimumContentWidth,
                                           SettingsComponent::minimumContentHeight),
                            juce::Point<int>(300, 420), juce::Point<int>(450, 280)})
    {
        settings.setSize(size.x, size.y);
        CHECK(settings.getLocalBounds().contains(modern.getBounds()));
        CHECK(settings.getLocalBounds().contains(vintage.getBounds()));
        CHECK_FALSE(modern.getBounds().intersects(vintage.getBounds()));
        CHECK(vintage.getX() - modern.getRight() >= 8);
        CHECK(modern.getHeight() >= 28);
        CHECK(vintage.getHeight() >= 28);
        for (auto* child : settings.getChildren())
        {
            if (!child->isVisible() || child == &modern || child == &vintage) continue;
            CAPTURE(child->getTitle(), child->getBounds().toString());
            CHECK(settings.getLocalBounds().contains(child->getBounds()));
            CHECK_FALSE(child->getBounds().intersects(modern.getBounds()));
            CHECK_FALSE(child->getBounds().intersects(vintage.getBounds()));
        }
    }
    settings.setSize(400, 300);
    const auto modernImage = skinSnapshot(settings, "modern-settings");
    vintage.triggerClick();
    processor.getAppSettings().dispatchPendingMessages();
    CHECK(vintage.getToggleState());
    CHECK_FALSE(modern.getToggleState());
    CHECK(editor.getSkinPreference() == Skin::vintage);
    CHECK(fire::ui::skinFor(settings) == Skin::vintage);
    const auto vintageImage = skinSnapshot(settings, "vintage-settings");
    CHECK(changedPixelFraction(modernImage, vintageImage) > 0.10);
    {
        juce::PropertiesFile saved(files.settingsFile("settings"), SkinTestFiles::options());
        CHECK(saved.getIntValue(fire::ui::skinSetting, -1) == static_cast<int>(Skin::vintage));
    }
    modern.triggerClick();
    processor.getAppSettings().dispatchPendingMessages();
    CHECK(modern.getToggleState());
    CHECK_FALSE(vintage.getToggleState());
    CHECK(editor.getSkinPreference() == Skin::modern);
    CHECK(fire::ui::skinFor(settings) == Skin::modern);
    juce::PropertiesFile saved(files.settingsFile("settings"), SkinTestFiles::options());
    CHECK(saved.getIntValue(fire::ui::skinSetting, -1) == static_cast<int>(Skin::modern));
    settings.setVisible(false);
    vintage.triggerClick();
    CHECK(processor.getAppSettings().getIntValue(fire::ui::skinSetting, -1) == static_cast<int>(Skin::modern));
}

TEST_CASE("Saving an appearance choice may synchronously close its settings page",
          "[skin][ui][settings][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SkinTestFiles files;
    CallbackSkinProperties properties(files.settingsFile("callback"));
    auto settings = std::make_unique<SettingsComponent>(properties);
    settings->setSize(400, 300);
    settings->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    settings->setVisible(true);
    auto* vintage = &skinButton(*settings, "skinVintage");
    properties.onChange = [&] { settings.reset(); };
    vintage->triggerClick();
    CHECK(settings == nullptr);
    CHECK(properties.getIntValue(fire::ui::skinSetting, -1) == static_cast<int>(Skin::vintage));
}
