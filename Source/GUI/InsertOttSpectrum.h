#pragma once

#include "OttVisuals.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

namespace fire::ui
{
// Reuses the editor's existing FFT for the inserted OTT's spectral colour.
// This is a presentation profile, not another audio tap or an OTT detector.
class InsertOttSpectrum
{
public:
    struct Frame
    {
        OttSpectrumProfile profile {};
        float centroid = 0.5f;
    };

    void reset() noexcept
    {
        targets.fill(0.0f);
        frame = {};
        ageSeconds = 1.0f;
        lastUpdateTimeMs = -1.0;
        lowFrequency = 20.0f;
        highFrequency = 20000.0f;
    }

    void update(const float* magnitudes, int bins, float binWidth,
                const float* reference = nullptr,
                float lowHz = 20.0f, float highHz = 20000.0f) noexcept
    {
        if (magnitudes == nullptr || bins < 2 || ! std::isfinite(binWidth)
            || binWidth <= 0.0f || ! std::isfinite(lowHz) || ! std::isfinite(highHz))
        {
            reset();
            return;
        }

        lowHz = juce::jlimit(20.0f, 20000.0f, lowHz);
        highHz = juce::jlimit(20.0f, 20000.0f, highHz);
        if (highHz <= lowHz)
        {
            reset();
            return;
        }

        lowFrequency = lowHz;
        highFrequency = highHz;
        const auto logRange = std::log(static_cast<double>(highHz) / lowHz);
        OttSpectrumProfile peaks {};
        for (int bin = 1; bin < bins; ++bin)
        {
            const auto frequency = static_cast<double>(bin) * binWidth;
            if (frequency < lowHz || frequency > highHz)
                continue;

            auto magnitude = std::isfinite(magnitudes[bin])
                ? juce::jmax(0.0f, magnitudes[bin]) : 0.0f;
            if (reference != nullptr && std::isfinite(reference[bin]))
                magnitude = juce::jmax(magnitude, reference[bin]);
            if (magnitude <= 0.0f)
                continue;

            const auto position = std::log(frequency / lowHz) / logRange;
            const auto column = static_cast<size_t>(juce::jlimit(0,
                static_cast<int>(peaks.size()) - 1,
                juce::roundToInt(position * static_cast<double>(peaks.size() - 1))));
            // Peak pooling retains a narrow tone even when a whole-band FFT
            // is reduced to the graph's small, fixed-size ripple profile.
            peaks[column] = juce::jmax(peaks[column], magnitude);
        }

        for (auto& peak : peaks)
        {
            const auto db = juce::Decibels::gainToDecibels(
                peak / static_cast<float>(bins), -100.0f);
            peak = juce::jlimit(0.0f, 1.0f, (db + 84.0f) / 72.0f);
        }
        for (size_t column = 0; column < peaks.size(); ++column)
            targets[column] = 0.5f * peaks[column]
                + 0.25f * peaks[column == 0 ? 0 : column - 1]
                + 0.25f * peaks[juce::jmin(column + 1, peaks.size() - 1)];

        ageSeconds = 0.0f;
        lastUpdateTimeMs = juce::Time::getMillisecondCounterHiRes();
    }

    Frame advance(float deltaSeconds) noexcept
    {
        const auto elapsed = std::isfinite(deltaSeconds)
            ? juce::jlimit(0.0f, 1.0f, deltaSeconds) : 0.0f;
        ageSeconds = juce::jmin(1.0f, ageSeconds + elapsed);
        // Wall time also expires a frame while its page/editor was hidden and
        // consequently received no animation ticks.
        if (lastUpdateTimeMs < 0.0 || ageSeconds >= 0.25f
            || juce::Time::getMillisecondCounterHiRes() - lastUpdateTimeMs >= 250.0)
            targets.fill(0.0f);

        const auto dt = juce::jmin(0.05f, elapsed);
        const auto attack = Motion::step(dt, 0.055f);
        const auto release = Motion::step(dt, 0.14f);
        float energy = 0.0f, weightedPosition = 0.0f;
        const auto lowPosition = std::log(lowFrequency / 20.0f) / std::log(1000.0f);
        const auto highPosition = std::log(highFrequency / 20.0f) / std::log(1000.0f);
        for (size_t column = 0; column < frame.profile.size(); ++column)
        {
            auto& value = frame.profile[column];
            const auto target = targets[column];
            value += (target - value) * (target > value ? attack : release);
            if (value < 0.0005f && target == 0.0f)
                value = 0.0f;
            const auto fraction = static_cast<float>(column)
                / static_cast<float>(frame.profile.size() - 1);
            weightedPosition += juce::jmap(fraction, lowPosition, highPosition) * value;
            energy += value;
        }
        frame.centroid = juce::jlimit(0.0f, 1.0f, energy > 0.0001f
            ? weightedPosition / energy : (lowPosition + highPosition) * 0.5f);
        return frame;
    }

private:
    OttSpectrumProfile targets {};
    Frame frame;
    float ageSeconds = 1.0f;
    double lastUpdateTimeMs = -1.0;
    float lowFrequency = 20.0f, highFrequency = 20000.0f;
};
}
