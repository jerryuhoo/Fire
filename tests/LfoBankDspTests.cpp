#include <PluginProcessor.h>
#include <Utility/LfoBankParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
namespace bank = fire::lfo_bank;
void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
LfoData triangle()
{
    LfoData shape;
    shape.points = {{0, 0}, {0.5f, 1}, {1, 0}};
    shape.curvatures = {0, 0};
    return shape;
}
void configureLfo(FireAudioProcessor& processor, int source, float rate, float phase = 0.0f)
{
    set(processor, bank::presentParameterID(source), 1);
    set(processor, bank::parameterID(source, bank::Field::syncMode), 0);
    set(processor, bank::parameterID(source, bank::Field::rateHz), rate);
    set(processor, bank::parameterID(source, bank::Field::phase), phase);
    processor.getLfoManager().setLfoData(source, triangle());
}
void neutral(FireAudioProcessor& processor)
{
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, "mode1", 4);
    set(processor, "driveBypass1", 0);
    set(processor, "linked1", 0);
    set(processor, "filterBypass", 0);
    set(processor, "downsampleBypass", 0);
}
juce::XmlElement hostState(FireAudioProcessor& processor)
{
    juce::MemoryBlock bytes;
    processor.getStateInformation(bytes);
    auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(xml != nullptr);
    return *xml;
}
void restore(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock bytes;
    juce::AudioProcessor::copyXmlToBinary(xml, bytes);
    processor.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
}
} // namespace

TEST_CASE("LFO presence identifier parsing accepts only the canonical stable slots",
          "[lfo-bank][dsp][parameters]")
{
    for (int source = 0; source < bank::capacity; ++source)
        CHECK(bank::isPresentParameterID(bank::presentParameterID(source)));
    for (const auto* invalid : {"", "lfo", "lfoPresent", "lfoPresent0", "lfoPresent01",
                                "lfoPresent17", "lfoPresent999999", "lfoPresent1x", "xlfoPresent1"})
        CHECK_FALSE(bank::isPresentParameterID(invalid));
}

TEST_CASE("LFO slots add and delete without moving identities and reset only the reused source",
          "[lfo-bank][dsp][state][lifecycle]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    REQUIRE(processor.getLfoManager().getLfoDataCopy().size() == bank::capacity);
    for (int source = 0; source < bank::capacity; ++source)
        CHECK(processor.isLfoPresent(source) == (source < bank::defaultCount));
    for (int source = bank::defaultCount; source < bank::capacity; ++source)
        REQUIRE(processor.addLfo() == source);
    CHECK(processor.addLfo() == -1);
    configureLfo(processor, 2, 2.3f, 0.37f);
    configureLfo(processor, 10, 7.4f, 0.61f);
    set(processor, "lfoSmooth11", 0.7f);
    REQUIRE(processor.assignLfoToTarget(10, "drive1") == LfoManager::AssignmentResult::changed);
    REQUIRE(processor.assignLfoToTarget(2, "pan1") == LfoManager::AssignmentResult::changed);
    const auto untouched = processor.getLfoManager().getLfoDataSnapshot(2);
    const auto previousRevision = processor.getLfoManager().getModulationRoutingRevision();
    REQUIRE(processor.removeLfo(10));
    CHECK_FALSE(processor.isLfoPresent(10));
    CHECK_FALSE(processor.removeLfo(10));
    CHECK_FALSE(processor.getModulationInfoForParameter("drive1").isModulated);
    CHECK(processor.getModulationInfoForParameter("pan1").sourceLfoIndex == 3);
    CHECK(processor.getLfoManager().getModulationRoutingRevision() > previousRevision);
    CHECK(processor.getLfoManager().getLfoDataSnapshot(2).revision == untouched.revision);
    CHECK(processor.getLfoManager().getLfoDataSnapshot(2).data.points == untouched.data.points);
    CHECK(processor.treeState.getRawParameterValue("lfoRateHz3")->load() == Catch::Approx(2.3f));
    const auto cleared = processor.getLfoManager().getLfoDataSnapshot(10);
    CHECK(cleared.data.points == LfoData {}.points);
    for (int field = 0; field < bank::timingFieldCount; ++field)
    {
        const auto* parameter = processor.treeState.getParameter(bank::parameterID(10, static_cast<bank::Field>(field)));
        REQUIRE(parameter != nullptr);
        CHECK(parameter->getValue() == Catch::Approx(parameter->getDefaultValue()));
    }
    const auto removedRevision = processor.getLfoManager().getModulationRoutingRevision();
    CHECK(processor.addLfo() == 10);
    CHECK(processor.getLfoManager().getModulationRoutingRevision() > removedRevision);
    CHECK_FALSE(processor.getModulationInfoForParameter("drive1").isModulated);
    for (int source = 0; source < bank::capacity; ++source) REQUIRE(processor.removeLfo(source));
    for (int source = 0; source < bank::capacity; ++source) CHECK_FALSE(processor.isLfoPresent(source));
    CHECK(processor.addLfo() == 0);
    CHECK_FALSE(processor.isLfoPresent(-1));
    CHECK_FALSE(processor.removeLfo(bank::capacity));
}

TEST_CASE("The sixteenth LFO renders while absent slots remain dormant and the original four stay identical",
          "[lfo-bank][dsp][render]")
{
    FireAudioProcessor subject, reference;
    for (int source = 0; source < bank::defaultCount; ++source)
        for (auto* processor : {&subject, &reference})
            configureLfo(*processor, source, 0.7f + source * 0.2f, source * 0.1f);
    configureLfo(subject, 15, 3.0f, 0.25f);
    // A shape/rate can exist in a saved dormant slot, but it must not advance.
    set(subject, "lfoRateHz8", 30.0f);
    subject.getLfoManager().setLfoData(7, triangle());
    subject.getLfoManager().prepare({48000, 128, 1});
    reference.getLfoManager().prepare({48000, 128, 1});
    juce::AudioBuffer<float> a(bank::capacity, 513), b(bank::capacity, 513);
    float difference = 0.0f, newPeak = 0.0f, dormantPeak = 0.0f;
    for (int count : {1, 37, 128, 513, 17, 513})
    {
        subject.getLfoManager().processBlock(a, 48000, nullptr, count);
        reference.getLfoManager().processBlock(b, 48000, nullptr, count);
        for (int sample = 0; sample < count; ++sample)
        {
            for (int source = 0; source < bank::defaultCount; ++source)
                difference = std::max(difference, std::abs(a.getSample(source, sample) - b.getSample(source, sample)));
            newPeak = std::max(newPeak, a.getSample(15, sample));
            for (int source = 4; source < 15; ++source)
                dormantPeak = std::max(dormantPeak, std::abs(a.getSample(source, sample)));
        }
    }
    CHECK(difference == 0.0f);
    CHECK(newPeak > 0.5f);
    CHECK(dormantPeak == 0.0f);
    CHECK(subject.getLfoManager().getLfoPhase(7) == 0.0f);
    CHECK(subject.getLfoManager().getLfoOutput(7) == 0.0f);
    CHECK(subject.getLfoManager().getLfoPhase(15) > 0.25f);
}

TEST_CASE("Highest LFO slot reaches the actual band Drive in base and HQ modes",
          "[lfo-bank][dsp][processor][hq]")
{
    for (bool hq : {false, true})
    {
        FireAudioProcessor subject, reference;
        for (auto* processor : {&subject, &reference})
        {
            neutral(*processor);
            set(*processor, "hq", hq ? 1.0f : 0.0f);
            set(*processor, "mode1", 2);
            set(*processor, "driveBypass1", 1);
            set(*processor, "drive1", 25);
            set(*processor, "safe1", 0);
        }
        configureLfo(subject, 15, 3.27f, 0.13f);
        configureLfo(reference, 0, 3.27f, 0.13f);
        REQUIRE(subject.assignLfoToTarget(15, "drive1") == LfoManager::AssignmentResult::changed);
        REQUIRE(reference.assignLfoToTarget(0, "drive1") == LfoManager::AssignmentResult::changed);
        subject.setModulationDepth("drive1", 0.35f); reference.setModulationDepth("drive1", 0.35f);
        subject.prepareToPlay(48000, 128); reference.prepareToPlay(48000, 128);
        juce::AudioBuffer<float> a(2, 128), b(2, 128);
        juce::MidiBuffer midi;
        float difference = 0.0f, peak = 0.0f;
        for (int block = 0; block < 160; ++block)
        {
            for (int sample = 0; sample < 128; ++sample)
                for (int channel = 0; channel < 2; ++channel)
                {
                    const double phase = (block * 128 + sample) * juce::MathConstants<double>::twoPi / 48000.0;
                    const auto value = static_cast<float>(0.1 * std::sin(223 * phase + channel * 0.3) + 0.02 * std::cos(997 * phase));
                    a.setSample(channel, sample, value); b.setSample(channel, sample, value);
                }
            subject.processBlock(a, midi); reference.processBlock(b, midi);
            for (int sample = 0; sample < 128; ++sample)
            {
                difference = std::max(difference, std::abs(a.getSample(0, sample) - b.getSample(0, sample)));
                peak = std::max(peak, std::abs(a.getSample(0, sample)));
            }
        }
        CAPTURE(hq, difference, peak);
        CHECK(difference < 2.0e-6f);
        CHECK(peak > 0.05f);
    }
}

TEST_CASE("Deleting an assigned LFO smoothly returns its target to the base value",
          "[lfo-bank][dsp][processor][transition]")
{
    FireAudioProcessor subject, held;
    for (auto* processor : {&subject, &held})
    {
        neutral(*processor);
        set(*processor, "widthBypass1", 1);
        configureLfo(*processor, 15, 1.0f);
        LfoData constant;
        constant.points = {{0, 1}, {1, 1}};
        processor->getLfoManager().setLfoData(15, constant);
        REQUIRE(processor->assignLfoToTarget(15, "width1") == LfoManager::AssignmentResult::changed);
        processor->setModulationDepth("width1", 0.6f);
        processor->prepareToPlay(48000, 128);
    }
    juce::AudioBuffer<float> a(2, 128), b(2, 128);
    juce::MidiBuffer midi;
    const auto fill = [&]
    {
        for (auto* buffer : {&a, &b})
        {
            juce::FloatVectorOperations::fill(buffer->getWritePointer(0), 0.1f, 128);
            juce::FloatVectorOperations::fill(buffer->getWritePointer(1), 0.04f, 128);
        }
    };
    for (int block = 0; block < 32; ++block) { fill(); subject.processBlock(a, midi); held.processBlock(b, midi); }
    REQUIRE(subject.removeLfo(15));
    fill(); subject.processBlock(a, midi); held.processBlock(b, midi);
    for (int sample = 0; sample <= subject.getLatencySamples(); ++sample)
        CHECK(a.getSample(0, sample) == Catch::Approx(b.getSample(0, sample)).margin(2.0e-6));
    for (int block = 0; block < 12; ++block) { fill(); subject.processBlock(a, midi); }
    CHECK(a.getSample(0, 127) == Catch::Approx(0.1f).margin(2.0e-5));
    CHECK(a.getSample(1, 127) == Catch::Approx(0.04f).margin(2.0e-5));
    CHECK_FALSE(subject.getModulationInfoForParameter("width1").isModulated);
}

TEST_CASE("Reusing an LFO source and its identical route between callbacks keeps the switching sample continuous",
          "[lfo-bank][dsp][processor][transition][reuse]")
{
    FireAudioProcessor processor;
    neutral(processor);
    set(processor, "widthBypass1", 1);
    REQUIRE(processor.addLfo() == 4);
    LfoData high;
    high.points = {{0, 1}, {1, 1}};
    processor.getLfoManager().setLfoData(4, high);
    REQUIRE(processor.assignLfoToTarget(4, "width1") == LfoManager::AssignmentResult::changed);
    processor.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    const auto render = [&]
    {
        juce::FloatVectorOperations::fill(audio.getWritePointer(0), 0.1f, 128);
        juce::FloatVectorOperations::fill(audio.getWritePointer(1), 0.04f, 128);
        processor.processBlock(audio, midi);
    };
    for (int block = 0; block < 32; ++block) render();
    auto previous = audio.getSample(0, 127);
    CHECK(previous == Catch::Approx(0.08f).margin(2.0e-5));
    REQUIRE(processor.removeLfo(4));
    REQUIRE(processor.addLfo() == 4);
    REQUIRE(processor.assignLfoToTarget(4, "width1") == LfoManager::AssignmentResult::changed);
    render();
    float maximumJump = 0.0f;
    for (int sample = 0; sample < 128; ++sample)
    {
        const auto value = audio.getSample(0, sample);
        maximumJump = std::max(maximumJump, std::abs(value - previous));
        previous = value;
    }
    CAPTURE(maximumJump);
    CHECK(maximumJump < 0.001f);
    for (int block = 0; block < 12; ++block) render();
    CHECK(audio.getSample(0, 127) == Catch::Approx(0.12f).margin(2.0e-5));
}

TEST_CASE("LFO bank host state preserves slot sixteen and migrates the original four without latent sources",
          "[lfo-bank][state][host]")
{
    FireAudioProcessor source, restored;
    source.hasUpdateCheckBeenPerformed = restored.hasUpdateCheckBeenPerformed = true;
    configureLfo(source, 15, 7.3f, 0.42f);
    REQUIRE(source.removeLfo(0));
    REQUIRE(source.assignLfoToTarget(15, "drive1") == LfoManager::AssignmentResult::changed);
    const auto saved = hostState(source);
    CHECK(saved.getIntAttribute("lfoBankSchemaVersion") == 1);
    REQUIRE(saved.getChildByName("LFO_STATE")->getNumChildElements() == bank::capacity);
    restore(restored, saved);
    CHECK_FALSE(restored.isLfoPresent(0));
    CHECK(restored.isLfoPresent(15));
    CHECK(restored.getLfoManager().getLfoDataSnapshot(15).data.points == triangle().points);
    CHECK(restored.treeState.getRawParameterValue("lfoRateHz16")->load() == Catch::Approx(7.3f));
    CHECK(restored.getModulationInfoForParameter("drive1").sourceLfoIndex == 16);
    CHECK(restored.getLfoManager().getModulationRoutingsCopy().getReference(0).sourceLfoIndex == 15);

    auto legacy = saved;
    legacy.removeAttribute("lfoBankSchemaVersion");
    auto* parameters = legacy.getChildByName("PARAMETERS");
    REQUIRE(parameters != nullptr);
    for (int index = parameters->getNumChildElements(); --index >= 0;)
        if (bank::isAppendedParameterID(parameters->getChildElement(index)->getStringAttribute("id")))
            parameters->removeChildElement(parameters->getChildElement(index), true);
    legacy.setAttribute("savedParameterCount", parameters->getNumChildElements());
    auto* shapes = legacy.getChildByName("LFO_STATE");
    for (int index = shapes->getNumChildElements(); --index >= bank::defaultCount;)
        shapes->removeChildElement(shapes->getChildElement(index), true);
    legacy.getChildByName("MODULATION_STATE")->deleteAllChildElements();
    restore(restored, legacy);
    for (int index = 0; index < bank::capacity; ++index)
    {
        CAPTURE(index);
        CHECK(restored.isLfoPresent(index) == bank::defaultPresent(index));
        if (index >= bank::defaultCount)
            CHECK(restored.getLfoManager().getLfoDataSnapshot(index).data.points == LfoData {}.points);
    }
    CHECK(restored.getLfoManager().getModulationRoutingsCopy().isEmpty());
}

TEST_CASE("Incomplete LFO bank host extensions and out of range sources are rejected before mutation",
          "[lfo-bank][state][host][corrupt]")
{
    FireAudioProcessor source;
    source.hasUpdateCheckBeenPerformed = true;
    configureLfo(source, 15, 3);
    REQUIRE(source.assignLfoToTarget(15, "drive1") == LfoManager::AssignmentResult::changed);
    const auto saved = hostState(source);
    for (int variant = 0; variant < 3; ++variant)
    {
        FireAudioProcessor restored;
        restored.hasUpdateCheckBeenPerformed = true;
        set(restored, "drive1", 77);
        auto invalid = saved;
        if (variant == 0)
        {
            auto* parameters = invalid.getChildByName("PARAMETERS");
            for (auto* parameter : parameters->getChildIterator())
                if (parameter->getStringAttribute("id") == "lfoPhase16")
                { parameters->removeChildElement(parameter, true); break; }
            invalid.setAttribute("savedParameterCount", parameters->getNumChildElements());
        }
        if (variant == 1)
        {
            auto* shapes = invalid.getChildByName("LFO_STATE");
            shapes->removeChildElement(shapes->getChildElement(15), true);
        }
        if (variant == 2)
            for (auto* routing : invalid.getChildByName("MODULATION_STATE")->getChildIterator())
                if (routing->getStringAttribute("target") == "drive1") routing->setAttribute("source", fire::mod_sources::sourceCount);
        restore(restored, invalid);
        CAPTURE(variant);
        CHECK(restored.treeState.getRawParameterValue("drive1")->load() == Catch::Approx(77));
        CHECK_FALSE(restored.isLfoPresent(15));
    }
}

TEST_CASE("LFO add and remove leave unrelated band Delay and frozen Clouds histories intact",
          "[lfo-bank][dsp][tail][clouds]")
{
    for (auto type : {fire::effects::Type::delay, fire::effects::Type::granular})
    {
        FireAudioProcessor subject, reference;
        for (auto* processor : {&subject, &reference})
        {
            neutral(*processor);
            REQUIRE(processor->addInsertEffect(1, type) == 0);
            const auto control = [&](int index, float value)
            { set(*processor, fire::effects::parameterID(1, 0, index), fire::effects::controls(type)[static_cast<size_t>(index)].range().convertTo0to1(value)); };
            control(5, 100);
            if (type == fire::effects::Type::delay) { control(0, 120); control(1, 70); control(3, 0); }
            else { control(0, 60); control(1, -50); control(3, 15); }
            processor->prepareToPlay(48000, 128);
        }
        juce::AudioBuffer<float> a(2, 128), b(2, 128);
        juce::MidiBuffer midi;
        int position = 0;
        float difference = 0;
        double energy = 0;
        const auto process = [&](bool silence)
        {
            for (int sample = 0; sample < 128; ++sample)
                for (int channel = 0; channel < 2; ++channel)
                {
                    const auto value = silence ? 0.0f : static_cast<float>(0.1 * std::sin((position + sample)
                        * juce::MathConstants<double>::twoPi * 223.0 / 48000.0 + channel * 0.3));
                    a.setSample(channel, sample, value); b.setSample(channel, sample, value);
                }
            subject.processBlock(a, midi); reference.processBlock(b, midi);
            if (silence)
                for (int sample = 0; sample < 128; ++sample)
                {
                    difference = std::max(difference, std::abs(a.getSample(0, sample) - b.getSample(0, sample)));
                    energy += static_cast<double>(b.getSample(0, sample)) * b.getSample(0, sample);
                }
            position += 128;
        };
        for (int block = 0; block < 256; ++block) process(false);
        if (type == fire::effects::Type::granular)
            for (auto* processor : {&subject, &reference})
                set(*processor, fire::clouds_params::parameterID(1, 0, fire::clouds_params::freezeField), 1);
        for (int block = 0; block < 32; ++block) process(false);
        REQUIRE(subject.addLfo() == 4);
        for (int block = 0; block < 40; ++block) process(true);
        REQUIRE(subject.removeLfo(4));
        for (int block = 0; block < 40; ++block) process(true);
        CAPTURE(static_cast<int>(type), difference, energy);
        REQUIRE(energy > 0.0001);
        CHECK(difference < 1.0e-6f);
    }
}
