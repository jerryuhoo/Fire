#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/ModulationEffectParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

namespace
{
namespace fx = fire::effects;
namespace extension = fire::modulation_fx;
constexpr const char* marker = "modulationEffectsSchemaVersion";

void plain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float plainValue(FireAudioProcessor& processor, const juce::String& id)
{
    auto* value = processor.treeState.getRawParameterValue(id);
    REQUIRE(value != nullptr);
    return value->load();
}

juce::XmlElement presetSnapshot(FireAudioProcessor& processor)
{
    juce::XmlElement xml("WINGSFIRE");
    state::saveStateToXml(processor, xml);
    return xml;
}

juce::XmlElement hostSnapshot(FireAudioProcessor& processor)
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

void stripPresetExtension(juce::XmlElement& xml)
{
    xml.removeAttribute(marker);
    for (const auto& id : extension::parameterIDs()) xml.removeAttribute(id);
}

void removeHostParameter(juce::XmlElement& xml, const juce::String& id)
{
    auto* parameters = xml.getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    auto* parameter = parameters->getChildByAttribute("id", id);
    REQUIRE(parameter != nullptr);
    parameters->removeChildElement(parameter, true);
    xml.setAttribute("savedParameterCount", parameters->getNumChildElements());
}

void changeHostOutput(juce::XmlElement& xml)
{
    auto* parameters = xml.getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    auto* output = parameters->getChildByAttribute("id", OUTPUT_ID);
    REQUIRE(output != nullptr);
    output->setAttribute("value", -9.0f);
}

fx::Type sceneType(int variant, int slot)
{
    constexpr std::array first {fx::Type::flanger, fx::Type::flanger, fx::Type::phaser, fx::Type::phaser};
    constexpr std::array second {fx::Type::phaser, fx::Type::flanger, fx::Type::flanger, fx::Type::phaser};
    return (variant == 0 ? first : second)[static_cast<size_t>(slot)];
}

float sceneControl(int variant, int scope, int slot)
{
    return 0.07f * static_cast<float>(1 + variant + scope + slot);
}

void configureScene(FireAudioProcessor& processor, int variant)
{
    plain(processor, NUM_BANDS_ID, 4.0f);
    for (int divider = 0; divider < 3; ++divider)
    {
        plain(processor, ParameterIDAndName::getIDString(LINE_STATE_ID, divider), 1.0f);
        plain(processor, ParameterIDAndName::getIDString(FREQ_ID, divider),
              std::array<float, 3> {200.0f, 1400.0f, 6500.0f}[static_cast<size_t>(divider)]);
    }
    for (int scope = 0; scope < 5; ++scope)
    {
        for (int slot = 0; slot < 8; ++slot) processor.removeInsertEffect(scope, slot);
        for (int slot = 0; slot < 4; ++slot)
        {
            REQUIRE(processor.addInsertEffect(scope, sceneType(variant, slot)) == slot);
            plain(processor, fx::parameterID(scope, slot, 2), sceneControl(variant, scope, slot));
            const auto target = fx::parameterID(scope, slot, 0);
            REQUIRE(processor.assignLfoToTarget(variant, target) == LfoManager::AssignmentResult::changed);
            processor.setModulationDepth(target, variant == 0 ? -0.25f : 0.5f);
            if (slot % 2 == 0) processor.toggleBipolarMode(target);
            if (slot == 3) processor.getLfoManager().toggleBypassForRouting(target);
        }
        processor.moveInsertEffect(scope, 0, 1);
    }
}

void checkScene(FireAudioProcessor& processor, int variant)
{
    for (int scope = 0; scope < 5; ++scope)
    {
        for (int slot = 0; slot < 4; ++slot)
        {
            CAPTURE(variant, scope, slot);
            CHECK(processor.getInsertEffectType(scope, slot) == sceneType(variant, slot));
            CHECK(plainValue(processor, fx::parameterID(scope, slot, 2))
                  == Catch::Approx(sceneControl(variant, scope, slot)));
            const auto route = processor.getModulationInfoForParameter(fx::parameterID(scope, slot, 0));
            CHECK(route.isModulated);
            CHECK(route.sourceLfoIndex == variant + 1);
            CHECK(route.depth == Catch::Approx(variant == 0 ? -0.25f : 0.5f));
            CHECK(route.isBipolar == (slot % 2 != 0));
            CHECK(route.isBypassed == (slot == 3));
        }
        CHECK(processor.getInsertEffectOrder(scope, 0) > processor.getInsertEffectOrder(scope, 1));
    }
}

void neutralise(FireAudioProcessor& processor)
{
    plain(processor, HQ_ID, 0.0f);
    plain(processor, NUM_BANDS_ID, 1.0f);
    plain(processor, FILTER_BYPASS_ID, 0.0f);
    plain(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    plain(processor, MIX_ID, 1.0f);
    plain(processor, OUTPUT_ID, 0.0f);
    for (auto* id : {DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, COMP_BYPASS_ID, WIDTH_BYPASS_ID,
                    OTT_ENABLED_ID, DC_FILTER_ID, LINKED_ID, BAND_SOLO_ID})
        plain(processor, ParameterIDAndName::getIDString(id, 0), 0.0f);
    plain(processor, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0), 1.0f);
    plain(processor, ParameterIDAndName::getIDString(MODE_ID, 0), 4.0f);
    plain(processor, ParameterIDAndName::getIDString(OUTPUT_ID, 0), 0.0f);
    plain(processor, ParameterIDAndName::getIDString(MIX_ID, 0), 1.0f);
}

juce::AudioBuffer<float> render(FireAudioProcessor& processor)
{
    constexpr int blockSize = 128, totalSamples = 12288;
    constexpr double sampleRate = 48000.0;
    processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> output(2, totalSamples), block(2, blockSize);
    juce::MidiBuffer midi;
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        for (int sample = 0; sample < blockSize; ++sample)
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto time = (offset + sample) / sampleRate;
                block.setSample(channel, sample, static_cast<float>(
                    0.13 * std::sin(juce::MathConstants<double>::twoPi * 431.0 * time + channel * 0.2)
                    + 0.06 * std::sin(juce::MathConstants<double>::twoPi * 2237.0 * time)));
            }
        processor.processBlock(block, midi);
        for (int channel = 0; channel < 2; ++channel)
            output.copyFrom(channel, offset, block, channel, 0, blockSize);
    }
    bool finite = true;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < totalSamples; ++sample)
            finite = finite && std::isfinite(output.getSample(channel, sample));
    REQUIRE(finite);
    return output;
}

float settledDifference(const juce::AudioBuffer<float>& first, const juce::AudioBuffer<float>& second)
{
    float difference = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = first.getNumSamples() - 4096; sample < first.getNumSamples(); ++sample)
            difference = juce::jmax(difference, std::abs(first.getSample(channel, sample) - second.getSample(channel, sample)));
    return difference;
}
}

TEST_CASE("Modulation inserts preserve every legacy normalized type automation anchor",
          "[modulation-insert][insertfx][parameters][compatibility]")
{
    FireAudioProcessor processor;
    const juce::StringArray oldNames {"Empty", "Chorus", "Delay", "Reverb", "Granular", "Lo-Fi"};
    constexpr std::array oldTypes {fx::Type::none, fx::Type::chorus, fx::Type::delay,
                                  fx::Type::reverb, fx::Type::granular, fx::Type::lofi};
    for (int scope = 0; scope < 5; ++scope)
        for (int slot = 0; slot < 8; ++slot)
        {
            CAPTURE(scope, slot);
            auto* type = dynamic_cast<juce::AudioParameterChoice*>(processor.treeState.getParameter(fx::parameterID(scope, slot, fx::typeField)));
            REQUIRE(type != nullptr);
            CHECK(type->choices == oldNames);
            CHECK(type->getVersionHint() == 3);
            for (int choice = 0; choice < 6; ++choice)
            {
                const float normalized = static_cast<float>(choice) / 5.0f;
                type->setValueNotifyingHost(normalized);
                CHECK(type->convertFrom0to1(normalized) == Catch::Approx(choice));
                CHECK(type->convertTo0to1(static_cast<float>(choice)) == Catch::Approx(normalized));
                CHECK(processor.getInsertEffectType(scope, slot) == oldTypes[static_cast<size_t>(choice)]);
            }
            auto* extra = processor.treeState.getParameter(extension::parameterID(scope, slot));
            REQUIRE(extra != nullptr);
            CHECK(extra->getVersionHint() == 8);
            CHECK(extension::parameterID(scope, slot)
                  == juce::String(scope == 0 ? "masterFx" : "bandFx") + juce::String(slot + 1)
                         + "ModulationType" + (scope == 0 ? juce::String() : juce::String(scope)));
        }
}

TEST_CASE("Repeated Flanger and Phaser instances retain controls routes order and both AB sides",
          "[modulation-insert][insertfx][state][preset][host][ab]")
{
    FireAudioProcessor source;
    configureScene(source, 0);
    const auto first = presetSnapshot(source);
    source.stateAB.copyAB(false);
    source.stateAB.toggleAB();
    configureScene(source, 1);
    const auto second = presetSnapshot(source);
    const auto host = hostSnapshot(source);
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(first, restored));
    checkScene(restored, 0);
    CHECK(restored.isCurrentStateEquivalentToPreset(first));
    REQUIRE(state::loadStateFromXml(second, restored));
    checkScene(restored, 1);
    restoreHost(restored, host);
    CHECK_FALSE(restored.stateAB.isCurrentA());
    checkScene(restored, 1);
    restored.stateAB.toggleAB();
    CHECK(restored.stateAB.isCurrentA());
    checkScene(restored, 0);
    restored.stateAB.copyAB(false);
    restored.stateAB.toggleAB();
    checkScene(restored, 0);
}

TEST_CASE("Legacy complete racks without modulation extension retain old raw and normalized types",
          "[modulation-insert][insertfx][legacy][preset][host][ab]")
{
    FireAudioProcessor source;
    constexpr std::array types {fx::Type::chorus, fx::Type::delay, fx::Type::reverb, fx::Type::granular, fx::Type::lofi};
    for (int scope = 0; scope < 5; ++scope) REQUIRE(source.addInsertEffect(scope, types[static_cast<size_t>(scope)]) == 0);
    source.stateAB.copyAB(false);
    auto preset = presetSnapshot(source);
    stripPresetExtension(preset);
    auto host = hostSnapshot(source);
    host.removeAttribute(marker);
    for (const auto& id : extension::parameterIDs()) removeHostParameter(host, id);
    auto* ab = host.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
    stripPresetExtension(*ab);
    FireAudioProcessor restored;
    const auto checkLegacy = [&]
    {
        for (int scope = 0; scope < 5; ++scope)
        {
            CHECK(restored.getInsertEffectType(scope, 0) == types[static_cast<size_t>(scope)]);
            CHECK(plainValue(restored, extension::parameterID(scope, 0)) == 0.0f);
        }
    };
    for (int scope = 0; scope < 5; ++scope) REQUIRE(restored.addInsertEffect(scope, fx::Type::flanger) == 0);
    REQUIRE(state::loadStateFromXml(preset, restored));
    checkLegacy();
    restoreHost(restored, host);
    checkLegacy();
    restored.stateAB.toggleAB();
    checkLegacy();
}

TEST_CASE("Malformed modulation extension families reject complete state replacement",
          "[modulation-insert][insertfx][state][validation]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(0, fx::Type::flanger) == 0);
    const auto baseline = presetSnapshot(processor);
    const auto savedHost = hostSnapshot(processor);
    const auto id = extension::parameterID(4, 7);
    for (const auto* version : {"99", "-1", "1.5", "junk"})
    {
        CAPTURE(version);
        auto malformedPreset = baseline;
        malformedPreset.setAttribute(marker, version);
        CHECK_FALSE(state::loadStateFromXml(malformedPreset, processor));
        CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(malformedPreset));
        CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
        auto malformedHost = savedHost;
        malformedHost.setAttribute(marker, version);
        changeHostOutput(malformedHost);
        restoreHost(processor, malformedHost);
        CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
    }
    for (bool keepMarker : {false, true})
    {
        CAPTURE(keepMarker);
        auto partialPreset = baseline;
        partialPreset.removeAttribute(id);
        if (! keepMarker) partialPreset.removeAttribute(marker);
        CHECK_FALSE(state::loadStateFromXml(partialPreset, processor));
        CHECK_FALSE(processor.isCurrentStateEquivalentToPreset(partialPreset));
        auto partialHost = savedHost;
        removeHostParameter(partialHost, id);
        if (! keepMarker) partialHost.removeAttribute(marker);
        changeHostOutput(partialHost);
        restoreHost(processor, partialHost);
        CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
    }
    for (const auto* invalid : {"-1", "3", "1.5", "NaN"})
    {
        CAPTURE(invalid);
        auto damaged = savedHost;
        auto* parameters = damaged.getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
        auto* extensionValue = parameters->getChildByAttribute("id", id); REQUIRE(extensionValue != nullptr);
        extensionValue->setAttribute("value", invalid);
        changeHostOutput(damaged);
        restoreHost(processor, damaged);
        CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
    }
    for (const auto* invalid : {"-0.1", "1.1", "NaN"})
    {
        auto damaged = baseline;
        damaged.setAttribute(id, invalid);
        CHECK_FALSE(state::loadStateFromXml(damaged, processor));
        CHECK(processor.isCurrentStateEquivalentToPreset(baseline));
    }
}

TEST_CASE("A damaged alternate modulation family falls back to the restored live rack",
          "[modulation-insert][insertfx][state][ab][validation]")
{
    FireAudioProcessor source;
    REQUIRE(source.addInsertEffect(0, fx::Type::flanger) == 0);
    source.stateAB.copyAB(false);
    source.stateAB.toggleAB();
    source.removeInsertEffect(0, 0);
    REQUIRE(source.addInsertEffect(0, fx::Type::phaser) == 0);
    plain(source, fx::parameterID(0, 0, 2), 0.73f);
    auto host = hostSnapshot(source);
    auto* ab = host.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
    SECTION("future alternate version") { ab->setAttribute(marker, 99); }
    SECTION("missing alternate extension") { ab->removeAttribute(extension::parameterID(0, 0)); }
    FireAudioProcessor restored;
    restoreHost(restored, host);
    CHECK(restored.getInsertEffectType(0, 0) == fx::Type::phaser);
    restored.stateAB.toggleAB();
    CHECK(restored.getInsertEffectType(0, 0) == fx::Type::phaser);
    CHECK(plainValue(restored, fx::parameterID(0, 0, 2)) == Catch::Approx(0.73f));
}

TEST_CASE("Band insertion deletion and slot reuse move modulation effects with their routings",
          "[modulation-insert][insertfx][topology][modulation]")
{
    FireAudioProcessor processor;
    plain(processor, NUM_BANDS_ID, 2.0f);
    plain(processor, "lineState1", 1.0f);
    plain(processor, "freq1", 1500.0f);
    REQUIRE(processor.addInsertEffect(1, fx::Type::flanger) == 0);
    REQUIRE(processor.addInsertEffect(2, fx::Type::phaser) == 0);
    plain(processor, fx::parameterID(1, 0, 2), 0.31f);
    plain(processor, fx::parameterID(2, 0, 2), 0.79f);
    REQUIRE(processor.assignLfoToTarget(1, fx::parameterID(1, 0, 2)) == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.assignLfoToTarget(2, fx::parameterID(2, 0, 2)) == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.addMultibandBand(0, 2, true, 300.0f));
    CHECK(processor.getInsertEffectType(1, 0) == fx::Type::none);
    CHECK(processor.getInsertEffectType(2, 0) == fx::Type::flanger);
    CHECK(processor.getInsertEffectType(3, 0) == fx::Type::phaser);
    CHECK(processor.getModulationInfoForParameter(fx::parameterID(2, 0, 2)).sourceLfoIndex == 2);
    CHECK(processor.getModulationInfoForParameter(fx::parameterID(3, 0, 2)).sourceLfoIndex == 3);
    REQUIRE(processor.deleteMultibandBand(0, 3));
    CHECK(processor.getInsertEffectType(1, 0) == fx::Type::flanger);
    CHECK(processor.getInsertEffectType(2, 0) == fx::Type::phaser);
    CHECK(processor.getInsertEffectType(3, 0) == fx::Type::none);
    CHECK(plainValue(processor, fx::parameterID(1, 0, 2)) == Catch::Approx(0.31f));
    CHECK(plainValue(processor, fx::parameterID(2, 0, 2)) == Catch::Approx(0.79f));
    processor.removeInsertEffect(1, 0);
    CHECK(plainValue(processor, extension::parameterID(1, 0)) == 0.0f);
    CHECK_FALSE(processor.getModulationInfoForParameter(fx::parameterID(1, 0, 2)).isModulated);
    REQUIRE(processor.addInsertEffect(1, fx::Type::delay) == 0);
    CHECK(processor.getInsertEffectType(1, 0) == fx::Type::delay);
    CHECK(plainValue(processor, extension::parameterID(1, 0)) == 0.0f);
    processor.removeInsertEffect(1, 0);
    REQUIRE(processor.addInsertEffect(1, fx::Type::phaser) == 0);
    CHECK(processor.getInsertEffectType(1, 0) == fx::Type::phaser);
}

TEST_CASE("Master and band modulation inserts affect audio while zero mix bypass and LFO zero mix stay dry",
          "[modulation-insert][insertfx][processor][audio]")
{
    FireAudioProcessor reference;
    neutralise(reference);
    const auto dry = render(reference);
    for (int scope : {0, 1})
        for (auto type : {fx::Type::flanger, fx::Type::phaser})
        {
            CAPTURE(scope, static_cast<int>(type));
            FireAudioProcessor processor;
            neutralise(processor);
            REQUIRE(processor.addInsertEffect(scope, type) == 0);
            CHECK(std::isfinite(processor.getTailLengthSeconds()));
            CHECK(processor.getTailLengthSeconds() > 0.0);
            const auto mix = fx::parameterID(scope, 0, 5);
            plain(processor, mix, 1.0f);
            CHECK(settledDifference(render(processor), dry) > 1.0e-4f);
            CHECK(processor.getLatencySamples() == reference.getLatencySamples());
            plain(processor, mix, 0.0f);
            CHECK(settledDifference(render(processor), dry) < 2.0e-6f);
            plain(processor, mix, 1.0f);
            plain(processor, fx::parameterID(scope, 0, fx::enabledField), 0.0f);
            CHECK(settledDifference(render(processor), dry) < 2.0e-6f);
            plain(processor, fx::parameterID(scope, 0, fx::enabledField), 1.0f);
            plain(processor, mix, 0.5f);
            REQUIRE(processor.assignLfoToTarget(0, mix) == LfoManager::AssignmentResult::changed);
            processor.setModulationDepth(mix, 1.0f);
            LfoData zero;
            zero.points = {{0, 0}, {1, 0}};
            processor.getLfoManager().setLfoData(0, zero);
            CHECK(settledDifference(render(processor), dry) < 2.0e-6f);
        }
}
