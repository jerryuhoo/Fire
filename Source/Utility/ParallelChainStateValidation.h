#pragma once
#include "ParallelChainParameters.h"
#include "EnvelopeStateValidation.h"

namespace fire::parallel_chain_state
{
inline constexpr const char* marker="parallelChainSchemaVersion";
inline bool validate(const juce::XmlElement& xml,const juce::AudioProcessor& processor,
                     bool requireFamily,bool hostValues,bool& legacyWithoutFamily)
{
    std::map<juce::String,juce::String> values;
    if(hostValues)
    {
        if(const auto* parameters=xml.getChildByName("PARAMETERS"))for(const auto* parameter:parameters->getChildIterator())
        {
            const auto id=parameter->getStringAttribute("id");if(!parallel_chain::isParameterID(id))continue;
            if(!parameter->hasTagName("PARAM") || !parameter->hasAttribute("value")
                || !values.emplace(id,parameter->getStringAttribute("value")).second)return false;
        }
    }
    else for(const auto& attribute:xml.getAttributeIterator())
        if(parallel_chain::isParameterID(attribute.name.toString()))values.emplace(attribute.name.toString(),attribute.value);
    legacyWithoutFamily=!xml.hasAttribute(marker)&&values.empty();if(legacyWithoutFamily)return !requireFamily;
    int version=0;
    if(!modulation_state::readStrictIntegerAttribute(xml,marker,version) || version!=parallel_chain::schemaVersion
        || values.size()!=parallel_chain::parameterCount)return false;
    int expected=0;
    for(const auto* parameter:processor.getParameters())
        if(const auto* ranged=dynamic_cast<const juce::RangedAudioParameter*>(parameter);ranged&&parallel_chain::isParameterID(ranged->paramID))
        {
            ++expected;const auto found=values.find(ranged->paramID);
            if(found==values.end() || !envelope_state::isValidValue(*ranged,found->second,!hostValues))return false;
        }
    return expected==parallel_chain::parameterCount;
}
}
