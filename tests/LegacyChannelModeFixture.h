#pragma once
#include <Utility/ChannelModeParameters.h>

namespace fire::test
{
inline void stripChannelModePresetFamily(juce::XmlElement& xml)
{
    xml.removeAttribute("channelModeSchemaVersion");
    for (int scope = 0; scope < channel_mode::parameterCount; ++scope)
        xml.removeAttribute(channel_mode::parameterID(scope));
    if (xml.getIntAttribute("presetFormatVersion",0) >= 5) xml.setAttribute("presetFormatVersion",4);
}
inline void stripChannelModeHostFamily(juce::XmlElement& xml)
{
    xml.removeAttribute("channelModeSchemaVersion");
    if (xml.getIntAttribute("stateFormatVersion",0) >= 4) xml.setAttribute("stateFormatVersion",3);
    if (auto* parameters = xml.getChildByName("PARAMETERS"))
    {
        for (int index = parameters->getNumChildElements(); --index >= 0;)
            if (channel_mode::isParameterID(parameters->getChildElement(index)->getStringAttribute("id")))
                parameters->removeChildElement(parameters->getChildElement(index),true);
        xml.setAttribute("savedParameterCount",parameters->getNumChildElements());
    }
    if (auto* alternate = xml.getChildByName("AB_STATE")) stripChannelModePresetFamily(*alternate);
}
}
