#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>

// A separately versioned extension to the original nine-field insert slots.
// Keep the original insert IDs, field indices and normalised ranges frozen.
namespace fire::clouds_params
{
inline constexpr int scopeCount = 5, slotCount = 8;
inline constexpr int engineField = 0, freezeField = 1, spreadField = 2;
inline constexpr int feedbackField = 3, reverbField = 4, fieldCount = 5;
inline constexpr int parameterCount = scopeCount * slotCount * fieldCount;
inline constexpr int schemaVersion = 2;
inline constexpr std::array<const char*, fieldCount> fieldNames {
    "Engine", "Freeze", "Spread", "Feedback", "Reverb"
};
// Engine retains its original ID/index/range for old host automation, but is
// no longer an audio control. New snapshots always write the Clouds value.
inline constexpr std::array<float, fieldCount> defaults { 1.0f, 0.0f, 0.5f, 0.0f, 0.0f };

inline juce::String parameterBase(int slot, int field, bool master)
{
    if (! juce::isPositiveAndBelow(slot, slotCount)
        || ! juce::isPositiveAndBelow(field, fieldCount))
        return {};
    return juce::String(master ? "masterFx" : "bandFx") + juce::String(slot + 1)
         + "Clouds" + fieldNames[static_cast<size_t>(field)];
}

inline juce::String parameterID(int scope, int slot, int field)
{
    if (! juce::isPositiveAndBelow(scope, scopeCount))
        return {};
    const auto base = parameterBase(slot, field, scope == 0);
    return base.isEmpty() || scope == 0 ? base : base + juce::String(scope);
}

inline const juce::StringArray& parameterIDs()
{
    static const auto ids = []
    {
        juce::StringArray result;
        for (int scope = 0; scope < scopeCount; ++scope)
            for (int slot = 0; slot < slotCount; ++slot)
                for (int field = 0; field < fieldCount; ++field)
                    result.add(parameterID(scope, slot, field));
        return result;
    }();
    return ids;
}

inline bool isParameterID(const juce::String& id)
{
    if ((! id.startsWith("masterFx") && ! id.startsWith("bandFx"))
        || ! id.contains("Clouds"))
        return false;
    static const auto sorted = [] { auto ids = parameterIDs(); ids.sort(false); return ids; }();
    return std::binary_search(sorted.begin(), sorted.end(), id);
}

inline bool isReservedEngineParameterID(const juce::String& id)
{
    return id.contains("CloudsEngine") && isParameterID(id);
}

// Version 1 and pre-Clouds snapshots used Fire's millisecond/rate controls.
// This approximates their physical scale in the sole remaining Clouds engine;
// it does not claim to preserve the removed engine's timbre or LFO trajectory.
inline std::array<float, 6> migrateLegacyGranular(std::array<float, 6> values) noexcept
{
    for (auto& value : values)
        value = std::clamp(std::isfinite(value) ? value : 0.5f, 0.0f, 1.0f);
    const double oldSizeMs = 10.0 + 240.0 * std::pow(static_cast<double>(values[0]), 1.0 / 0.6);
    const double requestedSizeMs = std::clamp(oldSizeMs, 32.0, 512.0);
    const double pitchRatio = std::pow(2.0, (static_cast<double>(values[2]) * 48.0 - 24.0) / 12.0);
    constexpr double recordingMs = 32696.0 / 32.0;
    const double effectiveSizeMs = pitchRatio > 1.0
        ? std::min(requestedSizeMs, recordingMs * 0.25 / pitchRatio) : requestedSizeMs;
    const double rate = 2.0 + 38.0 * values[1];
    const double overlap = std::cbrt(rate * effectiveSizeMs * 0.001 / 32.0);
    const double playedMs = effectiveSizeMs * pitchRatio;
    const double availableMs = std::max(1.0, recordingMs - playedMs - effectiveSizeMs);
    const double oldPositionMs = 1200.0 * values[3] * values[3];
    values[0] = static_cast<float>(std::clamp(std::log2(requestedSizeMs / 32.0) / 4.0, 0.0, 1.0));
    values[1] = static_cast<float>(std::clamp(0.47 - overlap / 2.12, 0.0, 0.47));
    values[3] = static_cast<float>(std::clamp((oldPositionMs - playedMs) / availableMs, 0.0, 1.0));
    values[4] = 0.5f + 0.5f * values[4];
    return values; // Pitch and Mix retain their original normalised values.
}
}
