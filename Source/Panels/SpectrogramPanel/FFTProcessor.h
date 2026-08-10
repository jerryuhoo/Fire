/*
  ==============================================================================

    FFTProcessor.h
    Created: 12 Nov 2018 2:16:02pm
    Author:  lenovo

  ==============================================================================
*/

#pragma once
#include "juce_dsp/juce_dsp.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

class SpectrumProcessor
{
public:
    SpectrumProcessor() : forwardFFT (fftOrder), window (fftSize, juce::dsp::WindowingFunction<float>::hamming)
    {
        window.fillWindowingTables (fftSize, juce::dsp::WindowingFunction<float>::blackman);
    }

    enum
    {
        fftOrder = 11,
        fftSize = 1 << fftOrder, // 2048
        numBins = fftSize / 2, // 1024
        fftBufferSize = 2 * fftSize
    };

    void pushNextSamplePairIntoFifo (float processedSample, float originalSample) noexcept
    {
        const auto index = static_cast<size_t>(fifoIndex++);
        processedFifo[index] = std::isfinite(processedSample) ? processedSample : 0.0f;
        originalFifo[index] = std::isfinite(originalSample) ? originalSample : 0.0f;

        if (fifoIndex == fftSize)
        {
            publishCompletedFrame();

            fifoIndex = 0;
        }
    }

    bool doProcessing(float* tempFFTData, int bufferSize)
    {
        if (tempFFTData == nullptr || bufferSize < fftBufferSize)
            return false;

        window.multiplyWithWindowingTable (tempFFTData, fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform (tempFFTData);
        return true;
    }

    bool hasCompleteFrame() const noexcept
    {
        const auto state = publishedState.load(std::memory_order_acquire);
        return state != 0 && state != lastConsumedState;
    }

    /**
        Copies the newest complete frame and discards any older queued frames.
        The audio thread is the sole producer and the message thread is the sole
        consumer, so this never requires either thread to wait for the other.
    */
    bool popLatestFramePair (float* processedDestination,
                             int processedDestinationSize,
                             float* originalDestination,
                             int originalDestinationSize) noexcept
    {
        if (processedDestination == nullptr || processedDestinationSize < fftBufferSize
            || originalDestination == nullptr || originalDestinationSize < fftBufferSize)
            return false;

        for (int attempt = 0; attempt < frameStorageSize; ++attempt)
        {
            const auto state = publishedState.load(std::memory_order_acquire);
            if (state == 0 || state == lastConsumedState)
                return false;

            const int newestFrameIndex = decodeFrameIndex(state);
            jassert(juce::isPositiveAndBelow(newestFrameIndex, frameStorageSize));

            // Claim the slot, then confirm it is still the published one. The
            // producer never overwrites either the published or claimed slot.
            readerFrameIndex.store(newestFrameIndex, std::memory_order_release);
            if (publishedState.load(std::memory_order_acquire) != state)
            {
                readerFrameIndex.store(-1, std::memory_order_release);
                continue;
            }

            juce::FloatVectorOperations::clear(processedDestination, processedDestinationSize);
            juce::FloatVectorOperations::clear(originalDestination, originalDestinationSize);
            const auto& newestFrame = frameStorage[static_cast<size_t>(newestFrameIndex)];
            std::copy_n(newestFrame.processed.data(), fftSize, processedDestination);
            std::copy_n(newestFrame.original.data(), fftSize, originalDestination);

            lastConsumedState = state;
            readerFrameIndex.store(-1, std::memory_order_release);
            return true;
        }

        readerFrameIndex.store(-1, std::memory_order_release);
        return false;
    }

private:
    // Triple buffering leaves one writable slot while one is published and
    // another may be held by the UI. New frames replace old unpublished data,
    // so resuming the UI always displays the most recent complete pair.
    static constexpr int frameStorageSize = 3;

    struct FramePair
    {
        std::array<float, fftSize> processed {};
        std::array<float, fftSize> original {};
    };

    static int decodeFrameIndex(std::uint64_t state) noexcept
    {
        return static_cast<int>((state & 0x3u) - 1u);
    }

    void publishCompletedFrame() noexcept
    {
        const auto currentState = publishedState.load(std::memory_order_acquire);
        const int publishedIndex = currentState == 0 ? -1 : decodeFrameIndex(currentState);
        const int claimedIndex = readerFrameIndex.load(std::memory_order_acquire);

        int destinationIndex = -1;
        for (int offset = 0; offset < frameStorageSize; ++offset)
        {
            const int candidate = (nextWriteIndex + offset) % frameStorageSize;
            if (candidate != publishedIndex && candidate != claimedIndex)
            {
                destinationIndex = candidate;
                break;
            }
        }

        jassert(juce::isPositiveAndBelow(destinationIndex, frameStorageSize));
        if (destinationIndex < 0)
            return;

        auto& destination = frameStorage[static_cast<size_t>(destinationIndex)];
        std::copy (processedFifo.begin(), processedFifo.end(), destination.processed.begin());
        std::copy (originalFifo.begin(), originalFifo.end(), destination.original.begin());

        const auto generation = (currentState >> 2u) + 1u;
        const auto newState = (generation << 2u)
                              | static_cast<std::uint64_t>(destinationIndex + 1);
        publishedState.store(newState, std::memory_order_release);
        nextWriteIndex = (destinationIndex + 1) % frameStorageSize;
    }

    std::array<float, fftSize> processedFifo {};
    std::array<float, fftSize> originalFifo {};
    std::array<FramePair, frameStorageSize> frameStorage {};
    std::atomic<std::uint64_t> publishedState { 0 };
    std::atomic<int> readerFrameIndex { -1 };
    std::uint64_t lastConsumedState = 0; // Message-thread owned.
    int nextWriteIndex = 0; // Audio-thread owned.
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    int fifoIndex = 0;
};
