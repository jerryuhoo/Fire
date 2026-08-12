#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 64;

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
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel,
                             sample,
                             0.01f * std::sin(0.13f * static_cast<float>(sample)));

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    DistortionGraphValues values;
    REQUIRE(processor.getLatestDistortionGraphValues(values));
    return values;
}
} // namespace

TEST_CASE("Distortion graph reports unity drive while the Drive stage is disabled",
          "[processor][graph-telemetry][drive][bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.setUiFocusBand(0);

    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, firstBandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, firstBandParameter(SAFE_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(EXTREME_ID), 0.0f);
    setPlainParameter(processor, firstBandParameter(DRIVE_ID), 100.0f);
    setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 0.0f);
    processor.prepareToPlay(sampleRate, blockSize);

    const auto disabled = processAndReadGraph(processor);
    CHECK(disabled.drive == Catch::Approx(1.0f).margin(1.0e-6f));

    setPlainParameter(processor, firstBandParameter(DRIVE_BYPASS_ID), 1.0f);
    const auto enabled = processAndReadGraph(processor);
    const float expectedEnabledDrive = std::exp2(6.5f);
    CHECK(enabled.drive
          == Catch::Approx(expectedEnabledDrive).margin(1.0e-5f));
}
