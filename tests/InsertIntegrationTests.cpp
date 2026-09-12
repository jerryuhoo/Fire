#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace fire::effects;
namespace
{
void set(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(FireAudioProcessor& p, const juce::String& id)
{
    auto* parameter = p.treeState.getRawParameterValue(id);
    REQUIRE(parameter != nullptr);
    return parameter->load();
}
void stripInserts(juce::XmlElement& xml)
{
    xml.removeAttribute("insertEffectsSchemaVersion");
    for (const auto& id : parameterIDs()) xml.removeAttribute(id);
}
float render(FireAudioProcessor& p)
{
    p.setRateAndBufferSizeDetails(48000, 256);
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    float last = 0;
    for (int block = 0; block < 160; ++block)
    {
        for (int sample = 0; sample < 256; ++sample)
            for (int channel = 0; channel < 2; ++channel)
                buffer.setSample(channel, sample, 0.1f * std::sin(static_cast<float>(block * 256 + sample) * 0.057f));
        p.processBlock(buffer, midi);
        REQUIRE(std::isfinite(buffer.getMagnitude(0, 256)));
        last = buffer.getSample(0, 255);
    }
    return last;
}
}

TEST_CASE("Insert slots retain automation identities while adding moving and removing effects", "[insertfx][parameters][state]")
{
    FireAudioProcessor p;
    REQUIRE(p.getParameters().size() > parameterCount);
    for (const auto& id : parameterIDs())
    {
        auto* parameter = p.treeState.getParameter(id);
        REQUIRE(parameter != nullptr);
        CHECK(parameter->getVersionHint() == 3);
    }
    const auto first = p.addInsertEffect(0, Type::delay);
    const auto second = p.addInsertEffect(0, Type::delay);
    REQUIRE(first == 0); REQUIRE(second == 1);
    CHECK(p.getInsertEffectOrder(0, first) < p.getInsertEffectOrder(0, second));
    set(p, parameterID(0, first, 0), 0.123f);
    REQUIRE(p.assignLfoToTarget(0, parameterID(0, first, 0)) == LfoManager::AssignmentResult::changed);
    p.moveInsertEffect(0, first, 1);
    CHECK(p.getInsertEffectOrder(0, first) > p.getInsertEffectOrder(0, second));
    CHECK(get(p, parameterID(0, first, 0)) == Catch::Approx(0.123f));
    CHECK(p.getModulationInfoForParameter(parameterID(0, first, 0)).isModulated);
    p.removeInsertEffect(0, first);
    CHECK_FALSE(p.getModulationInfoForParameter(parameterID(0, first, 0)).isModulated);
    CHECK(p.getInsertEffectType(0, first) == Type::none);
    REQUIRE(p.addInsertEffect(0, Type::reverb) == first);
    CHECK(p.getInsertEffectOrder(0, first) > p.getInsertEffectOrder(0, second));
    for (int i = 2; i < slotCount; ++i) REQUIRE(p.addInsertEffect(0, Type::chorus) == i);
    CHECK(p.addInsertEffect(0, Type::chorus) == -1);
    CHECK(p.addInsertEffect(5, Type::chorus) == -1);
}

TEST_CASE("Insert chains and tape controls round trip and old presets migrate to an empty rack", "[insertfx][preset][state]")
{
    FireAudioProcessor source;
    REQUIRE(source.addInsertEffect(0, Type::reverb) == 0);
    REQUIRE(source.addInsertEffect(2, Type::granular) == 0);
    set(source, tapeIDs[0], 0.7f); set(source, tapeIDs[2], 0.3f);
    set(source, parameterID(2, 0, 2), 0.8f);
    source.stateAB.copyAB();
    juce::XmlElement preset("WINGSFIRE");
    state::saveStateToXml(source, preset);
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK(restored.getInsertEffectType(0, 0) == Type::reverb);
    CHECK(restored.getInsertEffectType(2, 0) == Type::granular);
    CHECK(get(restored, parameterID(2, 0, 2)) == Catch::Approx(0.8f));
    CHECK(get(restored, tapeIDs[0]) == Catch::Approx(0.7f));
    auto incomplete = preset;
    incomplete.removeAttribute(parameterID(0, 0, 0));
    CHECK_FALSE(state::loadStateFromXml(incomplete, restored));
    CHECK(restored.getInsertEffectType(0, 0) == Type::reverb);
    auto legacy = preset; stripInserts(legacy);
    REQUIRE(state::loadStateFromXml(legacy, restored));
    CHECK(restored.getInsertEffectType(0, 0) == Type::none);
    CHECK(get(restored, tapeIDs[0]) == 0.0f);
    juce::MemoryBlock chunk;
    source.getStateInformation(chunk);
    restored.setStateInformation(chunk.getData(), static_cast<int>(chunk.getSize()));
    CHECK(restored.getInsertEffectType(2, 0) == Type::granular);
    restored.removeInsertEffect(0, 0);
    restored.stateAB.toggleAB();
    CHECK(restored.getInsertEffectType(0, 0) == Type::reverb);
    auto host = juce::AudioProcessor::getXmlFromBinary(chunk.getData(), static_cast<int>(chunk.getSize()));
    REQUIRE(host != nullptr);
    auto* parameters = host->getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    auto truncatedHost = *host;
    auto* truncatedParameters = truncatedHost.getChildByName("PARAMETERS");
    for (auto* child : truncatedParameters->getChildIterator())
        if (child->getStringAttribute("id") == parameterID(0, 0, 0))
        { truncatedParameters->removeChildElement(child, true); break; }
    truncatedHost.setAttribute("savedParameterCount", truncatedParameters->getNumChildElements());
    juce::MemoryBlock invalid;
    juce::AudioProcessor::copyXmlToBinary(truncatedHost, invalid);
    restored.removeInsertEffect(0, 0);
    restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
    CHECK(restored.getInsertEffectType(0, 0) == Type::none);
    host->removeAttribute("insertEffectsSchemaVersion");
    for (int i = parameters->getNumChildElements(); --i >= 0;)
        if (isParameterID(parameters->getChildElement(i)->getStringAttribute("id")))
            parameters->removeChildElement(parameters->getChildElement(i), true);
    host->setAttribute("savedParameterCount", parameters->getNumChildElements());
    if (auto* ab = host->getChildByName("AB_STATE")) stripInserts(*ab);
    juce::AudioProcessor::copyXmlToBinary(*host, chunk);
    restored.setStateInformation(chunk.getData(), static_cast<int>(chunk.getSize()));
    CHECK(restored.getInsertEffectType(2, 0) == Type::none);
    CHECK(get(restored, tapeIDs[0]) == 0.0f);
    restored.stateAB.toggleAB();
    CHECK(restored.getInsertEffectType(0, 0) == Type::none);
}

TEST_CASE("Master and band insert effects are wired to audio and respect zero mix", "[insertfx][processor][audio]")
{
    for (int scope : {0, 1})
    {
        FireAudioProcessor p;
        set(p, "driveBypass1", 0); set(p, "mode1", 4); set(p, "linked1", 0);
        const auto dry = render(p);
        REQUIRE(p.addInsertEffect(scope, Type::chorus) == 0);
        const auto wet = render(p);
        CHECK(std::abs(wet - dry) > 0.001f);
        set(p, parameterID(scope, 0, 5), 0);
        CHECK(render(p) == Catch::Approx(dry).margin(0.000001f));
        set(p, parameterID(scope, 0, 5), 0.5f);
        REQUIRE(p.assignLfoToTarget(0, parameterID(scope, 0, 5)) == LfoManager::AssignmentResult::changed);
        p.setModulationDepth(parameterID(scope, 0, 5), 1);
        LfoData shape; shape.points = {{0, 0}, {1, 0}};
        p.getLfoManager().setLfoData(0, shape);
        CHECK(render(p) == Catch::Approx(dry).margin(0.000001f));
    }
}

TEST_CASE("Master Lo-Fi tape and flutter extend the existing wet path", "[insertfx][processor][tape]")
{
    FireAudioProcessor p;
    set(p, "driveBypass1", 0); set(p, "mode1", 4); set(p, "linked1", 0);
    set(p, DOWNSAMPLE_BYPASS_ID, 1); set(p, DOWNSAMPLE_ID, 1); set(p, BIT_DEPTH_ID, 32);
    set(p, JITTER_ID, 0); set(p, DOWNSAMPLE_MIX_ID, 1);
    const auto dry = render(p);
    set(p, tapeIDs[0], 0.8f); set(p, tapeIDs[2], 0.6f);
    CHECK(std::abs(render(p) - dry) > 0.001f);
    set(p, DOWNSAMPLE_MIX_ID, 0);
    CHECK(render(p) == Catch::Approx(dry).margin(0.000001f));
}
