#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
constexpr double matchSampleRate = 16000.0;
constexpr int matchBlockSize = 512;
constexpr double measurementRenderSeconds = 3.6;
constexpr float gainToleranceDb = 0.15f;

void setPlain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float getPlain(FireAudioProcessor& processor, const juce::String& id)
{
    const auto* parameter = processor.treeState.getRawParameterValue(id);
    REQUIRE(parameter != nullptr);
    return parameter->load();
}

void neutralise(FireAudioProcessor& processor, float outputDb, bool hq = false)
{
    setPlain(processor, HQ_ID, hq ? 1.0f : 0.0f);
    setPlain(processor, NUM_BANDS_ID, 1.0f);
    setPlain(processor, FILTER_BYPASS_ID, 0.0f);
    setPlain(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlain(processor, MIX_ID, 1.0f);
    setPlain(processor, OUTPUT_ID, outputDb);
    const auto band = [](const juce::String& id)
    {
        return ParameterIDAndName::getIDString(id, 0);
    };
    setPlain(processor, band(BAND_ENABLE_ID), 1.0f);
    setPlain(processor, band(BAND_SOLO_ID), 0.0f);
    setPlain(processor, band(LINKED_ID), 0.0f);
    setPlain(processor, band(DRIVE_BYPASS_ID), 0.0f);
    setPlain(processor, band(SHAPE_BYPASS_ID), 0.0f);
    setPlain(processor, band(COMP_BYPASS_ID), 0.0f);
    setPlain(processor, band(WIDTH_BYPASS_ID), 0.0f);
    setPlain(processor, band(OTT_ENABLED_ID), 0.0f);
    setPlain(processor, band(DC_FILTER_ID), 0.0f);
    setPlain(processor, band(MODE_ID), 4.0f);
    setPlain(processor, band(OUTPUT_ID), 0.0f);
    setPlain(processor, band(MIX_ID), 1.0f);
}

void prepare(FireAudioProcessor& processor)
{
    processor.setRateAndBufferSizeDetails(matchSampleRate, matchBlockSize);
    processor.prepareToPlay(matchSampleRate, matchBlockSize);
}

juce::AudioBuffer<float> input(int firstSample, int samples)
{
    juce::AudioBuffer<float> result(2, samples);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto time = static_cast<double>(firstSample + sample) / matchSampleRate;
            const auto phase = 0.31 * static_cast<double>(channel);
            result.setSample(channel, sample,
                static_cast<float>(0.10 * std::sin(juce::MathConstants<double>::twoPi * 1000.0 * time + phase)
                                 + 0.04 * std::cos(juce::MathConstants<double>::twoPi * 250.0 * time - phase)));
        }
    return result;
}

void render(FireAudioProcessor& processor, int& timeline, double seconds, bool bypass = false)
{
    const auto total = juce::roundToInt(seconds * matchSampleRate);
    juce::MidiBuffer midi;
    for (int done = 0; done < total;)
    {
        const auto samples = juce::jmin(matchBlockSize, total - done);
        auto block = input(timeline, samples);
        if (bypass)
            processor.processBlockBypassed(block, midi);
        else
            processor.processBlock(block, midi);
        timeline += samples;
        done += samples;
    }
}

float outputGainDb(FireAudioProcessor& processor, int& timeline)
{
    // Both tones span complete cycles in 512 samples. A fixed PDC shift
    // therefore leaves this steady-state input/output energy ratio unchanged.
    auto block = input(timeline, matchBlockSize);
    const auto inputRms = block.getRMSLevel(0, 0, matchBlockSize);
    juce::MidiBuffer midi;
    processor.processBlock(block, midi);
    timeline += matchBlockSize;
    const auto outputRms = block.getRMSLevel(0, 0, matchBlockSize);
    REQUIRE(inputRms > 0.0f);
    REQUIRE(outputRms > 0.0f);
    return juce::Decibels::gainToDecibels(outputRms / inputRms);
}

void checkGain(FireAudioProcessor& processor, float expectedDb)
{
    const auto state = processor.getLoudnessMatchState();
    CHECK(state.enabled);
    CHECK(state.ready);
    CHECK_FALSE(state.measuring);
    CHECK_FALSE(state.limited);
    CHECK_FALSE(state.noSignal);
    CHECK(state.gainDb == Catch::Approx(expectedDb).margin(gainToleranceDb));
}

std::vector<float> parameterValues(FireAudioProcessor& processor)
{
    std::vector<float> result;
    for (const auto* parameter : processor.getParameters())
        result.push_back(parameter->getValue());
    return result;
}

juce::StringArray parameterIDs(FireAudioProcessor& processor)
{
    juce::StringArray result;
    for (const auto* parameter : processor.getParameters())
    {
        const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
        REQUIRE(identified != nullptr);
        result.add(identified->paramID);
    }
    return result;
}

juce::MemoryBlock saveHost(FireAudioProcessor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    REQUIRE(state.getSize() > 0);
    return state;
}

void restoreHost(FireAudioProcessor& processor, const juce::MemoryBlock& state)
{
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
}

constexpr std::array<const char*, 6> matchAttributes {
    "loudnessMatchVersion", "loudnessMatchEnabled", "loudnessMatchReadyA",
    "loudnessMatchReadyB", "loudnessMatchGainA", "loudnessMatchGainB"
};
}

TEST_CASE("Disabled loudness matching preserves exact audio parameter identities and PDC",
          "[loudness-match][processor][disabled][latency][oversized]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto hq : { false, true })
    {
        CAPTURE(hq);
        FireAudioProcessor subject, reference;
        neutralise(subject, 6.0f, hq);
        neutralise(reference, 6.0f, hq);
        prepare(subject);
        prepare(reference);
        const auto ids = parameterIDs(subject);
        const auto latency = subject.getLatencySamples();
        REQUIRE_FALSE(subject.getLoudnessMatchState().enabled);
        CHECK_FALSE(subject.getLoudnessMatchState().measuring);
        CHECK(ids == parameterIDs(reference));
        CHECK(latency == reference.getLatencySamples());

        int timeline = 0;
        juce::MidiBuffer midi;
        for (int samples : { 512, 2305, 511, 512 })
        {
            CAPTURE(samples);
            auto actual = input(timeline, samples);
            auto expected = input(timeline, samples);
            subject.processBlock(actual, midi);
            reference.processBlock(expected, midi);
            for (int channel = 0; channel < 2; ++channel)
                CHECK(std::equal(actual.getReadPointer(channel),
                                  actual.getReadPointer(channel) + samples,
                                  expected.getReadPointer(channel)));
            timeline += samples;
        }
        subject.setLoudnessMatchEnabled(true);
        subject.setLoudnessMatchEnabled(false);
        CHECK(parameterIDs(subject) == ids);
        CHECK(subject.getLatencySamples() == latency);
        CHECK(getPlain(subject, OUTPUT_ID) == Catch::Approx(6.0f));
    }
}

TEST_CASE("Loudness match learns opposite audition trim for known output offsets",
          "[loudness-match][processor][learn][gain]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto outputDb : { -6.0f, 6.0f })
    {
        CAPTURE(outputDb);
        FireAudioProcessor processor;
        neutralise(processor, outputDb);
        prepare(processor);
        const auto originalParameters = parameterValues(processor);
        const auto latency = processor.getLatencySamples();
        int timeline = 0;
        render(processor, timeline, 0.15);
        processor.setLoudnessMatchEnabled(true);
        CHECK(processor.getLoudnessMatchState().measuring);
        render(processor, timeline, measurementRenderSeconds);
        checkGain(processor, -outputDb);
        CHECK(outputGainDb(processor, timeline) == Catch::Approx(0.0f).margin(0.20f));
        CHECK(parameterValues(processor) == originalParameters);
        CHECK(processor.getLatencySamples() == latency);
    }
}

TEST_CASE("A-B sides learn independently and Copy duplicates the active audition gain",
          "[loudness-match][processor][ab][copy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    neutralise(processor, -6.0f);
    processor.stateAB.copyAB(false); // B is -6 dB; A will be +6 dB.
    setPlain(processor, OUTPUT_ID, 6.0f);
    prepare(processor);
    int timeline = 0;
    processor.setLoudnessMatchEnabled(true);
    render(processor, timeline, measurementRenderSeconds);
    checkGain(processor, -6.0f);
    CHECK(processor.getLoudnessMatchState().side == 0);

    processor.stateAB.toggleAB();
    CHECK(processor.getLoudnessMatchState().side == 1);
    render(processor, timeline, measurementRenderSeconds);
    checkGain(processor, 6.0f);
    CHECK(getPlain(processor, OUTPUT_ID) == Catch::Approx(-6.0f));

    processor.stateAB.toggleAB();
    CHECK(processor.getLoudnessMatchState().side == 0);
    render(processor, timeline, 0.20);
    checkGain(processor, -6.0f);
    processor.stateAB.copyAB(false);
    processor.stateAB.toggleAB();
    render(processor, timeline, 0.20);
    CHECK(processor.getLoudnessMatchState().side == 1);
    checkGain(processor, -6.0f);
    CHECK(getPlain(processor, OUTPUT_ID) == Catch::Approx(6.0f));
    CHECK(outputGainDb(processor, timeline) == Catch::Approx(0.0f).margin(0.20f));
}

TEST_CASE("A learned audition gain stays locked until manual relearning",
          "[loudness-match][processor][locked][relearn]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    neutralise(processor, 6.0f);
    prepare(processor);
    int timeline = 0;
    processor.setLoudnessMatchEnabled(true);
    render(processor, timeline, measurementRenderSeconds);
    checkGain(processor, -6.0f);
    const auto lockedGain = processor.getLoudnessMatchState().gainDb;

    setPlain(processor, OUTPUT_ID, -6.0f);
    render(processor, timeline, measurementRenderSeconds);
    CHECK(juce::exactlyEqual(processor.getLoudnessMatchState().gainDb, lockedGain));
    CHECK_FALSE(processor.getLoudnessMatchState().measuring);
    CHECK(outputGainDb(processor, timeline) == Catch::Approx(-12.0f).margin(0.20f));

    processor.learnLoudnessMatch();
    CHECK(processor.getLoudnessMatchState().measuring);
    render(processor, timeline, measurementRenderSeconds);
    checkGain(processor, 6.0f);
    CHECK(outputGainDb(processor, timeline) == Catch::Approx(0.0f).margin(0.20f));

    processor.learnLoudnessMatch();
    REQUIRE(processor.getLoudnessMatchState().measuring);
    processor.learnLoudnessMatch();
    CHECK_FALSE(processor.getLoudnessMatchState().measuring);
}

TEST_CASE("Host bypass neither applies audition trim nor advances a pending measurement",
          "[loudness-match][processor][host-bypass][learn]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject, reference;
    neutralise(subject, 6.0f);
    neutralise(reference, 6.0f);
    prepare(subject);
    prepare(reference);
    int subjectTimeline = 0, referenceTimeline = 0;
    subject.setLoudnessMatchEnabled(true);
    render(subject, subjectTimeline, measurementRenderSeconds);
    render(reference, referenceTimeline, measurementRenderSeconds);
    checkGain(subject, -6.0f);
    subject.learnLoudnessMatch();
    const auto before = subject.getLoudnessMatchState();
    REQUIRE(before.measuring);

    juce::MidiBuffer midi;
    for (int block = 0; block < 120; ++block)
    {
        auto actual = input(subjectTimeline, matchBlockSize);
        auto expected = input(referenceTimeline, matchBlockSize);
        subject.processBlockBypassed(actual, midi);
        reference.processBlockBypassed(expected, midi);
        for (int channel = 0; channel < 2; ++channel)
            CHECK(std::equal(actual.getReadPointer(channel),
                              actual.getReadPointer(channel) + matchBlockSize,
                              expected.getReadPointer(channel)));
        subjectTimeline += matchBlockSize;
        referenceTimeline += matchBlockSize;
    }
    const auto bypassed = subject.getLoudnessMatchState();
    CHECK(bypassed.bypassed);
    CHECK(juce::exactlyEqual(bypassed.gainDb, before.gainDb));
    CHECK(bypassed.progress <= before.progress);
    CHECK(subject.getLatencySamples() == reference.getLatencySamples());
}

TEST_CASE("Host state restores both audition gains while old metadata-free states reset matching",
          "[loudness-match][state][host][ab][legacy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    neutralise(source, -6.0f);
    source.stateAB.copyAB(false);
    setPlain(source, OUTPUT_ID, 6.0f);
    prepare(source);
    int sourceTimeline = 0;
    source.setLoudnessMatchEnabled(true);
    render(source, sourceTimeline, measurementRenderSeconds);
    checkGain(source, -6.0f);
    source.stateAB.toggleAB();
    render(source, sourceTimeline, measurementRenderSeconds);
    checkGain(source, 6.0f);
    const auto saved = saveHost(source);

    FireAudioProcessor restored;
    restoreHost(restored, saved);
    prepare(restored); // Preparation must retain the restored completed gains.
    int restoredTimeline = 0;
    render(restored, restoredTimeline, 0.20);
    CHECK(restored.getLoudnessMatchState().side == 1);
    checkGain(restored, 6.0f);
    CHECK(outputGainDb(restored, restoredTimeline) == Catch::Approx(0.0f).margin(0.20f));
    restored.stateAB.toggleAB();
    render(restored, restoredTimeline, 0.20);
    CHECK(restored.getLoudnessMatchState().side == 0);
    checkGain(restored, -6.0f);

    auto legacy = juce::AudioProcessor::getXmlFromBinary(
        saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(legacy != nullptr);
    auto* metadata = legacy->getChildByName("otherState");
    REQUIRE(metadata != nullptr);
    REQUIRE(metadata->getIntAttribute("loudnessMatchVersion") == 1);
    for (const auto* attribute : matchAttributes)
        metadata->removeAttribute(attribute);
    juce::MemoryBlock legacyState;
    juce::AudioProcessor::copyXmlToBinary(*legacy, legacyState);
    restoreHost(restored, legacyState);
    render(restored, restoredTimeline, 0.20);
    const auto state = restored.getLoudnessMatchState();
    CHECK_FALSE(state.enabled);
    CHECK_FALSE(state.measuring);
    CHECK_FALSE(state.ready);
    CHECK(state.gainDb == Catch::Approx(0.0f));
    CHECK(outputGainDb(restored, restoredTimeline) == Catch::Approx(-6.0f).margin(0.20f));
}

TEST_CASE("Ordinary presets omit audition matching and successful loads clear the current gain",
          "[loudness-match][state][preset][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    neutralise(processor, 6.0f);
    prepare(processor);
    int timeline = 0;
    processor.setLoudnessMatchEnabled(true);
    render(processor, timeline, measurementRenderSeconds);
    checkGain(processor, -6.0f);
    juce::XmlElement preset("WINGSFIRE");
    state::saveStateToXml(processor, preset);
    for (const auto* attribute : matchAttributes)
        CHECK_FALSE(preset.hasAttribute(attribute));
    CHECK(preset.getChildByName("otherState") == nullptr);

    auto corrupt = preset;
    corrupt.setAttribute("presetFormatVersion", 999);
    const auto before = processor.getLoudnessMatchState();
    CHECK_FALSE(state::loadStateFromXml(corrupt, processor));
    CHECK(processor.getLoudnessMatchState().ready == before.ready);
    CHECK(juce::exactlyEqual(processor.getLoudnessMatchState().gainDb, before.gainDb));

    REQUIRE(state::loadStateFromXml(preset, processor));
    const auto cleared = processor.getLoudnessMatchState();
    CHECK(cleared.enabled);
    CHECK_FALSE(cleared.ready);
    CHECK(cleared.gainDb == Catch::Approx(0.0f));
    render(processor, timeline, measurementRenderSeconds);
    checkGain(processor, -6.0f);
}

namespace
{
juce::MemoryBlock encodeHostXml(const juce::XmlElement& xml)
{
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary(xml, state);
    return state;
}

void restoreCompletedSides(FireAudioProcessor& processor, float gainA, float gainB)
{
    // The learning/audio tests above establish how completed gains are
    // obtained. These serialization tests use the documented host format to
    // seed distinct records without rendering another six seconds per case.
    const auto saved = saveHost(processor);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(xml != nullptr);
    auto* metadata = xml->getChildByName("otherState");
    REQUIRE(metadata != nullptr);
    metadata->setAttribute("loudnessMatchVersion", 1);
    metadata->setAttribute("loudnessMatchEnabled", true);
    metadata->setAttribute("loudnessMatchReadyA", true);
    metadata->setAttribute("loudnessMatchReadyB", true);
    metadata->setAttribute("loudnessMatchGainA", gainA);
    metadata->setAttribute("loudnessMatchGainB", gainB);
    restoreHost(processor, encodeHostXml(*xml));
}

void configureDistinctCompletedSides(FireAudioProcessor& processor)
{
    neutralise(processor, -6.0f);
    processor.stateAB.copyAB(false);
    setPlain(processor, OUTPUT_ID, 6.0f);
    restoreCompletedSides(processor, -6.0f, 6.0f);
    REQUIRE(processor.stateAB.isCurrentA());
    checkGain(processor, -6.0f);
}
}

TEST_CASE("Malformed loudness metadata rejects the complete host restore atomically",
          "[loudness-match][state][host][corrupt][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    configureDistinctCompletedSides(subject);
    const auto baseline = saveHost(subject);
    const auto baselineParameters = parameterValues(subject);

    FireAudioProcessor incoming;
    neutralise(incoming, -3.0f);
    incoming.stateAB.copyAB(false);
    setPlain(incoming, OUTPUT_ID, 3.0f);
    restoreCompletedSides(incoming, -3.0f, 3.0f);
    const auto incomingState = saveHost(incoming);
    auto incomingXml = juce::AudioProcessor::getXmlFromBinary(
        incomingState.getData(), static_cast<int>(incomingState.getSize()));
    REQUIRE(incomingXml != nullptr);

    const auto checkRejected = [&](const juce::XmlElement& damaged)
    {
        restoreHost(subject, encodeHostXml(damaged));
        CHECK(parameterValues(subject) == baselineParameters);
        CHECK(getPlain(subject, OUTPUT_ID) == Catch::Approx(6.0f));
        CHECK(subject.stateAB.isCurrentA());
        checkGain(subject, -6.0f);
        // Includes inactive B, LFO/routes, preset identity and both gains;
        // rejection must not partially install any section of the new chunk.
        CHECK(saveHost(subject) == baseline);
    };

    for (const auto& [attribute, value] : {
             std::pair { "loudnessMatchVersion", "2" },
             std::pair { "loudnessMatchVersion", "1oops" },
             std::pair { "loudnessMatchVersion", "-1" },
             std::pair { "loudnessMatchEnabled", "true" },
             std::pair { "loudnessMatchReadyB", "2" },
             std::pair { "loudnessMatchGainA", "nan" },
             std::pair { "loudnessMatchGainB", "6oops" },
             std::pair { "loudnessMatchGainA", "18.01" } })
    {
        CAPTURE(attribute, value);
        auto damaged = *incomingXml;
        auto* metadata = damaged.getChildByName("otherState");
        REQUIRE(metadata != nullptr);
        metadata->setAttribute(attribute, value);
        checkRejected(damaged);
    }
    for (const auto* attribute : matchAttributes)
    {
        CAPTURE(attribute);
        auto damaged = *incomingXml;
        auto* metadata = damaged.getChildByName("otherState");
        REQUIRE(metadata != nullptr);
        metadata->removeAttribute(attribute);
        checkRejected(damaged);
    }
}

TEST_CASE("Host snapshots keep audition gains paired with the captured A-B sound",
          "[loudness-match][state][host][ab][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    configureDistinctCompletedSides(source);
    bool hookWasCalled = false;
    source.setHostStateMainCaptureHookForTesting([&]
    {
        hookWasCalled = true;
        source.stateAB.toggleAB();
    });
    const auto saved = saveHost(source);
    REQUIRE(hookWasCalled);
    REQUIRE_FALSE(source.stateAB.isCurrentA());
    CHECK(getPlain(source, OUTPUT_ID) == Catch::Approx(-6.0f));
    checkGain(source, 6.0f);

    FireAudioProcessor restored;
    restoreHost(restored, saved);
    CHECK(restored.stateAB.isCurrentA());
    CHECK(restored.getLoudnessMatchState().side == 0);
    CHECK(getPlain(restored, OUTPUT_ID) == Catch::Approx(6.0f));
    checkGain(restored, -6.0f);
    prepare(restored);
    int timeline = 0;
    render(restored, timeline, 0.20);
    CHECK(outputGainDb(restored, timeline) == Catch::Approx(0.0f).margin(0.20f));

    restored.stateAB.toggleAB();
    CHECK(restored.getLoudnessMatchState().side == 1);
    CHECK(getPlain(restored, OUTPUT_ID) == Catch::Approx(-6.0f));
    checkGain(restored, 6.0f);
}

TEST_CASE("Damaged alternate state clears audition matching when fallback reassigns A-B sides",
          "[loudness-match][state][host][ab][fallback]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    configureDistinctCompletedSides(source);
    source.stateAB.toggleAB();
    REQUIRE_FALSE(source.stateAB.isCurrentA());
    checkGain(source, 6.0f);
    const auto saved = saveHost(source);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(xml != nullptr);
    auto* alternate = xml->getChildByName("AB_STATE");
    REQUIRE(alternate != nullptr);
    alternate->removeAttribute(OUTPUT_ID);

    FireAudioProcessor restored;
    restoreHost(restored, encodeHostXml(*xml));
    CHECK(restored.stateAB.isCurrentA());
    CHECK(getPlain(restored, OUTPUT_ID) == Catch::Approx(-6.0f));
    const auto fallback = restored.getLoudnessMatchState();
    CHECK_FALSE(fallback.enabled);
    CHECK_FALSE(fallback.ready);
    CHECK_FALSE(fallback.measuring);
    CHECK(fallback.gainDb == Catch::Approx(0.0f));
    CHECK(fallback.side == 0);

    restored.stateAB.toggleAB();
    CHECK(getPlain(restored, OUTPUT_ID) == Catch::Approx(-6.0f));
    CHECK_FALSE(restored.getLoudnessMatchState().enabled);
    CHECK_FALSE(restored.getLoudnessMatchState().ready);
    CHECK(restored.getLoudnessMatchState().gainDb == Catch::Approx(0.0f));
}
