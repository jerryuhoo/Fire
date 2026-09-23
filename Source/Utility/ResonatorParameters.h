#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>

namespace fire::resonator_params
{
inline constexpr int scopeCount = 5, slotCount = 8;
inline constexpr int parameterCount = scopeCount * slotCount;
inline constexpr int schemaVersion = 1;

// Independent presence flags keep both historical Type selectors and their
// normalised automation anchors unchanged. The six slot controls are reused.
inline juce::String parameterBase(int slot, bool master)
{
    return juce::String(master ? "masterFx" : "bandFx") + juce::String(slot + 1) + "Resonator";
}
inline juce::String parameterID(int scope, int slot)
{
    return parameterBase(slot, scope == 0) + (scope == 0 ? juce::String() : juce::String(scope));
}
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = []
    {
        juce::StringArray result;
        for (int scope = 0; scope < scopeCount; ++scope)
            for (int slot = 0; slot < slotCount; ++slot) result.add(parameterID(scope, slot));
        return result;
    }();
    return ids;
}
inline bool isParameterID(const juce::String& id)
{
    if (! id.contains("Resonator")) return false;
    static const auto sorted = [] { auto ids = parameterIDs(); ids.sort(false); return ids; }();
    return std::binary_search(sorted.begin(), sorted.end(), id);
}
} // namespace fire::resonator_params
