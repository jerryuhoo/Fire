#include <Panels/ControlPanel/LfoPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>

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

    static std::function<void(int)> createContextMenuResultHandler(
        LfoEditor& editor)
    {
        return editor.createContextMenuResultHandler();
    }

    static bool hasActiveContextMenuSession(const LfoEditor& editor)
    {
        return editor.contextMenuSessionActive;
    }
};

namespace
{
void prepareEditor(LfoEditor& editor)
{
    editor.setBounds(0, 0, 400, 200);
}

LfoEditor* findLfoEditor(juce::Component& root)
{
    if (auto* editor = dynamic_cast<LfoEditor*>(&root))
        return editor;

    for (auto* child : root.getChildren())
        if (child != nullptr)
            if (auto* editor = findLfoEditor(*child))
                return editor;

    return nullptr;
}

LfoData makeLfoData(std::initializer_list<juce::Point<float>> points)
{
    LfoData data;
    data.points.assign(points.begin(), points.end());
    data.curvatures.assign(data.points.size() - 1, 0.0f);
    return data;
}

LfoData makeClusteredLfoData(size_t pointCount)
{
    REQUIRE(pointCount >= 2);

    LfoData data;
    data.points.clear();
    data.points.reserve(pointCount);
    data.points.push_back({ 0.0f, 0.15f });

    const auto interiorPointCount = pointCount - 2;
    for (size_t i = 0; i < interiorPointCount; ++i)
    {
        const auto t = static_cast<float>(i + 1)
                     / static_cast<float>(interiorPointCount + 1);
        data.points.push_back({ 0.02f + 0.18f * t,
                                0.1f + 0.8f * static_cast<float>(i % 9) / 8.0f });
    }

    data.points.push_back({ 1.0f, 0.85f });
    data.curvatures.resize(pointCount - 1);
    for (size_t i = 0; i < data.curvatures.size(); ++i)
        data.curvatures[i] = 0.25f * static_cast<float>(static_cast<int>(i % 9) - 4);

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

PrimaryTextButton* findDirectButton(LfoPanel& panel,
                                    const juce::String& text)
{
    for (auto* child : panel.getChildren())
        if (auto* button = dynamic_cast<PrimaryTextButton*>(child);
            button != nullptr && button->getButtonText() == text)
            return button;

    return nullptr;
}

void performPrimaryClick(juce::Button& button)
{
    const auto position = button.getLocalBounds().toFloat().getCentre();
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(
        component,
        position,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    component.mouseUp(makeMouseEvent(component, position));
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

void checkBrushPointLimit(size_t initialPointCount)
{
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeClusteredLfoData(initialPointCount));
    editor.setGridDivisions(4, 4);
    editor.setCurrentBrush(LfoPresetShape::SineConvex);
    editor.setEditMode(LfoEditMode::BrushPaint);

    LfoData lastPublished;
    int publicationCount = 0;
    editor.onDataChanged = [&](const LfoData& data)
    {
        lastPublished = data;
        ++publicationCount;
    };

    const juce::Point<float> mouseDownPosition { 150.0f, 25.0f };
    editor.mouseDown(makeMouseEvent(editor, mouseDownPosition, leftButton));

    REQUIRE(LfoEditorTestAccess::pointCount(editor) <= LfoData::maximumNumberOfPoints);
    REQUIRE(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));

    editor.mouseDrag(makeMouseEvent(editor,
                                    { 250.0f, 25.0f },
                                    leftButton,
                                    mouseDownPosition));

    REQUIRE(publicationCount == 1);
    REQUIRE(LfoEditorTestAccess::pointCount(editor) <= LfoData::maximumNumberOfPoints);
    REQUIRE(lastPublished.points.size() <= LfoData::maximumNumberOfPoints);
    REQUIRE(hasValidLfoTopology(lastPublished));
    checkSameLfoData(lastPublished, LfoEditorTestAccess::data(editor));

    auto managerAcceptedData = lastPublished;
    managerAcceptedData.sanitise();
    checkSameLfoData(managerAcceptedData, lastPublished);

    editor.mouseUp(makeMouseEvent(editor,
                                  { 250.0f, 25.0f },
                                  {},
                                  mouseDownPosition));

    REQUIRE(publicationCount == 2);
    REQUIRE(LfoEditorTestAccess::pointCount(editor) <= LfoData::maximumNumberOfPoints);
    REQUIRE(hasValidLfoTopology(lastPublished));
    checkSameLfoData(lastPublished, LfoEditorTestAccess::data(editor));
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

TEST_CASE("Stale LFO context menu commands cannot edit replacement data",
          "[lfo][editor][popup-menu][context][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedLfoClipboardReset resetClipboard;
    const auto oldData = makeLfoData({
        { 0.0f, 0.15f }, { 0.35f, 0.80f }, { 0.70f, 0.25f }, { 1.0f, 0.65f }
    });
    const auto replacementData = makeLfoData({
        { 0.0f, 0.85f }, { 0.25f, 0.30f }, { 0.60f, 0.75f }, { 1.0f, 0.20f }
    });
    const auto clipboardData = makeLfoData({
        { 0.0f, 0.40f }, { 0.50f, 0.90f }, { 1.0f, 0.10f }
    });

    for (const auto command : {
             LfoEditor::CommandIDs::selectAll,
             LfoEditor::CommandIDs::clear,
             LfoEditor::CommandIDs::copy,
             LfoEditor::CommandIDs::paste,
             LfoEditor::CommandIDs::invertX,
             LfoEditor::CommandIDs::invertY })
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(oldData);
        lfoClipboard = clipboardData;
        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

        auto staleHandler =
            LfoEditorTestAccess::createContextMenuResultHandler(editor);
        REQUIRE(LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        editor.setDataToDisplay(replacementData);
        REQUIRE_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));

        staleHandler(command);

        CHECK(publicationCount == 0);
        CHECK(LfoEditorTestAccess::selectedPointCount(editor) == 0);
        checkSameLfoData(LfoEditorTestAccess::data(editor), replacementData);
        REQUIRE(lfoClipboard.has_value());
        checkSameLfoData(*lfoClipboard, clipboardData);
    }
}

TEST_CASE("Old LFO menu callbacks cannot consume a newer menu session",
          "[lfo][editor][popup-menu][generation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeLfoData({
        { 0.0f, 0.15f }, { 0.35f, 0.80f }, { 0.70f, 0.25f }, { 1.0f, 0.65f }
    }));

    int publicationCount = 0;
    editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };
    auto oldHandler =
        LfoEditorTestAccess::createContextMenuResultHandler(editor);
    editor.invalidateContextMenuSession();
    auto newHandler =
        LfoEditorTestAccess::createContextMenuResultHandler(editor);
    REQUIRE(LfoEditorTestAccess::hasActiveContextMenuSession(editor));

    oldHandler(LfoEditor::CommandIDs::clear);
    CHECK(publicationCount == 0);
    REQUIRE(LfoEditorTestAccess::hasActiveContextMenuSession(editor));

    newHandler(LfoEditor::CommandIDs::clear);
    CHECK(publicationCount == 1);
    CHECK_FALSE(LfoEditorTestAccess::hasActiveContextMenuSession(editor));
    CHECK(LfoEditorTestAccess::pointCount(editor) == 2);

    newHandler(LfoEditor::CommandIDs::invertY);
    CHECK(publicationCount == 1);
}

TEST_CASE("LFO panel invalidates menu sessions on dismissal and slot rebinding",
          "[lfo][editor][popup-menu][panel][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedLfoClipboardReset resetClipboard;
    FireAudioProcessor processor;
    const auto initialShapes = std::array<LfoData, 4> {
        makeLfoData({
            { 0.0f, 0.10f }, { 0.30f, 0.80f }, { 0.70f, 0.25f }, { 1.0f, 0.65f }
        }),
        makeLfoData({
            { 0.0f, 0.20f }, { 0.40f, 0.70f }, { 0.75f, 0.35f }, { 1.0f, 0.60f }
        }),
        makeLfoData({ { 0.0f, 0.30f }, { 1.0f, 0.50f } }),
        makeLfoData({ { 0.0f, 0.40f }, { 1.0f, 0.45f } })
    };
    processor.getLfoManager().replaceLfoDataAndRoutings(initialShapes, {});

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    auto* editor = findLfoEditor(panel);
    REQUIRE(editor != nullptr);

    int dirtyCount = 0;
    panel.setOnDataChangedCallback([&] { ++dirtyCount; });
    const auto clipboardSentinel = makeLfoData({
        { 0.0f, 0.95f }, { 0.45f, 0.15f }, { 1.0f, 0.75f }
    });
    constexpr auto commands = std::array {
        LfoEditor::CommandIDs::selectAll,
        LfoEditor::CommandIDs::clear,
        LfoEditor::CommandIDs::copy,
        LfoEditor::CommandIDs::paste,
        LfoEditor::CommandIDs::invertX,
        LfoEditor::CommandIDs::invertY
    };

    SECTION("Panel dismissal makes all delayed results inert")
    {
        for (const auto command : commands)
        {
            panel.refreshLfoDisplay();
            lfoClipboard = clipboardSentinel;
            auto staleHandler =
                LfoEditorTestAccess::createContextMenuResultHandler(*editor);

            panel.dismissTransientInteraction();
            staleHandler(command);

            checkSameLfoData(
                processor.getLfoManager().getLfoDataSnapshot(0).data,
                initialShapes[0]);
            REQUIRE(lfoClipboard.has_value());
            checkSameLfoData(*lfoClipboard, clipboardSentinel);
            CHECK(dirtyCount == 0);
        }
    }

    SECTION("LFO zero-to-one-to-zero rebinding defeats local ABA")
    {
        auto* lfoOneButton = findDirectButton(panel, "LFO 1");
        auto* lfoTwoButton = findDirectButton(panel, "LFO 2");
        REQUIRE(lfoOneButton != nullptr);
        REQUIRE(lfoTwoButton != nullptr);

        for (const auto command : commands)
        {
            panel.refreshLfoDisplay();
            lfoClipboard = clipboardSentinel;
            auto staleHandler =
                LfoEditorTestAccess::createContextMenuResultHandler(*editor);

            performPrimaryClick(*lfoTwoButton);
            REQUIRE(editor->getDataContext().lfoIndex == 1);
            performPrimaryClick(*lfoOneButton);
            REQUIRE(editor->getDataContext().lfoIndex == 0);
            staleHandler(command);

            checkSameLfoData(
                processor.getLfoManager().getLfoDataSnapshot(0).data,
                initialShapes[0]);
            checkSameLfoData(
                processor.getLfoManager().getLfoDataSnapshot(1).data,
                initialShapes[1]);
            REQUIRE(lfoClipboard.has_value());
            checkSameLfoData(*lfoClipboard, clipboardSentinel);
            CHECK(dirtyCount == 0);
        }

        auto currentHandler =
            LfoEditorTestAccess::createContextMenuResultHandler(*editor);
        currentHandler(LfoEditor::CommandIDs::clear);
        CHECK(processor.getLfoManager()
                  .getLfoDataSnapshot(0)
                  .data.points.size()
              == 2);
        CHECK(dirtyCount == 1);
    }
}

TEST_CASE("LFO context menus reject authoritative replacement before UI refresh",
          "[lfo][editor][popup-menu][context][revision][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedLfoClipboardReset resetClipboard;
    FireAudioProcessor processor;
    auto initialShapes = std::array<LfoData, 4> {
        makeLfoData({
            { 0.0f, 0.10f }, { 0.30f, 0.80f }, { 0.70f, 0.25f }, { 1.0f, 0.65f }
        }),
        makeLfoData({ { 0.0f, 0.20f }, { 1.0f, 0.70f } }),
        makeLfoData({ { 0.0f, 0.30f }, { 1.0f, 0.60f } }),
        makeLfoData({ { 0.0f, 0.40f }, { 1.0f, 0.50f } })
    };
    processor.getLfoManager().replaceLfoDataAndRoutings(initialShapes, {});

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    auto* editor = findLfoEditor(panel);
    REQUIRE(editor != nullptr);

    int dirtyCount = 0;
    panel.setOnDataChangedCallback([&] { ++dirtyCount; });
    const auto clipboardSentinel = makeLfoData({
        { 0.0f, 0.95f }, { 0.45f, 0.15f }, { 1.0f, 0.75f }
    });

    for (const auto command : {
             LfoEditor::CommandIDs::selectAll,
             LfoEditor::CommandIDs::clear,
             LfoEditor::CommandIDs::copy,
             LfoEditor::CommandIDs::paste,
             LfoEditor::CommandIDs::invertX,
             LfoEditor::CommandIDs::invertY })
    {
        processor.getLfoManager().replaceLfoDataAndRoutings(initialShapes, {});
        panel.refreshLfoDisplay();
        lfoClipboard = clipboardSentinel;
        auto staleHandler =
            LfoEditorTestAccess::createContextMenuResultHandler(*editor);

        auto replacementShapes = initialShapes;
        replacementShapes[0] = makeLfoData({
            { 0.0f, 0.85f }, { 0.20f, 0.35f }, { 0.55f, 0.90f }, { 1.0f, 0.15f }
        });
        // Deliberately do not refresh the panel. This is the real preset/host
        // window between synchronous model replacement and async UI delivery.
        processor.getLfoManager().replaceLfoDataAndRoutings(
            replacementShapes, {});

        staleHandler(command);

        checkSameLfoData(
            processor.getLfoManager().getLfoDataSnapshot(0).data,
            replacementShapes[0]);
        checkSameLfoData(LfoEditorTestAccess::data(*editor),
                         initialShapes[0]);
        CHECK(LfoEditorTestAccess::selectedPointCount(*editor) == 0);
        REQUIRE(lfoClipboard.has_value());
        checkSameLfoData(*lfoClipboard, clipboardSentinel);
        CHECK(dirtyCount == 0);
    }
}

TEST_CASE("Current LFO context menu commands commit once through the panel",
          "[lfo][editor][popup-menu][panel][transaction][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto smoothnessID =
        ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0);
    auto* smoothnessParameter =
        processor.treeState.getParameter(smoothnessID);
    REQUIRE(smoothnessParameter != nullptr);
    constexpr float authoritativeSmoothness = 0.63f;
    smoothnessParameter->setValueNotifyingHost(
        smoothnessParameter->convertTo0to1(authoritativeSmoothness));

    const auto initialShape = makeLfoData({
        { 0.0f, 0.10f }, { 0.30f, 0.80f }, { 0.70f, 0.25f }, { 1.0f, 0.65f }
    });
    processor.getLfoManager().setLfoData(0, initialShape);

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    auto* editor = findLfoEditor(panel);
    REQUIRE(editor != nullptr);

    int dirtyCount = 0;
    panel.setOnDataChangedCallback([&] { ++dirtyCount; });
    const auto contextBefore = editor->getDataContext();
    auto handler =
        LfoEditorTestAccess::createContextMenuResultHandler(*editor);

    handler(LfoEditor::CommandIDs::clear);

    const auto committed =
        processor.getLfoManager().getLfoDataSnapshot(0);
    CHECK(committed.data.points.size() == 2);
    CHECK(committed.revision != contextBefore.revision);
    CHECK(editor->getDataContext().lfoIndex == 0);
    CHECK(editor->getDataContext().revision == committed.revision);
    CHECK(juce::approximatelyEqual(committed.data.smoothness,
                                   authoritativeSmoothness));
    CHECK(dirtyCount == 1);
    checkSameLfoData(LfoEditorTestAccess::data(*editor), committed.data);

    // JUCE callbacks can occasionally be delivered more than once while a
    // modal component is being dismissed. A consumed menu must stay inert.
    handler(LfoEditor::CommandIDs::invertY);
    CHECK(processor.getLfoManager().getLfoDataSnapshot(0).revision
          == committed.revision);
    CHECK(dirtyCount == 1);
}

TEST_CASE("LFO revision compare-and-commit rejects stale and ABA writes",
          "[lfo][manager][revision][transaction][regression]")
{
    FireAudioProcessor processor;
    auto oldShape = makeLfoData({
        { 0.0f, 0.15f }, { 0.35f, 0.80f }, { 1.0f, 0.25f }
    });
    processor.getLfoManager().setLfoData(0, oldShape);
    const auto oldSnapshot =
        processor.getLfoManager().getLfoDataSnapshot(0);

    auto replacementShapes = std::array<LfoData, 4> {
        oldShape, LfoData {}, LfoData {}, LfoData {}
    };
    // Identical content is still a new preset identity and must defeat ABA.
    processor.getLfoManager().replaceLfoDataAndRoutings(
        replacementShapes, {});
    const auto replacementSnapshot =
        processor.getLfoManager().getLfoDataSnapshot(0);
    REQUIRE(replacementSnapshot.revision != oldSnapshot.revision);

    LfoData staleCandidate;
    staleCandidate.resetToDefault();
    std::uint64_t resultingRevision = 0;
    CHECK_FALSE(processor.getLfoManager().setLfoDataIfRevisionMatches(
        0,
        staleCandidate,
        oldSnapshot.revision,
        resultingRevision));
    checkSameLfoData(
        processor.getLfoManager().getLfoDataSnapshot(0).data,
        replacementShapes[0]);
}

TEST_CASE("LFO menu sessions freeze clipboard data and tolerate deletion",
          "[lfo][editor][popup-menu][snapshot][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ScopedLfoClipboardReset resetClipboard;
    const auto target = makeLfoData({
        { 0.0f, 0.15f }, { 0.40f, 0.80f }, { 1.0f, 0.25f }
    });
    const auto clipboardAtOpen = makeLfoData({
        { 0.0f, 0.90f }, { 0.25f, 0.20f }, { 0.70f, 0.75f }, { 1.0f, 0.10f }
    });
    const auto laterClipboard = makeLfoData({
        { 0.0f, 0.05f }, { 0.50f, 0.95f }, { 1.0f, 0.40f }
    });

    SECTION("Paste uses the clipboard snapshot from menu open")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(target);
        lfoClipboard = clipboardAtOpen;
        auto handler =
            LfoEditorTestAccess::createContextMenuResultHandler(editor);
        lfoClipboard = laterClipboard;

        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };
        handler(LfoEditor::CommandIDs::paste);

        CHECK(publicationCount == 1);
        checkSameLfoData(LfoEditorTestAccess::data(editor), clipboardAtOpen);
    }

    SECTION("Callback may delete its editor")
    {
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(target);
        auto handler =
            LfoEditorTestAccess::createContextMenuResultHandler(*editor);
        editor->onDataChanged = [&editor](const LfoData&) { editor.reset(); };

        handler(LfoEditor::CommandIDs::clear);
        CHECK(editor == nullptr);
        handler(LfoEditor::CommandIDs::clear);
    }

    SECTION("Callback after editor destruction is inert")
    {
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(target);
        auto handler =
            LfoEditorTestAccess::createContextMenuResultHandler(*editor);
        editor.reset();

        handler(LfoEditor::CommandIDs::clear);
        CHECK(editor == nullptr);
    }
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

TEST_CASE("LFO brush painting never publishes more points than the DSP accepts",
          "[lfo][editor][brush][regression]")
{
    SECTION("near the point limit")
    {
        checkBrushPointLimit(LfoData::maximumNumberOfPoints - 1);
    }

    SECTION("at the point limit")
    {
        checkBrushPointLimit(LfoData::maximumNumberOfPoints);
    }
}

#if JUCE_MAC
TEST_CASE("macOS Control-click never paints the active LFO brush",
          "[lfo][editor][brush][popup-menu][macos]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    LfoEditor editor;
    prepareEditor(editor);
    const auto original = makeLfoData({
        { 0.0f, 0.20f }, { 0.45f, 0.75f }, { 1.0f, 0.30f }
    });
    editor.setDataToDisplay(original);
    editor.setGridDivisions(4, 4);
    editor.setCurrentBrush(LfoPresetShape::SawUp);
    editor.setEditMode(LfoEditMode::BrushPaint);

    int publicationCount = 0;
    editor.onDataChanged = [&publicationCount](const LfoData&)
    {
        ++publicationCount;
    };

    const auto controlClick = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::ctrlModifier };
    editor.mouseDown(makeMouseEvent(editor,
                                    { 200.0f, 100.0f },
                                    controlClick));

    CHECK(publicationCount == 0);
    CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
    CHECK(LfoEditorTestAccess::lastBrushCell(editor)
          == juce::Point<int>(-1, -1));
    CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
    checkSameLfoData(LfoEditorTestAccess::data(editor), original);
}
#endif

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
