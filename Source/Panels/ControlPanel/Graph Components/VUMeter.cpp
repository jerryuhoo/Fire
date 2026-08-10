/*
  ==============================================================================

    VUMeter.cpp
    Created: 25 Jan 2021 2:55:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "VUMeter.h"
#include "../../../GUI/InterfaceDefines.h"
#include "../../../Utility/AudioHelpers.h"

#include <array>

//==============================================================================
VUMeter::VUMeter(FireAudioProcessor* inProcessor)
    : mProcessor(inProcessor),
      mIsInput(true),
      mBandIndex(-1),
      mRmsCh0Level(0.0f),
      mRmsCh1Level(0.0f),
      mPeakCh0Level(0.0f),
      mPeakCh1Level(0.0f),
      mPeakHoldCh0Level(0.0f),
      mPeakHoldCh1Level(0.0f),
      mPeakHoldDecayCounter(0)
{
    setInterceptsMouseClicks(false, false);
}

VUMeter::~VUMeter()
{
}

void VUMeter::paint(juce::Graphics& g)
{
    const auto displayScale = juce::jmax(
        0.25f,
        g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto channelCount = mProcessor->getTotalNumInputChannels();
    const auto logicalBounds = getLocalBounds();
    const auto expectedWidth = juce::jmax(
        1,
        juce::roundToInt(static_cast<float>(logicalBounds.getWidth()) * displayScale));
    const auto expectedHeight = juce::jmax(
        1,
        juce::roundToInt(static_cast<float>(logicalBounds.getHeight()) * displayScale));
    if (! backgroundCache.isValid()
        || backgroundCache.getWidth() != expectedWidth
        || backgroundCache.getHeight() != expectedHeight
        || backgroundCacheBounds != logicalBounds
        || ! juce::approximatelyEqual(backgroundCacheScale, displayScale)
        || cachedChannelCount != channelCount
        || cachedIsInput != mIsInput)
    {
        rebuildBackgroundCache(displayScale);
    }

    if (backgroundCache.isValid())
        g.drawImage(backgroundCache, logicalBounds.toFloat());

    const auto rms0 = juce::jlimit(0.0f, 1.0f, mRmsCh0Level);
    const auto rms1 = juce::jlimit(0.0f, 1.0f, mRmsCh1Level);
    const auto peak0 = juce::jlimit(0.0f, 1.0f, mPeakCh0Level);
    const auto peak1 = juce::jlimit(0.0f, 1.0f, mPeakCh1Level);
    const auto peakHold0 = juce::jlimit(0.0f, 1.0f, mPeakHoldCh0Level);
    const auto peakHold1 = juce::jlimit(0.0f, 1.0f, mPeakHoldCh1Level);

    const auto accent = mIsInput ? fire::ui::colours::flame
                                 : fire::ui::colours::positive;
    const auto drawChannel = [&g, accent, this](juce::Rectangle<int> bounds,
                                                float rms,
                                                float peak,
                                                float peakHold)
    {
        if (bounds.isEmpty())
            return;

        const auto floatBounds = bounds.toFloat().reduced(1.0f);
        const auto levelToY = [floatBounds](float level)
        {
            return floatBounds.getBottom() - floatBounds.getHeight() * juce::jlimit(0.0f, 1.0f, level);
        };

        const auto peakY = levelToY(peak);
        const auto rmsY = levelToY(rms);
        g.setColour(accent.withAlpha(0.22f));
        g.fillRect(floatBounds.withTop(peakY));

        g.setGradientFill(meterGradient);
        g.fillRect(floatBounds.withTop(rmsY));

        if (peakHold > 0.0001f)
        {
            const auto holdY = levelToY(peakHold);
            g.setColour(fire::ui::colours::whiteHot.withAlpha(0.30f));
            g.drawLine(floatBounds.getX(), holdY, floatBounds.getRight(), holdY, 4.0f);
            g.setColour(fire::ui::colours::whiteHot);
            g.drawLine(floatBounds.getX(), holdY, floatBounds.getRight(), holdY, 1.2f);
        }
    };

    drawChannel(leftMeterBounds, rms0, peak0, peakHold0);
    if (channelCount == 2)
        drawChannel(rightMeterBounds, rms1, peak1, peakHold1);
}

void VUMeter::resized()
{
    auto bounds = getLocalBounds();

    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        // Stereo layout: two bars with a gap
        auto meterWidth = bounds.getWidth() / 3;
        leftMeterBounds = bounds.removeFromLeft(meterWidth);
        rightMeterBounds = bounds.removeFromRight(meterWidth);
    }
    else // Mono layout
    {
        // Mono layout: one centered bar
        auto meterWidth = bounds.getWidth() / 3;
        leftMeterBounds = bounds.reduced((bounds.getWidth() - meterWidth) / 2, 0);
        rightMeterBounds = {}; // Not used
    }

    backgroundCache = {};
    backgroundCacheBounds = {};
    backgroundCacheScale = 0.0f;
}

void VUMeter::setParameters(bool isInput, int bandIndex)
{
    if (mIsInput == isInput && mBandIndex == bandIndex)
        return;

    const bool appearanceChanged = mIsInput != isInput;
    mIsInput = isInput;
    mBandIndex = bandIndex;
    if (appearanceChanged)
    {
        backgroundCache = {};
        backgroundCacheBounds = {};
        backgroundCacheScale = 0.0f;
    }
}

bool VUMeter::updateLevels(const MeterValues& latestValues)
{
    float rawRmsCh0 = 0.0f, rawRmsCh1 = 0.0f, rawPeakCh0 = 0.0f, rawPeakCh1 = 0.0f;
    const bool isGlobal = (mBandIndex == -1);

    if (mIsInput)
    {
        if (isGlobal)
        {
            rawRmsCh0 = latestValues.inputRMS_L;
            rawRmsCh1 = latestValues.inputRMS_R;
            rawPeakCh0 = latestValues.inputPeak_L;
            rawPeakCh1 = latestValues.inputPeak_R;
        }
        else if (juce::isPositiveAndBelow(mBandIndex, 4))
        {
            const auto bandIndex = static_cast<size_t>(mBandIndex);
            rawRmsCh0 = latestValues.bandInputRMS_L[bandIndex];
            rawRmsCh1 = latestValues.bandInputRMS_R[bandIndex];
            rawPeakCh0 = latestValues.bandInputPeak_L[bandIndex];
            rawPeakCh1 = latestValues.bandInputPeak_R[bandIndex];
        }
    }
    else // Output
    {
        if (isGlobal)
        {
            rawRmsCh0 = latestValues.outputRMS_L;
            rawRmsCh1 = latestValues.outputRMS_R;
            rawPeakCh0 = latestValues.outputPeak_L;
            rawPeakCh1 = latestValues.outputPeak_R;
        }
        else if (juce::isPositiveAndBelow(mBandIndex, 4))
        {
            const auto bandIndex = static_cast<size_t>(mBandIndex);
            rawRmsCh0 = latestValues.bandOutputRMS_L[bandIndex];
            rawRmsCh1 = latestValues.bandOutputRMS_R[bandIndex];
            rawPeakCh0 = latestValues.bandOutputPeak_L[bandIndex];
            rawPeakCh1 = latestValues.bandOutputPeak_R[bandIndex];
        }
    }

    return updateBallistics(dBToNormalizedGain(rawRmsCh0),
                            dBToNormalizedGain(rawRmsCh1),
                            dBToNormalizedGain(rawPeakCh0),
                            dBToNormalizedGain(rawPeakCh1));
}

bool VUMeter::decayToSilence()
{
    return updateBallistics(0.0f, 0.0f, 0.0f, 0.0f);
}

bool VUMeter::updateBallistics(float updatedRmsCh0,
                               float updatedRmsCh1,
                               float updatedPeakCh0,
                               float updatedPeakCh1)
{
    const std::array<float, 6> previous {
        mRmsCh0Level, mRmsCh1Level, mPeakCh0Level,
        mPeakCh1Level, mPeakHoldCh0Level, mPeakHoldCh1Level
    };

    auto applySmoothing = [](float current, float target)
    {
        if (target > current)
            return target;
        return current + 0.14f * (target - current);
    };

    mRmsCh0Level = applySmoothing(mRmsCh0Level, updatedRmsCh0);
    mRmsCh1Level = applySmoothing(mRmsCh1Level, updatedRmsCh1);

    mPeakCh0Level = applySmoothing(mPeakCh0Level, updatedPeakCh0);
    mPeakCh1Level = applySmoothing(mPeakCh1Level, updatedPeakCh1);

    // 4. Update peak-hold levels.
    mPeakHoldCh0Level = juce::jmax(mPeakHoldCh0Level, mPeakCh0Level);
    mPeakHoldCh1Level = juce::jmax(mPeakHoldCh1Level, mPeakCh1Level);

    if (mPeakHoldCh0Level > mPeakCh0Level || mPeakHoldCh1Level > mPeakCh1Level)
    {
        if (mPeakHoldDecayCounter < peakHoldFrames)
        {
            ++mPeakHoldDecayCounter;
        }
        else
        {
            mPeakHoldCh0Level = juce::jmax(0.0f, mPeakHoldCh0Level - 0.018f);
            mPeakHoldCh1Level = juce::jmax(0.0f, mPeakHoldCh1Level - 0.018f);
        }
    }
    else
    {
        mPeakHoldDecayCounter = 0;
    }

    mRmsCh0Level = helper_denormalize(mRmsCh0Level);
    mRmsCh1Level = helper_denormalize(mRmsCh1Level);
    mPeakCh0Level = helper_denormalize(mPeakCh0Level);
    mPeakCh1Level = helper_denormalize(mPeakCh1Level);
    mPeakHoldCh0Level = helper_denormalize(mPeakHoldCh0Level);
    mPeakHoldCh1Level = helper_denormalize(mPeakHoldCh1Level);

    const std::array<float, 6> current {
        mRmsCh0Level, mRmsCh1Level, mPeakCh0Level,
        mPeakCh1Level, mPeakHoldCh0Level, mPeakHoldCh1Level
    };
    for (size_t i = 0; i < current.size(); ++i)
        if (std::abs(current[i] - previous[i]) > 0.0001f)
            return true;

    return false;
}

float VUMeter::getRmsLeftChannelLevel() const noexcept { return mRmsCh0Level; }
float VUMeter::getRmsRightChannelLevel() const noexcept { return mRmsCh1Level; }
float VUMeter::getPeakLeftChannelLevel() const noexcept { return mPeakHoldCh0Level; }
float VUMeter::getPeakRightChannelLevel() const noexcept { return mPeakHoldCh1Level; }

void VUMeter::rebuildBackgroundCache(float displayScale)
{
    if (getWidth() <= 0 || getHeight() <= 0)
    {
        backgroundCache = {};
        backgroundCacheBounds = {};
        backgroundCacheScale = 0.0f;
        return;
    }

    displayScale = juce::jmax(0.25f, displayScale);
    backgroundCacheBounds = getLocalBounds();
    backgroundCacheScale = displayScale;
    cachedChannelCount = mProcessor->getTotalNumInputChannels();
    cachedIsInput = mIsInput;
    backgroundCache = juce::Image(
        juce::Image::ARGB,
        juce::jmax(1, juce::roundToInt(static_cast<float>(getWidth()) * displayScale)),
        juce::jmax(1, juce::roundToInt(static_cast<float>(getHeight()) * displayScale)),
        true);
    juce::Graphics cacheGraphics(backgroundCache);
    cacheGraphics.addTransform(juce::AffineTransform::scale(displayScale));

    const auto accent = mIsInput ? fire::ui::colours::flame
                                 : fire::ui::colours::positive;
    meterGradient = juce::ColourGradient(accent.darker(0.10f),
                                         getWidth() * 0.5f,
                                         static_cast<float>(getHeight()),
                                         fire::ui::colours::whiteHot,
                                         getWidth() * 0.5f,
                                         0.0f,
                                         false);
    meterGradient.addColour(0.68, accent);

    const auto drawTrack = [&cacheGraphics](juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty())
            return;

        const auto track = bounds.toFloat();
        cacheGraphics.setColour(fire::ui::colours::canvas.withAlpha(0.92f));
        cacheGraphics.fillRoundedRectangle(track, 2.0f);
        cacheGraphics.setColour(fire::ui::colours::hairline.withAlpha(0.85f));
        cacheGraphics.drawRoundedRectangle(track.reduced(0.5f), 2.0f, 1.0f);

        cacheGraphics.setColour(fire::ui::colours::textMuted.withAlpha(0.22f));
        for (int division = 1; division < 8; ++division)
        {
            const auto y = track.getY() + track.getHeight() * division / 8.0f;
            cacheGraphics.drawHorizontalLine(juce::roundToInt(y),
                                             track.getX() + 1.0f,
                                             track.getRight() - 1.0f);
        }
    };

    drawTrack(leftMeterBounds);
    if (cachedChannelCount == 2)
        drawTrack(rightMeterBounds);
}
