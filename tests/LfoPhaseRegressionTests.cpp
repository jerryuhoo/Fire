#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr float sampleRate = 48000.0f;
constexpr int preparedBlockSize = 64;
constexpr int testBlockSize = 37;
constexpr int phaseTransitionSamples = 480;
constexpr float freeRateHz = 100.0f;
constexpr int quarterNoteSyncRateIndex = 8;
constexpr float stoppedHostBpm = 120.0f;

class TestPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override { return position; }

    PositionInfo position;
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String lfoParameter(const juce::String& baseID)
{
    return ParameterIDAndName::getIDString(baseID, 0);
}

LfoData makeLinearPhaseProbeShape()
{
    LfoData shape;
    shape.points = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
    shape.curvatures = { 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

float wrapPhase(float phase)
{
    phase -= std::floor(phase);
    return phase;
}

float phaseDelta(bool syncMode)
{
    if (! syncMode)
        return freeRateHz / sampleRate;

    // Rate index 8 is one quarter note per cycle.
    return stoppedHostBpm / (60.0f * sampleRate);
}

void configureLfo(FireAudioProcessor& processor,
                  bool syncMode,
                  float phaseOffset)
{
    setPlainParameter(processor, lfoParameter(LFO_SYNC_MODE_ID), syncMode ? 1.0f : 0.0f);
    setPlainParameter(processor,
                      lfoParameter(LFO_RATE_SYNC_ID),
                      static_cast<float>(quarterNoteSyncRateIndex));
    setPlainParameter(processor, lfoParameter(LFO_RATE_HZ_ID), freeRateHz);
    setPlainParameter(processor, lfoParameter(LFO_PHASE_ID), phaseOffset);
    setPlainParameter(processor, lfoParameter(LFO_SMOOTH_ID), 0.0f);

    auto& manager = processor.getLfoManager();
    manager.prepare({ sampleRate,
                      static_cast<juce::uint32>(preparedBlockSize),
                      4 });
    manager.reset();
    manager.setLfoData(0, makeLinearPhaseProbeShape());
}

juce::AudioBuffer<float> processLfoBlock(LfoManager& manager,
                                         int numSamples,
                                         juce::AudioPlayHead* playHead = nullptr)
{
    juce::AudioBuffer<float> output(4, numSamples);
    output.clear();
    manager.processBlock(output, sampleRate, playHead, numSamples);
    return output;
}

void checkLinearBlock(const juce::AudioBuffer<float>& output,
                      float firstPhase,
                      float delta)
{
    REQUIRE(output.getNumChannels() >= 1);
    float maximumError = 0.0f;
    bool allSamplesAreFinite = true;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        const float actual = output.getSample(0, sample);
        allSamplesAreFinite = allSamplesAreFinite && std::isfinite(actual);
        maximumError = std::max(
            maximumError,
            std::abs(actual - wrapPhase(firstPhase
                                        + static_cast<float>(sample) * delta)));
    }

    CAPTURE(firstPhase, delta, maximumError);
    CHECK(allSamplesAreFinite);
    CHECK(maximumError <= 2.0e-5f);
}

void checkLinearRange(const juce::AudioBuffer<float>& output,
                      int firstSample,
                      int numSamples,
                      float firstPhase,
                      float delta)
{
    REQUIRE(output.getNumChannels() >= 1);
    REQUIRE(firstSample >= 0);
    REQUIRE(numSamples >= 0);
    REQUIRE(firstSample + numSamples <= output.getNumSamples());

    float maximumError = 0.0f;
    bool allSamplesAreFinite = true;
    for (int offset = 0; offset < numSamples; ++offset)
    {
        const float actual = output.getSample(0, firstSample + offset);
        allSamplesAreFinite = allSamplesAreFinite && std::isfinite(actual);
        maximumError = std::max(
            maximumError,
            std::abs(actual - wrapPhase(firstPhase
                                        + static_cast<float>(offset) * delta)));
    }

    CAPTURE(firstSample, numSamples, firstPhase, delta, maximumError);
    CHECK(allSamplesAreFinite);
    CHECK(maximumError <= 2.0e-5f);
}
} // namespace

TEST_CASE("Stopped LFO phase offset is applied once without a playhead",
          "[processor][lfo][phase][stopped][no-playhead]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool syncMode : std::array { false, true })
    {
        CAPTURE(syncMode);
        FireAudioProcessor zeroPhaseProcessor;
        FireAudioProcessor shiftedPhaseProcessor;
        configureLfo(zeroPhaseProcessor, syncMode, 0.0f);
        configureLfo(shiftedPhaseProcessor, syncMode, 0.25f);

        auto& zeroPhase = zeroPhaseProcessor.getLfoManager();
        auto& shiftedPhase = shiftedPhaseProcessor.getLfoManager();
        const float delta = phaseDelta(syncMode);
        int processedSamples = 0;

        // Repeated callbacks must advance one continuous oscillator. The Phase
        // knob is an offset, not an amount to add again at every block edge.
        for (int block = 0; block < 3; ++block)
        {
            const auto zeroOutput = processLfoBlock(zeroPhase, testBlockSize);
            const auto shiftedOutput = processLfoBlock(shiftedPhase, testBlockSize);
            const float zeroFirstPhase = static_cast<float>(processedSamples) * delta;
            const float shiftedFirstPhase = 0.25f + zeroFirstPhase;

            checkLinearBlock(zeroOutput, zeroFirstPhase, delta);
            checkLinearBlock(shiftedOutput, shiftedFirstPhase, delta);
            CHECK(shiftedOutput.getSample(0, 0) - zeroOutput.getSample(0, 0)
                  == Catch::Approx(0.25f).margin(2.0e-5f));

            processedSamples += testBlockSize;
            CHECK(zeroPhase.getLfoPhase(0)
                  == Catch::Approx(wrapPhase(static_cast<float>(processedSamples) * delta))
                         .margin(2.0e-5f));
            CHECK(shiftedPhase.getLfoPhase(0)
                  == Catch::Approx(wrapPhase(0.25f
                                             + static_cast<float>(processedSamples) * delta))
                         .margin(2.0e-5f));
        }

        // Changing Phase retains the currently audible event sample, then
        // applies only the offset delta (+0.25 here) over 10 ms. The oscillator
        // remains free-running throughout instead of restarting at a block.
        setPlainParameter(shiftedPhaseProcessor, lfoParameter(LFO_PHASE_ID), 0.5f);
        const float oldEventPhase = 0.25f
                                    + static_cast<float>(processedSamples) * delta;
        const auto transition = processLfoBlock(
            shiftedPhase,
            phaseTransitionSamples + testBlockSize);
        CHECK(transition.getSample(0, 0)
              == Catch::Approx(wrapPhase(oldEventPhase)).margin(2.0e-5f));
        checkLinearRange(
            transition,
            phaseTransitionSamples,
            testBlockSize,
            0.5f + static_cast<float>(processedSamples
                                      + phaseTransitionSamples)
                       * delta,
            delta);

        processedSamples += phaseTransitionSamples + testBlockSize;
        CHECK(shiftedPhase.getLfoPhase(0)
              == Catch::Approx(wrapPhase(0.5f
                                         + static_cast<float>(processedSamples) * delta))
                     .margin(2.0e-5f));

        const auto settled = processLfoBlock(shiftedPhase, testBlockSize);
        checkLinearBlock(settled,
                         0.5f + static_cast<float>(processedSamples) * delta,
                         delta);
        processedSamples += testBlockSize;

        CHECK_FALSE(zeroPhase.isDawPlaying());
        CHECK_FALSE(shiftedPhase.isDawPlaying());
    }
}

TEST_CASE("Playing synced LFO remains locked to absolute PPQ plus Phase",
          "[processor][lfo][phase][playing][ppq]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    configureLfo(processor, true, 0.25f);
    auto& manager = processor.getLfoManager();

    TestPlayHead playHead;
    playHead.position.setIsPlaying(true);
    playHead.position.setBpm(stoppedHostBpm);
    playHead.position.setTimeSignature(
        juce::AudioPlayHead::TimeSignature { 4, 4 });

    constexpr double initialPpq = 3.125;
    const double ppqPerSample = static_cast<double>(stoppedHostBpm)
                                / (60.0 * static_cast<double>(sampleRate));
    const float delta = phaseDelta(true);

    playHead.position.setPpqPosition(initialPpq);
    const auto first = processLfoBlock(manager, testBlockSize, &playHead);
    checkLinearBlock(first, wrapPhase(static_cast<float>(initialPpq) + 0.25f), delta);
    CHECK(manager.isDawPlaying());

    const double secondPpq = initialPpq + ppqPerSample * testBlockSize;
    playHead.position.setPpqPosition(secondPpq);
    const auto second = processLfoBlock(manager, testBlockSize, &playHead);
    checkLinearBlock(second, wrapPhase(static_cast<float>(secondPpq) + 0.25f), delta);

    setPlainParameter(processor, lfoParameter(LFO_PHASE_ID), 0.5f);
    const double changedPpq = secondPpq + ppqPerSample * testBlockSize;
    playHead.position.setPpqPosition(changedPpq);
    const auto changed = processLfoBlock(manager,
                                         phaseTransitionSamples + testBlockSize,
                                         &playHead);
    CHECK(changed.getSample(0, 0)
          == Catch::Approx(wrapPhase(static_cast<float>(changedPpq) + 0.25f))
                 .margin(2.0e-5f));
    checkLinearRange(
        changed,
        phaseTransitionSamples,
        testBlockSize,
        wrapPhase(static_cast<float>(changedPpq
                                     + ppqPerSample * phaseTransitionSamples)
                  + 0.5f),
        delta);

    const double finalPpq = changedPpq
                            + ppqPerSample
                                  * (phaseTransitionSamples + testBlockSize);
    playHead.position.setPpqPosition(finalPpq);
    const auto final = processLfoBlock(manager, testBlockSize, &playHead);
    checkLinearBlock(final, wrapPhase(static_cast<float>(finalPpq) + 0.5f), delta);
}

TEST_CASE("LFO reset reapplies a nonzero Phase offset on the first sample",
          "[processor][lfo][phase][reset]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool syncMode : std::array { false, true })
    {
        CAPTURE(syncMode);
        FireAudioProcessor processor;
        configureLfo(processor, syncMode, 0.25f);
        auto& manager = processor.getLfoManager();
        const float delta = phaseDelta(syncMode);

        // Initialise both the engine and the manager's applied-offset state,
        // then reset while the parameter itself remains at 0.25.
        const auto beforeReset = processLfoBlock(manager, testBlockSize);
        checkLinearBlock(beforeReset, 0.25f, delta);
        manager.reset();

        const auto afterReset = processLfoBlock(manager, testBlockSize);
        checkLinearBlock(afterReset, 0.25f, delta);
        CHECK(afterReset.getSample(0, 0)
              == Catch::Approx(0.25f).margin(2.0e-5f));
        CHECK(manager.getLfoPhase(0)
              == Catch::Approx(wrapPhase(0.25f + testBlockSize * delta))
                     .margin(2.0e-5f));
    }
}

TEST_CASE("LFO exits an absolute playing anchor without reapplying Phase",
          "[processor][lfo][phase][transport][stopped]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool syncMode : std::array { false, true })
    {
        CAPTURE(syncMode);
        FireAudioProcessor processor;
        configureLfo(processor, syncMode, 0.25f);
        auto& manager = processor.getLfoManager();
        const float delta = phaseDelta(syncMode);

        TestPlayHead playHead;
        playHead.position.setIsPlaying(true);
        playHead.position.setBpm(stoppedHostBpm);
        if (syncMode)
            playHead.position.setPpqPosition(0.3);
        else
            playHead.position.setTimeInSeconds(0.003); // 100 Hz * 3 ms = phase 0.3

        constexpr float anchoredFirstPhase = 0.55f; // timeline 0.3 + Phase 0.25
        const auto anchored = processLfoBlock(manager, testBlockSize, &playHead);
        checkLinearBlock(anchored, anchoredFirstPhase, delta);
        CHECK(manager.isDawPlaying());

        playHead.position.setIsPlaying(false);
        float stoppedFirstPhase = wrapPhase(anchoredFirstPhase
                                            + testBlockSize * delta);
        const auto firstStopped = processLfoBlock(manager, testBlockSize, &playHead);
        checkLinearBlock(firstStopped, stoppedFirstPhase, delta);

        stoppedFirstPhase = wrapPhase(stoppedFirstPhase
                                      + testBlockSize * delta);
        const auto secondStopped = processLfoBlock(manager, testBlockSize, &playHead);
        checkLinearBlock(secondStopped, stoppedFirstPhase, delta);
        CHECK_FALSE(manager.isDawPlaying());

        const float expectedFinalPhase = wrapPhase(stoppedFirstPhase
                                                   + testBlockSize * delta);
        CHECK(manager.getLfoPhase(0)
              == Catch::Approx(expectedFinalPhase).margin(2.0e-5f));
    }
}

TEST_CASE("Playing LFO without its required timeline coordinate free-runs",
          "[processor][lfo][phase][playing][missing-anchor]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool syncMode : std::array { false, true })
    {
        CAPTURE(syncMode);
        FireAudioProcessor processor;
        configureLfo(processor, syncMode, 0.25f);
        auto& manager = processor.getLfoManager();
        const float delta = phaseDelta(syncMode);

        TestPlayHead playHead;
        playHead.position.setIsPlaying(true);
        playHead.position.setBpm(stoppedHostBpm);
        // Deliberately omit PPQ in Sync mode and timeInSeconds in Free mode.

        int processedSamples = 0;
        for (int block = 0; block < 2; ++block)
        {
            const float expectedFirstPhase = 0.25f
                                           + static_cast<float>(processedSamples) * delta;
            const auto output = processLfoBlock(manager, testBlockSize, &playHead);
            checkLinearBlock(output, expectedFirstPhase, delta);
            processedSamples += testBlockSize;
        }

        setPlainParameter(processor, lfoParameter(LFO_PHASE_ID), 0.5f);
        const float oldEventPhase = 0.25f
                                    + static_cast<float>(processedSamples) * delta;
        const auto transition = processLfoBlock(
            manager,
            phaseTransitionSamples + testBlockSize,
            &playHead);
        CHECK(transition.getSample(0, 0)
              == Catch::Approx(wrapPhase(oldEventPhase)).margin(2.0e-5f));
        checkLinearRange(
            transition,
            phaseTransitionSamples,
            testBlockSize,
            0.5f + static_cast<float>(processedSamples
                                      + phaseTransitionSamples)
                       * delta,
            delta);
        processedSamples += phaseTransitionSamples + testBlockSize;

        const auto settled = processLfoBlock(manager,
                                             testBlockSize,
                                             &playHead);
        checkLinearBlock(settled,
                         0.5f + static_cast<float>(processedSamples) * delta,
                         delta);
        processedSamples += testBlockSize;

        CHECK(manager.isDawPlaying());
        CHECK(manager.getLfoPhase(0)
              == Catch::Approx(wrapPhase(0.5f
                                         + static_cast<float>(processedSamples) * delta))
                     .margin(2.0e-5f));
    }
}
