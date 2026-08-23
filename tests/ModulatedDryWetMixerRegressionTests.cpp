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
constexpr int lfoCycleSamples = 480;
constexpr int routeRampSamples = 480;
constexpr int legacyRampSamples = 2400;

enum class LfoShape
{
    constant,
    triangle
};

struct MixRecipe
{
    bool routed = false;
    int sourceIndex = -1;
    float base = 0.5f;
    float depth = 0.0f;
    bool bipolar = true;
    float constantLfo = 0.5f;
};

float triangleLfo(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoCycleSamples)
                        / static_cast<float>(lfoCycleSamples);
    return phase < 0.5f ? 2.0f * phase : 2.0f * (1.0f - phase);
}

float expectedRoutedMix(const MixRecipe& recipe, float lfoValue)
{
    const float clampedLfo = juce::jlimit(0.0f, 1.0f, lfoValue);
    const float mapped = recipe.bipolar ? 2.0f * clampedLfo - 1.0f
                                        : clampedLfo;
    const float effectiveDepth = recipe.bipolar ? 0.5f * recipe.depth
                                                 : recipe.depth;
    return juce::jlimit(0.0f,
                        1.0f,
                        recipe.base + mapped * effectiveDepth);
}

std::vector<float> processMixerBlock(ZeroLatencyModulatedDryWetMixer& mixer,
                                     int numSamples,
                                     const MixRecipe& recipe,
                                     float legacyBaseMix,
                                     bool enabled,
                                     int absoluteSample = 0,
                                     LfoShape shape = LfoShape::constant)
{
    REQUIRE(numSamples > 0);

    std::vector<float> lfo(static_cast<size_t>(numSamples));
    for (int sample = 0; sample < numSamples; ++sample)
    {
        lfo[static_cast<size_t>(sample)] =
            shape == LfoShape::triangle
                ? triangleLfo(absoluteSample + sample)
                : recipe.constantLfo;
    }

    ModulatedValueProvider provider;
    provider.lfoSignal = recipe.routed ? lfo.data() : nullptr;
    provider.baseValue = recipe.base;
    provider.modulationDepth = recipe.depth;
    provider.isBipolar = recipe.bipolar;
    provider.range = { 0.0f, 1.0f };

    // dry=0 and wet=1 turn every output sample into the mix coefficient,
    // giving this state-machine test an independent analytic oracle.
    juce::AudioBuffer<float> dry(1, numSamples);
    juce::AudioBuffer<float> wet(1, numSamples);
    dry.clear();
    juce::FloatVectorOperations::fill(wet.getWritePointer(0),
                                      1.0f,
                                      numSamples);
    auto dryBlock = juce::dsp::AudioBlock<float>(dry);
    auto wetBlock = juce::dsp::AudioBlock<float>(wet);
    mixer.pushDrySamples(dryBlock);
    mixer.mixWetSamples(wetBlock,
                        provider,
                        legacyBaseMix,
                        recipe.sourceIndex,
                        enabled);

    std::vector<float> output(static_cast<size_t>(numSamples));
    std::copy_n(wet.getReadPointer(0), numSamples, output.begin());
    return output;
}

bool allFinite(const std::vector<float>& values)
{
    return std::all_of(values.begin(), values.end(), [](float value)
    {
        return std::isfinite(value);
    });
}

float maximumDifference(const std::vector<float>& lhs,
                        const std::vector<float>& rhs)
{
    REQUIRE(lhs.size() == rhs.size());
    float maximumError = 0.0f;
    for (size_t sample = 0; sample < lhs.size(); ++sample)
        maximumError = std::max(maximumError,
                                std::abs(lhs[sample] - rhs[sample]));
    return maximumError;
}

std::vector<float> renderStableRoute(const std::vector<int>& pattern)
{
    REQUIRE_FALSE(pattern.empty());
    ZeroLatencyModulatedDryWetMixer mixer;
    mixer.prepare({ sampleRate,
                    static_cast<juce::uint32>(*std::max_element(pattern.begin(),
                                                                 pattern.end())),
                    1 });

    const MixRecipe recipe { true, 0, 0.5f, 1.0f, true, 0.0f };
    constexpr int totalSamples = 8 * lfoCycleSamples;
    std::vector<float> output;
    output.reserve(totalSamples);
    int position = 0;
    size_t blockIndex = 0;
    while (position < totalSamples)
    {
        const int blockSize = std::min(pattern[blockIndex % pattern.size()],
                                       totalSamples - position);
        ++blockIndex;
        auto block = processMixerBlock(mixer,
                                       blockSize,
                                       recipe,
                                       0.5f,
                                       true,
                                       position,
                                       LfoShape::triangle);
        output.insert(output.end(), block.begin(), block.end());
        position += blockSize;
    }
    return output;
}

void checkRecipeBridge(const char* label,
                       const MixRecipe& oldRecipe,
                       float oldLegacyMix,
                       const MixRecipe& newRecipe)
{
    ZeroLatencyModulatedDryWetMixer mixer;
    mixer.prepare({ sampleRate, 512, 1 });
    const auto prime = processMixerBlock(mixer,
                                         64,
                                         oldRecipe,
                                         oldLegacyMix,
                                         true);
    const auto transition = processMixerBlock(mixer,
                                              routeRampSamples + 1,
                                              newRecipe,
                                              newRecipe.base,
                                              true);

    const float oldMix = oldRecipe.routed
                             ? expectedRoutedMix(oldRecipe,
                                                oldRecipe.constantLfo)
                             : oldLegacyMix;
    const float newMix = expectedRoutedMix(newRecipe,
                                           newRecipe.constantLfo);
    const auto expectedAt = [&](int sample)
    {
        const float progress = juce::jlimit(
            0.0f,
            1.0f,
            static_cast<float>(sample)
                / static_cast<float>(routeRampSamples));
        return oldMix + progress * (newMix - oldMix);
    };

    CAPTURE(label, oldMix, newMix);
    REQUIRE(allFinite(prime));
    REQUIRE(allFinite(transition));
    CHECK(prime.back() == Catch::Approx(oldMix).margin(1.0e-6f));
    CHECK(transition[0] == Catch::Approx(oldMix).margin(1.0e-6f));
    CHECK(transition[240]
          == Catch::Approx(expectedAt(240)).margin(2.0e-5f));
    CHECK(transition[479]
          == Catch::Approx(expectedAt(479)).margin(2.0e-5f));
    CHECK(transition[480] == Catch::Approx(newMix).margin(1.0e-6f));
}

std::vector<float> renderPartitionedBridge(const std::vector<int>& pattern)
{
    REQUIRE_FALSE(pattern.empty());
    ZeroLatencyModulatedDryWetMixer mixer;
    mixer.prepare({ sampleRate,
                    static_cast<juce::uint32>(*std::max_element(pattern.begin(),
                                                                 pattern.end())),
                    1 });
    const MixRecipe oldRecipe { true, 0, 0.5f, 0.6f, true, 0.0f };
    const MixRecipe newRecipe { true, 1, 0.5f, 0.6f, true, 1.0f };
    processMixerBlock(mixer, 32, oldRecipe, 0.5f, true);

    constexpr int totalSamples = 2 * routeRampSamples;
    std::vector<float> output;
    output.reserve(totalSamples);
    int position = 0;
    size_t blockIndex = 0;
    while (position < totalSamples)
    {
        const int blockSize = std::min(pattern[blockIndex % pattern.size()],
                                       totalSamples - position);
        ++blockIndex;
        auto block = processMixerBlock(mixer,
                                       blockSize,
                                       newRecipe,
                                       0.5f,
                                       true);
        output.insert(output.end(), block.begin(), block.end());
        position += blockSize;
    }
    return output;
}

BandProcessingParameters makeCompressorMixParameters(bool useHq,
                                                      float staticMix,
                                                      bool routed)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 4; // Hard clip is identity for the input fixture.
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isCompEnabled = true;
    params.isDriveEnabled = false;
    params.isShapeEnabled = false;
    params.isWidthEnabled = false;
    params.isDcFilterEnabled = false;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.shapeMixVal = 1.0f;
    params.shapeMixValProvider.baseValue = 1.0f;
    params.compThreshold = -32.0f;
    params.compRatio = 20.0f;
    params.compAttack = 0.1f;
    params.compRelease = 10.0f;
    params.compMixVal = staticMix;
    params.compMixValProvider.baseValue = routed ? 0.5f : staticMix;
    params.compMixValProvider.range = { 0.0f, 1.0f };
    params.compMixValProvider.modulationDepth = routed ? 1.0f : 0.0f;
    params.compMixLfoSourceIndex = routed ? 0 : -1;
    params.widthMixVal = 1.0f;
    params.widthMixValProvider.baseValue = 1.0f;
    return params;
}

float compressorInput(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    return 0.76f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 997.0 * time
               + 0.41 * channel))
         + 0.11f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 2711.0 * time
               + 0.23 - 0.17 * channel));
}

struct BandMixRender
{
    std::array<std::vector<float>, 2> output;
    float analyticError = 0.0f;
    float endpointSeparation = 0.0f;
    bool finite = true;
};

BandMixRender renderBandCompressorMix(bool useHq, int preparedBlockSize)
{
    constexpr int warmupSamples = 12 * lfoCycleSamples;
    constexpr int captureSamples = 8 * lfoCycleSamples;
    constexpr int hostBlockSize = lfoCycleSamples;
    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32>(preparedBlockSize),
                                        2 };
    BandProcessor subject;
    BandProcessor alwaysDry;
    BandProcessor alwaysWet;
    subject.prepare(spec);
    alwaysDry.prepare(spec);
    alwaysWet.prepare(spec);
    const auto subjectParams = makeCompressorMixParameters(useHq, 0.5f, true);
    const auto dryParams = makeCompressorMixParameters(useHq, 0.0f, false);
    const auto wetParams = makeCompressorMixParameters(useHq, 1.0f, false);

    BandMixRender result;
    for (auto& channel : result.output)
        channel.reserve(captureSamples);

    const int totalSamples = warmupSamples + captureSamples;
    for (int position = 0; position < totalSamples; position += hostBlockSize)
    {
        const int samplesThisBlock = std::min(hostBlockSize,
                                              totalSamples - position);
        juce::AudioBuffer<float> subjectAudio(2, samplesThisBlock);
        juce::AudioBuffer<float> dryAudio(2, samplesThisBlock);
        juce::AudioBuffer<float> wetAudio(2, samplesThisBlock);
        juce::AudioBuffer<float> lfo(1, samplesThisBlock);
        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = position + sample;
            lfo.setSample(0, sample, triangleLfo(absoluteSample));
            for (int channel = 0; channel < 2; ++channel)
                subjectAudio.setSample(channel,
                                       sample,
                                       compressorInput(channel,
                                                       absoluteSample));
        }
        dryAudio.makeCopyOf(subjectAudio);
        wetAudio.makeCopyOf(subjectAudio);

        subject.process(subjectAudio, subjectParams, lfo);
        alwaysDry.process(dryAudio, dryParams, lfo);
        alwaysWet.process(wetAudio, wetParams, lfo);

        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            if (position + sample < warmupSamples)
                continue;
            const float mix = triangleLfo(position + sample);
            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectAudio.getSample(channel, sample);
                const float dry = dryAudio.getSample(channel, sample);
                const float wet = wetAudio.getSample(channel, sample);
                const float expected = dry + mix * (wet - dry);
                result.finite = result.finite && std::isfinite(actual)
                                && std::isfinite(expected);
                result.analyticError = std::max(result.analyticError,
                                                std::abs(actual - expected));
                result.endpointSeparation = std::max(
                    result.endpointSeparation,
                    std::abs(wet - dry));
                result.output[static_cast<size_t>(channel)].push_back(actual);
            }
        }
    }
    return result;
}
} // namespace

TEST_CASE("Zero-latency modulated mixer preserves the analytic LFO trajectory",
          "[dsp][mix][lfo][trajectory][zero-latency]")
{
    const auto fullCycles = renderStableRoute({ lfoCycleSamples });
    const auto irregular = renderStableRoute({ 17, 31, 43, 29, 53, 37 });
    REQUIRE(allFinite(fullCycles));
    REQUIRE(allFinite(irregular));
    REQUIRE(fullCycles.size() == irregular.size());

    float analyticError = 0.0f;
    for (size_t sample = 0; sample < fullCycles.size(); ++sample)
        analyticError = std::max(
            analyticError,
            std::abs(fullCycles[sample]
                     - triangleLfo(static_cast<int>(sample))));
    const float partitionError = maximumDifference(fullCycles, irregular);
    CAPTURE(analyticError, partitionError);
    CHECK(analyticError < 1.0e-6f);
    CHECK(partitionError < 1.0e-6f);
}

TEST_CASE("Zero-latency mixer retains the legacy fifty millisecond static ramp",
          "[dsp][mix][automation][compatibility][zero-latency]")
{
    const juce::dsp::ProcessSpec spec { sampleRate, legacyRampSamples, 1 };
    ZeroLatencyModulatedDryWetMixer subject;
    juce::dsp::DryWetMixer<float> reference { 0 };
    subject.prepare(spec);
    reference.prepare(spec);

    ModulatedValueProvider provider;
    provider.range = { 0.0f, 1.0f };
    const auto drySignal = [](int absoluteSample)
    {
        return 0.31f + 0.12f * std::sin(0.019f * absoluteSample);
    };
    const auto wetSignal = [](int absoluteSample)
    {
        return -0.27f + 0.15f * std::cos(0.031f * absoluteSample);
    };
    const auto fillSignals = [&](juce::AudioBuffer<float>& dry,
                                 juce::AudioBuffer<float>& wet,
                                 int absoluteSample)
    {
        for (int sample = 0; sample < dry.getNumSamples(); ++sample)
        {
            dry.setSample(0, sample, drySignal(absoluteSample + sample));
            wet.setSample(0, sample, wetSignal(absoluteSample + sample));
        }
    };
    const auto processSubject = [&](juce::AudioBuffer<float>& dry,
                                    juce::AudioBuffer<float>& wet,
                                    float mix)
    {
        auto dryBlock = juce::dsp::AudioBlock<float>(dry);
        auto wetBlock = juce::dsp::AudioBlock<float>(wet);
        subject.pushDrySamples(dryBlock);
        subject.mixWetSamples(wetBlock, provider, mix, -1, true);
    };
    const auto processReference = [&](juce::AudioBuffer<float>& dry,
                                      juce::AudioBuffer<float>& wet,
                                      float mix)
    {
        reference.setWetMixProportion(mix);
        auto dryBlock = juce::dsp::AudioBlock<float>(dry);
        auto wetBlock = juce::dsp::AudioBlock<float>(wet);
        reference.pushDrySamples(dryBlock);
        reference.mixWetSamples(wetBlock);
    };

    // Match the production first-callback priming contract: the custom mixer
    // snaps on first use, while the legacy JUCE mixer snapped by reset after
    // its initial proportion was supplied.
    reference.setWetMixProportion(0.2f);
    reference.reset();
    juce::AudioBuffer<float> subjectPrimeDry(1, 8);
    juce::AudioBuffer<float> subjectPrimeWet(1, 8);
    fillSignals(subjectPrimeDry, subjectPrimeWet, 0);
    auto referencePrimeDry = subjectPrimeDry;
    auto referencePrimeWet = subjectPrimeWet;
    processSubject(subjectPrimeDry, subjectPrimeWet, 0.2f);
    processReference(referencePrimeDry, referencePrimeWet, 0.2f);

    juce::AudioBuffer<float> subjectDry(1, legacyRampSamples);
    juce::AudioBuffer<float> subjectWet(1, legacyRampSamples);
    fillSignals(subjectDry, subjectWet, 8);
    auto referenceDry = subjectDry;
    auto referenceWet = subjectWet;
    processSubject(subjectDry, subjectWet, 0.8f);
    processReference(referenceDry, referenceWet, 0.8f);

    float maximumError = 0.0f;
    bool finite = true;
    for (int sample = 0; sample < legacyRampSamples; ++sample)
    {
        const float actual = subjectWet.getSample(0, sample);
        const float expected = referenceWet.getSample(0, sample);
        finite = finite && std::isfinite(actual) && std::isfinite(expected);
        maximumError = std::max(maximumError, std::abs(actual - expected));
    }

    // JUCE's two float SmoothedValues accumulate a few ulps independently.
    // The endpoint checks retain the audible advance-then-use contract while
    // the full-buffer comparison locks exact legacy lifecycle/rounding.
    const float firstMix = 0.2f + 0.6f / legacyRampSamples;
    const float expectedFirst = drySignal(8) * (1.0f - firstMix)
                                + wetSignal(8) * firstMix;
    const int lastAbsoluteSample = 8 + legacyRampSamples - 1;
    const float expectedLast = drySignal(lastAbsoluteSample) * 0.2f
                               + wetSignal(lastAbsoluteSample) * 0.8f;
    CAPTURE(maximumError,
            subjectWet.getSample(0, 0),
            subjectWet.getSample(0, legacyRampSamples - 1),
            expectedFirst,
            expectedLast);
    REQUIRE(finite);
    CHECK(maximumError < 2.0e-6f);
    CHECK(subjectWet.getSample(0, 0)
          == Catch::Approx(expectedFirst).margin(2.0e-5f));
    CHECK(subjectWet.getSample(0, legacyRampSamples - 1)
          == Catch::Approx(expectedLast).margin(2.0e-6f));
}

TEST_CASE("Zero-latency mixer bridges discrete routing recipes in ten milliseconds",
          "[dsp][mix][lfo][recipe-transition][zero-latency]")
{
    checkRecipeBridge("attach",
                      MixRecipe { false, -1, 0.5f, 0.0f, true, 0.5f },
                      0.5f,
                      MixRecipe { true, 0, 0.5f, 0.5f, true, 1.0f });
    checkRecipeBridge("source",
                      MixRecipe { true, 0, 0.5f, 0.5f, true, 0.0f },
                      0.5f,
                      MixRecipe { true, 1, 0.5f, 0.5f, true, 1.0f });
    checkRecipeBridge("depth",
                      MixRecipe { true, 0, 0.5f, 0.2f, true, 1.0f },
                      0.5f,
                      MixRecipe { true, 0, 0.5f, 0.8f, true, 1.0f });
    checkRecipeBridge("polarity",
                      MixRecipe { true, 0, 0.5f, 0.5f, true, 0.0f },
                      0.5f,
                      MixRecipe { true, 0, 0.5f, 0.5f, false, 0.0f });

    // Detach is also a discrete recipe change: bridge from the last routed
    // coefficient to the already-warm legacy scalar without a hard switch.
    ZeroLatencyModulatedDryWetMixer detachMixer;
    detachMixer.prepare({ sampleRate, 512, 1 });
    const MixRecipe routed { true, 0, 0.5f, 0.6f, true, 1.0f };
    const MixRecipe detached;
    const auto routedPrime = processMixerBlock(detachMixer,
                                                64,
                                                routed,
                                                0.2f,
                                                true);
    const auto detach = processMixerBlock(detachMixer,
                                          routeRampSamples + 1,
                                          detached,
                                          0.2f,
                                          true);
    REQUIRE(allFinite(routedPrime));
    REQUIRE(allFinite(detach));
    CHECK(detach[0] == Catch::Approx(0.8f).margin(1.0e-6f));
    CHECK(detach[240] == Catch::Approx(0.5f).margin(2.0e-5f));
    CHECK(detach[480] == Catch::Approx(0.2f).margin(1.0e-6f));
}

TEST_CASE("Routed mixer enable changes retain the legacy fifty millisecond ramp",
          "[dsp][mix][lfo][enable][compatibility][zero-latency]")
{
    ZeroLatencyModulatedDryWetMixer mixer;
    mixer.prepare({ sampleRate, legacyRampSamples, 1 });
    const MixRecipe routed { true, 0, 0.5f, 0.6f, true, 1.0f };
    processMixerBlock(mixer, 8, routed, 0.5f, true);

    const auto disable = processMixerBlock(mixer,
                                           legacyRampSamples,
                                           routed,
                                           0.5f,
                                           false);
    const auto enable = processMixerBlock(mixer,
                                          legacyRampSamples,
                                          routed,
                                          0.5f,
                                          true);
    REQUIRE(allFinite(disable));
    REQUIRE(allFinite(enable));
    const float routedMix = expectedRoutedMix(routed, routed.constantLfo);
    CHECK(disable[0]
          == Catch::Approx(routedMix
                           * (1.0f - 1.0f / legacyRampSamples))
                 .margin(2.0e-5f));
    CHECK(disable[legacyRampSamples - 1]
          == Catch::Approx(0.0f).margin(1.0e-7f));
    CHECK(enable[0]
          == Catch::Approx(routedMix / legacyRampSamples).margin(2.0e-5f));
    CHECK(enable[legacyRampSamples - 1]
          == Catch::Approx(routedMix).margin(1.0e-6f));
}

TEST_CASE("Zero-latency mixer recipe retargeting is latest-wins and partition invariant",
          "[dsp][mix][lfo][recipe-transition][rapid][block-size][zero-latency]")
{
    ZeroLatencyModulatedDryWetMixer mixer;
    mixer.prepare({ sampleRate, 512, 1 });
    const MixRecipe recipeA { true, 0, 0.5f, 0.6f, true, 0.0f };
    const MixRecipe recipeB { true, 1, 0.5f, 0.6f, true, 1.0f };
    const MixRecipe recipeC { true, 2, 0.5f, 1.0f, true, 0.25f };
    processMixerBlock(mixer, 32, recipeA, 0.5f, true);
    const auto towardB = processMixerBlock(mixer, 160, recipeB, 0.5f, true);
    const auto towardC = processMixerBlock(mixer,
                                           routeRampSamples + 1,
                                           recipeC,
                                           0.5f,
                                           true);
    const float anchor = towardB.back();
    const float targetC = expectedRoutedMix(recipeC, recipeC.constantLfo);
    CAPTURE(anchor, targetC);
    REQUIRE(allFinite(towardB));
    REQUIRE(allFinite(towardC));
    CHECK(towardC[0] == Catch::Approx(anchor).margin(1.0e-6f));
    CHECK(towardC[240]
          == Catch::Approx(anchor + 0.5f * (targetC - anchor))
                 .margin(2.0e-5f));
    CHECK(towardC[480] == Catch::Approx(targetC).margin(1.0e-6f));

    const auto fixed = renderPartitionedBridge({ routeRampSamples });
    const auto irregular = renderPartitionedBridge({ 17, 31, 43, 29, 53, 37 });
    const float partitionError = maximumDifference(fixed, irregular);
    CAPTURE(partitionError);
    CHECK(partitionError < 1.0e-6f);
}

TEST_CASE("Band compressor Mix wires the analytic mixer trajectory in Base and HQ",
          "[processor][band][compressor][mix][lfo][trajectory][internal-chunk]")
{
    for (const bool useHq : std::array { false, true })
    {
        const auto whole = renderBandCompressorMix(useHq, lfoCycleSamples);
        const auto chunked = renderBandCompressorMix(useHq, 64);
        const float chunkError = std::max(
            maximumDifference(whole.output[0], chunked.output[0]),
            maximumDifference(whole.output[1], chunked.output[1]));
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
