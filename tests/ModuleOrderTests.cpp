#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
void set(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
auto before(fire::module_order::Order order, int node, int destination)
{
    std::vector<int> nodes(order.begin(), order.end());
    nodes.erase(std::find(nodes.begin(), nodes.end(), node));
    nodes.insert(std::find(nodes.begin(), nodes.end(), destination), node);
    std::copy(nodes.begin(), nodes.end(), order.begin()); return order;
}
float difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float result = 0;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < a.getNumSamples(); ++sample)
            result = juce::jmax(result, std::abs(a.getSample(channel, sample) - b.getSample(channel, sample)));
    return result;
}
juce::AudioBuffer<float> bandRender(fire::module_order::Order order, bool hq, int blockSize, bool modulated = false)
{
    constexpr int total = 12000;
    BandProcessor processor; processor.prepare({48000, 128, 2});
    BandProcessingParameters p;
    p.moduleOrder = order; p.isHQ = hq; p.mode = 4; p.isDriveEnabled = true; p.isShapeEnabled = true;
    p.driveVal.baseValue = 24; p.driveVal.range = {0, 100}; p.isSafeModeOn = false;
    p.isCompEnabled = true; p.compRatio = 6; p.compThreshold = -30; p.compAttack = 1; p.compRelease = 40;
    p.isWidthEnabled = true; p.width = 0.9f; p.pan = 0.55f;
    p.biasVal.baseValue = 0.1f; p.recVal.baseValue = 0.15f;
    p.biasVal.range = {-1, 1}; p.recVal.range = {0, 1};
    p.inserts[0].effect = fire::effects::InsertEffect::Parameters(fire::effects::Type::lofi);
    if (modulated) {p.driveLfoSourceIndex = 0; p.biasLfoSourceIndex = 1; p.recLfoSourceIndex = 2;}
    juce::AudioBuffer<float> result(2, total);
    for (int offset = 0; offset < total; offset += blockSize)
    {
        const int count = juce::jmin(blockSize, total - offset);
        juce::AudioBuffer<float> buffer(2, count), lfo(4, count);
        for (int i = 0; i < count; ++i)
        {
            const auto phase = static_cast<float>(offset + i) * 0.08f;
            buffer.setSample(0, i, 0.7f * std::sin(phase));
            buffer.setSample(1, i, 0.2f * std::cos(phase * 0.7f));
            for (int c = 0; c < 4; ++c) lfo.setSample(c, i, 0.5f + 0.5f * std::sin(phase * (0.013f + c * 0.004f)));
        }
        processor.process(buffer, p, lfo);
        for (int c = 0; c < 2; ++c) result.copyFrom(c, offset, buffer, c, 0, count);
    }
    return result;
}
juce::AudioBuffer<float> masterRender(bool filterFirst)
{
    FireAudioProcessor p;
    set(p, "multibandEnable1", 0); set(p, DOWNSAMPLE_BYPASS_ID, 1);
    set(p, DOWNSAMPLE_ID, 8); set(p, BIT_DEPTH_ID, 5); set(p, JITTER_ID, 0); set(p, DOWNSAMPLE_MIX_ID, 1);
    set(p, FILTER_BYPASS_ID, 1); set(p, LOWCUT_BYPASSED_ID, 1); set(p, HIGHCUT_BYPASSED_ID, 1);
    set(p, PEAK_BYPASSED_ID, 0); set(p, PEAK_FREQ_ID, 1500); set(p, PEAK_GAIN_ID, 18); set(p, PEAK_Q_ID, 2);
    if (filterFirst) p.moveModuleBefore(0, 0, 1);
    p.setRateAndBufferSizeDetails(48000, 128); p.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> output(2, 12032), buffer(2, 128); juce::MidiBuffer midi;
    for (int offset = 0; offset < output.getNumSamples(); offset += 128)
    {
        for (int i = 0; i < 128; ++i)
            for (int c = 0; c < 2; ++c) buffer.setSample(c, i, 0.25f * std::sin((offset + i) * 0.195f + c * 0.3f));
        p.processBlock(buffer, midi);
        for (int c = 0; c < 2; ++c) output.copyFrom(c, offset, buffer, c, 0, 128);
    }
    return output;
}
void stripOrder(juce::XmlElement& xml)
{
    xml.removeAttribute("moduleOrderSchemaVersion");
    for (int scope = 0; scope < 5; ++scope)
        for (int node = 0; node < fire::module_order::capacity; ++node)
            if (fire::module_order::valid(scope, node)) xml.removeAttribute(fire::module_order::parameterID(scope, node));
}
}

TEST_CASE("Builtin and insert modules move in one persistent chain without rebinding controls", "[module-order][state]")
{
    FireAudioProcessor p;
    CHECK(p.getModuleOrder(0) == fire::module_order::masterDefault);
    CHECK(p.getModuleOrder(1) == fire::module_order::bandDefault);
    auto* drive = p.treeState.getParameter("drive1");
    set(p, "drive1", 37);
    REQUIRE(p.assignLfoToTarget(0, "drive1") == LfoManager::AssignmentResult::changed);
    p.addInsertEffect(1, fire::effects::Type::delay);
    p.moveModuleBefore(1, 2, 0); // Compressor before Drive.
    p.moveModuleBefore(1, 5, 1); // Insert between Drive and Shape.
    const auto order = p.getModuleOrder(1);
    CHECK(order[0] == 2); CHECK(order[1] == 0); CHECK(order[2] == 5); CHECK(order[3] == 1);
    CHECK(p.treeState.getParameter("drive1") == drive);
    CHECK(drive->convertFrom0to1(drive->getValue()) == Catch::Approx(37));
    CHECK(p.getModulationInfoForParameter("drive1").isModulated);
    p.stateAB.copyAB();
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(p, preset);
    FireAudioProcessor restored; REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK(restored.getModuleOrder(1) == order);
    auto truncated = preset; truncated.removeAttribute(fire::module_order::parameterID(1, 0));
    CHECK_FALSE(state::loadStateFromXml(truncated, restored));
    auto legacy = preset; stripOrder(legacy);
    REQUIRE(state::loadStateFromXml(legacy, restored));
    CHECK(restored.getModuleOrder(1)[0] == 0);
    CHECK(restored.getModuleOrder(1)[1] == 1);
    juce::MemoryBlock state; p.getStateInformation(state);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(restored.getModuleOrder(1) == order);
    restored.moveModuleBefore(1, 3, 0); restored.stateAB.toggleAB();
    CHECK(restored.getModuleOrder(1) == order);
    auto host = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(host != nullptr);
    host->removeAttribute("moduleOrderSchemaVersion");
    auto* parameters = host->getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
    for (int i = parameters->getNumChildElements(); --i >= 0;)
        if (fire::module_order::isParameterID(parameters->getChildElement(i)->getStringAttribute("id")))
            parameters->removeChildElement(parameters->getChildElement(i), true);
    host->setAttribute("savedParameterCount", parameters->getNumChildElements());
    if (auto* ab = host->getChildByName("AB_STATE")) stripOrder(*ab);
    juce::AudioProcessor::copyXmlToBinary(*host, state);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(restored.getModuleOrder(1)[0] == 0);
    CHECK(restored.getModuleOrder(1)[1] == 1);
}

TEST_CASE("Band processing honours Compressor Stereo Shape and insert positions in base and HQ", "[module-order][dsp][hq]")
{
    for (bool hq : {false, true})
    {
        CAPTURE(hq);
        const auto normal = bandRender(fire::module_order::bandDefault, hq, 128);
        for (const auto [node, target] : {std::pair{2, 0}, std::pair{3, 0}, std::pair{1, 0}, std::pair{5, 0}})
        {
            CAPTURE(node, target);
            const auto reordered = bandRender(before(fire::module_order::bandDefault, node, target), hq, 128);
            CHECK(difference(normal, reordered) > 0.001f);
        }
    }
}

TEST_CASE("Separated Drive and Shape retain LFO timebases across custom chain chunks", "[module-order][dsp][lfo][block-size]")
{
    auto order = before(fire::module_order::bandDefault, 2, 0);
    order = before(order, 1, 0);
    order = before(order, 5, 0);
    for (bool hq : {false, true})
    {
        const auto large = bandRender(order, hq, 12000, true);
        CHECK(difference(large, bandRender(order, hq, 37, true)) < 0.00003f);
        CHECK(difference(large, bandRender(order, hq, 128, true)) < 0.00003f);
    }
}

TEST_CASE("Master Filter and Lo-Fi order changes actual audio", "[module-order][dsp][filter][lofi]")
{
    CHECK(difference(masterRender(false), masterRender(true)) > 0.01f);
}
