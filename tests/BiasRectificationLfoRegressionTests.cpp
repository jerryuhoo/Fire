#include <PluginProcessor.h>
#include <DSP/DistortionLogic.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string_view>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int lfoPeriodSamples = 480; // 100 Hz at 48 kHz.
constexpr int smoothingSamples = 2400; // Legacy 50 ms control dezipper.
constexpr int warmupSamples = lfoPeriodSamples * 12;
constexpr int comparisonSamples = lfoPeriodSamples * 12;

enum class ShapeTarget
{
    bias,
    rectification
};

struct RenderResult
{
    std::array<std::vector<float>, 2> subject;
    std::array<std::vector<float>, 2> canonical;
    bool finite = true;
};

float inputSample(int channel, int absoluteSample)
{
    const float time = static_cast<float>(absoluteSample) / static_cast<float>(sampleRate);
    const float phase = channel == 0 ? 0.0f : 0.41f;
    return 0.49f * std::sin(juce::MathConstants<float>::twoPi * 733.0f * time + phase)
           + 0.17f * std::cos(juce::MathConstants<float>::twoPi * 1879.0f * time - phase);
}

float triangleLfo(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoPeriodSamples)
                        / static_cast<float>(lfoPeriodSamples);
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

float modulatedValue(ShapeTarget target, int absoluteSample)
{
    const float lfo = triangleLfo(absoluteSample);
    if (target == ShapeTarget::bias)
        return -0.82f + 1.64f * lfo;

    return 0.90f * lfo;
}

BandProcessingParameters makeParameters(ShapeTarget target, bool useHq, bool routed)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 2; // tanh: memoryless and strongly sensitive to both controls.
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isDriveEnabled = false;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.shapeMixVal = 1.0f;
    params.shapeMixValProvider.baseValue = 1.0f;
    params.driveVal.baseValue = 0.0f;
    params.driveVal.range = { 0.0f, 100.0f };

    // A bipolar, full-depth route is exactly the 0..1 triangle mapped over
    // the supplied normalisable range. Keep the non-target control neutral.
    params.biasVal.baseValue = 0.0f;
    params.biasVal.modulationDepth = routed && target == ShapeTarget::bias ? 1.0f : 0.0f;
    params.biasVal.range = { -0.82f, 0.82f };
    params.biasLfoSourceIndex = routed && target == ShapeTarget::bias ? 0 : -1;
    params.recVal.baseValue = routed && target == ShapeTarget::rectification
                                  ? 0.45f
                                  : 0.0f;
    params.recVal.modulationDepth = routed && target == ShapeTarget::rectification ? 1.0f : 0.0f;
    params.recVal.range = { 0.0f, 0.90f };
    params.recLfoSourceIndex = routed && target == ShapeTarget::rectification ? 0 : -1;
    return params;
}

DistortionLogic::State canonicalState(ShapeTarget target, int baseSample)
{
    DistortionLogic::State state;
    state.mode = 2;
    if (target == ShapeTarget::bias)
        state.bias = modulatedValue(target, baseSample);
    else
        state.rec = modulatedValue(target, baseSample);
    return state;
}

// This is deliberately not a BandProcessor-derived endpoint. It mirrors only
// the public HQ contract: the same JUCE oversampling filter, a held base-rate
// LFO value for each 4x frame, and the standalone memoryless shape transfer.
// Therefore a second 50 ms smoother in front of a stable LFO cannot hide here.
class CanonicalShapeRenderer
{
public:
    CanonicalShapeRenderer(bool useHq, int numChannels, int preparedBlockSize)
        : hq(useHq)
    {
        if (hq)
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
                static_cast<size_t>(numChannels),
                2,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                false);
            oversampling->initProcessing(static_cast<size_t>(preparedBlockSize));
        }
    }

    void process(juce::AudioBuffer<float>& buffer,
                 ShapeTarget target,
                 int absoluteStartSample)
    {
        if (! hq)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto state = canonicalState(target, absoluteStartSample + sample);
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    buffer.setSample(channel, sample,
                                     DistortionLogic::processSample(
                                         buffer.getSample(channel, sample), state));
            }
            return;
        }

        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto upsampled = oversampling->processSamplesUp(block);
        constexpr int oversamplingRatio = 4;
        for (size_t sample = 0; sample < upsampled.getNumSamples(); ++sample)
        {
            const auto state = canonicalState(
                target,
                absoluteStartSample + static_cast<int>(sample) / oversamplingRatio);
            for (size_t channel = 0; channel < upsampled.getNumChannels(); ++channel)
            {
                auto* channelData = upsampled.getChannelPointer(channel);
                channelData[sample] = DistortionLogic::processSample(channelData[sample], state);
            }
        }
        oversampling->processSamplesDown(block);
    }

private:
    bool hq = false;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
};

void processCanonicalInPreparedChunks(CanonicalShapeRenderer& renderer,
                                      juce::AudioBuffer<float>& buffer,
                                      ShapeTarget target,
                                      int absoluteStartSample,
                                      int preparedBlockSize)
{
    for (int offset = 0; offset < buffer.getNumSamples(); offset += preparedBlockSize)
    {
        const int chunkSize = std::min(preparedBlockSize, buffer.getNumSamples() - offset);
        juce::AudioBuffer<float> chunk(buffer.getArrayOfWritePointers(),
                                       buffer.getNumChannels(), offset, chunkSize);
        renderer.process(chunk, target, absoluteStartSample + offset);
    }
}

RenderResult renderStableLfo(ShapeTarget target,
                             bool useHq,
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
    CanonicalShapeRenderer canonical(useHq, numChannels, preparedBlockSize);
    const auto params = makeParameters(target, useHq, true);

    RenderResult result;
    for (auto& channel : result.subject)
        channel.reserve(comparisonSamples);
    for (auto& channel : result.canonical)
        channel.reserve(comparisonSamples);

    const int totalSamples = warmupSamples + comparisonSamples;
    int streamPosition = 0;
    size_t blockIndex = 0;
    while (streamPosition < totalSamples)
    {
        const int requested = hostBlockPattern[blockIndex % hostBlockPattern.size()];
        ++blockIndex;
        REQUIRE(requested > 0);
        const int blockSize = std::min(requested, totalSamples - streamPosition);
        juce::AudioBuffer<float> subject(numChannels, blockSize);
        juce::AudioBuffer<float> oracle(numChannels, blockSize);
        juce::AudioBuffer<float> lfoOutputs(1, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            lfoOutputs.setSample(0, sample, triangleLfo(absoluteSample));
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float value = inputSample(channel, absoluteSample);
                subject.setSample(channel, sample, value);
                oracle.setSample(channel, sample, value);
            }
        }

        band.process(subject, params, lfoOutputs);
        processCanonicalInPreparedChunks(canonical,
                                         oracle,
                                         target,
                                         streamPosition,
                                         preparedBlockSize);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            if (streamPosition + sample < warmupSamples)
                continue;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float actual = subject.getSample(channel, sample);
                const float expected = oracle.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(actual) && std::isfinite(expected);
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
        for (size_t sample = 0; sample < actual[static_cast<size_t>(channel)].size(); ++sample)
            error = std::max(error,
                             std::abs(actual[static_cast<size_t>(channel)][sample]
                                      - expected[static_cast<size_t>(channel)][sample]));
    }
    return error;
}

float maximumDifference(const RenderResult& first, const RenderResult& second, int numChannels)
{
    return maximumError(first.subject, second.subject, numChannels);
}

float staticValue(ShapeTarget target, bool highEndpoint)
{
    if (target == ShapeTarget::bias)
        return highEndpoint ? 0.70f : -0.70f;
    return highEndpoint ? 0.82f : 0.08f;
}

BandProcessingParameters makeStaticParameters(ShapeTarget target,
                                               bool useHq,
                                               bool highEndpoint)
{
    auto params = makeParameters(target, useHq, false);
    if (target == ShapeTarget::bias)
        params.biasVal.baseValue = staticValue(target, highEndpoint);
    else
        params.recVal.baseValue = staticValue(target, highEndpoint);
    return params;
}

float automationInput(ShapeTarget target)
{
    // Rectification only changes the negative half; Bias needs an unsaturated
    // positive probe. A constant input removes all unrelated filter state.
    return target == ShapeTarget::bias ? 0.28f : -0.43f;
}

std::vector<float> renderStaticEndpoint(ShapeTarget target, bool useHq, bool highEndpoint)
{
    BandProcessor band;
    constexpr int blockSize = 64;
    band.prepare({ sampleRate, blockSize, 1 });
    const auto params = makeStaticParameters(target, useHq, highEndpoint);
    std::vector<float> result;
    result.reserve(128);
    for (int block = 0; block < 64; ++block)
    {
        juce::AudioBuffer<float> buffer(1, blockSize);
        buffer.clear();
        juce::FloatVectorOperations::fill(buffer.getWritePointer(0), automationInput(target), blockSize);
        juce::AudioBuffer<float> lfoOutputs(1, blockSize);
        lfoOutputs.clear();
        band.process(buffer, params, lfoOutputs);
        if (block == 63)
            for (int sample = 0; sample < blockSize; ++sample)
                result.push_back(buffer.getSample(0, sample));
    }
    return result;
}

std::vector<float> renderUnroutedAutomation(ShapeTarget target, bool useHq)
{
    BandProcessor band;
    constexpr int blockSize = 64;
    band.prepare({ sampleRate, blockSize, 1 });
    const auto low = makeStaticParameters(target, useHq, false);
    const auto high = makeStaticParameters(target, useHq, true);
    juce::AudioBuffer<float> lfoOutputs(1, blockSize);
    lfoOutputs.clear();

    // Prime both the shape and all legacy mix/gain endpoints before the event.
    for (int block = 0; block < 64; ++block)
    {
        juce::AudioBuffer<float> buffer(1, blockSize);
        juce::FloatVectorOperations::fill(buffer.getWritePointer(0), automationInput(target), blockSize);
        band.process(buffer, low, lfoOutputs);
    }

    std::vector<float> result;
    result.reserve(smoothingSamples + 256);
    const int totalSamples = smoothingSamples + 256;
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        const int samplesThisBlock = std::min(blockSize, totalSamples - offset);
        juce::AudioBuffer<float> buffer(1, samplesThisBlock);
        juce::FloatVectorOperations::fill(buffer.getWritePointer(0), automationInput(target), samplesThisBlock);
        juce::AudioBuffer<float> blockLfo(1, samplesThisBlock);
        blockLfo.clear();
        band.process(buffer, high, blockLfo);
        for (int sample = 0; sample < samplesThisBlock; ++sample)
            result.push_back(buffer.getSample(0, sample));
    }
    return result;
}

// A route recipe is deliberately represented independently of a parameter
// object so the test can make source/depth/polarity edits without accidentally
// carrying an already-bound signal pointer between callbacks.
struct ShapeRouteRecipe
{
    int sourceIndex = -1;
    float depth = 0.0f;
    bool bipolar = true;
    std::array<float, 2> lfoValues { 0.0f, 0.0f };
};

BandProcessingParameters makeBridgeParameters(ShapeTarget target,
                                               const ShapeRouteRecipe& recipe)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 8; // Limit has an easily separated, memoryless probe transfer.
    params.isHQ = false;
    params.isSafeModeOn = false;
    params.isDriveEnabled = false;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.shapeMixVal = 1.0f;
    params.shapeMixValProvider.baseValue = 1.0f;
    params.driveVal.baseValue = 0.0f;
    params.driveVal.range = { 0.0f, 100.0f };

    // Keep the non-target control neutral. In particular, a positive Bias
    // would move the Rectification probe above zero before the negative-only
    // rectifier stage and make that entire bridge test insensitive.
    params.biasVal.baseValue = target == ShapeTarget::bias ? 0.55f : 0.0f;
    params.biasVal.range = { 0.20f, 0.90f };
    params.recVal.baseValue = 0.50f;
    params.recVal.range = { 0.10f, 0.90f };
    if (target == ShapeTarget::bias)
    {
        params.biasVal.modulationDepth = recipe.depth;
        params.biasVal.isBipolar = recipe.bipolar;
        params.biasLfoSourceIndex = recipe.sourceIndex;
    }
    else
    {
        params.recVal.modulationDepth = recipe.depth;
        params.recVal.isBipolar = recipe.bipolar;
        params.recLfoSourceIndex = recipe.sourceIndex;
    }
    return params;
}

juce::AudioBuffer<float> makeConstantLfo(const ShapeRouteRecipe& recipe,
                                         int numSamples)
{
    juce::AudioBuffer<float> result(2, numSamples);
    for (int channel = 0; channel < result.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(result.getWritePointer(channel),
                                           recipe.lfoValues[static_cast<size_t>(channel)],
                                           numSamples);
    return result;
}

float routeTarget(ShapeTarget target, const ShapeRouteRecipe& recipe)
{
    auto params = makeBridgeParameters(target, recipe);
    auto provider = target == ShapeTarget::bias ? params.biasVal : params.recVal;
    if (recipe.sourceIndex < 0)
        return provider.baseValue;
    REQUIRE(recipe.sourceIndex < static_cast<int>(recipe.lfoValues.size()));
    provider.lfoSignal = &recipe.lfoValues[static_cast<size_t>(recipe.sourceIndex)];
    return provider.get(0);
}

float bridgeProbe(ShapeTarget target)
{
    return target == ShapeTarget::bias ? 0.0f : -0.31f;
}

float probeOutput(ShapeTarget target, float controlValue)
{
    DistortionLogic::State state;
    state.mode = 8;
    if (target == ShapeTarget::bias)
        state.bias = controlValue;
    else
        state.rec = controlValue;
    return DistortionLogic::processSample(bridgeProbe(target), state);
}

std::vector<float> processBridgeBlock(BandProcessor& band,
                                      ShapeTarget target,
                                      const ShapeRouteRecipe& recipe,
                                      int numSamples)
{
    juce::AudioBuffer<float> buffer(1, numSamples);
    juce::FloatVectorOperations::fill(buffer.getWritePointer(0),
                                       bridgeProbe(target), numSamples);
    auto lfoOutputs = makeConstantLfo(recipe, numSamples);
    const auto params = makeBridgeParameters(target, recipe);
    band.process(buffer, params, lfoOutputs);

    std::vector<float> output;
    output.reserve(static_cast<size_t>(numSamples));
    for (int sample = 0; sample < numSamples; ++sample)
        output.push_back(buffer.getSample(0, sample));
    return output;
}

void settleBridgeRecipe(BandProcessor& band,
                        ShapeTarget target,
                        const ShapeRouteRecipe& recipe)
{
    // 512 samples is longer than the 10 ms/480 sample route bridge.
    const auto output = processBridgeBlock(band, target, recipe, 512);
    REQUIRE_FALSE(output.empty());
}

void primeBridgeFixture(BandProcessor& band)
{
    // Fire primes each public BandProcessor Gain to the current parameter
    // before playback. Mirror that lifecycle so this shape-control oracle is
    // not multiplied by Gain's unrelated first-use 50 ms fade from zero.
    band.gain.setGainDecibels(0.0f);
    band.gain.reset();
}

void checkBridgeEvent(const std::vector<float>& output,
                      ShapeTarget target,
                      float oldValue,
                      float newValue)
{
    REQUIRE(output.size() >= 481);
    const float expectedStart = probeOutput(target, oldValue);
    const float expectedMidpoint = probeOutput(target, 0.5f * (oldValue + newValue));
    const float expectedEnd = probeOutput(target, newValue);
    CAPTURE(target == ShapeTarget::bias ? "Bias" : "Rectification",
            oldValue,
            newValue,
            output[0],
            output[240],
            output[480],
            expectedStart,
            expectedMidpoint,
            expectedEnd);

    REQUIRE(std::abs(expectedEnd - expectedStart) > 1.0e-3f);

    // The event sample consumes the preceding audible value. The next 480
    // base-rate frames form the independent 10 ms route-recipe bridge.
    CHECK(output[0] == Catch::Approx(expectedStart).margin(1.0e-5f));
    CHECK(output[240] == Catch::Approx(expectedMidpoint).margin(1.0e-5f));
    CHECK(output[480] == Catch::Approx(expectedEnd).margin(1.0e-5f));
}
} // namespace

TEST_CASE("Bias and Rectification LFO trajectories bypass the legacy 50 ms dezipper",
          "[band][shape][bias][rectification][lfo][block-size][internal-chunk]")
{
    const std::array<std::pair<const char*, std::vector<int>>, 3> partitions {{
        { "fixed", { lfoPeriodSamples } },
        { "irregular", { 53, 17, 91, 7, 251, 61 } },
        { "internal-64", { lfoPeriodSamples } }
    }};

    for (const auto target : { ShapeTarget::bias, ShapeTarget::rectification })
        for (const bool useHq : { false, true })
            for (const int numChannels : { 1, 2 })
            {
                RenderResult fixed;
                for (const auto& [name, blocks] : partitions)
                {
                    const int preparedBlockSize = std::string_view(name) == "internal-64"
                                                      ? 64
                                                      : lfoPeriodSamples;
                    const auto rendered = renderStableLfo(target,
                                                          useHq,
                                                          numChannels,
                                                          preparedBlockSize,
                                                          blocks);
                    const float oracleError = maximumError(rendered.subject,
                                                           rendered.canonical,
                                                           numChannels);
                    CAPTURE(target == ShapeTarget::bias ? "Bias" : "Rectification",
                            useHq,
                            numChannels,
                            name,
                            oracleError);
                    REQUIRE(rendered.finite);
                    REQUIRE(rendered.subject[0].size() == comparisonSamples);

                    // A stable routed trajectory is an exact per-sample shape
                    // control after route settling. The old implementation
                    // retargeted a 50 ms smoother every sample; at 100 Hz that
                    // produces a conspicuous (roughly 0.1--0.5) transfer error.
                    CHECK(oracleError < 3.0e-4f);

                    if (std::string_view(name) == "fixed")
                        fixed = rendered;
                    else
                    {
                        const float partitionError = maximumDifference(fixed,
                                                                        rendered,
                                                                        numChannels);
                        CAPTURE(partitionError);
                        CHECK(partitionError < 3.0e-4f);
                    }
                }
            }
}

TEST_CASE("Unrouted Bias and Rectification automation retains the legacy 50 ms ramp",
          "[band][shape][bias][rectification][automation][smoothing]")
{
    for (const auto target : { ShapeTarget::bias, ShapeTarget::rectification })
        for (const bool useHq : { false, true })
        {
            const auto lowEndpoint = renderStaticEndpoint(target, useHq, false);
            const auto highEndpoint = renderStaticEndpoint(target, useHq, true);
            const auto automation = renderUnroutedAutomation(target, useHq);
            REQUIRE(lowEndpoint.size() == highEndpoint.size());
            REQUIRE(automation.size() >= static_cast<size_t>(smoothingSamples + 128));

            const float low = lowEndpoint.back();
            const float high = highEndpoint.back();
            const int midpoint = smoothingSamples / 2;
            const float startDistanceToLow = std::abs(automation.front() - low);
            const float midpointDistanceToLow = std::abs(automation[static_cast<size_t>(midpoint)] - low);
            const float midpointDistanceToHigh = std::abs(automation[static_cast<size_t>(midpoint)] - high);
            const float finalDistanceToHigh = std::abs(automation.back() - high);
            CAPTURE(target == ShapeTarget::bias ? "Bias" : "Rectification",
                    useHq,
                    low,
                    high,
                    startDistanceToLow,
                    midpointDistanceToLow,
                    midpointDistanceToHigh,
                    finalDistanceToHigh);

            REQUIRE(std::abs(high - low) > 0.05f);
            CHECK(startDistanceToLow < 2.0e-3f);
            CHECK(midpointDistanceToLow > 0.01f);
            CHECK(midpointDistanceToHigh > 0.01f);
            CHECK(finalDistanceToHigh < 2.0e-3f);
        }
}

TEST_CASE("Bias and Rectification route edits use a 10 ms held-anchor bridge",
          "[band][shape][bias][rectification][lfo][route][transition]")
{
    // Each recipe change targets a different constant LFO value. Together the
    // sequence covers attach, source, depth and bipolar edits, then detach.
    const ShapeRouteRecipe unrouted {};
    const ShapeRouteRecipe attached { 0, 0.50f, true, { 1.00f, 0.20f } };
    const ShapeRouteRecipe sourceChanged { 1, 0.50f, true, { 1.00f, 0.10f } };
    const ShapeRouteRecipe depthChanged { 1, 0.82f, true, { 1.00f, 0.10f } };
    const ShapeRouteRecipe polarityChanged { 1, 0.82f, false, { 1.00f, 0.10f } };
    const std::array<ShapeRouteRecipe, 5> edits {
        attached, sourceChanged, depthChanged, polarityChanged, unrouted
    };

    for (const auto target : { ShapeTarget::bias, ShapeTarget::rectification })
    {
        BandProcessor band;
        band.prepare({ sampleRate, 512, 1 });
        primeBridgeFixture(band);
        ShapeRouteRecipe previous = unrouted;
        settleBridgeRecipe(band, target, previous);
        for (const auto& next : edits)
        {
            const float oldValue = routeTarget(target, previous);
            const float newValue = routeTarget(target, next);
            const auto output = processBridgeBlock(band, target, next, 481);
            checkBridgeEvent(output, target, oldValue, newValue);
            settleBridgeRecipe(band, target, next);
            previous = next;
        }
    }
}

TEST_CASE("Bias and Rectification rapid route edits keep the latest audible anchor and reset snaps fresh",
          "[band][shape][bias][rectification][lfo][route][transition][reset]")
{
    const ShapeRouteRecipe unrouted {};
    const ShapeRouteRecipe firstRoute { 0, 0.62f, true, { 0.95f, 0.15f } };
    const ShapeRouteRecipe latestRoute { 1, 0.74f, false, { 0.95f, 0.30f } };
    const ShapeRouteRecipe resetInFlightRoute { 0, 0.41f, false, { 0.25f, 0.85f } };

    for (const auto target : { ShapeTarget::bias, ShapeTarget::rectification })
    {
        BandProcessor band;
        band.prepare({ sampleRate, 512, 1 });
        primeBridgeFixture(band);
        settleBridgeRecipe(band, target, unrouted);

        const float oldValue = routeTarget(target, unrouted);
        const float firstTarget = routeTarget(target, firstRoute);
        const auto firstPartial = processBridgeBlock(band, target, firstRoute, 160);
        REQUIRE(firstPartial.size() == 160);

        // The 160th consumed sample used mix=159/480. A rapid third request
        // must anchor to that actual value, instead of restarting from either
        // the original static base or the abandoned first target.
        const float heldAnchor = oldValue
                                 + (159.0f / 480.0f) * (firstTarget - oldValue);
        const float latestTarget = routeTarget(target, latestRoute);
        const auto latest = processBridgeBlock(band, target, latestRoute, 481);
        checkBridgeEvent(latest, target, heldAnchor, latestTarget);

        // Start another bridge and reset it while it is still in flight. Reset
        // is a discontinuity by contract: the first post-reset process call
        // must initialise from its recipe, never revive that partial anchor.
        const auto resetPartial = processBridgeBlock(
            band, target, resetInFlightRoute, 160);
        REQUIRE(resetPartial.size() == 160);
        band.reset();
        const auto fresh = processBridgeBlock(band, target, latestRoute, 1);
        REQUIRE(fresh.size() == 1);
        const float expectedFresh = probeOutput(target, latestTarget);
        CAPTURE(target == ShapeTarget::bias ? "Bias" : "Rectification",
                heldAnchor,
                latestTarget,
                fresh.front(),
                expectedFresh);
        CHECK(fresh.front() == Catch::Approx(expectedFresh).margin(1.0e-5f));
    }
}
