#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 64;
constexpr float driveAmount = 20.0f;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::String firstBandParameter(const juce::String& baseID)
{
    return ParameterIDAndName::getIDString(baseID, 0);
}

DistortionGraphValues processAndReadGraph(FireAudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    DistortionGraphValues values;
    REQUIRE(processor.getLatestDistortionGraphValues(values));
    return values;
}
} // namespace

TEST_CASE("Distortion graph applies the Extreme Drive multiplier",
          "[processor][graph-telemetry][drive][extreme]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.setUiFocusBand(0);

    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, firstBandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(SAFE_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DRIVE_ID), driveAmount);
    setPlainParameter(processor, firstBandParameter(EXTREME_ID), 0.0f);
    processor.prepareToPlay(sampleRate, blockSize);

    const float normalDriveForCalc = driveAmount * 6.5f / 100.0f;
    const auto normal = processAndReadGraph(processor);
    CHECK(normal.drive
          == Catch::Approx(std::exp2(normalDriveForCalc)).margin(1.0e-6f));

    setPlainParameter(processor, firstBandParameter(EXTREME_ID), 1.0f);
    const auto extreme = processAndReadGraph(processor);
    const float expectedExtremeDrive = std::exp2(std::log2(10.0f)
                                                  * normalDriveForCalc);
    CHECK(extreme.drive
          == Catch::Approx(expectedExtremeDrive).margin(1.0e-5f));
    CHECK(extreme.drive > normal.drive * 8.0f);
}
