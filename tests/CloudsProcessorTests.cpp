#include <PluginProcessor.h>
#include <Utility/CloudsParameters.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
namespace fx = fire::effects;
namespace clouds = fire::clouds_params;
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

void setPlain(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void configure(FireAudioProcessor& processor, int scope)
{
    processor.hasUpdateCheckBeenPerformed = true;
    setPlain(processor, NUM_BANDS_ID, 1.0f);
    setPlain(processor, HQ_ID, 0.0f);
    setPlain(processor, FILTER_BYPASS_ID, 0.0f);
    setPlain(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlain(processor, OUTPUT_ID, 0.0f);
    setPlain(processor, MIX_ID, 1.0f);
    const auto band = [&](const juce::String& id, float value)
    { setPlain(processor, ParameterIDAndName::getIDString(id, 0), value); };
    band(BAND_ENABLE_ID, 1.0f);
    band(BAND_SOLO_ID, 0.0f);
    band(LINKED_ID, 0.0f);
    band(DRIVE_BYPASS_ID, 0.0f);
    band(SHAPE_BYPASS_ID, 0.0f);
    band(COMP_BYPASS_ID, 0.0f);
    band(WIDTH_BYPASS_ID, 0.0f);
    band(OTT_ENABLED_ID, 0.0f);
    band(DC_FILTER_ID, 0.0f);
    band(MODE_ID, 4.0f); // A linear hard-clip region for this low-level probe.
    band(OUTPUT_ID, 0.0f);
    band(MIX_ID, 1.0f);
    REQUIRE(processor.addInsertEffect(scope, fx::Type::granular) == 0);
    const auto* engine = processor.treeState.getRawParameterValue(
        clouds::parameterID(scope, 0, clouds::engineField));
    REQUIRE(engine != nullptr);
    // Original insert controls are stored in normalised 0..1 units.
    setPlain(processor, fx::parameterID(scope, 0, 0), 0.25f);
    setPlain(processor, fx::parameterID(scope, 0, 5), 1.0f);
}

void prepare(FireAudioProcessor& processor)
{
    processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);
}

std::vector<float> render(FireAudioProcessor& processor, int samples, int& timeline, bool silence = false)
{
    juce::AudioBuffer<float> storage(2, blockSize);
    juce::MidiBuffer midi;
    std::vector<float> output(static_cast<size_t>(samples) * 2);
    bool finite = true;
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const auto count = std::min(blockSize, samples - offset);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < count; ++sample)
            {
                const double time = static_cast<double>(timeline + sample) / sampleRate;
                const double phase = static_cast<double>(channel) * 0.31;
                const auto value = silence ? 0.0f : static_cast<float>(
                    0.12 * std::sin(juce::MathConstants<double>::twoPi * 223.0 * time + phase)
                    + 0.04 * std::cos(juce::MathConstants<double>::twoPi * 997.0 * time - phase));
                storage.setSample(channel, sample, value);
            }
        juce::AudioBuffer<float> block(storage.getArrayOfWritePointers(), 2, count);
        processor.processBlock(block, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < count; ++sample)
            {
                const auto value = block.getSample(channel, sample);
                finite = finite && std::isfinite(value);
                output[static_cast<size_t>(offset + sample) * 2 + static_cast<size_t>(channel)] = value;
            }
        timeline += count;
    }
    REQUIRE(finite);
    return output;
}

double meanSquareDifference(const std::vector<float>& first, const std::vector<float>& second)
{
    REQUIRE(first.size() == second.size());
    // Inspect the settled half so an initial enable bridge cannot
    // masquerade as a working parameter capture or modulation route.
    const auto begin = first.size() / 2;
    double sum = 0.0;
    for (size_t sample = begin; sample < first.size(); ++sample)
    {
        const auto difference = static_cast<double>(first[sample]) - second[sample];
        sum += difference * difference;
    }
    return sum / static_cast<double>(first.size() - begin);
}
}

TEST_CASE("Retired granular engine values cannot change Master or Band DSP", "[clouds][processor][capture][audio][compatibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        FireAudioProcessor subject, reference;
        configure(subject, scope);
        configure(reference, scope);
        const auto retiredEngine = clouds::parameterID(scope, 0, clouds::engineField);
        setPlain(subject, retiredEngine, 0.0f);
        setPlain(reference, retiredEngine, 1.0f);
        prepare(subject);
        prepare(reference);
        int subjectTimeline = 0, referenceTimeline = 0;
        const auto actual = render(subject, 72000, subjectTimeline);
        const auto expected = render(reference, 72000, referenceTimeline);
        CHECK(std::equal(actual.begin(), actual.end(), expected.begin()));
        CHECK(std::any_of(actual.begin(), actual.end(), [](float value) { return std::abs(value) > 0.001f; }));
        // Old automation can still write the reserved ID. It must neither
        // choose another processor nor restart a live recording/fade.
        for (float retiredValue : {1.0f, 0.0f})
        {
            CAPTURE(retiredValue);
            setPlain(subject, retiredEngine, retiredValue);
            const auto continued = render(subject, 8192, subjectTimeline);
            const auto unchanged = render(reference, 8192, referenceTimeline);
            CHECK(std::equal(continued.begin(), continued.end(), unchanged.begin()));
        }
    }
}

TEST_CASE("Processor callback captures appended Clouds feedback reverb and extra LFO recipes", "[clouds][processor][capture][lfo]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int variant = 0; variant < 3; ++variant)
    {
        CAPTURE(variant);
        FireAudioProcessor subject, unmodulated;
        configure(subject, 0);
        configure(unmodulated, 0);
        const auto reverb = clouds::parameterID(0, 0, clouds::reverbField);
        if (variant == 0)
            setPlain(subject, clouds::parameterID(0, 0, clouds::feedbackField), 0.85f);
        else if (variant == 1)
            setPlain(subject, reverb, 0.9f);
        else
        {
            LfoData shape;
            shape.points = {{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}};
            shape.curvatures = {0.0f, 0.0f};
            subject.getLfoManager().setLfoData(0, shape);
            setPlain(subject, ParameterIDAndName::getIDString(LFO_SYNC_MODE_ID, 0), 0.0f);
            setPlain(subject, ParameterIDAndName::getIDString(LFO_RATE_HZ_ID, 0), 2.0f);
            REQUIRE(subject.assignLfoToTarget(0, reverb) == LfoManager::AssignmentResult::changed);
            subject.setModulationDepth(reverb, 1.0f);
            REQUIRE(subject.getModulationInfoForParameter(reverb).isModulated);
        }
        // Install recipes before preparing either processor; their audio
        // histories begin together, so a route-induced reset cannot pass this.
        prepare(subject);
        prepare(unmodulated);
        int subjectTimeline = 0, referenceTimeline = 0;
        const auto actual = render(subject, 72000, subjectTimeline);
        const auto reference = render(unmodulated, 72000, referenceTimeline);
        CHECK(meanSquareDifference(actual, reference) > 1.0e-7);
    }
}

TEST_CASE("Processor APVTS Freeze holds a nonzero Master Clouds history despite new live input", "[clouds][processor][capture][freeze]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor silenceInput, continuedInput;
    configure(silenceInput, 0);
    configure(continuedInput, 0);
    prepare(silenceInput);
    prepare(continuedInput);
    int firstTimeline = 0, secondTimeline = 0;
    const auto firstWarmup = render(silenceInput, 36000, firstTimeline);
    const auto secondWarmup = render(continuedInput, 36000, secondTimeline);
    REQUIRE(std::equal(firstWarmup.begin(), firstWarmup.end(), secondWarmup.begin()));
    const auto freeze = clouds::parameterID(0, 0, clouds::freezeField);
    setPlain(silenceInput, freeze, 1.0f);
    setPlain(continuedInput, freeze, 1.0f);
    render(silenceInput, 1536, firstTimeline);
    render(continuedInput, 1536, secondTimeline);
    const auto frozen = render(silenceInput, 24000, firstTimeline, true);
    const auto live = render(continuedInput, 24000, secondTimeline);
    float maximumDifference = 0.0f;
    double frozenEnergy = 0.0;
    for (size_t sample = 0; sample < frozen.size(); ++sample)
    {
        maximumDifference = std::max(maximumDifference, std::abs(frozen[sample] - live[sample]));
        frozenEnergy += static_cast<double>(frozen[sample]) * frozen[sample];
    }
    CHECK(frozenEnergy > 0.01);
    // Fire's outer wet mix still performs float arithmetic with the dry
    // input; allow its rounding without allowing live audio to leak through.
    CHECK(maximumDifference < 1.0e-6f);
}
