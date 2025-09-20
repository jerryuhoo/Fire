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
      mCh0Level(0),
      mCh1Level(0),
      mMaxCh0Level(-96.0f),
      mMaxCh1Level(-96.0f),
      mMaxValueDecayCounter(0)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(60);
}

VUMeter::~VUMeter()
{
    stopTimer();
}

void VUMeter::paint(juce::Graphics& g)
{
    // Ensure all level values are clamped between [0, 1]
    const auto level0 = juce::jlimit(0.0f, 1.0f, mCh0Level);
    const auto level1 = juce::jlimit(0.0f, 1.0f, mCh1Level);
    const auto maxLevel0 = juce::jlimit(0.0f, 1.0f, mMaxCh0Level);
    const auto maxLevel1 = juce::jlimit(0.0f, 1.0f, mMaxCh1Level);

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

    // 2. Calculate Fill Heights using the clamped values
    const auto h = (float) getHeight();
    const auto ch0fill_y = h - h * level0;
    const auto ch1fill_y = h - h * level1;

    // 3. Draw VU Meter Values (the moving bars)
    g.setColour(juce::Colours::yellowgreen.withBrightness(0.9));
    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        g.fillRect((float) leftMeterBounds.getX(), ch0fill_y, (float) leftMeterBounds.getWidth(), h - ch0fill_y);
        g.fillRect((float) rightMeterBounds.getX(), ch1fill_y, (float) rightMeterBounds.getWidth(), h - ch1fill_y);
    }
    else // Mono
    {
        g.fillRect((float) leftMeterBounds.getX(), ch0fill_y, (float) leftMeterBounds.getWidth(), h - ch0fill_y);
    }

    // 4. Draw Peak Level Lines using the clamped values
    g.setColour(juce::Colours::yellowgreen.withBrightness(0.5));
    const auto maxCh0_y = h - h * maxLevel0;
    const auto maxCh1_y = h - h * maxLevel1;

    const float peakLineThreshold = 0.0001f;

    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        if (maxLevel0 > peakLineThreshold)
            g.drawLine((float) leftMeterBounds.getX(), maxCh0_y, (float) leftMeterBounds.getRight(), maxCh0_y, 2.0f);

        if (maxLevel1 > peakLineThreshold)
            g.drawLine((float) rightMeterBounds.getX(), maxCh1_y, (float) rightMeterBounds.getRight(), maxCh1_y, 2.0f);
    }
    else // Mono
    {
        if (maxLevel0 > peakLineThreshold)
            g.drawLine((float) leftMeterBounds.getX(), maxCh0_y, (float) leftMeterBounds.getRight(), maxCh0_y, 2.0f);
    }
}

void VUMeter::resized()
{
    // All layout logic now lives here. This is only called when the component size changes.
    auto bounds = getLocalBounds();

    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        // Stereo layout: two bars with a gap in the middle
        auto meterWidth = bounds.getWidth() / 3;
        leftMeterBounds = bounds.removeFromLeft(meterWidth);
        rightMeterBounds = bounds.removeFromRight(meterWidth);
    }
    else // Mono layout
    {
        // Mono layout: one bar centered in the available space
        auto meterWidth = bounds.getWidth() / 3;
        leftMeterBounds = bounds.reduced((bounds.getWidth() - meterWidth) / 2, 0);
        rightMeterBounds = {}; // Not used in mono, so clear it
    }
}

void VUMeter::setParameters(bool isInput, int bandIndex)
{
    mIsInput = isInput;
    mBandIndex = bandIndex;
}

void VUMeter::timerCallback()
{
    float rawCh0Level = 0.0f; // Default to 0.0f linear, which is -inf dB
    float rawCh1Level = 0.0f;

    // 1. Safely fetch the latest meter data from the processor via public getters.
    // This call is thread-safe because the processor's getters read from std::atomic variables.
    if (mIsInput) // This meter is displaying the input signal
    {
        if (mBandIndex == -1) // Global meter
        {
            rawCh0Level = mProcessor->getGlobalInputMeterLevel(0);
            rawCh1Level = mProcessor->getGlobalInputMeterLevel(1);
        }
        else // Per-band meter
        {
            rawCh0Level = mProcessor->getBandInputMeterLevel(mBandIndex, 0);
            rawCh1Level = mProcessor->getBandInputMeterLevel(mBandIndex, 1);
        }
    }
    else // This meter is displaying the output signal
    {
        if (mBandIndex == -1) // Global meter
        {
            rawCh0Level = mProcessor->getGlobalOutputMeterLevel(0);
            rawCh1Level = mProcessor->getGlobalOutputMeterLevel(1);
        }
        else // Per-band meter
        {
            rawCh0Level = mProcessor->getBandOutputMeterLevel(mBandIndex, 0);
            rawCh1Level = mProcessor->getBandOutputMeterLevel(mBandIndex, 1);
        }
    }

    // 2. Convert the raw linear RMS values to normalized [0, 1] values suitable for the UI.
    float updatedCh0Level = dBToNormalizedGain(rawCh0Level);
    float updatedCh1Level = dBToNormalizedGain(rawCh1Level);

    // 3. Update the peak level display.
    // jmax ensures we always hold the highest value seen so far.
    mMaxCh0Level = juce::jmax(mMaxCh0Level, updatedCh0Level);
    mMaxCh1Level = juce::jmax(mMaxCh1Level, updatedCh1Level);

    // 4. Apply smoothing to the real-time level for a better visual experience.
    if (updatedCh0Level > mCh0Level)
    {
        // If the new level is higher, jump to it immediately (Fast Attack).
        mCh0Level = updatedCh0Level;
    }
    else
    {
        // If the new level is lower, fall to it slowly (Slow Release).
        // SMOOTH_COEFF controls the decay speed.
        mCh0Level = SMOOTH_COEFF * (mCh0Level - updatedCh0Level) + updatedCh0Level;
    }

    if (updatedCh1Level > mCh1Level)
    {
        mCh1Level = updatedCh1Level;
    }
    else
    {
        mCh1Level = SMOOTH_COEFF * (mCh1Level - updatedCh1Level) + updatedCh1Level;
    }

    // 5. Prevent denormalization issues where values become too small.
    mCh0Level = helper_denormalize(mCh0Level);
    mCh1Level = helper_denormalize(mCh1Level);

    // 6. Handle the decay logic for the peak level line.
    // The peak line holds for a set number of frames before starting to fall.
    if (mMaxCh0Level > mCh0Level || mMaxCh1Level > mCh1Level)
    {
        if (mMaxValueDecayCounter < MAX_VALUE_HOLD_FRAMES)
        {
            ++mMaxValueDecayCounter;
        }
        else
        {
            // After the hold time, the peak line starts to decay slowly.
            mMaxCh0Level -= 0.01f;
            mMaxCh1Level -= 0.01f;
        }
    }
    else
    {
        // If the real-time level catches up to the peak, reset the hold counter.
        mMaxValueDecayCounter = 0;
    }

    // 7. Trigger a repaint to update the component on screen.
    repaint();
}

float VUMeter::getLeftChannelLevel()
{
    return mCh0Level;
}

float VUMeter::getRightChannelLevel()
{
    return mCh1Level;
}

float VUMeter::getLeftChannelPeakLevel()
{
    return mMaxCh0Level;
}

float VUMeter::getRightChannelPeakLevel()
{
    return mMaxCh1Level;
}