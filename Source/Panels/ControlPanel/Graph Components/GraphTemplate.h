/*
  ==============================================================================

    GraphTemplate.h
    Created: 5 Oct 2021 11:42:57am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../GUI/InterfaceDefines.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>

//==============================================================================
/*
*/
struct GraphTemplateInputTestAccess;

class GraphTemplate : public juce::Component,
                      private juce::ComponentListener
{
public:
    GraphTemplate();
    ~GraphTemplate() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setScale(float scale);
    float getScale() const noexcept;
    bool getZoomState() const noexcept;
    void setZoomState(bool zoomState);
    void setZoomRequestCallback(std::function<void()> callback);
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void visibilityChanged() override;
    void enablementChanged() override;
    void focusGained(FocusChangeType cause) override;
    void focusLost(FocusChangeType cause) override;
    void parentHierarchyChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    friend struct GraphTemplateInputTestAccess;
    float scale = 1.0f;
    juce::String graphTitle { "SIGNAL" };
    fire::ui::ModuleRole graphRole = fire::ui::ModuleRole::neutral;
    juce::Image staticLayer;
    juce::Rectangle<int> staticLayerBounds;
    float staticLayerScale = 0.0f;
    juce::Array<juce::Component*> visibilityAncestors;
    bool lastKnownShowingState = false;
    std::function<void()> onZoomRequested;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    bool primaryPointerDown = false;
    fire::ui::DampedValue hoverAnimation;
    fire::ui::DampedValue pressAnimation;
    fire::ui::DampedValue focusAnimation;
    fire::ui::DampedValue disabledAnimation;

    class AnimationTimer final : public juce::Timer
    {
    public:
        explicit AnimationTimer(GraphTemplate& ownerToUse) : owner(ownerToUse) {}

    private:
        void timerCallback() override { owner.animationTimerCallback(); }
        GraphTemplate& owner;
    };

    AnimationTimer animationTimer;

    void rebuildStaticLayer(float displayScale);
    void rebuildVisibilityObservers();
    void updateShowingState();
    void componentVisibilityChanged(juce::Component&) override;
    void componentParentHierarchyChanged(juce::Component&) override;
    void componentBeingDeleted(juce::Component&) override;
    bool isCompletePrimaryDown(const juce::MouseEvent&) const noexcept;
    bool isPointerSource(const juce::MouseEvent&) const noexcept;
    void recoverMissingPointerUp(const juce::MouseEvent&) noexcept;
    void dismissPointerGesture() noexcept;
    bool canRequestZoom() const noexcept;
    void requestZoom();
    void updateHoverState() noexcept;
    void updateAnimationTargets() noexcept;
    bool advanceAnimation(float deltaSeconds) noexcept;
    bool animationsSettled() const noexcept;
    void animationTimerCallback();

protected:
    void setGraphIdentity(juce::String title, fire::ui::ModuleRole role);
    juce::Rectangle<float> getGraphHeaderBounds() const noexcept;
    juce::Rectangle<float> getGraphPlotBounds() const noexcept;
    juce::Colour getGraphAccent() const noexcept;
    virtual void graphShowingStateChanged(bool isNowShowing);

    bool isMouseOn = false;
    bool mZoomState = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphTemplate)
};
