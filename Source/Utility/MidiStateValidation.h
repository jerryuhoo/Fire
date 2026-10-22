#pragma once
#include "MidiModulationParameters.h"
#include "LfoPlaybackParameters.h"
#include "EnvelopeStateValidation.h"

namespace fire::midi_state
{
inline constexpr const char* ccMarker = "midiModulationSchemaVersion";
inline constexpr const char* playbackMarker = "lfoPlaybackSchemaVersion";

// New source configurations must be whole, versioned, and exactly discrete.
// Old sounds without a family load that family's parameter defaults.
inline bool validateFamily(const juce::XmlElement& xml, const juce::AudioProcessor& processor,
                           bool requireFamily, bool hostValues, bool playback, bool& legacyWithoutFamily)
{
    const auto& ids = playback ? lfo_playback::parameterIDs() : midi_mod::parameterIDs();
    const auto* marker = playback ? playbackMarker : ccMarker;
    const auto schema = playback ? lfo_playback::schemaVersion : midi_mod::schemaVersion;
    std::map<juce::String, juce::String> values;
    if (hostValues)
    {
        if (const auto* parameters = xml.getChildByName("PARAMETERS"))
            for (const auto* parameter : parameters->getChildIterator())
            {
                const auto id = parameter->getStringAttribute("id");
                if (!ids.contains(id)) continue;
                if (!parameter->hasTagName("PARAM") || !parameter->hasAttribute("value")
                    || !values.emplace(id, parameter->getStringAttribute("value")).second) return false;
            }
    }
    else
        for (const auto& attribute : xml.getAttributeIterator())
            if (ids.contains(attribute.name.toString())) values.emplace(attribute.name.toString(), attribute.value);
    legacyWithoutFamily = !xml.hasAttribute(marker) && values.empty();
    if (legacyWithoutFamily) return !requireFamily;
    int version = 0;
    if (!modulation_state::readStrictIntegerAttribute(xml, marker, version) || version != schema) return false;
    int expected = 0;
    for (const auto* parameter : processor.getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter);
            ranged != nullptr && ids.contains(ranged->paramID))
        {
            ++expected;
            const auto found = values.find(ranged->paramID);
            if (found == values.end() || !envelope_state::isValidValue(*ranged, found->second, !hostValues)) return false;
        }
    return expected == ids.size() && static_cast<int>(values.size()) == expected;
}
}
