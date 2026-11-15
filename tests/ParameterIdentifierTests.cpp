#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <map>
#include <set>

TEST_CASE("Exported parameter identifiers stay unique in AU VST3 and CLAP mappings",
          "[param-ids][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    std::set<juce::String> identifiers;
    std::map<std::uint32_t,juce::String> auAndVst3,clap;

    // Local JUCE AU generateAUParameterID and VST3ClientExtensions use
    // String::hashCode, masking its sign bit for Studio One compatibility.
    // clap-juce-extensions uses the same hash without the mask. APVTS IDs are
    // not indices; keeping their strings unique alone cannot detect collisions.
    constexpr std::uint32_t bypass=0x62797073u; // JUCE VST3 'byps'
    constexpr std::uint32_t program=0x70727374u; // JUCE VST3 'prst'
    constexpr std::uint32_t midiFirst=0x6d636d00u;
    constexpr std::uint32_t midiEnd=midiFirst+16u*130u; // 128 CCs + aftertouch/pitch bend.
    constexpr std::uint32_t invalidClap=0xffffffffu;

    for(const auto* parameter:processor.getParameters())
    {
        const auto* identified=dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
        REQUIRE(identified!=nullptr);
        const auto& id=identified->paramID;
        CAPTURE(id,parameter->getParameterIndex());
        REQUIRE(id.isNotEmpty());
        CHECK(identifiers.insert(id).second);
        const auto full=static_cast<std::uint32_t>(id.hashCode());
        const auto compatible=full&0x7fffffffu;
        const auto add=[&](auto& mapping,std::uint32_t mapped,const char* format)
        {
            const auto [existing,inserted]=mapping.emplace(mapped,id);
            CAPTURE(format,mapped,existing->second);
            CHECK(inserted);
        };
        add(auAndVst3,compatible,"AU/VST3 Studio One compatible");
        add(clap,full,"CLAP / unmasked JUCE");
        CHECK(compatible!=bypass);
        CHECK(compatible!=program);
        CHECK_FALSE((compatible>=midiFirst&&compatible<midiEnd));
        CHECK(full!=invalidClap);
    }
    CHECK(identifiers.size()==static_cast<size_t>(processor.getParameters().size()));
    CHECK(auAndVst3.size()==identifiers.size());
    CHECK(clap.size()==identifiers.size());
}
