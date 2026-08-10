/*
  ==============================================================================

    WidthGraph.cpp
    Created: 14 Dec 2020 3:40:40pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "WidthGraph.h"
#include "../../../GUI/InterfaceDefines.h"

//==============================================================================
WidthGraph::WidthGraph(FireAudioProcessor& p) : processor(p)
{
    setGraphIdentity("STEREO FIELD", fire::ui::ModuleRole::stereo);
}

WidthGraph::~WidthGraph()
{
    stopTimer();
}

void WidthGraph::paint(juce::Graphics& g)
{
    GraphTemplate::paint(g);

    const auto displayScale = juce::jmax(
        0.25f,
        g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto plotBounds = getGraphPlotBounds();
    const auto expectedWidth = juce::jmax(
        1,
        juce::roundToInt(plotBounds.getWidth() * displayScale));
    const auto expectedHeight = juce::jmax(
        1,
        juce::roundToInt(plotBounds.getHeight() * displayScale));
    if (cacheGeometryDirty
        || ! pointCloudCache.isValid()
        || pointCloudCache.getWidth() != expectedWidth
        || pointCloudCache.getHeight() != expectedHeight
        || pointCloudCacheBounds != plotBounds
        || ! juce::approximatelyEqual(pointCloudCacheScale, displayScale))
    {
        rebuildPointCloudCache(displayScale);
    }

    if (pointCloudCache.isValid())
        g.drawImage(pointCloudCache, plotBounds);

    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(plotBounds.getSmallestIntegerContainer());
    g.setColour(fire::ui::colours::signalCool.withAlpha(0.22f));
    g.drawLine(plotBounds.getX(), plotBounds.getBottom(),
               plotBounds.getRight(), plotBounds.getY(), 1.0f);
    g.setColour(fire::ui::colours::ember.withAlpha(0.16f));
    g.drawLine(plotBounds.getX(), plotBounds.getY(),
               plotBounds.getRight(), plotBounds.getBottom(), 1.0f);
}

void WidthGraph::timerCallback()
{
    if (! isShowing())
        return;

    if (cacheGeometryDirty || ! pointCloudCache.isValid())
    {
        repaint();
        return;
    }

    const auto historyGeneration = processor.getHistoryGeneration();
    if (historyGeneration == lastHistoryGeneration)
    {
        if (fadeFramesRemaining > 0 && drawLatestFrame(false))
            repaint(getGraphPlotBounds().getSmallestIntegerContainer());
        return;
    }
    lastHistoryGeneration = historyGeneration;

    const bool nextMonoChannel = processor.getTotalNumInputChannels() != 2;
    processor.copyHistoryArrays(historyScratchL, historyScratchR);
    const bool historyContentsChanged = monoChannel != nextMonoChannel
                                        || ! arraysMatch(historyL, historyScratchL)
                                        || (! nextMonoChannel
                                            && ! arraysMatch(historyR, historyScratchR));

    if (historyContentsChanged)
    {
        monoChannel = nextMonoChannel;
        historyL.swapWith(historyScratchL);
        if (monoChannel)
            historyR.clearQuick();
        else
            historyR.swapWith(historyScratchR);
    }

    // A new generation is a new visual frame even if its values happen to be
    // identical (DC input or a periodic window). Reinforce that frame instead
    // of letting the trail fade to nothing while audio is still arriving.
    if (drawLatestFrame(true))
        repaint(getGraphPlotBounds().getSmallestIntegerContainer());
}

void WidthGraph::resized()
{
    GraphTemplate::resized();
    restoreTrailOnCacheRebuild = cacheHasContent;
    pointCloudCache = {};
    cacheHasContent = false;
    fadeFramesRemaining = 0;
    cacheGeometryDirty = true;
    pointCloudCacheBounds = {};
    pointCloudCacheScale = 0.0f;
    if (isShowing())
        repaint();
}

void WidthGraph::visibilityChanged()
{
    GraphTemplate::visibilityChanged();
    if (isShowing() && cacheGeometryDirty)
        repaint();
}

void WidthGraph::graphShowingStateChanged(bool isNowShowing)
{
    if (! isNowShowing)
    {
        stopTimer();
        return;
    }

    startTimerHz(60);
    repaint();
}

void WidthGraph::rebuildPointCloudCache(float displayScale)
{
    cacheGeometryDirty = false;

    const auto plotBounds = getGraphPlotBounds();
    const bool shouldRestoreTrail = (cacheHasContent || restoreTrailOnCacheRebuild)
                                    && ! historyL.isEmpty()
                                    && (monoChannel || ! historyR.isEmpty());
    displayScale = juce::jmax(0.25f, displayScale);
    const auto cacheWidth = juce::roundToInt(plotBounds.getWidth() * displayScale);
    const auto cacheHeight = juce::roundToInt(plotBounds.getHeight() * displayScale);
    if (cacheWidth > 0 && cacheHeight > 0)
    {
        pointCloudCache = juce::Image(juce::Image::ARGB, cacheWidth, cacheHeight, true);
        pointCloudCacheBounds = plotBounds;
        pointCloudCacheScale = displayScale;
        cacheHasContent = false;
        fadeFramesRemaining = 0;
        restoreTrailOnCacheRebuild = false;
        if (shouldRestoreTrail)
            drawLatestFrame(true);
    }
    else
    {
        pointCloudCache = {};
        pointCloudCacheBounds = {};
        pointCloudCacheScale = 0.0f;
        restoreTrailOnCacheRebuild = shouldRestoreTrail;
    }
}

bool WidthGraph::arraysMatch(const juce::Array<float>& lhs,
                             const juce::Array<float>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;

    for (int i = 0; i < lhs.size(); ++i)
        if (! juce::approximatelyEqual(lhs.getUnchecked(i), rhs.getUnchecked(i)))
            return false;

    return true;
}

bool WidthGraph::drawLatestFrame(bool hasNewSamples)
{
    if (! pointCloudCache.isValid())
        return false;

    bool visualChanged = false;
    if (cacheHasContent)
    {
        pointCloudCache.multiplyAllAlphas(0.82f);
        visualChanged = true;
    }

    juce::Graphics cacheGraphics(pointCloudCache);
    cacheGraphics.addTransform(juce::AffineTransform::scale(pointCloudCacheScale));
    bool drewSignal = false;
    const int sampleCount = monoChannel ? historyL.size()
                                        : juce::jmin(historyL.size(), historyR.size());
    if (hasNewSamples && sampleCount > 0)
    {
        float maxValue = 0.0f;
        for (int sample = 0; sample < sampleCount; ++sample)
        {
            const auto left = historyL.getUnchecked(sample);
            const auto right = monoChannel ? left : historyR.getUnchecked(sample);
            maxValue = juce::jmax(maxValue, std::abs(left), std::abs(right));
        }

        if (maxValue > 0.00001f)
        {
            const auto logicalWidth = pointCloudCacheBounds.getWidth();
            const auto logicalHeight = pointCloudCacheBounds.getHeight();
            const auto centreX = logicalWidth * 0.5f;
            const auto centreY = logicalHeight * 0.5f;
            const auto scaleFactor = juce::jmin(logicalWidth, logicalHeight)
                                     * 0.44f / maxValue;

            const auto targetPointCount = juce::jlimit(64,
                                                       512,
                                                       juce::roundToInt(logicalWidth * 2.0f));
            const auto sampleStride = juce::jmax(1,
                                                 (sampleCount + targetPointCount - 1)
                                                     / targetPointCount);
            for (int sample = 0; sample < sampleCount; sample += sampleStride)
            {
                const auto left = historyL.getUnchecked(sample);
                const auto right = monoChannel ? left : historyR.getUnchecked(sample);
                const auto side = (left - right) * 0.5f;
                const auto mid = (left + right) * 0.5f;
                const auto point = juce::Point<float>(centreX + side * scaleFactor,
                                                      centreY - mid * scaleFactor);

                const auto phaseEnergy = juce::jlimit(0.0f, 1.0f, std::abs(side) / maxValue);
                cacheGraphics.setColour(fire::ui::colours::signalCool
                                            .interpolatedWith(fire::ui::colours::flame, phaseEnergy)
                                            .withAlpha(0.62f));
                cacheGraphics.fillEllipse(juce::Rectangle<float>(1.6f, 1.6f).withCentre(point));
            }

            drewSignal = true;
            visualChanged = true;
            cacheHasContent = true;
            fadeFramesRemaining = 24;
        }
    }

    if (! drewSignal && fadeFramesRemaining > 0)
        --fadeFramesRemaining;

    if (fadeFramesRemaining == 0 && cacheHasContent && ! drewSignal)
    {
        pointCloudCache.clear(pointCloudCache.getBounds(), juce::Colours::transparentBlack);
        cacheHasContent = false;
        visualChanged = true;
    }

    return visualChanged;
}
