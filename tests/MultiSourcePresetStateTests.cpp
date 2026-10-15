#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/ModulationStateValidation.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <functional>
#include <vector>

namespace
{
std::array<ModulationRouting, 4> mappings()
{
    std::array<ModulationRouting, 4> result;
    result[0] = {0, "drive1", .25f, true, false};
    result[1] = {fire::mod_sources::envelope, "drive1", -.75f, false, false};
    result[1].customRange = true; result[1].rangeStart = .75f; result[1].rangeEnd = -.5f;
    result[1].curve = 1; result[1].depthMacro = 2;
    result[2] = {fire::mod_sources::firstMacro + 1, "drive1", .5f, false, true};
    result[2].curve = 2; result[2].depthMacro = 3;
    result[3] = {0, "drive1", -.5f, true, false};
    result[3].customRange = true; result[3].rangeStart = -.25f; result[3].rangeEnd = .5f;
    result[3].curve = 3;
    return result;
}

std::unique_ptr<juce::XmlElement> presetWithRoutes(FireAudioProcessor& processor,
                                                const std::array<ModulationRouting, 4>& routes)
{
    auto preset = std::make_unique<juce::XmlElement>("WINGSFIRE");
    state::saveStateToXml(processor, *preset);
    auto* modulation = preset->getChildByName("MODULATION_STATE");
    REQUIRE(modulation != nullptr);
    modulation->deleteAllChildElements();
    for (const auto& route : routes)
        route.writeToXml(*modulation->createNewChildElement("ROUTING"));
    return preset;
}

std::vector<ModulationRouting> populatedRoutes(const FireAudioProcessor& processor)
{
    std::vector<ModulationRouting> result;
    for (const auto& route : processor.getLfoManager().getModulationRoutingsCopy())
        if (route.targetParameterID.isNotEmpty()) result.push_back(route);
    return result;
}

void requireRoutes(const FireAudioProcessor& processor, const std::array<ModulationRouting, 4>& expected)
{
    const auto actual = populatedRoutes(processor);
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {CAPTURE(i); CHECK(actual[i].hasSameState(expected[i]));}
}

void downgrade(juce::XmlElement& preset, int version)
{
    if (version < 0) preset.removeAttribute("presetFormatVersion");
    else preset.setAttribute("presetFormatVersion", version);
    auto* routes = preset.getChildByName("MODULATION_STATE");
    REQUIRE(routes != nullptr);
    routes->removeAttribute("schemaVersion");
    for (auto* route : routes->getChildIterator())
        for (const auto* field : {"customRange", "rangeStart", "rangeEnd", "curve", "depthMacro"})
            route->removeAttribute(field);
}

std::unique_ptr<juce::XmlElement> hostSnapshot(FireAudioProcessor& processor)
{
    juce::MemoryBlock bytes;
    processor.getStateInformation(bytes);
    auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(xml != nullptr);
    return xml;
}

juce::String soundSnapshot(FireAudioProcessor& processor)
{
    return hostSnapshot(processor)->toString();
}
}

TEST_CASE("Preset v3 retains every source and advanced mapping on a shared target", "[preset][multi-source][mapping][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto routes = mappings();
    const auto preset = presetWithRoutes(source, routes);
    REQUIRE(preset->getIntAttribute("presetFormatVersion") == 3);
    REQUIRE(preset->getChildByName("MODULATION_STATE")->getIntAttribute("schemaVersion") == 2);
    REQUIRE(state::canLoadStateFromXml(*preset, source));
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(*preset, restored));
    requireRoutes(restored, routes);
    juce::XmlElement saved("WINGSFIRE"); state::saveStateToXml(restored, saved);
    REQUIRE(saved.getChildByName("MODULATION_STATE")->getNumChildElements() == 4);
    FireAudioProcessor second;
    REQUIRE(state::loadStateFromXml(saved, second));
    requireRoutes(second, routes);
    CHECK(second.isCurrentStateEquivalentToPreset(saved));
}

TEST_CASE("Multi-source preset mapping errors reject the entire sound before mutation", "[preset][multi-source][mapping][corrupt][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto valid = presetWithRoutes(processor, mappings());
    REQUIRE(state::loadStateFromXml(*valid, processor));
    REQUIRE(processor.setMacroDisplayName(0, "Keep this name"));
    processor.stateAB.copyAB(false);
    const auto before = soundSnapshot(processor);
    const std::vector<std::pair<const char*, const char*>> values {
        {"depth", "nan"}, {"depth", "1.1"}, {"source", "1.25"}, {"source", "999"}, {"source", "20e-1"},
        {"target", "missing-target"}, {"target", "macro1"},
        {"bipolar", "true"}, {"bypassed", "2"}, {"customRange", "true"}, {"customRange", "-1"},
        {"rangeStart", "nan"}, {"rangeStart", "-1.01"}, {"rangeEnd", "inf"}, {"rangeEnd", "1.01"},
        {"curve", "-1"}, {"curve", "4"}, {"curve", "1.5"}, {"curve", "1tail"}, {"curve", "20e-1"}, {"curve", "+1"},
        {"depthMacro", "-2"}, {"depthMacro", "4"}, {"depthMacro", "0.5"}, {"depthMacro", "1tail"}, {"depthMacro", "10e-1"}
    };
    const auto reject = [&](const juce::XmlElement& bad)
    {
        CHECK_FALSE(state::canLoadStateFromXml(bad, processor));
        CHECK_FALSE(state::loadStateFromXml(bad, processor));
        CHECK(soundSnapshot(processor) == before);
    };
    for (const auto& [field, value] : values)
    {
        CAPTURE(field, value); juce::XmlElement bad(*valid);
        bad.setAttribute("drive1", .95f);
        bad.getChildByName("MODULATION_STATE")->getFirstChildElement()->setAttribute(field, value);
        reject(bad);
    }
    for (const auto* field : {"source", "target", "depth", "bipolar", "bypassed", "customRange", "rangeStart", "rangeEnd", "curve", "depthMacro"})
    {
        CAPTURE(field); juce::XmlElement bad(*valid);
        bad.getChildByName("MODULATION_STATE")->getFirstChildElement()->removeAttribute(field);
        reject(bad);
    }
    for (const auto* schema : {"1", "3", "2oops", "2.5"})
    {
        CAPTURE(schema); juce::XmlElement bad(*valid);
        bad.getChildByName("MODULATION_STATE")->setAttribute("schemaVersion", schema);
        reject(bad);
    }
    juce::XmlElement missingSchema(*valid);
    missingSchema.getChildByName("MODULATION_STATE")->removeAttribute("schemaVersion"); reject(missingSchema);
    juce::XmlElement childPayload(*valid);
    childPayload.getChildByName("MODULATION_STATE")->getFirstChildElement()->createNewChildElement("UNEXPECTED"); reject(childPayload);
    juce::XmlElement tooMany(*valid);
    auto* matrix = tooMany.getChildByName("MODULATION_STATE");
    while (matrix->getNumChildElements() <= 128) mappings()[0].writeToXml(*matrix->createNewChildElement("ROUTING"));
    reject(tooMany);
}

TEST_CASE("Legacy preset formats preserve single-source mapping and historical duplicate migration", "[preset][multi-source][mapping][legacy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    for (const int version : {-1, 0, 1, 2})
    {
        CAPTURE(version);
        auto preset = presetWithRoutes(source, mappings());
        auto* matrix = preset->getChildByName("MODULATION_STATE");
        matrix->deleteAllChildElements(); mappings()[0].writeToXml(*matrix->createNewChildElement("ROUTING"));
        downgrade(*preset, version);
        FireAudioProcessor restored;
        REQUIRE(state::loadStateFromXml(*preset, restored));
        auto actual = populatedRoutes(restored); REQUIRE(actual.size() == 1);
        CHECK(actual[0].hasSameState(mappings()[0]));
        // Attributes unknown to the old format cannot opt a historical scene
        // into new mapping behaviour; its explicitly declared schema wins.
        auto* original = matrix->getFirstChildElement();
        original->setAttribute("customRange", true); original->setAttribute("rangeStart", -.5f);
        original->setAttribute("rangeEnd", .25f); original->setAttribute("curve", 3); original->setAttribute("depthMacro", 2);
        REQUIRE(state::loadStateFromXml(*preset, restored));
        actual = populatedRoutes(restored); REQUIRE(actual.size() == 1);
        CHECK(actual[0].hasSameState(mappings()[0]));
        mappings()[1].writeToXml(*matrix->createNewChildElement("ROUTING"));
        downgrade(*preset, version);
        if (version == 2)
        {
            const auto before = soundSnapshot(restored);
            CHECK_FALSE(state::loadStateFromXml(*preset, restored));
            CHECK(soundSnapshot(restored) == before);
        }
        else
        {
            REQUIRE(state::loadStateFromXml(*preset, restored));
            actual = populatedRoutes(restored); REQUIRE(actual.size() == 1);
            CHECK(actual[0].hasSameState(mappings()[0]));
        }
        matrix->setAttribute("schemaVersion", 2);
        CHECK_FALSE(state::loadStateFromXml(*preset, restored));
    }
}

TEST_CASE("AB snapshots roundtrip multiple source mappings and reject damaged advanced alternates", "[preset][multi-source][mapping][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto a = mappings();
    auto b = a; b[0].curve = 3; b[0].depthMacro = 0; b[2].isBypassed = false;
    const auto presetA = presetWithRoutes(processor, a);
    const auto presetB = presetWithRoutes(processor, b);
    REQUIRE(state::loadStateFromXml(*presetA, processor));
    processor.stateAB.copyAB(false);
    const auto savedA = hostSnapshot(processor);
    const auto* alternate = savedA->getChildByName("AB_STATE");
    REQUIRE(alternate != nullptr);
    REQUIRE(alternate->getIntAttribute("presetFormatVersion") == 3);
    REQUIRE(alternate->getChildByName("MODULATION_STATE") != nullptr);
    REQUIRE(alternate->getChildByName("MODULATION_STATE")->getIntAttribute("schemaVersion") == 2);
    REQUIRE(state::loadStateFromXml(*presetB, processor));
    processor.stateAB.toggleAB(); requireRoutes(processor, a);
    processor.stateAB.toggleAB(); requireRoutes(processor, b);
    auto damaged = hostSnapshot(processor);
    auto* damagedAlternate = damaged->getChildByName("AB_STATE");
    REQUIRE(damagedAlternate != nullptr);
    auto* damagedRoutes = damagedAlternate->getChildByName("MODULATION_STATE");
    REQUIRE(damagedRoutes != nullptr); REQUIRE(damagedRoutes->getFirstChildElement() != nullptr);
    damagedRoutes->getFirstChildElement()->setAttribute("curve", 99);
    damagedAlternate->setAttribute("currentSideIsA", false);
    juce::MemoryBlock damagedBytes;
    juce::AudioProcessor::copyXmlToBinary(*damaged, damagedBytes);
    processor.setStateInformation(damagedBytes.getData(), static_cast<int>(damagedBytes.getSize()));
    // The public host restore keeps live B and rejects the damaged alternate,
    // rebuilding it from that complete live state with the historical A label.
    CHECK(processor.stateAB.isCurrentA());
    requireRoutes(processor, b);
    REQUIRE(state::loadStateFromXml(*presetA, processor));
    processor.stateAB.toggleAB(); requireRoutes(processor, b); // fallback mirrored the complete live B
}
