#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace fire::lfo_bank
{
inline constexpr int capacity = 16;
inline constexpr int defaultCount = 4;
inline constexpr int timingFieldCount = 5;
inline constexpr int appendedParameterCount = (capacity - defaultCount) * timingFieldCount + capacity;
inline constexpr int schemaVersion = 1;
enum class Field { syncMode = 0, rateSync, rateHz, smoothness, phase, present };
inline constexpr std::array<const char*, timingFieldCount> timingBases {
    "lfoSyncMode", "lfoRateSync", "lfoRateHz", "lfoSmooth", "lfoPhase"
};
inline constexpr bool validIndex(int index) noexcept { return index >= 0 && index < capacity; }
inline constexpr bool defaultPresent(int index) noexcept { return index >= 0 && index < defaultCount; }

// Zero-based stable slot indices. Existing timing IDs 1..4 are unchanged.
inline juce::String parameterID(int index, Field field)
{
    if (! validIndex(index)) return {};
    if (field == Field::present) return "lfoPresent" + juce::String(index + 1);
    const auto offset = static_cast<size_t>(field);
    return offset < timingBases.size() ? juce::String(timingBases[offset]) + juce::String(index + 1) : juce::String {};
}
inline juce::String presentParameterID(int index) { return parameterID(index, Field::present); }

// Append order: 12 new groups of the five historical timing controls, then
// all 16 presence controls. Presence is persisted only as an APVTS parameter.
inline const juce::StringArray& appendedParameterIDs()
{
    static const auto result = []
    {
        juce::StringArray ids;
        for (int index = defaultCount; index < capacity; ++index)
            for (int field = 0; field < timingFieldCount; ++field)
                ids.add(parameterID(index, static_cast<Field>(field)));
        for (int index = 0; index < capacity; ++index) ids.add(presentParameterID(index));
        return ids;
    }();
    return result;
}
inline bool isAppendedParameterID(const juce::String& id) { return appendedParameterIDs().contains(id); }
inline bool isPresentParameterID(const juce::String& id) noexcept
{
    // Parameter listeners may run on the audio thread. Parse the existing
    // character storage instead of constructing temporary parameter Strings.
    auto cursor = id.getCharPointer();
    for (const char* prefix = "lfoPresent"; *prefix != '\0'; ++prefix)
        if (cursor.getAndAdvance() != static_cast<juce::juce_wchar>(*prefix)) return false;
    if (cursor.isEmpty()) return false;
    const auto first = cursor.getAndAdvance();
    if (first < '1' || first > '9') return false;
    int number = static_cast<int>(first - '0');
    while (! cursor.isEmpty())
    {
        const auto digit = cursor.getAndAdvance();
        if (digit < '0' || digit > '9') return false;
        number = number * 10 + static_cast<int>(digit - '0');
        if (number > capacity) return false;
    }
    return number <= capacity;
}
} // namespace fire::lfo_bank
