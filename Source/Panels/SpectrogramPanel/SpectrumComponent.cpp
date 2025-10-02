/*
  ==============================================================================

    SpectrumComponent.cpp
    Created: 11 Nov 2018 9:40:21am
    Author:  lenovo

    MODIFIED to remove high-CPU waterfall effect and improve performance.
    Draws the spectrum directly without intermediate images.

  ==============================================================================
*/

#include "SpectrumComponent.h"
#include "../../Utility/AudioHelpers.h" // Assumed to contain AudioHelpers::transformToLog

//==============================================================================
SpectrumComponent::SpectrumComponent()
    : mStyle(1), mDrawPeak(true), mBinWidth(44100.0f / 2048.0f), numberOfBins(1024), attackCoeff(0.1f), releaseCoeff(0.5f) // Initialize smoothing coefficients
{
    spectrumData.fill(0.0f);
    maxData.fill(0.0f);
}

SpectrumComponent::SpectrumComponent(int style, bool drawPeak)
    : mStyle(style), mDrawPeak(drawPeak), mBinWidth(44100.0f / 2048.0f), numberOfBins(1024), attackCoeff(0.1f), releaseCoeff(0.5f) // Initialize smoothing coefficients
{
    spectrumData.fill(0.0f);
    maxData.fill(0.0f);
}

SpectrumComponent::~SpectrumComponent()
{
}

void SpectrumComponent::handleAsyncUpdate()
{
    // This is called on the message thread after triggerAsyncUpdate() is called from the audio thread.
    repaint();
}

void SpectrumComponent::updateSpectrum(const float* newData, int numBins, float binWidth)
{
    // This method is designed to be called from the audio thread.
    // It copies the new data with smoothing and then triggers an asynchronous repaint on the message thread.
    {
        juce::ScopedLock locker(dataLock);
        numberOfBins = std::min(numBins, (int) spectrumData.size());
        mBinWidth = binWidth;

        // Apply smoothing logic
        for (int i = 0; i < numberOfBins; ++i)
        {
            if (newData[i] > spectrumData[i])
            {
                // Attack: Respond quickly
                spectrumData[i] = (spectrumData[i] * attackCoeff) + (newData[i] * (1.0f - attackCoeff));
            }
            else
            {
                // Release: Fall slowly
                spectrumData[i] = (spectrumData[i] * releaseCoeff) + (newData[i] * (1.0f - releaseCoeff));
            }
        }
    }

    triggerAsyncUpdate();
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    // The waterfall effect is removed. We now draw the spectrum path directly.
    g.fillAll(juce::Colours::transparentBlack);

    auto width = getLocalBounds().getWidth();
    auto height = getLocalBounds().getHeight();
    auto mindB = -100.0f;
    auto maxdB = 0.0f;
    const float fftSize = 2048.0f;

    // Restore original mouse detection and peak reset logic from within paint()
    // to ensure behavior is identical to the original version.
    float mouseX = getMouseXYRelative().getX();
    float mouseY = getMouseXYRelative().getY();
    mouseOver = (mouseX >= 0 && mouseX < getWidth() && mouseY >= 0 && mouseY < getHeight());

    // Reset frame-specific peak info before recalculating.
    maxDecibelValue = -100.0f;

    if (mDrawPeak)
    {
        // Original behavior: Reset maxData every frame if mouse is not over the component.
        // This is inefficient but visually identical to the original.
        if (! mouseOver)
        {
            juce::ScopedLock locker(dataLock);
            maxData.fill(0.0f);
            maxFreq = 0.0f;
            maxDecibelPoint.setXY(-10.0f, -10.0f);
        }
    }

    juce::Path currentSpecPath, maxSpecPath;
    currentSpecPath.startNewSubPath(0, (float) height);
    if (mDrawPeak)
        maxSpecPath.startNewSubPath(0, (float) height);

    { // Scoped lock to read spectrum data safely
        juce::ScopedLock locker(dataLock);

        for (int i = 1; i < numberOfBins; ++i)
        {
            // Update peak-hold data for this bin. Only effective when mouse is over.
            if (mDrawPeak && spectrumData[i] > maxData[i])
            {
                maxData[i] = spectrumData[i];
            }

            // --- Restore original calculations to fix position and shape issues ---
            float currentDecibel = juce::Decibels::gainToDecibels(spectrumData[i] / (float) numberOfBins);
            // CORRECTED: Use the same normalization for maxDecibel as for currentDecibel.
            float maxDecibel = juce::Decibels::gainToDecibels(maxData[i] / (float) numberOfBins);

            float yPercent = juce::jmap(juce::jlimit(mindB, maxdB, currentDecibel), mindB, maxdB, 0.0f, 1.0f);
            float yMaxPercent = juce::jmap(juce::jlimit(mindB, maxdB, maxDecibel), mindB, maxdB, 0.0f, 1.0f);

            double currentFreq = i * mBinWidth.load();
            float currentX = transformToLog(currentFreq) * width;
            float currentY = juce::jmap(yPercent, 0.0f, 1.0f, (float) height, 0.0f);
            float maxY = juce::jmap(yMaxPercent, 0.0f, 1.0f, (float) height, 0.0f);

            if (! std::isnan(currentX) && ! std::isinf(currentX))
            {
                currentSpecPath.lineTo(currentX, currentY);
                if (mDrawPeak)
                    maxSpecPath.lineTo(currentX, maxY);
            }

            // Find the peak value across all bins for text display.
            // This now correctly finds the peak of the maxSpecPath (peak-hold line).
            if (maxDecibel > maxDecibelValue)
            {
                maxDecibelValue = maxDecibel;
                maxFreq = currentFreq;
                maxDecibelPoint.setXY(currentX, maxY); // Use maxY to bind to the peak-hold line
            }
        }
    } // End of scoped lock

    // --- Draw the paths ---

    // 1. Fill the current spectrum path
    auto roundedCurrentPath = currentSpecPath.createPathWithRoundedCorners(5.0f);
    roundedCurrentPath.lineTo((float) width, (float) height);
    roundedCurrentPath.lineTo(0.0f, (float) height);
    roundedCurrentPath.closeSubPath();

    juce::ColourGradient grad;
    if (mStyle == 1)
        grad = juce::ColourGradient(juce::Colours::red.withAlpha(specAlpha), 0, 0, COLOUR1.withAlpha(specAlpha), 0, (float) height, false);
    else
        grad = juce::ColourGradient(juce::Colours::white.withAlpha(0.2f), 0, 0, juce::Colours::grey.withAlpha(0.2f), 0, (float) height, false);

    g.setGradientFill(grad);
    g.fillPath(roundedCurrentPath);

    // 2. Draw peak-hold line and text, restoring original behavior
    if (mDrawPeak)
    {
        auto roundedMaxPath = maxSpecPath.createPathWithRoundedCorners(5.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.strokePath(roundedMaxPath, juce::PathStrokeType(1.5f));

        if (mouseOver && maxDecibelValue > -99.9f)
        {
            float boxWidth = 100.0f;
            g.setColour(juce::Colours::lightgrey);
            g.drawText(juce::String(maxDecibelValue, 1) + " db", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY() - boxWidth / 4.0f, boxWidth, boxWidth, juce::Justification::centred);
            g.drawText(juce::String(static_cast<int>(maxFreq)) + " Hz", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY(), boxWidth, boxWidth, juce::Justification::centred);
        }
    }
}

void SpectrumComponent::resized()
{
    // This method is not needed for image resizing anymore.
    // Can be used for child component bounds if any are added.
}

void SpectrumComponent::setSpecAlpha(const float alp)
{
    specAlpha = alp;
}

void SpectrumComponent::mouseEnter(const juce::MouseEvent& /*event*/)
{
    // mouseOver is now handled inside paint() to be identical to original code.
}

void SpectrumComponent::mouseExit(const juce::MouseEvent& /*event*/)
{
    // The reset logic is now in paint(), so this is cleared.
}

void SpectrumComponent::resetPeakData()
{
    // This is now handled by the logic inside paint().
    juce::ScopedLock locker(dataLock);
    maxData.fill(0.0f);
}
