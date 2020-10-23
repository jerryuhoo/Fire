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

    previousGainOutput = (float)*treeState.getRawParameterValue("outputGain");
    previousGainOutput = juce::Decibels::decibelsToGain(previousGainOutput);
    previousDrive = *treeState.getRawParameterValue("drive");
    previousCutoff = (float)*treeState.getRawParameterValue("cutoff");
    previousColor = (float)*treeState.getRawParameterValue("color");
    previousMix = (float)*treeState.getRawParameterValue("mix");
    newDrive = 0;

    driveSmoother.reset(sampleRate, 0.05); //0.05 second is rampLength, which means increasing to targetvalue needs 0.05s.
    driveSmoother.setCurrentAndTargetValue(previousDrive);

    outputSmoother.reset(sampleRate, 0.05);
    outputSmoother.setCurrentAndTargetValue(previousGainOutput);

    cutoffSmoother.reset(sampleRate, 0.001);
    cutoffSmoother.setCurrentAndTargetValue(previousCutoff);

    recSmoother.reset(sampleRate, 0.05);
    recSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue("rec"));

    biasSmoother.reset(sampleRate, 0.001);
    biasSmoother.setCurrentAndTargetValue(*treeState.getRawParameterValue("bias"));

    colorSmoother.reset(sampleRate, 0.001);
    colorSmoother.setCurrentAndTargetValue(previousColor);

    mixSmoother.reset(sampleRate, 0.05);
    mixSmoother.setCurrentAndTargetValue(previousMix);

    centralSmoother.reset(sampleRate, 0.1);
    centralSmoother.setCurrentAndTargetValue(0);

    normalSmoother.reset(sampleRate, 0.5);
    normalSmoother.setCurrentAndTargetValue(1);

    // clear visualiser
    visualiser.clear();

    // dry buffer init
    dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    dryBuffer.clear();

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
    int mode = *treeState.getRawParameterValue("mode");

    //float currentGainInput = *treeState.getRawParameterValue("inputGain");
    //currentGainInput = Decibels::decibelsToGain(currentGainInput);

    float drive = *treeState.getRawParameterValue("drive");

    float color = *treeState.getRawParameterValue("color");

    float currentGainOutput = *treeState.getRawParameterValue("outputGain");
    currentGainOutput = juce::Decibels::decibelsToGain(currentGainOutput);

    float mix = *treeState.getRawParameterValue("mix");
    float bias = *treeState.getRawParameterValue("bias");

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

    distortionProcessor.controls.protection = *treeState.getRawParameterValue("safe");

    if (distortionProcessor.controls.protection == true)
    {
        if (sampleMaxValue * drive > 2.f)
        {
            drive = 2.f / sampleMaxValue + 0.1 * std::log2f(drive);
        }
    }
    newDrive = drive;

    // set zipper noise smoother target
    driveSmoother.setTargetValue(drive);
    outputSmoother.setTargetValue(currentGainOutput);
    mixSmoother.setTargetValue(mix);
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

    // pre-filter
    bool preButton = *treeState.getRawParameterValue("pre");
    if (preButton)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        filterIIR.process(juce::dsp::ProcessContextReplacing<float>(block));
        //filterIIR.process(dsp::ProcessContextReplacing<float>(blockOutput));
        updateFilter();
    }

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
    juce::dsp::AudioBlock<float> blockInput(buffer);
    juce::dsp::AudioBlock<float> blockOutput;
    // oversampling

    if (*treeState.getRawParameterValue("hq"))
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

            inputTemp.add(channelData[sample] * driveSmoother.getNextValue());
            // distortion
            if (mode < 9)
            {
                distortionProcessor.controls.drive = driveSmoother.getNextValue();
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
    if (*treeState.getRawParameterValue("hq")) // oversampling
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
            channelData[sample] *= outputSmoother.getNextValue();
        }
    }

    // downsample
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            int rateDivide = *treeState.getRawParameterValue("downSample");
            //int rateDivide = (distortionProcessor.controls.drive - 1) / 63.f * 99.f + 1; //range(1,100)
            if (rateDivide > 1)
            {
                if (sample % rateDivide != 0)
                    channelData[sample] = channelData[sample - sample % rateDivide];
            }
        }
    }

    //post-filter
    bool postButton = *treeState.getRawParameterValue("post");
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
            float smoothMixValue = mixSmoother.getNextValue();
            // channelData[sample] = (1.f - mix) * cleanSignal[sample] + mix * channelData[sample];
            //channelData[sample] = (1.f - smoothMixValue) * cleanSignal[sample] + smoothMixValue * channelData[sample];
            channelData[sample] = (1.f - smoothMixValue) * mDelay.process(cleanSignal[sample], channel, buffer.getNumSamples()) + smoothMixValue * channelData[sample];
            // mDelay is delayed clean signal
        }
    }

    dryBuffer.clear();

    // ff output meter
    outputMeterSource.measureBlock(buffer);
    visualiser.pushBuffer(buffer);
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

    int xmlIndex = 0;
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName("state"))
    {
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
    float rec = *treeState.getRawParameterValue("rec");
    recSmoother.setTargetValue(rec);
    distortionProcessor.controls.rectification = recSmoother.getNextValue();
}

// Filter selection
void FireAudioProcessor::updateFilter()
{
    float cutoff = *treeState.getRawParameterValue("cutoff");
    float res = *treeState.getRawParameterValue("res");
    bool lowButton = *treeState.getRawParameterValue("low");
    bool bandButton = *treeState.getRawParameterValue("band");
    bool highButton = *treeState.getRawParameterValue("high");
    float color = *treeState.getRawParameterValue("color");
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
    return newDrive;
}

juce::AudioProcessorValueTreeState::ParameterLayout FireAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterInt>("mode", "Mode", 0, 9, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("hq", "Hq", false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("linked", "Linked", true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("safe", "Safe", true));

    //parameters.push_back(std::make_unique<AudioParameterFloat>("inputGain", "InputGain", NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("color", "Color", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("bias", "Bias", juce::NormalisableRange<float>(-0.5f, 0.5f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("downSample", "DownSample", juce::NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("rec", "Rec", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "OutputGain", juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre(1000.f);
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("cutoff", "Cutoff", cutoffRange, 20.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("res", "Res", juce::NormalisableRange<float>(1.0f, 5.0f, 0.1f), 1.0f));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>("off", "Off", true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("pre", "Pre", false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("post", "Post", false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("low", "Low", false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("band", "Band", false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("high", "High", true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("windowLeft", "WindowLeft", true));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("windowRight", "WindowRight", false));
    
    return {parameters.begin(), parameters.end()};
}
