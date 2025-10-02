/*
  ==============================================================================

    SpectrumComponent.h
    Created: 11 Nov 2018 9:40:21am
    Author:  lenovo
 
    MODIFIED to remove high-CPU waterfall effect and improve performance by
    drawing the spectrum directly and ensuring thread-safe data updates.

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <array>

//==============================================================================
/**
 A component that displays a frequency spectrum analyser.
 This version is optimized to draw directly to the graphics context, avoiding
 expensive image manipulation for effects like waterfalls, thus reducing CPU usage.
 It also uses AsyncUpdater for thread-safe repainting.
*/
class SpectrumComponent : public juce::Component,
                          public juce::AsyncUpdater
{
public:
    SpectrumComponent();
    SpectrumComponent(int style, bool drawPeak);
    ~SpectrumComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** This is called on the message thread to trigger a repaint. */
    void handleAsyncUpdate() override;

    /** Call this from your audio thread to update the spectrum data. */
    void updateSpectrum(const float* newData, int numBins, float binWidth);

    void setSpecAlpha(const float alp);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    /** Resets the peak-hold data. */
    void resetPeakData();

    int mStyle;
    bool mDrawPeak;

    std::atomic<float> mBinWidth;

    // Use a lock to protect access to spectrum data arrays from audio and message threads
    juce::CriticalSection dataLock;
    std::array<float, 1024> spectrumData;
    std::array<float, 1024> maxData;
    int numberOfBins;

    // Smoothing coefficients
    float attackCoeff;
    float releaseCoeff;

    // GUI-thread only members
    float maxDecibelValue = -100.0f;
    float maxFreq = 0.0f;
    bool mouseOver = false;
    juce::Point<float> maxDecibelPoint;
    float specAlpha = 0.8f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumComponent)
};
