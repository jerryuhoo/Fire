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
#include <array>
#include <cstdint>

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
    void presentMeterValues(const MeterValues& values, std::uint64_t generation);

private:
    friend struct VUPanelTestAccess;

    FireAudioProcessor& processor;
    int focusBandNum;
    VUMeter vuMeterIn;
    VUMeter vuMeterOut;

    float realtimeThresholdDb;
    std::atomic<float>* compBypassValue = nullptr;
    int staleTimerTicks = 0;
    bool thresholdVisible = false;
    bool meterPresentationActive = false;
    std::array<std::uint64_t, 5> lastPresentedMeterGenerationBySource {};

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
    void resetMeterPresentation();
    bool refreshReadoutText();
    void graphShowingStateChanged(bool isNowShowing) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUPanel)
};
