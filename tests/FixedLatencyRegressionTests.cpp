#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int preparedBlockSize = 257;
constexpr int renderedSamples = 4096;

enum class ProcessingPath
{
    normal,
    hostBypass
};

enum class Lifecycle
{
    fresh,
    dirtyThenReset,
    dirtyThenPrepare
};

struct RenderResult
{
    std::vector<std::vector<float>> output;
    bool finite = true;
    int reportedLatency = 0;
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
    juce::AudioProcessor::BusesLayout layout;
    const auto channelSet = numChannels == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    REQUIRE(processor.setBusesLayout(layout));
}

void configureNeutralPath(FireAudioProcessor& processor,
                          bool useHq,
                          float globalMix,
                          int numChannels,
                          int maximumBlockSize)
{
    setLayout(processor, numChannels);
    setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    setPlainParameter(processor, DOWNSAMPLE_BYPASS_ID, 0.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
    setPlainParameter(processor, MIX_ID, globalMix);
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);

    // A disabled single band is the processor's canonical raw band path. It
    // still traverses all latency-matching infrastructure, while avoiding any
    // nonlinear DSP that would obscure the physical delay under test.
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_ENABLE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(BAND_SOLO_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(COMP_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(DC_FILTER_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(OUTPUT_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(MIX_ID, 0),
                      1.0f);
    processor.prepareToPlay(sampleRate, maximumBlockSize);
}

float inputSample(int channel, int absoluteSample)
{
    const double time = static_cast<double>(absoluteSample) / sampleRate;
    const double channelPhase = channel == 0 ? 0.23 : 0.79;
    return 0.43f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 317.0 * time
                   + channelPhase))
         + 0.24f * static_cast<float>(std::cos(
               juce::MathConstants<double>::twoPi * 1741.0 * time
                   - 0.4 * channelPhase))
         + 0.11f * static_cast<float>(std::sin(
               juce::MathConstants<double>::twoPi * 6037.0 * time + 0.31));
}

float naturalHqLatency(int numChannels, int maximumBlockSize)
{
    juce::dsp::Oversampling<float> oversampling(
        static_cast<size_t>(numChannels),
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false);
    oversampling.initProcessing(static_cast<size_t>(maximumBlockSize));
    return oversampling.getLatencyInSamples();
}

float reservedInsertLatency(int numChannels, int maximumBlockSize)
{
    juce::dsp::Oversampling<float> shape(static_cast<size_t>(numChannels), 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);
    shape.initProcessing(static_cast<size_t>(maximumBlockSize));
    return 2 * static_cast<float>(fire::effects::slotCount) * shape.getLatencyInSamples();
}

void processDirtyHistory(FireAudioProcessor& processor,
                         ProcessingPath path,
                         int numChannels,
                         int maximumBlockSize)
{
    juce::MidiBuffer midi;
    for (int callback = 0; callback < 5; ++callback)
    {
        juce::AudioBuffer<float> buffer(numChannels, maximumBlockSize);
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < maximumBlockSize; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             20000
                                                 + callback * maximumBlockSize
                                                 + sample));
        if (path == ProcessingPath::normal)
            processor.processBlock(buffer, midi);
        else
            processor.processBlockBypassed(buffer, midi);
    }
}

RenderResult renderProcessor(bool useHq,
                             float globalMix,
                             int numChannels,
                             ProcessingPath path,
                             Lifecycle lifecycle,
                             const std::vector<int>& blockPattern,
                             int totalSamples = renderedSamples)
{
    REQUIRE(! blockPattern.empty());
    for (const int blockSize : blockPattern)
    {
        REQUIRE(blockSize > 0);
        REQUIRE(blockSize <= preparedBlockSize);
    }

    FireAudioProcessor processor;
    configureNeutralPath(processor,
                         useHq,
                         globalMix,
                         numChannels,
                         preparedBlockSize);
    if (lifecycle != Lifecycle::fresh)
    {
        processDirtyHistory(processor,
                            path,
                            numChannels,
                            preparedBlockSize);
        if (lifecycle == Lifecycle::dirtyThenReset)
            processor.reset();
        else
            processor.prepareToPlay(sampleRate, preparedBlockSize);
    }

    RenderResult result;
    result.reportedLatency = processor.getLatencySamples();
    result.output.resize(static_cast<size_t>(numChannels));
    for (auto& channel : result.output)
        channel.reserve(static_cast<size_t>(totalSamples));

    int streamPosition = 0;
    size_t blockIndex = 0;
    juce::MidiBuffer midi;
    while (streamPosition < totalSamples)
    {
        const int samplesThisBlock = std::min(
            blockPattern[blockIndex % blockPattern.size()],
            totalSamples - streamPosition);
        ++blockIndex;
        juce::AudioBuffer<float> buffer(numChannels, samplesThisBlock);
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));

        if (path == ProcessingPath::normal)
            processor.processBlock(buffer, midi);
        else
            processor.processBlockBypassed(buffer, midi);

        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        streamPosition += samplesThisBlock;
    }
    return result;
}

template <typename InterpolationType>
RenderResult renderDelayReference(int numChannels,
                                  float delaySamples,
                                  const std::vector<int>& blockPattern,
                                  int totalSamples = renderedSamples)
{
    juce::dsp::DelayLine<float, InterpolationType> delayLine(
        juce::jmax(64, juce::roundToInt(std::ceil(delaySamples)) + 2));
    delayLine.prepare({ sampleRate,
                        static_cast<juce::uint32>(preparedBlockSize),
                        static_cast<juce::uint32>(numChannels) });
    delayLine.setDelay(delaySamples);
    delayLine.reset();

    RenderResult result;
    result.output.resize(static_cast<size_t>(numChannels));
    int streamPosition = 0;
    size_t blockIndex = 0;
    while (streamPosition < totalSamples)
    {
        const int samplesThisBlock = std::min(
            blockPattern[blockIndex % blockPattern.size()],
            totalSamples - streamPosition);
        ++blockIndex;
        juce::AudioBuffer<float> buffer(numChannels, samplesThisBlock);
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));
        auto block = juce::dsp::AudioBlock<float>(buffer);
        delayLine.process(juce::dsp::ProcessContextReplacing<float>(block));

        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        streamPosition += samplesThisBlock;
    }
    return result;
}

float maximumDifference(const RenderResult& first,
                        const RenderResult& second)
{
    REQUIRE(first.output.size() == second.output.size());
    float maximumError = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (size_t sample = 0; sample < first.output[channel].size(); ++sample)
            maximumError = std::max(
                maximumError,
                std::abs(first.output[channel][sample]
                         - second.output[channel][sample]));
    }
    return maximumError;
}

void checkCanonicalPhysicalDelay(bool useHq,
                                 float globalMix,
                                 int numChannels)
{
    const std::vector<int> blocks { preparedBlockSize };
    const float naturalLatency = naturalHqLatency(numChannels,
                                                  preparedBlockSize) + reservedInsertLatency(numChannels, preparedBlockSize);
    const int fixedLatency = juce::roundToInt(naturalLatency);
    const auto normal = renderProcessor(useHq,
                                        globalMix,
                                        numChannels,
                                        ProcessingPath::normal,
                                        Lifecycle::fresh,
                                        blocks);
    const auto bypass = renderProcessor(useHq,
                                        globalMix,
                                        numChannels,
                                        ProcessingPath::hostBypass,
                                        Lifecycle::fresh,
                                        blocks);
    const auto reference = useHq
                               ? renderDelayReference<
                                     juce::dsp::DelayLineInterpolationTypes::Thiran>(
                                     numChannels, naturalLatency, blocks)
                               : renderDelayReference<
                                     juce::dsp::DelayLineInterpolationTypes::None>(
                                     numChannels,
                                     static_cast<float>(fixedLatency),
                                     blocks);
    REQUIRE(normal.finite);
    REQUIRE(bypass.finite);
    REQUIRE(reference.finite);
    const float normalReferenceError = maximumDifference(normal, reference);
    const float bypassReferenceError = maximumDifference(bypass, reference);
    const float normalBypassError = maximumDifference(normal, bypass);
    CAPTURE(useHq,
            globalMix,
            numChannels,
            naturalLatency,
            fixedLatency,
            normal.reportedLatency,
            bypass.reportedLatency,
            normalReferenceError,
            bypassReferenceError,
            normalBypassError);

    REQUIRE(naturalLatency > 0.0f);
    REQUIRE(fixedLatency > 0);
    CHECK(normal.reportedLatency == fixedLatency);
    CHECK(bypass.reportedLatency == fixedLatency);
    if (useHq)
    {
        // This independent Thiran golden locks the existing natural HQ path.
        CHECK(normalReferenceError < 1.0e-6f);
        CHECK(bypassReferenceError < 1.0e-6f);
    }
    else
    {
        // The integer delay itself is lossless. Allow only the sub-micro float
        // residue introduced by the surrounding endpoint mixers.
        CHECK(normalReferenceError < 1.0e-6f);
        CHECK(bypassReferenceError < 1.0e-6f);
    }
    CHECK(normalBypassError < 1.0e-6f);
}

void checkLifecycle(bool useHq,
                    int numChannels,
                    ProcessingPath path,
                    Lifecycle lifecycle)
{
    const std::vector<int> blocks { preparedBlockSize };
    const auto fresh = renderProcessor(useHq,
                                       0.5f,
                                       numChannels,
                                       path,
                                       Lifecycle::fresh,
                                       blocks);
    const auto subject = renderProcessor(useHq,
                                         0.5f,
                                         numChannels,
                                         path,
                                         lifecycle,
                                         blocks);
    REQUIRE(fresh.finite);
    REQUIRE(subject.finite);
    const float maximumError = maximumDifference(fresh, subject);
    CAPTURE(useHq,
            numChannels,
            path == ProcessingPath::hostBypass,
            lifecycle == Lifecycle::dirtyThenReset,
            fresh.reportedLatency,
            subject.reportedLatency,
            maximumError);
    REQUIRE(fresh.reportedLatency > 0);
    CHECK(subject.reportedLatency == fresh.reportedLatency);
    CHECK(maximumError < 1.0e-6f);
}

void checkPartition(bool useHq,
                    float globalMix,
                    int numChannels,
                    ProcessingPath path)
{
    const auto fixed = renderProcessor(useHq,
                                       globalMix,
                                       numChannels,
                                       path,
                                       Lifecycle::fresh,
                                       { preparedBlockSize });
    const auto irregular = renderProcessor(useHq,
                                           globalMix,
                                           numChannels,
                                           path,
                                           Lifecycle::fresh,
                                           { 7, 113, 19, 251, 37, 83, 2, 173 });
    REQUIRE(fixed.finite);
    REQUIRE(irregular.finite);
    const float maximumError = maximumDifference(fixed, irregular);
    CAPTURE(useHq,
            globalMix,
            numChannels,
            path == ProcessingPath::hostBypass,
            maximumError);
    CHECK(fixed.reportedLatency == irregular.reportedLatency);
    CHECK(maximumError < 1.0e-6f);
}

template <typename InterpolationType>
RenderResult renderImpulseDelayReference(int numChannels,
                                         float delaySamples,
                                         int numSamples)
{
    juce::dsp::DelayLine<float, InterpolationType> delayLine(
        juce::jmax(64, juce::roundToInt(std::ceil(delaySamples)) + 2));
    delayLine.prepare({ sampleRate,
                        static_cast<juce::uint32>(numSamples),
                        static_cast<juce::uint32>(numChannels) });
    delayLine.setDelay(delaySamples);
    delayLine.reset();

    juce::AudioBuffer<float> impulse(numChannels, numSamples);
    impulse.clear();
    for (int channel = 0; channel < numChannels; ++channel)
        impulse.setSample(channel, 0, 1.0f);
    auto block = juce::dsp::AudioBlock<float>(impulse);
    delayLine.process(juce::dsp::ProcessContextReplacing<float>(block));

    RenderResult result;
    result.output.resize(static_cast<size_t>(numChannels));
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& destination = result.output[static_cast<size_t>(channel)];
        destination.reserve(static_cast<size_t>(numSamples));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float value = impulse.getSample(channel, sample);
            result.finite = result.finite && std::isfinite(value);
            destination.push_back(value);
        }
    }
    return result;
}

RenderResult renderImpulseCallback(bool useHq,
                                   ProcessingPath path,
                                   int numChannels,
                                   int numSamples)
{
    // Give each callback a fresh lifecycle. Reusing one processor here would
    // mix deferred reset state with the quality-selection contract under test.
    FireAudioProcessor processor;
    configureNeutralPath(processor,
                         useHq,
                         0.5f,
                         numChannels,
                         numSamples);
    juce::AudioBuffer<float> impulse(numChannels, numSamples);
    impulse.clear();
    for (int channel = 0; channel < numChannels; ++channel)
        impulse.setSample(channel, 0, 1.0f);
    juce::MidiBuffer midi;
    if (path == ProcessingPath::normal)
        processor.processBlock(impulse, midi);
    else
        processor.processBlockBypassed(impulse, midi);

    RenderResult result;
    result.reportedLatency = processor.getLatencySamples();
    result.output.resize(static_cast<size_t>(numChannels));
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& destination = result.output[static_cast<size_t>(channel)];
        destination.reserve(static_cast<size_t>(numSamples));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float value = impulse.getSample(channel, sample);
            result.finite = result.finite && std::isfinite(value);
            destination.push_back(value);
        }
    }
    return result;
}

void checkCallbackModeSnapshots(int numChannels, ProcessingPath path)
{
    constexpr int callbackSamples = 128;
    const float naturalLatency = naturalHqLatency(numChannels,
                                                  callbackSamples) + reservedInsertLatency(numChannels, callbackSamples);
    const int fixedLatency = juce::roundToInt(naturalLatency);
    const auto baseReference = renderImpulseDelayReference<
        juce::dsp::DelayLineInterpolationTypes::None>(
        numChannels,
        static_cast<float>(fixedLatency),
        callbackSamples);
    const auto hqReference = renderImpulseDelayReference<
        juce::dsp::DelayLineInterpolationTypes::Thiran>(
        numChannels,
        naturalLatency,
        callbackSamples);

    const auto firstBase = renderImpulseCallback(false,
                                                 path,
                                                 numChannels,
                                                 callbackSamples);
    const auto hq = renderImpulseCallback(true,
                                          path,
                                          numChannels,
                                          callbackSamples);
    const auto secondBase = renderImpulseCallback(false,
                                                  path,
                                                  numChannels,
                                                  callbackSamples);

    const float baseRepeatError = maximumDifference(firstBase, secondBase);
    const float firstBaseError = maximumDifference(firstBase, baseReference);
    const float hqError = maximumDifference(hq, hqReference);
    CAPTURE(numChannels,
            path == ProcessingPath::hostBypass,
            naturalLatency,
            fixedLatency,
            firstBase.reportedLatency,
            hq.reportedLatency,
            secondBase.reportedLatency,
            baseRepeatError,
            firstBaseError,
            hqError);

    REQUIRE(firstBase.finite);
    REQUIRE(hq.finite);
    REQUIRE(secondBase.finite);
    CHECK(firstBase.reportedLatency == fixedLatency);
    CHECK(hq.reportedLatency == fixedLatency);
    CHECK(secondBase.reportedLatency == fixedLatency);
    CHECK(firstBaseError < 1.0e-6f);
    CHECK(baseRepeatError < 1.0e-6f);
    CHECK(hqError < 1.0e-6f);
}

RenderResult renderContinuousHostBypassSwitch(bool useHq,
                                              float globalMix,
                                              int numChannels,
                                              bool switchHostBypass)
{
    constexpr std::array<int, 12> callbackSizes {
        97, 211, 43, 257, 19, 173, 61, 251, 7, 149, 83, 229
    };
    FireAudioProcessor processor;
    configureNeutralPath(processor,
                         useHq,
                         globalMix,
                         numChannels,
                         preparedBlockSize);

    RenderResult result;
    result.reportedLatency = processor.getLatencySamples();
    result.output.resize(static_cast<size_t>(numChannels));
    int streamPosition = 0;
    juce::MidiBuffer midi;
    for (size_t callback = 0; callback < callbackSizes.size(); ++callback)
    {
        const int numSamples = callbackSizes[callback];
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < numSamples; ++sample)
                buffer.setSample(channel,
                                 sample,
                                 inputSample(channel,
                                             streamPosition + sample));

        const bool bypassThisCallback = switchHostBypass
                                     && callback >= 4
                                     && callback < 8;
        if (bypassThisCallback)
            processor.processBlockBypassed(buffer, midi);
        else
            processor.processBlock(buffer, midi);

        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                result.finite = result.finite && std::isfinite(value);
                result.output[static_cast<size_t>(channel)].push_back(value);
            }
        streamPosition += numSamples;
    }
    return result;
}

void checkContinuousHostBypassSwitch(bool useHq,
                                     float globalMix,
                                     int numChannels)
{
    const auto subject = renderContinuousHostBypassSwitch(useHq,
                                                          globalMix,
                                                          numChannels,
                                                          true);
    const auto alwaysNormal = renderContinuousHostBypassSwitch(useHq,
                                                               globalMix,
                                                               numChannels,
                                                               false);
    REQUIRE(subject.finite);
    REQUIRE(alwaysNormal.finite);
    const float maximumError = maximumDifference(subject, alwaysNormal);
    CAPTURE(useHq,
            globalMix,
            numChannels,
            subject.reportedLatency,
            alwaysNormal.reportedLatency,
            maximumError);
    REQUIRE(subject.reportedLatency > 0);
    CHECK(alwaysNormal.reportedLatency == subject.reportedLatency);
    if (! useHq)
    {
        // Both base paths are the same integer-D raw stream. Keeping the two
        // delay histories warm must make normal -> bypass -> normal transparent.
        CHECK(maximumError < 1.0e-6f);
    }
    else if (maximumError >= 1.0e-6f)
    {
        // The audible host-bypass path is deliberately delayed raw audio, so
        // it need not match the naturally delayed HQ wet signal while bypass
        // is active. Hidden wet-state continuity and the first normal callback
        // are locked independently by HostBypassWetStateRegressionTests.
        WARN("HQ audible wet/raw host-bypass difference: maximum error = "
             << maximumError);
    }
}
} // namespace

TEST_CASE("Fixed PDC preserves the base golden and natural HQ physical paths",
          "[processor][latency][fixed-pdc][golden]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const int numChannels : { 1, 2 })
        for (const bool useHq : { false, true })
            for (const float globalMix : { 0.0f, 0.5f, 1.0f })
                checkCanonicalPhysicalDelay(useHq,
                                            globalMix,
                                            numChannels);
}

TEST_CASE("Fixed PDC resets and re-prepares without stale delay history",
          "[processor][latency][fixed-pdc][reset][prepare]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const int numChannels : { 1, 2 })
        for (const bool useHq : { false, true })
            for (const auto path : { ProcessingPath::normal,
                                     ProcessingPath::hostBypass })
                for (const auto lifecycle : { Lifecycle::dirtyThenReset,
                                              Lifecycle::dirtyThenPrepare })
                    checkLifecycle(useHq,
                                   numChannels,
                                   path,
                                   lifecycle);
}

TEST_CASE("Fixed PDC is independent of host callback partitioning",
          "[processor][latency][fixed-pdc][block-size]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const int numChannels : { 1, 2 })
        for (const bool useHq : { false, true })
            for (const float globalMix : { 0.0f, 0.5f, 1.0f })
                for (const auto path : { ProcessingPath::normal,
                                         ProcessingPath::hostBypass })
                    checkPartition(useHq,
                                   globalMix,
                                   numChannels,
                                   path);
}

TEST_CASE("Each callback snapshots HQ while host PDC stays fixed",
          "[processor][latency][fixed-pdc][automation][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const int numChannels : { 1, 2 })
        for (const auto path : { ProcessingPath::normal,
                                 ProcessingPath::hostBypass })
            checkCallbackModeSnapshots(numChannels, path);
}

TEST_CASE("Base fixed-delay state stays continuous across host bypass",
          "[processor][latency][fixed-pdc][bypass][continuity]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const int numChannels : { 1, 2 })
        for (const float globalMix : { 0.0f, 0.5f, 1.0f })
            for (const bool useHq : { false, true })
                checkContinuousHostBypassSwitch(useHq,
                                                globalMix,
                                                numChannels);
}
