#include <DSP/EnvelopeDetectors.h>
#include <DSP/AuxiliaryModulation.h>
#include <PluginProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <limits>

namespace
{
using Detectors = fire::dsp::EnvelopeDetectors;
namespace ep = fire::envelope_params;
namespace ms = fire::mod_sources;
constexpr double detectorRate = 48000;
constexpr std::array<float, 3> detectorCrossovers {1000, 4000, 10000};
float detectorTone(int sample, double frequency, float amplitude = .5f)
{ return amplitude * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * frequency * sample / detectorRate)); }
void detectorSet(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

TEST_CASE("Detector bank keeps the original bypassed main envelope bit exact", "[envelope-bank][dsp][legacy]")
{
    Detectors bank; bank.prepare(detectorRate);
    fire::dsp::AuxiliaryModulation legacy; legacy.prepare(detectorRate);
    auto parameters = ms::defaults;
    parameters[0] = .7f; parameters[1] = 115; parameters[2] = 7;
    auto configuration = Detectors::Configurations{};
    configuration[0].attack = parameters[0]; configuration[0].release = parameters[1]; configuration[0].sensitivity = parameters[2];
    bank.setParameters(configuration, 4, detectorCrossovers);
    legacy.setParameters(parameters);
    for (int sample = 0; sample < 12000; ++sample)
    {
        const float left = sample < 6000 ? detectorTone(sample, 137) : 0;
        const float right = sample < 6000 ? -left : 0;
        const bool stereo = sample < 3000 || sample >= 6000;
        const auto value = bank.next({left, right, stereo}, {});
        CHECK(value.filtered[0].left == left);
        CHECK(value.filtered[0].right == (stereo ? right : left));
        CHECK(value.levels[0] == legacy.next(left, right, stereo)[0]);
    }
}

TEST_CASE("Four envelopes retain independent sources timing and sensitivity", "[envelope-bank][dsp]")
{
    Detectors bank; bank.prepare(detectorRate);
    auto configuration = Detectors::Configurations{};
    for (auto& settings : configuration) settings.attack = .1f;
    configuration[1].source = ep::externalInput;
    configuration[2].attack = 500;
    configuration[3].sensitivity = 6;
    bank.setParameters(configuration, 1, detectorCrossovers);
    Detectors::Result value;
    for (int sample = 0; sample < 4800; ++sample) value = bank.next({.25f, -.25f, true}, {.6f, -.6f, true});
    CHECK(value.levels[0] == Catch::Approx(.25f));
    CHECK(value.levels[1] == Catch::Approx(.6f));
    CHECK(value.levels[2] == Catch::Approx(.25 * (1 - std::exp(-.2))).margin(.00001));
    CHECK(value.levels[3] == Catch::Approx(.25 * std::pow(10., .3)).margin(.00001));
    for (int sample = 0; sample < 48000; ++sample) value = bank.next({.25f, -.25f, true}, {});
    CHECK(value.levels[1] < .005f); // Disconnected sidechain does not fall back to main.
    CHECK(value.levels[0] == Catch::Approx(.25f));
}

TEST_CASE("Band detector taps follow crossover regions and leave inactive bands silent", "[envelope-bank][dsp][bands]")
{
    for (double frequency : {100., 8000.})
    {
        Detectors bank; bank.prepare(detectorRate);
        auto configuration = Detectors::Configurations{};
        for (size_t index = 0; index < configuration.size(); ++index)
        { configuration[index].source = ep::band1 + static_cast<int>(index); configuration[index].attack = .1f; }
        bank.setParameters(configuration, 2, detectorCrossovers);
        std::array<double, 4> energy {};
        for (int sample = 0; sample < 12000; ++sample)
        {
            const auto signal = detectorTone(sample, frequency);
            const auto result = bank.next({signal, -signal, true}, {});
            if (sample >= 6000)
                for (size_t index = 0; index < energy.size(); ++index)
                    energy[index] += result.filtered[index].left * result.filtered[index].left;
            CHECK(result.filtered[2].left == 0); CHECK(result.filtered[3].left == 0);
        }
        if (frequency < detectorCrossovers[0]) CHECK(energy[0] > 10000 * energy[1]);
        else CHECK(energy[1] > 10000 * energy[0]);
        CHECK(energy[2] == 0); CHECK(energy[3] == 0);
    }
}

TEST_CASE("Detector filters reject unwanted frequencies without stereo cancellation", "[envelope-bank][dsp][filters]")
{
    Detectors bank; bank.prepare(detectorRate);
    auto configuration = Detectors::Configurations{};
    configuration[0].highPassEnabled = true; configuration[0].highPassFrequency = 2000;
    configuration[1].lowPassEnabled = true; configuration[1].lowPassFrequency = 500;
    configuration[2] = configuration[0]; configuration[3] = configuration[1];
    configuration[2].source = configuration[3].source = ep::externalInput;
    bank.setParameters(configuration, 1, detectorCrossovers);
    std::array<double, 4> energy {};
    Detectors::Result result;
    for (int sample = 0; sample < 18000; ++sample)
    {
        const auto low = detectorTone(sample, 100), high = detectorTone(sample, 8000);
        result = bank.next({low, -low, true}, {high, -high, true});
        if (sample > 9000)
            for (size_t index = 0; index < energy.size(); ++index)
            {
                energy[index] += result.filtered[index].left * result.filtered[index].left;
                CHECK(result.filtered[index].right == -result.filtered[index].left);
            }
    }
    CHECK(energy[1] > energy[0] * 1000);
    CHECK(energy[2] > energy[3] * 1000);
    CHECK(result.levels[1] > .2f); CHECK(result.levels[2] > .2f);
}

TEST_CASE("Detector automation is bounded and survives invalid controls and input", "[envelope-bank][dsp][transitions]")
{
    Detectors bank; bank.prepare(detectorRate);
    auto configuration = Detectors::Configurations{};
    bank.setParameters(configuration, 1, detectorCrossovers);
    for (int sample = 0; sample < 4096; ++sample) bank.next({.25f, -.25f, true}, {-.25f, .25f, true});
    configuration[0].source = ep::externalInput;
    configuration[1].highPassEnabled = true;
    configuration[2].lowPassEnabled = true;
    bank.setParameters(configuration, 1, detectorCrossovers);
    float previous = .25f, maximumStep = 0;
    for (int sample = 0; sample < 2048; ++sample)
    {
        const auto result = bank.next({.25f, -.25f, true}, {-.25f, .25f, true});
        maximumStep = std::max(maximumStep, std::abs(result.filtered[0].left - previous));
        previous = result.filtered[0].left;
        for (auto value : result.levels) { CHECK(std::isfinite(value)); CHECK(value >= 0); CHECK(value <= 1); }
    }
    CHECK(maximumStep < .001f); CHECK(previous == Catch::Approx(-.25f));
    configuration[0].source = ep::mainInput;
    bank.setParameters(configuration, 1, detectorCrossovers);
    for (int sample = 0; sample < 2048; ++sample)
        previous = bank.next({.25f, -.25f, true}, {-.25f, .25f, true}).filtered[0].left;
    CHECK(previous == .25f); // All source ramps must finish, including the old source.
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    for (auto& settings : configuration)
    {
        settings.source = 100; settings.attack = nan; settings.release = nan; settings.sensitivity = nan;
        settings.highPassEnabled = settings.lowPassEnabled = true;
        settings.highPassFrequency = nan; settings.lowPassFrequency = nan;
    }
    bank.setParameters(configuration, 100, {nan, nan, nan});
    for (int sample = 0; sample < 4096; ++sample)
    {
        const auto result = bank.next({nan, std::numeric_limits<float>::infinity(), true}, {nan, nan, true});
        for (auto value : result.levels) CHECK(std::isfinite(value));
        for (auto value : result.filtered) { CHECK(std::isfinite(value.left)); CHECK(std::isfinite(value.right)); }
    }
    bank.reset(); bank.setParameters({}, 1, detectorCrossovers);
    for (auto value : bank.next({}, {}).levels) CHECK(value == 0);
}

TEST_CASE("Lfo manager renders all envelope channels and audition with oversized sidechain offsets", "[envelope-bank][dsp][sidechain][range]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    detectorSet(processor, ep::parameterID(0, ep::source), ep::externalInput);
    detectorSet(processor, ep::parameterID(1, ep::source), ep::externalInput);
    detectorSet(processor, "macro4", .75f);
    auto& manager = processor.getLfoManager();
    manager.prepare({detectorRate, 31, 2});
    constexpr int samples = 1537, offset = 173;
    juce::AudioBuffer<float> main(2, samples), external(2, samples + offset), output(ms::signalCount, samples), audition(2, samples);
    main.clear(); audition.clear();
    for (int sample = 0; sample < external.getNumSamples(); ++sample)
    {
        external.setSample(0, sample, detectorTone(sample, 419));
        external.setSample(1, sample, -detectorTone(sample, 419));
    }
    manager.setAudioInput(&main); manager.setExternalAudioInput(&external, offset);
    manager.setEnvelopeAuditionOutput(&audition, 1);
    const auto snapshot = manager.captureAudioThreadParameterSnapshot();
    manager.processBlock(output, detectorRate, nullptr, samples);
    // APVTS converts defaults and notified values through normalised ranges;
    // that round trip can move even 10 ms or 75% by one ULP. Feed the exact
    // captured controls into the independent legacy recipe, preserving an
    // exact sample comparison rather than hiding the difference in a margin.
    fire::dsp::AuxiliaryModulation reference, secondReference;
    reference.prepare(detectorRate); reference.setParameters(snapshot.auxiliary);
    auto secondParameters = snapshot.auxiliary;
    secondParameters[0] = snapshot.envelopes[1].attack;
    secondParameters[1] = snapshot.envelopes[1].release;
    secondParameters[2] = snapshot.envelopes[1].sensitivity;
    secondReference.prepare(detectorRate); secondReference.setParameters(secondParameters);
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto left = external.getSample(0, sample + offset), right = external.getSample(1, sample + offset);
        const auto values = reference.next(left, right, true);
        CHECK(output.getSample(ms::envelope, sample) == values[0]);
        CHECK(output.getSample(ms::envelopeSource(1), sample) == secondReference.next(left, right, true)[0]);
        CHECK(output.getSample(ms::envelopeSource(2), sample) == 0);
        CHECK(output.getSample(ms::envelopeSource(3), sample) == 0);
        CHECK(output.getSample(ms::firstMacro + 3, sample) == values[4]);
        CHECK(audition.getSample(0, sample) == left); CHECK(audition.getSample(1, sample) == right);
    }
    CHECK(manager.getLfoOutput(ms::envelopeSource(1)) == output.getSample(ms::envelopeSource(1), samples - 1));
    manager.setAudioInput(nullptr); manager.setExternalAudioInput(nullptr); manager.setEnvelopeAuditionOutput(nullptr, -1);
}

TEST_CASE("Envelope snapshot rendering retains captured sources after APVTS changes", "[envelope-bank][dsp][snapshot]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    detectorSet(processor, ep::parameterID(3, ep::source), ep::externalInput);
    detectorSet(processor, ep::parameterID(3, ep::attack), .1f);
    auto& manager = processor.getLfoManager(); manager.prepare({detectorRate, 64, 2});
    const auto snapshot = manager.captureAudioThreadParameterSnapshot();
    detectorSet(processor, ep::parameterID(3, ep::source), ep::mainInput);
    juce::AudioBuffer<float> main(1, 256), external(1, 256), output(ms::signalCount, 256);
    main.clear(); juce::FloatVectorOperations::fill(external.getWritePointer(0), .4f, 256);
    manager.setAudioInput(&main); manager.setExternalAudioInput(&external);
    manager.processBlock(output, detectorRate, nullptr, 256, snapshot);
    CHECK(output.getSample(ms::envelopeSource(3), 255) == Catch::Approx(.4f));
    CHECK(output.getSample(ms::envelope, 255) == 0);
    manager.setAudioInput(nullptr); manager.setExternalAudioInput(nullptr);
}
