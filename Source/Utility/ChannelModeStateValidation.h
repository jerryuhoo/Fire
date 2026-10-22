#pragma once
#include "ChannelModeParameters.h"
#include "EnvelopeStateValidation.h"

namespace fire::channel_mode_state
{
inline constexpr const char* marker = "channelModeSchemaVersion";

// A family is either wholly absent in an older sound, or complete and
// explicitly versioned. Never silently round a corrupt processing mode.
inline bool validate(const juce::XmlElement& xml, const juce::AudioProcessor& processor,
                     bool requireFamily, bool hostValues, bool& legacyWithoutFamily)
{
    std::map<juce::String, juce::String> values;
    if (hostValues)
    {
        if (const auto* parameters = xml.getChildByName("PARAMETERS"))
            for (const auto* parameter : parameters->getChildIterator())
            {
                const auto id = parameter->getStringAttribute("id");
                if (!channel_mode::isParameterID(id)) continue;
                if (!parameter->hasTagName("PARAM") || !parameter->hasAttribute("value")
                    || !values.emplace(id, parameter->getStringAttribute("value")).second) return false;
            }
    }
    else
        for (const auto& attribute : xml.getAttributeIterator())
            if (channel_mode::isParameterID(attribute.name.toString()))
                values.emplace(attribute.name.toString(), attribute.value);

    legacyWithoutFamily = !xml.hasAttribute(marker) && values.empty();
    if (legacyWithoutFamily) return !requireFamily;
    int version = 0;
    if (!modulation_state::readStrictIntegerAttribute(xml, marker, version)
        || version != channel_mode::schemaVersion) return false;
    int expected = 0;
    for (const auto* parameter : processor.getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter);
            ranged != nullptr && channel_mode::isParameterID(ranged->paramID))
        {
            ++expected;
            const auto found = values.find(ranged->paramID);
            if (found == values.end() || !envelope_state::isValidValue(*ranged, found->second, !hostValues)) return false;
        }
    return expected == channel_mode::parameterCount && static_cast<int>(values.size()) == expected;
}
}
