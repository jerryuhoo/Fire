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
    for (int i = 0; i < numberOfLines; ++i)
    {
        const double proportion = frequenciesForLines[i] / (44100 / 2.0);
        int xPos = transformToLog(proportion) * (getWidth());
        g.drawVerticalLine(xPos, getHeight() / 5, getHeight());
        if (frequenciesForLines[i] == 10 || frequenciesForLines[i] == 100 || frequenciesForLines[i] == 20 || frequenciesForLines[i] == 200)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i]), xPos - 5, 0, 60, 30, juce::Justification::left, 2);
        else if ( frequenciesForLines[i] == 1000 || frequenciesForLines[i] == 10000 || frequenciesForLines[i] == 2000 || frequenciesForLines[i] == 20000)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i] / 1000) + "k", xPos - 20, 0, 60, 30, juce::Justification::left, 2);
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

	float yPercent = spectrum_data[0]> 0 ? float(0.5 + (juce::Decibels::gainToDecibels(spectrum_data[0]) / 150)) : -0.01;
    auto width = getLocalBounds().getWidth();
    auto height = getLocalBounds().getHeight();
    
    juce::Path p;
    p.startNewSubPath(0, height);
    //p.lineTo(0, juce::jmap (scopeData[0], 0.0f, 1.0f, (float) height, 0.0f));
    for (int i = 0; i < number_of_bins; i++)
    {
        yPercent = spectrum_data[i]> 0 ? float(0.5 + (juce::Decibels::gainToDecibels(spectrum_data[i]) / 150)) : -0.01;
        p.lineTo(transformToLog((float)i / number_of_bins) * width,
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
	number_of_bins = numBins;
	spectrum_data = data;
	//repaint();
}

float SpectrumComponent::transformToLog(float between0and1)
{
	const float minimum = 1.0f;
	const float maximum = 1000.0f;
	return log10(minimum + ((maximum - minimum) * between0and1)) / log10(maximum);
}
