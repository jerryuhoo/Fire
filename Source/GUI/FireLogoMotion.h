#pragma once

#include <algorithm>
#include <cmath>

namespace fire::ui
{
// Message-thread presentation state, advanced by the editor's existing UI clock.
// Meter magnitudes are linear and must come from a newly consumed audio packet
// when freshFrame is true. Holding the last arguments does not keep a fire alive.
class FireLogoMotion
{
public:
    void reset() noexcept
    {
        energyValue = 0.0f;
        attackValue = 0.0f;
        phaseValue = 0.0f;
        targetEnergy = 0.0f;
        previousPeak = 0.0f;
        secondsWithoutFrame = frameDeadline;
    }

    // Returns whether a visible value changed, for a local logo repaint.
    bool advance(float dtSeconds, bool freshFrame, float rms, float peak) noexcept
    {
        if (! std::isfinite(dtSeconds) || dtSeconds <= 0.0f)
            return false;

        const auto oldEnergy = energyValue;
        const auto oldAttack = attackValue;
        const auto oldPhase = phaseValue;

        // One second is enough to exhaust both the watchdog and the release.
        // Bounding a delayed UI tick also avoids unbounded phase arithmetic.
        const auto elapsed = std::min(static_cast<double>(dtSeconds), 1.0);
        if (freshFrame)
        {
            const auto rmsLevel = levelForMagnitude(rms);
            const auto peakLevel = levelForMagnitude(peak);
            targetEnergy = 0.9f * rmsLevel + 0.1f * peakLevel;
            attackValue = std::max(attackValue,
                std::clamp((peakLevel - previousPeak) * 1.25f, 0.0f, 1.0f));
            previousPeak = peakLevel;
            secondsWithoutFrame = 0.0;
            advanceEnvelopes(elapsed);
        }
        else
        {
            const auto heldDuration = std::min(elapsed,
                std::max(0.0, frameDeadline - secondsWithoutFrame));
            advanceEnvelopes(heldDuration);
            secondsWithoutFrame = std::min(frameDeadline,
                secondsWithoutFrame + elapsed);
            if (secondsWithoutFrame >= frameDeadline)
            {
                targetEnergy = 0.0f;
                previousPeak = 0.0f;
            }
            advanceEnvelopes(elapsed - heldDuration);
        }

        return energyValue != oldEnergy || attackValue != oldAttack
            || phaseValue != oldPhase;
    }

    float energy() const noexcept { return energyValue; }
    float attack() const noexcept { return attackValue; }
    // Radians, wrapped to [0, 2 pi). Remains stationary after silence settles.
    float phase() const noexcept { return phaseValue; }
    bool isAnimating() const noexcept { return energyValue > 0.0f || attackValue > 0.0f; }

private:
    static float levelForMagnitude(float magnitude) noexcept
    {
        if (! std::isfinite(magnitude) || magnitude <= 0.001f)
            return 0.0f;

        const auto decibels = 20.0f * std::log10(std::min(magnitude, 1.0f));
        const auto normalised = std::clamp((decibels + 60.0f) / 54.0f, 0.0f, 1.0f);
        return std::pow(normalised, 1.3f);
    }

    void advanceEnvelopes(double elapsed) noexcept
    {
        if (elapsed <= 0.0)
            return;

        const auto initialEnergy = static_cast<double>(energyValue);
        const auto initialAttack = static_cast<double>(attackValue);
        const auto target = static_cast<double>(targetEnergy);
        const auto energyTime = target > initialEnergy ? riseTime : releaseTime;

        // Integrate the speed over the envelopes rather than sampling once per
        // repaint, so the same audio has similar motion at different UI rates.
        auto movingDuration = elapsed;
        if (target == 0.0)
        {
            const auto energyTail = initialEnergy > settleThreshold
                ? releaseTime * std::log(initialEnergy / settleThreshold) : 0.0;
            const auto attackTail = initialAttack > settleThreshold
                ? attackTime * std::log(initialAttack / settleThreshold) : 0.0;
            movingDuration = std::min(elapsed, std::max(energyTail, attackTail));
        }

        if (movingDuration > 0.0)
        {
            const auto energyIntegral = target * movingDuration
                + (initialEnergy - target) * energyTime
                    * (1.0 - std::exp(-movingDuration / energyTime));
            const auto attackIntegral = initialAttack * attackTime
                * (1.0 - std::exp(-movingDuration / attackTime));
            phaseValue = static_cast<float>(std::fmod(static_cast<double>(phaseValue)
                + 2.2 * movingDuration + 6.2 * energyIntegral
                + 2.5 * attackIntegral, twoPi));
        }

        energyValue = static_cast<float>(target
            + (initialEnergy - target) * std::exp(-elapsed / energyTime));
        attackValue = static_cast<float>(initialAttack * std::exp(-elapsed / attackTime));
        if (target == 0.0 && energyValue <= settleThreshold)
            energyValue = 0.0f;
        if (attackValue <= settleThreshold)
            attackValue = 0.0f;
    }

    static constexpr double frameDeadline = 0.2;
    static constexpr double riseTime = 0.035;
    static constexpr double releaseTime = 0.095;
    static constexpr double attackTime = 0.070;
    static constexpr double settleThreshold = 0.004;
    static constexpr double twoPi = 6.28318530717958647692;

    float energyValue = 0.0f;
    float attackValue = 0.0f;
    float phaseValue = 0.0f;
    float targetEnergy = 0.0f;
    float previousPeak = 0.0f;
    double secondsWithoutFrame = frameDeadline;
};
} // namespace fire::ui
