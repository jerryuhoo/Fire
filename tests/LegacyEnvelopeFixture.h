#pragma once
#include <Utility/EnvelopeParameters.h>
#include "LegacyChannelModeFixture.h"
#include <juce_core/juce_core.h>

namespace fire::test
{
// Preserve each historical fixture's routing format while removing detector
// settings that did not exist in its original parameter contract.
inline void stripEnvelopePresetFamily(juce::XmlElement& xml)
{
    stripChannelModePresetFamily(xml);
    xml.removeAttribute("envelopeSchemaVersion");
    for (const auto& id : envelope_params::parameterIDs()) xml.removeAttribute(id);
    if (xml.getIntAttribute("presetFormatVersion", 0) >= 4)
        xml.setAttribute("presetFormatVersion", 3);
}
inline void stripEnvelopeHostFamily(juce::XmlElement& xml)
{
    stripChannelModeHostFamily(xml);
    xml.removeAttribute("envelopeSchemaVersion");
    if (xml.getIntAttribute("stateFormatVersion", 0) >= 3)
        xml.setAttribute("stateFormatVersion", 2);
    if (auto* parameters = xml.getChildByName("PARAMETERS"))
    {
        for (int index = parameters->getNumChildElements(); --index >= 0;)
            if (envelope_params::isParameterID(parameters->getChildElement(index)->getStringAttribute("id")))
                parameters->removeChildElement(parameters->getChildElement(index), true);
        xml.setAttribute("savedParameterCount", parameters->getNumChildElements());
    }
    if (auto* alternate = xml.getChildByName("AB_STATE")) stripEnvelopePresetFamily(*alternate);
}
}
