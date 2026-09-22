/*
  ==============================================================================

    GraphTemplate.cpp
    Created: 5 Oct 2021 11:42:57am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "GraphTemplate.h"
#include "../../../GUI/FireIcons.h"

#include <utility>

namespace
{
class GraphAccessibilityHandler final : public juce::AccessibilityHandler
{
public:
    GraphAccessibilityHandler(GraphTemplate& graphToUse,
                              juce::AccessibilityActions actions)
        : juce::AccessibilityHandler(graphToUse,
                                     juce::AccessibilityRole::button,
                                     std::move(actions)),
          graph(graphToUse)
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = juce::AccessibilityHandler::getCurrentState()
                         .withExpandable();
        return graph.getZoomState() ? state.withExpanded()
                                    : state.withCollapsed();
    }

private:
    GraphTemplate& graph;
};
} // namespace

//==============================================================================
GraphTemplate::GraphTemplate() : animationTimer(*this)
{
    setOpaque(false);
    setTitle(graphTitle + " graph");
    setHelpText("Activate to expand or restore this graph.");
}

GraphTemplate::~GraphTemplate()
{
    animationTimer.stopTimer();
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

    const auto hover = hoverAnimation.current;
    const auto press = pressAnimation.current;
    const auto focus = focusAnimation.current;
    const auto disabled = disabledAnimation.current;
    const auto emphasis = juce::jmax(mZoomState ? 1.0f : 0.0f,
                                     juce::jmax(hover * 0.52f, focus * 0.78f));
    if (emphasis > 0.001f)
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f + press * scale);
        const auto accent = getGraphAccent();
        g.setGradientFill(juce::ColourGradient(accent.withAlpha(emphasis * 0.055f * (1 - disabled * 0.72f)),
            bounds.getCentreX(), bounds.getY(), accent.withAlpha(0.0f), bounds.getCentreX(), bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, fire::ui::Metrics::radius);
        g.setColour(accent.withAlpha(emphasis * 0.25f * (1 - disabled * 0.72f)));
        g.drawRoundedRectangle(bounds, fire::ui::Metrics::radius, juce::jmax(0.8f, scale));
    }
    if (onZoomRequested)
    {
        auto header = getGraphHeaderBounds();
        const auto icon = header.removeFromRight(18 * scale).withSizeKeepingCentre(14 * scale, 14 * scale);
        fire::ui::drawIcon(g, mZoomState ? fire::ui::Icon::restore : fire::ui::Icon::expand,
                          icon, fire::ui::colours::textMuted.interpolatedWith(getGraphAccent(), emphasis)
                              .withAlpha((0.6f + emphasis * 0.35f) * (1 - disabled * 0.65f)));
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

    // Expanded/collapsed is exposed through getCurrentState(). JUCE has no
    // dedicated state-changed event, and changing the zoom also changes the
    // surrounding accessible layout, so a structure notification is the
    // closest matching public event.
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent(
            juce::AccessibilityEvent::structureChanged);
}

void GraphTemplate::setZoomRequestCallback(std::function<void()> callback)
{
    dismissPointerGesture();
    onZoomRequested = std::move(callback);
    setWantsKeyboardFocus(onZoomRequested != nullptr);
    updateHoverState();
    updateAnimationTargets();
}

void GraphTemplate::mouseDown(const juce::MouseEvent& e)
{
    if (onZoomRequested == nullptr || ! isEnabled() || ! isShowing()
        || ! isCompletePrimaryDown(e))
        return;

    if (primaryPointerDown)
    {
        if (! isPointerSource(e))
            return;
        dismissPointerGesture();
    }

    // JUCE may give the graph keyboard focus before dispatching mouseDown.
    // Preserve that focus for a subsequent Return/Space press, but do not
    // leave a keyboard-navigation outline behind after pointer interaction.
    keyboardFocusVisible = false;
    primaryPointerDown = true;
    pointerSourceType = e.source.getType();
    pointerSourceIndex = e.source.getIndex();
    updateAnimationTargets();
}

void GraphTemplate::mouseDrag(const juce::MouseEvent& e)
{
    if (! primaryPointerDown || ! isPointerSource(e))
        return;

    if (! isCompletePrimaryDown(e))
        dismissPointerGesture();
}

void GraphTemplate::mouseUp(const juce::MouseEvent& e)
{
    if (! primaryPointerDown || ! isPointerSource(e))
        return;

    const auto localPosition = e.getEventRelativeTo(this).position.roundToInt();
    const bool shouldActivate = isEnabled() && isShowing()
                                && getLocalBounds().contains(localPosition);
    dismissPointerGesture();
    if (! shouldActivate)
        return;

    // The owner may synchronously delete this graph. Invoke a retained copy as
    // the final operation and do not touch component state afterwards.
    requestZoom();
}

void GraphTemplate::mouseEnter(const juce::MouseEvent& e)
{
    recoverMissingPointerUp(e);
    updateHoverState();
}

void GraphTemplate::mouseMove(const juce::MouseEvent& e)
{
    recoverMissingPointerUp(e);
    updateHoverState();
}

void GraphTemplate::mouseExit(const juce::MouseEvent& e)
{
    recoverMissingPointerUp(e);
    updateHoverState();
}

bool GraphTemplate::keyPressed(const juce::KeyPress& key)
{
    if (hasKeyboardFocus(true) && ! keyboardFocusVisible)
    {
        keyboardFocusVisible = true;
        updateAnimationTargets();
    }

    const bool isActivationKey = key.isKeyCode(juce::KeyPress::returnKey)
                                 || key.isKeyCode(juce::KeyPress::spaceKey);
    if (isActivationKey && canRequestZoom())
    {
        // The owner may synchronously delete this graph. Keep activation as the
        // final operation, matching the owned pointer-release path.
        requestZoom();
        return true;
    }

    return juce::Component::keyPressed(key);
}

void GraphTemplate::visibilityChanged()
{
    updateShowingState();
    if (isVisible())
        repaint();
}

void GraphTemplate::enablementChanged()
{
    dismissPointerGesture();
    updateHoverState();
    updateAnimationTargets();
}

void GraphTemplate::focusGained(FocusChangeType cause)
{
    juce::Component::focusGained(cause);
    keyboardFocusVisible = cause != focusChangedByMouseClick;
    updateAnimationTargets();
}

void GraphTemplate::focusLost(FocusChangeType cause)
{
    juce::Component::focusLost(cause);
    keyboardFocusVisible = false;
    updateAnimationTargets();
}

void GraphTemplate::parentHierarchyChanged()
{
    rebuildVisibilityObservers();
    updateShowingState();
}

std::unique_ptr<juce::AccessibilityHandler>
GraphTemplate::createAccessibilityHandler()
{
    juce::AccessibilityActions actions;
    actions.addAction(
        juce::AccessibilityActionType::press,
        [safeThis = juce::Component::SafePointer<GraphTemplate>(this)]
        {
            if (safeThis != nullptr && safeThis->canRequestZoom())
                safeThis->requestZoom();
        });

    return std::make_unique<GraphAccessibilityHandler>(*this,
                                                       std::move(actions));
}

void GraphTemplate::setGraphIdentity(juce::String title, fire::ui::ModuleRole role)
{
    graphTitle = std::move(title);
    graphRole = role;
    setTitle(graphTitle + " graph");
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
    if (! nowShowing)
    {
        dismissPointerGesture();
        if (isMouseOn)
        {
            isMouseOn = false;
            repaint();
        }
    }
    updateAnimationTargets();
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

bool GraphTemplate::isCompletePrimaryDown(
    const juce::MouseEvent& event) const noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown()
        && ! event.mods.isPopupMenu();
}

bool GraphTemplate::isPointerSource(const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void GraphTemplate::recoverMissingPointerUp(
    const juce::MouseEvent& event) noexcept
{
    if (primaryPointerDown && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
        dismissPointerGesture();
}

void GraphTemplate::dismissPointerGesture() noexcept
{
    if (! primaryPointerDown && pointerSourceIndex == -1)
        return;

    primaryPointerDown = false;
    pointerSourceIndex = -1;
    updateAnimationTargets();
}

bool GraphTemplate::canRequestZoom() const noexcept
{
    return onZoomRequested != nullptr && isEnabled() && isShowing();
}

void GraphTemplate::requestZoom()
{
    auto callback = onZoomRequested;
    if (callback != nullptr)
        callback();
}

void GraphTemplate::updateHoverState() noexcept
{
    const bool shouldShowHover = onZoomRequested != nullptr
                                 && isEnabled() && isShowing()
                                 && isMouseOver(true);
    if (isMouseOn != shouldShowHover)
    {
        isMouseOn = shouldShowHover;
        updateAnimationTargets();
    }
}

void GraphTemplate::updateAnimationTargets() noexcept
{
    const bool interactive = onZoomRequested != nullptr && isShowing();
    if (! interactive)
    {
        animationTimer.stopTimer();
        keyboardFocusVisible = false;
        hoverAnimation.snapTo(0.0f);
        pressAnimation.snapTo(0.0f);
        focusAnimation.snapTo(0.0f);
        disabledAnimation.snapTo(0.0f);
        repaint();
        return;
    }

    const bool enabled = isEnabled();
    if (! enabled)
        keyboardFocusVisible = false;

    hoverAnimation.setTarget(enabled && isMouseOn ? 1.0f : 0.0f);
    pressAnimation.setTarget(enabled && primaryPointerDown ? 1.0f : 0.0f);
    focusAnimation.setTarget(enabled && keyboardFocusVisible
                                 && hasKeyboardFocus(true)
                             ? 1.0f
                             : 0.0f);
    disabledAnimation.setTarget(enabled ? 0.0f : 1.0f);
    if (! animationsSettled() && ! animationTimer.isTimerRunning())
        animationTimer.startTimerHz(60);
}

bool GraphTemplate::advanceAnimation(float deltaSeconds) noexcept
{
    auto changed = hoverAnimation.advance(deltaSeconds, fire::ui::Motion::hover * 0.5f);
    changed = pressAnimation.advance(deltaSeconds, fire::ui::Motion::press * 0.5f) || changed;
    changed = focusAnimation.advance(deltaSeconds, fire::ui::Motion::focus * 0.5f) || changed;
    changed = disabledAnimation.advance(deltaSeconds, fire::ui::Motion::disabled * 0.5f) || changed;
    return changed;
}

bool GraphTemplate::animationsSettled() const noexcept
{
    return hoverAnimation.isSettled() && pressAnimation.isSettled()
        && focusAnimation.isSettled() && disabledAnimation.isSettled();
}

void GraphTemplate::animationTimerCallback()
{
    if (onZoomRequested == nullptr || ! isShowing())
    {
        updateAnimationTargets();
        return;
    }

    updateAnimationTargets();
    if (advanceAnimation(1.0f / 60.0f))
        repaint();
    if (animationsSettled())
        animationTimer.stopTimer();
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
                           0.08f);

    cacheGraphics.setColour(fire::ui::colours::hairline.withAlpha(0.27f));
    cacheGraphics.drawHorizontalLine(juce::roundToInt(plotBounds.getCentreY()),
                                     plotBounds.getX(),
                                     plotBounds.getRight());
    cacheGraphics.drawVerticalLine(juce::roundToInt(plotBounds.getCentreX()),
                                   plotBounds.getY(),
                                   plotBounds.getBottom());

    auto headerBounds = getGraphHeaderBounds();
    headerBounds.removeFromRight(20 * scale);
    fire::ui::drawSectionTitle(cacheGraphics, headerBounds, graphTitle, accent);
}
