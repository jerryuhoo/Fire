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
    oversamplingHQ.reset(new juce::dsp::Oversampling<float>(getTotalNumInputChannels(), 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false));
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
    //previousGainInput = (float)*treeState.getRawParameterValue("inputGain");
    //previousGainInput = Decibels::decibelsToGain(previousGainInput);

    previousOutput1 = (float)*treeState.getRawParameterValue(OUTPUT_ID1);
    previousOutput1 = juce::Decibels::decibelsToGain(previousOutput1);
    previousDrive1 = *treeState.getRawParameterValue(DRIVE_ID1);
    previousCutoff = (float)*treeState.getRawParameterValue(CUTOFF_ID);
    previousColor = (float)*treeState.getRawParameterValue(COLOR_ID);
    previousMix1 = (float)*treeState.getRawParameterValue(MIX_ID1);
    newDrive1 = 0;

    driveSmoother1.reset(sampleRate, 0.05); //0.05 second is rampLength, which means increasing to targetvalue needs 0.05s.
    driveSmoother1.setCurrentAndTargetValue(previousDrive1);

    outputSmoother1.reset(sampleRate, 0.05);
    outputSmoother1.setCurrentAndTargetValue(previousOutput1);

    cutoffSmoother.reset(sampleRate, 0.001);
    cutoffSmoother.setCurrentAndTargetValue(previousCutoff);

    recSmoother.reset(sampleRate, 0.05);
    recSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue("rec"));

    biasSmoother.reset(sampleRate, 0.001);
    biasSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue("bias"));

    colorSmoother.reset(sampleRate, 0.001);
    colorSmoother.setCurrentAndTargetValue(previousColor);

    mixSmoother1.reset(sampleRate, 0.05);
    mixSmoother1.setCurrentAndTargetValue(previousMix1);

    centralSmoother.reset(sampleRate, 0.1);
    centralSmoother.setCurrentAndTargetValue(0);

    normalSmoother.reset(sampleRate, 0.5);
    normalSmoother.setCurrentAndTargetValue(1);

    // clear visualiser
    //visualiser.clear();
    for (int i = 0; i < samplesPerBlock; i++)
    {
        historyArrayL.add(0);
        historyArrayR.add(0);
    }
    
    // dry buffer init
    dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    dryBuffer.clear();

    // width buffer init
    midSideBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock); // channel = 2? 0 = mid 1 = side?
    midSideBuffer.clear();
    
    // oversampling init
    oversampling->reset();
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversamplingHQ->reset();
    oversamplingHQ->initProcessing(static_cast<size_t>(samplesPerBlock));
    mDelay.reset(0);
    // dsp init
    // dsp::ProcessSpec spec;

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
    filterIIR.reset();
    filterColor.reset();
    updateFilter();
    filterIIR.prepare(spec);
    filterColor.prepare(spec);

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

    // ff meter
    // this prepares the meterSource to measure all output blocks and average over 100ms to allow smooth movements
    inputMeterSource.resize(getTotalNumOutputChannels(), sampleRate * 0.1 / samplesPerBlock);
    outputMeterSource.resize(getTotalNumOutputChannels(), sampleRate * 0.1 / samplesPerBlock);
    
    
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

void FireAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
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

    // ff input meter
    inputMeterSource.measureBlock(buffer);

    // get parameters from sliders
    int mode = *treeState.getRawParameterValue(MODE_ID1);

    //float currentGainInput = *treeState.getRawParameterValue("inputGain");
    //currentGainInput = Decibels::decibelsToGain(currentGainInput);

    float drive = *treeState.getRawParameterValue(DRIVE_ID1);

    float color = *treeState.getRawParameterValue(COLOR_ID);

    float currentGainOutput = *treeState.getRawParameterValue(OUTPUT_ID1);
    currentGainOutput = juce::Decibels::decibelsToGain(currentGainOutput);

    float mix = *treeState.getRawParameterValue(MIX_ID1);
    float bias = *treeState.getRawParameterValue(BIAS_ID);

    // set distortion processor parameters
    distortionProcessor.controls.mode = mode;

    // input volume fix
    //    if (currentGainInput == previousGainInput)
    //    {
    //        buffer.applyGain(currentGainInput);
    //    }
    //    else
    //    {
    //        buffer.applyGainRamp(0, buffer.getNumSamples(), previousGainInput, currentGainInput);
    //        previousGainInput = currentGainInput;
    //    }

    // sausage
    //    if (mode == 6)
    //    {
    //        drive = (drive - 1) * 6.5 / 31.f;
    //        drive = powf(2, drive);
    //    }
    drive = drive * 6.5 / 100.f;
    drive = powf(2, drive);

    // color eq filter
    juce::dsp::AudioBlock<float> block(buffer);
    filterColor.process(juce::dsp::ProcessContextReplacing<float>(block));
    updateFilter();

    // protection
    float sampleMaxValue = 0;

    sampleMaxValue = buffer.getMagnitude(0, buffer.getNumSamples());

    distortionProcessor.controls.protection = *treeState.getRawParameterValue(SAFE_ID1);

    if (distortionProcessor.controls.protection == true)
    {
        if (sampleMaxValue * drive > 2.f)
        {
            drive = 2.f / sampleMaxValue + 0.1 * std::log2f(drive);
        }
    }
    newDrive1 = drive;

    // set zipper noise smoother target
    driveSmoother1.setTargetValue(drive);
    outputSmoother1.setTargetValue(currentGainOutput);
    mixSmoother1.setTargetValue(mix);
    colorSmoother.setTargetValue(color);
    biasSmoother.setTargetValue(bias * juce::roundToInt(sampleMaxValue * 100) / 100.f);

    // set distortion processor smooth parameters
    distortionProcessor.controls.color = colorSmoother.getNextValue();
    distortionProcessor.controls.bias = biasSmoother.getNextValue();
    // DBG(distortionProcessor.controls.bias);

    
    
    
    
    
    
    
    
    
    
    // bias
    if (sampleMaxValue == 0)
    {
        distortionProcessor.controls.bias = 0;
    }

    // multiband process
    int freqValue1 = *treeState.getRawParameterValue(FREQ_ID1);
    int freqValue2 = *treeState.getRawParameterValue(FREQ_ID2);
    int freqValue3 = *treeState.getRawParameterValue(FREQ_ID3);

    auto numSamples  = buffer.getNumSamples();
    mBuffer1.makeCopyOf(buffer);
    mBuffer2.makeCopyOf(buffer);
    mBuffer3.makeCopyOf(buffer);
    mBuffer4.makeCopyOf(buffer);
    
    // test
    multibandState1 = *treeState.getRawParameterValue(BAND_STATE_ID1);
    multibandState2 = *treeState.getRawParameterValue(BAND_STATE_ID2);
    multibandState3 = *treeState.getRawParameterValue(BAND_STATE_ID3);
    multibandState4 = *treeState.getRawParameterValue(BAND_STATE_ID4);
    
    multibandFocus1 = *treeState.getRawParameterValue(BAND_FOCUS_ID1);
    multibandFocus2 = *treeState.getRawParameterValue(BAND_FOCUS_ID2);
    multibandFocus3 = *treeState.getRawParameterValue(BAND_FOCUS_ID3);
    multibandFocus4 = *treeState.getRawParameterValue(BAND_FOCUS_ID4);
    
    
    
    
    if (multibandState1)
    {
        auto multibandBlock1 = juce::dsp::AudioBlock<float> (mBuffer1);
        auto context1 = juce::dsp::ProcessContextReplacing<float> (multibandBlock1);
        lowpass1.setCutoffFrequency(freqValue1);
        lowpass1.process (context1);
    }
    
    if (multibandState2)
    {
        auto multibandBlock2 = juce::dsp::AudioBlock<float> (mBuffer2);
        auto context2 = juce::dsp::ProcessContextReplacing<float> (multibandBlock2);
        highpass1.setCutoffFrequency(freqValue1);
        highpass1.process (context2);

        lowpass2.setCutoffFrequency(freqValue2);
        lowpass2.process (context2);
    }
    
    if (multibandState3)
    {
        auto multibandBlock3 = juce::dsp::AudioBlock<float> (mBuffer3);
        auto context3 = juce::dsp::ProcessContextReplacing<float> (multibandBlock3);
        highpass2.setCutoffFrequency(freqValue2);
        highpass2.process (context3);

        lowpass3.setCutoffFrequency(freqValue3);
        lowpass3.process (context3);
    }
    
    if (multibandState4)
    {
        auto multibandBlock4 = juce::dsp::AudioBlock<float> (mBuffer4);
        auto context4 = juce::dsp::ProcessContextReplacing<float> (multibandBlock4);
        highpass3.setCutoffFrequency(freqValue3);
        highpass3.process (context4);
    }
    
    
    
    buffer.clear();
    if (multibandState1)
    {
        buffer.addFrom(0, 0, mBuffer1, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer1, 1, 0, numSamples);
    }
    if (multibandState2)
    {
        buffer.addFrom(0, 0, mBuffer2, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer2, 1, 0, numSamples);
    }
    if (multibandState3)
    {
        buffer.addFrom(0, 0, mBuffer3, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer3, 1, 0, numSamples);
    }
    if (multibandState4)
    {
        buffer.addFrom(0, 0, mBuffer4, 0, 0, numSamples);
        buffer.addFrom(1, 0, mBuffer4, 1, 0, numSamples);
    }
    
    // pre-filter
    bool preButton = *treeState.getRawParameterValue(PRE_ID);
    if (preButton)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        filterIIR.process(juce::dsp::ProcessContextReplacing<float>(block));
        //filterIIR.process(dsp::ProcessContextReplacing<float>(blockOutput));
        updateFilter();
    }

    // TODO: ------put this in a new function--------

    // oversampling
    /*
    double newSampleRate = getSampleRate();
    int newBufferSize = buffer.getNumSamples();
    if (*treeState.getRawParameterValue("hq"))
    {
        oversampleFactor = oversamplingHQ->getOversamplingFactor();
        newSampleRate *= oversampleFactor;
        newBufferSize *= oversampleFactor;
        oversamplingHQ->getLatencyInSamples();
    }
    else
    {
        oversampleFactor = oversampling->getOversamplingFactor();
        newSampleRate *= oversampleFactor;
        newBufferSize *= oversampleFactor;
        oversampling->getLatencyInSamples();
    }
    */
    // TODO: ------put this in a new function--------
    juce::dsp::AudioBlock<float> blockInput(buffer);
    juce::dsp::AudioBlock<float> blockOutput;
    // oversampling

    if (*treeState.getRawParameterValue(HQ_ID))
    {
        blockInput = blockInput.getSubBlock(0, buffer.getNumSamples());
        blockOutput = oversamplingHQ->processSamplesUp(blockInput);

        // the wet in high quality mode will have a latency of 3~4 samples.
        // so I must add the same latency to drybuffer.
        // But I don't know why the drybuffer is still 1 sample slower than the wet one.
        // So I added 1.  really weird :(
        int latency = juce::roundToInt(oversamplingHQ->getLatencyInSamples()) + 1;

        mDelay.setLatency(latency);
        mDelay.setState(true);

        // report latency to the host
        //setLatencySamples(latency);
    }
    else
    {
        //dsp::AudioBlock<float> blockOutput = oversampling->processSamplesUp(blockInput);
        blockOutput = blockInput.getSubBlock(0, buffer.getNumSamples());
        mDelay.setState(false);
        // latency = 0
        //setLatencySamples(0);
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {

        // float* data = buffer.getWritePointer(channel);
        // auto *channelData = buffer.getWritePointer(channel);
        auto *channelData = blockOutput.getChannelPointer(channel);
        inputTemp.clear();

        for (int sample = 0; sample < blockOutput.getNumSamples(); ++sample)
        {
            // smooth end and start of bias
            if (channelData[sample] == 0 && channelData[sample + 1] == 0 && sample < blockOutput.getNumSamples() - 1)
            {
                distortionProcessor.controls.bias = 0;
            }

            inputTemp.add(channelData[sample] * driveSmoother1.getNextValue());
            // distortion
            if (mode < 9)
            {
                if (multibandFocus1) // 2020/12/6
                {
                    distortionProcessor.controls.drive = driveSmoother1.getNextValue();
                }
                else
                {
                    distortionProcessor.controls.drive = driveSmoother1.getNextValue(); // just for testing
                }
                
                
                // distortionProcessor.controls.output = outputSmoother.getNextValue();
                channelData[sample] = distortionProcessor.distortionProcess(channelData[sample]);
            }

            // rectification
            updateRectification();
            channelData[sample] = distortionProcessor.rectificationProcess(channelData[sample]);
        }
        if (mode == 9)
        {
            // left channel diode
            if (channel == 0)
            {
                VdiodeL = diodeClipper(inputTemp, getSampleRate(), VdiodeL, VinL, rootL, R1L);
            }
            // right channel diode
            else if (channel == 1)
            {
                VdiodeR = diodeClipper(inputTemp, getSampleRate(), VdiodeR, VinR, rootR, R1R);
            }

            for (int sample = 0; sample < blockOutput.getNumSamples(); ++sample)
            {
                channelData[sample] = inputTemp[sample];
                //channelData[sample] *= outputSmoother.getNextValue();
                // rectification
                updateRectification();
                channelData[sample] = distortionProcessor.rectificationProcess(channelData[sample]);
            }
            inputTemp.clear();
        }
    }

    // oversampling
    if (*treeState.getRawParameterValue(HQ_ID)) // oversampling
    {
        oversamplingHQ->processSamplesDown(blockInput);
    }
    //else
    //{
    //    oversampling->processSamplesDown(blockInput);
    //}

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {

        auto *channelData = buffer.getWritePointer(channel);

        juce::Range<float> range = buffer.findMinMax(channel, 0, buffer.getNumSamples());
        float min = range.getStart();
        float max = range.getEnd();
        float magnitude = range.getLength() / 2.f;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // centralization
            if (mode == 9 || recSmoother.getNextValue() > 0)
            {
                centralSmoother.setTargetValue((max + min) / 2.f);
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
            channelData[sample] *= outputSmoother1.getNextValue();
        }
    }

    // downsample
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            int rateDivide = *treeState.getRawParameterValue(DOWNSAMPLE_ID);
            //int rateDivide = (distortionProcessor.controls.drive - 1) / 63.f * 99.f + 1; //range(1,100)
            if (rateDivide > 1)
            {
                if (sample % rateDivide != 0)
                    channelData[sample] = channelData[sample - sample % rateDivide];
            }
        }
    }

    // width
    /**
    M = (L+R)/sqrt(2);   // obtain mid-signal from left and right
    S = (L-R)/sqrt(2);   // obtain side-signal from left and right

    // amplify mid and side signal seperately:
    M *= 2*(1-width);
    S *= 2*width;

    L = (M+S)/sqrt(2);   // obtain left signal from mid and side
    R = (M-S)/sqrt(2);   // obtain right signal from mid and side
     
     ======

    mSignal = 0.5 * (left + right);
    sSignal = left - right;

    float pan; // [-1; +1]
    left  = 0.5 * (1.0 + pan) * mSignal + sSignal;
    right = 0.5 * (1.0 - pan) * mSignal - sSignal;
    */
    
    auto* channeldataL = buffer.getWritePointer(0);
    auto* channeldataR = buffer.getWritePointer(1);
    auto* midBuffer = midSideBuffer.getWritePointer(0);
    auto* sideBuffer = midSideBuffer.getWritePointer(1);
    float width = 0.5f;
    float pan = 0.f;
    
//    for (int i = 0; i < buffer.getNumSamples(); i++)
//    {
//
//        midBuffer[i] = (channeldataL[i] + channeldataR[i]) * 0.5f;
//        sideBuffer[i] = (channeldataL[i] - channeldataR[i]);
//        midBuffer[i] *= 2 * (1 - width);
//        sideBuffer[i] *= 2 * width;
//        channeldataL[i] = (midBuffer[i] + sideBuffer[i]) / sqrt(2);
//        channeldataR[i] = (midBuffer[i] - sideBuffer[i]) / sqrt(2);
//
//    }

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float tmp = 1 / fmax(1 + width, 2);
        float coef_M = 1 * tmp;
        float coef_S = width * tmp;

        float mid = (channeldataL[i] + channeldataR[i])*coef_M;
        float sides = (channeldataR[i] - channeldataL[i])*coef_S;

        channeldataL[i] = mid - sides;
        channeldataR[i] = mid + sides;

    }
    
    //post-filter
    bool postButton = *treeState.getRawParameterValue(POST_ID);
    if (postButton)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        filterIIR.process(juce::dsp::ProcessContextReplacing<float>(block));
        //filterIIR.process(dsp::ProcessContextReplacing<float>(blockOutput));
        updateFilter();
    }

    // mix control
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        auto *cleanSignal = dryBuffer.getWritePointer(channel);
        //auto* cleanSignal = mDelayBuffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float smoothMixValue = mixSmoother1.getNextValue();
            // channelData[sample] = (1.f - mix) * cleanSignal[sample] + mix * channelData[sample];
            //channelData[sample] = (1.f - smoothMixValue) * cleanSignal[sample] + smoothMixValue * channelData[sample];
            channelData[sample] = (1.f - smoothMixValue) * mDelay.process(cleanSignal[sample], channel, buffer.getNumSamples()) + smoothMixValue * channelData[sample];
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

    
    // Spectrum
    if (buffer.getNumChannels() > 0)
    {
        auto* channelData = buffer.getReadPointer(0);

        for (auto i = 0; i < buffer.getNumSamples(); ++i)
            spectrum_processor.pushNextSampleIntoFifo(channelData[i]);
//            spectrum_processor.pushNextSampleIntoFifo (std::sin(0.14*i));
    }
    
    
    
    dryBuffer.clear();

    // ff output meter
    outputMeterSource.measureBlock(buffer);
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

    // 1. save treestate
    auto state = treeState.copyState();
    std::unique_ptr<juce::XmlElement> treeStateXml(state.createXml());
    xmlState.insertChildElement(treeStateXml.release(), xmlIndex++);

    // 2. save current preset ID
    std::unique_ptr<juce::XmlElement> currentStateXml{new juce::XmlElement{"currentPresetID"}};
    currentStateXml->setAttribute("ID", statePresets.getCurrentPresetId());
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
            int presetID = xmlCurrentState->getIntAttribute("ID", 0);
            statePresets.setCurrentPresetId(presetID);
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new FireAudioProcessor();
}

// Rectification selection
void FireAudioProcessor::updateRectification()
{
    float rec = *treeState.getRawParameterValue(REC_ID);
    recSmoother.setTargetValue(rec);
    distortionProcessor.controls.rectification = recSmoother.getNextValue();
}

// Filter selection
void FireAudioProcessor::updateFilter()
{
    float cutoff = *treeState.getRawParameterValue(CUTOFF_ID);
    float res = *treeState.getRawParameterValue(RES_ID);
    bool lowButton = *treeState.getRawParameterValue(LOW_ID);
    bool bandButton = *treeState.getRawParameterValue(BAND_ID);
    bool highButton = *treeState.getRawParameterValue(HIGH_ID);
    float color = *treeState.getRawParameterValue(COLOR_ID);
    float centreFreqSausage;

    colorSmoother.setTargetValue(color);
    color = colorSmoother.getNextValue();

    // smooth cutoff value
    cutoffSmoother.setTargetValue(cutoff);
    cutoff = cutoffSmoother.getNextValue();

    // 20 - 800 - 8000
    if (color < 0.5)
    {
        centreFreqSausage = 20 + color * 1560;
    }
    else
    {
        centreFreqSausage = 800 + (color - 0.5) * 14400;
    }

    *filterColor.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), centreFreqSausage, 0.4, color + 1);

    if (lowButton == true)
    {
        *filterIIR.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), cutoff, res);
    }
    else if (bandButton == true)
    {
        *filterIIR.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(getSampleRate(), cutoff, res);
    }
    else if (highButton == true)
    {
        *filterIIR.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), cutoff, res);
    }
}

bool FireAudioProcessor::isSlient(juce::AudioBuffer<float> buffer)
{
    if (buffer.getMagnitude(0, buffer.getNumSamples()) == 0)
        return true;
    else
        return false;
}

float FireAudioProcessor::getNewDrive()
{
    return newDrive1;
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
    return spectrum_processor.fftSize/2 ;
}

bool FireAudioProcessor::isFFTBlockReady()
{
    return spectrum_processor.nextFFTBlockReady;
}

void FireAudioProcessor::processFFT()
{
    spectrum_processor.doProcessing();
    spectrum_processor.nextFFTBlockReady = false;
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
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DYNAMIC_ID1, DYNAMIC_NAME1, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DYNAMIC_ID2, DYNAMIC_NAME2, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DYNAMIC_ID3, DYNAMIC_NAME3, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DYNAMIC_ID4, DYNAMIC_NAME4, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID1, WIDTH_NAME1, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 50.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID2, WIDTH_NAME2, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 50.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID3, WIDTH_NAME3, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 50.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(WIDTH_ID4, WIDTH_NAME4, juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 50.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID1, OUTPUT_NAME1, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID2, OUTPUT_NAME2, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID3, OUTPUT_NAME3, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(OUTPUT_ID4, OUTPUT_NAME4, juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID1, MIX_NAME1, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID2, MIX_NAME2, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID3, MIX_NAME3, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(MIX_ID4, MIX_NAME4, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(COLOR_ID, COLOR_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(BIAS_ID, BIAS_NAME, juce::NormalisableRange<float>(-0.5f, 0.5f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(DOWNSAMPLE_ID, DOWNSAMPLE_NAME, juce::NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(REC_ID, REC_NAME, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    
    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(CUTOFF_ID, CUTOFF_NAME, cutoffRange, 20.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(RES_ID, RES_NAME, juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(OFF_ID, OFF_NAME, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(PRE_ID, PRE_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(POST_ID, POST_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(LOW_ID, LOW_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_ID, BAND_NAME, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(HIGH_ID, HIGH_NAME, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(WINDOW_LEFT_ID, WINDOW_LEFT_NAME, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(WINDOW_RIGHT_ID, WINDOW_RIGHT_NAME, false));
    
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(LINENUM_ID, LINENUM_NAME, 0, 3, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(FREQ_ID1, FREQ_NAME1, 0, 22100, 500));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(FREQ_ID2, FREQ_NAME2, 0, 22100, 2000));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>(FREQ_ID3, FREQ_NAME3, 0, 22100, 9000));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_STATE_ID1, BAND_STATE_NAME1, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_STATE_ID2, BAND_STATE_NAME2, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_STATE_ID3, BAND_STATE_NAME3, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_STATE_ID4, BAND_STATE_NAME4, true));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID1, BAND_SOLO_NAME1, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID2, BAND_SOLO_NAME2, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID3, BAND_SOLO_NAME3, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_SOLO_ID4, BAND_SOLO_NAME4, true));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID1, BAND_FOCUS_NAME1, true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID2, BAND_FOCUS_NAME2, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID3, BAND_FOCUS_NAME3, false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(BAND_FOCUS_ID4, BAND_FOCUS_NAME4, false));
    
    return {parameters.begin(), parameters.end()};
}
