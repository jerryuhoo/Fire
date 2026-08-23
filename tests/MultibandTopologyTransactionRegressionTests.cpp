#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <thread>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int warmupSamples = 12000;
constexpr int transitionSamples = 480;
constexpr int transitionGuardSamples = 128;
constexpr int finalComparisonSamples = 2048;
constexpr float publicationTolerance = 2.0e-4f;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String bandParameter(const juce::String& baseID, int bandIndex)
{
    return ParameterIDAndName::getIDString(baseID, bandIndex);
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double phase = channel == 0 ? 0.13 : 0.61;
    const float dc = channel == 0 ? 0.105f : -0.082f;
    return dc
         + 0.16f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 73.0 * time + phase))
         + 0.13f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 619.0 * time
                   + 0.37 - phase))
         + 0.105f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 3101.0 * time
                   + 0.19 + phase))
         + 0.072f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 8701.0 * time
                   + 0.43 - 0.4 * phase));
}

void configureProcessor(FireAudioProcessor& processor)
{
    setPlainParameter(processor, HQ_ID, 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 2.0f);

    constexpr std::array<float, 3> crossoverFrequencies {
        500.0f, 2500.0f, 7000.0f
    };
    for (int divider = 0; divider < 3; ++divider)
    {
        setPlainParameter(processor,
                          bandParameter(FREQ_ID, divider),
                          crossoverFrequencies[static_cast<size_t>(divider)]);
        setPlainParameter(processor,
                          bandParameter(LINE_STATE_ID, divider),
                          divider == 0 ? 1.0f : 0.0f);
    }

    constexpr std::array<float, 4> bandOutputDb {
        0.0f, -6.0f, 3.0f, -3.0f
    };
    for (int band = 0; band < 4; ++band)
    {
        setPlainParameter(processor, bandParameter(BAND_ENABLE_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(BAND_SOLO_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(LINKED_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(COMP_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DC_FILTER_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(SAFE_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(EXTREME_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(MODE_ID, band), 4.0f);
        setPlainParameter(processor, bandParameter(DRIVE_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(BIAS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(REC_ID, band), 0.0f);
        setPlainParameter(processor,
                          bandParameter(OUTPUT_ID, band),
                          bandOutputDb[static_cast<size_t>(band)]);
        setPlainParameter(processor, bandParameter(MIX_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(SHAPE_MIX_ID, band), 1.0f);
    }
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

juce::AudioBuffer<float> makeTimelineInput(int firstSample, int numSamples)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
            buffer.setSample(channel,
                             sample,
                             inputSample(channel, firstSample + sample));
    return buffer;
}

float maximumBufferDifference(const juce::AudioBuffer<float>& first,
                              const juce::AudioBuffer<float>& second)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());
    float maximum = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            maximum = std::max(
                maximum,
                std::abs(first.getSample(channel, sample)
                         - second.getSample(channel, sample)));
    return maximum;
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite(buffer.getSample(channel, sample)))
                return false;
    return true;
}

void setPublishedThreeBandTopology(FireAudioProcessor& processor)
{
    setPlainParameter(processor, bandParameter(FREQ_ID, 0), 1100.0f);
    setPlainParameter(processor, bandParameter(FREQ_ID, 1), 5200.0f);
    setPlainParameter(processor, bandParameter(LINE_STATE_ID, 0), 1.0f);
    setPlainParameter(processor, bandParameter(LINE_STATE_ID, 1), 1.0f);
    setPlainParameter(processor, bandParameter(LINE_STATE_ID, 2), 0.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 0), -12.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 1), 5.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 2), 6.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 3.0f);
}

void setPublishedSameCountTopology(FireAudioProcessor& processor)
{
    setPlainParameter(processor, bandParameter(FREQ_ID, 0), 2800.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 0), -10.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 1), 5.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 2.0f);
}

void setPublishedOneBandTopology(FireAudioProcessor& processor)
{
    setPlainParameter(processor, bandParameter(FREQ_ID, 0), 850.0f);
    setPlainParameter(processor, bandParameter(LINE_STATE_ID, 0), 0.0f);
    setPlainParameter(processor, bandParameter(LINE_STATE_ID, 1), 0.0f);
    setPlainParameter(processor, bandParameter(LINE_STATE_ID, 2), 0.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 0), -18.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 1), 2.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID, 2), -4.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
}

bool setPlainParameterFromWriter(FireAudioProcessor& processor,
                                 const juce::String& parameterID,
                                 float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    if (parameter == nullptr)
        return false;
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    return true;
}

void configureTopologyLfo(FireAudioProcessor& processor)
{
    setPlainParameter(processor,
                      bandParameter(LFO_SYNC_MODE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      bandParameter(LFO_RATE_HZ_ID, 0),
                      37.0f);
    setPlainParameter(processor,
                      bandParameter(LFO_PHASE_ID, 0),
                      0.19f);
}

void assignTopologyLfoRoute(FireAudioProcessor& processor)
{
    const auto target = bandParameter(OUTPUT_ID, 0);
    processor.assignLfoToTarget(0, target);
    processor.setModulationDepth(target, 0.8f);
}

class ScopedRoutingRefreshBlocker
{
public:
    explicit ScopedRoutingRefreshBlocker(LfoManager& manager)
        : worker([this, &manager]
          {
              const juce::ScopedLock lock(manager.getLfoDataLock());
              lockAcquired.signal();
              releaseLock.wait();
          })
    {
    }

    ~ScopedRoutingRefreshBlocker()
    {
        unblock();
    }

    bool waitUntilLocked()
    {
        return lockAcquired.wait(2000);
    }

    void unblock()
    {
        releaseLock.signal();
        if (worker.joinable())
            worker.join();
    }

private:
    juce::WaitableEvent lockAcquired;
    juce::WaitableEvent releaseLock;
    std::thread worker;
};
} // namespace

TEST_CASE("A multiband topology edit is published as one atomic generation",
          "[processor][multiband][topology][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    FireAudioProcessor singlePublicationReference;
    configureProcessor(subject);
    configureProcessor(oldReference);
    configureProcessor(singlePublicationReference);

    const int fixedLatency = subject.getLatencySamples();
    REQUIRE(fixedLatency > 0);
    REQUIRE(oldReference.getLatencySamples() == fixedLatency);
    REQUIRE(singlePublicationReference.getLatencySamples() == fixedLatency);

    int streamPosition = 0;
    float stagedOldError = 0.0f;
    float stagedSinglePublicationError = 0.0f;
    float postPublicationError = 0.0f;
    float finalOldSeparation = 0.0f;
    bool allFinite = true;
    bool latencyInvariant = true;

    const auto processAlignedBlock = [&] (int numSamples,
                                          bool requireOldTopology,
                                          bool afterPublication)
    {
        auto subjectOutput = makeTimelineInput(streamPosition, numSamples);
        auto oldOutput = makeTimelineInput(streamPosition, numSamples);
        auto singlePublicationOutput = makeTimelineInput(streamPosition,
                                                         numSamples);
        juce::MidiBuffer subjectMidi;
        juce::MidiBuffer oldMidi;
        juce::MidiBuffer singlePublicationMidi;
        subject.processBlock(subjectOutput, subjectMidi);
        oldReference.processBlock(oldOutput, oldMidi);
        singlePublicationReference.processBlock(singlePublicationOutput,
                                                singlePublicationMidi);

        allFinite = allFinite && isFinite(subjectOutput)
                              && isFinite(oldOutput)
                              && isFinite(singlePublicationOutput);
        latencyInvariant = latencyInvariant
                           && subject.getLatencySamples() == fixedLatency
                           && oldReference.getLatencySamples() == fixedLatency
                           && singlePublicationReference.getLatencySamples()
                                  == fixedLatency;
        if (requireOldTopology)
        {
            stagedOldError = std::max(
                stagedOldError,
                maximumBufferDifference(subjectOutput, oldOutput));
            stagedSinglePublicationError = std::max(
                stagedSinglePublicationError,
                maximumBufferDifference(subjectOutput,
                                        singlePublicationOutput));
        }
        if (afterPublication)
        {
            // This locks NUM_BANDS plus generation to one consumption: the
            // reference changes only NUM_BANDS, so a second queued reset or
            // transition in the subject creates an immediately audible error.
            postPublicationError = std::max(
                postPublicationError,
                maximumBufferDifference(subjectOutput,
                                        singlePublicationOutput));
            const int finalStateSample = warmupSamples
                                       + 8 * preparedBlockSize
                                       + transitionSamples
                                       + transitionGuardSamples
                                       + fixedLatency;
            if (streamPosition >= finalStateSample)
                finalOldSeparation = std::max(
                    finalOldSeparation,
                    maximumBufferDifference(subjectOutput, oldOutput));
        }
        streamPosition += numSamples;
    };

    while (streamPosition < warmupSamples)
        processAlignedBlock(std::min(preparedBlockSize,
                                     warmupSamples - streamPosition),
                            true,
                            false);

    subject.beginMultibandTopologyEdit();
    setPlainParameter(subject, bandParameter(FREQ_ID, 0), 1100.0f);
    setPlainParameter(subject, bandParameter(OUTPUT_ID, 0), -12.0f);
    setPlainParameter(subject, bandParameter(OUTPUT_ID, 2), 6.0f);
    for (int block = 0; block < 4; ++block)
        processAlignedBlock(preparedBlockSize, true, false);

    // NUM_BANDS is observable in APVTS, but remains staged for audio until the
    // request call commits/ends the transaction.
    setPlainParameter(subject, bandParameter(FREQ_ID, 1), 5200.0f);
    setPlainParameter(subject, bandParameter(LINE_STATE_ID, 1), 1.0f);
    setPlainParameter(subject, bandParameter(OUTPUT_ID, 1), 5.0f);
    setPlainParameter(subject, NUM_BANDS_ID, 3.0f);
    const auto* stagedBandCount = subject.treeState.getRawParameterValue(
        NUM_BANDS_ID);
    REQUIRE(stagedBandCount != nullptr);
    REQUIRE(stagedBandCount->load() == 3.0f);
    for (int block = 0; block < 4; ++block)
        processAlignedBlock(preparedBlockSize, true, false);

    // Canonical one-shot publication changes all atomics between callbacks and
    // relies on NUM_BANDS alone. Subject publishes that same count together
    // with exactly one explicit generation.
    setPublishedThreeBandTopology(singlePublicationReference);
    subject.requestMultibandTopologyReset();

    const int postPublicationSamples = transitionSamples
                                     + transitionGuardSamples
                                     + finalComparisonSamples
                                     + fixedLatency;
    int processedAfterPublication = 0;
    while (processedAfterPublication < postPublicationSamples)
    {
        const int blockSize = std::min(preparedBlockSize,
                                       postPublicationSamples
                                           - processedAfterPublication);
        processAlignedBlock(blockSize, false, true);
        processedAfterPublication += blockSize;
    }

    CAPTURE(fixedLatency,
            stagedOldError,
            stagedSinglePublicationError,
            postPublicationError,
            finalOldSeparation);
    CHECK(allFinite);
    CHECK(latencyInvariant);
    CHECK(stagedOldError < 1.0e-6f);
    CHECK(stagedSinglePublicationError < 1.0e-6f);
    CHECK(postPublicationError < publicationTolerance);
    REQUIRE(finalOldSeparation > 0.02f);
}

TEST_CASE("An odd topology transaction retains one complete audio callback recipe",
          "[processor][multiband][topology][transaction][audio-state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    configureProcessor(subject);

    constexpr int callbackSamples = 16;
    constexpr int lfoIndex = 0;

    // Give every callback subsystem a distinct old value. The topology
    // transaction below models a host/preset state replacement which stages
    // the matching new values while its public generation remains odd.
    setPlainParameter(subject, NUM_BANDS_ID, 2.0f);
    setPlainParameter(subject, bandParameter(OUTPUT_ID, 0), 0.0f);
    setPlainParameter(subject, HQ_ID, 0.0f);
    setPlainParameter(subject, OUTPUT_ID, 0.0f);
    setPlainParameter(subject, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(subject, DOWNSAMPLE_ID, 1.0f);
    setPlainParameter(subject,
                      bandParameter(LFO_SYNC_MODE_ID, lfoIndex),
                      0.0f);
    setPlainParameter(subject,
                      bandParameter(LFO_RATE_HZ_ID, lfoIndex),
                      3.0f);

    juce::AudioBuffer<float> initialBuffer(2, callbackSamples);
    initialBuffer.clear();
    juce::MidiBuffer initialMidi;
    subject.processBlock(initialBuffer, initialMidi);

    const auto evenGeneration =
        subject.getMultibandTopologyGenerationForTesting();
    REQUIRE((evenGeneration & 1u) == 0u);

    bool stagedParametersValid = true;
    bool callbackStartedDuringOddGeneration = false;
    subject.beginMultibandTopologyEdit();
    try
    {
        const auto stage = [&] (const juce::String& parameterID,
                                float plainValue)
        {
            stagedParametersValid = setPlainParameterFromWriter(
                                        subject,
                                        parameterID,
                                        plainValue)
                                  && stagedParametersValid;
        };

        stage(bandParameter(FREQ_ID, 0), 1100.0f);
        stage(bandParameter(FREQ_ID, 1), 5200.0f);
        stage(bandParameter(LINE_STATE_ID, 0), 1.0f);
        stage(bandParameter(LINE_STATE_ID, 1), 1.0f);
        stage(bandParameter(LINE_STATE_ID, 2), 0.0f);
        stage(bandParameter(OUTPUT_ID, 0), -12.0f);
        stage(bandParameter(OUTPUT_ID, 1), 5.0f);
        stage(bandParameter(OUTPUT_ID, 2), 6.0f);
        stage(NUM_BANDS_ID, 3.0f);

        stage(HQ_ID, 1.0f);
        stage(OUTPUT_ID, -18.0f);
        stage(DOWNSAMPLE_BYPASS_ID, 1.0f);
        stage(DOWNSAMPLE_ID, 17.0f);
        stage(bandParameter(LFO_RATE_HZ_ID, lfoIndex), 37.0f);

        callbackStartedDuringOddGeneration =
            (subject.getMultibandTopologyGenerationForTesting() & 1u) != 0u;

        juce::AudioBuffer<float> oddCallbackBuffer(2, callbackSamples);
        oddCallbackBuffer.clear();
        juce::MidiBuffer oddCallbackMidi;
        subject.processBlock(oddCallbackBuffer, oddCallbackMidi);
    }
    catch (...)
    {
        subject.requestMultibandTopologyReset();
        throw;
    }
    subject.requestMultibandTopologyReset();

    // Retrieve and check the recipe only after balancing the writer lock, so
    // a Catch assertion cannot strand the processor in an odd transaction.
    const auto recipe = subject.getLastAudioCallbackRecipeForTesting();
    const auto publishedGeneration =
        subject.getMultibandTopologyGenerationForTesting();

    CAPTURE(evenGeneration,
            recipe.generationAtCallbackStart,
            recipe.topologyPublicationSequence,
            recipe.numBands,
            recipe.band0OutputDb,
            recipe.requestedHq,
            recipe.globalOutputDb,
            recipe.lofiEnabled,
            recipe.lofiRate,
            recipe.lfo1FreeRateHz,
            publishedGeneration);
    CHECK(stagedParametersValid);
    CHECK(callbackStartedDuringOddGeneration);
    CHECK(recipe.generationAtCallbackStart == evenGeneration + 1u);
    CHECK((recipe.generationAtCallbackStart & 1u) != 0u);
    CHECK(recipe.topologyPublicationSequence == evenGeneration);
    CHECK(recipe.numBands == 2);
    CHECK(recipe.band0OutputDb == Catch::Approx(0.0f).margin(1.0e-5f));
    CHECK_FALSE(recipe.requestedHq);
    CHECK(recipe.globalOutputDb == Catch::Approx(0.0f).margin(1.0e-5f));
    CHECK_FALSE(recipe.lofiEnabled);
    CHECK(recipe.lofiRate == Catch::Approx(1.0f));
    CHECK(recipe.lfo1FreeRateHz == Catch::Approx(3.0f));
    CHECK(publishedGeneration == evenGeneration + 2u);
    CHECK((publishedGeneration & 1u) == 0u);
}

TEST_CASE("An odd topology transaction cannot publish staged LFO timing, shape, or routing",
          "[processor][multiband][topology][transaction][audio-state][lfo]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    configureProcessor(subject);
    configureProcessor(oldReference);
    configureTopologyLfo(subject);
    configureTopologyLfo(oldReference);
    assignTopologyLfoRoute(subject);
    assignTopologyLfoRoute(oldReference);

    LfoData oldShape;
    oldShape.points = {
        { 0.0f, 0.0f },
        { 0.5f, 1.0f },
        { 1.0f, 0.0f }
    };
    oldShape.curvatures = { 0.0f, 0.0f };
    subject.getLfoManager().setLfoData(0, oldShape);
    oldReference.getLfoManager().setLfoData(0, oldShape);

    int streamPosition = 0;
    float warmupError = 0.0f;
    while (streamPosition < warmupSamples)
    {
        const int numSamples = std::min(preparedBlockSize,
                                        warmupSamples - streamPosition);
        auto subjectOutput = makeTimelineInput(streamPosition, numSamples);
        auto referenceOutput = makeTimelineInput(streamPosition, numSamples);
        juce::MidiBuffer subjectMidi;
        juce::MidiBuffer referenceMidi;
        subject.processBlock(subjectOutput, subjectMidi);
        oldReference.processBlock(referenceOutput, referenceMidi);
        warmupError = std::max(warmupError,
                               maximumBufferDifference(subjectOutput,
                                                       referenceOutput));
        streamPosition += numSamples;
    }

    bool stagedRateWasValid = false;
    bool callbackRanDuringOdd = false;
    float oddSnapshotError = 0.0f;
    subject.beginMultibandTopologyEdit();
    try
    {
        stagedRateWasValid = setPlainParameterFromWriter(
            subject,
            bandParameter(LFO_RATE_HZ_ID, 0),
            3.0f);

        LfoData stagedShape;
        stagedShape.points = {
            { 0.0f, 1.0f },
            { 1.0f, 1.0f }
        };
        stagedShape.curvatures = { 0.0f };
        subject.getLfoManager().setLfoData(0, stagedShape);
        subject.getLfoManager().clearModulationForTarget(
            bandParameter(OUTPUT_ID, 0));

        callbackRanDuringOdd =
            (subject.getMultibandTopologyGenerationForTesting() & 1u) != 0u;
        const int blocksToExposeFixedLatency =
            subject.getLatencySamples() / preparedBlockSize + 8;
        for (int block = 0; block < blocksToExposeFixedLatency; ++block)
        {
            auto subjectOutput = makeTimelineInput(streamPosition,
                                                   preparedBlockSize);
            auto referenceOutput = makeTimelineInput(streamPosition,
                                                     preparedBlockSize);
            juce::MidiBuffer subjectMidi;
            juce::MidiBuffer referenceMidi;
            subject.processBlock(subjectOutput, subjectMidi);
            oldReference.processBlock(referenceOutput, referenceMidi);
            for (int channel = 0; channel < subjectOutput.getNumChannels();
                 ++channel)
            {
                for (int sample = 0; sample < subjectOutput.getNumSamples();
                     ++sample)
                {
                    oddSnapshotError = std::max(
                        oddSnapshotError,
                        std::abs(subjectOutput.getSample(channel, sample)
                                 - referenceOutput.getSample(channel,
                                                             sample)));
                }
            }
            streamPosition += preparedBlockSize;
        }
    }
    catch (...)
    {
        subject.requestMultibandTopologyReset();
        throw;
    }
    subject.requestMultibandTopologyReset();

    const auto stagedRoutings =
        subject.getLfoManager().getModulationRoutingsCopy();
    const auto oldTarget = bandParameter(OUTPUT_ID, 0);
    const bool stagedRouteWasRemoved = std::none_of(
        stagedRoutings.begin(),
        stagedRoutings.end(),
        [&oldTarget] (const ModulationRouting& routing)
        {
            return routing.targetParameterID == oldTarget;
        });

    CAPTURE(warmupError, oddSnapshotError);
    CHECK(stagedRateWasValid);
    CHECK(stagedRouteWasRemoved);
    CHECK(callbackRanDuringOdd);
    CHECK(warmupError < 1.0e-6f);
    CHECK(oddSnapshotError < 1.0e-6f);
}

TEST_CASE("A callback capture that becomes odd discards its complete candidate",
          "[processor][multiband][topology][transaction][audio-state][lfo][race]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    configureProcessor(subject);
    configureProcessor(oldReference);
    configureTopologyLfo(subject);
    configureTopologyLfo(oldReference);
    assignTopologyLfoRoute(subject);
    assignTopologyLfoRoute(oldReference);

    LfoData oldShape;
    oldShape.points = {
        { 0.0f, 0.0f },
        { 0.5f, 1.0f },
        { 1.0f, 0.0f }
    };
    oldShape.curvatures = { 0.0f, 0.0f };
    subject.getLfoManager().setLfoData(0, oldShape);
    oldReference.getLfoManager().setLfoData(0, oldShape);

    int streamPosition = 0;
    float warmupError = 0.0f;
    while (streamPosition < warmupSamples)
    {
        const int numSamples = std::min(preparedBlockSize,
                                        warmupSamples - streamPosition);
        auto subjectOutput = makeTimelineInput(streamPosition, numSamples);
        auto referenceOutput = makeTimelineInput(streamPosition, numSamples);
        juce::MidiBuffer subjectMidi;
        juce::MidiBuffer referenceMidi;
        subject.processBlock(subjectOutput, subjectMidi);
        oldReference.processBlock(referenceOutput, referenceMidi);
        warmupError = std::max(warmupError,
                               maximumBufferDifference(subjectOutput,
                                                       referenceOutput));
        streamPosition += numSamples;
    }

    const auto generationBeforeCapture =
        subject.getMultibandTopologyGenerationForTesting();
    bool hookRan = false;
    bool hookObservedEvenGeneration = false;
    bool writerStarted = false;
    bool stagedParametersValid = true;
    subject.setAudioCallbackStateCaptureHookForTesting([&]
    {
        hookRan = true;
        hookObservedEvenGeneration =
            (subject.getMultibandTopologyGenerationForTesting() & 1u) == 0u;
        subject.beginMultibandTopologyEdit();
        writerStarted = true;

        const auto stage = [&] (const juce::String& parameterID,
                                float plainValue)
        {
            stagedParametersValid = setPlainParameterFromWriter(
                                        subject,
                                        parameterID,
                                        plainValue)
                                  && stagedParametersValid;
        };
        stage(bandParameter(OUTPUT_ID, 0), -12.0f);
        stage(HQ_ID, 1.0f);
        stage(OUTPUT_ID, -18.0f);
        stage(DOWNSAMPLE_BYPASS_ID, 1.0f);
        stage(DOWNSAMPLE_ID, 17.0f);
        stage(bandParameter(LFO_RATE_HZ_ID, 0), 3.0f);

        LfoData stagedShape;
        stagedShape.points = {
            { 0.0f, 1.0f },
            { 1.0f, 1.0f }
        };
        stagedShape.curvatures = { 0.0f };
        subject.getLfoManager().setLfoData(0, stagedShape);

        const auto oldTarget = bandParameter(OUTPUT_ID, 0);
        subject.getLfoManager().clearModulationForTarget(oldTarget);
        subject.assignLfoToTarget(0, OUTPUT_ID);
        subject.setModulationDepth(OUTPUT_ID, 0.8f);
    });

    FireAudioProcessor::AudioCallbackRecipeForTesting rejectedRecipe;
    bool activeOldRouteRetained = false;
    bool activeNewRouteRejected = false;
    float rejectedCandidateError = 0.0f;
    try
    {
        const int blocksToExposeFixedLatency =
            subject.getLatencySamples() / preparedBlockSize + 8;
        for (int block = 0; block < blocksToExposeFixedLatency; ++block)
        {
            auto subjectOutput = makeTimelineInput(streamPosition,
                                                   preparedBlockSize);
            auto referenceOutput = makeTimelineInput(streamPosition,
                                                     preparedBlockSize);
            juce::MidiBuffer subjectMidi;
            juce::MidiBuffer referenceMidi;
            subject.processBlock(subjectOutput, subjectMidi);
            oldReference.processBlock(referenceOutput, referenceMidi);
            rejectedCandidateError = std::max(
                rejectedCandidateError,
                maximumBufferDifference(subjectOutput, referenceOutput));

            if (block == 0)
            {
                rejectedRecipe =
                    subject.getLastAudioCallbackRecipeForTesting();
                auto* oldParameter = subject.treeState.getParameter(
                    bandParameter(OUTPUT_ID, 0));
                auto* newParameter = subject.treeState.getParameter(OUTPUT_ID);
                LfoManager::AudioThreadRoutingInfo routingInfo;
                activeOldRouteRetained =
                    subject.getLfoManager().getAudioThreadRoutingInfo(
                        oldParameter,
                        routingInfo);
                activeNewRouteRejected =
                    ! subject.getLfoManager().getAudioThreadRoutingInfo(
                        newParameter,
                        routingInfo);
            }

            streamPosition += preparedBlockSize;
        }
    }
    catch (...)
    {
        if (writerStarted)
            subject.requestMultibandTopologyReset();
        throw;
    }
    if (writerStarted)
        subject.requestMultibandTopologyReset();

    const auto stagedRoutings =
        subject.getLfoManager().getModulationRoutingsCopy();
    const auto oldTarget = bandParameter(OUTPUT_ID, 0);
    const bool liveOldRouteRemoved = std::none_of(
        stagedRoutings.begin(),
        stagedRoutings.end(),
        [&oldTarget] (const ModulationRouting& routing)
        {
            return routing.targetParameterID == oldTarget;
        });
    const bool liveNewRoutePresent = std::any_of(
        stagedRoutings.begin(),
        stagedRoutings.end(),
        [] (const ModulationRouting& routing)
        {
            return routing.targetParameterID == OUTPUT_ID;
        });
    const auto publishedGeneration =
        subject.getMultibandTopologyGenerationForTesting();

    CAPTURE(generationBeforeCapture,
            rejectedRecipe.generationAtCallbackStart,
            rejectedRecipe.topologyPublicationSequence,
            rejectedRecipe.band0OutputDb,
            rejectedRecipe.requestedHq,
            rejectedRecipe.globalOutputDb,
            rejectedRecipe.lofiEnabled,
            rejectedRecipe.lofiRate,
            rejectedRecipe.lfo1FreeRateHz,
            warmupError,
            rejectedCandidateError,
            publishedGeneration);
    CHECK((generationBeforeCapture & 1u) == 0u);
    CHECK(hookRan);
    CHECK(hookObservedEvenGeneration);
    CHECK(writerStarted);
    CHECK(stagedParametersValid);
    CHECK(rejectedRecipe.generationAtCallbackStart == generationBeforeCapture);
    CHECK(rejectedRecipe.topologyPublicationSequence == generationBeforeCapture);
    CHECK(rejectedRecipe.band0OutputDb
          == Catch::Approx(0.0f).margin(1.0e-5f));
    CHECK_FALSE(rejectedRecipe.requestedHq);
    CHECK(rejectedRecipe.globalOutputDb
          == Catch::Approx(0.0f).margin(1.0e-5f));
    CHECK_FALSE(rejectedRecipe.lofiEnabled);
    CHECK(rejectedRecipe.lofiRate == Catch::Approx(1.0f));
    CHECK(rejectedRecipe.lfo1FreeRateHz == Catch::Approx(37.0f));
    CHECK(activeOldRouteRetained);
    CHECK(activeNewRouteRejected);
    CHECK(liveOldRouteRemoved);
    CHECK(liveNewRoutePresent);
    CHECK(warmupError < 1.0e-6f);
    CHECK(rejectedCandidateError < 1.0e-6f);
    CHECK(publishedGeneration == generationBeforeCapture + 2u);
    CHECK((publishedGeneration & 1u) == 0u);
}

TEST_CASE("An odd topology transaction cannot enter global DSP state",
          "[processor][multiband][topology][transaction][audio-state][global]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    configureProcessor(subject);
    configureProcessor(oldReference);

    int streamPosition = 0;
    float warmupError = 0.0f;
    while (streamPosition < warmupSamples)
    {
        const int numSamples = std::min(preparedBlockSize,
                                        warmupSamples - streamPosition);
        auto subjectOutput = makeTimelineInput(streamPosition, numSamples);
        auto referenceOutput = makeTimelineInput(streamPosition, numSamples);
        juce::MidiBuffer subjectMidi;
        juce::MidiBuffer referenceMidi;
        subject.processBlock(subjectOutput, subjectMidi);
        oldReference.processBlock(referenceOutput, referenceMidi);
        warmupError = std::max(warmupError,
                               maximumBufferDifference(subjectOutput,
                                                       referenceOutput));
        streamPosition += numSamples;
    }

    bool stagedParametersValid = true;
    bool callbackRanDuringOdd = false;
    float oddSnapshotError = 0.0f;
    subject.beginMultibandTopologyEdit();
    try
    {
        const auto stage = [&] (const juce::String& parameterID,
                                float plainValue)
        {
            stagedParametersValid = setPlainParameterFromWriter(
                                        subject,
                                        parameterID,
                                        plainValue)
                                  && stagedParametersValid;
        };

        stage(HQ_ID, 1.0f);
        stage(OUTPUT_ID, -18.0f);
        stage(MIX_ID, 0.15f);
        stage(FILTER_BYPASS_ID, 1.0f);
        stage(PEAK_FREQ_ID, 1400.0f);
        stage(PEAK_GAIN_ID, 24.0f);
        stage(PEAK_Q_ID, 4.0f);
        stage(PEAK_BYPASSED_ID, 0.0f);
        stage(DOWNSAMPLE_BYPASS_ID, 1.0f);
        stage(DOWNSAMPLE_ID, 17.0f);
        stage(BIT_DEPTH_ID, 4.0f);
        stage(JITTER_ID, 0.0f);
        stage(DOWNSAMPLE_MIX_ID, 1.0f);

        callbackRanDuringOdd =
            (subject.getMultibandTopologyGenerationForTesting() & 1u) != 0u;
        const int blocksToExposeFixedLatency =
            subject.getLatencySamples() / preparedBlockSize + 12;
        for (int block = 0; block < blocksToExposeFixedLatency; ++block)
        {
            auto subjectOutput = makeTimelineInput(streamPosition,
                                                   preparedBlockSize);
            auto referenceOutput = makeTimelineInput(streamPosition,
                                                     preparedBlockSize);
            juce::MidiBuffer subjectMidi;
            juce::MidiBuffer referenceMidi;
            subject.processBlock(subjectOutput, subjectMidi);
            oldReference.processBlock(referenceOutput, referenceMidi);
            for (int channel = 0; channel < subjectOutput.getNumChannels();
                 ++channel)
            {
                for (int sample = 0; sample < subjectOutput.getNumSamples();
                     ++sample)
                {
                    oddSnapshotError = std::max(
                        oddSnapshotError,
                        std::abs(subjectOutput.getSample(channel, sample)
                                 - referenceOutput.getSample(channel,
                                                             sample)));
                }
            }
            streamPosition += preparedBlockSize;
        }
    }
    catch (...)
    {
        subject.requestMultibandTopologyReset();
        throw;
    }
    subject.requestMultibandTopologyReset();

    CAPTURE(warmupError, oddSnapshotError);
    CHECK(stagedParametersValid);
    CHECK(callbackRanDuringOdd);
    CHECK(warmupError < 1.0e-6f);
    CHECK(oddSnapshotError < 1.0e-6f);
}

TEST_CASE("A same-count topology generation is consumed only once",
          "[processor][multiband][topology][transaction][generation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    FireAudioProcessor finalReference;
    configureProcessor(subject);
    configureProcessor(oldReference);
    configureProcessor(finalReference);
    setPublishedSameCountTopology(finalReference);

    const int fixedLatency = subject.getLatencySamples();
    int streamPosition = 0;
    float prePublicationError = 0.0f;
    float finalError = 0.0f;
    float finalOldSeparation = 0.0f;
    bool allFinite = true;
    bool latencyInvariant = true;

    const auto processAlignedBlock = [&] (int numSamples, bool finalWindow)
    {
        auto subjectOutput = makeTimelineInput(streamPosition, numSamples);
        auto oldOutput = makeTimelineInput(streamPosition, numSamples);
        auto finalOutput = makeTimelineInput(streamPosition, numSamples);
        juce::MidiBuffer subjectMidi;
        juce::MidiBuffer oldMidi;
        juce::MidiBuffer finalMidi;
        subject.processBlock(subjectOutput, subjectMidi);
        oldReference.processBlock(oldOutput, oldMidi);
        finalReference.processBlock(finalOutput, finalMidi);
        allFinite = allFinite && isFinite(subjectOutput)
                              && isFinite(oldOutput)
                              && isFinite(finalOutput);
        latencyInvariant = latencyInvariant
                           && subject.getLatencySamples() == fixedLatency;
        if (streamPosition < warmupSamples)
            prePublicationError = std::max(
                prePublicationError,
                maximumBufferDifference(subjectOutput, oldOutput));
        if (finalWindow)
        {
            finalError = std::max(
                finalError,
                maximumBufferDifference(subjectOutput, finalOutput));
            finalOldSeparation = std::max(
                finalOldSeparation,
                maximumBufferDifference(finalOutput, oldOutput));
        }
        streamPosition += numSamples;
    };

    while (streamPosition < warmupSamples)
        processAlignedBlock(std::min(preparedBlockSize,
                                     warmupSamples - streamPosition),
                            false);

    subject.beginMultibandTopologyEdit();
    setPublishedSameCountTopology(subject);
    subject.requestMultibandTopologyReset();

    constexpr int recursiveStateSettlingSamples = 2048;
    const int finalWindowStart = warmupSamples
                               + transitionSamples
                               + transitionGuardSamples
                               + recursiveStateSettlingSamples
                               + fixedLatency;
    const int endSample = finalWindowStart + finalComparisonSamples;
    while (streamPosition < endSample)
    {
        const int blockSize = std::min(preparedBlockSize,
                                       endSample - streamPosition);
        processAlignedBlock(blockSize, streamPosition >= finalWindowStart);
    }

    CAPTURE(fixedLatency,
            prePublicationError,
            finalError,
            finalOldSeparation);
    CHECK(allFinite);
    CHECK(latencyInvariant);
    CHECK(prePublicationError < 1.0e-6f);
    // Re-consuming the same even generation resets the crossover recursion on
    // every callback and cannot converge to this continuously-running endpoint.
    CHECK(finalError < publicationTolerance);
    REQUIRE(finalOldSeparation > 0.02f);
}

TEST_CASE("Topology publication waits for the matching LFO routing snapshot",
          "[processor][multiband][topology][transaction][lfo]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    FireAudioProcessor delayedPublicationReference;
    FireAudioProcessor topologyOnlyReference;
    configureProcessor(subject);
    configureProcessor(oldReference);
    configureProcessor(delayedPublicationReference);
    configureProcessor(topologyOnlyReference);
    for (auto* processor : { &subject,
                             &oldReference,
                             &delayedPublicationReference,
                             &topologyOnlyReference })
        configureTopologyLfo(*processor);

    const int fixedLatency = subject.getLatencySamples();
    int streamPosition = 0;
    bool allFinite = true;
    float warmupError = 0.0f;

    const auto processFour = [&] (int numSamples)
    {
        std::array<juce::AudioBuffer<float>, 4> outputs {
            makeTimelineInput(streamPosition, numSamples),
            makeTimelineInput(streamPosition, numSamples),
            makeTimelineInput(streamPosition, numSamples),
            makeTimelineInput(streamPosition, numSamples)
        };
        std::array<FireAudioProcessor*, 4> processors {
            &subject,
            &oldReference,
            &delayedPublicationReference,
            &topologyOnlyReference
        };
        for (size_t index = 0; index < processors.size(); ++index)
        {
            juce::MidiBuffer midi;
            processors[index]->processBlock(outputs[index], midi);
            allFinite = allFinite && isFinite(outputs[index]);
        }
        streamPosition += numSamples;
        return outputs;
    };

    while (streamPosition < warmupSamples)
    {
        const auto outputs = processFour(std::min(preparedBlockSize,
                                                  warmupSamples
                                                      - streamPosition));
        warmupError = std::max(warmupError,
                               maximumBufferDifference(outputs[0],
                                                       outputs[1]));
    }

    subject.beginMultibandTopologyEdit();
    setPublishedThreeBandTopology(subject);
    assignTopologyLfoRoute(subject);
    subject.requestMultibandTopologyReset();
    // A lifecycle reset must not bypass route freshness and consume the newly
    // published topology identity before its matching runtime route exists.
    // Reset every renderer at the same timeline boundary so this comparison
    // isolates snapshot selection instead of cold-versus-warm DSP history.
    subject.reset();
    oldReference.reset();
    delayedPublicationReference.reset();
    topologyOnlyReference.reset();

    float blockedSnapshotError = 0.0f;
    {
        ScopedRoutingRefreshBlocker blocker(subject.getLfoManager());
        REQUIRE(blocker.waitUntilLocked());
        const auto blockedOutputs = processFour(preparedBlockSize);
        REQUIRE_FALSE(
            subject.getLfoManager().wasRoutingSnapshotRefreshedThisBlock());
        blockedSnapshotError = maximumBufferDifference(blockedOutputs[0],
                                                       blockedOutputs[1]);
        blocker.unblock();
    }

    // Publish both references only after the deliberately failed callback.
    // Their LFO engines have advanced over the same absolute sample range.
    setPublishedThreeBandTopology(delayedPublicationReference);
    assignTopologyLfoRoute(delayedPublicationReference);
    setPublishedThreeBandTopology(topologyOnlyReference);

    float acceptedSnapshotError = 0.0f;
    float finalRouteSeparation = 0.0f;
    bool observedSuccessfulRefresh = false;
    const int publicationSample = streamPosition;
    const int finalWindowStart = publicationSample
                               + transitionSamples
                               + transitionGuardSamples
                               + fixedLatency;
    const int endSample = finalWindowStart + finalComparisonSamples;
    while (streamPosition < endSample)
    {
        const int blockStart = streamPosition;
        const auto outputs = processFour(std::min(preparedBlockSize,
                                                  endSample
                                                      - streamPosition));
        observedSuccessfulRefresh = observedSuccessfulRefresh
            || subject.getLfoManager().wasRoutingSnapshotRefreshedThisBlock();
        acceptedSnapshotError = std::max(
            acceptedSnapshotError,
            maximumBufferDifference(outputs[0], outputs[2]));
        if (blockStart >= finalWindowStart)
            finalRouteSeparation = std::max(
                finalRouteSeparation,
                maximumBufferDifference(outputs[2], outputs[3]));
    }

    CAPTURE(fixedLatency,
            warmupError,
            blockedSnapshotError,
            acceptedSnapshotError,
            finalRouteSeparation);
    CHECK(allFinite);
    CHECK(warmupError < 1.0e-6f);
    CHECK(blockedSnapshotError < 1.0e-6f);
    REQUIRE(observedSuccessfulRefresh);
    CHECK(acceptedSnapshotError < publicationTolerance);
    REQUIRE(finalRouteSeparation > 0.01f);
}

TEST_CASE("A reentrant topology writer cannot publish its outer edit early",
          "[processor][multiband][topology][transaction][writer]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    FireAudioProcessor publicationReference;
    configureProcessor(subject);
    configureProcessor(oldReference);
    configureProcessor(publicationReference);

    int streamPosition = 0;
    float prematurePublicationError = 0.0f;
    float publishedError = 0.0f;
    float finalOldSeparation = 0.0f;
    bool allFinite = true;

    const auto processThree = [&] (int numSamples,
                                   bool expectOld,
                                   bool expectPublished)
    {
        auto subjectOutput = makeTimelineInput(streamPosition, numSamples);
        auto oldOutput = makeTimelineInput(streamPosition, numSamples);
        auto publicationOutput = makeTimelineInput(streamPosition, numSamples);
        juce::MidiBuffer subjectMidi;
        juce::MidiBuffer oldMidi;
        juce::MidiBuffer publicationMidi;
        subject.processBlock(subjectOutput, subjectMidi);
        oldReference.processBlock(oldOutput, oldMidi);
        publicationReference.processBlock(publicationOutput, publicationMidi);
        allFinite = allFinite && isFinite(subjectOutput)
                              && isFinite(oldOutput)
                              && isFinite(publicationOutput);
        if (expectOld)
            prematurePublicationError = std::max(
                prematurePublicationError,
                maximumBufferDifference(subjectOutput, oldOutput));
        if (expectPublished)
        {
            publishedError = std::max(
                publishedError,
                maximumBufferDifference(subjectOutput, publicationOutput));
            finalOldSeparation = std::max(
                finalOldSeparation,
                maximumBufferDifference(publicationOutput, oldOutput));
        }
        streamPosition += numSamples;
    };

    while (streamPosition < warmupSamples)
        processThree(std::min(preparedBlockSize,
                              warmupSamples - streamPosition),
                     true,
                     false);

    subject.beginMultibandTopologyEdit();
    setPlainParameter(subject, bandParameter(FREQ_ID, 0), 1100.0f);
    setPlainParameter(subject, bandParameter(OUTPUT_ID, 0), -12.0f);
    setPlainParameter(subject, bandParameter(OUTPUT_ID, 2), 6.0f);

    // A recursive writer shares the writer lock but owns only its inner depth.
    // Its request must not make the sequence even while the outer edit remains.
    subject.beginMultibandTopologyEdit();
    setPlainParameter(subject, bandParameter(FREQ_ID, 1), 5200.0f);
    setPlainParameter(subject, bandParameter(LINE_STATE_ID, 1), 1.0f);
    setPlainParameter(subject, bandParameter(OUTPUT_ID, 1), 5.0f);
    setPlainParameter(subject, NUM_BANDS_ID, 3.0f);
    subject.requestMultibandTopologyReset();
    for (int block = 0; block < 4; ++block)
        processThree(preparedBlockSize, true, false);

    setPublishedThreeBandTopology(publicationReference);
    subject.requestMultibandTopologyReset();
    const int publicationSample = streamPosition;
    const int finalWindowStart = publicationSample
                               + transitionSamples
                               + transitionGuardSamples
                               + subject.getLatencySamples();
    const int endSample = finalWindowStart + finalComparisonSamples;
    while (streamPosition < endSample)
    {
        const int blockStart = streamPosition;
        processThree(std::min(preparedBlockSize,
                              endSample - streamPosition),
                     false,
                     blockStart >= finalWindowStart);
    }

    CAPTURE(prematurePublicationError,
            publishedError,
            finalOldSeparation);
    CHECK(allFinite);
    CHECK(prematurePublicationError < 1.0e-6f);
    CHECK(publishedError < publicationTolerance);
    REQUIRE(finalOldSeparation > 0.02f);
}

TEST_CASE("Concurrent topology writers serialize complete publications",
          "[processor][multiband][topology][transaction][writer][thread]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor subject;
    FireAudioProcessor oldReference;
    FireAudioProcessor finalReference;
    configureProcessor(subject);
    configureProcessor(oldReference);
    configureProcessor(finalReference);

    int streamPosition = 0;
    float oddSnapshotError = 0.0f;
    float finalPublicationError = 0.0f;
    float finalOldSeparation = 0.0f;
    bool allFinite = true;

    const auto processThree = [&] (int numSamples,
                                   bool expectOld,
                                   bool expectFinal)
    {
        auto subjectOutput = makeTimelineInput(streamPosition, numSamples);
        auto oldOutput = makeTimelineInput(streamPosition, numSamples);
        auto finalOutput = makeTimelineInput(streamPosition, numSamples);
        juce::MidiBuffer subjectMidi;
        juce::MidiBuffer oldMidi;
        juce::MidiBuffer finalMidi;
        subject.processBlock(subjectOutput, subjectMidi);
        oldReference.processBlock(oldOutput, oldMidi);
        finalReference.processBlock(finalOutput, finalMidi);
        allFinite = allFinite && isFinite(subjectOutput)
                              && isFinite(oldOutput)
                              && isFinite(finalOutput);
        if (expectOld)
            oddSnapshotError = std::max(
                oddSnapshotError,
                maximumBufferDifference(subjectOutput, oldOutput));
        if (expectFinal)
        {
            finalPublicationError = std::max(
                finalPublicationError,
                maximumBufferDifference(subjectOutput, finalOutput));
            finalOldSeparation = std::max(
                finalOldSeparation,
                maximumBufferDifference(finalOutput, oldOutput));
        }
        streamPosition += numSamples;
    };

    while (streamPosition < warmupSamples)
        processThree(std::min(preparedBlockSize,
                              warmupSamples - streamPosition),
                     true,
                     false);

    // Writer 1 owns the recursive writer lock for this complete 2->3 edit.
    subject.beginMultibandTopologyEdit();
    setPublishedThreeBandTopology(subject);

    juce::WaitableEvent writer2AttemptingBegin;
    juce::WaitableEvent writer2Entered;
    juce::WaitableEvent allowWriter2FirstHalf;
    juce::WaitableEvent writer2FirstHalfWritten;
    juce::WaitableEvent allowWriter2Remainder;
    juce::WaitableEvent writer2CompleteStateWritten;
    juce::WaitableEvent allowWriter2Commit;
    juce::WaitableEvent writer2Finished;
    std::atomic<bool> writer2ParametersValid { true };

    std::thread writer2([&]
    {
        writer2AttemptingBegin.signal();
        subject.beginMultibandTopologyEdit();
        writer2Entered.signal();

        allowWriter2FirstHalf.wait();
        bool valid = setPlainParameterFromWriter(
            subject, bandParameter(FREQ_ID, 0), 850.0f);
        valid = setPlainParameterFromWriter(
                    subject, bandParameter(OUTPUT_ID, 0), -18.0f)
             && valid;
        writer2ParametersValid.store(valid, std::memory_order_release);
        writer2FirstHalfWritten.signal();

        allowWriter2Remainder.wait();
        valid = setPlainParameterFromWriter(
                    subject, bandParameter(LINE_STATE_ID, 0), 0.0f)
             && valid;
        valid = setPlainParameterFromWriter(
                    subject, bandParameter(LINE_STATE_ID, 1), 0.0f)
             && valid;
        valid = setPlainParameterFromWriter(
                    subject, bandParameter(LINE_STATE_ID, 2), 0.0f)
             && valid;
        valid = setPlainParameterFromWriter(
                    subject, bandParameter(OUTPUT_ID, 1), 2.0f)
             && valid;
        valid = setPlainParameterFromWriter(
                    subject, bandParameter(OUTPUT_ID, 2), -4.0f)
             && valid;
        valid = setPlainParameterFromWriter(subject, NUM_BANDS_ID, 1.0f)
             && valid;
        writer2ParametersValid.store(valid, std::memory_order_release);
        writer2CompleteStateWritten.signal();

        allowWriter2Commit.wait();
        subject.requestMultibandTopologyReset();
        writer2Finished.signal();
    });

    const bool observedWriter2Attempt = writer2AttemptingBegin.wait(2000);
    const bool writer2ReturnedBeforeWriter1Commit = writer2Entered.wait(75);
    for (int block = 0; block < 2; ++block)
        processThree(preparedBlockSize, true, false);

    // Only this matching request may release writer 1's lock. Writer 2 then
    // starts a new odd transaction, but is held before its first parameter write.
    subject.requestMultibandTopologyReset();
    const bool writer2ReturnedAfterWriter1Commit =
        writer2ReturnedBeforeWriter1Commit || writer2Entered.wait(2000);
    const auto* bandCountAfterFirstPublication =
        subject.treeState.getRawParameterValue(NUM_BANDS_ID);
    const bool firstPublicationWasComplete =
        bandCountAfterFirstPublication != nullptr
        && bandCountAfterFirstPublication->load() == 3.0f;
    processThree(preparedBlockSize, true, false);

    allowWriter2FirstHalf.signal();
    const bool observedFirstHalf = writer2FirstHalfWritten.wait(2000);
    for (int block = 0; block < 2; ++block)
        processThree(preparedBlockSize, true, false);

    allowWriter2Remainder.signal();
    const bool observedCompleteSecondState =
        writer2CompleteStateWritten.wait(2000);
    const auto* stagedSecondBandCount =
        subject.treeState.getRawParameterValue(NUM_BANDS_ID);
    const bool secondPublicationWasComplete =
        stagedSecondBandCount != nullptr
        && stagedSecondBandCount->load() == 1.0f;
    for (int block = 0; block < 2; ++block)
        processThree(preparedBlockSize, true, false);

    allowWriter2Commit.signal();
    const bool observedWriter2Finish = writer2Finished.wait(2000);
    if (writer2.joinable())
        writer2.join();

    // No audio callback occurred between writer 2's commit and this canonical
    // one-shot publication, so both processors start the final migration from
    // the identical old audible snapshot and absolute input sample.
    setPublishedOneBandTopology(finalReference);
    const int publicationSample = streamPosition;
    const int finalWindowStart = publicationSample
                               + transitionSamples
                               + transitionGuardSamples
                               + subject.getLatencySamples();
    const int endSample = finalWindowStart + finalComparisonSamples;
    while (streamPosition < endSample)
    {
        const int blockStart = streamPosition;
        processThree(std::min(preparedBlockSize,
                              endSample - streamPosition),
                     false,
                     blockStart >= finalWindowStart);
    }

    CAPTURE(observedWriter2Attempt,
            writer2ReturnedBeforeWriter1Commit,
            writer2ReturnedAfterWriter1Commit,
            firstPublicationWasComplete,
            observedFirstHalf,
            observedCompleteSecondState,
            secondPublicationWasComplete,
            observedWriter2Finish,
            oddSnapshotError,
            finalPublicationError,
            finalOldSeparation);
    CHECK(observedWriter2Attempt);
    CHECK_FALSE(writer2ReturnedBeforeWriter1Commit);
    CHECK(writer2ReturnedAfterWriter1Commit);
    CHECK(firstPublicationWasComplete);
    CHECK(observedFirstHalf);
    CHECK(observedCompleteSecondState);
    CHECK(secondPublicationWasComplete);
    CHECK(observedWriter2Finish);
    CHECK(writer2ParametersValid.load(std::memory_order_acquire));
    CHECK(allFinite);
    CHECK(oddSnapshotError < 1.0e-6f);
    CHECK(finalPublicationError < publicationTolerance);
    REQUIRE(finalOldSeparation > 0.02f);
}
