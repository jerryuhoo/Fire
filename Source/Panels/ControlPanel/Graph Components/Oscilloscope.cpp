/*
  ==============================================================================

    Oscilloscope.cpp
    Created: 25 Oct 2020 7:26:35pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "Oscilloscope.h"
#include <cmath>

namespace
{
constexpr float silenceFloor = 1.0e-5f;
const juce::String monoLabel { "M" }, leftLabel { "L" }, rightLabel { "R" };
float finiteSample(float value) noexcept { return std::isfinite(value) ? value : 0.0f; }
}

//==============================================================================
Oscilloscope::Oscilloscope(FireAudioProcessor& p) : processor(p)
{
    historySourceToken = processor.getHistorySourceToken();
    setGraphIdentity("WAVEFORM", fire::ui::ModuleRole::drive);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

void Oscilloscope::paint(juce::Graphics& g)
{
    GraphTemplate::paint(g);
    if (waveformGeometryDirty) updateWaveformPaths();
    if (historyL.isEmpty() || sampleIndexByPixel.empty()) return;
    const auto plot = getWaveformBounds();
    if (plot.getWidth() <= 1.0f || plot.getHeight() <= 1.0f) return;
    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(getGraphPlotBounds().getSmallestIntegerContainer());
    const auto scale = getScale();
    const auto stroke = juce::jlimit(0.85f, 2.2f, 1.2f * scale);
    const auto drawChannel = [&](size_t channel, const juce::Path& trace,
                                 const juce::Path& envelope, const juce::ColourGradient& gradient,
                                 juce::Colour colour, juce::Colour highlight, const juce::String& label)
    {
        // A quiet reference remains readable without turning silence into a
        // bright signal. The two lanes retain a shared amplitude scale.
        g.setColour(fire::ui::colours::hairline.withAlpha(0.38f));
        g.drawLine(plot.getX(), channelCentres[channel], plot.getRight(), channelCentres[channel], 0.7f * scale);
        const auto labelWidth = plot.getX() - getGraphPlotBounds().getX() - 3.0f * scale;
        if (labelWidth >= 6.0f * scale)
        {
            g.setColour((channelLight[channel] > 0.05f ? colour : fire::ui::colours::textMuted)
                            .withAlpha(0.42f + 0.28f * channelLight[channel]));
            g.setFont(fire::ui::labelFont(8.0f * scale));
            g.drawText(label, juce::Rectangle<float>(getGraphPlotBounds().getX(), channelCentres[channel] - 6.0f * scale,
                                                     labelWidth, 12.0f * scale), juce::Justification::centred);
        }
        if (trace.isEmpty()) return;
        g.setColour(colour.withAlpha(0.055f * channelLight[channel]));
        g.fillPath(envelope);
        g.setColour(colour.withAlpha(0.085f * channelLight[channel]));
        g.strokePath(trace, juce::PathStrokeType(stroke + 2.6f * scale,
                                                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill(gradient);
        g.strokePath(trace, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        if (newestLight[channel] > 0.01f)
        {
            const auto dot = juce::Rectangle<float>(2.0f * scale, 2.0f * scale).withCentre(newestPoints[channel]);
            g.setColour(colour.withAlpha(0.11f * newestLight[channel]));
            g.fillEllipse(dot.expanded(1.2f * scale));
            g.setColour(highlight.withAlpha(0.9f * newestLight[channel]));
            g.fillEllipse(dot);
        }
    };
    drawChannel(0, waveformL, envelopeL, leftGradient, fire::ui::colours::ember,
                fire::ui::colours::whiteHot, monoChannel ? monoLabel : leftLabel);
    if (! monoChannel)
        drawChannel(1, waveformR, envelopeR, rightGradient, fire::ui::colours::signalCool,
                    fire::ui::colours::signalCool.interpolatedWith(fire::ui::colours::textPrimary, 0.55f), rightLabel);
}

void Oscilloscope::timerCallback()
{
    synchroniseHistorySource();

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

    const bool nextMonoChannel = processor.getTotalNumInputChannels() == 1;
    if (! processor.copyHistorySnapshot(historyScratch)
        || historyScratch.sourceToken != historySourceToken
        || processor.getHistorySourceToken() != historySourceToken)
    {
        synchroniseHistorySource();
        return;
    }

    lastHistoryGeneration = historyScratch.generation;
    const bool historyChanged = monoChannel != nextMonoChannel
                                || ! arraysMatch(historyL, historyScratch.left)
                                || (! nextMonoChannel
                                    && ! arraysMatch(historyR, historyScratch.right));

    if (! historyChanged && ! waveformGeometryDirty)
        return;

    // Growing a silent startup history does not change the picture either.
    // Retain the last silent frame until real content, layout or channel mode
    // changes; an empty reset frame still clears the presentation normally.
    if (! waveformGeometryDirty && monoChannel == nextMonoChannel
        && ! historyL.isEmpty() && ! historyScratch.left.isEmpty()
        && isSilent(historyL) && isSilent(historyScratch.left)
        && (monoChannel || (isSilent(historyR) && isSilent(historyScratch.right))))
    {
        historyL.swapWith(historyScratch.left);
        if (! monoChannel) historyR.swapWith(historyScratch.right);
        return;
    }

    if (historyChanged)
    {
        monoChannel = nextMonoChannel;
        historyL.swapWith(historyScratch.left);
        if (monoChannel)
            historyR.clearQuick();
        else
            historyR.swapWith(historyScratch.right);
    }

    updateWaveformPaths();
    repaint(getGraphPlotBounds().getSmallestIntegerContainer());
}

void Oscilloscope::resized()
{
    GraphTemplate::resized();
    sampleIndexByPixel.clear();
    mappedSampleCount = 0;
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
    synchroniseHistorySource();
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

    synchroniseHistorySource();
    startTimerHz(60);
    timerCallback();
    repaint();
}

bool Oscilloscope::synchroniseHistorySource()
{
    const auto requestedSourceToken = processor.getHistorySourceToken();
    if (requestedSourceToken == historySourceToken)
        return false;

    historySourceToken = requestedSourceToken;
    lastHistoryGeneration = 0;
    historyL.clearQuick();
    historyR.clearQuick();
    historyScratch.left.clearQuick();
    historyScratch.right.clearQuick();
    historyScratch.sourceToken = requestedSourceToken;
    historyScratch.generation = 0;
    waveformL.clear();
    waveformR.clear();
    envelopeL.clear();
    envelopeR.clear();
    sampleIndexByPixel.clear();
    rangesL.clear();
    rangesR.clear();
    channelLight.fill(0.0f);
    newestLight.fill(0.0f);
    mappedSampleCount = 0;
    waveformGeometryDirty = false;
    repaint(getGraphPlotBounds().getSmallestIntegerContainer());
    return true;
}

bool Oscilloscope::arraysMatch(const juce::Array<float>& lhs,
                               const juce::Array<float>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;

    for (int i = 0; i < lhs.size(); ++i)
        if (! juce::approximatelyEqual(finiteSample(lhs.getUnchecked(i)), finiteSample(rhs.getUnchecked(i))))
            return false;

    return true;
}

bool Oscilloscope::isSilent(const juce::Array<float>& history) noexcept
{
    for (const auto value : history)
        if (std::abs(finiteSample(value)) > silenceFloor) return false;
    return true;
}

juce::Rectangle<float> Oscilloscope::getWaveformBounds() const noexcept
{
    auto bounds = getGraphPlotBounds();
    const auto scale = getScale();
    if (bounds.getWidth() >= 48.0f * scale) bounds.removeFromLeft(11.0f * scale);
    return bounds.reduced(1.5f * scale, 1.0f * scale);
}

void Oscilloscope::rebuildIndexMap(int sampleCount)
{
    const int pixelCount = juce::jmax(0, juce::roundToInt(getWaveformBounds().getWidth()));
    const int buckets = juce::jmin(sampleCount, pixelCount);
    sampleIndexByPixel.resize(static_cast<size_t>(juce::jmax(0, buckets)));
    mappedSampleCount = sampleCount;
    if (sampleCount <= 0 || buckets <= 0)
        return;
    // Half-open buckets cover every sample once, including both endpoints.
    for (int pixel = 0; pixel < buckets; ++pixel)
        sampleIndexByPixel[static_cast<size_t>(pixel)] = static_cast<int>(
            static_cast<std::int64_t>(pixel) * sampleCount / buckets);
}

void Oscilloscope::updateWaveformPaths()
{
    waveformL.clear();
    waveformR.clear();
    envelopeL.clear();
    envelopeR.clear();
    rangesL.clear();
    rangesR.clear();
    channelLight.fill(0.0f);
    newestLight.fill(0.0f);
    waveformGeometryDirty = false;

    const int sampleCount = monoChannel ? historyL.size()
                                        : juce::jmin(historyL.size(), historyR.size());
    const auto plotBounds = getWaveformBounds();
    const int pixelCount = juce::jmax(0, juce::roundToInt(plotBounds.getWidth()));
    if (sampleCount <= 0 || pixelCount <= 1 || plotBounds.getHeight() <= 1.0f)
    {
        sampleIndexByPixel.clear();
        mappedSampleCount = 0;
        return;
    }

    const int buckets = juce::jmin(sampleCount, pixelCount);
    if (static_cast<int>(sampleIndexByPixel.size()) != buckets || mappedSampleCount != sampleCount)
        rebuildIndexMap(sampleCount);

    std::array<float, 2> peaks {};
    std::array<double, 2> energy {};
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto left = finiteSample(historyL.getUnchecked(sample));
        peaks[0] = juce::jmax(peaks[0], std::abs(left));
        energy[0] += static_cast<double>(left) * left;
        if (! monoChannel)
        {
            const auto right = finiteSample(historyR.getUnchecked(sample));
            peaks[1] = juce::jmax(peaks[1], std::abs(right));
            energy[1] += static_cast<double>(right) * right;
        }
    }

    // Bound display gain to 4x. Unlike a hard auto-gain threshold this remains
    // continuous near silence and cannot enlarge a tiny noise floor to full
    // height. One normaliser preserves the real balance between L and R.
    const auto normaliser = 0.80 / juce::jmax(0.20, static_cast<double>(juce::jmax(peaks[0], peaks[1])));
    channelCentres[0] = monoChannel ? plotBounds.getCentreY()
                                    : plotBounds.getY() + plotBounds.getHeight() * 0.27f;
    channelCentres[1] = plotBounds.getY() + plotBounds.getHeight() * 0.73f;
    const auto amplitude = plotBounds.getHeight() * (monoChannel ? 0.44f : 0.20f);
    for (size_t channel = 0; channel < channelLight.size(); ++channel)
        if (peaks[channel] > silenceFloor)
        {
            const auto rms = std::sqrt(energy[channel] / static_cast<double>(sampleCount));
            const auto level = 0.65 * peaks[channel] + 0.35 * rms * 1.41421356237;
            channelLight[channel] = static_cast<float>(juce::jmin(1.0, std::sqrt(level) * 2.2));
        }

    const auto xForBucket = [&](int bucket)
    {
        return buckets == 1 ? plotBounds.getRight()
            : plotBounds.getX() + plotBounds.getWidth() * static_cast<float>(bucket) / static_cast<float>(buckets - 1);
    };
    const auto buildChannel = [&](const juce::Array<float>& history, std::vector<SampleRange>& ranges,
                                  juce::Path& trace, juce::Path& envelope, size_t channel)
    {
        if (channelLight[channel] <= 0.0f) return;
        ranges.resize(static_cast<size_t>(buckets));
        trace.preallocateSpace(buckets * 6 + 6);
        envelope.preallocateSpace(buckets * 6 + 6);
        const auto valueAt = [&](int sample)
        { return static_cast<float>(juce::jlimit(-1.0, 1.0,
            static_cast<double>(finiteSample(history.getUnchecked(sample))) * normaliser)); };
        const auto yForValue = [&](float value) { return channelCentres[channel] - value * amplitude; };
        bool drawing = false;
        for (int bucket = 0; bucket < buckets; ++bucket)
        {
            const auto start = sampleIndexByPixel[static_cast<size_t>(bucket)];
            const auto end = bucket + 1 < buckets ? sampleIndexByPixel[static_cast<size_t>(bucket + 1)] : sampleCount;
            auto& range = ranges[static_cast<size_t>(bucket)];
            range.minimum = range.maximum = valueAt(start);
            int minimumIndex = start, maximumIndex = start;
            for (int sample = start + 1; sample < end; ++sample)
            {
                const auto value = valueAt(sample);
                if (value < range.minimum) { range.minimum = value; minimumIndex = sample; }
                if (value > range.maximum) { range.maximum = value; maximumIndex = sample; }
            }
            range.minimumFirst = minimumIndex <= maximumIndex;
            const auto x = xForBucket(bucket);
            // A loud older transient must not illuminate the silent history
            // after it. Join each real signal span to the quiet baseline, but
            // do not stroke that baseline again as a bright waveform.
            if (juce::jmax(std::abs(range.minimum), std::abs(range.maximum)) <= 1.0e-6f)
            {
                if (drawing) trace.lineTo(x, yForValue(valueAt(start)));
                drawing = false;
                continue;
            }
            if (! drawing)
                trace.startNewSubPath(xForBucket(juce::jmax(0, bucket - 1)),
                                     yForValue(valueAt(juce::jmax(0, start - 1))));
            drawing = true;
            trace.lineTo(x, yForValue(range.minimumFirst ? range.minimum : range.maximum));
            if (! juce::exactlyEqual(range.minimum, range.maximum))
                trace.lineTo(x, yForValue(range.minimumFirst ? range.maximum : range.minimum));
        }
        newestPoints[channel] = {plotBounds.getRight(), yForValue(valueAt(sampleCount - 1))};
        if (drawing) trace.lineTo(newestPoints[channel]);
        newestLight[channel] = juce::jmin(channelLight[channel],
            static_cast<float>(juce::jmin(1.0, std::sqrt(static_cast<double>(std::abs(
                finiteSample(history.getUnchecked(sampleCount - 1))))) * 2.2)));
        envelope.startNewSubPath(xForBucket(0), channelCentres[channel]);
        for (int bucket = 0; bucket < buckets; ++bucket)
            envelope.lineTo(xForBucket(bucket), yForValue(juce::jmax(0.0f, ranges[static_cast<size_t>(bucket)].maximum)));
        for (int bucket = buckets; --bucket >= 0;)
            envelope.lineTo(xForBucket(bucket), yForValue(juce::jmin(0.0f, ranges[static_cast<size_t>(bucket)].minimum)));
        envelope.closeSubPath();
    };
    buildChannel(historyL, rangesL, waveformL, envelopeL, 0);
    if (! monoChannel) buildChannel(historyR, rangesR, waveformR, envelopeR, 1);

    // History is oldest at the left, newest at the right. Its age gradient and
    // signal-derived light make the flow visible without a decorative clock.
    leftGradient = juce::ColourGradient(fire::ui::colours::ember.withAlpha(0.32f * channelLight[0]),
        plotBounds.getX(), channelCentres[0], fire::ui::colours::whiteHot.withAlpha(0.92f * channelLight[0]),
        plotBounds.getRight(), channelCentres[0], false);
    leftGradient.addColour(0.6, fire::ui::colours::flame.withAlpha(0.70f * channelLight[0]));
    rightGradient = juce::ColourGradient(fire::ui::colours::signalCool.withAlpha(0.30f * channelLight[1]),
        plotBounds.getX(), channelCentres[1],
        fire::ui::colours::signalCool.interpolatedWith(fire::ui::colours::textPrimary, 0.45f).withAlpha(0.88f * channelLight[1]),
        plotBounds.getRight(), channelCentres[1], false);
    rightGradient.addColour(0.6, fire::ui::colours::signalCool.withAlpha(0.68f * channelLight[1]));
}
