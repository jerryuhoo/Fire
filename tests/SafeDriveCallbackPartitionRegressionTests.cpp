#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 512;
constexpr int warmupSamples = 4096;
constexpr int captureSamples = 2048;
constexpr int transientSample = 480;
constexpr float quietAmplitude = 0.008f;
constexpr float transientPeak = 3.0f;
constexpr float driveValue = 100.0f;

const std::vector<int> fixedCallbacks { 512 };
const std::vector<int> splitCallbacks { 64 };
// These sum to 512, with the transient deliberately in the final callback.
const std::vector<int> irregularCallbacks { 37, 91, 53, 17, 127, 41, 83, 63 };

BandProcessingParameters makeSafeDriveParameters(bool useHq, bool safeEnabled)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 4; // Hard clip has a simple independent static oracle.
    params.isHQ = useHq;
    params.isDriveEnabled = true;
    params.isSafeModeOn = safeEnabled;
    params.isExtremeModeOn = false;
    params.isShapeEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.isDcFilterEnabled = false;
    params.driveVal.baseValue = driveValue;
    params.driveVal.range = { 0.0f, 100.0f };
    params.biasVal.baseValue = 0.0f;
    params.recVal.baseValue = 0.0f;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.shapeMixVal = 1.0f;
    params.shapeMixValProvider.baseValue = 1.0f;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    params.widthMixVal = 1.0f;
    params.widthMixValProvider.baseValue = 1.0f;
    return params;
}

float quietSignal(int channel, int absoluteSample)
{
    const double phase = juce::MathConstants<double>::twoPi
                         * 375.0 * static_cast<double>(absoluteSample)
                         / sampleRate
                         + (channel == 0 ? 0.31 : -0.47);
    return quietAmplitude * static_cast<float>(std::sin(phase));
}

void fillTimelineBlock(juce::AudioBuffer<float>& audio,
                       int absoluteSample,
                       bool includeTransient)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            audio.setSample(channel,
                            sample,
                            quietSignal(channel, absoluteSample + sample));

    if (! includeTransient)
        return;

    const int localTransient = transientSample - absoluteSample;
    if (juce::isPositiveAndBelow(localTransient, audio.getNumSamples()))
        audio.setSample(audio.getNumChannels() - 1,
                        localTransient,
                        transientPeak);
}

struct PartitionRender
{
    std::vector<std::vector<float>> output;
    float maximumObservedPeak = 0.0f;
    float minimumReduction = 1.0f;
    bool finite = true;
};

PartitionRender renderPartitioned(bool useHq,
                                  int numChannels,
                                  bool safeEnabled,
                                  const std::vector<int>& callbackPattern)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    REQUIRE_FALSE(callbackPattern.empty());
    for (const int blockSize : callbackPattern)
        REQUIRE(blockSize > 0);

    BandProcessor processor;
    processor.prepare({ sampleRate,
                        static_cast<juce::uint32>(preparedBlockSize),
                        static_cast<juce::uint32>(numChannels) });
    const auto params = makeSafeDriveParameters(useHq, safeEnabled);

    // Every renderer receives exactly the same callback history before the
    // event. Quiet input leaves Safe inactive and primes Drive at its static
    // high-gain endpoint, so only event callback partitioning can diverge.
    for (int position = 0; position < warmupSamples; position += 256)
    {
        juce::AudioBuffer<float> audio(numChannels, 256);
        juce::AudioBuffer<float> lfoOutputs(1, 256);
        lfoOutputs.clear();
        fillTimelineBlock(audio, position, false);
        processor.process(audio, params, lfoOutputs);
    }

    PartitionRender result;
    result.output.resize(static_cast<size_t>(numChannels));
    for (auto& channel : result.output)
        channel.reserve(captureSamples);

    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < captureSamples)
    {
        const int requested = callbackPattern[
            callbackIndex % callbackPattern.size()];
        ++callbackIndex;
        const int samplesThisBlock = std::min(requested,
                                              captureSamples - streamPosition);
        juce::AudioBuffer<float> audio(numChannels, samplesThisBlock);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisBlock);
        lfoOutputs.clear();
        fillTimelineBlock(audio, streamPosition, true);
        processor.process(audio, params, lfoOutputs);

        result.maximumObservedPeak = std::max(
            result.maximumObservedPeak,
            processor.mSampleMaxValue.load(std::memory_order_relaxed));
        result.minimumReduction = std::min(
            result.minimumReduction,
            processor.mReductionPercent.load(std::memory_order_relaxed));
        for (int channel = 0; channel < numChannels; ++channel)
        {
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = audio.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        }

        streamPosition += samplesThisBlock;
    }
    return result;
}

float maximumDifference(const PartitionRender& first,
                        const PartitionRender& second)
{
    REQUIRE(first.output.size() == second.output.size());
    float error = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (size_t sample = 0; sample < first.output[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(first.output[channel][sample]
                                      - second.output[channel][sample]));
    }
    return error;
}

void checkLateTransientPartitioning(bool useHq, int numChannels)
{
    const auto fixedSafe = renderPartitioned(useHq,
                                             numChannels,
                                             true,
                                             fixedCallbacks);
    const auto splitSafe = renderPartitioned(useHq,
                                             numChannels,
                                             true,
                                             splitCallbacks);
    const auto irregularSafe = renderPartitioned(useHq,
                                                 numChannels,
                                                 true,
                                                 irregularCallbacks);
    const auto fixedUnsafe = renderPartitioned(useHq,
                                               numChannels,
                                               false,
                                               fixedCallbacks);
    const auto splitUnsafe = renderPartitioned(useHq,
                                               numChannels,
                                               false,
                                               splitCallbacks);
    const auto irregularUnsafe = renderPartitioned(useHq,
                                                   numChannels,
                                                   false,
                                                   irregularCallbacks);

    const float splitSafeError = maximumDifference(fixedSafe, splitSafe);
    const float irregularSafeError = maximumDifference(fixedSafe,
                                                       irregularSafe);
    const float splitUnsafeError = maximumDifference(fixedUnsafe,
                                                     splitUnsafe);
    const float irregularUnsafeError = maximumDifference(fixedUnsafe,
                                                         irregularUnsafe);
    const float safeEffect = maximumDifference(fixedSafe, fixedUnsafe);
    CAPTURE(useHq,
            numChannels,
            splitSafeError,
            irregularSafeError,
            splitUnsafeError,
            irregularUnsafeError,
            safeEffect,
            fixedSafe.minimumReduction,
            splitSafe.minimumReduction,
            irregularSafe.minimumReduction);

    REQUIRE(fixedSafe.finite);
    REQUIRE(splitSafe.finite);
    REQUIRE(irregularSafe.finite);
    REQUIRE(fixedUnsafe.finite);
    REQUIRE(splitUnsafe.finite);
    REQUIRE(irregularUnsafe.finite);
    CHECK(fixedSafe.maximumObservedPeak
          == Catch::Approx(transientPeak).margin(1.0e-6f));
    CHECK(splitSafe.maximumObservedPeak
          == Catch::Approx(transientPeak).margin(1.0e-6f));
    CHECK(irregularSafe.maximumObservedPeak
          == Catch::Approx(transientPeak).margin(1.0e-6f));

    // These controls prevent an oversampling or renderer partition bug from
    // masquerading as Safe Drive, and prove the transient actually changes
    // the audible Drive path.
    CHECK(splitUnsafeError < 2.0e-4f);
    CHECK(irregularUnsafeError < 2.0e-4f);
    REQUIRE(safeEffect > 0.01f);

    // One absolute input timeline must not acquire a different Drive envelope
    // merely because the host divides it into smaller callbacks.
    CHECK(splitSafeError < 2.0e-4f);
    CHECK(irregularSafeError < 2.0e-4f);
}

void checkStaticSafeLegacy()
{
    constexpr int blockSize = 512;
    constexpr int settlingBlocks = 12;
    constexpr float amplitude = 0.08f;
    constexpr float driveForCalculation = driveValue * 6.5f / 100.0f;
    const float expectedSafeGain = 2.0f / amplitude
                                   + 0.1f * driveForCalculation;
    const float expectedReduction = std::log2(expectedSafeGain)
                                    / driveForCalculation;

    BandProcessor processor;
    processor.prepare({ sampleRate, blockSize, 2 });
    const auto params = makeSafeDriveParameters(false, true);
    juce::AudioBuffer<float> lastOutput(2, blockSize);
    for (int block = 0; block < settlingBlocks; ++block)
    {
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::AudioBuffer<float> lfoOutputs(1, blockSize);
        lfoOutputs.clear();
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const int absoluteSample = block * blockSize + sample;
                const double phase = juce::MathConstants<double>::twoPi
                                     * 375.0 * absoluteSample / sampleRate
                                     + juce::MathConstants<double>::halfPi;
                audio.setSample(channel,
                                sample,
                                amplitude * static_cast<float>(std::sin(phase)));
            }
        }
        processor.process(audio, params, lfoOutputs);
        if (block == settlingBlocks - 1)
            lastOutput.makeCopyOf(audio);
    }

    float maximumOracleError = 0.0f;
    bool finite = true;
    for (int channel = 0; channel < lastOutput.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < lastOutput.getNumSamples(); ++sample)
        {
            const double phase = juce::MathConstants<double>::twoPi
                                 * 375.0 * sample / sampleRate
                                 + juce::MathConstants<double>::halfPi;
            const float input = amplitude
                                * static_cast<float>(std::sin(phase));
            const float expected = juce::jlimit(-1.0f,
                                                1.0f,
                                                input * expectedSafeGain);
            const float actual = lastOutput.getSample(channel, sample);
            finite = finite && std::isfinite(actual);
            maximumOracleError = std::max(maximumOracleError,
                                          std::abs(actual - expected));
        }
    }

    const float sampleMax = processor.mSampleMaxValue.load(
        std::memory_order_relaxed);
    const float reduction = processor.mReductionPercent.load(
        std::memory_order_relaxed);
    CAPTURE(expectedSafeGain,
            expectedReduction,
            sampleMax,
            reduction,
            maximumOracleError);
    REQUIRE(finite);
    CHECK(sampleMax == Catch::Approx(amplitude).margin(1.0e-6f));
    CHECK(reduction
          == Catch::Approx(expectedReduction).margin(1.0e-5f));
    CHECK(maximumOracleError < 2.0e-5f);
}

void checkCausalSafeAttack()
{
    constexpr int blockSize = 512;
    constexpr int eventSample = 256;
    constexpr int holdSamples = 64;
    constexpr float driveForCalculation = driveValue * 6.5f / 100.0f;
    const float unsafeDriveGain = std::pow(2.0f, driveForCalculation);
    const float causalCeiling = 2.0f / transientPeak
                                + 0.1f * driveForCalculation;
    const auto arctanOracle = [](float input, float gain)
    {
        return std::atan(input * gain) * 0.5f;
    };

    auto params = makeSafeDriveParameters(false, true);
    params.mode = 0; // Arctan remains distinguishable at the 3.0 transient.
    BandProcessor subject;
    BandProcessor noFuturePeakReference;
    const juce::dsp::ProcessSpec spec { sampleRate, blockSize, 1 };
    subject.prepare(spec);
    noFuturePeakReference.prepare(spec);

    // Establish identical, non-reducing high-Drive history. The quiet probe
    // stays below Safe's threshold even at the full static Drive gain.
    for (int position = 0; position < warmupSamples; position += blockSize)
    {
        juce::AudioBuffer<float> subjectWarmup(1, blockSize);
        juce::AudioBuffer<float> referenceWarmup(1, blockSize);
        juce::AudioBuffer<float> lfoOutputs(1, blockSize);
        lfoOutputs.clear();
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float input = quietSignal(0, position + sample);
            subjectWarmup.setSample(0, sample, input);
            referenceWarmup.setSample(0, sample, input);
        }
        subject.process(subjectWarmup, params, lfoOutputs);
        noFuturePeakReference.process(referenceWarmup, params, lfoOutputs);
    }

    juce::AudioBuffer<float> subjectBlock(1, blockSize);
    juce::AudioBuffer<float> referenceBlock(1, blockSize);
    juce::AudioBuffer<float> lfoOutputs(1, blockSize);
    lfoOutputs.clear();
    for (int sample = 0; sample < blockSize; ++sample)
    {
        const float input = quietSignal(0, warmupSamples + sample);
        subjectBlock.setSample(0, sample, input);
        referenceBlock.setSample(0, sample, input);
    }
    subjectBlock.setSample(0, eventSample, transientPeak);
    subject.process(subjectBlock, params, lfoOutputs);
    noFuturePeakReference.process(referenceBlock, params, lfoOutputs);

    float preEventError = 0.0f;
    for (int sample = 0; sample < eventSample; ++sample)
        preEventError = std::max(
            preEventError,
            std::abs(subjectBlock.getSample(0, sample)
                     - referenceBlock.getSample(0, sample)));

    const float eventExpected = arctanOracle(transientPeak, causalCeiling);
    const float eventActual = subjectBlock.getSample(0, eventSample);
    const float eventError = std::abs(eventActual - eventExpected);
    const float unsafeEvent = arctanOracle(transientPeak, unsafeDriveGain);
    float holdError = 0.0f;
    bool finite = std::isfinite(eventActual);
    for (int sample = eventSample + 1;
         sample <= eventSample + holdSamples;
         ++sample)
    {
        const float input = quietSignal(0, warmupSamples + sample);
        const float actual = subjectBlock.getSample(0, sample);
        const float expected = arctanOracle(input, causalCeiling);
        finite = finite && std::isfinite(actual);
        holdError = std::max(holdError, std::abs(actual - expected));
    }

    CAPTURE(unsafeDriveGain,
            causalCeiling,
            preEventError,
            eventActual,
            eventExpected,
            eventError,
            unsafeEvent,
            holdError);
    REQUIRE(std::abs(unsafeEvent - eventExpected) > 0.05f);
    REQUIRE(finite);

    // A future peak cannot alter already-emitted samples. On the event sample
    // itself Safe is a hard causal ceiling, not a 50 ms attack ramp, and that
    // ceiling is held long enough to avoid a one-sample gain rebound.
    CHECK(preEventError < 2.0e-4f);
    CHECK(eventError < 2.0e-4f);
    CHECK(holdError < 2.0e-4f);
}

bool processConstantProbe(BandProcessor& processor,
                          const BandProcessingParameters& params,
                          int numSamples,
                          float inputValue)
{
    bool finite = true;
    for (int processed = 0; processed < numSamples;)
    {
        const int samplesThisBlock = std::min(512, numSamples - processed);
        juce::AudioBuffer<float> audio(1, samplesThisBlock);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisBlock);
        juce::FloatVectorOperations::fill(audio.getWritePointer(0),
                                          inputValue,
                                          samplesThisBlock);
        lfoOutputs.clear();
        processor.process(audio, params, lfoOutputs);
        for (int sample = 0; sample < samplesThisBlock; ++sample)
            finite = finite && std::isfinite(audio.getSample(0, sample));
        processed += samplesThisBlock;
    }
    return finite;
}

void checkSafeHoldAndReleaseTimebase(bool useHq)
{
    constexpr int holdSamples = 2400;
    constexpr int releaseSamples = 2400;
    constexpr float probe = 0.008f;
    const float releaseCoefficient = static_cast<float>(
        std::exp(-1.0 / (sampleRate * 0.05)));
    const auto expectedReductionForPeak = [](float peak)
    {
        constexpr float driveForCalculation = driveValue * 6.5f / 100.0f;
        const float ceiling = 2.0f / peak
                              + 0.1f * driveForCalculation;
        return std::log2(ceiling) / driveForCalculation;
    };

    BandProcessor processor;
    processor.prepare({ sampleRate, 512, 1 });
    auto params = makeSafeDriveParameters(useHq, true);
    params.mode = 0;

    bool finite = processConstantProbe(processor,
                                       params,
                                       1,
                                       transientPeak);
    const float eventReduction = processor.mReductionPercent.load(
        std::memory_order_relaxed);

    // The event sample owns hold slot zero. The following 2399 samples must
    // retain the exact peak; sample 2400 is the first release coefficient.
    finite = processConstantProbe(processor,
                                  params,
                                  holdSamples - 1,
                                  probe)
             && finite;
    const float lastHoldReduction = processor.mReductionPercent.load(
        std::memory_order_relaxed);
    finite = processConstantProbe(processor, params, 1, probe) && finite;
    const float firstReleaseReduction = processor.mReductionPercent.load(
        std::memory_order_relaxed);

    // Including that first release sample, 2400 coefficient applications are
    // one exact 50 ms time constant at 48 kHz.
    finite = processConstantProbe(processor,
                                  params,
                                  releaseSamples - 1,
                                  probe)
             && finite;
    const float oneTauReduction = processor.mReductionPercent.load(
        std::memory_order_relaxed);

    const float heldExpected = expectedReductionForPeak(transientPeak);
    const float firstReleaseExpected = expectedReductionForPeak(
        transientPeak * releaseCoefficient);
    const float oneTauExpected = expectedReductionForPeak(
        transientPeak
        * std::pow(releaseCoefficient,
                   static_cast<float>(releaseSamples)));
    CAPTURE(useHq,
            releaseCoefficient,
            eventReduction,
            lastHoldReduction,
            firstReleaseReduction,
            oneTauReduction,
            heldExpected,
            firstReleaseExpected,
            oneTauExpected);
    REQUIRE(finite);
    CHECK(eventReduction == Catch::Approx(heldExpected).margin(2.0e-5f));
    CHECK(lastHoldReduction
          == Catch::Approx(heldExpected).margin(2.0e-5f));
    CHECK(firstReleaseReduction
          == Catch::Approx(firstReleaseExpected).margin(2.0e-5f));
    CHECK(oneTauReduction
          == Catch::Approx(oneTauExpected).margin(2.0e-5f));
    CHECK(firstReleaseReduction > lastHoldReduction);
}

void checkSafeResetLifecycle()
{
    constexpr float probe = 0.008f;
    auto params = makeSafeDriveParameters(false, true);
    params.mode = 0;
    const juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };

    BandProcessor subject;
    subject.prepare(spec);
    REQUIRE(processConstantProbe(subject, params, 1, transientPeak));
    REQUIRE(processConstantProbe(subject, params, 160, probe));
    subject.reset();

    // Match the outer juce::Gain target lifecycle before reset so the output
    // comparison isolates the Safe envelope rather than Gain's constructor
    // target, just as the production processor does after prior playback.
    BandProcessor fresh;
    fresh.prepare(spec);
    REQUIRE(processConstantProbe(fresh, params, 1, probe));
    fresh.reset();

    juce::AudioBuffer<float> subjectBlock(1, 64);
    juce::AudioBuffer<float> freshBlock(1, 64);
    juce::AudioBuffer<float> lfoOutputs(1, 64);
    subjectBlock.clear();
    freshBlock.clear();
    juce::FloatVectorOperations::fill(subjectBlock.getWritePointer(0),
                                      probe,
                                      subjectBlock.getNumSamples());
    freshBlock.makeCopyOf(subjectBlock);
    lfoOutputs.clear();
    subject.process(subjectBlock, params, lfoOutputs);
    fresh.process(freshBlock, params, lfoOutputs);

    float maximumError = 0.0f;
    bool finite = true;
    for (int sample = 0; sample < subjectBlock.getNumSamples(); ++sample)
    {
        const float actual = subjectBlock.getSample(0, sample);
        const float expected = freshBlock.getSample(0, sample);
        finite = finite && std::isfinite(actual) && std::isfinite(expected);
        maximumError = std::max(maximumError, std::abs(actual - expected));
    }
    const float subjectReduction = subject.mReductionPercent.load(
        std::memory_order_relaxed);
    const float freshReduction = fresh.mReductionPercent.load(
        std::memory_order_relaxed);
    CAPTURE(maximumError, subjectReduction, freshReduction);
    REQUIRE(finite);
    CHECK(maximumError < 1.0e-6f);
    CHECK(subjectReduction == Catch::Approx(1.0f).margin(1.0e-6f));
    CHECK(freshReduction == Catch::Approx(1.0f).margin(1.0e-6f));
}

void checkQualityResetRetainsSafeHistory()
{
    constexpr float probe = 0.008f;
    constexpr float driveForCalculation = driveValue * 6.5f / 100.0f;
    const float heldGain = 2.0f / transientPeak
                           + 0.1f * driveForCalculation;
    const float heldReduction = std::log2(heldGain) / driveForCalculation;
    auto params = makeSafeDriveParameters(true, true);
    params.mode = 0;
    const juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };

    BandProcessor subject;
    BandProcessor uninterrupted;
    subject.prepare(spec);
    uninterrupted.prepare(spec);
    for (auto* processor : { &subject, &uninterrupted })
    {
        REQUIRE(processConstantProbe(*processor, params, 1, transientPeak));
        REQUIRE(processConstantProbe(*processor, params, 160, probe));
    }

    subject.resetQualityTransitionState();
    const bool subjectFinite = processConstantProbe(subject, params, 1, probe);
    const bool referenceFinite = processConstantProbe(uninterrupted,
                                                      params,
                                                      1,
                                                      probe);
    const float subjectReduction = subject.mReductionPercent.load(
        std::memory_order_relaxed);
    const float referenceReduction = uninterrupted.mReductionPercent.load(
        std::memory_order_relaxed);
    CAPTURE(subjectReduction, referenceReduction, heldReduction);
    REQUIRE(subjectFinite);
    REQUIRE(referenceFinite);
    CHECK(subjectReduction
          == Catch::Approx(referenceReduction).margin(1.0e-6f));
    CHECK(subjectReduction
          == Catch::Approx(heldReduction).margin(2.0e-5f));
}
} // namespace

TEST_CASE("Safe Drive is independent of host callback partitioning",
          "[processor][band][safe-drive][callback-partition]")
{
    checkLateTransientPartitioning(false, 1);
    checkLateTransientPartitioning(false, 2);
    checkLateTransientPartitioning(true, 1);
    checkLateTransientPartitioning(true, 2);
}

TEST_CASE("Static Safe Drive keeps its established gain and meter semantics",
          "[processor][band][safe-drive][callback-partition][compatibility]")
{
    checkStaticSafeLegacy();
}

TEST_CASE("Safe Drive applies a causal immediate ceiling with a stable hold",
          "[processor][band][safe-drive][callback-partition][causal-attack]")
{
    checkCausalSafeAttack();
}

TEST_CASE("Safe Drive hold and release use the base-rate fifty millisecond timebase",
          "[processor][band][safe-drive][callback-partition][envelope]")
{
    checkSafeHoldAndReleaseTimebase(false);
    checkSafeHoldAndReleaseTimebase(true);
}

TEST_CASE("Safe Drive reset clears and quality reset retains envelope history",
          "[processor][band][safe-drive][callback-partition][envelope][lifecycle]")
{
    checkSafeResetLifecycle();
    checkQualityResetRetainsSafeHistory();
}
