#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace fire::lfo_performance
{
enum class Playback : int { loop = 0, retrigger = 1, oneShot = 2 };
enum class Wave : int { shape = 0, hold = 1, glide = 2 };

struct PerformanceConfig
{
    Playback playback = Playback::loop;
    Wave wave = Wave::shape;
    std::uint16_t seed = 0;
    double phaseOffset = 0;
};

struct CyclePosition
{
    std::int64_t cycle = 0;
    double phase = 0;
};

inline CyclePosition splitCycles(double cycles) noexcept
{
    if (!std::isfinite(cycles)) return {};
    const auto nearest = std::round(cycles);
    if (std::abs(cycles - nearest) <= 8 * std::numeric_limits<double>::epsilon()) cycles = nearest;
    const auto integral = std::floor(cycles);
    const auto index = integral >= static_cast<double>(std::numeric_limits<std::int64_t>::max())
        ? std::numeric_limits<std::int64_t>::max()
        : integral <= static_cast<double>(std::numeric_limits<std::int64_t>::min())
            ? std::numeric_limits<std::int64_t>::min() : static_cast<std::int64_t>(integral);
    return {index, cycles - integral};
}

inline float randomForCycle(std::uint16_t seed, std::uint64_t cycle, std::uint64_t triggerSerial = 0) noexcept
{
    // Stateless SplitMix64 avalanche. Negative signed cycle indices are cast
    // modulo 2^64 by the caller, giving defined backwards-timeline behaviour.
    auto value = cycle + 0x9e3779b97f4a7c15ULL * (static_cast<std::uint64_t>(seed) + 1u)
        + 0xd1b54a32d192ed03ULL * triggerSerial;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return static_cast<float>(value >> 40) * (1.0f / 16777215.0f);
}

struct PerformanceFrame
{
    // Shape reads are rotated independently from elapsed trigger progress.
    // At an unrotated OneShot endpoint, shapePhase is exactly 1 (not 0).
    float shapePhase = 0;
    float randomValue = 0;
    float progress = 0;
    bool running = true;
};

// Pure arithmetic state: no JUCE, allocation, atomics, locks or clock reads.
// LfoManager's audio thread owns it. Legacy Loop+Shape bypasses this class.
class PerformanceClock
{
public:
    void reset() noexcept
    {
        localCycles = 0;
        accumulationError = 0;
        triggered = false;
        triggerSerial = 0;
    }

    void configure(PerformanceConfig next) noexcept
    {
        next.phaseOffset = std::isfinite(next.phaseOffset) ? std::clamp(next.phaseOffset, 0.0, 1.0) : 0;
        // A full-turn offset is exactly the same phase as zero, while still
        // retaining the explicit OneShot phase=1 terminal-table lookup.
        if (next.phaseOffset == 1) next.phaseOffset = 0;
        if (next.playback != config.playback || next.seed != config.seed) reset();
        config = next;
    }

    bool trigger(std::uint32_t count = 1) noexcept
    {
        if (config.playback == Playback::loop || count == 0) return false;
        localCycles = 0;
        accumulationError = 0;
        triggered = true;
        triggerSerial += count;
        return true;
    }

    PerformanceFrame next(double increment, std::optional<double> absoluteCycles = {}) noexcept
    {
        const bool oneShot = config.playback == Playback::oneShot;
        const bool running = !oneShot || (triggered && localCycles < 1);
        const double elapsed = config.playback == Playback::loop && absoluteCycles && std::isfinite(*absoluteCycles)
            ? *absoluteCycles : localCycles;
        const auto position = splitCycles(elapsed + config.phaseOffset);
        const bool terminal = oneShot && triggered && localCycles >= 1;
        const auto shapePhase = terminal && config.phaseOffset == 0 ? 1.0f
            : std::min(static_cast<float>(position.phase), std::nextafter(1.0f, 0.0f));
        // Random OneShot uses elapsed progress, without Phase rotation:
        // Hold is constant for this trigger, Glide reaches its next target.
        // Rotating either would introduce a second random segment mid-shot.
        auto randomPosition = position;
        if (oneShot) { randomPosition.cycle = 0; randomPosition.phase = std::clamp(localCycles, 0.0, 1.0); }
        const auto key = static_cast<std::uint64_t>(randomPosition.cycle);
        const auto serial = config.playback == Playback::loop ? 0 : triggerSerial;
        const auto start = randomForCycle(config.seed, key, serial);
        const auto finish = randomForCycle(config.seed, key + 1u, serial);
        const auto t = randomPosition.phase;
        const auto blend = static_cast<float>(t * t * (3 - 2 * t));
        const auto randomValue = config.wave == Wave::glide
            ? (blend >= 1 ? finish : blend <= 0 ? start : start + blend * (finish - start)) : start;
        const PerformanceFrame result {static_cast<float>(shapePhase), randomValue,
            static_cast<float>(oneShot ? std::clamp(localCycles, 0.0, 1.0) : splitCycles(elapsed).phase), running};

        if (running && std::isfinite(increment) && increment > 0)
        {
            if (absoluteCycles && config.playback == Playback::loop)
            { localCycles = elapsed; accumulationError = 0; }
            // Compensated accumulation prevents exact cycle/rotation crossings
            // from remaining infinitesimally below an integer for one sample.
            const auto corrected = increment - accumulationError;
            const auto next = localCycles + corrected;
            accumulationError = (next - localCycles) - corrected;
            if (oneShot) localCycles = next >= 1.0 - 8 * std::numeric_limits<double>::epsilon() ? 1.0 : next;
            else
            {
                localCycles = next;
                if (!std::isfinite(localCycles)) localCycles = 0;
            }
        }
        return result;
    }

private:
    PerformanceConfig config;
    double localCycles = 0;
    double accumulationError = 0;
    bool triggered = false;
    std::uint64_t triggerSerial = 0;
};
}
