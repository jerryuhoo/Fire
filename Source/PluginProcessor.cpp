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
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      treeState(*this, nullptr, "PARAMETERS", createParameters()), stateAB
{
    *this
}
#if JUCE_MAC
, statePresets
{
    *this, "Audio/Presets/Wings/Fire"
}
#else
, statePresets
{
    *this, "Wings/Fire"
} //AppData/Roaming/...
#endif
, VinL(500.f, 0.f) // VinL(0.f, 500.f)
    ,
    VinR(500.f, 0.f), R1L(80.0f), R1R(80.0f), C1L(3.5e-5, getSampleRate()), C1R(3.5e-5, getSampleRate()), RCL(&R1L, &C1L), RCR(&R1R, &C1R), rootL(&VinL, &RCL), rootR(&VinR, &RCR)

#endif
{
    // factor = 2 means 2^2 = 4, 4x oversampling
    oversampling.reset(new juce::dsp::Oversampling<float>(getTotalNumInputChannels(), 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false));
    for (size_t i = 0; i < 4; i++)
    {
        oversamplingHQ[i].reset(new juce::dsp::Oversampling<float>(getTotalNumInputChannels(), 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false));
    }
}

FireAudioProcessor::~FireAudioProcessor()
{
    oversampling.reset();
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

void FireAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

//==============================================================================
void FireAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    // fix the artifacts (also called zipper noise)
    previousOutput1 = (float)*treeState.getRawParameterValue(OUTPUT_ID1);
    previousOutput1 = juce::Decibels::decibelsToGain(previousOutput1);
    previousOutput2 = (float)*treeState.getRawParameterValue(OUTPUT_ID2);
    previousOutput2 = juce::Decibels::decibelsToGain(previousOutput2);
    previousOutput3 = (float)*treeState.getRawParameterValue(OUTPUT_ID3);
    previousOutput3 = juce::Decibels::decibelsToGain(previousOutput3);
    previousOutput4 = (float)*treeState.getRawParameterValue(OUTPUT_ID4);
    previousOutput4 = juce::Decibels::decibelsToGain(previousOutput4);
    previousOutput = juce::Decibels::decibelsToGain((float)*treeState.getRawParameterValue(OUTPUT_ID));

    previousDrive1 = *treeState.getRawParameterValue(DRIVE_ID1);
    previousDrive2 = *treeState.getRawParameterValue(DRIVE_ID2);
    previousDrive3 = *treeState.getRawParameterValue(DRIVE_ID3);
    previousDrive4 = *treeState.getRawParameterValue(DRIVE_ID4);
    
    previousMix1 = (float)*treeState.getRawParameterValue(MIX_ID1);
    previousMix2 = (float)*treeState.getRawParameterValue(MIX_ID2);
    previousMix3 = (float)*treeState.getRawParameterValue(MIX_ID3);
    previousMix4 = (float)*treeState.getRawParameterValue(MIX_ID4);
    previousMix = (float)*treeState.getRawParameterValue(MIX_ID);
    
    previousLowcutFreq = (float)*treeState.getRawParameterValue(LOWCUT_FREQ_ID);
    previousHighcutFreq = (float)*treeState.getRawParameterValue(HIGHCUT_FREQ_ID);
    previousPeakFreq = (float)*treeState.getRawParameterValue(PEAK_FREQ_ID);
    
    newDrive1 = 0;
    newDrive2 = 0;
    newDrive3 = 0;
    newDrive4 = 0;

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
    dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    dryBuffer.clear();
    
    wetBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    wetBuffer.clear();

    // oversampling init
    oversampling->reset();
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));

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

    // spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = /*newBlockSize;*/ samplesPerBlock;
    spec.numChannels = getMainBusNumOutputChannels();

    // filter init
    updateFilter();
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    // mode 8 diode================
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

    dryBuffer1.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    dryBuffer2.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    dryBuffer3.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    dryBuffer4.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    dryBuffer1.clear();
    dryBuffer2.clear();
    dryBuffer3.clear();
    dryBuffer4.clear();
    
    lowpass1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowpass2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowpass3.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    highpass1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    highpass2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    highpass3.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    lowpass1.reset();
    lowpass2.reset();
    lowpass3.reset();
    highpass1.reset();
    highpass2.reset();
    highpass3.reset();
    lowpass1.prepare(spec);
    lowpass2.prepare(spec);
    lowpass3.prepare(spec);
    highpass1.prepare(spec);
    highpass2.prepare(spec);
    highpass3.prepare(spec);
    
    // compressor
    compressorProcessor1.reset();
    compressorProcessor1.prepare(spec);
    compressorProcessor1.setAttack(80.0f);
    compressorProcessor1.setRelease(200.0f);
    
    compressorProcessor2.reset();
    compressorProcessor2.prepare(spec);
    compressorProcessor2.setAttack(80.0f);
    compressorProcessor2.setRelease(200.0f);
    
    compressorProcessor3.reset();
    compressorProcessor3.prepare(spec);
    compressorProcessor3.setAttack(80.0f);
    compressorProcessor3.setRelease(200.0f);
    
    compressorProcessor4.reset();
    compressorProcessor4.prepare(spec);
    compressorProcessor4.setAttack(80.0f);
    compressorProcessor4.setRelease(200.0f);

    // overdrive
    auto& filter1 = overdrive1.template get<5>();
    filter1.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 20.0);
    
    auto& filter2 = overdrive2.template get<5>();
    filter2.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 20.0);
    
    auto& filter3 = overdrive3.template get<5>();
    filter3.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 20.0);
    
    auto& filter4 = overdrive4.template get<5>();
    filter4.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 20.0);
    
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
}

void FireAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FireAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
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
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void FireAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                           juce::MidiBuffer& midiMessages)
{
    // set bypass to true
    isBypassed = true;
}

void FireAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // save clean signal
    dryBuffer.makeCopyOf(buffer);

    // VU input meter
//    float absInputLeftValue = fabs(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    float absInputLeftValue = fabs(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
    float absInputRightValue = fabs(buffer.getRMSLevel(1, 0, buffer.getNumSamples()));
    mInputLeftSmoothed = SMOOTH_COEFF * (mInputLeftSmoothed - absInputLeftValue) + absInputLeftValue;
    mInputRightSmoothed = SMOOTH_COEFF * (mInputRightSmoothed - absInputRightValue) + absInputRightValue;
    
    juce::dsp::AudioBlock<float> block(buffer);
    // pre-filter
    bool preButton = static_cast<bool>(*treeState.getRawParameterValue(PRE_ID));
    if (preButton)
    {
        //filterIIR.process(juce::dsp::ProcessContextReplacing<float>(block));
        updateFilter();
    }

    // multiband process
    int freqValue1 = static_cast<int>(*treeState.getRawParameterValue(FREQ_ID1));
    int freqValue2 = static_cast<int>(*treeState.getRawParameterValue(FREQ_ID2));
    int freqValue3 = static_cast<int>(*treeState.getRawParameterValue(FREQ_ID3));

    auto numSamples  = buffer.getNumSamples();
    mBuffer1.makeCopyOf(buffer);
    mBuffer2.makeCopyOf(buffer);
    mBuffer3.makeCopyOf(buffer);
    mBuffer4.makeCopyOf(buffer);

    multibandState1 = *treeState.getRawParameterValue(BAND_ENABLE_ID1);
    multibandState2 = *treeState.getRawParameterValue(BAND_ENABLE_ID2);
    multibandState3 = *treeState.getRawParameterValue(BAND_ENABLE_ID3);
    multibandState4 = *treeState.getRawParameterValue(BAND_ENABLE_ID4);
    
    multibandFocus1 = *treeState.getRawParameterValue(BAND_FOCUS_ID1);
    multibandFocus2 = *treeState.getRawParameterValue(BAND_FOCUS_ID2);
    multibandFocus3 = *treeState.getRawParameterValue(BAND_FOCUS_ID3);
    multibandFocus4 = *treeState.getRawParameterValue(BAND_FOCUS_ID4);

    if (multibandState1)
    {
        auto multibandBlock1 = juce::dsp::AudioBlock<float> (mBuffer1);
        auto context1 = juce::dsp::ProcessContextReplacing<float> (multibandBlock1);
        
        if (lineNum > 0)
        {
            lowpass1.setCutoffFrequency(freqValue1);
            lowpass1.process (context1);
        }

        dryBuffer1.makeCopyOf(mBuffer1);

        // dsp process
        auto* channeldataL = mBuffer1.getWritePointer(0);
        auto* channeldataR = mBuffer1.getWritePointer(1);
        float width1 = *treeState.getRawParameterValue(WIDTH_ID1);

        processDistortion(mBuffer1, MODE_ID1, DRIVE_ID1, SAFE_ID1, OUTPUT_ID1, BIAS_ID1, REC_ID1, overdrive1);
        
        normalize(MODE_ID1, mBuffer1, totalNumInputChannels, recSmoother1, outputSmoother1);

        widthProcessor1.process(channeldataL, channeldataR, width1, mBuffer1.getNumSamples());

        // compressor process
        float ratio1 = *treeState.getRawParameterValue(COMP_RATIO_ID1);
        float thresh1 = *treeState.getRawParameterValue(COMP_THRESH_ID1);
        compressorProcessor1.setThreshold(thresh1);
        compressorProcessor1.setRatio(ratio1);
        compressorProcessor1.process(context1);

        // mix process
        mixDryWet(dryBuffer1, mBuffer1, MIX_ID1, dryWetMixer1);
    }
    
    if (multibandState2 && lineNum >= 1)
    {
        auto multibandBlock2 = juce::dsp::AudioBlock<float> (mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float> (multibandBlock2);
        highpass1.setCutoffFrequency(freqValue1);
        highpass1.process (context2);

        if (lineNum > 1)
        {
            lowpass2.setCutoffFrequency(freqValue2);
            lowpass2.process(context2);
        }

        dryBuffer2.makeCopyOf(mBuffer2);
        
        // dsp process
        auto* channeldataL = mBuffer2.getWritePointer(0);
        auto* channeldataR = mBuffer2.getWritePointer(1);
        float width2 = *treeState.getRawParameterValue(WIDTH_ID2);
        
        processDistortion(mBuffer2, MODE_ID2, DRIVE_ID2, SAFE_ID2, OUTPUT_ID2, BIAS_ID2, REC_ID2, overdrive2);
        
        normalize(MODE_ID2, mBuffer2, totalNumInputChannels, recSmoother2, outputSmoother2);
        
        widthProcessor2.process(channeldataL, channeldataR, width2, mBuffer2.getNumSamples());
        
        // compressor process
        float ratio2 = *treeState.getRawParameterValue(COMP_RATIO_ID2);
        float thresh2 = *treeState.getRawParameterValue(COMP_THRESH_ID2);
        compressorProcessor1.setThreshold(thresh2);
        compressorProcessor1.setRatio(ratio2);
        compressorProcessor1.process(context2);

        // mix process
        mixDryWet(dryBuffer2, mBuffer2, MIX_ID2, dryWetMixer2);
    }
    
    if (multibandState3 && lineNum >= 2)
    {
        auto multibandBlock3 = juce::dsp::AudioBlock<float> (mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float> (multibandBlock3);
        highpass2.setCutoffFrequency(freqValue2);
        highpass2.process (context3);

        if (lineNum > 2)
        {
            lowpass3.setCutoffFrequency(freqValue3);
            lowpass3.process(context3);
        }

        dryBuffer3.makeCopyOf(mBuffer3);

        // dsp process
        auto* channeldataL = mBuffer3.getWritePointer(0);
        auto* channeldataR = mBuffer3.getWritePointer(1);
        float width3 = *treeState.getRawParameterValue(WIDTH_ID3);
        
        processDistortion(mBuffer3, MODE_ID3, DRIVE_ID3, SAFE_ID3, OUTPUT_ID3, BIAS_ID3, REC_ID3, overdrive3);
        
        normalize(MODE_ID3, mBuffer3, totalNumInputChannels, recSmoother3, outputSmoother3);
        
        widthProcessor3.process(channeldataL, channeldataR, width3, mBuffer3.getNumSamples());
        
        // compressor process
        float ratio3 = *treeState.getRawParameterValue(COMP_RATIO_ID3);
        float thresh3 = *treeState.getRawParameterValue(COMP_THRESH_ID3);
        compressorProcessor1.setThreshold(thresh3);
        compressorProcessor1.setRatio(ratio3);
        compressorProcessor1.process(context3);

        // mix process
        mixDryWet(dryBuffer3, mBuffer3, MIX_ID3, dryWetMixer3);
    }
    
    if (multibandState4 && lineNum == 3)
    {
        auto multibandBlock4 = juce::dsp::AudioBlock<float> (mBuffer4);
        auto context4 = juce::dsp::ProcessContextReplacing<float> (multibandBlock4);
        highpass3.setCutoffFrequency(freqValue3);
        highpass3.process (context4);
        
        dryBuffer4.makeCopyOf(mBuffer4);

        // dsp process
        auto* channeldataL = mBuffer4.getWritePointer(0);
        auto* channeldataR = mBuffer4.getWritePointer(1);
        float width4 = *treeState.getRawParameterValue(WIDTH_ID4);
        
        processDistortion(mBuffer4, MODE_ID4, DRIVE_ID4, SAFE_ID4, OUTPUT_ID4, BIAS_ID4, REC_ID4, overdrive4);
        
        normalize(MODE_ID4, mBuffer4, totalNumInputChannels, recSmoother4, outputSmoother4);
        
        widthProcessor4.process(channeldataL, channeldataR, width4, mBuffer4.getNumSamples());
        
        // compressor process
        float ratio4 = *treeState.getRawParameterValue(COMP_RATIO_ID4);
        float thresh4 = *treeState.getRawParameterValue(COMP_THRESH_ID4);
        compressorProcessor1.setThreshold(thresh4);
        compressorProcessor1.setRatio(ratio4);
        compressorProcessor1.process(context4);

        // mix process
        mixDryWet(dryBuffer4, mBuffer4, MIX_ID4, dryWetMixer4);
    }
    
    buffer.clear();
    if (multibandState1)
    {
        buffer.addFrom(0, 0, mBuffer1, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer1, 1, 0, numSamples);
    }
    if (multibandState2 && lineNum >= 1)
    {
        buffer.addFrom(0, 0, mBuffer2, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer2, 1, 0, numSamples);
    }
    if (multibandState3 && lineNum >= 2)
    {
        buffer.addFrom(0, 0, mBuffer3, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer3, 1, 0, numSamples);
    }
    if (multibandState4 && lineNum == 3)
    {
        buffer.addFrom(0, 0, mBuffer4, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer4, 1, 0, numSamples);
    }

    // downsample
    int rateDivide = static_cast<int>(*treeState.getRawParameterValue(DOWNSAMPLE_ID));
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {   
            //int rateDivide = (distortionProcessor.controls.drive - 1) / 63.f * 99.f + 1; //range(1,100)
            if (rateDivide > 1)
            {
                if (sample % rateDivide != 0)
                    channelData[sample] = channelData[sample - sample % rateDivide];
            }
        }
    }

    updateFilter();
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    leftChain.process(juce::dsp::ProcessContextReplacing<float>(leftBlock));
    rightChain.process(juce::dsp::ProcessContextReplacing<float>(rightBlock));
    
    //post-filter
    bool postButton = *treeState.getRawParameterValue(POST_ID);
    if (postButton)
    {
        //filterIIR.process(juce::dsp::ProcessContextReplacing<float>(block));
//        mainChain.process(juce::dsp::ProcessContextReplacing<float>(block));
        updateFilter();
    }

    float mix = *treeState.getRawParameterValue(MIX_ID);
    mixSmootherGlobal.setTargetValue(mix);

    float output = juce::Decibels::decibelsToGain((float)*treeState.getRawParameterValue(OUTPUT_ID));
    outputSmootherGlobal.setTargetValue(output);

    // mix control
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        auto *cleanSignal = dryBuffer.getWritePointer(channel);
        //auto* cleanSignal = mDelayBuffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float smoothMixValue = mixSmootherGlobal.getNextValue();
            float smoothOutputValue = outputSmootherGlobal.getNextValue();

            // channelData[sample] = (1.f - mix) * cleanSignal[sample] + mix * channelData[sample];
            channelData[sample] = (1.f - smoothMixValue) * cleanSignal[sample] + smoothMixValue * channelData[sample] * smoothOutputValue;
            // channelData[sample] = (1.0f - smoothMixValue) * mDelay.process(cleanSignal[sample], channel, buffer.getNumSamples()) + smoothMixValue * channelData[sample];
            
            // mDelay is delayed clean signal
            if (sample % 10 == 0)
            {
                if (channel == 0)
                {
                    historyArrayL.add(channelData[sample]);
                }
                else if (channel == 1)
                {
                    historyArrayR.add(channelData[sample]);
                }
                if (historyArrayL.size() > historyLength)
                {
                    historyArrayL.remove(0);
                }
                if (historyArrayR.size() > historyLength)
                {
                    historyArrayR.remove(0);
                }
            }
        }
    }

    // VU output meter
    float absOutputLeftValue = fabs(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
    float absOutputRightValue = fabs(buffer.getRMSLevel(1, 0, buffer.getNumSamples()));
    mOutputLeftSmoothed = SMOOTH_COEFF * (mOutputLeftSmoothed - absOutputLeftValue) + absOutputLeftValue;
    mOutputRightSmoothed = SMOOTH_COEFF * (mOutputRightSmoothed - absOutputRightValue) + absOutputRightValue;
    
    
    // Spectrum
    wetBuffer.makeCopyOf(buffer);
    pushDataToFFT();

    dryBuffer.clear();


    //visualiser.pushBuffer(buffer);
    
//    historyBuffer.makeCopyOf(buffer);
}

//==============================================================================
bool FireAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *FireAudioProcessor::createEditor()
{
    return new FireAudioProcessorEditor(*this);
}

//==============================================================================
void FireAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    int xmlIndex = 0;
    juce::XmlElement xmlState{"state"};

    // 1. save treestate (parameters)
    auto state = treeState.copyState();
    std::unique_ptr<juce::XmlElement> treeStateXml(state.createXml());
    xmlState.insertChildElement(treeStateXml.release(), xmlIndex++);

    // 2. save current preset ID, width and height
    std::unique_ptr<juce::XmlElement> currentStateXml{new juce::XmlElement{"otherState"}};
    currentStateXml->setAttribute("currentPresetID", statePresets.getCurrentPresetId());
    currentStateXml->setAttribute("editorWidth", editorWidth);
    currentStateXml->setAttribute("editorHeight", editorHeight);

    xmlState.insertChildElement(currentStateXml.release(), xmlIndex++);

    copyXmlToBinary(xmlState, destData);
}

void FireAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
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
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
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
    settings.highCutFreq = apvts.getRawParameterValue(HIGHCUT_FREQ_ID)->load();
    settings.highCutQuality = apvts.getRawParameterValue(HIGHCUT_Q_ID)->load();
    settings.peakFreq = apvts.getRawParameterValue(PEAK_FREQ_ID)->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue(PEAK_GAIN_ID)->load();
    settings.peakQuality = apvts.getRawParameterValue(PEAK_Q_ID)->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue(LOWCUT_SLOPE_ID)->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID)->load());
    
    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load();// > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load();// > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load();// > 0.5f;
    
    return settings;
}

CoefficientsPtr makePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.peakFreq,
                                                               chainSettings.peakQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
}

void FireAudioProcessor::updatePeakFilter(const ChainSettings &chainSettings)
{
    auto peakCoefficients = makePeakFilter(chainSettings, getSampleRate());
    
    leftChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    rightChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void FireAudioProcessor::updateLowCutFilters(const ChainSettings &chainSettings)
{
    auto cutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    
    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    
    updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(leftLowCut, cutCoefficients, chainSettings.lowCutSlope);
}

void FireAudioProcessor::updateHighCutFilters(const ChainSettings &chainSettings)
{
    auto highCutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());
        
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    
    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    
    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
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

float FireAudioProcessor::getNewDrive(juce::String driveId)
{
    if (driveId == DRIVE_ID1)
        return newDrive1;
    if (driveId == DRIVE_ID2)
        return newDrive2;
    if (driveId == DRIVE_ID3)
        return newDrive3;
    if (driveId == DRIVE_ID4)
        return newDrive4;
    jassertfalse;
    return -1.0f;
}

juce::Array<float> FireAudioProcessor::getHistoryArrayL()
{
    return historyArrayL;
}

juce::Array<float> FireAudioProcessor::getHistoryArrayR()
{
    return historyArrayR;
}

float * FireAudioProcessor::getFFTData()
{
    return spectrum_processor.fftData;
}

int FireAudioProcessor::getFFTSize()
{
    return spectrum_processor.fftSize / 2 ;
}

bool FireAudioProcessor::isFFTBlockReady()
{
    return spectrum_processor.nextFFTBlockReady;
}

void FireAudioProcessor::pushDataToFFT()
{
    
    if (wetBuffer.getNumChannels() > 0)
    {
        auto* channelData = wetBuffer.getReadPointer(0);

        for (auto i = 0; i < wetBuffer.getNumSamples(); ++i)
            spectrum_processor.pushNextSampleIntoFifo(channelData[i]);
    }
}

void FireAudioProcessor::processFFT(float * tempFFTData)
{
    spectrum_processor.doProcessing(tempFFTData);
    spectrum_processor.nextFFTBlockReady = false;
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

void FireAudioProcessor::processDistortion(juce::AudioBuffer<float>& bandBuffer, juce::String modeID, juce::String driveID, juce::String safeID, juce::String outputID, juce::String biasID, juce::String recID, juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor, DCFilter, GainProcessor>& overdrive)
{
    // oversampling
    juce::dsp::AudioBlock<float> blockInput(bandBuffer);
    juce::dsp::AudioBlock<float> blockOutput;
    int num;
    if (modeID == MODE_ID1) num = 0;
    else if (modeID == MODE_ID2) num = 1;
    else if (modeID == MODE_ID3) num = 2;
    else if (modeID == MODE_ID4) num = 3;
    else num = -1;
    if (*treeState.getRawParameterValue(HQ_ID))
    {
        blockInput = blockInput.getSubBlock(0, bandBuffer.getNumSamples());
        blockOutput = oversamplingHQ[num]->processSamplesUp(blockInput);

        // the wet in high quality mode will have a latency of 3~4 samples.
        // so I must add the same latency to drybuffer.
        // But I don't know why the drybuffer is still 1 sample slower than the wet one.
        // So I added 1.  really weird :(
        int latency = juce::roundToInt(oversamplingHQ[num]->getLatencyInSamples()) + 1;

        mDelay.setLatency(latency);
        mDelay.setState(true);

        // report latency to the host
        //setLatencySamples(latency);
    }
    else
    {
        //dsp::AudioBlock<float> blockOutput = oversampling->processSamplesUp(blockInput);
        blockOutput = blockInput.getSubBlock(0, bandBuffer.getNumSamples());
        mDelay.setState(false);
        // latency = 0
        //setLatencySamples(0);
    }
    
    // band process
    auto context = juce::dsp::ProcessContextReplacing<float> (blockOutput);
    
    // get parameters from sliders
    int mode = static_cast<int>(*treeState.getRawParameterValue(modeID));
    float driveValue = static_cast<float>(*treeState.getRawParameterValue(driveID));
    float outputValue = static_cast<float>(*treeState.getRawParameterValue(outputID));
    float biasValue = static_cast<float>(*treeState.getRawParameterValue(biasID));
    float recValue = static_cast<float>(*treeState.getRawParameterValue(recID));
    
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
    
    float maxValue = juce::jmax(bandBuffer.getRMSLevel(0, 0, bandBuffer.getNumSamples()), bandBuffer.getRMSLevel(1, 0, bandBuffer.getNumSamples())) ;
    
    if (maxValue < 0.00001f)
    {
        biasValue = 0;
    }
    
    auto& bias = overdrive.get<1>();
    bias.setBias (biasValue); // -1,1
    bias.setRampDurationSeconds(0.05f);

    auto& waveShaper = overdrive.get<2>();
    
    switch (mode)
    {
        case 0:
            waveShaper.functionToUse = waveshaping::doNothing;
            break;
        case 1:
            waveShaper.functionToUse = waveshaping::arctanSoftClipping;
            break;
        case 2:
            waveShaper.functionToUse = waveshaping::expSoftClipping;
            break;
        case 3:
            waveShaper.functionToUse = waveshaping::tanhSoftClipping;
            break;
        case 4:
            waveShaper.functionToUse = waveshaping::cubicSoftClipping;
            break;
        case 5:
            waveShaper.functionToUse = waveshaping::hardClipping;
            break;
        case 6:
            waveShaper.functionToUse = waveshaping::sausageFattener;
            break;
        case 7:
            waveShaper.functionToUse = waveshaping::sinFoldback;
            break;
        case 8:
            waveShaper.functionToUse = waveshaping::linFoldback;
            break;
        case 9:
            waveShaper.functionToUse = waveshaping::limitclip;
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
    bias2.setBias (-biasValue); // -1,1
    bias2.setRampDurationSeconds(0.05f);

    auto& dcFilter = overdrive.get<5>();
    dcFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (getSampleRate(), 20.0);

    auto& gainDown = overdrive.get<6>();
    gainDown.setGainDecibels (outputValue);
    gainDown.setRampDurationSeconds(0.05f);

    overdrive.process(context);
    
    // oversampling
    if (*treeState.getRawParameterValue(HQ_ID)) // oversampling
    {
        oversamplingHQ[num]->processSamplesDown(blockInput);
    }
}

void FireAudioProcessor::mixDryWet(juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer, juce::String mixID, juce::dsp::DryWetMixer<float>& dryWetMixer)
{
    float mixValue = static_cast<float>(*treeState.getRawParameterValue(mixID));
    auto dryBlock = juce::dsp::AudioBlock<float> (dryBuffer);
    auto wetBlock = juce::dsp::AudioBlock<float> (wetBuffer);

    dryWetMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    dryWetMixer.pushDrySamples (dryBlock);
    dryWetMixer.setWetMixProportion(mixValue);
    dryWetMixer.mixWetSamples (wetBlock);
}

void FireAudioProcessor::normalize(juce::String modeID, juce::AudioBuffer<float>& buffer, int totalNumInputChannels, juce::SmoothedValue<float>& recSmoother, juce::SmoothedValue<float>& outputSmoother)
{
    int mode = static_cast<int>(*treeState.getRawParameterValue(modeID));
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);

        juce::Range<float> range = buffer.findMinMax(channel, 0, buffer.getNumSamples());
        float min = range.getStart();
        float max = range.getEnd();
        float magnitude = range.getLength() / 2.0f;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // centralization
            if (mode == 9 || recSmoother.getNextValue() > 0)
            {
                centralSmoother.setTargetValue((max + min) / 2.0f);
                channelData[sample] = channelData[sample] - centralSmoother.getNextValue();
            }

            // normalization
            if (mode == 9)
            {
                normalSmoother.setTargetValue(magnitude);
                if (normalSmoother.getNextValue() != 0 && channelData[sample] != 0)
                {
                    channelData[sample] = channelData[sample] / normalSmoother.getNextValue();
                }

                // final protection
                if (channelData[sample] > 1)
                {
                    channelData[sample] = 1;
                }
                else if (channelData[sample] < -1)
                {
                    channelData[sample] = -1;
                }
            }

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

void FireAudioProcessor::mixProcessor(juce::String mixId, juce::SmoothedValue<float> &mixSmoother, int totalNumInputChannels, juce::AudioBuffer<float> &buffer, juce::AudioBuffer<float> dryBuffer)
{
    float mix = *treeState.getRawParameterValue(mixId);
    mixSmoother.setTargetValue(mix);
    // mix control
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        auto *cleanSignal = dryBuffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float smoothMixValue = mixSmoother.getNextValue();
            channelData[sample] = (1.0f - smoothMixValue) * mDelay.process(cleanSignal[sample], channel, buffer.getNumSamples()) + smoothMixValue * channelData[sample];
        }
    }
}

//int FireAudioProcessor::getLineNum()
//{
//    return lineNum;
//}

void FireAudioProcessor::setLineNum(int lineNum)
{
    this->lineNum = lineNum;
}


// VU meters
float FireAudioProcessor::getInputMeterLevel(int channel)
{
    if (channel == 0) return dBToNormalizedGain(mInputLeftSmoothed);
    else return dBToNormalizedGain(mInputRightSmoothed);
}

float FireAudioProcessor::getOutputMeterLevel(int channel)
{
    if (channel == 0) return dBToNormalizedGain(mOutputLeftSmoothed);
    else return dBToNormalizedGain(mOutputRightSmoothed);
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

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(HQ_ID, HQ_NAME, false));
    
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(MODE_ID1, MODE_NAME1, 0, 9, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(MODE_ID2, MODE_NAME2, 0, 9, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(MODE_ID3, MODE_NAME3, 0, 9, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(MODE_ID4, MODE_NAME4, 0, 9, 1));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LINKED_ID1, LINKED_NAME1, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LINKED_ID2, LINKED_NAME2, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LINKED_ID3, LINKED_NAME3, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LINKED_ID4, LINKED_NAME4, true));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(SAFE_ID1, SAFE_NAME1, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(SAFE_ID2, SAFE_NAME2, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(SAFE_ID3, SAFE_NAME3, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(SAFE_ID4, SAFE_NAME4, true));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DRIVE_ID1, DRIVE_NAME1, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DRIVE_ID2, DRIVE_NAME2, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DRIVE_ID3, DRIVE_NAME3, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DRIVE_ID4, DRIVE_NAME4, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_RATIO_ID1, COMP_RATIO_NAME1, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_RATIO_ID2, COMP_RATIO_NAME2, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_RATIO_ID3, COMP_RATIO_NAME3, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_RATIO_ID4, COMP_RATIO_NAME4, juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 1.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_THRESH_ID1, COMP_THRESH_NAME1, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_THRESH_ID2, COMP_THRESH_NAME2, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_THRESH_ID3, COMP_THRESH_NAME3, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_THRESH_ID4, COMP_THRESH_NAME4, juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), 0.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID1, WIDTH_NAME1, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID2, WIDTH_NAME2, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID3, WIDTH_NAME3, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID4, WIDTH_NAME4, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID1, OUTPUT_NAME1, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID2, OUTPUT_NAME2, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID3, OUTPUT_NAME3, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID4, OUTPUT_NAME4, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID, OUTPUT_NAME, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID1, MIX_NAME1, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID2, MIX_NAME2, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID3, MIX_NAME3, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID4, MIX_NAME4, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID, MIX_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(BIAS_ID1, BIAS_NAME1, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(BIAS_ID2, BIAS_NAME2, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(BIAS_ID3, BIAS_NAME3, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(BIAS_ID4, BIAS_NAME4, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(REC_ID1, REC_NAME1, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(REC_ID2, REC_NAME2, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(REC_ID3, REC_NAME3, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(REC_ID4, REC_NAME4, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DOWNSAMPLE_ID, DOWNSAMPLE_NAME, juce::NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    
    
    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.0f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(LOWCUT_FREQ_ID, LOWCUT_FREQ_NAME, cutoffRange, 20.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(LOWCUT_Q_ID, LOWCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(HIGHCUT_FREQ_ID, HIGHCUT_FREQ_NAME, cutoffRange, 20000.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(HIGHCUT_Q_ID, HIGHCUT_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(PEAK_FREQ_ID, PEAK_FREQ_NAME, cutoffRange, 1000.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(PEAK_Q_ID, PEAK_Q_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(PEAK_GAIN_ID, PEAK_GAIN_NAME, juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(LOWCUT_SLOPE_ID, LOWCUT_SLOPE_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(HIGHCUT_SLOPE_ID, HIGHCUT_SLOPE_NAME, 0, 3, 0));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LOWCUT_BYPASSED_ID, LOWCUT_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(PEAK_BYPASSED_ID, PEAK_BYPASSED_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(HIGHCUT_BYPASSED_ID, HIGHCUT_BYPASSED_NAME, false));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(OFF_ID, OFF_NAME, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(PRE_ID, PRE_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(POST_ID, POST_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LOW_ID, LOW_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_ID, BAND_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(HIGH_ID, HIGH_NAME, true));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LINE_STATE_ID1, LINE_STATE_NAME1, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LINE_STATE_ID2, LINE_STATE_NAME2, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LINE_STATE_ID3, LINE_STATE_NAME3, false));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(FREQ_ID1, FREQ_NAME1, 21, 11040, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(FREQ_ID2, FREQ_NAME2, 21, 11040, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(FREQ_ID3, FREQ_NAME3, 21, 11040, 0));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_ENABLE_ID1, BAND_ENABLE_NAME1, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_ENABLE_ID2, BAND_ENABLE_NAME2, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_ENABLE_ID3, BAND_ENABLE_NAME3, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_ENABLE_ID4, BAND_ENABLE_NAME4, true));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID1, BAND_SOLO_NAME1, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID2, BAND_SOLO_NAME2, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID3, BAND_SOLO_NAME3, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID4, BAND_SOLO_NAME4, false));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID1, BAND_FOCUS_NAME1, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID2, BAND_FOCUS_NAME2, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID3, BAND_FOCUS_NAME3, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID4, BAND_FOCUS_NAME4, false));
    
    return {parameters.begin(), parameters.end()};
}
