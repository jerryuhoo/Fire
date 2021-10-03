/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DSP/Delay.h"
#include "DSP/WidthProcessor.h"
#include "Panels/TopPanel/Preset.h"
#include "Panels/SpectrogramPanel/FFTProcessor.h"
#include "GUI/InterfaceDefines.h"
#include "Utility/AudioHelpers.h"
#include "DSP/ClippingFunctions.h"
#include "DSP/DiodeWDF.h"

//#include "GUI/LookAndFeel.h"
//#define COLOUR1 Colour(244, 208, 63)
//#define COLOUR6 Colour(45, 40, 40)
//==============================================================================

class FireAudioProcessor : public juce::AudioProcessor
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

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void processBlockBypassed (juce::AudioBuffer<float>& buffer,
                               juce::MidiBuffer& midiMessages) override;
    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // filter
    void updateFilter();

    bool isSlient(juce::AudioBuffer<float> buffer);

    // new drive after protection
    float getNewDrive(juce::String driveId);

    juce::AudioProcessorValueTreeState treeState;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    juce::Array<float> getHistoryArrayL();
    juce::Array<float> getHistoryArrayR();
    
    // save presets
    state::StateAB stateAB;
    state::StatePresets statePresets;
    
    // FFT
    float * getFFTData();
    int getFFTSize();//TMP!!!!!!!!!!!
    bool isFFTBlockReady();
    void pushDataToFFT();
    void processFFT(float * tempFFTData);
    
    // save size
    void setSavedWidth(const int width);
    void setSavedHeight(const int height);
    int getSavedWidth() const;
    int getSavedHeight() const;
    
    // bypass
    bool getBypassedState();
    
    // get number of activated lines
    // int getLineNum();
    void setLineNum(int lineNum);
    
    // VU meters
    float getInputMeterLevel(int channel);
    float getOutputMeterLevel(int channel);
    
    // drive lookandfeel
    float getReductionPrecent(juce::String safeId);
    void setReductionPrecent(juce::String safeId, float reductionPrecent);
    float getSampleMaxValue(juce::String safeId);
    void setSampleMaxValue(juce::String safeId, float sampleMaxValue);
    
    float safeMode(float drive, juce::AudioBuffer<float>& buffer, juce::String safeID);
private:
    //==============================================================================
    
    // preset id
    int presetId = 0;
    
    // line number
    int lineNum = 0;
    
    // Oscilloscope
    juce::Array<float> historyArrayL;
    juce::Array<float> historyArrayR;
    int historyLength = 400;
    
    // Spectrum
    SpectrumProcessor spectrum_processor;

    // dry audio buffer
    juce::AudioBuffer<float> dryBuffer;
    // wet audio buffer
    juce::AudioBuffer<float> wetBuffer;
    
    // dsp::AudioBlock<float> blockOutput;
    juce::dsp::ProcessSpec spec;

    // filter
    MonoChain leftChain;
    MonoChain rightChain;

    void updateLowCutFilters(const ChainSettings& chainSettings);
    void updateHighCutFilters(const ChainSettings& chainSettings);
    void updatePeakFilter(const ChainSettings& chainSettings);

//    juce::dsp::ProcessorDuplicator<Filter, Coefficients> filterIIR;

    // fix the artifacts (also called zipper noise)
    //float previousGainInput;
    float previousOutput1 = 0.0f;
    float previousOutput2 = 0.0f;
    float previousOutput3 = 0.0f;
    float previousOutput4 = 0.0f;
    float previousOutput = 0.0f;
    float previousDrive1 = 0.0f;
    float previousDrive2 = 0.0f;
    float previousDrive3 = 0.0f;
    float previousDrive4 = 0.0f;
    float previousMix1 = 0.0f;
    float previousMix2 = 0.0f;
    float previousMix3 = 0.0f;
    float previousMix4 = 0.0f;
    float previousMix = 0.0f;
    float previousLowcutFreq = 0.0f;
    float previousHighcutFreq = 0.0f;
    float previousPeakFreq = 0.0f;
    
    float newDrive1 = 0.0f;
    float newDrive2 = 0.0f;
    float newDrive3 = 0.0f;
    float newDrive4 = 0.0f;

    juce::SmoothedValue<float> driveSmoother1;
    juce::SmoothedValue<float> driveSmoother2;
    juce::SmoothedValue<float> driveSmoother3;
    juce::SmoothedValue<float> driveSmoother4;
    juce::SmoothedValue<float> outputSmoother1;
    juce::SmoothedValue<float> outputSmoother2;
    juce::SmoothedValue<float> outputSmoother3;
    juce::SmoothedValue<float> outputSmoother4;
    juce::SmoothedValue<float> outputSmootherGlobal;
    juce::SmoothedValue<float> recSmoother1;
    juce::SmoothedValue<float> recSmoother2;
    juce::SmoothedValue<float> recSmoother3;
    juce::SmoothedValue<float> recSmoother4;
    juce::SmoothedValue<float> biasSmoother1;
    juce::SmoothedValue<float> biasSmoother2;
    juce::SmoothedValue<float> biasSmoother3;
    juce::SmoothedValue<float> biasSmoother4;
    juce::SmoothedValue<float> mixSmoother1;
    juce::SmoothedValue<float> mixSmoother2;
    juce::SmoothedValue<float> mixSmoother3;
    juce::SmoothedValue<float> mixSmoother4;
    juce::SmoothedValue<float> mixSmootherGlobal;
    juce::SmoothedValue<float> lowcutFreqSmoother;
    juce::SmoothedValue<float> highcutFreqSmoother;
    juce::SmoothedValue<float> peakFreqSmoother;
    
    juce::SmoothedValue<float> centralSmoother;
    juce::SmoothedValue<float> normalSmoother;

    // DSP Processors
    WidthProcessor widthProcessor1;
    WidthProcessor widthProcessor2;
    WidthProcessor widthProcessor3;
    WidthProcessor widthProcessor4;
    juce::dsp::Compressor<float> compressorProcessor1;
    juce::dsp::Compressor<float> compressorProcessor2;
    juce::dsp::Compressor<float> compressorProcessor3;
    juce::dsp::Compressor<float> compressorProcessor4;

    using GainProcessor   = juce::dsp::Gain<float>;
    using BiasProcessor   = juce::dsp::Bias<float>;
    using DriveProcessor  = juce::dsp::WaveShaper<float>;
    using DCFilter        = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    using DryWetMixer     = juce::dsp::DryWetMixer<float>;
    
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor, DCFilter, GainProcessor> overdrive1;
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor, DCFilter, GainProcessor> overdrive2;
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor, DCFilter, GainProcessor> overdrive3;
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor, DCFilter, GainProcessor> overdrive4;
    
    juce::dsp::DryWetMixer<float> dryWetMixer1;
    juce::dsp::DryWetMixer<float> dryWetMixer2;
    juce::dsp::DryWetMixer<float> dryWetMixer3;
    juce::dsp::DryWetMixer<float> dryWetMixer4;
    juce::dsp::DryWetMixer<float> dryWetMixerGlobal;
    
    // oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;   // normal use 2x
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplingHQ[4]; // HQ use 4x

    int oversampleFactor = 1;

    // oversampling delay, set to dry buffer
    Delay mDelay{0};

    // mode 8 diode================
    juce::Array<float> inputTemp;
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

    // multiband dsp
    juce::dsp::LinkwitzRileyFilter<float> lowpass1, highpass1,
                                          lowpass2, highpass2,
                                          lowpass3, highpass3;
    
    juce::AudioBuffer<float> mBuffer1, mBuffer2, mBuffer3, mBuffer4;
    juce::AudioBuffer<float> dryBuffer1, dryBuffer2, dryBuffer3, dryBuffer4;
    bool multibandState1 = true;
    bool multibandState2 = true;
    bool multibandState3 = true;
    bool multibandState4 = true;
    
//    bool multibandFocus1 = true;
//    bool multibandFocus2 = false;
//    bool multibandFocus3 = false;
//    bool multibandFocus4 = false;

    void processDistortion(juce::AudioBuffer<float>& bandBuffer, juce::String modeID, juce::String driveID, juce::String safeID, juce::String outputID, juce::String biasID, juce::String recID, juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor, DCFilter, GainProcessor>& overdrive);
    
    void processCompressor(juce::dsp::ProcessContextReplacing<float> context, juce::String threshID, juce::String ratioID, juce::dsp::Compressor<float>& compressor);
    
    void normalize(juce::String modeID, juce::AudioBuffer<float>& buffer, int totalNumInputChannels, juce::SmoothedValue<float>& recSmoother, juce::SmoothedValue<float>& outputSmoother);
    
    // void compressorProcessor(float ratio, float thresh, juce::dsp::Compressor<float> compressorProcessor, juce::dsp::ProcessContextReplacing<float> &context);
    
    void mixProcessor(juce::String mixId, juce::SmoothedValue<float> &mixSmoother, int totalNumInputChannels, juce::AudioBuffer<float> &buffer, juce::AudioBuffer<float> dryBuffer);
    
    void mixDryWet(juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer, juce::String mixID, juce::dsp::DryWetMixer<float>& dryWetMixer);
    
    // Save size
    int editorWidth = INIT_WIDTH;
    int editorHeight = INIT_HEIGHT;
    
    // bypass state
    bool isBypassed = false;
    
    // VU meters
    float mInputLeftSmoothed = 0;
    float mInputRightSmoothed = 0;
    float mOutputLeftSmoothed = 0;
    float mOutputRightSmoothed = 0;
    
    // Drive lookandfeel
    float mReductionPrecent1 = 0;
    float mReductionPrecent2 = 0;
    float mReductionPrecent3 = 0;
    float mReductionPrecent4 = 0;
    float mSampleMaxValue1 = 0;
    float mSampleMaxValue2 = 0;
    float mSampleMaxValue3 = 0;
    float mSampleMaxValue4 = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessor)
};
