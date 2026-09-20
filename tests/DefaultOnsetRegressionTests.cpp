#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
float defaultCurve(float value)
{
    const auto x = std::clamp(value, -1.0f, 1.0f);
    return 1.5f * x - 0.5f * x * x * x;
}
}

TEST_CASE("Default processing adds no stateful discontinuity to repeated sine note attacks",
          "[default-onset][dsp][regression]")
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int blockSize : {37, 128, 512})
        for (bool immediateFirstNote : {false, true})
        {
            CAPTURE(rate, blockSize, immediateFirstNote);
            FireAudioProcessor processor;
            processor.hasUpdateCheckBeenPerformed = true;
            processor.setRateAndBufferSizeDetails(rate, blockSize);
            processor.prepareToPlay(rate, blockSize);
            const auto latency = juce::roundToInt(processor.getTotalLatency());
            const int length = static_cast<int>(rate * 1.5);
            std::vector<float> input(static_cast<size_t>(length));
            for (int sample = 0; sample < length; ++sample)
            {
                const int noteLength = static_cast<int>(rate * 0.15);
                const int note = sample / noteLength;
                const int offset = sample % noteLength;
                const int start = immediateFirstNote && note == 0 ? 0
                    : noteLength / 4 + (note * 11) % 31;
                const int end = noteLength * 3 / 4;
                if (offset < start || offset >= end) continue;
                const auto ramp = rate * 0.002;
                const auto attack = immediateFirstNote && note == 0 ? 1.0
                    : std::min(1.0, (offset - start) / ramp);
                const auto release = std::min(1.0, (end - offset) / ramp);
                const auto envelope = 0.25 * (1.0 - std::cos(juce::MathConstants<double>::pi * attack))
                    * (1.0 - std::cos(juce::MathConstants<double>::pi * release));
                input[static_cast<size_t>(sample)] = static_cast<float>(0.35 * envelope
                    * std::sin(juce::MathConstants<double>::twoPi * (note % 2 ? 440.0 : 55.0)
                               * (offset - start) / rate + note * 0.71
                               + (immediateFirstNote ? 1.3 : 0.0)));
            }
            juce::AudioBuffer<float> audio(2, blockSize);
            juce::MidiBuffer midi;
            float error = 0.0f;
            bool finiteOutput = true;
            int worstSample = 0;
            for (int offset = 0; offset < length; offset += blockSize)
            {
                const int count = std::min(blockSize, length - offset);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < count; ++i)
                        audio.setSample(ch, i, input[static_cast<size_t>(offset + i)]);
                juce::AudioBuffer<float> view(audio.getArrayOfWritePointers(), 2, count);
                processor.processBlock(view, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < count; ++i)
                    {
                        const int position = offset + i - latency;
                        const auto expected = position >= 0 ? defaultCurve(input[static_cast<size_t>(position)]) : 0.0f;
                        const auto delta = std::abs(view.getSample(ch, i) - expected);
                        finiteOutput = finiteOutput && std::isfinite(delta);
                        if (delta > error) { error = delta; worstSample = offset + i; }
                    }
            }
            CAPTURE(latency, error, worstSample);
            CHECK(finiteOutput);
            CHECK(error < 1.0e-6f);
        }
}
