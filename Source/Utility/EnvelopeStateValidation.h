#pragma once

#include "EnvelopeParameters.h"
#include "ModulationStateValidation.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <cmath>

namespace fire::envelope_state
{
inline constexpr const char* marker = "envelopeSchemaVersion";

// Presets store normalised values, whereas host PARAMETERS stores plain
// values. Reject malformed or incomplete detector settings before either
// loader mutates the live sound; in particular never round an invalid source
// choice to a different input bus or band.
inline bool isValidValue(const juce::RangedAudioParameter& parameter,
                         const juce::String& text, bool normalised) noexcept
{
    double value = 0.0;
    if (!utility::parseStrictFiniteDouble(text, value)) return false;
    const auto& range = parameter.getNormalisableRange();
    if (normalised ? (value < 0.0 || value > 1.0)
                   : (value < range.start || value > range.end)) return false;
    const bool discrete = dynamic_cast<const juce::AudioParameterChoice*>(&parameter) != nullptr
                       || dynamic_cast<const juce::AudioParameterBool*>(&parameter) != nullptr
                       || dynamic_cast<const juce::AudioParameterInt*>(&parameter) != nullptr;
    if (!discrete) return true;
    if (!normalised) return std::floor(value) == value;
    const auto canonical = parameter.convertTo0to1(parameter.convertFrom0to1(static_cast<float>(value)));
    return std::abs(value - canonical) <= 1.0e-6;
}

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
                if (!envelope_params::isParameterID(id)) continue;
                if (!parameter->hasTagName("PARAM") || !parameter->hasAttribute("value")
                    || !values.emplace(id, parameter->getStringAttribute("value")).second) return false;
            }
    }
    else
        for (const auto& attribute : xml.getAttributeIterator())
            if (envelope_params::isParameterID(attribute.name.toString()))
                values.emplace(attribute.name.toString(), attribute.value);

    legacyWithoutFamily = !xml.hasAttribute(marker) && values.empty();
    if (legacyWithoutFamily) return !requireFamily;
    if (xml.hasAttribute(marker))
    {
        int version = 0;
        if (!modulation_state::readStrictIntegerAttribute(xml, marker, version)
            || version != envelope_params::schemaVersion) return false;
    }
    else if (requireFamily) return false;

    int expected = 0;
    for (const auto* parameter : processor.getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter);
            ranged != nullptr && envelope_params::isParameterID(ranged->paramID))
        {
            ++expected;
            const auto found = values.find(ranged->paramID);
            if (found == values.end() || !isValidValue(*ranged, found->second, !hostValues)) return false;
        }
    return expected == envelope_params::parameterCount && static_cast<int>(values.size()) == expected;
}

inline bool validatePreset(const juce::XmlElement& xml, const juce::AudioProcessor& processor,
                           bool requireFamily, bool& legacyWithoutFamily)
{ return validate(xml, processor, requireFamily, false, legacyWithoutFamily); }

inline bool validateHost(const juce::XmlElement& xml, const juce::AudioProcessor& processor,
                         bool requireFamily, bool& legacyWithoutFamily)
{ return validate(xml, processor, requireFamily, true, legacyWithoutFamily); }
}
