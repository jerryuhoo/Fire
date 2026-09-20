#include <PluginProcessor.h>
#include <DSP/EqProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
namespace eq = fire::eq;
void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

eq::Processor::Parameters nodeParameters(eq::Type type = eq::Type::bell)
{
    eq::Processor::Parameters result;
    result.enabled = true;
    result.state.present = true;
    result.state.type = type;
    result.state.frequency = 700.0f;
    result.state.gainDb = 9.0f;
    result.state.q = 1.3f;
    result.state.slope = 3;
    result.controls[0].baseValue = result.state.frequency;
    result.controls[0].range = {20.0f, 20000.0f};
    result.controls[0].range.setSkewForCentre(1000.0f);
    result.controls[1].baseValue = result.state.gainDb;
    result.controls[1].range = {-24.0f, 24.0f};
    result.controls[2].baseValue = result.state.q;
    result.controls[2].range = {0.1f, 18.0f};
    return result;
}

void configureIdentity(FireAudioProcessor& processor)
{
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, "multibandEnable1", 0);
    set(processor, "filterBypass", 1);
    set(processor, "lowcutBypassed", 1);
    set(processor, "peakBypassed", 1);
    set(processor, "highcutBypassed", 1);
}

std::vector<float> renderPartition(int blockSize)
{
    constexpr int total = 18000;
    constexpr double rate = 48000.0;
    eq::Processor engine;
    engine.prepare(rate);
    auto parameters = nodeParameters();
    juce::AudioBuffer<float> storage(2, blockSize);
    std::vector<float> lfo(static_cast<size_t>(blockSize));
    std::vector<float> output(static_cast<size_t>(total));
    const std::array<int, 9> events {4096, 5120, 5200, 5300, 8192, 9000, 11000, 14000, total};
    int position = 0;
    for (int next : events)
    {
        while (position < next)
        {
            const int count = std::min(blockSize, next - position);
            for (int sample = 0; sample < count; ++sample)
            {
                const double phase = juce::MathConstants<double>::twoPi * (position + sample) / rate;
                const auto value = static_cast<float>(0.12 * std::sin(113.0 * phase)
                                                       + 0.08 * std::cos(1301.0 * phase));
                storage.setSample(0, sample, value);
                storage.setSample(1, sample, -value);
                lfo[static_cast<size_t>(sample)] = static_cast<float>(0.5 + 0.5 * std::sin(7.0 * phase));
            }
            if (position >= 11000) parameters.controls[0].lfoSignal = lfo.data();
            engine.begin(3, parameters);
            auto block = juce::dsp::AudioBlock<float>(storage).getSubBlock(0, static_cast<size_t>(count));
            engine.process(3, block, 0, count);
            for (int sample = 0; sample < count; ++sample)
            {
                const auto value = storage.getSample(0, sample);
                REQUIRE(std::isfinite(value));
                CHECK(storage.getSample(1, sample) == -value);
                output[static_cast<size_t>(position + sample)] = value;
            }
            position += count;
        }
        if (next == 4096) parameters.controls[1].baseValue = -12.0f;
        if (next == 5120) parameters.state.type = eq::Type::lowCut;
        if (next == 5200) parameters.state.type = eq::Type::highShelf;
        if (next == 5300) parameters.state.type = eq::Type::bell;
        if (next == 8192) parameters.enabled = false;
        if (next == 9000) { parameters.enabled = true; parameters.state.type = eq::Type::notch; parameters.controls[0].baseValue = 2100; }
        if (next == 11000) { parameters.sources[0] = 1; parameters.controls[0].modulationDepth = 0.2f; }
        if (next == 14000) { parameters.sources[0] = 2; parameters.controls[0].modulationDepth = 0.35f; parameters.controls[0].isBipolar = false; }
    }
    return output;
}

juce::XmlElement captureHost(FireAudioProcessor& processor)
{
    juce::MemoryBlock data;
    processor.getStateInformation(data);
    auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
    REQUIRE(xml != nullptr);
    return *xml;
}
void restoreHost(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary(xml, data);
    processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
}
} // namespace

TEST_CASE("Dynamic EQ shares its actual static response with the display at every filter type",
          "[dynamic-eq][eq-dsp][filter]")
{
    for (double rate : {8000.0, 48000.0})
        for (int type = 0; type < 7; ++type)
        {
            eq::Processor engine;
            engine.prepare(rate);
            const auto parameters = nodeParameters(static_cast<eq::Type>(type));
            engine.begin(3, parameters);
            constexpr int blockSize = 100;
            juce::AudioBuffer<float> audio(1, blockSize);
            double inputEnergy = 0.0, outputEnergy = 0.0;
            for (int offset = 0; offset < static_cast<int>(rate * 0.4); offset += blockSize)
            {
                const int count = std::min(blockSize, static_cast<int>(rate * 0.4) - offset);
                for (int sample = 0; sample < count; ++sample)
                {
                    const auto value = static_cast<float>(0.1 * std::sin(juce::MathConstants<double>::twoPi
                                                                 * 1000.0 * (offset + sample) / rate));
                    audio.setSample(0, sample, value);
                    if (offset + sample >= static_cast<int>(rate * 0.2)) inputEnergy += value * value;
                }
                auto block = juce::dsp::AudioBlock<float>(audio).getSubBlock(0, static_cast<size_t>(count));
                engine.process(3, block, 0, count);
                for (int sample = 0; sample < count; ++sample)
                    if (offset + sample >= static_cast<int>(rate * 0.2))
                    {
                        const auto value = audio.getSample(0, sample);
                        outputEnergy += value * value;
                    }
            }
            const auto expected = eq::makeCoefficients(parameters.state, rate).magnitudeAt(1000.0, rate);
            CAPTURE(rate, type, expected);
            CHECK(std::sqrt(outputEnergy / inputEnergy) == Catch::Approx(expected).epsilon(0.0002));
        }
}

TEST_CASE("Dynamic EQ edits and LFO routes keep one timeline across host block partitions",
          "[dynamic-eq][eq-dsp][filter][modulation]")
{
    const auto reference = renderPartition(512);
    for (int block : {1, 37, 257})
    {
        const auto output = renderPartition(block);
        float difference = 0.0f;
        for (size_t sample = 0; sample < output.size(); ++sample)
            difference = std::max(difference, std::abs(output[sample] - reference[sample]));
        CAPTURE(block, difference);
        CHECK(difference < 2.0e-6f);
    }
}

TEST_CASE("Dynamic EQ remains finite at parameter extremes and below the display Nyquist range",
          "[dynamic-eq][eq-dsp][filter][boundary]")
{
    for (double rate : {8000.0, 16000.0, 44100.0, 192000.0})
        for (int type = 0; type < 7; ++type)
        {
            eq::Processor engine;
            engine.prepare(rate);
            auto parameters = nodeParameters(static_cast<eq::Type>(type));
            parameters.controls[0].baseValue = 20000;
            parameters.controls[1].baseValue = 24;
            parameters.controls[2].baseValue = 18;
            juce::AudioBuffer<float> audio(2, 256);
            bool finite = true;
            for (int blockIndex = 0; blockIndex < 12; ++blockIndex)
            {
                if (blockIndex == 4) parameters.controls[0].baseValue = 20;
                if (blockIndex == 8)
                {
                    parameters.controls[0].baseValue = std::numeric_limits<float>::infinity();
                    parameters.controls[1].baseValue = std::numeric_limits<float>::quiet_NaN();
                    parameters.controls[2].baseValue = -99;
                }
                engine.begin(3, parameters);
                for (int sample = 0; sample < 256; ++sample)
                {
                    audio.setSample(0, sample, sample == 0 ? 0.1f : 0.0f);
                    audio.setSample(1, sample, static_cast<float>(0.1 * std::sin(sample * 0.31)));
                }
                auto block = juce::dsp::AudioBlock<float>(audio);
                engine.process(3, block, 0, 256);
                for (int sample = 0; sample < 256; ++sample)
                    for (int channel = 0; channel < 2; ++channel)
                        finite = finite && std::isfinite(audio.getSample(channel, sample));
            }
            CAPTURE(rate, type);
            CHECK(finite);
            engine.reset();
            engine.begin(3, nodeParameters());
            audio.clear();
            auto block = juce::dsp::AudioBlock<float>(audio);
            engine.process(3, block, 0, 256);
            CHECK(audio.getMagnitude(0, 256) == 0.0f);
        }
}

TEST_CASE("Dynamic EQ bypass reaches exact dry and type changes preserve the switching sample",
          "[dynamic-eq][eq-dsp][filter][transition]")
{
    eq::Processor subject, held;
    subject.prepare(48000); held.prepare(48000);
    auto parameters = nodeParameters();
    subject.begin(3, parameters); held.begin(3, parameters);
    juce::AudioBuffer<float> a(1, 2048), b(1, 2048);
    for (int index = 0; index < 2048; ++index)
    {
        const auto value = static_cast<float>(0.1 * std::cos(index * 0.13));
        a.setSample(0, index, value); b.setSample(0, index, value);
    }
    auto blockA = juce::dsp::AudioBlock<float>(a), blockB = juce::dsp::AudioBlock<float>(b);
    subject.process(3, blockA, 0, 2048); held.process(3, blockB, 0, 2048);
    parameters.state.type = eq::Type::highCut;
    subject.begin(3, parameters);
    a.setSample(0, 0, 0.19f); b.setSample(0, 0, 0.19f);
    subject.process(3, blockA, 0, 1); held.process(3, blockB, 0, 1);
    CHECK(a.getSample(0, 0) == b.getSample(0, 0));
    parameters.enabled = false;
    subject.begin(3, parameters);
    for (int index = 0; index < 2048; ++index) a.setSample(0, index, 0.125f);
    subject.process(3, blockA, 0, 2048);
    for (int index = 961; index < 2048; ++index) CHECK(a.getSample(0, index) == 0.125f);
}

TEST_CASE("Dynamic EQ fixed slots retain legacy IDs and can all be removed and reused",
          "[dynamic-eq][eq-state][filter]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    CHECK(eq::parameterID(0, eq::Field::frequency) == "lowcutFreq");
    CHECK(eq::parameterID(1, eq::Field::gain) == "peakGain");
    CHECK(eq::parameterID(2, eq::Field::slope) == "highcutSlope");
    CHECK(eq::appendedParameterIDs().size() == 70);
    for (int slot = 3; slot < 12; ++slot)
    {
        REQUIRE(processor.addEqNode(300.0f + slot * 250.0f, 3.0f) == slot);
        CHECK(processor.getEqNodeState(slot).present);
        CHECK(processor.getEqNodeState(slot).type == eq::Type::bell);
    }
    CHECK(processor.addEqNode(1000, 6) == -1);
    CHECK(processor.treeState.getRawParameterValue("filterBypass")->load() == 1.0f);
    REQUIRE(processor.assignLfoToTarget(0, "eqNode5Freq") == LfoManager::AssignmentResult::changed);
    for (int slot = 0; slot < 12; ++slot) REQUIRE(processor.removeEqNode(slot));
    CHECK_FALSE(processor.getModulationInfoForParameter("eqNode5Freq").isModulated);
    CHECK(processor.addEqNode(1250, -4, eq::Type::notch) == 0);
    CHECK(processor.getEqNodeState(0).type == eq::Type::notch);
    CHECK(processor.getEqNodeState(0).frequency == Catch::Approx(1250.0f));
    CHECK_FALSE(processor.removeEqNode(12));
    CHECK(processor.addEqNode(std::numeric_limits<float>::quiet_NaN(), 0) == -1);
}

TEST_CASE("Dynamic EQ host state restores new nodes and rejects a truncated extension transactionally",
          "[dynamic-eq][eq-state][filter]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    REQUIRE(processor.addEqNode(2345, 7, eq::Type::highShelf) == 3);
    REQUIRE(processor.removeEqNode(0));
    const auto saved = captureHost(processor);
    CHECK(saved.getIntAttribute("eqSchemaVersion") == 1);
    REQUIRE(processor.removeEqNode(3));
    restoreHost(processor, saved);
    CHECK_FALSE(processor.getEqNodeState(0).present);
    CHECK(processor.getEqNodeState(3).present);
    CHECK(processor.getEqNodeState(3).type == eq::Type::highShelf);
    CHECK(processor.getEqNodeState(3).frequency == Catch::Approx(2345.0f));

    auto truncated = saved;
    auto* parameters = truncated.getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    for (auto* child : parameters->getChildIterator())
        if (child->getStringAttribute("id") == "eqNode4Gain") { parameters->removeChildElement(child, true); break; }
    truncated.setAttribute("savedParameterCount", parameters->getNumChildElements());
    set(processor, "eqNode4Gain", -6.0f);
    restoreHost(processor, truncated);
    CHECK(processor.getEqNodeState(3).gainDb == Catch::Approx(-6.0f));

    auto legacy = saved;
    legacy.removeAttribute("eqSchemaVersion");
    parameters = legacy.getChildByName("PARAMETERS");
    for (int index = parameters->getNumChildElements(); --index >= 0;)
        if (eq::isAppendedParameterID(parameters->getChildElement(index)->getStringAttribute("id")))
            parameters->removeChildElement(parameters->getChildElement(index), true);
    legacy.setAttribute("savedParameterCount", parameters->getNumChildElements());
    restoreHost(processor, legacy);
    for (int slot = 0; slot < 12; ++slot)
    {
        CAPTURE(slot);
        CHECK(processor.getEqNodeState(slot).present == (slot < 3));
        CHECK(processor.getEqNodeState(slot).type == eq::defaultType(slot));
    }
}

TEST_CASE("Adding an EQ node during callback capture publishes only complete audible states",
          "[dynamic-eq][eq-state][filter][transaction]")
{
    FireAudioProcessor subject, reference;
    configureIdentity(subject); configureIdentity(reference);
    subject.prepareToPlay(48000, 128); reference.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> a(2, 128), b(2, 128);
    juce::MidiBuffer midi;
    int position = 0;
    auto fill = [&]
    {
        for (int sample = 0; sample < 128; ++sample)
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto value = static_cast<float>(0.1 * std::sin((position + sample) * juce::MathConstants<double>::twoPi / 48.0));
                a.setSample(channel, sample, value); b.setSample(channel, sample, value);
            }
        position += 128;
    };
    for (int block = 0; block < 12; ++block)
    { fill(); subject.processBlock(a, midi); reference.processBlock(b, midi); }
    subject.setAudioCallbackStateCaptureHookForTesting([&] { REQUIRE(subject.addEqNode(1000, 12) == 3); });
    fill(); subject.processBlock(a, midi); reference.processBlock(b, midi);
    for (int sample = 0; sample < 128; ++sample) CHECK(a.getSample(0, sample) == b.getSample(0, sample));
    float audibleDifference = 0.0f;
    for (int block = 0; block < 20; ++block)
    {
        fill(); subject.processBlock(a, midi); reference.processBlock(b, midi);
        for (int sample = 0; sample < 128; ++sample)
            audibleDifference = std::max(audibleDifference, std::abs(a.getSample(0, sample) - b.getSample(0, sample)));
    }
    CHECK(audibleDifference > 0.1f);
}

TEST_CASE("Master EQ edits preserve band Delay tails and captured Clouds Freeze audio",
          "[dynamic-eq][eq-state][filter][tail][clouds]")
{
    for (const auto type : {fire::effects::Type::delay, fire::effects::Type::granular})
    {
        FireAudioProcessor subject, reference;
        for (auto* processor : {&subject, &reference})
        {
            configureIdentity(*processor);
            set(*processor, "multibandEnable1", 1);
            set(*processor, "mode1", 4);
            set(*processor, "driveBypass1", 0);
            set(*processor, "linked1", 0);
            REQUIRE(processor->addInsertEffect(1, type) == 0);
            const auto control = [&](int index, float value)
            {
                const auto normalised = fire::effects::controls(type)[static_cast<size_t>(index)].range().convertTo0to1(value);
                set(*processor, fire::effects::parameterID(1, 0, index), normalised);
            };
            control(5, 100);
            if (type == fire::effects::Type::delay)
            { control(0, 120); control(1, 70); control(3, 0); }
            else
            { control(0, 60); control(1, -50); control(2, 0); control(3, 15); }
            processor->prepareToPlay(48000, 128);
        }
        juce::AudioBuffer<float> a(2, 128), b(2, 128);
        juce::MidiBuffer midi;
        int position = 0;
        float difference = 0.0f;
        double tailEnergy = 0.0;
        const auto process = [&](bool silence, bool compare)
        {
            for (int sample = 0; sample < 128; ++sample)
                for (int channel = 0; channel < 2; ++channel)
                {
                    const auto value = silence ? 0.0f : static_cast<float>(
                        0.1 * std::sin((position + sample) * juce::MathConstants<double>::twoPi * 223.0 / 48000.0 + channel * 0.3)
                        + 0.03 * std::cos((position + sample) * juce::MathConstants<double>::twoPi * 997.0 / 48000.0));
                    a.setSample(channel, sample, value); b.setSample(channel, sample, value);
                }
            subject.processBlock(a, midi); reference.processBlock(b, midi);
            if (compare)
                for (int sample = 0; sample < 128; ++sample)
                {
                    difference = std::max(difference, std::abs(a.getSample(0, sample) - b.getSample(0, sample)));
                    tailEnergy += static_cast<double>(b.getSample(0, sample)) * b.getSample(0, sample);
                }
            position += 128;
        };
        for (int block = 0; block < 256; ++block) process(false, false);
        if (type == fire::effects::Type::granular)
            for (auto* processor : {&subject, &reference})
                set(*processor, fire::clouds_params::parameterID(1, 0, fire::clouds_params::freezeField), 1);
        for (int block = 0; block < 32; ++block) process(false, false);
        // A zero-gain bell is audible identity: any difference here would be
        // an unrelated topology fade/reset, not the requested EQ response.
        REQUIRE(subject.addEqNode(1300, 0) == 3);
        for (int block = 0; block < 40; ++block) process(true, true);
        REQUIRE(subject.removeEqNode(3));
        for (int block = 0; block < 40; ++block) process(true, true);
        CAPTURE(static_cast<int>(type), difference, tailEnergy);
        REQUIRE(tailEnergy > 0.0001);
        CHECK(difference < 1.0e-6f);
    }
}

TEST_CASE("Quickly reused EQ slots fade into new recursive history even at the same type",
          "[dynamic-eq][eq-dsp][filter][lifecycle]")
{
    eq::Processor subject, unchangedGeneration;
    subject.prepare(48000); unchangedGeneration.prepare(48000);
    auto parameters = nodeParameters();
    parameters.controls[0].baseValue = 120;
    parameters.controls[1].baseValue = 24;
    parameters.controls[2].baseValue = 18;
    subject.begin(3, parameters); unchangedGeneration.begin(3, parameters);
    juce::AudioBuffer<float> a(1, 2048), b(1, 2048);
    a.clear(); b.clear(); a.setSample(0, 0, 0.2f); b.setSample(0, 0, 0.2f);
    auto blockA = juce::dsp::AudioBlock<float>(a), blockB = juce::dsp::AudioBlock<float>(b);
    subject.process(3, blockA, 0, 200); unchangedGeneration.process(3, blockB, 0, 200);
    parameters.enabled = false;
    subject.begin(3, parameters); unchangedGeneration.begin(3, parameters);
    a.clear(); b.clear();
    blockA = juce::dsp::AudioBlock<float>(a); blockB = juce::dsp::AudioBlock<float>(b);
    subject.process(3, blockA, 0, 8); unchangedGeneration.process(3, blockB, 0, 8);
    parameters.enabled = true;
    unchangedGeneration.begin(3, parameters);
    ++parameters.generation;
    subject.begin(3, parameters);
    a.clear(); b.clear();
    // Reacquire writable views after clear(): AudioBuffer's cached isClear
    // flag cannot observe writes through a previously constructed AudioBlock.
    blockA = juce::dsp::AudioBlock<float>(a); blockB = juce::dsp::AudioBlock<float>(b);
    subject.process(3, blockA, 0, 2048); unchangedGeneration.process(3, blockB, 0, 2048);
    CHECK(a.getSample(0, 0) == b.getSample(0, 0));
    CHECK(a.getMagnitude(1000, 1048) == 0.0f);
    CHECK(b.getMagnitude(1000, 1048) > 1.0e-5f);
}

TEST_CASE("Retyping deleting and restoring each legacy EQ slot leaves no hidden legacy filtering",
          "[dynamic-eq][eq-dsp][filter][legacy]")
{
    for (int slot = 0; slot < 3; ++slot)
    {
        FireAudioProcessor processor;
        configureIdentity(processor);
        set(processor, eq::parameterID(slot, eq::Field::frequency), 700);
        set(processor, eq::parameterID(slot, eq::Field::gain), 9);
        set(processor, eq::parameterID(slot, eq::Field::q), 1.3f);
        set(processor, eq::parameterID(slot, eq::Field::slope), 3);
        set(processor, eq::parameterID(slot, eq::Field::bypassed), 0);
        processor.prepareToPlay(48000, 240);
        juce::AudioBuffer<float> audio(2, 240);
        juce::MidiBuffer midi;
        int position = 0;
        const auto measure = [&]
        {
            double inputEnergy = 0.0, outputEnergy = 0.0;
            for (int block = 0; block < 40; ++block)
            {
                for (int sample = 0; sample < 240; ++sample)
                {
                    const auto value = static_cast<float>(0.1 * std::sin((position + sample)
                        * juce::MathConstants<double>::twoPi / 48.0));
                    audio.setSample(0, sample, value); audio.setSample(1, sample, -value);
                    if (block >= 20) inputEnergy += static_cast<double>(value) * value;
                }
                processor.processBlock(audio, midi);
                if (block >= 20)
                    for (int sample = 0; sample < 240; ++sample)
                        outputEnergy += static_cast<double>(audio.getSample(0, sample)) * audio.getSample(0, sample);
                position += 240;
            }
            const auto state = processor.getEqNodeState(slot);
            const auto expected = eq::makeCoefficients(state, 48000).magnitudeAt(1000, 48000);
            const auto actual = std::sqrt(outputEnergy / inputEnergy);
            CAPTURE(slot, static_cast<int>(state.type), state.present, expected, actual);
            CHECK(actual == Catch::Approx(expected).epsilon(0.0015));
        };
        measure();
        set(processor, eq::parameterID(slot, eq::Field::type), static_cast<float>(eq::Type::highShelf));
        measure();
        REQUIRE(processor.removeEqNode(slot));
        measure();
        set(processor, eq::parameterID(slot, eq::Field::type), static_cast<float>(eq::defaultType(slot)));
        set(processor, eq::parameterID(slot, eq::Field::present), 1);
        measure();
    }
}
