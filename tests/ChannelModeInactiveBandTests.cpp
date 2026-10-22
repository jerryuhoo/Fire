#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>

namespace
{
void hiddenModeSet(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void prepareHiddenModeFixture(FireAudioProcessor& processor, bool hq)
{
    auto layout = processor.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
    layout.inputBuses.set(1, juce::AudioChannelSet::disabled());
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    REQUIRE(processor.setBusesLayout(layout));
    hiddenModeSet(processor, NUM_BANDS_ID, 1);
    hiddenModeSet(processor, HQ_ID, hq ? 1 : 0);
    hiddenModeSet(processor, FILTER_BYPASS_ID, 0);
    hiddenModeSet(processor, DOWNSAMPLE_BYPASS_ID, 0);
    hiddenModeSet(processor, OUTPUT_ID, 0);
    hiddenModeSet(processor, MIX_ID, 1);
    for (const auto* id : {DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, COMP_BYPASS_ID,
                          WIDTH_BYPASS_ID, OTT_ENABLED_ID, DC_FILTER_ID, LINKED_ID})
        hiddenModeSet(processor, ParameterIDAndName::getIDString(id, 0), 0);
    hiddenModeSet(processor, ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0), 1);
    hiddenModeSet(processor, ParameterIDAndName::getIDString(MIX_ID, 0), 1);
    hiddenModeSet(processor, ParameterIDAndName::getIDString(OUTPUT_ID, 0), 0);
    REQUIRE(processor.addInsertEffect(1, fire::effects::Type::delay) == 0);
    const std::array<float, 6> controls {60, 72, 12000, 0, 0, 100};
    for (size_t field = 0; field < controls.size(); ++field)
        hiddenModeSet(processor, fire::effects::parameterID(1, 0, static_cast<int>(field)),
            fire::effects::controls(fire::effects::Type::delay)[field].toNormalised(controls[field]));
    processor.setRateAndBufferSizeDetails(48000, 128);
    processor.prepareToPlay(48000, 128);
}
}

TEST_CASE("Automating a hidden band channel mode cannot mute the active band or clear its delay tail",
          "[channel-mode][dsp][topology]")
{
    for (bool hq : {false, true})
    {
        INFO("HQ " << hq);
        FireAudioProcessor edited, reference;
        prepareHiddenModeFixture(edited, hq); prepareHiddenModeFixture(reference, hq);
        juce::AudioBuffer<float> actual(2, 128), expected(2, 128);
        juce::MidiBuffer midi;
        double tailEnergy = 0;
        for (int block = 0; block < 140; ++block)
        {
            // Exercise hidden automation both under a steady signal and after
            // the source stops, when a destructive reset would erase a tail.
            if (block == 64 || block == 104) hiddenModeSet(edited, "bandChannelMode4", 1);
            if (block == 72 || block == 112) hiddenModeSet(edited, "bandChannelMode4", 2);
            if (block == 80 || block == 120) hiddenModeSet(edited, "bandChannelMode4", 0);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 128; ++sample)
                {
                    const auto value = block < 100
                        ? .24f * std::sin(static_cast<float>(block * 128 + sample) * (.031f + .012f * channel)) : 0.0f;
                    actual.setSample(channel, sample, value);
                    expected.setSample(channel, sample, value);
                }
            edited.processBlock(actual, midi); reference.processBlock(expected, midi);
            REQUIRE(edited.getLatencySamples() == reference.getLatencySamples());
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 128; ++sample)
                {
                    REQUIRE(std::isfinite(actual.getSample(channel, sample)));
                    CHECK(actual.getSample(channel, sample) == expected.getSample(channel, sample));
                    if (block >= 104)
                    {
                        const auto value = expected.getSample(channel, sample);
                        tailEnergy += static_cast<double>(value) * value;
                    }
                }
        }
        // Assert that this actually exercised a surviving delayed signal,
        // rather than allowing a pair of silent fixtures to pass trivially.
        CHECK(tailEnergy > 1.0);
        edited.releaseResources(); reference.releaseResources();
    }
}
