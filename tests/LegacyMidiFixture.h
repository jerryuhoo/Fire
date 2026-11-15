#pragma once
#include <Utility/MidiModulationParameters.h>
#include <Utility/LfoPlaybackParameters.h>
#include "LegacyDynamicEqFixture.h"

namespace fire::test
{
inline void stripMidiPresetFamily(juce::XmlElement& xml)
{
    stripDynamicEqPresetFamily(xml);
    xml.removeAttribute("midiModulationSchemaVersion");xml.removeAttribute("lfoPlaybackSchemaVersion");
    for (const auto& id : midi_mod::parameterIDs()) xml.removeAttribute(id);
    for (const auto& id : lfo_playback::parameterIDs()) xml.removeAttribute(id);
    if (xml.getIntAttribute("presetFormatVersion",0) >= 6) xml.setAttribute("presetFormatVersion",5);
}
inline void stripMidiHostFamily(juce::XmlElement& xml)
{
    stripDynamicEqHostFamily(xml);
    xml.removeAttribute("midiModulationSchemaVersion");xml.removeAttribute("lfoPlaybackSchemaVersion");
    if (xml.getIntAttribute("stateFormatVersion",0) >= 5) xml.setAttribute("stateFormatVersion",4);
    if (auto* parameters = xml.getChildByName("PARAMETERS"))
    {
        for (int index = parameters->getNumChildElements(); --index >= 0;)
        {
            const auto id = parameters->getChildElement(index)->getStringAttribute("id");
            if (midi_mod::isParameterID(id) || lfo_playback::isParameterID(id))
                parameters->removeChildElement(parameters->getChildElement(index),true);
        }
        xml.setAttribute("savedParameterCount",parameters->getNumChildElements());
    }
    if (auto* alternate = xml.getChildByName("AB_STATE")) stripMidiPresetFamily(*alternate);
}
}
