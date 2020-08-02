/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Distortion.h"
#include "Preset.h"
#include "Delay.h"

#define COLOUR1 Colour(244, 208, 63)
#define COLOUR6 Colour(45, 40, 40)
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
        setColours(COLOUR6, COLOUR1);
        setBoundsInset(BorderSize<int>(2, 2, 2, 2));
    }

    void paintChannel (Graphics& g, Rectangle<float> area,
                                                 const Range<float>* levels, int numLevels, int nextSample) override
    {
        Path p;
        getChannelAsPath (p, levels, numLevels, nextSample);
        ColourGradient grad(COLOUR1, area.getX() + area.getWidth() / 2, area.getY() + area.getHeight() / 2,
            COLOUR6, area.getX(), area.getY() + area.getHeight() / 2, true);
        g.setGradientFill(grad);
        g.fillPath (p, AffineTransform::fromTargetPoints (0.0f, -1.0f,               area.getX(), area.getY(),
                                                          0.0f, 1.0f,                area.getX(), area.getBottom(),
                                                          (float) numLevels, -1.0f,  area.getRight(), area.getY()));
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
    
    bool isSlient(AudioBuffer<float> buffer);
    
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
    
    // save presets
    state::StateAB stateAB;
    state::StatePresets statePresets;
    
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
    dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>> filterColor;
    
    // fix the artifacts (also called zipper noise)
    //float previousGainInput;
    float previousGainOutput;
    float previousDrive;
    float previousColor;
    float previousCutoff;
    float previousMix;
    
    SmoothedValue<float> driveSmoother;
    SmoothedValue<float> outputSmoother;
    SmoothedValue<float> colorSmoother;
    SmoothedValue<float> cutoffSmoother;
    SmoothedValue<float> recSmoother;
    SmoothedValue<float> biasSmoother;
    SmoothedValue<float> mixSmoother;
    SmoothedValue<float> centralSmoother;
    SmoothedValue<float> normalSmoother;
    
    Distortion distortionProcessor;

    // oversampling
    std::unique_ptr<dsp::Oversampling<float>> oversampling; // normal use 2x
    std::unique_ptr<dsp::Oversampling<float>> oversamplingHQ; // HQ use 4x
    int oversampleFactor = 1;
    
    // oversampling delay, set to dry buffer
    Delay mDelay{0};
    
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
