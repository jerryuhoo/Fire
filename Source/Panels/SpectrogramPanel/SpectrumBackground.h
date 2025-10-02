/*
  ==============================================================================

    SpectrumBackground.h
    Created: 22 May 2024 3:13:06pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/LookAndFeel.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"

//==============================================================================
/*
*/
class SpectrumBackground : public juce::Component
{
public:
    SpectrumBackground();
    ~SpectrumBackground();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void createBackgroundImage();

    int numberOfBins;

    static const int frequenciesForLines[];
    static const int numberOfLines;
    static const int frequenciesForTextLabels[];
    float mBinWidth;
    float scale = 1.0f;

    // The cached image for our static background.
    juce::Image cachedBackground;
    float lastDisplayScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumBackground)
};
