#include <DSP/InsertRack.h>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
constexpr int sampleRate = 48000;
fire::effects::RackParameters cloudsRack()
{
    fire::effects::RackParameters result;
    auto& p = result[0].effect;
    p.type = fire::effects::Type::granular;
    for (size_t i = 0; i < p.values.size(); ++i)
    {
        const auto& control = fire::effects::controls(fire::effects::Type::granular)[i];
        p.values[i].baseValue = control.toNormalised(control.initial);
        p.values[i].range = {0.0f, 1.0f};
    }
    return result;
}

juce::AudioBuffer<float> cloudsSignal(int channels, int length)
{
    juce::AudioBuffer<float> result(channels, length);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < length; ++i)
            result.setSample(ch, i, static_cast<float>(0.1 * std::sin(i * 0.057 + ch * 0.3)));
    return result;
}

float difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float result = 0;
    bool finite = true;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
        {
            finite &= std::isfinite(a.getSample(ch, i)) && std::isfinite(b.getSample(ch, i));
            result = std::max(result, std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        }
    REQUIRE(finite);
    return result;
}

void process(fire::effects::InsertRack& rack, juce::AudioBuffer<float>& audio,
             const fire::effects::RackParameters& p, const juce::AudioBuffer<float>& lfo,
             int start, int end, int blockSize)
{
    for (int offset = start; offset < end; offset += blockSize)
        rack.processSlot(juce::dsp::AudioBlock<float>(audio).getSubBlock(static_cast<size_t>(offset),
            static_cast<size_t>(std::min(blockSize, end - offset))), 0, p, lfo, offset);
}
}

TEST_CASE("Clouds insert retains exact dry bypass and zero mix in mono and stereo", "[clouds][insertfx][bypass]")
{
    juce::AudioBuffer<float> lfo;
    for (int channels : {1, 2})
        for (bool bypass : {false, true})
        {
            CAPTURE(channels, bypass);
            const auto input = cloudsSignal(channels, sampleRate / 3);
            juce::AudioBuffer<float> audio;
            audio.makeCopyOf(input);
            fire::effects::InsertRack rack;
            rack.prepare({sampleRate, 512, static_cast<juce::uint32>(channels)});
            auto p = cloudsRack();
            if (bypass) p[0].effect.enabled = false;
            else p[0].effect.values[5].baseValue = 0;
            process(rack, audio, p, lfo, 0, audio.getNumSamples(), 137);
            CHECK(difference(input, audio) == 0.0f);
        }
}

TEST_CASE("Granular modulation, freeze and effect changes are independent of callback partitioning", "[clouds][insertfx][lfo][block-size]")
{
    constexpr int segment = sampleRate / 4;
    constexpr int length = segment * 8;
    const auto input = cloudsSignal(2, length);
    juce::AudioBuffer<float> lfo(2, length);
    for (int i = 0; i < length; ++i)
    {
        lfo.setSample(0, i, 0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.0004f));
        lfo.setSample(1, i, 0.5f + 0.5f * std::cos(static_cast<float>(i) * 0.0007f));
    }
    const auto render = [&](int blockSize)
    {
        fire::effects::InsertRack rack;
        rack.prepare({sampleRate, 512, 2});
        auto p = cloudsRack();
        p[0].effect.values[5].baseValue = 0.75f;
        p[0].effect.clouds.values[1].baseValue = 0.2f;
        p[0].effect.clouds.values[2].baseValue = 0.3f;
        p[0].sources[3] = 0;
        p[0].effect.values[3].modulationDepth = 0.15f;
        juce::AudioBuffer<float> audio;
        audio.makeCopyOf(input);
        for (int phase = 0; phase < 8; ++phase)
        {
            if (phase == 1) p[0].cloudsSources = {0, 1, 0};
            if (phase == 2) { p[0].cloudsSources = {1, 0, 1}; p[0].effect.clouds.freeze = true; }
            if (phase == 3) { p[0].cloudsSources = {-1, -1, -1}; p[0].effect.clouds.freeze = false; }
            if (phase == 4) p[0].effect.type = fire::effects::Type::reverb;
            if (phase == 5) p[0].effect.type = fire::effects::Type::granular;
            if (phase == 6) p[0].effect.enabled = false;
            if (phase == 7) p[0].effect.enabled = true;
            process(rack, audio, p, lfo, segment * phase, segment * (phase + 1), blockSize);
        }
        return audio;
    };
    const auto reference = render(1);
    CHECK(difference(reference, input) > 0.01f);
    CHECK(difference(reference, render(37)) < 1.0e-6f);
    CHECK(difference(reference, render(512)) < 1.0e-6f);
    for (int boundary : {segment * 4, segment * 5, segment * 6, segment * 7})
        for (int ch = 0; ch < 2; ++ch)
            CHECK(std::abs(reference.getSample(ch, boundary) - reference.getSample(ch, boundary - 1)) < 0.03f);
}

TEST_CASE("Clouds insert reset removes frozen and feedback audio without leaking old slots", "[clouds][insertfx][reset]")
{
    fire::effects::InsertRack rack;
    rack.prepare({sampleRate, 512, 2});
    auto p = cloudsRack();
    p[0].effect.values[5].baseValue = 1;
    p[0].effect.clouds.values[1].baseValue = 0.95f;
    p[0].effect.clouds.values[2].baseValue = 1;
    juce::AudioBuffer<float> lfo;
    auto audio = cloudsSignal(2, sampleRate * 2);
    process(rack, audio, p, lfo, 0, audio.getNumSamples(), 127);
    CHECK(audio.getMagnitude(0, sampleRate, sampleRate) > 0.01f);
    p[0].effect.clouds.freeze = true;
    audio.clear();
    process(rack, audio, p, lfo, 0, audio.getNumSamples(), 127);
    CHECK(audio.getMagnitude(0, sampleRate, sampleRate) > 0.001f);
    rack.reset();
    audio.clear();
    process(rack, audio, p, lfo, 0, audio.getNumSamples(), 127);
    CHECK(audio.getMagnitude(0, audio.getNumSamples()) == 0.0f);
    CHECK(audio.getMagnitude(1, 0, audio.getNumSamples()) == 0.0f);
}

TEST_CASE("Default Clouds fades the first recorded boundary when enabled on an existing note", "[clouds][insertfx][quality][attack]")
{
    // Constant input isolates discontinuities introduced by the effect from
    // the input waveform's own attacks. Previously a newly filled recording
    // became audible in the middle of an existing grain window in one sample.
    fire::effects::InsertRack rack;
    rack.prepare({sampleRate, 512, 2});
    auto p = cloudsRack();
    p[0].effect.values[5].baseValue = 1.0f;
    juce::AudioBuffer<float> lfo;
    for (int activation = 0; activation < 2; ++activation)
    {
        CAPTURE(activation);
        juce::AudioBuffer<float> audio(2, sampleRate * 2);
        for (int ch = 0; ch < 2; ++ch)
            std::fill_n(audio.getWritePointer(ch), audio.getNumSamples(), 0.2f);
        process(rack, audio, p, lfo, 0, audio.getNumSamples(), 127);
        for (int ch = 0; ch < 2; ++ch)
        {
            float step = 0;
            for (int i = 1; i < audio.getNumSamples(); ++i)
                step = std::max(step, std::abs(audio.getSample(ch, i) - audio.getSample(ch, i - 1)));
            CAPTURE(ch, step);
            CHECK(step < 0.003f);
            CHECK(audio.getRMSLevel(ch, sampleRate / 2, sampleRate) > 0.01f);
        }
        rack.reset();
    }
}
