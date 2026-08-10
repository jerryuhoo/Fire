/*
  ==============================================================================

    Oscilloscope.cpp
    Created: 25 Oct 2020 7:26:35pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "Oscilloscope.h"

//==============================================================================
Oscilloscope::Oscilloscope(FireAudioProcessor& p) : processor(p)
{
    setGraphIdentity("WAVEFORM", fire::ui::ModuleRole::drive);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

void Oscilloscope::paint(juce::Graphics& g)
{
    GraphTemplate::paint(g);

    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(getGraphPlotBounds().getSmallestIntegerContainer());

    g.setColour(getGraphAccent().withAlpha(0.13f));
    g.strokePath(waveformL,
                 juce::PathStrokeType(5.0f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    g.setGradientFill(leftGradient);
    g.strokePath(waveformL,
                 juce::PathStrokeType(1.65f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    if (! monoChannel)
    {
        g.setColour(fire::ui::colours::signalCool.withAlpha(0.11f));
        g.strokePath(waveformR,
                     juce::PathStrokeType(4.0f,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
        g.setGradientFill(rightGradient);
        g.strokePath(waveformR,
                     juce::PathStrokeType(1.4f,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }
}

void Oscilloscope::timerCallback()
{
    if (! isShowing() || getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto historyGeneration = processor.getHistoryGeneration();
    if (historyGeneration == lastHistoryGeneration)
    {
        if (waveformGeometryDirty)
        {
            updateWaveformPaths();
            repaint(getGraphPlotBounds().getSmallestIntegerContainer());
        }
        return;
    }
    lastHistoryGeneration = historyGeneration;

    const bool nextMonoChannel = processor.getTotalNumInputChannels() == 1;
    processor.copyHistoryArrays(historyScratchL, historyScratchR);
    const bool historyChanged = monoChannel != nextMonoChannel
                                || ! arraysMatch(historyL, historyScratchL)
                                || (! nextMonoChannel
                                    && ! arraysMatch(historyR, historyScratchR));

    if (! historyChanged && ! waveformGeometryDirty)
        return;

    if (historyChanged)
    {
        monoChannel = nextMonoChannel;
        historyL.swapWith(historyScratchL);
        if (monoChannel)
            historyR.clearQuick();
        else
            historyR.swapWith(historyScratchR);
    }

    updateWaveformPaths();
    repaint(getGraphPlotBounds().getSmallestIntegerContainer());
}

void Oscilloscope::resized()
{
    GraphTemplate::resized();
    sampleIndexByPixel.clear();
    waveformGeometryDirty = true;
    if (isShowing())
    {
        updateWaveformPaths();
        repaint(getGraphPlotBounds().getSmallestIntegerContainer());
    }
}

void Oscilloscope::visibilityChanged()
{
    GraphTemplate::visibilityChanged();
    if (isShowing() && waveformGeometryDirty)
    {
        updateWaveformPaths();
        repaint(getGraphPlotBounds().getSmallestIntegerContainer());
    }
}

void Oscilloscope::graphShowingStateChanged(bool isNowShowing)
{
    if (! isNowShowing)
    {
        stopTimer();
        return;
    }

    startTimerHz(60);
    timerCallback();
    repaint();
}

bool Oscilloscope::arraysMatch(const juce::Array<float>& lhs,
                               const juce::Array<float>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;

    for (int i = 0; i < lhs.size(); ++i)
        if (! juce::approximatelyEqual(lhs.getUnchecked(i), rhs.getUnchecked(i)))
            return false;

    return true;
}

void Oscilloscope::rebuildIndexMap(int sampleCount)
{
    const auto plotBounds = getGraphPlotBounds();
    const int pixelCount = juce::jmax(0, juce::roundToInt(plotBounds.getWidth()));
    sampleIndexByPixel.resize(static_cast<size_t>(pixelCount));

    if (sampleCount <= 0 || pixelCount <= 0)
        return;

    const auto denominator = static_cast<float>(juce::jmax(1, pixelCount - 1));
    for (int pixel = 0; pixel < pixelCount; ++pixel)
    {
        sampleIndexByPixel[static_cast<size_t>(pixel)] = juce::jlimit(
            0,
            sampleCount - 1,
            juce::roundToInt((static_cast<float>(pixel) / denominator)
                             * static_cast<float>(sampleCount - 1)));
    }
}

void Oscilloscope::updateWaveformPaths()
{
    waveformL.clear();
    waveformR.clear();
    waveformGeometryDirty = false;

    const int sampleCount = monoChannel ? historyL.size()
                                        : juce::jmin(historyL.size(), historyR.size());
    const auto plotBounds = getGraphPlotBounds();
    const int pixelCount = juce::jmax(0, juce::roundToInt(plotBounds.getWidth()));
    if (sampleCount <= 0 || pixelCount <= 1 || plotBounds.getHeight() <= 1.0f)
        return;

    if (static_cast<int>(sampleIndexByPixel.size()) != pixelCount
        || sampleIndexByPixel.empty()
        || sampleIndexByPixel.back() != sampleCount - 1)
        rebuildIndexMap(sampleCount);

    float maxValue = 0.0f;
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        maxValue = juce::jmax(maxValue, std::abs(historyL.getUnchecked(sample)));
        if (! monoChannel)
            maxValue = juce::jmax(maxValue, std::abs(historyR.getUnchecked(sample)));
    }

    const auto normaliser = maxValue > 0.005f ? 0.72f / maxValue : 1.0f;
    const auto leftCentreY = monoChannel ? plotBounds.getCentreY()
                                         : plotBounds.getY() + plotBounds.getHeight() * 0.28f;
    const auto rightCentreY = plotBounds.getY() + plotBounds.getHeight() * 0.72f;
    const auto amplitude = plotBounds.getHeight() * (monoChannel ? 0.42f : 0.20f);
    const auto xIncrement = plotBounds.getWidth() / static_cast<float>(pixelCount - 1);

    waveformL.preallocateSpace(pixelCount * 3);
    waveformR.preallocateSpace(pixelCount * 3);

    for (int pixel = 0; pixel < pixelCount; ++pixel)
    {
        const int sourceIndex = sampleIndexByPixel[static_cast<size_t>(pixel)];
        const auto x = plotBounds.getX() + static_cast<float>(pixel) * xIncrement;
        const auto left = juce::jlimit(-1.0f,
                                       1.0f,
                                       historyL.getUnchecked(sourceIndex) * normaliser);
        const auto leftY = leftCentreY - left * amplitude;

        if (pixel == 0)
            waveformL.startNewSubPath(x, leftY);
        else
            waveformL.lineTo(x, leftY);

        if (! monoChannel)
        {
            const auto right = juce::jlimit(-1.0f,
                                            1.0f,
                                            historyR.getUnchecked(sourceIndex) * normaliser);
            const auto rightY = rightCentreY - right * amplitude;
            if (pixel == 0)
                waveformR.startNewSubPath(x, rightY);
            else
                waveformR.lineTo(x, rightY);
        }
    }

    leftGradient = juce::ColourGradient(fire::ui::colours::ember,
                                        plotBounds.getX(),
                                        leftCentreY,
                                        fire::ui::colours::whiteHot,
                                        plotBounds.getRight(),
                                        leftCentreY,
                                        false);
    leftGradient.addColour(0.55, fire::ui::colours::flame);

    rightGradient = juce::ColourGradient(fire::ui::colours::signalCool.withAlpha(0.48f),
                                         plotBounds.getX(),
                                         rightCentreY,
                                         fire::ui::colours::signalCool,
                                         plotBounds.getRight(),
                                         rightCentreY,
                                         false);
}
