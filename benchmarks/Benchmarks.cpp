#include <chrono>
#include <iostream>
#include <numeric>
#include "../Source/Utility/BuildDiagnostics.h"
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
    static void invalidateRaster(SpectrumComponent& component) { component.spectrumRaster.dirty = true; }
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

TEST_CASE("CPU audit spectrum raster and cached presentation", "[cpu-ui]")
{
    SpectrumComponent spectrum {1, false};
    spectrum.setSize(1000, 300);
    SpectrumComponentTestAccess::prime(spectrum);
    SpectrumComponentTestAccess::rebuild(spectrum);
    for (int scale : {1, 2})
    {
        juce::Image target(juce::Image::ARGB, 1000 * scale, 300 * scale, true);
        const auto paint = [&]
        {
            target.clear(target.getBounds());
            juce::Graphics graphics(target);
            graphics.addTransform(juce::AffineTransform::scale(static_cast<float>(scale)));
            spectrum.paint(graphics);
        };
        paint();
        BENCHMARK("Spectrum cached paint / " + std::to_string(scale) + "x") { paint(); };
        BENCHMARK("Spectrum new-frame raster / " + std::to_string(scale) + "x")
        {
            SpectrumComponentTestAccess::invalidateRaster(spectrum);
            paint();
        };
        spectrum.setSpecAlpha(0);
        BENCHMARK("Spectrum transparent paint / " + std::to_string(scale) + "x") { paint(); };
        spectrum.setSpecAlpha(.8f);
    }
}

TEST_CASE("CPU audit stereo spectrum frame and transform", "[cpu-ui][cpu-spectrum]")
{
    SpectrumProcessor spectrum;
    std::array<float, SpectrumProcessor::fftSize> source {};
    std::array<float, SpectrumProcessor::fftBufferSize> wet {}, dry {};
    for (size_t sample = 0; sample < source.size(); ++sample)
        source[sample] = .2f * std::sin(float(sample) * .073f);
    for (const auto mode : { SpectrumProcessor::ChannelMode::left,
                             SpectrumProcessor::ChannelMode::right,
                             SpectrumProcessor::ChannelMode::stereoEnergy,
                             SpectrumProcessor::ChannelMode::mono,
                             SpectrumProcessor::ChannelMode::side })
    {
        const std::string name = mode == SpectrumProcessor::ChannelMode::left ? "Left"
                               : mode == SpectrumProcessor::ChannelMode::right ? "Right"
                               : mode == SpectrumProcessor::ChannelMode::stereoEnergy ? "Stereo energy"
                               : mode == SpectrumProcessor::ChannelMode::mono ? "Mono" : "Side";
        BENCHMARK("Spectrum full-frame capture + FFT / " + name)
        {
            for (const auto sample : source)
                spectrum.pushNextStereoSamplePairIntoFifo(sample, -sample, sample * .5f, sample * .3f);
            spectrum.popLatestMagnitudes(wet.data(), int(wet.size()), dry.data(), int(dry.size()), mode);
            return wet[24];
        };
    }
}

// Use repeatable input for each iteration. Feeding processed audio back into
// the next iteration measures a changing signal (often eventual silence).
TEST_CASE("CPU audit processing scenarios", "[cpu]")
{
    for (const int blockSize : { 128, 512 })
        for (const auto* scenario : { "default", "four bands HQ", "delay", "reverb", "OTT", "modulated delay",
                                     "four bands Transformer", "four bands Transformer HQ", "four bands Tape", "four bands Tape HQ" })
        {
            FireAudioProcessor plugin;
            const auto set = [&] (const juce::String& id, float value)
            {
                auto* parameter = plugin.treeState.getParameter(id);
                REQUIRE(parameter != nullptr);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            };
            const juce::String name(scenario);
            if (name.startsWith("four bands"))
            {
                set(NUM_BANDS_ID, 4);
                for (int divider = 0; divider < 3; ++divider)
                    set(ParameterIDAndName::getIDString(LINE_STATE_ID, divider), 1);
                set(HQ_ID, name.endsWith("HQ") ? 1 : 0);
            }
            if (name.contains("Transformer") || name.contains("Tape"))
            {
                set(FILTER_BYPASS_ID, 0); set(DOWNSAMPLE_BYPASS_ID, 0);
                for (int band = 0; band < 4; ++band)
                {
                    set(ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, band), 1);
                    set(ParameterIDAndName::getIDString(LINKED_ID, band), 0);
                    set(ParameterIDAndName::getIDString(SAFE_ID, band), 0);
                    set(ParameterIDAndName::getIDString(DRIVE_ID, band), 28);
                    set(ParameterIDAndName::getIDString(OUTPUT_ID, band), -6);
                    REQUIRE(plugin.setShapeMode(band + 1, -1, name.contains("Tape") ? 23 : 22));
                }
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

// Absolute timing varies by machine and scheduler. Record callback percentiles
// and compare saved reports on the same machine; only correctness is asserted.
TEST_CASE("CPU audit realtime budget and multiple instances", "[cpu-realtime]")
{
    using Clock = std::chrono::steady_clock;
    juce::Array<juce::var> rows;
    const bool extended = juce::SystemStats::getEnvironmentVariable("FIRE_CPU_EXTENDED", "0") == "1";
    const auto values = [](const char* key, const char* fallback)
    {
        juce::Array<int> result;
        for (const auto& token : juce::StringArray::fromTokens(
                juce::SystemStats::getEnvironmentVariable(key, fallback), ",; ", ""))
            if (token.isNotEmpty()) result.add(token.getIntValue());
        return result;
    };
    const auto rates = values("FIRE_CPU_RATES", extended ? "48000,96000,192000" : "48000,96000");
    const auto sizes = values("FIRE_CPU_BLOCKS", extended ? "32,64,128" : "64,128");
    const auto instances = values("FIRE_CPU_INSTANCES", "1,4");
    const auto modes = values("FIRE_CPU_MODES", "0,22,23");
    const int measuredCallbacks = juce::jlimit(64, 8192,
        juce::SystemStats::getEnvironmentVariable("FIRE_CPU_CALLBACKS", "512").getIntValue());
    for (const auto sampleRate : rates)
        for (const int blockSize : sizes)
            for (const int instanceCount : instances)
                for (const int mode : modes)
                {
                    REQUIRE(sampleRate >= 8000);
                    REQUIRE(sampleRate <= 384000);
                    REQUIRE(blockSize >= 1);
                    REQUIRE(blockSize <= 4096);
                    REQUIRE(instanceCount >= 1);
                    REQUIRE(instanceCount <= 16);
                    REQUIRE((mode == 0 || mode == 22 || mode == 23));
                    const juce::String scenario = mode == 0 ? "default" : mode == 22 ? "four bands Transformer HQ" : "four bands Tape HQ";
                    std::vector<std::unique_ptr<FireAudioProcessor>> plugins;
                    std::vector<std::unique_ptr<FireAudioProcessor::HistoryCaptureSubscription>> captures;
                    for (int instance = 0; instance < instanceCount; ++instance)
                    {
                        auto plugin = std::make_unique<FireAudioProcessor>();
                        const auto set = [&](const juce::String& id, float value)
                        {
                            auto* parameter = plugin->treeState.getParameter(id);
                            REQUIRE(parameter != nullptr);
                            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                        };
                        if (mode != 0)
                        {
                            set(NUM_BANDS_ID, 4);
                            for (int divider = 0; divider < 3; ++divider)
                                set(ParameterIDAndName::getIDString(LINE_STATE_ID, divider), 1);
                            set(HQ_ID, 1); set(FILTER_BYPASS_ID, 0); set(DOWNSAMPLE_BYPASS_ID, 0);
                            for (int band = 0; band < 4; ++band)
                            {
                                set(ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, band), 1);
                                set(ParameterIDAndName::getIDString(LINKED_ID, band), 0);
                                set(ParameterIDAndName::getIDString(SAFE_ID, band), 0);
                                set(ParameterIDAndName::getIDString(DRIVE_ID, band), 28);
                                set(ParameterIDAndName::getIDString(OUTPUT_ID, band), -6);
                                REQUIRE(plugin->setShapeMode(band + 1, -1, mode));
                            }
                        }
                        plugin->setRateAndBufferSizeDetails(sampleRate, blockSize);
                        plugin->prepareToPlay(sampleRate, blockSize);
                        captures.push_back(std::make_unique<FireAudioProcessor::HistoryCaptureSubscription>(*plugin));
                        plugins.push_back(std::move(plugin));
                    }
                    juce::AudioBuffer<float> source(2, blockSize), buffer(2, blockSize);
                    juce::MidiBuffer midi;
                    std::vector<double> elapsed(static_cast<size_t>(measuredCallbacks));
                    for (const bool analysis : { false, true })
                    {
                        for (size_t i = 0; i < plugins.size(); ++i)
                        {
                            plugins[i]->setSpectrumCaptureEnabled(analysis);
                            captures[i]->setActive(analysis);
                        }
                        double checksum = 0;
                        for (int callback = -128; callback < int(elapsed.size()); ++callback)
                        {
                            // Continuous multi-tone source; no processed feedback
                            // and no repeated block-boundary waveform discontinuity.
                            const auto startSample = (callback + 128) * blockSize;
                            for (int ch = 0; ch < 2; ++ch)
                                for (int sample = 0; sample < blockSize; ++sample)
                                {
                                    const auto time = double(startSample + sample) / sampleRate;
                                    source.setSample(ch, sample, float(.12 * std::sin(juce::MathConstants<double>::twoPi * (110 + 63 * ch) * time)
                                                                      + .05 * std::sin(juce::MathConstants<double>::twoPi * 997 * time)));
                                }
                            const auto start = Clock::now();
                            for (auto& plugin : plugins)
                            {
                                buffer.makeCopyOf(source, true);
                                plugin->processBlock(buffer, midi);
                                checksum += buffer.getSample(0, blockSize - 1);
                            }
                            if (callback >= 0)
                                elapsed[size_t(callback)] = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
                        }
                        REQUIRE(std::isfinite(checksum));
                        std::sort(elapsed.begin(), elapsed.end());
                        const auto mean = std::accumulate(elapsed.begin(), elapsed.end(), 0.0) / double(elapsed.size());
                        const auto budget = double(blockSize) / sampleRate * 1.0e6;
                        auto* row = new juce::DynamicObject();
                        row->setProperty("scenario", scenario);
                        row->setProperty("sample_rate", sampleRate);
                        row->setProperty("block_size", blockSize);
                        row->setProperty("instances_serial", instanceCount);
                        row->setProperty("analysis_capture", analysis);
                        row->setProperty("callbacks", int(elapsed.size()));
                        row->setProperty("mean_us", mean);
                        row->setProperty("p50_us", elapsed[elapsed.size() / 2]);
                        row->setProperty("p95_us", elapsed[elapsed.size() * 95 / 100]);
                        row->setProperty("p99_us", elapsed[elapsed.size() * 99 / 100]);
                        row->setProperty("max_us", elapsed.back());
                        row->setProperty("mean_budget_percent", mean / budget * 100.0);
                        row->setProperty("p99_budget_percent", elapsed[elapsed.size() * 99 / 100] / budget * 100.0);
                        rows.add(juce::var(row));
                    }
                }
    auto* report = new juce::DynamicObject();
    report->setProperty("timestamp", juce::Time::getCurrentTime().toISO8601(true));
    report->setProperty("version", juce::String(VERSION));
    report->setProperty("build_identifier", fire::diagnostics::buildIdentifier());
    report->setProperty("architecture", fire::diagnostics::architecture());
    report->setProperty("extended_profile", extended);
    report->setProperty("cpu", juce::SystemStats::getCpuModel());
    report->setProperty("os", juce::SystemStats::getOperatingSystemName());
    report->setProperty("logical_cpus", juce::SystemStats::getNumCpus());
    report->setProperty("note", "Offline serial callback wall time, not DAW CPU or dropouts. Analysis includes FIFO/history capture only; UI FFT/render time is measured separately by cpu-ui benchmarks.");
    report->setProperty("rows", juce::var(rows));
    const juce::var result(report);
    const auto path = juce::SystemStats::getEnvironmentVariable("FIRE_CPU_REPORT_PATH", "");
    if (path.isNotEmpty()) REQUIRE(juce::File(path).replaceWithText(juce::JSON::toString(result)));
    else std::cout << juce::JSON::toString(result).toStdString() << std::endl;
}
