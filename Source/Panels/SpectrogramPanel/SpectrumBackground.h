/*
  ==============================================================================

    SpectrumBackground.h
    Created: 22 May 2024 3:13:06pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class SpectrumBackground : public juce::Component
{
public:
    SpectrumBackground();
    ~SpectrumBackground();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int numberOfBins;

    static const int frequenciesForLines[];
    static const int numberOfLines;
    float mBinWidth;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumBackground)
};
