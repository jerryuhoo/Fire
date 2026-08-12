#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int callbackSize = preparedBlockSize * 4 + 1;
constexpr int settlingCallbacks = 16;
constexpr float callbackPeak = 0.95f;

BandProcessingParameters makeSafeDriveParameters(bool useHq)
{
    BandProcessingParameters params;
    params.mode = 4; // Hard clip
    params.isHQ = useHq;
    params.isDriveEnabled = true;
    params.isSafeModeOn = true;
    params.isExtremeModeOn = false;
    params.isShapeEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = false;
    params.isDcFilterEnabled = false;
    params.driveVal.baseValue = 100.0f;
    params.biasVal.baseValue = 0.0f;
    params.recVal.baseValue = 0.0f;
    params.outputVal.baseValue = 0.0f;
    params.mixVal = 1.0f;
    params.shapeMixVal = 1.0f;
    return params;
}

juce::AudioBuffer<float> makeStereoProbe(int activeChannel, int peakSample)
{
    juce::AudioBuffer<float> buffer(2, callbackSize);
    buffer.clear();

    for (int sample = 0; sample < callbackSize; ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi
                           * 997.0 * static_cast<double>(sample) / sampleRate;
        buffer.setSample(activeChannel,
                         sample,
                         0.08f * static_cast<float>(std::sin(phase)));
    }

    buffer.setSample(activeChannel, peakSample, callbackPeak);
    return buffer;
}

struct SafeDriveSnapshot
{
    float sampleMax = 0.0f;
    float reduction = 1.0f;
    bool outputIsFinite = true;
};

SafeDriveSnapshot runSafeDriveProbe(bool useHq, int activeChannel, int peakSample)
{
    BandProcessor processor;
    processor.prepare({ sampleRate,
                        static_cast<juce::uint32>(preparedBlockSize),
                        2 });

    const auto params = makeSafeDriveParameters(useHq);
    juce::AudioBuffer<float> lfoOutputs(4, callbackSize);
    lfoOutputs.clear();

    SafeDriveSnapshot snapshot;
    for (int callback = 0; callback < settlingCallbacks; ++callback)
    {
        auto audio = makeStereoProbe(activeChannel, peakSample);
        processor.process(audio, params, lfoOutputs);

        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                snapshot.outputIsFinite = snapshot.outputIsFinite
                                          && std::isfinite(audio.getSample(channel, sample));
    }

    snapshot.sampleMax = processor.mSampleMaxValue.load(std::memory_order_relaxed);
    snapshot.reduction = processor.mReductionPercent.load(std::memory_order_relaxed);
    return snapshot;
}
} // namespace

TEST_CASE("Safe Drive uses the callback peak independently of stereo channel order",
          "[processor][band][safe-drive][stereo]")
{
    for (const bool useHq : std::array { false, true })
    {
        CAPTURE(useHq);

        // The callback is deliberately larger than the prepared capacity, and
        // its only full-scale peak is in the final one-sample internal chunk.
        // Safe Drive must scan the complete host callback, not channel zero or
        // only the first internal chunk.
        const auto leftPeakAtEnd = runSafeDriveProbe(useHq, 0, callbackSize - 1);
        const auto rightPeakAtEnd = runSafeDriveProbe(useHq, 1, callbackSize - 1);
        const auto leftPeakAtStart = runSafeDriveProbe(useHq, 0, 0);

        CHECK(leftPeakAtEnd.outputIsFinite);
        CHECK(rightPeakAtEnd.outputIsFinite);
        CHECK(leftPeakAtStart.outputIsFinite);

        CHECK(leftPeakAtEnd.sampleMax
              == Catch::Approx(callbackPeak).margin(1.0e-6f));
        CHECK(rightPeakAtEnd.sampleMax
              == Catch::Approx(leftPeakAtEnd.sampleMax).margin(1.0e-6f));
        CHECK(leftPeakAtStart.sampleMax
              == Catch::Approx(leftPeakAtEnd.sampleMax).margin(1.0e-6f));

        REQUIRE(leftPeakAtEnd.reduction < 0.99f);
        CHECK(rightPeakAtEnd.reduction < 0.99f);
        CHECK(rightPeakAtEnd.reduction
              == Catch::Approx(leftPeakAtEnd.reduction).margin(1.0e-5f));
        CHECK(leftPeakAtStart.reduction
              == Catch::Approx(leftPeakAtEnd.reduction).margin(1.0e-5f));
    }
}
