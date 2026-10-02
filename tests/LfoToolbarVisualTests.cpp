#include <PluginEditor.h>
#include <GUI/FireIcons.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <catch2/catch_test_macros.hpp>
#include "helpers/RepaintRecorder.h"

#include <array>
#include <cstdlib>
#include <vector>

namespace
{
template <typename Type, typename Predicate>
Type* findToolbarComponent(juce::Component& root, Predicate predicate)
{
    if (auto* result = dynamic_cast<Type*>(&root); result != nullptr && predicate(*result))
        return result;
    for (auto* child : root.getChildren())
        if (auto* result = findToolbarComponent<Type>(*child, predicate))
            return result;
    return nullptr;
}

PrimaryTextButton& tool(juce::Component& root, const juce::String& id)
{
    auto* result = findToolbarComponent<PrimaryTextButton>(root,
        [&](const auto& button) { return button.getComponentID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}

LfoBrushSelector& brushMenu(LfoPanel& panel)
{
    auto* result = findToolbarComponent<LfoBrushSelector>(panel, [](const auto&) { return true; });
    REQUIRE(result != nullptr);
    return *result;
}

LfoPanel& lfoPanel(FireAudioProcessorEditor& editor)
{
    auto* result = findToolbarComponent<LfoPanel>(editor, [](const auto&) { return true; });
    REQUIRE(result != nullptr);
    return *result;
}

void selectLfoWorkspace(FireAudioProcessorEditor& editor)
{
    auto* tab = findToolbarComponent<juce::Button>(editor,
        [](const auto& button) { return button.getButtonText() == "MOD FORGE"; });
    REQUIRE(tab != nullptr);
    tab->triggerClick();
}

void show(juce::Component& component)
{
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    REQUIRE(component.isShowing());
}

void checkLayout(LfoPanel& panel, float scale)
{
    auto* canvas = findToolbarComponent<LfoEditor>(panel, [](const auto&) { return true; });
    REQUIRE(canvas != nullptr);
    const auto canvasBounds = panel.getLocalArea(canvas, canvas->getLocalBounds());
    REQUIRE_FALSE(canvasBounds.isEmpty());
    std::vector<juce::Component*> controls;
    for (const auto* id : {"lfo_matrix", "lfo_sync", "lfo_assign", "lfo_edit_mode", "lfo_brush_mode"})
    {
        auto& button = tool(panel, id);
        CAPTURE(id, scale, button.getBounds().toString());
        CHECK(button.isShowing());
        CHECK(button.getHeight() >= juce::roundToInt(32.0f * scale));
        CHECK(button.getWidth() >= juce::roundToInt(32.0f * scale));
        // The complete tile remains interactive, not just the painted glyph.
        CHECK(button.hitTest(1, button.getHeight() / 2));
        CHECK(button.hitTest(button.getWidth() - 2, button.getHeight() / 2));
        CHECK(button.hitTest(button.getWidth() / 2, 1));
        CHECK(button.hitTest(button.getWidth() / 2, button.getHeight() - 2));
        if (juce::String(id) == "lfo_matrix" || juce::String(id) == "lfo_edit_mode"
            || juce::String(id) == "lfo_brush_mode")
            CHECK(std::abs(button.getWidth() - button.getHeight()) <= 1);
        controls.push_back(&button);
    }
    CHECK(tool(panel, "lfo_assign").getWidth() <= juce::roundToInt(128.0f * scale));
    CHECK(tool(panel, "lfo_sync").getWidth() >= juce::roundToInt(48.0f * scale) - 1);
    const auto firstRow = tool(panel, "lfo_matrix").getBounds();
    // A second tool row previously consumed another 34 px plus its gap,
    // substantially reducing the editable curve at the 1000 x 500 minimum.
    CHECK(canvasBounds.getY() > firstRow.getBottom());
    CHECK(canvasBounds.getY() - firstRow.getBottom() <= juce::roundToInt(7.0f * scale));
    auto& combo = brushMenu(panel);
    if (combo.isShowing()) controls.push_back(&combo);
    for (size_t first = 0; first < controls.size(); ++first)
    {
        const auto bounds = panel.getLocalArea(controls[first], controls[first]->getLocalBounds());
        CAPTURE(controls[first]->getTitle(), bounds.toString());
        CHECK(panel.getLocalBounds().contains(bounds));
        CHECK(bounds.getY() == firstRow.getY());
        CHECK(bounds.getHeight() == firstRow.getHeight());
        CHECK(bounds.getBottom() <= canvasBounds.getY());
        for (size_t second = first + 1; second < controls.size(); ++second)
            CHECK_FALSE(bounds.intersects(panel.getLocalArea(controls[second], controls[second]->getLocalBounds())));
    }
}

void checkFeedbackFits(PrimaryTextButton& assign, float scale)
{
    // Keep full feedback available to assistive technology and tooltips.
    // Only draw the text when it fits naturally beside the status icon.
    const auto font = fire::ui::labelFont(juce::jmin(12.0f * scale, assign.getHeight() * 0.38f));
    const auto textWidth = juce::GlyphArrangement::getStringWidth(font, assign.getButtonText());
    const auto availableWidth = assign.getWidth() - (16.0f + 20.0f + 5.0f) * scale;
    CAPTURE(assign.getButtonText(), textWidth, availableWidth, scale);
    if (static_cast<bool>(assign.getProperties()["fireToolLabel"]))
        CHECK(textWidth <= availableWidth);
    else
    {
        CHECK(textWidth > availableWidth);
        CHECK(assign.getTooltip() == assign.getButtonText());
        CHECK(assign.getHelpText() == assign.getButtonText());
        REQUIRE(assign.getAccessibilityHandler() != nullptr);
        CHECK(assign.getAccessibilityHandler()->getHelp().contains(assign.getButtonText()));
    }
}

bool snapshotsRequested()
{
    const auto* directory = std::getenv("FIRE_LIGHT_UI_SNAPSHOT_DIR");
    return directory != nullptr && *directory != '\0';
}

void prepareSnapshotShapes(FireAudioProcessor& processor)
{
    if (! snapshotsRequested()) return;
    // A musical rise-and-release shape makes the optional review images
    // useful; ordinary test runs retain the unmodified default LFO shapes.
    LfoData shape;
    shape.points = {{0.0f, 0.16f}, {0.22f, 0.90f}, {0.62f, 0.32f}, {1.0f, 0.16f}};
    shape.curvatures = {0.65f, -0.55f, 0.40f};
    for (int source : {0, 15})
        if (processor.isLfoPresent(source))
            processor.getLfoManager().setLfoData(source, shape);
}

void snapshot(juce::Component& component, const juce::String& name)
{
    const auto* directory = std::getenv("FIRE_LIGHT_UI_SNAPSHOT_DIR");
    if (directory == nullptr || *directory == '\0') return;
    const auto file = juce::File(juce::String::fromUTF8(directory)).getChildFile(name + ".png");
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), *stream));
}
}

TEST_CASE("LFO toolbar stays on one row with accessible Assign feedback across editor scales",
          "[lfo-toolbar][ui][layout][accessibility][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    for (int index = fire::lfo_bank::defaultCount; index < fire::lfo_bank::capacity; ++index)
        REQUIRE(processor.addLfo() == index);
    prepareSnapshotShapes(processor);
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    // Select before attaching the peer so the screenshot starts with a fully
    // revealed workspace, independent of wall-clock transition timing.
    selectLfoWorkspace(editor);
    show(editor);
    auto& panel = lfoPanel(editor);
    auto& assign = tool(panel, "lfo_assign");
    tool(panel, "lfoBankSelect16").triggerClick();
    REQUIRE(panel.getCurrentLfoIndex() == 15);
    if (snapshotsRequested())
        // Settle the rail before showing timed Assign feedback. The editor's
        // shared timer is deliberately stopped for deterministic snapshots.
        for (int frame = 0; frame < 90; ++frame)
            panel.animationTick(1.0f / 60.0f);

    for (int width : {1000, 1400, 2000})
    {
        CAPTURE(width);
        const auto scale = static_cast<float>(width) / 1000.0f;
        editor.setSize(width, width / 2);
        tool(panel, "lfo_edit_mode").triggerClick();
        REQUIRE_FALSE(brushMenu(panel).isShowing());
        checkLayout(panel, scale);
        auto* curve = findToolbarComponent<LfoEditor>(panel, [](const auto&) { return true; });
        REQUIRE(curve != nullptr);
        const auto curveBounds = curve->getBounds();
        const auto assignBounds = assign.getBounds();
        snapshot(editor, "lfo-toolbar-points-" + juce::String(width));
        tool(panel, "lfo_brush_mode").triggerClick();
        REQUIRE(brushMenu(panel).isShowing());
        checkLayout(panel, scale);
        CHECK(curve->getBounds() == curveBounds);
        CHECK(assign.getBounds() == assignBounds);
        snapshot(editor, "lfo-toolbar-brush-" + juce::String(width));

        panel.showAssignArmed(15);
        CHECK(assign.getButtonText() == "Assign LFO 16");
        CHECK(assign.getToggleState());
        CHECK(static_cast<int>(assign.getProperties()["fireToolStatus"]) == 1);
        CHECK(assign.getTooltip() == assign.getButtonText());
        CHECK(assign.getAccessibilityHandler()->getCurrentState().isChecked());
        checkFeedbackFits(assign, scale);
        snapshot(editor, "lfo-toolbar-armed-" + juce::String(width));
        panel.showAssignCompleted(15);
        CHECK(assign.getButtonText() == "LFO 16 Assigned");
        CHECK_FALSE(assign.getToggleState());
        CHECK(static_cast<int>(assign.getProperties()["fireToolStatus"]) == 2);
        CHECK_FALSE(assign.getAccessibilityHandler()->getCurrentState().isChecked());
        checkFeedbackFits(assign, scale);
        panel.showAssignUnchanged(15);
        CHECK(assign.getBounds() == assignBounds);
        CHECK(curve->getBounds() == curveBounds);
        CHECK(assign.getButtonText() == "LFO 16 Already Assigned");
        CHECK(static_cast<int>(assign.getProperties()["fireToolStatus"]) == 2);
        checkFeedbackFits(assign, scale);
        snapshot(editor, "lfo-toolbar-feedback-" + juce::String(width));
        panel.showAssignCapacityReached();
        CHECK(static_cast<int>(assign.getProperties()["fireToolStatus"]) == 3);
        checkFeedbackFits(assign, scale);
        panel.showAssignCancelled();
        CHECK(static_cast<int>(assign.getProperties()["fireToolStatus"]) == 0);
        checkFeedbackFits(assign, scale);
        panel.clearAssignFeedback();
        CHECK(assign.getButtonText() == "Assign");
        CHECK(static_cast<int>(assign.getProperties()["fireToolStatus"]) == 0);
    }
}

TEST_CASE("Narrow LFO toolbars fit one row without shrinking icon hit targets or covering the curve",
          "[lfo-toolbar][ui][layout][narrow][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    prepareSnapshotShapes(processor);
    FireLookAndFeel theme;
    LfoPanel panel(processor);
    panel.setLookAndFeel(&theme);
    panel.setSize(640, 360);
    show(panel);
    for (const auto scale : {1.0f, 1.5f})
    {
        theme.scale = scale;
        panel.setScale(scale);
        panel.setSize(juce::roundToInt(640.0f * scale), juce::roundToInt(360.0f * scale));
        tool(panel, "lfo_edit_mode").triggerClick();
        REQUIRE_FALSE(brushMenu(panel).isShowing());
        checkLayout(panel, scale);
        tool(panel, "lfo_brush_mode").triggerClick();
        REQUIRE(brushMenu(panel).isShowing());
        checkLayout(panel, scale);
        CHECK(tool(panel, "lfo_edit_mode").getY() == tool(panel, "lfo_matrix").getY());
        panel.showAssignUnchanged(15);
        checkFeedbackFits(tool(panel, "lfo_assign"), scale);
        snapshot(panel, "lfo-toolbar-narrow-" + juce::String(juce::roundToInt(scale * 100)));
    }
    panel.setLookAndFeel(nullptr);
}

TEST_CASE("Compact Shape Forge toolbar reclaims the curve below the former wrap threshold",
          "[lfo-toolbar][ui][layout][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireLookAndFeel theme;
    LfoPanel panel(processor);
    panel.setLookAndFeel(&theme);
    panel.setSize(640, 300);
    show(panel);
    for (const int width : {640, 820, 940, 984})
    {
        CAPTURE(width);
        panel.setSize(width, 300);
        tool(panel, "lfo_brush_mode").triggerClick();
        auto* curve = findToolbarComponent<LfoEditor>(panel, [](const auto&) { return true; });
        REQUIRE(curve != nullptr);
        if (width <= 940) CHECK(curve->getWidth() < 478);
        checkLayout(panel, 1.0f);
        const auto curveBounds = curve->getBounds();
        panel.showAssignUnchanged(15);
        checkFeedbackFits(tool(panel, "lfo_assign"), 1.0f);
        CHECK(curve->getBounds() == curveBounds);
        CHECK(curveBounds.getHeight() > 200);
    }
    // Hosts may momentarily restore below supported minimum dimensions.
    // Those frames may reduce hit areas but must never overlap or spill out.
    panel.setSize(400, 200);
    std::vector<juce::Component*> controls;
    for (const auto* id : {"lfo_matrix", "lfo_sync", "lfo_assign", "lfo_edit_mode", "lfo_brush_mode"})
        controls.push_back(&tool(panel, id));
    controls.push_back(&brushMenu(panel));
    for (size_t index = 0; index < controls.size(); ++index)
    {
        CHECK(panel.getLocalBounds().contains(controls[index]->getBounds()));
        for (size_t next = index + 1; next < controls.size(); ++next)
            CHECK_FALSE(controls[index]->getBounds().intersects(controls[next]->getBounds()));
    }
    panel.setLookAndFeel(nullptr);
}

TEST_CASE("LFO vector tool buttons retain semantic text roles and keyboard mode switching",
          "[lfo-toolbar][ui][keyboard][accessibility][icons]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    selectLfoWorkspace(editor);
    show(editor);
    auto& panel = lfoPanel(editor);
    struct Expected
    {
        const char* id;
        const char* text;
        fire::ui::Icon icon;
        bool labelled;
        juce::AccessibilityRole role;
    };
    const std::array expected {
        Expected {"lfo_matrix", "Matrix", fire::ui::Icon::matrix, false, juce::AccessibilityRole::button},
        Expected {"lfo_sync", "BPM", fire::ui::Icon::none, true, juce::AccessibilityRole::button},
        Expected {"lfo_assign", "Assign", fire::ui::Icon::assign, true, juce::AccessibilityRole::button},
        Expected {"lfo_edit_mode", "Edit Mode", fire::ui::Icon::points, false, juce::AccessibilityRole::radioButton},
        Expected {"lfo_brush_mode", "Brush Mode", fire::ui::Icon::brush, false, juce::AccessibilityRole::radioButton}
    };
    for (size_t index = 0; index < expected.size(); ++index)
    {
        const auto& item = expected[index];
        auto& button = tool(panel, item.id);
        CAPTURE(item.id);
        CHECK(button.getButtonText() == item.text);
        CHECK(static_cast<bool>(button.getProperties()["fireToolButton"]));
        CHECK(static_cast<int>(button.getProperties()["fireToolIcon"]) == static_cast<int>(item.icon));
        CHECK(static_cast<bool>(button.getProperties()["fireToolLabel"]) == item.labelled);
        CHECK(button.getExplicitFocusOrder() == static_cast<int>(index + 1));
        CHECK_FALSE(button.getTitle().isEmpty());
        CHECK_FALSE(button.getTooltip().isEmpty());
        CHECK_FALSE(button.getHelpText().isEmpty());
        auto* accessible = button.getAccessibilityHandler();
        REQUIRE(accessible != nullptr);
        CHECK(accessible->getRole() == item.role);
        CHECK_FALSE(accessible->getTitle().isEmpty());
        CHECK_FALSE(accessible->getHelp().isEmpty());
        if (! item.labelled)
        {
            // Changing semantic text must not accidentally reintroduce it
            // beneath or beside an icon-only toolbar drawing.
            const auto image = button.createComponentSnapshot(button.getLocalBounds());
            button.setButtonText("Semantic name is not painted here");
            CHECK(repaintTestImagesMatch(image, button.createComponentSnapshot(button.getLocalBounds())));
            button.setButtonText(item.text);
        }
    }
    CHECK(brushMenu(panel).getExplicitFocusOrder() == 6);
    auto& points = tool(panel, "lfo_edit_mode");
    auto& brush = tool(panel, "lfo_brush_mode");
    REQUIRE(points.getToggleState());
    REQUIRE_FALSE(brush.getToggleState());
    brush.grabKeyboardFocus();
    REQUIRE(brush.hasKeyboardFocus(false));
    CHECK(brush.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
    CHECK(brush.getToggleState());
    CHECK_FALSE(points.getToggleState());
    CHECK(brushMenu(panel).isShowing());
    CHECK(brush.getAccessibilityHandler()->getCurrentState().isChecked());
    CHECK_FALSE(points.getAccessibilityHandler()->getCurrentState().isChecked());
    points.grabKeyboardFocus();
    REQUIRE(points.hasKeyboardFocus(false));
    CHECK(points.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)));
    CHECK(points.getToggleState());
    CHECK_FALSE(brush.getToggleState());
    CHECK_FALSE(brushMenu(panel).isShowing());
    CHECK(points.getAccessibilityHandler()->getCurrentState().isChecked());

    panel.setEnabled(false);
    CHECK_FALSE(brush.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
    CHECK(points.getToggleState());
    CHECK_FALSE(brush.getToggleState());
    panel.setEnabled(true);
    panel.setVisible(false);
    CHECK_FALSE(brush.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
    CHECK(points.getToggleState());
    CHECK_FALSE(brush.getToggleState());
}
