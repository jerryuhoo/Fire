#include <PluginProcessor.h>
#include <DSP/LfoPerformanceClock.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>

namespace
{
namespace mm = fire::midi_mod;
namespace lp = fire::lfo_playback;
namespace fp = fire::lfo_performance;
namespace ms = fire::mod_sources;
void midiSet(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
struct MidiFixture
{
    FireAudioProcessor processor;
    LfoManager& manager = processor.getLfoManager();
    juce::AudioBuffer<float> output {ms::signalCount, 5000};
    explicit MidiFixture(int capacity = 64)
    {
        processor.hasUpdateCheckBeenPerformed = true;
        LfoData ramp; ramp.points = {{0, 0}, {1, 1}}; ramp.curvatures = {0};
        manager.setLfoData(0, ramp);
        manager.prepare({48000, static_cast<juce::uint32>(capacity), 2});
        manager.processBlock(output, 48000, nullptr, 1); // Publish the staged table before explicit snapshots.
        manager.reset();
    }
    void render(int count, LfoManager::AudioThreadParameterSnapshot snapshot,
                const juce::MidiBuffer* events = nullptr, int offset = 0)
    {
        output.setSize(ms::signalCount, count, false, false, true);
        manager.setMidiInput(events, offset);
        manager.processBlock(output, 48000, nullptr, count, snapshot, offset);
        manager.setMidiInput(nullptr);
    }
};
}

TEST_CASE("MIDI events leave the default Loop Shape path bit exact", "[midi-mod][dsp][legacy]")
{
    MidiFixture plain(31), withMidi(31);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(127)), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(16, 127, 127), 31);
    const std::array<juce::uint8, 1024> payload {};
    midi.addEvent(juce::MidiMessage::createSysExMessage(payload.data(), static_cast<int>(payload.size())), 63);
    auto snapshot = plain.manager.captureAudioThreadParameterSnapshot();
    snapshot.lfos[0].syncMode = 0; snapshot.lfos[0].freeRate = 13.7f;
    for (int block = 0; block < 12; ++block)
    {
        plain.render(127, snapshot); withMidi.render(127, snapshot, &midi);
        for (int source = 0; source < ms::firstMidiCC; ++source)
            for (int sample = 0; sample < 127; ++sample)
                CHECK(plain.output.getSample(source, sample) == withMidi.output.getSample(source, sample));
    }
}

TEST_CASE("CC sources respect event timestamps filters disabled mappings and five millisecond ramps", "[midi-mod][dsp]")
{
    MidiFixture fixture(31);
    auto snapshot = fixture.manager.captureAudioThreadParameterSnapshot();
    snapshot.midiCC[0] = {0, 2}; snapshot.midiCC[1] = {127, 0};
    juce::MidiBuffer events;
    events.addEvent(juce::MidiMessage::controllerEvent(1, 0, 127), 3); // wrong channel
    events.addEvent(juce::MidiMessage::controllerEvent(2, 1, 127), 5); // wrong number
    events.addEvent(juce::MidiMessage::controllerEvent(2, 0, 127), 7);
    events.addEvent(juce::MidiMessage::controllerEvent(16, 127, 64), 31);
    fixture.render(512, snapshot, &events);
    for (int sample = 0; sample < 512; ++sample)
    {
        const auto ramp = sample < 7 ? 0.0f : juce::jmin(1.0f, static_cast<float>(sample - 6) / 240.0f);
        CHECK(fixture.output.getSample(ms::firstMidiCC, sample) == Catch::Approx(ramp).margin(2e-6));
        CHECK(fixture.output.getSample(ms::firstMidiCC + 2, sample) == 0);
        CHECK(fixture.output.getSample(ms::firstMidiCC + 3, sample) == 0);
    }
    CHECK(fixture.manager.getMidiCCValue(0) == 1);
    CHECK(fixture.manager.getMidiCCValue(1) == Catch::Approx(64.0f / 127));
    snapshot.midiCC[0] = {1, 2}; fixture.render(64, snapshot);
    CHECK(fixture.output.getMagnitude(ms::firstMidiCC, 0, 64) == 0);
}

TEST_CASE("Outer callback ranges never replay CC or Note events", "[midi-mod][dsp][oversized]")
{
    MidiFixture whole(37), partitioned(37);
    auto snapshot = whole.manager.captureAudioThreadParameterSnapshot();
    snapshot.midiCC[0] = {7, 0};
    snapshot.lfos[0].playback = {lp::oneShot, lp::randomHold, 51, -1, 0};
    juce::MidiBuffer events;
    for (int sample : {0, 36, 37, 101, 300})
    {
        events.addEvent(juce::MidiMessage::controllerEvent(3, 7, (sample + 17) % 128), sample);
        events.addEvent(juce::MidiMessage::noteOn(3, 50, juce::uint8(100)), sample);
    }
    whole.render(512, snapshot, &events);
    for (int offset = 0; offset < 512; offset += 101)
    {
        const int count = juce::jmin(101, 512 - offset);
        partitioned.render(count, snapshot, &events, offset);
        for (int sample = 0; sample < count; ++sample)
            for (int source : {0, ms::firstMidiCC})
                CHECK(partitioned.output.getSample(source, sample) == whole.output.getSample(source, offset + sample));
    }
}

TEST_CASE("One shot Shape holds its start and runs a complete rotated cycle at the note sample", "[midi-mod][dsp][one-shot]")
{
    for (float offset : {0.0f, .25f})
    {
        MidiFixture fixture(67);
        auto snapshot = fixture.manager.captureAudioThreadParameterSnapshot();
        auto& lfo = snapshot.lfos[0];
        lfo.syncMode = 0; lfo.freeRate = 20; lfo.phaseOffset = offset;
        lfo.playback = {lp::oneShot, lp::shape, 0, 60, 2};
        juce::MidiBuffer events;
        events.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(127)), 1);
        events.addEvent(juce::MidiMessage::noteOn(2, 61, juce::uint8(127)), 2);
        events.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(0)), 3);
        events.addEvent(juce::MidiMessage::noteOn(2, 60, juce::uint8(127)), 11);
        fixture.render(2450, snapshot, &events);
        for (int sample = 0; sample < 2450; ++sample)
        {
            CAPTURE(offset, sample);
            const auto elapsed = juce::jlimit(0.0, 1.0, static_cast<double>(sample - 11) / 2400);
            const auto expected = sample >= 2411 && offset == 0 ? 1.0 : std::fmod(elapsed + offset, 1.0);
            CHECK(fixture.output.getSample(0, sample) == Catch::Approx(expected).margin(2e-6));
        }
    }
}

TEST_CASE("Every same sample Note On advances a deterministic random trigger sequence", "[midi-mod][dsp][random]")
{
    MidiFixture fixture;
    auto snapshot = fixture.manager.captureAudioThreadParameterSnapshot();
    snapshot.lfos[0].playback = {lp::oneShot, lp::randomHold, 139, -1, 0};
    juce::MidiBuffer events;
    events.addEvent(juce::MidiMessage::noteOn(1, 48, juce::uint8(100)), 7);
    events.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 7);
    events.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(100)), 7);
    events.addEvent(juce::MidiMessage::noteOn(1, 67, juce::uint8(100)), 40);
    fixture.render(80, snapshot, &events);
    for (int sample = 0; sample < 80; ++sample)
        CHECK(fixture.output.getSample(0, sample) == fp::randomForCycle(139, 0, sample < 7 ? 0 : sample < 40 ? 3 : 4));
    fixture.manager.reset(); fixture.render(80, snapshot, &events);
    CHECK(fixture.output.getSample(0, 79) == fp::randomForCycle(139, 0, 4));
}

TEST_CASE("Manual triggering coalesces counts and cannot target a recreated slot", "[midi-mod][dsp][manual]")
{
    MidiFixture fixture;
    CHECK_FALSE(fixture.manager.requestLfoTrigger(0, fixture.manager.getLfoSlotGeneration(0)));
    midiSet(fixture.processor, lp::parameterID(0, lp::playMode), lp::oneShot);
    midiSet(fixture.processor, lp::parameterID(0, lp::waveMode), lp::randomHold);
    midiSet(fixture.processor, lp::parameterID(0, lp::seed), 9);
    const auto generation = fixture.manager.getLfoSlotGeneration(0);
    for (int request = 0; request < 3; ++request) REQUIRE(fixture.manager.requestLfoTrigger(0, generation));
    fixture.manager.reset(); // Deferred processor reset must preserve queued UI requests.
    auto snapshot = fixture.manager.captureAudioThreadParameterSnapshot();
    fixture.render(64, snapshot);
    CHECK(fixture.output.getSample(0, 0) == fp::randomForCycle(9, 0, 3));
    REQUIRE(fixture.manager.requestLfoTrigger(0, generation));
    fixture.render(64, snapshot, nullptr, 64); // A later range cannot consume a new manual request.
    CHECK(fixture.output.getSample(0, 0) == fp::randomForCycle(9, 0, 3));
    fixture.render(64, snapshot);
    CHECK(fixture.output.getSample(0, 0) == fp::randomForCycle(9, 0, 4));
    fixture.manager.resetLfoSlot(0);
    CHECK_FALSE(fixture.manager.requestLfoTrigger(0, generation));
}

TEST_CASE("MIDI learn is owned cancellable and retains its first value after mapping publication", "[midi-mod][dsp][learn]")
{
    MidiFixture fixture;
    auto snapshot = fixture.manager.captureAudioThreadParameterSnapshot();
    const auto old = fixture.manager.beginMidiLearn(0);
    const auto token = fixture.manager.beginMidiLearn(2);
    fixture.manager.cancelMidiLearn(old);
    CHECK_FALSE(fixture.manager.isMidiLearnActive(old));
    REQUIRE(fixture.manager.isMidiLearnActive(token));
    juce::MidiBuffer events; events.addEvent(juce::MidiMessage::controllerEvent(16, 0, 93), 8);
    fixture.render(64, snapshot, &events);
    CHECK_FALSE(fixture.manager.pollMidiLearn(old));
    const auto learned = fixture.manager.pollMidiLearn(token); REQUIRE(learned);
    CHECK(learned->slot == 2); CHECK(learned->number == 0); CHECK(learned->channel == 16);
    CHECK(learned->value == Catch::Approx(93.0f / 127));
    CHECK(fixture.manager.isMidiLearnActive(token));
    fixture.manager.cancelMidiLearn(token);
    snapshot.midiCC[2] = {learned->number, learned->channel};
    fixture.manager.stageMidiCCInitialValue(2, learned->number, learned->channel, learned->value);
    fixture.render(300, snapshot);
    CHECK(fixture.manager.getMidiCCValue(2) == Catch::Approx(learned->value));
    const auto stale = fixture.manager.beginMidiLearn(1); fixture.manager.reset();
    CHECK(fixture.manager.isMidiLearnActive(stale));
    fixture.manager.prepare({48000, 64, 2});
    CHECK_FALSE(fixture.manager.isMidiLearnActive(stale));
}

TEST_CASE("Seeded random Loop seeks backwards deterministically and One Shot holds the exact target", "[midi-mod][dsp][random]")
{
    fp::PerformanceClock clock;
    clock.configure({fp::Playback::loop, fp::Wave::hold, 65535, 0});
    for (double coordinate : {-100.25, -1.0, -.001, 0.0, 5.625, -100.25})
    {
        const auto value = clock.next(.125, coordinate);
        const auto cycle = fp::splitCycles(coordinate);
        CHECK(value.randomValue == fp::randomForCycle(65535, static_cast<std::uint64_t>(cycle.cycle)));
    }
    clock.configure({fp::Playback::oneShot, fp::Wave::glide, 49, .7});
    REQUIRE(clock.trigger());
    for (int sample = 0; sample < 10; ++sample)
    {
        const auto value = clock.next(.25);
        if (sample == 0) CHECK(value.randomValue == fp::randomForCycle(49, 0, 1));
        if (sample >= 4) { CHECK(value.randomValue == fp::randomForCycle(49, 1, 1)); CHECK_FALSE(value.running); }
    }
}

TEST_CASE("State replacement invalidates performance requests without changing legacy phase identity", "[midi-mod][dsp][state]")
{
    MidiFixture fixture;
    midiSet(fixture.processor, lp::parameterID(0, lp::playMode), lp::oneShot);
    const auto token = fixture.manager.getLfoSlotGeneration(0);
    const auto oldSnapshot = fixture.manager.captureAudioThreadParameterSnapshot();
    REQUIRE(fixture.manager.requestLfoTrigger(0, token));
    std::array<LfoData, fire::lfo_bank::capacity> shapes;
    const auto current = fixture.manager.getLfoDataCopy();
    std::copy(current.begin(), current.end(), shapes.begin());
    const auto learn = fixture.manager.beginMidiLearn(0);
    REQUIRE(fixture.manager.replaceLfoDataAndRoutings(shapes, fixture.manager.getModulationRoutingsCopy()));
    CHECK_FALSE(fixture.manager.isMidiLearnActive(learn));
    CHECK_FALSE(fixture.manager.requestLfoTrigger(0, token));
    const auto snapshot = fixture.manager.captureAudioThreadParameterSnapshot();
    CHECK(snapshot.lfos[0].generation == oldSnapshot.lfos[0].generation);
    CHECK(snapshot.lfos[0].performanceGeneration != oldSnapshot.lfos[0].performanceGeneration);
    fixture.render(128, snapshot);
    CHECK(fixture.output.getSample(0, 127) == 0); // The old queued trigger was discarded.

    for (const auto& forbidden : {lp::parameterID(0, lp::seed), mm::parameterID(0, mm::number)})
    {
        CHECK(fixture.processor.assignLfoToTarget(0, forbidden) == LfoManager::AssignmentResult::invalidRequest);
        juce::Array<ModulationRouting> routes;
        routes.add({0, forbidden, 1, true, false});
        CHECK_FALSE(fixture.manager.replaceLfoDataAndRoutings(shapes, routes));
    }
}

TEST_CASE("Learned initial CC values cannot replay after configuration changes away and back", "[midi-mod][dsp][learn]")
{
    for (bool renderWhileAway : {false, true})
    {
        MidiFixture fixture;
        midiSet(fixture.processor, mm::parameterID(0, mm::number), 1);
        fixture.manager.stageMidiCCInitialValue(0, 1, 0, 102.0f / 127);
        midiSet(fixture.processor, mm::parameterID(0, mm::number), 2);
        if (renderWhileAway)
            fixture.render(300, fixture.manager.captureAudioThreadParameterSnapshot());
        midiSet(fixture.processor, mm::parameterID(0, mm::number), 1);
        fixture.render(300, fixture.manager.captureAudioThreadParameterSnapshot());
        CHECK(fixture.output.getMagnitude(ms::firstMidiCC, 0, 300) == 0);
        CHECK(fixture.manager.getMidiCCValue(0) == 0);
    }
}

TEST_CASE("A learned initial value waits for its matching accepted parameter snapshot", "[midi-mod][dsp][learn]")
{
    MidiFixture fixture;
    const auto oldSnapshot = fixture.manager.captureAudioThreadParameterSnapshot();
    midiSet(fixture.processor, mm::parameterID(0, mm::number), 1);
    midiSet(fixture.processor, mm::parameterID(0, mm::channel), 2);
    fixture.manager.stageMidiCCInitialValue(0, 1, 2, 102.0f / 127);
    fixture.render(300, oldSnapshot);
    CHECK(fixture.output.getMagnitude(ms::firstMidiCC, 0, 300) == 0);
    fixture.render(300, fixture.manager.captureAudioThreadParameterSnapshot());
    CHECK(fixture.manager.getMidiCCValue(0) == Catch::Approx(102.0f / 127));
}

TEST_CASE("One shot clock crosses rotated boundaries without exposing its terminal sample early", "[midi-mod][dsp][one-shot]")
{
    for (double offset : {0.0, .25})
    {
        fp::PerformanceClock clock;
        clock.configure({fp::Playback::oneShot, fp::Wave::shape, 0, offset});
        REQUIRE(clock.trigger());
        for (int sample = 0; sample <= 2402; ++sample)
        {
            CAPTURE(offset, sample);
            const auto value = clock.next(20.0 / 48000.0);
            CHECK(value.running == (sample < 2400));
            if (offset == .25 && sample == 1800) CHECK(value.shapePhase == 0);
            if (sample >= 2400) CHECK(value.shapePhase == (offset == 0 ? 1.0f : .25f));
        }
    }
}
