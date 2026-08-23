#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int lfoPeriodSamples = 480; // 100 Hz at 48 kHz.
constexpr int legacySmoothingSamples = 2400; // 50 ms at 48 kHz.
constexpr int routeTransitionSamples = 480; // 10 ms at 48 kHz.
constexpr int stableWarmupSamples = lfoPeriodSamples * 12;
constexpr int stableComparisonSamples = lfoPeriodSamples * 12;
constexpr float driveExponentAtMaximum = 6.5f;
constexpr float stableTolerance = 3.0e-4f;

float stableInputSample(int channel, int absoluteSample)
{
    const float time = static_cast<float>(absoluteSample)
                       / static_cast<float>(sampleRate);
    const float phase = channel == 0 ? 0.0f : 0.37f;
    return 0.0024f
               * std::sin(juce::MathConstants<float>::twoPi * 733.0f * time
                          + phase)
           + 0.0009f
                 * std::cos(juce::MathConstants<float>::twoPi * 1879.0f * time
                            - phase);
}

float triangleLfo(int absoluteSample)
{
    const int wrappedSample = absoluteSample % lfoPeriodSamples;
    const float phase = static_cast<float>(wrappedSample)
                        / static_cast<float>(lfoPeriodSamples);
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

float normalDriveGain(float normalisedDrive)
{
    const float safeNormalisedDrive = juce::jlimit(0.0f,
                                                    1.0f,
                                                    normalisedDrive);
    return std::exp2(driveExponentAtMaximum * safeNormalisedDrive);
}

float hardClipOracle(float input, float driveGain)
{
    return juce::jlimit(-1.0f, 1.0f, input * driveGain);
}

BandProcessingParameters makeNeutralDriveParameters(bool useHq)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 4; // Hard clip is exactly linear for this test's quiet input.
    params.isHQ = useHq;
    params.isDriveEnabled = true;
    params.isSafeModeOn = false;
    params.isExtremeModeOn = false;
    params.isShapeEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.isDcFilterEnabled = false;

    params.driveVal.baseValue = 0.0f;
    params.driveVal.range = { 0.0f, 100.0f };
    params.biasVal.baseValue = 0.0f;
    params.biasVal.range = { -1.0f, 1.0f };
    params.recVal.baseValue = 0.0f;
    params.recVal.range = { 0.0f, 1.0f };

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

BandProcessingParameters makeStableLfoParameters(bool useHq)
{
    auto params = makeNeutralDriveParameters(useHq);

    // With a linear 0..100 range this full-depth bipolar recipe maps the
    // supplied 0..1 triangle exactly to 0..100 Drive.
    params.driveVal.baseValue = 50.0f;
    params.driveVal.modulationDepth = 1.0f;
    params.driveVal.isBipolar = true;
    params.driveLfoSourceIndex = 0;
    return params;
}

void primeBandFixture(BandProcessor& band)
{
    // Match FireAudioProcessor's playback lifecycle and keep JUCE Gain's
    // unrelated first-use state out of the Drive oracle.
    band.gain.setGainDecibels(0.0f);
    band.gain.reset();
}

// This renderer shares only JUCE's public oversampling filter with the
// subject. Drive normalisation, gain conversion and hard clipping are all
// independently analytic, so a second Drive smoother cannot hide here.
class CanonicalDriveRenderer
{
public:
    CanonicalDriveRenderer(bool useHq,
                           int numChannels,
                           int preparedBlockSize)
        : hq(useHq)
    {
        if (hq)
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
                static_cast<size_t>(numChannels),
                2,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                false);
            oversampling->initProcessing(
                static_cast<size_t>(preparedBlockSize));
        }
    }

    void process(juce::AudioBuffer<float>& buffer,
                 const float* baseRateDriveGains,
                 int numBaseRateGains)
    {
        REQUIRE(baseRateDriveGains != nullptr);
        REQUIRE(numBaseRateGains == buffer.getNumSamples());

        if (! hq)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    buffer.setSample(
                        channel,
                        sample,
                        hardClipOracle(buffer.getSample(channel, sample),
                                       baseRateDriveGains[sample]));
            return;
        }

        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto upsampled = oversampling->processSamplesUp(block);
        REQUIRE(upsampled.getNumSamples()
                == static_cast<size_t>(buffer.getNumSamples() * 4));
        for (size_t sample = 0; sample < upsampled.getNumSamples(); ++sample)
        {
            const float driveGain = baseRateDriveGains[sample / 4];
            for (size_t channel = 0;
                 channel < upsampled.getNumChannels();
                 ++channel)
            {
                auto* channelData = upsampled.getChannelPointer(channel);
                channelData[sample] = hardClipOracle(channelData[sample],
                                                     driveGain);
            }
        }
        oversampling->processSamplesDown(block);
    }

private:
    bool hq = false;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
};

void processCanonicalInPreparedChunks(CanonicalDriveRenderer& renderer,
                                      juce::AudioBuffer<float>& buffer,
                                      const std::vector<float>& driveGains,
                                      int preparedBlockSize)
{
    REQUIRE(driveGains.size()
            == static_cast<size_t>(buffer.getNumSamples()));
    for (int offset = 0; offset < buffer.getNumSamples(); offset += preparedBlockSize)
    {
        const int chunkSize = std::min(preparedBlockSize,
                                       buffer.getNumSamples() - offset);
        juce::AudioBuffer<float> chunk(buffer.getArrayOfWritePointers(),
                                       buffer.getNumChannels(),
                                       offset,
                                       chunkSize);
        renderer.process(chunk,
                         driveGains.data() + offset,
                         chunkSize);
    }
}

struct StableRender
{
    std::array<std::vector<float>, 2> subject;
    std::array<std::vector<float>, 2> canonical;
    bool finite = true;
};

StableRender renderStableLfo(bool useHq,
                            int numChannels,
                            int preparedBlockSize,
                            const std::vector<int>& hostBlockPattern)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostBlockPattern.empty());

    BandProcessor band;
    band.prepare({ sampleRate,
                   static_cast<juce::uint32>(preparedBlockSize),
                   static_cast<juce::uint32>(numChannels) });
    primeBandFixture(band);
    CanonicalDriveRenderer canonical(useHq,
                                     numChannels,
                                     preparedBlockSize);
    const auto params = makeStableLfoParameters(useHq);

    StableRender result;
    for (auto& channel : result.subject)
        channel.reserve(stableComparisonSamples);
    for (auto& channel : result.canonical)
        channel.reserve(stableComparisonSamples);

    const int totalSamples = stableWarmupSamples + stableComparisonSamples;
    int streamPosition = 0;
    size_t blockIndex = 0;
    while (streamPosition < totalSamples)
    {
        const int requested = hostBlockPattern[blockIndex
                                                % hostBlockPattern.size()];
        ++blockIndex;
        REQUIRE(requested > 0);
        const int blockSize = std::min(requested,
                                       totalSamples - streamPosition);
        juce::AudioBuffer<float> subject(numChannels, blockSize);
        juce::AudioBuffer<float> oracle(numChannels, blockSize);
        juce::AudioBuffer<float> lfoOutputs(1, blockSize);
        std::vector<float> driveGains(static_cast<size_t>(blockSize));
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            const float lfo = triangleLfo(absoluteSample);
            lfoOutputs.setSample(0, sample, lfo);
            driveGains[static_cast<size_t>(sample)] = normalDriveGain(lfo);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float value = stableInputSample(channel,
                                                      absoluteSample);
                subject.setSample(channel, sample, value);
                oracle.setSample(channel, sample, value);
            }
        }

        band.process(subject, params, lfoOutputs);
        processCanonicalInPreparedChunks(canonical,
                                         oracle,
                                         driveGains,
                                         preparedBlockSize);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            if (streamPosition + sample < stableWarmupSamples)
                continue;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float actual = subject.getSample(channel, sample);
                const float expected = oracle.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(actual)
                                && std::isfinite(expected);
                result.subject[static_cast<size_t>(channel)].push_back(actual);
                result.canonical[static_cast<size_t>(channel)].push_back(expected);
            }
        }
        streamPosition += blockSize;
    }
    return result;
}

float maximumError(const std::array<std::vector<float>, 2>& actual,
                   const std::array<std::vector<float>, 2>& expected,
                   int numChannels)
{
    float error = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        REQUIRE(actual[static_cast<size_t>(channel)].size()
                == expected[static_cast<size_t>(channel)].size());
        for (size_t sample = 0;
             sample < actual[static_cast<size_t>(channel)].size();
             ++sample)
        {
            error = std::max(
                error,
                std::abs(actual[static_cast<size_t>(channel)][sample]
                         - expected[static_cast<size_t>(channel)][sample]));
        }
    }
    return error;
}

float maximumSubjectDifference(const StableRender& first,
                               const StableRender& second,
                               int numChannels)
{
    return maximumError(first.subject, second.subject, numChannels);
}

float maximumVectorError(const std::vector<float>& actual,
                         const std::vector<float>& expected)
{
    REQUIRE(actual.size() == expected.size());
    float error = 0.0f;
    for (size_t sample = 0; sample < actual.size(); ++sample)
        error = std::max(error,
                         std::abs(actual[sample] - expected[sample]));
    return error;
}

struct AutomationRender
{
    std::vector<float> subject;
    std::vector<float> canonical;
    bool finite = true;
};

AutomationRender renderUnroutedAutomation(bool useHq)
{
    constexpr int preparedBlockSize = 64;
    constexpr int warmupSamples = 4096;
    constexpr int tailSamples = 256;
    constexpr float inputValue = 0.006f;
    constexpr float lowDrive = 12.0f;
    constexpr float highDrive = 72.0f;
    const float lowGain = normalDriveGain(lowDrive / 100.0f);
    const float highGain = normalDriveGain(highDrive / 100.0f);

    BandProcessor band;
    band.prepare({ sampleRate, preparedBlockSize, 1 });
    primeBandFixture(band);
    CanonicalDriveRenderer canonical(useHq, 1, preparedBlockSize);
    auto lowParams = makeNeutralDriveParameters(useHq);
    lowParams.driveVal.baseValue = lowDrive;
    auto highParams = lowParams;
    highParams.driveVal.baseValue = highDrive;

    auto processTimelineBlock = [&](int blockSize,
                                    const BandProcessingParameters& params,
                                    const std::vector<float>& gains,
                                    bool capture,
                                    AutomationRender& result)
    {
        juce::AudioBuffer<float> subject(1, blockSize);
        juce::AudioBuffer<float> oracle(1, blockSize);
        juce::AudioBuffer<float> lfoOutputs(1, blockSize);
        juce::FloatVectorOperations::fill(subject.getWritePointer(0),
                                          inputValue,
                                          blockSize);
        oracle.makeCopyOf(subject);
        lfoOutputs.clear();
        band.process(subject, params, lfoOutputs);
        processCanonicalInPreparedChunks(canonical,
                                         oracle,
                                         gains,
                                         preparedBlockSize);
        if (! capture)
            return;
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float actual = subject.getSample(0, sample);
            const float expected = oracle.getSample(0, sample);
            result.finite = result.finite
                            && std::isfinite(actual)
                            && std::isfinite(expected);
            result.subject.push_back(actual);
            result.canonical.push_back(expected);
        }
    };

    AutomationRender result;
    for (int offset = 0; offset < warmupSamples; offset += preparedBlockSize)
    {
        const int blockSize = std::min(preparedBlockSize,
                                       warmupSamples - offset);
        processTimelineBlock(blockSize,
                             lowParams,
                             std::vector<float>(static_cast<size_t>(blockSize),
                                                lowGain),
                             false,
                             result);
    }

    const int captureSamples = legacySmoothingSamples + tailSamples;
    result.subject.reserve(static_cast<size_t>(captureSamples));
    result.canonical.reserve(static_cast<size_t>(captureSamples));
    for (int offset = 0; offset < captureSamples; offset += preparedBlockSize)
    {
        const int blockSize = std::min(preparedBlockSize,
                                       captureSamples - offset);
        std::vector<float> gains(static_cast<size_t>(blockSize));
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int eventSample = offset + sample;
            const int completedSteps = std::min(eventSample + 1,
                                                legacySmoothingSamples);
            const float mix = static_cast<float>(completedSteps)
                              / static_cast<float>(legacySmoothingSamples);
            gains[static_cast<size_t>(sample)] = lowGain
                                                 + mix * (highGain - lowGain);
        }
        processTimelineBlock(blockSize,
                             highParams,
                             gains,
                             true,
                             result);
    }
    return result;
}

struct DriveRouteRecipe
{
    int sourceIndex = -1;
    float depth = 0.0f;
    bool bipolar = true;
    std::array<float, 2> lfoValues { 0.95f, 0.40f };
};

constexpr float recipeBaseDrive = 50.0f;
constexpr float recipeProbe = 0.006f;

BandProcessingParameters makeBridgeParameters(const DriveRouteRecipe& recipe)
{
    auto params = makeNeutralDriveParameters(false);
    params.driveVal.baseValue = recipeBaseDrive;
    params.driveVal.modulationDepth = recipe.depth;
    params.driveVal.isBipolar = recipe.bipolar;
    params.driveLfoSourceIndex = recipe.sourceIndex;
    return params;
}

juce::AudioBuffer<float> makeConstantLfo(const DriveRouteRecipe& recipe,
                                         int numSamples)
{
    juce::AudioBuffer<float> result(2, numSamples);
    for (int channel = 0; channel < result.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(
            result.getWritePointer(channel),
            recipe.lfoValues[static_cast<size_t>(channel)],
            numSamples);
    return result;
}

float recipeTargetGain(const DriveRouteRecipe& recipe)
{
    float normalisedDrive = recipeBaseDrive / 100.0f;
    if (recipe.sourceIndex >= 0)
    {
        REQUIRE(recipe.sourceIndex
                < static_cast<int>(recipe.lfoValues.size()));
        const float lfo = juce::jlimit(
            0.0f,
            1.0f,
            recipe.lfoValues[static_cast<size_t>(recipe.sourceIndex)]);
        const float mappedLfo = recipe.bipolar ? 2.0f * lfo - 1.0f : lfo;
        const float effectiveDepth = recipe.bipolar
                                         ? recipe.depth * 0.5f
                                         : recipe.depth;
        normalisedDrive = juce::jlimit(0.0f,
                                       1.0f,
                                       normalisedDrive
                                           + mappedLfo * effectiveDepth);
    }
    return normalDriveGain(normalisedDrive);
}

std::vector<float> processBridgeBlock(BandProcessor& band,
                                      const DriveRouteRecipe& recipe,
                                      int numSamples)
{
    juce::AudioBuffer<float> buffer(1, numSamples);
    juce::FloatVectorOperations::fill(buffer.getWritePointer(0),
                                      recipeProbe,
                                      numSamples);
    auto lfoOutputs = makeConstantLfo(recipe, numSamples);
    const auto params = makeBridgeParameters(recipe);
    band.process(buffer, params, lfoOutputs);

    std::vector<float> output;
    output.reserve(static_cast<size_t>(numSamples));
    for (int sample = 0; sample < numSamples; ++sample)
        output.push_back(buffer.getSample(0, sample));
    return output;
}

void settleBridgeRecipe(BandProcessor& band,
                        const DriveRouteRecipe& recipe)
{
    // This exceeds both the new 480-sample recipe bridge and the old broken
    // 2400-sample final-value dezipper, keeping each event's old endpoint pure.
    const auto output = processBridgeBlock(band, recipe, 3000);
    REQUIRE_FALSE(output.empty());
    CHECK(output.back()
          == Catch::Approx(hardClipOracle(recipeProbe,
                                          recipeTargetGain(recipe)))
                 .margin(2.0e-5f));
}

void checkBridgeEvent(const std::vector<float>& output,
                      float oldGain,
                      float newGain)
{
    REQUIRE(output.size()
            >= static_cast<size_t>(routeTransitionSamples + 1));
    const float expectedStart = hardClipOracle(recipeProbe, oldGain);
    const float expectedMidpoint = hardClipOracle(recipeProbe,
                                                  0.5f
                                                      * (oldGain + newGain));
    const float expectedEnd = hardClipOracle(recipeProbe, newGain);
    CAPTURE(oldGain,
            newGain,
            output[0],
            output[routeTransitionSamples / 2],
            output[routeTransitionSamples],
            expectedStart,
            expectedMidpoint,
            expectedEnd);
    REQUIRE(std::abs(expectedEnd - expectedStart) > 2.0e-3f);
    CHECK(output[0] == Catch::Approx(expectedStart).margin(2.0e-5f));
    CHECK(output[routeTransitionSamples / 2]
          == Catch::Approx(expectedMidpoint).margin(2.0e-5f));
    CHECK(output[routeTransitionSamples]
          == Catch::Approx(expectedEnd).margin(2.0e-5f));
}
} // namespace

TEST_CASE("Drive LFO trajectories bypass the legacy 50 ms dezipper",
          "[band][drive][lfo][block-size][internal-chunk]")
{
    const std::array<std::pair<const char*, std::vector<int>>, 3> partitions {{
        { "fixed", { lfoPeriodSamples } },
        { "irregular", { 53, 17, 91, 7, 251, 61 } },
        { "internal-64", { lfoPeriodSamples } }
    }};

    for (const bool useHq : { false, true })
        for (const int numChannels : { 1, 2 })
        {
            StableRender fixed;
            for (const auto& [name, blocks] : partitions)
            {
                const int preparedBlockSize = std::string_view(name)
                                                      == "internal-64"
                                                  ? 64
                                                  : lfoPeriodSamples;
                const auto rendered = renderStableLfo(useHq,
                                                      numChannels,
                                                      preparedBlockSize,
                                                      blocks);
                const float oracleError = maximumError(rendered.subject,
                                                       rendered.canonical,
                                                       numChannels);
                CAPTURE(useHq,
                        numChannels,
                        name,
                        oracleError);
                REQUIRE(rendered.finite);
                REQUIRE(rendered.subject[0].size()
                        == stableComparisonSamples);

                // The old implementation repeatedly restarted a 2400-step
                // final-gain ramp and misses this 100 Hz trajectory by orders
                // of magnitude more than the numerical/oversampling tolerance.
                CHECK(oracleError < stableTolerance);

                if (std::string_view(name) == "fixed")
                    fixed = rendered;
                else
                {
                    const float partitionError = maximumSubjectDifference(
                        fixed,
                        rendered,
                        numChannels);
                    CAPTURE(partitionError);
                    CHECK(partitionError < stableTolerance);
                }
            }
        }
}

TEST_CASE("Unrouted Drive automation retains its legacy 50 ms linear-gain ramp",
          "[band][drive][automation][smoothing]")
{
    for (const bool useHq : { false, true })
    {
        const auto rendered = renderUnroutedAutomation(useHq);
        const float oracleError = maximumVectorError(rendered.subject,
                                                     rendered.canonical);
        CAPTURE(useHq, oracleError);
        REQUIRE(rendered.finite);
        REQUIRE(rendered.subject.size()
                >= static_cast<size_t>(legacySmoothingSamples + 128));
        CHECK(oracleError < stableTolerance);

        if (! useHq)
        {
            constexpr float inputValue = 0.006f;
            const float lowGain = normalDriveGain(0.12f);
            const float highGain = normalDriveGain(0.72f);
            const float firstGain = lowGain
                                    + (highGain - lowGain)
                                          / legacySmoothingSamples;
            const float midpointGain = 0.5f * (lowGain + highGain);
            CHECK(rendered.subject[0]
                  == Catch::Approx(inputValue * firstGain).margin(2.0e-5f));
            CHECK(rendered.subject[legacySmoothingSamples / 2 - 1]
                  == Catch::Approx(inputValue * midpointGain).margin(2.0e-5f));
            CHECK(rendered.subject[legacySmoothingSamples - 1]
                  == Catch::Approx(inputValue * highGain).margin(2.0e-5f));
        }
    }
}

TEST_CASE("Drive route edits use a 10 ms held-anchor linear-gain bridge",
          "[band][drive][lfo][route][transition]")
{
    const DriveRouteRecipe unrouted {};
    const DriveRouteRecipe attached { 0, 0.80f, true, { 0.95f, 0.40f } };
    const DriveRouteRecipe sourceChanged { 1,
                                           0.80f,
                                           true,
                                           { 0.95f, 0.40f } };
    const DriveRouteRecipe depthChanged { 1,
                                          0.25f,
                                          true,
                                          { 0.95f, 0.40f } };
    const DriveRouteRecipe polarityChanged { 1,
                                             0.25f,
                                             false,
                                             { 0.95f, 0.40f } };
    const std::array<DriveRouteRecipe, 5> edits {
        attached,
        sourceChanged,
        depthChanged,
        polarityChanged,
        unrouted
    };

    BandProcessor band;
    band.prepare({ sampleRate, 512, 1 });
    primeBandFixture(band);
    DriveRouteRecipe previous = unrouted;
    settleBridgeRecipe(band, previous);
    for (const auto& next : edits)
    {
        const float oldGain = recipeTargetGain(previous);
        const float newGain = recipeTargetGain(next);
        const auto output = processBridgeBlock(band,
                                               next,
                                               routeTransitionSamples + 1);
        checkBridgeEvent(output, oldGain, newGain);
        settleBridgeRecipe(band, next);
        previous = next;
    }
}

TEST_CASE("Rapid Drive route edits retain the latest audible anchor and reset snaps fresh",
          "[band][drive][lfo][route][transition][reset]")
{
    const DriveRouteRecipe unrouted {};
    const DriveRouteRecipe firstRoute { 0,
                                        0.62f,
                                        true,
                                        { 0.92f, 0.18f } };
    const DriveRouteRecipe latestRoute { 1,
                                         0.74f,
                                         false,
                                         { 0.92f, 0.30f } };
    const DriveRouteRecipe resetInFlightRoute { 0,
                                                0.41f,
                                                false,
                                                { 0.25f, 0.85f } };

    BandProcessor band;
    band.prepare({ sampleRate, 512, 1 });
    primeBandFixture(band);
    settleBridgeRecipe(band, unrouted);

    const float oldGain = recipeTargetGain(unrouted);
    const float firstTarget = recipeTargetGain(firstRoute);
    const auto firstPartial = processBridgeBlock(band, firstRoute, 160);
    REQUIRE(firstPartial.size() == 160);

    // The 160th consumed sample used mix=159/480. A third request must begin
    // from that exact audible gain, not either recipe endpoint.
    const float heldAnchorGain = oldGain
                                 + (159.0f / routeTransitionSamples)
                                       * (firstTarget - oldGain);
    CHECK(firstPartial.front()
          == Catch::Approx(hardClipOracle(recipeProbe, oldGain))
                 .margin(2.0e-5f));
    CHECK(firstPartial.back()
          == Catch::Approx(hardClipOracle(recipeProbe, heldAnchorGain))
                 .margin(2.0e-5f));

    const float latestTarget = recipeTargetGain(latestRoute);
    const auto latest = processBridgeBlock(band,
                                           latestRoute,
                                           routeTransitionSamples + 1);
    checkBridgeEvent(latest, heldAnchorGain, latestTarget);

    const auto resetPartial = processBridgeBlock(band,
                                                 resetInFlightRoute,
                                                 160);
    REQUIRE(resetPartial.size() == 160);
    band.reset();
    primeBandFixture(band);
    const auto fresh = processBridgeBlock(band, latestRoute, 1);
    REQUIRE(fresh.size() == 1);
    const float expectedFresh = hardClipOracle(recipeProbe, latestTarget);
    CAPTURE(heldAnchorGain,
            latestTarget,
            fresh.front(),
            expectedFresh);
    CHECK(fresh.front()
          == Catch::Approx(expectedFresh).margin(2.0e-5f));
}

TEST_CASE("Active routed Drive bridges Extreme changes in the linear-gain domain",
          "[band][drive][lfo][route][transition][extreme]")
{
    constexpr float lfoValue = 0.75f;
    constexpr float depth = 0.08f;
    constexpr float normalisedDrive = lfoValue * depth;
    const float normalGain = normalDriveGain(normalisedDrive);
    const float extremeGain = std::exp2(std::log2(10.0f)
                                        * driveExponentAtMaximum
                                        * normalisedDrive);

    auto params = makeNeutralDriveParameters(false);
    params.driveVal.baseValue = 0.0f; // Extreme cannot move the base smoother.
    params.driveVal.modulationDepth = depth;
    params.driveVal.isBipolar = false;
    params.driveLfoSourceIndex = 0;

    BandProcessor band;
    band.prepare({ sampleRate, 512, 1 });
    primeBandFixture(band);
    const auto process = [&](int numSamples)
    {
        juce::AudioBuffer<float> buffer(1, numSamples);
        juce::AudioBuffer<float> lfoOutputs(1, numSamples);
        juce::FloatVectorOperations::fill(buffer.getWritePointer(0),
                                          recipeProbe,
                                          numSamples);
        juce::FloatVectorOperations::fill(lfoOutputs.getWritePointer(0),
                                          lfoValue,
                                          numSamples);
        band.process(buffer, params, lfoOutputs);
        std::vector<float> output(static_cast<size_t>(numSamples));
        for (int sample = 0; sample < numSamples; ++sample)
            output[static_cast<size_t>(sample)] = buffer.getSample(0, sample);
        return output;
    };

    const auto settledNormal = process(512);
    REQUIRE_FALSE(settledNormal.empty());
    CHECK(settledNormal.back()
          == Catch::Approx(hardClipOracle(recipeProbe, normalGain))
                 .margin(2.0e-5f));

    params.isExtremeModeOn = true;
    const auto transitioned = process(routeTransitionSamples + 1);
    checkBridgeEvent(transitioned, normalGain, extremeGain);
}

TEST_CASE("Drive disable ramps from the last post-Safe audible gain",
          "[band][drive][safe-drive][enable][smoothing]")
{
    constexpr float transient = 3.0f;
    constexpr float probe = 0.10f;
    const float requestedGain = normalDriveGain(1.0f);
    const float requestedDriveForCalc = std::log2(requestedGain);
    const float safeGain = 2.0f / transient
                           + 0.1f * requestedDriveForCalc;

    auto params = makeNeutralDriveParameters(false);
    params.isSafeModeOn = true;
    params.driveVal.baseValue = 100.0f;

    BandProcessor band;
    band.prepare({ sampleRate,
                   static_cast<juce::uint32>(legacySmoothingSamples),
                   1 });
    primeBandFixture(band);

    juce::AudioBuffer<float> event(1, 1);
    juce::AudioBuffer<float> eventLfo(1, 1);
    event.setSample(0, 0, transient);
    eventLfo.clear();
    band.process(event, params, eventLfo);
    const float eventReduction = band.mReductionPercent.load(
        std::memory_order_relaxed);
    REQUIRE(std::isfinite(eventReduction));
    REQUIRE(eventReduction < 1.0f);

    params.isDriveEnabled = false;
    juce::AudioBuffer<float> output(1, legacySmoothingSamples);
    juce::AudioBuffer<float> lfoOutputs(1, legacySmoothingSamples);
    juce::FloatVectorOperations::fill(output.getWritePointer(0),
                                      probe,
                                      output.getNumSamples());
    lfoOutputs.clear();
    band.process(output, params, lfoOutputs);

    bool finite = true;
    float maximumError = 0.0f;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        const float mix = static_cast<float>(sample + 1)
                          / static_cast<float>(legacySmoothingSamples);
        const float expectedGain = safeGain + mix * (1.0f - safeGain);
        const float actual = output.getSample(0, sample);
        const float expected = hardClipOracle(probe, expectedGain);
        finite = finite && std::isfinite(actual);
        maximumError = std::max(maximumError, std::abs(actual - expected));
    }

    const float firstGain = safeGain
                            + (1.0f - safeGain)
                                  / legacySmoothingSamples;
    CAPTURE(safeGain,
            eventReduction,
            output.getSample(0, 0),
            output.getSample(0, legacySmoothingSamples - 1),
            maximumError);
    REQUIRE(finite);
    CHECK(output.getSample(0, 0)
          == Catch::Approx(probe * firstGain).margin(2.0e-5f));
    CHECK(output.getSample(0, legacySmoothingSamples - 1)
          == Catch::Approx(probe).margin(2.0e-5f));
    CHECK(maximumError < 2.0e-5f);
}
