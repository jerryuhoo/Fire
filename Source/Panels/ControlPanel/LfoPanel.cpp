#include "LfoPanel.h"
#include "../../DSP/LfoShapeGenerator.h"
#include "../../GUI/FireTheme.h"
#include "../../PluginProcessor.h"

static juce::Rectangle<int> makeNormalised(const juce::Point<int>& p1,
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
    setOpaque(true);
}

LfoEditor::~LfoEditor() {}

bool LfoEditor::isValidPointIndex(int index) const noexcept
{
    return index >= 0
        && static_cast<size_t>(index) < activeLfoData.points.size();
}

bool LfoEditor::isValidCurveIndex(int index) const noexcept
{
    return index >= 0
        && static_cast<size_t>(index) < activeLfoData.curvatures.size()
        && static_cast<size_t>(index) + 1 < activeLfoData.points.size();
}

bool LfoEditor::hasValidSelectedPointIndices() const noexcept
{
    return std::all_of(selectedPointIndices.begin(),
                       selectedPointIndices.end(),
                       [this](int index) { return isValidPointIndex(index); });
}

bool LfoEditor::hasValidPointDragState() const noexcept
{
    const bool isPointDrag = draggingState == DraggingState::Point
                          || draggingState == DraggingState::Selection;
    return isPointDrag
        && ! selectedPointIndices.empty()
        && selectedPointIndices.size() == initialDragPositions.size()
        && hasValidSelectedPointIndices();
}

bool LfoEditor::validateCurveInteractionOrCancel() noexcept
{
    if (isValidCurveIndex(editingCurveIndex))
        return true;

    cancelPointAndCurveInteraction();
    return false;
}

bool LfoEditor::validatePointDragInteractionOrCancel() noexcept
{
    if (hasValidPointDragState())
        return true;

    cancelPointAndCurveInteraction();
    return false;
}

void LfoEditor::cancelPointAndCurveInteraction() noexcept
{
    selectedPointIndices.clear();
    initialDragPositions.clear();
    draggingState = DraggingState::None;
    draggingPointIndex = -1;
    editingCurveIndex = -1;
    hoveredPointIndex = -1;
    initialCurvature = 0.0f;
    initialDragY = 0;
    dragAnchor = {};
    selectionRectangle = {};
}

void LfoEditor::cancelAllInteraction() noexcept
{
    cancelPointAndCurveInteraction();
    isBrushing = false;
    lastBrushCell = { -1, -1 };
}

void LfoEditor::setDataToDisplay(const LfoData& dataToDisplay)
{
    cancelAllInteraction();

    // Safely switch the data source by copying and normalising malformed
    // preset data before any paint or interaction code indexes it.
    activeLfoData = dataToDisplay;
    if (activeLfoData.points.size() < 2)
        activeLfoData.resetToDefault();

    for (auto& point : activeLfoData.points)
    {
        point.x = std::isfinite(point.x) ? juce::jlimit(0.0f, 1.0f, point.x) : 0.0f;
        point.y = std::isfinite(point.y) ? juce::jlimit(0.0f, 1.0f, point.y) : 0.0f;
    }
    std::stable_sort(activeLfoData.points.begin(), activeLfoData.points.end(), [](const auto& a, const auto& b)
                     { return a.x < b.x; });

    if (activeLfoData.points.size() > static_cast<size_t>(maxPoints))
    {
        // The editor intentionally supports a bounded number of handles. Keep
        // both endpoints and uniformly sample an oversized/malformed shape so
        // paint and hit-testing cannot be forced into unbounded work.
        const auto originalPoints = activeLfoData.points;
        activeLfoData.points.clear();
        activeLfoData.points.reserve(static_cast<size_t>(maxPoints));
        for (int i = 0; i < maxPoints; ++i)
        {
            const auto sourceIndex = static_cast<size_t>(i) * (originalPoints.size() - 1)
                                     / static_cast<size_t>(maxPoints - 1);
            activeLfoData.points.push_back(originalPoints[sourceIndex]);
        }
        activeLfoData.curvatures.assign(static_cast<size_t>(maxPoints - 1), 0.0f);
    }

    activeLfoData.points.front().x = 0.0f;
    activeLfoData.points.back().x = 1.0f;
    activeLfoData.curvatures.resize(activeLfoData.points.size() - 1, 0.0f);
    for (auto& curvature : activeLfoData.curvatures)
        curvature = std::isfinite(curvature) ? juce::jlimit(-2.0f, 2.0f, curvature) : 0.0f;

    dataIsActive = true;
    repaint();
}

void LfoEditor::paint(juce::Graphics& g)
{
    const auto physicalScale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (gridCache.isNull()
        || cachedGridWidth != getWidth()
        || cachedGridHeight != getHeight()
        || std::abs(cachedGridScale - physicalScale) > 0.01f
        || cachedHorizontalDivisions != hGridDivs
        || cachedVerticalDivisions != vGridDivs)
        rebuildGridCache(physicalScale);

    if (! gridCache.isNull())
        g.drawImage(gridCache, getLocalBounds().toFloat());

    if (! dataIsActive || activeLfoData.points.size() < 2)
        return;

    const auto signature = getWavePathSignature();
    if (signature != cachedWavePathSignature)
    {
        rebuildWavePath();
        cachedWavePathSignature = signature;
    }

    auto fillPath = cachedWavePath;
    fillPath.lineTo(static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillPath.lineTo(0.0f, static_cast<float>(getHeight()));
    fillPath.closeSubPath();
    juce::ColourGradient fill(fire::ui::colours::modulation.withAlpha(0.16f),
                              0.0f, 0.0f,
                              fire::ui::colours::ember.withAlpha(0.015f),
                              0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(fill);
    g.fillPath(fillPath);

    g.setColour(fire::ui::colours::modulation.withAlpha(0.13f));
    g.strokePath(cachedWavePath,
                 juce::PathStrokeType(5.5f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    juce::ColourGradient wave(fire::ui::colours::modulation, 0.0f, 0.0f,
                              fire::ui::colours::flame,
                              static_cast<float>(getWidth()), static_cast<float>(getHeight()), false);
    wave.addColour(0.68, fire::ui::colours::whiteHot);
    g.setGradientFill(wave);
    g.strokePath(cachedWavePath,
                 juce::PathStrokeType(2.0f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Draw control points, with visual feedback for selection.
    for (int i = 0; i < activeLfoData.points.size(); ++i)
    {
        bool isSelected = std::find(selectedPointIndices.begin(), selectedPointIndices.end(), i) != selectedPointIndices.end();
        bool isHovered = (i == hoveredPointIndex);

        auto localPoint = fromNormalized(activeLfoData.points[i]);

        float currentPointRadius = pointRadius;
        juce::Colour currentPointColour = isSelected ? fire::ui::colours::whiteHot
                                                     : fire::ui::colours::modulation;

        // Apply hover effect (enlarge and make transparent) to both selected and unselected points.
        if (isHovered)
        {
            currentPointRadius *= 1.5f;
            currentPointColour = currentPointColour.brighter(0.18f);
        }
        // If dragging a selection, make them slightly larger but keep them solid for clarity.
        else if (isSelected && (draggingState == DraggingState::Selection || draggingState == DraggingState::Point))
        {
            currentPointRadius *= 1.5f;
        }

        const auto pointBounds = juce::Rectangle<float>(currentPointRadius * 2.0f,
                                                        currentPointRadius * 2.0f)
                                     .withCentre(localPoint);
        g.setColour(fire::ui::colours::canvas.withAlpha(0.95f));
        g.fillEllipse(pointBounds);
        g.setColour(currentPointColour.withAlpha(isHovered ? 1.0f : 0.88f));
        g.drawEllipse(pointBounds.reduced(0.5f), isSelected ? 2.0f : 1.3f);
        if (isSelected)
            g.fillEllipse(pointBounds.reduced(currentPointRadius * 0.50f));
    }

    // Draw the marquee selection rectangle if the user is currently dragging it.
    if (draggingState == DraggingState::Marquee)
    {
        auto rectToDraw = makeNormalised(selectionRectangle.getPosition(),
                                         selectionRectangle.getBottomRight());

        g.setColour(fire::ui::colours::modulation.withAlpha(0.14f));
        g.fillRoundedRectangle(rectToDraw.toFloat(), 2.0f);
        g.setColour(fire::ui::colours::whiteHot.withAlpha(0.85f));
        g.drawRoundedRectangle(rectToDraw.toFloat(), 2.0f, 1.0f);
    }

    // Draw playhead
    if (playheadPos >= 0.0f)
    {
        const auto x = static_cast<float>(getWidth()) * playheadPos;
        juce::ColourGradient playhead(fire::ui::colours::whiteHot.withAlpha(0.92f), x, 0.0f,
                                      fire::ui::colours::ember.withAlpha(0.18f), x,
                                      static_cast<float>(getHeight()), false);
        g.setGradientFill(playhead);
        g.fillRect(x - 0.5f, 0.0f, 1.0f, static_cast<float>(getHeight()));
        g.setColour(fire::ui::colours::whiteHot);
        g.fillEllipse(x - 2.5f, 2.0f, 5.0f, 5.0f);
    }

    // Draw phase offset line when dragging
    if (phaseOffsetPosition >= 0.0f)
    {
        g.setColour(fire::ui::colours::modulation.withAlpha(0.62f));
        g.drawVerticalLine(juce::roundToInt(getWidth() * phaseOffsetPosition), 0.0f, (float) getHeight());
    }
}

void LfoEditor::resized()
{
    gridCache = {};
    cachedWavePathSignature = 0;
}

void LfoEditor::rebuildGridCache(float physicalScale)
{
    cachedGridWidth = getWidth();
    cachedGridHeight = getHeight();
    cachedGridScale = juce::jmax(1.0f, physicalScale);
    cachedHorizontalDivisions = hGridDivs;
    cachedVerticalDivisions = vGridDivs;

    if (cachedGridWidth <= 0 || cachedGridHeight <= 0)
    {
        gridCache = {};
        return;
    }

    gridCache = juce::Image(juce::Image::ARGB,
                            juce::jmax(1, juce::roundToInt(cachedGridWidth * cachedGridScale)),
                            juce::jmax(1, juce::roundToInt(cachedGridHeight * cachedGridScale)),
                            true);
    juce::Graphics cacheGraphics(gridCache);
    cacheGraphics.addTransform(juce::AffineTransform::scale(cachedGridScale));
    fire::ui::drawCanvas(cacheGraphics, getLocalBounds().toFloat());
    fire::ui::drawTechGrid(cacheGraphics, getLocalBounds().toFloat(),
                           juce::jmax(12.0f, 20.0f), 0.075f);

    cacheGraphics.setColour(fire::ui::colours::hairline.withAlpha(0.58f));
    for (int i = 1; i < hGridDivs; ++i)
        cacheGraphics.drawVerticalLine(juce::roundToInt(getWidth() * i / static_cast<float>(hGridDivs)),
                                       0.0f, static_cast<float>(getHeight()));
    for (int i = 1; i < vGridDivs; ++i)
        cacheGraphics.drawHorizontalLine(juce::roundToInt(getHeight() * i / static_cast<float>(vGridDivs)),
                                         0.0f, static_cast<float>(getWidth()));

    cacheGraphics.setColour(fire::ui::colours::modulation.withAlpha(0.34f));
    cacheGraphics.drawHorizontalLine(getHeight() / 2, 0.0f, static_cast<float>(getWidth()));
    cacheGraphics.setColour(fire::ui::colours::hairline.withAlpha(0.92f));
    cacheGraphics.drawRect(getLocalBounds(), 1);
}

uint64_t LfoEditor::getWavePathSignature() const noexcept
{
    uint64_t hash = 1469598103934665603ull;
    auto append = [&hash](int value)
    {
        hash ^= static_cast<uint32_t>(value);
        hash *= 1099511628211ull;
    };

    append(getWidth());
    append(getHeight());
    append(static_cast<int>(activeLfoData.points.size()));
    for (const auto& point : activeLfoData.points)
    {
        append(juce::roundToInt(point.x * 100000.0f));
        append(juce::roundToInt(point.y * 100000.0f));
    }
    for (const auto curvature : activeLfoData.curvatures)
        append(juce::roundToInt(curvature * 100000.0f));
    return hash;
}

void LfoEditor::rebuildWavePath()
{
    cachedWavePath.clear();
    if (activeLfoData.points.size() < 2)
        return;

    cachedWavePath.startNewSubPath(fromNormalized(activeLfoData.points.front()));
    for (size_t i = 0; i + 1 < activeLfoData.points.size(); ++i)
    {
        const auto p1 = fromNormalized(activeLfoData.points[i]);
        const auto p2 = fromNormalized(activeLfoData.points[i + 1]);
        if (std::abs(p1.x - p2.x) < 0.1f)
        {
            cachedWavePath.lineTo(p2);
            continue;
        }

        const auto curvature = i < activeLfoData.curvatures.size()
                                   ? activeLfoData.curvatures[i]
                                   : 0.0f;
        if (juce::approximatelyEqual(curvature, 0.0f))
        {
            cachedWavePath.lineTo(p2);
            continue;
        }

        constexpr int segmentCount = 24;
        const auto exponent = std::pow(4.0f, std::abs(curvature));
        for (int segment = 1; segment <= segmentCount; ++segment)
        {
            const auto t = static_cast<float>(segment) / static_cast<float>(segmentCount);
            const auto shaped = curvature >= 0.0f
                                    ? std::pow(t, exponent)
                                    : 1.0f - std::pow(juce::jmax(0.0f, 1.0f - t), exponent);
            cachedWavePath.lineTo(p1.x + (p2.x - p1.x) * t,
                                  p1.y + (p2.y - p1.y) * shaped);
        }
    }
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

void LfoEditor::setPhaseOffsetLinePosition(float position)
{
    if (! juce::approximatelyEqual(phaseOffsetPosition, position))
    {
        phaseOffsetPosition = position;
        repaint();
    }
}

void LfoEditor::setSmoothness(float smoothness)
{
    activeLfoData.smoothness = std::isfinite(smoothness)
                                   ? juce::jlimit(0.0f, 1.0f, smoothness)
                                   : 0.0f;
}

void LfoEditor::mouseDown(const juce::MouseEvent& event)
{
    if (! dataIsActive)
        return;

    // Treat macOS Ctrl-click exactly like a physical right-click. The former
    // mouseUp-only check allowed a popup gesture to paint a brush cell or
    // begin a point edit before the context menu opened.
    if (event.mods.isPopupMenu() || ! event.mods.isLeftButtonDown())
        return;

    if (isShowing() || isOnDesktop())
        grabKeyboardFocus();

    if (currentMode == LfoEditMode::BrushPaint)
    {
        if (event.mods.isLeftButtonDown())
        {
            isBrushing = true;
            applyBrushShape(event.getPosition());

            const float gridW = 1.0f / (float) hGridDivs;
            const float gridH = 1.0f / (float) vGridDivs;
            const int gridX = juce::jlimit(0, hGridDivs - 1, (int) ((float) event.x / (float) juce::jmax(1, getWidth()) / gridW));
            const int gridY = juce::jlimit(0, vGridDivs - 1, (int) ((float) event.y / (float) juce::jmax(1, getHeight()) / gridH));
            lastBrushCell = { gridX, gridY };
        }
        return;
    }

    if (currentMode != LfoEditMode::PointEdit)
        return;

    // A topology-changing command may have run between mouse gestures. Never
    // carry an index from the old point vector into a new gesture.
    if (! hasValidSelectedPointIndices())
        cancelPointAndCurveInteraction();

    initialDragPositions.clear();
    draggingState = DraggingState::None;
    editingCurveIndex = -1;

    // --- Shift + Drag for Marquee Selection ---
    if (event.mods.isShiftDown())
    {
        draggingState = DraggingState::Marquee;
        selectionRectangle.setPosition(event.getPosition());
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
        // Deselect points when clicking on an empty area.
        if (! selectedPointIndices.empty())
        {
            selectedPointIndices.clear();
            repaint();
        }

        editingCurveIndex = findSegmentIndexAt(event.getPosition());
        if (isValidCurveIndex(editingCurveIndex))
        {
            const auto curveIndex = static_cast<size_t>(editingCurveIndex);
            initialCurvature = activeLfoData.curvatures[curveIndex];
            initialDragY = event.y;
        }
        else
        {
            editingCurveIndex = -1;
        }
    }

    // If we are starting any kind of drag, store the initial positions of all selected points.
    if (draggingState == DraggingState::Point || draggingState == DraggingState::Selection)
    {
        dragAnchor = toNormalized(event.getPosition());
        initialDragPositions.clear();
        for (int index : selectedPointIndices)
        {
            if (! isValidPointIndex(index))
            {
                cancelPointAndCurveInteraction();
                repaint();
                return;
            }
            initialDragPositions.push_back(
                activeLfoData.points[static_cast<size_t>(index)]);
        }
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
        const int gridX = juce::jlimit(0, hGridDivs - 1, (int) ((float) event.x / (float) juce::jmax(1, getWidth()) / gridW));
        const int gridY = juce::jlimit(0, vGridDivs - 1, (int) ((float) event.y / (float) juce::jmax(1, getHeight()) / gridH));
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
        if (! validateCurveInteractionOrCancel())
        {
            repaint();
            return;
        }

        const auto curveIndex = static_cast<size_t>(editingCurveIndex);

        // Get the start and end points of the segment in screen coordinates to calculate the visual slope.
        auto p1_screen = fromNormalized(activeLfoData.points[curveIndex]);
        auto p2_screen = fromNormalized(activeLfoData.points[curveIndex + 1]);

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

        activeLfoData.curvatures[curveIndex] = juce::jlimit(-2.0f, 2.0f, initialCurvature + curvatureChange);

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
            // Validate the complete parallel state before reading either
            // vector. A partial drag would be worse than cancelling it.
            if (! validatePointDragInteractionOrCancel())
            {
                repaint();
                return;
            }

            // 1. Calculate the raw, un-snapped delta from the anchor point.
            auto currentNormPos = toNormalized(event.getPosition());
            auto delta = currentNormPos - dragAnchor;

            // 2. If snapping is active, calculate the *snapped* target positions.
            if (event.mods.isCommandDown() || event.mods.isCtrlDown())
            {
                // For the primary point being dragged, find its ideal snapped position.
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
                    if (isValidPointIndex(leftNeighborIndex))
                        maxLeftDelta = activeLfoData.points[static_cast<size_t>(leftNeighborIndex)].x
                                     - initialLeftmostX;
                }
            }

            if (rightmostSelectedPointIndex
                < static_cast<int>(activeLfoData.points.size()) - 1)
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
                    if (isValidPointIndex(rightNeighborIndex))
                        maxRightDelta = activeLfoData.points[static_cast<size_t>(rightNeighborIndex)].x
                                      - initialRightmostX;
                }
            }

            delta.x = juce::jlimit(maxLeftDelta, maxRightDelta, delta.x);

            // 4. Apply the final, constrained (and possibly snapped) delta to all selected points.
            for (size_t i = 0; i < selectedPointIndices.size(); ++i)
            {
                int pointIndex = selectedPointIndices[i];
                auto& initialPos = initialDragPositions[i];
                auto& point = activeLfoData.points[static_cast<size_t>(pointIndex)];
                point.y = juce::jlimit(0.0f, 1.0f, initialPos.y + delta.y);
                if (pointIndex > 0
                    && pointIndex < static_cast<int>(activeLfoData.points.size()) - 1)
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
    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem(CommandIDs::selectAll, "Select All");
        m.addItem(CommandIDs::clear, "Clear");
        m.addSeparator();
        m.addItem(CommandIDs::copy, "Copy", dataIsActive && activeLfoData.points.size() > 2);
        m.addItem(CommandIDs::paste, "Paste", canPasteShape());
        m.addSeparator();
        m.addItem(CommandIDs::invertX, "Invert Horizontally", dataIsActive && activeLfoData.points.size() > 2);
        m.addItem(CommandIDs::invertY, "Invert Vertically", dataIsActive && activeLfoData.points.size() > 2);

        auto callback = [safeThis = juce::Component::SafePointer<LfoEditor>(this)](int result)
        {
            if (safeThis == nullptr)
                return;

            switch (result)
            {
                case CommandIDs::selectAll:
                    safeThis->selectAllPoints();
                    break;
                case CommandIDs::clear:
                    safeThis->clearAllPoints();
                    if (safeThis->onDataChanged)
                        safeThis->onDataChanged(safeThis->activeLfoData);
                    break;
                case CommandIDs::copy:
                    safeThis->copyShape();
                    break;
                case CommandIDs::paste:
                    if (safeThis->pasteShape() && safeThis->onDataChanged)
                        safeThis->onDataChanged(safeThis->activeLfoData);
                    break;
                case CommandIDs::invertX:
                    safeThis->invertShape(true, false);
                    if (safeThis->onDataChanged)
                        safeThis->onDataChanged(safeThis->activeLfoData);
                    break;
                case CommandIDs::invertY:
                    safeThis->invertShape(false, true);
                    if (safeThis->onDataChanged)
                        safeThis->onDataChanged(safeThis->activeLfoData);
                    break;
                default:
                    break;
            }
        };

        m.showMenuAsync(fire::ui::prepareContextMenu(
                            m, *this, event.getScreenPosition()),
                        callback);

        return; // We've handled the right-click, so we exit here.
    }

    if (! dataIsActive)
        return;
    bool dataWasChanged = false;

    if (draggingState == DraggingState::Marquee)
    {
        auto finalRect = makeNormalised(event.getMouseDownPosition(), event.getPosition()).toFloat();
        for (int i = 0; i < activeLfoData.points.size(); ++i)
        {
            if (finalRect.contains(fromNormalized(activeLfoData.points[i])))
            {
                // Add to selection only if not already selected
                if (std::find(selectedPointIndices.begin(), selectedPointIndices.end(), i) == selectedPointIndices.end())
                    selectedPointIndices.push_back(i);
            }
        }
        selectionRectangle.setSize(0, 0);
    }

    if (draggingState == DraggingState::Point || draggingState == DraggingState::Selection)
    {
        if (hasValidPointDragState())
        {
            updateAndSortPoints();
            dataWasChanged = true;
        }
        else
        {
            cancelPointAndCurveInteraction();
        }
    }

    draggingState = DraggingState::None;
    editingCurveIndex = -1;
    initialDragPositions.clear();

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
        {
            const auto pointCountBeforeMerge = activeLfoData.points.size();
            activeLfoData.mergeDuplicatePoints();
            if (activeLfoData.points.size() != pointCountBeforeMerge)
                cancelPointAndCurveInteraction();
        }
        onDataChanged(activeLfoData);
    }
}

void LfoEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (! dataIsActive || currentMode != LfoEditMode::PointEdit
        || event.mods.isPopupMenu() || ! event.mods.isLeftButtonDown())
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

    cancelAllInteraction();

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
    if (! dataIsActive
        || ! isValidPointIndex(index)
        || index == 0
        || index == static_cast<int>(activeLfoData.points.size()) - 1)
        return;

    cancelAllInteraction();

    activeLfoData.points.erase(activeLfoData.points.begin() + index);

    // Removing a point merges two segments. We must remove one curvature value.
    // We remove the curvature of the first of the two merged segments.
    if (juce::isPositiveAndBelow(index - 1, static_cast<int>(activeLfoData.curvatures.size())))
        activeLfoData.curvatures.erase(activeLfoData.curvatures.begin() + index - 1);

    // We then set the curvature of the new, merged segment to 0.0 (linear).
    if (juce::isPositiveAndBelow(index - 1,
                                 static_cast<int>(activeLfoData.curvatures.size())))
    {
        activeLfoData.curvatures[static_cast<size_t>(index - 1)] = 0.0f;
    }

    repaint();
}

juce::Point<float> LfoEditor::toNormalized(juce::Point<int> localPoint)
{
    const auto width = juce::jmax(1, getWidth());
    const auto height = juce::jmax(1, getHeight());
    return { juce::jlimit(0.0f, 1.0f, (float) localPoint.x / (float) width),
             juce::jlimit(0.0f, 1.0f, 1.0f - ((float) localPoint.y / (float) height)) };
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

    // Brush painting replaces and reorders points, invalidating every point or
    // curve index. Keep the brush gesture itself alive so drag-to-paint and its
    // final mouseUp notification retain their existing semantics.
    cancelPointAndCurveInteraction();

    // 1. Identify grid cell and its boundaries.
    const float gridW = 1.0f / (float) hGridDivs;
    const float gridH = 1.0f / (float) vGridDivs;
    const int gridX = juce::jlimit(0, hGridDivs - 1, (int) ((float) clickPosition.x / (float) juce::jmax(1, getWidth()) / gridW));
    const int gridY = juce::jlimit(0, vGridDivs - 1, (int) ((float) clickPosition.y / (float) juce::jmax(1, getHeight()) / gridH));
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

    // Brush replacement can add up to three points to a sparse grid cell even
    // when the editor was already at its handle limit. Canonicalise the
    // completed topology before it can be displayed or published so the UI
    // and LfoManager never independently reduce different copies of it.
    if (activeLfoData.points.size() > LfoData::maximumNumberOfPoints)
        activeLfoData.sanitise();

    repaint();
}

int LfoEditor::findSegmentIndexAt(const juce::Point<int>& position) const
{
    // Safety checks: ensure there is data and at least one segment to check.
    if (! dataIsActive || activeLfoData.points.size() < 2 || getWidth() <= 0)
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
    cancelAllInteraction();
    currentMode = newMode;
    repaint();
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
            if (pasteShape() && onDataChanged)
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

    auto indicesToDelete = selectedPointIndices;
    cancelAllInteraction();
    std::sort(indicesToDelete.rbegin(), indicesToDelete.rend());
    for (int index : indicesToDelete)
    {
        if (isValidPointIndex(index)
            && index > 0
            && index < static_cast<int>(activeLfoData.points.size()) - 1)
            removePoint(index);
    }
    repaint();
}

//==============================================================================
// LfoPanel Implementation
//==============================================================================
LfoPanel::LfoPanel(FireAudioProcessor& p) : processor(p)
{
    setOpaque(true);
    for (int i = 0; i < 4; ++i)
    {
        syncParameterIDs[static_cast<size_t>(i)] = ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, i);
        smoothParameterIDs[static_cast<size_t>(i)] = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i);
    }

    lfoEditor.setDataToDisplay(getLfoDataCopy(currentLfoIndex));
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
        lfoSelectButtons[i] = std::make_unique<PrimaryTextButton>("LFO " + juce::String(i + 1));
        addAndMakeVisible(lfoSelectButtons[i].get());
        lfoSelectButtons[i]->setRadioGroupId(1);
        lfoSelectButtons[i]->getProperties().set("fireAnimatedSelection", true);
        styleLfoSelectButton(*lfoSelectButtons[i], lfoColours[static_cast<size_t>(i)]);
        lfoSelectButtons[i]->addListener(this);
    }
    lfoSelectButtons[0]->setToggleState(true, juce::dontSendNotification);
    lfoSelectionPosition.snapTo(0.0f);

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
    rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    rateSlider.setColour(juce::Slider::rotarySliderFillColourId, fire::ui::colours::modulation);
    rateSlider.addListener(this);

    addAndMakeVisible(rateLabel);
    rateLabel.setText("Rate", juce::dontSendNotification);
    rateLabel.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain") });
    rateLabel.attachToComponent(&rateSlider, false);
    rateLabel.setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
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
        processor.treeState.addParameterListener(syncParameterIDs[static_cast<size_t>(i)], this);
        processor.treeState.addParameterListener(smoothParameterIDs[static_cast<size_t>(i)], this);
    }

    // Initialize the smooth slider and label.
    addAndMakeVisible(lfoSmoothSlider);
    lfoSmoothSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoSmoothSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    lfoSmoothSlider.setColour(juce::Slider::rotarySliderFillColourId, fire::ui::colours::modulation);

    addAndMakeVisible(lfoSmoothLabel);
    lfoSmoothLabel.setText("Smooth", juce::dontSendNotification);
    lfoSmoothLabel.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain") });
    lfoSmoothLabel.attachToComponent(&lfoSmoothSlider, false);
    lfoSmoothLabel.setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
    lfoSmoothLabel.setJustificationType(juce::Justification::centred);

    // Initialize the phase slider and label.
    addAndMakeVisible(lfoPhaseSlider);
    lfoPhaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoPhaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    lfoPhaseSlider.setColour(juce::Slider::rotarySliderFillColourId, fire::ui::colours::modulation);
    lfoPhaseSlider.addListener(this);

    addAndMakeVisible(lfoPhaseLabel);
    lfoPhaseLabel.setText("Phase", juce::dontSendNotification);
    lfoPhaseLabel.setFont(juce::Font {
        juce::FontOptions()
            .withName(KNOB_FONT)
            .withHeight(KNOB_FONT_SIZE)
            .withStyle("Plain") });
    lfoPhaseLabel.attachToComponent(&lfoPhaseSlider, false);
    lfoPhaseLabel.setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
    lfoPhaseLabel.setJustificationType(juce::Justification::centred);

    // Attachments
    setLfo(currentLfoIndex); // Call helper to set up all attachments for the initial LFO.

}

LfoPanel::~LfoPanel()
{
    // Attachments are declared after their Sliders, so they are destroyed
    // first. Close every active Slider gesture while the old attachment can
    // still send its matching endGesture to the host.
    dismissTransientInteraction();
    dismissModulationMatrixDialog();

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
    rateSlider.removeListener(this);
    lfoPhaseSlider.removeListener(this);

    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.removeParameterListener(syncParameterIDs[static_cast<size_t>(i)], this);
        processor.treeState.removeParameterListener(smoothParameterIDs[static_cast<size_t>(i)], this);
    }

    cancelPendingUpdate();
}

void LfoPanel::paint(juce::Graphics& g)
{
    fire::ui::drawCanvas(g, getLocalBounds().toFloat());
    fire::ui::drawPanel(g, leftColumnArea.toFloat(), fire::ui::colours::modulation, false);
    fire::ui::drawPanel(g, centerColumnArea.toFloat(), fire::ui::colours::modulation, false);
    fire::ui::drawPanel(g, rightColumnArea.toFloat(), fire::ui::colours::flame, false);

    const auto titleHeight = juce::jmax(16.0f, 21.0f * scale);
    const auto drawPlainTitle = [&g, titleHeight, this](juce::Rectangle<int> area,
                                                        const juce::String& title)
    {
        auto titleArea = area.toFloat().removeFromTop(titleHeight)
                             .withTrimmedLeft(juce::jmax(7.0f, 10.0f * scale));
        g.setColour(fire::ui::colours::textSecondary);
        g.setFont(fire::ui::labelFont(juce::jlimit(9.0f, 13.0f, titleHeight * 0.36f)));
        g.drawText(title, titleArea, juce::Justification::centredLeft);
    };
    drawPlainTitle(leftColumnArea, "LFO BANK");
    drawPlainTitle(centerColumnArea, "SHAPE FORGE");
    drawPlainTitle(rightColumnArea, "MOTION");

    if (! separatorLine.isEmpty())
    {
        g.setColour(fire::ui::colours::hairline.withAlpha(0.68f));
        g.fillRect(separatorLine);
    }
}

void LfoPanel::paintOverChildren(juce::Graphics& g)
{
    if (lfoSelectButtons.empty() || lfoSelectButtons.front() == nullptr)
        return;

    const auto position = juce::jlimit(0.0f, 3.0f, lfoSelectionPosition.current);
    const auto lowerIndex = juce::jlimit(0, 3, static_cast<int>(std::floor(position)));
    const auto upperIndex = juce::jmin(3, lowerIndex + 1);
    const auto blend = position - static_cast<float>(lowerIndex);
    const auto lowerBounds = lfoSelectButtons[static_cast<size_t>(lowerIndex)]->getBounds().toFloat();
    const auto upperBounds = lfoSelectButtons[static_cast<size_t>(upperIndex)]->getBounds().toFloat();

    auto selectionBounds = juce::Rectangle<float>(
        juce::jmap(blend, lowerBounds.getX(), upperBounds.getX()),
        juce::jmap(blend, lowerBounds.getY(), upperBounds.getY()),
        juce::jmap(blend, lowerBounds.getWidth(), upperBounds.getWidth()),
        juce::jmap(blend, lowerBounds.getHeight(), upperBounds.getHeight()))
                               .reduced(0.75f);
    const auto colour = lfoColours[static_cast<size_t>(lowerIndex)].interpolatedWith(
        lfoColours[static_cast<size_t>(upperIndex)], blend);
    const auto radius = juce::jmin(selectionBounds.getHeight() * 0.5f,
                                   fire::ui::Metrics::radius * scale);

    // The moving outline is painted above the buttons so it cannot be hidden by
    // their hover surfaces.  A very light wash keeps the text fully legible.
    g.setColour(colour.withAlpha(0.055f));
    g.fillRoundedRectangle(selectionBounds, radius);
    g.setColour(colour.withAlpha(0.82f));
    g.drawRoundedRectangle(selectionBounds, radius, juce::jmax(1.0f, 1.25f * scale));
}

void LfoPanel::resized()
{
    const auto uiScale = scale;
    const auto outer = juce::jmax(4, juce::roundToInt(7.0f * uiScale));
    const auto gap = juce::jmax(4, juce::roundToInt(7.0f * uiScale));
    const auto titleHeight = juce::jmax(16, juce::roundToInt(21.0f * uiScale));
    const auto scaledKnobSize = juce::jmax(1, juce::roundToInt(KNOB_SIZE * 0.82f * uiScale));

    auto mainArea = getLocalBounds().reduced(outer);
    const auto leftWidth = juce::jlimit(104, juce::roundToInt(150.0f * uiScale),
                                        juce::roundToInt(mainArea.getWidth() * 0.14f));
    const auto rightWidth = juce::jlimit(248, juce::roundToInt(340.0f * uiScale),
                                         juce::roundToInt(mainArea.getWidth() * 0.33f));
    leftColumnArea = mainArea.removeFromLeft(leftWidth);
    mainArea.removeFromLeft(gap);
    rightColumnArea = mainArea.removeFromRight(rightWidth);
    mainArea.removeFromRight(gap);
    centerColumnArea = mainArea;

    auto leftContent = leftColumnArea.reduced(juce::jmax(4, outer / 2));
    leftContent.removeFromTop(titleHeight);
    juce::FlexBox lfoSelectBox;
    lfoSelectBox.flexDirection = juce::FlexBox::Direction::column;
    lfoSelectBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    for (const auto& button : lfoSelectButtons)
        lfoSelectBox.items.add(juce::FlexItem(*button).withFlex(1.0f)
                                   .withMargin(juce::FlexItem::Margin(2.0f * uiScale)));
    lfoSelectBox.performLayout(leftContent);

    auto centreContent = centerColumnArea.reduced(juce::jmax(4, outer / 2));
    centreContent.removeFromTop(titleHeight);
    topRowArea = centreContent.removeFromTop(juce::jmax(27, juce::roundToInt(34.0f * uiScale)));
    centreContent.removeFromTop(juce::jmax(3, gap / 2));
    lfoEditor.setBounds(centreContent);

    juce::FlexBox topRowFlexBox;
    topRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
    topRowFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    topRowFlexBox.alignItems = juce::FlexBox::AlignItems::stretch;
    std::vector<juce::Component*> topRowControls = {
        &matrixButton, &syncButton, &assignButton, &editModeButton, &brushModeButton, &brushSelector
    };
    for (auto* control : topRowControls)
        topRowFlexBox.items.add(juce::FlexItem(*control).withFlex(1.0f)
                                    .withMargin(juce::FlexItem::Margin(0.0f, 1.5f * uiScale,
                                                                       0.0f, 1.5f * uiScale)));
    topRowFlexBox.performLayout(topRowArea);

    auto rightColumnWorkArea = rightColumnArea.reduced(juce::jmax(4, outer / 2));
    rightColumnWorkArea.removeFromTop(titleHeight);
    auto gridArea = rightColumnWorkArea.removeFromBottom(juce::jmax(28, juce::roundToInt(36.0f * uiScale)));
    separatorLine = rightColumnWorkArea.removeFromBottom(1);
    rightColumnWorkArea.removeFromBottom(juce::jmax(3, gap / 2));
    auto knobsArea = rightColumnWorkArea;

    juce::FlexBox gridBox;
    gridBox.flexDirection = juce::FlexBox::Direction::row;
    gridBox.alignItems = juce::FlexBox::AlignItems::stretch;
    gridBox.items.add(juce::FlexItem(gridXLabel).withFlex(0.3f).withMargin({ 0, 2, 0, 0 }));
    gridBox.items.add(juce::FlexItem(gridXSlider).withFlex(1.0f));
    gridBox.items.add(juce::FlexItem().withWidth(5 * uiScale));
    gridBox.items.add(juce::FlexItem(gridYLabel).withFlex(0.3f).withMargin({ 0, 2, 0, 0 }));
    gridBox.items.add(juce::FlexItem(gridYSlider).withFlex(1.0f));
    gridBox.performLayout(gridArea);

    juce::Grid knobGrid;
    using Track = juce::Grid::TrackInfo;

    knobGrid.templateColumns = {
        Track(juce::Grid::Px(scaledKnobSize)),
        Track(juce::Grid::Px(juce::jmax(1.0f, 3.0f * uiScale))),
        Track(juce::Grid::Px(scaledKnobSize)),
        Track(juce::Grid::Px(juce::jmax(1.0f, 3.0f * uiScale))),
        Track(juce::Grid::Px(scaledKnobSize))
    };

    knobGrid.templateRows = { Track(juce::Grid::Px(scaledKnobSize)) };

    knobGrid.items.add(juce::GridItem(&rateSlider));
    knobGrid.items.add(juce::GridItem());
    knobGrid.items.add(juce::GridItem(&lfoSmoothSlider));
    knobGrid.items.add(juce::GridItem());
    knobGrid.items.add(juce::GridItem(&lfoPhaseSlider));

    knobGrid.justifyItems = juce::Grid::JustifyItems::center;
    knobGrid.alignItems = juce::Grid::AlignItems::center;
    knobGrid.justifyContent = juce::Grid::JustifyContent::center;
    knobGrid.alignContent = juce::Grid::AlignContent::center;

    knobGrid.performLayout(knobsArea);
}

void LfoPanel::animationTick(float deltaSeconds)
{
    if (pendingRateSliderUpdate.load(std::memory_order_acquire)
        || pendingSmoothnessUpdates.load(std::memory_order_acquire) != 0)
        handleAsyncUpdate();

    if (! isShowing())
    {
        lfoSelectionPosition.snapTo(lfoSelectionPosition.target);
        return;
    }

    const auto previousSelectionPosition = lfoSelectionPosition.current;
    lfoSelectionPosition.advance(deltaSeconds, 0.07f);
    if (! juce::approximatelyEqual(previousSelectionPosition, lfoSelectionPosition.current))
        repaint(leftColumnArea);

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
        showModulationMatrixDialog();
        return;
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
    if (! juce::isPositiveAndBelow(newIndex, static_cast<int>(lfoSelectButtons.size())))
        return;

    // This must precede changing currentLfoIndex, editor data, or resetting an
    // attachment. A stale drag/text editor belongs exclusively to the LFO that
    // was visible when the interaction began.
    dismissTransientInteraction();

    const bool selectionChanged = currentLfoIndex != newIndex;

    // Update the current LFO index and tell the editor to display the new data.
    currentLfoIndex = newIndex;
    lfoSelectionPosition.setTarget(static_cast<float>(currentLfoIndex));
    repaint(leftColumnArea);
    lfoEditor.setDataToDisplay(getLfoDataCopy(currentLfoIndex));

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
        processor.treeState, syncParameterIDs[static_cast<size_t>(currentLfoIndex)], syncButton);

    lfoSmoothAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.treeState, smoothParameterIDs[static_cast<size_t>(currentLfoIndex)], lfoSmoothSlider);

    lfoPhaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.treeState, ParameterIDAndName::getIDString(LFO_PHASE_ID, currentLfoIndex), lfoPhaseSlider);
    // This must be called after attachments are updated.
    updateRateSlider();

    if (selectionChanged && onCurrentLfoChanged)
        onCurrentLfoChanged(currentLfoIndex);
}

void LfoPanel::dismissTransientInteraction()
{
    for (auto& button : lfoSelectButtons)
        if (button != nullptr)
            button->dismissPointerGesture();
    editModeButton.dismissPointerGesture();
    brushModeButton.dismissPointerGesture();
    assignButton.dismissPointerGesture();
    matrixButton.dismissPointerGesture();
    syncButton.dismissPointerGesture();

    rateSlider.dismissTransientInteraction();
    gridXSlider.dismissTransientInteraction();
    gridYSlider.dismissTransientInteraction();
    lfoSmoothSlider.dismissTransientInteraction();
    lfoPhaseSlider.dismissTransientInteraction();
}

void LfoPanel::configureModulationMatrixDialog(
    juce::DialogWindow::LaunchOptions& launchOptions)
{
    launchOptions.content.setOwned(new ModulationMatrixPanel(processor));
    launchOptions.content->setSize(800, 400);
    launchOptions.dialogTitle = "Modulation Matrix";
    launchOptions.componentToCentreAround = this;
}

void LfoPanel::showModulationMatrixDialog()
{
    if (! isShowing())
        return;

    if (auto* existingDialog = modulationMatrixDialog.getComponent())
    {
        if (existingDialog->isShowing()
            && existingDialog->isCurrentlyModal(false))
        {
            existingDialog->toFront(true);
            return;
        }

        // DefaultDialogWindow::closeButtonPressed() hides the window and leaves
        // auto-deletion queued for ModalComponentManager's next async update.
        // Remove that stale window synchronously before reopening.
        dismissModulationMatrixDialog();
    }

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (modulationMatrixDialogFactoryForTesting)
    {
        modulationMatrixDialog =
            modulationMatrixDialogFactoryForTesting();
        return;
    }
#endif

    // LaunchOptions owns and automatically deletes the modal dialog and its
    // content when the modal state ends.
    juce::DialogWindow::LaunchOptions launchOptions;
    configureModulationMatrixDialog(launchOptions);
    modulationMatrixDialog = launchOptions.launchAsync();
}

void LfoPanel::dismissModulationMatrixDialog()
{
    // launchAsync() normally defers deletion until the modal manager's next
    // async update. The dialog content holds a reference to the processor, so
    // an editor/processor teardown must not leave that deletion queued.
    auto dialog = modulationMatrixDialog;
    modulationMatrixDialog = nullptr;

    if (dialog != nullptr)
    {
        dialog->exitModalState(0);
        dialog.deleteAndZero();
    }
}

void LfoPanel::visibilityChanged()
{
    juce::Component::visibilityChanged();

    if (! isShowing())
    {
        dismissTransientInteraction();
        dismissModulationMatrixDialog();
    }
}

void LfoPanel::setOnDataChangedCallback(std::function<void()> callback)
{
    // Here we connect the LfoPanel's callback to the LfoEditor's callback.
    // This completes the chain from the innermost component to the outermost.
    onDataChanged = callback;
}

LfoData LfoPanel::getLfoDataCopy(int index)
{
    const auto data = processor.getLfoManager().getLfoDataCopy();

    if (juce::isPositiveAndBelow(index, static_cast<int>(data.size())))
        return data[static_cast<size_t>(index)];

    jassertfalse;
    return {};
}

void LfoPanel::updateRateSlider()
{
    // A SliderAttachment owns the begin/end gesture pair for its parameter.
    // Replacing it while the slider is down strands the old parameter's begin
    // gesture and sends the eventual end gesture to the new parameter. Keep
    // the current attachment alive until Slider has finished notifying all of
    // its drag listeners, then refresh it on the next shared-clock tick.
    if (isDraggingRateSlider)
    {
        rateSliderRefreshWasDeferred = true;
        return;
    }

    rateSliderRefreshWasDeferred = false;

    // --- Get Parameter IDs using the robust ParameterID namespace ---
    // The currentLfoIndex is 0-based, which matches our arrays perfectly.
    const auto& syncModeID = syncParameterIDs[static_cast<size_t>(currentLfoIndex)];
    auto rateSyncID = ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, currentLfoIndex);
    auto rateHzID = ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, currentLfoIndex);

    // Find out if the current LFO is in sync mode from the parameter value.
    // We use .getParamID() to get the string from the juce::ParameterID object.
    auto* param = processor.treeState.getParameter(syncModeID);
    jassert(param != nullptr);
    if (param == nullptr)
        return;

    bool isInSyncMode = param->getValue() > 0.5f;

    if (isInSyncMode)
        rateSlider.hideTextBox(true);

    // First, always destroy the old attachment before creating a new one.
    rateSliderAttachment.reset();

    if (isInSyncMode)
    {
        // --- BPM SYNC MODE (Discrete Choices) ---
        rateSlider.setTextBoxIsEditable(false);

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
        rateSlider.setTextBoxIsEditable(true);
        rateSlider.textFromValueFunction = nullptr;
        rateSlider.valueFromTextFunction = nullptr;

        // Create the attachment using the correct ParameterID.
        rateSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.treeState, rateHzID, rateSlider);
    }
}

void LfoPanel::parameterChanged(const juce::String& parameterID, float /*newValue*/)
{
    // This callback may run on the audio thread. Avoid reading message-thread
    // UI state here; any sync-mode change can coalesce into one UI refresh.
    for (int i = 0; i < 4; ++i)
    {
        if (parameterID == syncParameterIDs[static_cast<size_t>(i)])
        {
            pendingRateSliderUpdate.store(true, std::memory_order_release);
            return;
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        if (parameterID == smoothParameterIDs[static_cast<size_t>(i)])
        {
            // APVTS listeners may run on the audio thread. Record a bit only;
            // copying vectors, taking the LFO-data lock, and notifying UI/state
            // are all deferred to handleAsyncUpdate on the message thread.
            pendingSmoothnessUpdates.fetch_or(1u << static_cast<unsigned int>(i),
                                              std::memory_order_release);

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
    if (slider == &rateSlider)
    {
        isDraggingRateSlider = true;
    }
    else if (slider == &lfoPhaseSlider)
    {
        isDraggingPhaseSlider = true;
        lfoEditor.setPhaseOffsetLinePosition(lfoPhaseSlider.getValue());
    }
}

void LfoPanel::sliderDragEnded(juce::Slider* slider)
{
    if (slider == &rateSlider)
    {
        isDraggingRateSlider = false;

        // Do not replace the attachment from inside Slider's ListenerList:
        // its own listener still needs this drag-ended callback in order to
        // close the old host gesture.
        if (rateSliderRefreshWasDeferred)
            pendingRateSliderUpdate.store(true, std::memory_order_release);
    }
    else if (slider == &lfoPhaseSlider)
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
    button.addListener(this);
    button.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface0);
    button.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::surface2);
    button.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    button.setColour(juce::TextButton::textColourOnId, fire::ui::colours::whiteHot);
    button.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);

    // Specific style for toggle buttons
    if (isToggle)
    {
        button.setClickingTogglesState(true);
    }
}

void LfoPanel::refreshLfoDisplay()
{
    lfoEditor.setDataToDisplay(getLfoDataCopy(currentLfoIndex));
}

void LfoEditor::selectAllPoints()
{
    if (! dataIsActive)
        return;

    cancelAllInteraction();
    for (int i = 0; i < static_cast<int>(activeLfoData.points.size()); ++i)
    {
        selectedPointIndices.push_back(i);
    }
    repaint();
}

void LfoEditor::clearAllPoints()
{
    if (! dataIsActive)
        return;

    cancelAllInteraction();
    activeLfoData.resetToDefault();
    repaint();
}

void LfoEditor::copyShape()
{
    if (! dataIsActive)
        return;
    lfoClipboard = activeLfoData;
}

bool LfoEditor::canPasteShape() const noexcept
{
    return dataIsActive && lfoClipboard.has_value();
}

bool LfoEditor::pasteShape()
{
    if (! canPasteShape())
        return false;

    setDataToDisplay(*lfoClipboard);
    return true;
}

void LfoEditor::invertShape(bool invertX, bool invertY)
{
    if (! dataIsActive || activeLfoData.points.size() < 2)
        return;

    cancelAllInteraction();

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
        // Swap the y-values of the first and last points.
        std::swap(activeLfoData.points.front().y, activeLfoData.points.back().y);

        updateAndSortPoints();

        // Invert curvatures when inverting horizontally.
        std::reverse(activeLfoData.curvatures.begin(), activeLfoData.curvatures.end());
        for (auto& curvature : activeLfoData.curvatures)
        {
            curvature = -curvature;
        }
    }

    repaint();
}

void LfoPanel::handleAsyncUpdate()
{
    // This function is guaranteed to be called on the main UI thread.
    // It is now safe to update the slider and its attachment here.
    if (pendingRateSliderUpdate.exchange(false, std::memory_order_acquire))
        updateRateSlider();

    const auto smoothnessUpdates = pendingSmoothnessUpdates.exchange(0, std::memory_order_acquire);
    for (int i = 0; i < 4; ++i)
    {
        if ((smoothnessUpdates & (1u << static_cast<unsigned int>(i))) == 0)
            continue;

        const auto& parameterID = smoothParameterIDs[static_cast<size_t>(i)];
        if (auto* value = processor.treeState.getRawParameterValue(parameterID))
        {
            auto lfoData = getLfoDataCopy(i);
            lfoData.smoothness = value->load(std::memory_order_relaxed);
            processor.getLfoManager().setLfoData(i, lfoData);
            if (i == currentLfoIndex)
                lfoEditor.setSmoothness(lfoData.smoothness);
        }
    }
}

void LfoPanel::styleLfoSelectButton(juce::TextButton& button, juce::Colour colour)
{
    button.setClickingTogglesState(true);
    button.setRadioGroupId(1);
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOnId, colour);
    button.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
}
