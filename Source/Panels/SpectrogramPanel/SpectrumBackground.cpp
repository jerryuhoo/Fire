/*
  ==============================================================================

    SpectrumBackground.cpp
    Created: 22 May 2024 3:13:06pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "SpectrumBackground.h"
#include "SpectrumComponent.h"
#include "../../Utility/AudioHelpers.h"

const int SpectrumBackground::frequenciesForLines[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
const int SpectrumBackground::numberOfLines = 28;
//==============================================================================
SpectrumBackground::SpectrumBackground() : numberOfBins (1024), mBinWidth (44100 / (float) 2048)
{
}

SpectrumBackground::~SpectrumBackground()
{
}

void SpectrumBackground::paint (juce::Graphics& g)
{
    // paint background
    g.setColour (COLOUR6);
    g.fillAll();
    
    // Set a scalable font size for the frequency labels
    g.setFont(12.0f * scale);

    // paint horizontal lines and frequency numbers
    g.setColour (juce::Colours::lightgrey.withAlpha (0.2f));
    g.drawLine (0, getHeight() / 5, getWidth(), getHeight() / 5, 1);

    for (int i = 0; i < numberOfLines; ++i)
    {
        const double proportion = frequenciesForLines[i] / 20000.0;
        int xPos = transformToLog (proportion * 20000) * (getWidth());
        g.drawVerticalLine (xPos, getHeight() / 5, getHeight());
        
        const int scaledWidth = (int)(60 * scale);
        const int scaledXOffset = (int)(30 * scale);
        
        juce::Rectangle<int> textBounds(xPos - scaledXOffset, 0, scaledWidth, getHeight() / 5);

        if (frequenciesForLines[i] == 10 || frequenciesForLines[i] == 100 || frequenciesForLines[i] == 200)
            g.drawFittedText (static_cast<juce::String> (frequenciesForLines[i]), textBounds, juce::Justification::centred, 2);
        else if (frequenciesForLines[i] == 1000 || frequenciesForLines[i] == 10000 || frequenciesForLines[i] == 2000)
            g.drawFittedText (static_cast<juce::String> (frequenciesForLines[i] / 1000) + "k", textBounds, juce::Justification::centred, 2);
        else if (frequenciesForLines[i] == 20)
            g.drawFittedText (static_cast<juce::String> (frequenciesForLines[i]), textBounds, juce::Justification::right, 2);
        else if (frequenciesForLines[i] == 20000)
            g.drawFittedText (static_cast<juce::String> (frequenciesForLines[i] / 1000) + "k", textBounds, juce::Justification::left, 2);
    }

    // paint vertical db numbers
    //    float fontWidth = 50;
    //    float fontHeight = getHeight() / 5;
    //    float centerAlign = fontHeight / 2;
    //    g.drawFittedText("-20 db", 0, getHeight() / 6 * 2 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    //    g.drawFittedText("-40 db", 0, getHeight() / 6 * 3 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    //    g.drawFittedText("-60 db", 0, getHeight() / 6 * 4 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    //    g.drawFittedText("-80 db", 0, getHeight() / 6 * 5 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
}

void SpectrumBackground::resized()
{
    // Add this block to get the scale factor from the editor
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* lnf = dynamic_cast<FireLookAndFeel*>(&editor->getLookAndFeel()))
            scale = lnf->scale;
}
