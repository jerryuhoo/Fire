#pragma once
#include "InsertParameters.h"
#include "../DSP/SpatialReverb.h"

namespace fire::reverb_params
{
inline constexpr int schemaVersion = 1, parameterCount = effects::scopeCount * effects::slotCount;
inline juce::String parameterID(int scope, int slot)
{return juce::String(scope == 0 ? "masterFx" : "bandFx") + juce::String(slot + 1) + "ReverbModel" + (scope == 0 ? juce::String() : juce::String(scope));}
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = [] {juce::StringArray result;
        for (int scope = 0; scope < effects::scopeCount; ++scope)
            for (int slot = 0; slot < effects::slotCount; ++slot) result.add(parameterID(scope, slot));
        result.sort(false); return result;
    }(); return ids;
}
inline bool isParameterID(const juce::String& id)
{const auto& ids = parameterIDs(); return std::binary_search(ids.begin(), ids.end(), id);}
}
