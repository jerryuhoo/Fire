/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include "PluginProcessor.h"
#include "DSP/DistortionLogic.h"
#include "PluginEditor.h"

//==============================================================================
// BandProcessor Implementation
//==============================================================================

// This is where we tell JUCE what to do when prepareToPlay is called for a single band.
void BandProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    // Prepare all the DSP modules with the sample rate and block size.
    compressor.prepare(spec);
    gain.prepare(spec);
    juce::dsp::ProcessSpec mixerSpec = spec;
    mixerSpec.maximumBlockSize = spec.maximumBlockSize * 4 + 64;
    dryWetMixer.prepare(mixerSpec);

    // Some processors need extra setup.
    compressor.setAttack(80.0f);
    compressor.setRelease(200.0f);

    // The DC filter needs its coefficients to be calculated.
    // dcFilter.prepare(spec);
    // *dcFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);

    // The oversampling object also needs to be prepared.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    oversampling->initProcessing(spec.maximumBlockSize);

    // Reset all smoothed values with the current sample rate and a ramp time.
    driveSmoother.reset(spec.sampleRate, 0.05);
    biasSmoother.reset(spec.sampleRate, 0.05);
    recSmoother.reset(spec.sampleRate, 0.05);
}

// This is what happens when we need to clear the internal state of a band's processors.
void BandProcessor::reset()
{
    isFirstBlock = true;
    compressor.reset();
    gain.reset();
    dryWetMixer.reset();
    // dcFilter.reset();

    if (oversampling)
        oversampling->reset();
}

//==============================================================================
// This is the new, self-contained processing function for a single band.
// It replaces the old `processOneBand` and `processDistortion` functions.
//==============================================================================
void BandProcessor::process(juce::AudioBuffer<float>& buffer,
                            const BandProcessingParameters& params)
{
    // Extract parameters for easier access
    const bool isHQ = params.isHQ;
    // MODULATED: outputVal is the final, LFO-modulated value
    const float outputVal = params.outputVal;
    // MODULATED: mixVal is the final, LFO-modulated value
    const float mixVal = params.mixVal;

    // Create a copy of the clean signal for the final dry/wet mix
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Core Processing (with correct Oversampling)
    auto block = juce::dsp::AudioBlock<float>(buffer);

    if (isHQ)
    {
        auto oversampledBlock = oversampling->processSamplesUp(block);

        // Call the loop to process the upsampled signal in-place
        processDistortion(oversampledBlock, dryBuffer, params);

        oversampling->processSamplesDown(block);
    }
    else
    {
        // Call the loop to process the signal at the original sample rate
        processDistortion(block, dryBuffer, params);
    }

    // auto dcFilterContext = juce::dsp::ProcessContextReplacing<float>(block);
    // dcFilter.process(dcFilterContext);

    // Final Gain and Dry/Wet Mix at block level
    auto finalContext = juce::dsp::ProcessContextReplacing<float>(block);

    // Apply the final output gain to the wet signal
    gain.setGainDecibels(outputVal);
    gain.setRampDurationSeconds(0.05f);
    gain.process(finalContext);

    if (isHQ)
    {
        dryWetMixer.setWetLatency(oversampling->getLatencyInSamples());
    }
    else
    {
        dryWetMixer.setWetLatency(0.0f);
    }

    // Apply the dry/wet mix
    if (mixVal <= 0.0f)
    {
        buffer.makeCopyOf(dryBuffer);
    }
    else if (mixVal < 1.0f)
    {
        // Only perform the mix operation if it's between 0 and 1
        dryWetMixer.setWetMixProportion(mixVal);
        dryWetMixer.pushDrySamples(juce::dsp::AudioBlock<float>(dryBuffer));
        dryWetMixer.mixWetSamples(block);
    }

    //==============================================================================
    // 2. POST-DISTORTION BLOCK-BASED EFFECTS
    //==============================================================================
    auto postDistortionBlock = juce::dsp::AudioBlock<float>(buffer);
    auto postDistortionContext = juce::dsp::ProcessContextReplacing<float>(postDistortionBlock);

    if (params.isCompEnabled)
    {
        this->compressor.setThreshold(params.compThreshold);
        this->compressor.setRatio(params.compRatio);
        this->compressor.process(postDistortionContext);
    }

    if (params.isWidthEnabled && buffer.getNumChannels() == 2)
    {
        this->widthProcessor.process(buffer.getWritePointer(0), buffer.getWritePointer(1), params.width, buffer.getNumSamples());
    }
}

void BandProcessor::processDistortion(juce::dsp::AudioBlock<float>& blockToProcess,
                                      const juce::AudioBuffer<float>& dryBuffer,
                                      const BandProcessingParameters& params)
{
    const bool isSafeModeOn = params.isSafeModeOn;
    const bool isExtremeModeOn = params.isExtremeModeOn;
    float driveVal = params.driveVal;
    const float biasVal = params.biasVal; // This is now the final value
    const float recVal = params.recVal; // This is now the final value
    const int currentMode = params.mode;
    // We calculate the max value from the 'dryBuffer' which represents the
    // clean, per-band signal right before it enters the distortion loop.
    // This ensures each band's Safe Mode reacts only to its own signal level.
    this->mSampleMaxValue = dryBuffer.getMagnitude(0, dryBuffer.getNumSamples());
    // --- Start per-sample processing loop ---
    if (isExtremeModeOn)
    {
        driveVal = log2f(10.0f) * driveVal;
    }

    // --- Safe Mode logic (calculates the final drive gain) ---
    const float driveForCalc = driveVal * 6.5f / 100.0f;
    float powerDrive = std::pow(2.0f, driveForCalc);

    if (isSafeModeOn && this->mSampleMaxValue * powerDrive > 2.0f)
        this->newDrive = 2.0f / this->mSampleMaxValue + 0.1f * driveForCalc;
    else
        this->newDrive = powerDrive;

    // Update the reduction percent for the UI to read
    if (driveForCalc == 0.0f || this->mSampleMaxValue <= 0.001f)
        this->mReductionPercent = 1.0f;
    else
        this->mReductionPercent = std::log2(this->newDrive) / driveForCalc;

    // --- Initial value handling for smoothers ---
    if (isFirstBlock)
    {
        // If it's the first block, forcibly set the current and target values
        driveSmoother.setCurrentAndTargetValue(this->newDrive);

        float finalBias = (this->mSampleMaxValue < 0.000001f) ? 0.0f : biasVal;
        biasSmoother.setCurrentAndTargetValue(finalBias);
        recSmoother.setCurrentAndTargetValue(recVal);

        isFirstBlock = false; // Disable this for subsequent blocks
    }
    else
    {
        // For all other blocks, use setTargetValue for smooth transitions
        driveSmoother.setTargetValue(this->newDrive);

        float finalBias = (this->mSampleMaxValue < 0.000001f) ? 0.0f : biasVal;
        biasSmoother.setTargetValue(finalBias);
        recSmoother.setTargetValue(recVal);
    }

    // Manual Per-Sample Processing Loop
    const int numSamples = (int) blockToProcess.getNumSamples();
    const int numChannels = (int) blockToProcess.getNumChannels();

    // A map to hold the final accumulated modulation amount for any per-sample parameter.
    // This makes the logic generic and ready for future expansion.
    std::map<juce::String, float> perSampleModulationAmounts;

    DistortionLogic::State currentState;
    currentState.mode = currentMode;
    auto waveshaperFunction = DistortionLogic::getWaveshaperForMode(currentMode);

    // --- Manual Per-Sample Processing Loop ---
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float smoothedDrive = driveSmoother.getNextValue();
        const float smoothedBias = biasSmoother.getNextValue();
        const float smoothedRec = recSmoother.getNextValue();

        // Apply the pre-calculated modulation amounts.
        // The '? 0.0f' is a safe fallback in case no modulation is active for that parameter.
        float finalBiasModAmount = perSampleModulationAmounts.count(params.biasID) ? perSampleModulationAmounts.at(params.biasID) : 0.0f;
        float finalRecModAmount = perSampleModulationAmounts.count(params.recID) ? perSampleModulationAmounts.at(params.recID) : 0.0f;

        float modulatedBias = juce::jlimit(params.biasRange.start, params.biasRange.end, smoothedBias + finalBiasModAmount);
        float modulatedRec = juce::jlimit(params.recRange.start, params.recRange.end, smoothedRec + finalRecModAmount);

        currentState.drive = smoothedDrive;
        currentState.bias = modulatedBias;
        currentState.rec = modulatedRec;
        currentState.mode = params.mode;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            // The reason we don't use processSample in DistortionLogic.h
            // is because we don't need to call getWaveshaperForMode in every for loop
            float currentSample = blockToProcess.getSample(channel, sample);
            currentSample *= currentState.drive; // 1. Drive Gain
            currentSample += currentState.bias; // 2. Pre-Bias

            currentSample = waveshaperFunction(currentSample); // 3. Waveshaper

            if (currentSample < 0.0f)
                currentSample *= (0.5f - currentState.rec) * 2.0f; // 4. Rectifier

            currentSample -= currentState.bias; // 5. Post-Bias

            blockToProcess.setSample(channel, sample, currentSample);
        }
    }
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
      ,
      VinL(500.f, 0.f) // VinL(0.f, 500.f)
      ,
      VinR(500.f, 0.f),
      R1L(80.0f),
      R1R(80.0f),
      C1L(3.5e-5, getSampleRate()),
      C1R(3.5e-5, getSampleRate()),
      RCL(&R1L, &C1L),
      RCR(&R1R, &C1R),
      rootL(&VinL, &RCL),
      rootR(&VinR, &RCR)

#endif
{
    // factor = 2 means 2^2 = 4, 4x oversampling
    for (size_t i = 0; i < 4; i++)
    {
        oversamplingHQ[i] = std::make_unique<juce::dsp::Oversampling<float>>(getTotalNumInputChannels(), 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    }

    // Initialize the band processors in a loop.
    for (int i = 0; i < 4; ++i)
        bands.push_back(std::make_unique<BandProcessor>());

    for (int i = 0; i < 4; ++i)
        realtimeModulatedThresholds[i].store(-48.0f);

    filterFifoBuffer.resize(filterFifo.getTotalSize());
    meterFifoBuffer.resize(meterFifo.getTotalSize());

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
    auto chainSettings = getChainSettings(treeState);

    const auto sampleRate = getSampleRate(); // Get the current sample rate
    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;

    if (maxFreq < minFreq)
    {
        maxFreq = minFreq;
    }

    chainSettings.lowCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(minFreq, maxFreq, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.highCutFreq);

    lowcutFreqSmoother.setCurrentAndTargetValue(chainSettings.lowCutFreq);
    lowcutGainSmoother.setCurrentAndTargetValue(chainSettings.lowCutGainInDecibels);
    lowcutQualitySmoother.setCurrentAndTargetValue(chainSettings.lowCutQuality);

    peakFreqSmoother.setCurrentAndTargetValue(chainSettings.peakFreq);
    peakGainSmoother.setCurrentAndTargetValue(chainSettings.peakGainInDecibels);
    peakQualitySmoother.setCurrentAndTargetValue(chainSettings.peakQuality);

    highcutFreqSmoother.setCurrentAndTargetValue(chainSettings.highCutFreq);
    highcutGainSmoother.setCurrentAndTargetValue(chainSettings.highCutGainInDecibels);
    highcutQualitySmoother.setCurrentAndTargetValue(chainSettings.highCutQuality);
}

FireAudioProcessor::~FireAudioProcessor()
{
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
}

const juce::String FireAudioProcessor::getProgramName(int index)
{
    return {};
}

void FireAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void FireAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    auto channels = static_cast<juce::uint32>(juce::jmin(getMainBusNumInputChannels(), getMainBusNumOutputChannels()));
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), channels };

    for (int i = 0; i < 4; ++i)
    {
        if (auto* band = bands[i].get())
        {
            // Call the newly simplified prepare method
            band->prepare(spec);

            // ** THIS IS THE CRUCIAL ADDITION **
            // Initialize smoothed values here, from the treeState
            // TODO: need to use new drive logic instead of ParameterID::driveIds[i])
            // band->newDriveSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterID::driveIds[i]));
            band->recSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(REC_ID, i)));
            band->biasSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(BIAS_ID, i)));
        }
    }

    lfoManager->prepare(spec);

    // historyArray init
    for (int i = 0; i < samplesPerBlock; i++)
    {
        historyArrayL.add(0);
        historyArrayR.add(0);
    }

    // dry wet buffer init
    //    mDryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    //    mDryBuffer.clear();
    delayMatchedDryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    delayMatchedDryBuffer.clear();

    mWetBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    mWetBuffer.clear();

    // oversampling init
    for (size_t i = 0; i < 4; i++)
    {
        oversamplingHQ[i]->reset();
        oversamplingHQ[i]->initProcessing(static_cast<size_t>(samplesPerBlock));
    }
    mDelay.reset(0);

    const float rampTimeSeconds = 0.0005f;

    lowcutFreqSmoother.reset(sampleRate, rampTimeSeconds);
    lowcutFreqSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(LOWCUT_FREQ_ID));
    lowcutGainSmoother.reset(sampleRate, rampTimeSeconds);
    lowcutGainSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(LOWCUT_GAIN_ID));
    lowcutQualitySmoother.reset(sampleRate, rampTimeSeconds);
    lowcutQualitySmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(LOWCUT_Q_ID));

    peakFreqSmoother.reset(sampleRate, rampTimeSeconds);
    peakFreqSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(PEAK_FREQ_ID));
    peakGainSmoother.reset(sampleRate, rampTimeSeconds);
    peakGainSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(PEAK_GAIN_ID));
    peakQualitySmoother.reset(sampleRate, rampTimeSeconds);
    peakQualitySmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(PEAK_Q_ID));

    highcutFreqSmoother.reset(sampleRate, rampTimeSeconds);
    highcutFreqSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(HIGHCUT_FREQ_ID));
    highcutGainSmoother.reset(sampleRate, rampTimeSeconds);
    highcutGainSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(HIGHCUT_GAIN_ID));
    highcutQualitySmoother.reset(sampleRate, rampTimeSeconds);
    highcutQualitySmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(HIGHCUT_Q_ID));

    smoothedFreq1.reset(sampleRate, rampTimeSeconds * 2);
    smoothedFreq1.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 0)));
    smoothedFreq2.reset(sampleRate, rampTimeSeconds * 2);
    smoothedFreq2.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 1)));
    smoothedFreq3.reset(sampleRate, rampTimeSeconds * 2);
    smoothedFreq3.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 2)));

    // filter init
    updateFilter(sampleRate);
    leftChain.prepare(spec);
    rightChain.prepare(spec);

    // mode diode================
    inputTemp.clear();
    VdiodeL = 0.0f;
    VdiodeR = 0.0f;
    RiL = 1;
    RiR = 1;
    /*
    Vin.Vs = 0.0f;
    Vin.R = Ri;
    R1.R = 80.f;
    C1.R = getSampleRate() / (2.0 * 3.5e-5);
    root = Serie(&Vin, &Serie(&R1, &C1));
    // mode 9 diode================
    */

    // multiband filters
    mBuffer1.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    mBuffer2.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    mBuffer3.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    mBuffer4.setSize(getTotalNumOutputChannels(), samplesPerBlock);
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

    compensatorEQ1.prepare(spec);
    compensatorEQ2.prepare(spec);
    compensatorEQ3.prepare(spec);

    // gain
    gainProcessorGlobal.prepare(spec);

    // dry wet
    juce::dsp::ProcessSpec globalMixerSpec = spec;
    globalMixerSpec.maximumBlockSize = spec.maximumBlockSize * 20; // set 20 to pass PluginVal
    dryWetMixerGlobal.prepare(globalMixerSpec);

    dryWetMixerGlobal.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    reset();
}

void FireAudioProcessor::reset()
{
    needsReset = true;

    std::vector<std::pair<float, bool>> freqAndStateVector;
    for (int i = 0; i < 3; ++i)
    {
        float freq = *treeState.getRawParameterValue(ParameterIDAndName::getIDString("freq", i));
        bool state = *treeState.getRawParameterValue(ParameterIDAndName::getIDString("lineState", i)) > 0.5f;
        freqAndStateVector.push_back({ freq, state });
    }

    // Sort by frequency
    std::sort(freqAndStateVector.begin(), freqAndStateVector.end());

    // Re-assign the sorted values back to the parameters
    for (int i = 0; i < 3; ++i)
    {
        treeState.getParameter(ParameterIDAndName::getIDString("freq", i))->setValueNotifyingHost(treeState.getParameter(ParameterIDAndName::getIDString("freq", i))->convertTo0to1(freqAndStateVector[i].first));
        treeState.getParameter(ParameterIDAndName::getIDString("lineState", i))->setValueNotifyingHost(freqAndStateVector[i].second ? 1.0f : 0.0f);
    }
}

void FireAudioProcessor::performReset()
{
    lowpass1.reset();
    lowpass2.reset();
    lowpass3.reset();
    highpass1.reset();
    highpass2.reset();
    highpass3.reset();
    compensatorLP.reset();
    compensatorHP.reset();
    compensatorEQ1.reset();
    compensatorEQ2.reset();
    compensatorEQ3.reset();
    dryWetMixerGlobal.reset();
    gainProcessorGlobal.reset();
    for (auto& band : bands)
        band->reset();
}

void FireAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    reset();
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
    // set bypass to true
    isBypassed = true;
}

void FireAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (isBypassed)
    {
        isBypassed = false;
    }

    if (needsReset.exchange(false))
    {
        performReset();
    }

    // report latency
    if (*treeState.getRawParameterValue(HQ_ID))
    {
        totalLatency = bands[0]->oversampling->getLatencyInSamples();
    }
    setLatencySamples(juce::roundToInt(totalLatency));

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto sampleRate = getSampleRate();

    if (sampleRate <= 0)
    {
        sampleRate = 48000;
    }

    juce::AudioBuffer<float> lfoOutputBuffer(4, buffer.getNumSamples());
    lfoOutputBuffer.clear();

    if (lfoManager->isModulationActive())
    {
        lfoManager->processBlock(sampleRate, getPlayHead(), buffer.getNumSamples());
    }

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    updateParameters();

    calculateAndStoreRMS(buffer, mInputLeftSmoothedGlobal, mInputRightSmoothedGlobal);

    // 1. GET PARAMETERS & SMOOTH FREQUENCIES
    int numBands = static_cast<int>(*treeState.getRawParameterValue(NUM_BANDS_ID));
    int lineNum = numBands - 1;

    // Set target values for smoothers from APVTS
    smoothedFreq1.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 0)));
    smoothedFreq2.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 1)));
    smoothedFreq3.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 2)));

    // Get the smoothed frequency values for this block
    // And also handle sorting based on active lines
    std::array<float, 3> freqArray = { 0.0f, 0.0f, 0.0f };
    int activeLineCount = 0;
    if (static_cast<bool>(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(LINE_STATE_ID, 0))))
        freqArray[activeLineCount++] = smoothedFreq1.getNextValue();
    if (static_cast<bool>(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(LINE_STATE_ID, 1))))
        freqArray[activeLineCount++] = smoothedFreq2.getNextValue();
    if (static_cast<bool>(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(LINE_STATE_ID, 2))))
        freqArray[activeLineCount++] = smoothedFreq3.getNextValue();

    if (activeLineCount > 1)
        std::sort(freqArray.begin(), freqArray.begin() + activeLineCount);

    float freqValue1 = freqArray[0];
    float freqValue2 = freqArray[1];
    float freqValue3 = freqArray[2];

    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;

    if (maxFreq < minFreq)
    {
        maxFreq = minFreq;
    }

    float safeFreqValue1 = juce::jlimit(minFreq, maxFreq, freqValue1);
    float safeFreqValue2 = juce::jlimit(minFreq, maxFreq, freqValue2);
    float safeFreqValue3 = juce::jlimit(minFreq, maxFreq, freqValue3);

    // 2. SET UP FILTERS WITH SMOOTHED VALUES
    lowpass1.setCutoffFrequency(safeFreqValue1);
    highpass1.setCutoffFrequency(safeFreqValue1);
    lowpass2.setCutoffFrequency(safeFreqValue2);
    highpass2.setCutoffFrequency(safeFreqValue2);
    lowpass3.setCutoffFrequency(safeFreqValue3);
    highpass3.setCutoffFrequency(safeFreqValue3);

    // Set up the all-pass coefficients for 3-band compensation
    if (lineNum == 2)
    {
        compensatorLP.setCutoffFrequency(safeFreqValue1);
        compensatorHP.setCutoffFrequency(safeFreqValue1);
    }

    // 3. PREPARE BUFFERS
    auto numSamples = buffer.getNumSamples();
    mBuffer1.setSize(totalNumOutputChannels, numSamples, false, false, true);
    mBuffer2.setSize(totalNumOutputChannels, numSamples, false, false, true);
    mBuffer3.setSize(totalNumOutputChannels, numSamples, false, false, true);
    mBuffer4.setSize(totalNumOutputChannels, numSamples, false, false, true);

    processMultiBand(buffer, sampleRate);

    applyDownsamplingEffect(buffer);
    // Call the simplified global effects function.
    applyGlobalEffects(buffer, sampleRate);

    // Call the simplified global mix function.
    applyGlobalMix(buffer);

    mWetBuffer.makeCopyOf(buffer);
    pushDataToFFT(mWetBuffer, processedSpecProcessor);
    pushDataToFFT(delayMatchedDryBuffer, originalSpecProcessor);
    calculateAndStoreRMS(mWetBuffer, mOutputLeftSmoothedGlobal, mOutputRightSmoothedGlobal);

    // --- 1. Push Modulated Filter Data to its FIFO ---
    if (filterFifo.getFreeSpace() >= 1)
    {
        // Get the final, modulated values for this block
        auto chainSettings = getChainSettings(treeState, *lfoManager);

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

        // Write the data packet to the buffer and notify the FIFO
        filterFifoBuffer[filterFifoWritePos] = filterVals;
        filterFifo.finishedWrite(1);
        filterFifoWritePos = (filterFifoWritePos + 1) % filterFifo.getTotalSize();
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
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    int xmlIndex = 0;
    juce::XmlElement xmlState { "state" };

    // 1. save treestate (parameters)
    auto state = treeState.copyState();
    std::unique_ptr<juce::XmlElement> treeStateXml(state.createXml());
    xmlState.insertChildElement(treeStateXml.release(), xmlIndex++);

    // 2. save current preset ID, width and height
    std::unique_ptr<juce::XmlElement> currentStateXml { new juce::XmlElement { "otherState" } };
    currentStateXml->setAttribute("currentPresetID", statePresets.getCurrentPresetId());
    currentStateXml->setAttribute("editorWidth", editorWidth);
    currentStateXml->setAttribute("editorHeight", editorHeight);

    xmlState.insertChildElement(currentStateXml.release(), xmlIndex++);

    // 3. Save LFO Shapes
    auto* lfoState = new juce::XmlElement("LFO_STATE");
    auto& lfoDataToSave = lfoManager->getLfoData();
    for (int i = 0; i < lfoDataToSave.size(); ++i)
    {
        auto* lfoXml = new juce::XmlElement("LFO");
        lfoXml->setAttribute("index", i);
        lfoDataToSave[i].writeToXml(*lfoXml);
        lfoState->addChildElement(lfoXml);
    }
    xmlState.insertChildElement(lfoState, xmlIndex++);

    // 4. Save Modulation Matrix Routings
    auto* modMatrixState = new juce::XmlElement("MODULATION_STATE");
    for (const auto& routing : lfoManager->getModulationRoutings()) // Get from manager
    {
        auto* routingXml = new juce::XmlElement("ROUTING");
        routing.writeToXml(*routingXml);
        modMatrixState->addChildElement(routingXml);
    }
    xmlState.insertChildElement(modMatrixState, xmlIndex++);

    copyXmlToBinary(xmlState, destData);
}

void FireAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName("state"))
    {
        int xmlIndex = 0;
        // 1. set treestate
        const auto xmlTreeState = xmlState->getChildElement(xmlIndex++);
        if (xmlTreeState != nullptr)
        {
            treeState.replaceState(juce::ValueTree::fromXml(*xmlTreeState));
        }

        // 2. set current preset ID
        const auto xmlCurrentState = xmlState->getChildElement(xmlIndex++);
        if (xmlCurrentState != nullptr)
        {
            // load preset
            presetId = xmlCurrentState->getIntAttribute("currentPresetID", 0);
            statePresets.setCurrentPresetId(presetId); // only set preset id not value
            // so it won't override the value you changed from the preset

            editorWidth = xmlCurrentState->getIntAttribute("editorWidth", INIT_WIDTH);
            editorHeight = xmlCurrentState->getIntAttribute("editorHeight", INIT_HEIGHT);
        }

        // 3. Load LFO Shapes
        if (auto* lfoState = xmlState->getChildByName("LFO_STATE"))
        {
            auto& lfoDataToLoad = lfoManager->getLfoData();
            for (auto* lfoXml : lfoState->getChildIterator())
            {
                const int index = lfoXml->getIntAttribute("index", -1);
                if (juce::isPositiveAndBelow(index, (int) lfoDataToLoad.size()))
                {
                    lfoDataToLoad[index] = LfoData::readFromXml(*lfoXml);
                }
            }
        }

        // 4. Load Modulation Matrix Routings
        if (auto* modMatrixState = xmlState->getChildByName("MODULATION_STATE"))
        {
            // Get a reference to the manager's routings array
            auto& routings = lfoManager->getModulationRoutings();
            routings.clear(); // Clear existing data before loading
            for (auto* routingXml : modMatrixState->getChildIterator())
            {
                routings.add(ModulationRouting::readFromXml(*routingXml));
            }
        }

        // IMPORTANT: After loading, ensure the UI is updated if the editor is open.
        // This is a simplified notification. A more robust system might use a
        // ChangeBroadcaster/Listener pattern.
        if (auto* editor = getActiveEditor())
        {
            // A simple repaint might be sufficient if your components read data in their paint calls.
            // For more complex updates, you'd call specific update functions on the editor.
            editor->repaint();
        }

        sendChangeMessage();
    }
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
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue(LOWCUT_SLOPE_ID)->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID)->load());

    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load(); // > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load(); // > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load(); // > 0.5f;

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
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue(LOWCUT_SLOPE_ID)->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID)->load());
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
    auto peakCoefficients = makePeakFilter(chainSettings, sampleRate);

    leftChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    rightChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void FireAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings, double sampleRate)
{
    auto cutCoefficients = makeLowCutFilter(chainSettings, sampleRate);
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();

    auto lowcutQCoefficients = makeLowcutQFilter(chainSettings, sampleRate);

    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    leftChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);

    updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(leftLowCut, cutCoefficients, chainSettings.lowCutSlope);

    updateCoefficients(leftChain.get<ChainPositions::LowCutQ>().coefficients, lowcutQCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::LowCutQ>().coefficients, lowcutQCoefficients);
}

void FireAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings, double sampleRate)
{
    auto highCutCoefficients = makeHighCutFilter(chainSettings, sampleRate);
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

    auto highcutQCoefficients = makeHighcutQFilter(chainSettings, sampleRate);

    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);

    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);

    updateCoefficients(leftChain.get<ChainPositions::HighCutQ>().coefficients, highcutQCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::HighCutQ>().coefficients, highcutQCoefficients);
}

void FireAudioProcessor::updateFilter(double sampleRate)
{
    auto chainSettings = getChainSettings(treeState);
    const float minFreq = 20.0f;
    if (sampleRate <= 0)
        return;

    float maxFreq = sampleRate / 2.0f;

    if (maxFreq < minFreq)
    {
        maxFreq = minFreq;
    }

    chainSettings.lowCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(minFreq, maxFreq, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.highCutFreq);

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
    smoothedSettings.lowCutFreq = lowcutFreqSmoother.getNextValue();
    smoothedSettings.lowCutGainInDecibels = lowcutGainSmoother.getNextValue();
    smoothedSettings.lowCutQuality = lowcutQualitySmoother.getNextValue();

    smoothedSettings.peakFreq = peakFreqSmoother.getNextValue();
    smoothedSettings.peakGainInDecibels = peakGainSmoother.getNextValue();
    smoothedSettings.peakQuality = peakQualitySmoother.getNextValue();

    // Get the smoothed high-cut frequency value.
    float highCutFreqValue = highcutFreqSmoother.getNextValue();
    // Define the Nyquist frequency.
    const float nyquist = (float) sampleRate / 2.0f;
    // Clamp the high-cut frequency to ensure it never exceeds the Nyquist frequency.
    smoothedSettings.highCutFreq = juce::jmin(highCutFreqValue, nyquist);

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
}

bool FireAudioProcessor::isSlient(juce::AudioBuffer<float> buffer)
{
    if (buffer.getMagnitude(0, buffer.getNumSamples()) == 0)
        return true;
    else
        return false;
}

void FireAudioProcessor::setHistoryArray(int bandIndex)
{
    // This array holds pointers to all possible source buffers.
    std::array<juce::AudioBuffer<float>*, 5> sourceBuffers = { &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4, &mWetBuffer };

    // Use the bandIndex to select the correct buffer. If the index is out of bounds (e.g., -1 for global),
    // we safely default to the last buffer in the array (mWetBuffer).
    const int effectiveIndex = (juce::isPositiveAndBelow(bandIndex, 4)) ? bandIndex : 4;
    auto* sourceBuffer = sourceBuffers[effectiveIndex];

    // Safety check: if the selected buffer is invalid for any reason, do nothing.
    if (sourceBuffer == nullptr)
        return;

    const int bufferSamples = sourceBuffer->getNumSamples();
    const int numChannels = sourceBuffer->getNumChannels();

    if (numChannels == 0 || bufferSamples == 0)
        return;

    // --- The processing loop is now much cleaner ---
    // We only need to get the channel data once.
    const float* channelDataL = sourceBuffer->getReadPointer(0);
    const float* channelDataR = (numChannels > 1) ? sourceBuffer->getReadPointer(1) : nullptr;

    for (int sample = 0; sample < bufferSamples; ++sample)
    {
        // Downsample the data by only taking every 10th sample.
        if (sample % 10 == 0)
        {
            // Process Left Channel
            historyArrayL.add(channelDataL[sample]);
            if (historyArrayL.size() > historyLength)
                historyArrayL.remove(0);

            // Process Right Channel (if it exists)
            if (channelDataR != nullptr)
            {
                historyArrayR.add(channelDataR[sample]);
                if (historyArrayR.size() > historyLength)
                    historyArrayR.remove(0);
            }
        }
    }
}

juce::Array<float> FireAudioProcessor::getHistoryArrayL()
{
    return historyArrayL;
}

juce::Array<float> FireAudioProcessor::getHistoryArrayR()
{
    return historyArrayR;
}

float* FireAudioProcessor::getFFTData(int dataIndex)
{
    if (dataIndex == 0)
    {
        return originalSpecProcessor.fftData;
    }
    else
    {
        return processedSpecProcessor.fftData;
    }
}

int FireAudioProcessor::getNumBins()
{
    return processedSpecProcessor.numBins;
}

int FireAudioProcessor::getFFTSize()
{
    return processedSpecProcessor.fftSize;
}

bool FireAudioProcessor::isFFTBlockReady()
{
    return processedSpecProcessor.nextFFTBlockReady;
}

void FireAudioProcessor::pushDataToFFT(juce::AudioBuffer<float>& buffer, SpectrumProcessor& specProcessor)
{
    if (buffer.getNumChannels() > 0)
    {
        auto* channelData = buffer.getReadPointer(0);

        for (auto i = 0; i < buffer.getNumSamples(); ++i)
            specProcessor.pushNextSampleIntoFifo(channelData[i]);
    }
}

void FireAudioProcessor::processFFT(float* tempFFTData, int dataIndex)
{
    SpectrumProcessor& specProcessor = (dataIndex == 0) ? originalSpecProcessor : processedSpecProcessor;
    specProcessor.doProcessing(tempFFTData);
    specProcessor.nextFFTBlockReady = false;
}

void FireAudioProcessor::processGain(juce::dsp::ProcessContextReplacing<float> context, juce::String outputID, GainProcessor& gainProcessor)
{
    float outputValue = *treeState.getRawParameterValue(outputID);
    gainProcessor.setGainDecibels(outputValue);
    gainProcessor.setRampDurationSeconds(0.05f);
    gainProcessor.process(context);
}

int FireAudioProcessor::getSavedWidth() const
{
    return editorWidth;
}

int FireAudioProcessor::getSavedHeight() const
{
    return editorHeight;
}

void FireAudioProcessor::setSavedWidth(const int width)
{
    editorWidth = width;
}

void FireAudioProcessor::setSavedHeight(const int height)
{
    editorHeight = height;
}

bool FireAudioProcessor::getBypassedState()
{
    return isBypassed;
}

// drive lookandfeel
float FireAudioProcessor::getReductionPrecent(int bandIndex)
{
    // Safely check if the provided index is valid for our bands vector.
    if (juce::isPositiveAndBelow(bandIndex, bands.size()))
        if (auto* band = bands[bandIndex].get())
            return band->mReductionPercent; // Return the value from the BandProcessor instance.

    // If the index is invalid, assert in debug mode and return a safe default.
    jassertfalse;
    return 0.0f;
}

float FireAudioProcessor::getSampleMaxValue(int bandIndex)
{
    if (juce::isPositiveAndBelow(bandIndex, bands.size()))
        if (auto* band = bands[bandIndex].get())
            return band->mSampleMaxValue;

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
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(WIDTH_ID, i), WIDTH_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(OUTPUT_ID, i), OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(MIX_ID, i), MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(BIAS_ID, i), BIAS_NAME, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterIDAndName::getID(REC_ID, i), REC_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_ENABLE_ID, i), BAND_ENABLE_NAME, true));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(BAND_SOLO_ID, i), BAND_SOLO_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(COMP_BYPASS_ID, i), COMP_BYPASS_NAME, false));
        parameters.push_back(std::make_unique<PBool>(ParameterIDAndName::getID(WIDTH_BYPASS_ID, i), WIDTH_BYPASS_NAME, false));
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

    // Get the number of active bands for processing loops.
    numBands = *treeState.getRawParameterValue(NUM_BANDS_ID);
    activeCrossovers = numBands - 1;

    // Update smoothers for crossover frequencies.
    smoothedFreq1.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 0)));
    smoothedFreq2.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 1)));
    smoothedFreq3.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(FREQ_ID, 2)));

    //==============================================================================
    // 2. Update Per-Band Smoothed Parameters
    //==============================================================================

    // Iterate through the bands vector to update each BandProcessor's smoothers.
    for (int i = 0; i < 4; ++i)
    {
        if (auto* band = bands[i].get())
        {
            //            band->drive.setTargetValue(*treeState.getRawParameterValue(ParameterID::driveIds[i]));
            band->recSmoother.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(REC_ID, i)));
            band->biasSmoother.setTargetValue(*treeState.getRawParameterValue(ParameterIDAndName::getIDString(BIAS_ID, i)));
        }
    }

    //==============================================================================
    // 3. Update Global Filter Smoothed Parameters
    //==============================================================================

    // Use the existing helper function to get all filter-related parameter values.
    auto chainSettings = getChainSettings(treeState);

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
            if (*treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_SOLO_ID, i)))
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

        const bool isThisBandSoloed = *treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_SOLO_ID, i)) > 0.5f;

        // Determine if this band should be added to the mix.
        const bool shouldAddBand = anySoloActive ? isThisBandSoloed : true;

        if (shouldAddBand || ignoreSoloLogic)
        {
            // The number of samples to process for this operation.
            const int numSamples = currentBandBuffer->getNumSamples();

            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            {
                // Ensure the destination buffer has enough space before adding.
                // This is a safety check.
                if (channel < currentBandBuffer->getNumChannels())
                {
                    // FIX: Use the number of samples from the *source* buffer, not the destination.
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

    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = inputBuffer.getNumSamples();

    if (sampleRate <= 0)
        return;
    const int lineNum = activeCrossovers; // Use the member variable updated in updateParameters()

    // Get the latest smoothed frequency values for the crossovers
    float freqValue1 = smoothedFreq1.getNextValue();
    float freqValue2 = smoothedFreq2.getNextValue();
    float freqValue3 = smoothedFreq3.getNextValue();

    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;

    if (maxFreq < minFreq)
    {
        maxFreq = minFreq;
    }

    freqValue1 = juce::jlimit(minFreq, maxFreq, freqValue1);
    freqValue2 = juce::jlimit(minFreq, maxFreq, freqValue2);
    freqValue3 = juce::jlimit(minFreq, maxFreq, freqValue3);

    // Set up crossover filters with these frequencies
    lowpass1.setCutoffFrequency(freqValue1);
    highpass1.setCutoffFrequency(freqValue1);
    lowpass2.setCutoffFrequency(freqValue2);
    highpass2.setCutoffFrequency(freqValue2);
    lowpass3.setCutoffFrequency(freqValue3);
    highpass3.setCutoffFrequency(freqValue3);

    // Set up the all-pass coefficients for 3-band compensation
    if (lineNum == 2)
    {
        compensatorLP.setCutoffFrequency(freqValue1);
        compensatorHP.setCutoffFrequency(freqValue1);
    }

    // --- TREE-BASED SIGNAL SPLITTING ---
    if (lineNum == 0)
    { // 1 Band: No splitting, the signal just passes through
        mBuffer1.makeCopyOf(inputBuffer);
    }
    else if (lineNum == 1)
    { // 2 Bands: One split
        mBuffer1.makeCopyOf(inputBuffer);
        mBuffer2.makeCopyOf(inputBuffer);

        auto block1 = juce::dsp::AudioBlock<float>(mBuffer1);
        auto context1 = juce::dsp::ProcessContextReplacing<float>(block1);
        lowpass1.process(context1);

        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        highpass1.process(context2);
    }
    else if (lineNum == 2)
    { // 3 Bands: Asymmetric tree with Dummy Filter compensation
        juce::AudioBuffer<float> highPassBuffer(totalNumOutputChannels, numSamples);
        juce::AudioBuffer<float> compensatorHighBuffer(totalNumOutputChannels, numSamples);

        mBuffer1.makeCopyOf(inputBuffer);
        highPassBuffer.makeCopyOf(inputBuffer);

        // Compensator Path for Band 1 to match group delay
        compensatorHighBuffer.makeCopyOf(mBuffer1);

        auto compLpBlock = juce::dsp::AudioBlock<float>(mBuffer1);
        auto compLpContext = juce::dsp::ProcessContextReplacing<float>(compLpBlock);
        compensatorLP.process(compLpContext);

        auto compHpBlock = juce::dsp::AudioBlock<float>(compensatorHighBuffer);
        auto compHpContext = juce::dsp::ProcessContextReplacing<float>(compHpBlock);
        compensatorHP.process(compHpContext);

        // Real Split Path for Band 2 and 3
        auto highPassBlock = juce::dsp::AudioBlock<float>(highPassBuffer);
        auto highPassContext = juce::dsp::ProcessContextReplacing<float>(highPassBlock);
        highpass1.process(highPassContext);

        mBuffer2.makeCopyOf(highPassBuffer);
        mBuffer3.makeCopyOf(highPassBuffer);

        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        lowpass2.process(context2);

        auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
        highpass2.process(context3);
    }
    else if (lineNum == 3)
    { // 4 Bands: Symmetric tree
        juce::AudioBuffer<float> lowMidBuffer(totalNumOutputChannels, numSamples);
        juce::AudioBuffer<float> highMidBuffer(totalNumOutputChannels, numSamples);
        lowMidBuffer.makeCopyOf(inputBuffer);
        highMidBuffer.makeCopyOf(inputBuffer);

        auto lowMidBlock = juce::dsp::AudioBlock<float>(lowMidBuffer);
        auto highMidBlock = juce::dsp::AudioBlock<float>(highMidBuffer);
        auto lowMidContext = juce::dsp::ProcessContextReplacing<float>(lowMidBlock);
        auto highMidContext = juce::dsp::ProcessContextReplacing<float>(highMidBlock);

        lowpass2.setCutoffFrequency(freqValue2); // Use middle frequency for first split
        highpass2.setCutoffFrequency(freqValue2);
        lowpass2.process(lowMidContext);
        highpass2.process(highMidContext);

        mBuffer1.makeCopyOf(lowMidBuffer);
        mBuffer2.makeCopyOf(lowMidBuffer);
        auto block1 = juce::dsp::AudioBlock<float>(mBuffer1);
        auto context1 = juce::dsp::ProcessContextReplacing<float>(block1);
        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        lowpass1.process(context1);
        highpass1.process(context2);

        mBuffer3.makeCopyOf(highMidBuffer);
        mBuffer4.makeCopyOf(highMidBuffer);
        auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
        auto block4 = juce::dsp::AudioBlock<float>(mBuffer4);
        auto context4 = juce::dsp::ProcessContextReplacing<float>(block4);
        lowpass3.process(context3);
        highpass3.process(context4);
    }

    //==============================================================================
    // DYNAMIC COMPENSATION
    // This logic is now part of the splitting process, applied after the crossovers.
    //==============================================================================
    const float ratioThreshold = 6.0f;

    // For Band 2
    if (lineNum > 1)
    {
        float f1 = freqValue1, f2 = freqValue2;
        if (f1 > 20.0f && f2 > f1 && (f2 / f1) < ratioThreshold)
        {
            float k = f2 / f1;
            const float denominator = k - 1.0f;
            if (std::abs(denominator) > 1e-6f)
            {
                float centerFreq = std::sqrt(f1 * f2);
                float Q = std::sqrt(k) / denominator;
                float gain = (1.0f + k * k) * (1.0f + k * k) / (k * k * k * k);

                *compensatorEQ1.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerFreq, Q, gain);
                auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
                auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
                compensatorEQ1.process(context2);
            }
        }
    }
    // For Band 3
    if (lineNum > 2)
    {
        float f1 = freqValue2, f2 = freqValue3;
        if (f1 > 20.0f && f2 > f1 && (f2 / f1) < ratioThreshold)
        {
            float k = f2 / f1;
            const float denominator = k - 1.0f;
            if (std::abs(denominator) > 1e-6f)
            {
                float centerFreq = std::sqrt(f1 * f2);
                float Q = std::sqrt(k) / denominator;
                float gain = (1.0f + k * k) * (1.0f + k * k) / (k * k * k * k);

                *compensatorEQ2.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerFreq, Q, gain);
                auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
                auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
                compensatorEQ2.process(context3);
            }
        }
    }
}

void FireAudioProcessor::updateGlobalFilters(double sampleRate)
{
    // Get the final, modulated settings for the entire filter chain.
    auto chainSettings = getChainSettings(treeState, *lfoManager);

    // It's good practice to ensure frequencies are within a valid range.
    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;

    if (maxFreq < minFreq)
        maxFreq = minFreq;

    // Clamp frequencies to be safe.
    chainSettings.lowCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(minFreq, maxFreq, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.highCutFreq);

    // Call the modular update functions with the final settings.
    updateLowCutFilters(chainSettings, sampleRate);
    updatePeakFilter(chainSettings, sampleRate);
    updateHighCutFilters(chainSettings, sampleRate);
}

float FireAudioProcessor::getTotalLatency() const
{
    return totalLatency;
}

void FireAudioProcessor::processMultiBand(juce::AudioBuffer<float>& wetBuffer, double sampleRate)
{
    splitBands(wetBuffer, sampleRate);

    delayMatchedDryBuffer.clear();

    std::array<juce::AudioBuffer<float>*, 4> dryBandBuffers = { &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4 };
    std::array<juce::AudioBuffer<float>*, 4> wetBandBuffers = { &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4 };

    sumBands(delayMatchedDryBuffer, dryBandBuffers, true);

    for (int i = 0; i < numBands; ++i)
    {
        if (auto* band = bands[i].get())
        {
            calculateAndStoreRMS(*dryBandBuffers[i], band->mInputLeftSmoothed, band->mInputRightSmoothed);

            if (*treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, i)))
            {
                BandProcessingParameters params;

                // Populate non-modulated parameters
                params.mode = *treeState.getRawParameterValue(ParameterIDAndName::getIDString(MODE_ID, i));
                params.isHQ = *treeState.getRawParameterValue(HQ_ID);
                params.isCompEnabled = *treeState.getRawParameterValue(ParameterIDAndName::getIDString(COMP_BYPASS_ID, i)) > 0.5f;
                params.isWidthEnabled = *treeState.getRawParameterValue(ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, i)) > 0.5f;
                params.isSafeModeOn = *treeState.getRawParameterValue(ParameterIDAndName::getIDString(SAFE_ID, i)) > 0.5f;
                params.isExtremeModeOn = *treeState.getRawParameterValue(ParameterIDAndName::getIDString(EXTREME_ID, i)) > 0.5f;

                // === GET FINAL MODULATED VALUES FROM LFO MANAGER ===
                params.driveVal = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(DRIVE_ID, i));
                params.compRatio = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(COMP_RATIO_ID, i));
                params.compThreshold = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(COMP_THRESH_ID, i));
                params.width = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(WIDTH_ID, i));
                params.outputVal = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(OUTPUT_ID, i));
                params.mixVal = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(MIX_ID, i));
                params.biasVal = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(BIAS_ID, i));
                params.recVal = lfoManager->getModulatedValue(ParameterIDAndName::getIDString(REC_ID, i));

                realtimeModulatedThresholds[i].store(params.compThreshold);

                // Call the simplified process method
                band->process(*wetBandBuffers[i], params);
            }
            calculateAndStoreRMS(*wetBandBuffers[i], band->mOutputLeftSmoothed, band->mOutputRightSmoothed);
        }
    }

    sumBands(wetBuffer, wetBandBuffers, false);
}

void FireAudioProcessor::applyGlobalEffects(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (*treeState.getRawParameterValue(FILTER_BYPASS_ID) > 0.5f)
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

    auto globalBlock = juce::dsp::AudioBlock<float>(buffer);

    // Get the final modulated output gain from the LfoManager
    gainProcessorGlobal.setGainDecibels(lfoManager->getModulatedValue(OUTPUT_ID));
    gainProcessorGlobal.setRampDurationSeconds(0.05f);
    gainProcessorGlobal.process(juce::dsp::ProcessContextReplacing<float>(globalBlock));
}

void FireAudioProcessor::applyDownsamplingEffect(juce::AudioBuffer<float>& buffer)
{
    if (*treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID) > 0.5f)
    {
        const int rateDivide = static_cast<int>(lfoManager->getModulatedValue(DOWNSAMPLE_ID));

        if (rateDivide > 1)
        {
            for (int channel = 0; channel < getTotalNumInputChannels(); ++channel)
            {
                auto* channelData = buffer.getWritePointer(channel);
                int samplesToHold = 0;
                float sampleToHold = 0.0f;

                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    if (samplesToHold == 0)
                    {
                        sampleToHold = channelData[sample];
                        samplesToHold = rateDivide - 1;
                    }
                    else
                    {
                        channelData[sample] = sampleToHold;
                        samplesToHold--;
                    }
                }
            }
        }
    }
}

void FireAudioProcessor::applyGlobalMix(juce::AudioBuffer<float>& buffer)
{
    if (*treeState.getRawParameterValue(HQ_ID))
    {
        dryWetMixerGlobal.setWetLatency(totalLatency);
    }
    else
    {
        dryWetMixerGlobal.setWetLatency(0);
    }

    // Get the final modulated mix value from the LfoManager
    dryWetMixerGlobal.setWetMixProportion(lfoManager->getModulatedValue(MIX_ID));
    dryWetMixerGlobal.pushDrySamples(juce::dsp::AudioBlock<float>(delayMatchedDryBuffer));
    dryWetMixerGlobal.mixWetSamples(juce::dsp::AudioBlock<float>(buffer));
}

FireAudioProcessor::ModulationInfo FireAudioProcessor::getModulationInfoForParameter(const juce::String& parameterID) const
{
    // Find the routing in the manager's list
    for (const auto& routing : lfoManager->getModulationRoutings())
    {
        if (routing.targetParameterID == parameterID)
        {
            if (juce::isPositiveAndBelow(routing.sourceLfoIndex, 4))
            {
                const float unipolarLfoValue = lfoManager->getLfoOutput(routing.sourceLfoIndex);
                float finalLfoValue = routing.isBipolar ? (unipolarLfoValue * 2.0f - 1.0f) : unipolarLfoValue;
                return { true, routing.sourceLfoIndex + 1, routing.depth, finalLfoValue, routing.isBipolar };
            }
        }
    }
    return { false, 0, 0.0f, 0.0f, true }; // Default "not modulated" state
}

void FireAudioProcessor::setModulationDepth(const juce::String& targetParameterID, float newDepth)
{
    // Find the matching routing in the modulation array.
    for (auto& routing : lfoManager->getModulationRoutings())
    {
        if (routing.targetParameterID == targetParameterID)
        {
            // Found it. Update its depth value, ensuring it stays within the [0, 1] range.
            routing.depth = juce::jlimit(-1.0f, 1.0f, newDepth);

            lfoDataHasChanged();

            // For now, we only handle the first match.
            return;
        }
    }
}

void FireAudioProcessor::toggleBipolarMode(const juce::String& targetParameterID)
{
    // Find the matching routing in the modulation array.
    for (auto& routing : lfoManager->getModulationRoutings())
    {
        if (routing.targetParameterID == targetParameterID)
        {
            // Found it. Flip the boolean state.
            routing.isBipolar = ! routing.isBipolar;

            lfoDataHasChanged();

            // For now, we only handle the first match.
            return;
        }
    }
}

void FireAudioProcessor::resetModulation(const juce::String& targetParameterID)
{
    // Find the matching routing in the modulation array.
    for (auto& routing : lfoManager->getModulationRoutings())
    {
        if (routing.targetParameterID == targetParameterID)
        {
            // Reset depth to 0.5 and mode to the default (bipolar)
            routing.depth = 0.5f;
            routing.isBipolar = true;

            lfoDataHasChanged();

            // We found the routing and reset it, so we can exit the function.
            return;
        }
    }
}

void FireAudioProcessor::assignModulation(int routingIndex, int sourceLfoIndex, const juce::String& targetParameterID)
{
    auto& routings = lfoManager->getModulationRoutings();
    if (! juce::isPositiveAndBelow(routingIndex, routings.size()))
        return;

    if (targetParameterID.isNotEmpty())
    {
        for (int i = 0; i < routings.size(); ++i)
        {
            if (i == routingIndex)
                continue;
            if (routings.getReference(i).targetParameterID == targetParameterID)
            {
                routings.getReference(i).targetParameterID = "";
            }
        }
    }

    auto& currentRouting = routings.getReference(routingIndex);
    currentRouting.sourceLfoIndex = sourceLfoIndex;
    currentRouting.targetParameterID = targetParameterID;

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
    lfoManager->onLfoShapeChanged(-1);
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

float FireAudioProcessor::getGlobalInputMeterLevel(int channel) const
{
    return channel == 0 ? mInputLeftSmoothedGlobal.load() : mInputRightSmoothedGlobal.load();
}

float FireAudioProcessor::getGlobalOutputMeterLevel(int channel) const
{
    return channel == 0 ? mOutputLeftSmoothedGlobal.load() : mOutputRightSmoothedGlobal.load();
}

float FireAudioProcessor::getBandInputMeterLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mInputLeftSmoothed.load() : bandProcessor->mInputRightSmoothed.load();
        }
    }
    jassertfalse; // Invalid band index
    return 0.0f;
}

float FireAudioProcessor::getBandOutputMeterLevel(int band, int channel) const
{
    if (juce::isPositiveAndBelow(band, bands.size()))
    {
        if (auto* bandProcessor = bands[band].get())
        {
            return channel == 0 ? bandProcessor->mOutputLeftSmoothed.load() : bandProcessor->mOutputRightSmoothed.load();
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
    auto presetTree = juce::ValueTree::fromXml(presetXml);
    if (! presetTree.isValid())
        return false;

    return treeState.state.isEquivalentTo(presetTree);
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
    auto& routings = lfoManager->getModulationRoutings();
    const auto& bandParamBases = ParameterIDAndName::getBandParameterInfo();

    for (auto& routing : routings)
    {
        if (routing.targetParameterID.isEmpty())
            continue;

        for (const auto& paramInfo : bandParamBases)
        {
            // Check if the target ID starts with a known band parameter ID base
            if (routing.targetParameterID.startsWith(paramInfo.idBase))
            {
                // Extract the number part of the ID
                juce::String indexStr = routing.targetParameterID.substring(paramInfo.idBase.length());

                if (! indexStr.containsOnly("0123456789"))
                    continue;

                int currentBandIndex = indexStr.getIntValue() - 1; // Convert to 0-based

                // Check if the current band is within the range we need to shift
                if (currentBandIndex >= startIndex && currentBandIndex <= endIndex)
                {
                    int newBandIndex = currentBandIndex + shiftAmount;
                    routing.targetParameterID = paramInfo.idBase + juce::String(newBandIndex + 1);

                    // Found a match and processed it, no need to check other bases for this routing
                    goto next_routing;
                }
            }
        }
    next_routing:; // Label to jump to for the next iteration of the outer loop
    }
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
    auto& routings = lfoManager->getModulationRoutings();
    const auto& bandParamBases = ParameterIDAndName::getBandParameterInfo();
    const juce::String bandSuffix = juce::String(bandIndex + 1);

    for (auto& routing : routings)
    {
        if (routing.targetParameterID.isEmpty())
            continue;

        for (const auto& paramInfo : bandParamBases)
        {
            // Check if the target is an exact match for a parameter in the specified band
            if (routing.targetParameterID == (paramInfo.idBase + bandSuffix))
            {
                routing.targetParameterID = ""; // Set target to "None"
                goto next_routing_clear;
            }
        }
    next_routing_clear:;
    }
    lfoDataHasChanged();
}