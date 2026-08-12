#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr float lfoRateHz = 100.0f;
constexpr int lfoCycleSamples = static_cast<int>(sampleRate / lfoRateHz);
constexpr int warmupSamples = lfoCycleSamples * 12;
constexpr int comparisonSamples = lfoCycleSamples * 12;

enum class CompressorTarget
{
    threshold,
    ratio,
    mix,
    attack,
    release
};

enum class DirectRouting
{
    active,
    legacyScalar,
    providerWithoutRoute,
    providerWithZeroDepth
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void setNormalisedParameter(FireAudioProcessor& processor,
                            const juce::String& parameterID,
                            float normalisedValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(normalisedValue);
}

juce::String firstBandParameter(const juce::String& baseID)
{
    return ParameterIDAndName::getIDString(baseID, 0);
}

juce::String targetParameter(CompressorTarget target)
{
    switch (target)
    {
        case CompressorTarget::threshold: return firstBandParameter(COMP_THRESH_ID);
        case CompressorTarget::ratio:     return firstBandParameter(COMP_RATIO_ID);
        case CompressorTarget::mix:       return firstBandParameter(COMP_MIX_ID);
        case CompressorTarget::attack:    return firstBandParameter(COMP_ATTACK_ID);
        case CompressorTarget::release:   return firstBandParameter(COMP_RELEASE_ID);
    }

    jassertfalse;
    return {};
}

const char* targetName(CompressorTarget target)
{
    switch (target)
    {
        case CompressorTarget::threshold: return "Threshold";
        case CompressorTarget::ratio:     return "Ratio";
        case CompressorTarget::mix:       return "Mix";
        case CompressorTarget::attack:    return "Attack";
        case CompressorTarget::release:   return "Release";
    }

    return "Unknown";
}

std::array<float, 2> targetEndpoints(CompressorTarget target)
{
    switch (target)
    {
        case CompressorTarget::threshold: return { -48.0f, 0.0f };
        case CompressorTarget::ratio:     return { 1.0f, 20.0f };
        case CompressorTarget::mix:       return { 0.0f, 1.0f };
        case CompressorTarget::attack:    return { 0.1f, 200.0f };
        case CompressorTarget::release:   return { 10.0f, 2000.0f };
    }

    return { 0.0f, 0.0f };
}

LfoData makeTriangleLfo()
{
    LfoData shape;
    shape.points = { { 0.0f, 0.0f },
                     { 0.5f, 1.0f },
                     { 1.0f, 0.0f } };
    shape.curvatures = { 0.0f, 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

void configureProcessor(FireAudioProcessor& processor,
                        int preparedBlockSize,
                        CompressorTarget target,
                        bool modulateTarget,
                        float targetValue,
                        bool useHq)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    setPlainParameter(processor, firstBandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(SHAPE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(COMP_BYPASS_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(MODE_ID), 2.0f); // tanh
    setPlainParameter(processor, firstBandParameter(OUTPUT_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(MIX_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(SHAPE_MIX_ID), 1.0f);

    // Strong compression makes the Threshold/Ratio/Mix endpoints audibly
    // distinct. Fast companion timing isolates whichever timing target moves.
    setPlainParameter(processor, firstBandParameter(COMP_THRESH_ID), -30.0f);
    setPlainParameter(processor, firstBandParameter(COMP_RATIO_ID), 20.0f);
    setPlainParameter(processor, firstBandParameter(COMP_ATTACK_ID), 0.1f);
    setPlainParameter(processor, firstBandParameter(COMP_RELEASE_ID), 10.0f);
    setPlainParameter(processor, firstBandParameter(COMP_MIX_ID), 1.0f);

    const auto parameterID = targetParameter(target);
    if (modulateTarget)
        setNormalisedParameter(processor, parameterID, 0.5f);
    else
        setPlainParameter(processor, parameterID, targetValue);

    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0),
                      lfoRateHz);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_PHASE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 0),
                      0.0f);
    processor.getLfoManager().setLfoData(0, makeTriangleLfo());
    if (modulateTarget)
    {
        processor.assignLfoToTarget(0, parameterID);
        processor.setModulationDepth(parameterID, 1.0f);
    }

    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float inputEnvelope(CompressorTarget target, int absoluteSample)
{
    const int cycleSample = absoluteSample % lfoCycleSamples;
    if (target == CompressorTarget::attack)
        return cycleSample < lfoCycleSamples / 2 ? 0.015f : 0.92f;
    if (target == CompressorTarget::release)
        return cycleSample < lfoCycleSamples / 4 ? 0.92f : 0.045f;
    return 0.82f;
}

float inputSample(CompressorTarget target,
                  int channel,
                  int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = channel == 0 ? 0.13 : 0.59;
    const float carrier = 0.82f * static_cast<float>(std::sin(
                            juce::MathConstants<double>::twoPi * 997.0 * time
                            + channelPhase))
                        + 0.18f * static_cast<float>(std::sin(
                            juce::MathConstants<double>::twoPi * 3251.0 * time
                            + 0.37 - channelPhase));
    return inputEnvelope(target, absoluteSample) * carrier;
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool isFinite = true;
};

RenderResult render(CompressorTarget target,
                    const std::vector<int>& hostBlockPattern,
                    bool modulateTarget,
                    float targetValue,
                    bool useHq)
{
    REQUIRE_FALSE(hostBlockPattern.empty());
    for (const int blockSize : hostBlockPattern)
        REQUIRE(blockSize > 0);

    FireAudioProcessor processor;
    configureProcessor(processor,
                       *std::max_element(hostBlockPattern.begin(),
                                         hostBlockPattern.end()),
                       target,
                       modulateTarget,
                       targetValue,
                       useHq);

    RenderResult result;
    for (auto& output : result.output)
        output.reserve(comparisonSamples);

    const int totalSamples = warmupSamples + comparisonSamples;
    int streamPosition = 0;
    size_t blockIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        const int requested = hostBlockPattern[blockIndex % hostBlockPattern.size()];
        ++blockIndex;
        const int samplesThisBlock = std::min(requested,
                                              totalSamples - streamPosition);
        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(target,
                                             channel,
                                             streamPosition + sample));

        processor.processBlock(buffer, midi);

        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            if (absoluteSample < warmupSamples)
                continue;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const float value = buffer.getSample(channel, sample);
                result.isFinite = result.isFinite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        }

        streamPosition += samplesThisBlock;
    }

    return result;
}

float triangleLfoSample(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoCycleSamples)
                        / static_cast<float>(lfoCycleSamples);
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

void configureProvider(ModulatedValueProvider& provider,
                       float baseValue,
                       float minimum,
                       float maximum,
                       float depth)
{
    provider.baseValue = baseValue;
    provider.range = { minimum, maximum };
    provider.modulationDepth = depth;
}

BandProcessingParameters makeDirectBandParameters(CompressorTarget target,
                                                   bool useHq,
                                                   DirectRouting routing)
{
    BandProcessingParameters params;
    params.mode = 2; // tanh
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isCompEnabled = true;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.mixVal = 1.0f;
    params.shapeMixVal = 1.0f;
    params.compThreshold = -30.0f;
    params.compRatio = 20.0f;
    params.compAttack = 0.1f;
    params.compRelease = 10.0f;
    params.compMixVal = target == CompressorTarget::mix ? 0.5f : 1.0f;

    if (routing == DirectRouting::active)
    {
        if (target == CompressorTarget::threshold)
        {
            configureProvider(params.compThresholdValProvider,
                              -24.0f,
                              -48.0f,
                              0.0f,
                              1.0f);
            params.compThresholdLfoSourceIndex = 0;
        }
        else if (target == CompressorTarget::mix)
        {
            configureProvider(params.compMixValProvider,
                              0.5f,
                              0.0f,
                              1.0f,
                              1.0f);
            params.compMixLfoSourceIndex = 0;
        }
        return params;
    }

    if (routing == DirectRouting::legacyScalar)
        return params;

    const float depth = routing == DirectRouting::providerWithZeroDepth
                            ? 0.0f
                            : 1.0f;
    const int sourceIndex = routing == DirectRouting::providerWithZeroDepth
                                ? 0
                                : -1;

    // These bases deliberately disagree with the scalar values. Without a
    // valid non-zero-depth route, the legacy scalar contract remains decisive.
    configureProvider(params.compThresholdValProvider, -2.0f, -48.0f, 0.0f, depth);
    configureProvider(params.compRatioValProvider, 1.2f, 1.0f, 20.0f, depth);
    configureProvider(params.compAttackValProvider, 180.0f, 0.1f, 200.0f, depth);
    configureProvider(params.compReleaseValProvider, 1800.0f, 10.0f, 2000.0f, depth);
    configureProvider(params.compMixValProvider, 0.07f, 0.0f, 1.0f, depth);
    params.compThresholdLfoSourceIndex = sourceIndex;
    params.compRatioLfoSourceIndex = sourceIndex;
    params.compAttackLfoSourceIndex = sourceIndex;
    params.compReleaseLfoSourceIndex = sourceIndex;
    params.compMixLfoSourceIndex = sourceIndex;
    return params;
}

RenderResult renderDirectBandProcessor(CompressorTarget target,
                                       int preparedBlockSize,
                                       bool useHq,
                                       DirectRouting routing)
{
    REQUIRE(preparedBlockSize > 0);

    BandProcessor band;
    band.prepare({ sampleRate,
                   static_cast<juce::uint32>(preparedBlockSize),
                   2 });
    const auto params = makeDirectBandParameters(target, useHq, routing);

    RenderResult result;
    for (auto& output : result.output)
        output.reserve(comparisonSamples);

    const int totalSamples = warmupSamples + comparisonSamples;
    for (int streamPosition = 0;
         streamPosition < totalSamples;
         streamPosition += lfoCycleSamples)
    {
        const int samplesThisBlock = std::min(lfoCycleSamples,
                                              totalSamples - streamPosition);
        juce::AudioBuffer<float> buffer(2, samplesThisBlock);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisBlock);
        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            lfoOutputs.setSample(0, sample, triangleLfoSample(absoluteSample));
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(target, channel, absoluteSample));
        }

        band.process(buffer, params, lfoOutputs);

        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            if (absoluteSample < warmupSamples)
                continue;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const float value = buffer.getSample(channel, sample);
                result.isFinite = result.isFinite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        }
    }

    return result;
}

float maximumDifference(const RenderResult& first,
                        const RenderResult& second)
{
    float maximumError = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (size_t sample = 0; sample < first.output[channel].size(); ++sample)
        {
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][sample]
                         - second.output[channel][sample]));
        }
    }

    return maximumError;
}

void checkBlockSizeIndependence(CompressorTarget target, bool useHq)
{
    const auto fullCycleBlocks = render(target,
                                        { lfoCycleSamples },
                                        true,
                                        0.0f,
                                        useHq);
    const auto irregularBlocks = render(target,
                                        { 17, 31, 43, 29, 53, 37 },
                                        true,
                                        0.0f,
                                        useHq);
    const auto endpoints = targetEndpoints(target);
    const auto firstEndpoint = render(target,
                                      { lfoCycleSamples },
                                      false,
                                      endpoints[0],
                                      useHq);
    const auto secondEndpoint = render(target,
                                       { lfoCycleSamples },
                                       false,
                                       endpoints[1],
                                       useHq);

    REQUIRE(fullCycleBlocks.output[0].size() == comparisonSamples);
    REQUIRE(irregularBlocks.output[0].size() == comparisonSamples);
    CHECK(fullCycleBlocks.isFinite);
    CHECK(irregularBlocks.isFinite);
    CHECK(firstEndpoint.isFinite);
    CHECK(secondEndpoint.isFinite);

    const float maximumError = maximumDifference(fullCycleBlocks,
                                                  irregularBlocks);
    const float endpointSeparation = maximumDifference(firstEndpoint,
                                                       secondEndpoint);
    CAPTURE(targetName(target), useHq, maximumError, endpointSeparation);

    REQUIRE(endpointSeparation > 0.01f);
    CHECK(maximumError < 2.0e-4f);
}

void checkInternalChunkOffset(CompressorTarget target, bool useHq)
{
    const auto wholeHostBlock = renderDirectBandProcessor(target,
                                                          lfoCycleSamples,
                                                          useHq,
                                                          DirectRouting::active);
    const auto internallyChunked = renderDirectBandProcessor(target,
                                                             64,
                                                             useHq,
                                                             DirectRouting::active);

    REQUIRE(wholeHostBlock.output[0].size() == comparisonSamples);
    REQUIRE(internallyChunked.output[0].size() == comparisonSamples);
    CHECK(wholeHostBlock.isFinite);
    CHECK(internallyChunked.isFinite);

    const float maximumError = maximumDifference(wholeHostBlock,
                                                  internallyChunked);
    CAPTURE(targetName(target), useHq, maximumError);
    CHECK(maximumError < 2.0e-4f);
}

void checkNoRouteScalarCompatibility(bool useHq, DirectRouting routing)
{
    const auto legacy = renderDirectBandProcessor(CompressorTarget::threshold,
                                                  lfoCycleSamples,
                                                  useHq,
                                                  DirectRouting::legacyScalar);
    const auto providerFields = renderDirectBandProcessor(CompressorTarget::threshold,
                                                          lfoCycleSamples,
                                                          useHq,
                                                          routing);

    CHECK(legacy.isFinite);
    CHECK(providerFields.isFinite);
    const float maximumError = maximumDifference(legacy, providerFields);
    CAPTURE(useHq,
            routing == DirectRouting::providerWithoutRoute
                ? "no route"
                : "zero depth",
            maximumError);
    CHECK(maximumError <= 1.0e-7f);
}

juce::AudioBuffer<float> makeGoldenInput(int numSamples)
{
    juce::AudioBuffer<float> input(2, numSamples);
    juce::Random random(0x51a7c0de);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float phase = static_cast<float>(sample % 997) / 997.0f;
        const float envelope = 0.025f + 0.95f
                                          * (0.5f - 0.5f * std::cos(
                                              juce::MathConstants<float>::twoPi
                                              * phase));
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
        {
            const float randomSample = random.nextFloat() * 2.0f - 1.0f;
            const float tone = std::sin(0.071f * static_cast<float>(sample)
                                        + 0.43f * static_cast<float>(channel));
            input.setSample(channel,
                            sample,
                            envelope * (0.72f * randomSample + 0.28f * tone));
        }
    }
    return input;
}

float maximumBufferDifference(const juce::AudioBuffer<float>& first,
                              const juce::AudioBuffer<float>& second)
{
    REQUIRE(first.getNumChannels() == second.getNumChannels());
    REQUIRE(first.getNumSamples() == second.getNumSamples());
    float maximumError = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            maximumError = std::max(maximumError,
                                    std::abs(first.getSample(channel, sample)
                                             - second.getSample(channel, sample)));
    return maximumError;
}

void setCompressorParameters(SampleAccurateCompressor& compressor,
                             float threshold,
                             float ratio,
                             float attack,
                             float release)
{
    compressor.setThreshold(threshold);
    compressor.setRatio(ratio);
    compressor.setAttack(attack);
    compressor.setRelease(release);
}

void setCompressorParameters(juce::dsp::Compressor<float>& compressor,
                             float threshold,
                             float ratio,
                             float attack,
                             float release)
{
    compressor.setThreshold(threshold);
    compressor.setRatio(ratio);
    compressor.setAttack(attack);
    compressor.setRelease(release);
}
} // namespace

TEST_CASE("Compressor LFO modulation is independent of host callback partitioning",
          "[processor][compressor][lfo][block-size]")
{
    SECTION("Threshold")
    {
        checkBlockSizeIndependence(CompressorTarget::threshold, false);
        checkBlockSizeIndependence(CompressorTarget::threshold, true);
    }

    SECTION("Ratio")
    {
        checkBlockSizeIndependence(CompressorTarget::ratio, false);
        checkBlockSizeIndependence(CompressorTarget::ratio, true);
    }

    SECTION("Mix")
    {
        checkBlockSizeIndependence(CompressorTarget::mix, false);
        checkBlockSizeIndependence(CompressorTarget::mix, true);
    }

    SECTION("Attack")
    {
        checkBlockSizeIndependence(CompressorTarget::attack, false);
        checkBlockSizeIndependence(CompressorTarget::attack, true);
    }

    SECTION("Release")
    {
        checkBlockSizeIndependence(CompressorTarget::release, false);
        checkBlockSizeIndependence(CompressorTarget::release, true);
    }
}

TEST_CASE("Compressor LFO providers preserve their offset across internal chunks",
          "[processor][compressor][lfo][block-size][internal-chunk]")
{
    SECTION("Threshold")
    {
        checkInternalChunkOffset(CompressorTarget::threshold, false);
        checkInternalChunkOffset(CompressorTarget::threshold, true);
    }

    SECTION("Mix")
    {
        checkInternalChunkOffset(CompressorTarget::mix, false);
        checkInternalChunkOffset(CompressorTarget::mix, true);
    }
}

TEST_CASE("Inactive compressor providers preserve legacy scalar processing",
          "[processor][compressor][provider][compatibility]")
{
    SECTION("Provider fields have no route")
    {
        checkNoRouteScalarCompatibility(false,
                                        DirectRouting::providerWithoutRoute);
        checkNoRouteScalarCompatibility(true,
                                        DirectRouting::providerWithoutRoute);
    }

    SECTION("Provider route has zero depth")
    {
        checkNoRouteScalarCompatibility(false,
                                        DirectRouting::providerWithZeroDepth);
        checkNoRouteScalarCompatibility(true,
                                        DirectRouting::providerWithZeroDepth);
    }
}

TEST_CASE("SampleAccurateCompressor matches JUCE compressor",
          "[compressor][sample-accurate][golden]")
{
    constexpr int goldenSamples = 8192;
    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32>(goldenSamples),
                                        2 };

    SECTION("Static settings and random stereo envelope")
    {
        SampleAccurateCompressor subject;
        juce::dsp::Compressor<float> reference;
        subject.prepare(spec);
        reference.prepare(spec);
        setCompressorParameters(subject, -23.7f, 6.3f, 7.25f, 143.0f);
        setCompressorParameters(reference, -23.7f, 6.3f, 7.25f, 143.0f);

        const auto input = makeGoldenInput(goldenSamples);
        auto actual = input;
        auto expected = input;
        auto actualBlock = juce::dsp::AudioBlock<float>(actual);
        auto expectedBlock = juce::dsp::AudioBlock<float>(expected);
        subject.process(juce::dsp::ProcessContextReplacing<float>(actualBlock));
        reference.process(juce::dsp::ProcessContextReplacing<float>(expectedBlock));

        const float maximumError = maximumBufferDifference(actual, expected);
        const float processedSeparation = maximumBufferDifference(input, expected);
        CAPTURE(maximumError, processedSeparation);
        REQUIRE(processedSeparation > 0.1f);
        CHECK(maximumError <= 1.0e-6f);
    }

    SECTION("Per-sample parameter automation uses the same setter order")
    {
        SampleAccurateCompressor subject;
        juce::dsp::Compressor<float> reference;
        subject.prepare(spec);
        reference.prepare(spec);

        const auto input = makeGoldenInput(goldenSamples);
        juce::AudioBuffer<float> actual(2, goldenSamples);
        juce::AudioBuffer<float> expected(2, goldenSamples);
        float maximumGainEnvelopeError = 0.0f;
        for (int sample = 0; sample < goldenSamples; ++sample)
        {
            const float phase = static_cast<float>(sample % 1201) / 1201.0f;
            const float sine = 0.5f + 0.5f * std::sin(
                juce::MathConstants<float>::twoPi * phase);
            const float triangle = phase < 0.5f ? phase * 2.0f
                                                : (1.0f - phase) * 2.0f;
            const float threshold = -46.0f + 44.0f * triangle;
            const float ratio = 1.0f + 19.0f * sine;
            const float attack = 0.1f + 199.9f * triangle;
            const float release = 10.0f + 1990.0f * sine;

            // The exact Threshold -> Ratio -> Attack -> Release sequence is
            // applied to both engines before either processes this sample.
            setCompressorParameters(subject,
                                    threshold,
                                    ratio,
                                    attack,
                                    release);
            setCompressorParameters(reference,
                                    threshold,
                                    ratio,
                                    attack,
                                    release);

            for (int channel = 0; channel < input.getNumChannels(); ++channel)
            {
                const float dry = input.getSample(channel, sample);
                const float actualSample = subject.processSample(channel, dry);
                const float expectedSample = reference.processSample(channel, dry);
                actual.setSample(channel, sample, actualSample);
                expected.setSample(channel, sample, expectedSample);
                if (std::abs(dry) > 1.0e-4f)
                {
                    maximumGainEnvelopeError = std::max(
                        maximumGainEnvelopeError,
                        std::abs(actualSample / dry - expectedSample / dry));
                }
            }
        }

        const float maximumError = maximumBufferDifference(actual, expected);
        CAPTURE(maximumError, maximumGainEnvelopeError);
        CHECK(maximumError <= 1.0e-6f);
        CHECK(maximumGainEnvelopeError <= 1.0e-6f);
    }
}
