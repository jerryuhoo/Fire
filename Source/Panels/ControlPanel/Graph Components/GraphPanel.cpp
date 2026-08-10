/*
  ==============================================================================

    GraphPanel.cpp
    Created: 30 Aug 2021 11:52:16pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "GraphPanel.h"

//==============================================================================
GraphPanel::GraphPanel(FireAudioProcessor& p) : processor(p)
{
    // Visualiser
    addAndMakeVisible(oscilloscope);

    // VU Meters
    addAndMakeVisible(vuPanel);

    // Distortion Graph
    addAndMakeVisible(distortionGraph);

    // Width Graph
    addAndMakeVisible(widthGraph);
    setInterceptsMouseClicks(false, true);
    setOpaque(false);
}

GraphPanel::~GraphPanel()
{
}

void GraphPanel::setLayoutMode(LayoutMode newMode)
{
    if (currentLayoutMode != newMode)
    {
        currentLayoutMode = newMode;

        // Update visibility based on the new mode
        distortionGraph.setVisible(currentLayoutMode == LayoutMode::Band);
        oscilloscope.setVisible(true);
        vuPanel.setVisible(true);
        widthGraph.setVisible(true);

        // Trigger a call to resized() to apply the new layout
        resized();
    }
}

void GraphPanel::paint(juce::Graphics& g)
{
    fire::ui::drawCanvas(g, getLocalBounds().toFloat());
}

void GraphPanel::toggleZoom(GraphTemplate* viewToToggle)
{
    // If we clicked the currently zoomed view, un-zoom it.
    if (viewToToggle != nullptr && viewToToggle == zoomedView)
    {
        zoomedView->setZoomState(false);
        zoomedView = nullptr;
    }
    // Otherwise, zoom the new view (and un-zoom any previous one).
    else
    {
        if (zoomedView)
            zoomedView->setZoomState(false);

        zoomedView = viewToToggle;

        if (zoomedView)
            zoomedView->setZoomState(true);
    }

    // Trigger a layout update.
    resized();
}

void GraphPanel::resized()
{
    constexpr int gap = 6;
    auto bounds = getLocalBounds().reduced(2);

    // === ZOOM LOGIC ===
    // If a view is zoomed, it takes up the entire panel.
    if (zoomedView != nullptr)
    {
        // Make only the zoomed view visible
        oscilloscope.setVisible(zoomedView == &oscilloscope);
        vuPanel.setVisible(zoomedView == &vuPanel);
        widthGraph.setVisible(zoomedView == &widthGraph);

        // Distortion graph is only available in Band mode
        distortionGraph.setVisible(zoomedView == &distortionGraph && currentLayoutMode == LayoutMode::Band);

        // Set its bounds to fill the panel
        zoomedView->setBounds(bounds);

        // IMPORTANT: Stop here so we don't apply the regular layout
        return;
    }

    // === REGULAR LAYOUT LOGIC ===
    // If nothing is zoomed, proceed with the normal layout.

    // First, ensure all standard views are visible again.
    oscilloscope.setVisible(true);
    vuPanel.setVisible(true);
    widthGraph.setVisible(true);
    distortionGraph.setVisible(currentLayoutMode == LayoutMode::Band);

    if (currentLayoutMode == LayoutMode::Band)
    {
        const auto halfWidth = juce::jmax(0, (bounds.getWidth() - gap) / 2);
        const auto halfHeight = juce::jmax(0, (bounds.getHeight() - gap) / 2);
        auto topRow = bounds.removeFromTop(halfHeight);

        oscilloscope.setBounds(topRow.removeFromLeft(halfWidth));
        topRow.removeFromLeft(gap);
        distortionGraph.setBounds(topRow);

        bounds.removeFromTop(gap);
        vuPanel.setBounds(bounds.removeFromLeft(halfWidth));
        bounds.removeFromLeft(gap);
        widthGraph.setBounds(bounds);
    }
    else // currentLayoutMode == LayoutMode::Global
    {
        // Vertical stack layout for Global view
        const auto thirdHeight = juce::jmax(0, (bounds.getHeight() - gap * 2) / 3);

        oscilloscope.setBounds(bounds.removeFromTop(thirdHeight));
        bounds.removeFromTop(gap);
        vuPanel.setBounds(bounds.removeFromTop(thirdHeight));
        bounds.removeFromTop(gap);
        widthGraph.setBounds(bounds);
    }
}

Oscilloscope* GraphPanel::getOscilloscope()
{
    return &oscilloscope;
}

VUPanel* GraphPanel::getVuPanel()
{
    return &vuPanel;
}

DistortionGraph* GraphPanel::getDistortionGraph()
{
    return &distortionGraph;
}

WidthGraph* GraphPanel::getWidthGraph()
{
    return &widthGraph;
}

void GraphPanel::setFocusBandNum(int num)
{
    vuPanel.setFocusBandNum(num);
}
