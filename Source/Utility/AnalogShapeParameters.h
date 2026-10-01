#pragma once
#include "InsertParameters.h"
#include "../DSP/AnalogDistortion.h"

namespace fire::analog_params
{
inline constexpr int schemaVersion = 1, parameterCount = 4 + 2 * effects::scopeCount * effects::slotCount;
inline juce::String bandID(int band) {return "shapeModel" + juce::String(band + 1);}
inline juce::String parameterID(int scope, int slot)
{return juce::String(scope == 0 ? "masterFx" : "bandFx") + juce::String(slot + 1) + "ShapeModel" + (scope == 0 ? juce::String() : juce::String(scope));}
inline juce::String driveID(int scope, int slot)
{return juce::String(scope == 0 ? "masterFx" : "bandFx") + juce::String(slot + 1) + "ShapeDrive" + (scope == 0 ? juce::String() : juce::String(scope));}
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = []
    {
        juce::StringArray result;
        for (int band = 0; band < 4; ++band) result.add(bandID(band));
        for (int scope = 0; scope < effects::scopeCount; ++scope)
            for (int slot = 0; slot < effects::slotCount; ++slot) {result.add(parameterID(scope, slot)); result.add(driveID(scope, slot));}
        result.sort(false); return result;
    }(); return ids;
}
inline bool isParameterID(const juce::String& id)
{const auto& ids = parameterIDs(); return std::binary_search(ids.begin(), ids.end(), id);}
}
