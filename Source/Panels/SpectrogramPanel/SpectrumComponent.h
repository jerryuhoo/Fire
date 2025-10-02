/*
  ==============================================================================

    SpectrumComponent.h
    Created: 11 Nov 2018 9:40:21am
    Author:  lenovo

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_gui_basics/juce_gui_basics.h"

//==============================================================================
/*
*/
class SpectrumComponent : public juce::Component
{
public:
    SpectrumComponent();
    SpectrumComponent(int style, bool drawPeak);
    ~SpectrumComponent();

    void paint(juce::Graphics& g) override;
    void prepareToPaintSpectrum(int numberOfBins, float* spectrumData, float binWidth);
    void setSpecAlpha(const float alp);
    void resized() override;
    void paintSpectrum();

private:
    int mStyle;
    bool mDrawPeak;
    int numberOfBins;
    float spectrumData[1024] = { 0 };
    float maxData[1024] = { 0 };
    float maxDecibelValue = -100.0f;
    float maxFreq = 0.0f;
    bool mouseOver = false;
    juce::Point<float> maxDecibelPoint;

    juce::Image currentSpectrumImage = juce::Image(juce::Image::ARGB, 1000, 300, true);
    juce::Image maxSpectrumImage = juce::Image(juce::Image::ARGB, 1000, 300, true);

    float mBinWidth;
    float specAlpha = 0.8f;

    juce::ColourGradient specGrad;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumComponent)
};
