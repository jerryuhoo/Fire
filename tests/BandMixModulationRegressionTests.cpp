#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int lfoCycleSamples = 480;
constexpr int routeRampSamples = 480;
constexpr int warmupSamples = 12 * lfoCycleSamples;
constexpr int captureSamples = 8 * lfoCycleSamples;

struct MixRecipe
{
    bool routed = false;
    int sourceIndex = -1;
    float base = 0.5f;
    float depth = 0.0f;
    bool bipolar = true;
};

float triangleLfo(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoCycleSamples)
                        / static_cast<float>(lfoCycleSamples);
    return phase < 0.5f ? 2.0f * phase : 2.0f * (1.0f - phase);
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    if (channel == 0)
    {
        return 0.31f * static_cast<float>(std::sin(
                   juce::MathConstants<double>::twoPi * 431.0 * time + 0.17))
             + 0.09f * static_cast<float>(std::cos(
                   juce::MathConstants<double>::twoPi * 1777.0 * time));
    }

    return 0.17f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 719.0 * time + 0.63))
         - 0.12f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 1231.0 * time + 0.31));
}

BandProcessingParameters makeParameters(bool useHq, const MixRecipe& recipe)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 4; // Hard clip is transparent for this fixture's input range.
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isDriveEnabled = false;
    params.isShapeEnabled = false;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;

    params.shapeMixVal = 1.0f;
    params.shapeMixValProvider.baseValue = 1.0f;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    params.widthMixVal = 1.0f;
    params.widthMixValProvider.baseValue = 1.0f;

    // Band Mix=0 is the latency-aligned input.  Band Mix=1 contains the
    // otherwise-transparent signal attenuated by 18 dB, giving a strong and
    // linear endpoint separation without relying on nonlinear DSP state.
    params.outputVal.baseValue = -18.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.outputLfoSourceIndex = -1;

    params.mixVal = recipe.base;
    params.mixValProvider.baseValue = recipe.base;
    params.mixValProvider.modulationDepth = recipe.depth;
    params.mixValProvider.isBipolar = recipe.bipolar;
    params.mixValProvider.range = { 0.0f, 1.0f };
    params.mixLfoSourceIndex = recipe.routed ? recipe.sourceIndex : -1;
    return params;
}

enum class LfoFixture
{
    triangle,
    constants
};

void fillBlock(juce::AudioBuffer<float>& audio,
               juce::AudioBuffer<float>& lfo,
               int absoluteSample,
               LfoFixture fixture)
{
    REQUIRE(audio.getNumChannels() == 2);
    REQUIRE(lfo.getNumChannels() == 2);
    REQUIRE(audio.getNumSamples() == lfo.getNumSamples());

    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const int position = absoluteSample + sample;
        for (int channel = 0; channel < 2; ++channel)
            audio.setSample(channel, sample, inputSample(channel, position));

        if (fixture == LfoFixture::triangle)
        {
            const float value = triangleLfo(position);
            lfo.setSample(0, sample, value);
            lfo.setSample(1, sample, value);
        }
        else
        {
            lfo.setSample(0, sample, 0.0f);
            lfo.setSample(1, sample, 1.0f);
        }
    }
}

float projectionError(const juce::AudioBuffer<float>& subject,
                      const juce::AudioBuffer<float>& dry,
                      const juce::AudioBuffer<float>& wet,
                      int sample,
                      float mix)
{
    float error = 0.0f;
    for (int channel = 0; channel < subject.getNumChannels(); ++channel)
    {
        const float expected = dry.getSample(channel, sample)
                             + mix * (wet.getSample(channel, sample)
                                      - dry.getSample(channel, sample));
        error = std::max(error,
                         std::abs(subject.getSample(channel, sample)
                                  - expected));
    }
    return error;
}

float maximumDifference(const juce::AudioBuffer<float>& lhs,
                        const juce::AudioBuffer<float>& rhs)
{
    REQUIRE(lhs.getNumChannels() == rhs.getNumChannels());
    REQUIRE(lhs.getNumSamples() == rhs.getNumSamples());
    float error = 0.0f;
    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
        for (int sample = 0; sample < lhs.getNumSamples(); ++sample)
            error = std::max(error,
                             std::abs(lhs.getSample(channel, sample)
                                      - rhs.getSample(channel, sample)));
    return error;
}

struct ProcessorSet
{
    BandProcessor subject;
    BandProcessor alwaysDry;
    BandProcessor alwaysWet;
    juce::dsp::DelayLine<float,
                         juce::dsp::DelayLineInterpolationTypes::Thiran>
        independentDryDelay { 2048 };

    ProcessorSet(bool useHq, int preparedBlockSize)
    {
        const juce::dsp::ProcessSpec spec {
            sampleRate,
            static_cast<juce::uint32>(preparedBlockSize),
            2
        };
        subject.prepare(spec);
        alwaysDry.prepare(spec);
        alwaysWet.prepare(spec);
        independentDryDelay.prepare(spec);
        REQUIRE(alwaysDry.oversampling != nullptr);
        independentDryDelay.setDelay(
            useHq ? alwaysDry.oversampling->getLatencyInSamples() : 0.0f);
    }
};

struct ProcessedBlock
{
    juce::AudioBuffer<float> subject;
    juce::AudioBuffer<float> dry;
    juce::AudioBuffer<float> wet;
    juce::AudioBuffer<float> independentDry;
};

ProcessedBlock processSet(ProcessorSet& processors,
                          const BandProcessingParameters& subjectParams,
                          bool useHq,
                          const juce::AudioBuffer<float>& input,
                          const juce::AudioBuffer<float>& lfo)
{
    ProcessedBlock result { input, input, input, input };
    const auto dryParams = makeParameters(
        useHq, MixRecipe { false, -1, 0.0f, 0.0f, true });
    const auto wetParams = makeParameters(
        useHq, MixRecipe { false, -1, 1.0f, 0.0f, true });

    auto canonicalBlock = juce::dsp::AudioBlock<float>(result.independentDry);
    processors.independentDryDelay.process(
        juce::dsp::ProcessContextReplacing<float>(canonicalBlock));
    processors.subject.process(result.subject, subjectParams, lfo);
    processors.alwaysDry.process(result.dry, dryParams, lfo);
    processors.alwaysWet.process(result.wet, wetParams, lfo);
    return result;
}

struct StableRender
{
    std::array<std::vector<float>, 2> output;
    float analyticError = 0.0f;
    float dryOracleError = 0.0f;
    float endpointSeparation = 0.0f;
    bool finite = true;
};

StableRender renderStable(bool useHq,
                          int preparedBlockSize,
                          const std::vector<int>& hostPattern)
{
    REQUIRE_FALSE(hostPattern.empty());
    for (const int blockSize : hostPattern)
        REQUIRE(blockSize > 0);

    ProcessorSet processors(useHq, preparedBlockSize);
    const auto params = makeParameters(
        useHq, MixRecipe { true, 0, 0.5f, 1.0f, true });
    StableRender result;
    for (auto& channel : result.output)
        channel.reserve(captureSamples);

    const int totalSamples = warmupSamples + captureSamples;
    int absoluteSample = 0;
    size_t patternIndex = 0;
    while (absoluteSample < totalSamples)
    {
        const int requested = hostPattern[patternIndex % hostPattern.size()];
        ++patternIndex;
        const int blockSize = std::min(requested,
                                       totalSamples - absoluteSample);
        juce::AudioBuffer<float> input(2, blockSize);
        juce::AudioBuffer<float> lfo(2, blockSize);
        fillBlock(input, lfo, absoluteSample, LfoFixture::triangle);
        const auto block = processSet(processors,
                                      params,
                                      useHq,
                                      input,
                                      lfo);

        result.dryOracleError = std::max(
            result.dryOracleError,
            maximumDifference(block.dry, block.independentDry));
        for (int sample = 0; sample < blockSize; ++sample)
        {
            if (absoluteSample + sample < warmupSamples)
                continue;

            const float mix = triangleLfo(absoluteSample + sample);
            result.analyticError = std::max(
                result.analyticError,
                projectionError(block.subject,
                                block.dry,
                                block.wet,
                                sample,
                                mix));
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = block.subject.getSample(channel, sample);
                const float dry = block.dry.getSample(channel, sample);
                const float wet = block.wet.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(actual)
                                && std::isfinite(dry)
                                && std::isfinite(wet);
                result.endpointSeparation = std::max(
                    result.endpointSeparation, std::abs(wet - dry));
                result.output[static_cast<size_t>(channel)].push_back(actual);
            }
        }
        absoluteSample += blockSize;
    }
    return result;
}

float renderDifference(const StableRender& lhs, const StableRender& rhs)
{
    float error = 0.0f;
    for (size_t channel = 0; channel < lhs.output.size(); ++channel)
    {
        REQUIRE(lhs.output[channel].size() == rhs.output[channel].size());
        for (size_t sample = 0; sample < lhs.output[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(lhs.output[channel][sample]
                                      - rhs.output[channel][sample]));
    }
    return error;
}

void checkSourceBridge(bool useHq)
{
    ProcessorSet processors(useHq, 512);
    const auto oldParams = makeParameters(
        useHq, MixRecipe { true, 0, 0.5f, 0.6f, true });
    const auto newParams = makeParameters(
        useHq, MixRecipe { true, 1, 0.5f, 0.6f, true });

    int absoluteSample = 0;
    constexpr int warmBlockSize = 256;
    for (int processed = 0; processed < 4096; processed += warmBlockSize)
    {
        juce::AudioBuffer<float> input(2, warmBlockSize);
        juce::AudioBuffer<float> lfo(2, warmBlockSize);
        fillBlock(input, lfo, absoluteSample, LfoFixture::constants);
        static_cast<void>(processSet(processors,
                                     oldParams,
                                     useHq,
                                     input,
                                     lfo));
        absoluteSample += warmBlockSize;
    }

    juce::AudioBuffer<float> input(2, routeRampSamples + 1);
    juce::AudioBuffer<float> lfo(2, routeRampSamples + 1);
    fillBlock(input, lfo, absoluteSample, LfoFixture::constants);
    const auto block = processSet(processors,
                                  newParams,
                                  useHq,
                                  input,
                                  lfo);

    const float firstError = projectionError(
        block.subject, block.dry, block.wet, 0, 0.2f);
    const float midpointError = projectionError(
        block.subject, block.dry, block.wet, 240, 0.5f);
    const float endpointError = projectionError(
        block.subject, block.dry, block.wet, 480, 0.8f);
    float endpointSeparation = 0.0f;
    bool finite = true;
    for (int sample = 0; sample < block.subject.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const float actual = block.subject.getSample(channel, sample);
            finite = finite && std::isfinite(actual);
            endpointSeparation = std::max(
                endpointSeparation,
                std::abs(block.wet.getSample(channel, sample)
                         - block.dry.getSample(channel, sample)));
        }
    }
    const float dryOracleError = maximumDifference(block.dry,
                                                    block.independentDry);
    CAPTURE(useHq,
            firstError,
            midpointError,
            endpointError,
            endpointSeparation,
            dryOracleError);
    REQUIRE(finite);
    REQUIRE(endpointSeparation > 0.05f);
    CHECK(firstError < 2.0e-4f);
    CHECK(midpointError < 2.0e-4f);
    CHECK(endpointError < 2.0e-4f);
    CHECK(dryOracleError < 2.0e-6f);
}
} // namespace

TEST_CASE("Band Mix follows an analytic LFO across host and internal chunks",
          "[processor][band][band-mix][lfo][trajectory][internal-chunk]")
{
    const std::vector<int> fixed { lfoCycleSamples };
    const std::vector<int> irregular { 17, 31, 43, 29, 53, 37 };
    for (const bool useHq : std::array { false, true })
    {
        const auto whole = renderStable(useHq, lfoCycleSamples, fixed);
        const auto irregularBlocks = renderStable(useHq, 64, irregular);
        const auto internalChunks = renderStable(useHq, 64, fixed);
        const float irregularError = renderDifference(whole,
                                                      irregularBlocks);
        const float internalChunkError = renderDifference(whole,
                                                          internalChunks);
        CAPTURE(useHq,
                whole.analyticError,
                irregularBlocks.analyticError,
                internalChunks.analyticError,
                whole.dryOracleError,
                irregularBlocks.dryOracleError,
                internalChunks.dryOracleError,
                whole.endpointSeparation,
                irregularError,
                internalChunkError);
        REQUIRE(whole.finite);
        REQUIRE(irregularBlocks.finite);
        REQUIRE(internalChunks.finite);
        REQUIRE(whole.endpointSeparation > 0.05f);
        CHECK(whole.analyticError < 2.0e-4f);
        CHECK(irregularBlocks.analyticError < 2.0e-4f);
        CHECK(internalChunks.analyticError < 2.0e-4f);
        CHECK(whole.dryOracleError < 2.0e-6f);
        CHECK(irregularBlocks.dryOracleError < 2.0e-6f);
        CHECK(internalChunks.dryOracleError < 2.0e-6f);
        CHECK(irregularError < 2.0e-4f);
        CHECK(internalChunkError < 2.0e-4f);
    }
}

TEST_CASE("Band Mix HQ dry endpoint is an independent fractional Thiran delay",
          "[processor][band][band-mix][hq][dry][latency][thiran]")
{
    const auto hq = renderStable(true, 64, { lfoCycleSamples });
    CAPTURE(hq.dryOracleError, hq.endpointSeparation);
    REQUIRE(hq.finite);
    REQUIRE(hq.endpointSeparation > 0.05f);
    CHECK(hq.dryOracleError < 2.0e-6f);
}

TEST_CASE("Band Mix bridges LFO source changes in ten milliseconds",
          "[processor][band][band-mix][lfo][recipe-transition]")
{
    checkSourceBridge(false);
    checkSourceBridge(true);
}
