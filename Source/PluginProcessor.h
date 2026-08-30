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
#include "DSP/ZeroLatencyModulatedDryWetMixer.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
#include <functional>
#endif


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
    bool isOutputLinked { false };
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

// Keeps LFO-routed output recipe changes continuous without low-pass filtering
// the LFO waveform itself. Ordinary routed-base automation owns a separate
// linear-gain dezipper, while the legacy tracker mirrors juce::dsp::Gain only so
// attaching a route can start from the gain that was actually audible.
struct OutputGainTransitionState
{
    struct RecipeSignature
    {
        bool routed = false;
        int sourceIndex = -1;
        float modulationDepth = 0.0f;
        bool isBipolar = true;
        bool isLinked = false;
    };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routeTransitionMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routedBaseGainSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        legacyGainTracker;
    RecipeSignature lastRecipe;
    float anchorLinearGain = 1.0f;
    float lastAppliedLinearGain = 1.0f;
    float routedBaseTargetDb = 0.0f;
    bool routedBaseGainPrimed = false;
    bool initialised = false;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
};

// Keeps discrete compressor routing changes continuous while leaving the
// routed LFO trajectory itself sample-accurate. Base-value automation is kept
// separate from route identity so it cannot restart this bridge at callback or
// internal-chunk boundaries.
struct CompressorRecipeTransitionState
{
    struct RecipeSignature
    {
        bool routed = false;
        int sourceIndex = -1;
        float modulationDepth = 0.0f;
        bool isBipolar = true;
    };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routeTransitionMix;
    RecipeSignature lastRecipe;
    float anchorValue = 0.0f;
    float lastAppliedValue = 0.0f;
    bool initialised = false;

    void prepare(double sampleRate, float initialValue) noexcept;
    void reset(float initialValue) noexcept;
};

// Bias and rectification keep their ordinary 50 ms base automation while a
// stable routed LFO remains sample-accurate. Only discrete route-recipe edits
// use this independent 10 ms bridge.
struct ShapeControlRecipeTransitionState
{
    struct RecipeSignature
    {
        bool routed = false;
        int sourceIndex = -1;
        float modulationDepth = 0.0f;
        bool isBipolar = true;
    };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routeTransitionMix;
    RecipeSignature lastRecipe;
    float anchorValue = 0.0f;
    float lastAppliedValue = 0.0f;
    bool initialised = false;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
};

// Drive needs three independent transitions so a stable routed LFO is not
// low-pass filtered by either ordinary parameter smoothing or Safe recovery.
// All stored values are linear gains, matching the multiplier consumed by the
// waveshaper and the domain of Drive's established 50 ms dezipper.
struct DriveControlTransitionState
{
    struct RecipeSignature
    {
        bool routed = false;
        int sourceIndex = -1;
        float modulationDepth = 0.0f;
        bool isBipolar = true;
        bool isExtremeModeOn = false;
    };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        routeTransitionMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        enableTransitionMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        safeRecoveryMix;
    RecipeSignature lastRecipe;
    float routeAnchorGain = 1.0f;
    float lastAppliedRouteGain = 1.0f;
    float enableAnchorGain = 1.0f;
    float lastAppliedFinalGain = 1.0f;
    float safeRecoveryAnchorGain = 1.0f;
    bool recipeInitialised = false;
    bool enableInitialised = false;
    bool lastDriveEnabled = true;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
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
    ZeroLatencyModulatedDryWetMixer bandMixer;
    ZeroLatencyModulatedDryWetMixer compressorMixer;
    ZeroLatencyModulatedDryWetMixer widthMixer;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    OutputGainTransitionState outputGainTransition;
    DriveControlTransitionState driveControlTransition;
    ShapeControlRecipeTransitionState biasRecipeTransition;
    ShapeControlRecipeTransitionState recRecipeTransition;
    CompressorRecipeTransitionState compressorThresholdRecipeTransition;
    CompressorRecipeTransitionState compressorRatioRecipeTransition;
    CompressorRecipeTransitionState compressorAttackRecipeTransition;
    CompressorRecipeTransitionState compressorReleaseRecipeTransition;

    // And its own set of smoothed parameter values.
    juce::SmoothedValue<float> driveSmoother;
    juce::SmoothedValue<float> biasSmoother;
    juce::SmoothedValue<float> recSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        compressorThresholdBaseSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        compressorRatioBaseSmoother;
    // A multiplicative ramp moves the detector pole smoothly across the
    // 0.1-200 ms range; a linear-ms ramp merely moves its largest jump to the
    // final sample when the target is very short.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>
        compressorAttackBaseSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>
        compressorReleaseBaseSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        shapeMixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        waveshaperModeMixSmoother;
    std::array<int, 2> waveshaperModeSlots { 3, 3 };
    int requestedWaveshaperMode = 3;
    bool waveshaperModeMixPrimed = false;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bandEnableMixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dcFilterMixSmoother;
    bool isFirstBlock = true;
    bool shapeMixSmootherPrimed = false;
    bool compressorBaseSmoothersPrimed = false;
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
                           const float* safePeakEnvelope,
                           int safePeakEnvelopeSamples,
                           float inputPeak,
                           bool updateReductionMeter);
    void fillSafePeakEnvelope(const juce::AudioBuffer<float>& buffer) noexcept;
    void processBandEnable(juce::AudioBuffer<float>& buffer, bool enabled);
    void processDcFilter(juce::AudioBuffer<float>& buffer, bool enabled);

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> dcFilterDryBuffer;
    juce::AudioBuffer<float> upsampledLfoOutputs;
    juce::AudioBuffer<float> safePeakEnvelopeBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Thiran>
        sharedBandDryDelay { 2048 };
    float safePeakEnvelope = 0.0f;
    float safePeakReleaseCoefficient = 0.0f;
    int safePeakHoldSamples = 1;
    int safePeakHoldRemaining = 0;
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
    struct SerializablePresetStateSnapshot
    {
        juce::ValueTree parameterState;
        std::vector<LfoData> lfoData;
        juce::Array<ModulationRouting> routings;
    };
    SerializablePresetStateSnapshot captureSerializablePresetStateSnapshot() const;
    bool isDawPlaying() const;
    float getLfoPhase(int lfoIndex) const;
    std::unique_ptr<LfoManager> lfoManager;
private:
    // StateAB snapshots itself while later processor members (notably
    // StatePresets) are still under construction. Keep that constructor path
    // on the LFO/APVTS-only snapshot until the full main model is alive.
    std::atomic<bool> serializableMainStateReady { false };
public:
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

    // New public method for the editor to call
    ModulationInfo getModulationInfoForParameter(const juce::String& parameterID) const;
    void setModulationValue(const juce::String& targetParameterID, float newValue);
    void setModulationDepth(const juce::String& targetParameterID, float newDepth);
    void toggleBipolarMode(const juce::String& targetParameterID);
    void resetModulation(const juce::String& targetParameterID);
    void clearModulationForParameter(const juce::String& targetParameterID);
    void invertModulationDepthForParameter(const juce::String& targetParameterID);

    void assignLfoToTarget(int sourceLfoIndex, const juce::String& targetParameterID);

    struct HistorySnapshot
    {
        juce::Array<float> left;
        juce::Array<float> right;
        std::uint64_t sourceToken = 0;
        std::uint64_t generation = 0;
    };

    static constexpr int globalHistorySourceIndex = 4;
    void setHistoryArray(int bandIndex);
    std::uint64_t getHistorySourceToken() const noexcept;
    std::uint64_t getHistoryGeneration() const noexcept;
    bool copyHistorySnapshot(HistorySnapshot& destination) const;
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
                            int originalDestinationSize,
                            std::uint64_t minimumCaptureEpoch = 0) noexcept;
    std::uint64_t requestFreshFFTFrameEpoch() noexcept;
    void pushDataPairToFFT(const juce::AudioBuffer<float>& processedBuffer,
                           const juce::AudioBuffer<float>& originalBuffer);
    bool processFFT(float* tempFFTData, int bufferSize);

    // save size
    void setSavedWidth(const int width);
    void setSavedHeight(const int height);
    int getSavedWidth() const;
    int getSavedHeight() const;

    // bypass
    bool getBypassedState() const noexcept;
    std::uint64_t getHostBypassPresentationEpoch() const noexcept;

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
    void splitBandsRange(const juce::AudioBuffer<float>& inputBuffer,
                         int startSample,
                         int numSamples,
                         double sampleRate);
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

    void shiftLfoModulationTargets(int startIndex,
                                   int endIndex,
                                   int shiftAmount,
                                   bool notifyHost = true);
    void clearLfoModulationForBand(int bandIndex,
                                   bool notifyHost = true);

    // Complete a band-insertion migration without depending on editor
    // lifetime. The returned topology is already fully published.
    bool addMultibandBand(int splitBandIndex,
                          int currentBandCount,
                          bool newBandIsOnLeft,
                          float crossoverFrequency);

    // Complete a band-removal migration without depending on editor lifetime.
    // Parameter notifications are synchronous and a host is allowed to close
    // the editor from any of them, so the processor must own the transaction.
    bool deleteMultibandBand(int deletedBandIndex, int currentBandCount);

    // Parameter migration for an add/remove operation is performed on the
    // message thread. Mark the start before the first slot/routing write, then
    // publish the completed transaction with requestMultibandTopologyReset().
    // While the sequence is odd the audio thread keeps using its last coherent
    // fixed-size snapshot instead of observing a half-migrated layout.
    void beginMultibandTopologyEdit();
    void requestMultibandTopologyReset() noexcept;
    bool isMultibandTopologyEditInProgress() const noexcept
    {
        return (multibandTopologyResetGeneration.load(std::memory_order_acquire)
                & 1u) != 0u;
    }
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    struct AudioCallbackRecipeForTesting
    {
        std::uint32_t generationAtCallbackStart = 0;
        std::uint32_t topologyPublicationSequence = 0;
        int numBands = 1;
        float band0OutputDb = 0.0f;
        bool requestedHq = false;
        float globalOutputDb = 0.0f;
        bool lofiEnabled = false;
        float lofiRate = 1.0f;
        float lfo1FreeRateHz = 1.0f;
    };

    void setSerializableStateReaderHookForTesting(std::function<void()> hook);
    void setHostStateMainCaptureHookForTesting(std::function<void()> hook);
    void setAudioCallbackStateCaptureHookForTesting(
        std::function<void()> hook);
    void setMultibandDeleteSnapshotHookForTesting(
        std::function<void()> hook);
    AudioCallbackRecipeForTesting
    getLastAudioCallbackRecipeForTesting() const noexcept;
    std::uint32_t getMultibandTopologyGenerationForTesting() const noexcept
    {
        return multibandTopologyResetGeneration.load(std::memory_order_seq_cst);
    }
    unsigned int getActiveSerializableStateReadersForTesting() const noexcept
    {
        return activeSerializableStateReaders.load(std::memory_order_seq_cst);
    }
    bool tryAcquireMultibandTopologyWriterLockForTesting() const noexcept
    {
        if (! multibandTopologyWriterLock.tryEnter())
            return false;

        multibandTopologyWriterLock.exit();
        return true;
    }
#endif

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

    struct ModulatedParameterSnapshot
    {
        ModulatedValueProvider provider;
        int lfoSourceIndex = -1;
    };

    struct GlobalFilterCallbackSnapshot
    {
        ChainSettings baseSettings;
        ModulatedParameterSnapshot lowCutFrequency;
        ModulatedParameterSnapshot lowCutGain;
        ModulatedParameterSnapshot lowCutQuality;
        ModulatedParameterSnapshot peakFrequency;
        ModulatedParameterSnapshot peakGain;
        ModulatedParameterSnapshot peakQuality;
        ModulatedParameterSnapshot highCutFrequency;
        ModulatedParameterSnapshot highCutGain;
        ModulatedParameterSnapshot highCutQuality;
        bool enabled = false;
    };

    struct AudioCallbackParameterSnapshot
    {
        LfoManager::AudioThreadParameterSnapshot lfoParameters;
        GlobalFilterCallbackSnapshot globalFilter;
        ModulatedParameterSnapshot globalOutput;
        ModulatedParameterSnapshot globalMix;
        ModulatedParameterSnapshot downsampleRate;
        ModulatedParameterSnapshot bitDepth;
        ModulatedParameterSnapshot jitter;
        ModulatedParameterSnapshot downsampleMix;
        std::uint32_t publicationSequence = 0;
        bool requestedHq = false;
        bool downsampleEnabled = false;
    };

    struct MultibandTopologySnapshot
    {
        HqCallbackContext callbackContext;
        std::array<float, 3> crossoverFrequencies { 200.0f, 1000.0f, 5000.0f };
        std::uint32_t publicationSequence = 0;
        int numBands = 1;
    };

    struct SerializableMainStateSnapshot
    {
        juce::ValueTree parameterState;
        std::vector<LfoData> lfoData;
        juce::Array<ModulationRouting> routings;
        int currentPresetID = 0;
        juce::String currentPresetKey;
        int editorWidth = static_cast<int>(INIT_WIDTH);
        int editorHeight = static_cast<int>(INIT_HEIGHT);
        juce::XmlElement abState { "AB_STATE" };
    };

    void initialiseParameterCache();
    CachedParameter cacheParameter(const juce::String& parameterID);
    static float loadCachedParameter(const CachedParameter& parameter, float fallback = 0.0f) noexcept;
    float getBlockModulatedValue(const CachedParameter& parameter,
                                 const juce::AudioBuffer<float>& lfoOutputs) const noexcept;
    float getModulatedValueAtSample(const CachedParameter& parameter,
                                    const juce::AudioBuffer<float>& lfoOutputs,
                                    int sampleIndex) const noexcept;
    float getSnapshotModulatedValueAtSample(
        const ModulatedParameterSnapshot& parameter,
        const juce::AudioBuffer<float>& lfoOutputs,
        int sampleIndex) const noexcept;
    ChainSettings getCachedChainSettings(const juce::AudioBuffer<float>* lfoOutputs) const noexcept;
    ChainSettings getCachedChainSettingsAtSample(const juce::AudioBuffer<float>& lfoOutputs,
                                                 int sampleIndex) const noexcept;
    ChainSettings getSnapshotChainSettingsAtSample(
        const juce::AudioBuffer<float>& lfoOutputs,
        int sampleIndex) const noexcept;
    bool hasActiveFilterModulation() const noexcept;
    void prepareAudioCallbackParameterSnapshot(
        std::uint32_t publicationSequence,
        AudioCallbackParameterSnapshot& snapshot) const;

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

    static constexpr std::uint64_t distortionGraphBandMask { 0x3u };
    static constexpr std::uint64_t initialDistortionGraphSourceToken { 0x4u };
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Distortion graph source publication must remain RT-safe");
    std::atomic<std::uint64_t> distortionGraphSourceToken {
        initialDistortionGraphSourceToken
    };
    // reset parameters
    void performReset();
    void resetMultibandProcessingState(
        const HqCallbackContext* callbackContext = nullptr) noexcept;
    bool addMultibandBandLocked(int splitBandIndex,
                                int currentBandCount,
                                bool newBandIsOnLeft,
                                float crossoverFrequency);
    bool deleteMultibandBandLocked(int deletedBandIndex,
                                   int currentBandCount);
    std::array<float, 3> getEffectiveCrossoverFrequencies(
        int crossoversToValidate) const noexcept;
    void snapCrossoverSmoothers(
        const std::array<float, 3>& frequencies) noexcept;
    void snapBandSoloGains(
        int snapshotNumBands,
        const HqCallbackContext& callbackContext) noexcept;
    void updateBandSoloGainEnvelope(
        int numSamples,
        bool useHQ,
        const std::array<bool, 4>& soloState,
        bool anySoloActive) noexcept;
    void synchroniseMultibandTopologyResetState() noexcept;
    SerializableMainStateSnapshot captureSerializableMainStateSnapshot() const;
    SerializableMainStateSnapshot captureCoherentSerializableMainStateSnapshot() const;
    SerializablePresetStateSnapshot
    captureCurrentSerializablePresetStateSnapshotForABFallback() const;
    bool tryCaptureMultibandTopologySnapshot(
        const juce::AudioBuffer<float>& lfoOutputs,
        std::uint32_t sequenceAtCallbackStart,
        bool routingSnapshotWasRefreshed,
        MultibandTopologySnapshot& snapshot,
        AudioCallbackParameterSnapshot& callbackParameters);
    void publishMultibandTelemetry(
        const HqCallbackContext& callbackContext,
        int snapshotNumBands,
        const juce::AudioBuffer<float>& lfoOutputs) noexcept;
    std::atomic<bool> needsReset { false };
    // Even values identify complete publications; odd values mean a
    // message-thread migration is in progress.
    std::atomic<std::uint32_t> multibandTopologyResetGeneration { 0 };
    // Writer-side only. The audio thread never enters this recursive lock; it
    // observes the odd/even publication sequence above.  A writer holds one
    // recursion level from beginMultibandTopologyEdit() until the matching
    // requestMultibandTopologyReset(), so another writer cannot publish an
    // outer transaction prematurely.
    juce::CriticalSection multibandTopologyWriterLock;
    int multibandTopologyEditDepth = 0;
    // Registered host-state readers finish copying APVTS before an outer
    // topology writer is allowed to mutate it. The seq_cst handshake with the
    // generation prevents the P->V / V->P listener-lock cycle.
    mutable std::atomic<unsigned int> activeSerializableStateReaders { 0 };
    std::shared_ptr<const SerializableMainStateSnapshot>
        mainStateBeforeTopologyEdit;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    mutable juce::CriticalSection serializableStateHookLock;
    mutable std::function<void()> serializableStateReaderHookForTesting;
    std::function<void()> hostStateMainCaptureHookForTesting;
    std::function<void()> audioCallbackStateCaptureHookForTesting;
    std::function<void()> multibandDeleteSnapshotHookForTesting;
#endif
    std::uint32_t appliedMultibandTopologyResetGeneration = 0;
    MultibandTopologySnapshot activeMultibandTopologySnapshot;
    bool activeMultibandTopologySnapshotInitialised = false;
    AudioCallbackParameterSnapshot activeAudioCallbackParameterSnapshot;
    bool activeAudioCallbackParameterSnapshotInitialised = false;
    std::uint32_t lastAudioCallbackGenerationAtStart = 0;

    friend class state::StateAB;

    std::vector<std::unique_ptr<BandProcessor>> bands;
    std::atomic<float> totalLatency { 0.0f };
    std::atomic<float> preparedHqLatency { 0.0f };

    enum class HqTransitionPhase;
    enum class TopologyTransitionPhase;

    void processWetBlock(juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midiMessages,
                         bool hostBypassShadow,
                         int playheadSampleOffset = 0,
                         bool publishMeterPacket = true);
    bool updateParameters(const juce::AudioBuffer<float>& lfoOutputs,
                          std::uint32_t topologySequenceAtCallbackStart,
                          bool routingSnapshotWasRefreshed,
                          HqCallbackContext& callbackContext);
    void prepareHqCallbackContext(
        const juce::AudioBuffer<float>& lfoOutputs,
        int snapshotNumBands,
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
    void processHqTransitionBlock(juce::AudioBuffer<float>& buffer,
                                  const juce::AudioBuffer<float>& lfoOutputs,
                                  double sampleRate,
                                  bool requestedHq,
                                  const HqCallbackContext& callbackContext,
                                  bool primeBypassDelay);
    void processActiveHqRange(
        juce::AudioBuffer<float>& buffer,
        juce::AudioBuffer<float>& delayMatchedDryBufferForRange,
        const std::array<juce::AudioBuffer<float>*, 4>& bandBuffers,
        const juce::AudioBuffer<float>& lfoOutputs,
        double sampleRate,
        bool useHQ,
        bool updateReductionMeter,
        const HqCallbackContext& callbackContext,
        bool applyFinalNonHqDelay = true,
        bool primeBypassDelay = true);
    void startHqTransitionRamp(float target,
                              HqTransitionPhase phase) noexcept;
    void applyHqTransitionRamp(juce::AudioBuffer<float>& buffer) noexcept;
    void resetHqQualityPathState() noexcept;
    void snapHqTransitionToParameter() noexcept;
    static bool sameTopologyIdentity(
        const MultibandTopologySnapshot& first,
        const MultibandTopologySnapshot& second) noexcept;
    bool hasPendingTopologyChange() const noexcept;
    void snapTopologyTransitionToActive() noexcept;
    void startTopologyTransitionRamp(
        float target,
        TopologyTransitionPhase phase) noexcept;
    void beginTopologyTransitionCallback() noexcept;
    void applyTopologyTransitionRamp(
        juce::AudioBuffer<float>& buffer) noexcept;
    bool commitPendingTopologySnapshot() noexcept;
    void processTopologyTransitionBlock(
        juce::AudioBuffer<float>& buffer,
        const juce::AudioBuffer<float>& lfoOutputs,
        double sampleRate,
        bool requestedHq,
        bool primeBypassDelay);

    // preset id
    int numBands = 1;
    int activeCrossovers = 0;

    std::unique_ptr<juce::PropertiesFile> appProperties;

    // Oscilloscope
    static constexpr int historyLength = 400;
    static constexpr int historyDecimationFactor = 10;
    static constexpr std::uint64_t historySourceMask = 0x7u;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "History source publication must remain RT-safe");
    std::array<std::atomic<float>, historyLength> historyArrayL {};
    std::array<std::atomic<float>, historyLength> historyArrayR {};
    std::atomic<int> historyWritePosition { 0 };
    std::atomic<int> historySamplesAvailable { 0 };
    // Audio-thread-only phase. Keeping it across callbacks makes the history
    // sampling interval independent of the host's block partitioning.
    int historySamplesUntilCapture = 0;
    // The message thread advances the epoch whenever the normalised source
    // changes. Keeping epoch and source in one atomic prevents the audio
    // thread from ever pairing one request's source with another's identity.
    std::atomic<std::uint64_t> historySourceRequestToken {
        static_cast<std::uint64_t>(globalHistorySourceIndex)
    };
    std::uint64_t activeHistorySourceToken =
        static_cast<std::uint64_t>(globalHistorySourceIndex);
    std::atomic<std::uint64_t> publishedHistorySourceToken {
        static_cast<std::uint64_t>(globalHistorySourceIndex)
    };
    std::atomic<std::uint64_t> historyGeneration { 0 };
    std::atomic<std::uint64_t> historyPublicationSequence { 0 };

    // Spectrum
    SpectrumProcessor spectrumProcessor;

    // dry audio buffer
    //    juce::AudioBuffer<float> mDryBuffer;
    juce::AudioBuffer<float> delayMatchedDryBuffer;
    // wet audio buffer
    juce::AudioBuffer<float> mWetBuffer;
    juce::AudioBuffer<float> hostBypassWetBuffer;
    juce::AudioBuffer<float> lfoOutputBuffer;
    juce::AudioBuffer<float> lofiDryBuffer;
    juce::AudioBuffer<float> globalMixAlignedDryBuffer;
    int preparedProcessingBlockCapacity = 1;

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
    juce::AudioBuffer<float> globalCutSlopeShadowBuffer;

    struct CutSlopeTransitionState
    {
        CutFilter leftStandby;
        CutFilter rightStandby;
        std::array<Slope, 2> slotSlopes { Slope_12, Slope_12 };
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> standbyMix;
        Slope requestedSlope { Slope_12 };
        float lastFrequency = 1000.0f;
        double lastSampleRate = 48000.0;
        bool highPass = false;
        bool initialised = false;
    };

    CutSlopeTransitionState lowCutSlopeTransition;
    CutSlopeTransitionState highCutSlopeTransition;

    ChainSettings cachedGlobalFilterSettings {};
    double cachedGlobalFilterSampleRate = 0.0;
    bool globalFilterCacheValid = false;

    void updateLowCutFilters(const ChainSettings& chainSettings, double sampleRate);
    void updateHighCutFilters(const ChainSettings& chainSettings, double sampleRate);
    void updatePeakFilter(const ChainSettings& chainSettings, double sampleRate);
    void updateCutSlopeTransition(CutSlopeTransitionState& transition,
                                  CutFilter& leftPrimary,
                                  CutFilter& rightPrimary,
                                  float frequency,
                                  double sampleRate,
                                  Slope requestedSlope,
                                  bool highPass);
    void processCutFilterStage(CutFilter& leftPrimary,
                               CutFilter& rightPrimary,
                               CutSlopeTransitionState& transition,
                               juce::dsp::AudioBlock<float>& fullBlock,
                               juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& wetMix,
                               int startSample,
                               int numSamples) noexcept;

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
    OutputGainTransitionState globalOutputGainTransition;
    // Retain JUCE's Thiran dry alignment, while the coefficient stage below
    // keeps routed LFO trajectories out of DryWetMixer's 50 ms smoother.
    juce::dsp::DryWetMixer<float> dryWetMixerGlobal { 2048 };
    ZeroLatencyModulatedDryWetMixer globalMixMixer;
    juce::dsp::DryWetMixer<float> bypassDelayMixer { 2048 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>
        nonHqOutputDelay { 2048 };

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

    enum class TopologyTransitionPhase
    {
        steady,
        fadingOut,
        warmingUp,
        fadingIn
    };

    TopologyTransitionPhase topologyTransitionPhase =
        TopologyTransitionPhase::steady;
    MultibandTopologySnapshot pendingMultibandTopologySnapshot;
    bool pendingMultibandTopologySnapshotInitialised = false;
    bool topologyPendingChangedThisCallback = false;
    float topologyTransitionGain = 1.0f;
    float topologyTransitionGainStep = 0.0f;
    int topologyTransitionRampSamples = 1;
    int topologyTransitionRampRemaining = 0;
    int topologyTransitionWarmupSamples = 1;
    int topologyTransitionWarmupRemaining = 0;

    ZeroLatencyModulatedDryWetMixer lofiMixer;
    juce::Random random;
    static constexpr size_t downsamplingStateChannels = 64;
    std::array<int, downsamplingStateChannels> downsampleSamplesRemaining {};
    std::array<float, downsamplingStateChannels> downsampleHeldSamples {};
    std::array<double, downsamplingStateChannels> downsampleHoldResiduals {};
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
    std::atomic<std::uint64_t> hostBypassPresentationEpoch { 0 };
    bool hostBypassSessionActive = false;
    bool hostBypassSessionHqMode = false;

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
    MeterValues lastPublishedMeterValues;
    void publishMeterValues(bool refreshBandMeters);
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
