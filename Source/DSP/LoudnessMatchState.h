#pragma once

#include "LoudnessMatcher.h"
#include <array>
#include <atomic>
#include <cstring>
#include <cstdint>

namespace fire::dsp
{
// The gain and its measurement generation share one atomic word. A completed
// audio-thread measurement can never overwrite a newer restore/reset/request.
class LoudnessMatchState
{
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Loudness comparison requires lock-free audio-thread records");
public:
    struct Settings
    {
        bool enabled = false;
        std::array<bool, 2> ready {}, limited {};
        std::array<float, 2> gainDb {};
    };
    struct Frame
    {
        bool enabled = false;
        int side = 0;
        std::array<std::uint64_t, 2> records {};
    };
    struct View
    {
        bool enabled = false, measuring = false, ready = false;
        bool limited = false, noSignal = false, bypassed = false;
        int side = 0;
        float gainDb = 0.0f, progress = 0.0f;
    };

    void prepare(double sampleRate) noexcept
    {
        matcher.prepare(sampleRate);
        gain.reset(std::isfinite(sampleRate) && sampleRate >= 8000.0 && sampleRate <= 768000.0
                       ? sampleRate : 48000.0, 0.05);
        gain.setCurrentAndTargetValue(1.0f);
        lastTargetDb = 0.0f;
        suspendMeasurement();
    }

    Frame capture(int side) const noexcept
    {
        return { enabled.load(std::memory_order_acquire), side == 1 ? 1 : 0,
                 { records[0].load(std::memory_order_acquire), records[1].load(std::memory_order_acquire) } };
    }
    Settings settings() const noexcept
    {
        const auto frame = capture(0);
        Settings result;
        result.enabled = frame.enabled;
        for (size_t i = 0; i < records.size(); ++i)
        {
            result.ready[i] = (frame.records[i] & readyBit) != 0;
            result.limited[i] = (frame.records[i] & limitedBit) != 0;
            result.gainDb[i] = result.ready[i] ? decodeGain(frame.records[i]) : 0.0f;
        }
        return result;
    }
    View view(int side, bool bypassed) const noexcept
    {
        const auto frame = capture(side);
        const auto record = frame.records[static_cast<size_t>(frame.side)];
        View result;
        result.enabled = frame.enabled;
        result.side = frame.side;
        result.bypassed = bypassed;
        result.ready = (record & readyBit) != 0;
        result.measuring = frame.enabled && (record & measuringBit) != 0;
        result.limited = (record & limitedBit) != 0;
        result.noSignal = (record & failedBit) != 0;
        result.gainDb = result.ready ? decodeGain(record) : 0.0f;
        if (result.measuring && progressToken.load(std::memory_order_acquire) == record
            && progressSide.load(std::memory_order_relaxed) == frame.side)
            result.progress = progress.load(std::memory_order_relaxed);
        return result;
    }

    void setEnabled(bool shouldEnable, int side) noexcept
    {
        enabled.store(shouldEnable, std::memory_order_release);
        if (! shouldEnable)
            cancelMeasurements();
        else if ((records[static_cast<size_t>(side == 1 ? 1 : 0)].load() & readyBit) == 0)
            requestLearn(side);
    }
    void requestLearn(int side) noexcept
    {
        enabled.store(true, std::memory_order_release);
        mutate(side, [](std::uint64_t previous)
        { return (previous & (gainMask | readyBit | limitedBit)) | measuringBit; });
    }
    void cancelMeasurements() noexcept
    {
        for (int side = 0; side < 2; ++side)
            mutate(side, [](std::uint64_t previous)
            { return previous & (gainMask | readyBit | limitedBit | failedBit); });
    }
    void cancelMeasurement(int side) noexcept
    {
        // A deliberate Cancel remains idle until Learn is pressed again.
        mutate(side, [](std::uint64_t previous)
        { return (previous & (gainMask | readyBit | limitedBit))
              | ((previous & readyBit) == 0 ? failedBit : 0); });
    }
    void clearSide(int side) noexcept
    {
        mutate(side, [](std::uint64_t) { return std::uint64_t(0); });
    }
    void copyToOtherSide(int sourceSide) noexcept
    {
        sourceSide = sourceSide == 1 ? 1 : 0;
        const auto source = records[static_cast<size_t>(sourceSide)].load(std::memory_order_acquire);
        mutate(1 - sourceSide, [source](std::uint64_t)
        { return source & (gainMask | readyBit | limitedBit); });
    }
    void restore(const Settings& restored) noexcept
    {
        for (int side = 0; side < 2; ++side)
        {
            const auto i = static_cast<size_t>(side);
            const bool valid = restored.ready[i] && std::isfinite(restored.gainDb[i])
                && std::abs(restored.gainDb[i]) <= 18.0f;
            const auto value = valid
                ? encodeGain(restored.gainDb[i]) | readyBit | (restored.limited[i] ? limitedBit : 0)
                : std::uint64_t(0);
            mutate(side, [value](std::uint64_t) { return value; });
        }
        enabled.store(restored.enabled, std::memory_order_release);
        completedNotification.store(false, std::memory_order_release);
    }

    bool needsReference(const Frame& frame) const noexcept
    {
        const auto record = frame.records[static_cast<size_t>(frame.side)];
        return frame.enabled && ((record & measuringBit) != 0
            || (record & (readyBit | failedBit)) == 0);
    }
    // Audio thread only. Retain the pending command so a fresh complete
    // window starts after host bypass or an HQ/topology transition ends.
    void suspendMeasurement() noexcept
    {
        if (activeToken == 0 && matcher.getState() == LoudnessMatcher::State::idle)
            return;
        matcher.cancel();
        activeToken = 0;
        progressToken.store(0, std::memory_order_release);
        progress.store(0.0f, std::memory_order_relaxed);
    }
    void process(const juce::AudioBuffer<float>& reference,
                 juce::AudioBuffer<float>& output, const Frame& frame,
                 bool stableForMeasurement) noexcept
    {
        const auto side = static_cast<size_t>(frame.side);
        const auto record = frame.records[side];
        if (activeToken != 0 && (activeSide != frame.side || ! frame.enabled))
        {
            auto expected = activeToken;
            records[static_cast<size_t>(activeSide)].compare_exchange_strong(
                expected, activeToken & ~measuringBit, std::memory_order_acq_rel);
            suspendMeasurement();
        }
        if (frame.enabled && stableForMeasurement
            && records[side].load(std::memory_order_acquire) == record)
        {
            if ((record & (readyBit | measuringBit | failedBit)) == 0)
            {
                auto expected = record;
                const auto next = nextGeneration(record) | measuringBit;
                records[side].compare_exchange_strong(expected, next, std::memory_order_acq_rel);
            }
            else if ((record & measuringBit) != 0)
            {
                if (activeToken != record || activeSide != frame.side)
                {
                    matcher.start();
                    activeToken = record;
                    activeSide = frame.side;
                }
                matcher.process(reference, output);
                progress.store(matcher.getProgress(), std::memory_order_relaxed);
                progressSide.store(frame.side, std::memory_order_relaxed);
                progressToken.store(record, std::memory_order_release);
                if (matcher.getState() == LoudnessMatcher::State::complete
                    || matcher.getState() == LoudnessMatcher::State::noSignal)
                {
                    const bool complete = matcher.getState() == LoudnessMatcher::State::complete;
                    const auto result = complete
                        ? (record & generationMask) | readyBit | encodeGain(matcher.getGainDb())
                            | (matcher.isLimited() ? limitedBit : 0)
                        : (record & ~measuringBit) | failedBit;
                    auto expected = record;
                    if (records[side].compare_exchange_strong(expected, result, std::memory_order_acq_rel))
                        completedNotification.store(true, std::memory_order_release);
                    suspendMeasurement();
                }
            }
            else if (activeToken != 0)
                suspendMeasurement();
        }
        else
            suspendMeasurement();

        // Use the same captured A/B frame as the audible DSP. Measurements
        // publish for the next callback, never for already-rendered samples.
        const auto targetDb = frame.enabled && (record & readyBit) != 0 ? decodeGain(record) : 0.0f;
        if (! juce::exactlyEqual(targetDb, lastTargetDb))
        {
            lastTargetDb = targetDb;
            gain.setTargetValue(juce::Decibels::decibelsToGain(targetDb));
        }
        if (! gain.isSmoothing())
        {
            if (gain.getCurrentValue() != 1.0f)
                for (int channel = 0; channel < output.getNumChannels(); ++channel)
                {
                    auto* samples = output.getWritePointer(channel);
                    for (int sample = 0; sample < output.getNumSamples(); ++sample)
                    {
                        const auto scaled = samples[sample] * gain.getCurrentValue();
                        samples[sample] = std::isfinite(scaled) ? scaled : 0.0f;
                    }
                }
            return;
        }
        for (int sample = 0; sample < output.getNumSamples(); ++sample)
        {
            const auto nextGain = gain.getNextValue();
            for (int channel = 0; channel < output.getNumChannels(); ++channel)
            {
                auto& value = output.getWritePointer(channel)[sample];
                const auto scaled = value * nextGain;
                value = std::isfinite(scaled) ? scaled : 0.0f;
            }
        }
    }
    bool takeCompletedNotification() noexcept
    { return completedNotification.exchange(false, std::memory_order_acq_rel); }

private:
    static constexpr std::uint64_t gainMask = 0xffffffffu;
    static constexpr std::uint64_t readyBit = std::uint64_t(1) << 32;
    static constexpr std::uint64_t measuringBit = std::uint64_t(1) << 33;
    static constexpr std::uint64_t limitedBit = std::uint64_t(1) << 34;
    static constexpr std::uint64_t failedBit = std::uint64_t(1) << 35;
    static constexpr std::uint64_t generationMask = ~((std::uint64_t(1) << 36) - 1);
    static std::uint64_t encodeGain(float value) noexcept
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    static float decodeGain(std::uint64_t value) noexcept
    {
        const auto bits = static_cast<std::uint32_t>(value & gainMask);
        float gainDb = 0.0f;
        std::memcpy(&gainDb, &bits, sizeof(gainDb));
        return gainDb;
    }
    static std::uint64_t nextGeneration(std::uint64_t value) noexcept
    { return ((value & generationMask) + (std::uint64_t(1) << 36)) & generationMask; }
    template <typename Edit> void mutate(int side, Edit edit) noexcept
    {
        auto& destination = records[static_cast<size_t>(side == 1 ? 1 : 0)];
        auto previous = destination.load(std::memory_order_acquire);
        while (! destination.compare_exchange_weak(previous, nextGeneration(previous) | edit(previous),
                                                    std::memory_order_acq_rel)) {}
    }
    std::atomic<bool> enabled { false }, completedNotification { false };
    std::array<std::atomic<std::uint64_t>, 2> records {{0, 0}};
    std::atomic<std::uint64_t> progressToken { 0 };
    std::atomic<float> progress { 0.0f };
    std::atomic<int> progressSide { 0 };
    LoudnessMatcher matcher;
    juce::SmoothedValue<float> gain;
    float lastTargetDb = 0.0f;
    std::uint64_t activeToken = 0;
    int activeSide = 0;
};
}
