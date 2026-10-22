#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace fire::midi_mod
{
inline constexpr int count = 4, schemaVersion = 1;
enum Field { number = 0, channel, fieldCount };
inline constexpr int parameterCount = count * fieldCount;
inline constexpr int versionHint = 17;
struct Config
{
    int number = -1; // -1 disables this source; otherwise MIDI CC 0..127.
    int channel = 0; // 0 accepts every channel; otherwise MIDI channel 1..16.
};
using Bank = std::array<Config, count>;
inline constexpr std::array<const char*, fieldCount> suffixes{"Number", "Channel"};
inline constexpr std::array<const char*, fieldCount> labels{"Number", "Channel"};
inline constexpr std::array<int, fieldCount> defaults{-1, 0};
inline constexpr std::array<int, fieldCount> minimums{-1, 0};
inline constexpr std::array<int, fieldCount> maximums{127, 16};
inline constexpr bool validSlot(int slot) noexcept { return slot >= 0 && slot < count; }
inline constexpr bool validField(int field) noexcept { return field >= 0 && field < fieldCount; }
inline juce::String parameterID(int slot, int field)
{
    if (!validSlot(slot) || !validField(field)) return {};
    return "midiCC" + juce::String(slot + 1) + suffixes[static_cast<size_t>(field)];
}
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = [] { juce::StringArray result;
        for (int slot = 0; slot < count; ++slot)
            for (int field = 0; field < fieldCount; ++field) result.add(parameterID(slot, field));
        return result; }();
    return ids;
}
inline bool isParameterID(const juce::String& id) { return parameterIDs().contains(id); }
inline bool valid(const Config& config) noexcept
{ return config.number >= -1 && config.number <= 127 && config.channel >= 0 && config.channel <= 16; }
}
