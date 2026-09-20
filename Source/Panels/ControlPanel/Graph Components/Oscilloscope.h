/*
  ==============================================================================

    Oscilloscope.h
    Created: 25 Oct 2020 7:26:35pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../PluginProcessor.h"
#include "GraphTemplate.h"
#include <cstdint>
#include <vector>

//==============================================================================
/*
*/
class Oscilloscope : public GraphTemplate, juce::Timer
{
public:
    Oscilloscope(FireAudioProcessor&);
    ~Oscilloscope() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void timerCallback() override;

private:
    friend struct OscilloscopeHistorySourceTestAccess;
    friend struct GraphViewSelectorTestAccess;

    FireAudioProcessor& processor;

    juce::Array<float> historyL;
    juce::Array<float> historyR;
    FireAudioProcessor::HistorySnapshot historyScratch;
    juce::Path waveformL;
    juce::Path waveformR;
    juce::ColourGradient leftGradient;
    juce::ColourGradient rightGradient;
    std::vector<int> sampleIndexByPixel;
    bool monoChannel = false;
    bool waveformGeometryDirty = true;
    std::uint64_t historySourceToken = 0;
    std::uint64_t lastHistoryGeneration = 0;

    static bool arraysMatch(const juce::Array<float>& lhs, const juce::Array<float>& rhs) noexcept;
    bool synchroniseHistorySource();
    void rebuildIndexMap(int sampleCount);
    void updateWaveformPaths();
    void graphShowingStateChanged(bool isNowShowing) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Oscilloscope)
};
