/*
  ==============================================================================

    GraphTemplate.cpp
    Created: 5 Oct 2021 11:42:57am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "GraphTemplate.h"

#include <utility>

//==============================================================================
GraphTemplate::GraphTemplate()
{
    setOpaque(false);
}

GraphTemplate::~GraphTemplate()
{
    for (auto* ancestor : visibilityAncestors)
        if (ancestor != nullptr)
            ancestor->removeComponentListener(this);
}

void GraphTemplate::paint(juce::Graphics& g)
{
    const auto displayScale = juce::jmax(
        0.25f,
        g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto logicalBounds = getLocalBounds();
    const auto expectedWidth = logicalBounds.getWidth() > 0
                                   ? juce::jmax(
                                         1,
                                         juce::roundToInt(
                                             static_cast<float>(logicalBounds.getWidth()) * displayScale))
                                   : 0;
    const auto expectedHeight = logicalBounds.getHeight() > 0
                                    ? juce::jmax(
                                          1,
                                          juce::roundToInt(
                                              static_cast<float>(logicalBounds.getHeight()) * displayScale))
                                    : 0;

    if (! staticLayer.isValid()
        || staticLayer.getWidth() != expectedWidth
        || staticLayer.getHeight() != expectedHeight
        || staticLayerBounds != logicalBounds
        || ! juce::approximatelyEqual(staticLayerScale, displayScale))
        rebuildStaticLayer(displayScale);

    if (staticLayer.isValid())
        g.drawImage(staticLayer, logicalBounds.toFloat());

    if (isMouseOn || mZoomState)
    {
        const auto accent = getGraphAccent();
        auto outline = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(accent.withAlpha(mZoomState ? 0.78f : 0.36f));
        g.drawRoundedRectangle(outline, fire::ui::Metrics::radius, mZoomState ? 1.5f : 1.0f);
    }
}

void GraphTemplate::resized()
{
    staticLayer = {};
    staticLayerBounds = {};
    staticLayerScale = 0.0f;
}

void GraphTemplate::setScale(float newScale)
{
    newScale = juce::jmax(0.25f, newScale);
    if (juce::approximatelyEqual(scale, newScale))
        return;

    scale = newScale;
    resized();
    repaint();
}

float GraphTemplate::getScale() const noexcept
{
    return scale;
}

bool GraphTemplate::getZoomState() const noexcept
{
    return mZoomState;
}

void GraphTemplate::setZoomState(bool zoomState)
{
    if (mZoomState == zoomState)
        return;

    mZoomState = zoomState;
    repaint();
}

void GraphTemplate::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    mZoomState = ! mZoomState;
    repaint();
}

void GraphTemplate::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isMouseOn = true;
    repaint();
}

void GraphTemplate::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isMouseOn = false;
    repaint();
}

void GraphTemplate::visibilityChanged()
{
    updateShowingState();
    if (isVisible())
        repaint();
}

void GraphTemplate::parentHierarchyChanged()
{
    rebuildVisibilityObservers();
    updateShowingState();
}

void GraphTemplate::setGraphIdentity(juce::String title, fire::ui::ModuleRole role)
{
    graphTitle = std::move(title);
    graphRole = role;
    staticLayer = {};
    staticLayerBounds = {};
    staticLayerScale = 0.0f;
}

juce::Rectangle<float> GraphTemplate::getGraphHeaderBounds() const noexcept
{
    auto bounds = getLocalBounds().toFloat().reduced(5.0f * scale);
    const auto headerHeight = juce::jlimit(14.0f * scale,
                                          23.0f * scale,
                                          bounds.getHeight() * 0.20f);
    return bounds.removeFromTop(headerHeight);
}

juce::Rectangle<float> GraphTemplate::getGraphPlotBounds() const noexcept
{
    auto bounds = getLocalBounds().toFloat().reduced(5.0f * scale);
    bounds.removeFromTop(getGraphHeaderBounds().getHeight());
    return bounds.reduced(4.0f * scale, 3.0f * scale);
}

juce::Colour GraphTemplate::getGraphAccent() const noexcept
{
    return fire::ui::colourForRole(graphRole);
}

void GraphTemplate::graphShowingStateChanged(bool isNowShowing)
{
    juce::ignoreUnused(isNowShowing);
}

void GraphTemplate::rebuildVisibilityObservers()
{
    for (auto* ancestor : visibilityAncestors)
        if (ancestor != nullptr)
            ancestor->removeComponentListener(this);

    visibilityAncestors.clearQuick();
    for (auto* ancestor = getParentComponent(); ancestor != nullptr;
         ancestor = ancestor->getParentComponent())
    {
        visibilityAncestors.add(ancestor);
        ancestor->addComponentListener(this);
    }
}

void GraphTemplate::updateShowingState()
{
    const bool nowShowing = isShowing();
    if (lastKnownShowingState == nowShowing)
        return;

    lastKnownShowingState = nowShowing;
    graphShowingStateChanged(nowShowing);
}

void GraphTemplate::componentVisibilityChanged(juce::Component& component)
{
    juce::ignoreUnused(component);
    updateShowingState();
}

void GraphTemplate::componentParentHierarchyChanged(juce::Component& component)
{
    juce::ignoreUnused(component);
    rebuildVisibilityObservers();
    updateShowingState();
}

void GraphTemplate::componentBeingDeleted(juce::Component& component)
{
    visibilityAncestors.removeFirstMatchingValue(&component);
}

void GraphTemplate::rebuildStaticLayer(float displayScale)
{
    if (getWidth() <= 0 || getHeight() <= 0)
    {
        staticLayer = {};
        staticLayerBounds = {};
        staticLayerScale = 0.0f;
        return;
    }

    displayScale = juce::jmax(0.25f, displayScale);
    staticLayerBounds = getLocalBounds();
    staticLayerScale = displayScale;
    staticLayer = juce::Image(juce::Image::ARGB,
                              juce::jmax(1, juce::roundToInt(getWidth() * displayScale)),
                              juce::jmax(1, juce::roundToInt(getHeight() * displayScale)),
                              true);

    juce::Graphics cacheGraphics(staticLayer);
    cacheGraphics.addTransform(juce::AffineTransform::scale(displayScale));

    const auto accent = getGraphAccent();
    const auto bounds = getLocalBounds().toFloat();
    fire::ui::drawPanel(cacheGraphics, bounds, accent, true);

    auto plotBounds = getGraphPlotBounds();
    cacheGraphics.setColour(fire::ui::colours::canvas.withAlpha(0.50f));
    cacheGraphics.fillRoundedRectangle(plotBounds, fire::ui::Metrics::radiusSmall);
    fire::ui::drawTechGrid(cacheGraphics,
                           plotBounds,
                           juce::jmax(12.0f, 20.0f * scale),
                           0.16f);

    cacheGraphics.setColour(fire::ui::colours::hairline.withAlpha(0.48f));
    cacheGraphics.drawHorizontalLine(juce::roundToInt(plotBounds.getCentreY()),
                                     plotBounds.getX(),
                                     plotBounds.getRight());
    cacheGraphics.drawVerticalLine(juce::roundToInt(plotBounds.getCentreX()),
                                   plotBounds.getY(),
                                   plotBounds.getBottom());

    auto headerBounds = getGraphHeaderBounds();
    fire::ui::drawSectionTitle(cacheGraphics, headerBounds, graphTitle, accent);
}
