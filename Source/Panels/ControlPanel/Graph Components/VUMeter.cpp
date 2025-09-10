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
    // ++ 关键修复：在使用前，将所有电平值严格限制在 [0, 1] 范围内 ++
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

    // =============================================================================
    // ++ 最终修正：只有当峰值大于一个很小的阈值时才绘制峰值线 ++
    // =============================================================================
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
    float updatedCh0Level = 0.0f;
    float updatedCh1Level = 0.0f;

    if (mIsInput) // input
    {
        updatedCh0Level = mProcessor->getMeterRMSLevel(true, 0, mBandIndex);
        updatedCh1Level = mProcessor->getMeterRMSLevel(true, 1, mBandIndex);
    }
    else // output
    {
        updatedCh0Level = mProcessor->getMeterRMSLevel(false, 0, mBandIndex);
        updatedCh1Level = mProcessor->getMeterRMSLevel(false, 1, mBandIndex);
    }

    // update max values
    mMaxCh0Level = juce::jmax(mMaxCh0Level, updatedCh0Level);
    mMaxCh1Level = juce::jmax(mMaxCh1Level, updatedCh1Level);

    if (updatedCh0Level > mCh0Level)
    {
        mCh0Level = updatedCh0Level;
    }
    else
    {
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

    mCh0Level = helper_denormalize(mCh0Level);
    mCh1Level = helper_denormalize(mCh1Level);

    // decay max values
    if (mMaxCh0Level > mCh0Level || mMaxCh1Level > mCh1Level)
    {
        if (mMaxValueDecayCounter < MAX_VALUE_HOLD_FRAMES)
        {
            ++mMaxValueDecayCounter;
        }
        else
        {
            mMaxCh0Level -= 0.01f;
            mMaxCh1Level -= 0.01f;
        }
    }
    else
    {
        mMaxValueDecayCounter = 0;
    }

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