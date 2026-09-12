#pragma once
#include "InsertParameters.h"

namespace fire::module_order
{
inline constexpr int firstInsert = 5, capacity = firstInsert + effects::slotCount;
inline constexpr int parameterCount = 4 * capacity + capacity - 2;
using Order = std::array<int, capacity>;
// Builtin IDs follow the existing panels' logical identities, not their rows.
// Legacy DSP ran OTT before Stereo, and Master Lo-Fi before Filter.
inline constexpr Order bandDefault {0, 1, 2, 4, 3, 5, 6, 7, 8, 9, 10, 11, 12};
inline constexpr Order masterDefault {1, 0, 2, 5, 6, 7, 8, 9, 10, 11, 12, -1, -1};
inline Order defaults(int scope) noexcept { return scope == 0 ? masterDefault : bandDefault; }
inline bool valid(int scope, int node) noexcept
{
    return scope >= 0 && scope < effects::scopeCount && node >= 0 && node < capacity
        && (scope != 0 || node < 3 || node >= firstInsert);
}
inline juce::String parameterBase(int node, bool master)
{
    return juce::String(master ? "masterModuleOrder" : "bandModuleOrder") + juce::String(node) + (master ? "" : "Band");
}
inline juce::String parameterID(int scope, int node)
{
    return parameterBase(node, scope == 0) + (scope == 0 ? juce::String() : juce::String(scope));
}
inline bool isParameterID(const juce::String& id)
{
    if (! id.startsWith("masterModuleOrder") && ! id.startsWith("bandModuleOrder")) return false;
    static const auto identifiers = [] {
        juce::StringArray result;
        for (int scope = 0; scope < effects::scopeCount; ++scope)
            for (int node = 0; node < capacity; ++node) if (valid(scope, node)) result.add(parameterID(scope, node));
        result.sort(false); return result;
    }();
    return std::binary_search(identifiers.begin(), identifiers.end(), id);
}

// Short dry bridges allow one prepared set of processors to change order
// without reallocating, doubling tails, or changing host-reported latency.
class Transition
{
public:
    void prepare(double rate) { mix.reset(rate, 0.015); reset(); }
    void reset() noexcept { primed = false; mix.setCurrentAndTargetValue(1); }
    const Order& begin(const Order& requested) noexcept
    {
        if (! primed) { active = requested; primed = true; }
        if (active != requested && juce::exactlyEqual(mix.getCurrentValue(), 0.0f)) active = requested;
        mix.setTargetValue(active == requested ? 1.0f : 0.0f);
        return active;
    }
    void apply(juce::dsp::AudioBlock<float> wet, const juce::AudioBuffer<float>& dry) noexcept
    {
        if (! mix.isSmoothing() && juce::exactlyEqual(mix.getCurrentValue(), 1.0f)) return;
        for (size_t sample = 0; sample < wet.getNumSamples(); ++sample)
        {
            const auto blend = mix.getNextValue();
            for (size_t channel = 0; channel < wet.getNumChannels(); ++channel)
            {
                const auto original = dry.getSample(static_cast<int>(channel), static_cast<int>(sample));
                auto& value = wet.getChannelPointer(channel)[sample];
                value = original + blend * (value - original);
            }
        }
    }
private:
    Order active {};
    juce::SmoothedValue<float> mix;
    bool primed = false;
};
}
