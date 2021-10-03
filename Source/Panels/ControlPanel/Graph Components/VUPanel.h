/*
  ==============================================================================

    VUPanel.h
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../../PluginProcessor.h"
#include "VUMeter.h"
//==============================================================================
/*
*/
class VUPanel  : public juce::Component
{
public:
    VUPanel(FireAudioProcessor &);
    ~VUPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool getZoomState();
    void setZoomState(bool zoomState);
    void mouseDown(const juce::MouseEvent &e) override;
    void setFocusBandNum(int num);
    
private:
    FireAudioProcessor &processor;
    int focusBandNum;
    VUMeter vuMeterIn;
    VUMeter vuMeterOut;
    bool mZoomState = false; // false means small
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUPanel)
};
