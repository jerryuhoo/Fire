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
        setColours(Colour(201, 98, 97), Colour(234, 191, 191));
        setBoundsInset(BorderSize<int>(2, 2, 2, 2));
    }
};

class FireAudioProcessor : public AudioProcessor
{

public:
    //==============================================================================
    FireAudioProcessor();
    ~FireAudioProcessor();

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
    
    // Rectification
    void updateRectification();

    // filter
    void updateFilter();

    AudioProcessorValueTreeState treeState;
    AudioProcessorValueTreeState::ParameterLayout createParameters();

    // ff meter
    // ff meter
    foleys::LevelMeterSource &getInputMeterSource()
    {
        return inputMeterSource;
    }
    foleys::LevelMeterSource &getOutputMeterSource()
    {
        return outputMeterSource;
    }

    // temporary use this method. This is not ideal. You should put your visualiser in Editor not in Processor
    // probably use fifo
    Visualiser visualiser;

private:
    //==============================================================================

    // ff meter
    foleys::LevelMeterSource inputMeterSource;
    foleys::LevelMeterSource outputMeterSource;

    // dry audio buffer
    AudioBuffer<float> dryBuffer;
    // dsp::AudioBlock<float> blockOutput;
    dsp::ProcessSpec spec;

    // filter
    dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>> filterIIR;
    
    // fix the artifacts (also called zipper noise)
    float previousGainInput;
    float previousGainOutput;
    float previousDrive;
    float previousMix;
    
    SmoothedValue<float> driveSmoother;
    SmoothedValue<float> outputSmoother;
    SmoothedValue<float> mixSmoother;
    
    Distortion distortionProcessor;

    // oversampling
    std::unique_ptr<dsp::Oversampling<float>> oversampling; // normal use 2x
    std::unique_ptr<dsp::Oversampling<float>> oversamplingHQ; // HQ use 4x
    int oversampleFactor = 1;

    // mode 8 diode================
    Array<float> inputTemp;
    float VdiodeL;
    float VdiodeR;
    float RiL;
    float RiR;
    VoltageSource VinL;
    VoltageSource VinR;
    Resistor R1L;
    Resistor R1R;
    Capacitor C1L;
    Capacitor C1R;
    Serie RCL;
    Serie RCR;
    Serie rootL;
    Serie rootR;
    // mode 9 diode=================

    // dsp::AudioBlock<float> blockOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessor)
};
