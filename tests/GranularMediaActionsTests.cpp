#include <PluginProcessor.h>
#include <Utility/ModuleSnapshotMapping.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>

namespace
{
using namespace fire::effects;
void mediaSet(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
FrozenRecordingPtr mediaRecording(int seed)
{
    auto result = std::make_shared<FrozenRecording>();
    result->head = result->validFrames = 311;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < result->validFrames; ++i)
            result->samples[static_cast<size_t>(c)][static_cast<size_t>(i)]
                = static_cast<std::int16_t>((i * (seed + 7) + c * 391) % 24000 - 12000);
    return result;
}
FireAudioProcessor::GranularAudioTarget mediaTarget(FireAudioProcessor& p, int scope = 0, int slot = 0)
{
    auto target = p.getGranularAudioTarget(scope, slot); REQUIRE(target); return *target;
}
FrozenRecordingPtr mediaExport(FireAudioProcessor& p, int scope = 0, int slot = 0)
{
    FrozenRecordingPtr result;
    REQUIRE(p.captureGranularAudioForExport(mediaTarget(p, scope, slot), result).wasOk());
    REQUIRE(result); return result;
}
void requireRecording(const FrozenRecordingPtr& actual, const FrozenRecordingPtr& expected)
{
    REQUIRE(actual); REQUIRE(expected);
    CHECK(actual->head == expected->head); CHECK(actual->validFrames == expected->validFrames);
    CHECK(actual->samples == expected->samples);
}
struct ReplaceOnFreeze final : juce::AudioProcessorParameter::Listener
{
    FireAudioProcessor& processor;
    const juce::MemoryBlock& state;
    int phase = 0, begins = 0, ends = 0;
    bool fired = false;
    ReplaceOnFreeze(FireAudioProcessor& p, const juce::MemoryBlock& s, int when)
        : processor(p), state(s), phase(when) {}
    void replace()
    {
        if (fired) return;
        fired = true;
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    }
    void parameterValueChanged(int, float) override { if (phase == 1) replace(); }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting) { ++begins; if (phase == 0) replace(); }
        else { ++ends; if (phase == 2) replace(); }
    }
};
}

TEST_CASE("Granular import is one undoable edit and can export before an audio callback",
          "[granular-media][granular-actions][state][edit-history]")
{
    FireAudioProcessor p; REQUIRE(p.addInsertEffect(0, Type::granular) == 0);
    const auto first = mediaRecording(3), second = mediaRecording(17);
    REQUIRE(p.applyImportedGranularAudio(mediaTarget(p), first).wasOk());
    CHECK(p.treeState.getRawParameterValue(fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField))->load() == 1);
    requireRecording(mediaExport(p), first);
    REQUIRE(p.applyImportedGranularAudio(mediaTarget(p), second).wasOk());
    requireRecording(mediaExport(p), second);
    REQUIRE(p.undoEdit()); requireRecording(mediaExport(p), first);
    REQUIRE(p.redoEdit()); requireRecording(mediaExport(p), second);
    juce::MemoryBlock saved; p.getStateInformation(saved);
    FireAudioProcessor restored; restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    requireRecording(mediaExport(restored), second);
    fire::chain::Snapshot module;
    REQUIRE(p.captureModuleSnapshot(0, fire::module_order::firstInsert, true, module).wasOk());
    int added = -1; REQUIRE(restored.pasteModuleSnapshot(1, -1, module, added).wasOk());
    requireRecording(mediaExport(restored, 1, added - fire::module_order::firstInsert), second);
}

TEST_CASE("Granular media rejects stale targets including raw host type ABA on every selector",
          "[granular-media][granular-actions][state]")
{
    FireAudioProcessor p; REQUIRE(p.addInsertEffect(0, Type::granular) == 0);
    const auto first = mediaRecording(3), second = mediaRecording(17);
    REQUIRE(p.applyImportedGranularAudio(mediaTarget(p), first).wasOk());
    for (const auto& id : { parameterID(0, 0, typeField),
            fire::core_modules::parameterID(0, 0, fire::core_modules::typeField),
            fire::modulation_fx::parameterID(0, 0), fire::resonator_params::parameterID(0, 0) })
    {
        const auto target = mediaTarget(p);
        const auto value = p.treeState.getRawParameterValue(id)->load();
        mediaSet(p, id, value == 0 ? 1 : 0); mediaSet(p, id, value);
        CHECK_FALSE(p.isGranularAudioTargetCurrent(target));
        CHECK(p.applyImportedGranularAudio(target, second).failed());
        FrozenRecordingPtr rejected = second;
        CHECK(p.captureGranularAudioForExport(target, rejected).failed()); CHECK_FALSE(rejected);
        requireRecording(mediaExport(p), first);
    }
    const auto old = mediaTarget(p);
    p.removeInsertEffect(0, 0); REQUIRE(p.addInsertEffect(0, Type::granular) == 0);
    CHECK(p.applyImportedGranularAudio(old, second).failed());
    CHECK_FALSE(p.getGranularAudioTarget(-1, 0)); CHECK_FALSE(p.getGranularAudioTarget(5, 0));
    CHECK_FALSE(p.getGranularAudioTarget(0, 8)); CHECK_FALSE(p.getGranularAudioTarget(0, 1));
}

TEST_CASE("Granular import rejects invalid audio and exporting never turns on Freeze",
          "[granular-media][granular-actions][state]")
{
    FireAudioProcessor p; REQUIRE(p.addInsertEffect(0, Type::granular) == 0);
    const auto target = mediaTarget(p);
    CHECK(p.applyImportedGranularAudio(target, {}).failed());
    CHECK(p.applyImportedGranularAudio(target, std::make_shared<FrozenRecording>()).failed());
    FrozenRecordingPtr out; CHECK(p.captureGranularAudioForExport(target, out).failed()); CHECK_FALSE(out);
    CHECK(p.treeState.getRawParameterValue(fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField))->load() == 0);
    REQUIRE(p.applyImportedGranularAudio(target, mediaRecording(3)).wasOk());
    REQUIRE(p.undoEdit());
    CHECK(p.getInsertEffectType(0, 0) == Type::granular);
    CHECK(p.treeState.getRawParameterValue(fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField))->load() == 0);
}

TEST_CASE("Granular imports remain independent across Master and every Band scope",
          "[granular-media][granular-actions][state][isolation]")
{
    FireAudioProcessor p;
    for (int scope = 0; scope < scopeCount; ++scope)
        REQUIRE(p.addInsertEffect(scope, Type::granular) == 0);
    for (int scope = 0; scope < scopeCount; ++scope)
        REQUIRE(p.applyImportedGranularAudio(mediaTarget(p, scope), mediaRecording(scope + 5)).wasOk());
    for (int scope = 0; scope < scopeCount; ++scope)
        requireRecording(mediaExport(p, scope), mediaRecording(scope + 5));
    const auto stale = mediaTarget(p, 4);
    p.stateAB.copyAB(false); p.stateAB.toggleAB();
    CHECK_FALSE(p.isGranularAudioTargetCurrent(stale));
    for (int scope = 0; scope < scopeCount; ++scope)
        requireRecording(mediaExport(p, scope), mediaRecording(scope + 5));
}

TEST_CASE("A host sound replacement during Freeze notification wins over an in flight import",
          "[granular-media][granular-actions][state][reentrant]")
{
    const int phase = GENERATE(0, 1, 2);
    FireAudioProcessor p; REQUIRE(p.addInsertEffect(0, Type::granular) == 0);
    const auto old = mediaRecording(3), requested = mediaRecording(17);
    REQUIRE(p.applyImportedGranularAudio(mediaTarget(p), old).wasOk());
    juce::MemoryBlock hostState; p.getStateInformation(hostState);
    mediaSet(p, fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField), 0);
    auto* freeze = p.treeState.getParameter(fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField));
    ReplaceOnFreeze listener(p, hostState, phase); freeze->addListener(&listener);
    const auto result = p.applyImportedGranularAudio(mediaTarget(p), requested);
    freeze->removeListener(&listener);
    CHECK(result.failed()); CHECK(listener.fired); CHECK(listener.begins == 1); CHECK(listener.ends == 1);
    requireRecording(mediaExport(p), old);
}

TEST_CASE("Granular importing a muted slot preserves another slot's live rolling recording",
          "[granular-media][granular-actions][frozen-audio][isolation][dsp]")
{
    FireAudioProcessor edited, reference;
    for (auto* p : {&edited, &reference})
    {
        p->hasUpdateCheckBeenPerformed = true;
        REQUIRE(p->addInsertEffect(0, Type::granular) == 0);
        REQUIRE(p->addInsertEffect(0, Type::granular) == 1);
        for (int slot = 0; slot < 2; ++slot)
        {
            mediaSet(*p, parameterID(0, slot, 5), 0);
            mediaSet(*p, fire::clouds_params::parameterID(0, slot, fire::clouds_params::feedbackField), 0);
        }
        mediaSet(*p, NUM_BANDS_ID, 1);
        p->setRateAndBufferSizeDetails(48000, 128); p->prepareToPlay(48000, 128);
    }
    juce::AudioBuffer<float> a(2, 128), b(2, 128); juce::MidiBuffer midi;
    const auto render = [&](int from, int to)
    {
        for (int block = from; block < to; ++block)
        {
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i)
                a.setSample(ch, i, .2f * std::sin((block * 128 + i) * (.031f + ch * .008f)));
            b.makeCopyOf(a); edited.processBlock(a, midi); reference.processBlock(b, midi);
        }
    };
    render(0, 450);
    REQUIRE(edited.applyImportedGranularAudio(mediaTarget(edited), mediaRecording(17)).wasOk());
    mediaSet(reference, fire::clouds_params::parameterID(0, 0, fire::clouds_params::freezeField), 1);
    render(450, 460);
    for (auto* p : {&edited, &reference})
        mediaSet(*p, fire::clouds_params::parameterID(0, 1, fire::clouds_params::freezeField), 1);
    render(460, 464);
    requireRecording(mediaExport(edited, 0, 1), mediaExport(reference, 0, 1));
}
