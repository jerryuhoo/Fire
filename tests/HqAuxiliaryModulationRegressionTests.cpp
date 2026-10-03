#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace
{
namespace sources = fire::mod_sources;
constexpr double sampleRate = 48000.0;
enum class Target { drive, bias, rectification, shapeMix };
constexpr std::array targets {Target::drive, Target::bias, Target::rectification, Target::shapeMix};

ModulatedValueProvider& targetProvider(BandProcessingParameters& p, Target target)
{
    switch (target)
    {
        case Target::drive: return p.driveVal;
        case Target::bias: return p.biasVal;
        case Target::rectification: return p.recVal;
        case Target::shapeMix: return p.shapeMixValProvider;
    }
    return p.driveVal;
}

BandProcessingParameters parameters(Target target, int source, bool hq, bool split)
{
    BandProcessingParameters p;
    p.mode = 2; // Tanh produces an audible response to all four controls.
    p.isHQ = hq;
    p.isDriveEnabled = p.isShapeEnabled = true;
    p.isSafeModeOn = false;
    p.driveVal.baseValue = 12;
    p.driveVal.range = {0, 100};
    p.biasVal.baseValue = -0.1f;
    p.biasVal.range = {-1, 1};
    p.recVal.baseValue = 0.05f;
    p.recVal.range = {0, 1};
    p.shapeMixVal = p.shapeMixValProvider.baseValue = 0.3f;
    p.shapeMixValProvider.range = {0, 1};
    p.mixValProvider.baseValue = 1;
    p.outputVal.range = {-48, 6};
    for (auto t : targets) targetProvider(p, t).modulationDepth = 0;
    if (source >= 0)
    {
        auto& provider = targetProvider(p, target);
        provider.modulationDepth = 0.4f;
        provider.isBipolar = false;
        switch (target)
        {
            case Target::drive: p.driveLfoSourceIndex = source; break;
            case Target::bias: p.biasLfoSourceIndex = source; break;
            case Target::rectification: p.recLfoSourceIndex = source; break;
            case Target::shapeMix: p.shapeMixLfoSourceIndex = source; break;
        }
    }
    // Separate Drive from Shape so Drive runs at base rate while Shape uses HQ.
    if (split) std::swap(p.moduleOrder[1], p.moduleOrder[2]);
    return p;
}

float input(int channel, int sample)
{
    const auto t = static_cast<double>(sample) / sampleRate;
    return static_cast<float>(0.14 * std::sin(2 * juce::MathConstants<double>::pi * 733 * t + channel * 0.31)
                              + 0.04 * std::cos(2 * juce::MathConstants<double>::pi * 1837 * t));
}

struct Comparison
{
    float error = 0, referenceEffect = 0, auxiliaryEffect = 0;
    bool finite = true;
};

Comparison compareSource(Target target, int source, bool hq, bool split, bool transitions)
{
    constexpr int phaseLength = 2048;
    const int totalSamples = transitions ? phaseLength * 5 : 6144;
    auto reference = std::make_unique<BandProcessor>();
    auto auxiliary = std::make_unique<BandProcessor>();
    auto unrouted = std::make_unique<BandProcessor>();
    for (auto* band : {reference.get(), auxiliary.get(), unrouted.get()})
    {
        band->prepare({sampleRate, 64, 2}, false);
        band->gain.setGainDecibels(0);
        band->gain.reset();
    }
    fire::dsp::AuxiliaryModulation generator;
    generator.prepare(sampleRate);
    auto controls = sources::defaults;
    controls[0] = 0.1f;
    controls[1] = 40;
    controls[2] = 12;
    for (int macro = 0; macro < sources::macroCount; ++macro) controls[static_cast<size_t>(3 + macro)] = 0.15f;
    generator.setParameters(controls);

    // All channels come from the real auxiliary generator. Copy only the
    // selected signal into LFO 1: identical audio must not depend on source ID.
    Comparison result;
    int position = 0;
    size_t callback = 0;
    bool previousHq = hq;
    constexpr std::array callbackSizes {31, 7, 257, 1, 129, 511};
    while (position < totalSamples)
    {
        const int phase = position / phaseLength;
        const bool useHq = transitions ? phase == 1 || phase == 2 || phase == 4 : hq;
        const bool separate = transitions ? phase == 2 || phase == 3 : split;
        if (useHq != previousHq)
            for (auto* band : {reference.get(), auxiliary.get(), unrouted.get()})
                band->resetQualityTransitionState();
        previousHq = useHq;
        const int size = std::min({callbackSizes[callback++ % callbackSizes.size()],
                                   totalSamples - position, phaseLength - position % phaseLength});
        if (position == phaseLength * 2)
        {
            for (int macro = 0; macro < sources::macroCount; ++macro) controls[static_cast<size_t>(3 + macro)] = 0.85f;
            generator.setParameters(controls);
        }
        juce::AudioBuffer<float> actual(2, size), expected(2, size), dry(2, size);
        juce::AudioBuffer<float> bank(sources::sourceCount, size);
        bank.clear();
        for (int sample = 0; sample < size; ++sample)
        {
            const float left = input(0, position + sample), right = input(1, position + sample);
            actual.setSample(0, sample, left); actual.setSample(1, sample, right);
            const auto values = generator.next(left, right, true);
            for (size_t index = 0; index < values.size(); ++index)
                bank.setSample(sources::envelope + static_cast<int>(index), sample, values[index]);
            bank.setSample(0, sample, values[static_cast<size_t>(source - sources::envelope)]);
        }
        expected.makeCopyOf(actual); dry.makeCopyOf(actual);
        reference->process(expected, parameters(target, 0, useHq, separate), bank);
        auxiliary->process(actual, parameters(target, source, useHq, separate), bank);
        unrouted->process(dry, parameters(target, -1, useHq, separate), bank);
        for (int sample = 0; sample < size; ++sample)
        {
            // Let control and order bridges settle, including after live changes.
            if (transitions ? (position + sample) % phaseLength < 1024 : position + sample < 4096) continue;
            for (int channel = 0; channel < 2; ++channel)
            {
                const float a = actual.getSample(channel, sample), e = expected.getSample(channel, sample), d = dry.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(a) && std::isfinite(e) && std::isfinite(d);
                result.error = std::max(result.error, std::abs(a - e));
                result.referenceEffect = std::max(result.referenceEffect, std::abs(e - d));
                result.auxiliaryEffect = std::max(result.auxiliaryEffect, std::abs(a - d));
            }
        }
        position += size;
    }
    return result;
}

void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* p = processor.treeState.getParameter(id); REQUIRE(p != nullptr);
    p->setValueNotifyingHost(p->convertTo0to1(value));
}

void configure(FireAudioProcessor& processor, bool hq)
{
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, NUM_BANDS_ID, 1); set(processor, HQ_ID, hq ? 1 : 0);
    set(processor, MIX_ID, 1); set(processor, OUTPUT_ID, 0);
    set(processor, FILTER_BYPASS_ID, 0); set(processor, DOWNSAMPLE_BYPASS_ID, 0);
    for (auto* id : {BAND_ENABLE_ID, DRIVE_BYPASS_ID, SHAPE_BYPASS_ID}) set(processor, ParameterIDAndName::getIDString(id, 0), 1);
    for (auto* id : {LINKED_ID, SAFE_ID, EXTREME_ID, COMP_BYPASS_ID, WIDTH_BYPASS_ID, DC_FILTER_ID}) set(processor, ParameterIDAndName::getIDString(id, 0), 0);
    set(processor, "driveCompModern1", 0); set(processor, "mode1", 2);
    set(processor, "drive1", 18); set(processor, "bias1", -0.1f);
    set(processor, "rec1", 0.1f); set(processor, "shapeMix1", 0.3f);
    set(processor, "mix1", 1); set(processor, "output1", 0);
}
}

TEST_CASE("HQ distortion receives Envelope and every Macro just like an equivalent LFO signal",
          "[aux-mod][hq-aux][dsp][hq][routing]")
{
    for (auto target : targets)
        for (int source = sources::envelope; source < sources::sourceCount; ++source)
            for (bool hq : {false, true})
                for (bool split : {false, true})
                {
                    CAPTURE(static_cast<int>(target), source, hq, split);
                    const auto result = compareSource(target, source, hq, split, false);
                    CHECK(result.finite);
                    REQUIRE(result.referenceEffect > 1.0e-4f);
                    CHECK(result.auxiliaryEffect > 1.0e-4f);
                    CHECK(result.error < 2.0e-6f);
                }
}

TEST_CASE("Auxiliary distortion routes survive live HQ changes and module reordering with oversized callbacks",
          "[aux-mod][hq-aux][dsp][hq][module-order][internal-chunk]")
{
    for (auto target : targets)
        for (int source = sources::envelope; source < sources::sourceCount; ++source)
        {
            CAPTURE(static_cast<int>(target), source);
            const auto result = compareSource(target, source, false, false, true);
            CHECK(result.finite);
            REQUIRE(result.referenceEffect > 1.0e-4f);
            CHECK(result.auxiliaryEffect > 1.0e-4f);
            CHECK(result.error < 2.0e-6f);
        }
}

TEST_CASE("Macro routing reaches real processor distortion audio in Base and HQ",
          "[aux-mod][hq-aux][dsp][hq][processor]")
{
    for (const auto* id : {"drive1", "bias1", "rec1", "shapeMix1"})
        for (bool hq : {false, true})
        {
            CAPTURE(id, hq);
            auto actual = std::make_unique<FireAudioProcessor>(), expected = std::make_unique<FireAudioProcessor>();
            configure(*actual, hq); configure(*expected, hq);
            constexpr float macro = 0.8f, depth = 0.25f;
            set(*actual, "macro1", macro);
            auto* target = expected->treeState.getParameter(id); REQUIRE(target != nullptr);
            target->setValueNotifyingHost(target->getValue() + macro * depth);
            REQUIRE(actual->assignLfoToTarget(sources::firstMacro, id) == LfoManager::AssignmentResult::changed);
            actual->setModulationDepth(id, depth);
            REQUIRE_FALSE(actual->getModulationInfoForParameter(id).isBipolar);
            actual->prepareToPlay(sampleRate, 128); expected->prepareToPlay(sampleRate, 128);
            float error = 0;
            bool finite = true;
            juce::MidiBuffer midi;
            for (int block = 0; block < 64; ++block)
            {
                juce::AudioBuffer<float> a(2, 128), e(2, 128);
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < 128; ++sample) a.setSample(channel, sample, input(channel, block * 128 + sample));
                e.makeCopyOf(a);
                actual->processBlock(a, midi); expected->processBlock(e, midi);
                if (block < 48) continue;
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < 128; ++sample)
                    {
                        finite = finite && std::isfinite(a.getSample(channel, sample)) && std::isfinite(e.getSample(channel, sample));
                        error = std::max(error, std::abs(a.getSample(channel, sample) - e.getSample(channel, sample)));
                    }
            }
            CHECK(finite);
            CHECK(error < 2.0e-4f);
        }
}
