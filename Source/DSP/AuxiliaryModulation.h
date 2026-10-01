#pragma once
#include "../Utility/ModulationSources.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace fire::dsp
{
// Stereo energy drives one linked input follower. Macro ramps and the follower
// advance in samples, independently of host block partition or editor lifetime.
class AuxiliaryModulation
{
public:
    using Parameters = std::array<float, mod_sources::parameterCount>;
    void prepare(double rate) noexcept
    {
        sampleRate = std::isfinite(rate) && rate > 0 ? rate : 48000;
        for (auto& macro : macros) macro.reset(sampleRate, 0.02);
        reset();
    }
    void reset() noexcept
    {
        level = 0; primed = false;
        previous.fill(std::numeric_limits<float>::quiet_NaN());
    }
    void setParameters(const Parameters& parameters) noexcept
    {
        Parameters safe;
        for (size_t index = 0; index < safe.size(); ++index)
            safe[index] = juce::jlimit(mod_sources::minimums[index], mod_sources::maximums[index],
                std::isfinite(parameters[index]) ? parameters[index] : mod_sources::defaults[index]);
        if (!juce::exactlyEqual(safe[0], previous[0])) attack = std::exp(-1.0 / (sampleRate * safe[0] * 0.001));
        if (!juce::exactlyEqual(safe[1], previous[1])) release = std::exp(-1.0 / (sampleRate * safe[1] * 0.001));
        if (!juce::exactlyEqual(safe[2], previous[2])) sensitivity = std::pow(10.0f, safe[2] * 0.05f);
        for (size_t index = 0; index < macros.size(); ++index)
            if (!primed) macros[index].setCurrentAndTargetValue(safe[index + 3]);
            else macros[index].setTargetValue(safe[index + 3]);
        primed = true; previous = safe;
    }
    std::array<float, 5> next(float left, float right, bool stereo) noexcept
    {
        const double l = std::isfinite(left) ? left : 0;
        const double r = stereo && std::isfinite(right) ? right : l;
        const double input = std::min(16.0, std::sqrt(stereo ? (l * l + r * r) * 0.5 : l * l));
        const double coefficient = input > level ? attack : release;
        level = input + coefficient * (level - input);
        if (level < 1.0e-20) level = 0;
        std::array<float, 5> result;
        result[0] = juce::jlimit(0.0f, 1.0f, static_cast<float>(level) * sensitivity);
        for (size_t index = 0; index < macros.size(); ++index) result[index + 1] = macros[index].getNextValue();
        return result;
    }
private:
    std::array<juce::SmoothedValue<float>, mod_sources::macroCount> macros;
    Parameters previous{};
    double sampleRate = 48000, level = 0, attack = 0, release = 0;
    float sensitivity = 1;
    bool primed = false;
};
}
