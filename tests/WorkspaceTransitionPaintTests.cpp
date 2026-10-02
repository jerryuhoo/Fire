#include <PluginEditor.h>
#include <GUI/Skin.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
template <typename Type, typename Predicate>
Type* findTransitionControl(juce::Component& root, Predicate predicate)
{
    if (auto* result = dynamic_cast<Type*>(&root); result != nullptr && predicate(*result))
        return result;
    for (auto* child : root.getChildren())
        if (auto* result = findTransitionControl<Type>(*child, predicate))
            return result;
    return nullptr;
}

juce::Button& transitionButton(juce::Component& root, const juce::String& text,
                               bool workspace)
{
    auto* result = findTransitionControl<juce::Button>(root, [&](const auto& button)
    {
        return button.getButtonText() == text
            && (workspace ? button.getComponentID() == "workspace_tab"
                          : static_cast<bool>(button.getProperties().getWithDefault("fireModuleRail", false)));
    });
    REQUIRE(result != nullptr);
    REQUIRE(result->isShowing());
    REQUIRE(result->isEnabled());
    return *result;
}

void selectWorkspaceForPaint(FireAudioProcessorEditor& editor, const juce::String& name)
{
    auto& button = transitionButton(editor, name, true);
    button.setToggleState(true, juce::sendNotificationSync);
    REQUIRE(button.getToggleState());
    REQUIRE_FALSE(editor.isTimerRunning());
}

void prepareTransitionEditor(FireAudioProcessorEditor& editor, fire::ui::Skin skin)
{
    editor.stopTimer();
    editor.setSize(1000, 500);
    // Exercise both render palettes without writing the user's appearance
    // preference. No private editor state or friend access is required.
    const auto previous = fire::ui::skinFor(editor);
    fire::ui::setSkin(editor, skin);
    auto* look = dynamic_cast<FireLookAndFeel*>(&editor.getLookAndFeel());
    REQUIRE(look != nullptr);
    look->setSkin(skin);
    fire::ui::remapSkinColours(editor, previous, skin);
    editor.sendLookAndFeelChange();
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    REQUIRE(editor.isShowing());
    REQUIRE_FALSE(editor.isTimerRunning());
}

void expectNoTransitionOverlay(juce::Component& component)
{
    REQUIRE(component.getWidth() > 0);
    REQUIRE(component.getHeight() > 0);
    juce::Image overlay(juce::Image::ARGB, component.getWidth(), component.getHeight(), true);
    {
        juce::Graphics graphics(overlay);
        component.paintOverChildren(graphics);
    }
    // Examine the actual overlay callback, independently of the panel's
    // intentional dark instrument screens or hardware artwork.
    const juce::Image::BitmapData pixels(overlay, juce::Image::BitmapData::readOnly);
    juce::Point<int> firstCoveredPixel {-1, -1};
    int opacity = 0;
    for (int y = 0; y < overlay.getHeight() && opacity == 0; ++y)
        for (int x = 0; x < overlay.getWidth(); ++x)
            if (const auto alpha = pixels.getPixelColour(x, y).getAlpha(); alpha != 0)
            {
                firstCoveredPixel = {x, y};
                opacity = alpha;
                break;
            }
    CAPTURE(component.getName(), firstCoveredPixel.toString(), opacity);
    CHECK(opacity == 0);
}

void expectOnlyWorkspaceVisible(FireAudioProcessorEditor& editor, const juce::String& name)
{
    auto* band = findTransitionControl<BandPanel>(editor, [](const auto&) { return true; });
    auto* master = findTransitionControl<GlobalPanel>(editor, [](const auto&) { return true; });
    auto* lfo = findTransitionControl<LfoPanel>(editor, [](const auto&) { return true; });
    REQUIRE(band != nullptr);
    REQUIRE(master != nullptr);
    REQUIRE(lfo != nullptr);
    CHECK(band->isVisible() == (name == "BAND LAB"));
    CHECK(master->isVisible() == (name == "MASTER LAB"));
    CHECK(lfo->isVisible() == (name == "MOD FORGE"));
    for (auto* panel : {static_cast<juce::Component*>(band),
                        static_cast<juce::Component*>(master),
                        static_cast<juce::Component*>(lfo)})
        if (panel->isVisible())
        {
            CHECK(panel->getAlpha() == 1.0f);
            expectNoTransitionOverlay(*panel);
        }
}
}

TEST_CASE("Workspace changes never cover the new page while the UI clock is paused",
          "[ui][workspace][transition-paint][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto skin : {fire::ui::Skin::modern, fire::ui::Skin::vintage})
    {
        CAPTURE(static_cast<int>(skin));
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        REQUIRE_FALSE(processor.getBypassedState());
        FireAudioProcessorEditor editor(processor);
        prepareTransitionEditor(editor, skin);
        // Before the fix, the first changed workspace writes an 85%-opaque
        // dark rectangle here, and cannot remove it until the timer advances.
        for (const auto* name : {"BAND LAB", "MASTER LAB", "MOD FORGE", "BAND LAB"})
        {
            CAPTURE(name);
            selectWorkspaceForPaint(editor, name);
            expectOnlyWorkspaceVisible(editor, name);
            expectNoTransitionOverlay(editor);
        }
        // No dispatch loop or animation tick is allowed between rapid changes.
        for (int repeat = 0; repeat < 3; ++repeat)
            for (const auto* name : {"MASTER LAB", "BAND LAB", "MOD FORGE", "MASTER LAB"})
            {
                CAPTURE(repeat, name);
                selectWorkspaceForPaint(editor, name);
                expectNoTransitionOverlay(editor);
            }
        editor.setVisible(false);
        editor.setVisible(true);
        REQUIRE(editor.isShowing());
        REQUIRE_FALSE(editor.isTimerRunning());
        expectOnlyWorkspaceVisible(editor, "MASTER LAB");
        expectNoTransitionOverlay(editor);
        selectWorkspaceForPaint(editor, "BAND LAB");
        expectNoTransitionOverlay(editor);
    }
}

TEST_CASE("Module selection paints no content veil before any animation tick",
          "[ui][workspace][module][transition-paint][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto skin : {fire::ui::Skin::modern, fire::ui::Skin::vintage})
    {
        CAPTURE(static_cast<int>(skin));
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        REQUIRE_FALSE(processor.getBypassedState());
        FireAudioProcessorEditor editor(processor);
        prepareTransitionEditor(editor, skin);
        auto* band = findTransitionControl<BandPanel>(editor, [](const auto&) { return true; });
        auto* master = findTransitionControl<GlobalPanel>(editor, [](const auto&) { return true; });
        REQUIRE(band != nullptr);
        REQUIRE(master != nullptr);
        selectWorkspaceForPaint(editor, "BAND LAB");
        for (const auto* name : {"Shape", "Compressor", "Stereo", "Drive", "Shape"})
        {
            CAPTURE(name);
            auto& button = transitionButton(*band, name, false);
            button.setToggleState(true, juce::sendNotificationSync);
            REQUIRE(button.getToggleState());
            // The old PanelBase immediately painted a 65%-opaque rectangle
            // over the selected module's graph and controls.
            expectNoTransitionOverlay(*band);
        }
        selectWorkspaceForPaint(editor, "MASTER LAB");
        for (const auto* name : {"Lo-Fi", "EQ", "Lo-Fi"})
        {
            CAPTURE(name);
            auto& button = transitionButton(*master, name, false);
            button.setToggleState(true, juce::sendNotificationSync);
            REQUIRE(button.getToggleState());
            expectNoTransitionOverlay(*master);
        }
        editor.setVisible(false);
        editor.setVisible(true);
        REQUIRE(editor.isShowing());
        REQUIRE_FALSE(editor.isTimerRunning());
        expectNoTransitionOverlay(*master);
        selectWorkspaceForPaint(editor, "BAND LAB");
        expectNoTransitionOverlay(*band);
    }
}
