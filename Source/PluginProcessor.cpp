/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// BandProcessor Implementation
//==============================================================================

// This is where we tell JUCE what to do when prepareToPlay is called for a single band.
void BandProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    // Prepare all the DSP modules with the sample rate and block size.
    compressor.prepare(spec);
    overdrive.prepare(spec);
    gain.prepare(spec);
    dryWetMixer.prepare(spec);

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
    drive.reset(spec.sampleRate, 0.05);
    rec.reset(spec.sampleRate, 0.05);
}

// This is what happens when we need to clear the internal state of a band's processors.
void BandProcessor::reset()
{
    compressor.reset();
    overdrive.reset();
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
void BandProcessor::process(juce::AudioBuffer<float>& buffer, const BandProcessingParameters& params)
{
    // Extract parameters for easier access
    const int   mode      = params.mode;
    const bool  isHQ      = params.isHQ;
    const float outputVal = params.outputVal;
    const float mixVal    = params.mixVal;

    // Create a copy of the clean signal for the final dry/wet mix
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Set the waveshaper function based on the current mode
    auto& waveShaper = this->overdrive.get<2>();
    switch (mode)
    {
        case 0:  waveShaper.functionToUse = waveshaping::arctanSoftClipping; break;
        case 1:  waveShaper.functionToUse = waveshaping::expSoftClipping;    break;
        case 2:  waveShaper.functionToUse = waveshaping::tanhSoftClipping;   break;
        case 3:  waveShaper.functionToUse = waveshaping::cubicSoftClipping;  break;
        case 4:  waveShaper.functionToUse = waveshaping::hardClipping;      break;
        case 5:  waveShaper.functionToUse = waveshaping::sausageFattener;   break;
        case 6:  waveShaper.functionToUse = waveshaping::sinFoldback;       break;
        case 7:  waveShaper.functionToUse = waveshaping::linFoldback;       break;
        case 8:  waveShaper.functionToUse = waveshaping::limitClip;         break;
        case 9:  waveShaper.functionToUse = waveshaping::singleSinClip;     break;
        case 10: waveShaper.functionToUse = waveshaping::logicClip;         break;
        case 11: waveShaper.functionToUse = waveshaping::tanclip;           break;
    }
    
    // === 2. Core Processing (with correct Oversampling) ===
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

    // === 3. Final Gain and Dry/Wet Mix at block level ===
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
    // This robust approach avoids issues when mix is exactly 0 or 1
    if (mixVal <= 0.0f)
    {
        buffer.makeCopyOf(dryBuffer); // If 0% wet, just use the clean dry signal
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

    if (params.isCompBypassed)
    {
        this->compressor.setThreshold(params.compThreshold);
        this->compressor.setRatio(params.compRatio);
        this->compressor.process(postDistortionContext);
    }

    if (params.isWidthBypassed && buffer.getNumChannels() == 2)
    {
        this->widthProcessor.process(buffer.getWritePointer(0), buffer.getWritePointer(1), params.width, buffer.getNumSamples());
    }
}

void BandProcessor::processDistortion(juce::dsp::AudioBlock<float>& blockToProcess,
                                      const juce::AudioBuffer<float>& dryBuffer,
                                      const BandProcessingParameters& params)
{
    const bool isSafeModeOn    = params.isSafeModeOn;
    const bool isExtremeModeOn = params.isExtremeModeOn;
    float driveVal             = params.driveVal;
    const float biasVal        = params.biasVal;
    const float recVal         = params.recVal;
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
        this->newDrive = 2.0f / this->mSampleMaxValue + 0.1f * std::log2(powerDrive);
    else
        this->newDrive = powerDrive;
    
    // Update the reduction percent for the UI to read
    if (driveForCalc == 0.0f || this->mSampleMaxValue <= 0.001f)
        this->mReductionPercent = 1.0f;
    else
        this->mReductionPercent = std::log2(this->newDrive) / driveForCalc;

    // --- Configure the ProcessorChain components ---

    // 1. Input Gain (Drive)
    auto& gainUp = this->overdrive.get<0>();
    gainUp.setGainLinear(this->newDrive);
    gainUp.setRampDurationSeconds(0.05f); // Set ramp time in prepareToPlay for better practice

    // 2. Pre-Waveshaper Bias
    auto& bias1 = this->overdrive.get<1>();
    // Mute bias if the input signal is silent to prevent DC offset noise
    float finalBias = (this->mSampleMaxValue < 0.000001f) ? 0.0f : biasVal;
    bias1.setBias(finalBias);
    bias1.setRampDurationSeconds(0.05f);

    // 3. Main Waveshaper (functionToUse should be already set)
    auto& waveShaper = this->overdrive.get<2>(); // No per-block setup needed if function is set elsewhere

    // 4. Rectification (asymmetrical processing)
    auto& rectifier = this->overdrive.get<3>();
    rectifier.functionToUse = [recVal](float input)
    {
        // This lambda implements the rectification logic
        if (input < 0.0f)
        {
            return input * (0.5f - recVal) * 2.0f;
        }
        return input;
    };

    // 5. Post-Waveshaper Bias (to cancel out the initial DC offset)
    auto& bias2 = this->overdrive.get<4>();
    bias2.setBias(-finalBias);
    bias2.setRampDurationSeconds(0.05f);

    // --- Process the entire block using the configured chain ---
    auto context = juce::dsp::ProcessContextReplacing<float>(blockToProcess);
    this->overdrive.process(context);
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
      } //AppData/Roaming/...
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
    
    chainSettings.lowCutFreq  = juce::jlimit(minFreq, maxFreq, chainSettings.lowCutFreq);
    chainSettings.peakFreq    = juce::jlimit(minFreq, maxFreq, chainSettings.peakFreq);
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
            band->drive.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterID::driveIds[i]));
            band->rec.setCurrentAndTargetValue(*treeState.getRawParameterValue(ParameterID::recIds[i]));
        }
    }
    
    for (auto& engine : lfoEngines)
    {
        engine.prepare(spec);
    }

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
    smoothedFreq1.setCurrentAndTargetValue(*treeState.getRawParameterValue(FREQ_ID1));
    smoothedFreq2.reset(sampleRate, rampTimeSeconds * 2);
    smoothedFreq2.setCurrentAndTargetValue(*treeState.getRawParameterValue(FREQ_ID2));
    smoothedFreq3.reset(sampleRate, rampTimeSeconds * 2);
    smoothedFreq3.setCurrentAndTargetValue(*treeState.getRawParameterValue(FREQ_ID3));

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
    globalMixSmoother.reset(sampleRate, rampTimeSeconds);
    globalMixSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue(MIX_ID));
    dryWetMixerGlobal.prepare(spec);
    reset();
}

void FireAudioProcessor::reset()
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

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    updateParameters();

    setLeftRightMeterRMSValues(buffer, mInputLeftSmoothedGlobal, mInputRightSmoothedGlobal);
//    mDryBuffer.makeCopyOf(buffer); // Keep a copy for the final global dry/wet mix.
    
    // At the start of the block, get the LFO values. For now, let's just get one value
    // for the whole block for simplicity. A more advanced implementation would
    // process this per-sample.
    float lfo1Value = lfoEngines[0].getNextSample(); // This gets the CURRENT LFO value
    // float lfo2Value = lfoEngines[1].getNextSample();
    // etc.
    
    // (This is a placeholder for the modulation system we will build next)
    // DBG("LFO 1 Value: " + juce::String(lfo1Value));

    // 1. GET PARAMETERS & SMOOTH FREQUENCIES
    int numBands = static_cast<int>(*treeState.getRawParameterValue(NUM_BANDS_ID));
    int lineNum = numBands - 1;

    // Set target values for smoothers from APVTS
    smoothedFreq1.setTargetValue(*treeState.getRawParameterValue(FREQ_ID1));
    smoothedFreq2.setTargetValue(*treeState.getRawParameterValue(FREQ_ID2));
    smoothedFreq3.setTargetValue(*treeState.getRawParameterValue(FREQ_ID3));

    // Get the smoothed frequency values for this block
    // And also handle sorting based on active lines
    std::array<float, 3> freqArray = { 0.0f, 0.0f, 0.0f };
    int activeLineCount = 0;
    if (static_cast<bool>(*treeState.getRawParameterValue(LINE_STATE_ID1)))
        freqArray[activeLineCount++] = smoothedFreq1.getNextValue();
    if (static_cast<bool>(*treeState.getRawParameterValue(LINE_STATE_ID2)))
        freqArray[activeLineCount++] = smoothedFreq2.getNextValue();
    if (static_cast<bool>(*treeState.getRawParameterValue(LINE_STATE_ID3)))
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

    splitBands(buffer, sampleRate);

    delayMatchedDryBuffer.clear();

    // 4. PROCESS INDIVIDUAL BANDS
    
    // Create an array of pointers to the clean, split buffers.
    std::array<juce::AudioBuffer<float>*, 4> dryBandBuffers = { &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4 };
    // Create an array of pointers to the wet, split buffers.
    std::array<juce::AudioBuffer<float>*, 4> wetBandBuffers = { &mBuffer1, &mBuffer2, &mBuffer3, &mBuffer4 };
    
    // Call sumBands to sum the clean signals into our new dry buffer.
    // Note: We might want a version of sumBands that ignores solo for this,
    // but for now this works.
    sumBands(delayMatchedDryBuffer, dryBandBuffers, true);

    for (int i = 0; i < numBands; ++i)
    {
        // Get a pointer to the current band's processor.
        if (auto* band = bands[i].get())
        {
            // First, set the input meter values for this band.
            setLeftRightMeterRMSValues(*dryBandBuffers[i], band->mInputLeftSmoothed, band->mInputRightSmoothed);

            if (*treeState.getRawParameterValue(ParameterID::bandEnableIds[i]))
            {
                // ** THIS IS THE CENTRAL CHANGE **
                // 1. Assemble the parameter struct before calling process.
                BandProcessingParameters params;
                
                // Populate the struct from treeState
                params.mode = *treeState.getRawParameterValue(ParameterID::modeIds[i]);
                params.isHQ = *treeState.getRawParameterValue(HQ_ID);
                params.outputVal = *treeState.getRawParameterValue(ParameterID::outputIds[i]);
                params.mixVal = *treeState.getRawParameterValue(ParameterID::mixIds[i]);
                params.compThreshold = *treeState.getRawParameterValue(ParameterID::compThreshIds[i]);
                params.compRatio = *treeState.getRawParameterValue(ParameterID::compRatioIds[i]);
                params.isCompBypassed = *treeState.getRawParameterValue(ParameterID::compBypassIds[i]) > 0.5f;
                params.width = *treeState.getRawParameterValue(ParameterID::widthIds[i]);
                params.isWidthBypassed = *treeState.getRawParameterValue(ParameterID::widthBypassIds[i]) > 0.5f;
                params.isSafeModeOn = *treeState.getRawParameterValue(ParameterID::safeIds[i]) > 0.5f;
                params.isExtremeModeOn = *treeState.getRawParameterValue(ParameterID::extremeIds[i]) > 0.5f;
                params.biasVal = *treeState.getRawParameterValue(ParameterID::biasIds[i]);
                // Note: We still update the smoothed values' targets in each block
                band->drive.setTargetValue(*treeState.getRawParameterValue(ParameterID::driveIds[i]));
                band->rec.setTargetValue(*treeState.getRawParameterValue(ParameterID::recIds[i]));

                params.driveVal = band->drive.getNextValue();
                params.recVal = band->rec.getNextValue();

                // 2. Call the refactored, decoupled process method
                band->process(*wetBandBuffers[i], params);
            }

            // After processing, set the output meter values for this band.
            setLeftRightMeterRMSValues(*wetBandBuffers[i], band->mOutputLeftSmoothed, band->mOutputRightSmoothed);
        }
    }

    sumBands(buffer, wetBandBuffers, false);

    // 5. PROCESS GLOBAL BAND
    if (*treeState.getRawParameterValue(FILTER_BYPASS_ID))
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
    processGain(juce::dsp::ProcessContextReplacing<float>(globalBlock), OUTPUT_ID, gainProcessorGlobal);
    
    float mixValue;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        mixValue = globalMixSmoother.getNextValue();
    }
    dryWetMixerGlobal.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    if (*treeState.getRawParameterValue(HQ_ID))
    {
        dryWetMixerGlobal.setWetLatency(totalLatency);
    }
    else
    {
        dryWetMixerGlobal.setWetLatency(0);
    }
    dryWetMixerGlobal.setWetMixProportion(mixValue);
    dryWetMixerGlobal.pushDrySamples(juce::dsp::AudioBlock<float>(delayMatchedDryBuffer));
    dryWetMixerGlobal.mixWetSamples(juce::dsp::AudioBlock<float>(buffer));
    
//    auto* wetBufferPtr = &buffer;
//    auto* dryBufferPtr = &delayMatchedDryBuffer;
//    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
//    {
//        float mixValue = globalMixSmoother.getNextValue();
//
//        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//        {
//            const float wetSample = wetBufferPtr->getSample(channel, sample);
//            const float drySample = dryBufferPtr->getSample(channel, sample);
//            const float mixedSample = drySample + (wetSample - drySample) * mixValue;
//
//            wetBufferPtr->setSample(channel, sample, mixedSample);
//        }
//    }

    mWetBuffer.makeCopyOf(buffer);
    pushDataToFFT(mWetBuffer, processedSpecProcessor);
    pushDataToFFT(delayMatchedDryBuffer, originalSpecProcessor);
    setLeftRightMeterRMSValues(buffer, mOutputLeftSmoothedGlobal, mOutputRightSmoothedGlobal);
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
    
    chainSettings.lowCutFreq  = juce::jlimit(minFreq, maxFreq, chainSettings.lowCutFreq);
    chainSettings.peakFreq    = juce::jlimit(minFreq, maxFreq, chainSettings.peakFreq);
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
    const float nyquist = (float)sampleRate / 2.0f;
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

// VU meters
void FireAudioProcessor::setLeftRightMeterRMSValues(juce::AudioBuffer<float> buffer, float& leftOutValue, float& rightOutValue)
{
    float absInputLeftValue;
    float absInputRightValue;
    if (getTotalNumInputChannels() == 2)
    {
        absInputLeftValue = fabs(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
        absInputRightValue = fabs(buffer.getRMSLevel(1, 0, buffer.getNumSamples()));
    }
    else
    {
        absInputLeftValue = fabs(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
        absInputRightValue = absInputLeftValue;
    }
    // smooth value
    leftOutValue = SMOOTH_COEFF * (leftOutValue - absInputLeftValue) + absInputLeftValue;
    rightOutValue = SMOOTH_COEFF * (rightOutValue - absInputRightValue) + absInputRightValue;
}

float FireAudioProcessor::getMeterRMSLevel(bool isInput, int channel, int bandIndex)
{
    // A bandIndex of -1 is a convention we'll use to signify the global meters.
    if (bandIndex == -1)
    {
        if (isInput)
        {
            // For global input, use the dedicated global smoothed values.
            return dBToNormalizedGain(channel == 0 ? mInputLeftSmoothedGlobal : mInputRightSmoothedGlobal);
        }
        else // isOutput
        {
            return dBToNormalizedGain(channel == 0 ? mOutputLeftSmoothedGlobal : mOutputRightSmoothedGlobal);
        }
    }

    // For per-band meters, we access the data inside the correct BandProcessor instance.
    // juce::isPositiveAndBelow is a safe way to check if the index is valid for our vector.
    if (juce::isPositiveAndBelow(bandIndex, bands.size()))
    {
        if (auto* band = bands[bandIndex].get())
        {
            if (isInput)
            {
                // Get the value directly from the BandProcessor member.
                return dBToNormalizedGain(channel == 0 ? band->mInputLeftSmoothed : band->mInputRightSmoothed);
            }
            else // isOutput
            {
                return dBToNormalizedGain(channel == 0 ? band->mOutputLeftSmoothed : band->mOutputRightSmoothed);
            }
        }
    }

    // If the bandIndex is invalid for any reason, assert in debug builds and return 0.
    jassertfalse;
    return 0.0f;
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
        
    // An array for each set of NAME macros. This allows us to use a loop below.
    const char* const MODE_NAMES[]        = { MODE_NAME1, MODE_NAME2, MODE_NAME3, MODE_NAME4 };
    const char* const LINKED_NAMES[]      = { LINKED_NAME1, LINKED_NAME2, LINKED_NAME3, LINKED_NAME4 };
    const char* const SAFE_NAMES[]        = { SAFE_NAME1, SAFE_NAME2, SAFE_NAME3, SAFE_NAME4 };
    const char* const EXTREME_NAMES[]     = { EXTREME_NAME1, EXTREME_NAME2, EXTREME_NAME3, EXTREME_NAME4 };
    const char* const DRIVE_NAMES[]       = { DRIVE_NAME1, DRIVE_NAME2, DRIVE_NAME3, DRIVE_NAME4 };
    const char* const COMP_RATIO_NAMES[]  = { COMP_RATIO_NAME1, COMP_RATIO_NAME2, COMP_RATIO_NAME3, COMP_RATIO_NAME4 };
    const char* const COMP_THRESH_NAMES[] = { COMP_THRESH_NAME1, COMP_THRESH_NAME2, COMP_THRESH_NAME3, COMP_THRESH_NAME4 };
    const char* const WIDTH_NAMES[]       = { WIDTH_NAME1, WIDTH_NAME2, WIDTH_NAME3, WIDTH_NAME4 };
    const char* const OUTPUT_NAMES[]      = { OUTPUT_NAME1, OUTPUT_NAME2, OUTPUT_NAME3, OUTPUT_NAME4 };
    const char* const MIX_NAMES[]         = { MIX_NAME1, MIX_NAME2, MIX_NAME3, MIX_NAME4 };
    const char* const BIAS_NAMES[]        = { BIAS_NAME1, BIAS_NAME2, BIAS_NAME3, BIAS_NAME4 };
    const char* const REC_NAMES[]         = { REC_NAME1, REC_NAME2, REC_NAME3, REC_NAME4 };
    const char* const BAND_ENABLE_NAMES[] = { BAND_ENABLE_NAME1, BAND_ENABLE_NAME2, BAND_ENABLE_NAME3, BAND_ENABLE_NAME4 };
    const char* const BAND_SOLO_NAMES[]   = { BAND_SOLO_NAME1, BAND_SOLO_NAME2, BAND_SOLO_NAME3, BAND_SOLO_NAME4 };
    const char* const COMP_BYPASS_NAMES[] = { COMP_BYPASS_NAME1, COMP_BYPASS_NAME2, COMP_BYPASS_NAME3, COMP_BYPASS_NAME4 };
    const char* const WIDTH_BYPASS_NAMES[]= { WIDTH_BYPASS_NAME1, WIDTH_BYPASS_NAME2, WIDTH_BYPASS_NAME3, WIDTH_BYPASS_NAME4 };
    
    using PBool = juce::AudioParameterBool;
    using PInt = juce::AudioParameterInt;
    using PFloat = juce::AudioParameterFloat;

    // --- Global Parameters ---
    parameters.push_back(std::make_unique<PBool>(ParameterID::hq, HQ_NAME, false));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::globalOutput, OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::globalMix, MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterID::numBands, NUM_BANDS_NAME, 1, 4, 1));
    parameters.push_back(std::make_unique<PBool>(ParameterID::filterBypass, FILTER_BYPASS_NAME, false));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::downsample, DOWNSAMPLE_NAME, juce::NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PBool>(ParameterID::downsampleBypass, DOWNSAMPLE_BYPASS_NAME, false));

    // --- Per-Band Parameters (created in a loop) ---
    for (int i = 0; i < 4; ++i)
    {
        parameters.push_back(std::make_unique<PInt>  (ParameterID::mode(i),       MODE_NAMES[i], 0, 11, 3));
        parameters.push_back(std::make_unique<PBool> (ParameterID::linked(i),     LINKED_NAMES[i], true));
        parameters.push_back(std::make_unique<PBool> (ParameterID::safe(i),       SAFE_NAMES[i], true));
        parameters.push_back(std::make_unique<PBool> (ParameterID::extreme(i),    EXTREME_NAMES[i], false));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::drive(i),      DRIVE_NAMES[i], juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::compRatio(i),  COMP_RATIO_NAMES[i], juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::compThresh(i), COMP_THRESH_NAMES[i], juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::width(i),      WIDTH_NAMES[i], juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::output(i),     OUTPUT_NAMES[i], juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::mix(i),        MIX_NAMES[i], juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::bias(i),       BIAS_NAMES[i], juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PFloat>(ParameterID::rec(i),        REC_NAMES[i], juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        parameters.push_back(std::make_unique<PBool> (ParameterID::bandEnable(i), BAND_ENABLE_NAMES[i], true));
        parameters.push_back(std::make_unique<PBool> (ParameterID::bandSolo(i),   BAND_SOLO_NAMES[i], false));
        parameters.push_back(std::make_unique<PBool> (ParameterID::compBypass(i), COMP_BYPASS_NAMES[i], false));
        parameters.push_back(std::make_unique<PBool> (ParameterID::widthBypass(i),WIDTH_BYPASS_NAMES[i], false));
    }
    
    // --- Crossover Parameters ---
    juce::NormalisableRange<float> freqRange(40.0f, 10024.0f, 1.0f);
    freqRange.setSkewForCentre(651.0f);
    parameters.push_back(std::make_unique<PFloat>(ParameterID::freq1, FREQ_NAME1, freqRange, 21.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::freq2, FREQ_NAME2, freqRange, 21.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::freq3, FREQ_NAME3, freqRange, 21.0f));
    parameters.push_back(std::make_unique<PBool>(ParameterID::lineState1, LINE_STATE_NAME1, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::lineState2, LINE_STATE_NAME2, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::lineState3, LINE_STATE_NAME3, false));
    
    // --- Global Filter Parameters ---
    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.0f);
    parameters.push_back(std::make_unique<PFloat>(ParameterID::lowCutFreq, LOWCUT_FREQ_NAME, cutoffRange, 20.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::lowCutQ, LOWCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::lowCutGain, LOWCUT_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::highCutFreq, HIGHCUT_FREQ_NAME, cutoffRange, 20000.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::highCutQ, HIGHCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::highCutGain, HIGHCUT_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::peakFreq, PEAK_FREQ_NAME, cutoffRange, 1000.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::peakQ, PEAK_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(ParameterID::peakGain, PEAK_GAIN_NAME, juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PInt>(ParameterID::lowCutSlope, LOWCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<PInt>(ParameterID::highCutSlope, HIGHCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<PBool>(ParameterID::lowCutBypassed, LOWCUT_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::peakBypassed, PEAK_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::highCutBypassed, HIGHCUT_BYPASSED_NAME, false));

    parameters.push_back(std::make_unique<PBool>(ParameterID::off, OFF_NAME, true));
    parameters.push_back(std::make_unique<PBool>(ParameterID::pre, PRE_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::post, POST_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::low, LOW_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::band, BAND_NAME, false));
    parameters.push_back(std::make_unique<PBool>(ParameterID::high, HIGH_NAME, true));

    return { parameters.begin(), parameters.end() };
}

//juce::AudioProcessorValueTreeState& FireAudioProcessor::getTreeState()
//{
//    return treeState;
//}

void FireAudioProcessor::updateParameters()
{
    //==============================================================================
    // 1. Update Global and Crossover Parameters
    //==============================================================================

    // Get the number of active bands for processing loops.
    numBands = *treeState.getRawParameterValue(NUM_BANDS_ID);
    activeCrossovers = numBands - 1;

    // Update smoothers for crossover frequencies.
    smoothedFreq1.setTargetValue(*treeState.getRawParameterValue(FREQ_ID1));
    smoothedFreq2.setTargetValue(*treeState.getRawParameterValue(FREQ_ID2));
    smoothedFreq3.setTargetValue(*treeState.getRawParameterValue(FREQ_ID3));
    
    // Update global output and mix smoothers.
    globalMixSmoother.setTargetValue(*treeState.getRawParameterValue(MIX_ID));
    
    //==============================================================================
    // 2. Update Per-Band Smoothed Parameters
    //==============================================================================
    
    // Iterate through the bands vector to update each BandProcessor's smoothers.
    for (int i = 0; i < 4; ++i)
    {
        if (auto* band = bands[i].get())
        {
            band->drive.setTargetValue(*treeState.getRawParameterValue(ParameterID::driveIds[i]));
            band->rec.setTargetValue(*treeState.getRawParameterValue(ParameterID::recIds[i]));
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
    if (!ignoreSoloLogic)
    {
        for (int i = 0; i < numBands; ++i)
        {
            if (*treeState.getRawParameterValue(ParameterID::bandSoloIds[i]))
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

        const bool isThisBandSoloed = *treeState.getRawParameterValue(ParameterID::bandSoloIds[i]) > 0.5f;

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
            float centerFreq = std::sqrt(f1 * f2);
            float Q = std::sqrt(k) / (k - 1.0f);
            float gain = (1.0f + k * k) * (1.0f + k * k) / (k * k * k * k);

            *compensatorEQ1.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerFreq, Q, gain);
            auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
            auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
            compensatorEQ1.process(context2);
        }
    }
    // For Band 3
    if (lineNum > 2)
    {
        float f1 = freqValue2, f2 = freqValue3;
        if (f1 > 20.0f && f2 > f1 && (f2 / f1) < ratioThreshold)
        {
            float k = f2 / f1;
            float centerFreq = std::sqrt(f1 * f2);
            float Q = std::sqrt(k) / (k - 1.0f);
            float gain = (1.0f + k * k) * (1.0f + k * k) / (k * k * k * k);

            *compensatorEQ2.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerFreq, Q, gain);
            auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
            auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
            compensatorEQ2.process(context3);
        }
    }
}

void FireAudioProcessor::updateGlobalFilters(double sampleRate)
{
    // This is your old `updateFilter` function.
    auto chainSettings = getChainSettings(treeState);
    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;
    if (sampleRate <= 0)
        return;

    if (maxFreq < minFreq)
    {
        maxFreq = minFreq;
    }
    
    chainSettings.lowCutFreq  = juce::jlimit(minFreq, maxFreq, chainSettings.lowCutFreq);
    chainSettings.peakFreq    = juce::jlimit(minFreq, maxFreq, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.highCutFreq);
    
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
    const float nyquist = (float)sampleRate / 2.0f;
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

float FireAudioProcessor::getTotalLatency() const
{
    return totalLatency;
}
