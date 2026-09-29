#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <limits>

namespace fire::dsp
{
// A Drive estimate, not a loudness detector. Normal Drive D uses 2^(6.5 D/100),
// so this exponent preserves the familiar -0.1 D dB estimate while following
// the gain actually used after LFO, Safe, and the Drive power transition.
class DriveCompensation
{
public:
    static constexpr float exponent = -10.0f / (6.5f * 20.0f * 0.3010299956639812f);

    void prepare(double sampleRate) noexcept
    {
        preparedRate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        enabledMix.reset(preparedRate, 0.02);
        reset();
    }

    void reset() noexcept
    {
        enabledMix.setCurrentAndTargetValue(0.0f);
        initialised = false;
        lastModern = lastEnabled = false;
        previousDrive = std::numeric_limits<float>::quiet_NaN();
        targetGain = lastGain = displayedGain = 1.0f;
        displayedDb = 0.0f;
    }

    void setMode(bool modern, bool enabled) noexcept
    {
        const bool requested = modern && enabled;
        if (! initialised)
        {
            enabledMix.setCurrentAndTargetValue(requested ? 1.0f : 0.0f);
            initialised = true;
        }
        else if (modern != lastModern || requested != lastEnabled)
        {
            // Moving the trim between Drive and legacy Output shares Output's
            // 50 ms time scale. Ordinary Comp switches retain their 20 ms fade.
            // Preserve the current mix when a request interrupts either fade.
            const auto current = enabledMix.getCurrentValue();
            enabledMix.reset(preparedRate, modern != lastModern ? 0.05 : 0.02);
            enabledMix.setCurrentAndTargetValue(current);
            enabledMix.setTargetValue(requested ? 1.0f : 0.0f);
        }
        lastModern = modern;
        lastEnabled = requested;
    }

    bool isActive() const noexcept
    { return enabledMix.getCurrentValue() > 0.0f || enabledMix.getTargetValue() > 0.0f; }

    // Call exactly once per base-rate sample, and hold the result across HQ
    // subframes. Shape's joined dry mix may remove part or all of Drive.
    float next(float actualDriveGain, float audibleDriveMix = 1.0f) noexcept
    {
        const auto mix = enabledMix.getNextValue();
        if (mix <= 0.0f) return lastGain = 1.0f;
        const auto drive = std::isfinite(actualDriveGain) ? juce::jmax(1.0f, actualDriveGain) : 1.0f;
        const auto contribution = juce::jlimit(0.0f, 1.0f,
            std::isfinite(audibleDriveMix) ? audibleDriveMix : 1.0f);
        const auto effectiveDrive = 1.0f + contribution * (drive - 1.0f);
        if (! juce::exactlyEqual(effectiveDrive, previousDrive))
        {
            previousDrive = effectiveDrive;
            targetGain = effectiveDrive > 1.0f ? std::pow(effectiveDrive, exponent) : 1.0f;
        }
        lastGain = 1.0f + mix * (targetGain - 1.0f);
        return lastGain;
    }

    // Only the audio thread calls this; publish the result once per range.
    // Stable settings incur neither a per-sample pow nor a per-block log.
    float getLastGainDb() noexcept
    {
        if (! juce::exactlyEqual(lastGain, displayedGain))
        {
            displayedGain = lastGain;
            displayedDb = juce::Decibels::gainToDecibels(lastGain);
        }
        return displayedDb;
    }

private:
    juce::SmoothedValue<float> enabledMix;
    double preparedRate = 48000.0;
    float previousDrive = std::numeric_limits<float>::quiet_NaN();
    float targetGain = 1.0f, lastGain = 1.0f, displayedGain = 1.0f, displayedDb = 0.0f;
    bool initialised = false, lastModern = false, lastEnabled = false;
};
} // namespace fire::dsp
