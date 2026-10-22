#pragma once
#include "LfoBankParameters.h"
#include <array>

namespace fire::lfo_playback
{
inline constexpr int count = lfo_bank::capacity, schemaVersion = 1;
enum Field { playMode = 0, waveMode, seed, triggerNote, triggerChannel, fieldCount };
enum PlayMode { loop = 0, retrigger, oneShot };
enum WaveMode { shape = 0, randomHold, randomGlide };
inline constexpr int parameterCount = count * fieldCount;
inline constexpr int versionHint = 17;
struct Config
{
    int playMode = loop;
    int waveMode = shape;
    int seed = 0;
    int triggerNote = -1; // -1 matches every Note On; otherwise MIDI note 0..127.
    int triggerChannel = 0; // 0 matches every channel; otherwise 1..16.
};
using Bank = std::array<Config, count>;
inline constexpr std::array<const char*, fieldCount> bases{
    "lfoPlayMode", "lfoWaveMode", "lfoSeed", "lfoTriggerNote", "lfoTriggerChannel"};
inline constexpr std::array<const char*, fieldCount> labels{
    "Playback", "Wave", "Seed", "Trigger Note", "Trigger Channel"};
inline constexpr std::array<int, fieldCount> defaults{loop, shape, 0, -1, 0};
inline constexpr std::array<int, fieldCount> minimums{0, 0, 0, -1, 0};
inline constexpr std::array<int, fieldCount> maximums{2, 2, 65535, 127, 16};
inline constexpr bool validSlot(int slot) noexcept { return lfo_bank::validIndex(slot); }
inline constexpr bool validField(int field) noexcept { return field >= 0 && field < fieldCount; }
inline juce::String parameterID(int slot, int field)
{
    if (!validSlot(slot) || !validField(field)) return {};
    return juce::String(bases[static_cast<size_t>(field)]) + juce::String(slot + 1);
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
inline const juce::StringArray& playModeNames()
{ static const juce::StringArray names{"Loop", "Retrigger", "One-shot"}; return names; }
inline const juce::StringArray& waveModeNames()
{ static const juce::StringArray names{"Shape", "Random Hold", "Random Glide"}; return names; }
inline bool valid(const Config& config) noexcept
{
    return config.playMode >= loop && config.playMode <= oneShot
        && config.waveMode >= shape && config.waveMode <= randomGlide
        && config.seed >= 0 && config.seed <= 65535
        && config.triggerNote >= -1 && config.triggerNote <= 127
        && config.triggerChannel >= 0 && config.triggerChannel <= 16;
}
}
