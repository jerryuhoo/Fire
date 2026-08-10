/*
  ==============================================================================

    VUPanel.h
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../PluginProcessor.h"
#include "GraphTemplate.h"
#include "VUMeter.h"

//==============================================================================
/*
*/
class VUPanel : public GraphTemplate, juce::Timer
{
public:
    VUPanel(FireAudioProcessor&);
    ~VUPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setFocusBandNum(int num);
    void timerCallback() override;
    void updateRealtimeThreshold(float newThresholdDb);

private:
    FireAudioProcessor& processor;
    int focusBandNum;
    VUMeter vuMeterIn;
    VUMeter vuMeterOut;

    float realtimeThresholdDb;
    std::atomic<float>* compBypassValue = nullptr;
    int staleTimerTicks = 0;
    bool thresholdVisible = false;

    juce::Image scaleLayer;
    juce::Rectangle<int> scaleLayerBounds;
    float scaleLayerScale = 0.0f;
    juce::Rectangle<float> leftReadoutBounds;
    juce::Rectangle<float> rightReadoutBounds;
    juce::Rectangle<float> scaleBounds;
    juce::Font peakReadoutFont { juce::FontOptions() };
    juce::Font rmsReadoutFont { juce::FontOptions() };
    juce::Font captionFont { juce::FontOptions() };
    juce::String inputPeakText { "-96.0" };
    juce::String inputRmsText { "-96.0" };
    juce::String outputPeakText { "-96.0" };
    juce::String outputRmsText { "-96.0" };
    int inputPeakTenths = -960;
    int inputRmsTenths = -960;
    int outputPeakTenths = -960;
    int outputRmsTenths = -960;

    void rebuildScaleLayer(float displayScale);
    bool refreshReadoutText();
    void graphShowingStateChanged(bool isNowShowing) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUPanel)
};
