#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <ctime>
#include <iostream>
#include <memory>

namespace
{
void setPhysicalParameter(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

TEST_CASE("Four physical tape bands render bounded audio in Base and HQ with a live macro route",
          "[analog-physical][tape][processor][multiband][hq][realtime]")
{
    constexpr double sampleRate = 48000;
    constexpr int blockSize = 64, blocks = 375;
    for (bool hq : {false, true})
    {
        CAPTURE(hq);
        auto processor = std::make_unique<FireAudioProcessor>();
        processor->hasUpdateCheckBeenPerformed = true;
        setPhysicalParameter(*processor, NUM_BANDS_ID, 4);
        setPhysicalParameter(*processor, HQ_ID, hq ? 1 : 0);
        setPhysicalParameter(*processor, FILTER_BYPASS_ID, 0);
        setPhysicalParameter(*processor, DOWNSAMPLE_BYPASS_ID, 0);
        setPhysicalParameter(*processor, "macro4", .6f);
        for (int band = 0; band < 4; ++band)
        {
            const auto setBand = [&](const char* id, float value)
            { setPhysicalParameter(*processor, ParameterIDAndName::getIDString(id, band), value); };
            setBand(BAND_ENABLE_ID, 1); setBand(BAND_SOLO_ID, 0);
            setBand(LINKED_ID, 0); setBand(SAFE_ID, 0); setBand(EXTREME_ID, 0);
            setBand(DRIVE_BYPASS_ID, 1); setBand(SHAPE_BYPASS_ID, 1);
            setBand(COMP_BYPASS_ID, 0); setBand(WIDTH_BYPASS_ID, 0);
            setBand(DRIVE_ID, 28); setBand(OUTPUT_ID, -6); setBand(MIX_ID, 1);
            REQUIRE(processor->setShapeMode(band + 1, -1, 23));
        }
        REQUIRE(processor->assignLfoToTarget(fire::mod_sources::firstMacro + 3, "drive4") == LfoManager::AssignmentResult::changed);
        processor->setModulationDepth("drive4", .1f);
        processor->prepareToPlay(sampleRate, blockSize);
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        float peak = 0;
        bool finite = true;
        const auto started = std::clock();
        for (int block = 0; block < blocks; ++block)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    const auto t = (block * blockSize + sample) / sampleRate;
                    audio.setSample(channel, sample, static_cast<float>(
                        .13 * std::sin(2 * 3.141592653589793 * 137 * t + channel * .3)
                        + .1 * std::sin(2 * 3.141592653589793 * 2131 * t)
                        + .07 * std::sin(2 * 3.141592653589793 * 8917 * t)));
                }
            processor->processBlock(audio, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    finite = finite && std::isfinite(audio.getSample(channel, sample));
                    peak = std::max(peak, std::abs(audio.getSample(channel, sample)));
                }
        }
        const auto cpuSeconds = static_cast<double>(std::clock() - started) / CLOCKS_PER_SEC;
        std::cout << "Four tape bands HQ=" << hq << ": 0.5 s audio, " << cpuSeconds << " s process CPU\n";
        CHECK(finite); CHECK(peak > .01f); CHECK(peak < 4);
        processor->releaseResources();
    }
}
