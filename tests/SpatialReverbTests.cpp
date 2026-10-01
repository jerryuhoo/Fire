#include <PluginProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
void set(FireAudioProcessor& p, const juce::String& id, float value)
{auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter); parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
}
TEST_CASE("Room Hall Plate Spring and Chamber have distinct finite decaying stereo tails", "[space-reverb][dsp][tail]")
{
    std::array<double, 5> signatures {};
    for (int model = 1; model < fire::space::count; ++model)
        for (double rate : {44100.0, 96000.0})
        {
            CAPTURE(model, rate);
            fire::space::Reverb reverb; reverb.prepare(rate);
            double initial = 0, late = 0; bool finite = true; float peak = 0;
            for (int sample = 0; sample < static_cast<int>(rate * 3); ++sample)
            {
                float l = sample == 0 ? 1.0f : 0, r = sample == 0 ? .7f : 0;
                reverb.process(l, r, model, 60, 40, 100);
                finite = finite && std::isfinite(l) && std::isfinite(r);
                peak = juce::jmax(peak, std::abs(l), std::abs(r));
                const auto energy = l * l + r * r;
                if (sample < rate) initial += energy;
                if (sample > rate * 2) late += energy;
                if (rate == 44100) signatures[static_cast<size_t>(model - 1)] += l * (sample % 23 + 1) + r * (sample % 19 + 1);
            }
            CHECK(finite); CHECK(peak < 2); CHECK(initial > .001); CHECK(late < initial * .8);
            reverb.reset(); float l = 0, r = 0; reverb.process(l, r, model, 60, 40, 100); CHECK(l == 0); CHECK(r == 0);
        }
    for (size_t a = 0; a < signatures.size(); ++a)
        for (size_t b = a + 1; b < signatures.size(); ++b) CHECK(std::abs(signatures[a] - signatures[b]) > .0001);
}

TEST_CASE("Reverb algorithms round trip without changing old type and control automation", "[space-reverb][state][preset][ab]")
{
    FireAudioProcessor processor;
    const auto slot = processor.addInsertEffect(0, fire::effects::Type::reverb); REQUIRE(slot == 0);
    const auto id = fire::reverb_params::parameterID(0, slot);
    set(processor, id, 4);
    processor.stateAB.copyAB();
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(processor, preset);
    FireAudioProcessor restored; REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK(restored.treeState.getRawParameterValue(id)->load() == 4);
    juce::MemoryBlock host; processor.getStateInformation(host); restored.setStateInformation(host.getData(), static_cast<int>(host.getSize()));
    CHECK(restored.treeState.getRawParameterValue(id)->load() == 4);
    auto* type = restored.treeState.getParameter(fire::effects::parameterID(0, slot, fire::effects::typeField)); REQUIRE(type);
    CHECK(type->convertFrom0to1(.6f) == 3);
    preset.removeAttribute("reverbModelsSchemaVersion");
    for (const auto& parameter : fire::reverb_params::parameterIDs()) preset.removeAttribute(parameter);
    REQUIRE(state::loadStateFromXml(preset, restored)); CHECK(restored.treeState.getRawParameterValue(id)->load() == 0);
}

TEST_CASE("Reverb model automation and oversized partitions keep a coherent bounded timeline", "[space-reverb][dsp][transition]")
{
    const auto render = [](int chunk)
    {
        fire::effects::InsertEffect effect; effect.prepare({48000, 64, 2});
        fire::effects::InsertEffect::Parameters p(fire::effects::Type::reverb); p.reverbModel = 2; p.values[5].baseValue = 100;
        juce::AudioBuffer<float> result(2, 8192);
        for (int offset = 0; offset < 8192;)
        {
            const int boundary = offset < 4096 ? 4096 : 8192;
            const int count = juce::jmin(chunk, boundary - offset); juce::AudioBuffer<float> block(2, count);
            p.reverbModel = offset < 4096 ? 2 : 4;
            for (int sample = 0; sample < count; ++sample)
                for (int channel = 0; channel < 2; ++channel) block.setSample(channel, sample, .17f * std::sin((offset + sample) * .085f + channel * .5f));
            auto audio = juce::dsp::AudioBlock<float>(block); effect.process(audio, p);
            for (int channel = 0; channel < 2; ++channel) result.copyFrom(channel, offset, block, channel, 0, count);
            offset += count;
        }
        return result;
    };
    auto expected = render(64), actual = render(256);
    float error = 0;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 8192; ++sample)
        {REQUIRE(std::isfinite(actual.getSample(channel, sample))); error = juce::jmax(error, std::abs(expected.getSample(channel, sample) - actual.getSample(channel, sample)));}
    CHECK(error < 1e-5f);
}
