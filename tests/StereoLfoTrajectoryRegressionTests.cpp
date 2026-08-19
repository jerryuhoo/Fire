#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int lfoCycleSamples = 480;
constexpr int warmupSamples = lfoCycleSamples * 12;
constexpr int captureSamples = lfoCycleSamples * 2;
constexpr int plainRampSamples = 480;
constexpr std::array<float, 2> inputValues { 0.08f, -0.06f };

enum class StereoParameter
{
    width,
    pan
};

const char* parameterName(StereoParameter parameter)
{
    return parameter == StereoParameter::width ? "Width" : "Pan";
}

float triangleSample(int absoluteSample)
{
    const float phase = static_cast<float>(absoluteSample % lfoCycleSamples)
                        / static_cast<float>(lfoCycleSamples);
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

std::array<float, 2> applyStereoControls(float left,
                                         float right,
                                         float width,
                                         float pan)
{
    width = juce::jlimit(0.0f, 1.0f, width);
    pan = juce::jlimit(-1.0f, 1.0f, pan);

    // This is the independent closed form of WidthProcessor's mid/side
    // matrix, followed by its balance-style pan law.
    const float widthLeft = left + right * (1.0f - 2.0f * width);
    const float widthRight = right + left * (1.0f - 2.0f * width);
    const float leftGain = pan > 0.0f ? 1.0f - pan : 1.0f;
    const float rightGain = pan < 0.0f ? 1.0f + pan : 1.0f;
    return { widthLeft * leftGain, widthRight * rightGain };
}

BandProcessingParameters makeBandParameters(StereoParameter parameter,
                                              bool useHq,
                                              bool routed,
                                              bool stereoEnabled)
{
    BandProcessingParameters params;
    params.isBandEnabled = true;
    params.mode = 8;
    params.isHQ = useHq;
    params.isSafeModeOn = false;
    params.isDriveEnabled = false;
    params.isShapeEnabled = true;
    params.isDcFilterEnabled = false;
    params.isCompEnabled = false;
    params.isWidthEnabled = stereoEnabled;

    // Shape Mix zero makes the distortion stage transparent while retaining
    // the real Base/HQ BandProcessor topology around the stereo stage.
    params.shapeMixVal = 0.0f;
    params.shapeMixValProvider.baseValue = 0.0f;
    params.compMixVal = 1.0f;
    params.compMixValProvider.baseValue = 1.0f;
    params.widthMixVal = 1.0f;
    params.widthMixValProvider.baseValue = 1.0f;
    params.mixVal = 1.0f;
    params.mixValProvider.baseValue = 1.0f;
    params.outputVal.baseValue = 0.0f;
    params.outputVal.range = { -48.0f, 6.0f };

    params.width = 0.5f;
    params.widthValProvider.baseValue = 0.5f;
    params.widthValProvider.range = { 0.0f, 1.0f };
    params.pan = 0.0f;
    params.panValProvider.baseValue = 0.0f;
    params.panValProvider.range = { -1.0f, 1.0f };

    if (routed && parameter == StereoParameter::width)
    {
        params.widthValProvider.modulationDepth = 1.0f;
        params.widthValProvider.isBipolar = true;
        params.widthLfoSourceIndex = 0;
    }
    else if (routed)
    {
        params.panValProvider.modulationDepth = 1.0f;
        params.panValProvider.isBipolar = true;
        params.panLfoSourceIndex = 0;
    }

    return params;
}

struct TrajectoryRender
{
    std::array<std::vector<float>, 2> output;
    float maximumCanonicalError = 0.0f;
    bool finite = true;
};

TrajectoryRender renderBandTrajectory(StereoParameter parameter,
                                      bool useHq,
                                      int preparedBlockSize,
                                      const std::vector<int>& hostPattern)
{
    REQUIRE(preparedBlockSize > 0);
    REQUIRE_FALSE(hostPattern.empty());
    for (const int blockSize : hostPattern)
        REQUIRE(blockSize > 0);

    BandProcessor subject;
    BandProcessor preStereoReference;
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        2
    };
    subject.prepare(spec);
    preStereoReference.prepare(spec);

    const auto subjectParams = makeBandParameters(parameter,
                                                   useHq,
                                                   true,
                                                   true);
    const auto referenceParams = makeBandParameters(parameter,
                                                     useHq,
                                                     false,
                                                     false);

    TrajectoryRender result;
    for (auto& channel : result.output)
        channel.reserve(captureSamples);

    const int totalSamples = warmupSamples + captureSamples;
    int streamPosition = 0;
    size_t callbackIndex = 0;
    while (streamPosition < totalSamples)
    {
        const int requested = hostPattern[callbackIndex % hostPattern.size()];
        ++callbackIndex;
        const int samplesThisBlock = std::min(requested,
                                              totalSamples - streamPosition);
        juce::AudioBuffer<float> subjectBuffer(2, samplesThisBlock);
        juce::AudioBuffer<float> referenceBuffer(2, samplesThisBlock);
        juce::AudioBuffer<float> lfoOutputs(1, samplesThisBlock);
        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            lfoOutputs.setSample(0, sample, triangleSample(absoluteSample));
            for (int channel = 0; channel < 2; ++channel)
            {
                subjectBuffer.setSample(channel,
                                        sample,
                                        inputValues[static_cast<size_t>(channel)]);
                referenceBuffer.setSample(channel,
                                          sample,
                                          inputValues[static_cast<size_t>(channel)]);
            }
        }

        subject.process(subjectBuffer, subjectParams, lfoOutputs);
        preStereoReference.process(referenceBuffer,
                                   referenceParams,
                                   lfoOutputs);

        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const int absoluteSample = streamPosition + sample;
            const float lfo = triangleSample(absoluteSample);
            const float width = parameter == StereoParameter::width ? lfo : 0.5f;
            const float pan = parameter == StereoParameter::pan
                                  ? -1.0f + 2.0f * lfo
                                  : 0.0f;
            const auto expected = applyStereoControls(
                referenceBuffer.getSample(0, sample),
                referenceBuffer.getSample(1, sample),
                width,
                pan);

            for (int channel = 0; channel < 2; ++channel)
            {
                const float actual = subjectBuffer.getSample(channel, sample);
                const float canonical = expected[static_cast<size_t>(channel)];
                result.finite = result.finite && std::isfinite(actual)
                                && std::isfinite(canonical);
                if (absoluteSample < warmupSamples)
                    continue;

                result.maximumCanonicalError = std::max(
                    result.maximumCanonicalError,
                    std::abs(actual - canonical));
                result.output[static_cast<size_t>(channel)].push_back(actual);
            }
        }

        streamPosition += samplesThisBlock;
    }

    return result;
}

float maximumDifference(const TrajectoryRender& first,
                        const TrajectoryRender& second)
{
    float error = 0.0f;
    for (size_t channel = 0; channel < first.output.size(); ++channel)
    {
        REQUIRE(first.output[channel].size() == second.output[channel].size());
        for (size_t sample = 0; sample < first.output[channel].size(); ++sample)
            error = std::max(error,
                             std::abs(first.output[channel][sample]
                                      - second.output[channel][sample]));
    }
    return error;
}

void checkNoRouteRamp(StereoParameter parameter)
{
    WidthProcessor processor;
    processor.prepare(sampleRate);

    std::array<float, 32> primeLeft {};
    std::array<float, 32> primeRight {};
    primeLeft.fill(inputValues[0]);
    primeRight.fill(inputValues[1]);
    const float oldWidth = parameter == StereoParameter::width ? 0.0f : 0.5f;
    const float newWidth = parameter == StereoParameter::width ? 1.0f : 0.5f;
    const float oldPan = parameter == StereoParameter::pan ? -1.0f : 0.0f;
    const float newPan = parameter == StereoParameter::pan ? 1.0f : 0.0f;
    processor.process(primeLeft.data(),
                      primeRight.data(),
                      oldWidth,
                      oldPan,
                      static_cast<int>(primeLeft.size()));

    std::array<float, plainRampSamples + 1> left {};
    std::array<float, plainRampSamples + 1> right {};
    left.fill(inputValues[0]);
    right.fill(inputValues[1]);
    processor.process(left.data(),
                      right.data(),
                      newWidth,
                      newPan,
                      static_cast<int>(left.size()));

    float maximumError = 0.0f;
    for (int sample = 0; sample <= plainRampSamples; ++sample)
    {
        // JUCE's legacy scalar path advances before consuming a sample: the
        // first event sample is 1/480 into the ramp and sample 479 is the new
        // endpoint. This contract must survive separating base smoothing from
        // the routed LFO trajectory.
        const float mix = std::min(1.0f,
                                   static_cast<float>(sample + 1)
                                       / static_cast<float>(plainRampSamples));
        const float width = oldWidth + mix * (newWidth - oldWidth);
        const float pan = oldPan + mix * (newPan - oldPan);
        const auto expected = applyStereoControls(inputValues[0],
                                                  inputValues[1],
                                                  width,
                                                  pan);
        maximumError = std::max(
            maximumError,
            std::abs(left[static_cast<size_t>(sample)] - expected[0]));
        maximumError = std::max(
            maximumError,
            std::abs(right[static_cast<size_t>(sample)] - expected[1]));
    }

    CAPTURE(parameterName(parameter), maximumError);
    CHECK(maximumError < 2.0e-6f);
}

struct RouteRecipe
{
    bool routed = false;
    int sourceIndex = -1;
    float baseValue = 0.35f;
    float depth = 0.6f;
    bool bipolar = true;
};

constexpr std::array<float, 2> constantLfoValues { 0.25f, 0.75f };

BandProcessingParameters makeRecipeParameters(StereoParameter parameter,
                                               bool useHq,
                                               const RouteRecipe& recipe)
{
    auto params = makeBandParameters(parameter, useHq, false, true);
    if (parameter == StereoParameter::width)
    {
        params.width = recipe.baseValue;
        params.widthValProvider.baseValue = recipe.baseValue;
        params.widthValProvider.modulationDepth = recipe.depth;
        params.widthValProvider.isBipolar = recipe.bipolar;
        params.widthLfoSourceIndex = recipe.routed ? recipe.sourceIndex : -1;
    }
    else
    {
        params.pan = recipe.baseValue;
        params.panValProvider.baseValue = recipe.baseValue;
        params.panValProvider.modulationDepth = recipe.depth;
        params.panValProvider.isBipolar = recipe.bipolar;
        params.panLfoSourceIndex = recipe.routed ? recipe.sourceIndex : -1;
    }
    return params;
}

float recipeTarget(StereoParameter parameter, const RouteRecipe& recipe)
{
    if (! recipe.routed)
        return recipe.baseValue;

    ModulatedValueProvider provider;
    provider.baseValue = recipe.baseValue;
    provider.modulationDepth = recipe.depth;
    provider.isBipolar = recipe.bipolar;
    provider.range = parameter == StereoParameter::width
                         ? juce::NormalisableRange<float> { 0.0f, 1.0f }
                         : juce::NormalisableRange<float> { -1.0f, 1.0f };
    const float lfo = constantLfoValues[static_cast<size_t>(recipe.sourceIndex)];
    provider.lfoSignal = &lfo;
    return provider.get(0);
}

void fillRecipeBlock(juce::AudioBuffer<float>& audio,
                     juce::AudioBuffer<float>& lfoOutputs)
{
    REQUIRE(audio.getNumChannels() == 2);
    REQUIRE(lfoOutputs.getNumChannels() == 2);
    REQUIRE(audio.getNumSamples() == lfoOutputs.getNumSamples());
    for (int channel = 0; channel < 2; ++channel)
    {
        juce::FloatVectorOperations::fill(
            audio.getWritePointer(channel),
            inputValues[static_cast<size_t>(channel)],
            audio.getNumSamples());
        juce::FloatVectorOperations::fill(
            lfoOutputs.getWritePointer(channel),
            constantLfoValues[static_cast<size_t>(channel)],
            lfoOutputs.getNumSamples());
    }
}

void warmRecipePair(BandProcessor& subject,
                    BandProcessor& preStereoReference,
                    const BandProcessingParameters& subjectParams,
                    const BandProcessingParameters& referenceParams)
{
    constexpr int warmBlockSize = 256;
    for (int processed = 0; processed < 4096; processed += warmBlockSize)
    {
        juce::AudioBuffer<float> subjectBuffer(2, warmBlockSize);
        juce::AudioBuffer<float> referenceBuffer(2, warmBlockSize);
        juce::AudioBuffer<float> lfoOutputs(2, warmBlockSize);
        fillRecipeBlock(subjectBuffer, lfoOutputs);
        referenceBuffer.makeCopyOf(subjectBuffer);
        subject.process(subjectBuffer, subjectParams, lfoOutputs);
        preStereoReference.process(referenceBuffer,
                                   referenceParams,
                                   lfoOutputs);
    }
}

float sampleError(const juce::AudioBuffer<float>& actual,
                  const juce::AudioBuffer<float>& preStereoReference,
                  int sample,
                  StereoParameter parameter,
                  float parameterValue)
{
    const auto expected = applyStereoControls(
        preStereoReference.getSample(0, sample),
        preStereoReference.getSample(1, sample),
        parameter == StereoParameter::width ? parameterValue : 0.5f,
        parameter == StereoParameter::pan ? parameterValue : 0.0f);
    return std::max(std::abs(actual.getSample(0, sample) - expected[0]),
                    std::abs(actual.getSample(1, sample) - expected[1]));
}

void checkRecipeBridge(StereoParameter parameter,
                       bool useHq,
                       const char* changeName,
                       const RouteRecipe& oldRecipe,
                       const RouteRecipe& newRecipe)
{
    constexpr int eventSamples = plainRampSamples + 1;
    BandProcessor subject;
    BandProcessor preStereoReference;
    const juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
    subject.prepare(spec);
    preStereoReference.prepare(spec);
    const auto oldParams = makeRecipeParameters(parameter, useHq, oldRecipe);
    const auto newParams = makeRecipeParameters(parameter, useHq, newRecipe);
    const auto referenceParams = makeBandParameters(parameter,
                                                     useHq,
                                                     false,
                                                     false);
    warmRecipePair(subject,
                   preStereoReference,
                   oldParams,
                   referenceParams);

    juce::AudioBuffer<float> subjectBuffer(2, eventSamples);
    juce::AudioBuffer<float> referenceBuffer(2, eventSamples);
    juce::AudioBuffer<float> lfoOutputs(2, eventSamples);
    fillRecipeBlock(subjectBuffer, lfoOutputs);
    referenceBuffer.makeCopyOf(subjectBuffer);
    subject.process(subjectBuffer, newParams, lfoOutputs);
    preStereoReference.process(referenceBuffer,
                               referenceParams,
                               lfoOutputs);

    const float oldTarget = recipeTarget(parameter, oldRecipe);
    const float newTarget = recipeTarget(parameter, newRecipe);
    const float midpointTarget = oldTarget + 0.5f * (newTarget - oldTarget);
    const float firstError = sampleError(subjectBuffer,
                                         referenceBuffer,
                                         0,
                                         parameter,
                                         oldTarget);
    const float midpointError = sampleError(subjectBuffer,
                                            referenceBuffer,
                                            plainRampSamples / 2,
                                            parameter,
                                            midpointTarget);
    const float endpointError = sampleError(subjectBuffer,
                                            referenceBuffer,
                                            plainRampSamples,
                                            parameter,
                                            newTarget);
    bool finite = true;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < eventSamples; ++sample)
            finite = finite
                     && std::isfinite(subjectBuffer.getSample(channel, sample));

    CAPTURE(parameterName(parameter),
            useHq,
            changeName,
            oldTarget,
            newTarget,
            firstError,
            midpointError,
            endpointError);
    REQUIRE(std::abs(newTarget - oldTarget) > 0.1f);
    REQUIRE(finite);
    CHECK(firstError < 2.0e-4f);
    CHECK(midpointError < 2.0e-4f);
    CHECK(endpointError < 2.0e-4f);
}

void checkRapidThirdRecipe()
{
    constexpr int firstLegSamples = 160;
    constexpr int finalLegSamples = plainRampSamples + 1;
    const RouteRecipe recipeA { true, 0, 0.35f, 0.4f, true };
    const RouteRecipe recipeB { true, 1, 0.35f, 0.8f, true };
    const RouteRecipe recipeC { true, 0, 0.35f, 0.4f, false };

    BandProcessor subject;
    BandProcessor preStereoReference;
    const juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
    subject.prepare(spec);
    preStereoReference.prepare(spec);
    const auto paramsA = makeRecipeParameters(StereoParameter::width,
                                               false,
                                               recipeA);
    const auto paramsB = makeRecipeParameters(StereoParameter::width,
                                               false,
                                               recipeB);
    const auto paramsC = makeRecipeParameters(StereoParameter::width,
                                               false,
                                               recipeC);
    const auto referenceParams = makeBandParameters(StereoParameter::width,
                                                     false,
                                                     false,
                                                     false);
    warmRecipePair(subject,
                   preStereoReference,
                   paramsA,
                   referenceParams);

    juce::AudioBuffer<float> firstLeg(2, firstLegSamples);
    juce::AudioBuffer<float> firstReference(2, firstLegSamples);
    juce::AudioBuffer<float> firstLfo(2, firstLegSamples);
    fillRecipeBlock(firstLeg, firstLfo);
    firstReference.makeCopyOf(firstLeg);
    subject.process(firstLeg, paramsB, firstLfo);
    preStereoReference.process(firstReference, referenceParams, firstLfo);

    juce::AudioBuffer<float> finalLeg(2, finalLegSamples);
    juce::AudioBuffer<float> finalReference(2, finalLegSamples);
    juce::AudioBuffer<float> finalLfo(2, finalLegSamples);
    fillRecipeBlock(finalLeg, finalLfo);
    finalReference.makeCopyOf(finalLeg);
    subject.process(finalLeg, paramsC, finalLfo);
    preStereoReference.process(finalReference, referenceParams, finalLfo);

    const float targetA = recipeTarget(StereoParameter::width, recipeA);
    const float targetB = recipeTarget(StereoParameter::width, recipeB);
    const float targetC = recipeTarget(StereoParameter::width, recipeC);
    const float retargetAnchor = targetA
                                 + (159.0f / plainRampSamples)
                                       * (targetB - targetA);
    const float retargetError = sampleError(finalLeg,
                                            finalReference,
                                            0,
                                            StereoParameter::width,
                                            retargetAnchor);
    const float midpointError = sampleError(
        finalLeg,
        finalReference,
        plainRampSamples / 2,
        StereoParameter::width,
        retargetAnchor + 0.5f * (targetC - retargetAnchor));
    const float endpointError = sampleError(finalLeg,
                                            finalReference,
                                            plainRampSamples,
                                            StereoParameter::width,
                                            targetC);
    float boundaryStep = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        boundaryStep = std::max(
            boundaryStep,
            std::abs(finalLeg.getSample(channel, 0)
                     - firstLeg.getSample(channel, firstLegSamples - 1)));

    CAPTURE(targetA,
            targetB,
            targetC,
            retargetAnchor,
            retargetError,
            midpointError,
            endpointError,
            boundaryStep);
    CHECK(retargetError < 2.0e-4f);
    CHECK(midpointError < 2.0e-4f);
    CHECK(endpointError < 2.0e-4f);
    CHECK(boundaryStep < 2.0e-4f);
}

void checkPartialTransitionReset()
{
    constexpr int partialSamples = 160;
    constexpr int comparisonSamples = 64;
    const RouteRecipe oldRecipe { true, 0, 0.35f, 0.4f, true };
    const RouteRecipe targetRecipe { true, 1, 0.35f, 0.8f, false };
    const auto oldParams = makeRecipeParameters(StereoParameter::width,
                                                 false,
                                                 oldRecipe);
    const auto targetParams = makeRecipeParameters(StereoParameter::width,
                                                    false,
                                                    targetRecipe);
    const auto referenceParams = makeBandParameters(StereoParameter::width,
                                                     false,
                                                     false,
                                                     false);
    const juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
    BandProcessor subject;
    BandProcessor historyReference;
    subject.prepare(spec);
    historyReference.prepare(spec);
    warmRecipePair(subject,
                   historyReference,
                   oldParams,
                   referenceParams);

    juce::AudioBuffer<float> partial(2, partialSamples);
    juce::AudioBuffer<float> partialLfo(2, partialSamples);
    fillRecipeBlock(partial, partialLfo);
    subject.process(partial, targetParams, partialLfo);
    subject.reset();

    BandProcessor freshTarget;
    BandProcessor freshPreStereo;
    freshTarget.prepare(spec);
    freshPreStereo.prepare(spec);

    // BandProcessor::reset() preserves juce::dsp::Gain's requested target.
    // The history-bearing subject has already requested unity through its
    // 0 dB Output parameter, whereas a merely prepared processor still owns
    // Gain's constructor target. Prime that outer lifecycle once before the
    // reset so this remains a Width route-state comparison.
    juce::AudioBuffer<float> targetPrime(2, 1);
    juce::AudioBuffer<float> referencePrime(2, 1);
    juce::AudioBuffer<float> primeLfo(2, 1);
    fillRecipeBlock(targetPrime, primeLfo);
    referencePrime.makeCopyOf(targetPrime);
    freshTarget.process(targetPrime, targetParams, primeLfo);
    freshPreStereo.process(referencePrime, referenceParams, primeLfo);
    freshTarget.reset();
    freshPreStereo.reset();
    juce::AudioBuffer<float> subjectBuffer(2, comparisonSamples);
    juce::AudioBuffer<float> freshBuffer(2, comparisonSamples);
    juce::AudioBuffer<float> referenceBuffer(2, comparisonSamples);
    juce::AudioBuffer<float> lfoOutputs(2, comparisonSamples);
    fillRecipeBlock(subjectBuffer, lfoOutputs);
    freshBuffer.makeCopyOf(subjectBuffer);
    referenceBuffer.makeCopyOf(subjectBuffer);
    subject.process(subjectBuffer, targetParams, lfoOutputs);
    freshTarget.process(freshBuffer, targetParams, lfoOutputs);
    freshPreStereo.process(referenceBuffer, referenceParams, lfoOutputs);

    float freshError = 0.0f;
    float canonicalError = 0.0f;
    const float target = recipeTarget(StereoParameter::width, targetRecipe);
    for (int sample = 0; sample < comparisonSamples; ++sample)
    {
        canonicalError = std::max(
            canonicalError,
            sampleError(subjectBuffer,
                        referenceBuffer,
                        sample,
                        StereoParameter::width,
                        target));
        for (int channel = 0; channel < 2; ++channel)
            freshError = std::max(
                freshError,
                std::abs(subjectBuffer.getSample(channel, sample)
                         - freshBuffer.getSample(channel, sample)));
    }

    CAPTURE(freshError, canonicalError, target);
    CHECK(freshError < 1.0e-6f);
    CHECK(canonicalError < 2.0e-4f);
}
} // namespace

TEST_CASE("Width and Pan LFO trajectories remain sample accurate",
          "[processor][stereo][lfo][trajectory]")
{
    constexpr std::array<StereoParameter, 2> parameters {
        StereoParameter::width,
        StereoParameter::pan
    };
    const std::vector<int> fixedCallbacks { lfoCycleSamples };
    const std::vector<int> irregularCallbacks { 17, 31, 43, 29, 53, 37 };

    for (const auto parameter : parameters)
    {
        for (const bool useHq : { false, true })
        {
            const auto fixed = renderBandTrajectory(parameter,
                                                    useHq,
                                                    lfoCycleSamples,
                                                    fixedCallbacks);
            const auto irregular = renderBandTrajectory(parameter,
                                                        useHq,
                                                        257,
                                                        irregularCallbacks);
            const float partitionError = maximumDifference(fixed, irregular);
            CAPTURE(parameterName(parameter),
                    useHq,
                    fixed.maximumCanonicalError,
                    irregular.maximumCanonicalError,
                    partitionError);

            REQUIRE(fixed.finite);
            REQUIRE(irregular.finite);
            CHECK(fixed.maximumCanonicalError < 2.0e-4f);
            CHECK(irregular.maximumCanonicalError < 2.0e-4f);
            CHECK(partitionError < 2.0e-4f);
        }
    }
}

TEST_CASE("Stereo LFO trajectories preserve offsets across internal chunks",
          "[processor][stereo][lfo][trajectory][internal-chunk]")
{
    // One representative HQ host callback is deliberately much larger than
    // prepare's capacity, exercising both Width/Pan providers at non-zero
    // internal offsets without repeating the full partition matrix above.
    for (const auto parameter : { StereoParameter::width,
                                  StereoParameter::pan })
    {
        const auto whole = renderBandTrajectory(parameter,
                                                true,
                                                lfoCycleSamples,
                                                { lfoCycleSamples });
        const auto chunked = renderBandTrajectory(parameter,
                                                  true,
                                                  64,
                                                  { lfoCycleSamples });
        const float chunkError = maximumDifference(whole, chunked);
        CAPTURE(parameterName(parameter),
                chunked.maximumCanonicalError,
                chunkError);

        REQUIRE(chunked.finite);
        CHECK(chunked.maximumCanonicalError < 2.0e-4f);
        CHECK(chunkError < 2.0e-4f);
    }
}

TEST_CASE("Unrouted Width and Pan keep their legacy ten millisecond ramp",
          "[processor][stereo][automation][compatibility]")
{
    checkNoRouteRamp(StereoParameter::width);
    checkNoRouteRamp(StereoParameter::pan);
}

TEST_CASE("Stereo route recipe changes use a ten millisecond held-anchor bridge",
          "[processor][stereo][lfo][trajectory][recipe]")
{
    const RouteRecipe unrouted { false, -1, 0.35f, 0.6f, true };
    const RouteRecipe source0 { true, 0, 0.35f, 0.6f, true };
    const RouteRecipe source1 { true, 1, 0.35f, 0.6f, true };
    const RouteRecipe shallow { true, 1, 0.35f, 0.2f, true };
    const RouteRecipe deep { true, 1, 0.35f, 0.8f, true };
    const RouteRecipe unipolar { true, 0, 0.35f, 0.6f, false };

    checkRecipeBridge(StereoParameter::width,
                      false,
                      "attach",
                      unrouted,
                      source1);
    checkRecipeBridge(StereoParameter::width,
                      false,
                      "source",
                      source0,
                      source1);
    checkRecipeBridge(StereoParameter::width,
                      false,
                      "depth",
                      shallow,
                      deep);
    checkRecipeBridge(StereoParameter::width,
                      false,
                      "bipolar",
                      source0,
                      unipolar);
    checkRecipeBridge(StereoParameter::pan,
                      true,
                      "source",
                      source0,
                      source1);
}

TEST_CASE("Stereo route recipe rapid retargets are latest-wins and reset-safe",
          "[processor][stereo][lfo][trajectory][recipe][lifecycle]")
{
    checkRapidThirdRecipe();
    checkPartialTransitionReset();
}
