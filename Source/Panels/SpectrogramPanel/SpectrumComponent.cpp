/*
  ==============================================================================

    SpectrumComponent.cpp
    Created: 2 Oct 2025
    Author:  Yifeng Yu

    MODIFIED to remove high-CPU waterfall effect and improve performance.
    Draws the spectrum directly without intermediate images.
    Further modified to use a Timer for temporal smoothing and applies
    spatial smoothing for a cleaner curve.

  ==============================================================================
*/

#include "SpectrumComponent.h"
#include "../../Utility/AudioHelpers.h" // Assumed to contain AudioHelpers::transformToLog

//==============================================================================
SpectrumComponent::SpectrumComponent()
    : mStyle(1), mDrawPeak(true), mBinWidth(44100.0f / 2048.0f), numberOfBins(1024)
{
    spectrumData.fill(0.0f);
    displayData.fill(0.0f); // Initialize display data
    maxData.fill(0.0f);

    startTimerHz(60); // Start a 60 FPS timer for smooth animation
}

SpectrumComponent::SpectrumComponent(int style, bool drawPeak)
    : mStyle(style), mDrawPeak(drawPeak), mBinWidth(44100.0f / 2048.0f), numberOfBins(1024)
{
    spectrumData.fill(0.0f);
    displayData.fill(0.0f);
    maxData.fill(0.0f);

    startTimerHz(60);
}

SpectrumComponent::~SpectrumComponent()
{
    stopTimer(); // Stop the timer when the component is destroyed
}

void SpectrumComponent::timerCallback()
{
    {
        juce::ScopedLock locker(dataLock);
        // 1. Interpolate the main spectrum line (no change here)
        for (int i = 0; i < numberOfBins; ++i)
        {
            displayData[i] += (spectrumData[i] - displayData[i]) * interpolationFactor;
        }

        // 2. NEW LOGIC for peak line decay
        // If mouse is NOT over, the peak line should fall towards zero.
        if (! mouseOver && isPeakLineVisible)
        {
            float maxPeakValue = 0.0f;
            for (int i = 0; i < numberOfBins; ++i)
            {
                // Decay towards a small negative value to ensure it goes below the bottom of the graph
                maxData[i] += (-0.1f - maxData[i]) * interpolationFactor;

                // Keep track of the highest peak value during decay
                if (maxData[i] > maxPeakValue)
                    maxPeakValue = maxData[i];
            }

            if (maxPeakValue < 0.001f)
            {
                isPeakLineVisible = false;
            }
        }
    }

    repaint();
}

void SpectrumComponent::handleAsyncUpdate()
{
    // This is no longer needed for rendering, as the timer handles it.
    // It can be left empty or used for other asynchronous tasks if necessary.
}

void SpectrumComponent::updateSpectrum(const float* newData, int numBins, float binWidth)
{
    // This method is called from the audio thread.
    // It just copies the new data; all smoothing is now on the message thread.
    {
        juce::ScopedLock locker(dataLock);
        numberOfBins = std::min(numBins, (int) spectrumData.size());
        mBinWidth = binWidth;

        // Directly copy the new data into the spectrumData buffer
        std::copy(newData, newData + numberOfBins, spectrumData.begin());
    }
    // No need to call triggerAsyncUpdate() anymore.
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);

    auto width = getLocalBounds().getWidth();
    auto height = getLocalBounds().getHeight();
    auto mindB = -100.0f;
    auto maxdB = 0.0f;

    // mouseOver state is updated in paint(), which is more robust
    mouseOver = getLocalBounds().contains(getMouseXYRelative());
    maxDecibelValue = -100.0f;

    if (mouseOver && ! wasMouseOver)
    {
        isPeakLineVisible = true;
        resetPeakData();
    }
    wasMouseOver = mouseOver;

    juce::Path currentSpecPath, maxSpecPath;
    currentSpecPath.startNewSubPath(0, (float) height);
    if (mDrawPeak)
        maxSpecPath.startNewSubPath(0, (float) height);

    std::array<float, 1024> smoothedDisplayData;

    {
        juce::ScopedLock locker(dataLock);

        for (int i = 0; i < numberOfBins; ++i)
        {
            if (i == 0 || i == numberOfBins - 1)
                smoothedDisplayData[i] = displayData[i];
            else
                smoothedDisplayData[i] = (displayData[i - 1] + displayData[i] + displayData[i + 1]) / 3.0f;
        }

        for (int i = 1; i < numberOfBins; ++i)
        {
            // --- CRITICAL CHANGE HERE ---
            // Only capture new peaks when the mouse is hovering over the component.
            if (mDrawPeak && mouseOver && smoothedDisplayData[i] > maxData[i])
            {
                maxData[i] = smoothedDisplayData[i];
            }

            float currentDecibel = juce::Decibels::gainToDecibels(smoothedDisplayData[i] / (float) numberOfBins);
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
            if (maxDecibel > maxDecibelValue)
            {
                maxDecibelValue = maxDecibel;
                maxFreq = currentFreq;
                maxDecibelPoint.setXY(currentX, maxY);
            }
        }
    }

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

    // Drawing of maxSpecPath and its text
    if (mDrawPeak && isPeakLineVisible)
    {
        // We always draw the path, because we want to see the decay animation
        // when the mouse leaves. It will naturally become flat at the bottom when decayed.
        auto roundedMaxPath = maxSpecPath.createPathWithRoundedCorners(5.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.strokePath(roundedMaxPath, juce::PathStrokeType(1.5f));

        // The text, however, should only be visible when hovering.
        if (mouseOver && maxDecibelValue > -99.9f)
        {
            float boxWidth = 100.0f;
            g.setColour(juce::Colours::lightgrey);
            g.drawText(juce::String(maxDecibelValue, 1) + " db", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY() - 20.0f, boxWidth, 20, juce::Justification::centred);
            g.drawText(juce::String(static_cast<int>(maxFreq)) + " Hz", maxDecibelPoint.getX() - boxWidth / 2.0f, maxDecibelPoint.getY() - 5.0f, boxWidth, 20, juce::Justification::centred);
        }
    }
}

void SpectrumComponent::resized()
{
    // This method is called when the component's size is changed.
}

void SpectrumComponent::setSpecAlpha(const float alp)
{
    specAlpha = alp;
}

void SpectrumComponent::mouseEnter(const juce::MouseEvent& /*event*/)
{
    // When the mouse enters, we want to start a fresh peak-hold session.
    // Calling resetPeakData() clears the old values.
    if (mDrawPeak)
    {
        resetPeakData();
    }
}

void SpectrumComponent::mouseExit(const juce::MouseEvent& /*event*/)
{
    // Mouse over state is now handled inside paint().
}

void SpectrumComponent::resetPeakData()
{
    juce::ScopedLock locker(dataLock);
    maxData.fill(0.0f);
    maxFreq = 0.0f;
    maxDecibelPoint.setXY(-10.0f, -10.0f);
}
