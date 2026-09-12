#pragma once
#include "../DSP/InsertEffect.h"

namespace fire::effects
{
inline constexpr int slotCount = 8;
inline constexpr int scopeCount = 5;
inline constexpr int typeField = 6, enabledField = 7, orderField = 8, fieldCount = 9;
inline constexpr std::array<const char*, 3> tapeIDs {"lofiTape", "lofiWow", "lofiFlutter"};
inline constexpr std::array<const char*, 3> tapeNames {"Tape", "Wow", "Flutter"};
inline constexpr int parameterCount = scopeCount * slotCount * fieldCount + 3;

// scope 0 is Master, scopes 1..4 are bands. IDs refer to permanent slots;
// moving a module never moves its automation or LFO assignments to another ID.
inline juce::String parameterBase(int slot, int field, bool master)
{
    const auto suffix = field == typeField ? "Type" : field == enabledField ? "Enabled"
                      : field == orderField ? "Order" : "Control" + juce::String(field + 1);
    return juce::String(master ? "masterFx" : "bandFx") + juce::String(slot + 1) + suffix;
}
inline juce::String parameterID(int scope, int slot, int field)
{
    return parameterBase(slot, field, scope == 0) + (scope == 0 ? juce::String() : juce::String(scope));
}
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = [] {
        juce::StringArray result;
        for (int scope = 0; scope < scopeCount; ++scope)
            for (int slot = 0; slot < slotCount; ++slot)
                for (int field = 0; field < fieldCount; ++field) result.add(parameterID(scope, slot, field));
        for (auto* id : tapeIDs) result.add(id);
        return result;
    }();
    return ids;
}
inline bool isParameterID(const juce::String& id)
{
    if (! id.startsWith("masterFx") && ! id.startsWith("bandFx")
        && id != tapeIDs[0] && id != tapeIDs[1] && id != tapeIDs[2]) return false;
    static const auto sorted = [] {auto ids = parameterIDs(); ids.sort(false); return ids;}();
    return std::binary_search(sorted.begin(), sorted.end(), id);
}
inline juce::String controlName(int slot, int control)
{
    return "FX " + juce::String(slot + 1) + " Control " + juce::String(control + 1);
}
}
