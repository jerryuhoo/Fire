#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
using Type = fire::effects::Type;
fire::effects::RackParameters delayRecipe()
{
    fire::effects::RackParameters p;
    auto& effect = p[0].effect;
    effect.type = Type::delay;
    effect.values[0].baseValue = fire::effects::controls(Type::delay)[0].toNormalised(20);
    effect.values[1].baseValue = 0;
    effect.values[2].baseValue = 1;
    effect.values[3].baseValue = 0;
    effect.values[4].baseValue = 0;
    effect.values[5].baseValue = 1;
    return p;
}
}

TEST_CASE("Empty insert slots prepare only requested effect families",
          "[insert-preparation][insertfx][memory][lifecycle]")
{
    fire::effects::InsertRack rack;
    rack.prepare({48000, 64, 2}, true, false);
    for (int slot = 0; slot < fire::effects::slotCount; ++slot)
    {
        CAPTURE(slot);
        CHECK(rack.isSlotPreparedForType(slot, Type::none));
        CHECK(rack.isSlotPreparedForType(slot, Type::phaser)); // Fixed-size state only.
        for (const auto type : {Type::delay, Type::reverb, Type::granular, Type::lofi, Type::chordResonator, Type::shape})
            CHECK_FALSE(rack.isSlotPreparedForType(slot, type));
    }
    REQUIRE(rack.prepareSlotForType(0, Type::delay));
    CHECK(rack.isSlotPreparedForType(0, Type::chorus));
    CHECK(rack.isSlotPreparedForType(0, Type::flanger));
    CHECK_FALSE(rack.isSlotPreparedForType(0, Type::reverb));
    CHECK_FALSE(rack.isSlotPreparedForType(0, Type::granular));
    CHECK_FALSE(rack.isSlotPreparedForType(0, Type::shape));
    CHECK_FALSE(rack.isSlotPreparedForType(1, Type::delay));
    REQUIRE(rack.prepareSlotForType(1, Type::shape));
    CHECK(rack.isSlotPreparedForType(1, Type::eq));
    CHECK(rack.isSlotPreparedForType(1, Type::compressor));
    CHECK_FALSE(rack.isSlotPreparedForType(1, Type::granular));
}

TEST_CASE("Audio requests missing engines without preparing them and retains fixed dry timing",
          "[insert-preparation][insertfx][realtime][latency]")
{
    fire::effects::InsertRack rack;
    rack.prepare({48000, 64, 2}, true, false);
    const auto p = delayRecipe();
    juce::AudioBuffer<float> block(2, 64), noModulation;
    bool correctDry = true;
    for (int callback = 0; callback < 8; ++callback)
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 64; ++sample) block.setSample(channel, sample, .123f);
        rack.process(juce::dsp::AudioBlock<float>(block), p, noModulation);
        if (callback > 0)
            for (int sample = 0; sample < 64; ++sample)
                correctDry = correctDry && std::abs(block.getSample(0, sample) - .123f) < 1e-6f;
        CHECK_FALSE(rack.isSlotPreparedForType(0, Type::delay));
    }
    CHECK(correctDry);
    rack.prepareRequestedFamilies(); // Explicitly outside the callback.
    REQUIRE(rack.isSlotPreparedForType(0, Type::delay));
    double difference = 0;
    for (int callback = 0; callback < 64; ++callback)
    {
        for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < 64; ++sample)
            block.setSample(channel, sample, .1f * std::sin((callback * 64 + sample) * .081f));
        const auto original = block;
        rack.process(juce::dsp::AudioBlock<float>(block), p, noModulation);
        for (int sample = 0; sample < 64; ++sample)
        {
            REQUIRE(std::isfinite(block.getSample(0, sample)));
            difference += std::abs(block.getSample(0, sample) - original.getSample(0, sample));
        }
    }
    CHECK(difference > 1);
}

TEST_CASE("Preparation worker publishes complete engines for generic host type automation",
          "[insert-preparation][insertfx][threading][automation]")
{
    fire::effects::InsertRack rack;
    rack.prepare({96000, 64, 2}, true, false);
    std::array<fire::effects::InsertRack*, fire::effects::scopeCount> targets{&rack};
    fire::effects::RackPreparationWorker worker(targets);
    fire::effects::RackParameters p;
    p[0].effect.type = Type::shape;
    p[0].effect.shapeModel = 1;
    p[0].effect.analogDrive.baseValue = 20;
    p[0].effect.analogDrive.range = {0, 100};
    p[0].effect.values[5].baseValue = 1;
    juce::AudioBuffer<float> block(2, 64), noModulation;
    block.clear(); rack.process(juce::dsp::AudioBlock<float>(block), p, noModulation, 0, true);
    const auto started = juce::Time::getMillisecondCounter();
    while (!rack.isSlotPreparedForType(0, Type::shape)
           && juce::Time::getMillisecondCounter() - started < 3000u)
        juce::Thread::sleep(5);
    REQUIRE(rack.isSlotPreparedForType(0, Type::shape));
    for (int callback = 0; callback < 100; ++callback)
    {
        for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < 64; ++sample)
            block.setSample(channel, sample, .1f * std::sin((callback * 64 + sample) * .041f));
        rack.process(juce::dsp::AudioBlock<float>(block), p, noModulation, 0, true);
        CHECK(std::isfinite(block.getMagnitude(0, 64)));
    }
}
