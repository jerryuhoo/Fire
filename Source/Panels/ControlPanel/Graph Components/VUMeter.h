/*
  ==============================================================================

    VUMeter.h
    Created: 25 Jan 2021 2:55:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../../PluginProcessor.h"

//==============================================================================
/*
*/
class VUMeter : public juce::Component
{
public:
    VUMeter(FireAudioProcessor* inProcessor);
    ~VUMeter() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void setParameters(bool isInput, int bandIndex);

    // Getters for RMS and Peak levels
    float getRmsLeftChannelLevel();
    float getRmsRightChannelLevel();
    float getPeakLeftChannelLevel();
    float getPeakRightChannelLevel();
    void updateLevels(const MeterValues& latestValues);

private:
    FireAudioProcessor* mProcessor;
    bool mIsInput;
    int mBandIndex;

    // RMS values for the dark bars
    float mRmsCh0Level;
    float mRmsCh1Level;

    // Peak values for the light bars
    float mPeakCh0Level;
    float mPeakCh1Level;

    // Peak-hold values for the thin lines
    float mPeakHoldCh0Level;
    float mPeakHoldCh1Level;

    int mPeakHoldDecayCounter;
    const int PEAK_HOLD_FRAMES = 60; // How many frames to hold the peak line

    juce::Rectangle<int> leftMeterBounds;
    juce::Rectangle<int> rightMeterBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};