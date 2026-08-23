#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
constexpr double testSampleRate = 48000.0;
constexpr int preparedBlockSize = 8192;
constexpr int oversizedBlockSize = preparedBlockSize + 1;

BandProcessingParameters makeModulatedHqParameters()
{
    BandProcessingParameters params;
    params.mode = 2; // tanh
    params.isHQ = true;
    params.isDriveEnabled = true;
    params.isShapeEnabled = true;
    params.isCompEnabled = true;
    params.isWidthEnabled = true;
    params.isDcFilterEnabled = true;
    params.isSafeModeOn = true;

    params.driveVal.baseValue = 68.0f;
    params.driveVal.modulationDepth = 0.55f;
    params.driveVal.range = { 0.0f, 100.0f };
    params.driveLfoSourceIndex = 0;

    params.biasVal.baseValue = 0.05f;
    params.biasVal.modulationDepth = 0.25f;
    params.biasVal.range = { -1.0f, 1.0f };
    params.biasLfoSourceIndex = 1;

    params.recVal.baseValue = 0.2f;
    params.recVal.modulationDepth = 0.3f;
    params.recVal.range = { 0.0f, 1.0f };
    params.recLfoSourceIndex = 2;

    params.outputVal.baseValue = -3.0f;
    params.outputVal.modulationDepth = 0.35f;
    params.outputVal.range = { -48.0f, 6.0f };
    params.outputLfoSourceIndex = 3;

    params.mixVal = 0.84f;
    params.shapeMixVal = 0.73f;
    params.compThreshold = -18.0f;
    params.compRatio = 3.5f;
    params.compAttack = 4.0f;
    params.compRelease = 90.0f;
    params.compMixVal = 0.62f;
    params.width = 0.72f;
    params.pan = -0.18f;
    params.widthMixVal = 0.67f;
    return params;
}

juce::AudioBuffer<float> makeInput()
{
    juce::AudioBuffer<float> buffer(2, oversizedBlockSize);

    for (int sample = 0; sample < oversizedBlockSize; ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi
                           * 613.0 * static_cast<double>(sample) / testSampleRate;
        buffer.setSample(0, sample, 0.12f * static_cast<float>(std::sin(phase)));
        buffer.setSample(1, sample, 0.09f * static_cast<float>(std::cos(phase * 1.37)));
    }

    // Put the callback peak exclusively in the one-sample overflow chunk.
    // Telemetry must still report it, while the causal Safe envelope must reach
    // that sample at the same absolute time without affecting earlier audio.
    buffer.setSample(0, oversizedBlockSize - 1, 0.95f);
    buffer.setSample(1, oversizedBlockSize - 1, -0.85f);
    return buffer;
}

juce::AudioBuffer<float> makeLfoOutputs()
{
    juce::AudioBuffer<float> lfo(4, oversizedBlockSize);

    for (int sample = 0; sample < oversizedBlockSize; ++sample)
    {
        const float position = static_cast<float>(sample)
                               / static_cast<float>(oversizedBlockSize - 1);
        lfo.setSample(0, sample, position);
        lfo.setSample(1, sample, 0.5f + 0.45f * std::sin(position * 19.0f));
        lfo.setSample(2, sample, 0.5f + 0.48f * std::cos(position * 11.0f));
        lfo.setSample(3, sample, 1.0f - position);
    }

    return lfo;
}
} // namespace

TEST_CASE("HQ bands safely process host blocks larger than their prepared capacity",
          "[processor][dsp][oversized-block]")
{
    BandProcessor chunkedProcessor;
    BandProcessor singleBlockReference;

    chunkedProcessor.prepare({ testSampleRate,
                               static_cast<juce::uint32>(preparedBlockSize),
                               2 });
    singleBlockReference.prepare({ testSampleRate,
                                   static_cast<juce::uint32>(oversizedBlockSize),
                                   2 });

    auto chunked = makeInput();
    auto reference = makeInput();
    const auto lfoOutputs = makeLfoOutputs();
    const auto params = makeModulatedHqParameters();

    REQUIRE_NOTHROW(chunkedProcessor.process(chunked, params, lfoOutputs));
    REQUIRE_NOTHROW(singleBlockReference.process(reference, params, lfoOutputs));

    CHECK(chunkedProcessor.mSampleMaxValue.load(std::memory_order_relaxed)
          == Catch::Approx(0.95f));
    CHECK(chunkedProcessor.mReductionPercent.load(std::memory_order_relaxed)
          == Catch::Approx(singleBlockReference.mReductionPercent.load(std::memory_order_relaxed))
                 .margin(1.0e-6f));

    bool allSamplesAreFinite = true;
    float maximumError = 0.0f;
    int maximumErrorChannel = 0;
    int maximumErrorSample = 0;

    for (int channel = 0; channel < chunked.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < chunked.getNumSamples(); ++sample)
        {
            const float actual = chunked.getSample(channel, sample);
            const float expected = reference.getSample(channel, sample);
            allSamplesAreFinite = allSamplesAreFinite && std::isfinite(actual);

            const float error = std::abs(actual - expected);
            if (error > maximumError)
            {
                maximumError = error;
                maximumErrorChannel = channel;
                maximumErrorSample = sample;
            }
        }
    }

    CHECK(allSamplesAreFinite);
    INFO("maximum error at channel " << maximumErrorChannel
                                      << ", sample " << maximumErrorSample);
    CHECK(maximumError <= 2.0e-5f);
}
