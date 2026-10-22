#pragma once
#include <Utility/DynamicEqParameters.h>

namespace fire::test
{
inline void stripDynamicEqPresetFamily(juce::XmlElement& xml)
{
    xml.removeAttribute("dynamicEqSchemaVersion");
    for(const auto& id:dynamic_eq::parameterIDs())xml.removeAttribute(id);
    if(xml.getIntAttribute("presetFormatVersion",0)>=7)xml.setAttribute("presetFormatVersion",6);
}
inline void stripDynamicEqHostFamily(juce::XmlElement& xml)
{
    xml.removeAttribute("dynamicEqSchemaVersion");
    if(xml.getIntAttribute("stateFormatVersion",0)>=6)xml.setAttribute("stateFormatVersion",5);
    if(auto* parameters=xml.getChildByName("PARAMETERS"))
    {
        for(int index=parameters->getNumChildElements();--index>=0;)
            if(dynamic_eq::isParameterID(parameters->getChildElement(index)->getStringAttribute("id")))
                parameters->removeChildElement(parameters->getChildElement(index),true);
        xml.setAttribute("savedParameterCount",parameters->getNumChildElements());
    }
    if(auto* alternate=xml.getChildByName("AB_STATE"))stripDynamicEqPresetFamily(*alternate);
}
}
