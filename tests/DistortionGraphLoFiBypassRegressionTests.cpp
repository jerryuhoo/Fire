#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 64;
constexpr float rateDivide = 9.5f;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
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

TEST_CASE("Distortion graph reports Lo-Fi rate only while the stage is enabled",
          "[processor][graph-telemetry][lofi][bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.setUiFocusBand(0);

    setPlainParameter(processor, DOWNSAMPLE_ID, rateDivide);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    processor.prepareToPlay(sampleRate, blockSize);

    const auto disabled = processAndReadGraph(processor);
    CHECK(disabled.rateDivide == Catch::Approx(1.0f).margin(1.0e-6f));

    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 1.0f);
    const auto enabled = processAndReadGraph(processor);
    CHECK(enabled.rateDivide == Catch::Approx(rateDivide).margin(1.0e-6f));
}
