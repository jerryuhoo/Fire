#include <PluginProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>

namespace
{
constexpr double rate = 48000.0;

BandProcessingParameters parameters()
{
    BandProcessingParameters p;
    p.mode = 4; // Hard clipping is linear for the quiet analytic fixtures.
    p.isDriveEnabled = true;
    p.isSafeModeOn = false;
    p.isOutputLinked = true;
    p.useModernDriveComp = true;
    p.driveVal.baseValue = 50;
    p.driveVal.range = {0, 100};
    p.outputVal.range = {-48, 6};
    p.biasVal.range = {-1, 1};
    p.recVal.range = {0, 1};
    p.mixVal = p.mixValProvider.baseValue = 1;
    p.shapeMixVal = p.shapeMixValProvider.baseValue = 1;
    return p;
}

void prepare(BandProcessor& band, const BandProcessingParameters& p, int capacity = 512)
{
    band.prepare({rate, static_cast<juce::uint32>(capacity), 2});
    band.gain.setGainDecibels(p.outputVal.baseValue);
    band.gain.reset();
}

juce::AudioBuffer<float> constant(BandProcessor& band, const BandProcessingParameters& p, int samples, float value = 0.002f)
{
    juce::AudioBuffer<float> audio(2, samples), lfo(0, samples);
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::fill(audio.getWritePointer(channel), value, samples);
    band.process(audio, p, lfo);
    return audio;
}

float expectedCompensation(float actualGain)
{
    const auto equivalentDrive = std::log2(std::max(1.0f, actualGain)) * (100.0f / 6.5f);
    return juce::Decibels::decibelsToGain(-0.1f * equivalentDrive);
}

float triangle(int sample)
{
    const float phase = static_cast<float>(sample % 480) / 480.0f;
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

float difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float maximum = 0;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int sample = 0; sample < a.getNumSamples(); ++sample)
            maximum = std::max(maximum, std::abs(a.getSample(channel, sample) - b.getSample(channel, sample)));
    return maximum;
}

struct Render
{
    juce::AudioBuffer<float> audio;
    float compensationDb = 0;
};

Render lfoRender(bool hq, int blockSize, bool canonical)
{
    constexpr int samples = 7200;
    auto p = parameters();
    p.isHQ = hq;
    p.driveLfoSourceIndex = 0;
    p.driveVal.modulationDepth = 1;
    BandProcessor band;
    prepare(band, p);
    juce::dsp::Oversampling<float> oversampling(2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    oversampling.initProcessing(512);
    Render rendered;
    rendered.audio.setSize(2, samples);
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const int count = std::min(blockSize, samples - offset);
        juce::AudioBuffer<float> audio(2, count), lfo(1, count);
        for (int i = 0; i < count; ++i)
        {
            lfo.setSample(0, i, triangle(offset + i));
            for (int channel = 0; channel < 2; ++channel)
                audio.setSample(channel, i, static_cast<float>(0.002 * std::sin((offset + i) * 0.093 + channel * 0.3)));
        }
        if (canonical)
        {
            auto block = juce::dsp::AudioBlock<float>(audio);
            auto working = hq ? oversampling.processSamplesUp(block) : block;
            const int stride = hq ? 4 : 1;
            for (size_t i = 0; i < working.getNumSamples(); ++i)
            {
                const auto drive = 100.0f * triangle(offset + static_cast<int>(i) / stride);
                const auto gain = std::exp2(6.5f * drive / 100.0f);
                const auto compensation = juce::Decibels::decibelsToGain(-0.1f * drive);
                for (size_t channel = 0; channel < working.getNumChannels(); ++channel)
                {
                    auto& value = working.getChannelPointer(channel)[i];
                    value = juce::jlimit(-1.0f, 1.0f, value * gain) * compensation;
                }
            }
            if (hq) oversampling.processSamplesDown(block);
        }
        else
            band.process(audio, p, lfo);
        for (int channel = 0; channel < 2; ++channel)
            rendered.audio.copyFrom(channel, offset, audio, channel, 0, count);
    }
    rendered.compensationDb = band.mDriveCompensationDb.load();
    return rendered;
}
}

TEST_CASE("Modern Drive compensation preserves manual Output as an independent trim",
          "[drive-comp][dsp][output]")
{
    auto p = parameters();
    p.outputVal.baseValue = -7;
    BandProcessor band;
    prepare(band, p);
    auto audio = constant(band, p, 4096);
    const auto driveGain = std::exp2(6.5f * 0.5f);
    const auto expected = 0.002f * driveGain * juce::Decibels::decibelsToGain(-5.0f - 7.0f);
    CHECK(audio.getSample(0, 4095) == Catch::Approx(expected).margin(0.000001f));
    CHECK(band.mDriveCompensationDb.load() == Catch::Approx(-5.0f).margin(0.0001f));
    p.outputVal.baseValue = -12;
    audio = constant(band, p, 4096);
    CHECK(audio.getSample(0, 4095) == Catch::Approx(expected * juce::Decibels::decibelsToGain(-5.0f)).margin(0.000001f));
    CHECK(band.mDriveCompensationDb.load() == Catch::Approx(-5.0f).margin(0.0001f));
}

TEST_CASE("Modern Drive compensation releases when Drive or its joined Shape mix removes the gain",
          "[drive-comp][dsp][bypass][shape]")
{
    auto p = parameters();
    p.driveVal.baseValue = 80;
    p.isDriveEnabled = false;
    BandProcessor bypassed;
    prepare(bypassed, p);
    auto audio = constant(bypassed, p, 4096);
    CHECK(audio.getSample(0, 4095) == 0.002f);
    CHECK(bypassed.mDriveCompensationDb.load() == 0.0f);

    p.isDriveEnabled = p.isShapeEnabled = true;
    p.shapeMixVal = p.shapeMixValProvider.baseValue = 0;
    BandProcessor joined;
    prepare(joined, p);
    audio = constant(joined, p, 4096);
    CHECK(audio.getSample(0, 4095) == 0.002f);
    CHECK(joined.mDriveCompensationDb.load() == 0.0f);

    // If another module separates the stages, Shape's own dry input already
    // contains Drive. Its zero Mix must not remove that earlier compensation.
    p.moduleOrder = {{0, 2, 1, 4, 3, 5, 6, 7, 8, 9, 10, 11, 12}};
    BandProcessor separated;
    prepare(separated, p);
    audio = constant(separated, p, 4096);
    CHECK(audio.getSample(0, 4095) == Catch::Approx(0.002f * std::exp2(6.5f * 0.8f)
        * juce::Decibels::decibelsToGain(-8.0f)).margin(0.000001f));
    CHECK(separated.mDriveCompensationDb.load() == Catch::Approx(-8.0f).margin(0.0001f));
}

TEST_CASE("Modern Drive compensation follows each routed Drive sample with the same HQ timebase",
          "[drive-comp][dsp][lfo][hq][block-size]")
{
    for (bool hq : {false, true})
    {
        CAPTURE(hq);
        const auto reference = lfoRender(hq, 512, true);
        const auto large = lfoRender(hq, 512, false);
        CHECK(difference(reference.audio, large.audio) < 0.000003f);
        CHECK(large.compensationDb == Catch::Approx(-10.0f * triangle(7199)).margin(0.0001f));
        for (int blockSize : {1, 37})
        {
            CAPTURE(blockSize);
            const auto small = lfoRender(hq, blockSize, false);
            CHECK(difference(large.audio, small.audio) < 0.000003f);
            CHECK(small.compensationDb == large.compensationDb);
        }
    }
}

TEST_CASE("Modern Drive compensation uses the final Safe Extreme and power-transition gain",
          "[drive-comp][dsp][safe][extreme][transition]")
{
    for (bool safe : {false, true})
        for (bool extreme : {false, true})
        {
            CAPTURE(safe, extreme);
            auto p = parameters();
            p.isSafeModeOn = safe; p.isExtremeModeOn = extreme;
            p.driveLfoSourceIndex = 0;
            p.driveVal.modulationDepth = 1;
            BandProcessor band;
            prepare(band, p, 1);
            juce::AudioBuffer<float> audio(2, 1), lfo(1, 1);
            float maximumError = 0;
            bool finite = true;
            for (int i = 0; i < 6500; ++i)
            {
                if (i == 1700) p.isDriveEnabled = false;
                const float input = i < 700 ? 0.01f : 0.2f;
                audio.setSample(0, 0, input); audio.setSample(1, 0, -input);
                lfo.setSample(0, 0, triangle(i));
                band.process(audio, p, lfo);
                const auto actualGain = band.driveControlTransition.lastAppliedFinalGain;
                const auto expected = juce::jlimit(-1.0f, 1.0f, input * actualGain) * expectedCompensation(actualGain);
                finite = finite && std::isfinite(audio.getSample(0, 0));
                maximumError = std::max(maximumError, std::abs(audio.getSample(0, 0) - expected));
            }
            CHECK(finite);
            CHECK(maximumError < 0.000003f);
            CHECK(band.mDriveCompensationDb.load() == 0.0f);
            CHECK(audio.getSample(0, 0) == Catch::Approx(0.2f).margin(0.000001f));
        }
}

TEST_CASE("Modern Comp switches fade while legacy processing remains untouched",
          "[drive-comp][dsp][toggle][legacy]")
{
    auto p = parameters();
    p.isOutputLinked = false;
    BandProcessor band;
    prepare(band, p);
    auto audio = constant(band, p, 4096);
    auto previous = audio.getSample(0, 4095);
    for (bool enabled : {true, false, true})
    {
        p.isOutputLinked = enabled;
        audio = constant(band, p, 2048);
        float jump = 0;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto value = audio.getSample(0, i);
            jump = std::max(jump, std::abs(value - previous)); previous = value;
        }
        CHECK(jump < 0.00005f);
        CHECK(band.mDriveCompensationDb.load() == Catch::Approx(enabled ? -5.0f : 0.0f).margin(0.0001f));
    }
    BandProcessor legacy, independent;
    p.useModernDriveComp = false;
    p.outputVal.baseValue = -5;
    prepare(legacy, p); prepare(independent, p);
    const auto original = constant(legacy, p, 4096);
    p.isOutputLinked = false;
    const auto unlinked = constant(independent, p, 4096);
    CHECK(difference(original, unlinked) == 0.0f);
    CHECK(legacy.mDriveCompensationDb.load() == 0.0f);
}

TEST_CASE("Modern Comp before a Delay leaves already-recorded echoes unchanged",
          "[drive-comp][dsp][insertfx][tail][order]")
{
    for (bool separated : {false, true})
    {
        CAPTURE(separated);
        auto p = parameters();
        p.driveVal.baseValue = 60;
        p.isOutputLinked = false;
        p.inserts[0].effect = fire::effects::InsertEffect::Parameters(fire::effects::Type::delay);
        auto& values = p.inserts[0].effect.values;
        values[0].baseValue = 100; values[1].baseValue = 60;
        values[3].baseValue = 0; values[5].baseValue = 100;
        if (separated) p.moduleOrder = {{0, 5, 1, 2, 4, 3, 6, 7, 8, 9, 10, 11, 12}};
        BandProcessor changed, reference;
        prepare(changed, p); prepare(reference, p);
        float error = 0, echoPeak = 0;
        for (int offset = 0; offset < 15104; offset += 128)
        {
            juce::AudioBuffer<float> a(2, 128), b(2, 128), lfo(0, 128);
            a.clear();
            if (offset == 4096) { a.setSample(0, 0, 0.01f); a.setSample(1, 0, 0.01f); }
            b.makeCopyOf(a);
            auto changedParameters = p;
            changedParameters.isOutputLinked = offset >= 5120;
            changed.process(a, changedParameters, lfo);
            reference.process(b, p, lfo);
            error = std::max(error, difference(a, b));
            if (offset >= 8192) echoPeak = std::max(echoPeak, a.getMagnitude(0, 128));
        }
        CHECK(echoPeak > 0.02f);
        CHECK(error == 0.0f);
        CHECK(changed.mDriveCompensationDb.load() == Catch::Approx(-6.0f).margin(0.0001f));
    }
}

TEST_CASE("A live legacy Link upgrade fades in stage compensation without an instantaneous double trim",
          "[drive-comp][dsp][upgrade][telemetry]")
{
    auto p = parameters();
    p.useModernDriveComp = false;
    p.outputVal.baseValue = -5.0f;
    BandProcessor band;
    prepare(band, p);
    REQUIRE(band.mDriveCompensationSequence.load() == 0);
    auto audio = constant(band, p, 4096);
    const auto before = audio.getSample(0, 4095);
    CHECK(band.mDriveCompensationSequence.load() == 0);
    p.useModernDriveComp = true;
    p.outputVal.baseValue = 0.0f;
    audio = constant(band, p, 4096);
    auto previous = before;
    float maximumJump = 0;
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto sample = audio.getSample(0, i);
        maximumJump = std::max(maximumJump, std::abs(sample - previous)); previous = sample;
    }
    CHECK(maximumJump < 0.00005f);
    CHECK(previous == Catch::Approx(before).margin(0.000001f));
    const auto sequence = band.mDriveCompensationSequence.load();
    REQUIRE(sequence > 0);
    constant(band, p, 32);
    CHECK(band.mDriveCompensationSequence.load() > sequence);
    band.reset();
    CHECK(band.mDriveCompensationSequence.load() == 0);
    CHECK(band.mDriveCompensationDb.load() == 0.0f);
}

TEST_CASE("Returning to legacy Link releases stage compensation while Output takes over",
          "[drive-comp][dsp][legacy][transition]")
{
    for (float drive : {50.0f, 100.0f})
    {
        CAPTURE(drive);
        auto p = parameters();
        p.driveVal.baseValue = drive;
        BandProcessor band;
        prepare(band, p);
        auto audio = constant(band, p, 4096);
        const auto before = audio.getSample(0, 4095);
        REQUIRE(band.mDriveCompensationSequence.load() > 0);
        p.useModernDriveComp = false;
        p.outputVal.baseValue = -0.1f * drive;
        audio = constant(band, p, 4096);
        auto previous = before;
        float maximumJump = 0, maximumLevel = 0;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto sample = audio.getSample(0, i);
            maximumJump = std::max(maximumJump, std::abs(sample - previous));
            maximumLevel = std::max(maximumLevel, std::abs(sample));
            previous = sample;
        }
        CHECK(maximumJump < 0.00005f);
        // Two linear gain ramps have a small smooth overlap; there must not
        // be the old immediate 5/10 dB removal of stage attenuation.
        CHECK(maximumLevel < 1.4f * before);
        CHECK(previous == Catch::Approx(before).margin(0.000001f));
        CHECK_FALSE(band.driveCompensation.isActive());
        CHECK(band.mDriveCompensationSequence.load() == 0);
        CHECK(band.mDriveCompensationDb.load() == 0.0f);
        BandProcessor stableLegacy;
        prepare(stableLegacy, p);
        constant(stableLegacy, p, 4096);
        CHECK(difference(constant(band, p, 128), constant(stableLegacy, p, 128)) == 0.0f);
    }
}

TEST_CASE("Interrupted modern legacy and Comp fades remain continuous across HQ callback sizes",
          "[drive-comp][dsp][legacy][toggle][hq][block-size]")
{
    const auto render = [](bool hq, int blockSize)
    {
        constexpr int total = 10000;
        constexpr std::array<int, 6> boundaries {{4096, 4352, 4480, 4736, 4800, total}};
        auto p = parameters();
        p.isHQ = hq;
        p.driveVal.baseValue = 80;
        p.isOutputLinked = false;
        BandProcessor band;
        prepare(band, p);
        juce::AudioBuffer<float> output(2, total);
        size_t event = 0;
        for (int offset = 0; offset < total;)
        {
            if (offset == boundaries[event])
            {
                if (event == 0) p.isOutputLinked = true;
                if (event == 1) { p.useModernDriveComp = false; p.outputVal.baseValue = -8; }
                if (event == 2) { p.useModernDriveComp = true; p.outputVal.baseValue = 0; }
                if (event == 3) p.isOutputLinked = false;
                if (event == 4) p.isOutputLinked = true;
                ++event;
            }
            const auto count = std::min(blockSize, boundaries[event] - offset);
            const auto audio = constant(band, p, count);
            for (int channel = 0; channel < 2; ++channel)
                output.copyFrom(channel, offset, audio, channel, 0, count);
            offset += count;
        }
        return output;
    };
    for (bool hq : {false, true})
    {
        CAPTURE(hq);
        const auto large = render(hq, 512);
        float maximumJump = 0;
        for (int i = 4096; i < large.getNumSamples(); ++i)
            maximumJump = std::max(maximumJump, std::abs(large.getSample(0, i) - large.getSample(0, i - 1)));
        CHECK(maximumJump < 0.0001f);
        for (int blockSize : {1, 37})
        {
            CAPTURE(blockSize);
            CHECK(difference(large, render(hq, blockSize)) < 0.000003f);
        }
    }
}

TEST_CASE("Routed Output transfers legacy attenuation with the Drive compensation fade",
          "[drive-comp][dsp][legacy][output][transition][block-size]")
{
    struct TransferRender
    {
        float before;
        juce::AudioBuffer<float> audio;
    };
    const auto render = [](float depth, bool startsModern, bool detach, int blockSize)
    {
        auto p = parameters();
        p.driveVal.baseValue = 100;
        p.useModernDriveComp = startsModern;
        p.outputVal.baseValue = startsModern ? 0.0f : -10.0f;
        p.outputVal.modulationDepth = depth;
        p.outputLfoSourceIndex = 0;
        BandProcessor band;
        prepare(band, p);
        const auto segment = [&](bool handoff)
        {
            juce::AudioBuffer<float> output(2, 4096);
            for (int offset = 0; offset < output.getNumSamples();)
            {
                if (handoff && detach && offset == 1200) p.outputLfoSourceIndex = -1;
                const int boundary = handoff && detach && offset < 1200 ? 1200 : 4096;
                const int count = std::min(blockSize, boundary - offset);
                juce::AudioBuffer<float> audio(2, count), lfo(1, count);
                for (int channel = 0; channel < 2; ++channel)
                    juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.002f, count);
                juce::FloatVectorOperations::fill(lfo.getWritePointer(0), 0.25f, count);
                band.process(audio, p, lfo);
                for (int channel = 0; channel < 2; ++channel)
                    output.copyFrom(channel, offset, audio, channel, 0, count);
                offset += count;
            }
            return output;
        };
        const auto before = segment(false).getSample(0, 4095);
        p.useModernDriveComp = ! startsModern;
        p.outputVal.baseValue = p.useModernDriveComp ? 0.0f : -10.0f;
        return TransferRender {before, segment(true)};
    };
    for (const auto depth : {0.00001f, 0.5f})
        for (const bool startsModern : {false, true})
            for (const bool detach : {false, true})
            {
                // A near-neutral route can be detached without changing the
                // intended level, isolating the handoff's timing from the edit.
                if (detach && depth > 0.00001f) continue;
                CAPTURE(depth, startsModern, detach);
                const auto reference = render(depth, startsModern, detach, 512);
                const auto before = reference.before;
                float minimumLevel = before, maximumLevel = before, maximumJump = 0;
                auto previous = before;
                for (int i = 0; i < reference.audio.getNumSamples(); ++i)
                {
                    const auto value = reference.audio.getSample(0, i);
                    minimumLevel = std::min(minimumLevel, value);
                    maximumLevel = std::max(maximumLevel, value);
                    maximumJump = std::max(maximumJump, std::abs(value - previous));
                    previous = value;
                }
                // Matching 50 ms linear-gain ramps have at most the same modest
                // overlap as scalar Output. A 10 ms Output bridge produces an
                // almost 9 dB surge on upgrade and a 7 dB dip on downgrade.
                CHECK(maximumLevel < 1.4f * before);
                CHECK(minimumLevel > 0.99f * before);
                CHECK(maximumJump < 0.002f * before);
                const auto finalLevel = detach ? before * juce::Decibels::decibelsToGain(13.5f * depth) : before;
                CHECK(previous == Catch::Approx(finalLevel).margin(0.000001f));
                for (int blockSize : {1, 37})
                {
                    CAPTURE(blockSize);
                    CHECK(difference(reference.audio, render(depth, startsModern, detach, blockSize).audio) < 0.000003f);
                }
            }
}

TEST_CASE("Routed Output handoffs retain their deadline through route edits and reversals",
          "[drive-comp][dsp][legacy][output][transition][hq][block-size]")
{
    constexpr std::array<int, 8> boundaries {{4096, 5296, 8192, 8256, 8320, 8448, 8704, 13000}};
    const auto render = [&](bool startsModern, bool dynamicLfo, bool hq, int blockSize)
    {
        auto p = parameters();
        p.isHQ = hq;
        p.driveVal.baseValue = 100;
        p.useModernDriveComp = startsModern;
        p.outputVal.baseValue = startsModern ? 0.0f : -10.0f;
        p.outputVal.modulationDepth = 0.5f;
        p.outputLfoSourceIndex = 0;
        BandProcessor band;
        prepare(band, p);
        juce::AudioBuffer<float> output(2, boundaries.back());
        size_t event = 0;
        for (int offset = 0; offset < output.getNumSamples();)
        {
            if (offset == boundaries[event])
            {
                if (event == 0 || event == 2 || event == 5)
                {
                    p.useModernDriveComp = ! p.useModernDriveComp;
                    p.outputVal.baseValue = p.useModernDriveComp ? 0.0f : -10.0f;
                }
                if (event == 1 || event == 6) p.outputLfoSourceIndex = 1;
                if (event == 3) p.outputLfoSourceIndex = -1;
                if (event == 4) p.outputLfoSourceIndex = 0;
                ++event;
            }
            const int count = std::min(blockSize, boundaries[event] - offset);
            juce::AudioBuffer<float> audio(2, count), lfo(2, count);
            for (int channel = 0; channel < 2; ++channel)
            {
                juce::FloatVectorOperations::fill(audio.getWritePointer(channel), 0.002f, count);
                for (int i = 0; i < count; ++i)
                    lfo.setSample(channel, i, dynamicLfo ? 0.1f + 0.4f * triangle(offset + i) : 0.25f);
            }
            band.process(audio, p, lfo);
            for (int channel = 0; channel < 2; ++channel)
                output.copyFrom(channel, offset, audio, channel, 0, count);
            offset += count;
        }
        return output;
    };
    for (bool startsModern : {false, true})
        for (bool dynamicLfo : {false, true})
            for (bool hq : {false, true})
            {
                CAPTURE(startsModern, dynamicLfo, hq);
                const auto reference = render(startsModern, dynamicLfo, hq, 512);
                for (const int boundary : boundaries)
                    if (boundary < reference.getNumSamples())
                        CHECK(std::abs(reference.getSample(0, boundary)
                                       - reference.getSample(0, boundary - 1)) < 0.0001f);
                if (! dynamicLfo)
                {
                    const auto before = reference.getSample(0, boundaries.front() - 1);
                    float maximumLevel = before, minimumLevel = before;
                    // At 25 ms, switch to a source with the same signal. The
                    // Output bridge must keep the original 50 ms deadline;
                    // restarting 50 ms or shortening to 10 ms causes a surge.
                    for (int i = boundaries.front(); i < boundaries[2]; ++i)
                    {
                        maximumLevel = std::max(maximumLevel, reference.getSample(0, i));
                        minimumLevel = std::min(minimumLevel, reference.getSample(0, i));
                    }
                    CHECK(maximumLevel < 1.4f * before);
                    CHECK(minimumLevel > 0.99f * before);
                }
                for (int blockSize : {1, 37})
                {
                    CAPTURE(blockSize);
                    CHECK(difference(reference, render(startsModern, dynamicLfo, hq, blockSize)) < 0.000003f);
                }
            }
}

TEST_CASE("Ordinary legacy Output route and Link changes retain their 10 ms bridge",
          "[drive-comp][dsp][legacy][output][transition]")
{
    auto p = parameters();
    p.useModernDriveComp = false;
    p.outputVal.baseValue = -5;
    p.outputVal.modulationDepth = 0.5f;
    p.outputLfoSourceIndex = 0;
    BandProcessor band;
    prepare(band, p);
    const auto render = [&](int samples)
    {
        juce::AudioBuffer<float> audio(2, samples), lfo(2, samples);
        audio.clear();
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(lfo.getWritePointer(channel), 0.25f, samples);
        band.process(audio, p, lfo);
    };
    render(4096);
    for (int event = 0; event < 2; ++event)
    {
        if (event == 0) p.outputLfoSourceIndex = 1;
        else { p.isOutputLinked = false; p.outputVal.baseValue = 0; }
        render(479);
        CHECK(band.outputGainTransition.routeTransitionMix.isSmoothing());
        render(1);
        CHECK_FALSE(band.outputGainTransition.routeTransitionMix.isSmoothing());
    }
}
