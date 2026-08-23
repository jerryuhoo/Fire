#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int lfoCycleSamples = 480;
constexpr int routeRampSamples = 480;
constexpr int legacyRampSamples = 2400;
constexpr std::array<float, 2> constantLfo { 0.0f, 1.0f };

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

float recipeTarget(const MixRecipe& recipe)
{
    if (! recipe.routed)
        return recipe.base;

    const float lfo = constantLfo[static_cast<size_t>(recipe.sourceIndex)];
    const float mapped = recipe.bipolar ? 2.0f * lfo - 1.0f : lfo;
    const float effectiveDepth = recipe.bipolar ? 0.5f * recipe.depth
                                                 : recipe.depth;
    return juce::jlimit(0.0f,
                        1.0f,
                        recipe.base + mapped * effectiveDepth);
}

BandProcessingParameters makeWidthMixParameters(bool useHq,
                                                 const MixRecipe& recipe,
                                                 bool stereoEnabled = true)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 8;
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isDriveEnabled = false;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = stereoEnabled;

    // Shape Mix zero makes the complete pre-stereo path transparent while
    // retaining the real Base/HQ topology and latency around Width Mix.
    params.shapeMixVal = 0.0f;
    params.shapeMixValProvider.baseValue = 0.0f;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };

    // A wide, panned wet path gives both channels strong and independent
    // separation from the dry endpoint.
    params.width = 0.94f;
    params.widthValProvider.baseValue = params.width;
    params.widthValProvider.range = { 0.0f, 1.0f };
    params.pan = 0.42f;
    params.panValProvider.baseValue = params.pan;
    params.panValProvider.range = { -1.0f, 1.0f };
    params.widthMixVal = recipe.base;
    params.widthMixValProvider.baseValue = recipe.base;
    params.widthMixValProvider.modulationDepth = recipe.depth;
    params.widthMixValProvider.isBipolar = recipe.bipolar;
    params.widthMixValProvider.range = { 0.0f, 1.0f };
    params.widthMixLfoSourceIndex = recipe.routed ? recipe.sourceIndex : -1;
    return params;
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
        {
            audio.setSample(channel, sample, inputSample(channel, position));
            lfo.setSample(channel,
                          sample,
                          fixture == LfoFixture::triangle
                              ? triangleLfo(position)
                              : constantLfo[static_cast<size_t>(channel)]);
        }
    }
}

float sampleProjectionError(const juce::AudioBuffer<float>& subject,
                            const juce::AudioBuffer<float>& alwaysDry,
                            const juce::AudioBuffer<float>& alwaysWet,
                            int sample,
                            float mix)
{
    float error = 0.0f;
    for (int channel = 0; channel < subject.getNumChannels(); ++channel)
    {
        const float dry = alwaysDry.getSample(channel, sample);
        const float wet = alwaysWet.getSample(channel, sample);
        const float expected = dry + mix * (wet - dry);
        error = std::max(error,
                         std::abs(subject.getSample(channel, sample)
                                  - expected));
    }
    return error;
}

struct ProcessorSet
{
    BandProcessor subject;
    BandProcessor alwaysDry;
    BandProcessor alwaysWet;

    ProcessorSet(bool useHq, int preparedBlockSize)
        : subject(), alwaysDry(), alwaysWet()
    {
        const juce::dsp::ProcessSpec spec {
            sampleRate,
            static_cast<juce::uint32>(preparedBlockSize),
            2
        };
        subject.prepare(spec);
        alwaysDry.prepare(spec);
        alwaysWet.prepare(spec);
        static_cast<void>(useHq);
    }
};

void processSet(ProcessorSet& processors,
                const BandProcessingParameters& subjectParams,
                bool useHq,
                juce::AudioBuffer<float>& subject,
                juce::AudioBuffer<float>& dry,
                juce::AudioBuffer<float>& wet,
                const juce::AudioBuffer<float>& lfo)
{
    const auto dryParams = makeWidthMixParameters(
        useHq, MixRecipe { false, -1, 0.0f, 0.0f, true }, true);
    const auto wetParams = makeWidthMixParameters(
        useHq, MixRecipe { false, -1, 1.0f, 0.0f, true }, true);
    processors.subject.process(subject, subjectParams, lfo);
    processors.alwaysDry.process(dry, dryParams, lfo);
    processors.alwaysWet.process(wet, wetParams, lfo);
}

void warmSet(ProcessorSet& processors,
             const BandProcessingParameters& subjectParams,
             bool useHq,
             int& absoluteSample,
             LfoFixture fixture)
{
    constexpr int warmBlockSize = 256;
    constexpr int warmSamples = 4096;
    for (int processed = 0; processed < warmSamples; processed += warmBlockSize)
    {
        juce::AudioBuffer<float> subject(2, warmBlockSize);
        juce::AudioBuffer<float> lfo(2, warmBlockSize);
        fillBlock(subject, lfo, absoluteSample, fixture);
        auto dry = subject;
        auto wet = subject;
        processSet(processors,
                   subjectParams,
                   useHq,
                   subject,
                   dry,
                   wet,
                   lfo);
        absoluteSample += warmBlockSize;
    }
}

struct StableRender
{
    std::array<std::vector<float>, 2> output;
    float analyticError = 0.0f;
    float endpointSeparation = 0.0f;
    bool finite = true;
};

StableRender renderStableTrajectory(bool useHq, int preparedBlockSize)
{
    constexpr int warmupSamples = 12 * lfoCycleSamples;
    constexpr int captureSamples = 8 * lfoCycleSamples;
    ProcessorSet processors(useHq, preparedBlockSize);
    const auto routedParams = makeWidthMixParameters(
        useHq, MixRecipe { true, 0, 0.5f, 1.0f, true }, true);
    StableRender result;
    for (auto& channel : result.output)
        channel.reserve(captureSamples);

    const int totalSamples = warmupSamples + captureSamples;
    for (int position = 0; position < totalSamples;
         position += lfoCycleSamples)
    {
        juce::AudioBuffer<float> subject(2, lfoCycleSamples);
        juce::AudioBuffer<float> lfo(2, lfoCycleSamples);
        fillBlock(subject, lfo, position, LfoFixture::triangle);
        auto dry = subject;
        auto wet = subject;
        processSet(processors,
                   routedParams,
                   useHq,
                   subject,
                   dry,
                   wet,
                   lfo);

        for (int sample = 0; sample < lfoCycleSamples; ++sample)
        {
            if (position + sample < warmupSamples)
                continue;
            const float mix = triangleLfo(position + sample);
            result.analyticError = std::max(
                result.analyticError,
                sampleProjectionError(subject, dry, wet, sample, mix));
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subject.getSample(channel, sample);
                const float drySample = dry.getSample(channel, sample);
                const float wetSample = wet.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(actual)
                                && std::isfinite(drySample)
                                && std::isfinite(wetSample);
                result.endpointSeparation = std::max(
                    result.endpointSeparation,
                    std::abs(wetSample - drySample));
                result.output[static_cast<size_t>(channel)].push_back(actual);
            }
        }
    }
    return result;
}

float maximumOutputDifference(const StableRender& lhs,
                              const StableRender& rhs)
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

void checkRecipeBridge(bool useHq,
                       const char* change,
                       const MixRecipe& oldRecipe,
                       const MixRecipe& newRecipe)
{
    ProcessorSet processors(useHq, 512);
    const auto oldParams = makeWidthMixParameters(useHq, oldRecipe, true);
    const auto newParams = makeWidthMixParameters(useHq, newRecipe, true);
    int absoluteSample = 0;
    warmSet(processors,
            oldParams,
            useHq,
            absoluteSample,
            LfoFixture::constants);

    juce::AudioBuffer<float> subject(2, routeRampSamples + 1);
    juce::AudioBuffer<float> lfo(2, routeRampSamples + 1);
    fillBlock(subject, lfo, absoluteSample, LfoFixture::constants);
    auto dry = subject;
    auto wet = subject;
    processSet(processors,
               newParams,
               useHq,
               subject,
               dry,
               wet,
               lfo);

    const float oldMix = recipeTarget(oldRecipe);
    const float newMix = recipeTarget(newRecipe);
    const auto expectedMix = [&](int sample)
    {
        const float progress = juce::jlimit(
            0.0f,
            1.0f,
            static_cast<float>(sample)
                / static_cast<float>(routeRampSamples));
        return oldMix + progress * (newMix - oldMix);
    };
    const float firstError = sampleProjectionError(subject,
                                                   dry,
                                                   wet,
                                                   0,
                                                   expectedMix(0));
    const float midpointError = sampleProjectionError(subject,
                                                      dry,
                                                      wet,
                                                      240,
                                                      expectedMix(240));
    const float endpointError = sampleProjectionError(subject,
                                                      dry,
                                                      wet,
                                                      480,
                                                      expectedMix(480));
    CAPTURE(useHq,
            change,
            oldMix,
            newMix,
            firstError,
            midpointError,
            endpointError);
    CHECK(firstError < 2.0e-4f);
    CHECK(midpointError < 2.0e-4f);
    CHECK(endpointError < 2.0e-4f);
}

void checkEnableRamp(bool useHq)
{
    const MixRecipe route { true, 1, 0.5f, 0.6f, true };
    ProcessorSet processors(useHq, 512);
    const auto enabledParams = makeWidthMixParameters(useHq, route, true);
    const auto disabledParams = makeWidthMixParameters(useHq, route, false);
    int absoluteSample = 0;
    warmSet(processors,
            enabledParams,
            useHq,
            absoluteSample,
            LfoFixture::constants);

    const auto processTransition = [&](const BandProcessingParameters& params)
    {
        juce::AudioBuffer<float> subject(2, legacyRampSamples);
        juce::AudioBuffer<float> lfo(2, legacyRampSamples);
        fillBlock(subject, lfo, absoluteSample, LfoFixture::constants);
        auto dry = subject;
        auto wet = subject;
        processSet(processors,
                   params,
                   useHq,
                   subject,
                   dry,
                   wet,
                   lfo);
        absoluteSample += legacyRampSamples;
        return std::array<juce::AudioBuffer<float>, 3> {
            std::move(subject), std::move(dry), std::move(wet)
        };
    };

    const float routedMix = recipeTarget(route);
    auto disable = processTransition(disabledParams);
    auto enable = processTransition(enabledParams);
    const auto rampError = [&](const std::array<juce::AudioBuffer<float>, 3>& data,
                               int sample,
                               float mix)
    {
        return sampleProjectionError(data[0], data[1], data[2], sample, mix);
    };
    const float disableFirst = routedMix
                               * (1.0f - 1.0f / legacyRampSamples);
    const float enableFirst = routedMix / legacyRampSamples;
    const float disableFirstError = rampError(disable, 0, disableFirst);
    const float disableMidError = rampError(disable, 1199, routedMix * 0.5f);
    const float disableEndError = rampError(disable, 2399, 0.0f);
    const float enableFirstError = rampError(enable, 0, enableFirst);
    const float enableMidError = rampError(enable, 1199, routedMix * 0.5f);
    const float enableEndError = rampError(enable, 2399, routedMix);
    CAPTURE(useHq,
            routedMix,
            disableFirstError,
            disableMidError,
            disableEndError,
            enableFirstError,
            enableMidError,
            enableEndError);
    CHECK(disableFirstError < 2.0e-4f);
    CHECK(disableMidError < 2.0e-4f);
    CHECK(disableEndError < 2.0e-4f);
    CHECK(enableFirstError < 2.0e-4f);
    CHECK(enableMidError < 2.0e-4f);
    CHECK(enableEndError < 2.0e-4f);
}
} // namespace

TEST_CASE("Width Mix follows the analytic LFO trajectory in Base and HQ",
          "[processor][band][stereo][width-mix][lfo][trajectory][internal-chunk]")
{
    for (const bool useHq : std::array { false, true })
    {
        const auto whole = renderStableTrajectory(useHq, lfoCycleSamples);
        const auto chunked = renderStableTrajectory(useHq, 64);
        const float chunkError = maximumOutputDifference(whole, chunked);
        CAPTURE(useHq,
                whole.analyticError,
                chunked.analyticError,
                whole.endpointSeparation,
                chunked.endpointSeparation,
                chunkError);
        REQUIRE(whole.finite);
        REQUIRE(chunked.finite);
        REQUIRE(whole.endpointSeparation > 0.05f);
        REQUIRE(chunked.endpointSeparation > 0.05f);
        CHECK(whole.analyticError < 2.0e-4f);
        CHECK(chunked.analyticError < 2.0e-4f);
        CHECK(chunkError < 2.0e-4f);
    }
}

TEST_CASE("Width Mix bridges routing recipe changes in ten milliseconds",
          "[processor][band][stereo][width-mix][lfo][recipe-transition]")
{
    for (const bool useHq : std::array { false, true })
    {
        checkRecipeBridge(useHq,
                          "attach",
                          MixRecipe { false, -1, 0.5f, 0.0f, true },
                          MixRecipe { true, 1, 0.5f, 0.6f, true });
        checkRecipeBridge(useHq,
                          "source",
                          MixRecipe { true, 0, 0.5f, 0.6f, true },
                          MixRecipe { true, 1, 0.5f, 0.6f, true });
        checkRecipeBridge(useHq,
                          "depth",
                          MixRecipe { true, 1, 0.5f, 0.2f, true },
                          MixRecipe { true, 1, 0.5f, 0.8f, true });
        checkRecipeBridge(useHq,
                          "polarity",
                          MixRecipe { true, 0, 0.5f, 0.6f, true },
                          MixRecipe { true, 0, 0.5f, 0.6f, false });
    }
}

TEST_CASE("Width Mix enable changes retain the fifty millisecond ramp",
          "[processor][band][stereo][width-mix][lfo][enable][compatibility]")
{
    checkEnableRamp(false);
    checkEnableRamp(true);
}
