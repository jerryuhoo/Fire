#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <exception>
#include <memory>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <process.h>
#else
 #include <pthread.h>
#endif

namespace
{
constexpr size_t audioStackBytes = 256u * 1024u;
constexpr int preparedBlockSize = 64;
constexpr double sampleRate = 48000.0;

void setStackTestParameter(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void configureStackTestProcessor(FireAudioProcessor& processor)
{
    processor.hasUpdateCheckBeenPerformed = true;
    setStackTestParameter(processor, NUM_BANDS_ID, 1);
    setStackTestParameter(processor, HQ_ID, 0);
    setStackTestParameter(processor, MIX_ID, 1);
    setStackTestParameter(processor, OUTPUT_ID, -3);
    for (int band = 0; band < 4; ++band)
    {
        const auto setBand = [&](const char* id, float value)
        { setStackTestParameter(processor, ParameterIDAndName::getIDString(id, band), value); };
        setBand(BAND_ENABLE_ID, 1);
        setBand(BAND_SOLO_ID, 0);
        setBand(LINKED_ID, 0);
        setBand(DRIVE_BYPASS_ID, 1);
        setBand(SHAPE_BYPASS_ID, 1);
        setBand(DRIVE_ID, 18 + band * 4.0f);
        setBand(SHAPE_MIX_ID, .75f);
        setBand(MIX_ID, 1);
        setBand(OUTPUT_ID, -3);
        REQUIRE(processor.setShapeMode(band + 1, -1, 12 + band));
    }
    // Added Shape instances exercise the extra CoreEffect/BandProcessor call
    // depth as well as the top-level callback snapshot capture.
    for (const int scope : {0, 1})
    {
        const int slot = processor.addInsertEffect(scope, fire::effects::Type::shape);
        REQUIRE(slot >= 0);
        REQUIRE(processor.setShapeMode(scope, slot, 12));
        setStackTestParameter(processor, fire::analog_params::driveID(scope, slot), 15);
    }
    REQUIRE(processor.assignLfoToTarget(0, "drive1") == LfoManager::AssignmentResult::changed);
    processor.setModulationDepth("drive1", .12f);
    processor.prepareToPlay(sampleRate, preparedBlockSize);
}

struct StackRenderTask
{
    FireAudioProcessor& processor;
    juce::AudioBuffer<float> block, output;
    int streamOffset = 0;
    int blocks = 0;
    bool bypassed = false;
    bool finite = true;
    int completedBlocks = 0;
    std::exception_ptr exception;

    StackRenderTask(FireAudioProcessor& owner, int blockSize, int count, int offset, bool bypass)
        : processor(owner), block(2, blockSize), output(2, blockSize * count),
          streamOffset(offset), blocks(count), bypassed(bypass)
    {
        block.clear();
        output.clear();
    }

    void render() noexcept
    {
        try
        {
            juce::MidiBuffer midi;
            for (int index = 0; index < blocks; ++index)
            {
                const int localOffset = index * block.getNumSamples();
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < block.getNumSamples(); ++sample)
                    {
                        const auto t = static_cast<double>(streamOffset + localOffset + sample) / sampleRate;
                        block.setSample(channel, sample, static_cast<float>(
                            .11 * std::sin(2.0 * juce::MathConstants<double>::pi * 137.0 * t + channel * .31)
                            + .055 * std::sin(2.0 * juce::MathConstants<double>::pi * 1837.0 * t)
                            + .025 * std::sin(2.0 * juce::MathConstants<double>::pi * 7013.0 * t)));
                    }
                if (bypassed) processor.processBlockBypassed(block, midi);
                else processor.processBlock(block, midi);
                for (int channel = 0; channel < 2; ++channel)
                {
                    output.copyFrom(channel, localOffset, block, channel, 0, block.getNumSamples());
                    for (int sample = 0; sample < block.getNumSamples(); ++sample)
                        finite = finite && std::isfinite(block.getSample(channel, sample));
                }
                ++completedBlocks;
            }
        }
        catch (...) { exception = std::current_exception(); }
    }
};

// A Windows thread's normal stack-size argument is only its initial commit;
// explicitly request the reservation so a larger Tests.exe linker stack cannot
// silently weaken this regression. POSIX sets the actual thread stack size.
class SmallAudioStackThread
{
public:
    ~SmallAudioStackThread() { join(); }
    bool start(StackRenderTask& task)
    {
#if JUCE_WINDOWS
        const auto result = ::_beginthreadex(nullptr, static_cast<unsigned>(audioStackBytes), entry,
            &task, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
        handle = reinterpret_cast<HANDLE>(result);
        return handle != nullptr;
#else
        pthread_attr_t attributes;
        if (pthread_attr_init(&attributes) != 0) return false;
        const int configured = pthread_attr_setstacksize(&attributes, audioStackBytes);
        const int created = configured == 0 ? pthread_create(&handle, &attributes, entry, &task) : configured;
        pthread_attr_destroy(&attributes);
        joinable = created == 0;
        return joinable;
#endif
    }
    void join() noexcept
    {
#if JUCE_WINDOWS
        if (handle == nullptr) return;
        ::WaitForSingleObject(handle, INFINITE);
        ::CloseHandle(handle);
        handle = nullptr;
#else
        if (!joinable) return;
        pthread_join(handle, nullptr);
        joinable = false;
#endif
    }
private:
#if JUCE_WINDOWS
    static unsigned __stdcall entry(void* data)
    { static_cast<StackRenderTask*>(data)->render(); return 0; }
    HANDLE handle = nullptr;
#else
    static void* entry(void* data)
    { static_cast<StackRenderTask*>(data)->render(); return nullptr; }
    pthread_t handle {};
    bool joinable = false;
#endif
};
}

TEST_CASE("Real processing and callback resets fit a 256 KiB audio thread stack",
          "[processor][audio-stack][realtime][hq][topology][host-bypass][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto subject = std::make_unique<FireAudioProcessor>();
    auto reference = std::make_unique<FireAudioProcessor>();
    configureStackTestProcessor(*subject);
    configureStackTestProcessor(*reference);
    int streamOffset = 0;
    const auto run = [&](int blockSize, int count, bool bypassed = false)
    {
        CAPTURE(blockSize, count, bypassed, streamOffset);
        StackRenderTask smallStack(*subject, blockSize, count, streamOffset, bypassed);
        StackRenderTask ordinaryStack(*reference, blockSize, count, streamOffset, bypassed);
        SmallAudioStackThread worker;
        REQUIRE(worker.start(smallStack));
        // Process another instance concurrently: scratch must belong to each
        // processor, rather than a process-wide static or shared buffer.
        ordinaryStack.render();
        worker.join();
        REQUIRE_FALSE(smallStack.exception);
        REQUIRE_FALSE(ordinaryStack.exception);
        REQUIRE(smallStack.completedBlocks == count);
        REQUIRE(ordinaryStack.completedBlocks == count);
        CHECK(smallStack.finite);
        CHECK(ordinaryStack.finite);
        float maximumError = 0;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < smallStack.output.getNumSamples(); ++sample)
                maximumError = juce::jmax(maximumError, std::abs(
                    smallStack.output.getSample(channel, sample) - ordinaryStack.output.getSample(channel, sample)));
        CHECK(maximumError < 1.0e-5f);
        CHECK(smallStack.output.getMagnitude(0, smallStack.output.getNumSamples()) > 1.0e-4f);
        streamOffset += blockSize * count;
    };

    run(64, 32);
    for (auto* processor : {subject.get(), reference.get()})
    {
        processor->beginMultibandTopologyEdit();
        const juce::ScopeGuard publish {[processor] { processor->requestMultibandTopologyReset(); }};
        setStackTestParameter(*processor, NUM_BANDS_ID, 4);
        setStackTestParameter(*processor, "freq1", 180);
        setStackTestParameter(*processor, "freq2", 1700);
        setStackTestParameter(*processor, "freq3", 6200);
        setStackTestParameter(*processor, HQ_ID, 1);
    }
    run(64, 64); // Complete topology and HQ transitions on the small stack.
    run(1025, 8); // Host block exceeds the prepared capacity: streamed ranges.
    run(513, 8, true); // Bypass also renders the hidden wet graph.
    run(128, 16);
    for (auto* processor : {subject.get(), reference.get()})
    {
        setStackTestParameter(*processor, HQ_ID, 0);
        processor->reset(); // Deferred reset/capture executes in the next callback.
    }
    run(257, 16);
    subject->releaseResources();
    reference->releaseResources();
}
