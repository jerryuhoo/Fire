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

void LfoEditor::setDataToDisplay(LfoData* dataToDisplay)
{
    // Safely switch the data source.
    activeLfoData = dataToDisplay;
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

    if (activeLfoData == nullptr || activeLfoData->points.size() < 2)
        return;

    // Draw LFO Path
    juce::Path lfoPath;
    lfoPath.startNewSubPath(fromNormalized(activeLfoData->points.front()));

    for (size_t i = 0; i < activeLfoData->points.size() - 1; ++i)
    {
        auto p1_screen = fromNormalized(activeLfoData->points[i]);
        auto p2_screen = fromNormalized(activeLfoData->points[i + 1]);

        if (std::abs(p1_screen.x - p2_screen.x) < 0.1f)
        {
            lfoPath.lineTo(p2_screen);
            continue;
        }

        bool isCurved = (i < activeLfoData->curvatures.size() && ! juce::approximatelyEqual(activeLfoData->curvatures[i], 0.0f));
        if (! isCurved)
        {
            lfoPath.lineTo(p2_screen);
        }
        else
        {
            const int numSegments = 30;
            const float curvature = activeLfoData->curvatures[i];

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
    for (int i = 0; i < activeLfoData->points.size(); ++i)
    {
        bool isSelected = std::find(selectedPointIndices.begin(), selectedPointIndices.end(), i) != selectedPointIndices.end();
        bool isHovered = (i == hoveredPointIndex);

        auto localPoint = fromNormalized(activeLfoData->points[i]);

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
}

void LfoEditor::resized() {} // No layout logic needed in the editor itself.

void LfoEditor::setOnDataChangedCallback(std::function<void()> callback)
{
    onDataChanged = callback;
}

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

void LfoEditor::mouseDown(const juce::MouseEvent& event)
{
    if (! activeLfoData)
        return;
    grabKeyboardFocus();

    if (currentMode != LfoEditMode::PointEdit)
        return;

    // --- Right-click is always for curvature ---
    if (event.mods.isRightButtonDown())
    {
        editingCurveIndex = findSegmentIndexAt(event.getPosition());
        if (editingCurveIndex != -1)
        {
            initialCurvature = activeLfoData->curvatures[editingCurveIndex];
            initialDragY = event.y;
        }
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
    for (size_t i = 0; i < activeLfoData->points.size(); ++i)
    {
        if (fromNormalized(activeLfoData->points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius * 1.5f)
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
        draggingState = DraggingState::Point;
        selectedPointIndices.clear();
        if (activeLfoData->points.size() < maxPoints)
        {
            addPoint(toNormalized(event.getPosition()));
            // Find the newly added point to start dragging it
            for (size_t i = 0; i < activeLfoData->points.size(); ++i)
            {
                if (juce::approximatelyEqual(activeLfoData->points[i].x, toNormalized(event.getPosition()).x))
                {
                    selectedPointIndices.push_back((int) i);
                    break;
                }
            }
        }
    }

    // If we are starting any kind of drag, store the initial positions of all selected points.
    if (draggingState == DraggingState::Point || draggingState == DraggingState::Selection)
    {
        dragAnchor = toNormalized(event.getPosition());
        initialDragPositions.clear();
        for (int index : selectedPointIndices)
            initialDragPositions.push_back(activeLfoData->points[index]);
    }

    repaint();
}

void LfoEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (! activeLfoData || currentMode != LfoEditMode::PointEdit)
        return;

    if (editingCurveIndex != -1 && event.mods.isRightButtonDown())
    {
        float deltaY = (float) (initialDragY - event.y);
        activeLfoData->curvatures[editingCurveIndex] = juce::jlimit(-1.0f, 1.0f, initialCurvature + deltaY * 0.005f);
        repaint();
        return;
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
                    maxLeftDelta = activeLfoData->points[leftNeighborIndex].x - initialLeftmostX;
                }
            }

            if (rightmostSelectedPointIndex < activeLfoData->points.size() - 1)
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
                    maxRightDelta = activeLfoData->points[rightNeighborIndex].x - initialRightmostX;
                }
            }

            delta.x = juce::jlimit(maxLeftDelta, maxRightDelta, delta.x);

            // 4. Apply the final, constrained (and possibly snapped) delta to all selected points.
            for (size_t i = 0; i < selectedPointIndices.size(); ++i)
            {
                int pointIndex = selectedPointIndices[i];
                auto& initialPos = initialDragPositions[i];
                auto& point = activeLfoData->points[pointIndex];
                point.y = juce::jlimit(0.0f, 1.0f, initialPos.y + delta.y);
                if (pointIndex > 0 && pointIndex < activeLfoData->points.size() - 1)
                {
                    point.x = initialPos.x + delta.x;
                }
            }
            repaint();
            break;
        }
        case DraggingState::None:
        default:
            break;
    }
}

void LfoEditor::mouseUp(const juce::MouseEvent& event)
{
    if (! activeLfoData)
        return;
    bool dataWasChanged = false;

    if (draggingState == DraggingState::Marquee)
    {
        auto finalRect = makeNormalised(event.getMouseDownPosition(), event.getPosition()).toFloat();
        selectedPointIndices.clear();
        for (int i = 0; i < activeLfoData->points.size(); ++i)
        {
            if (finalRect.contains(fromNormalized(activeLfoData->points[i])))
                selectedPointIndices.push_back(i);
        }
        selectionRectangle.setSize(0, 0);
    }

    if (draggingState == DraggingState::Point)
    {
        updateAndSortPoints();
        dataWasChanged = true;
    }

    draggingState = DraggingState::None;
    editingCurveIndex = -1;

    if (currentMode == LfoEditMode::BrushPaint && event.mouseWasClicked())
    {
        applyBrushShape(event.getPosition());
        dataWasChanged = true;
    }

    repaint();

    if (dataWasChanged && onDataChanged)
    {
        onDataChanged();
    }
}

void LfoEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (! activeLfoData || currentMode != LfoEditMode::PointEdit)
        return;

    if (event.mods.isRightButtonDown())
    {
        int segIdx = findSegmentIndexAt(event.getPosition());
        if (segIdx != -1)
        {
            activeLfoData->curvatures[segIdx] = 0.0f;
            if (onDataChanged)
                onDataChanged();
        }
    }
    else if (activeLfoData->points.size() > 2)
    {
        for (size_t i = 1; i < activeLfoData->points.size() - 1; ++i)
        {
            if (fromNormalized(activeLfoData->points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
            {
                removePoint((int) i);
                if (onDataChanged)
                    onDataChanged();
                return;
            }
        }
    }
    repaint();
}

void LfoEditor::mouseMove(const juce::MouseEvent& event)
{
    // Find if the mouse is currently hovering over any point
    int newHoveredIndex = -1;
    for (int i = 0; i < activeLfoData->points.size(); ++i)
    {
        auto pointScreen = fromNormalized(activeLfoData->points[i]);

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
    if (! activeLfoData || activeLfoData->points.size() >= maxPoints)
        return;

    // Add the point and sort the list to find its correct position.
    activeLfoData->points.push_back(newPoint);
    updateAndSortPoints();

    // Find the index of the point we just added.
    auto it = std::find_if(activeLfoData->points.begin(), activeLfoData->points.end(), [&](const auto& p)
                           { return juce::approximatelyEqual(p.x, newPoint.x) && juce::approximatelyEqual(p.y, newPoint.y); });

    if (it != activeLfoData->points.end())
    {
        int insertedAtIndex = (int) std::distance(activeLfoData->points.begin(), it);

        // A new point splits a segment, so we need one more curvature value.
        // We insert a new 0.0f (linear) curvature for the new segment being created.
        // All other curvatures are preserved.
        if (insertedAtIndex > 0)
        {
            activeLfoData->curvatures.insert(activeLfoData->curvatures.begin() + insertedAtIndex - 1, 0.0f);
        }
        else
        {
            // This should only happen if adding a point before the second point, very rare.
            activeLfoData->curvatures.insert(activeLfoData->curvatures.begin(), 0.0f);
        }
    }

    repaint();
}

void LfoEditor::removePoint(int index)
{
    if (! activeLfoData || index <= 0 || index >= activeLfoData->points.size() - 1)
        return;

    activeLfoData->points.erase(activeLfoData->points.begin() + index);

    // Removing a point merges two segments. We must remove one curvature value.
    // We remove the curvature of the first of the two merged segments.
    activeLfoData->curvatures.erase(activeLfoData->curvatures.begin() + index - 1);

    // We then set the curvature of the new, merged segment to 0.0 (linear).
    if (index - 1 < activeLfoData->curvatures.size())
    {
        activeLfoData->curvatures[index - 1] = 0.0f;
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
    if (! activeLfoData)
        return;
    // Use stable_sort to preserve the order of points with the same x-coordinate.
    std::stable_sort(activeLfoData->points.begin(), activeLfoData->points.end(), [](const auto& a, const auto& b)
                     { return a.x < b.x; });

    if (activeLfoData->points.empty())
        activeLfoData->resetToDefault();
    else
    {
        activeLfoData->points.front().x = 0.0f;
        activeLfoData->points.back().x = 1.0f;
    }
}

int LfoEditor::getOrCreatePointAtX(float targetX)
{
    if (activeLfoData == nullptr)
        return -1;

    // 1. Check if a point already exists at (or very close to) the target X.
    for (size_t i = 0; i < activeLfoData->points.size(); ++i)
    {
        if (juce::approximatelyEqual(activeLfoData->points[i].x, targetX))
            return static_cast<int>(i);
    }

    // 2. If not, find which segment the targetX falls into.
    for (size_t i = 0; i < activeLfoData->points.size() - 1; ++i)
    {
        auto& p1 = activeLfoData->points[i];
        auto& p2 = activeLfoData->points[i + 1];

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
            for (size_t j = 0; j < activeLfoData->points.size(); ++j)
            {
                if (juce::approximatelyEqual(activeLfoData->points[j].x, targetX))
                    return static_cast<int>(j);
            }
        }
    }
    return -1; // Should not happen on a valid curve.
}

void LfoEditor::rebuildCurvatures()
{
    if (! activeLfoData)
        return;
    size_t numSegments = activeLfoData->points.size() > 1 ? activeLfoData->points.size() - 1 : 0;
    activeLfoData->curvatures.assign(numSegments, 0.0f);
}

void LfoEditor::applyBrushShape(const juce::Point<int>& clickPosition)
{
    if (! activeLfoData)
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

    // 2. Generate the new shape. Assuming LfoShapeGenerator provides points exactly on boundaries.
    auto newPoints = LfoShapeGenerator::generateShape(currentBrush, cellBounds);
    if (newPoints.empty())
        return;

    // 3. CLEANUP: Remove all points that are ON or WITHIN the target cell's X-range.
    activeLfoData->points.erase(
        std::remove_if(activeLfoData->points.begin(), activeLfoData->points.end(), [&](const auto& p)
                       {
                           // This predicate is now simplified: remove any point within the horizontal span.
                           return p.x >= startX && p.x <= endX; }),
        activeLfoData->points.end());

    // 4. ADD the new points to the list.
    activeLfoData->points.insert(activeLfoData->points.end(), newPoints.begin(), newPoints.end());

    // 5. Finalize by sorting all points and completely rebuilding the curvatures list.
    // This is the most robust way to handle the data after such a destructive edit.
    updateAndSortPoints();
    rebuildCurvatures();

    repaint();
}

int LfoEditor::findSegmentIndexAt(const juce::Point<int>& position) const
{
    // Safety checks: ensure there is data and at least one segment to check.
    if (activeLfoData == nullptr || activeLfoData->points.size() < 2)
        return -1;

    // Convert the mouse's X position to a normalized value [0, 1]
    const float clickXNormalized = (float) position.x / (float) getWidth();

    // Iterate through all the points that define the start of a segment
    for (size_t i = 0; i < activeLfoData->points.size() - 1; ++i)
    {
        // A segment is defined by point 'i' and point 'i + 1'.
        const auto& p1 = activeLfoData->points[i];
        const auto& p2 = activeLfoData->points[i + 1];

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
    if (! selectedPointIndices.empty() && (key.isKeyCurrentlyDown(juce::KeyPress::deleteKey) || key.isKeyCurrentlyDown(juce::KeyPress::backspaceKey)))
    {
        deleteSelectedPoints();
        if (onDataChanged)
            onDataChanged();
        return true;
    }
    return false;
}

void LfoEditor::deleteSelectedPoints()
{
    if (! activeLfoData || selectedPointIndices.empty())
        return;
    std::sort(selectedPointIndices.rbegin(), selectedPointIndices.rend());
    for (int index : selectedPointIndices)
    {
        if (index > 0 && index < activeLfoData->points.size() - 1)
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
    lfoEditor.setDataToDisplay(&processor.getLfoManager().getLfoData()[currentLfoIndex]);
    addAndMakeVisible(lfoEditor);

    // Create UI Components
    for (int i = 0; i < 4; ++i)
    {
        lfoSelectButtons[i] = std::make_unique<juce::TextButton>("LFO " + juce::String(i + 1));
        addAndMakeVisible(lfoSelectButtons[i].get());
        lfoSelectButtons[i]->setRadioGroupId(1);
        lfoSelectButtons[i]->addListener(this);
        lfoSelectButtons[i]->setColour(juce::TextButton::buttonColourId, COLOUR7);
        lfoSelectButtons[i]->setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
        lfoSelectButtons[i]->setColour(juce::ComboBox::outlineColourId, COLOUR6);
        lfoSelectButtons[i]->setColour(juce::TextButton::textColourOnId, COLOUR1);
        lfoSelectButtons[i]->setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
        lfoSelectButtons[i]->setToggleState(false, juce::dontSendNotification);
    }
    lfoSelectButtons[0]->setToggleState(true, juce::dontSendNotification);

    // --- Setup Mode Buttons ---
    addAndMakeVisible(editModeButton);
    addAndMakeVisible(brushModeButton);
    editModeButton.setRadioGroupId(1002);
    brushModeButton.setRadioGroupId(1002);
    editModeButton.setClickingTogglesState(true);
    brushModeButton.setClickingTogglesState(true);
    editModeButton.addListener(this);
    brushModeButton.addListener(this);

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

    addAndMakeVisible(matrixButton);
    matrixButton.addListener(this);
    matrixButton.setColour(juce::TextButton::buttonColourId, COLOUR7);
    matrixButton.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    matrixButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    matrixButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    matrixButton.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));

    addAndMakeVisible(syncButton);
    syncButton.setClickingTogglesState(true);
    syncButton.setColour(juce::TextButton::buttonColourId, COLOUR7);
    syncButton.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    syncButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    syncButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    syncButton.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
    syncButton.setButtonText("BPM");

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
    rateLabel.setColour(juce::Label::textColourId, SHAPE_COLOUR);
    rateLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(gridXSlider);
    gridXSlider.setSliderStyle(juce::Slider::IncDecButtons);
    gridXSlider.setRange(2, 16, 1);
    gridXSlider.setValue(4);
    gridXSlider.addListener(this);
    gridXSlider.setColour(juce::Slider::backgroundColourId, COLOUR6);
    gridXSlider.setColour(juce::Slider::thumbColourId, COLOUR1); // For the arrow
    gridXSlider.setColour(juce::Slider::textBoxOutlineColourId, COLOUR6.withAlpha(0.5f)); // Border color
    gridXSlider.setColour(juce::Slider::textBoxHighlightColourId, COLOUR1);
    gridXSlider.setColour(juce::TextButton::textColourOnId, COLOUR1);

    addAndMakeVisible(gridXLabel);
    gridXLabel.setText("Grid X", juce::dontSendNotification);

    addAndMakeVisible(gridYSlider);
    gridYSlider.setSliderStyle(juce::Slider::IncDecButtons);
    gridYSlider.setRange(2, 16, 1);
    gridYSlider.setValue(4);
    gridYSlider.addListener(this);
    gridYSlider.setColour(juce::Slider::backgroundColourId, COLOUR6);
    gridYSlider.setColour(juce::Slider::thumbColourId, COLOUR1); // For the arrow
    // Add these two lines:
    gridYSlider.setColour(juce::Slider::textBoxOutlineColourId, COLOUR6.withAlpha(0.5f)); // Border color
    gridYSlider.setColour(juce::Slider::textBoxHighlightColourId, COLOUR1);
    gridYSlider.setColour(juce::TextButton::textColourOnId, COLOUR1);

    addAndMakeVisible(gridYLabel);
    gridYLabel.setText("Grid Y", juce::dontSendNotification);

    // ADD THIS: Register as a parameter listener for each of the LFO sync mode parameters.
    for (int i = 0; i < 4; ++i)
    {
        // Use the ParameterID helper function to get the correct ID.
        // The loop now correctly runs from 0 to 3 to match the array indices.
        processor.treeState.addParameterListener(ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, i), this);
    }

    // --- ADD Attachments ---
    // Attach the sync button to the first LFO's sync mode parameter
    syncButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.treeState, ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0), syncButton);

    // Set up the rate slider based on the initial state
    updateRateSlider();

    startTimerHz(60);
}

LfoPanel::~LfoPanel()
{
    stopTimer();

    for (int i = 0; i < 4; ++i)
    {
        // Use the same helper function to ensure we remove the correct listener.
        processor.treeState.removeParameterListener(ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, i), this);
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
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* lnf = dynamic_cast<FireLookAndFeel*>(&editor->getLookAndFeel()))
            scale = lnf->scale;

    // Define layout constants based on an initial design size.
    constexpr int initialMargin = 10;
    constexpr int initialLeftColWidth = 60;
    constexpr int initialRightColWidth = 120;
    constexpr int initialTopRowHeight = 30;
    constexpr int initialMatrixWidth = 150;
    constexpr int initialSyncWidth = 100;
    constexpr int initialGridLabelWidth = 50;

    // Create a working area, scaled from the initial margin.
    juce::Rectangle<int> bounds = getLocalBounds();
    bounds.reduce(juce::roundToInt(initialMargin * scale), juce::roundToInt(initialMargin * scale));

    // Calculate scaled dimensions for the main columns and rows.
    auto leftColumn = bounds.removeFromLeft(juce::roundToInt(initialLeftColWidth * scale));
    auto rightColumn = bounds.removeFromRight(juce::roundToInt(initialRightColWidth * scale));

    // Add some spacing between columns and the central editor
    bounds.removeFromLeft(juce::roundToInt(initialMargin * scale));
    bounds.removeFromRight(juce::roundToInt(initialMargin * scale));

    auto topRow = bounds.removeFromTop(juce::roundToInt(initialTopRowHeight * scale));
    bounds.removeFromTop(juce::roundToInt(initialMargin * scale)); // Spacing below top row

    // Layout the top row with scaled dimensions.
    {
        auto matrixButtonArea = topRow.removeFromLeft(juce::roundToInt(initialMatrixWidth * scale));
        matrixButton.setBounds(matrixButtonArea.reduced(0, juce::roundToInt(4 * scale)));

        auto syncButtonArea = topRow.removeFromRight(juce::roundToInt(initialSyncWidth * scale));
        syncButton.setBounds(syncButtonArea.reduced(juce::roundToInt(10 * scale), juce::roundToInt(4 * scale)));

        auto gridControlsArea = topRow;
        auto gridXArea = gridControlsArea.removeFromLeft(gridControlsArea.getWidth() / 2);
        auto gridYArea = gridControlsArea;

        // Add some spacing between the grid controls
        gridXArea.removeFromRight(juce::roundToInt(5 * scale));
        gridYArea.removeFromLeft(juce::roundToInt(5 * scale));

        gridXLabel.setBounds(gridXArea.removeFromLeft(juce::roundToInt(initialGridLabelWidth * scale)).reduced(juce::roundToInt(2 * scale)));
        gridXSlider.setBounds(gridXArea);

        gridYLabel.setBounds(gridYArea.removeFromLeft(juce::roundToInt(initialGridLabelWidth * scale)).reduced(juce::roundToInt(2 * scale)));
        gridYSlider.setBounds(gridYArea);
    }

    auto topStrip = bounds.removeFromTop(30);
    auto controlsStrip = topStrip.removeFromLeft(topStrip.getWidth() / 2);
    auto lfoSelectStrip = topStrip;

    // Layout for the main columns using FlexBox for the buttons.
    juce::FlexBox lfoSelectBox;
    lfoSelectBox.flexDirection = juce::FlexBox::Direction::column;
    for (const auto& button : lfoSelectButtons)
        lfoSelectBox.items.add(juce::FlexItem(*button).withFlex(1.0f).withMargin(juce::FlexItem::Margin(2 * scale)));
    lfoSelectBox.performLayout(leftColumn);

    editModeButton.setBounds(controlsStrip.removeFromLeft(80));
    brushModeButton.setBounds(controlsStrip.removeFromLeft(80));
    controlsStrip.removeFromLeft(10);
    brushSelector.setBounds(controlsStrip.removeFromLeft(100));

    // Center the rate slider in the right column with a scaled size.
    const int sliderSize = juce::roundToInt(juce::jmin(rightColumn.getWidth(), rightColumn.getHeight()) * 0.9f); // 90%
    rateSlider.setBounds(rightColumn.withSizeKeepingCentre(sliderSize, sliderSize));

    // The LFO editor takes the remaining central space.
    lfoEditor.setBounds(bounds);
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
        launchOptions.content->setSize(500, 300);
        launchOptions.launchAsync();
    }
    else if (button == &syncButton)
    {
        // Handle BPM sync logic here
    }
    else
    {
        // --- NEW LOGIC ---
        // Variable to hold the index of the button that was clicked.
        int clickedIndex = -1;

        // First, find which of our LFO buttons was the one that was clicked.
        for (int i = 0; i < lfoSelectButtons.size(); ++i)
        {
            if (button == lfoSelectButtons[i].get())
            {
                clickedIndex = i;
                break;
            }
        }

        // If one of the LFO select buttons was clicked...
        if (clickedIndex != -1)
        {
            // ...update the current LFO index and tell the editor to display the new data.
            currentLfoIndex = clickedIndex;
            lfoEditor.setDataToDisplay(&processor.getLfoManager().getLfoData()[currentLfoIndex]);

            // Explicitly set the toggle state for all buttons in the group.
            // This is the most robust way to manage radio button states.
            for (int i = 0; i < lfoSelectButtons.size(); ++i)
            {
                // If the button's index matches the one we just clicked, set its state to true.
                // Otherwise, set it to false.
                // The 'dontSendNotification' flag is crucial to prevent this action from triggering
                // another call to buttonClicked, which would cause an infinite loop.
                lfoSelectButtons[i]->setToggleState(i == clickedIndex, juce::dontSendNotification);
            }

            // Re-attach the sync button to the newly selected LFO's sync parameter
            auto syncModeID = ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, clickedIndex);
            syncButtonAttachment.reset();
            syncButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.treeState, syncModeID, syncButton);

            updateRateSlider();
        }
    }
}

void LfoPanel::setOnDataChangedCallback(std::function<void()> callback)
{
    // Here we connect the LfoPanel's callback to the LfoEditor's callback.
    // This completes the chain from the innermost component to the outermost.
    lfoEditor.setOnDataChangedCallback(callback);
}

void LfoPanel::updateRateSlider() // Or void LfoPanel::updateRateControls()
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
    bool isInSyncMode = param->getValue();

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
    else // --- HZ (FREE) MODE (Continuous) ---
    {
        // Revert to default behavior.
        rateSlider.textFromValueFunction = nullptr;
        rateSlider.valueFromTextFunction = nullptr;

        // Create the attachment using the correct ParameterID.
        rateSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.treeState, rateHzID, rateSlider);
    }
}

// ADD THIS NEW FUNCTION
void LfoPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Get the authoritative ParameterID for the currently active LFO's sync mode.
    auto currentSyncModeID = ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, currentLfoIndex);

    // Compare the incoming parameterID string with our authoritative one.
    if (parameterID == currentSyncModeID)
    {
        // If they match, it means the sync mode for the visible LFO has changed
        // (likely via automation or a preset load), so we must update the UI.
        updateRateSlider();
    }
}

void LfoPanel::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &gridXSlider || slider == &gridYSlider)
        lfoEditor.setGridDivisions((int) gridXSlider.getValue(), (int) gridYSlider.getValue());
}

void LfoPanel::setScale(float newScale)
{
    scale = newScale;
    //    flatLnf.scale = newScale; // Pass the scale to the LookAndFeel if it needs it.
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
