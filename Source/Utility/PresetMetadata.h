#pragma once
#include <juce_core/juce_core.h>

namespace fire::preset_metadata
{
inline constexpr int maximumTags=16,maximumTagLength=32,maximumDescriptionLength=2048;
struct Metadata
{
    juce::StringArray tags;
    juce::String description;
    bool factory=false,hasOverride=false;
    juce::String revision;
};
inline constexpr bool validXmlCodePoint(juce::juce_wchar c) noexcept
{
    return c==9 || c==10 || c==13 || (c>=0x20 && c<=0xd7ff)
        || (c>=0xe000 && c<=0xfffd) || (c>=0x10000 && c<=0x10ffff);
}
inline bool validText(const juce::String& text) noexcept
{
    for(auto cursor=text.getCharPointer();!cursor.isEmpty();)
    {
        const auto c=cursor.getAndAdvance();
        if(!validXmlCodePoint(c))return false;
    }
    return true;
}
inline juce::Result canonicalise(const Metadata& input,Metadata& output)
{
    if(!validText(input.description))return juce::Result::fail("The description contains unsupported control characters.");
    if (input.description.length()>maximumDescriptionLength)
        return juce::Result::fail("Descriptions may contain at most 2048 characters.");
    if (input.tags.size()>maximumTags) return juce::Result::fail("A preset may have at most 16 tags.");
    Metadata next=input;
    next.description=input.description.replace("\r\n","\n").replace("\r","\n").trim();
    next.tags.clear();
    for (const auto& raw:input.tags)
    {
        if(!validText(raw) || raw.containsAnyOf("\n\r\t"))
            return juce::Result::fail("Each tag must contain valid text on one line.");
        const auto tag=raw.trim();
        if (tag.isEmpty()) continue;
        if (tag.length()>maximumTagLength)
            return juce::Result::fail("Each tag must fit on one line and contain at most 32 characters.");
        if (!next.tags.contains(tag,true)) next.tags.add(tag);
    }
    output=std::move(next);return juce::Result::ok();
}
inline juce::Result readAttributes(const juce::XmlElement& xml,Metadata& output)
{
    Metadata next=output;next.description=xml.getStringAttribute("presetDescription");next.tags.clear();
    if (xml.hasAttribute("presetTags"))
    {
        const auto encoded=xml.getStringAttribute("presetTags");
        if (encoded.getNumBytesAsUTF8()>4096) return juce::Result::fail("The preset's tag list is too large.");
        juce::var parsed;
        if (juce::JSON::parse(encoded,parsed).failed() || !parsed.isArray())
            return juce::Result::fail("The preset's tag list is malformed.");
        const auto* tags=parsed.getArray();
        if (tags->size()>maximumTags) return juce::Result::fail("The preset has too many tags.");
        for (const auto& tag:*tags)
        {
            if (!tag.isString()) return juce::Result::fail("Every preset tag must be text.");
            next.tags.add(tag.toString());
        }
    }
    return canonicalise(next,output);
}
inline void writeAttributes(juce::XmlElement& xml,const Metadata& validated)
{
    juce::Array<juce::var> tags;for (const auto& tag:validated.tags) tags.add(tag);
    xml.setAttribute("presetTags",juce::JSON::toString(juce::var(tags),true));
    xml.setAttribute("presetDescription",validated.description);
}
}
