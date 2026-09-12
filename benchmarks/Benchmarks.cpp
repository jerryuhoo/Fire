#include "../Source/DSP/LfoManager.h"
#include "../Source/DSP/ModulationRouting.h"
#include "../Source/Panels/SpectrogramPanel/SpectrumComponent.h"

struct SpectrumComponentTestAccess
{
    static void prime(SpectrumComponent& component)
    {
        component.renderedDataIsClear = false;
        for (size_t i = 0; i < component.displayData.size(); ++i)
            component.displayData[i] = 3.0f + std::sin(static_cast<float>(i) * 0.11f);
    }
    static void rebuild(SpectrumComponent& component) { component.rebuildPaths(); }
};

TEST_CASE("CPU audit spectrum geometry", "[cpu-ui]")
{
    SpectrumComponent spectrum { 1, false };
    spectrum.setSize(1000, 300);
    SpectrumComponentTestAccess::prime(spectrum);
    SpectrumComponentTestAccess::rebuild(spectrum);
    BENCHMARK("Spectrum geometry / 1024 bins")
    {
        SpectrumComponentTestAccess::rebuild(spectrum);
    };
}

// Use repeatable input for each iteration. Feeding processed audio back into
// the next iteration measures a changing signal (often eventual silence).
TEST_CASE("CPU audit processing scenarios", "[cpu]")
{
    for (const int blockSize : { 128, 512 })
        for (const auto* scenario : { "default", "four bands HQ", "delay", "reverb", "OTT", "modulated delay" })
        {
            FireAudioProcessor plugin;
            const auto set = [&] (const juce::String& id, float value)
            {
                auto* parameter = plugin.treeState.getParameter(id);
                REQUIRE(parameter != nullptr);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            };
            const juce::String name(scenario);
            if (name == "four bands HQ")
            {
                set(NUM_BANDS_ID, 4);
                for (int divider = 0; divider < 3; ++divider)
                    set(ParameterIDAndName::getIDString(LINE_STATE_ID, divider), 1);
                set(HQ_ID, 1);
            }
            if (name.contains("delay") || name == "reverb")
            {
                const auto type = name == "reverb" ? fire::effects::Type::reverb : fire::effects::Type::delay;
                REQUIRE(plugin.addInsertEffect(0, type) == 0);
                if (name == "modulated delay")
                {
                    LfoData shape;
                    shape.points = { { 0.0f, 0.0f }, { 0.5f, 1.0f }, { 1.0f, 0.0f } };
                    shape.curvatures = { 0.0f, 0.0f };
                    plugin.getLfoManager().setLfoData(0, shape);
                    set(ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0), 0);
                    set(ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0), 3);
                    REQUIRE(plugin.assignLfoToTarget(0, fire::effects::parameterID(0, 0, 2))
                            == LfoManager::AssignmentResult::changed);
                }
            }
            if (name == "OTT")
                set(ParameterIDAndName::getIDString(OTT_ENABLED_ID, 0), 1);

            plugin.setRateAndBufferSizeDetails(48000.0, blockSize);
            plugin.prepareToPlay(48000.0, blockSize);
            juce::AudioBuffer<float> source(2, blockSize), buffer(2, blockSize);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < blockSize; ++sample)
                    source.setSample(channel, sample,
                        0.2f * std::sin(static_cast<float>(sample) * 0.057f + static_cast<float>(channel) * 0.3f));
            juce::MidiBuffer midi;
            const auto process = [&]
            {
                buffer.makeCopyOf(source, true);
                plugin.processBlock(buffer, midi);
                return buffer.getSample(0, blockSize - 1);
            };
            for (int warmup = 0; warmup < 256; ++warmup)
                process();
            BENCHMARK((name + " / " + juce::String(blockSize)).toStdString()) { return process(); };
        }
}

TEST_CASE("Boot performance")
{
    BENCHMARK_ADVANCED("Processor constructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::storage_for<FireAudioProcessor>> storage(size_t(meter.runs()));
        meter.measure([&](int i)
                      { storage[(size_t) i].construct(); });
    };

    BENCHMARK_ADVANCED("Processor destructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::destructable_object<FireAudioProcessor>> storage(size_t(meter.runs()));
        for (auto& s : storage)
            s.construct();
        meter.measure([&](int i)
                      { storage[(size_t) i].destruct(); });
    };

    BENCHMARK_ADVANCED("Editor open and close")
    (Catch::Benchmark::Chronometer meter)
    {
        FireAudioProcessor plugin;

        // due to complex construction logic of the editor, let's measure open/close together
        meter.measure([&](int /* i */)
                      {
            auto editor = plugin.createEditorIfNeeded();
            plugin.editorBeingDeleted (editor);
            delete editor;
            return plugin.getActiveEditor(); });
    };
}
