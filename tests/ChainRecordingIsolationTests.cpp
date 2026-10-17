#include <PluginProcessor.h>
#include <Utility/ModuleSnapshotMapping.h>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
void recordingSet(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void recordingInput(juce::AudioBuffer<float>& block, int blockIndex)
{
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto frame = blockIndex * block.getNumSamples() + sample;
            block.setSample(channel, sample, .19f * std::sin(frame * (.031f + channel * .009f))
                + .07f * std::cos(frame * .113f + channel));
        }
}
}

TEST_CASE("Pasting a disabled Band Compressor leaves another scope's live Granular recording intact",
          "[chain][operations][frozen-audio][isolation][dsp]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor edited, reference;
    for (auto* processor : {&edited, &reference})
    {
        processor->hasUpdateCheckBeenPerformed = true;
        REQUIRE(processor->addInsertEffect(0, fire::effects::Type::granular) == 0);
        recordingSet(*processor, NUM_BANDS_ID, 1);
        recordingSet(*processor, fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField), 0);
        recordingSet(*processor, fire::clouds_params::parameterID(0, 0, fire::clouds_params::feedbackField), 0);
        processor->setRateAndBufferSizeDetails(48000, 128);
        processor->prepareToPlay(48000, 128);
    }
    juce::AudioBuffer<float> a(2, 128), b(2, 128); juce::MidiBuffer midi;
    // 1.2 seconds fills the one-second 32 kHz recording, including SRC and
    // topology startup. Neither processor has requested Freeze yet.
    for (int block = 0; block < 450; ++block)
    {
        recordingInput(a, block); b.makeCopyOf(a);
        edited.processBlock(a, midi); reference.processBlock(b, midi);
    }
    REQUIRE(edited.captureSerializablePresetStateSnapshot().frozenAudio[0][0] == nullptr);
    REQUIRE(reference.captureSerializablePresetStateSnapshot().frozenAudio[0][0] == nullptr);
    fire::chain::Snapshot compressor;
    compressor.modules.push_back(fire::chain::defaultModule(fire::effects::Type::compressor));
    compressor.modules.front().enabled = false;
    int added = -1;
    REQUIRE(edited.pasteModuleSnapshot(1, 0, compressor, added).wasOk());
    REQUIRE(added >= fire::module_order::firstInsert);
    // Match the existing topology fade/reset in the reference. Only the edited
    // processor stages a module, so a difference detects a cross-scope media
    // restage rather than the expected multiband topology transition.
    reference.beginMultibandTopologyEdit(); reference.requestMultibandTopologyReset();
    for (auto* processor : {&edited, &reference})
        recordingSet(*processor, fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField), 1);
    for (int block = 450; block < 454; ++block)
    {
        recordingInput(a, block); b.makeCopyOf(a);
        edited.processBlock(a, midi); reference.processBlock(b, midi);
    }
    const auto copied = edited.captureSerializablePresetStateSnapshot().frozenAudio[0][0];
    const auto expected = reference.captureSerializablePresetStateSnapshot().frozenAudio[0][0];
    REQUIRE(expected != nullptr); REQUIRE(copied != nullptr);
    CHECK(copied->isValid()); CHECK(copied->validFrames == expected->validFrames);
    CHECK(copied->head == expected->head); CHECK(copied->samples == expected->samples);
    bool hasRecordedSignal = false;
    for (const auto& channel : copied->samples)
        for (const auto sample : channel) hasRecordedSignal |= sample != 0;
    CHECK(hasRecordedSignal);
}
