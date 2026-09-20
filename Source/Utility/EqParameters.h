#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace fire::eq
{
inline constexpr int maxNodes = 12;
inline constexpr int fieldCount = 7;
inline constexpr int appendedParameterCount = 70;
enum class Type { bell = 0, lowCut, highCut, lowShelf, highShelf, notch, bandPass };
enum class Field { frequency = 0, gain, q, slope, type, present, bypassed };
inline constexpr std::array<const char*, 7> typeNames {
    "Bell", "Low Cut", "High Cut", "Low Shelf", "High Shelf", "Notch", "Band Pass"
};

struct NodeState
{
    bool present = false;
    bool bypassed = false;
    Type type = Type::bell;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.70710678f;
    int slope = 0; // 0..3: 12, 24, 36, 48 dB/oct for cut filters.
};

inline bool validSlot(int slot) noexcept { return slot >= 0 && slot < maxNodes; }
inline bool isCut(Type type) noexcept { return type == Type::lowCut || type == Type::highCut; }
inline Type defaultType(int slot) noexcept
{
    return slot == 0 ? Type::lowCut : slot == 2 ? Type::highCut : Type::bell;
}
inline bool usesLegacyShape(int slot, Type type) noexcept
{
    return slot >= 0 && slot < 3 && type == defaultType(slot);
}
inline NodeState defaultNode(int slot) noexcept
{
    NodeState state;
    state.present = slot >= 0 && slot < 3;
    state.type = defaultType(slot);
    state.frequency = slot == 0 ? 20.0f : slot == 2 ? 20000.0f : 1000.0f;
    state.q = slot < 3 ? 1.0f : 0.70710678f;
    return state;
}

inline bool isAppendedParameter(int slot, Field field) noexcept
{
    return validSlot(slot) && (slot >= 3 || field == Field::type || field == Field::present
                              || (slot == 1 && field == Field::slope));
}

// Slots never move: the first three retain every historical host/LFO ID.
inline juce::String parameterID(int slot, Field field)
{
    if (! validSlot(slot)) return {};
    if (slot < 3)
    {
        static constexpr std::array<std::array<const char*, 5>, 3> legacy {{
            {{"lowcutFreq", "lowCutGain", "lowcutQ", "lowcutSlope", "lowcutBypassed"}},
            {{"peakFreq", "peakGain", "peakQ", "eqNode2Slope", "peakBypassed"}},
            {{"highcutFreq", "highCutGain", "highcutQ", "highcutSlope", "highcutBypassed"}}
        }};
        const auto& ids = legacy[static_cast<size_t>(slot)];
        if (field == Field::frequency) return ids[0];
        if (field == Field::gain) return ids[1];
        if (field == Field::q) return ids[2];
        if (field == Field::slope) return ids[3];
        if (field == Field::bypassed) return ids[4];
    }
    static constexpr std::array<const char*, fieldCount> suffixes {
        "Freq", "Gain", "Q", "Slope", "Type", "Present", "Bypassed"
    };
    const auto index = static_cast<size_t>(field);
    if (index >= suffixes.size()) return {};
    return "eqNode" + juce::String(slot + 1) + suffixes[index];
}

inline const juce::StringArray& appendedParameterIDs()
{
    static const auto ids = []
    {
        juce::StringArray result;
        for (int slot = 0; slot < maxNodes; ++slot)
            for (int field = 0; field < fieldCount; ++field)
                if (isAppendedParameter(slot, static_cast<Field>(field)))
                    result.add(parameterID(slot, static_cast<Field>(field)));
        return result;
    }();
    return ids;
}

inline bool isAppendedParameterID(const juce::String& id)
{
    return id.startsWith("eqNode") && appendedParameterIDs().contains(id);
}

// Message-thread/UI helper. Audio processing uses cached parameter pointers.
inline NodeState readNode(const juce::AudioProcessorValueTreeState& state, int slot)
{
    auto node = defaultNode(slot);
    if (! validSlot(slot)) { node.present = false; return node; }
    const auto read = [&](Field field, float fallback)
    {
        if (const auto* value = state.getRawParameterValue(parameterID(slot, field)))
        {
            const auto result = value->load(std::memory_order_relaxed);
            if (std::isfinite(result)) return result;
        }
        return fallback;
    };
    node.present = read(Field::present, node.present ? 1.0f : 0.0f) > 0.5f;
    node.bypassed = read(Field::bypassed, 0.0f) > 0.5f;
    node.type = static_cast<Type>(juce::jlimit(0, 6, juce::roundToInt(read(Field::type, static_cast<float>(node.type)))));
    node.frequency = read(Field::frequency, node.frequency);
    node.gainDb = read(Field::gain, node.gainDb);
    node.q = read(Field::q, node.q);
    node.slope = juce::jlimit(0, 3, juce::roundToInt(read(Field::slope, 0.0f)));
    return node;
}
} // namespace fire::eq
