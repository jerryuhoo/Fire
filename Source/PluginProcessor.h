/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "juce_audio_processors/juce_audio_processors.h"
#include "DSP/Delay.h"
#include "DSP/WidthProcessor.h"
#include "Panels/TopPanel/Preset.h"
#include "Panels/SpectrogramPanel/FFTProcessor.h"
#include "GUI/InterfaceDefines.h"
#include "Utility/AudioHelpers.h"
#include "Utility/FiltersUtil.h"
#include "DSP/ClippingFunctions.h"
#include "DSP/DiodeWDF.h"
#include "DSP/LfoManager.h"
#include "DSP/ModulationRouting.h"

/**
 * Calculates the RMS level for the left and right channels of a buffer
 * and atomically stores them in the provided atomic float variables.
*/
static inline void calculateAndStoreRMS(const juce::AudioBuffer<float>& buffer,
                                        std::atomic<float>& leftAtomic,
                                        std::atomic<float>& rightAtomic)
{
    const float rmsL = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    const float rmsR = (buffer.getNumChannels() > 1) ? buffer.getRMSLevel(1, 0, buffer.getNumSamples()) : rmsL;

    leftAtomic.store(rmsL);
    rightAtomic.store(rmsR);
}

//==============================================================================
// A struct to encapsulate all DSP modules for a single band.
//==============================================================================
struct BandProcessingParameters
{
    // Main process parameters
    int mode;
    bool isHQ;
    float outputVal;
    float mixVal;
    float compThreshold;
    float compRatio;
    bool isCompEnabled;
    float width;
    bool isWidthEnabled;

    // Distortion-specific parameters
    bool isSafeModeOn;
    bool isExtremeModeOn;
    float driveVal;
    float biasVal;
    float recVal;

    // Parameter IDs for modulation
    juce::String driveID;
    juce::String biasID;
    juce::String recID;
    juce::String compRatioID;
    juce::String compThreshID;
    juce::String widthID;
    juce::String outputID;
    juce::String mixID;

    // Normalisable ranges for parameter modulation
    juce::NormalisableRange<float> driveRange;
    juce::NormalisableRange<float> biasRange;
    juce::NormalisableRange<float> recRange;
    juce::NormalisableRange<float> compRatioRange;
    juce::NormalisableRange<float> compThreshRange;
    juce::NormalisableRange<float> widthRange;
    juce::NormalisableRange<float> outputRange;
    juce::NormalisableRange<float> mixRange;
};

//==============================================================================
// A struct to encapsulate all DSP modules for a single band.
//==============================================================================
struct BandProcessor
{
    using GainProcessor = juce::dsp::Gain<float>;
    // using DCFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    using CompressorProcessor = juce::dsp::Compressor<float>;

    // Each band has its own set of processors.
    CompressorProcessor compressor;
    WidthProcessor widthProcessor;
    // DCFilter dcFilter;
    GainProcessor gain;
    juce::dsp::DryWetMixer<float> dryWetMixer;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    BandProcessor() : dryWetMixer(2048)
    {
        // Set a default waveshaper so it's never null
        waveshaperFunction = [](float x)
        { return x; };
    }

    // And its own set of smoothed parameter values.
    juce::SmoothedValue<float> driveSmoother;
    juce::SmoothedValue<float> biasSmoother;
    juce::SmoothedValue<float> recSmoother;
    bool isFirstBlock = true;

    std::function<float(float)> waveshaperFunction;

    std::atomic<float> mInputLeftSmoothed { 0.0f };
    std::atomic<float> mInputRightSmoothed { 0.0f };
    std::atomic<float> mOutputLeftSmoothed { 0.0f };
    std::atomic<float> mOutputRightSmoothed { 0.0f };

    // Per-band state for Safe Mode
    float mReductionPercent = 1.0f;
    float mSampleMaxValue = 0.0f;
    float newDrive = 1.0f;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer,
                 const BandProcessingParameters& params);

private:
    void processDistortion(juce::dsp::AudioBlock<float>& blockToProcess,
                           const juce::AudioBuffer<float>& dryBuffer,
                           const BandProcessingParameters& params);
};

//==============================================================================

class FireAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    FireAudioProcessor();
    ~FireAudioProcessor();

    juce::PropertiesFile& getAppSettings() { return *appProperties; }

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages) override;
    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
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
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState treeState;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    bool hasUpdateCheckBeenPerformed = false;
    bool isSlient(juce::AudioBuffer<float> buffer);

    LfoManager& getLfoManager() { return *lfoManager; }
    const LfoManager& getLfoManager() const { return *lfoManager; }
    bool isDawPlaying() const;
    float getLfoPhase(int lfoIndex) const;
    std::unique_ptr<LfoManager> lfoManager;
    const juce::StringArray& getLfoRateSyncDivisions() const;

    // For writing/loading state
    std::vector<LfoData>& getLfoData() { return lfoManager->getLfoData(); }

    // For reading/saving state (read-only)
    const std::vector<LfoData>& getLfoData() const { return lfoManager->getLfoData(); }

    struct ModulationInfo
    {
        bool isModulated = false;
        int sourceLfoIndex = 0; // Will be 1-based for the UI
        float depth = 0.0f;
        float currentValue = 0.0f; // The current LFO output, bipolar [-1, 1]
        bool isBipolar = true;
    };

    void assignModulation(int routingIndex, int sourceLfoIndex, const juce::String& targetParameterID);

    // New public method for the editor to call
    ModulationInfo getModulationInfoForParameter(const juce::String& parameterID) const;
    void setModulationDepth(const juce::String& targetParameterID, float newDepth);
    void toggleBipolarMode(const juce::String& targetParameterID);
    void resetModulation(const juce::String& targetParameterID);

    void setHistoryArray(int bandIndex);
    juce::Array<float> getHistoryArrayL();
    juce::Array<float> getHistoryArrayR();

    // save presets
    state::StateAB stateAB;
    state::StatePresets statePresets;

    // FFT
    float* getFFTData(int dataIndex);
    int getNumBins();
    int getFFTSize();
    bool isFFTBlockReady();
    void pushDataToFFT(juce::AudioBuffer<float>& buffer, SpectrumProcessor& specProcessor);
    void processFFT(float* tempFFTData, int dataIndex);

    // save size
    void setSavedWidth(const int width);
    void setSavedHeight(const int height);
    int getSavedWidth() const;
    int getSavedHeight() const;

    // bypass
    bool getBypassedState();

    // VU meters
    float getRealtimeModulatedThreshold(int bandIndex) const;

    // drive lookandfeel
    float getReductionPrecent(int bandIndex);
    float getSampleMaxValue(int bandIndex);
    float getTotalLatency() const;

    bool getLatestModulatedFilterValues(ModulatedFilterValues& values);
    bool getLatestMeterValues(MeterValues& values);
    
    float getGlobalInputMeterLevel(int channel) const;
    float getGlobalOutputMeterLevel(int channel) const;
    float getBandInputMeterLevel(int band, int channel) const;
    float getBandOutputMeterLevel(int band, int channel) const;

private:
    std::vector<std::unique_ptr<BandProcessor>> bands;
    float totalLatency = 0.0f;

    void updateParameters();
    void splitBands(const juce::AudioBuffer<float>& inputBuffer, double sampleRate);
    void sumBands(juce::AudioBuffer<float>& outputBuffer,
                  const std::array<juce::AudioBuffer<float>*, 4>& sourceBandBuffers,
                  bool ignoreSoloLogic);
    void updateFilter(double sampleRate);
    void updateGlobalFilters(double sampleRate);
    void processMultiBand(juce::AudioBuffer<float>& wetBuffer, double sampleRate);
    void applyGlobalEffects(juce::AudioBuffer<float>& buffer, double sampleRate);
    void applyGlobalMix(juce::AudioBuffer<float>& buffer);
    void applyDownsamplingEffect(juce::AudioBuffer<float>& buffer);

    // preset id
    int presetId = 0;
    int numBands = 1;
    int activeCrossovers;

    std::unique_ptr<juce::PropertiesFile> appProperties;

    // Oscilloscope
    juce::Array<float> historyArrayL;
    juce::Array<float> historyArrayR;
    int historyLength = 400;

    // Spectrum
    SpectrumProcessor processedSpecProcessor;
    SpectrumProcessor originalSpecProcessor;

    // dry audio buffer
    //    juce::AudioBuffer<float> mDryBuffer;
    juce::AudioBuffer<float> delayMatchedDryBuffer;
    // wet audio buffer
    juce::AudioBuffer<float> mWetBuffer;

    // filter
    MonoChain leftChain;
    MonoChain rightChain;

    void updateLowCutFilters(const ChainSettings& chainSettings, double sampleRate);
    void updateHighCutFilters(const ChainSettings& chainSettings, double sampleRate);
    void updatePeakFilter(const ChainSettings& chainSettings, double sampleRate);

    // Low-Cut / Low-Shelf Filter
    juce::SmoothedValue<float> lowcutFreqSmoother;
    juce::SmoothedValue<float> lowcutGainSmoother;
    juce::SmoothedValue<float> lowcutQualitySmoother;

    // Peak Filter
    juce::SmoothedValue<float> peakFreqSmoother;
    juce::SmoothedValue<float> peakGainSmoother;
    juce::SmoothedValue<float> peakQualitySmoother;

    // High-Cut / High-Shelf Filter
    juce::SmoothedValue<float> highcutFreqSmoother;
    juce::SmoothedValue<float> highcutGainSmoother;
    juce::SmoothedValue<float> highcutQualitySmoother;

    using GainProcessor = juce::dsp::Gain<float>;

    GainProcessor gainProcessorGlobal;
    juce::dsp::DryWetMixer<float> dryWetMixerGlobal { 2048 };

    // oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplingHQ[4]; // HQ use 4x

    int oversampleFactor = 1;

    // oversampling delay, set to dry buffer
    Delay mDelay { 0 };

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

    // multiband dsp
    juce::dsp::LinkwitzRileyFilter<float> lowpass1, highpass1,
        lowpass2, highpass2,
        lowpass3, highpass3;

    juce::dsp::LinkwitzRileyFilter<float> compensatorLP, compensatorHP;

    // 1. Define the basic building block: a single-channel peak filter
    using PeakFilter = juce::dsp::IIR::Filter<float>;

    // 2. Use ProcessorDuplicator to create a stereo version of the peak filter
    using StereoPeakFilter = juce::dsp::ProcessorDuplicator<PeakFilter, juce::dsp::IIR::Coefficients<float>>;

    // 3. Create two instances for Band 2 and Band 3
    StereoPeakFilter compensatorEQ1, compensatorEQ2, compensatorEQ3;

    juce::AudioBuffer<float> mBuffer1, mBuffer2, mBuffer3, mBuffer4;

    juce::SmoothedValue<float> smoothedFreq1 = 200.0f;
    juce::SmoothedValue<float> smoothedFreq2 = 1000.0f;
    juce::SmoothedValue<float> smoothedFreq3 = 5000.0f;

    void processGain(juce::dsp::ProcessContextReplacing<float> context, juce::String outputID, GainProcessor& gainProcessor);

    // Save size
    int editorWidth = INIT_WIDTH;
    int editorHeight = INIT_HEIGHT;

    // bypass state
    bool isBypassed = false;

    // VU meters
    std::atomic<float> mInputLeftSmoothedGlobal { 0.0f };
    std::atomic<float> mInputRightSmoothedGlobal { 0.0f };
    std::atomic<float> mOutputLeftSmoothedGlobal { 0.0f };
    std::atomic<float> mOutputRightSmoothedGlobal { 0.0f };

    std::atomic<float> realtimeModulatedThresholds[4];

    // 1. For FilterControl
    juce::AbstractFifo filterFifo { 1024 };
    std::vector<ModulatedFilterValues> filterFifoBuffer;
    int filterFifoWritePos = 0;

    // 2. For VUMeter
    juce::AbstractFifo meterFifo { 1024 };
    std::vector<MeterValues> meterFifoBuffer;
    int meterFifoWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessor)
};
