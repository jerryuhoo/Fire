#include "../Source/DSP/Delay.h"
#include "../Source/DSP/ClippingFunctions.h"
#include "../Source/DSP/DistortionLogic.h"
#include "../Source/DSP/LfoData.h"
#include "../Source/DSP/LfoEngine.h"
#include "../Source/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>

namespace
{
#if defined(_MSC_VER)
#define FIRE_TEST_NOINLINE __declspec(noinline)
#else
#define FIRE_TEST_NOINLINE __attribute__((noinline))
#endif

FIRE_TEST_NOINLINE bool isPositiveZeroBits(std::uint32_t bits) noexcept
{
    return bits == std::bit_cast<std::uint32_t>(0.0f);
}

float runtimeFloatFromBits(std::uint32_t bits) noexcept
{
    volatile std::uint32_t runtimeBits = bits;
    const std::uint32_t copiedBits = runtimeBits;
    return std::bit_cast<float>(copiedBits);
}

#undef FIRE_TEST_NOINLINE

class TestPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override { return position; }

    PositionInfo position;
};

void setParameterNormalised(FireAudioProcessor& processor,
                            const juce::String& parameterID,
                            float normalisedValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(normalisedValue);
}

void setParameterValue(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float value)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->getNormalisableRange().convertTo0to1(value));
}

bool bufferContainsOnlyFiniteSamples(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite(buffer.getSample(channel, sample)))
                return false;

    return true;
}

constexpr size_t referenceLfoTableSize = 1024;

std::array<float, referenceLfoTableSize> makeReferenceLfoTable(const LfoData& shapeData,
                                                               float smoothness)
{
    std::array<float, referenceLfoTableSize> table {};
    size_t segmentIndex = 0;

    for (size_t i = 0; i < table.size(); ++i)
    {
        const float phase = static_cast<float>(i) / static_cast<float>(table.size() - 1);
        while (segmentIndex + 1 < shapeData.points.size()
               && phase > shapeData.points[segmentIndex + 1].x)
            ++segmentIndex;

        float value = 0.5f;
        if (segmentIndex + 1 < shapeData.points.size())
        {
            const auto& first = shapeData.points[segmentIndex];
            const auto& second = shapeData.points[segmentIndex + 1];
            const float width = second.x - first.x;
            if (phase >= first.x && phase <= second.x)
            {
                if (segmentIndex >= shapeData.curvatures.size() || std::abs(width) < 1.0e-9f)
                {
                    value = std::abs(width) < 1.0e-9f
                                ? first.y
                                : first.y + (second.y - first.y) * ((phase - first.x) / width);
                }
                else
                {
                    const float curvature = juce::jlimit(-2.0f, 2.0f,
                                                          shapeData.curvatures[segmentIndex]);
                    const float x = juce::jlimit(0.0f, 1.0f, (phase - first.x) / width);
                    const float exponent = std::pow(4.0f, std::abs(curvature));
                    const float curvedX = curvature >= 0.0f
                                              ? std::pow(x, exponent)
                                              : 1.0f - std::pow(juce::jmax(0.0f, 1.0f - x), exponent);
                    value = first.y + (second.y - first.y) * curvedX;
                }
            }
        }

        if (i == table.size() - 1)
            value = shapeData.points.back().y;
        table[i] = juce::jlimit(0.0f, 1.0f, value);
    }

    if (smoothness > 0.001f)
    {
        const float feedback = smoothness * 0.95f;
        const float feedforward = 1.0f - feedback;
        for (int pass = 0; pass < 2; ++pass)
        {
            float state = table.back();
            for (auto& value : table)
            {
                value = value * feedforward + state * feedback;
                state = value;
            }
        }
    }

    return table;
}

float interpolateReferenceLfo(const std::array<float, referenceLfoTableSize>& table,
                              float phase)
{
    const float position = juce::jlimit(0.0f, 1.0f, phase)
                           * static_cast<float>(table.size() - 1);
    const size_t index = juce::jmin(static_cast<size_t>(position), table.size() - 1);
    const float next = index + 1 < table.size() ? table[index + 1] : table.back();
    return table[index] + (position - static_cast<float>(index)) * (next - table[index]);
}
} // namespace

TEST_CASE("Malformed LFO data is normalised before DSP use", "[lfo][robustness]")
{
    LfoData data;
    data.points = {
        { 0.8f, 1.4f },
        { -0.2f, -0.3f },
        { 0.4f, 0.6f },
    };
    data.curvatures = { std::numeric_limits<float>::infinity() };
    data.smoothness = 2.0f;

    data.sanitise();

    REQUIRE(data.points.size() >= 2);
    CHECK(data.curvatures.size() == data.points.size() - 1);
    CHECK(data.points.front().x == Catch::Approx(0.0f));
    CHECK(data.points.back().x == Catch::Approx(1.0f));
    CHECK(data.smoothness == Catch::Approx(1.0f));

    CHECK(std::is_sorted(data.points.begin(), data.points.end(), [](const auto& lhs, const auto& rhs)
                         { return lhs.x < rhs.x; }));

    for (const auto& point : data.points)
    {
        CHECK(std::isfinite(point.x));
        CHECK(std::isfinite(point.y));
        CHECK(point.x >= 0.0f);
        CHECK(point.x <= 1.0f);
        CHECK(point.y >= 0.0f);
        CHECK(point.y <= 1.0f);
    }

    for (const auto curvature : data.curvatures)
        CHECK(std::isfinite(curvature));

    data.points[1].x = std::numeric_limits<float>::quiet_NaN();
    data.sanitise();
    REQUIRE(data.points.size() == 2);
    CHECK(data.points.front().x == Catch::Approx(0.0f));
    CHECK(data.points.back().x == Catch::Approx(1.0f));
}

TEST_CASE("Legacy oversized LFO XML preserves the complete curve", "[lfo][state][compatibility]")
{
    constexpr int legacyPointCount = 130;
    juce::XmlElement lfoXml("LFO");
    auto* points = lfoXml.createNewChildElement("POINTS");
    auto* curvatures = lfoXml.createNewChildElement("CURVATURES");

    for (int i = 0; i < legacyPointCount; ++i)
    {
        const float x = static_cast<float>(i) / static_cast<float>(legacyPointCount - 1);
        auto* point = points->createNewChildElement("P");
        point->setAttribute("x", x);
        point->setAttribute("y", i < legacyPointCount / 2 ? 0.0f : 1.0f);

        if (i + 1 < legacyPointCount)
        {
            auto* curvature = curvatures->createNewChildElement("C");
            curvature->setAttribute("v", 0.0f);
        }
    }

    const auto restored = LfoData::readFromXml(lfoXml);
    REQUIRE(restored.points.size() == LfoData::maximumNumberOfPoints);
    REQUIRE(restored.curvatures.size() == restored.points.size() - 1);
    CHECK(restored.points.front().x == Catch::Approx(0.0f));
    CHECK(restored.points.front().y == Catch::Approx(0.0f));
    CHECK(restored.points.back().x == Catch::Approx(1.0f));
    CHECK(restored.points.back().y == Catch::Approx(1.0f));
    CHECK(std::any_of(restored.points.begin()
                          + static_cast<std::ptrdiff_t>(restored.points.size() / 2),
                      restored.points.end(),
                      [](const auto& point) { return point.y > 0.5f; }));
}

TEST_CASE("Prebuilt LFO banks match the original curve and smoothing math", "[lfo][wavetable]")
{
    LfoData shape;
    shape.points = { { 0.0f, 0.1f }, { 0.3f, 0.9f }, { 0.7f, 0.2f }, { 1.0f, 0.8f } };
    shape.curvatures = { 1.0f, -0.5f, 2.0f };
    shape.sanitise();

    LfoEngine engine;
    engine.stageShape(shape);
    engine.publishStagedShape();
    engine.setPhaseDelta(0.0f);

    constexpr std::array<float, 7> phases { 0.0f, 0.0005f, 0.12345f, 0.3f, 0.5f, 0.999f, 1.0f };
    for (int step = 0; step <= 100; ++step)
    {
        const float smoothness = static_cast<float>(step) / 100.0f;
        const auto reference = makeReferenceLfoTable(shape, smoothness);
        engine.setSmoothness(smoothness);

        for (const float phase : phases)
        {
            engine.setPhase(phase);
            const float actual = engine.process();
            const float expected = interpolateReferenceLfo(reference, phase);
            CAPTURE(step, phase, actual, expected);
            CHECK(actual == Catch::Approx(expected).margin(2.0e-6f));
        }
    }

    engine.setSmoothness(0.0f);
    engine.setPhase(0.37f);
    const float activeValue = engine.process();

    LfoData replacement;
    replacement.points = { { 0.0f, 0.0f }, { 1.0f, 0.0f } };
    replacement.curvatures = { 0.0f };
    replacement.sanitise();
    engine.stageShape(replacement);

    engine.setPhase(0.37f);
    CHECK(engine.process() == Catch::Approx(activeValue).margin(1.0e-6f));

    // A newer staged shape must replace an unpublished one without touching
    // the bank that the audio thread is still reading.
    LfoData latestReplacement;
    latestReplacement.points = { { 0.0f, 1.0f }, { 1.0f, 1.0f } };
    latestReplacement.curvatures = { 0.0f };
    latestReplacement.sanitise();
    engine.stageShape(latestReplacement);
    engine.setPhase(0.37f);
    CHECK(engine.process() == Catch::Approx(activeValue).margin(1.0e-6f));

    engine.publishStagedShape();
    engine.setPhase(0.37f);
    CHECK(std::abs(engine.process() - activeValue) > 0.1f);
}

TEST_CASE("LFO Smooth row changes crossfade without output steps", "[lfo][wavetable][smoothing]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int transitionSamples = 480;

    LfoData shape;
    shape.points = { { 0.0f, 0.0f }, { 0.49f, 0.0f },
                     { 0.5f, 1.0f }, { 1.0f, 1.0f } };
    shape.curvatures = { 0.0f, 0.0f, 0.0f };
    shape.sanitise();

    LfoEngine engine;
    engine.stageShape(shape);
    engine.publishStagedShape();
    engine.prepare({ sampleRate, 64, 1 });
    engine.setPhaseDelta(0.0f);
    engine.setPhase(0.5f);
    engine.setSmoothness(0.0f);
    const float unsmoothed = engine.process();

    const auto smoothReference = makeReferenceLfoTable(shape, 1.0f);
    const float smoothTarget = interpolateReferenceLfo(smoothReference, 0.5f);
    REQUIRE(std::abs(smoothTarget - unsmoothed) > 0.25f);

    engine.setSmoothness(1.0f);
    engine.setPhase(0.5f);
    const float firstTransitionSample = engine.process();
    CHECK(std::abs(firstTransitionSample - unsmoothed) < 0.01f);

    float previous = firstTransitionSample;
    for (int sample = 1; sample < transitionSamples; ++sample)
    {
        engine.setPhase(0.5f);
        const float current = engine.process();
        CHECK(std::abs(current - previous) < 0.01f);
        previous = current;
    }
    CHECK(previous == Catch::Approx(smoothTarget).margin(2.0e-6f));

    // Retargeting while a transition is active must continue from the audible
    // blend, not jump back to either table.
    engine.setSmoothness(0.0f);
    for (int sample = 0; sample < 50; ++sample)
    {
        engine.setPhase(0.5f);
        previous = engine.process();
    }
    engine.setSmoothness(0.3f);
    engine.setPhase(0.5f);
    CHECK(std::abs(engine.process() - previous) < 0.01f);
}

TEST_CASE("Restored LFO Smooth value is active on the first sample", "[lfo][wavetable][smoothing][state]")
{
    constexpr double sampleRate = 48000.0;

    LfoData shape;
    shape.points = { { 0.0f, 0.0f }, { 0.49f, 0.0f },
                     { 0.5f, 1.0f }, { 1.0f, 1.0f } };
    shape.curvatures = { 0.0f, 0.0f, 0.0f };
    shape.sanitise();

    LfoEngine engine;
    engine.stageShape(shape);
    engine.publishStagedShape();
    engine.prepare({ sampleRate, 64, 1 });
    engine.setSmoothness(1.0f);
    engine.setPhase(0.5f);
    engine.setPhaseDelta(0.0f);

    const auto smoothReference = makeReferenceLfoTable(shape, 1.0f);
    CHECK(engine.process()
          == Catch::Approx(interpolateReferenceLfo(smoothReference, 0.5f)).margin(2.0e-6f));
}

TEST_CASE("Published LFO shapes crossfade without output steps", "[lfo][wavetable][smoothing]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int transitionSamples = 480;

    const auto makeConstantShape = [](float value)
    {
        LfoData shape;
        shape.points = { { 0.0f, value }, { 1.0f, value } };
        shape.curvatures = { 0.0f };
        shape.sanitise();
        return shape;
    };

    // A restored shape must be exact on the first rendered sample. Crossfade
    // only changes an already audible LFO, never startup/offline-render data.
    LfoEngine restoredEngine;
    restoredEngine.prepare({ sampleRate, 64, 1 });
    restoredEngine.stageShape(makeConstantShape(1.0f));
    restoredEngine.publishStagedShape();
    restoredEngine.setPhase(0.5f);
    restoredEngine.setPhaseDelta(0.0f);
    CHECK(restoredEngine.process() == Catch::Approx(1.0f).margin(1.0e-7f));

    LfoEngine engine;
    engine.stageShape(makeConstantShape(0.0f));
    engine.publishStagedShape();
    engine.prepare({ sampleRate, 64, 1 });
    engine.setPhaseDelta(0.0f);
    engine.setPhase(0.5f);
    float previous = engine.process();
    REQUIRE(previous == Catch::Approx(0.0f).margin(1.0e-7f));

    // Building an inactive bank must remain completely inaudible until the
    // audio thread publishes it.
    engine.stageShape(makeConstantShape(1.0f));
    engine.setPhase(0.5f);
    CHECK(engine.process() == Catch::Approx(previous).margin(1.0e-7f));

    engine.publishStagedShape();
    engine.setPhase(0.5f);
    float current = engine.process();
    CHECK(std::abs(current - previous) < 0.01f);
    previous = current;

    for (int sample = 1; sample < 50; ++sample)
    {
        engine.setPhase(0.5f);
        previous = engine.process();
    }

    // Publish twice during active transitions. This reuses both banks and
    // verifies that the fade reads its private snapshot, not an overwritten
    // inactive bank.
    engine.stageShape(makeConstantShape(0.3f));
    engine.publishStagedShape();
    engine.setPhase(0.5f);
    current = engine.process();
    CHECK(std::abs(current - previous) < 0.01f);
    previous = current;

    for (int sample = 1; sample < 50; ++sample)
    {
        engine.setPhase(0.5f);
        previous = engine.process();
    }

    engine.stageShape(makeConstantShape(0.8f));
    engine.publishStagedShape();
    engine.setPhase(0.5f);
    current = engine.process();
    CHECK(std::abs(current - previous) < 0.01f);
    previous = current;

    for (int sample = 1; sample < transitionSamples; ++sample)
    {
        engine.setPhase(0.5f);
        current = engine.process();
        CHECK(std::abs(current - previous) < 0.01f);
        previous = current;
    }
    CHECK(previous == Catch::Approx(0.8f).margin(2.0e-6f));
}

TEST_CASE("Delay keeps independent channel histories and an exact delay", "[delay][robustness]")
{
    Delay delay { 2 };
    delay.setState(true);

    const std::array<float, 4> leftInput { 1.0f, 2.0f, 3.0f, 4.0f };
    const std::array<float, 4> rightInput { 10.0f, 20.0f, 30.0f, 40.0f };
    std::array<float, 4> leftOutput {};
    std::array<float, 4> rightOutput {};

    for (size_t i = 0; i < leftInput.size(); ++i)
    {
        leftOutput[i] = delay.process(leftInput[i], 0, static_cast<int>(leftInput.size()));
        rightOutput[i] = delay.process(rightInput[i], 1, static_cast<int>(rightInput.size()));
    }

    const std::array<float, 4> expectedLeft { 0.0f, 0.0f, 1.0f, 2.0f };
    const std::array<float, 4> expectedRight { 0.0f, 0.0f, 10.0f, 20.0f };
    CHECK(leftOutput == expectedLeft);
    CHECK(rightOutput == expectedRight);

    delay.setLatency(0);
    CHECK(delay.process(7.0f, 0, 1) == Catch::Approx(7.0f));
    CHECK(delay.process(9.0f, -1, 1) == Catch::Approx(9.0f));
}

TEST_CASE("Spectrum frames are published as complete newest-only snapshots", "[fft][threading]")
{
    SpectrumProcessor spectrum;

    for (int frame = 1; frame <= 3; ++frame)
        for (int sample = 0; sample < SpectrumProcessor::fftSize; ++sample)
            spectrum.pushNextSamplePairIntoFifo(static_cast<float>(frame),
                                                static_cast<float>(frame + 10));

    REQUIRE(spectrum.hasCompleteFrame());

    std::array<float, 2 * SpectrumProcessor::fftSize> latestProcessed {};
    std::array<float, 2 * SpectrumProcessor::fftSize> latestOriginal {};
    REQUIRE(spectrum.popLatestFramePair(latestProcessed.data(), static_cast<int>(latestProcessed.size()),
                                        latestOriginal.data(), static_cast<int>(latestOriginal.size())));
    CHECK(latestProcessed.front() == Catch::Approx(3.0f));
    CHECK(latestProcessed[SpectrumProcessor::fftSize - 1] == Catch::Approx(3.0f));
    CHECK(latestProcessed[SpectrumProcessor::fftSize] == Catch::Approx(0.0f));
    CHECK(latestOriginal.front() == Catch::Approx(13.0f));
    CHECK(latestOriginal[SpectrumProcessor::fftSize - 1] == Catch::Approx(13.0f));
    CHECK_FALSE(spectrum.hasCompleteFrame());

    for (int sample = 0; sample < SpectrumProcessor::fftSize; ++sample)
        spectrum.pushNextSamplePairIntoFifo(4.0f, 14.0f);

    REQUIRE(spectrum.popLatestFramePair(latestProcessed.data(), static_cast<int>(latestProcessed.size()),
                                        latestOriginal.data(), static_cast<int>(latestOriginal.size())));
    CHECK(latestProcessed.front() == Catch::Approx(4.0f));
    CHECK(latestOriginal.front() == Catch::Approx(14.0f));

    std::array<float, SpectrumProcessor::fftSize> undersized {};
    CHECK_FALSE(spectrum.doProcessing(undersized.data(), static_cast<int>(undersized.size())));
}

TEST_CASE("Sample-accurate bipolar modulation matches block modulation depth", "[lfo][modulation]")
{
    const std::array<float, 1> lfo { 1.0f };
    ModulatedValueProvider provider;
    provider.lfoSignal = lfo.data();
    provider.baseValue = 0.25f;
    provider.modulationDepth = 1.0f;
    provider.isBipolar = true;
    provider.range = { 0.0f, 1.0f };

    CHECK(provider.get(0) == Catch::Approx(0.75f));
    provider.isBipolar = false;
    CHECK(provider.get(0) == Catch::Approx(1.0f));
}

TEST_CASE("Bar-synchronised LFO rates follow the host time signature", "[lfo][sync]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto& manager = processor.getLfoManager();
    manager.prepare({ 48000.0, 1, 4 });

    setParameterValue(processor, ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0), 1.0f);
    setParameterValue(processor, ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, 0), 11.0f);

    TestPlayHead playHead;
    playHead.position.setIsPlaying(true);
    playHead.position.setBpm(120.0);
    playHead.position.setPpqPosition(0.0);

    juce::AudioBuffer<float> output(4, 1);
    for (const auto signature : { juce::AudioPlayHead::TimeSignature { 3, 4 },
                                  juce::AudioPlayHead::TimeSignature { 6, 8 },
                                  juce::AudioPlayHead::TimeSignature { 5, 4 } })
    {
        manager.reset();
        playHead.position.setTimeSignature(signature);
        manager.processBlock(output, 48000.0f, &playHead, 1);

        const float quarterNotesPerBar = static_cast<float>(signature.numerator) * 4.0f
                                         / static_cast<float>(signature.denominator);
        const float expectedDelta = 1.0f / (quarterNotesPerBar * 0.5f * 48000.0f);
        CAPTURE(signature.numerator, signature.denominator,
                manager.getLfoPhase(0), expectedDelta);
        CHECK(manager.getLfoPhase(0) == Catch::Approx(expectedDelta).margin(1.0e-8f));
    }

    // Two bars in 3/4 contain six quarter notes.
    manager.reset();
    setParameterValue(processor, ParameterIDAndName::getIDString(LFO_RATE_SYNC_ID, 0), 12.0f);
    playHead.position.setTimeSignature(juce::AudioPlayHead::TimeSignature { 3, 4 });
    playHead.position.setPpqPosition(1.5);
    manager.processBlock(output, 48000.0f, &playHead, 1);
    const float twoBarDelta = 1.0f / (6.0f * 0.5f * 48000.0f);
    CHECK(manager.getLfoPhase(0) == Catch::Approx(0.25f + twoBarDelta).margin(1.0e-7f));
}

TEST_CASE("Logic clipping remains monotonic and saturated at high drive", "[distortion][robustness]")
{
    float previous = waveshaping::logicClip(-100.0f);
    CHECK(previous < -0.999f);

    for (const float input : { -20.0f, -5.0f, -1.0f, 0.0f, 1.0f, 5.0f, 20.0f, 100.0f })
    {
        const float output = waveshaping::logicClip(input);
        CAPTURE(input, output, previous);
        CHECK(std::isfinite(output));
        CHECK(output >= previous);
        CHECK(output >= -1.0f);
        CHECK(output <= 1.0f);
        CHECK(output == Catch::Approx(-waveshaping::logicClip(-input)).margin(1.0e-6f));
        previous = output;
    }

    CHECK(waveshaping::logicClip(100.0f) > 0.999f);
    CHECK(waveshaping::logicClip(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
}

TEST_CASE("Pit clipping preserves its high-drive foldback and polarity", "[distortion][robustness]")
{
    for (const float input : { -100.0f, -50.0f, -20.0f, -5.0f,
                                0.0f, 5.0f, 20.0f, 50.0f, 100.0f })
    {
        const float output = waveshaping::tanclip(input);
        const float expected = juce::jlimit(-1.0f, 1.0f,
                                            std::tanh(input) - 0.02f * input);
        CAPTURE(input, output, expected);
        CHECK(std::isfinite(output));
        CHECK(output == Catch::Approx(expected).margin(2.0e-4f));
        CHECK(output == Catch::Approx(-waveshaping::tanclip(-input)).margin(1.0e-6f));
    }

    CHECK(waveshaping::tanclip(100.0f) < -0.99f);
    CHECK(waveshaping::tanclip(-100.0f) > 0.99f);
    CHECK(waveshaping::tanclip(std::numeric_limits<float>::infinity()) == 0.0f);
}

TEST_CASE("Release DSP preserves non-finite input guards", "[distortion][robustness][release]")
{
    // Construct the values at runtime and inspect their result through a
    // no-inline integer barrier. This keeps the canary meaningful even under
    // LTO: a fast-math build cannot optimise the assertion itself on the
    // assumption that floating-point inputs are always finite.
    const float positiveInfinity = runtimeFloatFromBits(0x7f800000u);
    const float quietNan = runtimeFloatFromBits(0x7fc00000u);

    CHECK(isPositiveZeroBits(std::bit_cast<std::uint32_t>(waveshaping::tanclip(positiveInfinity))));
    CHECK(isPositiveZeroBits(std::bit_cast<std::uint32_t>(waveshaping::logicClip(quietNan))));
    CHECK(isPositiveZeroBits(std::bit_cast<std::uint32_t>(DistortionLogic::processSample(quietNan, {}))));

    for (const float input : { -1.0f, -0.75f, -0.25f, 0.0f, 0.25f, 0.75f, 1.0f })
    {
        const float expected = (input - input * input * input / 3.0f) * 1.5f;
        CHECK(waveshaping::cubicSoftClipping(input)
              == Catch::Approx(expected).margin(2.0e-7f));
    }
}

TEST_CASE("Stereo bypass crossfades without a block-boundary click", "[width][bypass]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr float inputValue = 0.25f;

    BandProcessor band;
    band.prepare({ sampleRate, static_cast<juce::uint32>(blockSize), 2 });

    BandProcessingParameters params;
    params.mode = 4; // Hard clipping is transparent for the chosen input.
    params.mixVal = 1.0f;
    params.shapeMixVal = 1.0f;
    params.width = 0.0f; // Pure mid: equal stereo input doubles at full wet.
    params.pan = 0.0f;
    params.widthMixVal = 1.0f;
    params.outputVal.baseValue = 0.0f;

    juce::AudioBuffer<float> audio(2, blockSize);
    juce::AudioBuffer<float> lfoOutputs(4, blockSize);
    lfoOutputs.clear();

    const auto processConstantBlock = [&](bool widthEnabled)
    {
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            juce::FloatVectorOperations::fill(audio.getWritePointer(channel),
                                              inputValue,
                                              blockSize);

        params.isWidthEnabled = widthEnabled;
        band.process(audio, params, lfoOutputs);
    };

    // Prime every mixer in the same state used at plugin startup.
    for (int block = 0; block < 12; ++block)
        processConstantBlock(false);
    CHECK(audio.getSample(0, blockSize - 1) == Catch::Approx(inputValue).margin(1.0e-6f));

    const float beforeEnable = audio.getSample(0, blockSize - 1);
    processConstantBlock(true);
    CHECK(std::abs(audio.getSample(0, 0) - beforeEnable) < 0.02f);

    for (int block = 0; block < 12; ++block)
        processConstantBlock(true);
    CHECK(audio.getSample(0, blockSize - 1) == Catch::Approx(inputValue * 2.0f).margin(1.0e-5f));

    const float beforeDisable = audio.getSample(0, blockSize - 1);
    processConstantBlock(false);
    CHECK(std::abs(audio.getSample(0, 0) - beforeDisable) < 0.02f);

    for (int block = 0; block < 12; ++block)
        processConstantBlock(false);
    CHECK(audio.getSample(0, blockSize - 1) == Catch::Approx(inputValue).margin(1.0e-5f));
}

TEST_CASE("Width and pan automation is smoothed within the audio block", "[width][smoothing]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int rampSamples = 480;
    constexpr float inputValue = 0.25f;

    WidthProcessor processor;
    processor.prepare(sampleRate);

    std::array<float, rampSamples> left {};
    std::array<float, rampSamples> right {};
    const auto fillInput = [&]
    {
        left.fill(inputValue);
        right.fill(inputValue);
    };

    fillInput();
    processor.process(left.data(), right.data(), 0.5f, 0.0f, rampSamples);
    CHECK(left.back() == Catch::Approx(inputValue).margin(1.0e-7f));
    CHECK(right.back() == Catch::Approx(inputValue).margin(1.0e-7f));

    const float widthBoundary = left.back();
    fillInput();
    processor.process(left.data(), right.data(), 0.0f, 0.0f, rampSamples);
    CHECK(std::abs(left.front() - widthBoundary) < 0.002f);
    for (int sample = 1; sample < rampSamples; ++sample)
        CHECK(std::abs(left[static_cast<size_t>(sample)]
                       - left[static_cast<size_t>(sample - 1)]) < 0.002f);
    CHECK(left.back() == Catch::Approx(inputValue * 2.0f).margin(1.0e-6f));
    CHECK(right.back() == Catch::Approx(inputValue * 2.0f).margin(1.0e-6f));

    // Return Width to neutral, then verify a hard pan target is also ramped.
    fillInput();
    processor.process(left.data(), right.data(), 0.5f, 0.0f, rampSamples);
    const float panBoundary = left.back();
    fillInput();
    processor.process(left.data(), right.data(), 0.5f, 1.0f, rampSamples);
    CHECK(std::abs(left.front() - panBoundary) < 0.002f);
    for (int sample = 1; sample < rampSamples; ++sample)
        CHECK(std::abs(left[static_cast<size_t>(sample)]
                       - left[static_cast<size_t>(sample - 1)]) < 0.002f);
    CHECK(left.back() == Catch::Approx(0.0f).margin(1.0e-6f));
    CHECK(right.back() == Catch::Approx(inputValue).margin(1.0e-6f));
}

TEST_CASE("Compressor bypass crossfades without a block-boundary click", "[compressor][bypass]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr float inputValue = 0.5f;

    BandProcessor band;
    band.prepare({ sampleRate, static_cast<juce::uint32>(blockSize), 2 });

    BandProcessingParameters params;
    params.mode = 4;
    params.mixVal = 1.0f;
    params.shapeMixVal = 1.0f;
    params.compThreshold = -40.0f;
    params.compRatio = 20.0f;
    params.compAttack = 0.1f;
    params.compRelease = 100.0f;
    params.compMixVal = 1.0f;
    params.outputVal.baseValue = 0.0f;

    juce::AudioBuffer<float> audio(2, blockSize);
    juce::AudioBuffer<float> lfoOutputs(4, blockSize);
    lfoOutputs.clear();

    const auto processConstantBlock = [&](bool compressorEnabled)
    {
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            juce::FloatVectorOperations::fill(audio.getWritePointer(channel),
                                              inputValue,
                                              blockSize);

        params.isCompEnabled = compressorEnabled;
        band.process(audio, params, lfoOutputs);
    };

    for (int block = 0; block < 16; ++block)
        processConstantBlock(true);
    const float compressedSteadyState = audio.getSample(0, blockSize - 1);
    REQUIRE(compressedSteadyState < inputValue * 0.25f);

    processConstantBlock(false);
    CHECK(std::abs(audio.getSample(0, 0) - compressedSteadyState) < 0.02f);
    for (int block = 0; block < 12; ++block)
        processConstantBlock(false);
    CHECK(audio.getSample(0, blockSize - 1) == Catch::Approx(inputValue).margin(1.0e-5f));

    const float drySteadyState = audio.getSample(0, blockSize - 1);
    processConstantBlock(true);
    CHECK(std::abs(audio.getSample(0, 0) - drySteadyState) < 0.02f);
    for (int block = 0; block < 12; ++block)
        processConstantBlock(true);
    CHECK(audio.getSample(0, blockSize - 1)
          == Catch::Approx(compressedSteadyState).margin(1.0e-4f));
}

TEST_CASE("Processor accepts zero, mono, and larger-than-prepared blocks", "[processor][robustness]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    juce::MidiBuffer midi;

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses.add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(processor.setBusesLayout(monoLayout));
    processor.prepareToPlay(32000.0, 16);

    juce::AudioBuffer<float> emptyBuffer(1, 0);
    REQUIRE_NOTHROW(processor.processBlock(emptyBuffer, midi));

    juce::AudioBuffer<float> monoBuffer(1, 64);
    for (int sample = 0; sample < monoBuffer.getNumSamples(); ++sample)
        monoBuffer.setSample(0, sample, 0.25f * std::sin(0.1f * static_cast<float>(sample)));

    setParameterNormalised(processor, HQ_ID, 1.0f);
    setParameterValue(processor, MIX_ID, 0.5f);
    REQUIRE_NOTHROW(processor.processBlock(monoBuffer, midi));
    CHECK(bufferContainsOnlyFiniteSamples(monoBuffer));
}

TEST_CASE("HQ automation keeps the processor host latency fixed", "[processor][latency]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);

    juce::AudioBuffer<float> buffer(2, 128);
    buffer.clear();
    juce::MidiBuffer midi;

    const auto preparedPhysicalLatency = processor.getTotalLatency();
    const int preparedHostLatency = processor.getLatencySamples();
    REQUIRE(preparedPhysicalLatency > 0.0f);
    REQUIRE(preparedHostLatency > 0);
    CHECK(preparedHostLatency
          == juce::roundToInt(preparedPhysicalLatency));

    setParameterNormalised(processor, HQ_ID, 1.0f);
    processor.processBlock(buffer, midi);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    CHECK(processor.getLatencySamples() == preparedHostLatency);
    CHECK(processor.getTotalLatency()
          == Catch::Approx(preparedPhysicalLatency));

    setParameterNormalised(processor, HQ_ID, 0.0f);
    processor.processBlock(buffer, midi);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    CHECK(processor.getLatencySamples() == preparedHostLatency);
    CHECK(processor.getTotalLatency()
          == Catch::Approx(preparedPhysicalLatency));
}

TEST_CASE("Downsampling state is independent of host block boundaries", "[processor][downsampling]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor wholeProcessor;
    FireAudioProcessor splitProcessor;

    for (auto* processor : { &wholeProcessor, &splitProcessor })
    {
        setParameterNormalised(*processor, DOWNSAMPLE_BYPASS_ID, 1.0f);
        setParameterValue(*processor, DOWNSAMPLE_ID, 7.0f);
        setParameterValue(*processor, BIT_DEPTH_ID, 32.0f);
        setParameterValue(*processor, JITTER_ID, 0.0f);
        setParameterValue(*processor, DOWNSAMPLE_MIX_ID, 1.0f);
        processor->prepareToPlay(48000.0, 64);
    }

    constexpr int totalSamples = 257;
    juce::AudioBuffer<float> input(2, totalSamples);
    for (int channel = 0; channel < input.getNumChannels(); ++channel)
        for (int sample = 0; sample < totalSamples; ++sample)
            input.setSample(channel, sample,
                            0.75f * std::sin(0.071f * static_cast<float>(sample + channel * 3)));

    auto wholeOutput = input;
    juce::MidiBuffer midi;
    wholeProcessor.processBlock(wholeOutput, midi);

    juce::AudioBuffer<float> splitOutput(2, totalSamples);
    splitOutput.clear();
    const std::array<int, 6> blockSizes { 13, 29, 5, 61, 17, 64 };
    int start = 0;
    size_t blockIndex = 0;
    while (start < totalSamples)
    {
        const int blockSize = juce::jmin(blockSizes[blockIndex % blockSizes.size()],
                                         totalSamples - start);
        juce::AudioBuffer<float> block(2, blockSize);
        for (int channel = 0; channel < 2; ++channel)
            block.copyFrom(channel, 0, input, channel, start, blockSize);

        splitProcessor.processBlock(block, midi);
        for (int channel = 0; channel < 2; ++channel)
            splitOutput.copyFrom(channel, start, block, channel, 0, blockSize);

        start += blockSize;
        ++blockIndex;
    }

    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < totalSamples; ++sample)
            CHECK(splitOutput.getSample(channel, sample)
                  == Catch::Approx(wholeOutput.getSample(channel, sample)).margin(1.0e-6f));
}

TEST_CASE("Host bypass retains the fixed latency in base and HQ modes",
          "[processor][latency][bypass]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const bool useHq : { false, true })
    {
        FireAudioProcessor processor;
        setParameterNormalised(processor, HQ_ID, useHq ? 1.0f : 0.0f);
        processor.prepareToPlay(48000.0, 128);

        juce::AudioBuffer<float> impulse(2, 128);
        impulse.clear();
        impulse.setSample(0, 0, 1.0f);
        impulse.setSample(1, 0, 1.0f);
        juce::MidiBuffer midi;
        processor.processBlockBypassed(impulse, midi);

        int peakIndex = 0;
        float peakMagnitude = 0.0f;
        for (int sample = 0; sample < impulse.getNumSamples(); ++sample)
        {
            const float magnitude = std::abs(impulse.getSample(0, sample));
            if (magnitude > peakMagnitude)
            {
                peakMagnitude = magnitude;
                peakIndex = sample;
            }
        }

        CAPTURE(useHq,
                peakIndex,
                processor.getTotalLatency(),
                processor.getLatencySamples());
        REQUIRE(processor.getLatencySamples() > 0);
        CHECK(peakMagnitude > 0.5f);
        CHECK(std::abs(peakIndex - processor.getLatencySamples()) <= 1);
    }
}

TEST_CASE("Neutral three-band crossover keeps a flat summed magnitude", "[processor][crossover]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const float frequency : { 350.0f, 500.0f, 900.0f, 1500.0f, 2000.0f, 2800.0f })
    {
        FireAudioProcessor processor;
        setParameterValue(processor, NUM_BANDS_ID, 3.0f);
        setParameterValue(processor, ParameterIDAndName::getIDString(FREQ_ID, 0), 500.0f);
        setParameterValue(processor, ParameterIDAndName::getIDString(FREQ_ID, 1), 2000.0f);
        for (int band = 0; band < 3; ++band)
            setParameterNormalised(processor,
                                   ParameterIDAndName::getIDString(BAND_ENABLE_ID, band),
                                   0.0f);

        constexpr double sampleRate = 48000.0;
        constexpr int numSamples = 8192;
        processor.prepareToPlay(sampleRate, 512);

        juce::AudioBuffer<float> buffer(2, numSamples);
        double inputEnergy = 0.0;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float value = 0.25f * std::sin(juce::MathConstants<float>::twoPi
                                                 * frequency
                                                 * static_cast<float>(sample)
                                                 / static_cast<float>(sampleRate));
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
            if (sample >= numSamples / 2)
                inputEnergy += static_cast<double>(value) * value;
        }

        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        double outputEnergy = 0.0;
        for (int sample = numSamples / 2; sample < numSamples; ++sample)
        {
            const double value = buffer.getSample(0, sample);
            outputEnergy += value * value;
        }

        const double magnitudeRatio = std::sqrt(outputEnergy / inputEnergy);
        CAPTURE(frequency, magnitudeRatio);
        CHECK(magnitudeRatio == Catch::Approx(1.0).margin(0.035));
    }
}

TEST_CASE("Neutral four-band crossover keeps a flat summed magnitude", "[processor][crossover]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const float frequency : { 350.0f, 500.0f, 750.0f, 1000.0f,
                                   1500.0f, 2000.0f, 2800.0f })
    {
        FireAudioProcessor processor;
        setParameterValue(processor, NUM_BANDS_ID, 4.0f);
        setParameterValue(processor, ParameterIDAndName::getIDString(FREQ_ID, 0), 500.0f);
        setParameterValue(processor, ParameterIDAndName::getIDString(FREQ_ID, 1), 1000.0f);
        setParameterValue(processor, ParameterIDAndName::getIDString(FREQ_ID, 2), 2000.0f);
        for (int band = 0; band < 4; ++band)
            setParameterNormalised(processor,
                                   ParameterIDAndName::getIDString(BAND_ENABLE_ID, band),
                                   0.0f);

        constexpr double sampleRate = 48000.0;
        constexpr int numSamples = 8192;
        processor.prepareToPlay(sampleRate, 512);

        juce::AudioBuffer<float> buffer(2, numSamples);
        double inputEnergy = 0.0;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float value = 0.25f * std::sin(juce::MathConstants<float>::twoPi
                                                 * frequency
                                                 * static_cast<float>(sample)
                                                 / static_cast<float>(sampleRate));
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
            if (sample >= numSamples / 2)
                inputEnergy += static_cast<double>(value) * value;
        }

        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        double outputEnergy = 0.0;
        for (int sample = numSamples / 2; sample < numSamples; ++sample)
        {
            const double value = buffer.getSample(0, sample);
            outputEnergy += value * value;
        }

        const double magnitudeRatio = std::sqrt(outputEnergy / inputEnergy);
        CAPTURE(frequency, magnitudeRatio);
        CHECK(magnitudeRatio == Catch::Approx(1.0).margin(0.035));
    }
}

TEST_CASE("Band solo also isolates the global dry path", "[processor][solo]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor dryMixProcessor;
    FireAudioProcessor wetMixProcessor;

    const auto configure = [](FireAudioProcessor& processor, float globalMix)
    {
        setParameterValue(processor, NUM_BANDS_ID, 2.0f);
        setParameterValue(processor, ParameterIDAndName::getIDString(FREQ_ID, 0), 1000.0f);
        for (int band = 0; band < 2; ++band)
            setParameterNormalised(processor,
                                   ParameterIDAndName::getIDString(BAND_ENABLE_ID, band),
                                   0.0f);
        setParameterNormalised(processor,
                               ParameterIDAndName::getIDString(BAND_SOLO_ID, 0),
                               1.0f);
        setParameterNormalised(processor,
                               ParameterIDAndName::getIDString(BAND_SOLO_ID, 1),
                               0.0f);
        setParameterValue(processor, MIX_ID, globalMix);
        processor.prepareToPlay(48000.0, 512);
    };

    configure(dryMixProcessor, 0.0f);
    configure(wetMixProcessor, 1.0f);

    constexpr int numSamples = 8192;
    juce::AudioBuffer<float> dryOutput(2, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float value = 0.25f * std::sin(juce::MathConstants<float>::twoPi
                                             * 4000.0f
                                             * static_cast<float>(sample)
                                             / 48000.0f);
        dryOutput.setSample(0, sample, value);
        dryOutput.setSample(1, sample, value);
    }
    auto wetOutput = dryOutput;

    juce::MidiBuffer midi;
    dryMixProcessor.processBlock(dryOutput, midi);
    wetMixProcessor.processBlock(wetOutput, midi);

    float maximumDifference = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = numSamples / 2; sample < numSamples; ++sample)
            maximumDifference = juce::jmax(
                maximumDifference,
                std::abs(dryOutput.getSample(channel, sample)
                         - wetOutput.getSample(channel, sample)));

    CHECK(maximumDifference < 1.0e-5f);
}

TEST_CASE("Headless LFO smooth automation reaches the DSP state", "[lfo][state][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(48000.0, 64);

    LfoData automatedShape;
    automatedShape.points = { { 0.0f, 0.0f }, { 0.45f, 1.0f }, { 1.0f, 0.2f } };
    automatedShape.curvatures = { 0.75f, -0.5f };
    automatedShape.smoothness = 0.0f;
    automatedShape.sanitise();
    processor.getLfoManager().setLfoData(1, automatedShape);

    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 1);
    auto* smoothnessParameter = processor.treeState.getParameter(smoothnessID);
    REQUIRE(smoothnessParameter != nullptr);
    smoothnessParameter->setValueNotifyingHost(0.73f);

    juce::AudioBuffer<float> buffer(2, 1);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    const auto lfoData = processor.getLfoManager().getLfoDataCopy();
    REQUIRE(lfoData.size() == 4);
    CHECK(lfoData[1].smoothness == Catch::Approx(0.73f));

    LfoEngine referenceEngine;
    referenceEngine.stageShape(automatedShape);
    referenceEngine.publishStagedShape();
    referenceEngine.prepare({ 48000.0, 1, 1 });
    referenceEngine.setSmoothness(0.73f);
    referenceEngine.setPhase(0.0f);
    referenceEngine.setPhaseDelta(0.0f);
    CHECK(processor.getLfoManager().getLfoOutput(1)
          == Catch::Approx(referenceEngine.process()).margin(2.0e-6f));
}

TEST_CASE("Stopped LFO smoothness survives host and preset state round-trips", "[lfo][state][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    const auto smoothnessID = ParameterIDAndName::getIDString(LFO_SMOOTH_ID, 2);
    auto* smoothnessParameter = source.treeState.getParameter(smoothnessID);
    REQUIRE(smoothnessParameter != nullptr);
    smoothnessParameter->setValueNotifyingHost(0.73f);

    // Deliberately do not run processBlock: this is the stopped-transport path
    // where APVTS used to be newer than the duplicated LFO_STATE value.
    const auto stoppedCopy = source.getLfoManager().getLfoDataCopy();
    REQUIRE(stoppedCopy.size() == 4);
    CHECK(stoppedCopy[2].smoothness == Catch::Approx(0.73f));

    juce::MemoryBlock hostState;
    source.getStateInformation(hostState);
    FireAudioProcessor hostRestored;
    hostRestored.setStateInformation(hostState.getData(), static_cast<int>(hostState.getSize()));
    hostRestored.prepareToPlay(48000.0, 1);
    juce::AudioBuffer<float> hostBuffer(2, 1);
    hostBuffer.clear();
    juce::MidiBuffer hostMidi;
    hostRestored.processBlock(hostBuffer, hostMidi);
    const auto hostRestoredData = hostRestored.getLfoManager().getLfoDataCopy();
    REQUIRE(hostRestoredData.size() == 4);
    CHECK(hostRestoredData[2].smoothness == Catch::Approx(0.73f));

    juce::XmlElement presetState { "PRESET" };
    state::saveStateToXml(source, presetState);
    FireAudioProcessor presetRestored;
    state::loadStateFromXml(presetState, presetRestored);
    presetRestored.prepareToPlay(48000.0, 1);
    juce::AudioBuffer<float> presetBuffer(2, 1);
    presetBuffer.clear();
    juce::MidiBuffer presetMidi;
    presetRestored.processBlock(presetBuffer, presetMidi);
    const auto presetRestoredData = presetRestored.getLfoManager().getLfoDataCopy();
    REQUIRE(presetRestoredData.size() == 4);
    CHECK(presetRestoredData[2].smoothness == Catch::Approx(0.73f));
}

TEST_CASE("Linked output compensation works without an editor", "[processor][link][headless]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor linkedProcessor;
    FireAudioProcessor explicitProcessor;

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto outputID = ParameterIDAndName::getIDString(OUTPUT_ID, 0);
    const auto linkedID = ParameterIDAndName::getIDString(LINKED_ID, 0);

    for (auto* processor : { &linkedProcessor, &explicitProcessor })
        setParameterValue(*processor, driveID, 30.0f);

    setParameterValue(linkedProcessor, linkedID, 1.0f);
    setParameterValue(linkedProcessor, outputID, 6.0f); // Must be ignored while linked.
    setParameterValue(explicitProcessor, linkedID, 0.0f);
    setParameterValue(explicitProcessor, outputID, -3.0f);

    linkedProcessor.prepareToPlay(48000.0, 256);
    explicitProcessor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> linkedBuffer(2, 256);
    for (int channel = 0; channel < linkedBuffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < linkedBuffer.getNumSamples(); ++sample)
            linkedBuffer.setSample(channel,
                                   sample,
                                   0.2f * std::sin(juce::MathConstants<float>::twoPi
                                                   * 440.0f * static_cast<float>(sample) / 48000.0f));

    juce::AudioBuffer<float> explicitBuffer;
    explicitBuffer.makeCopyOf(linkedBuffer);
    juce::MidiBuffer linkedMidi;
    juce::MidiBuffer explicitMidi;
    linkedProcessor.processBlock(linkedBuffer, linkedMidi);
    explicitProcessor.processBlock(explicitBuffer, explicitMidi);

    float maximumDifference = 0.0f;
    for (int channel = 0; channel < linkedBuffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < linkedBuffer.getNumSamples(); ++sample)
            maximumDifference = juce::jmax(maximumDifference,
                                           std::abs(linkedBuffer.getSample(channel, sample)
                                                    - explicitBuffer.getSample(channel, sample)));

    CHECK(maximumDifference < 1.0e-6f);
    const auto* linkedOutput = linkedProcessor.treeState.getRawParameterValue(outputID);
    REQUIRE(linkedOutput != nullptr);
    CHECK(linkedOutput->load() == Catch::Approx(6.0f));
}

TEST_CASE("Opening an editor does not overwrite stored output while Linked is active",
          "[processor][link][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto outputID = ParameterIDAndName::getIDString(OUTPUT_ID, 0);
    const auto linkedID = ParameterIDAndName::getIDString(LINKED_ID, 0);

    setParameterValue(processor, driveID, 30.0f);
    setParameterValue(processor, outputID, 6.0f);
    setParameterValue(processor, linkedID, 1.0f);

    const auto* storedOutput = processor.treeState.getRawParameterValue(outputID);
    REQUIRE(storedOutput != nullptr);
    REQUIRE(storedOutput->load() == Catch::Approx(6.0f));

    std::unique_ptr<juce::AudioProcessorEditor> editor { processor.createEditor() };
    REQUIRE(editor != nullptr);

    setParameterValue(processor, driveID, 40.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    CHECK(storedOutput->load() == Catch::Approx(6.0f));

    setParameterValue(processor, linkedID, 0.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    CHECK(storedOutput->load() == Catch::Approx(6.0f));
}

TEST_CASE("State round-trip preserves LFO data and upgrades legacy shape state", "[state][lfo]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;

    LfoData customShape;
    customShape.points = { { 0.0f, 0.2f }, { 0.5f, 0.9f }, { 1.0f, 0.3f } };
    customShape.curvatures = { 0.25f, -0.5f };
    customShape.smoothness = 0.4f;
    source.getLfoManager().setLfoData(2, customShape);
    source.assignLfoToTarget(2, ParameterIDAndName::getIDString(DRIVE_ID, 0));

    juce::MemoryBlock savedState;
    source.getStateInformation(savedState);

    FireAudioProcessor restored;
    restored.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));

    // Exercise the audio-thread snapshot refresh as well. Historically this
    // first block overwrote XML-restored smoothness with the APVTS default.
    restored.prepareToPlay(48000.0, 64);
    juce::AudioBuffer<float> restoredBuffer(2, 64);
    restoredBuffer.clear();
    juce::MidiBuffer restoredMidi;
    restored.processBlock(restoredBuffer, restoredMidi);

    const auto restoredLfos = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredLfos.size() == 4);
    REQUIRE(restoredLfos[2].points.size() == 3);
    CHECK(restoredLfos[2].points[1].x == Catch::Approx(0.5f));
    CHECK(restoredLfos[2].points[1].y == Catch::Approx(0.9f));
    CHECK(restoredLfos[2].smoothness == Catch::Approx(0.4f));

    // Some older states contain an LFO shape but predate its smoothness XML
    // attribute. In that case the APVTS smoothness parameter is authoritative.
    auto legacySmoothXml = juce::AudioProcessor::getXmlFromBinary(
        savedState.getData(), static_cast<int>(savedState.getSize()));
    REQUIRE(legacySmoothXml != nullptr);
    auto* legacyLfoState = legacySmoothXml->getChildByName("LFO_STATE");
    REQUIRE(legacyLfoState != nullptr);
    bool removedSmoothness = false;
    for (auto* lfoXml : legacyLfoState->getChildIterator())
    {
        if (lfoXml->getIntAttribute("index", -1) == 2)
        {
            lfoXml->removeAttribute("smoothness");
            removedSmoothness = true;
            break;
        }
    }
    REQUIRE(removedSmoothness);

    juce::MemoryBlock legacySmoothState;
    juce::AudioProcessor::copyXmlToBinary(*legacySmoothXml, legacySmoothState);
    FireAudioProcessor legacySmoothRestored;
    legacySmoothRestored.setStateInformation(legacySmoothState.getData(),
                                             static_cast<int>(legacySmoothState.getSize()));
    legacySmoothRestored.prepareToPlay(48000.0, 64);
    juce::AudioBuffer<float> legacySmoothBuffer(2, 64);
    legacySmoothBuffer.clear();
    juce::MidiBuffer legacySmoothMidi;
    legacySmoothRestored.processBlock(legacySmoothBuffer, legacySmoothMidi);
    const auto legacySmoothLfos = legacySmoothRestored.getLfoManager().getLfoDataCopy();
    REQUIRE(legacySmoothLfos.size() == 4);
    CHECK(legacySmoothLfos[2].smoothness == Catch::Approx(0.4f));

    const auto restoredRoutings = restored.getLfoManager().getModulationRoutingsCopy();
    const auto targetID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto matchingRouting = std::find_if(restoredRoutings.begin(), restoredRoutings.end(), [&](const auto& routing)
                                              { return routing.targetParameterID == targetID; });
    REQUIRE(matchingRouting != restoredRoutings.end());
    CHECK(matchingRouting->sourceLfoIndex == 2);

    const auto* modernShapeState = restored.treeState.getRawParameterValue(
        ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, 0));
    REQUIRE(modernShapeState != nullptr);
    CHECK(modernShapeState->load() <= 0.5f);

    // Simulate an older state written before the per-band shape enable
    // parameters existed. Loading must enable Shape to preserve the old sound.
    auto legacyParameterState = source.treeState.copyState();
    for (int band = 0; band < 4; ++band)
    {
        const auto parameterID = ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, band);
        for (int childIndex = legacyParameterState.getNumChildren() - 1;
             childIndex >= 0;
             --childIndex)
        {
            const auto child = legacyParameterState.getChild(childIndex);
            if (child.getProperty("id").toString() == parameterID)
                legacyParameterState.removeChild(childIndex, nullptr);
        }
    }

    juce::XmlElement legacyRoot { "state" };
    legacyRoot.addChildElement(legacyParameterState.createXml().release());
    juce::MemoryBlock legacyBinary;
    juce::AudioProcessor::copyXmlToBinary(legacyRoot, legacyBinary);

    FireAudioProcessor legacyRestored;
    legacyRestored.setStateInformation(
        legacyBinary.getData(), static_cast<int>(legacyBinary.getSize()));

    for (int band = 0; band < 4; ++band)
    {
        const auto* shapeEnabled = legacyRestored.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, band));
        REQUIRE(shapeEnabled != nullptr);
        CHECK(shapeEnabled->load() > 0.5f);
    }
}
