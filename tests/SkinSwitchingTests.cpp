#include <PluginEditor.h>
#include <GUI/SettingsComponent.h>
#include <GUI/PresetBrowserPanel.h>
#include <GUI/Skin.h>
#include <Utility/DriveCompensationParameters.h>
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
    // triggerClick posts a command message. Settle it before taking native
    // snapshots so a platform message pump cannot change the component tree
    // halfway through rendering a view that has not been selected yet.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    REQUIRE(control->getToggleState());
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

    for (const auto skin : {Skin::vintage,Skin::paper,Skin::ink})
    for (const auto* workspace : {"BAND LAB", "MASTER LAB", "MOD FORGE"})
    {
        CAPTURE(workspace,static_cast<int>(skin));
        selectWorkspace(editor, workspace);
        const auto bounds = editor.getBounds();
        const auto label = juce::String(workspace).toLowerCase().replaceCharacter(' ', '-');
        editor.setSkinPreference(Skin::modern);
        const auto modern = skinSnapshot(editor, "modern-" + label);
        editor.setSkinPreference(skin);
        CHECK(editor.getBounds() == bounds);
        CHECK(editor.getSkinPreference() == skin);
        const auto vintage = skinSnapshot(editor,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-"+label);
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

TEST_CASE("EQ and Drive layouts stay separated in both skins and all supported scales",
          "[skin][ui][layout][control-polish][regression][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SkinTestFiles files;
    FireAudioProcessor processor;
    files.prepare(processor, "controls");
    const auto setParameter = [&](const juce::String& id, float value)
    {
        auto* parameter = processor.treeState.getParameter(id);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    setParameter(fire::drive_comp::parameterID(0), 0.0f);
    setParameter("linked1", 1.0f);
    setParameter(fire::eq::parameterID(1, fire::eq::Field::frequency), 833.0f);
    setParameter(fire::eq::parameterID(1, fire::eq::Field::gain), -5.2f);
    setParameter(fire::eq::parameterID(1, fire::eq::Field::q), 1.8f);
    REQUIRE(processor.addInsertEffect(1, fire::effects::Type::eq) == 0);
    FireAudioProcessorEditor editor(processor);
    prepareEditor(editor);
    auto* band = findSkinControl<BandPanel>(editor, [](auto&) { return true; });
    auto* master = findSkinControl<GlobalPanel>(editor, [](auto&) { return true; });
    REQUIRE(band != nullptr); REQUIRE(master != nullptr);
    const auto selectModule = [&](juce::Component& panel, const juce::String& text)
    {
        auto* button = findSkinControl<juce::Button>(panel,
            [&](auto& b) { return b.getButtonText() == text; });
        REQUIRE(button != nullptr);
        button->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        auto* selected = findSkinControl<juce::Button>(panel,
            [&](auto& b) { return b.getButtonText() == text; });
        REQUIRE(selected != nullptr);
        REQUIRE(selected->getToggleState());
    };
    const auto checkEq = [&](EqControlsPanel& panel)
    {
        panel.refresh();
        for (auto* child : panel.getChildren())
        {
            if (! child->isVisible()) continue;
            CAPTURE(child->getTitle(), child->getBounds().toString());
            CHECK(panel.getLocalBounds().contains(child->getBounds()));
            for (auto* other : panel.getChildren())
                if (other != child && other->isVisible())
                    CHECK_FALSE(child->getBounds().intersects(other->getBounds()));
        }
    };
    for (const auto skin : fire::ui::skins)
    {
        editor.setSkinPreference(skin);
        const auto prefix = juce::String(fire::ui::skinName(skin)).toLowerCase()+"-";
        for (const int width : {1000, 1400, 2000})
        {
            CAPTURE(static_cast<int>(skin), width);
            editor.setSize(width, width / 2);
            selectWorkspace(editor, "BAND LAB");
            INFO("Rendering Band Drive");
            selectModule(*band, "Drive");
            band->animationTick(1.0f / 60.0f);
            auto& link = skinButton(*band, "linked1");
            auto& upgrade = skinButton(*band, "driveCompUpgrade");
            REQUIRE(upgrade.isVisible());
            CHECK(link.getY() == upgrade.getY());
            CHECK_FALSE(link.getBounds().intersects(upgrade.getBounds()));
            CHECK_FALSE(band->getDriveKnob()->getBounds().intersects(upgrade.getBounds()));
            skinSnapshot(editor, juce::String(prefix) + "legacy-drive-" + juce::String(width));

            INFO("Rendering Band Shape");
            selectModule(*band, "Shape");
            skinSnapshot(editor, juce::String(prefix) + "shape-selector-" + juce::String(width));
            INFO("Rendering Band EQ");
            selectModule(*band, "EQ");
            auto* insertedEq = findSkinControl<EqControlsPanel>(*band,
                [](auto& panel) { return panel.isVisible(); });
            REQUIRE(insertedEq != nullptr);
            checkEq(*insertedEq);
            skinSnapshot(editor, juce::String(prefix) + "band-eq-" + juce::String(width));

            INFO("Rendering Master EQ");
            selectWorkspace(editor, "MASTER LAB");
            selectModule(*master, "EQ");
            master->selectEqNode(1);
            checkEq(master->getEqControls());
            skinSnapshot(editor, juce::String(prefix) + "master-eq-" + juce::String(width));
        }
    }
    // Layout and skin changes must not migrate the legacy sound implicitly.
    CHECK(processor.treeState.getRawParameterValue(fire::drive_comp::parameterID(0))->load() == 0.0f);
    CHECK(processor.treeState.getRawParameterValue("linked1")->load() == 1.0f);
}

TEST_CASE("Paper and Ink preferences persist with readable settings and library surfaces",
          "[skin][line-skin][ui][preferences][layout][render][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SkinTestFiles files;
    FireAudioProcessor processor;
    files.prepare(processor,"line");
    FireAudioProcessorEditor editor(processor);
    prepareEditor(editor);
    const auto before = soundXml(processor);
    for (const auto skin : {Skin::paper,Skin::ink})
    {
        CAPTURE(static_cast<int>(skin));
        editor.setSkinPreference(skin);
        CHECK(processor.getAppSettings().getIntValue(fire::ui::skinSetting,-1)==static_cast<int>(skin));
        FireAudioProcessorEditor reopened(processor);
        prepareEditor(reopened);
        CHECK(reopened.getSkinPreference()==skin);
        SettingsComponent settings(processor.getAppSettings());
        settings.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        settings.setVisible(true);
        for (const auto size : {juce::Point<int>{300,250},juce::Point<int>{400,300}})
        {
            settings.setSize(size.x,size.y);
            for (const auto* id : {"skinModern","skinVintage","skinPaper","skinInk"})
            {
                const auto& button=skinButton(settings,id);
                CHECK(settings.getLocalBounds().contains(button.getBounds()));
                CHECK(button.getWidth()>=50);
                CHECK(button.getHeight()>=28);
            }
        }
        skinSnapshot(settings,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-settings");
        skinButton(reopened,"header_preset_browser").triggerClick();
        auto* library=findSkinControl<fire::ui::PresetBrowserPanel>(reopened,[](auto&) {return true;});
        REQUIRE(library!=nullptr); REQUIRE(library->isVisible());
        library->selectCategory("Analog Drive");
        CHECK(library->getVisiblePresetCount()==36);
        auto* search=findSkinControl<juce::TextEditor>(*library,[](auto&) {return true;});
        REQUIRE(search!=nullptr);
        const auto background=search->findColour(juce::TextEditor::backgroundColourId);
        const auto foreground=search->findColour(juce::TextEditor::textColourId);
        CHECK(std::abs(background.getPerceivedBrightness()-foreground.getPerceivedBrightness())>.50f);
        skinSnapshot(reopened,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-preset-library");
        skinButton(reopened,"preset_browser_close").triggerClick();
        CHECK(soundXml(processor)==before);
    }
    CHECK(fire::ui::skinFromValue(0)==Skin::modern);
    CHECK(fire::ui::skinFromValue(1)==Skin::vintage);
    CHECK(fire::ui::skinFromValue(2)==Skin::paper);
    CHECK(fire::ui::skinFromValue(3)==Skin::ink);
    CHECK(fire::ui::skinFromValue(44)==Skin::modern);
}

TEST_CASE("Shared website CSS colours retain their neutral roles through every skin",
          "[skin][line-skin][ui][theme][round-trip][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto& dark=fire::ui::skinPalette(Skin::ink);
    CHECK(dark.canvas==juce::Colour(0xff11161d));
    CHECK(dark.textPrimary==juce::Colour(0xffeeece4));
    CHECK(dark.textMuted==juce::Colour(0xffa7b0bd));
    CHECK(dark.hairline==juce::Colour(0xff343e4c));
    CHECK(dark.accent==juce::Colour(0xffefac88));
    const auto& source=fire::ui::skinPalette(Skin::modern);
    const std::array colours {source.canvas,source.surface0,source.surface1,source.surface2,
        source.raised,source.hairline,source.textPrimary,source.textSecondary,source.textMuted,source.textBright};
    juce::Component root;
    std::array<juce::Component,10> controls;
    for (size_t index=0;index<controls.size();++index)
    {
        root.addAndMakeVisible(controls[index]);
        controls[index].setColour(juce::Label::textColourId,colours[index].withAlpha(.73f));
    }
    auto previous=Skin::modern;
    for (const auto skin : {Skin::paper,Skin::ink,Skin::vintage,Skin::modern})
    {
        fire::ui::remapSkinColours(root,previous,skin);
        previous=skin;
    }
    for(size_t index=0;index<controls.size();++index)
        CHECK(controls[index].findColour(juce::Label::textColourId)==colours[index].withAlpha(.73f));
}
