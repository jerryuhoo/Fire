/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include "PluginProcessor.h"
#include "DSP/DistortionLogic.h"
#include "PluginEditor.h"
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace
{
bool parseStrictFiniteDouble(const juce::String& textToParse, double& result) noexcept
{
    const auto trimmed = textToParse.trim();
    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(utf8, &end);
    if (end == utf8 || end == nullptr || *end != '\0'
        || errno == ERANGE || ! std::isfinite(parsed))
        return false;

    result = parsed;
    return true;
}

bool arePresetFloatsEquivalent(float lhs, float rhs) noexcept
{
    constexpr float tolerance = 1.0e-6f;
    return std::isfinite(lhs) && std::isfinite(rhs)
           && std::abs(lhs - rhs) <= tolerance;
}

bool areLfoShapesEquivalent(const LfoData& current, const LfoData& expected) noexcept
{
    if (current.points.size() != expected.points.size()
        || current.curvatures.size() != expected.curvatures.size()
        || ! arePresetFloatsEquivalent(current.smoothness, expected.smoothness))
        return false;

    for (size_t i = 0; i < current.points.size(); ++i)
        if (! arePresetFloatsEquivalent(current.points[i].x, expected.points[i].x)
            || ! arePresetFloatsEquivalent(current.points[i].y, expected.points[i].y))
            return false;

    for (size_t i = 0; i < current.curvatures.size(); ++i)
        if (! arePresetFloatsEquivalent(current.curvatures[i], expected.curvatures[i]))
            return false;

    return true;
}

bool areModulationRoutingsEquivalent(const ModulationRouting& current,
                                     const ModulationRouting& expected) noexcept
{
    return current.sourceLfoIndex == expected.sourceLfoIndex
           && current.targetParameterID == expected.targetParameterID
           && arePresetFloatsEquivalent(current.depth, expected.depth)
           && current.isBipolar == expected.isBipolar
           && current.isBypassed == expected.isBypassed;
}

struct FilterFrequencyRange
{
    float minimum;
    float maximum;
};

FilterFrequencyRange getSafeFilterFrequencyRange(double sampleRate) noexcept
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    const float nyquist = static_cast<float>(safeSampleRate * 0.5);
    const float maximum = std::nextafter(nyquist, 0.0f);
    return { juce::jmin(20.0f, maximum), maximum };
}

Slope getSlopeParameterValue(const std::atomic<float>* parameter) noexcept
{
    const int value = parameter != nullptr ? juce::roundToInt(parameter->load(std::memory_order_relaxed)) : 0;
    return static_cast<Slope>(juce::jlimit(static_cast<int>(Slope_12),
                                           static_cast<int>(Slope_48),
                                           value));
}

// A few hosts occasionally exceed the block-size hint passed to prepareToPlay.
// Reserving a practical safety capacity keeps JUCE mixers/oversamplers inside
// their prepared bounds without allocating on the audio thread.
constexpr int minimumProcessingBlockCapacity = 8192;

template <typename FloatType>
bool sameCachedValue(FloatType lhs, FloatType rhs) noexcept
{
    const auto scale = juce::jmax(static_cast<FloatType>(1),
                                  juce::jmax(std::abs(lhs), std::abs(rhs)));
    return std::abs(lhs - rhs) <= std::numeric_limits<FloatType>::epsilon() * scale;
}

bool sameFilterSettings(const ChainSettings& lhs, const ChainSettings& rhs) noexcept
{
    return sameCachedValue(lhs.peakFreq, rhs.peakFreq)
        && sameCachedValue(lhs.peakGainInDecibels, rhs.peakGainInDecibels)
        && sameCachedValue(lhs.peakQuality, rhs.peakQuality)
        && sameCachedValue(lhs.lowCutFreq, rhs.lowCutFreq)
        && sameCachedValue(lhs.highCutFreq, rhs.highCutFreq)
        && sameCachedValue(lhs.lowCutQuality, rhs.lowCutQuality)
        && sameCachedValue(lhs.highCutQuality, rhs.highCutQuality)
        && sameCachedValue(lhs.lowCutGainInDecibels, rhs.lowCutGainInDecibels)
        && sameCachedValue(lhs.highCutGainInDecibels, rhs.highCutGainInDecibels)
        && lhs.lowCutSlope == rhs.lowCutSlope
        && lhs.highCutSlope == rhs.highCutSlope
        && lhs.lowCutBypassed == rhs.lowCutBypassed
        && lhs.peakBypassed == rhs.peakBypassed
        && lhs.highCutBypassed == rhs.highCutBypassed;
}

struct BandParameterAddress
{
    const ModulatableParameterInfo* parameter = nullptr;
    int bandIndex = -1;
};

BandParameterAddress findBandParameterAddress(const juce::String& parameterID)
{
    for (const auto& parameter : ParameterIDAndName::getBandParameterInfo())
    {
        for (int bandIndex = 0; bandIndex < 4; ++bandIndex)
        {
            if (parameterID == ParameterIDAndName::getIDString(parameter.idBase, bandIndex))
                return { &parameter, bandIndex };
        }
    }

    return {};
}

void resetModulationRouting(ModulationRouting& routing)
{
    routing = {};
}

using BiquadCoefficients = std::array<float, 6>;

void assignCutStage(CutFilter& chain,
                    int stage,
                    const BiquadCoefficients& coefficients) noexcept
{
    switch (stage)
    {
        case 0: *chain.get<0>().coefficients = coefficients; break;
        case 1: *chain.get<1>().coefficients = coefficients; break;
        case 2: *chain.get<2>().coefficients = coefficients; break;
        case 3: *chain.get<3>().coefficients = coefficients; break;
        default: jassertfalse; break;
    }
}

void setCutStageBypassed(CutFilter& chain, int stageCount) noexcept
{
    chain.setBypassed<0>(stageCount < 1);
    chain.setBypassed<1>(stageCount < 2);
    chain.setBypassed<2>(stageCount < 3);
    chain.setBypassed<3>(stageCount < 4);
}

void updateButterworthCutFilter(CutFilter& chain,
                                float frequency,
                                double sampleRate,
                                Slope slope,
                                bool highPass) noexcept
{
    const int stageCount = juce::jlimit(1, 4, static_cast<int>(slope) + 1);
    const int order = stageCount * 2;
    constexpr BiquadCoefficients identityBiquad { 1.0f, 0.0f, 0.0f,
                                                   1.0f, 0.0f, 0.0f };

    for (int stage = 0; stage < 4; ++stage)
    {
        if (stage >= stageCount)
        {
            // Keep even bypassed stages second-order. Otherwise a later slope
            // increase changes JUCE IIR::Filter's order from one to two and
            // makes its internal state allocate/reset on the audio thread.
            assignCutStage(chain, stage, identityBiquad);
            continue;
        }

        // This is the same Q sequence used by JUCE's high-order Butterworth
        // designer, but it writes into the filters' existing coefficient
        // storage instead of allocating ReferenceCountedObjects per block.
        const auto angle = (2.0 * static_cast<double>(stage) + 1.0)
                         * juce::MathConstants<double>::pi
                         / (2.0 * static_cast<double>(order));
        const float q = static_cast<float>(1.0 / (2.0 * std::cos(angle)));
        const auto coefficients = highPass
                                      ? juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate,
                                                                                                frequency,
                                                                                                q)
                                      : juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sampleRate,
                                                                                               frequency,
                                                                                               q);
        assignCutStage(chain, stage, coefficients);
    }

    setCutStageBypassed(chain, stageCount);
}
} // namespace

static void applyGain(juce::AudioBuffer<float>& buffer,
                      const ModulatedValueProvider& gainProvider,
                      juce::dsp::Gain<float>& gain)
{
    if (gainProvider.lfoSignal == nullptr)
    {
        gain.setGainDecibels(gainProvider.baseValue);

        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto context = juce::dsp::ProcessContextReplacing<float>(block);
        gain.process(context);
    }
    else
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                // The provider does all the complex calculation for us!
                const float gainDb = gainProvider.get(sample);
                channelData[sample] *= juce::Decibels::decibelsToGain(gainDb);
            }
        }

        // Synchronise the block gain with the last value that was actually
        // applied. If modulation is disabled on the next block, smoothing then
        // starts from the audible value rather than from stale internal state.
        const float lastGainDb = gainProvider.get(buffer.getNumSamples() - 1);
        gain.setRampDurationSeconds(0.0);
        gain.setGainDecibels(lastGainDb);
        gain.setRampDurationSeconds(0.05);
    }
}

template <typename ValueType>
static bool pushToFifo(juce::AbstractFifo& fifo,
                       std::vector<ValueType>& storage,
                       const ValueType& value)
{
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    fifo.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0)
        storage[static_cast<size_t>(start1)] = value;
    else if (size2 > 0)
        storage[static_cast<size_t>(start2)] = value;
    else
        return false;

    fifo.finishedWrite(1);
    return true;
}

//==============================================================================
// BandProcessor Implementation
//==============================================================================

// This is where we tell JUCE what to do when prepareToPlay is called for a single band.
void BandProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    // Prepare all the DSP modules with the sample rate and block size.
    compressor.prepare(spec);
    widthProcessor.prepare(spec.sampleRate);
    gain.setRampDurationSeconds(0.05);
    gain.prepare(spec);
    juce::dsp::ProcessSpec mixerSpec = spec;
    mixerSpec.maximumBlockSize = spec.maximumBlockSize * 4 + 64;
    dryWetMixer.prepare(mixerSpec);
    shapeMixer.prepare(mixerSpec);
    compressorMixer.prepare(mixerSpec);
    widthMixer.prepare(mixerSpec);

    // The DC filter needs its coefficients to be calculated.
    dcFilter.prepare(spec);
    *dcFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);

    // The oversampling object also needs to be prepared.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels, oversampleFactor, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    oversampling->initProcessing(spec.maximumBlockSize);

    const auto numChannels = static_cast<int>(spec.numChannels);
    const auto maximumBlockSize = static_cast<int>(spec.maximumBlockSize);
    dryBuffer.setSize(numChannels, maximumBlockSize);
    upsampledLfoOutputs.setSize(4, maximumBlockSize * 4);

    // Reset all smoothed values with the current sample rate and a ramp time.
    driveSmoother.reset(spec.sampleRate, 0.05);
    biasSmoother.reset(spec.sampleRate, 0.05);
    recSmoother.reset(spec.sampleRate, 0.05);
}

// This is what happens when we need to clear the internal state of a band's processors.
void BandProcessor::reset()
{
    isFirstBlock = true;
    dryWetMixerPrimed = false;
    shapeMixerPrimed = false;
    compressorMixerPrimed = false;
    widthMixerPrimed = false;
    compressor.reset();
    widthProcessor.reset();
    gain.reset();
    dryWetMixer.reset();
    shapeMixer.reset();
    compressorMixer.reset();
    widthMixer.reset();
    dcFilter.reset();

    if (oversampling)
        oversampling->reset();

    mInputLeftRMS.store(0.0f, std::memory_order_relaxed);
    mInputRightRMS.store(0.0f, std::memory_order_relaxed);
    mOutputLeftRMS.store(0.0f, std::memory_order_relaxed);
    mOutputRightRMS.store(0.0f, std::memory_order_relaxed);
    mInputLeftPeak.store(0.0f, std::memory_order_relaxed);
    mInputRightPeak.store(0.0f, std::memory_order_relaxed);
    mOutputLeftPeak.store(0.0f, std::memory_order_relaxed);
    mOutputRightPeak.store(0.0f, std::memory_order_relaxed);
    mReductionPercent.store(1.0f, std::memory_order_relaxed);
    mSampleMaxValue.store(0.0f, std::memory_order_relaxed);
}

//==============================================================================
// This is the new, self-contained processing function for a single band.
// It replaces the old `processOneBand` and `processDistortion` functions.
//==============================================================================
void BandProcessor::process(juce::AudioBuffer<float>& buffer,
                            const BandProcessingParameters& params,
                            const juce::AudioBuffer<float>& lfoOutputs)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    // 1. Preparation
    const float mixVal = params.mixVal;
    dryBuffer.makeCopyOf(buffer, true);
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto paramsForProcessing = params; // Create a mutable copy
    const bool useHQ = params.isHQ && oversampling != nullptr;
    paramsForProcessing.isHQ = useHQ;

    dryWetMixer.setWetLatency(useHQ ? oversampling->getLatencyInSamples() : 0.0f);
    dryWetMixer.setWetMixProportion(juce::jlimit(0.0f, 1.0f, mixVal));
    if (! dryWetMixerPrimed)
        dryWetMixer.reset();
    dryWetMixerPrimed = true;
    dryWetMixer.pushDrySamples(juce::dsp::AudioBlock<float>(dryBuffer));

    // 2. Core Distortion Processing
    if (useHQ)
    {
        auto oversampledBlock = oversampling->processSamplesUp(block);

        // --- LFO Upsampling ---
        upsampledLfoOutputs.setSize(lfoOutputs.getNumChannels(),
                                    static_cast<int>(oversampledBlock.getNumSamples()),
                                    false,
                                    false,
                                    true);
        upsampledLfoOutputs.clear();
        if (lfoOutputs.getNumSamples() > 0 && upsampledLfoOutputs.getNumSamples() > 0)
        {
            for (int channel = 0; channel < lfoOutputs.getNumChannels(); ++channel)
            {
                auto* dest = upsampledLfoOutputs.getWritePointer(channel);
                const auto* src = lfoOutputs.getReadPointer(channel);

                if (lfoOutputs.getNumSamples() == 1 || upsampledLfoOutputs.getNumSamples() == 1)
                {
                    juce::FloatVectorOperations::fill(dest, src[0], upsampledLfoOutputs.getNumSamples());
                    continue;
                }

                const float step = static_cast<float>(lfoOutputs.getNumSamples() - 1)
                                   / static_cast<float>(upsampledLfoOutputs.getNumSamples() - 1);
                for (int i = 0; i < upsampledLfoOutputs.getNumSamples(); ++i)
                {
                    const float sourcePos = (float) i * step;
                    const int index0 = (int) sourcePos;
                    const int index1 = juce::jmin(index0 + 1, lfoOutputs.getNumSamples() - 1);
                    const float frac = sourcePos - (float) index0;
                    dest[i] = src[index0] * (1.0f - frac) + src[index1] * frac;
                }
            }
        }

        // Bind the upsampled LFO signals to the providers
        if (juce::isPositiveAndBelow(params.driveLfoSourceIndex, upsampledLfoOutputs.getNumChannels()))
            paramsForProcessing.driveVal.lfoSignal = upsampledLfoOutputs.getReadPointer(params.driveLfoSourceIndex);
        if (juce::isPositiveAndBelow(params.biasLfoSourceIndex, upsampledLfoOutputs.getNumChannels()))
            paramsForProcessing.biasVal.lfoSignal = upsampledLfoOutputs.getReadPointer(params.biasLfoSourceIndex);
        if (juce::isPositiveAndBelow(params.recLfoSourceIndex, upsampledLfoOutputs.getNumChannels()))
            paramsForProcessing.recVal.lfoSignal = upsampledLfoOutputs.getReadPointer(params.recLfoSourceIndex);
        // Output gain runs after downsampling, so it must use the original-rate
        // LFO. Using the oversampled signal here consumed only its first quarter.
        if (juce::isPositiveAndBelow(params.outputLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.outputVal.lfoSignal = lfoOutputs.getReadPointer(params.outputLfoSourceIndex);

        processDistortion(oversampledBlock, dryBuffer, paramsForProcessing);
        oversampling->processSamplesDown(block);

        // The DC filter is designed at the base sample rate. Running it on the
        // 4x block moved its effective cutoff to 4x the selected frequency.
        if (params.isDcFilterEnabled)
        {
            auto dcContext = juce::dsp::ProcessContextReplacing<float>(block);
            dcFilter.process(dcContext);
        }
    }
    else
    {
        // Bind the original LFO signals to the providers
        if (juce::isPositiveAndBelow(params.driveLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.driveVal.lfoSignal = lfoOutputs.getReadPointer(params.driveLfoSourceIndex);
        if (juce::isPositiveAndBelow(params.biasLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.biasVal.lfoSignal = lfoOutputs.getReadPointer(params.biasLfoSourceIndex);
        if (juce::isPositiveAndBelow(params.recLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.recVal.lfoSignal = lfoOutputs.getReadPointer(params.recLfoSourceIndex);
        if (juce::isPositiveAndBelow(params.outputLfoSourceIndex, lfoOutputs.getNumChannels()))
            paramsForProcessing.outputVal.lfoSignal = lfoOutputs.getReadPointer(params.outputLfoSourceIndex);

        processDistortion(block, dryBuffer, paramsForProcessing);
    }

    // 3. Block-wise Compressor and Width
    // These operate on the downsampled block, so their mixers are safe.
    auto postDistortionContext = juce::dsp::ProcessContextReplacing<float>(block);
    // Keep the compressor detector and dry path warm while bypassed. Gating the
    // complete branch hard-switched between compressed and dry audio at a block
    // boundary; using the mixer's existing 50 ms ramp removes that click.
    const float effectiveCompressorMix = params.isCompEnabled
                                             ? juce::jlimit(0.0f, 1.0f, params.compMixVal)
                                             : 0.0f;
    compressorMixer.setWetMixProportion(effectiveCompressorMix);
    if (! compressorMixerPrimed)
        compressorMixer.reset();
    compressorMixerPrimed = true;
    compressorMixer.pushDrySamples(postDistortionContext.getOutputBlock());
    this->compressor.setThreshold(params.compThreshold);
    this->compressor.setRatio(juce::jmax(1.0f, params.compRatio));
    this->compressor.setAttack(juce::jmax(0.01f, params.compAttack));
    this->compressor.setRelease(juce::jmax(0.01f, params.compRelease));
    this->compressor.process(postDistortionContext);
    compressorMixer.mixWetSamples(postDistortionContext.getOutputBlock());
    if (buffer.getNumChannels() == 2)
    {
        // Keep the width path warm and use the mixer's existing 50 ms ramp for
        // bypass transitions. Skipping the complete branch when Stereo was
        // disabled hard-switched between wet and dry samples at a block
        // boundary, which could produce an audible click at extreme Width/Pan
        // settings.
        const float effectiveWidthMix = params.isWidthEnabled
                                            ? juce::jlimit(0.0f, 1.0f, params.widthMixVal)
                                            : 0.0f;
        widthMixer.setWetMixProportion(effectiveWidthMix);
        if (! widthMixerPrimed)
            widthMixer.reset();
        widthMixerPrimed = true;
        widthMixer.pushDrySamples(postDistortionContext.getOutputBlock());
        this->widthProcessor.process(buffer.getWritePointer(0), buffer.getWritePointer(1), params.width, params.pan, buffer.getNumSamples());
        widthMixer.mixWetSamples(postDistortionContext.getOutputBlock());
    }

    // 4. Post-Distortion Effects
    // Per-sample Output Gain
    applyGain(buffer, paramsForProcessing.outputVal, gain);

    // 5. Final Dry/Wet Mix. Always run the mixer so its dry delay remains
    // primed and HQ mix=0 stays aligned with wet/other-band paths.
    dryWetMixer.mixWetSamples(block);
}

void BandProcessor::processBypassed(juce::AudioBuffer<float>& buffer, bool useHQ)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    auto block = juce::dsp::AudioBlock<float>(buffer);
    const float latency = useHQ && oversampling != nullptr
                              ? oversampling->getLatencyInSamples()
                              : 0.0f;
    dryWetMixer.setWetLatency(latency);
    dryWetMixer.setWetMixProportion(0.0f);
    if (! dryWetMixerPrimed)
        dryWetMixer.reset();
    dryWetMixerPrimed = true;
    dryWetMixer.pushDrySamples(block);
    dryWetMixer.mixWetSamples(block);
}

void BandProcessor::processDistortion(juce::dsp::AudioBlock<float>& blockToProcess,
                                      const juce::AudioBuffer<float>& dryBuffer, // This is original-sized dry buffer
                                      const BandProcessingParameters& params)
{
    // Create a copy of the incoming block (which might be oversampled)
    // to use as the correctly-sized "dry" signal for the shape mixer.
    // This must be done BEFORE blockToProcess is modified.
    auto dryBlockForShapeMixer = blockToProcess;

    // Shape Mix historically wraps the complete distortion stage, including
    // Drive. Once Shape is disabled its controls are frozen in the UI, so a
    // stale Mix value must not silently blend Drive back to the dry signal.
    // Keep the existing Shape-on sound, but make Shape-off bypass only the
    // Shape controls (Bias/Rectification/DC) rather than the Drive module.
    const float effectiveShapeMix = params.isShapeEnabled
                                      ? juce::jlimit(0.0f, 1.0f, params.shapeMixVal)
                                      : 1.0f;
    shapeMixer.setWetMixProportion(effectiveShapeMix);
    if (! shapeMixerPrimed)
        shapeMixer.reset();
    shapeMixerPrimed = true;
    shapeMixer.pushDrySamples(dryBlockForShapeMixer);

    const int numSamples = (int) blockToProcess.getNumSamples();
    const int numChannels = (int) blockToProcess.getNumChannels();

    // Note: mSampleMaxValue is still calculated from the original-sized dryBuffer, which is correct.
    const float sampleMaxValue = dryBuffer.getMagnitude(0, dryBuffer.getNumSamples());
    mSampleMaxValue.store(sampleMaxValue, std::memory_order_relaxed);
    auto waveshaperFunction = DistortionLogic::getWaveshaperForMode(params.mode);

    // The providers are now correctly prepared with LFO signals (if any)
    auto driveProvider = params.driveVal;
    auto biasProvider = params.biasVal;
    auto recProvider = params.recVal;

    if (! params.isShapeEnabled)
    {
        // Disable LFO modulation for Bias and Rectification
        biasProvider.baseValue = 0.0f;
        recProvider.baseValue = 0.0f;
        biasProvider.lfoSignal = nullptr;
        recProvider.lfoSignal = nullptr;
    }

    DistortionLogic::State currentState;
    currentState.mode = params.mode;

    if (isFirstBlock)
    {
        // On the first block, calculate the FINAL gain for the *first sample*
        // to properly initialize the smoothers and prevent clicks.

        float initialFinalDriveGain;

        if (! params.isDriveEnabled)
        {
            // If bypassed at startup, initialize the smoother to a gain of 1.0.
            initialFinalDriveGain = 1.0f;
        }
        else
        {
            // If not bypassed, perform the full calculation as before.
            float initialDrive = driveProvider.get(0); // Get LFO-modulated value for sample 0
            if (params.isExtremeModeOn)
                initialDrive = log2f(10.0f) * initialDrive;
            const float initialDriveForCalc = initialDrive * 6.5f / 100.0f;
            float initialPowerDrive = std::pow(2.0f, initialDriveForCalc);

            if (params.isSafeModeOn && sampleMaxValue > 0.0001f && sampleMaxValue * initialPowerDrive > 2.0f)
                initialFinalDriveGain = 2.0f / sampleMaxValue + 0.1f * initialDriveForCalc;
            else
                initialFinalDriveGain = initialPowerDrive;
        }

        driveSmoother.setCurrentAndTargetValue(initialFinalDriveGain);

        // Initialize Bias and Rec smoothers with their final modulated value for sample 0
        biasSmoother.setCurrentAndTargetValue(biasProvider.get(0));
        recSmoother.setCurrentAndTargetValue(recProvider.get(0));

        isFirstBlock = false;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // 1. Get the final, LFO-modulated value for each parameter for the CURRENT sample.
        float currentDrive = driveProvider.get(sample);
        const float currentBias = biasProvider.get(sample);
        const float currentRec = recProvider.get(sample);

        // 2. Calculate the final drive gain, including Extreme and Safe modes. This is the potentially "blocky" signal.
        if (params.isExtremeModeOn)
            currentDrive = log2f(10.0f) * currentDrive;

        const float driveForCalc = currentDrive * 6.5f / 100.0f;
        float powerDrive = std::pow(2.0f, driveForCalc);

        float finalDriveGain;
        if (! params.isDriveEnabled)
        {
            // If drive is bypassed, the gain should be 1.0 (no change).
            finalDriveGain = 1.0f;
        }
        else
        {
            // Otherwise, use the existing Safe Mode logic.
            if (params.isSafeModeOn && sampleMaxValue > 0.0001f && sampleMaxValue * powerDrive > 2.0f)
                finalDriveGain = 2.0f / sampleMaxValue + 0.1f * driveForCalc;
            else
                finalDriveGain = powerDrive;
        }

        // 3. Set the smoothers' targets to these final, per-sample values.
        // This makes the smoothers act like a one-pole filter, restoring the old behavior
        // where the output of the complex logic was smoothed.
        driveSmoother.setTargetValue(finalDriveGain);
        biasSmoother.setTargetValue(currentBias);
        recSmoother.setTargetValue(currentRec);

        // In HQ mode the block contains 4x as many samples, while these
        // smoothers were prepared at the base sample rate. Advance them once
        // per base-rate sample so their time constants do not become 4x faster.
        const int smoothingStride = params.isHQ ? (1 << oversampleFactor) : 1;
        if ((sample % smoothingStride) == 0)
        {
            currentState.drive = driveSmoother.getNextValue();
            currentState.bias = biasSmoother.getNextValue();
            currentState.rec = recSmoother.getNextValue();
        }
        else
        {
            currentState.drive = driveSmoother.getCurrentValue();
            currentState.bias = biasSmoother.getCurrentValue();
            currentState.rec = recSmoother.getCurrentValue();
        }

        // Update reduction meter (can be done once per block)
        if (sample == 0)
        {
            if (driveForCalc == 0.0f || sampleMaxValue <= 0.001f)
                mReductionPercent.store(1.0f, std::memory_order_relaxed);
            else
                // Use the smoothed value for a more stable meter reading
                mReductionPercent.store(std::log2(currentState.drive) / driveForCalc, std::memory_order_relaxed);
        }

        // 5. Apply audio processing using the correctly smoothed values
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float currentSample = blockToProcess.getSample(channel, sample);

            currentSample *= currentState.drive;
            currentSample += currentState.bias;
            currentSample = waveshaperFunction(currentSample);
            if (currentSample < 0.0f)
                currentSample *= (0.5f - currentState.rec) * 2.0f;
            currentSample -= currentState.bias;

            blockToProcess.setSample(channel, sample, currentSample);
        }
    }

    shapeMixer.mixWetSamples(blockToProcess);

    if (params.isDcFilterEnabled && ! params.isHQ)
    {
        auto dcContext = juce::dsp::ProcessContextReplacing<float>(blockToProcess);
        dcFilter.process(dcContext);
    }
}

//==============================================================================
FireAudioProcessor::CachedParameter FireAudioProcessor::cacheParameter(const juce::String& parameterID)
{
    return { treeState.getRawParameterValue(parameterID),
             treeState.getParameter(parameterID) };
}

float FireAudioProcessor::loadCachedParameter(const CachedParameter& parameter, float fallback) noexcept
{
    if (parameter.raw == nullptr)
        return fallback;

    const float value = parameter.raw->load(std::memory_order_relaxed);
    return std::isfinite(value) ? value : fallback;
}

float FireAudioProcessor::getBlockModulatedValue(const CachedParameter& parameter,
                                                 const juce::AudioBuffer<float>& lfoOutputs) const noexcept
{
    const float defaultValue = parameter.ranged != nullptr
                                   ? parameter.ranged->convertFrom0to1(parameter.ranged->getDefaultValue())
                                   : 0.0f;
    const float baseValue = loadCachedParameter(parameter, defaultValue);
    if (parameter.ranged == nullptr)
        return baseValue;

    LfoManager::AudioThreadRoutingInfo routingInfo;
    if (! lfoManager->getAudioThreadRoutingInfo(parameter.ranged, routingInfo)
        || ! juce::isPositiveAndBelow(routingInfo.sourceLfoIndex, lfoOutputs.getNumChannels())
        || lfoOutputs.getNumSamples() <= 0)
        return baseValue;

    float lfoValue = lfoOutputs.getSample(routingInfo.sourceLfoIndex, 0);
    if (! std::isfinite(lfoValue))
        return baseValue;

    const float rawNormalisedBase = parameter.ranged->getValue();
    const float normalisedBase = std::isfinite(rawNormalisedBase)
                                     ? juce::jlimit(0.0f, 1.0f, rawNormalisedBase)
                                     : parameter.ranged->getDefaultValue();

    float effectiveDepth = routingInfo.depth;
    if (routingInfo.isBipolar)
    {
        lfoValue = lfoValue * 2.0f - 1.0f;
        effectiveDepth *= 0.5f;
    }

    const float normalisedValue = juce::jlimit(0.0f, 1.0f,
                                               normalisedBase + lfoValue * effectiveDepth);
    const float value = parameter.ranged->convertFrom0to1(normalisedValue);
    return std::isfinite(value) ? value : baseValue;
}

ChainSettings FireAudioProcessor::getCachedChainSettings(const juce::AudioBuffer<float>* lfoOutputs) const noexcept
{
    const auto getValue = [this, lfoOutputs](const CachedParameter& parameter)
    {
        return lfoOutputs != nullptr
                   ? getBlockModulatedValue(parameter, *lfoOutputs)
                   : loadCachedParameter(parameter);
    };

    ChainSettings settings;
    settings.lowCutFreq = getValue(filterParameterCache.lowCutFrequency);
    settings.lowCutGainInDecibels = getValue(filterParameterCache.lowCutGain);
    settings.lowCutQuality = getValue(filterParameterCache.lowCutQuality);
    settings.lowCutSlope = getSlopeParameterValue(filterParameterCache.lowCutSlope.raw);
    settings.lowCutBypassed = loadCachedParameter(filterParameterCache.lowCutBypassed) > 0.5f;

    settings.peakFreq = getValue(filterParameterCache.peakFrequency);
    settings.peakGainInDecibels = getValue(filterParameterCache.peakGain);
    settings.peakQuality = getValue(filterParameterCache.peakQuality);
    settings.peakBypassed = loadCachedParameter(filterParameterCache.peakBypassed) > 0.5f;

    settings.highCutFreq = getValue(filterParameterCache.highCutFrequency);
    settings.highCutGainInDecibels = getValue(filterParameterCache.highCutGain);
    settings.highCutQuality = getValue(filterParameterCache.highCutQuality);
    settings.highCutSlope = getSlopeParameterValue(filterParameterCache.highCutSlope.raw);
    settings.highCutBypassed = loadCachedParameter(filterParameterCache.highCutBypassed) > 0.5f;
    return settings;
}

void FireAudioProcessor::initialiseParameterCache()
{
    const auto indexed = [this](const juce::String& base, int index)
    {
        return cacheParameter(ParameterIDAndName::getIDString(base, index));
    };

    for (int i = 0; i < static_cast<int>(bandParameterCache.size()); ++i)
    {
        auto& parameters = bandParameterCache[static_cast<size_t>(i)];
        parameters.enabled = indexed(BAND_ENABLE_ID, i);
        parameters.solo = indexed(BAND_SOLO_ID, i);
        parameters.mode = indexed(MODE_ID, i);
        parameters.linked = indexed(LINKED_ID, i);
        parameters.safe = indexed(SAFE_ID, i);
        parameters.extreme = indexed(EXTREME_ID, i);
        parameters.driveEnabled = indexed(DRIVE_BYPASS_ID, i);
        parameters.shapeEnabled = indexed(SHAPE_BYPASS_ID, i);
        parameters.compressorEnabled = indexed(COMP_BYPASS_ID, i);
        parameters.widthEnabled = indexed(WIDTH_BYPASS_ID, i);
        parameters.dcFilterEnabled = indexed(DC_FILTER_ID, i);
        parameters.drive = indexed(DRIVE_ID, i);
        parameters.bias = indexed(BIAS_ID, i);
        parameters.rec = indexed(REC_ID, i);
        parameters.output = indexed(OUTPUT_ID, i);
        parameters.compressorRatio = indexed(COMP_RATIO_ID, i);
        parameters.compressorThreshold = indexed(COMP_THRESH_ID, i);
        parameters.compressorAttack = indexed(COMP_ATTACK_ID, i);
        parameters.compressorRelease = indexed(COMP_RELEASE_ID, i);
        parameters.compressorMix = indexed(COMP_MIX_ID, i);
        parameters.width = indexed(WIDTH_ID, i);
        parameters.pan = indexed(PAN_ID, i);
        parameters.widthMix = indexed(WIDTH_MIX_ID, i);
        parameters.mix = indexed(MIX_ID, i);
        parameters.shapeMix = indexed(SHAPE_MIX_ID, i);

        lfoSmoothParameters[static_cast<size_t>(i)] = indexed(LFO_SMOOTH_ID, i).raw;
    }

    for (int i = 0; i < static_cast<int>(crossoverFrequencyParameters.size()); ++i)
        crossoverFrequencyParameters[static_cast<size_t>(i)] = indexed(FREQ_ID, i);

    numBandsParameter = cacheParameter(NUM_BANDS_ID);
    hqParameter = cacheParameter(HQ_ID);
    globalOutputParameter = cacheParameter(OUTPUT_ID);
    globalMixParameter = cacheParameter(MIX_ID);
    filterEnabledParameter = cacheParameter(FILTER_BYPASS_ID);
    downsampleEnabledParameter = cacheParameter(DOWNSAMPLE_BYPASS_ID);
    downsampleRateParameter = cacheParameter(DOWNSAMPLE_ID);
    bitDepthParameter = cacheParameter(BIT_DEPTH_ID);
    jitterParameter = cacheParameter(JITTER_ID);
    downsampleMixParameter = cacheParameter(DOWNSAMPLE_MIX_ID);

    filterParameterCache.lowCutFrequency = cacheParameter(LOWCUT_FREQ_ID);
    filterParameterCache.lowCutGain = cacheParameter(LOWCUT_GAIN_ID);
    filterParameterCache.lowCutQuality = cacheParameter(LOWCUT_Q_ID);
    filterParameterCache.lowCutSlope = cacheParameter(LOWCUT_SLOPE_ID);
    filterParameterCache.lowCutBypassed = cacheParameter(LOWCUT_BYPASSED_ID);
    filterParameterCache.peakFrequency = cacheParameter(PEAK_FREQ_ID);
    filterParameterCache.peakGain = cacheParameter(PEAK_GAIN_ID);
    filterParameterCache.peakQuality = cacheParameter(PEAK_Q_ID);
    filterParameterCache.peakBypassed = cacheParameter(PEAK_BYPASSED_ID);
    filterParameterCache.highCutFrequency = cacheParameter(HIGHCUT_FREQ_ID);
    filterParameterCache.highCutGain = cacheParameter(HIGHCUT_GAIN_ID);
    filterParameterCache.highCutQuality = cacheParameter(HIGHCUT_Q_ID);
    filterParameterCache.highCutSlope = cacheParameter(HIGHCUT_SLOPE_ID);
    filterParameterCache.highCutBypassed = cacheParameter(HIGHCUT_BYPASSED_ID);
}

//==============================================================================
FireAudioProcessor::FireAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      treeState(*this, nullptr, "PARAMETERS", createParameters()),
      lfoManager(std::make_unique<LfoManager>(treeState)),
      stateAB {
          *this
      }
#if JUCE_MAC
      ,
      statePresets {
          *this,
          "Audio/Presets/Wings/Fire"
      }
#else
      ,
      statePresets {
          *this,
          "Wings/Fire"
      } // AppData/Roaming/...
#endif
#endif
{
    initialiseParameterCache();

    // Initialize the band processors in a loop.
    for (int i = 0; i < 4; ++i)
        bands.push_back(std::make_unique<BandProcessor>());

    for (int i = 0; i < 4; ++i)
        realtimeModulatedThresholds[i].store(-48.0f);

    filterFifoBuffer.resize(filterFifo.getTotalSize());
    meterFifoBuffer.resize(meterFifo.getTotalSize());
    graphFifoBuffer.resize(graphFifo.getTotalSize());

    // Set up the properties file options.
    juce::PropertiesFile::Options options;
    options.applicationName = JucePlugin_Name;
    options.filenameSuffix = ".settings";
    options.folderName = "Wings";
    options.osxLibrarySubFolder = "Application Support";
    options.commonToAllUsers = false;

    // Create the properties file object.
    appProperties = std::make_unique<juce::PropertiesFile>(options);

    // Initialize all 9 smoothers with default parameter values
    auto chainSettings = getCachedChainSettings(nullptr);

    const auto frequencyRange = getSafeFilterFrequencyRange(getSampleRate());

    chainSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.highCutFreq);

    lowcutFreqSmoother.setCurrentAndTargetValue(chainSettings.lowCutFreq);
    lowcutGainSmoother.setCurrentAndTargetValue(chainSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.setCurrentAndTargetValue(chainSettings.lowCutQuality);

    peakFreqSmoother.setCurrentAndTargetValue(chainSettings.peakFreq);
    peakGainSmoother.setCurrentAndTargetValue(chainSettings.peakGainInDecibels);
    peakQualitySmoother.setCurrentAndTargetValue(chainSettings.peakQuality);

    highcutFreqSmoother.setCurrentAndTargetValue(chainSettings.highCutFreq);
    highcutGainSmoother.setCurrentAndTargetValue(chainSettings.highCutGainInDecibels);
    highcutQualitySmoother.setCurrentAndTargetValue(chainSettings.highCutQuality);
    startTimerHz(30);
}

FireAudioProcessor::~FireAudioProcessor()
{
    stopTimer();
}

//==============================================================================
const juce::String FireAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FireAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool FireAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool FireAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double FireAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FireAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int FireAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FireAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String FireAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void FireAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void FireAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const double safeSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0
                                      ? sampleRate
                                      : 48000.0;
    const int maximumBlockSize = juce::jmax(minimumProcessingBlockCapacity,
                                             juce::jmax(1, samplesPerBlock));
    const int outputChannels = juce::jmax(1, getMainBusNumOutputChannels());
    juce::dsp::ProcessSpec spec { safeSampleRate,
                                  static_cast<juce::uint32>(maximumBlockSize),
                                  static_cast<juce::uint32>(outputChannels) };

    globalFilterCacheValid = false;

    for (int i = 0; i < 4; ++i)
    {
        if (auto* band = bands[i].get())
        {
            const auto& parameters = bandParameterCache[static_cast<size_t>(i)];
            band->prepare(spec);
            band->recSmoother.setCurrentAndTargetValue(loadCachedParameter(parameters.rec));
            band->biasSmoother.setCurrentAndTargetValue(loadCachedParameter(parameters.bias));

            const float initialOutput = loadCachedParameter(parameters.linked) > 0.5f
                                            ? -0.1f * loadCachedParameter(parameters.drive)
                                            : loadCachedParameter(parameters.output);
            band->gain.setRampDurationSeconds(0.0);
            band->gain.setGainDecibels(initialOutput);
            band->gain.setRampDurationSeconds(0.05);
        }
    }

    float hqLatency = 0.0f;
    if (! bands.empty() && bands.front() != nullptr && bands.front()->oversampling != nullptr)
        hqLatency = bands.front()->oversampling->getLatencyInSamples();
    preparedHqLatency.store(hqLatency, std::memory_order_release);

    lfoManager->prepare(spec);

    for (int i = 0; i < historyLength; ++i)
    {
        historyArrayL[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
        historyArrayR[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
    }
    historyWritePosition.store(0, std::memory_order_relaxed);
    historySamplesAvailable.store(historyLength, std::memory_order_release);
    historyGeneration.fetch_add(1, std::memory_order_release);

    delayMatchedDryBuffer.setSize(outputChannels, maximumBlockSize);
    delayMatchedDryBuffer.clear();

    mWetBuffer.setSize(outputChannels, maximumBlockSize);
    mWetBuffer.clear();
    lfoOutputBuffer.setSize(4, maximumBlockSize);
    lfoOutputBuffer.clear();
    lofiDryBuffer.setSize(outputChannels, maximumBlockSize);
    lofiDryBuffer.clear();

    const float rampTimeSeconds = 0.0005f;

    lowcutFreqSmoother.reset(safeSampleRate, rampTimeSeconds);
    lowcutFreqSmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.lowCutFrequency));
    lowcutGainSmoother.reset(safeSampleRate, rampTimeSeconds);
    lowcutGainSmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.lowCutGain));
    lowcutQualitySmoother.reset(safeSampleRate, rampTimeSeconds);
    lowcutQualitySmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.lowCutQuality));

    peakFreqSmoother.reset(safeSampleRate, rampTimeSeconds);
    peakFreqSmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.peakFrequency));
    peakGainSmoother.reset(safeSampleRate, rampTimeSeconds);
    peakGainSmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.peakGain));
    peakQualitySmoother.reset(safeSampleRate, rampTimeSeconds);
    peakQualitySmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.peakQuality));

    highcutFreqSmoother.reset(safeSampleRate, rampTimeSeconds);
    highcutFreqSmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.highCutFrequency));
    highcutGainSmoother.reset(safeSampleRate, rampTimeSeconds);
    highcutGainSmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.highCutGain));
    highcutQualitySmoother.reset(safeSampleRate, rampTimeSeconds);
    highcutQualitySmoother.setCurrentAndTargetValue(loadCachedParameter(filterParameterCache.highCutQuality));

    smoothedFreq1.reset(safeSampleRate, rampTimeSeconds * 2);
    smoothedFreq2.reset(safeSampleRate, rampTimeSeconds * 2);
    smoothedFreq3.reset(safeSampleRate, rampTimeSeconds * 2);
    synchroniseMultibandTopologyResetState();

    // filter init
    updateFilter(safeSampleRate);
    leftChain.prepare(spec);
    rightChain.prepare(spec);

    // multiband filters
    mBuffer1.setSize(outputChannels, maximumBlockSize);
    mBuffer2.setSize(outputChannels, maximumBlockSize);
    mBuffer3.setSize(outputChannels, maximumBlockSize);
    mBuffer4.setSize(outputChannels, maximumBlockSize);
    mSplitTemp1.setSize(outputChannels, maximumBlockSize);
    mSplitTemp2.setSize(outputChannels, maximumBlockSize);
    mSplitTemp3.setSize(outputChannels, maximumBlockSize);
    mBuffer1.clear();
    mBuffer2.clear();
    mBuffer3.clear();
    mBuffer4.clear();

    lowpass1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowpass2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowpass3.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    highpass1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    highpass2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    highpass3.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    lowpass1.prepare(spec);
    lowpass2.prepare(spec);
    lowpass3.prepare(spec);
    highpass1.prepare(spec);
    highpass2.prepare(spec);
    highpass3.prepare(spec);

    compensatorLP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    compensatorHP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    compensatorLP.prepare(spec);
    compensatorHP.prepare(spec);
    secondCompensatorLP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    secondCompensatorHP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    secondCompensatorLP.prepare(spec);
    secondCompensatorHP.prepare(spec);

    // gain
    gainProcessorGlobal.setRampDurationSeconds(0.05);
    gainProcessorGlobal.prepare(spec);
    gainProcessorGlobal.setRampDurationSeconds(0.0);
    gainProcessorGlobal.setGainDecibels(loadCachedParameter(globalOutputParameter));
    gainProcessorGlobal.setRampDurationSeconds(0.05);

    // dry wet
    juce::dsp::ProcessSpec globalMixerSpec = spec;
    globalMixerSpec.maximumBlockSize = spec.maximumBlockSize * 20; // set 20 to pass PluginVal
    dryWetMixerGlobal.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    dryWetMixerGlobal.setWetMixProportion(juce::jlimit(0.0f, 1.0f,
                                                       loadCachedParameter(globalMixParameter)));
    dryWetMixerGlobal.prepare(globalMixerSpec);

    bypassDelayMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    // Set the target before prepare/reset so the first bypassed sample is not
    // blended with the undelayed wet input by DryWetMixer's 50 ms ramp.
    bypassDelayMixer.setWetMixProportion(0.0f);
    bypassDelayMixer.prepare(globalMixerSpec);

    lofiMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    lofiMixer.setWetMixProportion(juce::jlimit(0.0f, 1.0f,
                                               loadCachedParameter(downsampleMixParameter)));
    lofiMixer.prepare(globalMixerSpec);
    updateReportedLatency();
    publishLatencyToHost();
    reset();
}

void FireAudioProcessor::reset()
{
    needsReset = true;
}

void FireAudioProcessor::requestMultibandTopologyReset() noexcept
{
    multibandTopologyResetGeneration.fetch_add(1, std::memory_order_release);
}

void FireAudioProcessor::resetMultibandProcessingState() noexcept
{
    lowpass1.reset();
    lowpass2.reset();
    lowpass3.reset();
    highpass1.reset();
    highpass2.reset();
    highpass3.reset();
    compensatorLP.reset();
    compensatorHP.reset();
    secondCompensatorLP.reset();
    secondCompensatorHP.reset();

    for (size_t bandIndex = 0; bandIndex < bands.size(); ++bandIndex)
    {
        auto& band = bands[bandIndex];
        if (band == nullptr)
            continue;

        band->reset();

        // juce::dsp::Gain::reset() snaps its smoother to the physical slot's
        // previous target.  After an add/remove that slot can represent a
        // different logical band, so initialise it from the completed
        // parameter migration instead of ramping from stale output gain for
        // the next 50 ms.
        const auto& parameters = bandParameterCache[bandIndex];
        const float initialOutput = loadCachedParameter(parameters.linked) > 0.5f
                                        ? -0.1f * loadCachedParameter(parameters.drive)
                                        : loadCachedParameter(parameters.output);
        band->gain.setRampDurationSeconds(0.0);
        band->gain.setGainDecibels(initialOutput);
        band->gain.setRampDurationSeconds(0.05);
    }
}

std::array<float, 3> FireAudioProcessor::getEffectiveCrossoverFrequencies() const noexcept
{
    constexpr std::array<float, 3> defaultCrossoverFrequencies { 200.0f, 1000.0f, 5000.0f };
    constexpr float minimumParameterFrequency = 40.0f;
    constexpr float maximumParameterFrequency = 10024.0f;

    std::array<float, 3> frequencies {
        loadCachedParameter(crossoverFrequencyParameters[0]),
        loadCachedParameter(crossoverFrequencyParameters[1]),
        loadCachedParameter(crossoverFrequencyParameters[2])
    };

    bool activeFrequenciesAreValid = true;
    for (int index = 0; index < activeCrossovers; ++index)
    {
        const auto frequency = frequencies[static_cast<size_t>(index)];
        activeFrequenciesAreValid = activeFrequenciesAreValid
                                     && std::isfinite(frequency)
                                     && frequency >= minimumParameterFrequency
                                     && frequency <= maximumParameterFrequency
                                     && (index == 0
                                         || frequency > frequencies[static_cast<size_t>(index - 1)]);
    }

    // Old sessions use 21 Hz as the inactive-slot sentinel. If a host exposes
    // NUM_BANDS on its generic parameter surface and enables those slots by
    // itself, use safe local crossover values without rewriting the session's
    // APVTS state.
    if (! activeFrequenciesAreValid)
    {
        for (int index = 0; index < activeCrossovers; ++index)
            frequencies[static_cast<size_t>(index)] = defaultCrossoverFrequencies[static_cast<size_t>(index)];
    }

    return frequencies;
}

void FireAudioProcessor::snapCrossoverSmoothersToParameters() noexcept
{
    const auto frequencies = getEffectiveCrossoverFrequencies();
    smoothedFreq1.setCurrentAndTargetValue(frequencies[0]);
    smoothedFreq2.setCurrentAndTargetValue(frequencies[1]);
    smoothedFreq3.setCurrentAndTargetValue(frequencies[2]);
}

void FireAudioProcessor::synchroniseMultibandTopologyResetState() noexcept
{
    // The release/acquire pair makes the preceding message-thread parameter
    // migration visible before its new slot layout is consumed here.
    const auto requestedGeneration = multibandTopologyResetGeneration.load(std::memory_order_acquire);

    numBands = juce::jlimit(1, 4,
                            juce::roundToInt(loadCachedParameter(numBandsParameter, 1.0f)));
    activeCrossovers = numBands - 1;
    snapCrossoverSmoothersToParameters();
    appliedMultibandTopologyResetGeneration = requestedGeneration;
}

void FireAudioProcessor::performReset()
{
    resetMultibandProcessingState();
    synchroniseMultibandTopologyResetState();
    dryWetMixerGlobal.reset();
    globalMixerPrimed = false;
    bypassDelayMixer.reset();
    lofiMixer.reset();
    lofiMixerPrimed = false;
    resetDownsamplingState();
    gainProcessorGlobal.reset();
    lfoManager->reset();
}

void FireAudioProcessor::releaseResources()
{
    needsReset.store(false, std::memory_order_release);
    performReset();
}

void FireAudioProcessor::updateReportedLatency()
{
    const bool useHQ = loadCachedParameter(hqParameter) > 0.5f;

    const float newLatency = useHQ
                                 ? preparedHqLatency.load(std::memory_order_acquire)
                                 : 0.0f;

    totalLatency.store(newLatency, std::memory_order_relaxed);

}

void FireAudioProcessor::publishLatencyToHost()
{
    const int latencyInSamples = juce::roundToInt(totalLatency.load(std::memory_order_acquire));
    if (latencyInSamples != getLatencySamples())
        setLatencySamples(latencyInSamples);
}

void FireAudioProcessor::timerCallback()
{
    // setLatencySamples synchronously notifies the host, so keep it on the
    // message thread. Re-read HQ here as well so stopped transports update PDC
    // without waiting for the next audio callback.
    updateReportedLatency();
    publishLatencyToHost();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FireAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void FireAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    isBypassed.store(true, std::memory_order_relaxed);

    if (needsReset.exchange(false, std::memory_order_acq_rel))
        performReset();

    updateReportedLatency();

    calculateAndStoreLevels(buffer, mInputLeftRMSGlobal, mInputRightRMSGlobal, mInputLeftPeakGlobal, mInputRightPeakGlobal);
    resetDownsamplingState();
    processLatencyMatchedBypass(buffer);
    calculateAndStoreLevels(buffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);
}

void FireAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    isBypassed.store(false, std::memory_order_relaxed);

    if (needsReset.exchange(false, std::memory_order_acq_rel))
        performReset();

    updateReportedLatency();

    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int numBufferChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    auto sampleRate = getSampleRate();

    if (! std::isfinite(sampleRate) || sampleRate <= 0)
    {
        sampleRate = 48000;
    }

    for (int channel = juce::jlimit(0, numBufferChannels, totalNumInputChannels);
         channel < numBufferChannels;
         ++channel)
        buffer.clear(channel, 0, numSamples);

    calculateAndStoreLevels(buffer, mInputLeftRMSGlobal, mInputRightRMSGlobal, mInputLeftPeakGlobal, mInputRightPeakGlobal);

    if (numBufferChannels == 0 || numSamples == 0)
    {
        calculateAndStoreLevels(buffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);
        return;
    }

    lfoOutputBuffer.setSize(4, numSamples, false, false, true);
    lfoOutputBuffer.clear();
    lfoManager->processBlock(lfoOutputBuffer, static_cast<float>(sampleRate), getPlayHead(), numSamples);

    updateParameters();

    mBuffer1.setSize(numBufferChannels, numSamples, false, false, true);
    mBuffer2.setSize(numBufferChannels, numSamples, false, false, true);
    mBuffer3.setSize(numBufferChannels, numSamples, false, false, true);
    mBuffer4.setSize(numBufferChannels, numSamples, false, false, true);
    delayMatchedDryBuffer.setSize(numBufferChannels, numSamples, false, false, true);
    mWetBuffer.setSize(numBufferChannels, numSamples, false, false, true);
    lofiDryBuffer.setSize(numBufferChannels, numSamples, false, false, true);

    // Keep a raw-input delay line warm while processing normally. This makes a
    // host bypass transition retain the same latency as the HQ signal path.
    primeLatencyMatchedBypass(buffer);

    processMultiBand(buffer, lfoOutputBuffer, sampleRate);

    applyDownsamplingEffect(buffer);
    // Call the simplified global effects function.
    applyGlobalEffects(buffer, lfoOutputBuffer, sampleRate);

    // Call the simplified global mix function.
    applyGlobalMix(buffer);

    mWetBuffer.makeCopyOf(buffer, true);
    captureHistorySamples();
    pushDataPairToFFT(mWetBuffer, delayMatchedDryBuffer);
    calculateAndStoreLevels(mWetBuffer, mOutputLeftRMSGlobal, mOutputRightRMSGlobal, mOutputLeftPeakGlobal, mOutputRightPeakGlobal);

    // --- 1. Push Modulated Filter Data to its FIFO ---
    if (filterFifo.getFreeSpace() >= 1)
    {
        // Get the final, modulated values for this block
        auto chainSettings = getCachedChainSettings(&lfoOutputBuffer);

        ModulatedFilterValues filterVals;
        filterVals.lowCutFreq = chainSettings.lowCutFreq;
        filterVals.lowCutGain = chainSettings.lowCutGainInDecibels;
        filterVals.lowCutQ = chainSettings.lowCutQuality;
        filterVals.highCutFreq = chainSettings.highCutFreq;
        filterVals.highCutGain = chainSettings.highCutGainInDecibels;
        filterVals.highCutQ = chainSettings.highCutQuality;
        filterVals.peakFreq = chainSettings.peakFreq;
        filterVals.peakGain = chainSettings.peakGainInDecibels;
        filterVals.peakQ = chainSettings.peakQuality;

        pushToFifo(filterFifo, filterFifoBuffer, filterVals);
    }

    if (graphFifo.getFreeSpace() >= 1)
    {
        DistortionGraphValues vals;
        const int bandIndex = juce::jlimit(0, 3, uiFocusBand.load(std::memory_order_relaxed));
        const auto& parameters = bandParameterCache[static_cast<size_t>(bandIndex)];

        const bool shapeEnabled = loadCachedParameter(parameters.shapeEnabled) > 0.5f;
        vals.rec = shapeEnabled
                       ? getBlockModulatedValue(parameters.rec, lfoOutputBuffer)
                       : 0.0f;
        const float bandMix = getBlockModulatedValue(parameters.mix, lfoOutputBuffer);
        const float shapeMix = shapeEnabled
                                   ? getBlockModulatedValue(parameters.shapeMix, lfoOutputBuffer)
                                   : 1.0f;
        vals.mix = bandMix * shapeMix;
        vals.bias = shapeEnabled
                        ? getBlockModulatedValue(parameters.bias, lfoOutputBuffer)
                        : 0.0f;
        float driveBase = getBlockModulatedValue(parameters.drive, lfoOutputBuffer);

        vals.mode = juce::roundToInt(loadCachedParameter(parameters.mode));
        const bool isSafeModeOn = loadCachedParameter(parameters.safe) > 0.5f;

        float driveForCalc = driveBase * 6.5f / 100.0f;
        float powerDrive = powf(2, driveForCalc);
        float sampleMaxValue = getSampleMaxValue(bandIndex);

        if (isSafeModeOn && sampleMaxValue > 0.0001f && sampleMaxValue * powerDrive > 2.0f)
            vals.drive = 2.0f / sampleMaxValue + 0.1f * driveForCalc;
        else
            vals.drive = powerDrive;

        vals.rateDivide = getBlockModulatedValue(downsampleRateParameter, lfoOutputBuffer);
        if (loadCachedParameter(downsampleEnabledParameter) > 0.5f)
            vals.rateDivide = 1.0f;

        pushToFifo(graphFifo, graphFifoBuffer, vals);
    }
    if (meterFifo.getFreeSpace() >= 1)
    {
        MeterValues values;

        // Global Meters
        values.inputRMS_L = mInputLeftRMSGlobal.load();
        values.inputRMS_R = mInputRightRMSGlobal.load();
        values.inputPeak_L = mInputLeftPeakGlobal.load();
        values.inputPeak_R = mInputRightPeakGlobal.load();
        values.outputRMS_L = mOutputLeftRMSGlobal.load();
        values.outputRMS_R = mOutputRightRMSGlobal.load();
        values.outputPeak_L = mOutputLeftPeakGlobal.load();
        values.outputPeak_R = mOutputRightPeakGlobal.load();

        // Per-Band Meters
        for (int i = 0; i < 4; ++i)
        {
            if (auto* band = bands[i].get())
            {
                values.bandInputRMS_L[i] = band->mInputLeftRMS.load();
                values.bandInputRMS_R[i] = band->mInputRightRMS.load();
                values.bandInputPeak_L[i] = band->mInputLeftPeak.load();
                values.bandInputPeak_R[i] = band->mInputRightPeak.load();

                values.bandOutputRMS_L[i] = band->mOutputLeftRMS.load();
                values.bandOutputRMS_R[i] = band->mOutputRightRMS.load();
                values.bandOutputPeak_L[i] = band->mOutputLeftPeak.load();
                values.bandOutputPeak_R[i] = band->mOutputRightPeak.load();
            }
        }

        pushToFifo(meterFifo, meterFifoBuffer, values);
    }
}

//==============================================================================
bool FireAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FireAudioProcessor::createEditor()
{
    return new FireAudioProcessorEditor(*this);
}

//==============================================================================
void FireAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    int xmlIndex = 0;
    juce::XmlElement xmlState { "state" };

    // 1. save treestate (parameters)
    auto state = treeState.copyState();
    std::unique_ptr<juce::XmlElement> treeStateXml(state.createXml());
    xmlState.insertChildElement(treeStateXml.release(), xmlIndex++);

    // 2. save current preset ID, width and height
    auto currentStateXml = std::make_unique<juce::XmlElement>("otherState");
    currentStateXml->setAttribute("currentPresetID", statePresets.getCurrentPresetId());
    currentStateXml->setAttribute("currentPresetKey", statePresets.getCurrentPresetKey());
    currentStateXml->setAttribute("editorWidth", editorWidth.load(std::memory_order_relaxed));
    currentStateXml->setAttribute("editorHeight", editorHeight.load(std::memory_order_relaxed));

    xmlState.insertChildElement(currentStateXml.release(), xmlIndex++);

    const auto lfoDataToSave = lfoManager->getLfoDataCopy();
    const auto routingsToSave = lfoManager->getModulationRoutingsCopy();

    // 3. Save LFO Shapes
    auto lfoState = std::make_unique<juce::XmlElement>("LFO_STATE");
    for (int i = 0; i < static_cast<int>(lfoDataToSave.size()); ++i)
    {
        auto lfoXml = std::make_unique<juce::XmlElement>("LFO");
        lfoXml->setAttribute("index", i);
        lfoDataToSave[static_cast<size_t>(i)].writeToXml(*lfoXml);
        lfoState->addChildElement(lfoXml.release());
    }
    xmlState.insertChildElement(lfoState.release(), xmlIndex++);

    // 4. Save Modulation Matrix Routings
    auto modMatrixState = std::make_unique<juce::XmlElement>("MODULATION_STATE");
    for (const auto& routing : routingsToSave)
    {
        auto routingXml = std::make_unique<juce::XmlElement>("ROUTING");
        routing.writeToXml(*routingXml);
        modMatrixState->addChildElement(routingXml.release());
    }
    xmlState.insertChildElement(modMatrixState.release(), xmlIndex++);

    // Persist the inactive A/B snapshot alongside the currently active state.
    stateAB.writeToXml(xmlState);

    copyXmlToBinary(xmlState, destData);
}

void FireAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName("state"))
        return;

    // Parse and validate every section before mutating live state. Hosts may
    // retain a damaged or truncated chunk for years; applying only its UI/LFO
    // tail while leaving the old parameters in place creates a state that was
    // never saved and is much harder to recover from than rejecting the chunk.
    const auto* xmlTreeState = xmlState->getChildByName(treeState.state.getType().toString());
    if (xmlTreeState == nullptr)
        xmlTreeState = xmlState->getChildElement(0); // Legacy chunks relied on ordering.

    if (xmlTreeState == nullptr)
        return;

    const auto incomingParameterState = juce::ValueTree::fromXml(*xmlTreeState);
    if (! incomingParameterState.isValid()
        || ! incomingParameterState.hasType(treeState.state.getType()))
        return;

    const auto parameterStateTemplate = treeState.copyState();
    juce::ValueTree treeToLoad(parameterStateTemplate.getType());
    juce::StringArray loadedParameterIDs;
    std::array<bool, 4> smoothnessPresentInParameterState {};

    const auto findParameterState = [](juce::ValueTree& state, const juce::String& parameterID)
    {
        for (auto child : state)
            if (child.getProperty("id").toString() == parameterID)
                return child;

        return juce::ValueTree {};
    };

    // Begin with one canonical child for every current parameter. Missing
    // parameters in an older state therefore migrate to their defaults instead
    // of inheriting whatever value happened to be live before the load.
    for (const auto& templateChild : parameterStateTemplate)
    {
        const auto parameterID = templateChild.getProperty("id").toString();
        auto* parameter = treeState.getParameter(parameterID);
        if (parameter == nullptr)
            continue;

        auto child = templateChild.createCopy();
        child.setProperty("value",
                          parameter->convertFrom0to1(parameter->getDefaultValue()),
                          nullptr);
        treeToLoad.addChild(child, -1, nullptr);
    }

    int recognisedParameterCount = 0;
    for (const auto& incomingChild : incomingParameterState)
    {
        const auto parameterID = incomingChild.getProperty("id").toString();
        auto* parameter = treeState.getParameter(parameterID);
        if (parameterID.isEmpty() || parameter == nullptr
            || loadedParameterIDs.contains(parameterID))
            continue;

        if (! incomingChild.hasProperty("value"))
            return;

        double parsedValue = 0.0;
        if (! parseStrictFiniteDouble(incomingChild.getProperty("value").toString(), parsedValue))
            return;

        // APVTS child values are denormalised. Clamp before snapping so a
        // damaged out-of-range chunk does not trip JUCE's debug assertion in
        // convertTo0to1(), while integer/choice parameters remain legal.
        const auto& range = parameter->getNormalisableRange();
        const float boundedValue = juce::jlimit(range.start,
                                                range.end,
                                                static_cast<float>(parsedValue));
        const float safeValue = range.snapToLegalValue(boundedValue);
        auto targetChild = findParameterState(treeToLoad, parameterID);
        if (! targetChild.isValid())
            return;

        targetChild.setProperty("value", safeValue, nullptr);
        loadedParameterIDs.add(parameterID);
        ++recognisedParameterCount;

        for (int i = 0; i < static_cast<int>(smoothnessPresentInParameterState.size()); ++i)
            if (parameterID == ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i))
                smoothnessPresentInParameterState[static_cast<size_t>(i)] = true;
    }

    // A correctly named but empty/foreign PARAMETERS node is not a usable Fire
    // state. Reject it transactionally rather than interpreting it as Init.
    if (recognisedParameterCount == 0)
        return;

    // Shape was always active before its explicit enable parameter existed.
    // Preserve that sound for old host chunks. The root-property lookup retains
    // compatibility with the brief legacy representation used by some builds.
    for (int i = 0; i < 4; ++i)
    {
        const auto shapeID = ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, i);
        if (loadedParameterIDs.contains(shapeID))
            continue;

        float legacyShapeValue = 1.0f;
        if (incomingParameterState.hasProperty(shapeID))
        {
            double parsedValue = 0.0;
            if (! parseStrictFiniteDouble(incomingParameterState.getProperty(shapeID).toString(), parsedValue))
                return;
            legacyShapeValue = parsedValue > 0.5 ? 1.0f : 0.0f;
        }

        if (auto shapeState = findParameterState(treeToLoad, shapeID); shapeState.isValid())
            shapeState.setProperty("value", legacyShapeValue, nullptr);
    }

    std::array<LfoData, 4> loadedLfoData;
    std::array<bool, 4> loadedLfoSmoothnessFromXml {};
    std::array<bool, 4> loadedLfoIndices {};
    if (auto* lfoState = xmlState->getChildByName("LFO_STATE"))
    {
        for (auto* lfoXml : lfoState->getChildIterator())
        {
            if (! lfoXml->hasTagName("LFO"))
                continue;

            const int index = lfoXml->getIntAttribute("index", -1);
            if (juce::isPositiveAndBelow(index, static_cast<int>(loadedLfoData.size()))
                && ! loadedLfoIndices[static_cast<size_t>(index)])
            {
                loadedLfoData[static_cast<size_t>(index)] = LfoData::readFromXml(*lfoXml);
                loadedLfoSmoothnessFromXml[static_cast<size_t>(index)] = lfoXml->hasAttribute("smoothness");
                loadedLfoIndices[static_cast<size_t>(index)] = true;
            }
        }
    }

    // The staged tree, rather than the still-live APVTS atomics, is authoritative
    // while this transaction is being assembled.
    for (size_t i = 0; i < loadedLfoData.size(); ++i)
    {
        if (! smoothnessPresentInParameterState[i] && loadedLfoSmoothnessFromXml[i])
            continue;

        const auto smoothnessID = ParameterIDAndName::getIDString(
            LFO_SMOOTH_ID, static_cast<int>(i));
        const auto smoothnessState = findParameterState(treeToLoad, smoothnessID);
        if (smoothnessState.isValid())
        {
            const float smoothness = static_cast<float>(smoothnessState.getProperty("value"));
            if (std::isfinite(smoothness))
                loadedLfoData[i].smoothness = juce::jlimit(0.0f, 1.0f, smoothness);
        }
    }

    constexpr int maximumStateModulationRoutings = 128;
    juce::Array<ModulationRouting> loadedRoutings;
    juce::StringArray loadedRoutingTargets;
    if (auto* modMatrixState = xmlState->getChildByName("MODULATION_STATE"))
    {
        for (auto* routingXml : modMatrixState->getChildIterator())
        {
            if (loadedRoutings.size() >= maximumStateModulationRoutings)
                break;
            if (! routingXml->hasTagName("ROUTING"))
                continue;

            auto routing = ModulationRouting::readFromXml(*routingXml);
            if (routing.targetParameterID.isEmpty()
                || treeState.getParameter(routing.targetParameterID) == nullptr
                || loadedRoutingTargets.contains(routing.targetParameterID))
                continue;

            loadedRoutingTargets.add(routing.targetParameterID);
            loadedRoutings.add(std::move(routing));
        }
    }

    const auto* xmlCurrentState = xmlState->getChildByName("otherState");
    const auto presetKey = xmlCurrentState != nullptr
                               ? xmlCurrentState->getStringAttribute("currentPresetKey").trim()
                               : juce::String {};
    const int legacyPresetID = xmlCurrentState != nullptr
                                   ? juce::jlimit(0,
                                                  statePresets.getNumPresets(),
                                                  xmlCurrentState->getIntAttribute("currentPresetID", 0))
                                   : 0;
    const int restoredEditorWidth = xmlCurrentState != nullptr
                                        ? juce::jlimit(static_cast<int>(INIT_WIDTH),
                                                       2000,
                                                       xmlCurrentState->getIntAttribute(
                                                           "editorWidth", static_cast<int>(INIT_WIDTH)))
                                        : editorWidth.load(std::memory_order_relaxed);
    const int restoredEditorHeight = xmlCurrentState != nullptr
                                         ? juce::jlimit(static_cast<int>(INIT_HEIGHT),
                                                        1000,
                                                        xmlCurrentState->getIntAttribute(
                                                            "editorHeight", static_cast<int>(INIT_HEIGHT)))
                                         : editorHeight.load(std::memory_order_relaxed);

    // Commit only after the complete chunk has passed validation.
    treeState.replaceState(treeToLoad);
    if (xmlCurrentState != nullptr)
    {
        if (presetKey.isNotEmpty())
            statePresets.setCurrentPresetKey(presetKey);
        else
            statePresets.setCurrentPresetId(legacyPresetID);
        editorWidth.store(restoredEditorWidth, std::memory_order_relaxed);
        editorHeight.store(restoredEditorHeight, std::memory_order_relaxed);
    }

    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        lfoManager->clearAllLfoData();
        for (int i = 0; i < static_cast<int>(loadedLfoData.size()); ++i)
            lfoManager->setLfoData(i, loadedLfoData[static_cast<size_t>(i)]);
        lfoManager->getModulationRoutings() = std::move(loadedRoutings);
    }

    // A/B restoration mutates its internal snapshot, so keep it inside the
    // commit phase after the complete host chunk has passed validation.
    stateAB.readFromXml(xmlState->getChildByName("AB_STATE"));

    // State replacement can migrate band parameters while NUM_BANDS stays
    // unchanged. Publish an explicit topology generation so the next audio
    // block cannot reuse DSP history from the previous logical band layout.
    requestMultibandTopologyReset();
    sendChangeMessage();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FireAudioProcessor();
}

// Filter selection
void updateCoefficients(CoefficientsPtr& old, const CoefficientsPtr& replacements)
{
    *old = *replacements;
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowCutFreq = apvts.getRawParameterValue(LOWCUT_FREQ_ID)->load();
    settings.lowCutQuality = apvts.getRawParameterValue(LOWCUT_Q_ID)->load();
    settings.lowCutGainInDecibels = apvts.getRawParameterValue(LOWCUT_GAIN_ID)->load();
    settings.highCutFreq = apvts.getRawParameterValue(HIGHCUT_FREQ_ID)->load();
    settings.highCutQuality = apvts.getRawParameterValue(HIGHCUT_Q_ID)->load();
    settings.highCutGainInDecibels = apvts.getRawParameterValue(HIGHCUT_GAIN_ID)->load();
    settings.peakFreq = apvts.getRawParameterValue(PEAK_FREQ_ID)->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue(PEAK_GAIN_ID)->load();
    settings.peakQuality = apvts.getRawParameterValue(PEAK_Q_ID)->load();
    settings.lowCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(LOWCUT_SLOPE_ID));
    settings.highCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID));

    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load() > 0.5f;

    return settings;
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts, LfoManager& lfoManager)
{
    ChainSettings settings;

    // For each filter parameter, get its final value from the LfoManager.
    settings.lowCutFreq = lfoManager.getModulatedValue(LOWCUT_FREQ_ID);
    settings.lowCutQuality = lfoManager.getModulatedValue(LOWCUT_Q_ID);
    settings.lowCutGainInDecibels = lfoManager.getModulatedValue(LOWCUT_GAIN_ID);
    settings.highCutFreq = lfoManager.getModulatedValue(HIGHCUT_FREQ_ID);
    settings.highCutQuality = lfoManager.getModulatedValue(HIGHCUT_Q_ID);
    settings.highCutGainInDecibels = lfoManager.getModulatedValue(HIGHCUT_GAIN_ID);
    settings.peakFreq = lfoManager.getModulatedValue(PEAK_FREQ_ID);
    settings.peakGainInDecibels = lfoManager.getModulatedValue(PEAK_GAIN_ID);
    settings.peakQuality = lfoManager.getModulatedValue(PEAK_Q_ID);

    // These parameters are not modulatable, so we get them directly from the apvts.
    settings.lowCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(LOWCUT_SLOPE_ID));
    settings.highCutSlope = getSlopeParameterValue(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID));
    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load() > 0.5f;

    return settings;
}

CoefficientsPtr makePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.peakFreq,
                                                               chainSettings.peakQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
}

CoefficientsPtr makeLowcutQFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.lowCutFreq,
                                                               chainSettings.lowCutQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.lowCutGainInDecibels));
}

CoefficientsPtr makeHighcutQFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.highCutFreq,
                                                               chainSettings.highCutQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.highCutGainInDecibels));
}

void FireAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    const auto peakCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate,
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    leftChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    rightChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    *leftChain.get<ChainPositions::Peak>().coefficients = peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = peakCoefficients;
}

void FireAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings, double sampleRate)
{
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();

    const auto lowcutQCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate,
        chainSettings.lowCutFreq,
        chainSettings.lowCutQuality,
        juce::Decibels::decibelsToGain(chainSettings.lowCutGainInDecibels));

    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    leftChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);

    updateButterworthCutFilter(rightLowCut,
                               chainSettings.lowCutFreq,
                               sampleRate,
                               chainSettings.lowCutSlope,
                               true);
    updateButterworthCutFilter(leftLowCut,
                               chainSettings.lowCutFreq,
                               sampleRate,
                               chainSettings.lowCutSlope,
                               true);

    *leftChain.get<ChainPositions::LowCutQ>().coefficients = lowcutQCoefficients;
    *rightChain.get<ChainPositions::LowCutQ>().coefficients = lowcutQCoefficients;
}

void FireAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings, double sampleRate)
{
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

    const auto highcutQCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        sampleRate,
        chainSettings.highCutFreq,
        chainSettings.highCutQuality,
        juce::Decibels::decibelsToGain(chainSettings.highCutGainInDecibels));

    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    leftChain.setBypassed<ChainPositions::HighCutQ>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCutQ>(chainSettings.highCutBypassed);

    updateButterworthCutFilter(leftHighCut,
                               chainSettings.highCutFreq,
                               sampleRate,
                               chainSettings.highCutSlope,
                               false);
    updateButterworthCutFilter(rightHighCut,
                               chainSettings.highCutFreq,
                               sampleRate,
                               chainSettings.highCutSlope,
                               false);

    *leftChain.get<ChainPositions::HighCutQ>().coefficients = highcutQCoefficients;
    *rightChain.get<ChainPositions::HighCutQ>().coefficients = highcutQCoefficients;
}

void FireAudioProcessor::updateFilter(double sampleRate)
{
    auto chainSettings = getChainSettings(treeState);
    if (! std::isfinite(sampleRate) || sampleRate <= 0.0)
        return;

    const auto frequencyRange = getSafeFilterFrequencyRange(sampleRate);

    chainSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.highCutFreq);

    // Set the target values for all 9 smoothers
    lowcutFreqSmoother.setTargetValue(chainSettings.lowCutFreq);
    lowcutGainSmoother.setTargetValue(chainSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.setTargetValue(chainSettings.lowCutQuality);

    peakFreqSmoother.setTargetValue(chainSettings.peakFreq);
    peakGainSmoother.setTargetValue(chainSettings.peakGainInDecibels);
    peakQualitySmoother.setTargetValue(chainSettings.peakQuality);

    highcutFreqSmoother.setTargetValue(chainSettings.highCutFreq);
    highcutGainSmoother.setTargetValue(chainSettings.highCutGainInDecibels);
    highcutQualitySmoother.setTargetValue(chainSettings.highCutQuality);

    // Create a new settings object with the smoothed values for this audio block
    ChainSettings smoothedSettings;
    smoothedSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum,
                                               frequencyRange.maximum,
                                               lowcutFreqSmoother.getNextValue());
    smoothedSettings.lowCutGainInDecibels = lowcutGainSmoother.getNextValue();
    smoothedSettings.lowCutQuality = lowcutQualitySmoother.getNextValue();

    smoothedSettings.peakFreq = juce::jlimit(frequencyRange.minimum,
                                            frequencyRange.maximum,
                                            peakFreqSmoother.getNextValue());
    smoothedSettings.peakGainInDecibels = peakGainSmoother.getNextValue();
    smoothedSettings.peakQuality = peakQualitySmoother.getNextValue();

    smoothedSettings.highCutFreq = juce::jlimit(frequencyRange.minimum,
                                                frequencyRange.maximum,
                                                highcutFreqSmoother.getNextValue());

    smoothedSettings.highCutGainInDecibels = highcutGainSmoother.getNextValue();
    smoothedSettings.highCutQuality = highcutQualitySmoother.getNextValue();

    // Slopes and bypass states don't need smoothing, use them directly
    smoothedSettings.lowCutSlope = chainSettings.lowCutSlope;
    smoothedSettings.highCutSlope = chainSettings.highCutSlope;
    smoothedSettings.lowCutBypassed = chainSettings.lowCutBypassed;
    smoothedSettings.peakBypassed = chainSettings.peakBypassed;
    smoothedSettings.highCutBypassed = chainSettings.highCutBypassed;

    // Call the modular update functions
    updateLowCutFilters(smoothedSettings, sampleRate);
    updatePeakFilter(smoothedSettings, sampleRate);
    updateHighCutFilters(smoothedSettings, sampleRate);

    cachedGlobalFilterSettings = smoothedSettings;
    cachedGlobalFilterSampleRate = sampleRate;
    globalFilterCacheValid = true;
}

bool FireAudioProcessor::isSlient(const juce::AudioBuffer<float>& buffer)
{
    return buffer.getNumChannels() == 0
        || buffer.getNumSamples() == 0
        || buffer.getMagnitude(0, buffer.getNumSamples()) == 0.0f;
}

void FireAudioProcessor::setHistoryArray(int bandIndex)
{
    historySourceBand.store(juce::isPositiveAndBelow(bandIndex, 4) ? bandIndex : 4,
                            std::memory_order_relaxed);
}

void FireAudioProcessor::captureHistorySamples()
{
    const std::array<const juce::AudioBuffer<float>*, 5> sourceBuffers {
        &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4, &mWetBuffer
    };
    const int sourceIndex = juce::jlimit(0, 4, historySourceBand.load(std::memory_order_relaxed));
    const auto* sourceBuffer = sourceBuffers[static_cast<size_t>(sourceIndex)];

    if (sourceBuffer == nullptr || sourceBuffer->getNumChannels() == 0 || sourceBuffer->getNumSamples() == 0)
        return;

    const auto* left = sourceBuffer->getReadPointer(0);
    const auto* right = sourceBuffer->getNumChannels() > 1 ? sourceBuffer->getReadPointer(1) : left;
    int writePosition = historyWritePosition.load(std::memory_order_relaxed);

    for (int sample = 0; sample < sourceBuffer->getNumSamples(); sample += 10)
    {
        const auto index = static_cast<size_t>(writePosition);
        historyArrayL[index].store(left[sample], std::memory_order_relaxed);
        historyArrayR[index].store(right[sample], std::memory_order_relaxed);
        writePosition = (writePosition + 1) % historyLength;
    }

    historySamplesAvailable.store(historyLength, std::memory_order_relaxed);
    historyWritePosition.store(writePosition, std::memory_order_release);
    historyGeneration.fetch_add(1, std::memory_order_release);
}

std::uint64_t FireAudioProcessor::getHistoryGeneration() const noexcept
{
    return historyGeneration.load(std::memory_order_acquire);
}

void FireAudioProcessor::copyHistoryArrays(juce::Array<float>& leftDestination,
                                           juce::Array<float>& rightDestination) const
{
    const int writePosition = historyWritePosition.load(std::memory_order_acquire);
    const int count = juce::jlimit(0, historyLength, historySamplesAvailable.load(std::memory_order_relaxed));
    const int start = (writePosition - count + historyLength) % historyLength;

    leftDestination.resize(count);
    rightDestination.resize(count);
    for (int i = 0; i < count; ++i)
    {
        const auto index = static_cast<size_t>((start + i) % historyLength);
        leftDestination.setUnchecked(i, historyArrayL[index].load(std::memory_order_relaxed));
        rightDestination.setUnchecked(i, historyArrayR[index].load(std::memory_order_relaxed));
    }
}

juce::Array<float> FireAudioProcessor::getHistoryArrayL()
{
    juce::Array<float> left;
    juce::Array<float> right;
    copyHistoryArrays(left, right);
    return left;
}

juce::Array<float> FireAudioProcessor::getHistoryArrayR()
{
    juce::Array<float> left;
    juce::Array<float> right;
    copyHistoryArrays(left, right);
    return right;
}

int FireAudioProcessor::getNumBins() const noexcept
{
    return SpectrumProcessor::numBins;
}

int FireAudioProcessor::getFFTSize() const noexcept
{
    return SpectrumProcessor::fftSize;
}

bool FireAudioProcessor::popLatestFFTFrames(float* processedDestination,
                                            int processedDestinationSize,
                                            float* originalDestination,
                                            int originalDestinationSize) noexcept
{
    return spectrumProcessor.popLatestFramePair(processedDestination,
                                                 processedDestinationSize,
                                                 originalDestination,
                                                 originalDestinationSize);
}

void FireAudioProcessor::pushDataPairToFFT(const juce::AudioBuffer<float>& processedBuffer,
                                           const juce::AudioBuffer<float>& originalBuffer)
{
    if (processedBuffer.getNumChannels() <= 0 || originalBuffer.getNumChannels() <= 0)
        return;

    const auto* processedData = processedBuffer.getReadPointer(0);
    const auto* originalData = originalBuffer.getReadPointer(0);
    const int samplesToPush = juce::jmin(processedBuffer.getNumSamples(),
                                          originalBuffer.getNumSamples());

    for (int sample = 0; sample < samplesToPush; ++sample)
        spectrumProcessor.pushNextSamplePairIntoFifo(processedData[sample], originalData[sample]);
}

bool FireAudioProcessor::processFFT(float* tempFFTData, int bufferSize)
{
    return spectrumProcessor.doProcessing(tempFFTData, bufferSize);
}

int FireAudioProcessor::getSavedWidth() const
{
    return editorWidth.load(std::memory_order_relaxed);
}

int FireAudioProcessor::getSavedHeight() const
{
    return editorHeight.load(std::memory_order_relaxed);
}

void FireAudioProcessor::setSavedWidth(const int width)
{
    editorWidth.store(width, std::memory_order_relaxed);
}

void FireAudioProcessor::setSavedHeight(const int height)
{
    editorHeight.store(height, std::memory_order_relaxed);
}

bool FireAudioProcessor::getBypassedState() const
{
    return isBypassed.load(std::memory_order_relaxed);
}

// drive lookandfeel
float FireAudioProcessor::getReductionPrecent(int bandIndex)
{
    // Safely check if the provided index is valid for our bands vector.
    if (juce::isPositiveAndBelow(bandIndex, bands.size()))
        if (auto* band = bands[bandIndex].get())
            return band->mReductionPercent.load(std::memory_order_relaxed);

    // If the index is invalid, assert in debug mode and return a safe default.
    jassertfalse;
    return 0.0f;
}

float FireAudioProcessor::getSampleMaxValue(int bandIndex)
{
    if (juce::isPositiveAndBelow(bandIndex, bands.size()))
        if (auto* band = bands[bandIndex].get())
            return band->mSampleMaxValue.load(std::memory_order_relaxed);

    jassertfalse;
    return 0.0f;
}

juce::AudioProcessorValueTreeState::ParameterLayout FireAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    using PBool = juce::AudioParameterBool;
    using PInt = juce::AudioParameterInt;
    using PFloat = juce::AudioParameterFloat;
    using PChoice = juce::AudioParameterChoice;

    // --- Global Parameters ---
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(HQ_ID), HQ_NAME, false));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(OUTPUT_ID), GLOBAL_OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(MIX_ID), GLOBAL_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(NUM_BANDS_ID), NUM_BANDS_NAME, 1, 4, 1));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(FILTER_BYPASS_ID), FILTER_BYPASS_NAME, false));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(DOWNSAMPLE_ID), DOWNSAMPLE_NAME, juce::NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(BIT_DEPTH_ID), BIT_DEPTH_NAME, 4, 32, 32));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(JITTER_ID), JITTER_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(DOWNSAMPLE_MIX_ID), DOWNSAMPLE_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(DOWNSAMPLE_BYPASS_ID), DOWNSAMPLE_BYPASS_NAME, false));

    // --- Per-Band Parameters (created in a loop) ---
    for (int i = 0; i < 4; ++i)
    {
        parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(MODE_ID, i), MODE_NAME, 0, 11, 3));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LINKED_ID, i), LINKED_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(SAFE_ID, i), SAFE_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(EXTREME_ID, i), EXTREME_NAME, false));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(DRIVE_ID, i), DRIVE_NAME, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_RATIO_ID, i), COMP_RATIO_NAME, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_THRESH_ID, i), COMP_THRESH_NAME, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_ATTACK_ID, i), COMP_ATTACK_NAME, juce::NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.3f), 10.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_RELEASE_ID, i), COMP_RELEASE_NAME, juce::NormalisableRange<float>(10.0f, 2000.0f, 1.f, 0.3f), 100.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(COMP_MIX_ID, i), COMP_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(WIDTH_ID, i), WIDTH_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PAN_ID, i), PAN_NAME, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(WIDTH_MIX_ID, i), WIDTH_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(OUTPUT_ID, i), OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(MIX_ID, i), MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(BIAS_ID, i), BIAS_NAME, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(REC_ID, i), REC_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(SHAPE_MIX_ID, i), SHAPE_MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_ENABLE_ID, i), BAND_ENABLE_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_SOLO_ID, i), BAND_SOLO_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(DRIVE_BYPASS_ID, i), DRIVE_BYPASS_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(COMP_BYPASS_ID, i), COMP_BYPASS_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(WIDTH_BYPASS_ID, i), WIDTH_BYPASS_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(SHAPE_BYPASS_ID, i), SHAPE_BYPASS_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(DC_FILTER_ID, i), DC_FILTER_NAME, false));
    }

    // --- Crossover Parameters ---
    juce::NormalisableRange<float> freqRange(40.0f, 10024.0f, 1.0f);
    freqRange.setSkewForCentre(651.0f);

    for (int i = 0; i < 3; ++i)
    {
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(FREQ_ID, i), ParameterIDAndName::getName(FREQ_NAME, i), freqRange, 21.0f));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LINE_STATE_ID, i), ParameterIDAndName::getName(LINE_STATE_NAME, i), false));
    }

    // --- Global Filter Parameters ---
    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.0f);
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(LOWCUT_FREQ_ID), LOWCUT_FREQ_NAME, cutoffRange, 20.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(LOWCUT_Q_ID), LOWCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(LOWCUT_GAIN_ID), LOWCUT_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(HIGHCUT_FREQ_ID), HIGHCUT_FREQ_NAME, cutoffRange, 20000.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(HIGHCUT_Q_ID), HIGHCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(HIGHCUT_GAIN_ID), HIGHCUT_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PEAK_FREQ_ID), PEAK_FREQ_NAME, cutoffRange, 1000.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PEAK_Q_ID), PEAK_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(PEAK_GAIN_ID), PEAK_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(LOWCUT_SLOPE_ID), LOWCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<PInt>(ParameterIDAndName::getID(HIGHCUT_SLOPE_ID), HIGHCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LOWCUT_BYPASSED_ID), LOWCUT_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(PEAK_BYPASSED_ID), PEAK_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(HIGHCUT_BYPASSED_ID), HIGHCUT_BYPASSED_NAME, false));

    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(OFF_ID), OFF_NAME, true));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(PRE_ID), PRE_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(POST_ID), POST_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(LOW_ID), LOW_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_ID), BAND_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(HIGH_ID), HIGH_NAME, true));

    juce::StringArray lfoRateSyncDivisions = {
        "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T", "1/2", "1 Bar", "2 Bars", "4 Bars"
    };

    // --- Per-LFO Parameters (created in a loop using the new structure) ---
    for (int i = 0; i < 4; ++i)
    {
        parameters.push_back(std::make_unique<PBool>(
            ParameterIDAndName::getID(LFO_SYNC_MODE_ID, i),
            LFO_SYNC_MODE_NAME,
            true));

        parameters.push_back(std::make_unique<PChoice>(
            ParameterIDAndName::getID(LFO_RATE_SYNC_ID, i),
            LFO_RATE_SYNC_NAME,
            lfoRateSyncDivisions,
            8 // Default index for "1/4"
            ));

        parameters.push_back(std::make_unique<PFloat>(
            ParameterIDAndName::getID(LFO_RATE_HZ_ID, i),
            LFO_RATE_HZ_NAME,
            juce::NormalisableRange<float>(0.01f, 100.0f, 0.01f, 0.3f),
            1.0f,
            "Hz"));

        parameters.push_back(std::make_unique<PFloat>(
            ParameterIDAndName::getID(LFO_SMOOTH_ID, i),
            LFO_SMOOTH_NAME,
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.0f));

        parameters.push_back(std::make_unique<PFloat>(
            ParameterIDAndName::getID(LFO_PHASE_ID, i),
            LFO_PHASE_NAME,
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.0f));
    }

    return { parameters.begin(), parameters.end() };
}

bool FireAudioProcessor::isDawPlaying() const
{
    return lfoManager->isDawPlaying();
}

float FireAudioProcessor::getLfoPhase(int lfoIndex) const
{
    return lfoManager->getLfoPhase(lfoIndex);
}

void FireAudioProcessor::updateParameters()
{
    //==============================================================================
    // 1. Update Global and Crossover Parameters
    //==============================================================================

    // Get the number of active bands for processing loops. A count change can
    // also originate in host automation, where there is no UI callback to
    // request the reset explicitly.
    const auto requestedGeneration = multibandTopologyResetGeneration.load(std::memory_order_acquire);
    const int requestedNumBands = juce::jlimit(
        1, 4, juce::roundToInt(loadCachedParameter(numBandsParameter, 1.0f)));
    const bool topologyChanged = requestedNumBands != numBands
                                 || requestedGeneration != appliedMultibandTopologyResetGeneration;

    numBands = requestedNumBands;
    activeCrossovers = numBands - 1;

    if (topologyChanged)
    {
        // Slot reuse after an add/remove must not inherit compressor,
        // oversampling, mixer or crossover history from the previous logical
        // band. Snapping the three smoothers also prevents the first block of
        // a newly enabled crossover from ramping up from its hidden value.
        resetMultibandProcessingState();
        snapCrossoverSmoothersToParameters();
        appliedMultibandTopologyResetGeneration = requestedGeneration;
    }
    else
    {
        // Ordinary divider dragging remains smoothly interpolated.
        const auto frequencies = getEffectiveCrossoverFrequencies();
        smoothedFreq1.setTargetValue(frequencies[0]);
        smoothedFreq2.setTargetValue(frequencies[1]);
        smoothedFreq3.setTargetValue(frequencies[2]);
    }

    //==============================================================================
    // 2. Update Per-Band Smoothed Parameters
    //==============================================================================

    // Iterate through the bands vector to update each BandProcessor's smoothers.
    for (int i = 0; i < 4; ++i)
    {
        if (auto* band = bands[i].get())
        {
            const auto& parameters = bandParameterCache[static_cast<size_t>(i)];
            band->recSmoother.setTargetValue(loadCachedParameter(parameters.rec));
            band->biasSmoother.setTargetValue(loadCachedParameter(parameters.bias));
        }
    }

    //==============================================================================
    // 3. Update Global Filter Smoothed Parameters
    //==============================================================================

    // Use the existing helper function to get all filter-related parameter values.
    auto chainSettings = getCachedChainSettings(nullptr);

    // Set the target values for all global filter smoothers.
    lowcutFreqSmoother.setTargetValue(chainSettings.lowCutFreq);
    lowcutGainSmoother.setTargetValue(chainSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.setTargetValue(chainSettings.lowCutQuality);

    peakFreqSmoother.setTargetValue(chainSettings.peakFreq);
    peakGainSmoother.setTargetValue(chainSettings.peakGainInDecibels);
    peakQualitySmoother.setTargetValue(chainSettings.peakQuality);

    highcutFreqSmoother.setTargetValue(chainSettings.highCutFreq);
    highcutGainSmoother.setTargetValue(chainSettings.highCutGainInDecibels);
    highcutQualitySmoother.setTargetValue(chainSettings.highCutQuality);
}

void FireAudioProcessor::sumBands(juce::AudioBuffer<float>& outputBuffer,
                                  const std::array<juce::AudioBuffer<float>*, 4>& sourceBandBuffers,
                                  bool ignoreSoloLogic = false)
{
    outputBuffer.clear();

    bool anySoloActive = false;
    // Only check for solo state if we are NOT ignoring the solo logic.
    if (! ignoreSoloLogic)
    {
        for (int i = 0; i < numBands; ++i)
        {
            if (loadCachedParameter(bandParameterCache[static_cast<size_t>(i)].solo) > 0.5f)
            {
                anySoloActive = true;
                break;
            }
        }
    }

    // The main summing loop now operates on the provided sourceBandBuffers.
    for (int i = 0; i < numBands; ++i)
    {
        // Get a pointer to the current source buffer for this band.
        auto* currentBandBuffer = sourceBandBuffers[i];

        // Skip if the buffer pointer is invalid for any reason.
        if (currentBandBuffer == nullptr)
            continue;

        const bool isThisBandSoloed = loadCachedParameter(
                                          bandParameterCache[static_cast<size_t>(i)].solo)
                                      > 0.5f;

        // Determine if this band should be added to the mix.
        const bool shouldAddBand = anySoloActive ? isThisBandSoloed : true;

        if (shouldAddBand || ignoreSoloLogic)
        {
            // The number of samples to process for this operation.
            const int numSamples = juce::jmin(currentBandBuffer->getNumSamples(), outputBuffer.getNumSamples());

            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            {
                // Ensure the destination buffer has enough space before adding.
                // This is a safety check.
                if (channel < currentBandBuffer->getNumChannels())
                {
                    // Use the number of samples from the *source* buffer, not the destination.
                    outputBuffer.addFrom(channel, 0, *currentBandBuffer, channel, 0, numSamples);
                }
            }
        }
    }
}

void FireAudioProcessor::splitBands(const juce::AudioBuffer<float>& inputBuffer, double sampleRate)
{
    // This function encapsulates the entire multiband crossover logic.
    // The code is moved directly from your original processBlock.

    const int totalNumOutputChannels = inputBuffer.getNumChannels();
    const int numSamples = inputBuffer.getNumSamples();

    if (! std::isfinite(sampleRate) || sampleRate <= 0.0)
        return;
    const int lineNum = activeCrossovers; // Use the member variable updated in updateParameters()

    // Get the latest smoothed frequency values for the crossovers
    float freqValue1 = smoothedFreq1.getNextValue();
    float freqValue2 = smoothedFreq2.getNextValue();
    float freqValue3 = smoothedFreq3.getNextValue();
    if (numSamples > 1)
    {
        smoothedFreq1.skip(numSamples - 1);
        smoothedFreq2.skip(numSamples - 1);
        smoothedFreq3.skip(numSamples - 1);
    }

    const auto frequencyRange = getSafeFilterFrequencyRange(sampleRate);

    freqValue1 = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, freqValue1);
    freqValue2 = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, freqValue2);
    freqValue3 = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, freqValue3);

    // Set up crossover filters with these frequencies
    lowpass1.setCutoffFrequency(freqValue1);
    highpass1.setCutoffFrequency(freqValue1);
    lowpass2.setCutoffFrequency(freqValue2);
    highpass2.setCutoffFrequency(freqValue2);
    lowpass3.setCutoffFrequency(freqValue3);
    highpass3.setCutoffFrequency(freqValue3);

    // Set up the all-pass branches that make each asymmetric crossover tree
    // share one phase response before its bands are summed.
    if (lineNum == 2)
    {
        // The low branch must pass through the phase response of the second
        // (f2) split so it remains aligned with the mid+high branches.
        compensatorLP.setCutoffFrequency(freqValue2);
        compensatorHP.setCutoffFrequency(freqValue2);
    }
    else if (lineNum == 3)
    {
        // Low half of the f2 split must also carry the phase of f3, while the
        // high half must carry the phase of f1. The subsequent f1/f3 splits
        // then produce A1*A3*(LP2+HP2), which sums flat.
        compensatorLP.setCutoffFrequency(freqValue3);
        compensatorHP.setCutoffFrequency(freqValue3);
        secondCompensatorLP.setCutoffFrequency(freqValue1);
        secondCompensatorHP.setCutoffFrequency(freqValue1);
    }

    // --- TREE-BASED SIGNAL SPLITTING ---
    if (lineNum == 0)
    { // 1 Band: No splitting, the signal just passes through
        mBuffer1.makeCopyOf(inputBuffer, true);
    }
    else if (lineNum == 1)
    { // 2 Bands: One split
        mBuffer1.makeCopyOf(inputBuffer, true);
        mBuffer2.makeCopyOf(inputBuffer, true);

        auto block1 = juce::dsp::AudioBlock<float>(mBuffer1);
        auto context1 = juce::dsp::ProcessContextReplacing<float>(block1);
        lowpass1.process(context1);

        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        highpass1.process(context2);
    }
    else if (lineNum == 2)
    { // 3 Bands: Asymmetric tree with Dummy Filter compensation
        mSplitTemp1.setSize(totalNumOutputChannels, numSamples, false, false, true);
        mSplitTemp2.setSize(totalNumOutputChannels, numSamples, false, false, true);
        auto& highPassBuffer = mSplitTemp1;
        auto& compensatorHighBuffer = mSplitTemp2;

        mBuffer1.makeCopyOf(inputBuffer, true);
        highPassBuffer.makeCopyOf(inputBuffer, true);

        // First create the actual low branch at f1.
        auto lowBranchBlock = juce::dsp::AudioBlock<float>(mBuffer1);
        auto lowBranchContext = juce::dsp::ProcessContextReplacing<float>(lowBranchBlock);
        lowpass1.process(lowBranchContext);

        // Then pass that low branch through the all-pass response of the f2
        // split so all three branches have the same phase response.
        compensatorHighBuffer.makeCopyOf(mBuffer1, true);

        auto compLpBlock = juce::dsp::AudioBlock<float>(mBuffer1);
        auto compLpContext = juce::dsp::ProcessContextReplacing<float>(compLpBlock);
        compensatorLP.process(compLpContext);

        auto compHpBlock = juce::dsp::AudioBlock<float>(compensatorHighBuffer);
        auto compHpContext = juce::dsp::ProcessContextReplacing<float>(compHpBlock);
        compensatorHP.process(compHpContext);

        // LR low-pass + high-pass is an all-pass response. The old code
        // discarded this high-pass half, causing a dip around the crossover.
        for (int channel = 0; channel < mBuffer1.getNumChannels(); ++channel)
            mBuffer1.addFrom(channel, 0, compensatorHighBuffer, channel, 0, numSamples);

        // Real Split Path for Band 2 and 3
        auto highPassBlock = juce::dsp::AudioBlock<float>(highPassBuffer);
        auto highPassContext = juce::dsp::ProcessContextReplacing<float>(highPassBlock);
        highpass1.process(highPassContext);

        mBuffer2.makeCopyOf(highPassBuffer, true);
        mBuffer3.makeCopyOf(highPassBuffer, true);

        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        lowpass2.process(context2);

        auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
        highpass2.process(context3);
    }
    else if (lineNum == 3)
    { // 4 Bands: Symmetric tree
        mSplitTemp1.setSize(totalNumOutputChannels, numSamples, false, false, true);
        mSplitTemp2.setSize(totalNumOutputChannels, numSamples, false, false, true);
        mSplitTemp3.setSize(totalNumOutputChannels, numSamples, false, false, true);
        auto& lowMidBuffer = mSplitTemp1;
        auto& highMidBuffer = mSplitTemp2;
        auto& allPassScratch = mSplitTemp3;
        lowMidBuffer.makeCopyOf(inputBuffer, true);
        highMidBuffer.makeCopyOf(inputBuffer, true);

        auto lowMidBlock = juce::dsp::AudioBlock<float>(lowMidBuffer);
        auto highMidBlock = juce::dsp::AudioBlock<float>(highMidBuffer);
        auto lowMidContext = juce::dsp::ProcessContextReplacing<float>(lowMidBlock);
        auto highMidContext = juce::dsp::ProcessContextReplacing<float>(highMidBlock);

        lowpass2.setCutoffFrequency(freqValue2); // Use middle frequency for first split
        highpass2.setCutoffFrequency(freqValue2);
        lowpass2.process(lowMidContext);
        highpass2.process(highMidContext);

        // Apply the f3 all-pass response to the complete low half before its
        // f1 split. Both halves of a Linkwitz-Riley split are required.
        allPassScratch.makeCopyOf(lowMidBuffer, true);
        auto lowHalfCompLpBlock = juce::dsp::AudioBlock<float>(lowMidBuffer);
        auto lowHalfCompLpContext = juce::dsp::ProcessContextReplacing<float>(lowHalfCompLpBlock);
        compensatorLP.process(lowHalfCompLpContext);
        auto lowHalfCompHpBlock = juce::dsp::AudioBlock<float>(allPassScratch);
        auto lowHalfCompHpContext = juce::dsp::ProcessContextReplacing<float>(lowHalfCompHpBlock);
        compensatorHP.process(lowHalfCompHpContext);
        for (int channel = 0; channel < lowMidBuffer.getNumChannels(); ++channel)
            lowMidBuffer.addFrom(channel, 0, allPassScratch, channel, 0, numSamples);

        mBuffer1.makeCopyOf(lowMidBuffer, true);
        mBuffer2.makeCopyOf(lowMidBuffer, true);
        auto block1 = juce::dsp::AudioBlock<float>(mBuffer1);
        auto context1 = juce::dsp::ProcessContextReplacing<float>(block1);
        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        lowpass1.process(context1);
        highpass1.process(context2);

        // Mirror the correction on the high half with the f1 all-pass before
        // splitting it at f3.
        allPassScratch.makeCopyOf(highMidBuffer, true);
        auto highHalfCompLpBlock = juce::dsp::AudioBlock<float>(highMidBuffer);
        auto highHalfCompLpContext = juce::dsp::ProcessContextReplacing<float>(highHalfCompLpBlock);
        secondCompensatorLP.process(highHalfCompLpContext);
        auto highHalfCompHpBlock = juce::dsp::AudioBlock<float>(allPassScratch);
        auto highHalfCompHpContext = juce::dsp::ProcessContextReplacing<float>(highHalfCompHpBlock);
        secondCompensatorHP.process(highHalfCompHpContext);
        for (int channel = 0; channel < highMidBuffer.getNumChannels(); ++channel)
            highMidBuffer.addFrom(channel, 0, allPassScratch, channel, 0, numSamples);

        mBuffer3.makeCopyOf(highMidBuffer, true);
        mBuffer4.makeCopyOf(highMidBuffer, true);
        auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
        auto block4 = juce::dsp::AudioBlock<float>(mBuffer4);
        auto context4 = juce::dsp::ProcessContextReplacing<float>(block4);
        lowpass3.process(context3);
        highpass3.process(context4);
    }
}

void FireAudioProcessor::updateGlobalFilters(double sampleRate)
{
    // Get the final, modulated settings for the entire filter chain.
    auto chainSettings = getCachedChainSettings(&lfoOutputBuffer);

    // It's good practice to ensure frequencies are within a valid range.
    if (! std::isfinite(sampleRate) || sampleRate <= 0.0)
        return;

    const auto frequencyRange = getSafeFilterFrequencyRange(sampleRate);

    // Clamp frequencies to be safe.
    chainSettings.lowCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(frequencyRange.minimum, frequencyRange.maximum, chainSettings.highCutFreq);

    if (globalFilterCacheValid
        && sameCachedValue(cachedGlobalFilterSampleRate, sampleRate)
        && sameFilterSettings(cachedGlobalFilterSettings, chainSettings))
        return;

    // Call the modular update functions with the final settings.
    updateLowCutFilters(chainSettings, sampleRate);
    updatePeakFilter(chainSettings, sampleRate);
    updateHighCutFilters(chainSettings, sampleRate);

    cachedGlobalFilterSettings = chainSettings;
    cachedGlobalFilterSampleRate = sampleRate;
    globalFilterCacheValid = true;
}

float FireAudioProcessor::getTotalLatency() const
{
    return totalLatency.load(std::memory_order_relaxed);
}

void FireAudioProcessor::processMultiBand(juce::AudioBuffer<float>& wetBuffer, const juce::AudioBuffer<float>& lfoOutputs, double sampleRate)
{
    splitBands(wetBuffer, sampleRate);

    delayMatchedDryBuffer.clear();

    std::array<juce::AudioBuffer<float>*, 4> dryBandBuffers = { &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4 };
    std::array<juce::AudioBuffer<float>*, 4> wetBandBuffers = { &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4 };
    const bool useHQ = loadCachedParameter(hqParameter) > 0.5f;

    // Solo must affect both sides of the global dry/wet mix. Otherwise the
    // un-soloed bands leak back through the dry side whenever Mix is below 1.
    sumBands(delayMatchedDryBuffer, dryBandBuffers, false);

    for (int i = 0; i < numBands; ++i)
    {
        if (auto* band = bands[i].get())
        {
            const auto& parameters = bandParameterCache[static_cast<size_t>(i)];
            calculateAndStoreLevels(*dryBandBuffers[i], band->mInputLeftRMS, band->mInputRightRMS, band->mInputLeftPeak, band->mInputRightPeak);

            if (loadCachedParameter(parameters.enabled) > 0.5f)
            {
                BandProcessingParameters params;

                // 1. Fill General Settings
                params.mode = juce::roundToInt(loadCachedParameter(parameters.mode));
                params.isHQ = useHQ;
                params.isDriveEnabled = loadCachedParameter(parameters.driveEnabled) > 0.5f;
                params.isShapeEnabled = loadCachedParameter(parameters.shapeEnabled) > 0.5f;
                params.isCompEnabled = loadCachedParameter(parameters.compressorEnabled) > 0.5f;
                params.isWidthEnabled = loadCachedParameter(parameters.widthEnabled) > 0.5f;
                params.isSafeModeOn = loadCachedParameter(parameters.safe) > 0.5f;
                params.isExtremeModeOn = loadCachedParameter(parameters.extreme) > 0.5f;
                params.isDcFilterEnabled = params.isShapeEnabled
                                           && loadCachedParameter(parameters.dcFilterEnabled) > 0.5f;

                // 2. Setup ModulatedValueProviders AND their LFO source indices
                auto setupProvider = [&](ModulatedValueProvider& provider,
                                         int& lfoIndex,
                                         const CachedParameter& parameter)
                {
                    if (parameter.ranged == nullptr)
                        return;

                    provider.baseValue = loadCachedParameter(parameter);
                    provider.range = parameter.ranged->getNormalisableRange();

                    LfoManager::AudioThreadRoutingInfo routingInfo;
                    if (lfoManager->getAudioThreadRoutingInfo(parameter.ranged, routingInfo))
                    {
                        provider.modulationDepth = routingInfo.depth;
                        provider.isBipolar = routingInfo.isBipolar;
                        lfoIndex = routingInfo.sourceLfoIndex;
                    }
                };

                setupProvider(params.driveVal, params.driveLfoSourceIndex, parameters.drive);
                setupProvider(params.biasVal, params.biasLfoSourceIndex, parameters.bias);
                setupProvider(params.recVal, params.recLfoSourceIndex, parameters.rec);
                setupProvider(params.outputVal, params.outputLfoSourceIndex, parameters.output);

                // Linked output compensation is a DSP rule, not an editor side
                // effect. It follows the unmodulated Drive base, matching the
                // original UI formula while keeping Output's own range/LFO.
                if (loadCachedParameter(parameters.linked) > 0.5f)
                    params.outputVal.baseValue = -0.1f * loadCachedParameter(parameters.drive);

                // 3. Get final values for Block-wise parameters
                params.compRatio = getBlockModulatedValue(parameters.compressorRatio, lfoOutputs);
                params.compThreshold = getBlockModulatedValue(parameters.compressorThreshold, lfoOutputs);
                params.compAttack = getBlockModulatedValue(parameters.compressorAttack, lfoOutputs);
                params.compRelease = getBlockModulatedValue(parameters.compressorRelease, lfoOutputs);
                params.compMixVal = getBlockModulatedValue(parameters.compressorMix, lfoOutputs);
                params.width = getBlockModulatedValue(parameters.width, lfoOutputs);
                params.pan = getBlockModulatedValue(parameters.pan, lfoOutputs);
                params.widthMixVal = getBlockModulatedValue(parameters.widthMix, lfoOutputs);
                params.mixVal = getBlockModulatedValue(parameters.mix, lfoOutputs);
                params.shapeMixVal = getBlockModulatedValue(parameters.shapeMix, lfoOutputs);

                realtimeModulatedThresholds[i].store(params.compThreshold);

                // 4. Call BandProcessor
                band->process(*wetBandBuffers[i], params, lfoOutputs);
            }
            else
            {
                band->processBypassed(*wetBandBuffers[i], useHQ);
            }
            calculateAndStoreLevels(*wetBandBuffers[i], band->mOutputLeftRMS, band->mOutputRightRMS, band->mOutputLeftPeak, band->mOutputRightPeak);
        }
    }

    sumBands(wetBuffer, wetBandBuffers, false);
}

void FireAudioProcessor::applyGlobalEffects(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& lfoOutputs, double sampleRate)
{
    // ==============================================================================
    // 1. Global Filter Processing (Block-based)
    // ==============================================================================
    if (loadCachedParameter(filterEnabledParameter) > 0.5f)
    {
        updateGlobalFilters(sampleRate);
        auto block = juce::dsp::AudioBlock<float>(buffer);

        auto leftBlock = block.getSingleChannelBlock(0);
        leftChain.process(juce::dsp::ProcessContextReplacing<float>(leftBlock));

        if (buffer.getNumChannels() > 1)
        {
            auto rightBlock = block.getSingleChannelBlock(1);
            rightChain.process(juce::dsp::ProcessContextReplacing<float>(rightBlock));
        }
    }

    // ==============================================================================
    // 2. Global Gain Processing
    // ==============================================================================

    // a. Prepare the "recipe" for the global output gain.
    ModulatedValueProvider globalGainProvider;
    if (globalOutputParameter.ranged == nullptr)
        return;

    globalGainProvider.baseValue = loadCachedParameter(globalOutputParameter);
    globalGainProvider.range = globalOutputParameter.ranged->getNormalisableRange();

    LfoManager::AudioThreadRoutingInfo routingInfo;
    if (lfoManager->getAudioThreadRoutingInfo(globalOutputParameter.ranged, routingInfo))
    {
        const int sourceIndex = routingInfo.sourceLfoIndex;
        if (juce::isPositiveAndBelow(sourceIndex, lfoOutputs.getNumChannels()))
        {
            globalGainProvider.lfoSignal = lfoOutputs.getReadPointer(sourceIndex);
            globalGainProvider.modulationDepth = routingInfo.depth;
            globalGainProvider.isBipolar = routingInfo.isBipolar;
        }
    }

    // b. Apply the gain using our new, clean helper function.
    applyGain(buffer, globalGainProvider, gainProcessorGlobal);
}

void FireAudioProcessor::applyDownsamplingEffect(juce::AudioBuffer<float>& buffer)
{
    const bool isActive = loadCachedParameter(downsampleEnabledParameter) > 0.5f;
    if (! isActive)
    {
        resetDownsamplingState();
        return;
    }

    if (! downsamplingWasActive)
    {
        downsampleSamplesRemaining.fill(0);
        downsampleHeldSamples.fill(0.0f);
        downsamplingWasActive = true;
    }

    // --- 1. Prepare Dry Signal & Mixer ---
    // A copy of the original signal is needed for the dry/wet mix.
    lofiDryBuffer.makeCopyOf(buffer, true);

    // Set up the mixer with the correct wet proportion from its parameter.
    lofiMixer.setWetMixProportion(getBlockModulatedValue(downsampleMixParameter, lfoOutputBuffer));
    if (! lofiMixerPrimed)
        lofiMixer.reset();
    lofiMixerPrimed = true;
    lofiMixer.pushDrySamples(juce::dsp::AudioBlock<float>(lofiDryBuffer));

    // --- 2. Get All Parameter Values Once Per Block ---
    const int bits = juce::jlimit(4, 32,
                                 juce::roundToInt(getBlockModulatedValue(bitDepthParameter,
                                                                        lfoOutputBuffer)));
    const float jitter = getBlockModulatedValue(jitterParameter, lfoOutputBuffer);
    const float rateReduceValue = getBlockModulatedValue(downsampleRateParameter, lfoOutputBuffer);

    // --- 3. Process Audio ---
    const int channelsToProcess = juce::jmin(buffer.getNumChannels(),
                                              static_cast<int>(downsamplingStateChannels));
    const float quantisationStep = bits < 32
                                       ? 2.0f / std::ldexp(1.0f, bits)
                                       : 0.0f;

    // Iterate samples first so jitter consumes the same random sequence
    // regardless of how the host partitions the stream into blocks.
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            const auto stateIndex = static_cast<size_t>(channel);

            // --- Rate Reduction (Sample & Hold) ---
            if (downsampleSamplesRemaining[stateIndex] <= 0)
            {
                // It's time to grab a new sample.
                downsampleHeldSamples[stateIndex] = channelData[sample];

                // Determine the hold duration for this new sample.
                float currentRateReduce = rateReduceValue;

                // Apply Jitter if the parameter is active.
                if (jitter > 0.0f)
                {
                    // Introduce a random variation to the hold time.
                    // random.nextFloat() returns [0, 1]. We map it to [-1, 1].
                    float randomFactor = 1.0f + (random.nextFloat() * 2.0f - 1.0f) * jitter;
                    currentRateReduce *= randomFactor;
                }

                // Set how many samples we need to hold for. Must be at least 1.
                downsampleSamplesRemaining[stateIndex] = juce::jmax(1, static_cast<int>(currentRateReduce));
            }

            // Output the held sample.
            channelData[sample] = downsampleHeldSamples[stateIndex];
            --downsampleSamplesRemaining[stateIndex];

            // --- Bit Crushing ---
            // Apply this effect after the sample has been selected (or held).
            if (bits < 32)
                channelData[sample] = quantisationStep
                                      * std::floor(channelData[sample] / quantisationStep + 0.5f);
        }
    }

    // --- 4. Mix with Dry Signal ---
    // Finally, mix the processed (wet) buffer with the original (dry) buffer.
    lofiMixer.mixWetSamples(juce::dsp::AudioBlock<float>(buffer));
}

void FireAudioProcessor::resetDownsamplingState() noexcept
{
    downsampleSamplesRemaining.fill(0);
    downsampleHeldSamples.fill(0.0f);
    downsamplingWasActive = false;
    lofiMixerPrimed = false;
}

void FireAudioProcessor::primeLatencyMatchedBypass(juce::AudioBuffer<float>& inputBuffer)
{
    if (inputBuffer.getNumChannels() == 0 || inputBuffer.getNumSamples() == 0)
        return;

    mWetBuffer.makeCopyOf(inputBuffer, true);
    bypassDelayMixer.setWetLatency(totalLatency.load(std::memory_order_acquire));
    bypassDelayMixer.setWetMixProportion(0.0f);
    bypassDelayMixer.pushDrySamples(juce::dsp::AudioBlock<float>(inputBuffer));
    bypassDelayMixer.mixWetSamples(juce::dsp::AudioBlock<float>(mWetBuffer));
}

void FireAudioProcessor::processLatencyMatchedBypass(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    bypassDelayMixer.setWetLatency(totalLatency.load(std::memory_order_acquire));
    bypassDelayMixer.setWetMixProportion(0.0f);
    bypassDelayMixer.pushDrySamples(juce::dsp::AudioBlock<float>(buffer));
    bypassDelayMixer.mixWetSamples(juce::dsp::AudioBlock<float>(buffer));
}

void FireAudioProcessor::applyGlobalMix(juce::AudioBuffer<float>& buffer)
{
    if (loadCachedParameter(hqParameter) > 0.5f)
    {
        dryWetMixerGlobal.setWetLatency(totalLatency.load(std::memory_order_relaxed));
    }
    else
    {
        dryWetMixerGlobal.setWetLatency(0);
    }

    // Get the final modulated mix value from the LfoManager
    dryWetMixerGlobal.setWetMixProportion(getBlockModulatedValue(globalMixParameter, lfoOutputBuffer));
    if (! globalMixerPrimed)
        dryWetMixerGlobal.reset();
    globalMixerPrimed = true;
    dryWetMixerGlobal.pushDrySamples(juce::dsp::AudioBlock<float>(delayMatchedDryBuffer));
    dryWetMixerGlobal.mixWetSamples(juce::dsp::AudioBlock<float>(buffer));
}

FireAudioProcessor::ModulationInfo FireAudioProcessor::getModulationInfoForParameter(const juce::String& parameterID) const
{
    const juce::ScopedLock sl(lfoManager->getLfoDataLock());
    // Find the routing in the manager's list
    for (const auto& routing : lfoManager->getModulationRoutings())
    {
        if (routing.targetParameterID == parameterID)
        {
            if (juce::isPositiveAndBelow(routing.sourceLfoIndex, 4))
            {
                const float unipolarLfoValue = lfoManager->getLfoOutput(routing.sourceLfoIndex);
                float finalLfoValue = routing.isBipolar ? (unipolarLfoValue * 2.0f - 1.0f) : unipolarLfoValue;
                return { true, routing.sourceLfoIndex + 1, routing.depth, finalLfoValue, routing.isBipolar, routing.isBypassed };
            }
        }
    }
    return { false, 0, 0.0f, 0.0f, true, false }; // Default "not modulated" state
}

void FireAudioProcessor::setModulationValue(const juce::String& targetParameterID, float newValue)
{
    auto* parameter = treeState.getParameter(targetParameterID);
    auto* rawParameter = treeState.getRawParameterValue(targetParameterID);
    if (parameter == nullptr || rawParameter == nullptr || ! std::isfinite(newValue))
        return;

    const auto range = parameter->getNormalisableRange();
    const float valueNormalized = range.convertTo0to1(newValue);
    const float baseNormalized = range.convertTo0to1(rawParameter->load(std::memory_order_relaxed));
    bool didUpdate = false;

    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID != targetParameterID)
                continue;

            // The new depth is the difference between the target value's normalized position
            // and the base value's normalized position.
            float newDepth = valueNormalized - baseNormalized;

            // In Bipolar mode, the DSP logic effectively halves the depth's impact
            // to create a symmetrical swing. To make our `newValue` the actual extreme
            // of that swing, we must pre-emptively double the calculated depth.
            if (routing.isBipolar)
            {
                newDepth *= 2.0f;
            }

            // Clamp the final depth to the valid range [-1.0, 1.0] and update the routing.
            routing.depth = juce::jlimit(-1.0f, 1.0f, newDepth);
            didUpdate = true;
            break;
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::setModulationDepth(const juce::String& targetParameterID, float newDepth)
{
    if (! std::isfinite(newDepth))
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID == targetParameterID)
            {
                routing.depth = juce::jlimit(-1.0f, 1.0f, newDepth);
                didUpdate = true;
                break;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::toggleBipolarMode(const juce::String& targetParameterID)
{
    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID == targetParameterID)
            {
                routing.isBipolar = ! routing.isBipolar;
                didUpdate = true;
                break;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::resetModulation(const juce::String& targetParameterID)
{
    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        for (auto& routing : lfoManager->getModulationRoutings())
        {
            if (routing.targetParameterID == targetParameterID)
            {
                routing.depth = 0.5f;
                routing.isBipolar = true;
                didUpdate = true;
                break;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

void FireAudioProcessor::assignModulation(int routingIndex, int sourceLfoIndex, const juce::String& targetParameterID)
{
    if (targetParameterID.isNotEmpty()
        && (! juce::isPositiveAndBelow(sourceLfoIndex, 4) || treeState.getParameter(targetParameterID) == nullptr))
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        auto& routings = lfoManager->getModulationRoutings();
        if (! juce::isPositiveAndBelow(routingIndex, routings.size()))
            return;

        if (targetParameterID.isNotEmpty())
        {
            for (int i = 0; i < routings.size(); ++i)
            {
                if (i != routingIndex && routings.getReference(i).targetParameterID == targetParameterID)
                    routings.getReference(i).targetParameterID.clear();
            }
        }

        auto& currentRouting = routings.getReference(routingIndex);
        currentRouting.sourceLfoIndex = juce::jlimit(0, 3, sourceLfoIndex);
        currentRouting.targetParameterID = targetParameterID;
        didUpdate = true;
    }

    if (didUpdate)
        lfoDataHasChanged();
}

float FireAudioProcessor::getRealtimeModulatedThreshold(int bandIndex) const
{
    if (juce::isPositiveAndBelow(bandIndex, 4))
        return realtimeModulatedThresholds[bandIndex].load();

    return -48.0f;
}

const juce::StringArray& FireAudioProcessor::getLfoRateSyncDivisions() const
{
    return lfoManager->getLfoRateSyncDivisions();
}

void FireAudioProcessor::lfoDataHasChanged()
{
    updateHostDisplay(
        juce::AudioProcessorListener::ChangeDetails {}.withNonParameterStateChanged(true));

    if (auto* editor = dynamic_cast<FireAudioProcessorEditor*>(getActiveEditor()))
    {
        editor->markPresetAsDirty();
    }
}

bool FireAudioProcessor::getLatestMeterValues(MeterValues& values)
{
    // Check how many complete data packets are ready to be read.
    int numAvailable = meterFifo.getNumReady();

    // If there's at least one, we can proceed.
    if (numAvailable > 0)
    {
        // We want to get the most recent value. If there are multiple values
        // queued up, we'll read them all but only copy the last one.
        // This prevents the GUI from lagging behind the audio thread.

        int start1, size1, start2, size2;
        meterFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

        // The fifo might wrap around, so it gives us two contiguous blocks.
        // We just need to find the last element across these two blocks.
        if (size2 > 0)
        {
            // The latest element is at the end of the second block.
            values = meterFifoBuffer[start2 + size2 - 1];
        }
        else
        {
            // The latest element is at the end of the first block.
            values = meterFifoBuffer[start1 + size1 - 1];
        }

        // Tell the fifo that we've now "consumed" all the available data.
        meterFifo.finishedRead(numAvailable);

        // Return true to indicate that we successfully updated the 'values' object.
        return true;
    }

    // If no new data was available, return false.
    return false;
}

bool FireAudioProcessor::getLatestModulatedFilterValues(ModulatedFilterValues& values)
{
    int numAvailable = filterFifo.getNumReady();

    if (numAvailable > 0)
    {
        int start1, size1, start2, size2;
        filterFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

        if (size2 > 0)
        {
            values = filterFifoBuffer[start2 + size2 - 1];
        }
        else
        {
            values = filterFifoBuffer[start1 + size1 - 1];
        }
        filterFifo.finishedRead(numAvailable);
        return true;
    }
    return false;
}

float FireAudioProcessor::getGlobalInputRMSLevel(int channel) const
{
    return channel == 0 ? mInputLeftRMSGlobal.load() : mInputRightRMSGlobal.load();
}

float FireAudioProcessor::getGlobalOutputRMSLevel(int channel) const
{
    return channel == 0 ? mOutputLeftRMSGlobal.load() : mOutputRightRMSGlobal.load();
}

float FireAudioProcessor::getGlobalInputPeakLevel(int channel) const
{
    return channel == 0 ? mInputLeftPeakGlobal.load() : mInputRightPeakGlobal.load();
}

float FireAudioProcessor::getGlobalOutputPeakLevel(int channel) const
{
    return channel == 0 ? mOutputLeftPeakGlobal.load() : mOutputRightPeakGlobal.load();
}

float FireAudioProcessor::getBandInputRMSLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mInputLeftRMS.load() : bandProcessor->mInputRightRMS.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

float FireAudioProcessor::getBandOutputRMSLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mOutputLeftRMS.load() : bandProcessor->mOutputRightRMS.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

float FireAudioProcessor::getBandInputPeakLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mInputLeftPeak.load() : bandProcessor->mInputRightPeak.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

float FireAudioProcessor::getBandOutputPeakLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mOutputLeftPeak.load() : bandProcessor->mOutputRightPeak.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

void FireAudioProcessor::assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID)
{
    lfoManager->assignLfoToTarget(sourceLfoIndex, targetParameterID);

    // When the modulation assignment changes, notify the active editor to update its UI
    if (auto* editor = getActiveEditor())
    {
        // We must cast the generic editor pointer to our specific FireAudioProcessorEditor
        // type to access the triggerAsyncUpdate() method from its AsyncUpdater base class.
        // A dynamic_cast is used for type safety.
        if (auto* fireEditor = dynamic_cast<FireAudioProcessorEditor*>(editor))
        {
            fireEditor->triggerAsyncUpdate();
        }
    }
    lfoDataHasChanged();
}

void FireAudioProcessor::clearModulationForParameter(const juce::String& targetParameterID)
{
    lfoManager->clearModulationForTarget(targetParameterID);

    if (auto* editor = getActiveEditor())
    {
        if (auto* fireEditor = dynamic_cast<FireAudioProcessorEditor*>(editor))
        {
            fireEditor->triggerAsyncUpdate();
        }
    }
    lfoDataHasChanged();
}

void FireAudioProcessor::invertModulationDepthForParameter(const juce::String& targetParameterID)
{
    lfoManager->invertModulationDepth(targetParameterID);
    lfoDataHasChanged();
}

bool FireAudioProcessor::isCurrentStateEquivalentToPreset(const juce::XmlElement& presetXml)
{
    constexpr float comparisonTolerance = 1.0e-6f;

    // Recreate the exact model produced by loadStateFromXml rather than
    // comparing XML text. Older presets omit attributes whose loader defaults
    // are well-defined, and decimal formatting is not part of the preset's
    // audible state.
    std::array<LfoData, 4> expectedLfoData;
    std::array<bool, 4> expectedSmoothnessFromLfoXml {};
    std::array<bool, 4> expectedSmoothnessFromParameter {};
    std::array<bool, 4> loadedLfoIndices {};
    for (int i = 0; i < static_cast<int>(expectedSmoothnessFromParameter.size()); ++i)
        expectedSmoothnessFromParameter[static_cast<size_t>(i)] = presetXml.hasAttribute(
            ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i));

    // getChildByName intentionally selects the first section, matching the
    // loader when a malformed/legacy document contains duplicate sections.
    if (const auto* lfoState = presetXml.getChildByName("LFO_STATE"))
    {
        for (auto* lfoXml : lfoState->getChildIterator())
        {
            if (! lfoXml->hasTagName("LFO"))
                continue;

            const int index = lfoXml->getIntAttribute("index", -1);
            if (juce::isPositiveAndBelow(index, static_cast<int>(expectedLfoData.size()))
                && ! loadedLfoIndices[static_cast<size_t>(index)])
            {
                expectedLfoData[static_cast<size_t>(index)] = LfoData::readFromXml(*lfoXml);
                expectedSmoothnessFromLfoXml[static_cast<size_t>(index)] =
                    lfoXml->hasAttribute("smoothness");
                loadedLfoIndices[static_cast<size_t>(index)] = true;
            }
        }
    }

    for (const auto& parameter : getParameters())
    {
        auto* parameterWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        if (parameterWithID == nullptr)
            continue;

        float presetValue = parameterWithID->getDefaultValue();
        if (presetXml.hasAttribute(parameterWithID->paramID))
        {
            double parsedValue = 0.0;
            if (parseStrictFiniteDouble(presetXml.getStringAttribute(parameterWithID->paramID),
                                        parsedValue))
                presetValue = juce::jlimit(0.0f, 1.0f, static_cast<float>(parsedValue));
        }
        else if (parameterWithID->paramID.startsWith(SHAPE_BYPASS_ID))
        {
            presetValue = 1.0f;
        }
        else
        {
            // Before Smoothness became an APVTS parameter it lived only on the
            // LFO element. loadStateFromXml promotes that legacy value back to
            // the parameter through setLfoData(), so compare against the same
            // effective normalised parameter value.
            for (int i = 0; i < static_cast<int>(expectedLfoData.size()); ++i)
                if (parameterWithID->paramID
                        == ParameterIDAndName::getIDString(LFO_SMOOTH_ID, i)
                    && expectedSmoothnessFromLfoXml[static_cast<size_t>(i)])
                {
                    if (const auto* ranged = treeState.getParameter(parameterWithID->paramID))
                        presetValue = ranged->convertTo0to1(
                            expectedLfoData[static_cast<size_t>(i)].smoothness);
                    break;
                }
        }

        if (std::abs(parameterWithID->getValue() - presetValue) > comparisonTolerance)
            return false;
    }

    for (size_t i = 0; i < expectedLfoData.size(); ++i)
    {
        if (! expectedSmoothnessFromParameter[i] && expectedSmoothnessFromLfoXml[i])
            continue;

        if (const auto* smoothness = treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(LFO_SMOOTH_ID, static_cast<int>(i))))
            expectedLfoData[i].smoothness = juce::jlimit(
                0.0f, 1.0f, smoothness->load(std::memory_order_relaxed));
    }

    const auto currentLfoData = lfoManager->getLfoDataCopy();
    if (currentLfoData.size() != expectedLfoData.size())
        return false;

    for (size_t i = 0; i < expectedLfoData.size(); ++i)
        if (! areLfoShapesEquivalent(currentLfoData[i], expectedLfoData[i]))
            return false;

    constexpr int maximumPresetRoutings = 128;
    juce::Array<ModulationRouting> expectedRoutings;
    if (const auto* routingState = presetXml.getChildByName("MODULATION_STATE"))
    {
        for (auto* routingXml : routingState->getChildIterator())
        {
            if (! routingXml->hasTagName("ROUTING")
                || expectedRoutings.size() >= maximumPresetRoutings)
                continue;

            auto routing = ModulationRouting::readFromXml(*routingXml);
            bool targetAlreadyUsed = false;
            for (const auto& existing : expectedRoutings)
                if (existing.targetParameterID == routing.targetParameterID)
                {
                    targetAlreadyUsed = true;
                    break;
                }

            if (! targetAlreadyUsed
                && routing.targetParameterID.isNotEmpty()
                && treeState.getParameter(routing.targetParameterID) != nullptr)
                expectedRoutings.add(std::move(routing));
        }
    }

    // Empty routing slots are an implementation detail of the live manager and
    // are neither written nor restored by presets. Compare unique targets as a
    // set because their array order has no DSP meaning.
    juce::Array<ModulationRouting> currentRoutings;
    juce::StringArray currentRoutingTargets;
    for (const auto& routing : lfoManager->getModulationRoutingsCopy())
    {
        if (routing.targetParameterID.isEmpty())
            continue;
        if (currentRoutingTargets.contains(routing.targetParameterID))
            return false;

        currentRoutingTargets.add(routing.targetParameterID);
        currentRoutings.add(routing);
    }

    if (currentRoutings.size() != expectedRoutings.size())
        return false;

    for (const auto& expected : expectedRoutings)
    {
        bool foundEquivalentRouting = false;
        for (const auto& candidate : currentRoutings)
            if (candidate.targetParameterID == expected.targetParameterID)
            {
                foundEquivalentRouting = areModulationRoutingsEquivalent(candidate, expected);
                break;
            }

        if (! foundEquivalentRouting)
            return false;
    }

    return true;
}

/**
 * @brief Shifts the band index for any LFO modulation routing target within a specified range.
 *
 * This function iterates through all modulation routings. If a routing's target parameter
 * has a band index between startIndex and endIndex (inclusive), it adjusts that index
 * by shiftAmount. This is crucial for keeping modulation assignments correct when bands
 * are added or removed.
 *
 * @param startIndex The starting band index of the range to affect.
 * @param endIndex The ending band index of the range to affect.
 * @param shiftAmount The amount to add to the band index (can be positive or negative).
 */
void FireAudioProcessor::shiftLfoModulationTargets(int startIndex, int endIndex, int shiftAmount)
{
    if (! juce::isPositiveAndBelow(startIndex, 4)
        || ! juce::isPositiveAndBelow(endIndex, 4)
        || startIndex > endIndex
        || shiftAmount == 0)
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        auto& routings = lfoManager->getModulationRoutings();
        juce::StringArray remappedTargets;
        remappedTargets.ensureStorageAllocated(routings.size());

        // Calculate the complete move from an immutable snapshot first. A band
        // shift has memmove semantics: every destination slot is replaced even
        // when its source band has no routing. This is especially important on
        // deletion, where the routing on the deleted band must not survive and
        // collide with the routing shifted in from its right-hand neighbour.
        for (const auto& routing : routings)
        {
            auto newTarget = routing.targetParameterID;
            const auto address = findBandParameterAddress(routing.targetParameterID);

            if (address.parameter != nullptr)
            {
                if (address.bandIndex >= startIndex && address.bandIndex <= endIndex)
                {
                    const int newBandIndex = address.bandIndex + shiftAmount;
                    newTarget = juce::isPositiveAndBelow(newBandIndex, 4)
                                    ? ParameterIDAndName::getIDString(address.parameter->idBase, newBandIndex)
                                    : juce::String();
                }
                else
                {
                    // If this band is a destination of the requested move, its
                    // old routing is overwritten even if the corresponding
                    // source has no routing of its own.
                    const int sourceBandIndex = address.bandIndex - shiftAmount;
                    if (sourceBandIndex >= startIndex && sourceBandIndex <= endIndex)
                        newTarget.clear();
                }
            }

            remappedTargets.add(newTarget);
        }

        // A band target is intentionally unique throughout the UI/API. Corrupt
        // or legacy states can contain duplicate source routings; keep the
        // first one produced by this move and reset the remaining slots. Leave
        // unrelated (including global) routings exactly as they were.
        for (int routingIndex = 0; routingIndex < remappedTargets.size(); ++routingIndex)
        {
            auto& target = remappedTargets.getReference(routingIndex);
            if (target.isEmpty()
                || target == routings.getReference(routingIndex).targetParameterID)
                continue;

            for (int earlierIndex = 0; earlierIndex < routingIndex; ++earlierIndex)
            {
                if (remappedTargets[earlierIndex] == target)
                {
                    target.clear();
                    break;
                }
            }
        }

        for (int routingIndex = 0; routingIndex < routings.size(); ++routingIndex)
        {
            auto& routing = routings.getReference(routingIndex);
            const auto& newTarget = remappedTargets[routingIndex];

            if (newTarget == routing.targetParameterID)
                continue;

            if (newTarget.isEmpty())
                resetModulationRouting(routing);
            else
                routing.targetParameterID = newTarget;

            didUpdate = true;
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

/**
 * @brief Clears (un-assigns) any LFO modulation that targets a specific band.
 *
 * This is used to clean up modulation routings when a band is being reset to its
 * default state, for example, when a new band is created.
 *
 * @param bandIndex The 0-based index of the band whose modulation targets should be cleared.
 */
void FireAudioProcessor::clearLfoModulationForBand(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, 4))
        return;

    bool didUpdate = false;
    {
        const juce::ScopedLock lock(lfoManager->getLfoDataLock());
        auto& routings = lfoManager->getModulationRoutings();

        for (auto& routing : routings)
        {
            const auto address = findBandParameterAddress(routing.targetParameterID);
            if (address.parameter != nullptr && address.bandIndex == bandIndex)
            {
                resetModulationRouting(routing);
                didUpdate = true;
            }
        }
    }

    if (didUpdate)
        lfoDataHasChanged();
}

bool FireAudioProcessor::getLatestDistortionGraphValues(DistortionGraphValues& values)
{
    int numAvailable = graphFifo.getNumReady();
    if (numAvailable > 0)
    {
        int start1, size1, start2, size2;
        graphFifo.prepareToRead(numAvailable, start1, size1, start2, size2);

        if (size2 > 0)
            values = graphFifoBuffer[start2 + size2 - 1];
        else
            values = graphFifoBuffer[start1 + size1 - 1];

        graphFifo.finishedRead(numAvailable);
        return true;
    }
    return false;
}

void FireAudioProcessor::setUiFocusBand(int bandIndex)
{
    if (juce::isPositiveAndBelow(bandIndex, 4))
    {
        uiFocusBand.store(bandIndex);
    }
}

void FireAudioProcessor::calculateAndStoreLevels(const juce::AudioBuffer<float>& buffer,
                                                 std::atomic<float>& rmsLeft,
                                                 std::atomic<float>& rmsRight,
                                                 std::atomic<float>& peakLeft,
                                                 std::atomic<float>& peakRight)
{
    // This function calculates RMS and Peak levels for a given buffer and stores them
    // in the provided atomic float variables for thread-safe access from the UI.

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // If there's no audio, reset levels to zero to prevent stale values.
    if (numChannels <= 0 || numSamples <= 0)
    {
        rmsLeft.store(0.0f);
        rmsRight.store(0.0f);
        peakLeft.store(0.0f);
        peakRight.store(0.0f);
        return;
    }

    // Use JUCE's built-in functions for efficient calculation.
    // getRMSLevel() returns linear RMS amplitude.
    // getMagnitude() with arguments (0, numSamples) finds the peak absolute value.

    // Calculate for Left Channel (or Mono)
    rmsLeft.store(buffer.getRMSLevel(0, 0, numSamples));
    peakLeft.store(buffer.getMagnitude(0, 0, numSamples));

    // Calculate for Right Channel if it exists, otherwise mirror the left channel.
    if (numChannels > 1)
    {
        rmsRight.store(buffer.getRMSLevel(1, 0, numSamples));
        peakRight.store(buffer.getMagnitude(1, 0, numSamples));
    }
    else
    {
        rmsRight.store(rmsLeft.load());
        peakRight.store(peakLeft.load());
    }
}
