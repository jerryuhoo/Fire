#include <PluginEditor.h>
#include <GUI/EffectRackNavigation.h>
#include <GUI/Skin.h>
#include <catch2/catch_test_macros.hpp>

struct SkinNavigationMotionTestAccess
{
    static fire::ui::SpringValue workspace(const FireAudioProcessorEditor& editor)
    { return editor.workspaceSelection; }
    static fire::ui::SpringValue band(const BandPanel& panel)
    { return panel.moduleSelectionPosition; }
    static fire::ui::SpringValue master(const GlobalPanel& panel)
    { return panel.selectionY; }
    static fire::ui::SpringValue lfo(const LfoPanel& panel)
    { return panel.lfoSelectionPosition; }
    static fire::ui::SpringValue rack(const fire::ui::EffectRackNavigation& navigation)
    { return navigation.selectionY; }
    static const fire::ui::EffectRackNavigation& rack(const BandPanel& panel)
    { return panel.effectNavigation; }
    static const fire::ui::EffectRackNavigation& rack(const GlobalPanel& panel)
    { return panel.effectNavigation; }
    static float selectedRackRowY(const fire::ui::EffectRackNavigation& navigation)
    {
        const auto* selected = navigation.selectedButton();
        REQUIRE(selected != nullptr);
        return static_cast<float>(selected->getY());
    }
    // The UI-only entry point avoids touching the user's PropertiesFile.
    static void applySkin(FireAudioProcessorEditor& editor, fire::ui::Skin skin)
    { editor.applySkin(skin); }
    static void advanceWorkspaceFrame(FireAudioProcessorEditor& editor)
    { editor.advanceAnimations(1.0f / 60.0f); }
};

namespace
{
using Access = SkinNavigationMotionTestAccess;
using Skin = fire::ui::Skin;
constexpr float frameSeconds = 1.0f / 60.0f;

template <typename Type, typename Predicate>
Type* findMotionComponent(juce::Component& root, Predicate predicate)
{
    if (auto* result = dynamic_cast<Type*>(&root); result != nullptr && predicate(*result))
        return result;
    for (auto* child : root.getChildren())
        if (auto* result = findMotionComponent<Type>(*child, predicate)) return result;
    return nullptr;
}

template <typename Type>
Type& motionPanel(juce::Component& root)
{
    auto* result = findMotionComponent<Type>(root, [](const auto&) { return true; });
    REQUIRE(result != nullptr);
    return *result;
}

void selectMotionButton(juce::Component& root, const juce::String& text,
                        const juce::String& componentID = {})
{
    auto* button = findMotionComponent<juce::Button>(root, [&](const auto& candidate)
    {
        return (text.isEmpty() || candidate.getButtonText() == text)
            && (componentID.isEmpty()
                ? static_cast<bool>(candidate.getProperties().getWithDefault("fireModuleRail", false))
                : candidate.getComponentID() == componentID);
    });
    REQUIRE(button != nullptr);
    REQUIRE(button->isShowing());
    REQUIRE(button->isEnabled());
    button->setToggleState(true, juce::sendNotificationSync);
    REQUIRE(button->getToggleState());
}

void selectMotionWorkspace(FireAudioProcessorEditor& editor, const juce::String& name)
{ selectMotionButton(editor, name, "workspace_tab"); }

void prepareMotionEditor(FireAudioProcessorEditor& editor, Skin skin)
{
    editor.stopTimer();
    editor.setSize(1000, 500);
    Access::applySkin(editor, skin);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    REQUIRE(editor.isShowing());
    REQUIRE_FALSE(editor.isTimerRunning());
}

void expectSettled(const fire::ui::SpringValue& motion)
{
    CAPTURE(motion.current, motion.target, motion.velocity);
    CHECK(motion.current == motion.target);
    CHECK(motion.velocity == 0.0f);
    CHECK(motion.isSettled());
}

void expectMoving(const fire::ui::SpringValue& motion, bool alreadyAdvanced = false)
{
    CAPTURE(motion.current, motion.target, motion.velocity);
    CHECK(motion.current != motion.target);
    CHECK_FALSE(motion.isSettled());
    if (alreadyAdvanced) CHECK(motion.velocity != 0.0f);
}

void expectSkinMotion(const fire::ui::SpringValue& motion, Skin skin, bool advanced = false)
{
    if (skin == Skin::vintage) expectSettled(motion);
    else expectMoving(motion, advanced);
}

void expectRackMotion(const fire::ui::EffectRackNavigation& rack, Skin skin, bool advanced = false)
{
    const auto motion = Access::rack(rack);
    CHECK(motion.target == Access::selectedRackRowY(rack));
    expectSkinMotion(motion, skin, advanced);
}
}

TEST_CASE("Navigation springs remain active in Modern and select immediately in Vintage",
          "[skin][navigation-motion][ui][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto skin : {Skin::modern, Skin::vintage})
    {
        CAPTURE(static_cast<int>(skin));
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        FireAudioProcessorEditor editor(processor);
        prepareMotionEditor(editor, skin);
        auto& band = motionPanel<BandPanel>(editor);
        auto& master = motionPanel<GlobalPanel>(editor);
        auto& lfo = motionPanel<LfoPanel>(editor);

        selectMotionWorkspace(editor, "BAND LAB");
        selectMotionButton(band, "Compressor");
        CHECK(Access::band(band).target == 2.0f);
        expectSkinMotion(Access::band(band), skin);
        expectRackMotion(Access::rack(band), skin);
        band.animationTick(frameSeconds);
        expectSkinMotion(Access::band(band), skin, true);
        expectRackMotion(Access::rack(band), skin, true);

        selectMotionWorkspace(editor, "MASTER LAB");
        CHECK(Access::workspace(editor).target == 2.0f);
        expectSkinMotion(Access::workspace(editor), skin);
        Access::advanceWorkspaceFrame(editor);
        expectSkinMotion(Access::workspace(editor), skin, true);
        selectMotionButton(master, "Lo-Fi");
        expectSkinMotion(Access::master(master), skin);
        expectRackMotion(Access::rack(master), skin);
        master.animationTick(frameSeconds);
        expectSkinMotion(Access::master(master), skin, true);
        expectRackMotion(Access::rack(master), skin, true);

        selectMotionWorkspace(editor, "MOD FORGE");
        CHECK(Access::workspace(editor).target == 1.0f);
        expectSkinMotion(Access::workspace(editor), skin);
        Access::advanceWorkspaceFrame(editor);
        expectSkinMotion(Access::workspace(editor), skin, true);
        selectMotionButton(lfo, {}, "lfoBankSelect2");
        REQUIRE(lfo.getCurrentLfoIndex() == 1);
        CHECK(Access::lfo(lfo).target == 1.0f);
        expectSkinMotion(Access::lfo(lfo), skin);
        lfo.animationTick(frameSeconds);
        expectSkinMotion(Access::lfo(lfo), skin, true);

        // A rapid reversal must also snap immediately in Vintage.
        selectMotionButton(lfo, {}, "lfoBankSelect1");
        expectSkinMotion(Access::lfo(lfo), skin);
        selectMotionWorkspace(editor, "BAND LAB");
        CHECK(Access::workspace(editor).target == 0.0f);
        expectSkinMotion(Access::workspace(editor), skin);
        selectMotionButton(band, "Shape");
        CHECK(Access::band(band).target == 1.0f);
        expectSkinMotion(Access::band(band), skin);
        expectRackMotion(Access::rack(band), skin);
    }
}

TEST_CASE("Selecting added DSP modules obeys the skin's rack motion policy",
          "[skin][navigation-motion][ui][rack][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto skin : {Skin::modern, Skin::vintage})
    {
        CAPTURE(static_cast<int>(skin));
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        const auto slot = processor.addInsertEffect(0, fire::effects::Type::chorus);
        REQUIRE(slot >= 0);
        FireAudioProcessorEditor editor(processor);
        prepareMotionEditor(editor, skin);
        auto& master = motionPanel<GlobalPanel>(editor);
        selectMotionWorkspace(editor, "MASTER LAB");
        selectMotionButton(master, {}, fire::effects::parameterID(0, slot, fire::effects::typeField));
        expectRackMotion(Access::rack(master), skin);
        master.animationTick(frameSeconds);
        expectRackMotion(Access::rack(master), skin, true);
    }
}

TEST_CASE("Switching an in-flight Modern selection to Vintage clears its velocity immediately",
          "[skin][navigation-motion][ui][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    prepareMotionEditor(editor, Skin::modern);
    auto& band = motionPanel<BandPanel>(editor);
    auto& master = motionPanel<GlobalPanel>(editor);
    auto& lfo = motionPanel<LfoPanel>(editor);

    selectMotionWorkspace(editor, "MASTER LAB");
    Access::advanceWorkspaceFrame(editor);
    expectMoving(Access::workspace(editor), true);
    Access::applySkin(editor, Skin::vintage);
    expectSettled(Access::workspace(editor));
    CHECK(Access::workspace(editor).target == 2.0f);

    Access::applySkin(editor, Skin::modern);
    selectMotionButton(master, "Lo-Fi");
    master.animationTick(frameSeconds);
    expectMoving(Access::master(master), true);
    expectMoving(Access::rack(Access::rack(master)), true);
    Access::applySkin(editor, Skin::vintage);
    expectSettled(Access::master(master));
    expectRackMotion(Access::rack(master), Skin::vintage);

    Access::applySkin(editor, Skin::modern);
    selectMotionWorkspace(editor, "BAND LAB");
    selectMotionButton(band, "Compressor");
    band.animationTick(frameSeconds);
    expectMoving(Access::band(band), true);
    expectMoving(Access::rack(Access::rack(band)), true);
    Access::applySkin(editor, Skin::vintage);
    expectSettled(Access::band(band));
    expectRackMotion(Access::rack(band), Skin::vintage);
    expectSettled(Access::workspace(editor));

    Access::applySkin(editor, Skin::modern);
    selectMotionWorkspace(editor, "MOD FORGE");
    selectMotionButton(lfo, {}, "lfoBankSelect2");
    lfo.animationTick(frameSeconds);
    expectMoving(Access::lfo(lfo), true);
    Access::applySkin(editor, Skin::vintage);
    expectSettled(Access::lfo(lfo));
    CHECK(Access::lfo(lfo).target == 1.0f);
    expectSettled(Access::workspace(editor));

    // Returning to Modern restores motion, rather than permanently disabling
    // the editor's springs after the first Vintage selection.
    Access::applySkin(editor, Skin::modern);
    selectMotionButton(lfo, {}, "lfoBankSelect3");
    lfo.animationTick(frameSeconds);
    expectMoving(Access::lfo(lfo), true);
}
