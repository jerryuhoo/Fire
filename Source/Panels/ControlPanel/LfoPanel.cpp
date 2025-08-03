#include "LfoPanel.h"

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
        g.drawVerticalLine(juce::roundToInt(getWidth() * i / (float)hGridDivs), 0.0f, getHeight());
    for (int i = 1; i < vGridDivs; ++i)
        g.drawHorizontalLine(juce::roundToInt(getHeight() * i / (float)vGridDivs), 0.0f, getWidth());

    // Defensive check: ensure data is valid before drawing.
    if (activeLfoData == nullptr || activeLfoData->points.size() < 2)
        return;

    // Draw LFO Path
    juce::Path lfoPath;
    lfoPath.startNewSubPath(fromNormalized(activeLfoData->points.front()));

    for (size_t i = 0; i < activeLfoData->points.size() - 1; ++i)
    {
        auto p1_screen = fromNormalized(activeLfoData->points[i]);
        auto p2_screen = fromNormalized(activeLfoData->points[i+1]);
        
        // Safety check to prevent array out of bounds.
        bool isCurved = (i < activeLfoData->curvatures.size() && !juce::approximatelyEqual(activeLfoData->curvatures[i], 0.0f));

        if (juce::approximatelyEqual(p1_screen.x, p2_screen.x) || !isCurved)
        {
            lfoPath.lineTo(p2_screen);
        }
        else
        {
            const int numSegments = 30;
            const float curvature = activeLfoData->curvatures[i];
            
            for (int j = 1; j <= numSegments; ++j)
            {
                float tx = (float)j / (float)numSegments;
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
    for (const auto& point : activeLfoData->points)
    {
        auto localPoint = fromNormalized(point);
        g.fillEllipse(localPoint.x - pointRadius, localPoint.y - pointRadius, pointRadius * 2, pointRadius * 2);
    }
    
    // Draw playhead
    if (playheadPos >= 0.0f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawVerticalLine(juce::roundToInt(getWidth() * playheadPos), 0.0f, (float)getHeight());
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
//    if (! juce::approximatelyEqual(playheadPos, position))
//    {
//        playheadPos = position;
//        repaint();
//    }
}

void LfoEditor::mouseDown(const juce::MouseEvent& event)
{
    if (activeLfoData == nullptr) return; // Safety check

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
        for (int i = (int)activeLfoData->points.size() - 2; i >= 0; --i)
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

void LfoEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (activeLfoData == nullptr) return; // Safety check
    
    if (event.mods.isLeftButtonDown() && draggingPointIndex != -1)
    {
        auto currentPos = event.getPosition();
        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            float snappedX = std::round((float)currentPos.x * (float)hGridDivs / (float)getWidth()) * ((float)getWidth() / (float)hGridDivs);
            float snappedY = std::round((float)currentPos.y * (float)vGridDivs / (float)getHeight()) * ((float)getHeight() / (float)vGridDivs);
            currentPos = { juce::roundToInt(snappedX), juce::roundToInt(snappedY) };
        }
        
        float minX = 0.0f;
        float maxX = (float)getWidth();
        if (draggingPointIndex > 0)
            minX = fromNormalized(activeLfoData->points[draggingPointIndex - 1]).x;
        if (draggingPointIndex < activeLfoData->points.size() - 1)
            maxX = fromNormalized(activeLfoData->points[draggingPointIndex + 1]).x;
        
        currentPos.setX(juce::jlimit(minX, maxX, (float)currentPos.x));
        auto constrainedPos = getLocalBounds().getConstrainedPoint(currentPos);
        activeLfoData->points[draggingPointIndex] = toNormalized(constrainedPos);
        
        repaint();
    }
    else if (event.mods.isRightButtonDown() && editingCurveIndex != -1)
    {
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
    if (activeLfoData == nullptr || activeLfoData->points.empty()) return;

    activeLfoData->points.front().x = 0.0f;
    activeLfoData->points.back().x = 1.0f;
    
    draggingPointIndex = -1;
    editingCurveIndex = -1;
    repaint();
}

void LfoEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (activeLfoData == nullptr) return;
    
    if (event.mods.isRightButtonDown())
    {
        int curveToReset = -1;
        for (int i = (int)activeLfoData->points.size() - 2; i >= 0; --i)
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
    
    if (activeLfoData->points.size() <= 2) return;
    for (size_t i = activeLfoData->points.size() - 1; i > 0; --i)
    {
        if (fromNormalized(activeLfoData->points[i]).getDistanceFrom(event.position.toFloat()) < pointRadius)
        {
            removePoint(static_cast<int>(i));
            return;
        }
    }
}

void LfoEditor::addPoint(juce::Point<float> newPoint)
{
    if (activeLfoData == nullptr || activeLfoData->points.size() >= maxPoints) return;
    
    activeLfoData->points.push_back(newPoint);
    updateAndSortPoints();
    
    auto it = std::find(activeLfoData->points.begin(), activeLfoData->points.end(), newPoint);
    int insertIndex = static_cast<int>(std::distance(activeLfoData->points.begin(), it));
    
    if (insertIndex > 0)
    {
        activeLfoData->curvatures.insert(activeLfoData->curvatures.begin() + insertIndex - 1, 0.0f);
        if (insertIndex < activeLfoData->curvatures.size())
            activeLfoData->curvatures[insertIndex] = 0.0f;
    }
    else
    {
        activeLfoData->curvatures.insert(activeLfoData->curvatures.begin(), 0.0f);
    }
    repaint();
}

void LfoEditor::removePoint(int index)
{
    if (activeLfoData == nullptr || index <= 0 || index >= activeLfoData->points.size() - 1) return;
    
    activeLfoData->points.erase(activeLfoData->points.begin() + index);
    
    activeLfoData->curvatures.erase(activeLfoData->curvatures.begin() + index - 1);
    if (index - 1 < activeLfoData->curvatures.size())
        activeLfoData->curvatures[index - 1] = 0.0f;

    repaint();
}

juce::Point<float> LfoEditor::toNormalized(juce::Point<int> localPoint)
{
    return { (float)localPoint.x / (float)getWidth(), (float)localPoint.y / (float)getHeight() };
}

juce::Point<float> LfoEditor::fromNormalized(juce::Point<float> normalizedPoint)
{
    // Ensure the calculation is done with floats and returns a float point
    return { normalizedPoint.x * (float)getWidth(), normalizedPoint.y * (float)getHeight() };
}

void LfoEditor::updateAndSortPoints()
{
    if (activeLfoData == nullptr) return;
    std::sort(activeLfoData->points.begin(), activeLfoData->points.end(), [](const auto& a, const auto& b) {
        return a.x < b.x;
    });
}

//==============================================================================
// LfoPanel Implementation
//==============================================================================
LfoPanel::LfoPanel(FireAudioProcessor& p) : processor(p)
{
    // First, create and initialize all data models.
    lfoData.resize(4);

    // Then, set the editor to point to the first LFO's data.
    lfoEditor.setDataToDisplay(&lfoData[currentLfoIndex]);
    addAndMakeVisible(lfoEditor);
    
    // Create UI Components
    for (int i = 0; i < 4; ++i)
    {
        lfoSelectButtons[i] = std::make_unique<juce::TextButton>("LFO " + juce::String(i + 1));
        addAndMakeVisible(lfoSelectButtons[i].get());
        lfoSelectButtons[i]->setRadioGroupId(1);
        lfoSelectButtons[i]->addListener(this);
    }
    lfoSelectButtons[0]->setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(parameterMenu);
    parameterMenu.addItem("Drive (Band 1)", 1);
    parameterMenu.addItem("Mix (Band 1)", 2);
    parameterMenu.setSelectedId(1);
    
    addAndMakeVisible(syncButton);
    syncButton.setButtonText("BPM Sync");
    
    addAndMakeVisible(rateSlider);
    rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    rateSlider.setRange(0.1, 20.0, 0.01);
    
    addAndMakeVisible(rateLabel);
    rateLabel.setText("Rate", juce::dontSendNotification);
    rateLabel.attachToComponent(&rateSlider, false);
    
    addAndMakeVisible(gridXSlider);
    gridXSlider.setSliderStyle(juce::Slider::IncDecButtons);
    gridXSlider.setRange(2, 16, 1);
    gridXSlider.setValue(4);
    gridXSlider.addListener(this);
    
    addAndMakeVisible(gridXLabel);
    gridXLabel.setText("Grid X", juce::dontSendNotification);
    
    addAndMakeVisible(gridYSlider);
    gridYSlider.setSliderStyle(juce::Slider::IncDecButtons);
    gridYSlider.setRange(2, 16, 1);
    gridYSlider.setValue(4);
    gridYSlider.addListener(this);

    addAndMakeVisible(gridYLabel);
    gridYLabel.setText("Grid Y", juce::dontSendNotification);

    startTimerHz(60);
}

LfoPanel::~LfoPanel() { stopTimer(); }

void LfoPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff303030));
    g.drawRect(getLocalBounds(), 1);
}

void LfoPanel::resized()
{
    juce::Rectangle<int> bounds = getLocalBounds();
    bounds.reduce(10, 10);
    
    auto leftColumn = bounds.removeFromLeft(60);
    auto rightColumn = bounds.removeFromRight(120);
    auto topRow = bounds.removeFromTop(30);
    
    juce::FlexBox lfoSelectBox;
    lfoSelectBox.flexDirection = juce::FlexBox::Direction::column;
    for (const auto& button : lfoSelectButtons)
        lfoSelectBox.items.add(juce::FlexItem(*button).withFlex(1.0f).withMargin(2));
    lfoSelectBox.performLayout(leftColumn);
    
    juce::FlexBox rightControlsBox;
    rightControlsBox.flexDirection = juce::FlexBox::Direction::column;
    rightControlsBox.items.add(juce::FlexItem(rateSlider).withFlex(3.0f));
    rightControlsBox.items.add(juce::FlexItem(gridXLabel).withFlex(0.5f));
    rightControlsBox.items.add(juce::FlexItem(gridXSlider).withFlex(1.0f));
    rightControlsBox.items.add(juce::FlexItem(gridYLabel).withFlex(0.5f));
    rightControlsBox.items.add(juce::FlexItem(gridYSlider).withFlex(1.0f));
    rightControlsBox.performLayout(rightColumn);
    
    syncButton.setBounds(topRow.removeFromRight(100));
    parameterMenu.setBounds(topRow);
    
    lfoEditor.setBounds(bounds);
}

void LfoPanel::timerCallback()
{
//    // This is the most robust way to handle potentially buggy hosts.
//    try
//    {
//        // 1. Safely get the playhead pointer.
//        if (auto* playHead = processor.getPlayHead())
//        {
//            // 2. Get the position information optional. This is the call that can crash.
//            auto positionInfoOpt = playHead->getPosition();
//
//            // 3. Check if the optional contains a valid object.
//            if (positionInfoOpt)
//            {
//                const auto& positionInfo = *positionInfoOpt;
//
//                if (positionInfo.getIsPlaying())
//                {
//                    if (auto ppq = positionInfo.getPpqPosition())
//                    {
//                        // This is a basic example. A real implementation would use the Rate slider and Sync button.
//                        double beatsPerBar = 4.0;
//                        double currentBeat = std::fmod(*ppq, beatsPerBar);
//                        float normalizedPosition = static_cast<float>(currentBeat / beatsPerBar);
//                        lfoEditor.setPlayheadPosition(normalizedPosition);
//                    }
//                    else
//                    {
//                        // Host is playing but not providing PPQ.
//                        lfoEditor.setPlayheadPosition(-1.0f);
//                    }
//                }
//                else
//                {
//                    // Host is not playing.
//                    lfoEditor.setPlayheadPosition(-1.0f);
//                }
//            }
//            else // positionInfoOpt is empty (juce::nullopt)
//            {
//                lfoEditor.setPlayheadPosition(-1.0f);
//            }
//        }
//        else // playHead is a nullptr
//        {
//            lfoEditor.setPlayheadPosition(-1.0f);
//        }
//    }
//    catch (...)
//    {
//        // If any part of the above code causes a severe error (like the host bug),
//        // this block will catch it and prevent the plugin UI from crashing.
//        // We can simply do nothing and wait for the next timer callback,
//        // hoping the host has sorted itself out.
//        lfoEditor.setPlayheadPosition(-1.0f);
//    }
}

void LfoPanel::buttonClicked(juce::Button* button)
{
    if (button == &syncButton)
    {
        // Handle BPM sync logic here
    }
    else
    {
        for (int i = 0; i < 4; ++i)
        {
            if (button == lfoSelectButtons[i].get())
            {
                currentLfoIndex = i;
                lfoEditor.setDataToDisplay(&lfoData[currentLfoIndex]);
                break;
            }
        }
    }
}

void LfoPanel::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &parameterMenu)
    {
        // Handle parameter mapping logic here
    }
}

void LfoPanel::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &gridXSlider || slider == &gridYSlider)
        lfoEditor.setGridDivisions((int)gridXSlider.getValue(), (int)gridYSlider.getValue());
}
