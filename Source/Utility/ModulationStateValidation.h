#pragma once

#include "StrictNumberParser.h"
#include "../DSP/ModulationRouting.h"
#include <limits>

namespace fire::modulation_state
{
inline constexpr int schemaVersion = 2;
inline constexpr int presetFormatVersion = 3;

inline bool readStrictIntegerAttribute(const juce::XmlElement& xml, const char* name, int& result) noexcept
{
    if (!xml.hasAttribute(name)) return false;
    const auto text = xml.getStringAttribute(name).trim();
    const auto digits = text.startsWithChar('-') ? text.substring(1) : text;
    // Match XmlElement::getIntAttribute's decimal representation, not just an
    // integral double: e.g. "20e-1" must not validate as 2 then load as 20.
    if (digits.isEmpty() || !digits.containsOnly("0123456789")) return false;
    double value = 0;
    if (!utility::parseStrictFiniteDouble(text, value)
        || value < static_cast<double>(std::numeric_limits<int>::min())
        || value > static_cast<double>(std::numeric_limits<int>::max())) return false;
    result = static_cast<int>(value);
    return true;
}

inline bool hasStrictBooleanAttribute(const juce::XmlElement& xml, const char* name) noexcept
{
    if (!xml.hasAttribute(name)) return false;
    const auto text = xml.getStringAttribute(name).trim();
    return text == "0" || text == "1";
}

inline bool hasSchema(const juce::XmlElement& state, bool advancedRequired) noexcept
{
    if (!state.hasAttribute("schemaVersion")) return !advancedRequired;
    int version = 0;
    return readStrictIntegerAttribute(state, "schemaVersion", version)
        && version == (advancedRequired ? schemaVersion : 1);
}

// Validate external data before calling ModulationRouting::readFromXml(),
// whose sanitisation is intended for internal edits, not corrupted snapshots.
inline bool hasValidMapping(const juce::XmlElement& route) noexcept
{
    if (!hasStrictBooleanAttribute(route, "customRange")) return false;
    for (const auto* name : {"rangeStart", "rangeEnd"})
    {
        double value = 0;
        if (!route.hasAttribute(name) || !utility::parseStrictFiniteDouble(route.getStringAttribute(name), value)
            || value < -1.0 || value > 1.0) return false;
    }
    int curve = -1, depthMacro = -2;
    return readStrictIntegerAttribute(route, "curve", curve) && curve >= 0 && curve <= 3
        && readStrictIntegerAttribute(route, "depthMacro", depthMacro)
        && depthMacro >= -1 && depthMacro < mod_sources::macroCount;
}

inline ModulationRouting readLegacy(const juce::XmlElement& xml)
{
    // Read only the historical five fields. Unknown mapping attributes must
    // neither change the sound nor enter numeric parsers for a legacy format.
    ModulationRouting route {xml.getIntAttribute("source", 0), xml.getStringAttribute("target"),
        static_cast<float>(xml.getDoubleAttribute("depth", .5)),
        xml.getBoolAttribute("bipolar", true), xml.getBoolAttribute("bypassed", false)};
    route.sanitise();
    return route;
}
}
