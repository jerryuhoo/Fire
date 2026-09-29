#pragma once

#include <juce_core/juce_core.h>

namespace fire::drive_comp
{
inline constexpr int parameterCount = 4;
inline constexpr int schemaVersion = 1;
inline constexpr const char* parameterBase = "driveCompModern";

// New bands use modern compensation. Missing historical state is explicitly
// migrated to false by the preset/host loaders, preserving the old Link sound.
inline juce::String parameterID(int band)
{
    return juce::String(parameterBase) + juce::String(band + 1);
}

inline const juce::StringArray& parameterIDs()
{
    static const auto ids = []
    {
        juce::StringArray result;
        for (int band = 0; band < parameterCount; ++band) result.add(parameterID(band));
        return result;
    }();
    return ids;
}

inline bool isParameterID(const juce::String& id)
{
    return id.startsWith(parameterBase) && parameterIDs().contains(id);
}
} // namespace fire::drive_comp
