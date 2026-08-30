#include <Panels/ControlPanel/LfoPanel.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>

struct LfoEditorTestAccess
{
    static int hoveredPoint(const LfoEditor& editor) noexcept
    {
        return editor.hoveredPointIndex;
    }

    static float hoverAmount(const LfoEditor& editor) noexcept
    {
        return editor.pointHoverAnimation.current;
    }

    static float focusAmount(const LfoEditor& editor) noexcept
    {
        return editor.focusAnimation.current;
    }

    static float pointHitRadius(const LfoEditor& editor) noexcept
    {
        return editor.getPointVisualRadius();
    }

    static bool animationIsRunning(const LfoEditor& editor) noexcept
    {
        return editor.isTimerRunning();
    }

    static void tickAnimation(LfoEditor& editor)
    {
        editor.timerCallback();
    }

    static void focusGained(LfoEditor& editor)
    {
        editor.focusGained(juce::Component::focusChangedDirectly);
    }

    static void setTrackedPointerSource(
        LfoEditor& editor,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        editor.pointerSourceType = type;
        editor.pointerSourceIndex = index;
    }

    static bool hasNoPointerGesture(const LfoEditor& editor) noexcept
    {
        return editor.activePointerGesture
               == LfoEditor::PointerGesture::none;
    }

    static bool isPrimaryPointerGesture(const LfoEditor& editor) noexcept
    {
        return editor.activePointerGesture
               == LfoEditor::PointerGesture::primary;
    }

    static bool isPopupPointerGesture(const LfoEditor& editor) noexcept
    {
        return editor.activePointerGesture
               == LfoEditor::PointerGesture::popupMenu;
    }

    static bool isRejectedPointerGesture(const LfoEditor& editor) noexcept
    {
        return editor.activePointerGesture
               == LfoEditor::PointerGesture::rejected;
    }

    static bool hasPrimaryDoubleClickAuthorization(
        const LfoEditor& editor) noexcept
    {
        return editor.primaryDoubleClickAuthorized;
    }

    static void setTrackedDoubleClickSource(
        LfoEditor& editor,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        editor.doubleClickSourceType = type;
        editor.doubleClickSourceIndex = index;
    }

    static bool isMarqueeInteraction(const LfoEditor& editor) noexcept
    {
        return editor.draggingState == LfoEditor::DraggingState::Marquee;
    }

    static bool isPointInteraction(const LfoEditor& editor) noexcept
    {
        return editor.draggingState == LfoEditor::DraggingState::Point
            || editor.draggingState == LfoEditor::DraggingState::Selection;
    }

    static bool isCurveInteraction(const LfoEditor& editor) noexcept
    {
        return editor.editingCurveIndex != -1;
    }

    static bool hasSelectionRectangle(const LfoEditor& editor) noexcept
    {
        return ! editor.selectionRectangle.isEmpty();
    }

    static bool interactionStateIsValid(const LfoEditor& editor)
    {
        const bool hasNoPointerGesture =
            editor.activePointerGesture == LfoEditor::PointerGesture::none;
        if (hasNoPointerGesture != (editor.pointerSourceIndex == -1))
            return false;

        if (editor.primaryDoubleClickAuthorized
            != (editor.doubleClickSourceIndex != -1))
            return false;

        if (editor.primaryDoubleClickAuthorized
            && ! hasNoPointerGesture)
            return false;

        if (editor.activePointerGesture != LfoEditor::PointerGesture::primary
            && (editor.isBrushing
                || editor.draggingState != LfoEditor::DraggingState::None
                || editor.editingCurveIndex != -1))
            return false;

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

    static void setContextMenuLaunchHook(LfoEditor& editor,
                                         std::function<void()> hook)
    {
        editor.contextMenuLaunchHook = std::move(hook);
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
                                juce::Point<float> mouseDownPosition = {},
                                bool wasDragged = false,
                                int clickCount = 1)
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
             clickCount,
             wasDragged };
}

void makeTrackedPointerIndexForeign(LfoEditor& editor,
                                    const juce::MouseInputSource& source)
{
    LfoEditorTestAccess::setTrackedPointerSource(
        editor, source.getType(),
        source.getIndex() + 1);
}

void makeTrackedPointerTypeForeign(LfoEditor& editor,
                                   const juce::MouseInputSource& source)
{
    LfoEditorTestAccess::setTrackedPointerSource(
        editor,
        source.getType() == juce::MouseInputSource::mouse
            ? juce::MouseInputSource::touch
            : juce::MouseInputSource::mouse,
        source.getIndex());
}

void restoreTrackedPointerSource(LfoEditor& editor,
                                 const juce::MouseInputSource& source)
{
    LfoEditorTestAccess::setTrackedPointerSource(
        editor, source.getType(), source.getIndex());
}

void performOwnedPrimaryDoubleClick(LfoEditor& editor,
                                    juce::Point<float> position)
{
    const auto primaryButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    editor.mouseDown(makeMouseEvent(
        editor, position, primaryButton, position, false, 2));
    const auto completedRelease = makeMouseEvent(
        editor, position, primaryButton, position, false, 2);
    editor.mouseUp(completedRelease);
    editor.mouseDoubleClick(completedRelease);
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

struct DeletingLfoCallbackState
{
    int liveCallbackInstances = 0;
    int liveInstancesDuringCallback = -1;
    const LfoData* editorDataAddress = nullptr;
    bool publishedDataWasSnapshot = false;
    size_t pointCountBeforeDeletion = 0;
    size_t pointCountAfterDeletion = 0;
    bool callbackCompleted = false;
    std::function<void()> deleteOwner;
};

class DeletingLfoCallback
{
public:
    explicit DeletingLfoCallback(
        std::shared_ptr<DeletingLfoCallbackState> stateToUse)
        : state(std::move(stateToUse))
    {
        ++state->liveCallbackInstances;
    }

    DeletingLfoCallback(const DeletingLfoCallback& other)
        : state(other.state)
    {
        if (state != nullptr)
            ++state->liveCallbackInstances;
    }

    DeletingLfoCallback(DeletingLfoCallback&& other) noexcept
        : state(std::move(other.state))
    {
    }

    DeletingLfoCallback& operator=(const DeletingLfoCallback&) = delete;
    DeletingLfoCallback& operator=(DeletingLfoCallback&&) = delete;

    ~DeletingLfoCallback()
    {
        if (state != nullptr)
            --state->liveCallbackInstances;
    }

    void operator()(const LfoData& publishedData) const
    {
        auto stateToKeepAlive = state;
        stateToKeepAlive->publishedDataWasSnapshot =
            &publishedData != stateToKeepAlive->editorDataAddress;
        stateToKeepAlive->pointCountBeforeDeletion =
            publishedData.points.size();
        auto deleteOwner = stateToKeepAlive->deleteOwner;
        deleteOwner();

        stateToKeepAlive->liveInstancesDuringCallback =
            stateToKeepAlive->liveCallbackInstances;
        if (stateToKeepAlive->publishedDataWasSnapshot)
            stateToKeepAlive->pointCountAfterDeletion =
                publishedData.points.size();
        stateToKeepAlive->callbackCompleted = true;
    }

private:
    std::shared_ptr<DeletingLfoCallbackState> state;
};

std::shared_ptr<DeletingLfoCallbackState> installDeletingCallback(
    std::unique_ptr<LfoEditor>& editor)
{
    auto state = std::make_shared<DeletingLfoCallbackState>();
    state->editorDataAddress = &LfoEditorTestAccess::data(*editor);
    state->deleteOwner = [&editor] { editor.reset(); };
    editor->onDataChanged = DeletingLfoCallback { state };
    REQUIRE(state->liveCallbackInstances == 1);
    return state;
}

void checkSafeCallbackDeletion(
    const std::unique_ptr<LfoEditor>& editor,
    const std::shared_ptr<DeletingLfoCallbackState>& state)
{
    CHECK(editor == nullptr);
    CHECK(state->callbackCompleted);
    CHECK(state->liveInstancesDuringCallback >= 1);
    CHECK(state->publishedDataWasSnapshot);
    CHECK(state->pointCountAfterDeletion
          == state->pointCountBeforeDeletion);
    CHECK(state->liveCallbackInstances == 0);
}

juce::KeyPress commandKey(juce::juce_wchar character)
{
    return { static_cast<int>(character),
             juce::ModifierKeys(juce::ModifierKeys::commandModifier),
             character };
}

juce::KeyPress commandKeyWithoutText(juce::juce_wchar keyCode)
{
    return { static_cast<int>(keyCode),
             juce::ModifierKeys(juce::ModifierKeys::commandModifier),
             0 };
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

    REQUIRE(publicationCount == 1);
    REQUIRE(LfoEditorTestAccess::pointCount(editor) <= LfoData::maximumNumberOfPoints);
    REQUIRE(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));
    checkSameLfoData(lastPublished, LfoEditorTestAccess::data(editor));

    editor.mouseDrag(makeMouseEvent(editor,
                                    { 250.0f, 25.0f },
                                    leftButton,
                                    mouseDownPosition));

    REQUIRE(publicationCount == 2);
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

TEST_CASE("LFO command shortcuts use key codes when text is unavailable",
          "[lfo][editor][keyboard][regression]")
{
    ScopedLfoClipboardReset resetClipboard;
    LfoEditor editor;
    prepareEditor(editor);

    const auto source = makeLfoData({
        { 0.0f, 0.90f }, { 0.25f, 0.20f },
        { 0.70f, 0.75f }, { 1.0f, 0.10f }
    });
    const auto target = makeLfoData({
        { 0.0f, 0.15f }, { 0.40f, 0.80f }, { 1.0f, 0.25f }
    });

    editor.setDataToDisplay(source);
    CHECK(editor.keyPressed(commandKeyWithoutText('c')));
    REQUIRE(LfoEditorTestAccess::canPasteShape(editor));

    editor.setDataToDisplay(target);
    int publicationCount = 0;
    editor.onDataChanged = [&](const LfoData&)
    {
        ++publicationCount;
    };
    CHECK(editor.keyPressed(commandKeyWithoutText('V')));
    CHECK(publicationCount == 1);
    checkSameLfoData(LfoEditorTestAccess::data(editor), source);

    editor.setDataToDisplay(target);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);
    CHECK(editor.keyPressed(commandKeyWithoutText('A')));
    CHECK(LfoEditorTestAccess::selectedPointCount(editor)
          == target.points.size());
}

TEST_CASE("LFO deletion shortcuts use the delivered key event",
          "[lfo][editor][keyboard][regression]")
{
    const auto original = makeLfoData({
        { 0.0f, 0.15f }, { 0.30f, 0.80f },
        { 0.65f, 0.25f }, { 1.0f, 0.70f }
    });

    const auto checkDeletionKey = [&](int keyCode)
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(original);
        LfoEditorTestAccess::selectAllPoints(editor);

        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&)
        {
            ++publicationCount;
        };

        CHECK(editor.keyPressed(juce::KeyPress { keyCode }));
        CHECK(LfoEditorTestAccess::pointCount(editor) == 2);
        CHECK(LfoEditorTestAccess::selectedPointCount(editor) == 0);
        CHECK(publicationCount == 1);
        CHECK(hasValidLfoTopology(LfoEditorTestAccess::data(editor)));
    };

    SECTION("Delete")
    {
        checkDeletionKey(juce::KeyPress::deleteKey);
    }

    SECTION("Backspace")
    {
        checkDeletionKey(juce::KeyPress::backspaceKey);
    }
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

    REQUIRE(publicationCount == 1);
    REQUIRE(LfoEditorTestAccess::pointCount(editor) == 2);
    REQUIRE(LfoEditorTestAccess::selectedPointCount(editor) == 0);
    REQUIRE(LfoEditorTestAccess::interactionStateIsValid(editor));
    REQUIRE(LfoEditorTestAccess::isBrushing(editor));
    REQUIRE(LfoEditorTestAccess::lastBrushCell(editor) == juce::Point<int>(0, 0));
    checkSameLfoData(lastPublished, LfoEditorTestAccess::data(editor));

    editor.mouseUp(makeMouseEvent(editor, { 200.0f, 100.0f }));
    REQUIRE_FALSE(LfoEditorTestAccess::isBrushing(editor));
    REQUIRE(publicationCount == 1);

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

    REQUIRE(publicationCount == 3);
    CHECK(hasValidLfoTopology(lastPublished));
    CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
}

TEST_CASE("LFO brush mouse-down immediately commits through the panel",
          "[lfo][editor][brush][panel][immediate][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto initialShape = makeLfoData({
        { 0.0f, 0.15f }, { 0.35f, 0.80f }, { 0.70f, 0.25f }, { 1.0f, 0.65f }
    });
    processor.getLfoManager().setLfoData(0, initialShape);

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    auto* editor = findLfoEditor(panel);
    REQUIRE(editor != nullptr);
    prepareEditor(*editor);
    editor->setGridDivisions(4, 4);
    editor->setCurrentBrush(LfoPresetShape::SineConvex);
    editor->setEditMode(LfoEditMode::BrushPaint);

    int dirtyCount = 0;
    panel.setOnDataChangedCallback([&] { ++dirtyCount; });
    const juce::Point<float> mouseDownPosition { 150.0f, 25.0f };
    const auto initialContext = editor->getDataContext();
    editor->mouseDown(makeMouseEvent(
        *editor, mouseDownPosition, leftButton));

    REQUIRE(dirtyCount == 1);
    const auto firstCommit =
        processor.getLfoManager().getLfoDataSnapshot(0);
    REQUIRE(firstCommit.revision != initialContext.revision);
    CHECK(editor->getDataContext().revision == firstCommit.revision);
    checkSameLfoData(firstCommit.data,
                     LfoEditorTestAccess::data(*editor));

    editor->mouseDrag(makeMouseEvent(*editor,
                                     { 160.0f, 30.0f },
                                     leftButton,
                                     mouseDownPosition));
    REQUIRE(dirtyCount == 1);
    CHECK(processor.getLfoManager().getLfoDataSnapshot(0).revision
          == firstCommit.revision);

    // Crossing into an adjacent cell within this same held gesture is a
    // second transaction based on the revision committed by mouseDown.
    const juce::Point<float> adjacentCell { 250.0f, 25.0f };
    editor->mouseDrag(makeMouseEvent(*editor,
                                     adjacentCell,
                                     leftButton,
                                     mouseDownPosition));
    REQUIRE(dirtyCount == 2);
    const auto secondCommit =
        processor.getLfoManager().getLfoDataSnapshot(0);
    REQUIRE(secondCommit.revision != firstCommit.revision);
    CHECK(editor->getDataContext().revision == secondCommit.revision);
    checkSameLfoData(secondCommit.data,
                     LfoEditorTestAccess::data(*editor));
    editor->mouseUp(makeMouseEvent(
        *editor, adjacentCell, {}, mouseDownPosition));
    CHECK(dirtyCount == 2);
    CHECK(processor.getLfoManager().getLfoDataSnapshot(0).revision
          == secondCommit.revision);

    // Repainting that already identical cell in a new gesture is a no-op.
    editor->mouseDown(makeMouseEvent(
        *editor, adjacentCell, leftButton));
    CHECK(dirtyCount == 2);
    CHECK(editor->getDataContext().revision == secondCommit.revision);
    editor->mouseUp(makeMouseEvent(
        *editor, adjacentCell, {}, adjacentCell));
    CHECK(dirtyCount == 2);
    CHECK(processor.getLfoManager().getLfoDataSnapshot(0).revision
          == secondCommit.revision);
}

TEST_CASE("LFO brush publication tolerates editor deletion",
          "[lfo][editor][brush][lifetime][regression]")
{
    auto editor = std::make_unique<LfoEditor>();
    prepareEditor(*editor);
    editor->setDataToDisplay(makeLfoData({
        { 0.0f, 0.15f }, { 0.35f, 0.80f }, { 1.0f, 0.25f }
    }));
    editor->setGridDivisions(4, 4);
    editor->setCurrentBrush(LfoPresetShape::SineConvex);
    editor->setEditMode(LfoEditMode::BrushPaint);

    bool callbackRan = false;
    editor->onDataChanged = [&](const LfoData& published)
    {
        callbackRan = published.points.size() >= 2;
        editor.reset();
    };

    auto* rawEditor = editor.get();
    rawEditor->mouseDown(makeMouseEvent(
        *rawEditor, { 150.0f, 25.0f }, leftButton));

    CHECK(callbackRan);
    CHECK(editor == nullptr);
}

TEST_CASE("Stale LFO brush publication restores manager authority",
          "[lfo][editor][brush][panel][revision][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto initialShape = makeLfoData({
        { 0.0f, 0.15f }, { 0.35f, 0.80f }, { 0.70f, 0.25f }, { 1.0f, 0.65f }
    });
    processor.getLfoManager().setLfoData(0, initialShape);

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    auto* editor = findLfoEditor(panel);
    REQUIRE(editor != nullptr);
    prepareEditor(*editor);
    editor->setGridDivisions(4, 4);
    editor->setCurrentBrush(LfoPresetShape::SineConvex);
    editor->setEditMode(LfoEditMode::BrushPaint);

    int dirtyCount = 0;
    panel.setOnDataChangedCallback([&] { ++dirtyCount; });
    const auto replacement = makeLfoData({
        { 0.0f, 0.90f }, { 0.20f, 0.35f }, { 0.55f, 0.75f }, { 1.0f, 0.10f }
    });
    auto replacementShapes = std::array<LfoData, 4> {
        replacement, LfoData {}, LfoData {}, LfoData {}
    };
    // Keep the editor stale to exercise the CAS failure path itself.
    processor.getLfoManager().replaceLfoDataAndRoutings(
        replacementShapes, {});
    const auto replacementSnapshot =
        processor.getLfoManager().getLfoDataSnapshot(0);

    const juce::Point<float> mouseDownPosition { 150.0f, 25.0f };
    editor->mouseDown(makeMouseEvent(
        *editor, mouseDownPosition, leftButton));

    CHECK(dirtyCount == 0);
    checkSameLfoData(
        processor.getLfoManager().getLfoDataSnapshot(0).data,
        replacementSnapshot.data);
    checkSameLfoData(LfoEditorTestAccess::data(*editor),
                     replacementSnapshot.data);
    CHECK(editor->getDataContext().revision
          == replacementSnapshot.revision);
    CHECK_FALSE(LfoEditorTestAccess::isBrushing(*editor));
    CHECK(LfoEditorTestAccess::lastBrushCell(*editor)
          == juce::Point<int>(-1, -1));
    CHECK(LfoEditorTestAccess::hasNoPointerGesture(*editor));
    CHECK(LfoEditorTestAccess::interactionStateIsValid(*editor));

    editor->mouseDrag(makeMouseEvent(*editor,
                                     { 250.0f, 25.0f },
                                     leftButton,
                                     mouseDownPosition));
    editor->mouseUp(makeMouseEvent(
        *editor, { 250.0f, 25.0f }, {}, mouseDownPosition));
    CHECK(dirtyCount == 0);
    const auto finalSnapshot =
        processor.getLfoManager().getLfoDataSnapshot(0);
    CHECK(finalSnapshot.revision == replacementSnapshot.revision);
    checkSameLfoData(finalSnapshot.data, replacementSnapshot.data);
}

TEST_CASE("LFO brush publication tolerates panel deletion",
          "[lfo][editor][brush][panel][lifetime][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.getLfoManager().setLfoData(0, makeLfoData({
        { 0.0f, 0.15f }, { 0.35f, 0.80f }, { 1.0f, 0.25f }
    }));

    auto panel = std::make_unique<LfoPanel>(processor);
    panel->setBounds(0, 0, 1000, 500);
    auto* editor = findLfoEditor(*panel);
    REQUIRE(editor != nullptr);
    prepareEditor(*editor);
    editor->setGridDivisions(4, 4);
    editor->setCurrentBrush(LfoPresetShape::SineConvex);
    editor->setEditMode(LfoEditMode::BrushPaint);
    panel->setOnDataChangedCallback([&] { panel.reset(); });

    editor->mouseDown(makeMouseEvent(
        *editor, { 150.0f, 25.0f }, leftButton));

    CHECK(panel == nullptr);
}

TEST_CASE("LFO edit publications retain callback and data through editor deletion",
          "[lfo][editor][callback-safety][lifetime][regression]")
{
    const auto data = makeLfoData({
        { 0.0f, 0.20f }, { 0.35f, 0.70f },
        { 0.70f, 0.35f }, { 1.0f, 0.80f }
    });

    SECTION("curve drag")
    {
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(data);
        auto state = installDeletingCallback(editor);
        auto* rawEditor = editor.get();
        const juce::Point<float> downPosition { 200.0f, 175.0f };

        rawEditor->mouseDown(makeMouseEvent(
            *rawEditor, downPosition, leftButton));
        rawEditor->mouseDrag(makeMouseEvent(
            *rawEditor,
            { 200.0f, 135.0f },
            leftButton,
            downPosition,
            true));

        checkSafeCallbackDeletion(editor, state);
    }

    SECTION("point drag")
    {
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(data);
        const auto downPosition =
            LfoEditorTestAccess::pointScreenPosition(*editor, 1);
        auto state = installDeletingCallback(editor);
        auto* rawEditor = editor.get();

        rawEditor->mouseDown(makeMouseEvent(
            *rawEditor, downPosition, leftButton));
        rawEditor->mouseDrag(makeMouseEvent(
            *rawEditor,
            downPosition + juce::Point<float> { 20.0f, -15.0f },
            leftButton,
            downPosition,
            true));

        checkSafeCallbackDeletion(editor, state);
    }

    SECTION("double-click removal")
    {
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(data);
        auto* rawEditor = editor.get();
        const auto position =
            LfoEditorTestAccess::pointScreenPosition(*rawEditor, 1);
        rawEditor->mouseDown(makeMouseEvent(
            *rawEditor, position, leftButton, position, false, 2));
        const auto completedRelease = makeMouseEvent(
            *rawEditor, position, leftButton, position, false, 2);
        rawEditor->mouseUp(completedRelease);

        auto state = installDeletingCallback(editor);
        rawEditor->mouseDoubleClick(completedRelease);

        checkSafeCallbackDeletion(editor, state);
    }

    SECTION("double-click insertion")
    {
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(data);
        auto* rawEditor = editor.get();
        const juce::Point<float> position { 220.0f, 175.0f };
        rawEditor->mouseDown(makeMouseEvent(
            *rawEditor, position, leftButton, position, false, 2));
        const auto completedRelease = makeMouseEvent(
            *rawEditor, position, leftButton, position, false, 2);
        rawEditor->mouseUp(completedRelease);

        auto state = installDeletingCallback(editor);
        rawEditor->mouseDoubleClick(completedRelease);

        checkSafeCallbackDeletion(editor, state);
    }

    SECTION("keyboard paste")
    {
        ScopedLfoClipboardReset resetClipboard;
        lfoClipboard = makeLfoData({
            { 0.0f, 0.90f }, { 0.25f, 0.30f },
            { 0.65f, 0.75f }, { 1.0f, 0.10f }
        });
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(data);
        auto state = installDeletingCallback(editor);
        auto* rawEditor = editor.get();

        CHECK(rawEditor->keyPressed(commandKey('v')));

        checkSafeCallbackDeletion(editor, state);
    }
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

TEST_CASE("LFO pointer sequences require an owned down and exact button kind",
          "[lfo][editor][input][gesture][popup][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto popupButton = juce::ModifierKeys {
        juce::ModifierKeys::rightButtonModifier
    };
    const auto mixedPrimaryPopup = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::rightButtonModifier
    };
    const auto mixedPrimaryMiddle = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::middleButtonModifier
    };
    const auto position = juce::Point<float> { 200.0f, 100.0f };

    LfoEditor editor;
    prepareEditor(editor);
    editor.setDataToDisplay(makeLfoData({
        { 0.0f, 0.20f }, { 0.45f, 0.75f }, { 1.0f, 0.30f }
    }));
    int menuLaunchCount = 0;
    LfoEditorTestAccess::setContextMenuLaunchHook(
        editor, [&] { ++menuLaunchCount; });

    SECTION("a popup release without its popup down is inert")
    {
        editor.mouseUp(makeMouseEvent(editor, position, popupButton));
        CHECK_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK(menuLaunchCount == 0);

        editor.dismissTransientInteraction();
        juce::PopupMenu::dismissAllActiveMenus();
    }

    SECTION("mixed buttons are rejected as one owned sequence")
    {
        editor.mouseDown(makeMouseEvent(
            editor, position, mixedPrimaryPopup));
        CHECK(LfoEditorTestAccess::isRejectedPointerGesture(editor));

        editor.mouseUp(makeMouseEvent(editor, position));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(menuLaunchCount == 0);

        editor.mouseUp(makeMouseEvent(editor, position, popupButton));
        CHECK(menuLaunchCount == 0);

        editor.mouseDown(makeMouseEvent(
            editor, position, mixedPrimaryMiddle));
        CHECK(LfoEditorTestAccess::isRejectedPointerGesture(editor));
        editor.mouseUp(makeMouseEvent(editor, position));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK(menuLaunchCount == 0);
    }

    SECTION("only the popup-down source can open its menu")
    {
        editor.mouseDown(makeMouseEvent(editor, position, popupButton));
        CHECK(LfoEditorTestAccess::isPopupPointerGesture(editor));

        makeTrackedPointerIndexForeign(editor, source);
        editor.mouseUp(makeMouseEvent(editor, position));
        CHECK(LfoEditorTestAccess::isPopupPointerGesture(editor));
        CHECK_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(menuLaunchCount == 0);

        makeTrackedPointerTypeForeign(editor, source);
        editor.mouseUp(makeMouseEvent(editor, position));
        CHECK(LfoEditorTestAccess::isPopupPointerGesture(editor));
        CHECK_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(menuLaunchCount == 0);

        restoreTrackedPointerSource(editor, source);
        editor.mouseUp(makeMouseEvent(editor, position));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK(LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(menuLaunchCount == 1);

        editor.mouseUp(makeMouseEvent(editor, position, popupButton));
        CHECK(menuLaunchCount == 1);
        CHECK(LfoEditorTestAccess::hasActiveContextMenuSession(editor));

        editor.dismissTransientInteraction();
        juce::PopupMenu::dismissAllActiveMenus();
    }

    SECTION("popup modifiers on primary release cannot steal a brush")
    {
        editor.setGridDivisions(4, 4);
        editor.setCurrentBrush(LfoPresetShape::SawUp);
        editor.setEditMode(LfoEditMode::BrushPaint);
        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

        editor.mouseDown(makeMouseEvent(
            editor, position, leftButton));
        REQUIRE(publicationCount == 1);
        REQUIRE(LfoEditorTestAccess::isBrushing(editor));

        editor.mouseUp(makeMouseEvent(
            editor, position, popupButton, position));
        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(publicationCount == 1);
        CHECK(menuLaunchCount == 0);

        editor.dismissTransientInteraction();
        juce::PopupMenu::dismissAllActiveMenus();
    }
}

TEST_CASE("LFO edit gestures ignore foreign pointer drag and release events",
          "[lfo][editor][input][gesture][ownership][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto data = makeLfoData({
        { 0.0f, 0.20f }, { 0.35f, 0.70f },
        { 0.70f, 0.35f }, { 1.0f, 0.80f }
    });

    SECTION("brush")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        editor.setGridDivisions(4, 4);
        editor.setCurrentBrush(LfoPresetShape::SineConvex);
        editor.setEditMode(LfoEditMode::BrushPaint);
        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

        const juce::Point<float> downPosition { 150.0f, 25.0f };
        const juce::Point<float> dragPosition { 250.0f, 25.0f };
        editor.mouseDown(makeMouseEvent(
            editor, downPosition, leftButton));
        REQUIRE(publicationCount == 1);
        const auto afterMouseDown = LfoEditorTestAccess::data(editor);

        makeTrackedPointerIndexForeign(editor, source);
        editor.mouseDown(makeMouseEvent(
            editor, dragPosition, leftButton));
        CHECK(publicationCount == 1);
        CHECK(LfoEditorTestAccess::lastBrushCell(editor)
              == juce::Point<int>(1, 0));
        checkSameLfoData(
            LfoEditorTestAccess::data(editor), afterMouseDown);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        CHECK(publicationCount == 1);
        checkSameLfoData(
            LfoEditorTestAccess::data(editor), afterMouseDown);
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::isPrimaryPointerGesture(editor));

        makeTrackedPointerTypeForeign(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(publicationCount == 1);
        CHECK(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::isPrimaryPointerGesture(editor));

        restoreTrackedPointerSource(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        CHECK(publicationCount == 2);
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
    }

    SECTION("point")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

        const auto downPosition =
            LfoEditorTestAccess::pointScreenPosition(editor, 1);
        const auto dragPosition =
            downPosition + juce::Point<float> { 24.0f, -18.0f };
        editor.mouseDown(makeMouseEvent(
            editor, downPosition, leftButton));
        REQUIRE(LfoEditorTestAccess::isPointInteraction(editor));

        makeTrackedPointerIndexForeign(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        CHECK(publicationCount == 0);
        checkSameLfoData(LfoEditorTestAccess::data(editor), data);
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(LfoEditorTestAccess::isPointInteraction(editor));

        restoreTrackedPointerSource(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        CHECK(publicationCount == 1);
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(publicationCount == 2);
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
    }

    SECTION("curve")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

        const juce::Point<float> downPosition { 200.0f, 175.0f };
        const juce::Point<float> dragPosition { 200.0f, 135.0f };
        editor.mouseDown(makeMouseEvent(
            editor, downPosition, leftButton));
        REQUIRE(LfoEditorTestAccess::isCurveInteraction(editor));

        makeTrackedPointerTypeForeign(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        CHECK(publicationCount == 0);
        checkSameLfoData(LfoEditorTestAccess::data(editor), data);
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(LfoEditorTestAccess::isCurveInteraction(editor));

        restoreTrackedPointerSource(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        CHECK(publicationCount == 1);
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
    }

    SECTION("marquee")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        const auto shiftPrimary = juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier
            | juce::ModifierKeys::shiftModifier
        };
        const juce::Point<float> downPosition { 10.0f, 10.0f };
        const juce::Point<float> dragPosition { 260.0f, 170.0f };
        editor.mouseDown(makeMouseEvent(
            editor, downPosition, shiftPrimary));
        REQUIRE(LfoEditorTestAccess::isMarqueeInteraction(editor));

        makeTrackedPointerIndexForeign(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, shiftPrimary, downPosition, true));
        CHECK_FALSE(
            LfoEditorTestAccess::hasSelectionRectangle(editor));
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(LfoEditorTestAccess::isMarqueeInteraction(editor));
        CHECK(LfoEditorTestAccess::selectedPointCount(editor) == 0);

        restoreTrackedPointerSource(editor, source);
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, shiftPrimary, downPosition, true));
        CHECK(LfoEditorTestAccess::hasSelectionRectangle(editor));
        editor.mouseUp(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
    }
}

TEST_CASE("LFO editor recovers omitted releases only from the owning pointer",
          "[lfo][editor][input][gesture][recovery][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto data = makeLfoData({
        { 0.0f, 0.20f }, { 0.35f, 0.70f },
        { 0.70f, 0.35f }, { 1.0f, 0.80f }
    });

    SECTION("a second input source cannot complete an owned point edit")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

        const auto downPosition =
            LfoEditorTestAccess::pointScreenPosition(editor, 1);
        const auto dragPosition =
            downPosition + juce::Point<float> { 24.0f, -18.0f };
        editor.mouseDown(makeMouseEvent(
            editor, downPosition, leftButton));
        editor.mouseDrag(makeMouseEvent(
            editor, dragPosition, leftButton, downPosition, true));
        REQUIRE(publicationCount == 1);
        REQUIRE(LfoEditorTestAccess::isPointInteraction(editor));

        // Model an interleaved hover from source 0 while the accepted down is
        // owned by source 1. It must not recover or steal that edit.
        makeTrackedPointerIndexForeign(editor, source);
        editor.mouseMove(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(publicationCount == 1);
        CHECK(LfoEditorTestAccess::isPointInteraction(editor));
        CHECK(LfoEditorTestAccess::isPrimaryPointerGesture(editor));

        // The first same-source hover without the primary button is the missing
        // release boundary and performs the normal final Point publication.
        restoreTrackedPointerSource(editor, source);
        editor.mouseMove(makeMouseEvent(
            editor, dragPosition, {}, downPosition, true));
        CHECK(publicationCount == 2);
        CHECK_FALSE(LfoEditorTestAccess::isPointInteraction(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
    }

    SECTION("an owned brush edit completes on a same-source exit")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        editor.setGridDivisions(4, 4);
        editor.setCurrentBrush(LfoPresetShape::SineConvex);
        editor.setEditMode(LfoEditMode::BrushPaint);
        int publicationCount = 0;
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };

        const juce::Point<float> downPosition { 150.0f, 25.0f };
        editor.mouseDown(makeMouseEvent(
            editor, downPosition, leftButton));
        REQUIRE(publicationCount == 1);
        REQUIRE(LfoEditorTestAccess::isBrushing(editor));

        editor.mouseExit(makeMouseEvent(
            editor, downPosition, {}, downPosition, true));
        CHECK(publicationCount == 1);
        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::lastBrushCell(editor)
              == juce::Point<int>(-1, -1));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK(LfoEditorTestAccess::interactionStateIsValid(editor));
    }

    SECTION("popup and rejected downs recover as cancellation without a menu")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        int menuLaunchCount = 0;
        LfoEditorTestAccess::setContextMenuLaunchHook(
            editor, [&] { ++menuLaunchCount; });
        const juce::Point<float> position { 200.0f, 100.0f };
        const auto popupButton = juce::ModifierKeys {
            juce::ModifierKeys::rightButtonModifier
        };
        const auto rejectedButtons = juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier
            | juce::ModifierKeys::middleButtonModifier
        };

        editor.mouseDown(makeMouseEvent(editor, position, popupButton));
        REQUIRE(LfoEditorTestAccess::isPopupPointerGesture(editor));
        editor.mouseEnter(makeMouseEvent(editor, position));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(menuLaunchCount == 0);

        editor.mouseDown(makeMouseEvent(editor, position, rejectedButtons));
        REQUIRE(LfoEditorTestAccess::isRejectedPointerGesture(editor));
        editor.mouseMove(makeMouseEvent(editor, position));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK_FALSE(
            LfoEditorTestAccess::hasActiveContextMenuSession(editor));
        CHECK(menuLaunchCount == 0);
    }

    SECTION("a recovered Point publication may delete the editor")
    {
        auto editor = std::make_unique<LfoEditor>();
        prepareEditor(*editor);
        editor->setDataToDisplay(data);
        const auto downPosition =
            LfoEditorTestAccess::pointScreenPosition(*editor, 1);
        const auto dragPosition =
            downPosition + juce::Point<float> { 24.0f, -18.0f };
        int publicationCount = 0;
        editor->onDataChanged = [&](const LfoData&)
        {
            if (++publicationCount == 2)
                editor.reset();
        };
        auto* const rawEditor = editor.get();

        rawEditor->mouseDown(makeMouseEvent(
            *rawEditor, downPosition, leftButton));
        rawEditor->mouseDrag(makeMouseEvent(
            *rawEditor, dragPosition, leftButton, downPosition, true));
        REQUIRE(publicationCount == 1);

        rawEditor->mouseMove(makeMouseEvent(
            *rawEditor, dragPosition, {}, downPosition, true));
        CHECK(publicationCount == 2);
        CHECK(editor == nullptr);
    }
}

TEST_CASE("LFO editor lifecycle boundaries cancel pointer state without publishing",
          "[lfo][editor][input][gesture][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto data = makeLfoData({
        { 0.0f, 0.20f }, { 0.35f, 0.70f },
        { 0.70f, 0.35f }, { 1.0f, 0.80f }
    });

    const auto configureBrush = [&](LfoEditor& editor,
                                    int& publicationCount)
    {
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        editor.setGridDivisions(4, 4);
        editor.setCurrentBrush(LfoPresetShape::SineConvex);
        editor.setEditMode(LfoEditMode::BrushPaint);
        editor.onDataChanged = [&](const LfoData&) { ++publicationCount; };
        editor.mouseDown(makeMouseEvent(
            editor, { 150.0f, 25.0f }, leftButton));
        REQUIRE(publicationCount == 1);
        REQUIRE(LfoEditorTestAccess::isBrushing(editor));
    };

    SECTION("explicit dismissal")
    {
        LfoEditor editor;
        int publicationCount = 0;
        configureBrush(editor, publicationCount);
        editor.dismissTransientInteraction();

        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        editor.mouseDrag(makeMouseEvent(
            editor, { 250.0f, 25.0f }, leftButton,
            { 150.0f, 25.0f }));
        editor.mouseUp(makeMouseEvent(
            editor, { 250.0f, 25.0f }, {},
            { 150.0f, 25.0f }));
        CHECK(publicationCount == 1);
    }

    SECTION("hiding")
    {
        LfoEditor editor;
        int publicationCount = 0;
        editor.setVisible(true);
        configureBrush(editor, publicationCount);
        editor.setVisible(false);

        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK(publicationCount == 1);
    }

    SECTION("disabling")
    {
        LfoEditor editor;
        int publicationCount = 0;
        configureBrush(editor, publicationCount);
        editor.setEnabled(false);

        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK(publicationCount == 1);
    }

    SECTION("data and mode replacement")
    {
        LfoEditor editor;
        int publicationCount = 0;
        configureBrush(editor, publicationCount);
        editor.setEditMode(LfoEditMode::PointEdit);
        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));

        editor.setEditMode(LfoEditMode::BrushPaint);
        editor.mouseDown(makeMouseEvent(
            editor, { 250.0f, 25.0f }, leftButton));
        REQUIRE(LfoEditorTestAccess::isBrushing(editor));
        editor.setDataToDisplay(data);
        CHECK_FALSE(LfoEditorTestAccess::isBrushing(editor));
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
    }

    SECTION("a fresh owned down replaces a stale marquee")
    {
        LfoEditor editor;
        prepareEditor(editor);
        editor.setDataToDisplay(data);
        const auto shiftPrimary = juce::ModifierKeys {
            juce::ModifierKeys::leftButtonModifier
            | juce::ModifierKeys::shiftModifier
        };
        const juce::Point<float> marqueeStart { 10.0f, 10.0f };
        editor.mouseDown(makeMouseEvent(
            editor, marqueeStart, shiftPrimary));
        editor.mouseDrag(makeMouseEvent(
            editor, { 100.0f, 100.0f }, shiftPrimary, marqueeStart));
        REQUIRE(LfoEditorTestAccess::hasSelectionRectangle(editor));

        const auto pointPosition =
            LfoEditorTestAccess::pointScreenPosition(editor, 1);
        editor.mouseDown(makeMouseEvent(
            editor, pointPosition, leftButton));
        CHECK_FALSE(
            LfoEditorTestAccess::hasSelectionRectangle(editor));
        CHECK(LfoEditorTestAccess::isPointInteraction(editor));
        CHECK(LfoEditorTestAccess::isPrimaryPointerGesture(editor));
    }
}

TEST_CASE("LFO panel dismissal cancels its editor gesture without another commit",
          "[lfo][panel][editor][input][gesture][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.getLfoManager().setLfoData(0, makeLfoData({
        { 0.0f, 0.20f }, { 0.35f, 0.70f }, { 1.0f, 0.30f }
    }));
    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    auto* editor = findLfoEditor(panel);
    REQUIRE(editor != nullptr);
    prepareEditor(*editor);
    editor->setGridDivisions(4, 4);
    editor->setCurrentBrush(LfoPresetShape::SineConvex);
    editor->setEditMode(LfoEditMode::BrushPaint);

    int dirtyCount = 0;
    panel.setOnDataChangedCallback([&] { ++dirtyCount; });
    editor->mouseDown(makeMouseEvent(
        *editor, { 150.0f, 25.0f }, leftButton));
    REQUIRE(dirtyCount == 1);
    REQUIRE(LfoEditorTestAccess::isBrushing(*editor));

    panel.dismissTransientInteraction();
    CHECK_FALSE(LfoEditorTestAccess::isBrushing(*editor));
    CHECK(LfoEditorTestAccess::hasNoPointerGesture(*editor));
    CHECK(dirtyCount == 1);
}

TEST_CASE("LFO double-click edits require an enabled pure-primary sequence",
          "[lfo][editor][input][double-click][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto original = makeLfoData({
        { 0.0f, 0.20f }, { 0.50f, 0.50f }, { 1.0f, 0.80f }
    });
    const auto replacement = makeLfoData({
        { 0.0f, 0.85f }, { 0.30f, 0.25f },
        { 0.75f, 0.70f }, { 1.0f, 0.15f }
    });
    const juce::Point<float> interiorPoint { 200.0f, 100.0f };
    const auto mixedPrimaryMiddle = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::middleButtonModifier
    };
    const auto popupButton = juce::ModifierKeys {
        juce::ModifierKeys::rightButtonModifier
    };

    LfoEditor editor;
    prepareEditor(editor);

    SECTION("a double-click without its completed primary sequence is inert")
    {
        editor.setDataToDisplay(original);
        editor.mouseDoubleClick(makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2));
        checkSameLfoData(LfoEditorTestAccess::data(editor), original);
    }

    SECTION("mixed buttons")
    {
        editor.setDataToDisplay(original);
        editor.mouseDown(makeMouseEvent(
            editor, interiorPoint, mixedPrimaryMiddle,
            interiorPoint, false, 2));
        const auto mixedRelease = makeMouseEvent(
            editor, interiorPoint, mixedPrimaryMiddle,
            interiorPoint, false, 2);
        editor.mouseUp(mixedRelease);
        editor.mouseDoubleClick(mixedRelease);
        checkSameLfoData(LfoEditorTestAccess::data(editor), original);
    }

    SECTION("disabled editor")
    {
        editor.setDataToDisplay(original);
        editor.setEnabled(false);
        performOwnedPrimaryDoubleClick(editor, interiorPoint);
        checkSameLfoData(LfoEditorTestAccess::data(editor), original);
    }

    SECTION("a popup sequence cannot be reclassified as a primary double-click")
    {
        editor.setDataToDisplay(original);
        editor.mouseDown(makeMouseEvent(
            editor, interiorPoint, popupButton,
            interiorPoint, false, 2));
        editor.mouseDoubleClick(makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2));
        checkSameLfoData(LfoEditorTestAccess::data(editor), original);
        editor.dismissTransientInteraction();
    }

    SECTION("the completed primary authorization freezes both source dimensions")
    {
        editor.setDataToDisplay(original);
        editor.mouseDown(makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2));
        const auto completedRelease = makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2);
        editor.mouseUp(completedRelease);
        REQUIRE(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));

        LfoEditorTestAccess::setTrackedDoubleClickSource(
            editor, source.getType(), source.getIndex() + 1);
        editor.mouseDoubleClick(completedRelease);
        CHECK(LfoEditorTestAccess::pointCount(editor) == 3);
        CHECK(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));

        LfoEditorTestAccess::setTrackedDoubleClickSource(
            editor,
            source.getType() == juce::MouseInputSource::mouse
                ? juce::MouseInputSource::touch
                : juce::MouseInputSource::mouse,
            source.getIndex());
        editor.mouseDoubleClick(completedRelease);
        CHECK(LfoEditorTestAccess::pointCount(editor) == 3);
        CHECK(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));

        LfoEditorTestAccess::setTrackedDoubleClickSource(
            editor, source.getType(), source.getIndex());
        editor.mouseDoubleClick(completedRelease);
        CHECK(LfoEditorTestAccess::pointCount(editor) == 2);
        CHECK_FALSE(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));
    }

    SECTION("dismissal consumes a completed primary authorization")
    {
        editor.setDataToDisplay(original);
        editor.mouseDown(makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2));
        const auto completedRelease = makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2);
        editor.mouseUp(completedRelease);
        REQUIRE(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));

        editor.dismissTransientInteraction();
        CHECK_FALSE(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));
        editor.mouseDoubleClick(completedRelease);
        checkSameLfoData(LfoEditorTestAccess::data(editor), original);
    }

    SECTION("data replacement consumes a completed primary authorization")
    {
        editor.setDataToDisplay(original);
        editor.mouseDown(makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2));
        const auto completedRelease = makeMouseEvent(
            editor, interiorPoint, leftButton,
            interiorPoint, false, 2);
        editor.mouseUp(completedRelease);
        REQUIRE(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));

        editor.setDataToDisplay(replacement);
        CHECK_FALSE(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));
        editor.mouseDoubleClick(completedRelease);
        checkSameLfoData(
            LfoEditorTestAccess::data(editor), replacement);
    }

    SECTION("pure primary")
    {
        editor.setDataToDisplay(original);
        performOwnedPrimaryDoubleClick(editor, interiorPoint);
        CHECK(LfoEditorTestAccess::pointCount(editor) == 2);
        CHECK(LfoEditorTestAccess::hasNoPointerGesture(editor));
        CHECK_FALSE(LfoEditorTestAccess::hasPrimaryDoubleClickAuthorization(
            editor));
    }
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
    performOwnedPrimaryDoubleClick(editor, pointToRemove);

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

TEST_CASE("LFO editor point hover and focus feedback animate only while visible",
          "[lfo][editor][ui][animation]")
{
    LfoEditor editor;
    prepareEditor(editor);
    editor.setVisible(true);
    editor.setDataToDisplay(makeLfoData({ { 0.0f, 0.2f }, { 1.0f, 0.8f } }));

    const auto point = LfoEditorTestAccess::pointScreenPosition(editor, 0);
    editor.mouseMove(makeMouseEvent(editor, point));
    REQUIRE(LfoEditorTestAccess::hoveredPoint(editor) == 0);
    REQUIRE(LfoEditorTestAccess::animationIsRunning(editor));
    LfoEditorTestAccess::tickAnimation(editor);
    CHECK(LfoEditorTestAccess::hoverAmount(editor) > 0.0f);
    CHECK(LfoEditorTestAccess::hoverAmount(editor) < 1.0f);

    LfoEditorTestAccess::focusGained(editor);
    LfoEditorTestAccess::tickAnimation(editor);
    CHECK(LfoEditorTestAccess::focusAmount(editor) > 0.0f);

    editor.setVisible(false);
    CHECK_FALSE(LfoEditorTestAccess::animationIsRunning(editor));
    CHECK(LfoEditorTestAccess::hoveredPoint(editor) == -1);
    CHECK(LfoEditorTestAccess::hoverAmount(editor) == 0.0f);
}

TEST_CASE("LFO editor discards focus animation when its workspace ancestor hides",
          "[lfo][editor][ui][animation][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 500);
    panel.setVisible(true);

    auto* editor = findLfoEditor(panel);
    REQUIRE(editor != nullptr);
    editor->setDataToDisplay(
        makeLfoData({ { 0.0f, 0.2f }, { 1.0f, 0.8f } }));

    LfoEditorTestAccess::focusGained(*editor);
    LfoEditorTestAccess::tickAnimation(*editor);
    REQUIRE(LfoEditorTestAccess::focusAmount(*editor) > 0.0f);

    // JUCE does not send visibilityChanged() to a child when only its parent
    // is hidden. LfoPanel's workspace cleanup must still clear the animation
    // synchronously because the hidden child's timer cannot finish the fade.
    panel.setVisible(false);
    CHECK(LfoEditorTestAccess::focusAmount(*editor) == 0.0f);
    CHECK_FALSE(LfoEditorTestAccess::animationIsRunning(*editor));

    panel.setVisible(true);
    CHECK(LfoEditorTestAccess::focusAmount(*editor) == 0.0f);
    CHECK_FALSE(LfoEditorTestAccess::animationIsRunning(*editor));
}

TEST_CASE("LFO editor point hit radius follows its visual scale",
          "[lfo][editor][ui][scale]")
{
    LfoEditor editor;
    editor.setDataToDisplay(makeLfoData({ { 0.5f, 0.5f }, { 1.0f, 0.8f } }));

    editor.setBounds(0, 0, 240, 120);
    const auto compactRadius = LfoEditorTestAccess::pointHitRadius(editor);
    editor.setBounds(0, 0, 800, 400);
    const auto largeRadius = LfoEditorTestAccess::pointHitRadius(editor);
    REQUIRE(largeRadius > compactRadius);

    const auto point = LfoEditorTestAccess::pointScreenPosition(editor, 0);
    editor.mouseMove(makeMouseEvent(editor,
                                    point + juce::Point<float>(largeRadius * 0.9f, 0.0f)));
    CHECK(LfoEditorTestAccess::hoveredPoint(editor) == 0);
    editor.mouseMove(makeMouseEvent(editor,
                                    point + juce::Point<float>(largeRadius * 1.1f, 0.0f)));
    CHECK(LfoEditorTestAccess::hoveredPoint(editor) == -1);
}
