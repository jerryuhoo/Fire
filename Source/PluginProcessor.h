/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Distortion.h"
//==============================================================================
/**
*/

// temporary use this method. This is not ideal. You should put your visualiser in Editor not in Processor
// probably use fifo
class Visualiser : public AudioVisualiserComponent
{
public:
    Visualiser() : AudioVisualiserComponent(2)
    {
        setBufferSize(128);
        setSamplesPerBlock(16);
        setColours(Colour(50, 0, 0), Colours::red);
    }
};

class BloodAudioProcessor : public AudioProcessor
{

public:
    //==============================================================================
    BloodAudioProcessor();
    ~BloodAudioProcessor();

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(AudioBuffer<float> &, MidiBuffer &) override;

    //==============================================================================
    AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String &newName) override;

    //==============================================================================
    void getStateInformation(MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    AudioProcessorValueTreeState treeState;
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ff meter
    // ff meter
    FFAU::LevelMeterSource &getInputMeterSource()
    {
        return inputMeterSource;
    }
    FFAU::LevelMeterSource &getOutputMeterSource()
    {
        return outputMeterSource;
    }

    // temporary use this method. This is not ideal. You should put your visualiser in Editor not in Processor
    // probably use fifo
    Visualiser visualiser;

private:
    //==============================================================================

    //ff meter
    FFAU::LevelMeterSource inputMeterSource;
    FFAU::LevelMeterSource outputMeterSource;
    
    float previousGainInput;
    float previousGainOutput;
    float previousDrive;
    
    std::atomic<float> *inputGainValue = nullptr;
    std::atomic<float> *outputGainValue = nullptr;
    std::atomic<float> *previousDriveValue = nullptr;
    
    // fix the artifacts (also called zipper noise)
    SmoothedValue<float> driveSmoother;
    SmoothedValue<float> outputSmoother;
    
    Distortion distortionProcessor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BloodAudioProcessor)
};
