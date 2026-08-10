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

//==============================================================================
/*
*/
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
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    float scale = 1.0f;
    juce::String graphTitle { "SIGNAL" };
    fire::ui::ModuleRole graphRole = fire::ui::ModuleRole::neutral;
    juce::Image staticLayer;
    juce::Rectangle<int> staticLayerBounds;
    float staticLayerScale = 0.0f;
    juce::Array<juce::Component*> visibilityAncestors;
    bool lastKnownShowingState = false;

    void rebuildStaticLayer(float displayScale);
    void rebuildVisibilityObservers();
    void updateShowingState();
    void componentVisibilityChanged(juce::Component&) override;
    void componentParentHierarchyChanged(juce::Component&) override;
    void componentBeingDeleted(juce::Component&) override;

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
