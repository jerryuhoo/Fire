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
 
    // paint lines and numbers
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
	paintSpectrum(g);
}

void SpectrumComponent::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
}

void SpectrumComponent::paintSpectrum(juce::Graphics & g)
{
    auto width = getLocalBounds().getWidth();
    auto height = getLocalBounds().getHeight();
    
    juce::Path p;
    p.startNewSubPath(0, height);
    
    for (int i = 0; i < numberOfBins; i++)
    {
        float yPercent = spectrumData[i]> 0 ? float(0.5 + (juce::Decibels::gainToDecibels(spectrumData[i]) / 150)) : -0.01;
        p.lineTo(transformToLog((float)i / numberOfBins) * width,
                         juce::jmap (yPercent, 0.0f, 1.0f, (float) height, 0.0f));
//            p.lineTo(transformToLog((float)i / scopeSize) * width,
//                             height - height * scopeData[i]);
    }

    p.lineTo(width, height);
    p.lineTo(0, height);
    
    g.setColour (COLOUR1);
            
    juce::ColourGradient grad(juce::Colours::red, 0, 0,
                              COLOUR1, 0, getLocalBounds().getHeight(), false);
    
//    juce::ColourGradient grad(juce::Colours::red, 0, 0,
//                              juce::Colours::yellow, 0, getLocalBounds().getHeight(), false);
    
    g.setGradientFill(grad);
    
    g.fillPath(p);
}

void SpectrumComponent::prepareToPaintSpectrum(int numBins, float * data)
{
	numberOfBins = numBins;
	spectrumData = data;
	//repaint();
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

