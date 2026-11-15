#include <DSP/DynamicEqDetector.h>
#include <DSP/EqProcessor.h>
#include <DSP/InsertEffect.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <limits>

namespace
{
namespace de = fire::dynamic_eq;
constexpr double detectorRate = 48000;
float detectorInput(int sample, double frequency, float peak = .5f)
{ return peak * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * frequency * sample / detectorRate)); }
float settledGain(double signalFrequency, fire::eq::Type type, bool antiphase = false, float range = -6)
{
    de::Detector detector; detector.prepare(detectorRate);
    de::Configuration config; config.enabled = true; config.rangeDb = range;
    detector.setConfiguration(config);
    float result = 0;
    for (int i = 0; i < 24000; ++i)
    {
        const auto signal = detectorInput(i, signalFrequency);
        result = detector.process(signal, antiphase ? -signal : signal, true, 1000, 2, type);
    }
    return result;
}
}

TEST_CASE("Dynamic EQ defaults are silent and unsupported filters cannot apply gain", "[dynamic-eq-level][dsp]")
{
    de::Detector detector; detector.prepare(detectorRate); detector.setConfiguration({});
    for (int i = 0; i < 1000; ++i)
        CHECK(detector.process(.9f, -.9f, true, 1000, 2, fire::eq::Type::bell) == 0);
    de::Configuration enabled; enabled.enabled = true; detector.setConfiguration(enabled);
    for (auto type : {fire::eq::Type::lowCut, fire::eq::Type::highCut, fire::eq::Type::notch, fire::eq::Type::bandPass})
        CHECK(detector.process(.9f, -.9f, true, 1000, 2, type) == 0);
}

TEST_CASE("Dynamic EQ reacts to its selected frequency region with linked anti-phase detection", "[dynamic-eq-level][dsp]")
{
    const auto centre = settledGain(1000, fire::eq::Type::bell);
    CHECK(centre == Catch::Approx(-6).margin(.03));
    CHECK(settledGain(1000, fire::eq::Type::bell, true) == centre);
    CHECK(std::abs(settledGain(50, fire::eq::Type::bell)) < .1);
    CHECK(settledGain(100, fire::eq::Type::lowShelf) < -5);
    CHECK(std::abs(settledGain(12000, fire::eq::Type::lowShelf)) < .1);
    CHECK(settledGain(12000, fire::eq::Type::highShelf) < -5);
    CHECK(std::abs(settledGain(100, fire::eq::Type::highShelf)) < .1);
    CHECK(settledGain(1000, fire::eq::Type::bell, false, 6) == Catch::Approx(6).margin(.03));
}

TEST_CASE("Dynamic EQ attacks releases and becomes exactly inactive after its disable bridge", "[dynamic-eq-level][dsp]")
{
    de::Detector fast, slow;
    fast.prepare(detectorRate); slow.prepare(detectorRate);
    de::Configuration a; a.enabled = true; a.attackMs = .1f; a.releaseMs = 30;
    auto b = a; b.attackMs = 200;
    fast.setConfiguration(a); slow.setConfiguration(b);
    for (int i = 0; i < 1000; ++i)
    {
        const auto signal = detectorInput(i, 1000);
        fast.process(signal, signal, true, 1000, 2, fire::eq::Type::bell);
        slow.process(signal, signal, true, 1000, 2, fire::eq::Type::bell);
    }
    CHECK(fast.getGainDb() < slow.getGainDb() - 1);
    for (int i = 0; i < 24000; ++i) fast.process(0, 0, true, 1000, 2, fire::eq::Type::bell);
    CHECK(std::abs(fast.getGainDb()) < .01);
    a.enabled = false; fast.setConfiguration(a);
    for (int i = 0; i < 241; ++i) fast.process(.9f, -.9f, true, 1000, 2, fire::eq::Type::bell);
    CHECK_FALSE(fast.isActive()); CHECK(fast.getGainDb() == 0);
}

TEST_CASE("Dynamic EQ timing and coefficient grids do not depend on host partitions", "[dynamic-eq-level][dsp]")
{
    const auto render = [](int size)
    {
        de::Detector detector; detector.prepare(detectorRate);
        de::Configuration config; config.enabled = true;
        std::vector<float> result(12000);
        for (int offset = 0; offset < 12000; offset += size)
        {
            detector.setConfiguration(config);
            for (int i = offset; i < juce::jmin(12000, offset + size); ++i)
            {
                const auto signal = detectorInput(i, 1000);
                const auto frequency = 1000 + 200 * std::sin(static_cast<float>(i) * .0007f);
                result[static_cast<size_t>(i)] = detector.process(signal, -signal, true, frequency, 1.2f, fire::eq::Type::bell);
            }
        }
        return result;
    };
    const auto reference = render(1);
    CHECK(render(64) == reference); CHECK(render(257) == reference);
}

TEST_CASE("Dynamic EQ stays finite through malformed audio and recovers", "[dynamic-eq-level][dsp]")
{
    de::Detector detector; detector.prepare(48000);
    de::Configuration config; config.enabled = true; detector.setConfiguration(config);
    for (float sample : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()})
    {
        for (int i = 0; i < 128; ++i)
        {
            const auto delta = detector.process(sample, -sample, true, 1000, 2, fire::eq::Type::bell);
            CHECK(std::isfinite(delta)); CHECK(delta >= -24); CHECK(delta <= 24);
        }
    }
    detector.reset(); detector.setConfiguration(config);
    for (int i = 0; i < 1024; ++i) CHECK(detector.process(0, 0, true, 1000, 2, fire::eq::Type::bell) == 0);
}

TEST_CASE("Dynamic EQ parameter addresses are unique complete and append-only", "[dynamic-eq-level][parameters]")
{
    const auto& ids = de::parameterIDs();
    REQUIRE(ids.size() == 2952);
    auto unique = ids; unique.removeDuplicates(false); CHECK(unique.size() == ids.size());
    CHECK(de::parameterID(0, -1, 1, de::enabled) == "masterEqDynamic2Enabled");
    CHECK(de::parameterID(4, 7, 11, de::release) == "bandFx8EqDynamic12ReleaseBand4");
    CHECK(de::parameterID(1, -1, 0, de::enabled).isEmpty());
    for (const auto& id : ids) CHECK(de::isParameterID(id));
    CHECK_FALSE(de::isParameterID("masterEqDynamic13Enabled"));
}

namespace
{
fire::eq::Processor::Parameters dynamicNode(float gain = 0, bool enabled = true)
{
    fire::eq::Processor::Parameters p;
    p.enabled = true; p.state.present = true; p.state.type = fire::eq::Type::bell;
    p.controls[0].baseValue = 1000; p.controls[0].range = {20, 20000};
    p.controls[1].baseValue = gain; p.controls[1].range = {-24, 24};
    p.controls[2].baseValue = 2; p.controls[2].range = {.1f, 20};
    p.dynamic.enabled = enabled;
    return p;
}
}

TEST_CASE("Disabled dynamics leaves both static and modulated EQ output bit exact", "[dynamic-eq-level][dsp][legacy]")
{
    fire::eq::Processor baseline, disabled;
    baseline.prepare({48000, 127, 2}); disabled.prepare({48000, 127, 2});
    auto original = dynamicNode(4, false), altered = original;
    altered.dynamic.thresholdDb = -60; altered.dynamic.ratio = 20; altered.dynamic.rangeDb = 24;
    altered.dynamic.attackMs = .1f; altered.dynamic.releaseMs = 2000;
    juce::AudioBuffer<float> a(2, 127), b(2, 127);
    std::array<float, 127> lfo {};
    for (auto* p : {&original, &altered})
    { p->controls[1].modulationDepth = .2f; p->controls[1].lfoSignal = lfo.data(); p->sources[1] = 0; }
    for (int offset = 0; offset < 12065; offset += 127)
    {
        for (int sample = 0; sample < 127; ++sample)
        {
            const auto input = detectorInput(offset + sample, 1000, .15f);
            a.setSample(0, sample, input); a.setSample(1, sample, -input);
            lfo[static_cast<size_t>(sample)] = .5f + .5f * std::sin(static_cast<float>(offset + sample) * .01f);
        }
        b.makeCopyOf(a, true);
        auto left = juce::dsp::AudioBlock<float>(a), right = juce::dsp::AudioBlock<float>(b);
        baseline.begin(3, original); disabled.begin(3, altered);
        baseline.beginModuleInput(left); disabled.beginModuleInput(right);
        baseline.process(3, left, 0, 127); disabled.process(3, right, 0, 127);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 127; ++sample) CHECK(a.getSample(channel, sample) == b.getSample(channel, sample));
        CHECK(disabled.getDynamicEqVisualState(3).gainChangeDb == 0);
        CHECK_FALSE(disabled.getDynamicEqVisualState(3).active);
    }
}

TEST_CASE("Every dynamic node detects the original module input instead of preceding EQ output", "[dynamic-eq-level][dsp][chain]")
{
    fire::eq::Processor boosted, plain;
    boosted.prepare({48000, 128, 2}); plain.prepare({48000, 128, 2});
    auto boost = dynamicNode(18, false), neutral = dynamicNode(0, false);
    auto dynamic = dynamicNode(); dynamic.dynamic.thresholdDb = -18; dynamic.dynamic.rangeDb = -12;
    juce::AudioBuffer<float> a(2, 128), b(2, 128);
    double boostedEnergy = 0, plainEnergy = 0;
    for (int offset = 0; offset < 24064; offset += 128)
    {
        for (int sample = 0; sample < 128; ++sample)
        { const auto input = detectorInput(offset + sample, 1000, .1f); a.setSample(0, sample, input); a.setSample(1, sample, -input); }
        b.makeCopyOf(a, true);
        auto left = juce::dsp::AudioBlock<float>(a), right = juce::dsp::AudioBlock<float>(b);
        boosted.begin(3, boost); boosted.begin(4, dynamic);
        plain.begin(3, neutral); plain.begin(4, dynamic);
        boosted.beginModuleInput(left); plain.beginModuleInput(right);
        for (int node : {3, 4}) { boosted.process(node, left, 0, 128); plain.process(node, right, 0, 128); }
        CHECK(boosted.getDynamicEqVisualState(4).gainChangeDb == plain.getDynamicEqVisualState(4).gainChangeDb);
        if (offset > 12000)
            for (int sample = 0; sample < 128; ++sample)
            { boostedEnergy += a.getSample(0, sample) * a.getSample(0, sample); plainEnergy += b.getSample(0, sample) * b.getSample(0, sample); }
    }
    CHECK(boostedEnergy > 20 * plainEnergy); // The earlier node really changed the audible signal.
}

TEST_CASE("Dynamic gain adds to routed gain and reports the effective bounded response", "[dynamic-eq-level][dsp][modulation]")
{
    for (bool boost : {false, true})
    {
        fire::eq::Processor engine; engine.prepare({48000, 128, 2});
        auto p = dynamicNode(boost ? 23 : 0);
        std::array<float, 128> signal; signal.fill(1);
        if (!boost)
        {
            p.controls[1].modulationDepth = .25f; p.controls[1].isBipolar = false;
            p.controls[1].lfoSignal = signal.data(); p.sources[1] = 17;
        }
        p.dynamic.rangeDb = boost ? 6 : -6;
        juce::AudioBuffer<float> audio(2, 128);
        for (int offset = 0; offset < 24064; offset += 128)
        {
            for (int sample = 0; sample < 128; ++sample)
            { const auto input = detectorInput(offset + sample, 1000); audio.setSample(0, sample, input); audio.setSample(1, sample, input); }
            auto block = juce::dsp::AudioBlock<float>(audio);
            engine.begin(3, p); engine.beginModuleInput(block); engine.process(3, block, 0, 128);
        }
        const auto state = engine.getDynamicEqVisualState(3);
        REQUIRE(state.active); CHECK(state.renderSequence > 0);
        CHECK(state.effectiveGainDb == Catch::Approx(boost ? 24 : 6).margin(.04));
        CHECK(state.gainChangeDb == Catch::Approx(boost ? 1 : -6).margin(.04));
        CHECK(engine.currentState(3).gainDb == state.effectiveGainDb);
        p.enabled = false; engine.begin(3, p);
        CHECK(engine.getDynamicEqVisualState(3).renderSequence == 0);
    }
}

TEST_CASE("Core EQ forwards dynamic settings and retires telemetry when inactive", "[dynamic-eq-level][dsp][insertfx]")
{
    fire::effects::CoreEffect core; core.prepare({48000, 128, 2});
    std::array<fire::effects::CoreEffect::EqNode, fire::eq::maxNodes> nodes;
    auto& node = nodes[3]; node.state.present = true; node.state.type = fire::eq::Type::bell;
    node.controls[0].value = 1000; node.controls[1].value = 0; node.controls[2].value = 2;
    node.dynamic.enabled = true;
    std::array<ModulatedValueProvider, 6> controls;
    juce::AudioBuffer<float> audio(2, 128);
    for (int offset = 0; offset < 24064; offset += 128)
    {
        for (int sample = 0; sample < 128; ++sample)
        { const auto input = detectorInput(offset + sample, 1000); audio.setSample(0, sample, input); audio.setSample(1, sample, -input); }
        core.process(juce::dsp::AudioBlock<float>(audio), fire::effects::Type::eq, controls, false, 0, nullptr, nodes);
    }
    CHECK(core.getDynamicEqVisualState(3).gainChangeDb == Catch::Approx(-6).margin(.04));
    REQUIRE(core.getDynamicEqVisualState(3).active);
    core.reset();
    CHECK(core.getDynamicEqVisualState(3).renderSequence == 0);
}

TEST_CASE("Changing a dynamic Bell to an unsupported type retires detection completely", "[dynamic-eq-level][dsp][type-switch]")
{
    de::Detector detector; detector.prepare(detectorRate);
    de::Configuration configuration; configuration.enabled = true;
    detector.setConfiguration(configuration);
    for (int sample = 0; sample < 2400; ++sample)
        detector.process(detectorInput(sample, 1000), 0, false, 1000, 2, fire::eq::Type::bell);
    REQUIRE(detector.isActive()); REQUIRE(detector.getGainDb() < -1);
    configuration.enabled = false; detector.setConfiguration(configuration);
    CHECK(detector.process(.5f, -.5f, true, 1000, 2, fire::eq::Type::lowCut) == 0);
    CHECK_FALSE(detector.isActive());
    configuration.enabled = true; detector.setConfiguration(configuration);
    CHECK(detector.process(0, 0, true, 1000, 2, fire::eq::Type::bell) == 0);
}
