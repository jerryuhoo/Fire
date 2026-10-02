/*
  ==============================================================================

    SpectrumBackground.h
    Created: 22 May 2024 3:13:06pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/FireTheme.h"
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
    ~SpectrumBackground() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

private:
    void createBackgroundImage();

    static const int frequenciesForLines[];
    static const int frequenciesForTextLabels[];
    float scale = 1.0f;

    // The cached image for our static background.
    juce::Image cachedBackground;
    float lastDisplayScale = 1.0f;
    float cachedUiScale = 0.0f;
    juce::Rectangle<int> cachedLogicalBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumBackground)
};
