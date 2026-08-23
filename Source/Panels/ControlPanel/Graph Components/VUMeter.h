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

    float getRmsLeftChannelLevel() const noexcept;
    float getRmsRightChannelLevel() const noexcept;
    float getPeakLeftChannelLevel() const noexcept;
    float getPeakRightChannelLevel() const noexcept;
    bool updateLevels(const MeterValues& latestValues);
    bool decayToSilence();

private:
    friend struct VUMeterTestAccess;

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
    static constexpr int peakHoldFrames = 42;

    juce::Rectangle<int> leftMeterBounds;
    juce::Rectangle<int> rightMeterBounds;
    juce::Image backgroundCache;
    juce::ColourGradient meterGradient;
    juce::Rectangle<int> backgroundCacheBounds;
    float backgroundCacheScale = 0.0f;
    int cachedChannelCount = 0;
    bool cachedIsInput = true;

    bool updateBallistics(float rmsCh0, float rmsCh1, float peakCh0, float peakCh1);
    void resetLevels() noexcept;
    void updateMeterBounds(int channelCount);
    void rebuildBackgroundCache(float displayScale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};
