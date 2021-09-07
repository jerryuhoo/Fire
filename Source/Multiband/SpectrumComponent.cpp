/*
  ==============================================================================

    SpectrumComponent.cpp
    Created: 11 Nov 2018 9:40:21am
    Author:  lenovo

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SpectrumComponent.h"

const int SpectrumComponent::frequenciesForLines[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
const int SpectrumComponent::numberOfLines = 28;
//==============================================================================
SpectrumComponent::SpectrumComponent()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
}

SpectrumComponent::~SpectrumComponent()
{
}

void SpectrumComponent::paint (juce::Graphics& g)
{
    /* This demo code just fills the component's background and
       draws some placeholder text to get you started.

       You should replace everything in this method with your own
       drawing code..
    */
    
    // paint horizontal lines and frequency numbers
    g.setColour(COLOUR1.withAlpha(0.2f));
    g.drawLine(0, getHeight() / 5, getWidth(), getHeight() / 5, 1);
    for (int i = 0; i < numberOfLines; ++i)
    {
        const double proportion = frequenciesForLines[i] / (44100 / 2.0);
        int xPos = transformToLog(proportion) * (getWidth());
        g.drawVerticalLine(xPos, getHeight() / 5, getHeight());
        if (frequenciesForLines[i] == 10 || frequenciesForLines[i] == 100 || frequenciesForLines[i] == 20 || frequenciesForLines[i] == 200)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i]), xPos - 30, 0, 60, getHeight() / 5, juce::Justification::centred, 2);
        else if ( frequenciesForLines[i] == 1000 || frequenciesForLines[i] == 10000 || frequenciesForLines[i] == 2000 || frequenciesForLines[i] == 20000)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i] / 1000) + "k", xPos - 30, 0, 60, getHeight() / 5, juce::Justification::centred, 2);
    }
    
    // paint vertical db numbers
    // g.drawFittedText("db", 0, 0, 60, getHeight() / 5, juce::Justification::centred, 2);
    float fontWidth = 50;
    float fontHeight = getHeight() / 5;
    float centerAlign = fontHeight / 2;
    //g.drawFittedText("-20 db", 0, getHeight() / 6 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    g.drawFittedText("-20 db", 0, getHeight() / 6 * 2 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    g.drawFittedText("-40 db", 0, getHeight() / 6 * 3 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    g.drawFittedText("-60 db", 0, getHeight() / 6 * 4 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    g.drawFittedText("-80 db", 0, getHeight() / 6 * 5 - centerAlign, fontWidth, fontHeight, juce::Justification::centred, 2);
    
    // paint graph
	paintSpectrum(g);
}

void SpectrumComponent::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}

void SpectrumComponent::paintSpectrum(juce::Graphics & g)
{
    // this method is to paint spectrogram
    auto width = getLocalBounds().getWidth();
    auto height = getLocalBounds().getHeight();
    
    juce::Path p;
    p.startNewSubPath(0, height);
    
    auto mindB = -100.0f;
    auto maxdB =    0.0f;
    
    for (int i = 0; i < numberOfBins; i++)
    {
        // sample range [0, 1] to decibel range[-∞, 0] to [0, 1]
        // 4096 is 1 << 11, which is fftSize.
        auto fftSize = 1 << 11;
        float yPercent = juce::jmap (juce::jlimit (mindB, maxdB, juce::Decibels::gainToDecibels (spectrumData[i])
                                                 - juce::Decibels::gainToDecibels(static_cast<float>(fftSize))),
                       mindB, maxdB, 0.0f, 1.0f);
        
        // skip some points to save cpu
//        if (i > numberOfBins / 8 && i % 2 != 0) continue;
//        if (i > numberOfBins / 4 && i % 3 != 0) continue;
//        if (i > numberOfBins / 2 && i % 4 != 0) continue;
//        if (i > numberOfBins / 4 * 3 && i % 10 != 0) continue;
        
        // connect points
        p.lineTo(transformToLog((float)i / numberOfBins) * width,
                         juce::jmap (yPercent, 0.0f, 1.0f, (float) height, 0.0f));
        
        // reference: https://docs.juce.com/master/tutorial_spectrum_analyser.html
    }

    // this step is to round the path
    juce::Path roundedPath = p.createPathWithRoundedCorners(10.0f);
    
    // draw the outline of the path
//    g.setColour (COLOUR7.withAlpha(0.5f));
//    g.strokePath(roundedPath, juce::PathStrokeType(4.0f));
    
    roundedPath.lineTo(width, height);
    roundedPath.lineTo(0, height);
    
    g.setColour (COLOUR1);
            
    juce::ColourGradient grad(juce::Colours::red.withAlpha(0.5f), 0, 0,
                              COLOUR1.withAlpha(0.5f), 0, getLocalBounds().getHeight(), false);
    
//    juce::ColourGradient grad(juce::Colours::red, 0, 0,
//                              juce::Colours::yellow, 0, getLocalBounds().getHeight(), false);
    
    g.setGradientFill(grad);
    
    g.fillPath(roundedPath);
}

void SpectrumComponent::prepareToPaintSpectrum(int numBins, float * data)
{
	numberOfBins = numBins;
    memmove(spectrumData, data, sizeof(spectrumData));
}

float SpectrumComponent::transformToLog(float between0and1)
{
	const float minimum = 1.0f;
	const float maximum = 1000.0f;
	return log10(minimum + ((maximum - minimum) * between0and1)) / log10(maximum);
}

float SpectrumComponent::transformFromLog(float valueToTransform)
{
    const float minimum = 1.0f;
    const float maximum = 1000.0f;
    return (std::pow(10, valueToTransform * log10(maximum)) - minimum) / static_cast<float>(maximum - minimum);
}

