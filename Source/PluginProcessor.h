/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
//==============================================================================
/**
*/
class BloodAudioProcessor  : public AudioProcessor
{

public:
    //==============================================================================
    BloodAudioProcessor();
    ~BloodAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
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
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    AudioProcessorValueTreeState treeState;
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // ff meter
    // ff meter
    FFAU::LevelMeterSource& getInputMeterSource()
    {
        return inputMeterSource;
    }
    FFAU::LevelMeterSource& getOutputMeterSource()
    {
        return outputMeterSource;
    }
    
private:
    //==============================================================================
    
    //ff meter
    FFAU::LevelMeterSource inputMeterSource;
    FFAU::LevelMeterSource outputMeterSource;
    float previousGainInput;
    float previousGainOutput;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BloodAudioProcessor)
};
