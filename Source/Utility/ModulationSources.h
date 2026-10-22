#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include "LfoBankParameters.h"
#include "EnvelopeParameters.h"
#include "MidiModulationParameters.h"
#include "LfoPlaybackParameters.h"

namespace fire::mod_sources
{
inline constexpr int envelope = lfo_bank::capacity;
inline constexpr int firstMacro = envelope + 1;
inline constexpr int macroCount = 4;
inline constexpr int legacySourceCount = firstMacro + macroCount;
inline constexpr int envelopeSourceCount = legacySourceCount + envelope_params::count - 1;
inline constexpr int firstMidiCC = envelopeSourceCount;
inline constexpr int midiCCCount = midi_mod::count;
inline constexpr int sourceCount = firstMidiCC + midiCCCount;
inline bool isMidiCC(int index) noexcept { return index >= firstMidiCC && index < sourceCount; }
inline bool isMacro(int index) noexcept { return index >= firstMacro && index < firstMacro + macroCount; }
inline int envelopeSource(int index) noexcept { return index == 0 ? envelope : legacySourceCount + index - 1; }
inline int envelopeIndex(int source) noexcept
{ return source == envelope ? 0 : source >= legacySourceCount && source < envelopeSourceCount ? source - legacySourceCount + 1 : -1; }
// Internal aggregate channels are never persisted as user source identities.
inline constexpr int maximumRoutings = 128;
inline constexpr int firstAggregateSignal = sourceCount;
inline constexpr int signalCount = firstAggregateSignal + maximumRoutings;
inline bool isAggregateSignal(int index) noexcept
{ return index >= firstAggregateSignal && index < signalCount; }
inline constexpr int parameterCount = 3 + macroCount;
inline constexpr int schemaVersion = 1;
inline constexpr std::array<const char*, parameterCount> ids{
    "envAttack", "envRelease", "envSensitivity", "macro1", "macro2", "macro3", "macro4"
};
inline constexpr std::array<float, parameterCount> defaults{10, 200, 0, 0, 0, 0, 0};
inline constexpr std::array<float, parameterCount> minimums{0.1f, 5, -24, 0, 0, 0, 0};
inline constexpr std::array<float, parameterCount> maximums{500, 2000, 24, 1, 1, 1, 1};
inline bool validSource(int index) noexcept { return index >= 0 && index < sourceCount; }
inline bool isAuxiliary(int index) noexcept { return index >= envelope && index < sourceCount; }
inline bool isLegacyParameterID(const juce::String& id)
{ for (auto* parameter : ids) if (id == parameter) return true; return false; }
inline bool isParameterID(const juce::String& id)
{ return isLegacyParameterID(id) || envelope_params::isParameterID(id)
    || midi_mod::isParameterID(id) || lfo_playback::isParameterID(id); }
inline const juce::StringArray& parameterIDs()
{
    static const auto values = [] { juce::StringArray result; for (auto* id : ids) result.add(id); return result; }();
    return values;
}
inline juce::String name(int index)
{
    if (index == envelope) return "Envelope";
    if (isMacro(index)) return "Macro " + juce::String(index - firstMacro + 1);
    if (isMidiCC(index)) return "MIDI CC " + juce::String(index - firstMidiCC + 1);
    if (envelopeIndex(index) > 0) return "Envelope " + juce::String(envelopeIndex(index) + 1);
    return "LFO " + juce::String(index + 1);
}
inline juce::String badge(int index)
{
    if (index == envelope) return "ENV";
    if (isMacro(index)) return "M" + juce::String(index - firstMacro + 1);
    if (isMidiCC(index)) return "CC" + juce::String(index - firstMidiCC + 1);
    if (envelopeIndex(index) > 0) return "ENV" + juce::String(envelopeIndex(index) + 1);
    return juce::String(index + 1);
}
}
