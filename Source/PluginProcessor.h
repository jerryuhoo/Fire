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

    juce::AudioProcessorValueTreeState treeState;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    void setHistoryArray(int bandIndex);
    juce::Array<float> getHistoryArrayL();
    juce::Array<float> getHistoryArrayR();
    
    // save presets
    state::StateAB stateAB;
    state::StatePresets statePresets;
    
    // FFT
    float * getFFTData();
    int getNumBins();
    int getFFTSize();
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
    void setLineNum();
    
    // VU meters
    float getInputMeterRMSLevel(int channel, juce::String bandName);
    float getOutputMeterRMSLevel(int channel, juce::String bandName);
    
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
    juce::AudioBuffer<float> mDryBuffer;
    // wet audio buffer
    juce::AudioBuffer<float> mWetBuffer;
    
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
    
    float newDrive1 = 1.0f;
    float newDrive2 = 1.0f;
    float newDrive3 = 1.0f;
    float newDrive4 = 1.0f;

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
    
    using GainProcessor         = juce::dsp::Gain<float>;
    using BiasProcessor         = juce::dsp::Bias<float>;
    using DriveProcessor        = juce::dsp::WaveShaper<float>;
    using DCFilter              = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    using CompressorProcessor   = juce::dsp::Compressor<float>;
    using DryWetMixer           = juce::dsp::DryWetMixer<float>;
    
    CompressorProcessor compressorProcessor1;
    CompressorProcessor compressorProcessor2;
    CompressorProcessor compressorProcessor3;
    CompressorProcessor compressorProcessor4;
    
    DCFilter dcFilter1;
    DCFilter dcFilter2;
    DCFilter dcFilter3;
    DCFilter dcFilter4;
    
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor> overdrive1;
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor> overdrive2;
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor> overdrive3;
    juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor> overdrive4;
    
    GainProcessor gainProcessor1;
    GainProcessor gainProcessor2;
    GainProcessor gainProcessor3;
    GainProcessor gainProcessor4;
    GainProcessor gainProcessorGlobal;
    
    DryWetMixer dryWetMixer1{100};
    DryWetMixer dryWetMixer2{100};
    DryWetMixer dryWetMixer3{100};
    DryWetMixer dryWetMixer4{100};
    DryWetMixer dryWetMixerGlobal{100};
    
    // oversampling
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
    
    bool multibandEnable1 = true;
    bool multibandEnable2 = true;
    bool multibandEnable3 = true;
    bool multibandEnable4 = true;
    
    bool multibandSolo1 = false;
    bool multibandSolo2 = false;
    bool multibandSolo3 = false;
    bool multibandSolo4 = false;
    
//    bool multibandFocus1 = true;
//    bool multibandFocus2 = false;
//    bool multibandFocus3 = false;
//    bool multibandFocus4 = false;

    bool shouldSetBlackMask(int index);
    bool getSoloStateFromIndex(int index);
    
    void processOneBand(juce::AudioBuffer<float>& bandBuffer, juce::dsp::ProcessContextReplacing<float> context, juce::String modeID, juce::String driveID, juce::String safeID, juce::String biasID, juce::String recID, juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor>& overdrive, juce::String outputID, GainProcessor& gainProcessor, juce::String threshID, juce::String ratioID, CompressorProcessor& compressorProcessor, int totalNumInputChannels, juce::SmoothedValue<float>& recSmoother, juce::SmoothedValue<float>& outputSmoother, juce::String mixID, juce::dsp::DryWetMixer<float>& dryWetMixer, juce::String widthID, WidthProcessor widthProcessor, DCFilter &dcFilter, juce::String widthBypassID, juce::String compBypassID);
    
    void processDistortion(juce::AudioBuffer<float>& bandBuffer, juce::String modeID, juce::String driveID, juce::String safeID, juce::String biasID, juce::String recID, juce::dsp::ProcessorChain<GainProcessor, BiasProcessor, DriveProcessor, juce::dsp::WaveShaper<float, std::function<float (float)>>, BiasProcessor>& overdrive, DCFilter& dcFilter);
    
    void processGain(juce::dsp::ProcessContextReplacing<float> context, juce::String outputID, GainProcessor& gainProcessor);
    
    void processCompressor(juce::dsp::ProcessContextReplacing<float> context, juce::String threshID, juce::String ratioID, CompressorProcessor& compressor);
    
    void normalize(juce::String modeID, juce::AudioBuffer<float>& buffer, int totalNumInputChannels, juce::SmoothedValue<float>& recSmoother, juce::SmoothedValue<float>& outputSmoother);
    
    // void compressorProcessor(float ratio, float thresh, juce::dsp::Compressor<float> compressorProcessor, juce::dsp::ProcessContextReplacing<float> &context);
    
    void mixDryWet(juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer, juce::String mixID, juce::dsp::DryWetMixer<float>& dryWetMixer, float latency);
    
    float mLatency = 0.0f;
    
    // Save size
    int editorWidth = INIT_WIDTH;
    int editorHeight = INIT_HEIGHT;
    
    // bypass state
    bool isBypassed = false;
    
    // VU meters
    
    void setLeftRightMeterRMSValues(juce::AudioBuffer<float> buffer, float& leftOutValue, float& rightOutValue);
    float mInputLeftSmoothedGlobal = 0;
    float mInputRightSmoothedGlobal = 0;
    float mOutputLeftSmoothedGlobal = 0;
    float mOutputRightSmoothedGlobal = 0;
    
    float mInputLeftSmoothedBand1 = 0;
    float mInputRightSmoothedBand1 = 0;
    float mOutputLeftSmoothedBand1 = 0;
    float mOutputRightSmoothedBand1 = 0;
    
    float mInputLeftSmoothedBand2 = 0;
    float mInputRightSmoothedBand2 = 0;
    float mOutputLeftSmoothedBand2 = 0;
    float mOutputRightSmoothedBand2 = 0;
    
    float mInputLeftSmoothedBand3 = 0;
    float mInputRightSmoothedBand3 = 0;
    float mOutputLeftSmoothedBand3 = 0;
    float mOutputRightSmoothedBand3 = 0;
    
    float mInputLeftSmoothedBand4 = 0;
    float mInputRightSmoothedBand4 = 0;
    float mOutputLeftSmoothedBand4 = 0;
    float mOutputRightSmoothedBand4 = 0;
    
    
    // Drive lookandfeel
    float mReductionPrecent1 = 1;
    float mReductionPrecent2 = 1;
    float mReductionPrecent3 = 1;
    float mReductionPrecent4 = 1;
    float mSampleMaxValue1 = 0;
    float mSampleMaxValue2 = 0;
    float mSampleMaxValue3 = 0;
    float mSampleMaxValue4 = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessor)
};
