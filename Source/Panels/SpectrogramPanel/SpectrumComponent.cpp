/*
  ==============================================================================

    SpectrumComponent.cpp
    Created: 11 Nov 2018 9:40:21am
    Author:  lenovo

  ==============================================================================
*/

#include "SpectrumComponent.h"
#include "../../Utility/AudioHelpers.h"

//==============================================================================
SpectrumComponent::SpectrumComponent() : mStyle(1), mDrawPeak(true), numberOfBins(1024), mBinWidth(44100 / (float) 2048)
{
}

SpectrumComponent::SpectrumComponent(int style, bool drawPeak) : numberOfBins(1024), mBinWidth(44100 / (float) 2048)
{
    mStyle = style;
    mDrawPeak = drawPeak;
}

SpectrumComponent::~SpectrumComponent()
{
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    // paint current spectrum
    g.setColour(juce::Colours::white);
    paintSpectrum();
    currentSpectrumImage.multiplyAllAlphas(0.9);
    currentSpectrumImage.moveImageSection(0, 10, 0, 0, currentSpectrumImage.getWidth(), currentSpectrumImage.getHeight());
    g.drawImageAt(currentSpectrumImage, 0, 0);

    if (mDrawPeak)
    {
        // paint peak spectrum
        maxSpectrumImage.multiplyAllAlphas(0.5);
        g.drawImageAt(maxSpectrumImage, 0, 0);

        // paint peak text
        float mouseX = getMouseXYRelative().getX();
        float mouseY = getMouseXYRelative().getY();

        if (mouseX > 0 && mouseX < getWidth()
            && mouseY > 0 && mouseY < getHeight())
        {
            mouseOver = true;
        }
        else
        {
            mouseOver = false;
        }

        if (maxDecibelValue >= -99.9f && mouseOver)
        {
            float boxWidth = 100.0f;
            g.setColour(juce::Colours::lightgrey);
            g.drawText(juce::String(maxDecibelValue, 1) + " db", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY() - boxWidth / 4.0f, boxWidth, boxWidth, juce::Justification::centred);
            g.drawText(juce::String(static_cast<int>(maxFreq)) + " Hz", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY(), boxWidth, boxWidth, juce::Justification::centred);
        }
        else
        {
            maxDecibelValue = -100.0f;
            maxFreq = 0.0f;
            maxDecibelPoint.setXY(-10.0f, -10.0f);
            for (int i = 0; i < 1024; i++)
            {
                maxData[i] = 0;
            }
        }
    }
}

void SpectrumComponent::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    currentSpectrumImage = currentSpectrumImage.rescaled(getWidth(), getHeight());
    maxSpectrumImage = maxSpectrumImage.rescaled(getWidth(), getHeight());
}

void SpectrumComponent::paintSpectrum()
{
    // this method is to paint spectrogram

    // init graphics
    juce::Graphics gCurrent(currentSpectrumImage);
    juce::Graphics gMax(maxSpectrumImage);

    auto width = getLocalBounds().getWidth();
    auto height = getLocalBounds().getHeight();
    auto mindB = -100.0f;
    auto maxdB = 0.0f;

    juce::Path currentSpecPath;
    currentSpecPath.startNewSubPath(0, height);

    juce::Path maxSpecPath;
    maxSpecPath.startNewSubPath(0, height + 1);
    int resolution = 2;
    for (int i = 1; i < numberOfBins; i += resolution)
    {
        // sample range [0, 1] to decibel range[-∞, 0] to [0, 1]
        auto fftSize = 1 << 11;
        float currentDecibel = juce::Decibels::gainToDecibels(spectrumData[i] / static_cast<float>(numberOfBins));
        float maxDecibel = juce::Decibels::gainToDecibels(maxData[i])
                           - juce::Decibels::gainToDecibels(static_cast<float>(fftSize));
        float yPercent = juce::jmap(juce::jlimit(mindB, maxdB, currentDecibel),
                                    mindB,
                                    maxdB,
                                    0.0f,
                                    1.0f);
        float yMaxPercent = juce::jmap(juce::jlimit(mindB, maxdB, maxDecibel),
                                       mindB,
                                       maxdB,
                                       0.0f,
                                       1.0f);
        // skip some points to save cpu
        //        if (i > numberOfBins / 8 && i % 2 != 0) continue;
        //        if (i > numberOfBins / 4 && i % 3 != 0) continue;
        //        if (i > numberOfBins / 2 && i % 4 != 0) continue;
        //        if (i > numberOfBins / 4 * 3 && i % 10 != 0) continue;

        // connect points
        double currentFreq = i * mBinWidth;
        float currentX = transformToLog(currentFreq) * width;
        float currentY = juce::jmap(yPercent, 0.0f, 1.0f, (float) height, 0.0f);
        float maxY = juce::jmap(yMaxPercent, 0.0f, 1.0f, (float) height, 0.0f);
        currentSpecPath.lineTo(currentX, currentY);

        maxSpecPath.lineTo(currentX, maxY);

        if (currentDecibel > maxDecibelValue)
        {
            maxDecibelValue = currentDecibel;
            maxFreq = currentFreq;
            maxDecibelPoint.setXY(currentX, currentY);
        }
        if (spectrumData[i] > maxData[i])
        {
            maxData[i] = spectrumData[i];
        }

        // reference: https://docs.juce.com/master/tutorial_spectrum_analyser.html
    }

    // this step is to round the path
    juce::Path roundedCurrentPath = currentSpecPath.createPathWithRoundedCorners(10.0f);

    // draw the outline of the path
    roundedCurrentPath.lineTo(width, height);
    roundedCurrentPath.lineTo(0, height);
    roundedCurrentPath.closeSubPath();

    juce::Path roundedMaxPath = maxSpecPath.createPathWithRoundedCorners(10.0f);
    roundedMaxPath.lineTo(width, height + 1);
    //    roundedMaxPath.lineTo(0, height);
    //    roundedMaxPath.closeSubPath();

    gCurrent.setColour(COLOUR1);

    juce::ColourGradient grad;

    if (mStyle == 1)
    {
        grad = juce::ColourGradient(juce::Colours::red.withAlpha(specAlpha), 0, 0, COLOUR1.withAlpha(specAlpha), 0, getLocalBounds().getHeight(), false);
    }
    else
    {
        grad = juce::ColourGradient(juce::Colours::white.withAlpha(0.2f), 0, 0, juce::Colours::grey.withAlpha(0.2f), 0, getLocalBounds().getHeight(), false);
    }

    gCurrent.setGradientFill(grad);

    gCurrent.setGradientFill(grad);
    gCurrent.fillPath(roundedCurrentPath);
    //    g.strokePath(roundedPath, juce::PathStrokeType(2));

    if (mouseOver)
    {
        gMax.setColour(juce::Colours::white);
        gMax.strokePath(roundedMaxPath, juce::PathStrokeType(2));
        gMax.drawEllipse(maxDecibelPoint.getX() - 2.0f, maxDecibelPoint.getY() + 10.0f, 4.0f, 4.0f, 1.0f);
    }
}

void SpectrumComponent::prepareToPaintSpectrum(int numBins, float* data, float binWidth)
{
    numberOfBins = numBins;
    memmove(spectrumData, data, sizeof(spectrumData));
    mBinWidth = binWidth;
}

void SpectrumComponent::setSpecAlpha(const float alp)
{
    specAlpha = alp;
}
