/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "juce_audio_processors/juce_audio_processors.h"
#include "DSP/WidthProcessor.h"
#include "DSP/SampleAccurateCompressor.h"
#include "Panels/TopPanel/Preset.h"
#include "Panels/SpectrogramPanel/FFTProcessor.h"
#include "GUI/InterfaceDefines.h"
#include "Utility/AudioHelpers.h"
#include "Utility/FiltersUtil.h"
#include "DSP/LfoManager.h"
#include "DSP/ModulationRouting.h"
#include "DSP/ModulatedValueProvider.h"
#include <array>
#include <atomic>


//==============================================================================
// A struct to encapsulate all DSP modules for a single band.
//==============================================================================
struct BandProcessingParameters
{
    // Main process parameters
    bool isBandEnabled { true };
    int mode { 0 };
    bool isHQ { false };

    ModulatedValueProvider outputVal;
    float mixVal { 1.0f };
    ModulatedValueProvider mixValProvider;
    float compThreshold { 0.0f };
    ModulatedValueProvider compThresholdValProvider;
    float compRatio { 1.0f };
    ModulatedValueProvider compRatioValProvider;
    float compAttack { 10.0f };
    ModulatedValueProvider compAttackValProvider;
    float compRelease { 100.0f };
    ModulatedValueProvider compReleaseValProvider;
    float compMixVal { 1.0f };
    ModulatedValueProvider compMixValProvider;
    bool isCompEnabled { false };
    float width { 0.5f };
    ModulatedValueProvider widthValProvider;
    float pan { 0.0f };
    ModulatedValueProvider panValProvider;
    float widthMixVal { 1.0f };
    ModulatedValueProvider widthMixValProvider;
    bool isDriveEnabled { false };
    bool isWidthEnabled { false };
    bool isShapeEnabled { false };
    bool isDcFilterEnabled { false };

    // Distortion-specific parameters
    bool isSafeModeOn { true };
    bool isExtremeModeOn { false };
    ModulatedValueProvider driveVal;
    ModulatedValueProvider biasVal;
    ModulatedValueProvider recVal;
    float shapeMixVal { 1.0f };
    ModulatedValueProvider shapeMixValProvider;

    // LFO source indices for the above parameters (-1 if not modulated)
    int driveLfoSourceIndex = -1;
    int biasLfoSourceIndex = -1;
    int recLfoSourceIndex = -1;
    int outputLfoSourceIndex = -1;
    int mixLfoSourceIndex = -1;
    int shapeMixLfoSourceIndex = -1;
    int widthLfoSourceIndex = -1;
    int panLfoSourceIndex = -1;
    int widthMixLfoSourceIndex = -1;
    int compThresholdLfoSourceIndex = -1;
    int compRatioLfoSourceIndex = -1;
    int compAttackLfoSourceIndex = -1;
    int compReleaseLfoSourceIndex = -1;
    int compMixLfoSourceIndex = -1;

};

//==============================================================================
// A struct to encapsulate all DSP modules for a single band.
//==============================================================================
struct BandProcessor
{
    using GainProcessor = juce::dsp::Gain<float>;
    using DCFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    using CompressorProcessor = SampleAccurateCompressor;

    // Each band has its own set of processors.
    CompressorProcessor compressor;
    WidthProcessor widthProcessor;
    DCFilter dcFilter;
    GainProcessor gain;
    juce::dsp::DryWetMixer<float> dryWetMixer;
    juce::dsp::DryWetMixer<float> shapeMixer;
    juce::dsp::DryWetMixer<float> compressorMixer;
    juce::dsp::DryWetMixer<float> widthMixer;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    BandProcessor() : dryWetMixer(2048), shapeMixer(2048), compressorMixer(2048), widthMixer(2048) {}

    // And its own set of smoothed parameter values.
    juce::SmoothedValue<float> driveSmoother;
    juce::SmoothedValue<float> biasSmoother;
    juce::SmoothedValue<float> recSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bandEnableMixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dcFilterMixSmoother;
    bool isFirstBlock = true;
    bool dryWetMixerPrimed = false;
    bool shapeMixerPrimed = false;
    bool compressorMixerPrimed = false;
    bool widthMixerPrimed = false;
    bool bandEnableMixPrimed = false;
    bool dcFilterMixPrimed = false;

    // Atomics for RMS levels
    std::atomic<float> mInputLeftRMS { 0.0f };
    std::atomic<float> mInputRightRMS { 0.0f };
    std::atomic<float> mOutputLeftRMS { 0.0f };
    std::atomic<float> mOutputRightRMS { 0.0f };

    // Atomics for Peak levels
    std::atomic<float> mInputLeftPeak { 0.0f };
    std::atomic<float> mInputRightPeak { 0.0f };
    std::atomic<float> mOutputLeftPeak { 0.0f };
    std::atomic<float> mOutputRightPeak { 0.0f };


    // Per-band state for Safe Mode
    std::atomic<float> mReductionPercent { 1.0f };
    std::atomic<float> mSampleMaxValue { 0.0f };

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void resetQualityTransitionState() noexcept;
    void process(juce::AudioBuffer<float>& buffer,
                 const BandProcessingParameters& params,
                 const juce::AudioBuffer<float>& lfoOutputs);
    void process(juce::AudioBuffer<float>& buffer,
                 const BandProcessingParameters& params,
                 const juce::AudioBuffer<float>& lfoOutputs,
                 float callbackInputPeak,
                 bool updateReductionMeter);

    const int oversampleFactor = 2;

private:
    void processChunk(juce::AudioBuffer<float>& buffer,
                      const BandProcessingParameters& params,
                      const juce::AudioBuffer<float>& lfoOutputs,
                      int lfoSampleOffset,
                      float inputPeak,
                      bool updateReductionMeter);
    void processDistortion(juce::dsp::AudioBlock<float>& blockToProcess,
                           const BandProcessingParameters& params,
                           float inputPeak,
                           bool updateReductionMeter);
    void processBandEnable(juce::AudioBuffer<float>& buffer, bool enabled);
    void processDcFilter(juce::AudioBuffer<float>& buffer, bool enabled);

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> dcFilterDryBuffer;
    juce::AudioBuffer<float> upsampledLfoOutputs;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Thiran>
        bandEnableDryDelay { 2048 };
    int maximumPreparedBlockSize = 1;
};

//==============================================================================

class FireAudioProcessor : public juce::AudioProcessor,
                           public juce::ChangeBroadcaster,
                           private juce::Timer
{
public:
    //==============================================================================
    FireAudioProcessor();
    ~FireAudioProcessor() override;

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
    bool isSlient(const juce::AudioBuffer<float>& buffer);

    LfoManager& getLfoManager() { return *lfoManager; }
    const LfoManager& getLfoManager() const { return *lfoManager; }
    bool isDawPlaying() const;
    float getLfoPhase(int lfoIndex) const;
    std::unique_ptr<LfoManager> lfoManager;
    const juce::StringArray& getLfoRateSyncDivisions() const;

    std::vector<LfoData> getLfoData() const { return lfoManager->getLfoDataCopy(); }

    struct ModulationInfo
    {
        bool isModulated = false;
        int sourceLfoIndex = 0; // Will be 1-based for the UI
        float depth = 0.0f;
        float currentValue = 0.0f; // The current LFO output, bipolar [-1, 1]
        bool isBipolar = true;
        bool isBypassed = false;
    };

    void assignModulation(int routingIndex, int sourceLfoIndex, const juce::String& targetParameterID);

    // New public method for the editor to call
    ModulationInfo getModulationInfoForParameter(const juce::String& parameterID) const;
    void setModulationValue(const juce::String& targetParameterID, float newValue);
    void setModulationDepth(const juce::String& targetParameterID, float newDepth);
    void toggleBipolarMode(const juce::String& targetParameterID);
    void resetModulation(const juce::String& targetParameterID);
    void clearModulationForParameter(const juce::String& targetParameterID);
    void invertModulationDepthForParameter(const juce::String& targetParameterID);

    void assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID);

    void setHistoryArray(int bandIndex);
    std::uint64_t getHistoryGeneration() const noexcept;
    void copyHistoryArrays(juce::Array<float>& leftDestination,
                           juce::Array<float>& rightDestination) const;
    juce::Array<float> getHistoryArrayL();
    juce::Array<float> getHistoryArrayR();

    // save presets
    state::StateAB stateAB;
    state::StatePresets statePresets;

    // FFT
    int getNumBins() const noexcept;
    int getFFTSize() const noexcept;
    bool popLatestFFTFrames(float* processedDestination,
                            int processedDestinationSize,
                            float* originalDestination,
                            int originalDestinationSize) noexcept;
    void pushDataPairToFFT(const juce::AudioBuffer<float>& processedBuffer,
                           const juce::AudioBuffer<float>& originalBuffer);
    bool processFFT(float* tempFFTData, int bufferSize);

    // save size
    void setSavedWidth(const int width);
    void setSavedHeight(const int height);
    int getSavedWidth() const;
    int getSavedHeight() const;

    // bypass
    bool getBypassedState() const;

    // VU meters
    float getRealtimeModulatedThreshold(int bandIndex) const;

    // drive lookandfeel
    float getReductionPrecent(int bandIndex);
    float getSampleMaxValue(int bandIndex);
    float getTotalLatency() const;

    bool getLatestModulatedFilterValues(ModulatedFilterValues& values);
    bool getLatestMeterValues(MeterValues& values);

    // Getters for meter levels
    float getGlobalInputRMSLevel(int channel) const;
    float getGlobalOutputRMSLevel(int channel) const;
    float getGlobalInputPeakLevel(int channel) const;
    float getGlobalOutputPeakLevel(int channel) const;

    float getBandInputRMSLevel(int band, int channel) const;
    float getBandOutputRMSLevel(int band, int channel) const;
    float getBandInputPeakLevel(int band, int channel) const;
    float getBandOutputPeakLevel(int band, int channel) const;


    void lfoDataHasChanged();
    bool isCurrentStateEquivalentToPreset(const juce::XmlElement& presetXml);

    void splitBands(const juce::AudioBuffer<float>& inputBuffer, double sampleRate);
    void sumBands(juce::AudioBuffer<float>& outputBuffer,
                  const std::array<juce::AudioBuffer<float>*, 4>& sourceBandBuffers,
                  bool ignoreSoloLogic,
                  bool useDelayedSoloEnvelope);
    void updateFilter(double sampleRate);
    bool updateGlobalFilters(double sampleRate,
                             const juce::AudioBuffer<float>& lfoOutputs,
                             int lfoSampleIndex);
    struct HqCallbackContext
    {
        std::array<BandProcessingParameters, 4> bandParameters;
        std::array<float, 4> bandInputPeaks {};
        std::array<bool, 4> soloState {};
        bool anySoloActive = false;
    };
    void processMultiBandRange(
        juce::AudioBuffer<float>& wetBuffer,
        juce::AudioBuffer<float>& delayMatchedDryBufferForRange,
        const std::array<juce::AudioBuffer<float>*, 4>& bandBuffers,
        const juce::AudioBuffer<float>& lfoOutputs,
        const HqCallbackContext& callbackContext,
        bool useHQ,
        bool updateReductionMeter);
    void applyGlobalEffects(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& lfoOutputs, double sampleRate);
    void applyGlobalMix(juce::AudioBuffer<float>& buffer,
                        juce::AudioBuffer<float>& delayMatchedDryBufferForRange,
                        const juce::AudioBuffer<float>& lfoOutputs,
                        bool useHQ);
    void applyDownsamplingEffect(juce::AudioBuffer<float>& buffer,
                                 const juce::AudioBuffer<float>& lfoOutputs);

    void shiftLfoModulationTargets(int startIndex, int endIndex, int shiftAmount);
    void clearLfoModulationForBand(int bandIndex);

    // Parameter migration for an add/remove operation is performed on the
    // message thread. The audio thread consumes this generation and resets the
    // slot-based DSP state once the complete topology has been published.
    void requestMultibandTopologyReset() noexcept;

    bool getLatestDistortionGraphValues(DistortionGraphValues& values);
    void setUiFocusBand(int bandIndex);

private:
    struct CachedParameter
    {
        std::atomic<float>* raw = nullptr;
        juce::RangedAudioParameter* ranged = nullptr;
    };

    struct BandParameterCache
    {
        CachedParameter enabled;
        CachedParameter solo;
        CachedParameter mode;
        CachedParameter linked;
        CachedParameter safe;
        CachedParameter extreme;
        CachedParameter driveEnabled;
        CachedParameter shapeEnabled;
        CachedParameter compressorEnabled;
        CachedParameter widthEnabled;
        CachedParameter dcFilterEnabled;
        CachedParameter drive;
        CachedParameter bias;
        CachedParameter rec;
        CachedParameter output;
        CachedParameter compressorRatio;
        CachedParameter compressorThreshold;
        CachedParameter compressorAttack;
        CachedParameter compressorRelease;
        CachedParameter compressorMix;
        CachedParameter width;
        CachedParameter pan;
        CachedParameter widthMix;
        CachedParameter mix;
        CachedParameter shapeMix;
    };

    struct FilterParameterCache
    {
        CachedParameter lowCutFrequency;
        CachedParameter lowCutGain;
        CachedParameter lowCutQuality;
        CachedParameter lowCutSlope;
        CachedParameter lowCutBypassed;
        CachedParameter peakFrequency;
        CachedParameter peakGain;
        CachedParameter peakQuality;
        CachedParameter peakBypassed;
        CachedParameter highCutFrequency;
        CachedParameter highCutGain;
        CachedParameter highCutQuality;
        CachedParameter highCutSlope;
        CachedParameter highCutBypassed;
    };

    void initialiseParameterCache();
    CachedParameter cacheParameter(const juce::String& parameterID);
    static float loadCachedParameter(const CachedParameter& parameter, float fallback = 0.0f) noexcept;
    float getBlockModulatedValue(const CachedParameter& parameter,
                                 const juce::AudioBuffer<float>& lfoOutputs) const noexcept;
    float getModulatedValueAtSample(const CachedParameter& parameter,
                                    const juce::AudioBuffer<float>& lfoOutputs,
                                    int sampleIndex) const noexcept;
    ChainSettings getCachedChainSettings(const juce::AudioBuffer<float>* lfoOutputs) const noexcept;
    ChainSettings getCachedChainSettingsAtSample(const juce::AudioBuffer<float>& lfoOutputs,
                                                 int sampleIndex) const noexcept;
    bool hasActiveFilterModulation() const noexcept;

    std::array<BandParameterCache, 4> bandParameterCache;
    std::array<CachedParameter, 3> crossoverFrequencyParameters;
    std::array<std::atomic<float>*, 4> lfoSmoothParameters {};
    FilterParameterCache filterParameterCache;
    CachedParameter numBandsParameter;
    CachedParameter hqParameter;
    CachedParameter globalOutputParameter;
    CachedParameter globalMixParameter;
    CachedParameter filterEnabledParameter;
    CachedParameter downsampleEnabledParameter;
    CachedParameter downsampleRateParameter;
    CachedParameter bitDepthParameter;
    CachedParameter jitterParameter;
    CachedParameter downsampleMixParameter;

    std::atomic<int> uiFocusBand { 0 };
    // reset parameters
    void performReset();
    void resetMultibandProcessingState() noexcept;
    std::array<float, 3> getEffectiveCrossoverFrequencies() const noexcept;
    void snapCrossoverSmoothersToParameters() noexcept;
    void snapBandSoloGainsToParameters() noexcept;
    void updateBandSoloGainEnvelope(
        int numSamples,
        bool useHQ,
        const std::array<bool, 4>& soloState,
        bool anySoloActive) noexcept;
    void synchroniseMultibandTopologyResetState() noexcept;
    std::atomic<bool> needsReset { false };
    std::atomic<std::uint32_t> multibandTopologyResetGeneration { 0 };
    std::uint32_t appliedMultibandTopologyResetGeneration = 0;

    std::vector<std::unique_ptr<BandProcessor>> bands;
    std::atomic<float> totalLatency { 0.0f };
    std::atomic<float> preparedHqLatency { 0.0f };

    enum class HqTransitionPhase;

    void updateParameters();
    void prepareHqCallbackContext(
        const juce::AudioBuffer<float>& lfoOutputs,
        HqCallbackContext& callbackContext);
    void publishLatencyToHost();
    void timerCallback() override;
    void captureHistorySamples();
    void resetDownsamplingState() noexcept;
    void primeLatencyMatchedBypass(juce::AudioBuffer<float>& inputBuffer,
                                   bool useHQ);
    void processLatencyMatchedBypass(juce::AudioBuffer<float>& buffer,
                                     bool useHQ);
    void advanceNonHqOutputDelay(const juce::AudioBuffer<float>& inputBuffer);
    void applyNonHqOutputDelay(juce::AudioBuffer<float>& buffer);
    void beginHqTransitionCallback(bool requestedHq) noexcept;
    void noteHqRequestWhileBypassed(bool requestedHq) noexcept;
    void processHqTransitionBlock(juce::AudioBuffer<float>& buffer,
                                  const juce::AudioBuffer<float>& lfoOutputs,
                                  double sampleRate,
                                  bool requestedHq,
                                  const HqCallbackContext& callbackContext);
    void processActiveHqRange(
        juce::AudioBuffer<float>& buffer,
        juce::AudioBuffer<float>& delayMatchedDryBufferForRange,
        const std::array<juce::AudioBuffer<float>*, 4>& bandBuffers,
        const juce::AudioBuffer<float>& lfoOutputs,
        double sampleRate,
        bool useHQ,
        bool updateReductionMeter,
        const HqCallbackContext& callbackContext);
    void startHqTransitionRamp(float target,
                              HqTransitionPhase phase) noexcept;
    void applyHqTransitionRamp(juce::AudioBuffer<float>& buffer) noexcept;
    void resetHqQualityPathState() noexcept;
    void snapHqTransitionToParameter() noexcept;

    // preset id
    int numBands = 1;
    int activeCrossovers = 0;

    std::unique_ptr<juce::PropertiesFile> appProperties;

    // Oscilloscope
    static constexpr int historyLength = 400;
    std::array<std::atomic<float>, historyLength> historyArrayL {};
    std::array<std::atomic<float>, historyLength> historyArrayR {};
    std::atomic<int> historyWritePosition { 0 };
    std::atomic<int> historySamplesAvailable { historyLength };
    std::atomic<int> historySourceBand { 4 };
    std::atomic<std::uint64_t> historyGeneration { 0 };

    // Spectrum
    SpectrumProcessor spectrumProcessor;

    // dry audio buffer
    //    juce::AudioBuffer<float> mDryBuffer;
    juce::AudioBuffer<float> delayMatchedDryBuffer;
    // wet audio buffer
    juce::AudioBuffer<float> mWetBuffer;
    juce::AudioBuffer<float> lfoOutputBuffer;
    juce::AudioBuffer<float> lofiDryBuffer;

    // filter
    MonoChain leftChain;
    MonoChain rightChain;
    juce::dsp::DryWetMixer<float> globalFilterMixer { 0 };
    bool globalFilterMixerPrimed = false;

    enum GlobalFilterStageIndex : size_t
    {
        lowCutStage,
        peakStage,
        highCutStage,
        lowCutQStage,
        highCutQStage,
        numGlobalFilterStages
    };

    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>,
               numGlobalFilterStages> globalFilterStageMix;
    juce::AudioBuffer<float> globalFilterStageDryBuffer;

    ChainSettings cachedGlobalFilterSettings {};
    double cachedGlobalFilterSampleRate = 0.0;
    bool globalFilterCacheValid = false;

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
    juce::dsp::DryWetMixer<float> bypassDelayMixer { 2048 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>
        nonHqOutputDelay { 2048 };
    bool globalMixerPrimed = false;

    enum class HqTransitionPhase
    {
        steady,
        fadingOut,
        warmingUp,
        fadingIn
    };

    HqTransitionPhase hqTransitionPhase = HqTransitionPhase::steady;
    bool activeHqMode = false;
    bool pendingHqMode = false;
    bool hqTransitionInitialised = false;
    float hqTransitionGain = 1.0f;
    float hqTransitionGainStep = 0.0f;
    int hqTransitionRampSamples = 1;
    int hqTransitionRampRemaining = 0;
    int hqTransitionWarmupSamples = 1;
    int hqTransitionWarmupRemaining = 0;

    juce::dsp::DryWetMixer<float> lofiMixer { 2048 };
    bool lofiMixerPrimed = false;
    juce::Random random;
    static constexpr size_t downsamplingStateChannels = 64;
    std::array<int, downsamplingStateChannels> downsampleSamplesRemaining {};
    std::array<float, downsamplingStateChannels> downsampleHeldSamples {};
    bool downsamplingWasActive = false;

    // multiband dsp
    juce::dsp::LinkwitzRileyFilter<float> lowpass1, highpass1,
        lowpass2, highpass2,
        lowpass3, highpass3;

    juce::dsp::LinkwitzRileyFilter<float> compensatorLP, compensatorHP,
        secondCompensatorLP, secondCompensatorHP;

    juce::AudioBuffer<float> mBuffer1, mBuffer2, mBuffer3, mBuffer4;
    juce::AudioBuffer<float> mSplitTemp1, mSplitTemp2, mSplitTemp3;
    juce::AudioBuffer<float> bandSoloGainEnvelope;
    juce::AudioBuffer<float> delayedBandSoloGainEnvelope;
    std::array<juce::dsp::DelayLine<float,
                                    juce::dsp::DelayLineInterpolationTypes::Linear>, 4>
        bandSoloGainDelayLines;
    bool bandSoloGainDelayLinesPrepared = false;

    juce::SmoothedValue<float> smoothedFreq1 = 200.0f;
    juce::SmoothedValue<float> smoothedFreq2 = 1000.0f;
    juce::SmoothedValue<float> smoothedFreq3 = 5000.0f;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 4>
        bandSoloGainSmoothers;

    // Save size
    std::atomic<int> editorWidth { static_cast<int>(INIT_WIDTH) };
    std::atomic<int> editorHeight { static_cast<int>(INIT_HEIGHT) };

    // bypass state
    std::atomic<bool> isBypassed { false };

    // VU meters data
    std::atomic<float> mInputLeftRMSGlobal { 0.0f };
    std::atomic<float> mInputRightRMSGlobal { 0.0f };
    std::atomic<float> mOutputLeftRMSGlobal { 0.0f };
    std::atomic<float> mOutputRightRMSGlobal { 0.0f };

    std::atomic<float> mInputLeftPeakGlobal { 0.0f };
    std::atomic<float> mInputRightPeakGlobal { 0.0f };
    std::atomic<float> mOutputLeftPeakGlobal { 0.0f };
    std::atomic<float> mOutputRightPeakGlobal { 0.0f };

    std::atomic<float> realtimeModulatedThresholds[4];

    // 1. For FilterControl
    juce::AbstractFifo filterFifo { 1024 };
    std::vector<ModulatedFilterValues> filterFifoBuffer;

    // 2. For VUMeter
    juce::AbstractFifo meterFifo { 1024 };
    std::vector<MeterValues> meterFifoBuffer;
    void calculateAndStoreLevels(const juce::AudioBuffer<float>& buffer,
                                       std::atomic<float>& rmsLeft,
                                       std::atomic<float>& rmsRight,
                                       std::atomic<float>& peakLeft,
                                       std::atomic<float>& peakRight);

    // 3. For Distortion Graph
    juce::AbstractFifo graphFifo { 1024 };
    std::vector<DistortionGraphValues> graphFifoBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireAudioProcessor)
};
