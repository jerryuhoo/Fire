#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <limits>

using HostBypassDenormalStateHook = void (*) (bool) noexcept;

void setHostBypassDenormalStateHookForTesting(
    HostBypassDenormalStateHook hook) noexcept;

namespace
{
std::atomic<int> hookCallCount { 0 };
std::atomic<bool> hookObservedDisabledDenormals { false };

void captureHostBypassDenormalState(bool denormalsAreDisabled) noexcept
{
    hookObservedDisabledDenormals.store(denormalsAreDisabled,
                                        std::memory_order_relaxed);
    hookCallCount.fetch_add(1, std::memory_order_relaxed);
}

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void setLayout(FireAudioProcessor& processor, int numChannels)
{
    const auto channelSet = numChannels == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    REQUIRE(processor.setBusesLayout(layout));
}
} // namespace

TEST_CASE("Host bypass disables denormal processing for its complete audible path",
          "[processor][host-bypass][denormals][realtime]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 257;
    const bool denormalsWereInitiallyDisabled =
        juce::FloatVectorOperations::areDenormalsDisabled();
    const juce::ScopeGuard restoreThreadAndHookState {
        [denormalsWereInitiallyDisabled]
        {
            setHostBypassDenormalStateHookForTesting(nullptr);
            juce::FloatVectorOperations::disableDenormalisedNumberSupport(
                denormalsWereInitiallyDisabled);
        }
    };

    // Exercise the contract that the plug-in itself establishes FTZ/DAZ rather
    // than inheriting it from a friendly host or test runner.
    juce::FloatVectorOperations::disableDenormalisedNumberSupport(false);
    REQUIRE_FALSE(juce::FloatVectorOperations::areDenormalsDisabled());
    setHostBypassDenormalStateHookForTesting(
        &captureHostBypassDenormalState);

    for (const bool useHq : std::array { false, true })
    {
        for (const int numChannels : std::array { 1, 2 })
        {
            DYNAMIC_SECTION((useHq ? "HQ" : "Base")
                            << ", channels=" << numChannels)
            {
                hookCallCount.store(0, std::memory_order_relaxed);
                hookObservedDisabledDenormals.store(false,
                                                     std::memory_order_relaxed);

                FireAudioProcessor processor;
                setLayout(processor, numChannels);
                setPlainParameter(processor, HQ_ID, useHq ? 1.0f : 0.0f);
                processor.prepareToPlay(sampleRate, blockSize);

                juce::AudioBuffer<float> buffer(numChannels, blockSize);
                const float subnormal = std::numeric_limits<float>::denorm_min();
                for (int channel = 0; channel < numChannels; ++channel)
                    buffer.clear(channel, 0, blockSize);
                buffer.setSample(0, 0, subnormal);
                if (numChannels > 1)
                    buffer.setSample(1, 0, -subnormal);

                juce::MidiBuffer midi;
                processor.processBlockBypassed(buffer, midi);

                CHECK(hookCallCount.load(std::memory_order_relaxed) == 1);
                CHECK(hookObservedDisabledDenormals.load(
                    std::memory_order_relaxed));
                // ScopedNoDenormals must restore the caller's FP mode as well as
                // protecting the work inside the callback.
                CHECK_FALSE(
                    juce::FloatVectorOperations::areDenormalsDisabled());
            }
        }
    }
}
