/*
  ==============================================================================

    SpectrumComponent.h
    Created: 2 Oct 2025
    Author:  Yifeng Yu
 
    Message-thread presentation of FFT bins with bounded attack/release and
    cached, peak-preserving frequency geometry.

  ==============================================================================
*/

#pragma once

#include "../../GUI/FireTheme.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <array>
#include <cstdint>

struct SpectrumComponentTestAccess;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct SpectrumHostBypassPresentationTestAccess;
#endif

//==============================================================================
class SpectrumComponent : public juce::Component,
                          public juce::AsyncUpdater,
                          private juce::Timer // Inherit from juce::Timer for smooth rendering
{
public:
    SpectrumComponent();
    SpectrumComponent(int style, bool drawPeak);
    ~SpectrumComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Thread-safe. The data is copied and rendered on the message thread. */
    void updateSpectrum(const float* newData, int numBins, float binWidth);

    void setSpecAlpha(const float alp);

    /**
        Starts or ends the host-bypass presentation transition.

        This must be called on the message thread. Entering bypass fades the
        current trace to silence before discarding it. Leaving bypass keeps the
        trace hidden until updateSpectrum() publishes a newer frame, so a
        retained pre-bypass path can never flash back onto the analyser.
    */
    void setHostBypassed(bool shouldBeBypassed, bool animate = true);

private:
    friend struct SpectrumComponentTestAccess;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct SpectrumHostBypassPresentationTestAccess;
#endif

    void handleAsyncUpdate() override;
    void timerCallback() override;

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

    void resetPeakData();
    void resetRenderedData();
    bool consumePendingFrame(bool startFromSilence);
    void setMouseOverSpectrum(bool shouldBeOver);
    void resetHoverPresentation();
    void rebuildFrequencyLayout();
    void rebuildPaths();
    void updateAnimationTimer();

    int mStyle;
    bool mDrawPeak;

    float mBinWidth = 44100.0f / 2048.0f;

    // Producer data is copied into message-thread-owned arrays before rendering.
    juce::CriticalSection dataLock;
    std::array<float, 1024> pendingData {};
    std::array<float, 1024> targetData {};
    std::array<float, 1024> displayData {};
    std::array<float, 1024> maxData {};
    int pendingNumberOfBins = 1024;
    float pendingBinWidth = 44100.0f / 2048.0f;
    int numberOfBins = 1024;
    std::atomic<std::uint64_t> pendingGeneration { 0 };
    std::uint64_t consumedGeneration = 0;

    float interpolationFactor = 0.2f;
    static constexpr float releaseFactor = 0.12f;
    bool interpolationActive = false;
    bool geometryDirty = true;
    bool renderedDataIsClear = true;

    struct FrequencyBinPosition
    {
        int bin = 0;
        int bucket = 0;
        float x = 0.0f;
    };
    std::array<FrequencyBinPosition, 1024> frequencyLayout {};
    int visibleFrequencyBins = 0;
    bool frequencyLayoutDirty = true;

    // Host-bypass is a presentation epoch as well as an opacity animation.
    // Updates that race with entry are rejected by acceptingSpectrumUpdates;
    // consumedGeneration is snapped at each boundary so only a post-resume
    // generation may release the hidden trace.
    std::atomic<bool> acceptingSpectrumUpdates { true };
    bool hostBypassed = false;
    bool awaitingFreshFrame = false;
    fire::ui::DampedValue presentationOpacity;

    juce::Path spectrumLinePath;
    juce::Path spectrumFillPath;
    juce::Path peakLinePath;

    // GUI-thread only members
    float maxDecibelValue = -100.0f;
    float maxFreq = 0.0f;
    bool mouseOver = false;
    fire::ui::DampedValue hoverOpacity;
    juce::Point<float> maxDecibelPoint;
    float specAlpha = 0.8f;
    bool isPeakLineVisible = false;
    juce::Component* observedMouseSource = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumComponent)
};
