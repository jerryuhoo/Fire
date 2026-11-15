#pragma once
#include <juce_core/juce_core.h>

namespace fire::channel_mode
{
inline constexpr int parameterCount = 5, schemaVersion = 1;
enum Mode { stereo = 0, mid = 1, side = 2 };
inline bool valid(int mode) noexcept { return mode >= stereo && mode <= side; }
inline juce::String parameterID(int scope)
{ return scope == 0 ? juce::String("masterChannelMode") : "bandChannelMode" + juce::String(scope); }
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = [] { juce::StringArray result;
        for (int scope = 0; scope < parameterCount; ++scope) result.add(parameterID(scope));
        return result; }();
    return ids;
}
inline bool isParameterID(const juce::String& id) { return parameterIDs().contains(id); }
inline const juce::StringArray& names()
{ static const juce::StringArray labels{"Stereo", "Mid", "Side"}; return labels; }
}
