#include <Panels/ControlPanel/LfoPanel.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>

struct LfoEditorTestAccess
{
    static bool interactionStateIsValid(const LfoEditor& editor)
    {
        if (editor.draggingPointIndex != -1)
            return false;

        if (! editor.hasValidSelectedPointIndices())
            return false;

        if (editor.editingCurveIndex != -1
            && ! editor.isValidCurveIndex(editor.editingCurveIndex))
            return false;

        if (editor.hoveredPointIndex != -1
            && ! editor.isValidPointIndex(editor.hoveredPointIndex))
            return false;

        const bool isPointDrag = editor.draggingState == LfoEditor::DraggingState::Point
                              || editor.draggingState == LfoEditor::DraggingState::Selection;
        if (isPointDrag)
            return editor.hasValidPointDragState();

        if (editor.isBrushing
            && editor.currentMode != LfoEditMode::BrushPaint)
            return false;

        if (! editor.isBrushing
            && editor.lastBrushCell != juce::Point<int>(-1, -1))
            return false;

        return editor.initialDragPositions.empty();
    }

    static void selectAllPoints(LfoEditor& editor)
    {
        editor.selectAllPoints();
    }

    static void addPoint(LfoEditor& editor, juce::Point<float> point)
    {
        editor.addPoint(point);
    }

    static void invertHorizontally(LfoEditor& editor)
    {
        editor.invertShape(true, false);
    }

    static void clearAllPoints(LfoEditor& editor)
    {
        editor.clearAllPoints();
    }

    static bool canPasteShape(const LfoEditor& editor)
    {
        return editor.canPasteShape();
    }

    static size_t pointCount(const LfoEditor& editor)
    {
        return editor.activeLfoData.points.size();
    }

    static size_t selectedPointCount(const LfoEditor& editor)
    {
        return editor.selectedPointIndices.size();
    }

    static juce::Point<float> pointScreenPosition(LfoEditor& editor, int index)
    {
        REQUIRE(editor.isValidPointIndex(index));
        return editor.fromNormalized(editor.activeLfoData.points[static_cast<size_t>(index)]);
    }

    static const LfoData& data(const LfoEditor& editor)
    {
        return editor.activeLfoData;
    }

    static bool isBrushing(const LfoEditor& editor)
    {
        return editor.isBrushing;
    }

    static juce::Point<int> lastBrushCell(const LfoEditor& editor)
    {
        return editor.lastBrushCell;
    }

    static void seedInvalidCurveInteraction(LfoEditor& editor)
    {
        editor.cancelAllInteraction();
        editor.editingCurveIndex = static_cast<int>(editor.activeLfoData.points.size()) - 1;
    }

    static void seedInvalidPointIndexDrag(LfoEditor& editor)
    {
        editor.cancelAllInteraction();
        editor.selectedPointIndices = {
            0,
            static_cast<int>(editor.activeLfoData.points.size()) + 3
        };
        editor.initialDragPositions = {
            editor.activeLfoData.points.front(),
            editor.activeLfoData.points.front()
        };
        editor.draggingState = LfoEditor::DraggingState::Selection;
    }

    static void seedMismatchedPointDrag(LfoEditor& editor)
    {
        editor.cancelAllInteraction();
        editor.selectedPointIndices = { 0, 1 };
        editor.initialDragPositions = { editor.activeLfoData.points.front() };
        editor.draggingState = LfoEditor::DraggingState::Selection;
    }

    static bool validateCurveInteractionOrCancel(LfoEditor& editor)
    {
        return editor.validateCurveInteractionOrCancel();
    }

    static bool validatePointDragInteractionOrCancel(LfoEditor& editor)
    {
        return editor.validatePointDragInteractionOrCancel();
    }
};

namespace
{
void prepareEditor(LfoEditor& editor)
{
    editor.setBounds(0, 0, 400, 200);
}

LfoData makeLfoData(std::initializer_list<juce::Point<float>> points)
{
    LfoData data;
    data.points.assign(points.begin(), points.end());
    data.curvatures.assign(data.points.size() - 1, 0.0f);
    return data;
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers = {},
                                juce::Point<float> mouseDownPosition = {})
{
    if (mouseDownPosition == juce::Point<float>())
        mouseDownPosition = position;

    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             modifiers,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             time,
             mouseDownPosition,
             time,
             1,
             false };
}

bool hasValidLfoTopology(const LfoData& data)
{
    if (data.points.size() < 2
        || data.curvatures.size() + 1 != data.points.size())
        return false;

    for (size_t i = 0; i < data.points.size(); ++i)
    {
        const auto& point = data.points[i];
        if (! std::isfinite(point.x)
            || ! std::isfinite(point.y)
            || point.x < 0.0f
            || point.x > 1.0f
            || point.y < 0.0f
            || point.y > 1.0f)
            return false;

        if (i > 0 && data.points[i - 1].x > point.x)
            return false;
    }

    return std::all_of(data.curvatures.begin(),
                       data.curvatures.end(),
                       [](float curvature) { return std::isfinite(curvature); });
}

const auto leftButton = juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier);

struct ScopedLfoClipboardReset
{
    ScopedLfoClipboardReset() { lfoClipboard.reset(); }
    ~ScopedLfoClipboardReset() { lfoClipboard.reset(); }
};

juce::KeyPress commandKey(juce::juce_wchar character)
{
    return { static_cast<int>(character),
             juce::ModifierKeys(juce::ModifierKeys::commandModifier),
             character };
}

void checkSameLfoData(const LfoData& actual, const LfoData& expected)
{
    REQUIRE(actual.points.size() == expected.points.size());
    REQUIRE(actual.curvatures.size() == expected.curvatures.size());

    for (size_t i = 0; i < actual.points.size(); ++i)
    {
        CHECK(juce::approximatelyEqual(actual.points[i].x, expected.points[i].x));
        CHECK(juce::approximatelyEqual(actual.points[i].y, expected.points[i].y));
    }

    for (size_t i = 0; i < actual.curvatures.size(); ++i)
        CHECK(juce::approximatelyEqual(actual.curvatures[i], expected.curvatures[i]));

    CHECK(juce::approximatelyEqual(actual.smoothness, expected.smoothness));
}
} // namespace

TEST_CASE("LFO paste remains unavailable until a shape has been copied",
          "[lfo][editor][clipboard][regression]")
{
    ScopedLfoClipboardReset resetClipboard;
    LfoEditor editor;
    prepareEditor(editor);

    auto target = makeLfoData({
        { 0.0f, 0.15f }, { 0.40f, 0.80f }, { 1.0f, 0.25f }
    });
    target.curvatures = { -0.25f, 0.50f };
    target.smoothness = 0.20f;
    editor.setDataToDisplay(target);

    LfoData lastPublished;
    int publicationCount = 0;
    editor.onDataChanged = [&](const LfoData& data)
    {
        lastPublished = data;
        ++publicationCount;
    };

    REQUIRE_FALSE(LfoEditorTestAccess::canPasteShape(editor));
    CHECK(editor.keyPressed(commandKey('v')));
    CHECK(publicationCount == 0);
    checkSameLfoData(LfoEditorTestAccess::data(editor), target);

    auto source = makeLfoData({
        { 0.0f, 0.90f }, { 0.25f, 0.20f }, { 0.70f, 0.75f }, { 1.0f, 0.10f }
    });
    source.curvatures = { -1.25f, 0.35f, 1.75f };
    source.smoothness = 0.65f;
    editor.setDataToDisplay(source);

    CHECK(editor.keyPressed(commandKey('c')));
    REQUIRE(LfoEditorTestAccess::canPasteShape(editor));

    editor.setDataToDisplay(target);
    CHECK(editor.keyPressed(commandKey('v')));
    REQUIRE(publicationCount == 1);
    checkSameLfoData(LfoEditorTestAccess::data(editor), source);
    checkSameLfoData(lastPublished, source);
}

TEST_CASE("LFO brush replacement cancels stale point selection",
          "[lfo][editor][interaction][regression]")
{
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeLfoData({
        { 0.0f, 0.10f }, { 0.14f, 0.80f }, { 0.28f, 0.20f }, { 0.42f, 0.70f },
        { 0.58f, 0.30f }, { 0.72f, 0.60f }, { 0.86f, 0.40f }, { 1.0f, 0.90f }
    }));

    LfoData lastPublished;
    int publicationCount = 0;
    editor.onDataChanged = [&](const LfoData& data)
    {
        lastPublished = data;
        ++publicationCount;
    };

    LfoEditorTestAccess::selectAllPoints(editor);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 8);

    editor.setGridDivisions(1, 1);
    editor.setCurrentBrush(LfoPresetShape::SawUp);
    editor.setEditMode(LfoEditMode::BrushPaint);
    editor.mouseDown(makeMouseEvent(editor, { 200.0f, 100.0f }, leftButton));

    REQUIRE(LfoEditorTestAccess::pointCount(editor) == 2);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    REQUIRE(LfoEditorTestAccess::isBrushing(editor));
    REQUIRE(LfoEditorTestAccess::lastBrushCell(editor) == juce::Point<int>(0, 0));

    editor.mouseUp(makeMouseEvent(editor, { 200.0f, 100.0f }));
    REQUIRE_FALSE(LfoEditorTestAccess::isBrushing(editor));

    editor.setEditMode(LfoEditMode::PointEdit);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));

    const auto endpoint = LfoEditorTestAccess::pointScreenPosition(editor, 0);
    editor.mouseDown(makeMouseEvent(editor, endpoint, leftButton));
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    editor.mouseDrag(makeMouseEvent(editor,
                                    endpoint + juce::Point<float>(5.0f, -5.0f),
                                    leftButton,
                                    endpoint));
    editor.mouseUp(makeMouseEvent(editor,
                                  endpoint + juce::Point<float>(5.0f, -5.0f),
                                  {},
                                  endpoint));

    REQUIRE(publicationCount >= 2);
    CHECK(hasValidLfoTopology(lastPublished));
    CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
}

TEST_CASE("LFO point removal clears indices from the previous topology",
          "[lfo][editor][interaction][regression]")
{
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeLfoData({
        { 0.0f, 0.20f }, { 0.30f, 0.70f }, { 0.65f, 0.35f }, { 1.0f, 0.80f }
    }));

    LfoEditorTestAccess::selectAllPoints(editor);
    const auto pointToRemove = LfoEditorTestAccess::pointScreenPosition(editor, 1);
    editor.mouseDoubleClick(makeMouseEvent(editor, pointToRemove, leftButton));

    REQUIRE(LfoEditorTestAccess::pointCount(editor) == 3);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));

    const auto endpoint = LfoEditorTestAccess::pointScreenPosition(editor, 0);
    editor.mouseDown(makeMouseEvent(editor, endpoint, leftButton));
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    editor.mouseUp(makeMouseEvent(editor, endpoint));

    CHECK(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));
}

TEST_CASE("LFO add and horizontal invert invalidate point index identity",
          "[lfo][editor][interaction][regression]")
{
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeLfoData({
        { 0.0f, 0.10f }, { 0.25f, 0.75f }, { 0.70f, 0.25f }, { 1.0f, 0.90f }
    }));

    LfoEditorTestAccess::selectAllPoints(editor);
    LfoEditorTestAccess::addPoint(editor, { 0.45f, 0.55f });
    REQUIRE(LfoEditorTestAccess::pointCount(editor) == 5);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    REQUIRE(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));

    LfoEditorTestAccess::selectAllPoints(editor);
    LfoEditorTestAccess::invertHorizontally(editor);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    CHECK(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));
}

TEST_CASE("LFO duplicate merge clears indices when the point count shrinks",
          "[lfo][editor][interaction][regression]")
{
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeLfoData({
        { 0.0f, 0.20f }, { 0.50f, 0.50f }, { 0.50f, 0.50f }, { 1.0f, 0.80f }
    }));
    editor.onDataChanged = [](const LfoData&) {};

    LfoEditorTestAccess::selectAllPoints(editor);
    const auto endpoint = LfoEditorTestAccess::pointScreenPosition(editor, 0);
    editor.mouseDown(makeMouseEvent(editor, endpoint, leftButton));
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    editor.mouseUp(makeMouseEvent(editor, endpoint));

    REQUIRE(LfoEditorTestAccess::pointCount(editor) == 3);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    CHECK(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));
}

TEST_CASE("LFO invalid curve and point drag states are cancelled before indexed reads",
          "[lfo][editor][interaction][regression]")
{
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeLfoData({
        { 0.0f, 0.20f }, { 0.35f, 0.70f }, { 0.70f, 0.35f }, { 1.0f, 0.80f }
    }));

    int publicationCount = 0;
    editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

    SECTION("invalid curve index")
    {
        LfoEditorTestAccess::seedInvalidCurveInteraction(editor);
        REQUIRE_FALSE(LfoEditorTestAccess::validateCurveInteractionOrCancel(editor));
        REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));

        editor.mouseDrag(makeMouseEvent(editor, { 210.0f, 90.0f }, leftButton, { 200.0f, 100.0f }));
        CHECK(publicationCount == 0);
        CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
    }

    SECTION("out-of-range selected point")
    {
        LfoEditorTestAccess::seedInvalidPointIndexDrag(editor);
        REQUIRE_FALSE(LfoEditorTestAccess::validatePointDragInteractionOrCancel(editor));
        REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));

        editor.mouseDrag(makeMouseEvent(editor, { 210.0f, 90.0f }, leftButton, { 200.0f, 100.0f }));
        CHECK(publicationCount == 0);
        CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
    }

    SECTION("selection and initial-position size mismatch")
    {
        LfoEditorTestAccess::seedMismatchedPointDrag(editor);
        REQUIRE_FALSE(LfoEditorTestAccess::validatePointDragInteractionOrCancel(editor));
        REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));

        editor.mouseDrag(makeMouseEvent(editor, { 210.0f, 90.0f }, leftButton, { 200.0f, 100.0f }));
        CHECK(publicationCount == 0);
        CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
    }
}

TEST_CASE("LFO clear mode switch and data replacement fully cancel transient state",
          "[lfo][editor][interaction][regression]")
{
    const auto data = makeLfoData({
        { 0.0f, 0.20f }, { 0.35f, 0.70f }, { 0.70f, 0.35f }, { 1.0f, 0.80f }
    });
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(data);

    LfoEditorTestAccess::seedInvalidPointIndexDrag(editor);
    editor.setEditMode(LfoEditMode::BrushPaint);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);

    LfoEditorTestAccess::selectAllPoints(editor);
    LfoEditorTestAccess::clearAllPoints(editor);
    REQUIRE(LfoEditorTestAccess::pointCount(editor) == 2);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));

    LfoEditorTestAccess::seedInvalidCurveInteraction(editor);
    editor.setDataToDisplay(data);
    REQUIRE(LfoEditorTestAccess::pointCount(editor) == 4);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    CHECK(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));
}
