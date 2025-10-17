/*
  ==============================================================================

    SpectrumComponent.h
    Created: 2 Oct 2025
    Author:  Yifeng Yu
 
    MODIFIED to implement timer-based interpolation and spatial smoothing for
    a more fluid visual experience.

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <array>

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

    /** Call this from your audio thread to update the spectrum data. */
    void updateSpectrum(const float* newData, int numBins, float binWidth);

    void setSpecAlpha(const float alp);

private:
    /** This is called on the message thread to trigger a repaint. Now handled by timer. */
    void handleAsyncUpdate() override;

    /** Timer callback for smooth, interpolated rendering at a fixed frame rate. */
    void timerCallback() override;

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    /** Resets the peak-hold data. */
    void resetPeakData();

    int mStyle;
    bool mDrawPeak;

    std::atomic<float> mBinWidth;

    // Use a lock to protect access to spectrum data arrays from audio and message threads
    juce::CriticalSection dataLock;
    std::array<float, 1024> spectrumData; // Holds the latest data from the audio thread
    std::array<float, 1024> displayData; // Holds the interpolated data used for painting
    std::array<float, 1024> maxData;
    int numberOfBins;

    // New parameter for controlling animation smoothness
    float interpolationFactor = 0.2f;

    // GUI-thread only members
    float maxDecibelValue = -100.0f;
    float maxFreq = 0.0f;
    bool mouseOver = false;
    juce::Point<float> maxDecibelPoint;
    float specAlpha = 0.8f;
    bool isPeakLineVisible = false;
    bool wasMouseOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumComponent)
};