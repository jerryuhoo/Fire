#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace fire::analog
{
inline constexpr int legacyCount = 12, count = 12, modeCount = legacyCount + count;
inline constexpr std::array<const char*, count> names{
    "Warm Triode", "Bright Pentode", "Class A Console", "Tweed Breakup",
    "British Crunch", "Modern High Gain", "Diode Overdrive", "Germanium Fuzz",
    "Silicon Fuzz", "MOSFET Drive", "Transformer", "Tape Saturation"
};
struct Profile
{
    float gain, asymmetry, stages, highPass, tone, midGain, sag, memory, output;
};
inline constexpr std::array<Profile, count> profiles{{
    {1.9f, .17f, 1, 25, 14500, .06f, .12f, .01f, .84f},
    {2.5f, .09f, 1, 50, 17000, .24f, .08f, 0, .79f},
    {1.3f, .08f, 1, 15, 18500, .04f, .03f, .04f, .94f},
    {2.8f, .13f, 2, 65, 7500, .18f, .35f, .02f, .87f},
    {3.5f, .06f, 2, 110, 6700, .42f, .18f, .01f, .83f},
    {5.0f, .02f, 3, 150, 5600, .31f, .09f, 0, .79f},
    {2.2f, .04f, 1, 170, 9500, .40f, .02f, 0, .90f},
    {4.0f, .21f, 2, 35, 5200, -.12f, .24f, .08f, .79f},
    {5.4f, .02f, 2, 85, 10500, -.22f, .05f, 0, .75f},
    {2.8f, .14f, 1, 70, 12500, .16f, .05f, .015f, .88f},
    {1.4f, .05f, 1, 12, 17500, -.05f, .07f, .18f, .91f},
    {1.8f, .035f, 1, 20, 11000, -.06f, .16f, .12f, .89f}
}};
inline int resolve(int legacy, int family) noexcept
{ return family >= 1 && family <= count ? legacyCount + family - 1 : juce::jlimit(0, legacyCount - 1, legacy); }

// Circuit-inspired colour, rather than a claim of a measured device clone.
// Curves have a zero crossing at silence; asymmetry supplies even harmonics.
inline float transfer(int profile, float input) noexcept
{
    if (!juce::isPositiveAndBelow(profile, count) || !std::isfinite(input)) return 0;
    const auto& p = profiles[static_cast<size_t>(profile)];
    const auto x = juce::jlimit(-32.0f, 32.0f, input) * p.gain;
    const auto soft = [](float value) {return value / std::sqrt(1.0f + value * value);};
    float y = soft(x + p.asymmetry) - soft(p.asymmetry);
    if (profile == 6) y = std::asinh(x * 1.7f) / 2.3f;
    if (profile == 7) y = std::tanh(x + .24f) - std::tanh(.24f);
    if (profile == 8) y = std::tanh(x * 1.35f) + .11f * std::tanh(x * 4.0f);
    if (profile == 9) y = soft(x * (x >= 0 ? 1.12f : .82f));
    for (int stage = 1; stage < static_cast<int>(p.stages); ++stage)
        y = std::tanh(y * (profile == 5 ? 2.0f : 1.35f) + p.asymmetry * .25f) - std::tanh(p.asymmetry * .25f);
    return juce::jlimit(-1.25f, 1.25f, y * p.output);
}
template <int profile> float curve(float input) noexcept {return transfer(profile, input);}

// Two warm banks per channel allow the existing mode crossfade to preserve
// RC filter, power-supply sag and magnetic history on the audible bank.
class Stage
{
public:
    void prepare(double rate) noexcept
    {sampleRate = std::isfinite(rate) && rate > 0 ? rate : 48000; current = -1; reset();}
    void reset() noexcept {inputLow = midLow = toneLow = envelope = flux = dcInput = dcOutput = previous = 0;}
    void setProfile(int mode, double rate) noexcept
    {
        const auto kind = mode - legacyCount;
        const auto safeRate = std::isfinite(rate) && rate > 0 ? rate : 48000;
        if (current == kind && juce::exactlyEqual(sampleRate, safeRate)) return;
        if (current != kind) reset();
        current = kind; sampleRate = safeRate;
        if (!juce::isPositiveAndBelow(current, count)) return;
        const auto& p = profiles[static_cast<size_t>(current)];
        const auto pole = [&](double hz) {return static_cast<float>(1 - std::exp(-juce::MathConstants<double>::twoPi * juce::jmin(hz, sampleRate * .45) / sampleRate));};
        inputPole = pole(p.highPass); midPole = pole(1100); tonePole = pole(p.tone);
        attack = static_cast<float>(1 - std::exp(-1 / (sampleRate * .004)));
        release = static_cast<float>(1 - std::exp(-1 / (sampleRate * .085)));
        fluxPole = pole(180); dcPole = static_cast<float>(std::exp(-juce::MathConstants<double>::twoPi * 12 / sampleRate));
    }
    float process(float input) noexcept
    {
        if (!juce::isPositiveAndBelow(current, count)) return input;
        input = juce::jlimit(-64.0f, 64.0f, std::isfinite(input) ? input : 0.0f);
        const auto& p = profiles[static_cast<size_t>(current)];
        inputLow += inputPole * (input - inputLow);
        auto x = input - inputLow;
        midLow += midPole * (x - midLow);
        x += p.midGain * (midLow - inputLow);
        const auto magnitude = std::abs(x);
        envelope += (magnitude > envelope ? attack : release) * (magnitude - envelope);
        x = x / (1 + p.sag * envelope) + p.memory * flux;
        // A bounded midpoint integration softens the newly generated high
        // harmonics without adding host latency. HQ supplies the 4x path.
        auto y = .25f * transfer(current, previous) + .5f * transfer(current, (previous + x) * .5f) + .25f * transfer(current, x);
        previous = x;
        flux += fluxPole * (y - flux);
        toneLow += tonePole * (y - toneLow);
        y = toneLow - dcInput + dcPole * dcOutput;
        dcInput = toneLow; dcOutput = std::isfinite(y) ? y : 0;
        return dcOutput;
    }
private:
    int current = -1;
    double sampleRate = 48000;
    float inputPole = 0, midPole = 0, tonePole = 0, attack = 0, release = 0, fluxPole = 0, dcPole = 0;
    float inputLow = 0, midLow = 0, toneLow = 0, envelope = 0, flux = 0, dcInput = 0, dcOutput = 0, previous = 0;
};
}
