#include <PluginProcessor.h>
#include <DSP/LfoEngine.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
LfoData ramp()
{
    LfoData shape;
    shape.points = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    shape.curvatures = {0.0f};
    return shape;
}
void set(FireAudioProcessor& processor, int source, const juce::String& base, float value)
{
    auto* parameter = processor.treeState.getParameter(base + juce::String(source + 1));
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void configure(FireAudioProcessor& processor, int source, bool synced, float phase = 0.25f)
{
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, source, "lfoPresent", 1.0f);
    set(processor, source, LFO_SYNC_MODE_ID, synced ? 1.0f : 0.0f);
    set(processor, source, LFO_RATE_SYNC_ID, 8.0f);
    set(processor, source, LFO_RATE_HZ_ID, 2.0f);
    set(processor, source, LFO_PHASE_ID, phase);
    set(processor, source, LFO_SMOOTH_ID, 0.0f);
    processor.getLfoManager().setLfoData(source, ramp());
}
class PlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override { return position; }
    PositionInfo position;
};
}

TEST_CASE("LFO visual phase identifies the rendered sample while canonical phase remains one sample ahead",
          "[lfo-visual][lfo][dsp][phase]")
{
    LfoEngine engine;
    engine.stageShape(ramp());
    engine.publishStagedShape();
    engine.prepare({48000.0, 64, 1});
    CHECK(engine.getLastRenderedPhaseForAudioThread() == -1.0f);
    engine.setPhase(0.25f);
    engine.setPhaseDelta(0.001f);
    CHECK(engine.process() == Catch::Approx(0.25f).margin(2.0e-6f));
    CHECK(engine.getLastRenderedPhaseForAudioThread() == Catch::Approx(0.25f));
    CHECK(engine.getPhase() == Catch::Approx(0.251f));

    const auto oldAudibleNextPhase = engine.getPhase();
    engine.setPhaseWithCorrection(0.8f);
    for (int sample = 0; sample <= 480; ++sample)
    {
        const auto canonicalBefore = engine.getPhase();
        const auto output = engine.process();
        const auto rendered = engine.getLastRenderedPhaseForAudioThread();
        // The linear ramp is an independent phase oracle, including the
        // wrapped correction bridge; publishing canonical phase fails here.
        CHECK(rendered == Catch::Approx(output).margin(2.0e-6f));
        if (sample == 0)
        {
            CHECK(rendered == Catch::Approx(oldAudibleNextPhase).margin(2.0e-6f));
            CHECK(std::abs(rendered - engine.getPhase()) > 0.25f);
        }
        if (sample == 480)
            CHECK(rendered == Catch::Approx(canonicalBefore).margin(2.0e-6f));
    }
    engine.reset();
    CHECK(engine.getLastRenderedPhaseForAudioThread() == -1.0f);
    CHECK(engine.getPhase() == 0.0f);
}

TEST_CASE("LFO visual snapshots publish the final rendered sample and monotonic lifecycle invalidations",
          "[lfo-visual][lfo][dsp][telemetry][lifecycle]")
{
    FireAudioProcessor processor;
    configure(processor, 0, false);
    configure(processor, 15, false, 0.1f);
    auto& manager = processor.getLfoManager();
    const auto initial = processor.getLfoVisualState(0);
    CHECK(initial.phase == -1.0f);
    manager.prepare({48000.0, 64, 1});
    auto previous = processor.getLfoVisualState(0);
    CHECK(previous.phase == -1.0f);
    CHECK(previous.renderSequence > initial.renderSequence);
    juce::AudioBuffer<float> output(fire::lfo_bank::capacity, 257);
    int processed = 0;
    for (int count : {1, 37, 257})
    {
        manager.processBlock(output, 48000.0f, nullptr, count);
        const auto current = processor.getLfoVisualState(0);
        const auto highest = processor.getLfoVisualState(15);
        processed += count;
        CHECK_FALSE(manager.isDawPlaying());
        CHECK(current.renderSequence > previous.renderSequence);
        CHECK((current.renderSequence & 1u) == 0u);
        CHECK(current.phase == Catch::Approx(output.getSample(0, count - 1)).margin(2.0e-6f));
        CHECK(current.phase == Catch::Approx(0.25f + (processed - 1) * 2.0f / 48000.0f).margin(2.0e-5f));
        CHECK(highest.phase == Catch::Approx(output.getSample(15, count - 1)).margin(2.0e-6f));
        CHECK(highest.renderSequence == current.renderSequence);
        CHECK(processor.getLfoVisualState(7).phase == -1.0f);
        previous = current;
    }
    manager.processBlock(output, 48000.0f, nullptr, 0);
    CHECK(processor.getLfoVisualState(0).renderSequence == previous.renderSequence);

    set(processor, 0, LFO_PHASE_ID, 0.75f);
    manager.processBlock(output, 48000.0f, nullptr, 1);
    const auto corrected = processor.getLfoVisualState(0);
    CHECK(corrected.phase == Catch::Approx(output.getSample(0, 0)).margin(2.0e-6f));
    CHECK(std::abs(corrected.phase - manager.getLfoPhase(0)) > 0.25f);

    manager.reset();
    const auto reset = processor.getLfoVisualState(0);
    CHECK(reset.phase == -1.0f);
    CHECK(reset.renderSequence > corrected.renderSequence);
    manager.prepare({96000.0, 64, 1});
    const auto prepared = processor.getLfoVisualState(0);
    CHECK(prepared.phase == -1.0f);
    CHECK(prepared.renderSequence > reset.renderSequence);
    manager.processBlock(output, 96000.0f, nullptr, 1);
    CHECK(processor.getLfoVisualState(0).renderSequence > prepared.renderSequence);
    CHECK(processor.getLfoVisualState(0).phase == Catch::Approx(0.75f).margin(2.0e-6f));
    CHECK(processor.getLfoVisualState(-1).phase == -1.0f);
    CHECK(processor.getLfoVisualState(fire::lfo_bank::capacity).phase == -1.0f);
}

TEST_CASE("Stopped transport and missing playheads keep visual phase advancing in free and sync modes",
          "[lfo-visual][lfo][dsp][phase][stopped]")
{
    for (bool synced : {false, true})
        for (bool usePlayhead : {false, true})
        {
            FireAudioProcessor processor;
            configure(processor, 0, synced);
            auto& manager = processor.getLfoManager();
            manager.prepare({48000.0, 64, 1});
            PlayHead playhead;
            playhead.position.setIsPlaying(false);
            playhead.position.setBpm(90.0);
            // Stopped timeline coordinates must not pin the still-running LFO.
            playhead.position.setPpqPosition(123.0);
            playhead.position.setTimeInSeconds(456.0);
            juce::AudioBuffer<float> output(fire::lfo_bank::capacity, 64);
            const auto frequency = synced && usePlayhead ? 1.5f : 2.0f;
            std::uint64_t sequence = 0;
            for (int block = 0; block < 3; ++block)
            {
                manager.processBlock(output, 48000.0f, usePlayhead ? &playhead : nullptr, 64);
                const auto visual = processor.getLfoVisualState(0);
                CAPTURE(synced, usePlayhead, block);
                CHECK_FALSE(processor.isDawPlaying());
                CHECK(visual.renderSequence > sequence);
                CHECK(visual.phase == Catch::Approx(0.25f + ((block + 1) * 64 - 1) * frequency / 48000.0f).margin(2.0e-5f));
                CHECK(visual.phase == Catch::Approx(output.getSample(0, 63)).margin(2.0e-6f));
                sequence = visual.renderSequence;
            }
            set(processor, 0, "lfoPresent", 0.0f);
            manager.processBlock(output, 48000.0f, usePlayhead ? &playhead : nullptr, 64);
            CHECK(processor.getLfoVisualState(0).phase == -1.0f);
            CHECK(processor.getLfoVisualState(0).renderSequence > sequence);
        }
}

TEST_CASE("Playing visual phase already includes the host timeline and Phase offset",
          "[lfo-visual][lfo][dsp][phase][playing]")
{
    for (bool synced : {false, true})
    {
        FireAudioProcessor processor;
        configure(processor, 0, synced);
        processor.getLfoManager().prepare({48000.0, 64, 1});
        PlayHead playhead;
        playhead.position.setIsPlaying(true);
        playhead.position.setBpm(120.0);
        playhead.position.setPpqPosition(0.125);
        playhead.position.setTimeInSeconds(0.125);
        juce::AudioBuffer<float> output(fire::lfo_bank::capacity, 64);
        processor.getLfoManager().processBlock(output, 48000.0f, &playhead, 64);
        const auto visual = processor.getLfoVisualState(0);
        CHECK(processor.isDawPlaying());
        const auto start = synced ? 0.375f : 0.5f;
        CHECK(visual.phase == Catch::Approx(start + 63 * 2.0f / 48000.0f).margin(2.0e-5f));
        CHECK(visual.phase == Catch::Approx(output.getSample(0, 63)).margin(2.0e-6f));
    }
}
