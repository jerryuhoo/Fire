/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    // fix the artifacts (also called zipper noise)
    previousOutput1 = (float) *treeState.getRawParameterValue(OUTPUT_ID1);
    previousOutput1 = juce::Decibels::decibelsToGain(previousOutput1);
    previousOutput2 = (float) *treeState.getRawParameterValue(OUTPUT_ID2);
    previousOutput2 = juce::Decibels::decibelsToGain(previousOutput2);
    previousOutput3 = (float) *treeState.getRawParameterValue(OUTPUT_ID3);
    previousOutput3 = juce::Decibels::decibelsToGain(previousOutput3);
    previousOutput4 = (float) *treeState.getRawParameterValue(OUTPUT_ID4);
    previousOutput4 = juce::Decibels::decibelsToGain(previousOutput4);
    previousOutput = juce::Decibels::decibelsToGain((float) *treeState.getRawParameterValue(OUTPUT_ID));

    previousDrive1 = *treeState.getRawParameterValue(DRIVE_ID1);
    previousDrive2 = *treeState.getRawParameterValue(DRIVE_ID2);
    previousDrive3 = *treeState.getRawParameterValue(DRIVE_ID3);
    previousDrive4 = *treeState.getRawParameterValue(DRIVE_ID4);

    previousMix1 = (float) *treeState.getRawParameterValue(MIX_ID1);
    previousMix2 = (float) *treeState.getRawParameterValue(MIX_ID2);
    previousMix3 = (float) *treeState.getRawParameterValue(MIX_ID3);
    previousMix4 = (float) *treeState.getRawParameterValue(MIX_ID4);
    previousMix = (float) *treeState.getRawParameterValue(MIX_ID);

    previousLowcutFreq = (float) *treeState.getRawParameterValue(LOWCUT_FREQ_ID);
    previousHighcutFreq = (float) *treeState.getRawParameterValue(HIGHCUT_FREQ_ID);
    previousPeakFreq = (float) *treeState.getRawParameterValue(PEAK_FREQ_ID);

    driveSmoother1.reset(sampleRate, 0.05); //0.05 second is rampLength, which means increasing to targetvalue needs 0.05s.
    driveSmoother1.setCurrentAndTargetValue(previousDrive1);
    driveSmoother2.reset(sampleRate, 0.05);
    driveSmoother2.setCurrentAndTargetValue(previousDrive2);
    driveSmoother3.reset(sampleRate, 0.05);
    driveSmoother3.setCurrentAndTargetValue(previousDrive3);
    driveSmoother4.reset(sampleRate, 0.05);
    driveSmoother4.setCurrentAndTargetValue(previousDrive4);

    outputSmoother1.reset(sampleRate, 0.05);
    outputSmoother1.setCurrentAndTargetValue(previousOutput1);
    outputSmoother2.reset(sampleRate, 0.05);
    outputSmoother2.setCurrentAndTargetValue(previousOutput2);
    outputSmoother3.reset(sampleRate, 0.05);
    outputSmoother3.setCurrentAndTargetValue(previousOutput3);
    outputSmoother4.reset(sampleRate, 0.05);
    outputSmoother4.setCurrentAndTargetValue(previousOutput4);
    outputSmootherGlobal.reset(sampleRate, 0.05);
    outputSmootherGlobal.setCurrentAndTargetValue(previousOutput);

    lowcutFreqSmoother.reset(sampleRate, 0.001);
    lowcutFreqSmoother.setCurrentAndTargetValue(previousLowcutFreq);
    highcutFreqSmoother.reset(sampleRate, 0.001);
    highcutFreqSmoother.setCurrentAndTargetValue(previousHighcutFreq);
    peakFreqSmoother.reset(sampleRate, 0.001);
    peakFreqSmoother.setCurrentAndTargetValue(previousPeakFreq);

    recSmoother1.reset(sampleRate, 0.05);
    recSmoother1.setCurrentAndTargetValue(*treeState.getRawParameterValue(REC_ID1));
    recSmoother2.reset(sampleRate, 0.05);
    recSmoother2.setCurrentAndTargetValue(*treeState.getRawParameterValue(REC_ID2));
    recSmoother3.reset(sampleRate, 0.05);
    recSmoother3.setCurrentAndTargetValue(*treeState.getRawParameterValue(REC_ID3));
    recSmoother4.reset(sampleRate, 0.05);
    recSmoother4.setCurrentAndTargetValue(*treeState.getRawParameterValue(REC_ID4));

    biasSmoother1.reset(sampleRate, 0.001);
    biasSmoother1.setCurrentAndTargetValue(*treeState.getRawParameterValue(BIAS_ID1));
    biasSmoother2.reset(sampleRate, 0.001);
    biasSmoother2.setCurrentAndTargetValue(*treeState.getRawParameterValue(BIAS_ID2));
    biasSmoother3.reset(sampleRate, 0.001);
    biasSmoother3.setCurrentAndTargetValue(*treeState.getRawParameterValue(BIAS_ID3));
    biasSmoother4.reset(sampleRate, 0.001);
    biasSmoother4.setCurrentAndTargetValue(*treeState.getRawParameterValue(BIAS_ID4));

    mixSmoother1.reset(sampleRate, 0.05);
    mixSmoother1.setCurrentAndTargetValue(previousMix1);
    mixSmoother2.reset(sampleRate, 0.05);
    mixSmoother2.setCurrentAndTargetValue(previousMix2);
    mixSmoother3.reset(sampleRate, 0.05);
    mixSmoother3.setCurrentAndTargetValue(previousMix3);
    mixSmoother4.reset(sampleRate, 0.05);
    mixSmoother4.setCurrentAndTargetValue(previousMix4);
    mixSmootherGlobal.reset(sampleRate, 0.05);
    mixSmootherGlobal.setCurrentAndTargetValue(previousMix);

    centralSmoother.reset(sampleRate, 0.1);
    centralSmoother.setCurrentAndTargetValue(0);

    normalSmoother.reset(sampleRate, 0.5);
    normalSmoother.setCurrentAndTargetValue(1);

    // historyArray init
    for (int i = 0; i < samplesPerBlock; i++)
    {
        historyArrayL.add(0);
        historyArrayR.add(0);
    }

    // dry wet buffer init
    mDryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    mDryBuffer.clear();

    mWetBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    mWetBuffer.clear();

    // oversampling init
    for (size_t i = 0; i < 4; i++)
    {
        oversamplingHQ[i]->reset();
        oversamplingHQ[i]->initProcessing(static_cast<size_t>(samplesPerBlock));
    }
    mDelay.reset(0);

    // dsp init

    //int newBlockSize = (int)oversampling->getOversamplingFactor() * samplesPerBlock;

    /*
    if (*treeState.getRawParameterValue("hq")) // oversampling
    {
        spec.sampleRate = sampleRate * 4;
    }
    else
    {
         spec.sampleRate = sampleRate;
    }
    */

    // filter init
    updateFilter();
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

    compensatorLP.prepare(spec);
    compensatorHP.prepare(spec);

    compensatorEQ1.prepare(spec);
    compensatorEQ2.prepare(spec);
    compensatorEQ3.prepare(spec);

    // limiter
    //    limiterProcessorGlobal.prepare(spec);

    // gain
    gainProcessor1.prepare(spec);
    gainProcessor2.prepare(spec);
    gainProcessor3.prepare(spec);
    gainProcessor4.prepare(spec);
    gainProcessorGlobal.prepare(spec);

    // compressor
    compressorProcessor1.prepare(spec);
    compressorProcessor1.setAttack(80.0f);
    compressorProcessor1.setRelease(200.0f);

    compressorProcessor2.prepare(spec);
    compressorProcessor2.setAttack(80.0f);
    compressorProcessor2.setRelease(200.0f);

    compressorProcessor3.prepare(spec);
    compressorProcessor3.setAttack(80.0f);
    compressorProcessor3.setRelease(200.0f);

    compressorProcessor4.prepare(spec);
    compressorProcessor4.setAttack(80.0f);
    compressorProcessor4.setRelease(200.0f);

    // dc filters
    dcFilter1.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0);
    dcFilter2.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0);
    dcFilter3.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0);
    dcFilter4.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0);

    // overdrive
    overdrive1.prepare(spec);
    overdrive2.prepare(spec);
    overdrive3.prepare(spec);
    overdrive4.prepare(spec);

    // dry wet
    dryWetMixerGlobal.prepare(spec);
    dryWetMixer1.prepare(spec);
    dryWetMixer2.prepare(spec);
    dryWetMixer3.prepare(spec);
    dryWetMixer4.prepare(spec);
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
    compressorProcessor1.reset();
    compressorProcessor2.reset();
    compressorProcessor3.reset();
    compressorProcessor4.reset();
    dryWetMixerGlobal.reset();
    dryWetMixer1.reset();
    dryWetMixer2.reset();
    dryWetMixer3.reset();
    dryWetMixer4.reset();
    //    limiterProcessorGlobal.reset();
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
    // set bypass to false

    if (isBypassed)
    {
        //        buffer.clear();
        isBypassed = false;
    }

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto sampleRate = getSampleRate();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    setLeftRightMeterRMSValues(buffer, mInputLeftSmoothedGlobal, mInputRightSmoothedGlobal);
    mDryBuffer.makeCopyOf(buffer); // Keep a copy for the final global dry/wet mix.

    // ======================== FINAL TREE-BASED ARCHITECTURE ========================

    // 1. GET PARAMETERS & SORT FREQUENCIES
    int numBands = static_cast<int>(*treeState.getRawParameterValue(NUM_BANDS_ID));
    int lineNum = numBands - 1;

    std::array<int, 3> freqArray = { 0, 0, 0 };
    int activeLineCount = 0;
    if (static_cast<bool>(*treeState.getRawParameterValue(LINE_STATE_ID1)))
        freqArray[activeLineCount++] = static_cast<int>(*treeState.getRawParameterValue(FREQ_ID1));
    if (static_cast<bool>(*treeState.getRawParameterValue(LINE_STATE_ID2)))
        freqArray[activeLineCount++] = static_cast<int>(*treeState.getRawParameterValue(FREQ_ID2));
    if (static_cast<bool>(*treeState.getRawParameterValue(LINE_STATE_ID3)))
        freqArray[activeLineCount++] = static_cast<int>(*treeState.getRawParameterValue(FREQ_ID3));
    std::sort(freqArray.begin(), freqArray.begin() + activeLineCount);

    int freqValue1 = freqArray[0];
    int freqValue2 = freqArray[1];
    int freqValue3 = freqArray[2];

    // 2. SET UP FILTERS
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

    // 3. PREPARE BUFFERS
    auto numSamples = buffer.getNumSamples();
    mBuffer1.setSize(totalNumOutputChannels, numSamples, false, false, true);
    mBuffer2.setSize(totalNumOutputChannels, numSamples, false, false, true);
    mBuffer3.setSize(totalNumOutputChannels, numSamples, false, false, true);
    mBuffer4.setSize(totalNumOutputChannels, numSamples, false, false, true);

    // 4. TREE-BASED SIGNAL SPLITTING
    if (lineNum == 0)
    { // 1 Band: No splitting
        mBuffer1.makeCopyOf(buffer);
    }
    else if (lineNum == 1)
    { // 2 Bands: One split
        mBuffer1.makeCopyOf(buffer);
        mBuffer2.makeCopyOf(buffer);

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

        mBuffer1.makeCopyOf(buffer);
        highPassBuffer.makeCopyOf(buffer);

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
        lowMidBuffer.makeCopyOf(buffer);
        highMidBuffer.makeCopyOf(buffer);

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

    // 5. ======== CALCULATE AND APPLY DYNAMIC COMPENSATION (THE FIX) ========
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
    if (lineNum == 3)
    {
        // This compensator is only for the problematic crossover at freqValue2.
        float centerFreq = freqValue2;
        float k = freqValue3 / freqValue1;
        float Q = std::sqrt(k) / (k - 1.0f) * 3.0f;
        float baseGain = (1.0f + k * k) * (1.0f + k * k) / (k * k * k * k);
        const float enhancedGain = 3.0f;
        float blendRangeStart = 11.0f;
        float blendRangeEnd = 1.0f;
        float t = (k - blendRangeStart) / (blendRangeEnd - blendRangeStart);
        t = juce::jlimit(0.0f, 1.0f, t);
        float blendFactor = t * t * (3.0f - 2.0f * t);
        float finalGain = (1.0f - blendFactor) * baseGain + blendFactor * enhancedGain;

        *compensatorEQ3.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerFreq, Q, finalGain);

        // Apply the same +3dB peak to both Band 2 and Band 3
        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        compensatorEQ3.process(context2);

        auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
        compensatorEQ3.process(context3);
    }

    // ======== 6. CREATE THE DELAY-COMPENSATED DRY SIGNAL (YOUR SOLUTION) ========
    // Before applying any processing (distortion, etc.), we sum the clean, split bands
    // to create a dry signal that has the exact same group delay as the wet path.
    juce::AudioBuffer<float> delayMatchedDryBuffer(totalNumOutputChannels, numSamples);
    delayMatchedDryBuffer.clear();

    for (int i = 0; i < numBands; ++i)
    {
        juce::AudioBuffer<float>* bandBuffer = nullptr;
        if (i == 0)
            bandBuffer = &mBuffer1;
        else if (i == 1)
            bandBuffer = &mBuffer2;
        else if (i == 2)
            bandBuffer = &mBuffer3;
        else if (i == 3)
            bandBuffer = &mBuffer4;

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
            delayMatchedDryBuffer.addFrom(channel, 0, *bandBuffer, channel, 0, numSamples);
    }

    // 6. PROCESS INDIVIDUAL BANDS
    multibandEnable1 = *treeState.getRawParameterValue(BAND_ENABLE_ID1);
    multibandEnable2 = *treeState.getRawParameterValue(BAND_ENABLE_ID2);
    multibandEnable3 = *treeState.getRawParameterValue(BAND_ENABLE_ID3);
    multibandEnable4 = *treeState.getRawParameterValue(BAND_ENABLE_ID4);

    if (multibandEnable1)
    {
        auto block1 = juce::dsp::AudioBlock<float>(mBuffer1);
        auto context1 = juce::dsp::ProcessContextReplacing<float>(block1);
        processOneBand(mBuffer1, context1, MODE_ID1, DRIVE_ID1, SAFE_ID1, EXTREME_ID1, BIAS_ID1, REC_ID1, overdrive1, OUTPUT_ID1, gainProcessor1, COMP_THRESH_ID1, COMP_RATIO_ID1, compressorProcessor1, totalNumInputChannels, recSmoother1, outputSmoother1, MIX_ID1, dryWetMixer1, WIDTH_ID1, widthProcessor1, dcFilter1, WIDTH_BYPASS_ID1, COMP_BYPASS_ID1);
    }
    if (lineNum >= 1 && multibandEnable2)
    {
        auto block2 = juce::dsp::AudioBlock<float>(mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float>(block2);
        processOneBand(mBuffer2, context2, MODE_ID2, DRIVE_ID2, SAFE_ID2, EXTREME_ID2, BIAS_ID2, REC_ID2, overdrive2, OUTPUT_ID2, gainProcessor2, COMP_THRESH_ID2, COMP_RATIO_ID2, compressorProcessor2, totalNumInputChannels, recSmoother2, outputSmoother2, MIX_ID2, dryWetMixer2, WIDTH_ID2, widthProcessor2, dcFilter2, WIDTH_BYPASS_ID2, COMP_BYPASS_ID2);
    }
    if (lineNum >= 2 && multibandEnable3)
    {
        auto block3 = juce::dsp::AudioBlock<float>(mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float>(block3);
        processOneBand(mBuffer3, context3, MODE_ID3, DRIVE_ID3, SAFE_ID3, EXTREME_ID3, BIAS_ID3, REC_ID3, overdrive3, OUTPUT_ID3, gainProcessor3, COMP_THRESH_ID3, COMP_RATIO_ID3, compressorProcessor3, totalNumInputChannels, recSmoother3, outputSmoother3, MIX_ID3, dryWetMixer3, WIDTH_ID3, widthProcessor3, dcFilter3, WIDTH_BYPASS_ID3, COMP_BYPASS_ID3);
    }
    if (lineNum >= 3 && multibandEnable4)
    {
        auto block4 = juce::dsp::AudioBlock<float>(mBuffer4);
        auto context4 = juce::dsp::ProcessContextReplacing<float>(block4);
        processOneBand(mBuffer4, context4, MODE_ID4, DRIVE_ID4, SAFE_ID4, EXTREME_ID4, BIAS_ID4, REC_ID4, overdrive4, OUTPUT_ID4, gainProcessor4, COMP_THRESH_ID4, COMP_RATIO_ID4, compressorProcessor4, totalNumInputChannels, recSmoother4, outputSmoother4, MIX_ID4, dryWetMixer4, WIDTH_ID4, widthProcessor4, dcFilter4, WIDTH_BYPASS_ID4, COMP_BYPASS_ID4);
    }

    // 6. SUM THE PROCESSED BANDS
    buffer.clear();
    multibandSolo1 = *treeState.getRawParameterValue(BAND_SOLO_ID1);
    multibandSolo2 = *treeState.getRawParameterValue(BAND_SOLO_ID2);
    multibandSolo3 = *treeState.getRawParameterValue(BAND_SOLO_ID3);
    multibandSolo4 = *treeState.getRawParameterValue(BAND_SOLO_ID4);

    for (int i = 0; i < numBands; ++i)
    {
        if (! shouldSetBlackMask(i))
        {
            juce::AudioBuffer<float>* bandBuffer = nullptr;
            if (i == 0)
                bandBuffer = &mBuffer1;
            else if (i == 1)
                bandBuffer = &mBuffer2;
            else if (i == 2)
                bandBuffer = &mBuffer3;
            else if (i == 3)
                bandBuffer = &mBuffer4;

            for (int channel = 0; channel < totalNumOutputChannels; ++channel)
                buffer.addFrom(channel, 0, *bandBuffer, channel, 0, numSamples);
        }
    }

    // 7. GLOBAL PROCESSING (Your existing logic, unchanged)
    juce::dsp::AudioBlock<float> globalBlock(buffer);

    // downsample
    if (*treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID))
    {
        int rateDivide = static_cast<int>(*treeState.getRawParameterValue(DOWNSAMPLE_ID));
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                if (rateDivide > 1)
                {
                    if (sample % rateDivide != 0)
                        channelData[sample] = channelData[sample - sample % rateDivide];
                }
            }
        }
    }

    // global filter
    if (*treeState.getRawParameterValue(FILTER_BYPASS_ID))
    {
        updateFilter();
        auto leftBlock = globalBlock.getSingleChannelBlock(0);
        auto rightBlock = globalBlock.getSingleChannelBlock(totalNumOutputChannels > 1 ? 1 : 0);
        leftChain.process(juce::dsp::ProcessContextReplacing<float>(leftBlock));
        rightChain.process(juce::dsp::ProcessContextReplacing<float>(rightBlock));
    }

    // global gain
    processGain(juce::dsp::ProcessContextReplacing<float>(globalBlock), OUTPUT_ID, gainProcessorGlobal);

    // Use the new, delay-matched dry buffer for the mix, NOT the original mDryBuffer.
    mixDryWet(delayMatchedDryBuffer, buffer, MIX_ID, dryWetMixerGlobal, mLatency);

    // 8. FINAL OUTPUT STAGES
    // Spectrum
    mWetBuffer.makeCopyOf(buffer);
    pushDataToFFT(mWetBuffer, processedSpecProcessor);
    pushDataToFFT(mDryBuffer, originalSpecProcessor);

    // VU output meter
    setLeftRightMeterRMSValues(buffer, mOutputLeftSmoothedGlobal, mOutputRightSmoothedGlobal);

    mDryBuffer.clear();
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

void FireAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings)
{
    auto peakCoefficients = makePeakFilter(chainSettings, getSampleRate());

    leftChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    rightChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void FireAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings)
{
    auto cutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();

    auto lowcutQCoefficients = makeLowcutQFilter(chainSettings, getSampleRate());

    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    leftChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);

    updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(leftLowCut, cutCoefficients, chainSettings.lowCutSlope);

    updateCoefficients(leftChain.get<ChainPositions::LowCutQ>().coefficients, lowcutQCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::LowCutQ>().coefficients, lowcutQCoefficients);
}

void FireAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings)
{
    auto highCutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

    auto highcutQCoefficients = makeHighcutQFilter(chainSettings, getSampleRate());

    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);

    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);

    updateCoefficients(leftChain.get<ChainPositions::HighCutQ>().coefficients, highcutQCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::HighCutQ>().coefficients, highcutQCoefficients);
}

void FireAudioProcessor::updateFilter()
{
    auto chainSettings = getChainSettings(treeState);
    updateLowCutFilters(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighCutFilters(chainSettings);
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
    for (int channel = 0; channel < getTotalNumOutputChannels(); ++channel)
    {
        int bufferSamples = 0;
        float* channelData;
        if (bandIndex == 0)
        {
            channelData = mBuffer1.getWritePointer(channel);
            bufferSamples = mBuffer1.getNumSamples();
        }
        else if (bandIndex == 1)
        {
            channelData = mBuffer2.getWritePointer(channel);
            bufferSamples = mBuffer2.getNumSamples();
        }
        else if (bandIndex == 2)
        {
            channelData = mBuffer3.getWritePointer(channel);
            bufferSamples = mBuffer3.getNumSamples();
        }
        else if (bandIndex == 3)
        {
            channelData = mBuffer4.getWritePointer(channel);
            bufferSamples = mBuffer4.getNumSamples();
        }
        else
        {
            channelData = mWetBuffer.getWritePointer(channel);
            bufferSamples = mWetBuffer.getNumSamples();
        }

        for (int sample = 0; sample < bufferSamples; ++sample)
        {
            // mDelay is delayed clean signal
            if (sample % 10 == 0)
            {
                if (channel == 0)
                {
                    historyArrayL.add(channelData[sample]);
                    if (historyArrayL.size() > historyLength)
                    {
                        historyArrayL.remove(0);
                    }
                }
                else if (channel == 1)
                {
                    historyArrayR.add(channelData[sample]);
                    if (historyArrayR.size() > historyLength)
                    {
                        historyArrayR.remove(0);
                    }
                }
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

float FireAudioProcessor::safeMode(float drive, juce::AudioBuffer<float>& buffer, juce::String safeID)
{
    // protection
    drive = drive * 6.5f / 100.0f;
    float powerDrive = powf(2, drive);

    float sampleMaxValue = 0;
    sampleMaxValue = buffer.getMagnitude(0, buffer.getNumSamples());

    bool isSafeModeOn = *treeState.getRawParameterValue(safeID);

    float newDrive = 0.0f;
    if (isSafeModeOn && sampleMaxValue * powerDrive > 2.0f)
    {
        newDrive = 2.0f / sampleMaxValue + 0.1 * std::log2f(powerDrive);
        //newDrive = 2.0f / sampleMaxValue + 0.1 * drive;
    }
    else
    {
        newDrive = powerDrive;
    }

    if (drive == 0 || sampleMaxValue <= 0.001f)
    {
        setReductionPrecent(safeID, 1);
    }
    else
    {
        setReductionPrecent(safeID, std::log2f(newDrive) / drive);
    }
    setSampleMaxValue(safeID, sampleMaxValue);

    return newDrive;
}

bool FireAudioProcessor::shouldSetBlackMask(int index)
{
    // Get the total number of active BANDS directly from the treeState.
    int numBands = static_cast<int>(*treeState.getRawParameterValue(NUM_BANDS_ID));

    bool selfBandIsOn = getSoloStateFromIndex(index);
    bool otherBandSoloIsOn = false;

    // The loop should iterate up to the number of active BANDS.
    for (int i = 0; i < numBands; i++)
    {
        if (i == index)
            continue;

        if (getSoloStateFromIndex(i))
        {
            otherBandSoloIsOn = true;
            break;
        }
    }
    return (! selfBandIsOn && otherBandSoloIsOn);
}

bool FireAudioProcessor::getSoloStateFromIndex(int index)
{
    if (index == 0)
        return *treeState.getRawParameterValue(BAND_SOLO_ID1);
    else if (index == 1)
        return *treeState.getRawParameterValue(BAND_SOLO_ID2);
    else if (index == 2)
        return *treeState.getRawParameterValue(BAND_SOLO_ID3);
    else if (index == 3)
        return *treeState.getRawParameterValue(BAND_SOLO_ID4);
    else
        jassertfalse;
    return false;
}

void FireAudioProcessor::processOneBand(juce::AudioBuffer<float>& bandBuffer, juce::dsp::ProcessContextReplacing<float> context, juce::String modeID, juce::String driveID, juce::String safeID, juce::String extremeID, juce::String biasID, juce::String recID, juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float(float)>>, BiasProcessor>& overdrive, juce::String outputID, GainProcessor& gainProcessor, juce::String threshID, juce::String ratioID, CompressorProcessor& compressorProcessor, int totalNumInputChannels, juce::SmoothedValue<float>& recSmoother, juce::SmoothedValue<float>& outputSmoother, juce::String mixID, juce::dsp::DryWetMixer<float>& dryWetMixer, juce::String widthID, WidthProcessor widthProcessor, DCFilter& dcFilter, juce::String widthBypassID, juce::String compBypassID)
{
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(bandBuffer);

    // distortion process
    processDistortion(bandBuffer, modeID, driveID, safeID, extremeID, biasID, recID, overdrive, dcFilter);

    // normalize wave center position
    normalize(modeID, bandBuffer, totalNumInputChannels, recSmoother, outputSmoother1);

    // width process
    if (*treeState.getRawParameterValue(widthBypassID) && totalNumInputChannels == 2)
    {
        float* channeldataL;
        float* channeldataR;
        float width = *treeState.getRawParameterValue(widthID);
        channeldataL = bandBuffer.getWritePointer(0);
        channeldataR = bandBuffer.getWritePointer(1);
        widthProcessor.process(channeldataL, channeldataR, width, bandBuffer.getNumSamples());
    }

    // compressor process
    if (*treeState.getRawParameterValue(compBypassID))
        processCompressor(context, threshID, ratioID, compressorProcessor);

    // gain process
    processGain(context, outputID, gainProcessor);

    // mix process
    mixDryWet(dryBuffer, bandBuffer, mixID, dryWetMixer, mLatency);
}

void FireAudioProcessor::processDistortion(juce::AudioBuffer<float>& bandBuffer, juce::String modeID, juce::String driveID, juce::String safeID, juce::String extremeID, juce::String biasID, juce::String recID, juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float(float)>>, BiasProcessor>& overdrive, DCFilter& dcFilter)
{
    // oversampling
    juce::dsp::AudioBlock<float> blockInput(bandBuffer);
    juce::dsp::AudioBlock<float> blockOutput;
    int num;
    if (modeID == MODE_ID1)
        num = 0;
    else if (modeID == MODE_ID2)
        num = 1;
    else if (modeID == MODE_ID3)
        num = 2;
    else if (modeID == MODE_ID4)
        num = 3;
    else
        num = -1;
    if (*treeState.getRawParameterValue(HQ_ID))
    {
        blockInput = blockInput.getSubBlock(0, bandBuffer.getNumSamples());
        blockOutput = oversamplingHQ[num]->processSamplesUp(blockInput);

        // use float is more accurate and solved the +1 problem caused by int
        mLatency = oversamplingHQ[num]->getLatencyInSamples();

        // report latency to the host
        setLatencySamples(juce::roundToInt(mLatency));
    }
    else
    {
        blockOutput = blockInput.getSubBlock(0, bandBuffer.getNumSamples());
        mLatency = 0;
        setLatencySamples(0);
    }

    // band process
    auto context = juce::dsp::ProcessContextReplacing<float>(blockOutput);

    // get parameters from sliders
    int mode = static_cast<int>(*treeState.getRawParameterValue(modeID));
    float driveValue = static_cast<float>(*treeState.getRawParameterValue(driveID));
    float biasValue = static_cast<float>(*treeState.getRawParameterValue(biasID));
    float recValue = static_cast<float>(*treeState.getRawParameterValue(recID));

    if (static_cast<bool>(*treeState.getRawParameterValue(extremeID)))
    {
        driveValue = log2(10.0f) * driveValue;
    }
    float newDrive = safeMode(driveValue, bandBuffer, safeID);

    if (driveID == DRIVE_ID1)
        newDrive1 = newDrive;
    else if (driveID == DRIVE_ID2)
        newDrive2 = newDrive;
    else if (driveID == DRIVE_ID3)
        newDrive3 = newDrive;
    else if (driveID == DRIVE_ID4)
        newDrive4 = newDrive;

    auto& gainUp = overdrive.get<0>();
    gainUp.setGainLinear(newDrive);
    gainUp.setRampDurationSeconds(0.05f);

    float maxValue;
    if (getTotalNumInputChannels() == 2)
    {
        maxValue = juce::jmax(bandBuffer.getRMSLevel(0, 0, bandBuffer.getNumSamples()), bandBuffer.getRMSLevel(1, 0, bandBuffer.getNumSamples()));
    }
    else
    {
        maxValue = bandBuffer.getRMSLevel(0, 0, bandBuffer.getNumSamples());
    }

    if (maxValue < 0.000001f)
    {
        biasValue = 0;
    }

    auto& bias = overdrive.get<1>();
    bias.setBias(biasValue); // -1,1
    bias.setRampDurationSeconds(0.05f);

    auto& waveShaper = overdrive.get<2>();

    switch (mode)
    {
        case 0:
            waveShaper.functionToUse = waveshaping::arctanSoftClipping;
            break;
        case 1:
            waveShaper.functionToUse = waveshaping::expSoftClipping;
            break;
        case 2:
            waveShaper.functionToUse = waveshaping::tanhSoftClipping;
            break;
        case 3:
            waveShaper.functionToUse = waveshaping::cubicSoftClipping;
            break;
        case 4:
            waveShaper.functionToUse = waveshaping::hardClipping;
            break;
        case 5:
            waveShaper.functionToUse = waveshaping::sausageFattener;
            break;
        case 6:
            waveShaper.functionToUse = waveshaping::sinFoldback;
            break;
        case 7:
            waveShaper.functionToUse = waveshaping::linFoldback;
            break;
        case 8:
            waveShaper.functionToUse = waveshaping::limitClip;
            break;
        case 9:
            waveShaper.functionToUse = waveshaping::singleSinClip;
            break;
        case 10:
            waveShaper.functionToUse = waveshaping::logicClip;
            break;
        case 11:
            waveShaper.functionToUse = waveshaping::tanclip;
            break;
    }

    auto& waveShaper2 = overdrive.get<3>();

    waveShaper2.functionToUse = [recValue](float input)
    {
        if (input < 0)
        {
            input *= (0.5f - recValue) * 2.0f;
        }
        return input;
    };

    auto& bias2 = overdrive.get<4>();
    bias2.setBias(-biasValue); // -1,1
    bias2.setRampDurationSeconds(0.05f);

    // ======================== THE FIX ========================
    // Only process the overdrive chain if distortion is actually being applied.
    // This avoids phase coloration when controls are at their neutral/zero state.
    if (driveValue > 0.0f || std::abs(biasValue) > 0.000001f || recValue > 0.0f)
    {
        overdrive.process(context);
    }
    // =========================================================

    // oversampling
    if (*treeState.getRawParameterValue(HQ_ID)) // oversampling
    {
        oversamplingHQ[num]->processSamplesDown(blockInput);
    }

    dcFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), 20.0);
}

void FireAudioProcessor::processCompressor(juce::dsp::ProcessContextReplacing<float> context, juce::String threshID, juce::String ratioID, CompressorProcessor& compressor)
{
    float ratio = *treeState.getRawParameterValue(ratioID);
    float thresh = *treeState.getRawParameterValue(threshID);
    compressor.setThreshold(thresh);
    compressor.setRatio(ratio);
    compressor.process(context);
}

//void FireAudioProcessor::processLimiter (juce::dsp::ProcessContextReplacing<float> context, juce::String limiterThreshID, juce::String limiterReleaseID, LimiterProcessor& limiterProcessor)
//{
//    float limiterThreshValue = *treeState.getRawParameterValue (limiterThreshID);
//    float limiterReleaseValue = *treeState.getRawParameterValue (limiterReleaseID);
//    limiterProcessor.setThreshold(-limiterThreshValue);
//    limiterProcessor.setRelease(limiterReleaseValue);
//    limiterProcessor.process (context);
//}

void FireAudioProcessor::processGain(juce::dsp::ProcessContextReplacing<float> context, juce::String outputID, GainProcessor& gainProcessor)
{
    float outputValue = *treeState.getRawParameterValue(outputID);
    gainProcessor.setGainDecibels(outputValue);
    gainProcessor.setRampDurationSeconds(0.05f);
    gainProcessor.process(context);
}

void FireAudioProcessor::mixDryWet(juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer, juce::String mixID, juce::dsp::DryWetMixer<float>& dryWetMixer, float latency)
{
    float mixValue = static_cast<float>(*treeState.getRawParameterValue(mixID));
    auto dryBlock = juce::dsp::AudioBlock<float>(dryBuffer);
    auto wetBlock = juce::dsp::AudioBlock<float>(wetBuffer);
    dryWetMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    dryWetMixer.pushDrySamples(dryBlock);
    if (*treeState.getRawParameterValue(HQ_ID))
    {
        dryWetMixer.setWetLatency(latency);
    }
    else
    {
        dryWetMixer.setWetLatency(0);
    }
    dryWetMixer.setWetMixProportion(mixValue);
    dryWetMixer.mixWetSamples(wetBlock);
}

void FireAudioProcessor::normalize(juce::String modeID, juce::AudioBuffer<float>& buffer, int totalNumInputChannels, juce::SmoothedValue<float>& recSmoother, juce::SmoothedValue<float>& outputSmoother)
{
    //    int mode = static_cast<int>(*treeState.getRawParameterValue(modeID));

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        juce::Range<float> range = buffer.findMinMax(channel, 0, buffer.getNumSamples());
        float min = range.getStart();
        float max = range.getEnd();

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // centralization
            if (/*mode == diodemode  || */ recSmoother.getNextValue() > 0)
            {
                centralSmoother.setTargetValue((max + min) / 2.0f);
                channelData[sample] = channelData[sample] - centralSmoother.getNextValue();
            }

            //            // normalization
            //            if (mode == diode mode)
            //            {
            //                float magnitude = range.getLength() / 2.0f;
            //                normalSmoother.setTargetValue(magnitude);
            //                if (normalSmoother.getNextValue() != 0 && channelData[sample] != 0)
            //                {
            //                    channelData[sample] = channelData[sample] / normalSmoother.getNextValue();
            //                }
            //
            //                // final protection
            //                if (channelData[sample] > 1)
            //                {
            //                    channelData[sample] = 1;
            //                }
            //                else if (channelData[sample] < -1)
            //                {
            //                    channelData[sample] = -1;
            //                }
            //            }

            // output control
            channelData[sample] *= outputSmoother.getNextValue();
        }
    }
}

//void FireAudioProcessor::compressorProcessor(float ratio, float thresh, juce::dsp::Compressor<float> compressorProcessor, juce::dsp::ProcessContextReplacing<float> &context)
//{
//    compressorProcessor.setThreshold(thresh);
//    compressorProcessor.setRatio(ratio);
//    compressorProcessor.process(context);
//}

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

float FireAudioProcessor::getInputMeterRMSLevel(int channel, juce::String bandName)
{
    float outputValue = 0;
    if (bandName == "Global")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mInputLeftSmoothedGlobal);
        else
            outputValue = dBToNormalizedGain(mInputRightSmoothedGlobal);
    }
    else if (bandName == "Band1")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mInputLeftSmoothedBand1);
        else
            outputValue = dBToNormalizedGain(mInputRightSmoothedBand1);
    }
    else if (bandName == "Band2")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mInputLeftSmoothedBand2);
        else
            outputValue = dBToNormalizedGain(mInputRightSmoothedBand2);
    }
    else if (bandName == "Band3")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mInputLeftSmoothedBand3);
        else
            outputValue = dBToNormalizedGain(mInputRightSmoothedBand3);
    }
    else if (bandName == "Band4")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mInputLeftSmoothedBand4);
        else
            outputValue = dBToNormalizedGain(mInputRightSmoothedBand4);
    }
    else
    {
        jassertfalse;
    }
    return outputValue;
}

float FireAudioProcessor::getOutputMeterRMSLevel(int channel, juce::String bandName)
{
    float outputValue = 0;
    if (bandName == "Global")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mOutputLeftSmoothedGlobal);
        else
            outputValue = dBToNormalizedGain(mOutputRightSmoothedGlobal);
    }
    else if (bandName == "Band1")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mOutputLeftSmoothedBand1);
        else
            outputValue = dBToNormalizedGain(mOutputRightSmoothedBand1);
    }
    else if (bandName == "Band2")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mOutputLeftSmoothedBand2);
        else
            outputValue = dBToNormalizedGain(mOutputRightSmoothedBand2);
    }
    else if (bandName == "Band3")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mOutputLeftSmoothedBand3);
        else
            outputValue = dBToNormalizedGain(mOutputRightSmoothedBand3);
    }
    else if (bandName == "Band4")
    {
        if (channel == 0)
            outputValue = dBToNormalizedGain(mOutputLeftSmoothedBand4);
        else
            outputValue = dBToNormalizedGain(mOutputRightSmoothedBand4);
    }
    else
    {
        jassertfalse;
    }
    return outputValue;
}

// drive lookandfeel
float FireAudioProcessor::getReductionPrecent(juce::String safeId)
{
    if (safeId == SAFE_ID1)
        return mReductionPrecent1;
    if (safeId == SAFE_ID2)
        return mReductionPrecent2;
    if (safeId == SAFE_ID3)
        return mReductionPrecent3;
    if (safeId == SAFE_ID4)
        return mReductionPrecent4;
    return 0;
}

void FireAudioProcessor::setReductionPrecent(juce::String safeId, float reductionPrecent)
{
    if (safeId == SAFE_ID1)
        mReductionPrecent1 = reductionPrecent;
    if (safeId == SAFE_ID2)
        mReductionPrecent2 = reductionPrecent;
    if (safeId == SAFE_ID3)
        mReductionPrecent3 = reductionPrecent;
    if (safeId == SAFE_ID4)
        mReductionPrecent4 = reductionPrecent;
}

float FireAudioProcessor::getSampleMaxValue(juce::String safeId)
{
    if (safeId == SAFE_ID1)
        return mSampleMaxValue1;
    if (safeId == SAFE_ID2)
        return mSampleMaxValue2;
    if (safeId == SAFE_ID3)
        return mSampleMaxValue3;
    if (safeId == SAFE_ID4)
        return mSampleMaxValue4;
    return 0;
}

void FireAudioProcessor::setSampleMaxValue(juce::String safeId, float sampleMaxValue)
{
    if (safeId == SAFE_ID1)
        mSampleMaxValue1 = sampleMaxValue;
    if (safeId == SAFE_ID2)
        mSampleMaxValue2 = sampleMaxValue;
    if (safeId == SAFE_ID3)
        mSampleMaxValue3 = sampleMaxValue;
    if (safeId == SAFE_ID4)
        mSampleMaxValue4 = sampleMaxValue;
}

juce::AudioProcessorValueTreeState::ParameterLayout FireAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    int versionNum = 1;
    using PBool = juce::AudioParameterBool;
    using PInt = juce::AudioParameterInt;
    using PFloat = juce::AudioParameterFloat;
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { HQ_ID, versionNum }, HQ_NAME, false));

    parameters.push_back(std::make_unique<PInt>(juce::ParameterID { MODE_ID1, versionNum }, MODE_NAME1, 0, 11, 3));
    parameters.push_back(std::make_unique<PInt>(juce::ParameterID { MODE_ID2, versionNum }, MODE_NAME2, 0, 11, 3));
    parameters.push_back(std::make_unique<PInt>(juce::ParameterID { MODE_ID3, versionNum }, MODE_NAME3, 0, 11, 3));
    parameters.push_back(std::make_unique<PInt>(juce::ParameterID { MODE_ID4, versionNum }, MODE_NAME4, 0, 11, 3));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LINKED_ID1, versionNum }, LINKED_NAME1, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LINKED_ID2, versionNum }, LINKED_NAME2, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LINKED_ID3, versionNum }, LINKED_NAME3, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LINKED_ID4, versionNum }, LINKED_NAME4, true));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { SAFE_ID1, versionNum }, SAFE_NAME1, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { SAFE_ID2, versionNum }, SAFE_NAME2, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { SAFE_ID3, versionNum }, SAFE_NAME3, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { SAFE_ID4, versionNum }, SAFE_NAME4, true));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { EXTREME_ID1, versionNum }, EXTREME_NAME1, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { EXTREME_ID2, versionNum }, EXTREME_NAME2, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { EXTREME_ID3, versionNum }, EXTREME_NAME3, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { EXTREME_ID4, versionNum }, EXTREME_NAME4, false));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { DRIVE_ID1, versionNum }, DRIVE_NAME1, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { DRIVE_ID2, versionNum }, DRIVE_NAME2, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { DRIVE_ID3, versionNum }, DRIVE_NAME3, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { DRIVE_ID4, versionNum }, DRIVE_NAME4, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_RATIO_ID1, versionNum }, COMP_RATIO_NAME1, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_RATIO_ID2, versionNum }, COMP_RATIO_NAME2, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_RATIO_ID3, versionNum }, COMP_RATIO_NAME3, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_RATIO_ID4, versionNum }, COMP_RATIO_NAME4, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_THRESH_ID1, versionNum }, COMP_THRESH_NAME1, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_THRESH_ID2, versionNum }, COMP_THRESH_NAME2, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_THRESH_ID3, versionNum }, COMP_THRESH_NAME3, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { COMP_THRESH_ID4, versionNum }, COMP_THRESH_NAME4, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { WIDTH_ID1, versionNum }, WIDTH_NAME1, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { WIDTH_ID2, versionNum }, WIDTH_NAME2, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { WIDTH_ID3, versionNum }, WIDTH_NAME3, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { WIDTH_ID4, versionNum }, WIDTH_NAME4, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { OUTPUT_ID1, versionNum }, OUTPUT_NAME1, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { OUTPUT_ID2, versionNum }, OUTPUT_NAME2, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { OUTPUT_ID3, versionNum }, OUTPUT_NAME3, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { OUTPUT_ID4, versionNum }, OUTPUT_NAME4, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { OUTPUT_ID, versionNum }, OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { MIX_ID1, versionNum }, MIX_NAME1, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { MIX_ID2, versionNum }, MIX_NAME2, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { MIX_ID3, versionNum }, MIX_NAME3, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { MIX_ID4, versionNum }, MIX_NAME4, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { MIX_ID, versionNum }, MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { BIAS_ID1, versionNum }, BIAS_NAME1, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { BIAS_ID2, versionNum }, BIAS_NAME2, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { BIAS_ID3, versionNum }, BIAS_NAME3, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { BIAS_ID4, versionNum }, BIAS_NAME4, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { REC_ID1, versionNum }, REC_NAME1, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { REC_ID2, versionNum }, REC_NAME2, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { REC_ID3, versionNum }, REC_NAME3, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { REC_ID4, versionNum }, REC_NAME4, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.0f);
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { LOWCUT_FREQ_ID, versionNum }, LOWCUT_FREQ_NAME, cutoffRange, 20.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { LOWCUT_Q_ID, versionNum }, LOWCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { HIGHCUT_FREQ_ID, versionNum }, HIGHCUT_FREQ_NAME, cutoffRange, 20000.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { HIGHCUT_Q_ID, versionNum }, HIGHCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { LOWCUT_GAIN_ID, versionNum }, LOWCUT_GAIN_NAME, juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { HIGHCUT_GAIN_ID, versionNum }, HIGHCUT_GAIN_NAME, juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { PEAK_FREQ_ID, versionNum }, PEAK_FREQ_NAME, cutoffRange, 1000.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { PEAK_Q_ID, versionNum }, PEAK_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { PEAK_GAIN_ID, versionNum }, PEAK_GAIN_NAME, juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));

    parameters.push_back(std::make_unique<PInt>(juce::ParameterID { LOWCUT_SLOPE_ID, versionNum }, LOWCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<PInt>(juce::ParameterID { HIGHCUT_SLOPE_ID, versionNum }, HIGHCUT_SLOPE_NAME, 0, 3, 0));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LOWCUT_BYPASSED_ID, versionNum }, LOWCUT_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { PEAK_BYPASSED_ID, versionNum }, PEAK_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { HIGHCUT_BYPASSED_ID, versionNum }, HIGHCUT_BYPASSED_NAME, false));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { OFF_ID, versionNum }, OFF_NAME, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { PRE_ID, versionNum }, PRE_NAME, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { POST_ID, versionNum }, POST_NAME, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LOW_ID, versionNum }, LOW_NAME, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_ID, versionNum }, BAND_NAME, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { HIGH_ID, versionNum }, HIGH_NAME, true));

    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { DOWNSAMPLE_ID, versionNum }, DOWNSAMPLE_NAME, juce::NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    //    parameters.push_back (std::make_unique<PFloat> (juce::ParameterID { LIMITER_THRESH_ID, versionNum }, LIMITER_THRESH_NAME, juce::NormalisableRange<float> (-24.0f, 0.0f, 0.1f), 0.0f));
    //    juce::NormalisableRange<float> limiterReleaseRange (0.01f, 3000.0f, 0.01f);
    //    limiterReleaseRange.setSkewForCentre (6.0f);
    //    parameters.push_back (std::make_unique<PFloat> (juce::ParameterID { LIMITER_RELEASE_ID, versionNum }, LIMITER_RELEASE_NAME, limiterReleaseRange, 300.0f));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LINE_STATE_ID1, versionNum }, LINE_STATE_NAME1, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LINE_STATE_ID2, versionNum }, LINE_STATE_NAME2, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { LINE_STATE_ID3, versionNum }, LINE_STATE_NAME3, false));

    juce::NormalisableRange<float> freq(40.0f, 10024.0f, 1.0f);
    freq.setSkewForCentre(651.0f);
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { FREQ_ID1, versionNum }, FREQ_NAME1, freq, 21));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { FREQ_ID2, versionNum }, FREQ_NAME2, freq, 21));
    parameters.push_back(std::make_unique<PFloat>(juce::ParameterID { FREQ_ID3, versionNum }, FREQ_NAME3, freq, 21));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_ENABLE_ID1, versionNum }, BAND_ENABLE_NAME1, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_ENABLE_ID2, versionNum }, BAND_ENABLE_NAME2, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_ENABLE_ID3, versionNum }, BAND_ENABLE_NAME3, true));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_ENABLE_ID4, versionNum }, BAND_ENABLE_NAME4, true));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_SOLO_ID1, versionNum }, BAND_SOLO_NAME1, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_SOLO_ID2, versionNum }, BAND_SOLO_NAME2, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_SOLO_ID3, versionNum }, BAND_SOLO_NAME3, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { BAND_SOLO_ID4, versionNum }, BAND_SOLO_NAME4, false));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { COMP_BYPASS_ID1, versionNum }, COMP_BYPASS_NAME1, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { COMP_BYPASS_ID2, versionNum }, COMP_BYPASS_NAME2, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { COMP_BYPASS_ID3, versionNum }, COMP_BYPASS_NAME3, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { COMP_BYPASS_ID4, versionNum }, COMP_BYPASS_NAME4, false));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { WIDTH_BYPASS_ID1, versionNum }, WIDTH_BYPASS_NAME1, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { WIDTH_BYPASS_ID2, versionNum }, WIDTH_BYPASS_NAME2, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { WIDTH_BYPASS_ID3, versionNum }, WIDTH_BYPASS_NAME3, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { WIDTH_BYPASS_ID4, versionNum }, WIDTH_BYPASS_NAME4, false));

    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { FILTER_BYPASS_ID, versionNum }, FILTER_BYPASS_NAME, false));
    parameters.push_back(std::make_unique<PBool>(juce::ParameterID { DOWNSAMPLE_BYPASS_ID, versionNum }, DOWNSAMPLE_BYPASS_NAME, false));
    //    parameters.push_back (std::make_unique<PBool> (juce::ParameterID { LIMITER_BYPASS_ID, versionNum }, DOWNSAMPLE_BYPASS_NAME, false));

    parameters.push_back(std::make_unique<PInt>(juce::ParameterID { NUM_BANDS_ID, versionNum }, NUM_BANDS_NAME, 1, 4, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { AUTO_UPDATE_ID, versionNum }, AUTO_UPDATE_NAME, true));

    return { parameters.begin(), parameters.end() };
}
