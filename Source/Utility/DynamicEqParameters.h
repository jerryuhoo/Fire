#pragma once
#include "EqParameters.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fire::dynamic_eq
{
inline constexpr int schemaVersion = 1, versionHint = 18;
inline constexpr int scopeCount = 5, slotsPerScope = 8, nodesPerModule = eq::maxNodes;
enum Field { enabled = 0, threshold, ratio, range, attack, release, fieldCount };
inline constexpr int parameterCount = (1 + scopeCount * slotsPerScope) * nodesPerModule * fieldCount;
static_assert(parameterCount == 2952);
inline constexpr std::array<const char*, fieldCount> suffixes {"Enabled", "Threshold", "Ratio", "Range", "Attack", "Release"};
inline constexpr std::array<float, fieldCount> defaults {0, -24, 2, -6, 10, 100};
inline constexpr std::array<float, fieldCount> minimums {0, -60, 1, -24, .1f, 5};
inline constexpr std::array<float, fieldCount> maximums {1, 0, 20, 24, 200, 2000};

struct Configuration
{
    bool enabled = false;
    float thresholdDb = -24, ratio = 2, rangeDb = -6, attackMs = 10, releaseMs = 100;
};
struct VisualState
{
    float gainChangeDb = 0, effectiveGainDb = 0;
    std::uint64_t renderSequence = 0;
    bool active = false;
};

inline juce::NormalisableRange<float> rangeFor(int field)
{
    switch (field)
    {
        case enabled: return {0, 1, 1};
        case threshold: return {-60, 0, .1f};
        case ratio: return {1, 20, .01f, .5f};
        case range: return {-24, 24, .1f};
        case attack: return {.1f, 200, .1f, .35f};
        case release: return {5, 2000, 1, .35f};
        default: return {0, 1};
    }
}

inline bool supports(eq::Type type) noexcept
{ return type == eq::Type::bell || type == eq::Type::lowShelf || type == eq::Type::highShelf; }

inline Configuration sanitise(Configuration config) noexcept
{
    const auto clean = [](float value, Field field)
    {
        const auto index = static_cast<size_t>(field);
        return std::isfinite(value) ? std::clamp(value, minimums[index], maximums[index]) : defaults[index];
    };
    config.thresholdDb = clean(config.thresholdDb, threshold); config.ratio = clean(config.ratio, ratio);
    config.rangeDb = clean(config.rangeDb, range); config.attackMs = clean(config.attackMs, attack);
    config.releaseMs = clean(config.releaseMs, release);
    return config;
}

inline bool validAddress(int scope, int slot, int node, int field) noexcept
{
    return scope >= 0 && scope < scopeCount && node >= 0 && node < nodesPerModule
        && field >= 0 && field < fieldCount && ((slot == -1 && scope == 0) || (slot >= 0 && slot < slotsPerScope));
}

inline juce::String parameterID(int scope, int slot, int node, int field)
{
    if (!validAddress(scope, slot, node, field)) return {};
    const auto suffix = "EqDynamic" + juce::String(node + 1) + suffixes[static_cast<size_t>(field)];
    if (slot == -1) return "master" + suffix;
    return juce::String(scope == 0 ? "masterFx" : "bandFx") + juce::String(slot + 1) + suffix
        + (scope == 0 ? juce::String() : "Band" + juce::String(scope));
}
inline juce::String parameterBase(int slot, int node, int field)
{ return parameterID(1, slot, node, field).dropLastCharacters(1); }

inline const juce::StringArray& parameterIDs()
{
    static const auto ids = []
    {
        juce::StringArray result;
        for (int node = 0; node < nodesPerModule; ++node)
            for (int field = 0; field < fieldCount; ++field) result.add(parameterID(0, -1, node, field));
        for (int scope = 0; scope < scopeCount; ++scope)
            for (int slot = 0; slot < slotsPerScope; ++slot)
                for (int node = 0; node < nodesPerModule; ++node)
                    for (int field = 0; field < fieldCount; ++field) result.add(parameterID(scope, slot, node, field));
        return result;
    }();
    return ids;
}

inline bool isParameterID(const juce::String& id)
{
    if (!id.contains("EqDynamic")) return false;
    static const auto sorted = [] { auto ids = parameterIDs(); ids.sort(false); return ids; }();
    return std::binary_search(sorted.begin(), sorted.end(), id);
}
}
