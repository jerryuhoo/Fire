#pragma once
#include "DynamicEqParameters.h"
#include "EnvelopeStateValidation.h"

namespace fire::dynamic_eq_state
{
inline constexpr const char* marker="dynamicEqSchemaVersion";
inline bool validate(const juce::XmlElement& xml,const juce::AudioProcessor& processor,
                     bool requireFamily,bool hostValues,bool& legacyWithoutFamily)
{
    // Use the family's prefix + binary-search predicate. A linear scan of
    // 2952 IDs for every host/preset attribute would unnecessarily square work.
    std::map<juce::String,juce::String> values;
    if(hostValues)
    {
        if(const auto* parameters=xml.getChildByName("PARAMETERS"))
            for(const auto* parameter:parameters->getChildIterator())
            {
                const auto id=parameter->getStringAttribute("id");
                if(!dynamic_eq::isParameterID(id))continue;
                if(!parameter->hasTagName("PARAM") || !parameter->hasAttribute("value")
                    || !values.emplace(id,parameter->getStringAttribute("value")).second)return false;
            }
    }
    else for(const auto& attribute:xml.getAttributeIterator())
        if(dynamic_eq::isParameterID(attribute.name.toString()))values.emplace(attribute.name.toString(),attribute.value);
    legacyWithoutFamily=!xml.hasAttribute(marker)&&values.empty();
    if(legacyWithoutFamily)return !requireFamily;
    int version=0;
    if(!modulation_state::readStrictIntegerAttribute(xml,marker,version)
        || version!=dynamic_eq::schemaVersion)return false;
    if(values.size()!=static_cast<size_t>(dynamic_eq::parameterCount))return false;
    int expected=0;
    for(const auto* parameter:processor.getParameters())
        if(const auto* ranged=dynamic_cast<const juce::RangedAudioParameter*>(parameter);
            ranged && dynamic_eq::isParameterID(ranged->paramID))
        {
            ++expected;const auto found=values.find(ranged->paramID);
            if(found==values.end() || !envelope_state::isValidValue(*ranged,found->second,!hostValues))return false;
        }
    return expected==dynamic_eq::parameterCount;
}
}
