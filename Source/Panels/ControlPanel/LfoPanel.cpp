#include "LfoPanel.h"
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
    return { (float)localPoint.x / (float)getWidth(), 1.0f - ((float)localPoint.y / (float)getHeight()) };
}

juce::Point<float> LfoEditor::fromNormalized(juce::Point<float> normalizedPoint)
{
    // Ensure the calculation is done with floats and returns a float point
    return { normalizedPoint.x * (float)getWidth(), (1.0f - normalizedPoint.y) * (float)getHeight() };
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
    lfoEditor.setDataToDisplay(&processor.lfoData[currentLfoIndex]);
    addAndMakeVisible(lfoEditor);
    
    // Create UI Components
    for (int i = 0; i < 4; ++i)
    {
        lfoSelectButtons[i] = std::make_unique<juce::TextButton>("LFO " + juce::String(i + 1));
        addAndMakeVisible(lfoSelectButtons[i].get());
        lfoSelectButtons[i]->setRadioGroupId(1);
        lfoSelectButtons[i]->addListener(this);
        lfoSelectButtons[i]->setLookAndFeel(&flatLnf);
        lfoSelectButtons[i]->setColour(juce::TextButton::buttonColourId, COLOUR7);
        lfoSelectButtons[i]->setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
        lfoSelectButtons[i]->setColour(juce::ComboBox::outlineColourId, COLOUR6);
        lfoSelectButtons[i]->setColour(juce::TextButton::textColourOnId, COLOUR1);
        lfoSelectButtons[i]->setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
        lfoSelectButtons[i]->setToggleState(false, juce::dontSendNotification);
    }
    lfoSelectButtons[0]->setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(matrixButton);
    matrixButton.addListener(this);
    matrixButton.setLookAndFeel(&flatLnf);
    
    addAndMakeVisible(syncButton);
    syncButton.setClickingTogglesState(true);
    syncButton.setColour(juce::TextButton::buttonColourId, COLOUR7);
    syncButton.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    syncButton.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    syncButton.setColour(juce::TextButton::textColourOnId, COLOUR1);
    syncButton.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
    syncButton.setButtonText("BPM");
    syncButton.setLookAndFeel(&flatLnf);
    
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
    gridXSlider.setLookAndFeel(&flatLnf);
    
    addAndMakeVisible(gridXLabel);
    gridXLabel.setText("Grid X", juce::dontSendNotification);
    
    addAndMakeVisible(gridYSlider);
    gridYSlider.setSliderStyle(juce::Slider::IncDecButtons);
    gridYSlider.setRange(2, 16, 1);
    gridYSlider.setValue(4);
    gridYSlider.addListener(this);
    gridYSlider.setLookAndFeel(&flatLnf);

    addAndMakeVisible(gridYLabel);
    gridYLabel.setText("Grid Y", juce::dontSendNotification);

    // ADD THIS: Register as a parameter listener for each of the LFO sync mode parameters.
    for (int i = 0; i < 4; ++i)
    {
        // Use the ParameterID helper function to get the correct ID.
        // The loop now correctly runs from 0 to 3 to match the array indices.
        processor.treeState.addParameterListener(ParameterID::lfoSyncMode(i).getParamID(), this);
    }
    
    // --- ADD Attachments ---
    // Attach the sync button to the first LFO's sync mode parameter
    syncButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.treeState, LFO_SYNC_MODE_ID1, syncButton
    );
    
    // Set up the rate slider based on the initial state
    updateRateSlider();

    startTimerHz(60);
}

LfoPanel::~LfoPanel()
{
    stopTimer();
    syncButton.setLookAndFeel(nullptr);
    gridXSlider.setLookAndFeel(nullptr);
    gridYSlider.setLookAndFeel(nullptr);
    matrixButton.setLookAndFeel(nullptr);
    for (auto& button : lfoSelectButtons)
        button->setLookAndFeel(nullptr);

    for (int i = 0; i < 4; ++i)
    {
        // Use the same helper function to ensure we remove the correct listener.
        processor.treeState.removeParameterListener(ParameterID::lfoSyncMode(i).getParamID(), this);
    }
}

void LfoPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff303030));
    g.drawRect(getLocalBounds(), 1);
}

void LfoPanel::resized()
{
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
    auto topRow = bounds.removeFromTop(juce::roundToInt(initialTopRowHeight * scale));
    
    // Layout the top row with scaled dimensions.
    {
        auto matrixButtonArea = topRow.removeFromLeft(juce::roundToInt(initialMatrixWidth * scale));
        matrixButton.setBounds(matrixButtonArea.reduced(0, juce::roundToInt(4 * scale)));

        auto syncButtonArea = topRow.removeFromRight(juce::roundToInt(initialSyncWidth * scale));
        syncButton.setBounds(syncButtonArea.reduced(juce::roundToInt(10 * scale), juce::roundToInt(4 * scale)));

        auto gridControlsArea = topRow;
        auto gridXArea = gridControlsArea.removeFromLeft(gridControlsArea.getWidth() / 2);
        auto gridYArea = gridControlsArea;
        
        gridXLabel.setBounds(gridXArea.removeFromLeft(juce::roundToInt(initialGridLabelWidth * scale)).reduced(juce::roundToInt(2 * scale)));
        gridXSlider.setBounds(gridXArea);
        
        gridYLabel.setBounds(gridYArea.removeFromLeft(juce::roundToInt(initialGridLabelWidth * scale)).reduced(juce::roundToInt(2 * scale)));
        gridYSlider.setBounds(gridYArea);
    }

    // Layout for the main columns using FlexBox (FlexBox handles scaling internally).
    juce::FlexBox lfoSelectBox;
    lfoSelectBox.flexDirection = juce::FlexBox::Direction::column;
    for (const auto& button : lfoSelectButtons)
        lfoSelectBox.items.add(juce::FlexItem(*button).withFlex(1.0f).withMargin(2));
    lfoSelectBox.performLayout(leftColumn);
    
    // Center the rate slider in the right column with a scaled size.
    const int sliderSize = juce::roundToInt(juce::jmin(rightColumn.getWidth(), rightColumn.getHeight()) * 0.8f); // 80% of the smallest dimension
    rateSlider.setBounds(rightColumn.withSizeKeepingCentre(sliderSize, sliderSize));
    
    // The LFO editor takes the remaining central space.
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
            lfoEditor.setDataToDisplay(&processor.lfoData[currentLfoIndex]);

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
            auto syncModeID = ParameterID::lfoSyncMode(clickedIndex);
            syncButtonAttachment.reset();
            syncButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.treeState, syncModeID.getParamID(), syncButton
            );

            updateRateSlider();
        }
    }
}

void LfoPanel::updateRateSlider() // Or void LfoPanel::updateRateControls()
{
    // --- Get Parameter IDs using the robust ParameterID namespace ---
    // The currentLfoIndex is 0-based, which matches our arrays perfectly.
    auto syncModeID = ParameterID::lfoSyncMode(currentLfoIndex);
    auto rateSyncID = ParameterID::lfoRateSync(currentLfoIndex);
    auto rateHzID   = ParameterID::lfoRateHz(currentLfoIndex);

    // Find out if the current LFO is in sync mode from the parameter value.
    // We use .getParamID() to get the string from the juce::ParameterID object.
    auto* param = processor.treeState.getParameter(syncModeID.getParamID());
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
            if (juce::isPositiveAndBelow(index, processor.lfoRateSyncDivisions.size()))
                return processor.lfoRateSyncDivisions[index];
            return juce::String();
        };
        
        // Disable text entry for stability.
        rateSlider.valueFromTextFunction = nullptr;

        // Create the attachment using the correct ParameterID.
        rateSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.treeState, rateSyncID.getParamID(), rateSlider
        );
    }
    else // --- HZ (FREE) MODE (Continuous) ---
    {
        // Revert to default behavior.
        rateSlider.textFromValueFunction = nullptr;
        rateSlider.valueFromTextFunction = nullptr;

        // Create the attachment using the correct ParameterID.
        rateSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.treeState, rateHzID.getParamID(), rateSlider
        );
    }
}


// ADD THIS NEW FUNCTION
void LfoPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Get the authoritative ParameterID for the currently active LFO's sync mode.
    auto currentSyncModeID = ParameterID::lfoSyncMode(currentLfoIndex);

    // Compare the incoming parameterID string with our authoritative one.
    if (parameterID == currentSyncModeID.getParamID())
    {
        // If they match, it means the sync mode for the visible LFO has changed
        // (likely via automation or a preset load), so we must update the UI.
        updateRateSlider();
    }
}

void LfoPanel::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &gridXSlider || slider == &gridYSlider)
        lfoEditor.setGridDivisions((int)gridXSlider.getValue(), (int)gridYSlider.getValue());
}

void LfoPanel::setScale(float newScale)
{
    scale = newScale;
//    flatLnf.scale = newScale; // Pass the scale to the LookAndFeel if it needs it.
}
