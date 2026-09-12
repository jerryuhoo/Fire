#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace {
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
juce::String bandId(const juce::String& base, int band = 0) { return ParameterIDAndName::getIDString(base, band); }
float processSteady(FireAudioProcessor& p)
{
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 150; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 0.001f, 256);
        p.processBlock(buffer, midi);
    }
    return buffer.getSample(0, 255);
}
void removeOttAttributes(juce::XmlElement& xml, FireAudioProcessor& p)
{
    xml.removeAttribute("ottSchemaVersion");
    for (const auto* parameter : p.getParameters())
        if (const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
            identified && ParameterIDAndName::isOttParameterID(identified->paramID))
            xml.removeAttribute(identified->paramID);
}
}

TEST_CASE("OTT parameters append to the existing automation list with new AU hints", "[ott][processor][parameters]")
{
    FireAudioProcessor p;
    const auto& parameters = p.getParameters();
    REQUIRE(parameters.size() > 28);
    const int firstOtt = parameters.indexOf(p.treeState.getParameter(bandId(OTT_ENABLED_ID)));
    REQUIRE(firstOtt > 0);
    const auto* previous = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[firstOtt - 1]);
    REQUIRE(previous != nullptr);
    CHECK(previous->paramID == bandId(LFO_PHASE_ID, 3));
    for (int index = 0; index < parameters.size(); ++index)
    {
        const auto* parameter = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[index]);
        REQUIRE(parameter != nullptr);
        CHECK(ParameterIDAndName::isOttParameterID(parameter->paramID) == (index >= firstOtt && index < firstOtt + 28));
        CHECK(parameter->getVersionHint() == (index >= firstOtt + 28 ? 3 : index >= firstOtt ? 2 : 1));
    }
    for (int band = 0; band < 4; ++band)
        CHECK(get(p, bandId(OTT_ENABLED_ID, band)) == 0.0f);
}

TEST_CASE("OTT is connected to the band signal path and supports live LFO routing", "[ott][processor][lfo]")
{
    FireAudioProcessor p;
    p.prepareToPlay(48000.0, 256);
    set(p, bandId(MODE_ID), 4); // Neutral hard-clip path for this quiet probe.
    set(p, bandId(DRIVE_BYPASS_ID), 0);
    set(p, bandId(LINKED_ID), 0);
    set(p, bandId(OUTPUT_ID), 0);
    const auto dry = processSteady(p);
    REQUIRE(dry > 0.0005f);
    set(p, bandId(OTT_ENABLED_ID), 1);
    set(p, bandId(OTT_DEPTH_ID), 1);
    const auto wet = processSteady(p);
    CHECK(wet > dry * 2.6f);
    CHECK(wet < dry * 3.0f);
    MeterValues meters;
    REQUIRE(p.getLatestMeterValues(meters));
    CHECK(meters.bandLevelsAreFresh);
    CHECK(meters.ottInputLevelDb[0] == Catch::Approx(-60.0f).margin(0.1f));
    CHECK(meters.ottGainChangeDb[0] > 8.0f);
    CHECK(meters.ottDynamicsActivityDb[0] > 8.0f);
    set(p, bandId(OTT_MIX_ID), 0);
    CHECK(processSteady(p) == Catch::Approx(dry).margin(1.0e-6f));
    set(p, bandId(OTT_MIX_ID), 1);
    set(p, bandId(OTT_DEPTH_ID), 0.5f);
    LfoData shape;
    shape.points = {{0.0f, 1.0f}, {1.0f, 1.0f}};
    p.getLfoManager().setLfoData(0, shape);
    REQUIRE(p.assignLfoToTarget(0, bandId(OTT_DEPTH_ID)) == LfoManager::AssignmentResult::changed);
    p.setModulationDepth(bandId(OTT_DEPTH_ID), 1.0f);
    CHECK(processSteady(p) == Catch::Approx(wet).epsilon(0.01));
    set(p, bandId(OTT_ENABLED_ID), 0);
    CHECK(processSteady(p) == Catch::Approx(dry).margin(1.0e-6f));
}

TEST_CASE("OTT presets and host state restore settings and migrate old snapshots with OTT disabled", "[ott][preset][state]")
{
    FireAudioProcessor source;
    set(source, bandId(DRIVE_ID), 11);
    source.stateAB.copyAB();
    set(source, bandId(DRIVE_ID), 22);
    set(source, bandId(OTT_ENABLED_ID), 1);
    set(source, bandId(OTT_DEPTH_ID), 0.83f);
    set(source, bandId(OTT_UPWARD_ID), -55);
    set(source, bandId(OTT_ENABLED_ID, 2), 1);
    set(source, bandId(OTT_DOWNWARD_ID, 2), -24);
    juce::XmlElement preset("WINGSFIRE");
    state::saveStateToXml(source, preset);
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK(get(restored, bandId(OTT_DEPTH_ID)) == Catch::Approx(0.83f));
    CHECK(get(restored, bandId(OTT_DOWNWARD_ID, 2)) == Catch::Approx(-24));

    auto truncated = preset;
    truncated.removeAttribute(bandId(OTT_DEPTH_ID));
    CHECK_FALSE(state::loadStateFromXml(truncated, restored));
    CHECK(get(restored, bandId(OTT_DEPTH_ID)) == Catch::Approx(0.83f));
    auto legacy = preset;
    removeOttAttributes(legacy, source);
    REQUIRE(state::loadStateFromXml(legacy, restored));
    CHECK(get(restored, bandId(DRIVE_ID)) == Catch::Approx(22));
    CHECK(get(restored, bandId(OTT_ENABLED_ID)) == 0.0f);
    CHECK(get(restored, bandId(OTT_DEPTH_ID)) == Catch::Approx(0.5f));

    juce::MemoryBlock state;
    source.getStateInformation(state);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(get(restored, bandId(OTT_ENABLED_ID)) == 1.0f);
    CHECK(get(restored, bandId(OTT_UPWARD_ID)) == Catch::Approx(-55));
    auto host = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(host != nullptr);
    host->removeAttribute("ottSchemaVersion");
    auto* parameters = host->getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    for (int index = parameters->getNumChildElements(); --index >= 0;)
    {
        auto* child = parameters->getChildElement(index);
        if (ParameterIDAndName::isOttParameterID(child->getStringAttribute("id")))
            parameters->removeChildElement(child, true);
    }
    host->setAttribute("savedParameterCount", parameters->getNumChildElements());
    if (auto* ab = host->getChildByName("AB_STATE")) removeOttAttributes(*ab, source);
    juce::AudioProcessor::copyXmlToBinary(*host, state);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(get(restored, bandId(OTT_ENABLED_ID)) == 0.0f);
    CHECK(get(restored, bandId(DRIVE_ID)) == Catch::Approx(22));
    restored.stateAB.toggleAB();
    CHECK(get(restored, bandId(DRIVE_ID)) == Catch::Approx(11));
    CHECK(get(restored, bandId(OTT_ENABLED_ID)) == 0.0f);
}

TEST_CASE("OTT routing retains its LFO sample offset across oversized band callbacks", "[ott][processor][oversized][lfo]")
{
    constexpr int samples = 4096;
    BandProcessor whole, split;
    whole.prepare({48000.0, 64, 2});
    split.prepare({48000.0, 64, 2});
    BandProcessingParameters parameters;
    parameters.mode = 4;
    parameters.ott.enabled = true;
    parameters.ott.sources[OttProcessor::depth] = 0;
    parameters.ott.controls[OttProcessor::depth].modulationDepth = 1.0f;
    juce::AudioBuffer<float> audio(2, samples), reference(2, samples), lfo(1, samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto input = 0.001f * std::sin(static_cast<float>(sample) * 0.09f);
        audio.setSample(0, sample, input);
        audio.setSample(1, sample, input * -0.5f);
        lfo.setSample(0, sample, 0.5f + 0.5f * std::sin(static_cast<float>(sample) * 0.031f));
    }
    reference.makeCopyOf(audio);
    whole.process(audio, parameters, lfo);
    for (int offset = 0; offset < samples; offset += 64)
    {
        juce::AudioBuffer<float> block(reference.getArrayOfWritePointers(), 2, offset, 64);
        juce::AudioBuffer<float> lfoBlock(lfo.getArrayOfWritePointers(), 1, offset, 64);
        split.process(block, parameters, lfoBlock);
    }
    float maximumError = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < samples; ++sample)
            maximumError = juce::jmax(maximumError, std::abs(audio.getSample(channel, sample) - reference.getSample(channel, sample)));
    CHECK(maximumError < 1.0e-6f);
}
