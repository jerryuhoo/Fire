#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double hostBpm = 120.0;
constexpr int transitionSamples = 480; // 10 ms at 48 kHz.
constexpr int eventSample = 257 * 24;
constexpr int endpointWindowSamples = 64;
constexpr float comparisonTolerance = 3.0e-5f;

const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

class TimelinePlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override { return position; }

    void setCallbackStart(int absoluteSample)
    {
        position = {};
        position.setIsPlaying(true);
        position.setBpm(hostBpm);
        position.setTimeSignature(TimeSignature { 4, 4 });
        position.setTimeInSeconds(static_cast<double>(absoluteSample) / sampleRate);
        position.setPpqPosition(static_cast<double>(absoluteSample) * hostBpm
                                / (60.0 * sampleRate));
    }

    void setPlayingWithoutTimeline()
    {
        position = {};
        position.setIsPlaying(true);
        position.setBpm(hostBpm);
        position.setTimeSignature(TimeSignature { 4, 4 });
    }

private:
    PositionInfo position;
};

struct LfoSettings
{
    bool sync = false;
    int syncRateIndex = 8;
    float freeRateHz = 1.0f;
    float phase = 0.05f;
};

struct ParameterEvent
{
    int absoluteSample = 0;
    LfoSettings settings;
};

struct TransitionScenario
{
    const char* name = "";
    LfoSettings initial;
    LfoSettings target;
    bool useAbsolutePlayingTimeline = true;
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

void applySettings(FireAudioProcessor& processor, const LfoSettings& settings)
{
    setPlainParameter(processor,
                      lfoParameter(LFO_SYNC_MODE_ID),
                      settings.sync ? 1.0f : 0.0f);
    setPlainParameter(processor,
                      lfoParameter(LFO_RATE_SYNC_ID),
                      static_cast<float>(settings.syncRateIndex));
    setPlainParameter(processor,
                      lfoParameter(LFO_RATE_HZ_ID),
                      settings.freeRateHz);
    setPlainParameter(processor, lfoParameter(LFO_PHASE_ID), settings.phase);
    setPlainParameter(processor, lfoParameter(LFO_SMOOTH_ID), 0.0f);
}

LfoData makeContinuousTrianglePhaseProbeShape()
{
    LfoData shape;
    shape.points = {
        { 0.0f, 0.5f },
        { 0.25f, 1.0f },
        { 0.75f, 0.0f },
        { 1.0f, 0.5f }
    };
    shape.curvatures = { 0.0f, 0.0f, 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

LfoManager& prepareTestLfo(FireAudioProcessor& processor,
                           const LfoSettings& settings,
                           int maximumBlockSize = 8192)
{
    applySettings(processor, settings);
    auto& manager = processor.getLfoManager();
    manager.prepare({ sampleRate,
                      static_cast<juce::uint32>(maximumBlockSize),
                      4 });
    manager.reset();
    manager.setLfoData(0, makeContinuousTrianglePhaseProbeShape());
    return manager;
}

juce::AudioBuffer<float> processLfoBlock(LfoManager& manager,
                                         int numSamples,
                                         juce::AudioPlayHead* playHead = nullptr)
{
    juce::AudioBuffer<float> output(4, numSamples);
    output.clear();
    manager.processBlock(output,
                         static_cast<float>(sampleRate),
                         playHead,
                         numSamples);
    return output;
}

juce::AudioBuffer<float> processTimelineBlock(LfoManager& manager,
                                              TimelinePlayHead& playHead,
                                              int timelineSample,
                                              int numSamples)
{
    playHead.setCallbackStart(timelineSample);
    return processLfoBlock(manager, numSamples, &playHead);
}

std::vector<float> renderLfo(const LfoSettings& initialSettings,
                             const std::vector<ParameterEvent>& events,
                             const std::vector<int>& callbackPattern,
                             int totalSamples,
                             bool useAbsolutePlayingTimeline)
{
    REQUIRE_FALSE(callbackPattern.empty());
    REQUIRE(totalSamples > 0);

    FireAudioProcessor processor;
    auto& manager = prepareTestLfo(processor, initialSettings, 257);

    TimelinePlayHead playHead;
    std::vector<float> output(static_cast<size_t>(totalSamples), 0.0f);
    size_t eventIndex = 0;
    size_t patternIndex = 0;
    int absoluteSample = 0;

    while (absoluteSample < totalSamples)
    {
        while (eventIndex < events.size()
               && events[eventIndex].absoluteSample == absoluteSample)
        {
            applySettings(processor, events[eventIndex].settings);
            ++eventIndex;
        }

        REQUIRE((eventIndex >= events.size()
                 || events[eventIndex].absoluteSample > absoluteSample));

        int samplesThisCallback = callbackPattern[patternIndex % callbackPattern.size()];
        ++patternIndex;
        samplesThisCallback = std::min(samplesThisCallback,
                                       totalSamples - absoluteSample);
        if (eventIndex < events.size())
            samplesThisCallback = std::min(samplesThisCallback,
                                           events[eventIndex].absoluteSample
                                               - absoluteSample);
        REQUIRE(samplesThisCallback > 0);

        juce::AudioBuffer<float> lfoOutput(4, samplesThisCallback);
        lfoOutput.clear();
        juce::AudioPlayHead* playHeadForCallback = nullptr;
        if (useAbsolutePlayingTimeline)
        {
            playHead.setCallbackStart(absoluteSample);
            playHeadForCallback = &playHead;
        }

        manager.processBlock(lfoOutput,
                             static_cast<float>(sampleRate),
                             playHeadForCallback,
                             samplesThisCallback);
        std::copy_n(lfoOutput.getReadPointer(0),
                    samplesThisCallback,
                    output.begin() + absoluteSample);
        absoluteSample += samplesThisCallback;
    }

    REQUIRE(eventIndex == events.size());
    return output;
}

bool allFinite(const std::vector<float>& samples)
{
    return std::all_of(samples.begin(), samples.end(), [](float sample)
    {
        return std::isfinite(sample);
    });
}

float maximumError(const std::vector<float>& lhs,
                   const std::vector<float>& rhs,
                   int firstSample,
                   int endSample)
{
    REQUIRE(lhs.size() == rhs.size());
    REQUIRE(firstSample >= 0);
    REQUIRE(endSample >= firstSample);
    REQUIRE(endSample <= static_cast<int>(lhs.size()));

    float result = 0.0f;
    for (int sample = firstSample; sample < endSample; ++sample)
        result = std::max(result,
                          std::abs(lhs[static_cast<size_t>(sample)]
                                   - rhs[static_cast<size_t>(sample)]));
    return result;
}

float maximumAdjacentStep(const std::vector<float>& samples,
                          int firstSample,
                          int endSample)
{
    REQUIRE(firstSample > 0);
    REQUIRE(endSample > firstSample);
    REQUIRE(endSample <= static_cast<int>(samples.size()));

    float result = 0.0f;
    for (int sample = firstSample; sample < endSample; ++sample)
        result = std::max(result,
                          std::abs(samples[static_cast<size_t>(sample)]
                                   - samples[static_cast<size_t>(sample - 1)]));
    return result;
}

float maximumBufferError(const juce::AudioBuffer<float>& lhs,
                         const juce::AudioBuffer<float>& rhs,
                         int firstSample,
                         int endSample)
{
    REQUIRE(lhs.getNumChannels() >= 1);
    REQUIRE(rhs.getNumChannels() >= 1);
    REQUIRE(lhs.getNumSamples() == rhs.getNumSamples());
    REQUIRE(firstSample >= 0);
    REQUIRE(endSample >= firstSample);
    REQUIRE(endSample <= lhs.getNumSamples());

    float result = 0.0f;
    for (int sample = firstSample; sample < endSample; ++sample)
        result = std::max(result,
                          std::abs(lhs.getSample(0, sample)
                                   - rhs.getSample(0, sample)));
    return result;
}

bool allFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite(buffer.getSample(channel, sample)))
                return false;
    return true;
}

void checkTimelineCorrection(const juce::AudioBuffer<float>& subject,
                             const juce::AudioBuffer<float>& continuousOld,
                             const juce::AudioBuffer<float>& canonicalNew)
{
    REQUIRE(subject.getNumSamples() >= transitionSamples
                                          + endpointWindowSamples);
    REQUIRE(subject.getNumSamples() == continuousOld.getNumSamples());
    REQUIRE(subject.getNumSamples() == canonicalNew.getNumSamples());
    REQUIRE(allFinite(subject));

    const float endpointSeparation = std::abs(continuousOld.getSample(0, 0)
                                              - canonicalNew.getSample(0, 0));
    const float firstSampleError = std::abs(subject.getSample(0, 0)
                                            - continuousOld.getSample(0, 0));
    const float settledError = maximumBufferError(subject,
                                                  canonicalNew,
                                                  transitionSamples,
                                                  subject.getNumSamples());
    CAPTURE(endpointSeparation, firstSampleError, settledError);
    REQUIRE(endpointSeparation >= 0.08f);
    CHECK(firstSampleError <= comparisonTolerance);
    CHECK(settledError <= comparisonTolerance);
}
} // namespace

TEST_CASE("Live LFO timing parameters transition continuously to the new timeline",
          "[processor][lfo][live-parameter][transition]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    constexpr LfoSettings freeInitial { false, 8, 0.5f, 0.05f };
    constexpr LfoSettings syncInitial { true, 11, 0.5f, 0.05f };
    const std::array scenarios {
        TransitionScenario { "playing Free rate 0.5 Hz to 4 Hz",
                             freeInitial,
                             { false, 8, 4.0f, 0.05f },
                             true },
        TransitionScenario { "playing Sync division one bar to one quarter",
                             syncInitial,
                             { true, 8, 1.0f, 0.05f },
                             true },
        TransitionScenario { "playing Sync to Free",
                             syncInitial,
                             { false, 8, 4.0f, 0.05f },
                             true },
        TransitionScenario { "playing Free to Sync",
                             freeInitial,
                             { true, 8, 1.0f, 0.05f },
                             true },
        TransitionScenario { "playing Phase 0.05 to 0.45",
                             freeInitial,
                             { false, 8, 0.5f, 0.45f },
                             true },
        TransitionScenario { "stopped no-playhead Phase 0.05 to 0.45",
                             freeInitial,
                             { false, 8, 0.5f, 0.45f },
                             false }
    };

    constexpr int totalSamples = eventSample + transitionSamples
                                 + endpointWindowSamples;

    for (const auto& scenario : scenarios)
    {
        DYNAMIC_SECTION(scenario.name)
        {
            const std::vector<ParameterEvent> event {
                { eventSample, scenario.target }
            };
            const auto subject = renderLfo(scenario.initial,
                                           event,
                                           fixedCallbacks,
                                           totalSamples,
                                           scenario.useAbsolutePlayingTimeline);
            const auto irregular = renderLfo(scenario.initial,
                                             event,
                                             irregularCallbacks,
                                             totalSamples,
                                             scenario.useAbsolutePlayingTimeline);
            const auto alwaysOld = renderLfo(scenario.initial,
                                             {},
                                             fixedCallbacks,
                                             totalSamples,
                                             scenario.useAbsolutePlayingTimeline);
            const auto alwaysNew = renderLfo(scenario.target,
                                             {},
                                             fixedCallbacks,
                                             totalSamples,
                                             scenario.useAbsolutePlayingTimeline);

            REQUIRE(allFinite(subject));
            REQUIRE(allFinite(irregular));
            const float preEventError = maximumError(subject,
                                                     alwaysOld,
                                                     0,
                                                     eventSample);
            const float endpointSeparation = std::abs(
                alwaysOld[static_cast<size_t>(eventSample)]
                - alwaysNew[static_cast<size_t>(eventSample)]);
            const float firstSampleError = std::abs(
                subject[static_cast<size_t>(eventSample)]
                - alwaysOld[static_cast<size_t>(eventSample)]);
            const float settledError = maximumError(subject,
                                                    alwaysNew,
                                                    eventSample
                                                        + transitionSamples,
                                                    totalSamples);
            // Without an absolute host coordinate, two independently
            // accumulated float phases differ slightly depending on whether
            // the Phase offset was present from reset or applied as a delta.
            // Keep the audible event boundary strict while allowing that
            // sub-millipercent free-running endpoint roundoff.
            const float settledTolerance = scenario.useAbsolutePlayingTimeline
                                               ? comparisonTolerance
                                               : 3.0e-4f;
            const float partitionError = maximumError(subject,
                                                      irregular,
                                                      0,
                                                      totalSamples);

            CAPTURE(preEventError,
                    endpointSeparation,
                    firstSampleError,
                    settledError,
                    settledTolerance,
                    partitionError);
            CHECK(preEventError <= comparisonTolerance);
            REQUIRE(endpointSeparation >= 0.08f);

            // The event sample is still the old audible timeline. A hard
            // setPhase() here produces a visible discontinuity.
            CHECK(firstSampleError <= comparisonTolerance);

            // At 10 ms the transition is complete and follows the same
            // absolute host timeline as a processor that started at the final
            // settings. This also prevents a merely slew-limited wrong rate.
            CHECK(settledError <= settledTolerance);
            CHECK(partitionError <= comparisonTolerance);
        }
    }
}

TEST_CASE("Rapid live LFO changes are continuous and latest-target wins",
          "[processor][lfo][live-parameter][transition][rapid][partition]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    constexpr LfoSettings initial { false, 8, 0.5f, 0.05f };
    constexpr LfoSettings firstTarget { false, 8, 0.5f, 0.35f };
    constexpr LfoSettings secondTarget { false, 8, 0.5f, 0.75f };
    constexpr LfoSettings latestTarget { false, 8, 0.5f, 0.20f };
    constexpr int secondEventSample = eventSample + 160;
    constexpr int latestEventSample = eventSample + 320;
    constexpr int totalSamples = latestEventSample + transitionSamples
                                 + endpointWindowSamples;

    const std::vector<ParameterEvent> events {
        { eventSample, firstTarget },
        { secondEventSample, secondTarget },
        { latestEventSample, latestTarget }
    };
    const auto subject = renderLfo(initial,
                                   events,
                                   fixedCallbacks,
                                   totalSamples,
                                   true);
    const auto irregular = renderLfo(initial,
                                     events,
                                     irregularCallbacks,
                                     totalSamples,
                                     true);
    const auto alwaysInitial = renderLfo(initial,
                                         {},
                                         fixedCallbacks,
                                         totalSamples,
                                         true);
    const auto alwaysLatest = renderLfo(latestTarget,
                                        {},
                                        fixedCallbacks,
                                        totalSamples,
                                        true);

    REQUIRE(allFinite(subject));
    REQUIRE(allFinite(irregular));
    const float firstSampleError = std::abs(
        subject[static_cast<size_t>(eventSample)]
        - alwaysInitial[static_cast<size_t>(eventSample)]);
    const float maximumStep = maximumAdjacentStep(subject,
                                                  eventSample,
                                                  latestEventSample
                                                      + transitionSamples);
    const float settledLatestError = maximumError(
        subject,
        alwaysLatest,
        latestEventSample + transitionSamples,
        totalSamples);
    const float wrongIntermediateSeparation = maximumError(
        alwaysLatest,
        renderLfo(secondTarget,
                  {},
                  fixedCallbacks,
                  totalSamples,
                  true),
        latestEventSample + transitionSamples,
        totalSamples);
    const float partitionError = maximumError(subject,
                                              irregular,
                                              0,
                                              totalSamples);

    CAPTURE(firstSampleError,
            maximumStep,
            settledLatestError,
            wrongIntermediateSeparation,
            partitionError);
    CHECK(firstSampleError <= comparisonTolerance);
    CHECK(maximumStep <= 0.01f);
    REQUIRE(wrongIntermediateSeparation >= 0.20f);
    CHECK(settledLatestError <= comparisonTolerance);
    CHECK(partitionError <= comparisonTolerance);
}

TEST_CASE("LFO absolute timeline anchors transition without an audible jump",
          "[processor][lfo][live-parameter][transition][timeline][anchor]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    constexpr LfoSettings settings { false, 8, 0.5f, 0.05f };
    constexpr int warmupSamples = 1024;
    constexpr int eventBlockSamples = transitionSamples
                                      + endpointWindowSamples;

    SECTION("playing seek and loop")
    {
        struct AnchorJump
        {
            const char* name;
            int initialTimelineSample;
            int targetTimelineSample;
        };
        const std::array jumps {
            AnchorJump { "forward seek", 0, 24000 },
            AnchorJump { "backward loop", 24000, 4096 }
        };

        for (const auto& jump : jumps)
        {
            DYNAMIC_SECTION(jump.name)
            {
                FireAudioProcessor subjectProcessor;
                FireAudioProcessor oldProcessor;
                FireAudioProcessor newProcessor;
                auto& subject = prepareTestLfo(subjectProcessor, settings);
                auto& continuousOld = prepareTestLfo(oldProcessor, settings);
                auto& canonicalNew = prepareTestLfo(newProcessor, settings);
                TimelinePlayHead subjectPlayHead;
                TimelinePlayHead oldPlayHead;
                TimelinePlayHead newPlayHead;

                const auto subjectWarmup = processTimelineBlock(
                    subject,
                    subjectPlayHead,
                    jump.initialTimelineSample,
                    warmupSamples);
                const auto oldWarmup = processTimelineBlock(
                    continuousOld,
                    oldPlayHead,
                    jump.initialTimelineSample,
                    warmupSamples);
                REQUIRE(maximumBufferError(subjectWarmup,
                                           oldWarmup,
                                           0,
                                           warmupSamples)
                        <= comparisonTolerance);

                const auto subjectEvent = processTimelineBlock(
                    subject,
                    subjectPlayHead,
                    jump.targetTimelineSample,
                    eventBlockSamples);
                const auto oldEvent = processTimelineBlock(
                    continuousOld,
                    oldPlayHead,
                    jump.initialTimelineSample + warmupSamples,
                    eventBlockSamples);
                const auto newEvent = processTimelineBlock(
                    canonicalNew,
                    newPlayHead,
                    jump.targetTimelineSample,
                    eventBlockSamples);
                checkTimelineCorrection(subjectEvent, oldEvent, newEvent);
            }
        }
    }

    SECTION("stopped or missing-anchor free-run regains an absolute anchor")
    {
        for (const bool playingWithoutAnchor : std::array { false, true })
        {
            DYNAMIC_SECTION("playingWithoutAnchor=" << playingWithoutAnchor)
            {
                FireAudioProcessor subjectProcessor;
                FireAudioProcessor oldProcessor;
                FireAudioProcessor newProcessor;
                auto& subject = prepareTestLfo(subjectProcessor, settings);
                auto& continuousOld = prepareTestLfo(oldProcessor, settings);
                auto& canonicalNew = prepareTestLfo(newProcessor, settings);
                TimelinePlayHead subjectPlayHead;
                TimelinePlayHead oldPlayHead;
                TimelinePlayHead newPlayHead;

                juce::AudioPlayHead* subjectWarmupPlayHead = nullptr;
                juce::AudioPlayHead* oldWarmupPlayHead = nullptr;
                if (playingWithoutAnchor)
                {
                    subjectPlayHead.setPlayingWithoutTimeline();
                    oldPlayHead.setPlayingWithoutTimeline();
                    subjectWarmupPlayHead = &subjectPlayHead;
                    oldWarmupPlayHead = &oldPlayHead;
                }

                const auto subjectWarmup = processLfoBlock(
                    subject,
                    warmupSamples,
                    subjectWarmupPlayHead);
                const auto oldWarmup = processLfoBlock(
                    continuousOld,
                    warmupSamples,
                    oldWarmupPlayHead);
                REQUIRE(maximumBufferError(subjectWarmup,
                                           oldWarmup,
                                           0,
                                           warmupSamples)
                        <= comparisonTolerance);

                const auto subjectEvent = processTimelineBlock(subject,
                                                               subjectPlayHead,
                                                               24000,
                                                               eventBlockSamples);
                if (playingWithoutAnchor)
                    oldPlayHead.setPlayingWithoutTimeline();
                const auto oldEvent = processLfoBlock(
                    continuousOld,
                    eventBlockSamples,
                    playingWithoutAnchor ? &oldPlayHead : nullptr);
                const auto newEvent = processTimelineBlock(canonicalNew,
                                                           newPlayHead,
                                                           24000,
                                                           eventBlockSamples);
                checkTimelineCorrection(subjectEvent, oldEvent, newEvent);
            }
        }
    }
}

TEST_CASE("Continuous absolute LFO timeline does not trigger a false seek",
          "[processor][lfo][live-parameter][transition][timeline][continuous]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    constexpr LfoSettings settings { false, 8, 0.5f, 0.05f };
    constexpr int largeCallbackSamples = 8192;
    constexpr int totalSamples = largeCallbackSamples + transitionSamples
                                 + endpointWindowSamples;
    const std::vector<int> largeThenFollowup {
        largeCallbackSamples,
        transitionSamples + endpointWindowSamples
    };

    const auto large = renderLfo(settings,
                                 {},
                                 largeThenFollowup,
                                 totalSamples,
                                 true);
    const auto canonicalSmall = renderLfo(settings,
                                          {},
                                          fixedCallbacks,
                                          totalSamples,
                                          true);
    const auto irregular = renderLfo(settings,
                                     {},
                                     irregularCallbacks,
                                     totalSamples,
                                     true);

    REQUIRE(allFinite(large));
    REQUIRE(allFinite(canonicalSmall));
    const float largeBlockError = maximumError(large,
                                               canonicalSmall,
                                               0,
                                               totalSamples);
    const float irregularError = maximumError(irregular,
                                              canonicalSmall,
                                              0,
                                              totalSamples);
    const float postBoundaryError = maximumError(
        large,
        canonicalSmall,
        largeCallbackSamples,
        totalSamples);
    CAPTURE(largeBlockError, irregularError, postBoundaryError);
    CHECK(largeBlockError <= comparisonTolerance);
    CHECK(irregularError <= comparisonTolerance);
    CHECK(postBoundaryError <= comparisonTolerance);
}

TEST_CASE("LFO reset clears an in-flight timeline correction",
          "[processor][lfo][live-parameter][transition][reset]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    constexpr LfoSettings initial { false, 8, 0.5f, 0.05f };
    constexpr LfoSettings target { false, 8, 0.5f, 0.45f };
    constexpr int correctionSamplesBeforeReset = 160;

    FireAudioProcessor subjectProcessor;
    FireAudioProcessor oldProcessor;
    FireAudioProcessor transitionTargetProcessor;
    auto& subject = prepareTestLfo(subjectProcessor, initial);
    auto& continuousOld = prepareTestLfo(oldProcessor, initial);
    auto& transitionTarget = prepareTestLfo(transitionTargetProcessor, target);
    TimelinePlayHead subjectPlayHead;
    TimelinePlayHead oldPlayHead;
    TimelinePlayHead targetPlayHead;

    processTimelineBlock(subject, subjectPlayHead, 0, eventSample);
    processTimelineBlock(continuousOld, oldPlayHead, 0, eventSample);
    applySettings(subjectProcessor, target);

    const auto partialCorrection = processTimelineBlock(
        subject,
        subjectPlayHead,
        eventSample,
        correctionSamplesBeforeReset);
    const auto oldAtEvent = processTimelineBlock(
        continuousOld,
        oldPlayHead,
        eventSample,
        correctionSamplesBeforeReset);
    const auto targetAtEvent = processTimelineBlock(
        transitionTarget,
        targetPlayHead,
        eventSample,
        correctionSamplesBeforeReset);
    CHECK(partialCorrection.getSample(0, 0)
          == Catch::Approx(oldAtEvent.getSample(0, 0))
                 .margin(comparisonTolerance));
    const float midCorrectionSeparation = std::abs(
        partialCorrection.getSample(0, correctionSamplesBeforeReset - 1)
        - targetAtEvent.getSample(0, correctionSamplesBeforeReset - 1));
    REQUIRE(midCorrectionSeparation >= 0.05f);

    subject.reset();

    FireAudioProcessor resetReferenceProcessor;
    auto& resetReference = prepareTestLfo(resetReferenceProcessor, target);
    TimelinePlayHead resetReferencePlayHead;
    const int resetTimelineSample = eventSample
                                    + correctionSamplesBeforeReset;
    const auto afterReset = processTimelineBlock(subject,
                                                 subjectPlayHead,
                                                 resetTimelineSample,
                                                 endpointWindowSamples);
    const auto canonicalAfterReset = processTimelineBlock(
        resetReference,
        resetReferencePlayHead,
        resetTimelineSample,
        endpointWindowSamples);
    const float resetError = maximumBufferError(afterReset,
                                                canonicalAfterReset,
                                                0,
                                                endpointWindowSamples);
    CAPTURE(midCorrectionSeparation, resetError);
    REQUIRE(allFinite(afterReset));
    CHECK(resetError <= comparisonTolerance);
}
