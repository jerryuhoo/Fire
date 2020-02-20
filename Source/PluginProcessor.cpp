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
                         ),treeState(*this, nullptr, "PARAMETERS",
                {
                    std::make_unique<AudioParameterFloat>("mode", "Mode", NormalisableRange<float>(1, 8, 1), 1),
                    std::make_unique<AudioParameterFloat>("inputGain", "InputGain", NormalisableRange<float>(-36.0f, 36.0f, 0.1f), 0.0f),
                    std::make_unique<AudioParameterFloat>("drive", "Drive", NormalisableRange<float>(1.0f, 64.0f, 0.01f), 1.0f),
                    std::make_unique<AudioParameterFloat>("outputGain", "OutputGain", NormalisableRange<float>(-48.0f, 6.0f, 0.1f), 0.0f),
                    std::make_unique<AudioParameterFloat>("mix", "Mix", NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f),
                    
                })
#endif
{
}

BloodAudioProcessor::~BloodAudioProcessor()
{
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
    inputGainValue = treeState.getRawParameterValue("inputGain");
    // previousGainInput = (Decibels::decibelsToGain(*inputGainValue));
    previousGainInput = (float)*inputGainValue;
    previousGainInput = Decibels::decibelsToGain(previousGainInput);
    
    outputGainValue = treeState.getRawParameterValue("outputGain");
    previousGainOutput = (float)*outputGainValue;
    previousGainOutput = Decibels::decibelsToGain(previousGainOutput);
    
    previousDriveValue = treeState.getRawParameterValue("drive");
    previousDrive = *previousDriveValue;
    
    driveSmoother.reset(sampleRate, 0.05); //0.05 second is rampLength, which means increasing to targetvalue needs 0.05s.
    driveSmoother.setCurrentAndTargetValue(previousDrive);
    
    outputSmoother.reset(sampleRate, 0.05);
    outputSmoother.setCurrentAndTargetValue(previousGainOutput);
    
    visualiser.clear();
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
    
    
    
    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    //    float menuChoiceValue = 1.0f;
    auto modeValue = treeState.getRawParameterValue("mode");
    float mode = *modeValue;

    auto inputGainValue = treeState.getRawParameterValue("inputGain");
    float currentGainInput = *inputGainValue;
    currentGainInput = Decibels::decibelsToGain(currentGainInput);

    // (deleted drive)
    auto driveValue = treeState.getRawParameterValue("drive");
    float drive = *driveValue;

    auto outputGainValue = treeState.getRawParameterValue("outputGain");
    float currentGainOutput = *outputGainValue;
    currentGainOutput = Decibels::decibelsToGain(currentGainOutput);

    auto mixValue = treeState.getRawParameterValue("mix");
    float mix = *mixValue;
    
    distortionProcessor.controls.mode = mode;
    // distortionProcessor.controls.drive = drive;
    // distortionProcessor.controls.output = currentGainOutput;
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
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        
        float* data = buffer.getWritePointer(channel);
        
        
        auto *channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            distortionProcessor.controls.drive = driveSmoother.getNextValue();
            distortionProcessor.controls.output = outputSmoother.getNextValue();
            channelData[sample] = distortionProcessor.distortionProcess(channelData[sample]);
            
            if (mode == 8) {
                
                int rateDivide = (distortionProcessor.controls.drive - 1) / 63.f * 200.f;
                if (rateDivide > 1)
                {
                    if (sample%rateDivide != 0) data[sample] = data[sample - sample%rateDivide];
                }
            }
        }

        //        visualiser.pushBuffer(buffer);
    }


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
