#include <PluginProcessor.h>
#include <Utility/ParallelChainParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
namespace pc = fire::parallel_chain;
namespace fx = fire::effects;
constexpr int blockSize = 128;
void parallelSet(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void parallelBase(FireAudioProcessor& p, bool hq = false, int channels = 2)
{
    p.hasUpdateCheckBeenPerformed = true;
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0, layout.inputBuses[0]);
    if (layout.inputBuses.size() > 1) layout.inputBuses.set(1, juce::AudioChannelSet::disabled());
    REQUIRE(p.setBusesLayout(layout));
    parallelSet(p, NUM_BANDS_ID, 1); parallelSet(p, HQ_ID, hq ? 1 : 0);
    parallelSet(p, FILTER_BYPASS_ID, 0); parallelSet(p, DOWNSAMPLE_BYPASS_ID, 0);
    parallelSet(p, OUTPUT_ID, 0); parallelSet(p, MIX_ID, 1);
    for (const auto* base : {DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, COMP_BYPASS_ID,
                           WIDTH_BYPASS_ID, OTT_ENABLED_ID, DC_FILTER_ID, LINKED_ID, BAND_SOLO_ID})
        parallelSet(p, ParameterIDAndName::getIDString(base, 0), 0);
    parallelSet(p, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0), 1);
    parallelSet(p, ParameterIDAndName::getIDString(OUTPUT_ID, 0), 0);
    parallelSet(p, ParameterIDAndName::getIDString(MIX_ID, 0), 1);
    // Serial retains its historic waveshaper even with the Shape controls off.
    // Hard Clip is an actual identity for this fixture's sub-unity reference.
    parallelSet(p, ParameterIDAndName::getIDString(MODE_ID, 0), 4);
}
int parallelEq(FireAudioProcessor& p, int scope, float gain, bool enabled = true)
{
    const int slot = p.addInsertEffect(scope, fx::Type::eq); REQUIRE(slot >= 0);
    for (int node = 0; node < fire::eq::maxNodes; ++node)
        parallelSet(p, fire::core_modules::eqParameterID(scope, slot, node, fire::eq::Field::present), node == 1 ? 1 : 0);
    parallelSet(p, fire::core_modules::eqParameterID(scope, slot, 1, fire::eq::Field::type), 0);
    parallelSet(p, fire::core_modules::eqParameterID(scope, slot, 1, fire::eq::Field::frequency), 1000);
    parallelSet(p, fire::core_modules::eqParameterID(scope, slot, 1, fire::eq::Field::q), 1);
    parallelSet(p, fire::core_modules::eqParameterID(scope, slot, 1, fire::eq::Field::gain), gain);
    parallelSet(p, fx::parameterID(scope, slot, fx::enabledField), enabled ? 1 : 0);
    return slot;
}
void parallelPrepare(FireAudioProcessor& p)
{ p.setRateAndBufferSizeDetails(48000, blockSize); p.prepareToPlay(48000, blockSize); }
void parallelInput(juce::AudioBuffer<float>& buffer, int offset, bool silent = false)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const double t = static_cast<double>(offset + sample) / 48000;
        const auto mid = .12f * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * 1000 * t));
        const auto side = .08f * static_cast<float>(std::cos(2 * juce::MathConstants<double>::pi * 1000 * t + .3));
        buffer.setSample(0, sample, silent ? 0 : mid + side);
        if (buffer.getNumChannels() > 1) buffer.setSample(1, sample, silent ? 0 : mid - side);
    }
}
float parallelError(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float error = 0;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int sample = 0; sample < a.getNumSamples(); ++sample)
            error = std::max(error, std::abs(a.getSample(channel, sample) - b.getSample(channel, sample)));
    return error;
}
void configureTwoBranches(FireAudioProcessor& p, int scope, bool parallel, int serialBranch = -1, bool hq = false)
{
    parallelBase(p, hq);
    REQUIRE(parallelEq(p, scope, 6, serialBranch != 1) == 0);
    REQUIRE(parallelEq(p, scope, -6, serialBranch != 0) == 1);
    if (parallel)
    {
        REQUIRE(p.setModuleBranch(scope, fire::module_order::firstInsert + 1, pc::b));
        REQUIRE(p.setChainRoutingMode(scope, pc::parallel));
    }
    parallelPrepare(p);
}
}

TEST_CASE("Band and Master parallel endpoints match their filtered serial chains", "[parallel-processor][dsp]")
{
    for (int scope : {0, 1})
    {
        FireAudioProcessor actual, serialA, serialB;
        configureTwoBranches(actual, scope, true);
        configureTwoBranches(serialA, scope, false, 0);
        configureTwoBranches(serialB, scope, false, 1);
        parallelSet(actual, pc::blendID(scope), 0);
        juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize), c(2, blockSize); juce::MidiBuffer midi;
        float errorA = 0, errorB = 0;
        for (int block = 0; block < 112; ++block)
        {
            if (block == 64) parallelSet(actual, pc::blendID(scope), 1);
            parallelInput(a, block * blockSize); b.makeCopyOf(a, true); c.makeCopyOf(a, true);
            actual.processBlock(a, midi); serialA.processBlock(b, midi); serialB.processBlock(c, midi);
            if (block >= 32 && block < 64) errorA = std::max(errorA, parallelError(a, b));
            if (block >= 96) errorB = std::max(errorB, parallelError(a, c));
        }
        CAPTURE(scope, errorA, errorB);
        CHECK(errorA < 2e-6f); CHECK(errorB < 2e-6f);
        CHECK(actual.getLatencySamples() == serialA.getLatencySamples());
    }
}

TEST_CASE("An empty parallel branch is aligned dry and midpoint is the linear blend", "[parallel-processor][dsp][mix]")
{
    FireAudioProcessor actual, wet, dry;
    for (auto* p : {&actual, &wet, &dry}) parallelBase(*p);
    parallelEq(actual, 1, 6); parallelEq(wet, 1, 6); parallelEq(dry, 1, 6, false);
    REQUIRE(actual.setChainRoutingMode(1, pc::parallel)); parallelSet(actual, pc::blendID(1), .5f);
    for (auto* p : {&actual, &wet, &dry}) parallelPrepare(*p);
    juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize), c(2, blockSize); juce::MidiBuffer midi;
    float error = 0; double wetDifference = 0;
    for (int block = 0; block < 80; ++block)
    {
        parallelInput(a, block * blockSize); b.makeCopyOf(a, true); c.makeCopyOf(a, true);
        actual.processBlock(a, midi); wet.processBlock(b, midi); dry.processBlock(c, midi);
        if (block < 32) continue;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
            {
                error = std::max(error, std::abs(a.getSample(channel, sample) - .5f * (b.getSample(channel, sample) + c.getSample(channel, sample))));
                wetDifference += std::abs(b.getSample(channel, sample) - c.getSample(channel, sample));
            }
    }
    CHECK(error < 2e-6f); CHECK(wetDifference > 10);
}

TEST_CASE("Serial mode ignores latent branch edits without resetting or muting the audio", "[parallel-processor][dsp][topology]")
{
    FireAudioProcessor edited, reference;
    configureTwoBranches(edited, 1, false); configureTwoBranches(reference, 1, false);
    juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize); juce::MidiBuffer midi;
    float error = 0;
    for (int block = 0; block < 100; ++block)
    {
        if (block == 40) REQUIRE(edited.setModuleBranch(1, fire::module_order::firstInsert, pc::b));
        if (block == 44) REQUIRE(edited.setModuleBranch(1, fire::module_order::firstInsert + 1, pc::b));
        if (block == 48) REQUIRE(edited.setChainRoutingMode(4, pc::parallel)); // Inactive band.
        parallelInput(a, block * blockSize); b.makeCopyOf(a, true);
        edited.processBlock(a, midi); reference.processBlock(b, midi);
        error = std::max(error, parallelError(a, b));
    }
    CHECK(error == 0);
}

TEST_CASE("Independent Shape reserve and legacy HQ latency stay fixed across parallel routing", "[parallel-processor][dsp][hq][latency]")
{
    for (int scope : {0, 1}) for (bool hq : {false, true})
    {
        FireAudioProcessor actual, reference;
        for (auto* p : {&actual, &reference})
        {
            parallelBase(*p, hq);
            REQUIRE(p->addInsertEffect(scope, fx::Type::shape) == 0);
            REQUIRE(p->setShapeMode(scope, 0, 4)); // Identity hard clip for this sub-unity input.
        }
        REQUIRE(actual.setModuleBranch(scope, fire::module_order::firstInsert, pc::b));
        // Keep BOTH oversamplers on the selected branch. Its fractional phase
        // then matches the serial reference; arbitrary dry/HQ blends need not.
        if (scope == 1) REQUIRE(actual.setModuleBranch(1, 1, pc::b));
        REQUIRE(actual.setChainRoutingMode(scope, pc::parallel)); parallelSet(actual, pc::blendID(scope), 1);
        for (auto* p : {&actual, &reference}) parallelPrepare(*p);
        juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize); juce::MidiBuffer midi;
        float error = 0;
        for (int block = 0; block < 80; ++block)
        {
            parallelInput(a, block * blockSize); b.makeCopyOf(a, true);
            actual.processBlock(a, midi); reference.processBlock(b, midi);
            if (block >= 32) error = std::max(error, parallelError(a, b));
            CHECK(actual.getLatencySamples() == reference.getLatencySamples());
        }
        CAPTURE(scope, hq, error); CHECK(error < 2e-5f);
    }
}

TEST_CASE("Parallel EQ respects untouched Mid Side components and mono Side pass through", "[parallel-processor][dsp][mid-side]")
{
    for (int mode : {1, 2, 3})
    {
        const bool mono = mode == 3;
        const int channels = mono ? 1 : 2, channelMode = mono ? 2 : mode;
        FireAudioProcessor actual, reference;
        for (auto* p : {&actual, &reference})
        {
            parallelBase(*p, true, channels);
            parallelEq(*p, 1, 6, p == &actual);
            REQUIRE(p->setChainRoutingMode(1, pc::parallel));
            parallelSet(*p, pc::blendID(1), .5f);
            parallelSet(*p, fire::channel_mode::parameterID(1), static_cast<float>(channelMode));
            parallelPrepare(*p);
        }
        juce::AudioBuffer<float> a(channels, blockSize), b(channels, blockSize); juce::MidiBuffer midi;
        float untouched = 0; double actualEnergy = 0, referenceEnergy = 0;
        for (int block = 0; block < 96; ++block)
        {
            parallelInput(a, block * blockSize); b.makeCopyOf(a, true);
            actual.processBlock(a, midi); reference.processBlock(b, midi);
            if (block < 48) continue;
            for (int sample = 0; sample < blockSize; ++sample)
            {
                if (mono) { untouched = std::max(untouched, std::abs(a.getSample(0, sample) - b.getSample(0, sample))); continue; }
                const auto am = .5f * (a.getSample(0, sample) + a.getSample(1, sample));
                const auto as = .5f * (a.getSample(0, sample) - a.getSample(1, sample));
                const auto bm = .5f * (b.getSample(0, sample) + b.getSample(1, sample));
                const auto bs = .5f * (b.getSample(0, sample) - b.getSample(1, sample));
                untouched = std::max(untouched, std::abs(mode == 1 ? as - bs : am - bm));
                const auto active = mode == 1 ? am : as, passive = mode == 1 ? bm : bs;
                actualEnergy += active * active; referenceEnergy += passive * passive;
            }
        }
        CAPTURE(mode, untouched, actualEnergy, referenceEnergy);
        CHECK(untouched < 2e-5f);
        if (!mono) { REQUIRE(referenceEnergy > 1); CHECK(actualEnergy > referenceEnergy * 1.5); }
    }
}

namespace
{
juce::AudioBuffer<float> renderParallelModulation(int size, bool cc)
{
    FireAudioProcessor p;
    configureTwoBranches(p, 1, true, -1, true);
    parallelSet(p, pc::blendID(1), .5f);
    if (cc)
    {
        parallelSet(p, fire::midi_mod::parameterID(0, fire::midi_mod::number), 74);
        parallelSet(p, fire::midi_mod::parameterID(0, fire::midi_mod::channel), 2);
    }
    else
    {
        parallelSet(p, fire::lfo_bank::parameterID(0, fire::lfo_bank::Field::syncMode), 0);
        parallelSet(p, fire::lfo_bank::parameterID(0, fire::lfo_bank::Field::rateHz), 17.3f);
        LfoData shape; shape.points = {{0, 0}, {1, 1}}; shape.curvatures = {0}; p.getLfoManager().setLfoData(0, shape);
    }
    REQUIRE(p.assignLfoToTarget(cc ? fire::mod_sources::firstMidiCC : 0, pc::blendID(1)) == LfoManager::AssignmentResult::changed);
    p.setModulationDepth(pc::blendID(1), 1);
    constexpr int total = 20000;
    constexpr std::array<int, 8> events {17, 127, 128, 8191, 8192, 17002, 17003, 19001};
    juce::AudioBuffer<float> result(2, total);
    for (int offset = 0; offset < total; offset += size)
    {
        const int count = std::min(size, total - offset);
        juce::AudioBuffer<float> audio(2, count); juce::MidiBuffer midi; parallelInput(audio, offset);
        if (cc)
            for (size_t index = 0; index < events.size(); ++index)
                if (events[index] >= offset && events[index] < offset + count)
                    midi.addEvent(juce::MidiMessage::controllerEvent(2, 74, static_cast<int>((index * 37 + 17) % 128)), events[index] - offset);
        p.processBlock(audio, midi);
        for (int channel = 0; channel < 2; ++channel) result.copyFrom(channel, offset, audio, channel, 0, count);
    }
    return result;
}
}

TEST_CASE("Parallel Blend LFO and MIDI CC offsets survive oversized and irregular callbacks", "[parallel-processor][dsp][modulation][oversized]")
{
    for (bool cc : {false, true})
    {
        const auto reference = renderParallelModulation(128, cc);
        for (int size : {31, 17003})
        {
            const auto actual = renderParallelModulation(size, cc);
            const auto error = parallelError(actual, reference);
            CAPTURE(cc, size, error); CHECK(error < 3e-5f);
        }
    }
}

TEST_CASE("Rapid routing and HQ automation advances identically through host bypass", "[parallel-processor][dsp][host-bypass][topology]")
{
    FireAudioProcessor hidden, reference;
    configureTwoBranches(hidden, 1, false); configureTwoBranches(reference, 1, false);
    juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize); juce::MidiBuffer midi;
    const int latency = hidden.getLatencySamples();
    float error = 0, maximum = 0; double energy = 0;
    for (int block = 0; block < 144; ++block)
    {
        for (auto* p : {&hidden, &reference})
        {
            if (block >= 24 && block <= 68 && block % 4 == 0)
            {
                parallelSet(*p, pc::modeID(1), block == 44 ? 0 : 1);
                parallelSet(*p, pc::branchID(1, fire::module_order::firstInsert), (block / 4) % 2);
                parallelSet(*p, pc::branchID(1, fire::module_order::firstInsert + 1), 1 - (block / 4) % 2);
                parallelSet(*p, pc::blendID(1), block % 8 == 0 ? .2f : .8f);
                parallelSet(*p, HQ_ID, block >= 36 && block < 48 ? 1 : block >= 64 ? 1 : 0);
            }
        }
        parallelInput(a, block * blockSize); b.makeCopyOf(a, true);
        if (block >= 20 && block < 100) hidden.processBlockBypassed(a, midi); else hidden.processBlock(a, midi);
        reference.processBlock(b, midi);
        REQUIRE(std::isfinite(a.getMagnitude(0, blockSize)));
        maximum = std::max(maximum, a.getMagnitude(0, blockSize));
        CHECK(hidden.getLatencySamples() == latency);
        if (block >= 112)
        {
            error = std::max(error, parallelError(a, b));
            energy += a.getRMSLevel(0, 0, blockSize);
        }
    }
    CAPTURE(error, maximum, energy);
    CHECK(error < 3e-5f); CHECK(maximum < 2); CHECK(energy > .1);
}

TEST_CASE("A zero-weight parallel branch keeps its delay recording and tail warm", "[parallel-processor][dsp][tail]")
{
    FireAudioProcessor hidden, audible;
    for (auto* p : {&hidden, &audible})
    {
        parallelBase(*p);
        REQUIRE(p->addInsertEffect(1, fx::Type::delay) == 0);
        const std::array<float, 6> values {100, 80, 12000, 0, 0, 100};
        for (int control = 0; control < 6; ++control)
            parallelSet(*p, fx::parameterID(1, 0, control), fx::controls(fx::Type::delay)[static_cast<size_t>(control)].toNormalised(values[static_cast<size_t>(control)]));
        REQUIRE(p->setModuleBranch(1, fire::module_order::firstInsert, pc::b));
        REQUIRE(p->setChainRoutingMode(1, pc::parallel));
        parallelSet(*p, pc::blendID(1), p == &hidden ? 0 : 1);
        parallelPrepare(*p);
    }
    juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize); juce::MidiBuffer midi;
    float error = 0; double tail = 0;
    for (int block = 0; block < 144; ++block)
    {
        if (block == 64) parallelSet(hidden, pc::blendID(1), 1);
        parallelInput(a, block * blockSize, block >= 64); b.makeCopyOf(a, true);
        hidden.processBlock(a, midi); audible.processBlock(b, midi);
        if (block >= 100)
        {
            error = std::max(error, parallelError(a, b));
            tail += b.getRMSLevel(0, 0, blockSize);
        }
    }
    CAPTURE(error, tail); CHECK(error < 2e-6f); CHECK(tail > .1);
}

TEST_CASE("Parallel tail reporting sums each branch then takes its maximum before the next scope", "[parallel-processor][tail]")
{
    FireAudioProcessor p; parallelBase(p);
    REQUIRE(p.addInsertEffect(0, fx::Type::delay) == 0);
    REQUIRE(p.addInsertEffect(0, fx::Type::reverb) == 1);
    REQUIRE(p.addInsertEffect(0, fx::Type::chorus) == 2);
    CHECK(p.getTailLengthSeconds() == Catch::Approx(242)); // Serial 180 + 60 + 2.
    REQUIRE(p.setModuleBranch(0, fire::module_order::firstInsert + 1, pc::b));
    REQUIRE(p.setModuleBranch(0, fire::module_order::firstInsert + 2, pc::b));
    CHECK(p.getTailLengthSeconds() == Catch::Approx(242)); // Latent assignments do not change Serial.
    REQUIRE(p.setChainRoutingMode(0, pc::parallel));
    CHECK(p.getTailLengthSeconds() == Catch::Approx(180)); // max(180, 60 + 2).
    for (float blend : {0.0f, 1.0f})
    {
        parallelSet(p, pc::blendID(0), blend);
        CHECK(p.getTailLengthSeconds() == Catch::Approx(180)); // Zero-weight branches stay warm.
    }

    REQUIRE(p.addInsertEffect(1, fx::Type::delay) == 0);
    REQUIRE(p.addInsertEffect(1, fx::Type::reverb) == 1);
    CHECK(p.getTailLengthSeconds() == Catch::Approx(420)); // Band Serial240, then Master Parallel180.
    REQUIRE(p.setModuleBranch(1, fire::module_order::firstInsert + 1, pc::b));
    REQUIRE(p.setChainRoutingMode(1, pc::parallel));
    CHECK(p.getTailLengthSeconds() == Catch::Approx(360)); // Band max180 + Master max180.
    REQUIRE(p.setChainRoutingMode(0, pc::serial));
    CHECK(p.getTailLengthSeconds() == Catch::Approx(422)); // Master now Serial242.
    REQUIRE(p.setChainRoutingMode(1, pc::serial));
    CHECK(p.getTailLengthSeconds() == Catch::Approx(482));
}

TEST_CASE("Disabled or deleted native Shape leaves empty Parallel branches dry at low and high levels", "[parallel-processor][dsp][shape-bypass]")
{
    for (bool removeRows : {false, true})
    {
        FireAudioProcessor actual, reference;
        parallelBase(actual); parallelBase(reference);
        parallelSet(actual, "mode1", 3); // Must not leak the default Cubic into a disabled branch.
        if (removeRows) for (int node = 0; node < 5; ++node) actual.removeModule(1, node);
        REQUIRE(actual.setChainRoutingMode(1, pc::parallel));
        parallelSet(reference, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0), 0);
        parallelPrepare(actual); parallelPrepare(reference);
        juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize); juce::MidiBuffer midi;
        float error = 0;
        for (int block = 0; block < 80; ++block)
        {
            parallelInput(a, block * blockSize);
            a.applyGain(block < 40 ? .25f : 12.0f); // Above unity must not be secretly clipped either.
            b.makeCopyOf(a, true);
            actual.processBlock(a, midi); reference.processBlock(b, midi);
            error = std::max(error, parallelError(a, b));
        }
        CAPTURE(removeRows, error); CHECK(error < 2e-6f);
    }
}

namespace
{
BandProcessingParameters parallelShapeParameters(bool hq)
{
    BandProcessingParameters p;
    p.isBandEnabled = true; p.isHQ = hq; p.mode = 3;
    p.isDriveEnabled = p.isCompEnabled = p.isWidthEnabled = p.isDcFilterEnabled = p.isSafeModeOn = false;
    p.ott.enabled = false; p.isShapeEnabled = false;
    p.outputVal.baseValue = 0; p.outputVal.range = {-48, 6};
    p.mixVal = 1; p.mixValProvider.baseValue = 1;
    p.shapeMixVal = 1; p.shapeMixValProvider.baseValue = 1;
    p.driveVal.baseValue = 0; p.driveVal.range = {0, 100};
    p.biasVal.baseValue = 0; p.biasVal.range = {-1, 1};
    p.recVal.baseValue = 0; p.recVal.range = {0, 1};
    p.parallelRouting.mode = pc::parallel;
    p.parallelBlend.baseValue = .5f; p.parallelBlend.range = {0, 1};
    return p;
}
}

TEST_CASE("HQ disabled Shape retains its linear oversampler without an implicit clipping curve", "[parallel-processor][dsp][shape-bypass][hq]")
{
    BandProcessor low, high;
    low.prepare({48000, 64, 2}, false); high.prepare({48000, 64, 2}, false);
    auto p = parallelShapeParameters(true);
    juce::AudioBuffer<float> a(2, 64), b(2, 64), modulation;
    float error = 0;
    for (int block = 0; block < 192; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 64; ++sample)
            {
                const auto value = .08f * std::sin(static_cast<float>(block * 64 + sample) * .13f + channel * .4f);
                a.setSample(channel, sample, value); b.setSample(channel, sample, value * 20);
            }
        low.process(a, p, modulation); high.process(b, p, modulation);
        if (block < 64) continue;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 64; ++sample)
                error = std::max(error, std::abs(b.getSample(channel, sample) - 20 * a.getSample(channel, sample)));
    }
    CAPTURE(error); CHECK(error < 3e-5f);
}

TEST_CASE("Parallel native Shape power crossfades to identity without residual Bias or Rectification", "[parallel-processor][dsp][shape-bypass][transition]")
{
    for (bool hq : {false, true})
    {
        BandProcessor engine; engine.prepare({48000, 64, 2}, false);
        auto p = parallelShapeParameters(hq); p.isShapeEnabled = true;
        p.parallelBlend.baseValue = 0; p.biasVal.baseValue = .2f; p.recVal.baseValue = .35f;
        juce::AudioBuffer<float> audio(2, 64), modulation;
        std::array<float, 2> previous {};
        float largestStep = 0, settledError = 0;
        for (int block = 0; block < 128; ++block)
        {
            if (block == 64) p.isShapeEnabled = false;
            if (block == 96) p.isShapeEnabled = true;
            for (int sample = 0; sample < 64; ++sample)
            { audio.setSample(0, sample, .2f); audio.setSample(1, sample, -.3f); }
            engine.process(audio, p, modulation);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 64; ++sample)
                {
                    const auto value = audio.getSample(channel, sample);
                    REQUIRE(std::isfinite(value));
                    if (block >= 64) largestStep = std::max(largestStep, std::abs(value - previous[static_cast<size_t>(channel)]));
                    if (block >= 80 && block < 96)
                        settledError = std::max(settledError, std::abs(value - (channel == 0 ? .2f : -.3f)));
                    previous[static_cast<size_t>(channel)] = value;
                }
        }
        CAPTURE(hq, largestStep, settledError);
        CHECK(largestStep < .005f); CHECK(settledError < 2e-5f);
    }
}
