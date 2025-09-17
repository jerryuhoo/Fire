#include "LfoPanel.h"
#include "../../DSP/LfoShapeGenerator.h"
#include "../../PluginProcessor.h"

//==============================================================================
// LfoEditor Implementation
//==============================================================================

LfoEditor::LfoEditor() {} // Constructor is now simple.
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
        g.drawVerticalLine(juce::roundToInt(getWidth() * i / (float) hGridDivs), 0.0f, getHeight());
    for (int i = 1; i < vGridDivs; ++i)
        g.drawHorizontalLine(juce::roundToInt(getHeight() * i / (float) vGridDivs), 0.0f, getWidth());

    // Defensive check: ensure data is valid before drawing.
    if (activeLfoData == nullptr || activeLfoData->points.size() < 2)
        return;

    // Draw LFO Path
    juce::Path lfoPath;
    lfoPath.startNewSubPath(fromNormalized(activeLfoData->points.front()));

    for (size_t i = 0; i < activeLfoData->points.size() - 1; ++i)
    {
        auto p1_screen = fromNormalized(activeLfoData->points[i]);
        auto p2_screen = fromNormalized(activeLfoData->points[i + 1]);

        // Safety check to prevent array out of bounds.
        bool isCurved = (i < activeLfoData->curvatures.size() && ! juce::approximatelyEqual(activeLfoData->curvatures[i], 0.0f));

        if (juce::approximatelyEqual(p1_screen.x, p2_screen.x) || ! isCurved)
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

    // Draw control points
    g.setColour(juce::Colours::yellow);
    for (int i = 0; i < activeLfoData->points.size(); ++i)
    {
        auto localPoint = fromNormalized(activeLfoData->points[i]);

        // Determine point's appearance based on its state
        float currentPointRadius = pointRadius; // Use the base radius
        juce::Colour currentPointColour = juce::Colours::yellow;

        if (i == hoveredPointIndex)
        {
            // State: Hovered
            currentPointRadius = pointRadius * 1.5f; // Make it 50% larger
            currentPointColour = juce::Colours::yellow.withAlpha(0.5f);
        }

        if (i == draggingPointIndex)
        {
            // State: Being Dragged (overrides hover)
            currentPointRadius = pointRadius * 1.5f;
            currentPointColour = juce::Colours::yellow.withAlpha(0.8f);
        }

        g.setColour(currentPointColour);
        g.fillEllipse(localPoint.x - currentPointRadius,
                      localPoint.y - currentPointRadius,
                      currentPointRadius * 2,
                      currentPointRadius * 2);
    }

    // Draw playhead
    if (playheadPos >= 0.0f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawVerticalLine(juce::roundToInt(getWidth() * playheadPos), 0.0f, (float) getHeight());
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

void LfoEditor::mouseDown(const juce::MouseEvent& event)
{
    if (activeLfoData == nullptr)
        return; // Safety check

    switch (currentMode)
    {
        case LfoEditMode::PointEdit:
        {
            if (event.mods.isLeftButtonDown())
            {
                draggingPointIndex = -1;
                for (size_t i = 0; i < activeLfoData->points.size(); ++i)
                {
                    if (fromNormalized(activeLfoData->points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
                    {
                        draggingPointIndex = static_cast<int>(i);
                        return;
                    }
                }

                if (activeLfoData->points.size() < maxPoints)
                {
                    addPoint(toNormalized(event.getPosition()));
                    auto it = std::find(activeLfoData->points.begin(), activeLfoData->points.end(), toNormalized(event.getPosition()));
                    if (it != activeLfoData->points.end())
                        draggingPointIndex = static_cast<int>(std::distance(activeLfoData->points.begin(), it));
                }
            }
            else if (event.mods.isRightButtonDown())
            {
                editingCurveIndex = -1;
                for (int i = (int) activeLfoData->points.size() - 2; i >= 0; --i)
                {
                    auto p1 = fromNormalized(activeLfoData->points[i]);
                    auto p2 = fromNormalized(activeLfoData->points[i + 1]);
                    auto segmentBounds = juce::Rectangle<float>(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p1.x - p2.x), std::abs(p1.y - p2.y));
                    segmentBounds = segmentBounds.expanded(0, 15.0f);
                    if (segmentBounds.contains(event.position.toFloat()))
                    {
                        editingCurveIndex = i;
                        break;
                    }
                }
            }
        }
        case LfoEditMode::BrushPaint:
        {
            // Action for brush mode happens on mouseUp to feel like a "stamp"
            break;
        }
    }
}

void LfoEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (activeLfoData == nullptr || currentMode != LfoEditMode::PointEdit)
        return;

    if (event.mods.isLeftButtonDown() && draggingPointIndex != -1)
    {
        auto currentPos = event.getPosition();
        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            float snappedX = std::round((float) currentPos.x * (float) hGridDivs / (float) getWidth()) * ((float) getWidth() / (float) hGridDivs);
            float snappedY = std::round((float) currentPos.y * (float) vGridDivs / (float) getHeight()) * ((float) getHeight() / (float) vGridDivs);
            currentPos = { juce::roundToInt(snappedX), juce::roundToInt(snappedY) };
        }

        // ================== CORE MODIFICATION START ==================

        // Check if the point being dragged is the first or the last one.
        bool isFirstPoint = (draggingPointIndex == 0);
        bool isLastPoint = (draggingPointIndex == activeLfoData->points.size() - 1);

        if (isFirstPoint)
        {
            // For the first point, lock its X coordinate to the far left (0.0f).
            currentPos.setX(0.0f);
        }
        else if (isLastPoint)
        {
            // For the last point, lock its X coordinate to the far right.
            currentPos.setX((float) getWidth());
        }
        else
        {
            // For all middle points, use the original logic to constrain
            // their horizontal movement between their neighbors.
            float minX = fromNormalized(activeLfoData->points[draggingPointIndex - 1]).x;
            float maxX = fromNormalized(activeLfoData->points[draggingPointIndex + 1]).x;
            currentPos.setX(juce::jlimit(minX, maxX, (float) currentPos.x));
        }

        // =================== CORE MODIFICATION END ===================

        // The rest of the function remains the same.
        // It correctly constrains the final position within the bounds and updates the data.
        auto constrainedPos = getLocalBounds().getConstrainedPoint(currentPos);
        activeLfoData->points[draggingPointIndex] = toNormalized(constrainedPos);

        repaint();
    }
    else if (event.mods.isRightButtonDown() && editingCurveIndex != -1)
    {
        // Curve editing logic is unchanged.
        auto p1_screen = fromNormalized(activeLfoData->points[editingCurveIndex]);
        auto p2_screen = fromNormalized(activeLfoData->points[editingCurveIndex + 1]);
        float dx = p2_screen.x - p1_screen.x;
        float dy = p2_screen.y - p1_screen.y;
        float slope = (dx != 0.0f) ? (dy / dx) : std::numeric_limits<float>::infinity();
        float dragDistY = event.getDistanceFromDragStartY();
        const float divisor = 100.0f;
        const float sensitivity = 1.0f;
        float rawCurv = (slope < 0.0f ? dragDistY / divisor : -dragDistY / divisor) * sensitivity;
        activeLfoData->curvatures[editingCurveIndex] = juce::jlimit(-2.0f, 2.0f, rawCurv);
        repaint();
    }
}

void LfoEditor::mouseUp(const juce::MouseEvent& event)
{
    if (activeLfoData == nullptr || activeLfoData->points.empty())
        return;

    switch (currentMode)
    {
        case LfoEditMode::PointEdit:
        {
            activeLfoData->points.front().x = 0.0f;
            activeLfoData->points.back().x = 1.0f;
            if (draggingPointIndex != -1 || editingCurveIndex != -1)
            {
                updateAndSortPoints(); // Final sort after dragging
            }
            draggingPointIndex = -1;
            editingCurveIndex = -1;
            break;
        }
        case LfoEditMode::BrushPaint:
        {
            if (event.mouseWasClicked()) // Only apply on a click, not a drag
            {
                applyBrushShape(event.getPosition());
            }
            break;
        }
    }
    repaint();
}

void LfoEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (activeLfoData == nullptr)
        return;

    if (event.mods.isRightButtonDown())
    {
        int curveToReset = -1;
        for (int i = (int) activeLfoData->points.size() - 2; i >= 0; --i)
        {
            auto p1 = fromNormalized(activeLfoData->points[i]);
            auto p2 = fromNormalized(activeLfoData->points[i + 1]);
            auto segmentBounds = juce::Rectangle<float>(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p1.x - p2.x), std::abs(p1.y - p2.y));
            if (segmentBounds.expanded(0, 15.0f).contains(event.position.toFloat()))
            {
                curveToReset = i;
                break;
            }
        }
        if (curveToReset != -1)
        {
            activeLfoData->curvatures[curveToReset] = 0.0f;
            repaint();
        }
        return;
    }

    if (activeLfoData->points.size() <= 2)
        return;
    for (size_t i = activeLfoData->points.size() - 1; i > 0; --i)
    {
        if (fromNormalized(activeLfoData->points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
        {
            removePoint(static_cast<int>(i));
            return;
        }
    }
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

    // English: Add the point and sort the list to find its correct position.
    activeLfoData->points.push_back(newPoint);
    updateAndSortPoints();

    // English: Find the index of the point we just added.
    auto it = std::find_if(activeLfoData->points.begin(), activeLfoData->points.end(), [&](const auto& p)
                           { return juce::approximatelyEqual(p.x, newPoint.x) && juce::approximatelyEqual(p.y, newPoint.y); });

    if (it != activeLfoData->points.end())
    {
        int insertedAtIndex = (int) std::distance(activeLfoData->points.begin(), it);

        // English: A new point splits a segment, so we need one more curvature value.
        // We insert a new 0.0f (linear) curvature for the new segment being created.
        // All other curvatures are preserved.
        if (insertedAtIndex > 0)
        {
            activeLfoData->curvatures.insert(activeLfoData->curvatures.begin() + insertedAtIndex - 1, 0.0f);
        }
        else
        {
            // English: This should only happen if adding a point before the second point, very rare.
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

    // English: Removing a point merges two segments. We must remove one curvature value.
    // We remove the curvature of the first of the two merged segments.
    activeLfoData->curvatures.erase(activeLfoData->curvatures.begin() + index - 1);

    // English: We then set the curvature of the new, merged segment to 0.0 (linear).
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
    // English: Use stable_sort to preserve the order of points with the same x-coordinate.
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

    // English: 1. Check if a point already exists at (or very close to) the target X.
    for (size_t i = 0; i < activeLfoData->points.size(); ++i)
    {
        if (juce::approximatelyEqual(activeLfoData->points[i].x, targetX))
            return static_cast<int>(i);
    }

    // English: 2. If not, find which segment the targetX falls into.
    for (size_t i = 0; i < activeLfoData->points.size() - 1; ++i)
    {
        auto& p1 = activeLfoData->points[i];
        auto& p2 = activeLfoData->points[i + 1];

        if (targetX > p1.x && targetX < p2.x)
        {
            // English: 3. Calculate the Y value on the segment at targetX.
            // For simplicity, this uses linear interpolation. A more advanced version could calculate the curved position.
            float segmentWidth = p2.x - p1.x;
            float t = (segmentWidth > 0) ? (targetX - p1.x) / segmentWidth : 0.0f;
            float newY = p1.y + (p2.y - p1.y) * t;

            // English: 4. Create and add the new point.
            addPoint({ targetX, newY });

            // English: 5. After adding, the points are re-sorted. We need to find the new point's index again.
            for (size_t j = 0; j < activeLfoData->points.size(); ++j)
            {
                if (juce::approximatelyEqual(activeLfoData->points[j].x, targetX))
                    return static_cast<int>(j);
            }
        }
    }
    return -1; // English: Should not happen on a valid curve.
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
