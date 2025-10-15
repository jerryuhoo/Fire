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
    // Ensure all level values are clamped between [0, 1] for drawing
    const auto rms0 = juce::jlimit(0.0f, 1.0f, mRmsCh0Level);
    const auto rms1 = juce::jlimit(0.0f, 1.0f, mRmsCh1Level);
    const auto peak0 = juce::jlimit(0.0f, 1.0f, mPeakCh0Level);
    const auto peak1 = juce::jlimit(0.0f, 1.0f, mPeakCh1Level);
    const auto peakHold0 = juce::jlimit(0.0f, 1.0f, mPeakHoldCh0Level);
    const auto peakHold1 = juce::jlimit(0.0f, 1.0f, mPeakHoldCh1Level);

    // 1. Draw Backgrounds
    g.setColour(COLOUR6);
    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        g.fillRect(leftMeterBounds);
        g.fillRect(rightMeterBounds);
    }
    else // Mono
    {
        g.fillRect(leftMeterBounds);
    }

    // 2. Calculate Fill Heights
    const auto h = (float) getHeight();
    const auto rmsCh0FillY = h - h * rms0;
    const auto rmsCh1FillY = h - h * rms1;
    const auto peakCh0FillY = h - h * peak0;
    const auto peakCh1FillY = h - h * peak1;

    // 3. Draw Peak Level Bars (Light color)
    g.setColour(juce::Colours::yellowgreen.withAlpha(0.6f));
    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        g.fillRect((float) leftMeterBounds.getX(), peakCh0FillY, (float) leftMeterBounds.getWidth(), h - peakCh0FillY);
        g.fillRect((float) rightMeterBounds.getX(), peakCh1FillY, (float) rightMeterBounds.getWidth(), h - peakCh1FillY);
    }
    else // Mono
    {
        g.fillRect((float) leftMeterBounds.getX(), peakCh0FillY, (float) leftMeterBounds.getWidth(), h - peakCh0FillY);
    }

    // 4. Draw RMS Level Bars (Dark color)
    g.setColour(juce::Colours::yellowgreen);
    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        g.fillRect((float) leftMeterBounds.getX(), rmsCh0FillY, (float) leftMeterBounds.getWidth(), h - rmsCh0FillY);
        g.fillRect((float) rightMeterBounds.getX(), rmsCh1FillY, (float) rightMeterBounds.getWidth(), h - rmsCh1FillY);
    }
    else // Mono
    {
        g.fillRect((float) leftMeterBounds.getX(), rmsCh0FillY, (float) leftMeterBounds.getWidth(), h - rmsCh0FillY);
    }

    // 5. Draw Peak-Hold Lines
    g.setColour(juce::Colours::yellowgreen.withBrightness(0.5f));
    const auto peakHoldCh0Y = h - h * peakHold0;
    const auto peakHoldCh1Y = h - h * peakHold1;

    const float peakLineThreshold = 0.0001f;

    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        if (peakHold0 > peakLineThreshold)
            g.drawLine((float) leftMeterBounds.getX(), peakHoldCh0Y, (float) leftMeterBounds.getRight(), peakHoldCh0Y, 2.0f);

        if (peakHold1 > peakLineThreshold)
            g.drawLine((float) rightMeterBounds.getX(), peakHoldCh1Y, (float) rightMeterBounds.getRight(), peakHoldCh1Y, 2.0f);
    }
    else // Mono
    {
        if (peakHold0 > peakLineThreshold)
            g.drawLine((float) leftMeterBounds.getX(), peakHoldCh0Y, (float) leftMeterBounds.getRight(), peakHoldCh0Y, 2.0f);
    }
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
}

void VUMeter::setParameters(bool isInput, int bandIndex)
{
    mIsInput = isInput;
    mBandIndex = bandIndex;
}

void VUMeter::updateLevels(const MeterValues& latestValues)
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
            rawRmsCh0 = latestValues.bandInputRMS_L[mBandIndex];
            rawRmsCh1 = latestValues.bandInputRMS_R[mBandIndex];
            rawPeakCh0 = latestValues.bandInputPeak_L[mBandIndex];
            rawPeakCh1 = latestValues.bandInputPeak_R[mBandIndex];
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
            rawRmsCh0 = latestValues.bandOutputRMS_L[mBandIndex];
            rawRmsCh1 = latestValues.bandOutputRMS_R[mBandIndex];
            rawPeakCh0 = latestValues.bandOutputPeak_L[mBandIndex];
            rawPeakCh1 = latestValues.bandOutputPeak_R[mBandIndex];
        }
    }

    // --- The rest of the function remains identical ---

    // 2. Convert from linear gain to normalized dB for UI display.
    float updatedRmsCh0 = dBToNormalizedGain(rawRmsCh0);
    float updatedRmsCh1 = dBToNormalizedGain(rawRmsCh1);
    float updatedPeakCh0 = dBToNormalizedGain(rawPeakCh0);
    float updatedPeakCh1 = dBToNormalizedGain(rawPeakCh1);

    // 3. Apply smoothing to RMS for a more stable visual.
    auto applySmoothing = [](float current, float target)
    {
        if (target > current)
            return target; // Fast attack
        return current + 0.1f * (target - current); // Slow release
    };

    mRmsCh0Level = applySmoothing(mRmsCh0Level, updatedRmsCh0);
    mRmsCh1Level = applySmoothing(mRmsCh1Level, updatedRmsCh1);

    // Peak levels jump immediately (no smoothing)
    mPeakCh0Level = updatedPeakCh0;
    mPeakCh1Level = updatedPeakCh1;

    // 4. Update peak-hold levels.
    mPeakHoldCh0Level = juce::jmax(mPeakHoldCh0Level, mPeakCh0Level);
    mPeakHoldCh1Level = juce::jmax(mPeakHoldCh1Level, mPeakCh1Level);

    // 5. Handle decay logic for the peak-hold line.
    if (mPeakHoldCh0Level > mPeakCh0Level || mPeakHoldCh1Level > mPeakCh1Level)
    {
        if (mPeakHoldDecayCounter < PEAK_HOLD_FRAMES)
        {
            ++mPeakHoldDecayCounter;
        }
        else
        {
            mPeakHoldCh0Level -= 0.01f;
            mPeakHoldCh1Level -= 0.01f;
        }
    }
    else
    {
        mPeakHoldDecayCounter = 0;
    }

    // Prevent denormalization
    mRmsCh0Level = helper_denormalize(mRmsCh0Level);
    mRmsCh1Level = helper_denormalize(mRmsCh1Level);
    mPeakCh0Level = helper_denormalize(mPeakCh0Level);
    mPeakCh1Level = helper_denormalize(mPeakCh1Level);
    mPeakHoldCh0Level = helper_denormalize(mPeakHoldCh0Level);
    mPeakHoldCh1Level = helper_denormalize(mPeakHoldCh1Level);

    // 6. Trigger a repaint.
    repaint();
}

// Getter implementations
float VUMeter::getRmsLeftChannelLevel() { return mRmsCh0Level; }
float VUMeter::getRmsRightChannelLevel() { return mRmsCh1Level; }
float VUMeter::getPeakLeftChannelLevel() { return mPeakHoldCh0Level; }
float VUMeter::getPeakRightChannelLevel() { return mPeakHoldCh1Level; }