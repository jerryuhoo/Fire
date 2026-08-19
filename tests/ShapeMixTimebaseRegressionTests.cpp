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
constexpr int shapeMixRampSamples = 2400; // 50 ms at 48 kHz.
constexpr int warmupSamples = 4096;
constexpr int capturedRampSamples = shapeMixRampSamples + 256;
constexpr int endpointWindowSamples = 64;
constexpr float projectionTolerance = 0.025f;

constexpr std::array<float, 2> inputValues { 0.43f, -0.37f };
const std::vector<int> fixedCallbacks { 257 };
const std::vector<int> irregularCallbacks { 31, 7, 193, 1, 89, 251, 13 };

BandProcessingParameters makeParameters(bool useHq, float shapeMix)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 8; // Limit: a stable, strongly separated +/-0.1 wet endpoint.
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
    params.shapeMixVal = shapeMix;
    params.shapeMixValProvider.baseValue = shapeMix;
    params.driveVal.baseValue = 0.0f;
    params.driveVal.range = { 0.0f, 100.0f };
    params.biasVal.baseValue = 0.0f;
    params.biasVal.range = { -1.0f, 1.0f };
    params.recVal.baseValue = 0.0f;
    params.recVal.range = { 0.0f, 1.0f };
    return params;
}

juce::AudioBuffer<float> makeInput(int numChannels, int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
        juce::FloatVectorOperations::fill(
            buffer.getWritePointer(channel),
            inputValues[static_cast<size_t>(channel)],
            numSamples);
    return buffer;
}

struct EnvelopeResult
{
    std::vector<float> envelope;
    std::array<float, 2> dryEndpoint {};
    std::array<float, 2> wetEndpoint {};
    float initialDryError = 0.0f;
    float finalWetError = 0.0f;
    float minimumEndpointSeparation = std::numeric_limits<float>::max();
    float minimumProjectedMix = std::numeric_limits<float>::max();
    float maximumProjectedMix = std::numeric_limits<float>::lowest();
    bool finite = true;
};

EnvelopeResult renderEnvelope(bool useHq,
                              int numChannels,
                              int preparedBlockSize,
                              const std::vector<int>& hostBlockPattern)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostBlockPattern.empty());

    BandProcessor subject;
    BandProcessor alwaysDry;
    BandProcessor alwaysWet;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        static_cast<juce::uint32>(numChannels)
    };
    subject.prepare(spec);
    alwaysDry.prepare(spec);
    alwaysWet.prepare(spec);

    const auto dryParams = makeParameters(useHq, 0.0f);
    const auto wetParams = makeParameters(useHq, 1.0f);
    EnvelopeResult result;
    result.envelope.reserve(capturedRampSamples);

    constexpr int totalSamples = warmupSamples + capturedRampSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        int samplesThisCallback = hostBlockPattern[
            callbackIndex % hostBlockPattern.size()];
        ++callbackIndex;
        REQUIRE(samplesThisCallback > 0);
        samplesThisCallback = std::min(samplesThisCallback,
                                       totalSamples - streamPosition);
        if (streamPosition < warmupSamples)
            samplesThisCallback = std::min(samplesThisCallback,
                                           warmupSamples - streamPosition);

        auto input = makeInput(numChannels, samplesThisCallback);
        juce::AudioBuffer<float> subjectBuffer;
        juce::AudioBuffer<float> dryBuffer;
        juce::AudioBuffer<float> wetBuffer;
        subjectBuffer.makeCopyOf(input);
        dryBuffer.makeCopyOf(input);
        wetBuffer.makeCopyOf(input);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisCallback);
        lfoOutputs.clear();

        subject.process(subjectBuffer,
                        streamPosition < warmupSamples ? dryParams : wetParams,
                        lfoOutputs);
        alwaysDry.process(dryBuffer, dryParams, lfoOutputs);
        alwaysWet.process(wetBuffer, wetParams, lfoOutputs);

        for (int sample = 0; sample < samplesThisCallback; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            float projectedMixSum = 0.0f;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float subjectSample = subjectBuffer.getSample(channel, sample);
                const float drySample = dryBuffer.getSample(channel, sample);
                const float wetSample = wetBuffer.getSample(channel, sample);
                result.finite = result.finite
                                && std::isfinite(subjectSample)
                                && std::isfinite(drySample)
                                && std::isfinite(wetSample);

                if (absoluteSample < warmupSamples)
                {
                    result.initialDryError = std::max(
                        result.initialDryError,
                        std::abs(subjectSample - drySample));
                    continue;
                }

                const float separation = wetSample - drySample;
                result.minimumEndpointSeparation = std::min(
                    result.minimumEndpointSeparation,
                    std::abs(separation));
                REQUIRE(std::abs(separation) > 0.10f);
                projectedMixSum += (subjectSample - drySample) / separation;

                const int eventSample = absoluteSample - warmupSamples;
                if (eventSample >= capturedRampSamples
                                       - endpointWindowSamples)
                {
                    result.finalWetError = std::max(
                        result.finalWetError,
                        std::abs(subjectSample - wetSample));
                }

                if (eventSample == capturedRampSamples - 1)
                {
                    result.dryEndpoint[static_cast<size_t>(channel)] = drySample;
                    result.wetEndpoint[static_cast<size_t>(channel)] = wetSample;
                }
            }

            if (absoluteSample >= warmupSamples)
            {
                const float projectedMix = projectedMixSum
                                           / static_cast<float>(numChannels);
                result.envelope.push_back(projectedMix);
                result.minimumProjectedMix = std::min(result.minimumProjectedMix,
                                                      projectedMix);
                result.maximumProjectedMix = std::max(result.maximumProjectedMix,
                                                      projectedMix);
            }
        }

        streamPosition += samplesThisCallback;
    }

    REQUIRE(result.envelope.size() == capturedRampSamples);
    return result;
}

float maximumEnvelopeDifference(const EnvelopeResult& first,
                                const EnvelopeResult& second)
{
    REQUIRE(first.envelope.size() == second.envelope.size());
    float result = 0.0f;
    for (size_t sample = 0; sample < first.envelope.size(); ++sample)
        result = std::max(result,
                          std::abs(first.envelope[sample]
                                   - second.envelope[sample]));
    return result;
}

int firstSampleAtOrAbove(const EnvelopeResult& result, float threshold)
{
    const auto found = std::find_if(result.envelope.begin(),
                                    result.envelope.end(),
                                    [threshold](float value)
                                    {
                                        return value >= threshold;
                                    });
    return found == result.envelope.end()
               ? static_cast<int>(result.envelope.size())
               : static_cast<int>(std::distance(result.envelope.begin(), found));
}

void requireValidEndpoints(const EnvelopeResult& result)
{
    REQUIRE(result.finite);
    REQUIRE(result.minimumEndpointSeparation > 0.10f);
    CHECK(result.initialDryError <= 1.0e-6f);
    CHECK(result.finalWetError <= 2.0e-4f);
    CHECK(std::isfinite(result.minimumProjectedMix));
    CHECK(std::isfinite(result.maximumProjectedMix));
    CHECK(result.minimumProjectedMix >= -0.03f);
    CHECK(result.maximumProjectedMix <= 1.03f);
}
} // namespace

TEST_CASE("Shape Mix ramp uses base-rate time in Base and HQ modes",
          "[band][shape-mix][timebase][hq]")
{
    for (const int numChannels : { 1, 2 })
    {
        DYNAMIC_SECTION("channels=" << numChannels)
        {
            const auto base = renderEnvelope(false,
                                             numChannels,
                                             257,
                                             fixedCallbacks);
            const auto hq = renderEnvelope(true,
                                           numChannels,
                                           257,
                                           fixedCallbacks);
            requireValidEndpoints(base);
            requireValidEndpoints(hq);

            constexpr int quarterRampSample = shapeMixRampSamples / 4 - 1;
            constexpr int halfRampSample = shapeMixRampSamples / 2 - 1;
            const int baseCompletionSample = firstSampleAtOrAbove(base, 0.99f);
            const int hqCompletionSample = firstSampleAtOrAbove(hq, 0.99f);
            CAPTURE(numChannels,
                    base.envelope[quarterRampSample],
                    hq.envelope[quarterRampSample],
                    base.envelope[halfRampSample],
                    hq.envelope[halfRampSample],
                    baseCompletionSample,
                    hqCompletionSample);

            CHECK(base.envelope[quarterRampSample]
                  == Catch::Approx(0.25f).margin(projectionTolerance));
            CHECK(base.envelope[halfRampSample]
                  == Catch::Approx(0.50f).margin(projectionTolerance));
            CHECK(std::abs(hq.envelope[quarterRampSample]
                           - base.envelope[quarterRampSample])
                  <= projectionTolerance);
            CHECK(std::abs(hq.envelope[halfRampSample]
                           - base.envelope[halfRampSample])
                  <= projectionTolerance);
            REQUIRE(baseCompletionSample >= 2340);
            REQUIRE(baseCompletionSample <= shapeMixRampSamples);
            CHECK(std::abs(hqCompletionSample - baseCompletionSample) <= 24);

            // Static Base endpoints remain the independent legacy transfer:
            // Shape Mix 0 is the input and mode 8 limits Shape Mix 1 to +/-0.1.
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float input = inputValues[static_cast<size_t>(channel)];
                const float expectedWet = juce::jlimit(-0.1f, 0.1f, input);
                CHECK(base.dryEndpoint[static_cast<size_t>(channel)]
                      == Catch::Approx(input).margin(1.0e-5f));
                CHECK(base.wetEndpoint[static_cast<size_t>(channel)]
                      == Catch::Approx(expectedWet).margin(1.0e-5f));
            }
        }
    }
}

TEST_CASE("Shape Mix ramp ignores host partition and internal chunks",
          "[band][shape-mix][timebase][block-size][internal-chunk]")
{
    for (const bool useHq : { false, true })
        for (const int numChannels : { 1, 2 })
        {
            DYNAMIC_SECTION("HQ=" << useHq << ", channels=" << numChannels)
            {
                const auto fixed = renderEnvelope(useHq,
                                                  numChannels,
                                                  257,
                                                  fixedCallbacks);
                const auto irregular = renderEnvelope(useHq,
                                                      numChannels,
                                                      257,
                                                      irregularCallbacks);
                const auto internallyChunked = renderEnvelope(useHq,
                                                              numChannels,
                                                              64,
                                                              fixedCallbacks);
                requireValidEndpoints(fixed);
                requireValidEndpoints(irregular);
                requireValidEndpoints(internallyChunked);

                const float partitionError = maximumEnvelopeDifference(fixed,
                                                                       irregular);
                const float chunkError = maximumEnvelopeDifference(fixed,
                                                                   internallyChunked);
                CAPTURE(useHq,
                        numChannels,
                        partitionError,
                        chunkError);
                CHECK(partitionError <= 2.0e-3f);
                CHECK(chunkError <= 2.0e-3f);
            }
        }
}
