#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>

namespace fire::modulation_fx
{
inline constexpr int scopeCount = 5, slotCount = 8;
inline constexpr int parameterCount = scopeCount * slotCount;
inline constexpr int schemaVersion = 1;

// The original six-choice Type parameter keeps its normalised automation
// anchors. New algorithms are selected by an append-only, independent family:
// 0 = standard Type, 1 = Flanger, 2 = Phaser.
inline juce::String parameterBase(int slot, bool master)
{
    return juce::String(master ? "masterFx" : "bandFx") + juce::String(slot + 1) + "ModulationType";
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
    if (! id.contains("ModulationType")) return false;
    static const auto sorted = [] { auto ids = parameterIDs(); ids.sort(false); return ids; }();
    return std::binary_search(sorted.begin(), sorted.end(), id);
}
} // namespace fire::modulation_fx
