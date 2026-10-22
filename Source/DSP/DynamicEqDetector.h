#pragma once
#include "../Utility/DynamicEqParameters.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <limits>

namespace fire::dynamic_eq
{
// One audio-thread-owned instance per EQ node. Feed the EQ MODULE'S original
// input, before any of its nodes. M/S scopes supply their selected dual-mono
// input. Never feed the output of the preceding EQ node into this detector.
class Detector
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate = std::isfinite(rate) && rate > 0 ? rate : 48000;
        period = juce::jmax(1, juce::roundToInt(sampleRate / 6000));
        depth.reset(sampleRate, .005);
        for (auto* parameter : {&threshold, &ratio, &range}) parameter->reset(sampleRate, .02);
        reset();
    }

    void reset() noexcept
    {
        memory = {}; current = target = {1, 0, 0, 0, 0}; increments = {};
        controlRemaining = 0; coefficientRemaining = 0;
        coefficientPrimed = configurationPrimed = false;
        envelope = gain = targetGain = gainStep = lastGain = 0;
        dormant = true;
        depth.setCurrentAndTargetValue(0);
        lastFrequency = lastQ = -1;
        lastType = eq::Type::notch;
    }

    void setConfiguration(Configuration next) noexcept
    {
        next = sanitise(next);
        if (!isActive() && !dormant)
        {
            memory = {}; envelope = gain = targetGain = gainStep = 0;
            coefficientPrimed = false; controlRemaining = 0; dormant = true;
        }
        const bool snap = !configurationPrimed || !isActive();
        const auto update = [snap](auto& smoother, float value)
        { if (snap) smoother.setCurrentAndTargetValue(value); else smoother.setTargetValue(value); };
        update(threshold, next.thresholdDb); update(ratio, next.ratio); update(range, next.rangeDb);
        if (!configurationPrimed || next.attackMs != configuration.attackMs)
            attackCoefficient = std::exp(-1.0 / (sampleRate * .001 * next.attackMs));
        if (!configurationPrimed || next.releaseMs != configuration.releaseMs)
            releaseCoefficient = std::exp(-1.0 / (sampleRate * .001 * next.releaseMs));
        configuration = next;
        configurationPrimed = true;
        depth.setTargetValue(next.enabled && next.ratio > 1 && next.rangeDb != 0 ? 1.0f : 0.0f);
    }

    bool isActive() const noexcept { return depth.getCurrentValue() > 0 || depth.getTargetValue() > 0; }
    float getGainDb() const noexcept { return lastGain; }
    int getControlPeriod() const noexcept { return period; }

    // frequency/q are the same *resolved* values used by the audible EQ node,
    // including its base smoothing and modulation. Returns only a dB offset.
    float process(float left, float right, bool stereo, float frequency, float q, eq::Type type) noexcept
    {
        // Unsupported types cannot retain an unfinished enable bridge, or the
        // surrounding module would keep capturing detector input indefinitely.
        if (!supports(type)) depth.setCurrentAndTargetValue(0);
        if (!isActive() || !supports(type))
        {
            lastGain = 0;
            // No transcendental calls, filters, detector or audible arithmetic
            // on stable Disabled. Re-enable starts from a clean detector.
            if (!dormant)
            {
                memory = {}; envelope = gain = targetGain = gainStep = 0;
                coefficientPrimed = false; controlRemaining = 0;
                dormant = true;
            }
            return 0;
        }
        dormant = false;
        const auto currentThreshold = threshold.getNextValue();
        const auto currentRatio = ratio.getNextValue();
        const auto currentRange = range.getNextValue();
        const bool update = controlRemaining == 0;
        if (update) updateCoefficients(frequency, q, type);
        if (coefficientRemaining > 0)
        {
            if (--coefficientRemaining == 0) current = target;
            else for (size_t i = 0; i < current.size(); ++i) current[i] += increments[i];
        }
        const auto l = filtered(0, std::isfinite(left) ? left : 0);
        const auto r = stereo ? filtered(1, std::isfinite(right) ? right : 0) : l;
        const auto instantaneous = std::max(std::abs(l), std::abs(r));
        const auto coefficient = instantaneous > envelope ? attackCoefficient : releaseCoefficient;
        envelope = instantaneous + coefficient * (envelope - instantaneous);
        if (!std::isfinite(envelope) || envelope < 1.0e-30) envelope = 0;
        if (update)
        {
            double amount = 0;
            if (currentThreshold != previousThreshold)
            {
                kneeStartAmplitude = std::pow(10.0, (currentThreshold - kneeWidthDb * .5) / 20.0);
                previousThreshold = currentThreshold;
            }
            if (envelope > kneeStartAmplitude)
            {
                const auto levelDb = 20 * std::log10(envelope);
                const auto over = levelDb - currentThreshold;
                const auto softOver = over >= kneeWidthDb * .5 ? over
                    : (over + kneeWidthDb * .5) * (over + kneeWidthDb * .5) / (2 * kneeWidthDb);
                amount = std::min(std::abs(static_cast<double>(currentRange)),
                    softOver * (1 - 1.0 / currentRatio));
            }
            targetGain = std::copysign(amount, currentRange);
            gainStep = (targetGain - gain) / period;
            controlRemaining = period;
        }
        if (--controlRemaining == 0) gain = targetGain;
        else gain += gainStep;
        const auto mix = depth.getCurrentValue();
        depth.getNextValue();
        lastGain = static_cast<float>(gain * mix);
        return lastGain;
    }

private:
    using Coefficients = std::array<double, 5>; // b0 b1 b2 a1 a2, a0=1.
    void updateCoefficients(float rawFrequency, float rawQ, eq::Type type) noexcept
    {
        const auto upper = sampleRate * .475;
        const auto frequency = juce::jlimit(juce::jmin(20.0, upper), upper,
            std::isfinite(rawFrequency) ? static_cast<double>(rawFrequency) : 1000);
        const auto q = type == eq::Type::bell ? juce::jlimit(.1, 18.0,
            std::isfinite(rawQ) ? static_cast<double>(rawQ) : .70710678) : .7071067811865476;
        if (coefficientPrimed && type == lastType && frequency == lastFrequency && q == lastQ) return;
        using Array = juce::dsp::IIR::ArrayCoefficients<double>;
        const auto recipe = type == eq::Type::bell ? Array::makeBandPass(sampleRate, frequency, q)
            : type == eq::Type::lowShelf ? Array::makeLowPass(sampleRate, frequency, q)
                                       : Array::makeHighPass(sampleRate, frequency, q);
        target = {recipe[0]/recipe[3], recipe[1]/recipe[3], recipe[2]/recipe[3], recipe[4]/recipe[3], recipe[5]/recipe[3]};
        if (!coefficientPrimed || type != lastType)
        {
            current = target; coefficientRemaining = 0; memory = {}; envelope = 0;
        }
        else
        {
            for (size_t i = 0; i < current.size(); ++i) increments[i] = (target[i] - current[i]) / period;
            coefficientRemaining = period;
        }
        coefficientPrimed = true; lastType = type; lastFrequency = frequency; lastQ = q;
    }

    double filtered(size_t channel, double input) noexcept
    {
        auto& z = memory[channel];
        const auto value = current[0] * input + z[0];
        z[0] = current[1] * input - current[3] * value + z[1];
        z[1] = current[2] * input - current[4] * value;
        if (!std::isfinite(value) || !std::isfinite(z[0]) || !std::isfinite(z[1])) { z = {}; return 0; }
        for (auto& state : z) if (std::abs(state) < 1.0e-30) state = 0;
        return value;
    }

    Configuration configuration;
    double sampleRate = 48000, attackCoefficient = 0, releaseCoefficient = 0, kneeStartAmplitude = 0;
    float previousThreshold = std::numeric_limits<float>::quiet_NaN();
    static constexpr double kneeWidthDb = 6;
    double envelope = 0, gain = 0, targetGain = 0, gainStep = 0, lastFrequency = -1, lastQ = -1;
    float lastGain = 0;
    Coefficients current {1,0,0,0,0}, target {1,0,0,0,0}, increments {};
    std::array<std::array<double,2>,2> memory {};
    int period = 8, controlRemaining = 0, coefficientRemaining = 0;
    bool coefficientPrimed = false, configurationPrimed = false, dormant = true;
    eq::Type lastType = eq::Type::notch;
    juce::SmoothedValue<float> depth;
    juce::SmoothedValue<float> threshold, ratio, range;
};
}
