#pragma once

#include "InsertParameters.h"
#include "EqParameters.h"
#include "ModuleOrder.h"

namespace fire::core_modules
{
inline constexpr int schemaVersion = 1;
inline constexpr int presenceCount = 23; // Master EQ, Lo-Fi, Analysis; five modules in each band.
inline constexpr int typeField = 0, jitterField = 1;
inline constexpr int slotFieldCount = 2 + eq::maxNodes * eq::fieldCount;
inline constexpr int parameterCount = presenceCount + effects::scopeCount * effects::slotCount * slotFieldCount;

inline juce::String presenceID(int scope, int node)
{ return juce::String(scope == 0 ? "masterModule" : "bandModule") + juce::String(node)
    + "Present" + (scope == 0 ? juce::String() : juce::String(scope)); }
inline bool validLegacy(int scope, int node) noexcept
{ return juce::isPositiveAndBelow(scope, effects::scopeCount) && juce::isPositiveAndBelow(node, scope == 0 ? 3 : 5); }
inline effects::Type legacyType(int scope, int node) noexcept
{
    if (! validLegacy(scope, node)) return effects::Type::none;
    if (scope == 0) return node == 0 ? effects::Type::eq : node == 1 ? effects::Type::lofi : effects::Type::none;
    constexpr effects::Type kinds[] {effects::Type::drive, effects::Type::shape, effects::Type::compressor,
                                    effects::Type::stereo, effects::Type::ott};
    return kinds[node];
}
inline int legacyNode(int scope, effects::Type type) noexcept
{
    for (int node = 0; node < (scope == 0 ? 3 : 5); ++node)
        if (legacyType(scope, node) == type && type != effects::Type::none) return node;
    return -1;
}
inline juce::String parameterID(int scope, int slot, int field)
{ return effects::parameterBase(slot, effects::typeField, scope == 0).upToLastOccurrenceOf("Type", false, false)
    + (field == typeField ? "CoreType" : field == jitterField ? "Jitter" : "EqField" + juce::String(field - 2))
    + (scope == 0 ? juce::String() : juce::String(scope)); }
inline juce::String eqParameterID(int scope, int slot, int node, eq::Field field)
{ return parameterID(scope, slot, 2 + node * eq::fieldCount + static_cast<int>(field)); }
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = []
    {
        juce::StringArray result;
        for (int scope = 0; scope < effects::scopeCount; ++scope)
        {
            for (int node = 0; node < (scope == 0 ? 3 : 5); ++node) result.add(presenceID(scope, node));
            for (int slot = 0; slot < effects::slotCount; ++slot)
                for (int field = 0; field < slotFieldCount; ++field) result.add(parameterID(scope, slot, field));
        }
        result.sort(false); return result;
    }();
    return ids;
}
inline bool isParameterID(const juce::String& id)
{ const auto& ids = parameterIDs(); return std::binary_search(ids.begin(), ids.end(), id); }
inline effects::Type decodeType(int family) noexcept
{ return family >= 1 && family <= 6 ? static_cast<effects::Type>(static_cast<int>(effects::Type::drive) + family - 1) : effects::Type::none; }
inline int encodeType(effects::Type type) noexcept
{ return effects::isCore(type) ? static_cast<int>(type) - static_cast<int>(effects::Type::drive) + 1 : 0; }
}
