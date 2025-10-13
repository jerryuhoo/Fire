#include "LfoPanel.h"
#include "../../DSP/LfoShapeGenerator.h"
#include "../../PluginProcessor.h"

juce::Rectangle<int> makeNormalised(const juce::Point<int>& p1,
                                    const juce::Point<int>& p2)
{
    return juce::Rectangle<int>::leftTopRightBottom(juce::jmin(p1.x, p2.x),
                                                    juce::jmin(p1.y, p2.y),
                                                    juce::jmax(p1.x, p2.x),
                                                    juce::jmax(p1.y, p2.y));
}

//==============================================================================
// LfoEditor Implementation
//==============================================================================

LfoEditor::LfoEditor()
{
    setWantsKeyboardFocus(true);
}

LfoEditor::~LfoEditor() {}

void LfoEditor::setDataToDisplay(const LfoData& dataToDisplay)
{
    // Safely switch the data source by copying.
    activeLfoData = dataToDisplay;
    dataIsActive = true;
    repaint();
}

void LfoEditor::paint(juce::Graphics& g)
{
    // Paint background
    g.fillAll(juce::Colours::black.brighter(0.1f));
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1.0f);

    // Paint grid
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    for (int i = 1; i < hGridDivs; ++i)
        g.drawVerticalLine(juce::roundToInt(getWidth() * i / (float) hGridDivs), 0.0f, (float) getHeight());
    for (int i = 1; i < vGridDivs; ++i)
        g.drawHorizontalLine(juce::roundToInt(getHeight() * i / (float) vGridDivs), 0.0f, (float) getWidth());

    if (! dataIsActive || activeLfoData.points.size() < 2)
        return;

    // Draw LFO Path
    juce::Path lfoPath;
    lfoPath.startNewSubPath(fromNormalized(activeLfoData.points.front()));

    for (size_t i = 0; i < activeLfoData.points.size() - 1; ++i)
    {
        auto p1_screen = fromNormalized(activeLfoData.points[i]);
        auto p2_screen = fromNormalized(activeLfoData.points[i + 1]);

        if (std::abs(p1_screen.x - p2_screen.x) < 0.1f)
        {
            lfoPath.lineTo(p2_screen);
            continue;
        }

        bool isCurved = (i < activeLfoData.curvatures.size() && ! juce::approximatelyEqual(activeLfoData.curvatures[i], 0.0f));
        if (! isCurved)
        {
            lfoPath.lineTo(p2_screen);
        }
        else
        {
            const int numSegments = 30;
            const float curvature = activeLfoData.curvatures[i];

            for (int j = 1; j <= numSegments; ++j)
            {
                float tx = (float) j / (float) numSegments;
                const float absExp = std::pow(4.0f, std::abs(curvature));
                float ty;

                if (curvature >= 0.0f)
                {
                    ty = std::pow(tx, absExp);
                }
                else
                {
                    const float base = juce::jmax(0.0f, 1.0f - tx); // Prevents NaN
                    ty = 1.0f - std::pow(base, absExp);
                }

                float currentX = p1_screen.x + (p2_screen.x - p1_screen.x) * tx;
                float currentY = p1_screen.y + (p2_screen.y - p1_screen.y) * ty;

                if (std::isnan(currentX) || std::isnan(currentY))
                    continue;

                lfoPath.lineTo({ currentX, currentY });
            }
        }
    }

    g.setColour(juce::Colours::orange);
    g.strokePath(lfoPath, juce::PathStrokeType(2.0f));

    // Draw control points, with visual feedback for selection.
    for (int i = 0; i < activeLfoData.points.size(); ++i)
    {
        bool isSelected = std::find(selectedPointIndices.begin(), selectedPointIndices.end(), i) != selectedPointIndices.end();
        bool isHovered = (i == hoveredPointIndex);

        auto localPoint = fromNormalized(activeLfoData.points[i]);

        float currentPointRadius = pointRadius;
        juce::Colour currentPointColour = isSelected ? juce::Colours::cyan : juce::Colours::yellow;

        // Apply hover effect (enlarge and make transparent) to both selected and unselected points.
        if (isHovered)
        {
            currentPointRadius *= 1.5f;
            currentPointColour = currentPointColour.withAlpha(0.5f);
        }
        // If dragging a selection, make them slightly larger but keep them solid for clarity.
        else if (isSelected && (draggingState == DraggingState::Selection || draggingState == DraggingState::Point))
        {
            currentPointRadius *= 1.5f;
        }

        g.setColour(currentPointColour);
        g.fillEllipse(localPoint.x - currentPointRadius,
                      localPoint.y - currentPointRadius,
                      currentPointRadius * 2,
                      currentPointRadius * 2);
    }

    // Draw the marquee selection rectangle if the user is currently dragging it.
    if (draggingState == DraggingState::Marquee)
    {
        auto rectToDraw = makeNormalised(selectionRectangle.getPosition(),
                                         selectionRectangle.getBottomRight());

        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRoundedRectangle(rectToDraw.toFloat(), 2.0f);
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(rectToDraw.toFloat(), 2.0f, 1.0f);
    }

    // Draw playhead
    if (playheadPos >= 0.0f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawVerticalLine(juce::roundToInt(getWidth() * playheadPos), 0.0f, (float) getHeight());
    }

    // Draw phase offset line when dragging
    if (phaseOffsetPosition >= 0.0f)
    {
        g.setColour(COLOUR5.withAlpha(0.5f));
        g.drawVerticalLine(juce::roundToInt(getWidth() * phaseOffsetPosition), 0.0f, (float) getHeight());
    }
}

void LfoEditor::resized() {} // No layout logic needed in the editor itself.

void LfoEditor::setGridDivisions(int horizontal, int vertical)
{
    hGridDivs = juce::jmax(1, horizontal);
    vGridDivs = juce::jmax(1, vertical);
    repaint();
}

void LfoEditor::setPlayheadPosition(float position)
{
    if (! juce::approximatelyEqual(playheadPos, position))
    {
        playheadPos = position;
        repaint();
    }
}

void LfoEditor::setPhaseOffsetLinePosition(float position)
{
    if (! juce::approximatelyEqual(phaseOffsetPosition, position))
    {
        phaseOffsetPosition = position;
        repaint();
    }
}

void LfoEditor::mouseDown(const juce::MouseEvent& event)
{
    if (! dataIsActive)
        return;
    grabKeyboardFocus();

    if (currentMode == LfoEditMode::BrushPaint)
    {
        if (event.mods.isLeftButtonDown())
        {
            isBrushing = true;
            applyBrushShape(event.getPosition());

            const float gridW = 1.0f / (float) hGridDivs;
            const float gridH = 1.0f / (float) vGridDivs;
            const int gridX = juce::jmin(hGridDivs - 1, (int) ((float) event.x / (float) getWidth() / gridW));
            const int gridY = juce::jmin(vGridDivs - 1, (int) ((float) event.y / (float) getHeight() / gridH));
            lastBrushCell = { gridX, gridY };
        }
        return;
    }

    if (currentMode != LfoEditMode::PointEdit)
        return;

    // --- Right-click is always for curvature ---
    if (event.mods.isRightButtonDown())
    {
        return;
    }

    // --- Shift + Drag for Marquee Selection ---
    if (event.mods.isShiftDown())
    {
        draggingState = DraggingState::Marquee;
        selectionRectangle.setPosition(event.getPosition());
        selectedPointIndices.clear(); // Start a new selection
        repaint();
        return;
    }

    // --- Standard Left-click Logic ---
    int clickedPointIndex = -1;
    for (size_t i = 0; i < activeLfoData.points.size(); ++i)
    {
        if (fromNormalized(activeLfoData.points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius * 1.5f)
        {
            clickedPointIndex = (int) i;
            break;
        }
    }

    bool isPointAlreadySelected = std::find(selectedPointIndices.begin(), selectedPointIndices.end(), clickedPointIndex) != selectedPointIndices.end();

    if (clickedPointIndex != -1)
    {
        if (isPointAlreadySelected)
        {
            // Clicked on an already selected point: prepare to drag the whole selection.
            draggingState = DraggingState::Selection;
        }
        else
        {
            // Clicked on an unselected point: clear old selection, select this new one, and prepare to drag it.
            draggingState = DraggingState::Point;
            selectedPointIndices.clear();
            selectedPointIndices.push_back(clickedPointIndex);
        }
    }
    else // Clicked on empty space: create a new point and prepare to drag it.
    {
        editingCurveIndex = findSegmentIndexAt(event.getPosition());
        if (editingCurveIndex != -1)
        {
            initialCurvature = activeLfoData.curvatures[editingCurveIndex];
            initialDragY = event.y;
        }
    }

    // If we are starting any kind of drag, store the initial positions of all selected points.
    if (draggingState == DraggingState::Point || draggingState == DraggingState::Selection)
    {
        dragAnchor = toNormalized(event.getPosition());
        initialDragPositions.clear();
        for (int index : selectedPointIndices)
            initialDragPositions.push_back(activeLfoData.points[index]);
    }

    repaint();
}

void LfoEditor::mouseDrag(const juce::MouseEvent& event)
{
    // First, handle the brush drag if it's active.
    if (isBrushing && event.mods.isLeftButtonDown())
    {
        const float gridW = 1.0f / (float) hGridDivs;
        const float gridH = 1.0f / (float) vGridDivs;
        const int gridX = juce::jmin(hGridDivs - 1, (int) ((float) event.x / (float) getWidth() / gridW));
        const int gridY = juce::jmin(vGridDivs - 1, (int) ((float) event.y / (float) getHeight() / gridH));
        const juce::Point<int> currentCell { gridX, gridY };

        if (currentCell != lastBrushCell)
        {
            applyBrushShape(event.getPosition());
            lastBrushCell = currentCell;
            if (onDataChanged)
                onDataChanged(activeLfoData);
        }
        return; // Brush drag is handled, so we exit here.
    }

    // If not brushing, proceed with the point editing logic.
    if (! dataIsActive || currentMode != LfoEditMode::PointEdit)
        return;

    if (editingCurveIndex != -1)
    {
        // Get the start and end points of the segment in screen coordinates to calculate the visual slope.
        auto p1_screen = fromNormalized(activeLfoData.points[editingCurveIndex]);
        auto p2_screen = fromNormalized(activeLfoData.points[editingCurveIndex + 1]);

        float dx = p2_screen.x - p1_screen.x;
        // Note: in screen coordinates, a smaller Y is higher up.
        float dy = p2_screen.y - p1_screen.y;

        float slope = (dx != 0.0f) ? (dy / dx) : 0.0f;

        // getDistanceFromDragStartY() returns (currentY - startY). Dragging down is positive.
        float dragDistY = event.getDistanceFromDragStartY();

        const float sensitivity = 0.01f; // Controls how much effect the drag has.

        // The core logic: we want UP drag (negative distance) to always produce an UPWARD bulge (negative curvature).
        // To achieve this, we make the curvature change proportional to the drag distance,
        // but we invert it if the line is visually sloping downwards on the screen (positive slope).
        float curvatureChange = (slope > 0.0f) ? -dragDistY * sensitivity : dragDistY * sensitivity;

        activeLfoData.curvatures[editingCurveIndex] = juce::jlimit(-2.0f, 2.0f, initialCurvature + curvatureChange);

        repaint();
        if (onDataChanged)
            onDataChanged(activeLfoData);
        return; // Curvature drag is handled, so we exit here.
    }

    switch (draggingState)
    {
        case DraggingState::Marquee:
            selectionRectangle = makeNormalised(event.getMouseDownPosition(), event.getPosition());
            repaint();
            break;

        case DraggingState::Point:
        case DraggingState::Selection:
        {
            if (selectedPointIndices.empty())
                return;

            // 1. Calculate the raw, un-snapped delta from the anchor point.
            auto currentNormPos = toNormalized(event.getPosition());
            auto delta = currentNormPos - dragAnchor;

            // 2. If snapping is active, calculate the *snapped* target positions.
            if (event.mods.isCommandDown() || event.mods.isCtrlDown())
            {
                // For the primary point being dragged, find its ideal snapped position.
                int primaryIndex = selectedPointIndices.front();
                auto initialPrimaryPos = initialDragPositions.front();
                auto unsnappedTargetPos = initialPrimaryPos + delta;

                juce::Point<float> snappedTargetPos;
                snappedTargetPos.x = std::round(unsnappedTargetPos.x * hGridDivs) / (float) hGridDivs;
                snappedTargetPos.y = std::round(unsnappedTargetPos.y * vGridDivs) / (float) vGridDivs;

                // The new delta is the difference between the snapped target and the initial position.
                delta = snappedTargetPos - initialPrimaryPos;
            }

            // 3. Group Constraint Calculation (from previous fix)
            float maxLeftDelta = -2.0f, maxRightDelta = 2.0f; // Allow full range initially
            int leftmostSelectedPointIndex = -1, rightmostSelectedPointIndex = -1;

            for (int index : selectedPointIndices)
            {
                if (leftmostSelectedPointIndex == -1 || index < leftmostSelectedPointIndex)
                    leftmostSelectedPointIndex = index;
                if (rightmostSelectedPointIndex == -1 || index > rightmostSelectedPointIndex)
                    rightmostSelectedPointIndex = index;
            }

            if (leftmostSelectedPointIndex > 0)
            {
                int leftNeighborIndex = leftmostSelectedPointIndex - 1;
                if (std::find(selectedPointIndices.begin(), selectedPointIndices.end(), leftNeighborIndex) == selectedPointIndices.end())
                {
                    float initialLeftmostX = 0.0f;
                    for (size_t i = 0; i < selectedPointIndices.size(); ++i)
                        if (selectedPointIndices[i] == leftmostSelectedPointIndex)
                        {
                            initialLeftmostX = initialDragPositions[i].x;
                            break;
                        }
                    maxLeftDelta = activeLfoData.points[leftNeighborIndex].x - initialLeftmostX;
                }
            }

            if (rightmostSelectedPointIndex < activeLfoData.points.size() - 1)
            {
                int rightNeighborIndex = rightmostSelectedPointIndex + 1;
                if (std::find(selectedPointIndices.begin(), selectedPointIndices.end(), rightNeighborIndex) == selectedPointIndices.end())
                {
                    float initialRightmostX = 0.0f;
                    for (size_t i = 0; i < selectedPointIndices.size(); ++i)
                        if (selectedPointIndices[i] == rightmostSelectedPointIndex)
                        {
                            initialRightmostX = initialDragPositions[i].x;
                            break;
                        }
                    maxRightDelta = activeLfoData.points[rightNeighborIndex].x - initialRightmostX;
                }
            }

            delta.x = juce::jlimit(maxLeftDelta, maxRightDelta, delta.x);

            // 4. Apply the final, constrained (and possibly snapped) delta to all selected points.
            for (size_t i = 0; i < selectedPointIndices.size(); ++i)
            {
                int pointIndex = selectedPointIndices[i];
                auto& initialPos = initialDragPositions[i];
                auto& point = activeLfoData.points[pointIndex];
                point.y = juce::jlimit(0.0f, 1.0f, initialPos.y + delta.y);
                if (pointIndex > 0 && pointIndex < activeLfoData.points.size() - 1)
                {
                    point.x = initialPos.x + delta.x;
                }
            }
            repaint();

            if (onDataChanged)
                onDataChanged(activeLfoData);

            break;
        }

        case DraggingState::None:
        default:
            break;
    }
}

void LfoEditor::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        juce::PopupMenu m;
        m.addItem(CommandIDs::selectAll, "Select All");
        m.addItem(CommandIDs::clear, "Clear");
        m.addSeparator();
        m.addItem(CommandIDs::copy, "Copy", dataIsActive && activeLfoData.points.size() > 2);
        m.addItem(CommandIDs::paste, "Paste", lfoClipboard.points.size() > 0);
        m.addSeparator();
        m.addItem(CommandIDs::invertX, "Invert Horizontally", dataIsActive && activeLfoData.points.size() > 2);
        m.addItem(CommandIDs::invertY, "Invert Vertically", dataIsActive && activeLfoData.points.size() > 2);

        auto callback = [this](int result)
        {
            switch (result)
            {
                case CommandIDs::selectAll:
                    selectAllPoints();
                    break;
                case CommandIDs::clear:
                    clearAllPoints();
                    if (onDataChanged)
                        onDataChanged(activeLfoData);
                    break;
                case CommandIDs::copy:
                    copyShape();
                    break;
                case CommandIDs::paste:
                    pasteShape();
                    if (onDataChanged)
                        onDataChanged(activeLfoData);
                    break;
                case CommandIDs::invertX:
                    invertShape(true, false);
                    if (onDataChanged)
                        onDataChanged(activeLfoData);
                    break;
                case CommandIDs::invertY:
                    invertShape(false, true);
                    if (onDataChanged)
                        onDataChanged(activeLfoData);
                    break;
                default:
                    break;
            }
        };

        const auto screenArea = juce::Rectangle<int>(event.getScreenX(), event.getScreenY(), 1, 1);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenArea), callback);

        return; // We've handled the right-click, so we exit here.
    }

    if (! dataIsActive)
        return;
    bool dataWasChanged = false;

    if (draggingState == DraggingState::Marquee)
    {
        auto finalRect = makeNormalised(event.getMouseDownPosition(), event.getPosition()).toFloat();
        selectedPointIndices.clear();
        for (int i = 0; i < activeLfoData.points.size(); ++i)
        {
            if (finalRect.contains(fromNormalized(activeLfoData.points[i])))
                selectedPointIndices.push_back(i);
        }
        selectionRectangle.setSize(0, 0);
    }

    if (draggingState == DraggingState::Point || draggingState == DraggingState::Selection)
    {
        updateAndSortPoints();
        dataWasChanged = true;
    }

    draggingState = DraggingState::None;
    editingCurveIndex = -1;

    if (isBrushing)
    {
        isBrushing = false;
        lastBrushCell = { -1, -1 };
        dataWasChanged = true;
    }

    repaint();

    if (dataWasChanged && onDataChanged)
    {
        if (dataIsActive)
            activeLfoData.mergeDuplicatePoints();
        onDataChanged(activeLfoData);
    }
}

void LfoEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (! dataIsActive || currentMode != LfoEditMode::PointEdit)
        return;

    // First, check if double-clicking on an existing point to delete it.
    // We check from the second to the second-to-last point, as the ends cannot be deleted.
    if (activeLfoData.points.size() > 2)
    {
        for (size_t i = 1; i < activeLfoData.points.size() - 1; ++i)
        {
            if (fromNormalized(activeLfoData.points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius * 1.5f)
            {
                removePoint((int) i);
                if (onDataChanged)
                    onDataChanged(activeLfoData);
                repaint();
                return; // Point was found and removed, so we're done.
            }
        }
    }

    // If we reach here, it means we didn't double-click on an existing point.
    // So, we create a new point at the double-click location.
    if (activeLfoData.points.size() < maxPoints)
    {
        addPoint(toNormalized(event.getPosition()));
        if (onDataChanged)
            onDataChanged(activeLfoData);
        // addPoint already calls repaint().
    }
}

void LfoEditor::mouseMove(const juce::MouseEvent& event)
{
    // Find if the mouse is currently hovering over any point
    int newHoveredIndex = -1;
    for (int i = 0; i < activeLfoData.points.size(); ++i)
    {
        auto pointScreen = fromNormalized(activeLfoData.points[i]);

        if (pointScreen.getDistanceFrom(event.getPosition().toFloat()) < 5.0f)
        {
            newHoveredIndex = i;
            break;
        }
    }

    // If the hovered status has changed, update and repaint
    if (newHoveredIndex != hoveredPointIndex)
    {
        hoveredPointIndex = newHoveredIndex;
        repaint(); // Force a repaint to show the highlight/size change
    }
}

void LfoEditor::mouseExit(const juce::MouseEvent& event)
{
    // When the mouse leaves the component, clear any hovered state and repaint
    if (hoveredPointIndex != -1)
    {
        hoveredPointIndex = -1;
        repaint(); // Force a repaint to remove the highlight/size change
    }
}

void LfoEditor::addPoint(juce::Point<float> newPoint)
{
    if (! dataIsActive || activeLfoData.points.size() >= maxPoints)
        return;

    // Add the point and sort the list to find its correct position.
    activeLfoData.points.push_back(newPoint);
    updateAndSortPoints();

    // Find the index of the point we just added.
    auto it = std::find_if(activeLfoData.points.begin(), activeLfoData.points.end(), [&](const auto& p)
                           { return juce::approximatelyEqual(p.x, newPoint.x) && juce::approximatelyEqual(p.y, newPoint.y); });

    if (it != activeLfoData.points.end())
    {
        int insertedAtIndex = (int) std::distance(activeLfoData.points.begin(), it);

        // A new point splits a segment, so we need one more curvature value.
        // We insert a new 0.0f (linear) curvature for the new segment being created.
        // All other curvatures are preserved.
        if (insertedAtIndex > 0)
        {
            activeLfoData.curvatures.insert(activeLfoData.curvatures.begin() + insertedAtIndex - 1, 0.0f);
        }
        else
        {
            // This should only happen if adding a point before the second point, very rare.
            activeLfoData.curvatures.insert(activeLfoData.curvatures.begin(), 0.0f);
        }
    }

    repaint();
}

void LfoEditor::removePoint(int index)
{
    if (! dataIsActive || index <= 0 || index >= activeLfoData.points.size() - 1)
        return;

    activeLfoData.points.erase(activeLfoData.points.begin() + index);

    // Removing a point merges two segments. We must remove one curvature value.
    // We remove the curvature of the first of the two merged segments.
    activeLfoData.curvatures.erase(activeLfoData.curvatures.begin() + index - 1);

    // We then set the curvature of the new, merged segment to 0.0 (linear).
    if (index - 1 < activeLfoData.curvatures.size())
    {
        activeLfoData.curvatures[index - 1] = 0.0f;
    }

    repaint();
}

juce::Point<float> LfoEditor::toNormalized(juce::Point<int> localPoint)
{
    return { (float) localPoint.x / (float) getWidth(), 1.0f - ((float) localPoint.y / (float) getHeight()) };
}

juce::Point<float> LfoEditor::fromNormalized(juce::Point<float> normalizedPoint)
{
    // Ensure the calculation is done with floats and returns a float point
    return { normalizedPoint.x * (float) getWidth(), (1.0f - normalizedPoint.y) * (float) getHeight() };
}

void LfoEditor::updateAndSortPoints()
{
    if (! dataIsActive)
        return;
    // Use stable_sort to preserve the order of points with the same x-coordinate.
    std::stable_sort(activeLfoData.points.begin(), activeLfoData.points.end(), [](const auto& a, const auto& b)
                     { return a.x < b.x; });

    if (activeLfoData.points.empty())
        activeLfoData.resetToDefault();
    else
    {
        activeLfoData.points.front().x = 0.0f;
        activeLfoData.points.back().x = 1.0f;
    }
}

int LfoEditor::getOrCreatePointAtX(float targetX)
{
    if (! dataIsActive)
        return -1;

    // 1. Check if a point already exists at (or very close to) the target X.
    for (size_t i = 0; i < activeLfoData.points.size(); ++i)
    {
        if (juce::approximatelyEqual(activeLfoData.points[i].x, targetX))
            return static_cast<int>(i);
    }

    // 2. If not, find which segment the targetX falls into.
    for (size_t i = 0; i < activeLfoData.points.size() - 1; ++i)
    {
        auto& p1 = activeLfoData.points[i];
        auto& p2 = activeLfoData.points[i + 1];

        if (targetX > p1.x && targetX < p2.x)
        {
            // 3. Calculate the Y value on the segment at targetX.
            // For simplicity, this uses linear interpolation. A more advanced version could calculate the curved position.
            float segmentWidth = p2.x - p1.x;
            float t = (segmentWidth > 0) ? (targetX - p1.x) / segmentWidth : 0.0f;
            float newY = p1.y + (p2.y - p1.y) * t;

            // 4. Create and add the new point.
            addPoint({ targetX, newY });

            // 5. After adding, the points are re-sorted. We need to find the new point's index again.
            for (size_t j = 0; j < activeLfoData.points.size(); ++j)
            {
                if (juce::approximatelyEqual(activeLfoData.points[j].x, targetX))
                    return static_cast<int>(j);
            }
        }
    }
    return -1; // Should not happen on a valid curve.
}

void LfoEditor::rebuildCurvatures()
{
    if (! dataIsActive)
        return;
    size_t numSegments = activeLfoData.points.size() > 1 ? activeLfoData.points.size() - 1 : 0;
    activeLfoData.curvatures.assign(numSegments, 0.0f);
}

void LfoEditor::applyBrushShape(const juce::Point<int>& clickPosition)
{
    if (! dataIsActive)
        return;

    // 1. Identify grid cell and its boundaries.
    const float gridW = 1.0f / (float) hGridDivs;
    const float gridH = 1.0f / (float) vGridDivs;
    const int gridX = juce::jmin(hGridDivs - 1, (int) ((float) clickPosition.x / (float) getWidth() / gridW));
    const int gridY = juce::jmin(vGridDivs - 1, (int) ((float) clickPosition.y / (float) getHeight() / gridH));
    const float startX = (float) gridX * gridW;
    const float endX = (float) (gridX + 1) * gridW;
    const float bottomY = 1.0f - ((float) (gridY + 1) * gridH);
    const float topY = 1.0f - ((float) gridY * gridH);
    const juce::Rectangle<float> cellBounds(startX, bottomY, endX - startX, topY - bottomY);

    const auto oldPoints = activeLfoData.points;

    // 2. Cleanup old points with smarter boundary logic.
    activeLfoData.points.erase(
        std::remove_if(activeLfoData.points.begin(),
                       activeLfoData.points.end(),
                       [&](const auto& p)
                       {
                           // Check if the point is at the absolute start or end of the LFO.
                           bool isPointAtStartBoundary = juce::approximatelyEqual(p.x, 0.0f);
                           bool isPointAtEndBoundary = juce::approximatelyEqual(p.x, 1.0f);

                           // Check if the brush is painting over the start or end cell.
                           bool isBrushAtStartBoundary = juce::approximatelyEqual(startX, 0.0f);
                           bool isBrushAtEndBoundary = juce::approximatelyEqual(endX, 1.0f);

                           // Protect the boundary points ONLY if the brush is NOT painting over them.
                           if ((isPointAtStartBoundary && ! isBrushAtStartBoundary) || (isPointAtEndBoundary && ! isBrushAtEndBoundary))
                           {
                               return false; // Keep this point.
                           }

                           // For all other points, remove them if they fall within the brush's horizontal range.
                           return p.x >= startX && p.x <= endX;
                       }),
        activeLfoData.points.end());

    // 3. Generate the new shape's points.
    auto newPoints = LfoShapeGenerator::generateShape(currentBrush, cellBounds);

    activeLfoData.points.insert(activeLfoData.points.end(), newPoints.begin(), newPoints.end());

    updateAndSortPoints();

    // 4. Intelligently rebuild the curvatures vector to preserve old values.
    std::vector<float> newCurvatures;
    const auto& currentPoints = activeLfoData.points;

    for (size_t i = 0; i < currentPoints.size() - 1; ++i)
    {
        const auto& p1 = currentPoints[i];
        const auto& p2 = currentPoints[i + 1];

        bool isNewlyCreatedSineSegment = false;

        // Check if this segment is part of our newly added sine wave.
        bool isSineBrush = (currentBrush == LfoPresetShape::SineConvex || currentBrush == LfoPresetShape::SineConcave);
        if (isSineBrush && newPoints.size() == 3)
        {
            // Assign a predefined curvature for the two halves of the sine shape.
            float curvature = 0.0f;
            float sineCurvature = 0.55f;
            // Check if the current segment matches the first half of the generated sine points.
            if (juce::approximatelyEqual(p1.x, newPoints[0].x) && juce::approximatelyEqual(p2.x, newPoints[1].x))
            {
                curvature = -sineCurvature; // First half curves up.
            }
            // Check if the current segment matches the second half.
            else if (juce::approximatelyEqual(p1.x, newPoints[1].x) && juce::approximatelyEqual(p2.x, newPoints[2].x))
            {
                curvature = sineCurvature; // Second half curves down to complete the shape.
            }

            // If a curvature was set, add it and mark the segment as handled.
            if (curvature != 0.0f)
            {
                newCurvatures.push_back(curvature);
                isNewlyCreatedSineSegment = true;
            }
        }

        if (! isNewlyCreatedSineSegment)
        {
            float oldCurvature = 0.0f;
            for (size_t j = 0; j < oldPoints.size() - 1; ++j)
            {
                if (juce::approximatelyEqual(oldPoints[j].x, p1.x) && juce::approximatelyEqual(oldPoints[j + 1].x, p2.x))
                {
                    if (j < activeLfoData.curvatures.size())
                    {
                        oldCurvature = activeLfoData.curvatures[j];
                        break;
                    }
                }
            }
            newCurvatures.push_back(oldCurvature);
        }
    }

    activeLfoData.curvatures.swap(newCurvatures);

    repaint();
}

int LfoEditor::findSegmentIndexAt(const juce::Point<int>& position) const
{
    // Safety checks: ensure there is data and at least one segment to check.
    if (! dataIsActive || activeLfoData.points.size() < 2)
        return -1;

    // Convert the mouse's X position to a normalized value [0, 1]
    const float clickXNormalized = (float) position.x / (float) getWidth();

    // Iterate through all the points that define the start of a segment
    for (size_t i = 0; i < activeLfoData.points.size() - 1; ++i)
    {
        // A segment is defined by point 'i' and point 'i + 1'.
        const auto& p1 = activeLfoData.points[i];
        const auto& p2 = activeLfoData.points[i + 1];

        // Check if the normalized click position falls horizontally between the two points.
        if (clickXNormalized >= p1.x && clickXNormalized <= p2.x)
        {
            // If it does, we've found our segment. Return its starting index.
            return static_cast<int>(i);
        }
    }

    // If the loop completes, no segment was found at that X position.
    return -1;
}

void LfoEditor::setEditMode(LfoEditMode newMode)
{
    currentMode = newMode;

    // Reset any interaction state when changing modes
    draggingPointIndex = -1;
    editingCurveIndex = -1;
    hoveredPointIndex = -1;
}

void LfoEditor::setCurrentBrush(LfoPresetShape newBrush)
{
    currentBrush = newBrush;
}

bool LfoEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown()) // Command for macOS, Ctrl for Windows/Linux
    {
        if (key.getTextCharacter() == 'c' || key.getTextCharacter() == 'C')
        {
            copyShape();
            return true;
        }

        if (key.getTextCharacter() == 'v' || key.getTextCharacter() == 'V')
        {
            pasteShape();
            if (onDataChanged)
                onDataChanged(activeLfoData);
            return true;
        }

        if (key.getTextCharacter() == 'a' || key.getTextCharacter() == 'A')
        {
            selectAllPoints();
            return true;
        }
    }

    if (! selectedPointIndices.empty() && (key.isKeyCurrentlyDown(juce::KeyPress::deleteKey) || key.isKeyCurrentlyDown(juce::KeyPress::backspaceKey)))
    {
        deleteSelectedPoints();
        if (onDataChanged)
            onDataChanged(activeLfoData);
        return true;
    }
    return false;
}

void LfoEditor::deleteSelectedPoints()
{
    if (! dataIsActive || selectedPointIndices.empty())
        return;
    std::sort(selectedPointIndices.rbegin(), selectedPointIndices.rend());
    for (int index : selectedPointIndices)
    {
        if (index > 0 && index < activeLfoData.points.size() - 1)
            removePoint(index);
    }
    selectedPointIndices.clear();
    repaint();
}

//==============================================================================
// LfoPanel Implementation
//==============================================================================
LfoPanel::LfoPanel(FireAudioProcessor& p) : processor(p)
{
    lfoEditor.setDataToDisplay(processor.getLfoManager().getLfoData()[currentLfoIndex]);
    addAndMakeVisible(lfoEditor);

    // FIX: Set up the callback to send updated data back to the manager
    lfoEditor.onDataChanged = [this](const LfoData& newData)
    {
        // Use the thread-safe setter to update the authoritative data
        processor.getLfoManager().setLfoData(currentLfoIndex, newData);

        // Notify the main editor that a change has occurred (e.g., to mark preset as dirty)
        if (onDataChanged)
            onDataChanged();
    };

    // Create UI Components
    for (int i = 0; i < 4; ++i)
    {
        lfoSelectButtons[i] = std::make_unique<juce::TextButton>("LFO " + juce::String(i + 1));
        addAndMakeVisible(lfoSelectButtons[i].get());
        lfoSelectButtons[i]->setRadioGroupId(1);
        styleButton(*lfoSelectButtons[i], true); // It's a toggle button
    }
    lfoSelectButtons[0]->setToggleState(true, juce::dontSendNotification);

    // --- Setup Mode Buttons ---
    addAndMakeVisible(editModeButton);
    editModeButton.setRadioGroupId(1002);
    styleButton(editModeButton, true);

    addAndMakeVisible(brushModeButton);
    brushModeButton.setRadioGroupId(1002);
    styleButton(brushModeButton, true);

    // --- Setup Brush Selector ---
    addAndMakeVisible(brushSelector);
    brushSelector.addItem("Saw Up", (int) LfoPresetShape::SawUp);
    brushSelector.addItem("Saw Down", (int) LfoPresetShape::SawDown);
    brushSelector.addItem("Sine Convex", (int) LfoPresetShape::SineConvex);
    brushSelector.addItem("Sine Concave", (int) LfoPresetShape::SineConcave);
    brushSelector.addItem("Square High", (int) LfoPresetShape::SquareHigh);
    brushSelector.addItem("Square Low", (int) LfoPresetShape::SquareLow);
    brushSelector.onChange = [this]
    { lfoEditor.setCurrentBrush(static_cast<LfoPresetShape>(brushSelector.getSelectedId())); };

    setEditMode(LfoEditMode::PointEdit); // Set initial state

    addAndMakeVisible(assignButton);
    assignButton.setButtonText("Assign");
    styleButton(assignButton, true);

    addAndMakeVisible(matrixButton);
    matrixButton.setButtonText("Matrix"); // Set button text
    styleButton(matrixButton, false);

    addAndMakeVisible(syncButton);
    syncButton.setButtonText("BPM");
    styleButton(syncButton, true);

    addAndMakeVisible(rateSlider);
    rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);

    addAndMakeVisible(rateLabel);
    rateLabel.setText("Rate", juce::dontSendNotification);
    rateLabel.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain") });
    rateLabel.attachToComponent(&rateSlider, false);
    rateLabel.setColour(juce::Label::textColourId, COLOUR1);
    rateLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(gridXSlider);
    gridXSlider.setSliderStyle(juce::Slider::IncDecButtons);
    gridXSlider.setRange(2, 16, 1);
    gridXSlider.setValue(4);
    gridXSlider.addListener(this);
    gridXSlider.setColour(juce::Slider::thumbColourId, COLOUR1); // For the arrow
    gridXSlider.setColour(juce::Slider::textBoxOutlineColourId, COLOUR6.withAlpha(0.5f)); // Border color
    gridXSlider.setColour(juce::Slider::textBoxHighlightColourId, COLOUR8);
    gridXSlider.setColour(juce::TextButton::textColourOnId, COLOUR1);

    addAndMakeVisible(gridXLabel);
    gridXLabel.setText("X", juce::dontSendNotification);

    addAndMakeVisible(gridYSlider);
    gridYSlider.setSliderStyle(juce::Slider::IncDecButtons);
    gridYSlider.setRange(2, 16, 1);
    gridYSlider.setValue(4);
    gridYSlider.addListener(this);
    gridYSlider.setColour(juce::Slider::thumbColourId, COLOUR1); // For the arrow
    gridYSlider.setColour(juce::Slider::textBoxOutlineColourId, COLOUR6.withAlpha(0.5f)); // Border color
    gridYSlider.setColour(juce::Slider::textBoxHighlightColourId, COLOUR8);
    gridYSlider.setColour(juce::TextButton::textColourOnId, COLOUR1);

    addAndMakeVisible(gridYLabel);
    gridYLabel.setText("Y", juce::dontSendNotification);

    // Register as a parameter listener for each of the LFO sync mode parameters.
    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.addParameterListener(ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, i), this);
        processor.treeState.addParameterListener(ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i), this);
    }

    // Initialize the smooth slider and label.
    addAndMakeVisible(lfoSmoothSlider);
    lfoSmoothSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoSmoothSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);

    addAndMakeVisible(lfoSmoothLabel);
    lfoSmoothLabel.setText("Smooth", juce::dontSendNotification);
    lfoSmoothLabel.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain") });
    lfoSmoothLabel.attachToComponent(&lfoSmoothSlider, false);
    lfoSmoothLabel.setColour(juce::Label::textColourId, COLOUR1);
    lfoSmoothLabel.setJustificationType(juce::Justification::centred);

    // Initialize the phase slider and label.
    addAndMakeVisible(lfoPhaseSlider);
    lfoPhaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoPhaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    lfoPhaseSlider.addListener(this);

    addAndMakeVisible(lfoPhaseLabel);
    lfoPhaseLabel.setText("Phase", juce::dontSendNotification);
    lfoPhaseLabel.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain") });
    lfoPhaseLabel.attachToComponent(&lfoPhaseSlider, false);
    lfoPhaseLabel.setColour(juce::Label::textColourId, COLOUR1);
    lfoPhaseLabel.setJustificationType(juce::Justification::centred);

    // Attachments
    setLfo(currentLfoIndex); // Call helper to set up all attachments for the initial LFO.

    // Set up the rate slider based on the initial state
    triggerAsyncUpdate();

    startTimerHz(60);
}

LfoPanel::~LfoPanel()
{
    stopTimer();

    // Remove listeners from all buttons styled with styleButton()
    for (auto& button : lfoSelectButtons)
        button->removeListener(this);

    editModeButton.removeListener(this);
    brushModeButton.removeListener(this);
    assignButton.removeListener(this);
    matrixButton.removeListener(this);
    syncButton.removeListener(this);

    // Remove listeners from sliders
    gridXSlider.removeListener(this);
    gridYSlider.removeListener(this);
    lfoPhaseSlider.removeListener(this);

    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.removeParameterListener(ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, i), this);
        processor.treeState.removeParameterListener(ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i), this);
    }
}

void LfoPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff303030));
    g.drawRect(getLocalBounds(), 1);
}

void LfoPanel::resized()
{
    // First, get the scale factor from the parent editor
    float scale = 1.0f;
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* lnf = dynamic_cast<FireLookAndFeel*>(&editor->getLookAndFeel()))
            scale = lnf->scale;

    // --- Define layout constants ---
    constexpr int initialMargin = 10;
    // MODIFIED: Increased right column width to fit 3 sliders and shrink the LFO editor.
    constexpr int initialRightColWidth = 270;
    constexpr int initialLeftColWidth = 60; // For the LFO 1-4 buttons
    constexpr int initialTopRowHeight = 30; // For matrix, sync, etc. buttons

    // Create a working area
    juce::Rectangle<int> bounds = getLocalBounds();
    bounds.reduce(juce::roundToInt(initialMargin * scale), juce::roundToInt(initialMargin * scale));

    // --- Create the main Left and Right areas ---
    auto rightArea = bounds.removeFromRight(juce::roundToInt(initialRightColWidth * scale));
    bounds.removeFromRight(juce::roundToInt(initialMargin * scale)); // Spacer
    auto leftArea = bounds;

    // --- Layout Left Area (LFO Select, Top Buttons, Editor) ---
    {
        // Carve out a column for the LFO select buttons from the far left.
        auto lfoSelectColumn = leftArea.removeFromLeft(juce::roundToInt(initialLeftColWidth * scale));
        leftArea.removeFromLeft(juce::roundToInt(initialMargin * scale)); // Spacer

        // From the rest of leftArea, take the top row for mode buttons.
        auto topRow = leftArea.removeFromTop(juce::roundToInt(initialTopRowHeight * scale));
        leftArea.removeFromTop(juce::roundToInt(initialMargin * scale)); // Spacer

        // The main LFO editor takes the remaining space, which is now narrower.
        lfoEditor.setBounds(leftArea);

        // Use FlexBox to lay out the LFO select buttons vertically.
        juce::FlexBox lfoSelectBox;
        lfoSelectBox.flexDirection = juce::FlexBox::Direction::column;
        for (const auto& button : lfoSelectButtons)
            lfoSelectBox.items.add(juce::FlexItem(*button).withFlex(1.0f).withMargin(juce::FlexItem::Margin(juce::roundToInt(2 * scale))));
        lfoSelectBox.performLayout(lfoSelectColumn);

        // Use FlexBox to lay out the top row controls horizontally.
        juce::FlexBox topRowFlexBox;
        topRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
        topRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        topRowFlexBox.alignItems = juce::FlexBox::AlignItems::stretch;
        std::vector<juce::Component*> topRowControls = {
            &matrixButton, &syncButton, &assignButton, &editModeButton, &brushModeButton, &brushSelector
        };
        const float scaledPadding = 4 * scale;
        for (auto* control : topRowControls)
        {
            topRowFlexBox.items.add(juce::FlexItem(*control)
                                        .withFlex(1.0f)
                                        .withMargin({ scaledPadding, 2.0f * scale, scaledPadding, 2.0f * scale }));
        }
        topRowFlexBox.performLayout(topRow);
    }

    // --- MODIFIED: Layout Right Area (3 sliders side-by-side + Grid row at bottom) ---
    {
        // 1. Define and populate the Grid Area at the absolute bottom (unchanged).
        const int gridAreaHeight = juce::roundToInt(30 * scale);
        auto gridArea = rightArea.removeFromBottom(gridAreaHeight);

        juce::FlexBox gridBox;
        gridBox.flexDirection = juce::FlexBox::Direction::row;
        gridBox.alignItems = juce::FlexBox::AlignItems::stretch;
        gridBox.items.add(juce::FlexItem(gridXLabel).withFlex(0.3f).withMargin({ 0, 2, 0, 0 }));
        gridBox.items.add(juce::FlexItem(gridXSlider).withFlex(1.0f));
        gridBox.items.add(juce::FlexItem().withWidth(5 * scale)); // Spacer
        gridBox.items.add(juce::FlexItem(gridYLabel).withFlex(0.3f).withMargin({ 0, 2, 0, 0 }));
        gridBox.items.add(juce::FlexItem(gridYSlider).withFlex(1.0f));
        gridBox.performLayout(gridArea);

        // 2. Lay out the 3 sliders in the remaining space above the grid.
        rightArea.reduce(0, juce::roundToInt(initialMargin * scale)); // Vertical padding

        // Divide the remaining area into 3 vertical columns for the sliders.
        int sliderWidth = rightArea.getWidth() / 3;
        auto rateSliderArea = rightArea.removeFromLeft(sliderWidth);
        auto smoothSliderArea = rightArea.removeFromLeft(sliderWidth);
        auto phaseSliderArea = rightArea; // Takes the rest

        // Assign each slider to its column with some padding.
        int padding = 10;
        rateSlider.setBounds(rateSliderArea.reduced(juce::roundToInt(padding * scale)));
        lfoSmoothSlider.setBounds(smoothSliderArea.reduced(juce::roundToInt(padding * scale)));
        lfoPhaseSlider.setBounds(phaseSliderArea.reduced(juce::roundToInt(padding * scale)));
    }
}

void LfoPanel::timerCallback()
{
    if (processor.isDawPlaying())
    {
        // If the DAW is playing, get the current phase and show the playhead.
        const float currentPhase = processor.getLfoPhase(currentLfoIndex);
        lfoEditor.setPlayheadPosition(currentPhase);
    }
    else
    {
        // If the DAW is stopped, pass a special value (-1.0f) to hide the playhead.
        lfoEditor.setPlayheadPosition(-1.0f);
    }
}

void LfoPanel::buttonClicked(juce::Button* button)
{
    // Mode Switching
    if (button == &assignButton)
    {
        if (onAssignButtonClicked)
            onAssignButtonClicked(currentLfoIndex);
        return;
    }
    if (button == &editModeButton)
    {
        setEditMode(LfoEditMode::PointEdit);
        return;
    }
    if (button == &brushModeButton)
    {
        setEditMode(LfoEditMode::BrushPaint);
        return;
    }

    if (button == &matrixButton)
    {
        // This opens our new panel in a non-modal dialog window.
        juce::DialogWindow::LaunchOptions launchOptions;
        launchOptions.content.setOwned(new ModulationMatrixPanel(processor));
        launchOptions.content->setSize(800, 400);
        launchOptions.launchAsync();
    }
    else if (button == &syncButton)
    {
        // This button's state is managed by the ButtonAttachment, so we don't need to do anything here.
        // The parameterChanged callback will handle the UI update.
    }
    else
    {
        int clickedIndex = -1;
        for (int i = 0; i < lfoSelectButtons.size(); ++i)
        {
            if (button == lfoSelectButtons[i].get())
            {
                clickedIndex = i;
                break;
            }
        }

        if (clickedIndex != -1)
        {
            setLfo(clickedIndex); // Call the new helper function
        }
    }
}

void LfoPanel::setLfo(int newIndex)
{
    // Update the current LFO index and tell the editor to display the new data.
    currentLfoIndex = newIndex;
    lfoEditor.setDataToDisplay(processor.getLfoManager().getLfoData()[currentLfoIndex]);

    // Explicitly set the toggle state for all buttons in the group.
    for (int i = 0; i < lfoSelectButtons.size(); ++i)
    {
        lfoSelectButtons[i]->setToggleState(i == currentLfoIndex, juce::dontSendNotification);
    }

    // Reset and re-create all attachments to point to the new LFO's parameters.
    syncButtonAttachment.reset();
    rateSliderAttachment.reset();
    lfoSmoothAttachment.reset();
    lfoPhaseAttachment.reset();

    syncButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.treeState, ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, currentLfoIndex), syncButton);

    lfoSmoothAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.treeState, ParameterIDAndName::getIDString(LFO_SMOOTH_ID, currentLfoIndex), lfoSmoothSlider);

    lfoPhaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.treeState, ParameterIDAndName::getIDString(LFO_PHASE_ID, currentLfoIndex), lfoPhaseSlider);
    // This must be called after attachments are updated.
    updateRateSlider();
}

void LfoPanel::setOnDataChangedCallback(std::function<void()> callback)
{
    // Here we connect the LfoPanel's callback to the LfoEditor's callback.
    // This completes the chain from the innermost component to the outermost.
    onDataChanged = callback;
}

void LfoPanel::updateRateSlider()
{
    // --- Get Parameter IDs using the robust ParameterID namespace ---
    // The currentLfoIndex is 0-based, which matches our arrays perfectly.
    auto syncModeID = ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, currentLfoIndex);
    auto rateSyncID = ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, currentLfoIndex);
    auto rateHzID = ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, currentLfoIndex);

    // Find out if the current LFO is in sync mode from the parameter value.
    // We use .getParamID() to get the string from the juce::ParameterID object.
    auto* param = processor.treeState.getParameter(syncModeID);
    jassert(param != nullptr);
    bool isInSyncMode = param->getValue() > 0.5f;

    // First, always destroy the old attachment before creating a new one.
    rateSliderAttachment.reset();

    if (isInSyncMode)
    {
        // --- BPM SYNC MODE (Discrete Choices) ---

        // We only provide the text conversion lambda.
        rateSlider.textFromValueFunction = [this](double value)
        {
            const int index = juce::roundToInt(value);
            // Use the new getter function
            const auto& divisions = processor.getLfoRateSyncDivisions();
            if (juce::isPositiveAndBelow(index, divisions.size()))
                return divisions[index];
            return juce::String();
        };

        // Disable text entry for stability.
        rateSlider.valueFromTextFunction = nullptr;

        // Create the attachment using the correct ParameterID.
        rateSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.treeState, rateSyncID, rateSlider);
    }
    else // HZ (FREE) MODE
    {
        // Revert to default behavior.
        rateSlider.textFromValueFunction = nullptr;
        rateSlider.valueFromTextFunction = nullptr;

        // Create the attachment using the correct ParameterID.
        rateSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.treeState, rateHzID, rateSlider);
    }
}

void LfoPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    auto currentSyncModeID = ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, currentLfoIndex);
    if (parameterID == currentSyncModeID)
    {
        triggerAsyncUpdate();
        return;
    }

    for (int i = 0; i < 4; ++i)
    {
        auto smoothParamID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i);
        if (parameterID == smoothParamID)
        {
            // FIX: Correct, thread-safe way to update a member of LfoData
            // 1. Get a copy of the current data
            auto lfoDataCopy = processor.getLfoManager().getLfoData()[i];
            // 2. Modify the copy
            lfoDataCopy.smoothness = newValue;
            // 3. Set the data back using the thread-safe method
            processor.getLfoManager().setLfoData(i, lfoDataCopy);

            // The LfoManager will handle flagging the shape for update.
            // We still call onDataChanged to mark the preset as dirty.
            if (onDataChanged)
                onDataChanged();

            return;
        }
    }
}

void LfoPanel::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &gridXSlider || slider == &gridYSlider)
    {
        lfoEditor.setGridDivisions((int) gridXSlider.getValue(), (int) gridYSlider.getValue());
    }
    else if (slider == &lfoPhaseSlider)
    {
        if (isDraggingPhaseSlider)
        {
            lfoEditor.setPhaseOffsetLinePosition(lfoPhaseSlider.getValue());
        }
    }
}

void LfoPanel::sliderDragStarted(juce::Slider* slider)
{
    if (slider == &lfoPhaseSlider)
    {
        isDraggingPhaseSlider = true;
        lfoEditor.setPhaseOffsetLinePosition(lfoPhaseSlider.getValue());
    }
}

void LfoPanel::sliderDragEnded(juce::Slider* slider)
{
    if (slider == &lfoPhaseSlider)
    {
        isDraggingPhaseSlider = false;
        lfoEditor.setPhaseOffsetLinePosition(-1.0f);
    }
}

void LfoPanel::setScale(float newScale)
{
    scale = newScale;
    resized(); // Call resized to apply the new scale
}

void LfoPanel::setEditMode(LfoEditMode newMode)
{
    // 1. Update the internal state of the LfoPanel
    // currentMode = newMode; // Assuming you add a currentMode member to LfoPanel

    if (newMode == LfoEditMode::PointEdit)
    {
        // 2. Set the toggle state of the mode buttons
        editModeButton.setToggleState(true, juce::dontSendNotification);
        brushModeButton.setToggleState(false, juce::dontSendNotification);

        // 3. Hide the brush selector UI
        brushSelector.setVisible(false);
    }
    else // newMode == LfoEditMode::BrushPaint
    {
        // 2. Set the toggle state of the mode buttons
        editModeButton.setToggleState(false, juce::dontSendNotification);
        brushModeButton.setToggleState(true, juce::dontSendNotification);

        // 3. Show the brush selector UI
        brushSelector.setVisible(true);

        // 4. Ensure a valid brush is selected when entering brush mode
        if (brushSelector.getSelectedId() == 0) // Check if nothing is selected
            brushSelector.setSelectedId(1, juce::dontSendNotification);
    }

    // 5. IMPORTANT: Tell the LfoEditor view to change its behavior
    lfoEditor.setEditMode(newMode);
}

void LfoPanel::styleButton(juce::Button& button, bool isToggle)
{
    // Common style for all buttons
    button.addListener(this);
    button.setColour(juce::TextButton::buttonColourId, COLOUR7);
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, COLOUR1);
    button.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));

    // Specific style for toggle buttons
    if (isToggle)
    {
        button.setClickingTogglesState(true);
    }
}

void LfoPanel::refreshLfoDisplay()
{
    lfoEditor.setDataToDisplay(processor.getLfoManager().getLfoData()[currentLfoIndex]);
}

void LfoEditor::selectAllPoints()
{
    if (! dataIsActive)
        return;

    selectedPointIndices.clear();
    for (int i = 0; i < activeLfoData.points.size(); ++i)
    {
        selectedPointIndices.push_back(i);
    }
    repaint();
}

void LfoEditor::clearAllPoints()
{
    if (! dataIsActive)
        return;

    activeLfoData.resetToDefault();
    selectedPointIndices.clear();
    repaint();
}

void LfoEditor::copyShape()
{
    if (! dataIsActive)
        return;
    lfoClipboard = activeLfoData;
}

void LfoEditor::pasteShape()
{
    if (! dataIsActive || lfoClipboard.points.empty())
        return;
    activeLfoData = lfoClipboard;
    selectedPointIndices.clear();
    repaint();
}

void LfoEditor::invertShape(bool invertX, bool invertY)
{
    if (! dataIsActive || activeLfoData.points.size() < 2)
        return;

    for (auto& point : activeLfoData.points)
    {
        if (invertX)
        {
            // We don't invert the first and last points on the x-axis
            // because they are fixed at 0.0 and 1.0.
            if (! juce::approximatelyEqual(point.x, 0.0f) && ! juce::approximatelyEqual(point.x, 1.0f))
            {
                point.x = 1.0f - point.x;
            }
        }

        if (invertY)
        {
            point.y = 1.0f - point.y;
        }
    }

    // Inverting X will mess up the order, so we need to sort again.
    if (invertX)
    {
        updateAndSortPoints();
    }

    repaint();
}

void LfoPanel::handleAsyncUpdate()
{
    // This function is guaranteed to be called on the main UI thread.
    // It is now safe to update the slider and its attachment here.
    updateRateSlider();
}
