#pragma once
#include <Utility/ParallelChainParameters.h>

namespace fire::test
{
inline void stripParallelPresetFamily(juce::XmlElement& xml)
{
    xml.removeAttribute("parallelChainSchemaVersion");
    for(const auto& id:parallel_chain::parameterIDs())xml.removeAttribute(id);
    if(xml.getIntAttribute("presetFormatVersion",0)>=8)xml.setAttribute("presetFormatVersion",7);
}
inline void stripParallelHostFamily(juce::XmlElement& xml)
{
    xml.removeAttribute("parallelChainSchemaVersion");
    if(xml.getIntAttribute("stateFormatVersion",0)>=7)xml.setAttribute("stateFormatVersion",6);
    if(auto* parameters=xml.getChildByName("PARAMETERS"))
    {
        for(int index=parameters->getNumChildElements();--index>=0;)
            if(parallel_chain::isParameterID(parameters->getChildElement(index)->getStringAttribute("id")))
                parameters->removeChildElement(parameters->getChildElement(index),true);
        xml.setAttribute("savedParameterCount",parameters->getNumChildElements());
    }
    if(auto* alternate=xml.getChildByName("AB_STATE"))stripParallelPresetFamily(*alternate);
}
inline void stripParallelChainSnapshot(juce::XmlElement& xml)
{
    xml.setAttribute("schemaVersion",3);xml.removeAttribute("routingMode");xml.removeAttribute("parallelBlend");
    if(auto* routes=xml.getChildByName("SCOPE_ROUTINGS"))xml.removeChildElement(routes,true);
    for(auto* module:xml.getChildIterator())if(module->hasTagName("MODULE"))module->removeAttribute("branch");
}
}
