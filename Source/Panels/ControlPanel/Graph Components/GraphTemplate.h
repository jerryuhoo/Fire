/*
  ==============================================================================

    GraphTemplate.h
    Created: 5 Oct 2021 11:42:57am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../../GUI/InterfaceDefines.h"

//==============================================================================
/*
*/
class GraphTemplate  : public juce::Component
{
public:
    GraphTemplate();
    ~GraphTemplate() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setScale(float scale);
    float getScale();
    bool getZoomState();
    void setZoomState(bool zoomState);
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;

private:
    float scale = 1.0f;
    
protected:
    bool isMouseOn = false;
    bool mZoomState = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphTemplate)
};
