#include "../Source/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr double hostBpm = 120.0;
    constexpr int preparedBlockSize = 257;
    constexpr int warmupSamples = 8192;
    constexpr int bypassHalfSamples = 4096;
    constexpr int recoveryProbeSamples = 1536;
    constexpr int finalSettlingSamples = 32768;
    constexpr int comparisonGuardSamples = 64;

    class TimelinePlayHead final : public juce::AudioPlayHead
    {
    public:
        juce::Optional<PositionInfo> getPosition() const override
        {
            return position;
        }

        void setCallbackStart(int absoluteTimelineSample)
        {
            position = {};
            position.setIsPlaying(true);
            position.setBpm(hostBpm);
            position.setTimeSignature(TimeSignature { 4, 4 });
            position.setTimeInSamples(static_cast<juce::int64>(
                absoluteTimelineSample));
            position.setTimeInSeconds(static_cast<double>(absoluteTimelineSample)
                                      / sampleRate);
            position.setPpqPosition(static_cast<double>(absoluteTimelineSample)
                                    * hostBpm / (60.0 * sampleRate));
        }

    private:
        PositionInfo position;
    };

    void setPlainParameter(FireAudioProcessor& processor,
                           const juce::String& parameterID,
                           float plainValue)
    {
        auto* parameter = processor.treeState.getParameter(parameterID);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(
            parameter->getNormalisableRange().convertTo0to1(plainValue));
    }

    void setLayout(FireAudioProcessor& processor, int numChannels)
    {
        REQUIRE((numChannels == 1 || numChannels == 2));
        juce::AudioProcessor::BusesLayout layout;
        const auto channelSet = numChannels == 1
                                    ? juce::AudioChannelSet::mono()
                                    : juce::AudioChannelSet::stereo();
        layout.inputBuses.add(channelSet);
        layout.outputBuses.add(channelSet);
        REQUIRE(processor.setBusesLayout(layout));
    }

    juce::String lfoParameter(const juce::String& baseID)
    {
        return ParameterIDAndName::getIDString(baseID, 0);
    }

    LfoData makeFilterProbeShape()
    {
        LfoData shape;
        shape.points = {
            { 0.0f, 0.5f },
            { 0.25f, 1.0f },
            { 0.75f, 0.0f },
            { 1.0f, 0.5f }
        };
        shape.curvatures = { 0.0f, 0.0f, 0.0f };
        shape.smoothness = 0.0f;
        shape.sanitise();
        return shape;
    }

    void configureProcessor(FireAudioProcessor& processor,
                            bool useHq,
                            int numChannels)
    {
        setLayout(processor, numChannels);
        setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
        setPlainParameter(processor, MIX_ID, 1.0f);
        setPlainParameter(processor, OUTPUT_ID, 0.0f);
        setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
        setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                          0.0f);

        // Keep only one deliberately resonant recursive stage active. The filter
        // receives a new base gain while the host is bypassed, and its gain also
        // follows a transport-anchored LFO, so a correct hidden wet render must
        // consume the audio, parameter automation, and playhead timeline together.
        setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
        setPlainParameter(processor, LOWCUT_BYPASSED_ID, 1.0f);
        setPlainParameter(processor, HIGHCUT_BYPASSED_ID, 1.0f);
        setPlainParameter(processor, PEAK_BYPASSED_ID, 0.0f);
        setPlainParameter(processor, PEAK_FREQ_ID, 1837.0f);
        setPlainParameter(processor, PEAK_Q_ID, 5.0f);
        setPlainParameter(processor, PEAK_GAIN_ID, 15.0f);

        setPlainParameter(processor, lfoParameter(LFO_SYNC_MODE_ID), 1.0f);
        setPlainParameter(processor, lfoParameter(LFO_RATE_SYNC_ID), 8.0f);
        setPlainParameter(processor, lfoParameter(LFO_PHASE_ID), 0.17f);
        setPlainParameter(processor, lfoParameter(LFO_SMOOTH_ID), 0.0f);
        processor.getLfoManager().setLfoData(0, makeFilterProbeShape());
        processor.assignLfoToTarget(0, PEAK_GAIN_ID);
        processor.setModulationDepth(PEAK_GAIN_ID, 0.18f);

        processor.prepareToPlay(sampleRate, preparedBlockSize);
    }

    juce::AudioBuffer<float> makeProbeInput(int firstStreamSample,
                                            int numSamples,
                                            bool evolvedInput,
                                            int numChannels)
    {
        juce::AudioBuffer<float> input(numChannels, numSamples);
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const double absoluteSample = static_cast<double>(firstStreamSample
                                                                  + sample);
                const double channelPhase = static_cast<double>(channel) * 0.31;
                const double resonantPhase = juce::MathConstants<double>::twoPi
                                             * 1837.0 * absoluteSample / sampleRate;
                const double secondaryPhase = juce::MathConstants<double>::twoPi
                                              * 317.0 * absoluteSample / sampleRate;
                const double lowPhase = juce::MathConstants<double>::twoPi
                                        * 37.0 * absoluteSample / sampleRate;

                const float value = evolvedInput
                                        ? 0.11f
                                              + 0.31f * static_cast<float>(std::sin(resonantPhase + 0.43 + channelPhase))
                                              + 0.17f * static_cast<float>(std::cos(secondaryPhase + 0.71 + channelPhase))
                                              + 0.09f * static_cast<float>(std::sin(lowPhase + 0.23 + channelPhase))
                                        : 0.24f * static_cast<float>(std::sin(resonantPhase + 0.13 + channelPhase))
                                              + 0.08f * static_cast<float>(std::cos(secondaryPhase + 0.29 + channelPhase));
                input.setSample(channel, sample, value);
            }
        }
        return input;
    }

    struct RenderedTriplet
    {
        juce::AudioBuffer<float> subject;
        juce::AudioBuffer<float> continuous;
        juce::AudioBuffer<float> frozen;
    };

    bool allFinite(const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (! std::isfinite(buffer.getSample(channel, sample)))
                    return false;
        return true;
    }

    float maximumDifferenceInRange(const juce::AudioBuffer<float>& first,
                                   const juce::AudioBuffer<float>& second,
                                   int firstSample,
                                   int numSamples)
    {
        REQUIRE(first.getNumChannels() == second.getNumChannels());
        REQUIRE(first.getNumSamples() == second.getNumSamples());
        REQUIRE(firstSample >= 0);
        REQUIRE(numSamples >= 0);
        REQUIRE(firstSample + numSamples <= first.getNumSamples());

        float maximumDifference = 0.0f;
        for (int channel = 0; channel < first.getNumChannels(); ++channel)
            for (int sample = firstSample;
                 sample < firstSample + numSamples;
                 ++sample)
                maximumDifference = std::max(
                    maximumDifference,
                    std::abs(first.getSample(channel, sample)
                             - second.getSample(channel, sample)));
        return maximumDifference;
    }
} // namespace

TEST_CASE("Host bypass keeps the complete wet DSP graph on the live timeline",
          "[processor][host-bypass][wet-state][filter]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const int numChannels : std::array { 1, 2 })
    {
        for (const bool useHq : std::array { false, true })
        {
            CAPTURE(useHq, numChannels);

            FireAudioProcessor subject;
            FireAudioProcessor continuousReference;
            FireAudioProcessor frozenReference;
            configureProcessor(subject, useHq, numChannels);
            configureProcessor(continuousReference, useHq, numChannels);
            configureProcessor(frozenReference, useHq, numChannels);

            TimelinePlayHead subjectPlayHead;
            TimelinePlayHead continuousPlayHead;
            TimelinePlayHead frozenPlayHead;
            subject.setPlayHead(&subjectPlayHead);
            continuousReference.setPlayHead(&continuousPlayHead);
            frozenReference.setPlayHead(&frozenPlayHead);

            int streamPosition = 0;
            int transportOffset = 0;
            juce::MidiBuffer midi;

            const auto render = [&](int numSamples,
                                    bool hostBypassSubject,
                                    bool processFrozen,
                                    bool evolvedInput)
            {
                RenderedTriplet rendered {
                    juce::AudioBuffer<float>(numChannels, numSamples),
                    juce::AudioBuffer<float>(numChannels, numSamples),
                    juce::AudioBuffer<float>(numChannels, numSamples)
                };
                rendered.subject.clear();
                rendered.continuous.clear();
                rendered.frozen.clear();

                int renderedSamples = 0;
                while (renderedSamples < numSamples)
                {
                    const int samplesThisCallback = std::min(
                        preparedBlockSize, numSamples - renderedSamples);
                    const auto input = makeProbeInput(streamPosition,
                                                      samplesThisCallback,
                                                      evolvedInput,
                                                      numChannels);
                    auto subjectOutput = input;
                    auto continuousOutput = input;
                    auto frozenOutput = input;
                    const int timelineSample = streamPosition + transportOffset;
                    subjectPlayHead.setCallbackStart(timelineSample);
                    continuousPlayHead.setCallbackStart(timelineSample);
                    frozenPlayHead.setCallbackStart(timelineSample);

                    if (hostBypassSubject)
                        subject.processBlockBypassed(subjectOutput, midi);
                    else
                        subject.processBlock(subjectOutput, midi);
                    continuousReference.processBlock(continuousOutput, midi);
                    if (processFrozen)
                        frozenReference.processBlock(frozenOutput, midi);

                    for (int channel = 0; channel < numChannels; ++channel)
                    {
                        rendered.subject.copyFrom(channel,
                                                  renderedSamples,
                                                  subjectOutput,
                                                  channel,
                                                  0,
                                                  samplesThisCallback);
                        rendered.continuous.copyFrom(channel,
                                                     renderedSamples,
                                                     continuousOutput,
                                                     channel,
                                                     0,
                                                     samplesThisCallback);
                        if (processFrozen)
                            rendered.frozen.copyFrom(channel,
                                                     renderedSamples,
                                                     frozenOutput,
                                                     channel,
                                                     0,
                                                     samplesThisCallback);
                    }

                    streamPosition += samplesThisCallback;
                    renderedSamples += samplesThisCallback;
                }
                return rendered;
            };

            // Establish bit-identical wet histories before the host bypasses only
            // the subject. The frozen reference is deliberately left untouched
            // during bypass to prove that this fixture can distinguish a stale
            // graph from the continuously rendered canonical graph.
            const auto warmup = render(warmupSamples, false, true, false);
            CHECK(maximumDifferenceInRange(warmup.subject,
                                           warmup.continuous,
                                           0,
                                           warmupSamples)
                  < 1.0e-6f);
            CHECK(maximumDifferenceInRange(warmup.frozen,
                                           warmup.continuous,
                                           0,
                                           warmupSamples)
                  < 1.0e-6f);

            for (auto* processor : std::array { &subject,
                                                &continuousReference,
                                                &frozenReference })
                setPlainParameter(*processor, PEAK_GAIN_ID, -15.0f);

            const auto bypassBeforeSeek = render(bypassHalfSamples,
                                                 true,
                                                 false,
                                                 true);
            CHECK(allFinite(bypassBeforeSeek.subject));
            CHECK(allFinite(bypassBeforeSeek.continuous));

            // Seek to a deliberately non-periodic transport anchor while bypassed.
            // The sync LFO and modulated filter must still follow this new anchor.
            transportOffset += 12347;
            const auto bypassAfterSeek = render(bypassHalfSamples,
                                                true,
                                                false,
                                                true);
            CHECK(allFinite(bypassAfterSeek.subject));
            CHECK(allFinite(bypassAfterSeek.continuous));

            const auto recovery = render(recoveryProbeSamples,
                                         false,
                                         true,
                                         true);
            CHECK(allFinite(recovery.subject));
            CHECK(allFinite(recovery.continuous));
            CHECK(allFinite(recovery.frozen));

            const int comparisonSamples = recoveryProbeSamples
                                          - comparisonGuardSamples;
            const float firstRecoveryError = maximumDifferenceInRange(
                recovery.subject,
                recovery.continuous,
                0,
                comparisonGuardSamples);
            const float recoveryError = maximumDifferenceInRange(
                recovery.subject,
                recovery.continuous,
                comparisonGuardSamples,
                comparisonSamples);
            const float frozenGraphSeparation = maximumDifferenceInRange(
                recovery.frozen,
                recovery.continuous,
                comparisonGuardSamples,
                comparisonSamples);
            INFO("first post-host-bypass window error = " << firstRecoveryError);
            INFO("later post-host-bypass state error = " << recoveryError);
            INFO("frozen/continuous fixture separation = "
                 << frozenGraphSeparation);

            REQUIRE(frozenGraphSeparation > 0.05f);
            CHECK(firstRecoveryError < 2.0e-4f);
            CHECK(recoveryError < 2.0e-4f);

            // The defect is state freshness, not permanent corruption: after the
            // stale graph has consumed enough new input it must converge again.
            const auto settled = render(finalSettlingSamples,
                                        false,
                                        true,
                                        true);
            const int finalWindowStart = finalSettlingSamples - 1024;
            const float finalError = maximumDifferenceInRange(
                settled.subject, settled.continuous, finalWindowStart, 1024);
            INFO("settled post-bypass state error = " << finalError);
            CHECK(allFinite(settled.subject));
            CHECK(allFinite(settled.continuous));
            CHECK(finalError < 2.0e-4f);
        }
    }
}
