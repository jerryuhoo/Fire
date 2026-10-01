#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/ResonatorParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

namespace
{
namespace fx = fire::effects;
namespace resonator = fire::resonator_params;
namespace modulation = fire::modulation_fx;
constexpr const char* marker = "resonatorSchemaVersion";

void setPlain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float getPlain(FireAudioProcessor& processor, const juce::String& id)
{
    auto* value = processor.treeState.getRawParameterValue(id);
    REQUIRE(value != nullptr);
    return value->load();
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

juce::XmlElement& hostParameter(juce::XmlElement& xml, const juce::String& id)
{
    auto* parameters = xml.getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    auto* value = parameters->getChildByAttribute("id", id);
    REQUIRE(value != nullptr);
    return *value;
}

void removeHostParameter(juce::XmlElement& xml, const juce::String& id)
{
    auto* parameters = xml.getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    parameters->removeChildElement(&hostParameter(xml, id), true);
    xml.setAttribute("savedParameterCount", parameters->getNumChildElements());
}

void stripResonatorPreset(juce::XmlElement& xml)
{
    xml.removeAttribute(marker);
    for (const auto& id : resonator::parameterIDs()) xml.removeAttribute(id);
}

float sceneControl(int variant, int scope, int slot, int control)
{
    return 0.04f * static_cast<float>(1 + variant * 2 + scope + slot * 3 + control);
}

fx::Type sceneType(int variant, int slot)
{
    return variant == 1 && slot == 1 ? fx::Type::phaser : fx::Type::chordResonator;
}

void configureScene(FireAudioProcessor& processor, int variant)
{
    setPlain(processor, NUM_BANDS_ID, 4.0f);
    for (int divider = 0; divider < 3; ++divider)
    {
        setPlain(processor, ParameterIDAndName::getIDString(LINE_STATE_ID, divider), 1.0f);
        setPlain(processor, ParameterIDAndName::getIDString(FREQ_ID, divider),
                 std::array<float, 3> {200, 1400, 6500}[static_cast<size_t>(divider)]);
    }
    for (int scope = 0; scope < 5; ++scope)
    {
        for (int slot = 0; slot < 8; ++slot) processor.removeInsertEffect(scope, slot);
        for (int slot = 0; slot < 2; ++slot)
        {
            REQUIRE(processor.addInsertEffect(scope, sceneType(variant, slot)) == slot);
            for (int control = 0; control < 6; ++control)
                setPlain(processor, fx::parameterID(scope, slot, control), sceneControl(variant, scope, slot, control));
            const auto target = fx::parameterID(scope, slot, 3);
            REQUIRE(processor.assignLfoToTarget(variant + slot, target) == LfoManager::AssignmentResult::changed);
            processor.setModulationDepth(target, slot == 0 ? 0.37f : -0.21f);
            if (slot == 1) processor.toggleBipolarMode(target);
            if (variant == 1) processor.getLfoManager().toggleBypassForRouting(target);
        }
        processor.moveInsertEffect(scope, 0, 1);
    }
}

void checkScene(FireAudioProcessor& processor, int variant)
{
    for (int scope = 0; scope < 5; ++scope)
    {
        for (int slot = 0; slot < 2; ++slot)
        {
            CAPTURE(variant, scope, slot);
            const bool isResonator = sceneType(variant, slot) == fx::Type::chordResonator;
            CHECK(processor.getInsertEffectType(scope, slot) == sceneType(variant, slot));
            CHECK(getPlain(processor, resonator::parameterID(scope, slot)) == (isResonator ? 1.0f : 0.0f));
            CHECK(getPlain(processor, fx::parameterID(scope, slot, fx::typeField)) == 0.0f);
            CHECK(getPlain(processor, modulation::parameterID(scope, slot)) == (isResonator ? 0.0f : 2.0f));
            for (int control = 0; control < 6; ++control)
                CHECK(getPlain(processor, fx::parameterID(scope, slot, control))
                      == Catch::Approx(sceneControl(variant, scope, slot, control)));
            const auto route = processor.getModulationInfoForParameter(fx::parameterID(scope, slot, 3));
            CHECK(route.isModulated);
            CHECK(route.sourceLfoIndex == variant + slot + 1);
            CHECK(route.depth == Catch::Approx(slot == 0 ? 0.37f : -0.21f));
            CHECK(route.isBipolar == (slot == 0));
            CHECK(route.isBypassed == (variant == 1));
        }
        CHECK(processor.getInsertEffectOrder(scope, 0) > processor.getInsertEffectOrder(scope, 1));
    }
}

void neutralise(FireAudioProcessor& processor)
{
    setPlain(processor, HQ_ID, 0);
    setPlain(processor, NUM_BANDS_ID, 1);
    setPlain(processor, FILTER_BYPASS_ID, 0);
    setPlain(processor, DOWNSAMPLE_BYPASS_ID, 0);
    setPlain(processor, MIX_ID, 1);
    setPlain(processor, OUTPUT_ID, 0);
    for (auto* id : {DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, COMP_BYPASS_ID, WIDTH_BYPASS_ID,
                    OTT_ENABLED_ID, DC_FILTER_ID, LINKED_ID, BAND_SOLO_ID})
        setPlain(processor, ParameterIDAndName::getIDString(id, 0), 0);
    setPlain(processor, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0), 1);
    setPlain(processor, ParameterIDAndName::getIDString(MODE_ID, 0), 4);
    setPlain(processor, ParameterIDAndName::getIDString(OUTPUT_ID, 0), 0);
    setPlain(processor, ParameterIDAndName::getIDString(MIX_ID, 0), 1);
}

juce::AudioBuffer<float> render(FireAudioProcessor& processor)
{
    constexpr int total = 12288, blockSize = 128;
    constexpr double sampleRate = 48000.0;
    processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> result(2, total), block(2, blockSize);
    juce::MidiBuffer midi;
    juce::Random random(0x43484f52);
    bool finite = true;
    for (int offset = 0; offset < total; offset += blockSize)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto phase = juce::MathConstants<double>::twoPi * (offset + sample) / sampleRate;
            const auto noise = (random.nextFloat() - 0.5f) * 0.02f;
            for (int channel = 0; channel < 2; ++channel)
                block.setSample(channel, sample, static_cast<float>(
                    0.12 * std::sin(phase * 130.81278265 + 0.2 * channel)
                    + 0.05 * std::sin(phase * 392.43834795)) + noise);
        }
        processor.processBlock(block, midi);
        for (int channel = 0; channel < 2; ++channel)
        {
            result.copyFrom(channel, offset, block, channel, 0, blockSize);
            for (int sample = 0; sample < blockSize; ++sample)
                finite = finite && std::isfinite(block.getSample(channel, sample));
        }
    }
    REQUIRE(finite);
    return result;
}

float settledDifference(const juce::AudioBuffer<float>& first, const juce::AudioBuffer<float>& second)
{
    float result = 0;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = first.getNumSamples() - 4096; sample < first.getNumSamples(); ++sample)
            result = juce::jmax(result, std::abs(first.getSample(channel, sample) - second.getSample(channel, sample)));
    return result;
}
}

TEST_CASE("Chord Resonator appends an independent boolean family and preserves previous host automation identities",
          "[chord-resonator][state][parameters][compatibility]")
{
    FireAudioProcessor processor;
    const juce::StringArray oldNames {"Empty", "Chorus", "Delay", "Reverb", "Granular", "Lo-Fi"};
    const juce::StringArray modulationNames {"Standard", "Flanger", "Phaser"};
    constexpr std::array oldTypes {fx::Type::none, fx::Type::chorus, fx::Type::delay,
                                  fx::Type::reverb, fx::Type::granular, fx::Type::lofi};
    REQUIRE(processor.getParameters().size() == 1047 + fire::core_modules::parameterCount + fire::analog_params::parameterCount + fire::reverb_params::parameterCount);
    for (int scope = 0; scope < 5; ++scope)
        for (int slot = 0; slot < 8; ++slot)
        {
            CAPTURE(scope, slot);
            auto* oldType = dynamic_cast<juce::AudioParameterChoice*>(processor.treeState.getParameter(fx::parameterID(scope, slot, fx::typeField)));
            auto* modType = dynamic_cast<juce::AudioParameterChoice*>(processor.treeState.getParameter(modulation::parameterID(scope, slot)));
            const auto id = (scope == 0 ? juce::String("masterFx") : "bandFx") + juce::String(slot + 1)
                            + "Resonator" + (scope == 0 ? juce::String() : juce::String(scope));
            auto* added = dynamic_cast<juce::AudioParameterBool*>(processor.treeState.getParameter(id));
            REQUIRE(oldType != nullptr); REQUIRE(modType != nullptr); REQUIRE(added != nullptr);
            CHECK(oldType->choices == oldNames);
            CHECK(modType->choices == modulationNames);
            CHECK(oldType->getParameterIndex() == 190 + scope * 72 + slot * 9);
            CHECK(modType->getParameterIndex() == 956 + scope * 8 + slot);
            CHECK(added->getParameterIndex() == 996 + scope * 8 + slot);
            CHECK(oldType->getVersionHint() == 3);
            CHECK(modType->getVersionHint() == 8);
            CHECK(added->getVersionHint() == 9);
            CHECK(static_cast<juce::AudioProcessorParameter*>(added)->getDefaultValue() == 0.0f);
            CHECK(resonator::parameterID(scope, slot) == id);
            CHECK(resonator::isParameterID(id));
            CHECK_FALSE(fx::isParameterID(id));
            CHECK_FALSE(modulation::isParameterID(id));
            CHECK_FALSE(fire::clouds_params::isParameterID(id));
            for (int choice = 0; choice < 6; ++choice)
            {
                const auto normalized = static_cast<float>(choice) / 5.0f;
                oldType->setValueNotifyingHost(normalized);
                CHECK(oldType->convertFrom0to1(normalized) == Catch::Approx(choice));
                CHECK(oldType->convertTo0to1(static_cast<float>(choice)) == Catch::Approx(normalized));
                CHECK(processor.getInsertEffectType(scope, slot) == oldTypes[static_cast<size_t>(choice)]);
            }
            for (int choice = 0; choice < 3; ++choice)
            {
                const auto normalized = static_cast<float>(choice) / 2.0f;
                modType->setValueNotifyingHost(normalized);
                CHECK(modType->convertFrom0to1(normalized) == Catch::Approx(choice));
                CHECK(modType->convertTo0to1(static_cast<float>(choice)) == Catch::Approx(normalized));
                CHECK(processor.getInsertEffectType(scope, slot)
                      == (choice == 0 ? fx::Type::lofi : choice == 1 ? fx::Type::flanger : fx::Type::phaser));
            }
            added->setValueNotifyingHost(1);
            CHECK(processor.getInsertEffectType(scope, slot) == fx::Type::chordResonator);
            added->setValueNotifyingHost(0);
            CHECK(processor.getInsertEffectType(scope, slot) == fx::Type::phaser);
        }
}

TEST_CASE("Repeated Chord Resonators preserve controls order and routes across presets host restore and AB",
          "[chord-resonator][state][preset][host][ab][modulation]")
{
    FireAudioProcessor source;
    configureScene(source, 0);
    const auto first = preset(source);
    source.stateAB.copyAB(false);
    source.stateAB.toggleAB();
    configureScene(source, 1);
    const auto second = preset(source);
    const auto savedHost = hostState(source);
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(first, restored)); checkScene(restored, 0);
    CHECK(restored.isCurrentStateEquivalentToPreset(first));
    REQUIRE(state::loadStateFromXml(second, restored)); checkScene(restored, 1);
    restoreHost(restored, savedHost);
    CHECK_FALSE(restored.stateAB.isCurrentA()); checkScene(restored, 1);
    restored.stateAB.toggleAB();
    CHECK(restored.stateAB.isCurrentA()); checkScene(restored, 0);
    restored.stateAB.copyAB(false);
    restored.stateAB.toggleAB(); checkScene(restored, 0);
}

TEST_CASE("A wholly absent Resonator family preserves legacy and modulation insert meanings on both AB sides",
          "[chord-resonator][state][legacy][preset][host][ab]")
{
    FireAudioProcessor source;
    constexpr std::array types {fx::Type::flanger, fx::Type::phaser, fx::Type::lofi,
                               fx::Type::delay, fx::Type::granular};
    for (int scope = 0; scope < 5; ++scope) REQUIRE(source.addInsertEffect(scope, types[static_cast<size_t>(scope)]) == 0);
    source.stateAB.copyAB(false);
    auto savedPreset = preset(source);
    stripResonatorPreset(savedPreset);
    auto savedHost = hostState(source);
    savedHost.removeAttribute(marker);
    for (const auto& id : resonator::parameterIDs()) removeHostParameter(savedHost, id);
    auto* ab = savedHost.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
    stripResonatorPreset(*ab);
    FireAudioProcessor restored;
    const auto primeResonators = [&]
    {
        for (int scope = 0; scope < 5; ++scope)
        {
            restored.removeInsertEffect(scope, 0);
            REQUIRE(restored.addInsertEffect(scope, fx::Type::chordResonator) == 0);
        }
    };
    const auto checkLegacy = [&]
    {
        for (int scope = 0; scope < 5; ++scope)
        {
            CHECK(restored.getInsertEffectType(scope, 0) == types[static_cast<size_t>(scope)]);
            CHECK(getPlain(restored, resonator::parameterID(scope, 0)) == 0.0f);
        }
    };
    primeResonators();
    REQUIRE(state::loadStateFromXml(savedPreset, restored)); checkLegacy();
    CHECK(restored.isCurrentStateEquivalentToPreset(savedPreset));
    primeResonators(); restoreHost(restored, savedHost); checkLegacy();
    restored.stateAB.toggleAB(); checkLegacy();
}

TEST_CASE("Chord Resonator family validation rejects future partial and invalid states atomically",
          "[chord-resonator][state][validation]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(0, fx::Type::chordResonator) == 0);
    const auto baseline = preset(processor);
    const auto savedHost = hostState(processor);
    const auto id = resonator::parameterID(4, 7);
    const auto rejectHost = [&](juce::XmlElement candidate)
    {
        // A void loader must be proved to reject rather than silently load
        // an otherwise identical state.
        hostParameter(candidate, OUTPUT_ID).setAttribute("value", -9.0f);
        restoreHost(processor, candidate);
        CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
    };
    for (const auto* version : {"99", "-1", "1.5", "junk"})
    {
        CAPTURE(version);
        auto candidate = baseline; candidate.setAttribute(marker, version);
        CHECK_FALSE(state::loadStateFromXml(candidate, processor));
        CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(candidate));
        auto host = savedHost; host.setAttribute(marker, version); rejectHost(host);
    }
    for (bool keepMarker : {false, true})
    {
        auto candidate = baseline; candidate.removeAttribute(id);
        if (! keepMarker) candidate.removeAttribute(marker);
        CHECK_FALSE(state::loadStateFromXml(candidate, processor));
        auto host = savedHost; removeHostParameter(host, id);
        if (! keepMarker) host.removeAttribute(marker);
        rejectHost(host);
    }
    for (const auto* value : {"-1", "2", "0.5", "NaN"})
    {
        auto host = savedHost; hostParameter(host, id).setAttribute("value", value); rejectHost(host);
    }
    for (const auto* value : {"-0.1", "1.1", "NaN"})
    {
        auto candidate = baseline; candidate.setAttribute(id, value);
        CHECK_FALSE(state::loadStateFromXml(candidate, processor));
    }
    CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
}

TEST_CASE("Resonator booleans canonicalise identically in preset loading comparison and alternate snapshots",
          "[chord-resonator][state][preset][ab][canonical]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(0, fx::Type::chordResonator) == 0);
    const auto original = preset(processor);
    const auto id = resonator::parameterID(0, 0);
    for (float normalized : {0.25f, 0.75f})
    {
        CAPTURE(normalized);
        const auto canonical = normalized < 0.5f ? 0.0f : 1.0f;
        auto candidate = original; candidate.setAttribute(id, normalized);
        REQUIRE(state::loadStateFromXml(candidate, processor));
        CHECK(getPlain(processor, id) == canonical);
        CHECK(processor.treeState.getParameter(id)->getValue() == canonical);
        CHECK(processor.isCurrentStateEquivalentToPreset(candidate));
        CHECK(preset(processor).getDoubleAttribute(id) == canonical);
        processor.stateAB.copyAB(false);
        auto host = hostState(processor);
        auto* ab = host.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
        ab->setAttribute(id, normalized);
        restoreHost(processor, host);
        processor.stateAB.toggleAB();
        CHECK(processor.treeState.getParameter(id)->getValue() == canonical);
        CHECK(processor.getInsertEffectType(0, 0) == (canonical > 0 ? fx::Type::chordResonator : fx::Type::none));
    }
}

TEST_CASE("A damaged Resonator alternate falls back to the restored main effect",
          "[chord-resonator][state][host][ab][validation]")
{
    FireAudioProcessor source;
    REQUIRE(source.addInsertEffect(0, fx::Type::chordResonator) == 0);
    source.stateAB.copyAB(false); source.stateAB.toggleAB();
    source.removeInsertEffect(0, 0);
    REQUIRE(source.addInsertEffect(0, fx::Type::phaser) == 0);
    auto host = hostState(source);
    auto* ab = host.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
    SECTION("future marker") { ab->setAttribute(marker, 99); }
    SECTION("partial family") { ab->removeAttribute(resonator::parameterID(0, 0)); }
    FireAudioProcessor restored;
    restoreHost(restored, host);
    CHECK(restored.getInsertEffectType(0, 0) == fx::Type::phaser);
    restored.stateAB.toggleAB();
    CHECK(restored.getInsertEffectType(0, 0) == fx::Type::phaser);
}

TEST_CASE("Band migration and reused slots keep Resonator state attached to their controls and routes",
          "[chord-resonator][state][topology][modulation]")
{
    FireAudioProcessor processor;
    setPlain(processor, NUM_BANDS_ID, 2); setPlain(processor, "lineState1", 1); setPlain(processor, "freq1", 1500);
    REQUIRE(processor.addInsertEffect(1, fx::Type::chordResonator) == 0);
    REQUIRE(processor.addInsertEffect(2, fx::Type::phaser) == 0);
    const auto originalTarget = fx::parameterID(1, 0, 3);
    setPlain(processor, originalTarget, 0.71f);
    REQUIRE(processor.assignLfoToTarget(1, originalTarget) == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.addMultibandBand(0, 2, true, 300));
    CHECK(processor.getInsertEffectType(1, 0) == fx::Type::none);
    CHECK(getPlain(processor, resonator::parameterID(1, 0)) == 0);
    CHECK(processor.getInsertEffectType(2, 0) == fx::Type::chordResonator);
    CHECK(processor.getInsertEffectType(3, 0) == fx::Type::phaser);
    CHECK(getPlain(processor, fx::parameterID(2, 0, 3)) == Catch::Approx(0.71f));
    CHECK_FALSE(processor.getModulationInfoForParameter(originalTarget).isModulated);
    CHECK(processor.getModulationInfoForParameter(fx::parameterID(2, 0, 3)).sourceLfoIndex == 2);
    REQUIRE(processor.deleteMultibandBand(0, 3));
    CHECK(processor.getInsertEffectType(1, 0) == fx::Type::chordResonator);
    CHECK(processor.getInsertEffectType(2, 0) == fx::Type::phaser);
    CHECK(getPlain(processor, resonator::parameterID(3, 0)) == 0);
    CHECK(processor.getModulationInfoForParameter(originalTarget).sourceLfoIndex == 2);
    for (auto nextType : {fx::Type::flanger, fx::Type::delay, fx::Type::chordResonator})
    {
        processor.removeInsertEffect(1, 0);
        CHECK(getPlain(processor, resonator::parameterID(1, 0)) == 0);
        CHECK_FALSE(processor.getModulationInfoForParameter(originalTarget).isModulated);
        REQUIRE(processor.addInsertEffect(1, nextType) == 0);
        CHECK(processor.getInsertEffectType(1, 0) == nextType);
        CHECK(getPlain(processor, resonator::parameterID(1, 0)) == (nextType == fx::Type::chordResonator ? 1.0f : 0.0f));
    }
    CHECK(getPlain(processor, fx::parameterID(1, 0, fx::typeField)) == 0);
    CHECK(getPlain(processor, modulation::parameterID(1, 0)) == 0);
}

TEST_CASE("Chord Resonator reaches master and band audio without changing zero mix bypass or latency",
          "[chord-resonator][processor][audio][bypass]")
{
    FireAudioProcessor reference;
    neutralise(reference);
    const auto dry = render(reference);
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        FireAudioProcessor processor;
        neutralise(processor);
        REQUIRE(processor.addInsertEffect(scope, fx::Type::chordResonator) == 0);
        CHECK(std::isfinite(processor.getTailLengthSeconds()));
        CHECK(processor.getTailLengthSeconds() > 0.0);
        const auto mix = fx::parameterID(scope, 0, 5);
        setPlain(processor, mix, 1);
        CHECK(settledDifference(render(processor), dry) > 1.0e-4f);
        CHECK(processor.getLatencySamples() == reference.getLatencySamples());
        setPlain(processor, mix, 0);
        CHECK(settledDifference(render(processor), dry) < 2.0e-6f);
        setPlain(processor, mix, 1);
        setPlain(processor, fx::parameterID(scope, 0, fx::enabledField), 0);
        CHECK(settledDifference(render(processor), dry) < 2.0e-6f);
    }
}
