#include <PluginProcessor.h>
#include <Utility/FrozenAudioState.h>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <thread>

namespace
{
using namespace fire::effects;
FrozenRecordingPtr material(double frequency = 220.0)
{
    auto result = std::make_shared<FrozenRecording>();
    result->validFrames = frozenRecordingFrames;
    for (size_t channel = 0; channel < result->samples.size(); ++channel)
        for (int frame = 0; frame < frozenRecordingFrames; ++frame)
            result->samples[channel][static_cast<size_t>(frame)] = static_cast<std::int16_t>(
                12000.0 * std::sin(6.283185307179586 * (frequency + 55.0 * channel) * frame / 32000.0));
    return result;
}
FrozenRecordings scene(const FrozenRecordingPtr& recording, int scope = 0)
{ FrozenRecordings result{}; result[static_cast<size_t>(scope)][0] = recording; return result; }
void plain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void granular(FireAudioProcessor& processor, int scope = 0)
{
    REQUIRE(processor.addInsertEffect(scope, Type::granular) == 0);
    plain(processor, fire::clouds_params::parameterID(scope, 0, fire::clouds_params::freezeField), 1);
    plain(processor, parameterID(scope, 0, 5), 1);
}
FrozenRecordingPtr savedMaterial(FireAudioProcessor& processor, int scope = 0)
{ return processor.captureSerializablePresetStateSnapshot().frozenAudio[static_cast<size_t>(scope)][0]; }
double renderSilence(FireAudioProcessor& processor, double rate = 48000)
{
    processor.setRateAndBufferSizeDetails(rate, 128);
    processor.prepareToPlay(rate, 128);
    juce::AudioBuffer<float> block(2, 128);
    juce::MidiBuffer midi;
    double energy = 0;
    bool finite = true;
    for (int iteration = 0; iteration < 300; ++iteration)
    {
        block.clear(); processor.processBlock(block, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < block.getNumSamples(); ++sample)
            {
                const auto value = block.getSample(channel, sample);
                finite = finite && std::isfinite(value);
                energy += value * value;
            }
    }
    CHECK(finite);
    return energy;
}
}

TEST_CASE("Frozen recording codec retains exact PCM and rejects corrupt or oversized packets", "[frozen-audio][state]")
{
    auto original = material();
    auto encoded = encodeFrozenRecording(*original);
    auto restored = decodeFrozenRecording(encoded);
    REQUIRE(restored != nullptr);
    CHECK(restored->samples == original->samples);
    CHECK(restored->head == original->head);
    CHECK_FALSE(decodeFrozenRecording(encoded.dropLastCharacters(8)));
    CHECK_FALSE(decodeFrozenRecording(juce::String::repeatedString("A", 200001)));
    encoded = encoded.replaceSection(24, 4, "AAAA");
    CHECK_FALSE(decodeFrozenRecording(encoded));
    juce::XmlElement xml("WINGSFIRE");
    writeFrozenAudioState(xml, scene(original));
    FrozenRecordings parsed;
    REQUIRE(readFrozenAudioState(xml, parsed));
    CHECK(parsed[0][0]->samples == original->samples);
    auto* parent = xml.getChildByName("FROZEN_AUDIO");
    REQUIRE(parent != nullptr);
    parent->addChildElement(new juce::XmlElement(*parent->getFirstChildElement()));
    CHECK_FALSE(readFrozenAudioState(xml, parsed));
}

TEST_CASE("Frozen mailbox reads are coherent and wait for the accepted state publication", "[frozen-audio][threading]")
{
    auto mailbox = std::make_unique<AtomicFrozenRecording>();
    auto saved = material();
    mailbox->publish(saved.get(), 4);
    auto copy = std::make_unique<FrozenRecording>();
    std::uint64_t version = 0;
    CHECK_FALSE(mailbox->copy(*copy, version, 2, true));
    REQUIRE(mailbox->copy(*copy, version, 4, true));
    CHECK(copy->samples == saved->samples);
    std::atomic<bool> finished{false};
    std::thread writer([&]
    {
        for (int tag = 1; tag <= 100; ++tag)
            mailbox->publish(frozenRecordingFrames, tag, 0,
                [tag](size_t, int) { return static_cast<std::int16_t>(tag); });
        finished.store(true);
    });
    bool coherent = true;
    int reads = 0;
    while (!finished.load())
        if (mailbox->copy(*copy, version) && copy->head > 0)
        {
            ++reads;
            for (const auto& channel : copy->samples)
                for (auto sample : channel) coherent = coherent && sample == copy->head;
        }
    writer.join();
    REQUIRE(mailbox->copy(*copy, version));
    CHECK(copy->head == 100);
    CHECK(coherent);
    juce::ignoreUnused(reads);
}

TEST_CASE("Clouds captured material can replay silence after restoring at another host sample rate", "[frozen-audio][clouds][dsp]")
{
    CloudsEngine source; source.prepare(48000);
    CloudsParameters parameters;
    for (int frame = 0; frame < 48000; ++frame)
    {
        float left = 0.25f * static_cast<float>(std::sin(6.283185307179586 * 220 * frame / 48000));
        float right = left; source.process(left, right, parameters);
    }
    parameters.freeze = true;
    for (int frame = 0; frame < 512; ++frame) { float left = 0, right = 0; source.process(left, right, parameters); }
    auto recording = source.copyFrozenRecording();
    REQUIRE(recording != nullptr);
    CloudsEngine restored; restored.stageFrozenRecording(recording); restored.prepare(96000);
    double energy = 0;
    for (int frame = 0; frame < 96000; ++frame)
    {
        float left = 0, right = 0; restored.process(left, right, parameters);
        energy += left * left + right * right;
    }
    CHECK(energy > 0.01);
    REQUIRE(restored.copyFrozenRecording() != nullptr);
    CHECK(restored.copyFrozenRecording()->samples == recording->samples);
    restored.prepare(44100);
    for (int frame = 0; frame < 512; ++frame) { float left = 0, right = 0; restored.process(left, right, parameters); }
    REQUIRE(restored.copyFrozenRecording() != nullptr);
    CHECK(restored.copyFrozenRecording()->samples == recording->samples);
}

TEST_CASE("Projects presets AB and undo retain distinct frozen materials", "[frozen-audio][state][edit-history]")
{
    FireAudioProcessor processor;
    granular(processor);
    auto first = material(110), second = material(550);
    processor.restoreFrozenAudio(scene(first));
    processor.stateAB.copyAB();
    processor.checkpointEditHistory();
    processor.restoreFrozenAudio(scene(second));
    processor.checkpointEditHistory();
    REQUIRE(processor.undoEdit());
    REQUIRE(savedMaterial(processor) != nullptr);
    CHECK(savedMaterial(processor)->samples == first->samples);
    REQUIRE(processor.redoEdit());
    CHECK(savedMaterial(processor)->samples == second->samples);
    processor.stateAB.toggleAB();
    CHECK(savedMaterial(processor)->samples == first->samples);
    processor.stateAB.toggleAB();
    CHECK(savedMaterial(processor)->samples == second->samples);
    juce::MemoryBlock host;
    processor.getStateInformation(host);
    FireAudioProcessor restored;
    restored.setStateInformation(host.getData(), static_cast<int>(host.getSize()));
    REQUIRE(savedMaterial(restored) != nullptr);
    CHECK(savedMaterial(restored)->samples == second->samples);
    restored.stateAB.toggleAB();
    CHECK(savedMaterial(restored)->samples == first->samples);
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(processor, preset);
    REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK(restored.isCurrentStateEquivalentToPreset(preset));
    CHECK(renderSilence(restored, 96000) > 0.01);
    processor.removeInsertEffect(0, 0);
    REQUIRE(processor.undoEdit());
    REQUIRE(savedMaterial(processor) != nullptr);
    CHECK(savedMaterial(processor)->samples == second->samples);
    processor.removeInsertEffect(0, 0);
    REQUIRE(processor.addInsertEffect(0, Type::granular) == 0);
    plain(processor, fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField), 1);
    CHECK_FALSE(savedMaterial(processor));
}

TEST_CASE("Invalid frozen state cannot partly load and legacy states discard stale material", "[frozen-audio][state][legacy]")
{
    FireAudioProcessor processor;
    granular(processor);
    auto recording = material(); processor.restoreFrozenAudio(scene(recording));
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(processor, preset);
    preset.getChildByName("FROZEN_AUDIO")->getFirstChildElement()->setAttribute("data", "broken");
    CHECK_FALSE(state::loadStateFromXml(preset, processor));
    REQUIRE(savedMaterial(processor) != nullptr);
    CHECK(savedMaterial(processor)->samples == recording->samples);
    preset.removeChildElement(preset.getChildByName("FROZEN_AUDIO"), true);
    REQUIRE(state::loadStateFromXml(preset, processor));
    CHECK_FALSE(savedMaterial(processor));
}

TEST_CASE("Band removal moves frozen material with its logical band", "[frozen-audio][multiband][state]")
{
    FireAudioProcessor processor;
    plain(processor, NUM_BANDS_ID, 2);
    plain(processor, "lineState1", 1);
    granular(processor, 2);
    auto recording = material(); processor.restoreFrozenAudio(scene(recording, 2));
    REQUIRE(processor.deleteMultibandBand(0, 2));
    REQUIRE(savedMaterial(processor, 1) != nullptr);
    CHECK(savedMaterial(processor, 1)->samples == recording->samples);
    REQUIRE(processor.undoEdit());
    REQUIRE(savedMaterial(processor, 2) != nullptr);
    CHECK(savedMaterial(processor, 2)->samples == recording->samples);
}

TEST_CASE("A full forty-slot frozen preset fits the file limit and survives a disk scan", "[frozen-audio][preset][filesystem]")
{
    FireAudioProcessor processor;
    for (int scope = 0; scope < fire::effects::scopeCount; ++scope)
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        {
            plain(processor, parameterID(scope, slot, typeField), 4);
            plain(processor, fire::clouds_params::parameterID(scope, slot, fire::clouds_params::freezeField), 1);
        }
    auto noise = std::make_shared<FrozenRecording>(); noise->validFrames = frozenRecordingFrames;
    juce::Random random(0x46525a);
    for (auto& channel : noise->samples)
        for (auto& sample : channel) sample = static_cast<std::int16_t>(random.nextInt());
    FrozenRecordings recordings;
    for (auto& scope : recordings) for (auto& slot : scope) slot = noise;
    processor.restoreFrozenAudio(recordings);
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(processor, preset);
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile("TestTemp")
        .getChildFile("FrozenPreset-" + juce::Uuid().toString());
    REQUIRE(folder.createDirectory().wasOk());
    const juce::ScopeGuard cleanup{[&] { folder.deleteRecursively(); }};
    const auto file = folder.getChildFile("Forty frozen slots.fire");
    REQUIRE(preset.writeTo(file));
    CHECK(file.getSize() > 4 * 1024 * 1024);
    CHECK(file.getSize() < 12 * 1024 * 1024);
    state::StatePresets library(processor, folder.getFullPathName());
    CHECK(library.getNumPresets() == 1);
    FrozenRecordings decoded;
    auto document = juce::XmlDocument::parse(file);
    REQUIRE(document != nullptr);
    REQUIRE(readFrozenAudioState(*document, decoded));
    CHECK(decoded[4][7]->samples == noise->samples);
}
