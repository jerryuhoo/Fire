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
//    g.setColour(COLOUR1.withAlpha(0.2f));
    g.setColour(COLOUR6);
    g.fillAll();
    
    g.setColour(juce::Colours::lightgrey.withAlpha(0.2f));
    g.drawLine(0, getHeight() / 5, getWidth(), getHeight() / 5, 1);
    for (int i = 0; i < numberOfLines; ++i)
    {
        const double proportion = frequenciesForLines[i] / 20000.0                  ;
        int xPos = transformToLog(proportion * 20000) * (getWidth());
        g.drawVerticalLine(xPos, getHeight() / 5, getHeight());
        if (frequenciesForLines[i] == 10 || frequenciesForLines[i] == 100 || frequenciesForLines[i] == 200)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i]), xPos - 30, 0, 60, getHeight() / 5, juce::Justification::centred, 2);
        else if ( frequenciesForLines[i] == 1000 || frequenciesForLines[i] == 10000 || frequenciesForLines[i] == 2000)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i] / 1000) + "k", xPos - 30, 0, 60, getHeight() / 5, juce::Justification::centred, 2);
        else if (frequenciesForLines[i] == 20)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i]), xPos - 30, 0, 60, getHeight() / 5, juce::Justification::right, 2);
        else if (frequenciesForLines[i] == 20000)
            g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i] / 1000) + "k", xPos - 30, 0, 60, getHeight() / 5, juce::Justification::left, 2);
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
    g.setColour(juce::Colours::white);
	paintSpectrum();
    spectrumImage.multiplyAllAlphas(0.9);
    spectrumImage.moveImageSection(0, 10, 0, 0, spectrumImage.getWidth(), spectrumImage.getHeight());
    g.drawImageAt(spectrumImage, 0, 0);
    
    float boxWidth = 100.0f;
    bool mouseOver;
    if (getMouseXYRelative().getX() > 0 && getMouseXYRelative().getX() < getWidth()
        && getMouseXYRelative().getY() > 0 && getMouseXYRelative().getY() < getHeight())
    {
        mouseOver = true;
    }
    else
    {
        mouseOver = false;
    }
    
    if (maxDecibel >= -99.9f && mouseOver)
    {
        g.setColour(COLOUR1);
        g.drawText(static_cast<juce::String>(maxDecibel) + " db", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY() - boxWidth / 2.0f, boxWidth, boxWidth, juce::Justification::centred);
        g.drawText(static_cast<juce::String>(maxFreq) + " Hz", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY(), boxWidth, boxWidth, juce::Justification::centred);
    }
    
}

void SpectrumComponent::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    spectrumImage = spectrumImage.rescaled(getWidth(), getHeight());
}

void SpectrumComponent::paintSpectrum()
{
    // this method is to paint spectrogram
    juce::Graphics g(spectrumImage);
    
    auto width = getLocalBounds().getWidth();
    auto height = getLocalBounds().getHeight();
    
    juce::Path p;
    p.startNewSubPath(0, height);
    
    auto mindB = -100.0f;
    auto maxdB =    0.0f;
    
    for (int i = 1; i < numberOfBins; i++)
    {
        // sample range [0, 1] to decibel range[-∞, 0] to [0, 1]
        // 4096 is 1 << 11, which is fftSize.
        auto fftSize = 1 << 11;
        float currentDecibel = juce::Decibels::gainToDecibels (spectrumData[i])
        - juce::Decibels::gainToDecibels(static_cast<float>(fftSize));
        float yPercent = juce::jmap (juce::jlimit (mindB, maxdB, currentDecibel),
                       mindB, maxdB, 0.0f, 1.0f);
        
        // skip some points to save cpu
//        if (i > numberOfBins / 8 && i % 2 != 0) continue;
//        if (i > numberOfBins / 4 && i % 3 != 0) continue;
//        if (i > numberOfBins / 2 && i % 4 != 0) continue;
//        if (i > numberOfBins / 4 * 3 && i % 10 != 0) continue;
        
        // connect points
        float currentX = transformToLog((float)i / numberOfBins * 22050) * width;
        float currentY = juce::jmap (yPercent, 0.0f, 1.0f, (float) height, 0.0f);
        p.lineTo(currentX, currentY);

        if (i == 1)
        {
            maxDecibel = -100.0f;
            maxFreq = 0.0f;
        }
        if (currentDecibel > maxDecibel)
        {
            maxDecibel = currentDecibel;
            maxFreq = (float)i / numberOfBins * 22050;
            maxDecibelPoint.setXY(currentX, currentY);
        }

        // reference: https://docs.juce.com/master/tutorial_spectrum_analyser.html
    }

    // this step is to round the path
    juce::Path roundedPath = p.createPathWithRoundedCorners(10.0f);
    
    // draw the outline of the path
    roundedPath.lineTo(width, height);
    roundedPath.lineTo(0, height);
    roundedPath.closeSubPath();
    
    g.setColour (COLOUR1);
            
    juce::ColourGradient grad(juce::Colours::red.withAlpha(0.8f), 0, 0,
                              COLOUR1.withAlpha(0.8f), 0, getLocalBounds().getHeight(), false);

    g.setGradientFill(grad);
    g.fillPath(roundedPath);
//    g.strokePath(roundedPath, juce::PathStrokeType(2));
}

void SpectrumComponent::prepareToPaintSpectrum(int numBins, float * data)
{
	numberOfBins = numBins;
    memmove(spectrumData, data, sizeof(spectrumData));
}

float SpectrumComponent::transformToLog(double valueToTransform) // freq to x
{
    // input: 20-20000
    // output: x
    auto value = juce::mapFromLog10(valueToTransform, 20.0, 20000.0);
    return static_cast<float>(value);
}

float SpectrumComponent::transformFromLog(double between0and1) // x to freq
{
    // input: 0.1-0.9 x pos
    // output: freq
    
    auto value = juce::mapToLog10(between0and1, 20.0, 20000.0);
    return static_cast<float>(value);
}

