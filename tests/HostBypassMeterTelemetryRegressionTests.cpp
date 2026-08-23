#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 257;
constexpr int normalWarmupCallbacks = 4;
constexpr int bypassCallbacks = 6;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void setLayout(FireAudioProcessor& processor, int numChannels)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    const auto channelSet = numChannels == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    REQUIRE(processor.setBusesLayout(layout));
}

void configureProcessor(FireAudioProcessor& processor,
                        bool useHq,
                        int numChannels)
{
    setLayout(processor, numChannels);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    // Keep the hidden wet graph audibly distinct from the host-bypass raw
    // path. A mistakenly published shadow packet must not satisfy the raw
    // output oracle below.
    setPlainParameter(processor, OUTPUT_ID, -12.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    const auto bandParameter = [] (const juce::String& base)
    {
        return ParameterIDAndName::getIDString(base, 0);
    };
    setPlainParameter(processor, bandParameter(BAND_ENABLE_ID), 0.0f);
    setPlainParameter(processor, bandParameter(BAND_SOLO_ID), 0.0f);
    setPlainParameter(processor, bandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(COMP_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID), 0.0f);
    setPlainParameter(processor, bandParameter(DC_FILTER_ID), 0.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID), 0.0f);
    setPlainParameter(processor, bandParameter(MIX_ID), 1.0f);
    processor.prepareToPlay(sampleRate, blockSize);
}

juce::AudioBuffer<float> makeInput(int numChannels,
                                    int firstAbsoluteSample,
                                    float amplitude)
{
    juce::AudioBuffer<float> buffer(numChannels, blockSize);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float channelScale = channel == 0 ? 1.0f : 0.61f;
        const double channelPhase = channel == 0 ? 0.17 : 0.83;
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto absoluteSample = static_cast<double>(firstAbsoluteSample
                                                             + sample);
            const auto primaryPhase = juce::MathConstants<double>::twoPi
                                      * 733.0 * absoluteSample / sampleRate;
            const auto secondaryPhase = juce::MathConstants<double>::twoPi
                                        * 2179.0 * absoluteSample / sampleRate;
            const float value = amplitude * channelScale
                              * (0.78f * static_cast<float>(std::sin(
                                     primaryPhase + channelPhase))
                                 + 0.22f * static_cast<float>(std::cos(
                                     secondaryPhase - 0.37 * channelPhase)));
            buffer.setSample(channel, sample, value);
        }
    }
    return buffer;
}

struct ExpectedLevels
{
    float rmsLeft = 0.0f;
    float rmsRight = 0.0f;
    float peakLeft = 0.0f;
    float peakRight = 0.0f;
};

ExpectedLevels getExpectedLevels(const juce::AudioBuffer<float>& buffer)
{
    REQUIRE(buffer.getNumChannels() >= 1);
    REQUIRE(buffer.getNumSamples() > 0);

    ExpectedLevels result;
    result.rmsLeft = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    result.peakLeft = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() > 1)
    {
        result.rmsRight = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        result.peakRight = buffer.getMagnitude(1, 0, buffer.getNumSamples());
    }
    else
    {
        result.rmsRight = result.rmsLeft;
        result.peakRight = result.peakLeft;
    }
    return result;
}

void requireGlobalLevels(const MeterValues& packet,
                         const ExpectedLevels& input,
                         const ExpectedLevels& output)
{
    REQUIRE(std::abs(packet.inputRMS_L - input.rmsLeft) < 1.0e-6f);
    REQUIRE(std::abs(packet.inputRMS_R - input.rmsRight) < 1.0e-6f);
    REQUIRE(std::abs(packet.inputPeak_L - input.peakLeft) < 1.0e-6f);
    REQUIRE(std::abs(packet.inputPeak_R - input.peakRight) < 1.0e-6f);
    REQUIRE(std::abs(packet.outputRMS_L - output.rmsLeft) < 1.0e-6f);
    REQUIRE(std::abs(packet.outputRMS_R - output.rmsRight) < 1.0e-6f);
    REQUIRE(std::abs(packet.outputPeak_L - output.peakLeft) < 1.0e-6f);
    REQUIRE(std::abs(packet.outputPeak_R - output.peakRight) < 1.0e-6f);

    REQUIRE(packet.inputRMS_L > 0.01f);
    REQUIRE(packet.inputRMS_R > 0.01f);
    REQUIRE(packet.inputPeak_L > 0.01f);
    REQUIRE(packet.inputPeak_R > 0.01f);
    REQUIRE(packet.outputRMS_L > 0.01f);
    REQUIRE(packet.outputRMS_R > 0.01f);
    REQUIRE(packet.outputPeak_L > 0.01f);
    REQUIRE(packet.outputPeak_R > 0.01f);
}

float maximumBandMeterDifference(const MeterValues& first,
                                 const MeterValues& second)
{
    float maximumDifference = 0.0f;
    const auto compare = [&maximumDifference] (const auto& lhs,
                                               const auto& rhs)
    {
        for (size_t band = 0; band < lhs.size(); ++band)
            maximumDifference = std::max(maximumDifference,
                                         std::abs(lhs[band] - rhs[band]));
    };

    compare(first.bandInputRMS_L, second.bandInputRMS_L);
    compare(first.bandInputRMS_R, second.bandInputRMS_R);
    compare(first.bandInputPeak_L, second.bandInputPeak_L);
    compare(first.bandInputPeak_R, second.bandInputPeak_R);
    compare(first.bandOutputRMS_L, second.bandOutputRMS_L);
    compare(first.bandOutputRMS_R, second.bandOutputRMS_R);
    compare(first.bandOutputPeak_L, second.bandOutputPeak_L);
    compare(first.bandOutputPeak_R, second.bandOutputPeak_R);
    return maximumDifference;
}
} // namespace

TEST_CASE("Host bypass continuously publishes delayed-raw global VU packets",
          "[processor][host-bypass][meter][telemetry]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const bool useHq : std::array { false, true })
    {
        for (const int numChannels : std::array { 1, 2 })
        {
            DYNAMIC_SECTION("HQ=" << useHq << ", channels=" << numChannels)
            {
                FireAudioProcessor processor;
                configureProcessor(processor, useHq, numChannels);
                REQUIRE(processor.getLatencySamples() > 0);

                MeterValues packet;
                REQUIRE_FALSE(processor.getLatestMeterValues(packet));

                juce::MidiBuffer midi;
                int streamPosition = 0;
                for (int callback = 0;
                     callback < normalWarmupCallbacks;
                     ++callback)
                {
                    auto buffer = makeInput(numChannels,
                                            streamPosition,
                                            0.09f);
                    processor.processBlock(buffer, midi);
                    streamPosition += blockSize;
                }

                MeterValues lastNormalPacket;
                REQUIRE(processor.getLatestMeterValues(lastNormalPacket));
                REQUIRE(lastNormalPacket.inputPeak_L > 0.01f);
                REQUIRE(lastNormalPacket.outputPeak_L > 0.001f);
                REQUIRE(lastNormalPacket.bandInputRMS_L[0] > 0.001f);
                REQUIRE(lastNormalPacket.bandOutputRMS_L[0] > 0.001f);
                REQUIRE(lastNormalPacket.bandInputPeak_L[0] > 0.01f);
                REQUIRE(lastNormalPacket.bandOutputPeak_L[0] > 0.001f);

                for (int callback = 0; callback < bypassCallbacks; ++callback)
                {
                    const float amplitude = 0.31f
                                            + 0.035f
                                                * static_cast<float>(callback);
                    auto buffer = makeInput(numChannels,
                                            streamPosition,
                                            amplitude);
                    const auto expectedInput = getExpectedLevels(buffer);
                    processor.processBlockBypassed(buffer, midi);
                    const auto expectedOutput = getExpectedLevels(buffer);
                    streamPosition += blockSize;

                    MeterValues bypassPacket;
                    CAPTURE(useHq, numChannels, callback);
                    REQUIRE(processor.getLatestMeterValues(bypassPacket));
                    requireGlobalLevels(bypassPacket,
                                        expectedInput,
                                        expectedOutput);

                    // Shadow rendering intentionally advances the wet graph,
                    // but host bypass does not publish new per-band telemetry.
                    // Every bypass packet therefore carries the last normally
                    // published band values while its global fields describe
                    // this callback's latency-matched raw input and output.
                    const float bandPacketError = maximumBandMeterDifference(
                        bypassPacket,
                        lastNormalPacket);
                    INFO("band packet error during host bypass = "
                         << bandPacketError);
                    REQUIRE(bandPacketError < 1.0e-7f);

                    // The larger bypass signal keeps stale global packets from
                    // satisfying the equality checks by coincidence.
                    REQUIRE(std::abs(bypassPacket.inputPeak_L
                                     - lastNormalPacket.inputPeak_L)
                            > 0.1f);
                }
            }
        }
    }
}
