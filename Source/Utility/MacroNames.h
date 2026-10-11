#pragma once
#include "ModulationSources.h"

namespace fire::macro_names
{
using Names = std::array<juce::String, mod_sources::macroCount>;
inline constexpr int maximumLength = 32;
inline Names defaults() { return {"Macro 1", "Macro 2", "Macro 3", "Macro 4"}; }
inline juce::String attribute(int index) { return "macroName" + juce::String(index + 1); }
inline juce::String sanitise(juce::String text, int index)
{
    juce::String result;
    for (auto character : text)
        if (character >= 32 && character != 127 && result.length() < maximumLength)
            result += juce::String::charToString(character);
    result = result.trim();
    return result.isEmpty() ? defaults()[static_cast<size_t>(index)] : result;
}
inline bool read(const juce::XmlElement& xml, Names& names)
{
    names = defaults();
    bool present = xml.hasAttribute("macroNamesSchemaVersion");
    for (int i = 0; i < mod_sources::macroCount; ++i) present |= xml.hasAttribute(attribute(i));
    if (! present) return true;
    if (xml.getStringAttribute("macroNamesSchemaVersion") != "1") return false;
    for (int i = 0; i < mod_sources::macroCount; ++i)
    {
        const auto text = xml.getStringAttribute(attribute(i));
        if (text.isEmpty() || text != sanitise(text, i)) return false;
        names[static_cast<size_t>(i)] = text;
    }
    return true;
}
inline bool valid(const juce::XmlElement& xml) { Names names; return read(xml, names); }
inline void write(juce::XmlElement& xml, const Names& names)
{
    xml.setAttribute("macroNamesSchemaVersion", 1);
    for (int i = 0; i < mod_sources::macroCount; ++i)
        xml.setAttribute(attribute(i), sanitise(names[static_cast<size_t>(i)], i));
}
}
