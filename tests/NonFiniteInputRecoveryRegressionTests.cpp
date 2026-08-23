#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 64;
constexpr int hostBlockSize = 257;
constexpr int injectionSample = hostBlockSize / 2;
constexpr int warmupCallbacks = 16;
constexpr int recoveryCallbacks = 12;
constexpr float comparisonTolerance = 1.0e-6f;

struct NonFiniteProbe
{
    const char* name;
    float value;
};

const std::array nonFiniteProbes {
    NonFiniteProbe { "NaN", std::numeric_limits<float>::quiet_NaN() },
    NonFiniteProbe { "+Inf", std::numeric_limits<float>::infinity() },
    NonFiniteProbe { "-Inf", -std::numeric_limits<float>::infinity() }
};

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void setLayout(FireAudioProcessor& processor, int numChannels)
{
    REQUIRE((numChannels == 1 || numChannels == 2));
    const auto channelSet = numChannels == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    REQUIRE(processor.setBusesLayout(layout));
}

juce::String bandParameter(const juce::String& baseID, int band)
{
    return ParameterIDAndName::getIDString(baseID, band);
}

void configureProcessor(FireAudioProcessor& processor,
                        bool useHq,
                        int numChannels,
                        int numBands)
{
    REQUIRE((numBands == 1 || numBands == 4));
    setLayout(processor, numChannels);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, -1.5f);
    setPlainParameter(processor, NUM_BANDS_ID, static_cast<float>(numBands));

    // A resonant recursive global stage makes even a one-sample poisoned state
    // persist. The finite reference receives silence at exactly that sample.
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOWCUT_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 1.0f);
    setPlainParameter(processor, PEAK_BYPASSED_ID, 0.0f);
    setPlainParameter(processor, PEAK_FREQ_ID, 1837.0f);
    setPlainParameter(processor, PEAK_GAIN_ID, 20.0f);
    setPlainParameter(processor, PEAK_Q_ID, 5.0f);

    constexpr std::array crossoverFrequencies { 260.0f, 1850.0f, 7350.0f };
    for (int crossover = 0; crossover < 3; ++crossover)
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, crossover),
                          crossoverFrequencies[static_cast<size_t>(crossover)]);

    for (int band = 0; band < 4; ++band)
    {
        setPlainParameter(processor, bandParameter(BAND_ENABLE_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(BAND_SOLO_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(LINKED_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(SAFE_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(EXTREME_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(SHAPE_BYPASS_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(COMP_BYPASS_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(WIDTH_BYPASS_ID, band), 0.0f);
        setPlainParameter(processor, bandParameter(DC_FILTER_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(MODE_ID, band), 2.0f);
        setPlainParameter(processor, bandParameter(BIAS_ID, band), 0.07f);
        setPlainParameter(processor, bandParameter(REC_ID, band), 0.10f);
        setPlainParameter(processor, bandParameter(SHAPE_MIX_ID, band), 1.0f);
        setPlainParameter(processor, bandParameter(OUTPUT_ID, band), -2.0f);
        setPlainParameter(processor, bandParameter(MIX_ID, band), 1.0f);

        // Exercise the detector and its recursive attack/release envelope.
        setPlainParameter(processor, bandParameter(COMP_THRESH_ID, band), -30.0f);
        setPlainParameter(processor, bandParameter(COMP_RATIO_ID, band), 20.0f);
        setPlainParameter(processor, bandParameter(COMP_ATTACK_ID, band), 0.1f);
        setPlainParameter(processor, bandParameter(COMP_RELEASE_ID, band), 80.0f);
        setPlainParameter(processor, bandParameter(COMP_MIX_ID, band), 1.0f);
    }

    // Deliberately advertise less than one host callback so the contract is
    // covered when a host exceeds its prepareToPlay block-size hint.
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

juce::AudioBuffer<float> makeInput(int numChannels,
                                    int firstAbsoluteSample)
{
    juce::AudioBuffer<float> buffer(numChannels, hostBlockSize);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const double channelPhase = channel == 0 ? 0.19 : 0.73;
        for (int sample = 0; sample < hostBlockSize; ++sample)
        {
            const double time = static_cast<double>(firstAbsoluteSample + sample)
                                / sampleRate;
            const float value = 0.065f
                              + 0.24f * static_cast<float>(std::sin(
                                    juce::MathConstants<double>::twoPi
                                    * 173.0 * time + channelPhase))
                              + 0.14f * static_cast<float>(std::cos(
                                    juce::MathConstants<double>::twoPi
                                    * 1837.0 * time + 0.31 - channelPhase))
                              + 0.07f * static_cast<float>(std::sin(
                                    juce::MathConstants<double>::twoPi
                                    * 7919.0 * time + 0.87 + channelPhase));
            buffer.setSample(channel, sample, value);
        }
    }
    return buffer;
}

struct BufferComparison
{
    bool subjectFinite = true;
    bool referenceFinite = true;
    float maximumDifference = 0.0f;
    float referencePeak = 0.0f;
};

BufferComparison compareBuffers(const juce::AudioBuffer<float>& subject,
                                const juce::AudioBuffer<float>& reference)
{
    REQUIRE(subject.getNumChannels() == reference.getNumChannels());
    REQUIRE(subject.getNumSamples() == reference.getNumSamples());

    BufferComparison result;
    for (int channel = 0; channel < subject.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < subject.getNumSamples(); ++sample)
        {
            const float actual = subject.getSample(channel, sample);
            const float expected = reference.getSample(channel, sample);
            const bool actualFinite = std::isfinite(actual);
            const bool expectedFinite = std::isfinite(expected);
            result.subjectFinite = result.subjectFinite && actualFinite;
            result.referenceFinite = result.referenceFinite && expectedFinite;
            if (actualFinite && expectedFinite)
            {
                result.maximumDifference = std::max(result.maximumDifference,
                                                     std::abs(actual - expected));
                result.referencePeak = std::max(result.referencePeak,
                                                std::abs(expected));
            }
        }
    }
    return result;
}

void requireEquivalent(const BufferComparison& comparison)
{
    REQUIRE(comparison.referenceFinite);
    REQUIRE(comparison.subjectFinite);
    REQUIRE(comparison.maximumDifference < comparisonTolerance);
}

BufferComparison processPair(FireAudioProcessor& subject,
                             FireAudioProcessor& reference,
                             int numChannels,
                             int firstAbsoluteSample,
                             bool hostBypass,
                             const NonFiniteProbe* injection)
{
    auto referenceBuffer = makeInput(numChannels, firstAbsoluteSample);
    if (injection != nullptr)
        referenceBuffer.setSample(numChannels - 1, injectionSample, 0.0f);

    auto subjectBuffer = referenceBuffer;
    if (injection != nullptr)
        subjectBuffer.setSample(numChannels - 1,
                                injectionSample,
                                injection->value);

    juce::MidiBuffer subjectMidi;
    juce::MidiBuffer referenceMidi;
    if (hostBypass)
    {
        subject.processBlockBypassed(subjectBuffer, subjectMidi);
        reference.processBlockBypassed(referenceBuffer, referenceMidi);
    }
    else
    {
        subject.processBlock(subjectBuffer, subjectMidi);
        reference.processBlock(referenceBuffer, referenceMidi);
    }
    return compareBuffers(subjectBuffer, referenceBuffer);
}

bool meterValuesAreFinite(const MeterValues& values)
{
    const std::array globalValues {
        values.inputRMS_L, values.inputRMS_R,
        values.inputPeak_L, values.inputPeak_R,
        values.outputRMS_L, values.outputRMS_R,
        values.outputPeak_L, values.outputPeak_R
    };
    for (const float value : globalValues)
        if (! std::isfinite(value))
            return false;

    const auto arrayIsFinite = [] (const auto& array)
    {
        return std::all_of(array.begin(), array.end(), [] (float value)
        {
            return std::isfinite(value);
        });
    };
    return arrayIsFinite(values.bandInputRMS_L)
        && arrayIsFinite(values.bandInputRMS_R)
        && arrayIsFinite(values.bandInputPeak_L)
        && arrayIsFinite(values.bandInputPeak_R)
        && arrayIsFinite(values.bandOutputRMS_L)
        && arrayIsFinite(values.bandOutputRMS_R)
        && arrayIsFinite(values.bandOutputPeak_L)
        && arrayIsFinite(values.bandOutputPeak_R);
}

void requireLatestMeterFinite(FireAudioProcessor& processor)
{
    MeterValues values;
    REQUIRE(processor.getLatestMeterValues(values));
    REQUIRE(meterValuesAreFinite(values));
}
} // namespace

TEST_CASE("A runtime non-finite input sample is equivalent to silence without poisoning DSP state",
          "[processor][robustness][non-finite][recovery]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const auto& probe : nonFiniteProbes)
    {
        for (const bool useHq : std::array { false, true })
        {
            for (const int numChannels : std::array { 1, 2 })
            {
                for (const int numBands : std::array { 1, 4 })
                {
                    DYNAMIC_SECTION(probe.name << ", HQ=" << useHq
                                    << ", channels=" << numChannels
                                    << ", bands=" << numBands)
                    {
                        FireAudioProcessor subject;
                        FireAudioProcessor reference;
                        configureProcessor(subject, useHq, numChannels, numBands);
                        configureProcessor(reference, useHq, numChannels, numBands);

                        int streamPosition = 0;
                        for (int callback = 0; callback < warmupCallbacks; ++callback)
                        {
                            CAPTURE(callback);
                            requireEquivalent(processPair(subject,
                                                          reference,
                                                          numChannels,
                                                          streamPosition,
                                                          false,
                                                          nullptr));
                            streamPosition += hostBlockSize;
                        }

                        const auto pollutedCallback = processPair(subject,
                                                                  reference,
                                                                  numChannels,
                                                                  streamPosition,
                                                                  false,
                                                                  &probe);
                        streamPosition += hostBlockSize;
                        requireEquivalent(pollutedCallback);

                        float recoveryReferencePeak = pollutedCallback.referencePeak;
                        for (int callback = 0; callback < recoveryCallbacks; ++callback)
                        {
                            CAPTURE(callback);
                            const auto recovered = processPair(subject,
                                                               reference,
                                                               numChannels,
                                                               streamPosition,
                                                               false,
                                                               nullptr);
                            streamPosition += hostBlockSize;
                            requireEquivalent(recovered);
                            recoveryReferencePeak = std::max(recoveryReferencePeak,
                                                             recovered.referencePeak);
                        }
                        REQUIRE(recoveryReferencePeak > 0.01f);
                    }
                }
            }
        }
    }
}

TEST_CASE("Host bypass sanitises non-finite input before its delayed tap and hidden HQ graph",
          "[processor][robustness][non-finite][host-bypass][meter][hq]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const auto& probe : nonFiniteProbes)
    {
        DYNAMIC_SECTION(probe.name)
        {
            constexpr int numChannels = 2;
            constexpr int numBands = 4;
            FireAudioProcessor subject;
            FireAudioProcessor reference;
            configureProcessor(subject, true, numChannels, numBands);
            configureProcessor(reference, true, numChannels, numBands);
            REQUIRE(subject.getLatencySamples() > 0);
            REQUIRE(subject.getLatencySamples() == reference.getLatencySamples());

            int streamPosition = 0;
            for (int callback = 0; callback < warmupCallbacks; ++callback)
            {
                requireEquivalent(processPair(subject,
                                              reference,
                                              numChannels,
                                              streamPosition,
                                              false,
                                              nullptr));
                streamPosition += hostBlockSize;
            }
            requireLatestMeterFinite(subject);

            requireEquivalent(processPair(subject,
                                          reference,
                                          numChannels,
                                          streamPosition,
                                          true,
                                          &probe));
            streamPosition += hostBlockSize;
            requireLatestMeterFinite(subject);

            // Keep bypass active until the injected sample has crossed the
            // complete reported delay, while the hidden HQ graph also consumes
            // every sanitized callback on its live timeline.
            const int bypassContinuationCallbacks =
                (subject.getLatencySamples() + hostBlockSize - 1)
                    / hostBlockSize
                + 3;
            for (int callback = 0;
                 callback < bypassContinuationCallbacks;
                 ++callback)
            {
                CAPTURE(callback);
                requireEquivalent(processPair(subject,
                                              reference,
                                              numChannels,
                                              streamPosition,
                                              true,
                                              nullptr));
                streamPosition += hostBlockSize;
                requireLatestMeterFinite(subject);
            }

            // Leaving host bypass exposes any poisoned recursive shadow state
            // immediately; every finite recovery block must remain canonical.
            float recoveryReferencePeak = 0.0f;
            for (int callback = 0; callback < recoveryCallbacks; ++callback)
            {
                CAPTURE(callback);
                const auto recovered = processPair(subject,
                                                   reference,
                                                   numChannels,
                                                   streamPosition,
                                                   false,
                                                   nullptr);
                streamPosition += hostBlockSize;
                requireEquivalent(recovered);
                requireLatestMeterFinite(subject);
                recoveryReferencePeak = std::max(recoveryReferencePeak,
                                                 recovered.referencePeak);
            }
            REQUIRE(recoveryReferencePeak > 0.01f);
        }
    }
}
