#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <complex>
#include <vector>

namespace
{
void setShapeParameter(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
double toneMagnitude(const std::vector<float>& audio, double frequency)
{
    std::complex<double> value{};
    for (size_t sample = 0; sample < audio.size(); ++sample)
        value += static_cast<double>(audio[sample]) * std::polar(1.0,
            -juce::MathConstants<double>::twoPi * frequency * static_cast<double>(sample) / 48000);
    return std::abs(value) * 2 / static_cast<double>(audio.size());
}
std::vector<float> renderIndependentShape(int scope, bool hq)
{
    auto owner = std::make_unique<FireAudioProcessor>(); auto& p = *owner;
    setShapeParameter(p, HQ_ID, hq ? 1 : 0);
    setShapeParameter(p, FILTER_BYPASS_ID, 0);
    setShapeParameter(p, DOWNSAMPLE_BYPASS_ID, 0);
    for (const auto* id : {DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, COMP_BYPASS_ID, WIDTH_BYPASS_ID, OTT_ENABLED_ID})
        setShapeParameter(p, ParameterIDAndName::getIDString(id, 0), 0);
    const int slot = p.addInsertEffect(scope, fire::effects::Type::shape);
    REQUIRE(slot == 0);
    REQUIRE(p.setShapeMode(scope, slot, 12));
    setShapeParameter(p, fire::analog_params::driveID(scope, slot), 30);
    setShapeParameter(p, fire::effects::parameterID(scope, slot, 5), 1);
    p.setRateAndBufferSizeDetails(48000, 128); p.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> buffer(2, 128); juce::MidiBuffer midi;
    std::vector<float> result; result.reserve(8192);
    for (int offset = 0; offset < 40960; offset += 128)
    {
        for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < 128; ++sample)
            buffer.setSample(channel, sample, .7f * static_cast<float>(std::sin(
                juce::MathConstants<double>::twoPi * 9000 * (offset + sample) / 48000)));
        p.processBlock(buffer, midi);
        if (offset >= 32768) result.insert(result.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + 128);
    }
    return result;
}
std::vector<float> renderRoutedShape(int source, int control, int partition, bool hq, float mix = 70)
{
    constexpr int frames = 4096;
    fire::effects::InsertEffect effect; effect.prepare({48000, 64, 2});
    fire::effects::InsertEffect::Parameters p(fire::effects::Type::shape);
    p.shapeModel = 1; p.highQuality = hq; p.fixedShapeLatency = true;
    p.analogDrive.baseValue = 20; p.analogDrive.range = {0, 100};
    p.values[1].baseValue = .05f; p.values[2].baseValue = .1f; p.values[5].baseValue = mix;
    std::vector<float> signal(frames);
    for (int sample = 0; sample < frames; ++sample) signal[static_cast<size_t>(sample)] = .5f + .45f * std::sin(sample * .005f);
    std::array<int, 6> indices{-1, -1, -1, -1, -1, -1};
    if (control == 0)
    {
        p.analogDrive.lfoSignal = signal.data(); p.analogDrive.modulationDepth = .12f;
        p.analogDriveSource = source;
    }
    else if (control > 0)
    {
        p.values[static_cast<size_t>(control)].lfoSignal = signal.data();
        p.values[static_cast<size_t>(control)].modulationDepth = .12f;
        indices[static_cast<size_t>(control)] = source;
    }
    std::vector<float> output; output.reserve(frames);
    for (int offset = 0; offset < frames;)
    {
        const int count = std::min(partition, frames - offset);
        juce::AudioBuffer<float> buffer(2, count);
        for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < count; ++sample)
            buffer.setSample(channel, sample, .18f * std::sin((offset + sample) * .081f + channel * .3f));
        effect.process(juce::dsp::AudioBlock<float>(buffer), p, offset, &indices);
        output.insert(output.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + count);
        offset += count;
    }
    return output;
}
}

TEST_CASE("Independent Master and Band Shape apply HQ antialiasing to analogue colour",
          "[independent-shape-hq][hq][shared-modules][dsp][aliasing]")
{
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        const auto base = renderIndependentShape(scope, false), hq = renderIndependentShape(scope, true);
        const auto baseAlias = toneMagnitude(base, 21000) / toneMagnitude(base, 9000);
        const auto hqAlias = toneMagnitude(hq, 21000) / toneMagnitude(hq, 9000);
        CAPTURE(baseAlias, hqAlias);
        CHECK(20 * std::log10(baseAlias / hqAlias) > 25);
    }
}

TEST_CASE("Independent HQ Shape retains Envelope Macro and Mix routes across callback partitions",
          "[independent-shape-hq][hq][shared-modules][modulation][oversized-block]")
{
    for (int control : {0, 1, 2, 5})
    {
        CAPTURE(control);
        const auto reference = renderRoutedShape(0, control, 64, true);
        for (int source = fire::mod_sources::envelope; source < fire::mod_sources::sourceCount; ++source)
        {
            CAPTURE(source);
            const auto actual = renderRoutedShape(source, control, 257, true);
            double error = 0;
            for (size_t sample = 0; sample < reference.size(); ++sample)
                error = std::max(error, static_cast<double>(std::abs(reference[sample] - actual[sample])));
            CHECK(error < 1e-6);
        }
    }
}

TEST_CASE("Independent Shape Mix combines aligned dry and wet signals in Base and HQ",
          "[independent-shape-hq][hq][shared-modules][mix][latency]")
{
    for (bool hq : {false, true})
    {
        CAPTURE(hq);
        const auto dry = renderRoutedShape(0, -1, 64, hq, 0);
        const auto wet = renderRoutedShape(0, -1, 64, hq, 100);
        const auto mixed = renderRoutedShape(0, -1, 64, hq, 50);
        double error = 0;
        for (size_t sample = 0; sample < dry.size(); ++sample)
            error = std::max(error, static_cast<double>(std::abs(mixed[sample] - .5f * (dry[sample] + wet[sample]))));
        CHECK(error < 1e-6);
    }
}

TEST_CASE("Independent Shape insertion removal and HQ automation keep a constant PDC budget",
          "[independent-shape-hq][hq][shared-modules][processor][latency][state]")
{
    FireAudioProcessor p;
    p.setRateAndBufferSizeDetails(48000, 64); p.prepareToPlay(48000, 64);
    const int latency = p.getLatencySamples();
    juce::dsp::Oversampling<float> legacy(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    juce::dsp::Oversampling<float> insert(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);
    legacy.initProcessing(64); insert.initProcessing(64);
    CHECK(latency == juce::roundToInt(legacy.getLatencyInSamples())
        + 2 * fire::effects::slotCount * juce::roundToInt(insert.getLatencyInSamples()));
    juce::AudioBuffer<float> buffer(2, 64); juce::MidiBuffer midi;
    for (int scope : {0, 1}) for (int slot = 0; slot < fire::effects::slotCount; ++slot)
    {
        REQUIRE(p.addInsertEffect(scope, fire::effects::Type::shape) == slot);
        REQUIRE(p.setShapeMode(scope, slot, 12));
        setShapeParameter(p, HQ_ID, slot % 2 == 0 ? 1 : 0);
        for (int callback = 0; callback < 8; ++callback) {buffer.clear(); p.processBlock(buffer, midi);}
        CHECK(p.getLatencySamples() == latency);
    }
    for (int scope : {0, 1}) for (int slot = 0; slot < fire::effects::slotCount; ++slot)
    {
        p.removeInsertEffect(scope, slot);
        buffer.clear(); p.processBlock(buffer, midi);
        CHECK(p.getLatencySamples() == latency);
    }
}

TEST_CASE("Live HQ changes in an independent analogue Shape settle on the matching static audio",
          "[independent-shape-hq][hq][transition][processor][modulation]")
{
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        const auto render = [scope](bool startHq, bool toggle)
        {
            auto p = std::make_unique<FireAudioProcessor>();
            setShapeParameter(*p, HQ_ID, startHq ? 1 : 0);
            setShapeParameter(*p, FILTER_BYPASS_ID, 0);
            setShapeParameter(*p, DOWNSAMPLE_BYPASS_ID, 0);
            for (const auto* id : {DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, COMP_BYPASS_ID, WIDTH_BYPASS_ID, OTT_ENABLED_ID})
                setShapeParameter(*p, ParameterIDAndName::getIDString(id, 0), 0);
            REQUIRE(p->addInsertEffect(scope, fire::effects::Type::shape) == 0);
            REQUIRE(p->setShapeMode(scope, 0, 12));
            setShapeParameter(*p, fire::analog_params::driveID(scope, 0), 20);
            setShapeParameter(*p, fire::effects::parameterID(scope, 0, 5), 1);
            p->setRateAndBufferSizeDetails(48000, 64); p->prepareToPlay(48000, 64);
            juce::AudioBuffer<float> buffer(2, 64); juce::MidiBuffer midi;
            std::vector<float> audio;
            for (int offset = 0; offset < 32768; offset += 64)
            {
                if (toggle && offset == 8192) setShapeParameter(*p, HQ_ID, startHq ? 0 : 1);
                for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < 64; ++sample)
                    buffer.setSample(channel, sample, .2f * std::sin((offset + sample) * .071f));
                p->processBlock(buffer, midi);
                if (offset >= 24576) audio.insert(audio.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + 64);
            }
            return audio;
        };
        for (bool start : {false, true})
        {
            CAPTURE(start);
            const auto reference = render(!start, false), actual = render(start, true);
            double error = 0;
            for (size_t sample = 0; sample < actual.size(); ++sample)
            {
                REQUIRE(std::isfinite(actual[sample]));
                error = std::max(error, static_cast<double>(std::abs(actual[sample] - reference[sample])));
            }
            CHECK(error < 2e-4);
        }
    }
}
