/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BloodAudioProcessor::BloodAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
        .withInput("Input", AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", AudioChannelSet::stereo(), true)
#endif
    ), treeState(*this, nullptr, "PARAMETERS", createParameters())
    , VinL(500.f, 0.f) // VinL(0.f, 500.f)
    , VinR(500.f, 0.f)
    , R1L(80.0f)
    , R1R(80.0f)
    , C1L(3.5e-5, getSampleRate())
    , C1R(3.5e-5, getSampleRate())
    , RCL(&R1L, &C1L)
    , RCR(&R1R, &C1R)
    , rootL(&VinL, &RCL)
    , rootR(&VinR, &RCR)
#endif
{
    // factor = 2 means 2^2 = 4, 4x oversampling    
    oversampling.reset(new dsp::Oversampling<float>(getTotalNumInputChannels(), 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false));
    oversamplingHQ.reset(new dsp::Oversampling<float>(getTotalNumInputChannels(), 2, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false));
}

BloodAudioProcessor::~BloodAudioProcessor()
{
    oversampling.reset();
}

//==============================================================================
const String BloodAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BloodAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool BloodAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool BloodAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double BloodAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BloodAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int BloodAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BloodAudioProcessor::setCurrentProgram(int index)
{
}

const String BloodAudioProcessor::getProgramName(int index)
{
    return {};
}

void BloodAudioProcessor::changeProgramName(int index, const String &newName)
{
}

//==============================================================================
void BloodAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    // fix the artifacts (also called zipper noise)
    previousGainInput = (float)*treeState.getRawParameterValue("inputGain");
    previousGainInput = Decibels::decibelsToGain(previousGainInput);
    
    previousGainOutput = (float)*treeState.getRawParameterValue("outputGain");
    previousGainOutput = Decibels::decibelsToGain(previousGainOutput);
    
    previousDrive = *treeState.getRawParameterValue("drive");
    
    driveSmoother.reset(sampleRate, 0.05); //0.05 second is rampLength, which means increasing to targetvalue needs 0.05s.
    driveSmoother.setCurrentAndTargetValue(previousDrive);
    
    outputSmoother.reset(sampleRate, 0.05);
    outputSmoother.setCurrentAndTargetValue(previousGainOutput);
    
    // clear visualiser
    visualiser.clear();
    
    // dry buffer init
    dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    dryBuffer.clear();

    // oversampling init
    oversampling->reset();
    oversampling->initProcessing(static_cast<size_t> (samplesPerBlock));
    oversamplingHQ->reset();
    oversamplingHQ->initProcessing(static_cast<size_t> (samplesPerBlock));

    // dsp init
    // dsp::ProcessSpec spec;

    int newBlockSize = (int)oversampling->getOversamplingFactor() * samplesPerBlock;

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
    spec.maximumBlockSize = newBlockSize;
    spec.numChannels = getMainBusNumOutputChannels();

    // filter init
    filterIIR.reset();
    updateFilter();
    filterIIR.prepare(spec);
    
    
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
}

void BloodAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BloodAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
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

void BloodAudioProcessor::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages)
{
    ScopedNoDenormals noDenormals;
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
    
    // TODO------put this in a new function--------

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
    // TODO------put this in a new function--------
    dsp::AudioBlock<float> blockInput(buffer);
    dsp::AudioBlock<float> blockOutput;
    // oversampling
    //if (*treeState.getRawParameterValue("hq")) // oversampling
    //{
        //dsp::AudioBlock<float> blockInput(buffer);
        // blockOutput.clear();
        // blockOutput = oversampling->processSamplesUp(blockInput);
    //blockOutput.clear();
    if (*treeState.getRawParameterValue("hq"))
    {
        blockInput = blockInput.getSubBlock(0, buffer.getNumSamples());
        blockOutput = oversamplingHQ->processSamplesUp(blockInput);
    }
    else
    {
        //dsp::AudioBlock<float> blockOutput = oversampling->processSamplesUp(blockInput);
        blockOutput = blockInput.getSubBlock(0, buffer.getNumSamples());
    }
        
        
        //}
    
    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    //    float menuChoiceValue = 1.0f;

    int mode = *treeState.getRawParameterValue("mode");

    float currentGainInput = *treeState.getRawParameterValue("inputGain");
    currentGainInput = Decibels::decibelsToGain(currentGainInput);

    float drive = *treeState.getRawParameterValue("drive");

    float currentGainOutput = *treeState.getRawParameterValue("outputGain");
    currentGainOutput = Decibels::decibelsToGain(currentGainOutput);

    float mix = *treeState.getRawParameterValue("mix");
    
    distortionProcessor.controls.mode = mode;
    distortionProcessor.controls.mix = mix;
    
    // ff input meter
    inputMeterSource.measureBlock(buffer);

    // input volume fix
    if (currentGainInput == previousGainInput)
    {
        buffer.applyGain(currentGainInput);
    }
    else
    {
        buffer.applyGainRamp(0, buffer.getNumSamples(), previousGainInput, currentGainInput);
        previousGainInput = currentGainInput;
    }
    
    // set zipper noise smoother target
    driveSmoother.setTargetValue(drive);
    outputSmoother.setTargetValue(currentGainOutput);
    
    
    // save clean signal
    dryBuffer.makeCopyOf(buffer);

    // pre-filter
    bool preButton = *treeState.getRawParameterValue("pre");
    if (preButton)
    {
        //dsp::AudioBlock<float> block (buffer);
        //filterIIR.process(dsp::ProcessContextReplacing<float>(block));
        filterIIR.process(dsp::ProcessContextReplacing<float>(blockOutput));
        updateFilter();
    }

//    bool lowButton = *treeState.getRawParameterValue("low");
//    bool bandButton = *treeState.getRawParameterValue("band");
//    bool highButton = *treeState.getRawParameterValue("high");
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {

        // float* data = buffer.getWritePointer(channel);
        // auto *channelData = buffer.getWritePointer(channel);
        auto* channelData = blockOutput.getChannelPointer(channel);
        inputTemp.clear();
        for (int sample = 0; sample < blockOutput.getNumSamples(); ++sample)
        {
            inputTemp.add(channelData[sample] * driveSmoother.getNextValue());

            // distortion
            if (mode < 8) 
            {
                distortionProcessor.controls.drive = driveSmoother.getNextValue();
                distortionProcessor.controls.output = outputSmoother.getNextValue();
                channelData[sample] = distortionProcessor.distortionProcess(channelData[sample]);
            }

            // downsample
            int rateDivide = *treeState.getRawParameterValue("downSample");
            //int rateDivide = (distortionProcessor.controls.drive - 1) / 63.f * 99.f + 1; //range(1,100)
            if (rateDivide > 1)
            {
                if (sample%rateDivide != 0) 
                    channelData[sample] = channelData[sample - sample%rateDivide];
            }
            
            // rectification
            updateRectification();
            channelData[sample] = distortionProcessor.rectificationProcess(channelData[sample]);
        }
        if (mode == 8)
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
                channelData[sample] *= outputSmoother.getNextValue();
                // rectification
                updateRectification();
                channelData[sample] = distortionProcessor.rectificationProcess(channelData[sample]);
            }
            inputTemp.clear();
        }
        
        //        visualiser.pushBuffer(buffer);
    }

    
    //post-filter
    bool postButton = *treeState.getRawParameterValue("post");
    if (postButton)
    {
        //dsp::AudioBlock<float> block (buffer);
        //filterIIR.process(dsp::ProcessContextReplacing<float>(block));
        filterIIR.process(dsp::ProcessContextReplacing<float>(blockOutput));
        updateFilter();
    }
    
    // oversampling
    if (*treeState.getRawParameterValue("hq")) // oversampling
    {
        oversamplingHQ->processSamplesDown(blockInput);
    }
    //else
    //{
    //    oversampling->processSamplesDown(blockInput);
    //}
    
    // mix control
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* cleanSignal = dryBuffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = (1.f - mix) * cleanSignal[sample] + mix * channelData[sample];
        }
        
    }

    dryBuffer.clear();

    // ff output meter
    outputMeterSource.measureBlock(buffer);
    visualiser.pushBuffer(buffer);
}

//==============================================================================
bool BloodAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor *BloodAudioProcessor::createEditor()
{
    return new BloodAudioProcessorEditor(*this);
}

//==============================================================================
void BloodAudioProcessor::getStateInformation(MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    //MemoryOutputStream (destData, true).writeFloat (*gain);
    auto state = treeState.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BloodAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    //*gain = MemoryInputStream (data, static_cast<size_t> (sizeInBytes), false).readFloat();
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(treeState.state.getType()))
            treeState.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new BloodAudioProcessor();
} 

// Rectification selection
void BloodAudioProcessor::updateRectification()
{
    bool recOffButton = *treeState.getRawParameterValue("recOff");
    bool recHalfButton = *treeState.getRawParameterValue("recHalf");
    bool recFullButton = *treeState.getRawParameterValue("recFull");
    if (recOffButton == true)
    {
        distortionProcessor.controls.rectification = 0;
    }
    else if (recHalfButton == true)
    {
        distortionProcessor.controls.rectification = 1;
    }
    else if (recFullButton == true)
    {
        distortionProcessor.controls.rectification = 2;
    }
}

// Filter selection
void BloodAudioProcessor::updateFilter()
{
    float cutoff = *treeState.getRawParameterValue("cutoff");
    float res = *treeState.getRawParameterValue("res");
    bool lowButton = *treeState.getRawParameterValue("low");
    bool bandButton = *treeState.getRawParameterValue("band");
    bool highButton = *treeState.getRawParameterValue("high");
    if (lowButton == true)
    {
        *filterIIR.state = *dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), cutoff, res);
    }
    else if (bandButton == true)
    {
        *filterIIR.state = *dsp::IIR::Coefficients<float>::makeBandPass(getSampleRate(), cutoff, res);
    }
    else if (highButton == true)
    {
        *filterIIR.state = *dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), cutoff, res);
    }
}



AudioProcessorValueTreeState::ParameterLayout BloodAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> parameters;
                 
    parameters.push_back(std::make_unique<AudioParameterInt>("mode", "Mode", 0, 8, 0));
    parameters.push_back(std::make_unique<AudioParameterFloat>("inputGain", "InputGain", NormalisableRange<float>(-36.0f, 36.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("drive", "Drive", NormalisableRange<float>(1.0f, 64.0f, 0.01f), 2.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("downSample", "DownSample", NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("outputGain", "OutputGain", NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("mix", "Mix", NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.f);
    parameters.push_back(std::make_unique<AudioParameterFloat>("cutoff", "Cutoff", cutoffRange, 50.0f));
    parameters.push_back(std::make_unique<AudioParameterFloat>("res", "Res", NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));
    
    parameters.push_back(std::make_unique<AudioParameterBool>("hq", "Hq", false));
    parameters.push_back(std::make_unique<AudioParameterBool>("linked", "Linked", true));
    parameters.push_back(std::make_unique<AudioParameterBool>("recOff", "RecOff", true));
    parameters.push_back(std::make_unique<AudioParameterBool>("recHalf", "RecHalf", false));
    parameters.push_back(std::make_unique<AudioParameterBool>("recFull", "RecFull", false));
    parameters.push_back(std::make_unique<AudioParameterBool>("off", "Off", false));
    parameters.push_back(std::make_unique<AudioParameterBool>("pre", "Pre", false));
    parameters.push_back(std::make_unique<AudioParameterBool>("post", "Post", true));
    parameters.push_back(std::make_unique<AudioParameterBool>("low", "Low", false));
    parameters.push_back(std::make_unique<AudioParameterBool>("band", "Band", false));
    parameters.push_back(std::make_unique<AudioParameterBool>("high", "High", true));
    
    return { parameters.begin(), parameters.end() };
}
