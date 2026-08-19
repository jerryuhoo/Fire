#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double hostBpm = 123.0;
constexpr int preparedBlockSize = 257;
constexpr int oversizedSamples = 262145;
constexpr int tailSamples = 16384;
// The absolute-time LFO uses float phase accumulation. Across a 262145-sample
// callback its partition-dependent rounding is slightly larger than in the
// ordinary Global Mix regression, while the pre-fix dry-tail loss is orders of
// magnitude larger.
constexpr float comparisonTolerance = 3.0e-4f;

const std::vector<int> referenceBlocks { 4093, 2053, 3079, 257 };

enum class MainPath
{
    normal,
    hostBypass
};

class TimelinePlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        return position;
    }

    void setCallbackStart(int absoluteSample)
    {
        position = {};
        position.setIsPlaying(true);
        position.setBpm(hostBpm);
        position.setTimeSignature(TimeSignature { 4, 4 });
        position.setTimeInSamples(static_cast<juce::int64>(absoluteSample));
        position.setTimeInSeconds(static_cast<double>(absoluteSample)
                                  / sampleRate);
        position.setPpqPosition(static_cast<double>(absoluteSample)
                                * hostBpm / (60.0 * sampleRate));
    }

private:
    PositionInfo position;
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String bandParameter(const juce::String& baseID)
{
    return ParameterIDAndName::getIDString(baseID, 0);
}

juce::String lfoParameter(const juce::String& baseID)
{
    return ParameterIDAndName::getIDString(baseID, 0);
}

LfoData makeTriangleLfo()
{
    LfoData shape;
    shape.points = {
        { 0.0f, 0.0f },
        { 0.5f, 1.0f },
        { 1.0f, 0.0f }
    };
    shape.curvatures = { 0.0f, 0.0f };
    shape.smoothness = 0.0f;
    shape.sanitise();
    return shape;
}

void configureProcessor(FireAudioProcessor& processor,
                        bool useHq,
                        float globalMix,
                        bool modulateGlobalMix)
{
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, globalMix);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    // Keep the graph deterministic but make its wet endpoint clearly
    // different from the latency-matched global dry endpoint.
    setPlainParameter(processor, bandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, bandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, bandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, bandParameter(SAFE_ID), 0.0f);
    setPlainParameter(processor, bandParameter(EXTREME_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID), -18.0f);
    setPlainParameter(processor, bandParameter(MIX_ID), 1.0f);

    setPlainParameter(processor, lfoParameter(LFO_SYNC_MODE_ID), 0.0f);
    setPlainParameter(processor, lfoParameter(LFO_RATE_HZ_ID), 100.0f);
    setPlainParameter(processor, lfoParameter(LFO_PHASE_ID), 0.13f);
    setPlainParameter(processor, lfoParameter(LFO_SMOOTH_ID), 0.0f);
    processor.getLfoManager().setLfoData(0, makeTriangleLfo());
    if (modulateGlobalMix)
    {
        processor.assignLfoToTarget(0, MIX_ID);
        processor.setModulationDepth(MIX_ID, 1.0f);
    }

    // Intentionally retain the small host declaration. The regression covers
    // defensive handling when a host violates that advertised block hint.
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = static_cast<double>(channel) * 0.37;
    return 0.43f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 731.0 * time
                   + 0.19 + channelPhase))
         + 0.21f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 1817.0 * time
                   + 0.47 - channelPhase))
         + 0.08f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 67.0 * time
                   + 0.71 + 0.5 * channelPhase));
}

struct RenderResult
{
    std::array<std::vector<float>, 2> output;
    bool finite = true;
    int reportedLatency = 0;
};

RenderResult render(bool oneOversizedCallback,
                    bool useHq,
                    float globalMix,
                    bool modulateGlobalMix,
                    MainPath mainPath)
{
    FireAudioProcessor processor;
    configureProcessor(processor, useHq, globalMix, modulateGlobalMix);
    TimelinePlayHead playHead;
    processor.setPlayHead(&playHead);

    RenderResult result;
    result.reportedLatency = processor.getLatencySamples();
    REQUIRE(result.reportedLatency > 0);
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(oversizedSamples + tailSamples));

    juce::MidiBuffer midi;
    int streamPosition = 0;
    const auto processCallback = [&] (int numSamples, bool hostBypassed)
    {
        REQUIRE(numSamples > 0);
        juce::AudioBuffer<float> buffer(2, numSamples);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));

        playHead.setCallbackStart(streamPosition);
        if (hostBypassed)
            processor.processBlockBypassed(buffer, midi);
        else
            processor.processBlock(buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto& destination = result.output[static_cast<size_t>(channel)];
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                destination.push_back(value);
            }
        }
        streamPosition += numSamples;
    };

    size_t blockIndex = 0;
    const auto processPartitionedRange = [&] (int rangeEnd, bool hostBypassed)
    {
        while (streamPosition < rangeEnd)
        {
            const int requested = referenceBlocks[
                blockIndex % referenceBlocks.size()];
            ++blockIndex;
            processCallback(std::min(requested, rangeEnd - streamPosition),
                            hostBypassed);
        }
    };

    const bool bypassMain = mainPath == MainPath::hostBypass;
    if (oneOversizedCallback)
        processCallback(oversizedSamples, bypassMain);
    else
        processPartitionedRange(oversizedSamples, bypassMain);

    // Use the opposite path for a safely sized tail. Normal -> bypass exposes
    // the bypass mixer's primed raw history; bypass -> normal exposes the
    // hidden wet graph. Together they make both sides of the wrapper audible.
    blockIndex = 0;
    processPartitionedRange(oversizedSamples + tailSamples, ! bypassMain);
    REQUIRE(streamPosition == oversizedSamples + tailSamples);
    return result;
}

float maximumDifference(const RenderResult& first,
                        const RenderResult& second,
                        int firstSample,
                        int numSamples)
{
    REQUIRE(firstSample >= 0);
    REQUIRE(numSamples >= 0);
    float maximum = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        REQUIRE(firstSample + numSamples
                <= static_cast<int>(first.output[channel].size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
            maximum = std::max(
                maximum,
                std::abs(first.output[channel][static_cast<size_t>(sample)]
                         - second.output[channel][static_cast<size_t>(sample)]));
    }
    return maximum;
}

float maximumMagnitude(const RenderResult& result,
                       int firstSample,
                       int numSamples)
{
    float maximum = 0.0f;
    for (const auto& channel : result.output)
    {
        REQUIRE(firstSample >= 0);
        REQUIRE(firstSample + numSamples <= static_cast<int>(channel.size()));
        for (int sample = firstSample;
             sample < firstSample + numSamples;
             ++sample)
            maximum = std::max(
                maximum,
                std::abs(channel[static_cast<size_t>(sample)]));
    }
    return maximum;
}
} // namespace

TEST_CASE("Oversized callbacks preserve global mixer and host-bypass state",
          "[processor][oversized][global-mix][host-bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    // The processor reserves at least 8192 samples and prepares its global
    // DryWetMixers from that capacity. Their FIFO rounds 20 * 8192 up to
    // 262144 samples, so the one-sample-larger callback below used to jassert
    // in Debug and lose the dry tail in release builds.
    for (const bool useHq : std::array { false, true })
    {
        DYNAMIC_SECTION("normal HQ=" << useHq)
        {
            const auto dryOversized = render(true,
                                             useHq,
                                             0.0f,
                                             false,
                                             MainPath::normal);
            const auto dryReference = render(false,
                                             useHq,
                                             0.0f,
                                             false,
                                             MainPath::normal);
            const auto mixOversized = render(true,
                                             useHq,
                                             0.5f,
                                             true,
                                             MainPath::normal);
            const auto mixReference = render(false,
                                             useHq,
                                             0.5f,
                                             true,
                                             MainPath::normal);

            REQUIRE(dryOversized.finite);
            REQUIRE(dryReference.finite);
            REQUIRE(mixOversized.finite);
            REQUIRE(mixReference.finite);
            REQUIRE(dryOversized.reportedLatency
                    == dryReference.reportedLatency);
            REQUIRE(mixOversized.reportedLatency
                    == mixReference.reportedLatency);

            const float dryMainError = maximumDifference(
                dryOversized, dryReference, 0, oversizedSamples);
            const float dryTailError = maximumDifference(
                dryOversized, dryReference, oversizedSamples, tailSamples);
            const float mixMainError = maximumDifference(
                mixOversized, mixReference, 0, oversizedSamples);
            const float mixTailError = maximumDifference(
                mixOversized, mixReference, oversizedSamples, tailSamples);
            const float dryMixSeparation = maximumDifference(
                dryReference, mixReference, 4096, oversizedSamples - 4096);
            CAPTURE(useHq,
                    dryMainError,
                    dryTailError,
                    mixMainError,
                    mixTailError,
                    dryMixSeparation);

            REQUIRE(dryMixSeparation > 0.10f);
            CHECK(dryMainError < comparisonTolerance);
            CHECK(dryTailError < comparisonTolerance);
            CHECK(mixMainError < comparisonTolerance);
            CHECK(mixTailError < comparisonTolerance);
        }

        DYNAMIC_SECTION("host bypass HQ=" << useHq)
        {
            const auto oversized = render(true,
                                          useHq,
                                          0.5f,
                                          true,
                                          MainPath::hostBypass);
            const auto reference = render(false,
                                          useHq,
                                          0.5f,
                                          true,
                                          MainPath::hostBypass);
            REQUIRE(oversized.finite);
            REQUIRE(reference.finite);
            REQUIRE(oversized.reportedLatency == reference.reportedLatency);

            const float bypassError = maximumDifference(
                oversized, reference, 0, oversizedSamples);
            const float recoveryTailError = maximumDifference(
                oversized, reference, oversizedSamples, tailSamples);
            const float bypassMagnitude = maximumMagnitude(
                oversized, 4096, oversizedSamples - 4096);
            CAPTURE(useHq,
                    bypassError,
                    recoveryTailError,
                    bypassMagnitude);

            REQUIRE(bypassMagnitude > 0.10f);
            CHECK(bypassError < comparisonTolerance);
            CHECK(recoveryTailError < comparisonTolerance);
        }
    }
}
