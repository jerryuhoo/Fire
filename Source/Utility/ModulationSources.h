#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include "LfoBankParameters.h"

namespace fire::mod_sources
{
inline constexpr int envelope = lfo_bank::capacity;
inline constexpr int firstMacro = envelope + 1;
inline constexpr int macroCount = 4;
inline constexpr int sourceCount = firstMacro + macroCount;
inline constexpr int parameterCount = 3 + macroCount;
inline constexpr int schemaVersion = 1;
inline constexpr std::array<const char*, parameterCount> ids{
    "envAttack", "envRelease", "envSensitivity", "macro1", "macro2", "macro3", "macro4"
};
inline constexpr std::array<float, parameterCount> defaults{10, 200, 0, 0, 0, 0, 0};
inline constexpr std::array<float, parameterCount> minimums{0.1f, 5, -24, 0, 0, 0, 0};
inline constexpr std::array<float, parameterCount> maximums{500, 2000, 24, 1, 1, 1, 1};
inline bool validSource(int index) noexcept { return index >= 0 && index < sourceCount; }
inline bool isAuxiliary(int index) noexcept { return index >= envelope && index < sourceCount; }
inline bool isParameterID(const juce::String& id)
{ for (auto* parameter : ids) if (id == parameter) return true; return false; }
inline const juce::StringArray& parameterIDs()
{
    static const auto values = [] { juce::StringArray result; for (auto* id : ids) result.add(id); return result; }();
    return values;
}
inline juce::String name(int index)
{
    if (index == envelope) return "Envelope";
    if (index >= firstMacro && index < sourceCount) return "Macro " + juce::String(index - firstMacro + 1);
    return "LFO " + juce::String(index + 1);
}
inline juce::String badge(int index)
{
    if (index == envelope) return "ENV";
    if (index >= firstMacro && index < sourceCount) return "M" + juce::String(index - firstMacro + 1);
    return juce::String(index + 1);
}
}
