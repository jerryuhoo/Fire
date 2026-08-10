/*
  ==============================================================================

    WidthGraph.h
    Created: 14 Dec 2020 3:40:40pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../PluginProcessor.h"
#include "GraphTemplate.h"
#include <cstdint>

//==============================================================================
/*
*/
class WidthGraph : public GraphTemplate, juce::Timer
{
public:
    WidthGraph(FireAudioProcessor&);
    ~WidthGraph() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;
    void resized() override;
    void visibilityChanged() override;

private:
    FireAudioProcessor& processor;
    juce::Array<float> historyL;
    juce::Array<float> historyR;
    juce::Array<float> historyScratchL;
    juce::Array<float> historyScratchR;
    juce::Image pointCloudCache;
    juce::Rectangle<float> pointCloudCacheBounds;
    float pointCloudCacheScale = 0.0f;
    int fadeFramesRemaining = 0;
    bool cacheHasContent = false;
    bool cacheGeometryDirty = true;
    bool restoreTrailOnCacheRebuild = false;
    bool monoChannel = false;
    std::uint64_t lastHistoryGeneration = 0;

    static bool arraysMatch(const juce::Array<float>& lhs, const juce::Array<float>& rhs) noexcept;
    void rebuildPointCloudCache(float displayScale);
    bool drawLatestFrame(bool hasNewSamples);
    void graphShowingStateChanged(bool isNowShowing) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WidthGraph)
};
