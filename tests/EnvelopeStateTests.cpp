#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/EnvelopeStateValidation.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include "LegacyChannelModeFixture.h"

namespace
{
namespace env = fire::envelope_params;
void setEnvelopePlain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float envelopePlain(FireAudioProcessor& processor, const juce::String& id)
{
    const auto* parameter = processor.treeState.getRawParameterValue(id); REQUIRE(parameter != nullptr);
    return parameter->load();
}
juce::XmlElement envelopePreset(FireAudioProcessor& processor)
{
    juce::XmlElement xml("WINGSFIRE"); state::saveStateToXml(processor, xml); return xml;
}
juce::XmlElement envelopeHost(FireAudioProcessor& processor)
{
    juce::MemoryBlock bytes; processor.getStateInformation(bytes);
    auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(xml != nullptr); return *xml;
}
void restoreEnvelopeHost(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock bytes; juce::AudioProcessor::copyXmlToBinary(xml, bytes);
    processor.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
}
juce::XmlElement& envelopeHostParameter(juce::XmlElement& xml, const juce::String& id)
{
    auto* parameters = xml.getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
    auto* parameter = parameters->getChildByAttribute("id", id); REQUIRE(parameter != nullptr);
    return *parameter;
}
void configureEnvelopes(FireAudioProcessor& processor, int scene)
{
    for (int index = 0; index < env::count; ++index)
    {
        const std::array<float, env::fieldCount> values {
            static_cast<float>((index + scene + 1) % 6), static_cast<float>((index + scene) % 2),
            80.0f + index * 170.0f + scene * 40.0f, static_cast<float>((index + scene + 1) % 2),
            12000.0f - index * 1500.0f - scene * 500.0f,
            9.0f + index * 7.0f + scene, 120.0f + index * 100.0f + scene * 20.0f,
            -9.0f + index * 4.0f + scene };
        for (int field = 0; field < env::fieldCount; ++field)
            setEnvelopePlain(processor, env::parameterID(index, field), values[static_cast<size_t>(field)]);
    }
    setEnvelopePlain(processor, "macro4", scene ? .85f : .35f);
}
void requireDefaultEnvelopeFamily(FireAudioProcessor& processor)
{
    for (const auto& id : env::parameterIDs())
    {
        auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
        CHECK(parameter->getValue() == Catch::Approx(parameter->getDefaultValue()));
    }
}
void stripEnvelopePreset(juce::XmlElement& xml, int version = 3)
{
    fire::test::stripChannelModePresetFamily(xml);
    xml.removeAttribute(fire::envelope_state::marker);
    for (const auto& id : env::parameterIDs()) xml.removeAttribute(id);
    if (version < 0) xml.removeAttribute("presetFormatVersion");
    else xml.setAttribute("presetFormatVersion", version);
    if (version < 3)
        if (auto* routes = xml.getChildByName("MODULATION_STATE"))
        {
            routes->removeAttribute("schemaVersion");
            for (auto* route : routes->getChildIterator())
                for (const auto* field : {"customRange", "rangeStart", "rangeEnd", "curve", "depthMacro"})
                    route->removeAttribute(field);
        }
}
void stripEnvelopeHost(juce::XmlElement& xml)
{
    fire::test::stripChannelModeHostFamily(xml);
    xml.removeAttribute(fire::envelope_state::marker); xml.setAttribute("stateFormatVersion", 2);
    auto* parameters = xml.getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
    for (int index = parameters->getNumChildElements(); --index >= 0;)
        if (env::isParameterID(parameters->getChildElement(index)->getStringAttribute("id")))
            parameters->removeChildElement(parameters->getChildElement(index), true);
    xml.setAttribute("savedParameterCount", parameters->getNumChildElements());
    if (auto* alternate = xml.getChildByName("AB_STATE")) stripEnvelopePreset(*alternate);
}
void addEnvelopeRouting(juce::XmlElement& xml, int source)
{
    auto* matrix = xml.getChildByName("MODULATION_STATE"); REQUIRE(matrix != nullptr);
    ModulationRouting route {source, "drive1", .37f, false, false};
    route.customRange = true; route.rangeStart = -.1f; route.rangeEnd = .7f; route.curve = 2; route.depthMacro = 3;
    route.writeToXml(*matrix->createNewChildElement("ROUTING"));
}
}

TEST_CASE("Envelope bank settings and fixed source identities roundtrip presets host and AB",
          "[envelope-bank][state][preset][host][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    configureEnvelopes(processor, 0);
    auto first = envelopePreset(processor);
    CHECK(first.getIntAttribute("presetFormatVersion") == 8);
    CHECK(first.getIntAttribute(fire::envelope_state::marker) == 1);
    for (int source : {16, 17, 20, 21, 22, 23}) addEnvelopeRouting(first, source);
    REQUIRE(state::loadStateFromXml(first, processor));
    CHECK(processor.isCurrentStateEquivalentToPreset(first));
    processor.stateAB.copyAB(false); processor.stateAB.toggleAB();
    configureEnvelopes(processor, 1);
    const auto second = envelopePreset(processor), savedHost = envelopeHost(processor);
    CHECK(savedHost.getIntAttribute(fire::envelope_state::marker) == 1);
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(second, restored));
    CHECK(restored.isCurrentStateEquivalentToPreset(second));
    restoreEnvelopeHost(restored, savedHost);
    CHECK_FALSE(restored.stateAB.isCurrentA());
    CHECK(restored.isCurrentStateEquivalentToPreset(second));
    restored.stateAB.toggleAB(); CHECK(restored.isCurrentStateEquivalentToPreset(first));
    restored.stateAB.toggleAB(); CHECK(restored.isCurrentStateEquivalentToPreset(second));
    restored.stateAB.copyAB(false); restored.stateAB.toggleAB();
    CHECK(restored.isCurrentStateEquivalentToPreset(second));
}

TEST_CASE("Older presets and host states reset appended envelope controls while preserving original Envelope and Macros",
          "[envelope-bank][state][legacy][preset][host][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source, restored;
    configureEnvelopes(source, 0);
    auto previous = envelopePreset(source);
    for (int version : {-1, 0, 1, 2, 3})
    {
        CAPTURE(version);
        auto old = previous; stripEnvelopePreset(old, version);
        configureEnvelopes(restored, 1);
        REQUIRE(state::loadStateFromXml(old, restored));
        requireDefaultEnvelopeFamily(restored);
        CHECK(envelopePlain(restored, "envAttack") == Catch::Approx(9.0f));
        CHECK(envelopePlain(restored, "envRelease") == Catch::Approx(120.0f));
        CHECK(envelopePlain(restored, "envSensitivity") == Catch::Approx(-9.0f));
        CHECK(envelopePlain(restored, "macro4") == Catch::Approx(.35f));
        CHECK(restored.isCurrentStateEquivalentToPreset(old));
    }
    source.stateAB.copyAB(false); source.stateAB.toggleAB(); configureEnvelopes(source, 1);
    auto oldHost = envelopeHost(source); stripEnvelopeHost(oldHost);
    configureEnvelopes(restored, 0); restoreEnvelopeHost(restored, oldHost);
    CHECK_FALSE(restored.stateAB.isCurrentA()); requireDefaultEnvelopeFamily(restored);
    CHECK(envelopePlain(restored, "envAttack") == Catch::Approx(10.0f));
    restored.stateAB.toggleAB(); requireDefaultEnvelopeFamily(restored);
    CHECK(envelopePlain(restored, "envAttack") == Catch::Approx(9.0f));
    CHECK(envelopePlain(restored, "macro4") == Catch::Approx(.35f));
}

TEST_CASE("Envelope preset schema validation is complete finite discrete and atomic",
          "[envelope-bank][state][preset][validation][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; configureEnvelopes(processor, 0);
    const auto valid = envelopePreset(processor);
    const auto before = envelopeHost(processor).toString();
    const auto reject = [&](juce::XmlElement candidate)
    {
        candidate.setAttribute("drive1", .95f);
        CHECK_FALSE(state::canLoadStateFromXml(candidate, processor));
        CHECK_FALSE(state::loadStateFromXml(candidate, processor));
        CHECK(envelopeHost(processor).toString() == before);
    };
    for (const auto* marker : {"0", "2", "1.5", "1e0", "nan", "1tail"})
    {
        auto invalid = valid; invalid.setAttribute(fire::envelope_state::marker, marker); reject(invalid);
    }
    for (const auto& id : env::parameterIDs())
    {
        CAPTURE(id); auto invalid = valid; invalid.removeAttribute(id); reject(invalid);
    }
    for (const auto* value : {"nan", "inf", "0.5tail", "-0.1", "1.01"})
    {
        auto invalid = valid; invalid.setAttribute(env::parameterID(2, env::highPassFrequency), value); reject(invalid);
    }
    for (const auto field : {env::source, env::highPassEnabled, env::lowPassEnabled})
    {
        auto invalid = valid; invalid.setAttribute(env::parameterID(2, field), .13f); reject(invalid);
    }
    auto noMarker = valid; noMarker.removeAttribute(fire::envelope_state::marker); reject(noMarker);
    auto noFamily = valid; stripEnvelopePreset(noFamily); noFamily.setAttribute("presetFormatVersion", 4); reject(noFamily);
    // An old-format document with any appended field must still contain the
    // entire validated family, even if its schema marker has been omitted.
    auto partialOld = valid; stripEnvelopePreset(partialOld); partialOld.setAttribute(env::parameterID(1, env::source), .2f); reject(partialOld);
    for (int version : {-1, 0, 1, 2, 3})
        for (int source : {21, 22, 23, 24})
        {
            CAPTURE(version, source);
            auto oldRoute = valid; stripEnvelopePreset(oldRoute, version); addEnvelopeRouting(oldRoute, source); reject(oldRoute);
        }
    auto unknownRoute = valid; addEnvelopeRouting(unknownRoute, 28); reject(unknownRoute);
    auto badTarget = valid; addEnvelopeRouting(badTarget, 23);
    badTarget.getChildByName("MODULATION_STATE")->getFirstChildElement()->setAttribute("target", env::parameterID(1, env::attack));
    reject(badTarget);
}

TEST_CASE("Envelope host schema rejects incomplete malformed and out of range detector settings atomically",
          "[envelope-bank][state][host][validation][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; configureEnvelopes(processor, 0);
    const auto valid = envelopeHost(processor), before = envelopePreset(processor);
    const auto reject = [&](juce::XmlElement candidate)
    {
        envelopeHostParameter(candidate, "drive1").setAttribute("value", 90.0f);
        restoreEnvelopeHost(processor, candidate);
        CHECK(processor.isCurrentStateEquivalentToPreset(before));
    };
    for (const auto* marker : {"0", "2", "1.5", "1e0", "nan"})
    { auto invalid = valid; invalid.setAttribute(fire::envelope_state::marker, marker); reject(invalid); }
    for (const auto& id : env::parameterIDs())
    {
        CAPTURE(id); auto invalid = valid; auto* parameters = invalid.getChildByName("PARAMETERS");
        parameters->removeChildElement(&envelopeHostParameter(invalid, id), true);
        invalid.setAttribute("savedParameterCount", parameters->getNumChildElements()); reject(invalid);
    }
    const std::array<std::pair<int, const char*>, 10> badValues {{
        {env::source, "1.5"}, {env::source, "6"}, {env::source, "-1"},
        {env::highPassEnabled, "0.5"}, {env::lowPassEnabled, "2"},
        {env::highPassFrequency, "NaN"}, {env::highPassFrequency, "19"},
        {env::lowPassFrequency, "20001"}, {env::attack, "0"}, {env::sensitivity, "25"} }};
    for (const auto& [field, value] : badValues)
    {
        CAPTURE(field, value); auto invalid = valid;
        envelopeHostParameter(invalid, env::parameterID(3, field)).setAttribute("value", value); reject(invalid);
    }
    auto noMarker = valid; noMarker.removeAttribute(fire::envelope_state::marker); reject(noMarker);
    auto absent = valid; stripEnvelopeHost(absent); absent.setAttribute("stateFormatVersion", 3); reject(absent);
    for (int source : {21, 22, 23})
    {
        CAPTURE(source); auto oldRoute = valid; stripEnvelopeHost(oldRoute);
        addEnvelopeRouting(oldRoute, source); reject(oldRoute);
    }
}

TEST_CASE("Damaged envelope alternate falls back to current sound without partial detector replacement",
          "[envelope-bank][state][ab][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source; configureEnvelopes(source, 0);
    source.stateAB.copyAB(false); source.stateAB.toggleAB(); configureEnvelopes(source, 1);
    auto saved = envelopeHost(source); const auto current = envelopePreset(source);
    auto* alternate = saved.getChildByName("AB_STATE"); REQUIRE(alternate != nullptr);
    SECTION("truncated") { alternate->removeAttribute(env::parameterID(3, env::source)); }
    SECTION("unknown schema") { alternate->setAttribute(fire::envelope_state::marker, 2); }
    SECTION("invalid detector source") { alternate->setAttribute(env::parameterID(2, env::source), .13f); }
    FireAudioProcessor restored; restoreEnvelopeHost(restored, saved);
    CHECK(restored.stateAB.isCurrentA()); CHECK(restored.isCurrentStateEquivalentToPreset(current));
    restored.stateAB.toggleAB(); CHECK(restored.isCurrentStateEquivalentToPreset(current));
}
