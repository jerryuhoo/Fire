#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 64;
constexpr float inputPeak = 3.0f;

struct ReductionSnapshot
{
    float sampleMax = 0.0f;
    float reduction = 1.0f;
};

ReductionSnapshot processSafeDrive(bool driveEnabled)
{
    BandProcessor processor;
    processor.prepare({ sampleRate,
                        static_cast<juce::uint32>(blockSize),
                        2 });

    BandProcessingParameters params;
    params.mode = 4; // hard clip
    params.isDriveEnabled = driveEnabled;
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

    juce::AudioBuffer<float> audio(2, blockSize);
    audio.clear();
    audio.setSample(0, blockSize / 2, inputPeak);
    juce::AudioBuffer<float> lfoOutputs(4, blockSize);
    lfoOutputs.clear();
    processor.process(audio, params, lfoOutputs);

    return {
        processor.mSampleMaxValue.load(std::memory_order_relaxed),
        processor.mReductionPercent.load(std::memory_order_relaxed)
    };
}
} // namespace

TEST_CASE("Safe Drive reduction meter respects Drive bypass",
          "[processor][band][safe-drive][meter][bypass]")
{
    const auto disabled = processSafeDrive(false);
    const auto enabled = processSafeDrive(true);

    REQUIRE(disabled.sampleMax
            == Catch::Approx(inputPeak).margin(1.0e-6f));
    REQUIRE(enabled.sampleMax
            == Catch::Approx(inputPeak).margin(1.0e-6f));

    CHECK(std::isfinite(disabled.reduction));
    CHECK(disabled.reduction == Catch::Approx(1.0f).margin(1.0e-6f));

    // The enabled control proves the same signal is strong enough to engage
    // Safe, rather than merely producing unity because no reduction was needed.
    CHECK(std::isfinite(enabled.reduction));
    CHECK(enabled.reduction > 0.0f);
    CHECK(enabled.reduction < 0.99f);
}
