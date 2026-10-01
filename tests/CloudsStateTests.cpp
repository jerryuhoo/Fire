#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/CloudsParameters.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

namespace
{
namespace clouds = fire::clouds_params;
namespace fx = fire::effects;

void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float get(FireAudioProcessor& processor, const juce::String& id)
{
    const auto* parameter = processor.treeState.getRawParameterValue(id);
    REQUIRE(parameter != nullptr);
    return parameter->load();
}

juce::XmlElement preset(FireAudioProcessor& processor)
{
    juce::XmlElement result("WINGSFIRE");
    state::saveStateToXml(processor, result);
    return result;
}

juce::XmlElement hostState(FireAudioProcessor& processor)
{
    juce::MemoryBlock bytes;
    processor.getStateInformation(bytes);
    auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(xml != nullptr);
    return *xml;
}

void restoreHost(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock bytes;
    juce::AudioProcessor::copyXmlToBinary(xml, bytes);
    processor.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
}

void stripClouds(juce::XmlElement& xml)
{
    xml.removeAttribute("cloudsSchemaVersion");
    for (const auto& id : clouds::parameterIDs()) xml.removeAttribute(id);
}

void configureClouds(FireAudioProcessor& processor, int scope, int slot, float amount)
{
    set(processor, fx::parameterID(scope, slot, fx::typeField), 4.0f);
    set(processor, clouds::parameterID(scope, slot, clouds::engineField), 1.0f);
    set(processor, clouds::parameterID(scope, slot, clouds::freezeField), 1.0f);
    set(processor, clouds::parameterID(scope, slot, clouds::spreadField), amount);
    set(processor, clouds::parameterID(scope, slot, clouds::feedbackField), amount * 0.5f);
    set(processor, clouds::parameterID(scope, slot, clouds::reverbField), amount * 0.75f);
}

void checkExtensionDefaults(FireAudioProcessor& processor, int scope, int slot, bool newGranular)
{
    for (int field = 0; field < clouds::fieldCount; ++field)
    {
        CAPTURE(scope, slot, field);
        const auto expected = newGranular && field == clouds::engineField
                                ? 1.0f : clouds::defaults[static_cast<size_t>(field)];
        CHECK(get(processor, clouds::parameterID(scope, slot, field)) == Catch::Approx(expected));
    }
}
}

TEST_CASE("Clouds appends a separate parameter family without changing original insert identities",
          "[clouds][state][parameters][compatibility]")
{
    FireAudioProcessor processor;
    const auto& parameters = processor.getParameters();
    REQUIRE(fx::controlCount == 6);
    REQUIRE(fx::fieldCount == 9);
    REQUIRE(fx::parameterCount == 363);
    REQUIRE(clouds::parameterIDs().size() == 200);
    int index = parameters.indexOf(processor.treeState.getParameter(fx::parameterID(0, 0, 0)));
    REQUIRE(index >= 0);
    const auto checkParameter = [&](const juce::String& id, int hint)
    {
        CAPTURE(id, index);
        auto* parameter = processor.treeState.getParameter(id);
        REQUIRE(parameter != nullptr);
        REQUIRE(index < parameters.size());
        CHECK(parameters[index++] == parameter);
        CHECK(parameter->getVersionHint() == hint);
    };
    for (int scope = 0; scope < fx::scopeCount; ++scope)
        for (int slot = 0; slot < fx::slotCount; ++slot)
            for (int field = 0; field < fx::fieldCount; ++field)
                checkParameter(fx::parameterID(scope, slot, field), 3);
    for (const auto* id : fx::tapeIDs) checkParameter(id, 3);
    for (int scope = 0; scope < fx::scopeCount; ++scope)
        for (int node = 0; node < fire::module_order::capacity; ++node)
            if (fire::module_order::valid(scope, node))
                checkParameter(fire::module_order::parameterID(scope, node), 4);
    for (const auto& id : clouds::parameterIDs())
    {
        checkParameter(id, 5);
        CHECK(clouds::isParameterID(id));
        CHECK_FALSE(fx::isParameterID(id));
        if (clouds::isReservedEngineParameterID(id))
        {
            const auto* reserved = processor.treeState.getParameter(id);
            CHECK_FALSE(reserved->isAutomatable());
            CHECK(reserved->isMetaParameter());
            CHECK(reserved->getNormalisableRange().start == 0.0f);
            CHECK(reserved->getNormalisableRange().end == 1.0f);
        }
    }
    // All pre-EQ host indices, IDs and version hints above remain untouched.
    // Keep an independent append-order contract rather than deriving the
    // expected sequence from the new parameter-registration helper.
    CHECK(index == 810);
    for (const auto* id : {"eqNode1Type", "eqNode1Present", "eqNode2Slope",
                           "eqNode2Type", "eqNode2Present", "eqNode3Type", "eqNode3Present"})
        checkParameter(id, 6);
    for (int node = 4; node <= 12; ++node)
        for (const auto* suffix : {"Freq", "Gain", "Q", "Slope", "Type", "Present", "Bypassed"})
            checkParameter("eqNode" + juce::String(node) + suffix, 6);
    CHECK(index == 880);
    for (int source = 5; source <= 16; ++source)
        for (const auto* base : {"lfoSyncMode", "lfoRateSync", "lfoRateHz", "lfoSmooth", "lfoPhase"})
            checkParameter(juce::String(base) + juce::String(source), 7);
    for (int source = 1; source <= 16; ++source)
        checkParameter("lfoPresent" + juce::String(source), 7);
    CHECK(index == 956);
    for (int scope = 0; scope <= 4; ++scope)
        for (int slot = 1; slot <= 8; ++slot)
            checkParameter((scope == 0 ? juce::String("masterFx") : "bandFx") + juce::String(slot)
                               + "ModulationType" + (scope == 0 ? juce::String() : juce::String(scope)), 8);
    CHECK(index == 996);
    for (int scope = 0; scope <= 4; ++scope)
        for (int slot = 1; slot <= 8; ++slot)
            checkParameter((scope == 0 ? juce::String("masterFx") : "bandFx") + juce::String(slot)
                               + "Resonator" + (scope == 0 ? juce::String() : juce::String(scope)), 9);
    CHECK(index == 1036);
    for (int band = 1; band <= 4; ++band)
        checkParameter("driveCompModern" + juce::String(band), 10);
    CHECK(index == 1040);
    for (auto* id : fire::mod_sources::ids) checkParameter(id, 11);
    CHECK(index == 1047);
    for (int scope = 0; scope < fx::scopeCount; ++scope)
    {
        for (int node = 0; node < (scope == 0 ? 3 : 5); ++node)
            checkParameter(fire::core_modules::presenceID(scope, node), 12);
        for (int slot = 0; slot < fx::slotCount; ++slot)
            for (int field = 0; field < fire::core_modules::slotFieldCount; ++field)
                checkParameter(fire::core_modules::parameterID(scope, slot, field), 12);
    }
    CHECK(index == 1047 + fire::core_modules::parameterCount);
    for (int band = 0; band < 4; ++band) checkParameter(fire::analog_params::bandID(band), 13);
    for (int scope = 0; scope < 5; ++scope)
        for (int slot = 0; slot < 8; ++slot) checkParameter(fire::analog_params::parameterID(scope, slot), 13);
    for (int scope = 0; scope < 5; ++scope)
        for (int slot = 0; slot < 8; ++slot) checkParameter(fire::analog_params::driveID(scope, slot), 13);
    CHECK(index == parameters.size());
    CHECK_FALSE(clouds::isParameterID("masterFx1CloudsBogus"));
    CHECK_FALSE(clouds::isParameterID("bandFx9CloudsEngine1"));
    const auto* type = processor.treeState.getParameter(fx::parameterID(0, 0, fx::typeField));
    REQUIRE(type != nullptr);
    CHECK(type->convertFrom0to1(0.8f) == Catch::Approx(4.0f));
    for (int scope = 0; scope < clouds::scopeCount; ++scope)
        for (int slot = 0; slot < clouds::slotCount; ++slot)
            checkExtensionDefaults(processor, scope, slot, false);
}

TEST_CASE("Legacy Granular presets and host AB snapshots migrate once to Clouds",
          "[clouds][state][preset][host][ab][legacy]")
{
    FireAudioProcessor source, restored;
    REQUIRE(source.addInsertEffect(0, fx::Type::granular) == 0);
    configureClouds(source, 0, 0, 0.8f); // Old Legacy ignored these optional settings.
    constexpr std::array<float, 6> oldValues { 0.6f, 0.2f, 0.7f, 0.4f, 0.9f, 0.3f };
    for (int control = 0; control < 6; ++control)
        set(source, fx::parameterID(0, 0, control), oldValues[static_cast<size_t>(control)]);
    source.stateAB.copyAB(false);
    auto legacy = preset(source);
    bool missingFamily = false;
    SECTION("before the Clouds family existed") { missingFamily = true; }
    SECTION("version one with the removed Legacy engine") { missingFamily = false; }
    legacy.setAttribute("cloudsSchemaVersion", 1);
    legacy.setAttribute(clouds::parameterID(0, 0, clouds::engineField), 0.0f);
    if (missingFamily) stripClouds(legacy);
    REQUIRE(state::loadStateFromXml(legacy, restored));
    CHECK(restored.getInsertEffectType(0, 0) == fx::Type::granular);
    checkExtensionDefaults(restored, 0, 0, false);
    // Independently calculated physical migration: 112.4385 ms grain,
    // 9.6 grains/s, 192 ms position; +9.6 st and Mix remain unchanged.
    constexpr std::array<float, 6> expected { 0.45324816f, 0.31759135f, 0.7f, 0.0f, 0.95f, 0.3f };
    for (int control = 0; control < 6; ++control)
        CHECK(get(restored, fx::parameterID(0, 0, control)) == Catch::Approx(expected[static_cast<size_t>(control)]).margin(1.0e-6f));
    CHECK(get(restored, fx::parameterID(0, 0, 2)) == oldValues[2]);
    CHECK(get(restored, fx::parameterID(0, 0, 5)) == oldValues[5]);
    CHECK(restored.isCurrentStateEquivalentToPreset(legacy));
    const auto migrated = preset(restored);
    CHECK(migrated.getIntAttribute("cloudsSchemaVersion") == 2);
    REQUIRE(state::loadStateFromXml(migrated, restored));
    CHECK(restored.isCurrentStateEquivalentToPreset(migrated));

    auto host = hostState(source);
    host.setAttribute("cloudsSchemaVersion", 1);
    auto* parameters = host.getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    auto* engine = parameters->getChildByAttribute("id", clouds::parameterID(0, 0, clouds::engineField));
    REQUIRE(engine != nullptr);
    engine->setAttribute("value", 0.0f);
    if (missingFamily)
    {
        host.removeAttribute("cloudsSchemaVersion");
        for (int child = parameters->getNumChildElements(); --child >= 0;)
            if (clouds::isParameterID(parameters->getChildElement(child)->getStringAttribute("id")))
                parameters->removeChildElement(parameters->getChildElement(child), true);
    }
    host.setAttribute("savedParameterCount", parameters->getNumChildElements());
    auto* alternate = host.getChildByName("AB_STATE");
    REQUIRE(alternate != nullptr);
    alternate->setAttribute("cloudsSchemaVersion", 1);
    alternate->setAttribute(clouds::parameterID(0, 0, clouds::engineField), 0.0f);
    if (missingFamily) stripClouds(*alternate);
    configureClouds(restored, 0, 0, 0.8f);
    restoreHost(restored, host);
    checkExtensionDefaults(restored, 0, 0, false);
    CHECK(restored.isCurrentStateEquivalentToPreset(migrated));
    set(restored, fx::parameterID(0, 0, 0), 0.1f);
    restored.stateAB.toggleAB();
    CHECK(get(restored, fx::parameterID(0, 0, 0)) == Catch::Approx(expected[0]).margin(1.0e-6f));
    checkExtensionDefaults(restored, 0, 0, false);
}

TEST_CASE("Clouds controls and modulation survive preset host and independent AB round trips",
          "[clouds][state][preset][host][ab][modulation]")
{
    FireAudioProcessor source, restored;
    configureClouds(source, 0, 0, 0.24f);
    configureClouds(source, 3, 6, 0.68f);
    const auto routed = clouds::parameterID(3, 6, clouds::reverbField);
    REQUIRE(source.assignLfoToTarget(2, routed) == LfoManager::AssignmentResult::changed);
    source.setModulationDepth(routed, -0.37f);
    const auto alternate = preset(source);
    source.stateAB.copyAB(false);
    source.stateAB.toggleAB();
    configureClouds(source, 0, 0, 0.81f);
    set(source, clouds::parameterID(3, 6, clouds::engineField), 0.0f);
    const auto current = preset(source);
    REQUIRE(current.getIntAttribute("cloudsSchemaVersion") == 2);
    REQUIRE(state::loadStateFromXml(current, restored));
    CHECK(restored.isCurrentStateEquivalentToPreset(current));
    const auto host = hostState(source);
    REQUIRE(host.getIntAttribute("cloudsSchemaVersion") == 2);
    restoreHost(restored, host);
    CHECK_FALSE(restored.stateAB.isCurrentA());
    CHECK(restored.isCurrentStateEquivalentToPreset(current));
    restored.stateAB.toggleAB();
    CHECK(restored.stateAB.isCurrentA());
    CHECK(restored.isCurrentStateEquivalentToPreset(alternate));
    CHECK(restored.getModulationInfoForParameter(routed).isModulated);
    restored.stateAB.copyAB(false);
    restored.stateAB.toggleAB();
    CHECK(restored.isCurrentStateEquivalentToPreset(alternate));
}

TEST_CASE("Incomplete and future Clouds schemas reject whole preset and host replacements",
          "[clouds][state][preset][host][corrupt]")
{
    FireAudioProcessor source, restored;
    configureClouds(source, 0, 0, 0.75f);
    configureClouds(restored, 0, 0, 0.21f);
    const auto unchanged = preset(restored);
    const auto unchangedHost = hostState(restored).toString();
    auto incomingPreset = preset(source);
    auto incomingHost = hostState(source);
    const auto missing = clouds::parameterID(0, 7, clouds::spreadField);
    SECTION("missing field")
    {
        incomingPreset.removeAttribute(missing);
        auto* parameters = incomingHost.getChildByName("PARAMETERS");
        REQUIRE(parameters != nullptr);
        auto* child = parameters->getChildByAttribute("id", missing);
        REQUIRE(child != nullptr);
        parameters->removeChildElement(child, true);
        incomingHost.setAttribute("savedParameterCount", parameters->getNumChildElements());
    }
    SECTION("future schema")
    {
        incomingPreset.setAttribute("cloudsSchemaVersion", 3);
        incomingHost.setAttribute("cloudsSchemaVersion", 3);
    }
    SECTION("nonfinite field")
    {
        incomingPreset.setAttribute(missing, "nan");
        auto* parameters = incomingHost.getChildByName("PARAMETERS");
        REQUIRE(parameters != nullptr);
        auto* child = parameters->getChildByAttribute("id", missing);
        REQUIRE(child != nullptr);
        child->setAttribute("value", "nan");
    }
    CHECK_FALSE(state::loadStateFromXml(incomingPreset, restored));
    CHECK(restored.isCurrentStateEquivalentToPreset(unchanged));
    restoreHost(restored, incomingHost);
    CHECK(hostState(restored).toString() == unchangedHost);
}

TEST_CASE("A damaged Clouds alternate falls back without partially replacing the live sound",
          "[clouds][state][host][ab][fallback]")
{
    FireAudioProcessor source, restored;
    configureClouds(source, 0, 0, 0.25f);
    source.stateAB.copyAB(false);
    source.stateAB.toggleAB();
    configureClouds(source, 0, 0, 0.82f);
    const auto current = preset(source);
    auto host = hostState(source);
    auto* alternate = host.getChildByName("AB_STATE");
    REQUIRE(alternate != nullptr);
    alternate->removeAttribute(clouds::parameterID(0, 0, clouds::freezeField));
    restoreHost(restored, host);
    CHECK(restored.stateAB.isCurrentA());
    CHECK(restored.isCurrentStateEquivalentToPreset(current));
    restored.stateAB.toggleAB();
    CHECK(restored.isCurrentStateEquivalentToPreset(current));
}

TEST_CASE("Clouds preset equivalence rejects damage before migration can fill or replace it",
          "[clouds][state][preset][equivalence][corrupt]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(0, fx::Type::granular) == 0);
    const auto complete = preset(processor);
    REQUIRE(processor.isCurrentStateEquivalentToPreset(complete));
    auto damaged = complete;
    SECTION("missing default Freeze")
    {
        damaged.removeAttribute(clouds::parameterID(0, 0, clouds::freezeField));
    }
    SECTION("malformed schema")
    {
        damaged.setAttribute("cloudsSchemaVersion", "2oops");
    }
    SECTION("unsupported future schema")
    {
        damaged.setAttribute("cloudsSchemaVersion", 99);
    }
    CHECK_FALSE(state::canLoadStateFromXml(damaged, processor));
    CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(damaged));
    CHECK_FALSE(state::loadStateFromXml(damaged, processor));
    CHECK(processor.isCurrentStateEquivalentToPreset(complete));
}

TEST_CASE("New Granular slots initialise Clouds and reuse clears extensions and their routings",
          "[clouds][state][slots][modulation]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(1, fx::Type::granular) == 0);
    checkExtensionDefaults(processor, 1, 0, true);
    constexpr std::array<float, 6> normalisedDefaults { 0.5f, 0.25f, 0.5f, 0.1f, 0.5f, 0.35f };
    for (int control = 0; control < 6; ++control)
        CHECK(get(processor, fx::parameterID(1, 0, control)) == Catch::Approx(normalisedDefaults[static_cast<size_t>(control)]));
    configureClouds(processor, 1, 0, 0.9f);
    for (int field = clouds::spreadField; field < clouds::fieldCount; ++field)
        REQUIRE(processor.assignLfoToTarget(0, clouds::parameterID(1, 0, field)) == LfoManager::AssignmentResult::changed);
    processor.removeInsertEffect(1, 0);
    checkExtensionDefaults(processor, 1, 0, false);
    for (int field = clouds::spreadField; field < clouds::fieldCount; ++field)
        CHECK_FALSE(processor.getModulationInfoForParameter(clouds::parameterID(1, 0, field)).isModulated);
    REQUIRE(processor.addInsertEffect(1, fx::Type::delay) == 0);
    checkExtensionDefaults(processor, 1, 0, false);
    processor.removeInsertEffect(1, 0);
    REQUIRE(processor.addInsertEffect(1, fx::Type::granular) == 0);
    checkExtensionDefaults(processor, 1, 0, true);
}

TEST_CASE("Band insertion and deletion migrate complete Clouds extension state and targets",
          "[clouds][state][topology][modulation]")
{
    FireAudioProcessor processor;
    set(processor, NUM_BANDS_ID, 2.0f);
    set(processor, "lineState1", 1.0f);
    set(processor, "freq1", 300.0f);
    configureClouds(processor, 1, 0, 0.61f);
    const auto originalTarget = clouds::parameterID(1, 0, clouds::feedbackField);
    REQUIRE(processor.assignLfoToTarget(1, originalTarget) == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.addMultibandBand(0, 2, true, 100.0f));
    checkExtensionDefaults(processor, 1, 0, false);
    CHECK(get(processor, clouds::parameterID(2, 0, clouds::engineField)) == 1.0f);
    CHECK(get(processor, clouds::parameterID(2, 0, clouds::freezeField)) == 1.0f);
    CHECK(get(processor, clouds::parameterID(2, 0, clouds::spreadField)) == Catch::Approx(0.61f));
    CHECK(processor.getModulationInfoForParameter(clouds::parameterID(2, 0, clouds::feedbackField)).isModulated);
    CHECK_FALSE(processor.getModulationInfoForParameter(originalTarget).isModulated);
    REQUIRE(processor.deleteMultibandBand(0, 3));
    CHECK(get(processor, clouds::parameterID(1, 0, clouds::spreadField)) == Catch::Approx(0.61f));
    CHECK(processor.getModulationInfoForParameter(originalTarget).isModulated);
    checkExtensionDefaults(processor, 3, 0, false);
}

TEST_CASE("Clouds reports a conservative tail and infinite active freeze or full feedback",
          "[clouds][state][tail]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(0, fx::Type::granular) == 0);
    CHECK(processor.getTailLengthSeconds() >= 180.0);
    CHECK(std::isfinite(processor.getTailLengthSeconds()));
    set(processor, clouds::parameterID(0, 0, clouds::freezeField), 1.0f);
    CHECK(std::isinf(processor.getTailLengthSeconds()));
    set(processor, fx::parameterID(0, 0, fx::enabledField), 0.0f);
    CHECK(std::isfinite(processor.getTailLengthSeconds()));
    set(processor, fx::parameterID(0, 0, fx::enabledField), 1.0f);
    set(processor, clouds::parameterID(0, 0, clouds::freezeField), 0.0f);
    set(processor, clouds::parameterID(0, 0, clouds::feedbackField), 1.0f);
    CHECK(std::isinf(processor.getTailLengthSeconds()));
    set(processor, clouds::parameterID(0, 0, clouds::engineField), 0.0f);
    CHECK(std::isinf(processor.getTailLengthSeconds())); // Deprecated engine automation is inert.
    set(processor, clouds::parameterID(0, 0, clouds::engineField), 1.0f);
    set(processor, fx::parameterID(0, 0, fx::typeField), 2.0f);
    CHECK(std::isfinite(processor.getTailLengthSeconds()));

    // Dormant band settings survive a band-count reduction. They must not
    // advertise an endless tail when their frozen audio cannot be heard.
    REQUIRE(processor.addInsertEffect(3, fx::Type::granular) == 0);
    set(processor, clouds::parameterID(3, 0, clouds::freezeField), 1.0f);
    set(processor, NUM_BANDS_ID, 1.0f);
    CHECK(std::isfinite(processor.getTailLengthSeconds()));
    set(processor, NUM_BANDS_ID, 3.0f);
    set(processor, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 2), 1.0f);
    CHECK(std::isinf(processor.getTailLengthSeconds()));
    set(processor, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 2), 0.0f);
    CHECK(std::isfinite(processor.getTailLengthSeconds()));
    set(processor, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 2), 1.0f);
    set(processor, ParameterIDAndName::getIDString(BAND_SOLO_ID, 0), 1.0f);
    CHECK(std::isfinite(processor.getTailLengthSeconds()));
    set(processor, ParameterIDAndName::getIDString(BAND_SOLO_ID, 2), 1.0f);
    CHECK(std::isinf(processor.getTailLengthSeconds()));
}

TEST_CASE("Clouds discrete preset values are canonical before comparison and AB switching",
          "[clouds][state][preset][ab][normalised]")
{
    FireAudioProcessor source, restored;
    configureClouds(source, 0, 0, 0.63f);
    const auto freezeID = clouds::parameterID(0, 0, clouds::freezeField);
    const auto engineID = clouds::parameterID(0, 0, clouds::engineField);
    auto canonical = preset(source);
    for (const auto fractionalValue : { 0.25f, 0.5f, 0.75f })
    {
        CAPTURE(fractionalValue);
        auto incoming = canonical;
        incoming.setAttribute(freezeID, fractionalValue);
        incoming.setAttribute(engineID, fractionalValue);
        REQUIRE(state::loadStateFromXml(incoming, restored));
        for (const auto& id : { freezeID, engineID })
        {
            CAPTURE(id);
            auto* parameter = restored.treeState.getParameter(id);
            REQUIRE(parameter != nullptr);
            const auto expected = id == engineID ? 1.0f
                : parameter->convertTo0to1(parameter->convertFrom0to1(fractionalValue));
            CHECK(parameter->getValue() == expected);
            CHECK(get(restored, id) == expected);
        }
        CHECK(restored.isCurrentStateEquivalentToPreset(incoming));

        // Host alternate snapshots carry normalised preset data too. Their
        // accepted fractional values must take the identical canonical path.
        auto host = hostState(source);
        auto* alternate = host.getChildByName("AB_STATE");
        REQUIRE(alternate != nullptr);
        *alternate = incoming;
        alternate->setTagName("AB_STATE");
        alternate->setAttribute("currentSideIsA", true);
        restoreHost(restored, host);
        restored.stateAB.toggleAB();
        CHECK(restored.isCurrentStateEquivalentToPreset(incoming));
        auto* freeze = restored.treeState.getParameter(freezeID);
        REQUIRE(freeze != nullptr);
        CHECK(freeze->getValue() == get(restored, freezeID));
    }
}

TEST_CASE("Existing Clouds one states and all version two states keep their control values",
          "[clouds][state][migration][idempotent]")
{
    FireAudioProcessor source, restored;
    configureClouds(source, 0, 0, 0.73f);
    constexpr std::array<float, 6> values { 0.17f, 0.69f, 0.83f, 0.57f, 0.42f, 0.91f };
    for (int control = 0; control < 6; ++control)
        set(source, fx::parameterID(0, 0, control), values[static_cast<size_t>(control)]);
    source.stateAB.copyAB(false);
    const auto original = preset(source);
    for (int version : { 1, 2 })
    {
        CAPTURE(version);
        auto incoming = original;
        incoming.setAttribute("cloudsSchemaVersion", version);
        // A version-two reserved automation value of zero must not trigger
        // a second Legacy conversion or select a removed implementation.
        incoming.setAttribute(clouds::parameterID(0, 0, clouds::engineField), version == 1 ? 1.0f : 0.0f);
        REQUIRE(state::loadStateFromXml(incoming, restored));
        for (int control = 0; control < 6; ++control)
            CHECK(get(restored, fx::parameterID(0, 0, control)) == values[static_cast<size_t>(control)]);
        CHECK(get(restored, clouds::parameterID(0, 0, clouds::freezeField)) == 1.0f);
        CHECK(get(restored, clouds::parameterID(0, 0, clouds::spreadField)) == 0.73f);
        CHECK(restored.isCurrentStateEquivalentToPreset(original));

        auto host = hostState(source);
        host.setAttribute("cloudsSchemaVersion", version);
        auto* parameters = host.getChildByName("PARAMETERS");
        REQUIRE(parameters != nullptr);
        auto* engine = parameters->getChildByAttribute("id", clouds::parameterID(0, 0, clouds::engineField));
        REQUIRE(engine != nullptr);
        engine->setAttribute("value", version == 1 ? 1.0f : 0.0f);
        auto* alternate = host.getChildByName("AB_STATE");
        REQUIRE(alternate != nullptr);
        *alternate = incoming;
        alternate->setTagName("AB_STATE");
        alternate->setAttribute("currentSideIsA", true);
        restoreHost(restored, host);
        CHECK(restored.isCurrentStateEquivalentToPreset(original));
        restored.stateAB.toggleAB();
        CHECK(restored.isCurrentStateEquivalentToPreset(original));
        const auto migratedHost = hostState(restored);
        CHECK(migratedHost.getIntAttribute("cloudsSchemaVersion") == 2);
        REQUIRE(migratedHost.getChildByName("AB_STATE") != nullptr);
        CHECK(migratedHost.getChildByName("AB_STATE")->getIntAttribute("cloudsSchemaVersion") == 2);
        restoreHost(restored, migratedHost);
        CHECK(restored.isCurrentStateEquivalentToPreset(original));
    }
}

TEST_CASE("Reserved engine automation cannot alter or dirty the sole Granular engine",
          "[clouds][state][reserved][automation]")
{
    FireAudioProcessor processor;
    configureClouds(processor, 0, 0, 0.67f);
    const auto expected = preset(processor);
    const auto id = clouds::parameterID(0, 0, clouds::engineField);
    const auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    CHECK_FALSE(parameter->isAutomatable());
    CHECK(parameter->isMetaParameter());
    for (float historicalValue : { 0.0f, 1.0f, 0.0f })
    {
        set(processor, id, historicalValue);
        CHECK(processor.isCurrentStateEquivalentToPreset(expected));
        CHECK(std::isinf(processor.getTailLengthSeconds()));
        const auto savedPreset = preset(processor);
        CHECK(savedPreset.getDoubleAttribute(id) == 1.0);
        const auto savedHost = hostState(processor);
        auto* parameters = savedHost.getChildByName("PARAMETERS");
        REQUIRE(parameters != nullptr);
        auto* engine = parameters->getChildByAttribute("id", id);
        REQUIRE(engine != nullptr);
        CHECK(engine->getDoubleAttribute("value") == 1.0);
    }
}

TEST_CASE("Legacy migration stays bounded at extreme grain size pitch and position",
          "[clouds][state][migration][bounds]")
{
    for (float size : { 0.0f, 1.0f })
        for (float density : { 0.0f, 1.0f })
            for (float pitch : { 0.0f, 0.5f, 1.0f })
                for (float position : { 0.0f, 1.0f })
                    for (float spray : { 0.0f, 1.0f })
                    {
                        CAPTURE(size, density, pitch, position, spray);
                        const auto converted = clouds::migrateLegacyGranular({size, density, pitch, position, spray, 0.37f});
                        for (const auto value : converted)
                        {
                            CHECK(std::isfinite(value));
                            CHECK(value >= 0.0f);
                            CHECK(value <= 1.0f);
                        }
                        CHECK(converted[2] == pitch);
                        CHECK(converted[5] == 0.37f);
                    }
}

TEST_CASE("Host Legacy Granular conversion invalidates matching only when a sound is migrated",
          "[clouds][state][migration][host][ab][loudness-match]")
{
    FireAudioProcessor source, restored;
    configureClouds(source, 0, 0, 0.67f);
    source.stateAB.copyAB(false);
    auto host = hostState(source);
    auto* parameters = host.getChildByName("PARAMETERS");
    auto* alternate = host.getChildByName("AB_STATE");
    auto* metadata = host.getChildByName("otherState");
    REQUIRE(parameters != nullptr);
    REQUIRE(alternate != nullptr);
    REQUIRE(metadata != nullptr);
    metadata->setAttribute("loudnessMatchVersion", 1);
    metadata->setAttribute("loudnessMatchEnabled", true);
    metadata->setAttribute("loudnessMatchReadyA", true);
    metadata->setAttribute("loudnessMatchReadyB", true);
    metadata->setAttribute("loudnessMatchGainA", -6.0f);
    metadata->setAttribute("loudnessMatchGainB", 3.0f);
    bool shouldClear = false;

    SECTION("the main sound used Legacy")
    {
        shouldClear = true;
        host.setAttribute("cloudsSchemaVersion", 1);
        auto* engine = parameters->getChildByAttribute("id", clouds::parameterID(0, 0, clouds::engineField));
        REQUIRE(engine != nullptr);
        engine->setAttribute("value", 0.0f);
    }
    SECTION("the alternate sound used Legacy")
    {
        shouldClear = true;
        alternate->setAttribute("cloudsSchemaVersion", 1);
        alternate->setAttribute(clouds::parameterID(0, 0, clouds::engineField), 0.0f);
    }
    SECTION("existing Clouds version one sounds keep their calibration")
    {
        host.setAttribute("cloudsSchemaVersion", 1);
        alternate->setAttribute("cloudsSchemaVersion", 1);
    }
    SECTION("current version two sounds keep their calibration")
    {
        REQUIRE(host.getIntAttribute("cloudsSchemaVersion") == 2);
    }
    SECTION("a missing optional family without Granular does not change the sound")
    {
        const auto typeID = fx::parameterID(0, 0, fx::typeField);
        auto* type = parameters->getChildByAttribute("id", typeID);
        REQUIRE(type != nullptr);
        type->setAttribute("value", 0.0f);
        alternate->setAttribute(typeID, 0.0f);
        host.removeAttribute("cloudsSchemaVersion");
        for (int child = parameters->getNumChildElements(); --child >= 0;)
            if (clouds::isParameterID(parameters->getChildElement(child)->getStringAttribute("id")))
                parameters->removeChildElement(parameters->getChildElement(child), true);
        host.setAttribute("savedParameterCount", parameters->getNumChildElements());
        stripClouds(*alternate);
    }

    restoreHost(restored, host);
    const auto current = restored.getLoudnessMatchState();
    CHECK(current.enabled == ! shouldClear);
    CHECK(current.ready == ! shouldClear);
    CHECK_FALSE(current.measuring);
    CHECK(current.gainDb == Catch::Approx(shouldClear ? 0.0f : -6.0f));
    restored.stateAB.toggleAB();
    const auto other = restored.getLoudnessMatchState();
    CHECK(other.enabled == ! shouldClear);
    CHECK(other.ready == ! shouldClear);
    CHECK_FALSE(other.measuring);
    CHECK(other.gainDb == Catch::Approx(shouldClear ? 0.0f : 3.0f));
}

TEST_CASE("Legacy migration removes latent Clouds extension routings without dropping audible controls",
          "[clouds][state][migration][modulation][preset][host][ab]")
{
    for (int scope : { 0, 2 })
        for (bool migratingLegacy : { false, true })
        {
            CAPTURE(scope, migratingLegacy);
            FireAudioProcessor source, restored;
            configureClouds(source, scope, 0, 0.67f);
            configureClouds(source, scope, 1, 0.43f);
            const auto feedbackID = clouds::parameterID(scope, 0, clouds::feedbackField);
            const auto mainPitchID = fx::parameterID(scope, 0, 2);
            const auto existingCloudsFeedbackID = clouds::parameterID(scope, 1, clouds::feedbackField);
            set(source, feedbackID, 0.0f);
            LfoData fullLevel;
            fullLevel.points = { { 0.0f, 1.0f }, { 1.0f, 1.0f } };
            fullLevel.curvatures = { 0.0f };
            source.getLfoManager().setLfoData(0, fullLevel);
            for (int field = clouds::spreadField; field < clouds::fieldCount; ++field)
            {
                const auto target = clouds::parameterID(scope, 0, field);
                REQUIRE(source.assignLfoToTarget(0, target) == LfoManager::AssignmentResult::changed);
                source.setModulationDepth(target, 1.0f);
                source.toggleBipolarMode(target);
            }
            REQUIRE(source.assignLfoToTarget(1, mainPitchID) == LfoManager::AssignmentResult::changed);
            source.setModulationDepth(mainPitchID, 0.3f);
            REQUIRE(source.assignLfoToTarget(0, existingCloudsFeedbackID) == LfoManager::AssignmentResult::changed);
            source.setModulationDepth(existingCloudsFeedbackID, 1.0f);
            source.toggleBipolarMode(existingCloudsFeedbackID);
            REQUIRE(get(source, feedbackID) == 0.0f);
            REQUIRE(source.getModulationInfoForParameter(feedbackID).depth == 1.0f);
            REQUIRE_FALSE(source.getModulationInfoForParameter(feedbackID).isBipolar);
            source.stateAB.copyAB(false);

            auto incomingPreset = preset(source);
            incomingPreset.setAttribute("cloudsSchemaVersion", 1);
            incomingPreset.setAttribute(clouds::parameterID(scope, 0, clouds::engineField), migratingLegacy ? 0.0f : 1.0f);
            const auto checkRestored = [&]
            {
                for (int field = clouds::spreadField; field < clouds::fieldCount; ++field)
                {
                    const auto id = clouds::parameterID(scope, 0, field);
                    CHECK(restored.getModulationInfoForParameter(id).isModulated == ! migratingLegacy);
                    if (migratingLegacy)
                        CHECK(get(restored, id) == clouds::defaults[static_cast<size_t>(field)]);
                }
                CHECK(get(restored, feedbackID) == 0.0f);
                CHECK(restored.getModulationInfoForParameter(mainPitchID).isModulated);
                CHECK(restored.getModulationInfoForParameter(mainPitchID).depth == Catch::Approx(0.3f));
                CHECK(restored.getModulationInfoForParameter(existingCloudsFeedbackID).isModulated);
                CHECK(restored.getModulationInfoForParameter(existingCloudsFeedbackID).depth == 1.0f);
                CHECK_FALSE(restored.getModulationInfoForParameter(existingCloudsFeedbackID).isBipolar);
                CHECK(restored.isCurrentStateEquivalentToPreset(incomingPreset));
            };
            REQUIRE(state::loadStateFromXml(incomingPreset, restored));
            checkRestored();

            auto host = hostState(source);
            host.setAttribute("cloudsSchemaVersion", 1);
            auto* parameters = host.getChildByName("PARAMETERS");
            REQUIRE(parameters != nullptr);
            auto* engine = parameters->getChildByAttribute("id", clouds::parameterID(scope, 0, clouds::engineField));
            REQUIRE(engine != nullptr);
            engine->setAttribute("value", migratingLegacy ? 0.0f : 1.0f);
            auto* alternate = host.getChildByName("AB_STATE");
            REQUIRE(alternate != nullptr);
            *alternate = incomingPreset;
            alternate->setTagName("AB_STATE");
            alternate->setAttribute("currentSideIsA", true);
            restoreHost(restored, host);
            checkRestored();
            restored.stateAB.toggleAB();
            checkRestored();
        }
}
