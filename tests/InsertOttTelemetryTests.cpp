#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
namespace fx = fire::effects;
constexpr int blockSize = 128;
void fillOtt(juce::AudioBuffer<float>& audio, float value = .5f)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(audio.getWritePointer(channel), value, audio.getNumSamples());
}
void setOtt(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
int setupOtt(FireAudioProcessor& processor, int scope)
{
    processor.hasUpdateCheckBeenPerformed = true;
    REQUIRE(processor.pasteChainSnapshot(0, {}).wasOk());
    REQUIRE(processor.pasteChainSnapshot(1, {}).wasOk());
    setOtt(processor, NUM_BANDS_ID, 1);
    setOtt(processor, HQ_ID, 0);
    // The legacy Serial Shape stage retains its historical curve when off.
    // Choose its identity region so it cannot colour this meter fixture.
    setOtt(processor, ParameterIDAndName::getIDString(MODE_ID, 0), 4);
    const int slot = processor.addInsertEffect(scope, fx::Type::ott); REQUIRE(slot >= 0);
    setOtt(processor, fx::parameterID(scope, slot, 0), 1);
    processor.setRateAndBufferSizeDetails(48000, blockSize);
    processor.prepareToPlay(48000, blockSize);
    return slot;
}
void renderOtt(FireAudioProcessor& processor, int blocks = 64)
{
    juce::AudioBuffer<float> audio(2, blockSize); juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block)
    {
        fillOtt(audio);
        processor.processBlock(audio, midi);
    }
}
}

TEST_CASE("Insert OTT telemetry includes the module mix and excludes trim from activity",
          "[insert-ott-telemetry][ott][dsp][telemetry]")
{
    fx::InsertEffect effect; effect.prepare({48000, blockSize, 2});
    fx::InsertEffect::Parameters p(fx::Type::ott);
    p.typeGeneration = 42;
    p.values[0].baseValue = 100;
    p.values[4].baseValue = 24;
    p.values[5].baseValue = 37;
    juce::AudioBuffer<float> audio(2, blockSize);
    for (int i = 0; i < 128; ++i) { fillOtt(audio); effect.process(juce::dsp::AudioBlock<float>(audio), p); }
    const auto meter = effect.getOttVisualState();
    REQUIRE(meter.valid); CHECK(meter.typeGeneration == 42);
    CHECK(meter.inputLevelDb == Catch::Approx(juce::Decibels::gainToDecibels(.5f)).margin(.001f));
    CHECK(meter.gainChangeDb == Catch::Approx(juce::Decibels::gainToDecibels(audio.getSample(0, blockSize - 1) / .5f)).margin(.00003f));
    CHECK(meter.gainChangeDb > 0);
    CHECK(meter.dynamicsActivityDb == Catch::Approx(
        OttProcessor::gainForLevel(meter.inputLevelDb, -48, -18, 1) * .37f).margin(.0001f));
    CHECK_FALSE(meter.mixModulated);
    p.values[5].baseValue = 0;
    for (int i = 0; i < 16; ++i) { fillOtt(audio); effect.process(juce::dsp::AudioBlock<float>(audio), p); }
    CHECK(effect.getOttVisualState().gainChangeDb == 0);
    CHECK(effect.getOttVisualState().dynamicsActivityDb == 0);
    p.enabled = false; fillOtt(audio); effect.process(juce::dsp::AudioBlock<float>(audio), p);
    CHECK_FALSE(effect.getOttVisualState().valid);
    effect.reset(); CHECK_FALSE(effect.getOttVisualState().valid);
}

TEST_CASE("Insert OTT telemetry follows final modulated wet sample and retires on type change",
          "[insert-ott-telemetry][ott][dsp][modulation][telemetry]")
{
    fx::InsertEffect effect; effect.prepare({48000, blockSize, 2});
    fx::InsertEffect::Parameters p(fx::Type::ott);
    p.values[0].baseValue = 100;
    p.values[5].baseValue = 0;
    p.values[5].modulationDepth = 1;
    p.values[5].isBipolar = false;
    std::array<float, blockSize> lfo;
    p.values[5].lfoSignal = lfo.data();
    juce::AudioBuffer<float> audio(2, blockSize);
    for (int block = 0; block < 48; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
            lfo[static_cast<size_t>(sample)] = float((sample + block * 11) % blockSize) / float(blockSize - 1);
        fillOtt(audio); effect.process(juce::dsp::AudioBlock<float>(audio), p);
        const auto meter = effect.getOttVisualState();
        REQUIRE(meter.valid); CHECK(meter.mixModulated);
        CHECK(meter.gainChangeDb == Catch::Approx(juce::Decibels::gainToDecibels(audio.getSample(0, blockSize - 1) / .5f)).margin(.00003f));
    }
    lfo.fill(0); fillOtt(audio); effect.process(juce::dsp::AudioBlock<float>(audio), p);
    CHECK(effect.getOttVisualState().gainChangeDb == 0);
    CHECK(effect.getOttVisualState().dynamicsActivityDb == 0);
    p.type = fx::Type::stereo;
    effect.process(juce::dsp::AudioBlock<float>(audio), p);
    CHECK_FALSE(effect.getOttVisualState().valid);
}

TEST_CASE("Master and Band insert OTT publish independent telemetry and invalidate stopped host edits",
          "[insert-ott-telemetry][ott][processor][telemetry]")
{
    for (const int scope : {0, 1})
    {
        CAPTURE(scope);
        FireAudioProcessor processor; const int slot = setupOtt(processor, scope);
        CHECK_FALSE(processor.getInsertOttVisualState(scope, slot).valid);
        renderOtt(processor);
        auto state = processor.getInsertOttVisualState(scope, slot);
        REQUIRE(state.valid); CHECK(state.renderSequence > 0);
        CHECK(state.inputLevelDb > -12); CHECK(state.dynamicsActivityDb < -5);
        CHECK_FALSE(processor.getInsertOttVisualState(1 - scope, slot).valid);
        CHECK_FALSE(processor.getInsertOttVisualState(scope, slot + 1).valid);
        const auto sequence = state.renderSequence;
        renderOtt(processor, 1);
        CHECK(processor.getInsertOttVisualState(scope, slot).renderSequence > sequence);
        setOtt(processor, fx::parameterID(scope, slot, 5), 0);
        CHECK(processor.getInsertOttVisualState(scope, slot).dynamicsActivityDb == 0);
        setOtt(processor, fx::parameterID(scope, slot, fx::enabledField), 0);
        CHECK_FALSE(processor.getInsertOttVisualState(scope, slot).valid);
        setOtt(processor, fx::parameterID(scope, slot, fx::enabledField), 1);
        const auto typeID = fire::core_modules::parameterID(scope, slot, fire::core_modules::typeField);
        setOtt(processor, typeID, float(fire::core_modules::encodeType(fx::Type::stereo)));
        CHECK_FALSE(processor.getInsertOttVisualState(scope, slot).valid);
        setOtt(processor, typeID, float(fire::core_modules::encodeType(fx::Type::ott)));
        // Type away/back without an intervening callback cannot revive an old meter.
        CHECK_FALSE(processor.getInsertOttVisualState(scope, slot).valid);
        renderOtt(processor);
        REQUIRE(processor.getInsertOttVisualState(scope, slot).valid);
        if (scope == 1)
        {
            setOtt(processor, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0), 0);
            CHECK_FALSE(processor.getInsertOttVisualState(scope, slot).valid);
        }
        processor.removeInsertEffect(scope, slot);
        CHECK_FALSE(processor.getInsertOttVisualState(scope, slot).valid);
    }
}

TEST_CASE("Inactive or invalid insert locations cannot expose an OTT meter",
          "[insert-ott-telemetry][ott][processor][telemetry]")
{
    FireAudioProcessor processor; const int slot = setupOtt(processor, 4);
    renderOtt(processor);
    CHECK_FALSE(processor.getInsertOttVisualState(4, slot).valid);
    for (const int scope : {-1, 5}) CHECK_FALSE(processor.getInsertOttVisualState(scope, 0).valid);
    for (const int invalidSlot : {-1, 8}) CHECK_FALSE(processor.getInsertOttVisualState(0, invalidSlot).valid);
    processor.releaseResources();
    CHECK_FALSE(processor.getInsertOttVisualState(4, slot).valid);
}
