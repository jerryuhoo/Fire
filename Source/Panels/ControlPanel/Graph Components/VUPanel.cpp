/*
  ==============================================================================

    VUPanel.cpp
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "VUPanel.h"
#include "../../../Utility/AudioHelpers.h"

#include <array>

//==============================================================================
VUPanel::VUPanel(FireAudioProcessor& p) : processor(p),
                                          focusBandNum(0),
                                          vuMeterIn(&p),
                                          vuMeterOut(&p),
                                          realtimeThresholdDb(-100.0f)
{
    setGraphIdentity("LEVELS", fire::ui::ModuleRole::compressor);
    vuMeterIn.setParameters(true, focusBandNum);
    vuMeterOut.setParameters(false, focusBandNum);
    compBypassValue = processor.treeState.getRawParameterValue(
        ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum));
    thresholdVisible = compBypassValue != nullptr
                       && compBypassValue->load(std::memory_order_relaxed) > 0.5f;

    addAndMakeVisible(vuMeterIn);
    addAndMakeVisible(vuMeterOut);
}

VUPanel::~VUPanel()
{
    stopTimer();
}

void VUPanel::paint(juce::Graphics& g)
{
    GraphTemplate::paint(g);

    const auto displayScale = juce::jmax(
        0.25f,
        g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto logicalBounds = getLocalBounds();
    const auto expectedWidth = juce::jmax(
        1,
        juce::roundToInt(static_cast<float>(logicalBounds.getWidth()) * displayScale));
    const auto expectedHeight = juce::jmax(
        1,
        juce::roundToInt(static_cast<float>(logicalBounds.getHeight()) * displayScale));
    if (! scaleLayer.isValid()
        || scaleLayer.getWidth() != expectedWidth
        || scaleLayer.getHeight() != expectedHeight
        || scaleLayerBounds != logicalBounds
        || ! juce::approximatelyEqual(scaleLayerScale, displayScale))
    {
        rebuildScaleLayer(displayScale);
    }

    if (scaleLayer.isValid())
        g.drawImage(scaleLayer, logicalBounds.toFloat());

    if (focusBandNum != -1 && thresholdVisible)
    {
        const auto normalized = juce::jlimit(0.0f, 1.0f, (realtimeThresholdDb + 96.0f) / 96.0f);
        const auto thresholdY = scaleBounds.getBottom() - scaleBounds.getHeight() * normalized;
        g.setColour(fire::ui::colours::warning.withAlpha(0.20f));
        g.drawLine(scaleBounds.getX(), thresholdY, scaleBounds.getRight(), thresholdY, 4.0f);
        g.setColour(fire::ui::colours::warning.withAlpha(0.94f));
        g.drawLine(scaleBounds.getX(), thresholdY, scaleBounds.getRight(), thresholdY, 1.0f);
    }

    const auto drawReadout = [&g, this](juce::Rectangle<float> area,
                                         const juce::String& caption,
                                         const juce::String& peak,
                                         const juce::String& rms,
                                         juce::Colour accent)
    {
        g.setColour(fire::ui::colours::textMuted);
        g.setFont(captionFont);
        g.drawText(caption, area.removeFromTop(captionFont.getHeight() + 2.0f),
                   juce::Justification::centred);

        g.setColour(accent);
        g.setFont(peakReadoutFont);
        g.drawText(peak, area.removeFromTop(area.getHeight() * 0.58f),
                   juce::Justification::centred);

        g.setColour(fire::ui::colours::textSecondary);
        g.setFont(rmsReadoutFont);
        g.drawText(rms, area, juce::Justification::centredTop);
    };

    drawReadout(leftReadoutBounds, "IN", inputPeakText, inputRmsText,
                fire::ui::colours::flame);
    drawReadout(rightReadoutBounds, "OUT", outputPeakText, outputRmsText,
                fire::ui::colours::positive);
}

void VUPanel::resized()
{
    GraphTemplate::resized();

    const auto plotBounds = getGraphPlotBounds();
    const auto meterHeight = juce::jmax(1.0f, plotBounds.getHeight() - 8.0f);
    const auto meterWidth = juce::jlimit(12.0f, 30.0f, plotBounds.getWidth() * 0.13f);
    const auto meterY = plotBounds.getCentreY() - meterHeight * 0.5f;
    const auto inputCentreX = plotBounds.getX() + plotBounds.getWidth() * 0.34f;
    const auto outputCentreX = plotBounds.getX() + plotBounds.getWidth() * 0.66f;

    vuMeterIn.setBounds(juce::Rectangle<float>(meterWidth, meterHeight)
                            .withCentre({ inputCentreX, plotBounds.getCentreY() })
                            .toNearestInt());
    vuMeterOut.setBounds(juce::Rectangle<float>(meterWidth, meterHeight)
                             .withCentre({ outputCentreX, plotBounds.getCentreY() })
                             .toNearestInt());

    leftReadoutBounds = juce::Rectangle<float>(plotBounds.getX(),
                                               meterY,
                                               juce::jmax(1.0f, vuMeterIn.getX() - plotBounds.getX() - 4.0f),
                                               meterHeight);
    rightReadoutBounds = juce::Rectangle<float>(static_cast<float>(vuMeterOut.getRight()) + 4.0f,
                                                meterY,
                                                juce::jmax(1.0f, plotBounds.getRight() - vuMeterOut.getRight() - 4.0f),
                                                meterHeight);
    scaleBounds = juce::Rectangle<float>(static_cast<float>(vuMeterIn.getRight()) + 2.0f,
                                         meterY,
                                         juce::jmax(1.0f, vuMeterOut.getX() - vuMeterIn.getRight() - 4.0f),
                                         meterHeight);

    const auto readoutSize = juce::jlimit(9.0f, 15.0f, plotBounds.getWidth() * 0.08f);
    peakReadoutFont = fire::ui::displayFont(readoutSize);
    rmsReadoutFont = fire::ui::bodyFont(juce::jmax(8.0f, readoutSize * 0.70f));
    captionFont = fire::ui::labelFont(juce::jmax(8.0f, readoutSize * 0.62f));
    scaleLayer = {};
    scaleLayerBounds = {};
    scaleLayerScale = 0.0f;
}

void VUPanel::setFocusBandNum(int num)
{
    if (num != -1 && ! juce::isPositiveAndBelow(num, 4))
        return;
    if (focusBandNum == num)
        return;

    focusBandNum = num;
    vuMeterIn.setParameters(true, focusBandNum);
    vuMeterOut.setParameters(false, focusBandNum);
    compBypassValue = focusBandNum == -1
                          ? nullptr
                          : processor.treeState.getRawParameterValue(
                              ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum));
    thresholdVisible = compBypassValue != nullptr
                       && compBypassValue->load(std::memory_order_relaxed) > 0.5f;
    staleTimerTicks = 0;
    refreshReadoutText();
    vuMeterIn.repaint();
    vuMeterOut.repaint();
    repaint();
}

void VUPanel::presentMeterValues(const MeterValues& values,
                                 std::uint64_t generation)
{
    if (! meterPresentationActive || generation == 0)
        return;

    // Host-bypass packets refresh only the global taps.  Ignore their retained
    // band payload before consuming the generation or resetting stale decay.
    if (focusBandNum >= 0 && ! values.bandLevelsAreFresh)
        return;

    const auto sourceIndex = static_cast<size_t>(focusBandNum + 1);
    auto& lastPresentedGeneration =
        lastPresentedMeterGenerationBySource[sourceIndex];
    if (generation <= lastPresentedGeneration)
        return;

    lastPresentedGeneration = generation;
    staleTimerTicks = 0;

    const bool inputChanged = vuMeterIn.updateLevels(values);
    const bool outputChanged = vuMeterOut.updateLevels(values);

    if (inputChanged)
        vuMeterIn.repaint();
    if (outputChanged)
        vuMeterOut.repaint();
    if (refreshReadoutText())
    {
        repaint(leftReadoutBounds.getSmallestIntegerContainer());
        repaint(rightReadoutBounds.getSmallestIntegerContainer());
    }
}

void VUPanel::timerCallback()
{
    if (! isShowing())
        return;

    const bool nextThresholdVisible = compBypassValue != nullptr
                                      && compBypassValue->load(std::memory_order_relaxed) > 0.5f;
    if (thresholdVisible != nextThresholdVisible)
    {
        thresholdVisible = nextThresholdVisible;
        repaint(scaleBounds.getSmallestIntegerContainer());
    }

    bool inputChanged = false;
    bool outputChanged = false;
    if (++staleTimerTicks > 3)
    {
        inputChanged = vuMeterIn.decayToSilence();
        outputChanged = vuMeterOut.decayToSilence();
    }

    if (! inputChanged && ! outputChanged)
        return;

    if (inputChanged)
        vuMeterIn.repaint();
    if (outputChanged)
        vuMeterOut.repaint();
    if (refreshReadoutText())
    {
        repaint(leftReadoutBounds.getSmallestIntegerContainer());
        repaint(rightReadoutBounds.getSmallestIntegerContainer());
    }
}

void VUPanel::updateRealtimeThreshold(float newThresholdDb)
{
    if (juce::approximatelyEqual(realtimeThresholdDb, newThresholdDb))
        return;

    realtimeThresholdDb = newThresholdDb;
    if (isShowing() && thresholdVisible)
        repaint(scaleBounds.getSmallestIntegerContainer());
}

void VUPanel::graphShowingStateChanged(bool isNowShowing)
{
    meterPresentationActive = isNowShowing;
    resetMeterPresentation();

    if (! isNowShowing)
    {
        stopTimer();
        return;
    }

    startTimerHz(60);
    timerCallback();
    repaint();
}

void VUPanel::resetMeterPresentation()
{
    staleTimerTicks = 0;
    vuMeterIn.resetLevels();
    vuMeterOut.resetLevels();
    refreshReadoutText();
}

void VUPanel::rebuildScaleLayer(float displayScale)
{
    if (getWidth() <= 0 || getHeight() <= 0)
    {
        scaleLayer = {};
        scaleLayerBounds = {};
        scaleLayerScale = 0.0f;
        return;
    }

    displayScale = juce::jmax(0.25f, displayScale);
    scaleLayerBounds = getLocalBounds();
    scaleLayerScale = displayScale;
    scaleLayer = juce::Image(
        juce::Image::ARGB,
        juce::jmax(1, juce::roundToInt(static_cast<float>(getWidth()) * displayScale)),
        juce::jmax(1, juce::roundToInt(static_cast<float>(getHeight()) * displayScale)),
        true);
    juce::Graphics cacheGraphics(scaleLayer);
    cacheGraphics.addTransform(juce::AffineTransform::scale(displayScale));
    cacheGraphics.setFont(fire::ui::labelFont(juce::jlimit(7.0f, 10.0f,
                                                           scaleBounds.getWidth() * 0.24f)));
    cacheGraphics.setColour(fire::ui::colours::textMuted.withAlpha(0.85f));

    constexpr std::array<int, 5> marks { 0, -24, -48, -72, -96 };
    for (const auto mark : marks)
    {
        const auto normalized = (static_cast<float>(mark) + 96.0f) / 96.0f;
        const auto y = scaleBounds.getBottom() - scaleBounds.getHeight() * normalized;
        cacheGraphics.setColour(fire::ui::colours::hairline.withAlpha(mark == 0 ? 0.75f : 0.38f));
        cacheGraphics.drawHorizontalLine(juce::roundToInt(y),
                                         scaleBounds.getX(),
                                         scaleBounds.getRight());
        cacheGraphics.setColour(mark == 0 ? fire::ui::colours::warning.withAlpha(0.88f)
                                          : fire::ui::colours::textMuted.withAlpha(0.85f));
        cacheGraphics.drawText(juce::String(mark),
                               scaleBounds.withY(y - 6.0f).withHeight(12.0f),
                               juce::Justification::centred);
    }
}

bool VUPanel::refreshReadoutText()
{
    const auto normalizedToDb = [](float value)
    {
        return juce::jmap(juce::jlimit(0.0f, 1.0f, value), 0.0f, 1.0f, -96.0f, 0.0f);
    };

    const auto updateText = [&normalizedToDb](juce::String& text,
                                               int& cachedTenths,
                                               float normalizedValue)
    {
        const auto nextTenths = juce::roundToInt(normalizedToDb(normalizedValue) * 10.0f);
        if (cachedTenths == nextTenths)
            return false;

        cachedTenths = nextTenths;
        text = juce::String(static_cast<float>(nextTenths) * 0.1f, 1);
        return true;
    };

    const auto loudestVisibleChannel = [](float left,
                                          float right,
                                          int channelCount)
    {
        return channelCount > 1 ? juce::jmax(left, right) : left;
    };

    const auto inputChannelCount = processor.getTotalNumInputChannels();
    const auto outputChannelCount = processor.getTotalNumOutputChannels();

    bool changed = updateText(inputPeakText, inputPeakTenths,
                              loudestVisibleChannel(
                                  vuMeterIn.getPeakLeftChannelLevel(),
                                  vuMeterIn.getPeakRightChannelLevel(),
                                  inputChannelCount));
    changed = updateText(inputRmsText, inputRmsTenths,
                         loudestVisibleChannel(
                             vuMeterIn.getRmsLeftChannelLevel(),
                             vuMeterIn.getRmsRightChannelLevel(),
                             inputChannelCount)) || changed;
    changed = updateText(outputPeakText, outputPeakTenths,
                         loudestVisibleChannel(
                             vuMeterOut.getPeakLeftChannelLevel(),
                             vuMeterOut.getPeakRightChannelLevel(),
                             outputChannelCount)) || changed;
    changed = updateText(outputRmsText, outputRmsTenths,
                         loudestVisibleChannel(
                             vuMeterOut.getRmsLeftChannelLevel(),
                             vuMeterOut.getRmsRightChannelLevel(),
                             outputChannelCount)) || changed;
    return changed;
}
