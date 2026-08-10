/*
  ==============================================================================

    SpectrumComponent.cpp
    Event-driven, CPU-rendered spectrum view for the Fire analyser.

  ==============================================================================
*/

#include "SpectrumComponent.h"
#include "../../Utility/AudioHelpers.h"

namespace
{
constexpr float minDisplayDb = -100.0f;
constexpr float maxDisplayDb = 0.0f;
constexpr float minimumDisplayFrequency = 20.0f;
constexpr float maximumDisplayFrequency = 20000.0f;

float magnitudeToDb(float magnitude, int numberOfBins)
{
    const auto normalisedMagnitude = juce::jmax(1.0e-9f,
                                                magnitude / static_cast<float>(juce::jmax(1, numberOfBins)));
    return juce::Decibels::gainToDecibels(normalisedMagnitude, minDisplayDb);
}
} // namespace

SpectrumComponent::SpectrumComponent()
    : SpectrumComponent(1, true)
{
}

SpectrumComponent::SpectrumComponent(int style, bool drawPeak)
    : mStyle(style), mDrawPeak(drawPeak)
{
    setOpaque(false);
    setInterceptsMouseClicks(false, false);
}

SpectrumComponent::~SpectrumComponent()
{
    stopTimer();
    cancelPendingUpdate();

    if (observedMouseSource != nullptr)
        observedMouseSource->removeMouseListener(this);
}

void SpectrumComponent::updateSpectrum(const float* newData, int numBins, float binWidth)
{
    if (newData == nullptr || numBins <= 0 || ! std::isfinite(binWidth) || binWidth <= 0.0f)
        return;

    const auto binsToCopy = juce::jlimit(0, static_cast<int>(pendingData.size()), numBins);
    {
        const juce::ScopedLock locker(dataLock);
        std::copy_n(newData, binsToCopy, pendingData.begin());
        if (binsToCopy < static_cast<int>(pendingData.size()))
            std::fill(pendingData.begin() + binsToCopy, pendingData.end(), 0.0f);

        pendingNumberOfBins = binsToCopy;
        pendingBinWidth = binWidth;
        pendingGeneration.fetch_add(1, std::memory_order_release);
    }

    triggerAsyncUpdate();
}

void SpectrumComponent::handleAsyncUpdate()
{
    if (! isShowing())
        return;

    if (! isTimerRunning())
        startTimerHz(60);
}

void SpectrumComponent::timerCallback()
{
    if (! isShowing())
    {
        stopTimer();
        return;
    }

    bool visualStateChanged = false;
    const auto newestGeneration = pendingGeneration.load(std::memory_order_acquire);
    if (newestGeneration != consumedGeneration)
    {
        const juce::ScopedLock locker(dataLock);
        targetData = pendingData;
        numberOfBins = pendingNumberOfBins;
        mBinWidth = pendingBinWidth;
        consumedGeneration = pendingGeneration.load(std::memory_order_relaxed);
        interpolationActive = true;
        visualStateChanged = true;
    }

    bool stillInterpolating = false;
    if (interpolationActive)
    {
        for (int i = 0; i < numberOfBins; ++i)
        {
            const float difference = targetData[static_cast<size_t>(i)]
                                   - displayData[static_cast<size_t>(i)];

            if (std::abs(difference) > 1.0e-5f)
            {
                displayData[static_cast<size_t>(i)] += difference * interpolationFactor;
                stillInterpolating = true;
            }
            else
            {
                displayData[static_cast<size_t>(i)] = targetData[static_cast<size_t>(i)];
            }
        }

        interpolationActive = stillInterpolating;
        visualStateChanged = true;
    }

    if (mDrawPeak && mouseOver && visualStateChanged)
    {
        for (int i = 0; i < numberOfBins; ++i)
            maxData[static_cast<size_t>(i)] = juce::jmax(maxData[static_cast<size_t>(i)],
                                                        displayData[static_cast<size_t>(i)]);

        isPeakLineVisible = true;
    }
    else if (mDrawPeak && ! mouseOver && isPeakLineVisible)
    {
        float loudestPeakDb = minDisplayDb;
        for (int i = 0; i < numberOfBins; ++i)
        {
            auto& peak = maxData[static_cast<size_t>(i)];
            peak *= 0.88f;
            loudestPeakDb = juce::jmax(loudestPeakDb, magnitudeToDb(peak, numberOfBins));
        }

        if (loudestPeakDb <= minDisplayDb + 0.5f)
        {
            maxData.fill(0.0f);
            isPeakLineVisible = false;
        }

        visualStateChanged = true;
    }

    if (visualStateChanged || geometryDirty)
    {
        rebuildPaths();
        repaint();
    }

    updateAnimationTimer();
}

void SpectrumComponent::rebuildPaths()
{
    spectrumLinePath.clear();
    spectrumFillPath.clear();
    peakLinePath.clear();
    geometryDirty = false;

    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty() || numberOfBins < 2 || mBinWidth <= 0.0f)
        return;

    spectrumLinePath.preallocateSpace(juce::jmax(64, getWidth() * 4));
    peakLinePath.preallocateSpace(juce::jmax(64, getWidth() * 4));

    for (int i = 0; i < numberOfBins; ++i)
    {
        if (i == 0 || i == numberOfBins - 1)
            smoothedData[static_cast<size_t>(i)] = displayData[static_cast<size_t>(i)];
        else
            smoothedData[static_cast<size_t>(i)] = (displayData[static_cast<size_t>(i - 1)]
                                                    + displayData[static_cast<size_t>(i)] * 2.0f
                                                    + displayData[static_cast<size_t>(i + 1)])
                                                   * 0.25f;
    }

    int currentBucket = -1;
    float currentBucketY = bounds.getBottom();
    float peakBucketY = bounds.getBottom();
    bool hasSpectrumPoint = false;
    bool hasPeakPoint = false;

    maxDecibelValue = minDisplayDb;
    maxFreq = 0.0f;
    maxDecibelPoint = { -10.0f, -10.0f };

    const auto dbToY = [&bounds](float db)
    {
        const float proportion = juce::jmap(juce::jlimit(minDisplayDb, maxDisplayDb, db),
                                            minDisplayDb,
                                            maxDisplayDb,
                                            0.0f,
                                            1.0f);
        return juce::jmap(proportion, 0.0f, 1.0f, bounds.getBottom(), bounds.getY());
    };

    const auto flushBucket = [&](int bucket)
    {
        if (bucket < 0)
            return;

        const float x = juce::jlimit(bounds.getX(), bounds.getRight(),
                                     static_cast<float>(bucket) + 0.5f);
        if (! hasSpectrumPoint)
        {
            spectrumLinePath.startNewSubPath(x, currentBucketY);
            hasSpectrumPoint = true;
        }
        else
        {
            spectrumLinePath.lineTo(x, currentBucketY);
        }

        if (mDrawPeak && isPeakLineVisible)
        {
            if (! hasPeakPoint)
            {
                peakLinePath.startNewSubPath(x, peakBucketY);
                hasPeakPoint = true;
            }
            else
            {
                peakLinePath.lineTo(x, peakBucketY);
            }
        }
    };

    for (int i = 1; i < numberOfBins; ++i)
    {
        const float frequency = static_cast<float>(i) * mBinWidth;
        if (frequency < minimumDisplayFrequency || frequency > maximumDisplayFrequency)
            continue;

        const float normalisedX = transformToLog(frequency);
        if (! std::isfinite(normalisedX))
            continue;

        const float x = bounds.getX() + normalisedX * bounds.getWidth();
        const int bucket = juce::jlimit(0, juce::jmax(0, getWidth() - 1),
                                       static_cast<int>(std::floor(x)));
        const float currentDb = magnitudeToDb(smoothedData[static_cast<size_t>(i)], numberOfBins);
        const float currentY = dbToY(currentDb);
        const float peakDb = magnitudeToDb(maxData[static_cast<size_t>(i)], numberOfBins);
        const float peakY = dbToY(peakDb);

        if (bucket != currentBucket)
        {
            flushBucket(currentBucket);
            currentBucket = bucket;
            currentBucketY = currentY;
            peakBucketY = peakY;
        }
        else
        {
            // Preserve the strongest bin in each physical x-column. This keeps
            // narrow transients visible while limiting the path to screen width.
            currentBucketY = juce::jmin(currentBucketY, currentY);
            peakBucketY = juce::jmin(peakBucketY, peakY);
        }

        if (mDrawPeak && isPeakLineVisible && peakDb > maxDecibelValue)
        {
            maxDecibelValue = peakDb;
            maxFreq = frequency;
            maxDecibelPoint = { x, peakY };
        }
    }

    flushBucket(currentBucket);

    if (hasSpectrumPoint)
    {
        spectrumFillPath = spectrumLinePath;
        spectrumFillPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumFillPath.lineTo(bounds.getX(), bounds.getBottom());
        spectrumFillPath.closeSubPath();
    }
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty() || spectrumLinePath.isEmpty())
        return;

    if (mStyle == 1)
    {
        juce::ColourGradient fill(fire::ui::colours::flame.withAlpha(specAlpha * 0.16f),
                                  bounds.getX(), bounds.getY(),
                                  fire::ui::colours::ember.withAlpha(0.0f),
                                  bounds.getX(), bounds.getBottom(), false);
        fill.addColour(0.42, fire::ui::colours::ember.withAlpha(specAlpha * 0.09f));
        g.setGradientFill(fill);
        g.fillPath(spectrumFillPath);

        g.setColour(fire::ui::colours::ember.withAlpha(specAlpha * 0.13f));
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));

        juce::ColourGradient heat(fire::ui::colours::whiteHot.withAlpha(specAlpha * 0.90f),
                                  bounds.getX(), bounds.getY(),
                                  fire::ui::colours::ember.withAlpha(specAlpha * 0.88f),
                                  bounds.getX(), bounds.getBottom(), false);
        heat.addColour(0.55, fire::ui::colours::flame.withAlpha(specAlpha * 0.94f));
        g.setGradientFill(heat);
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(1.45f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour(fire::ui::colours::signalCool.withAlpha(specAlpha * 0.055f));
        g.fillPath(spectrumFillPath);
        g.setColour(fire::ui::colours::signalCool.withAlpha(specAlpha * 0.52f));
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }

    if (mDrawPeak && isPeakLineVisible && ! peakLinePath.isEmpty())
    {
        g.setColour(fire::ui::colours::whiteHot.withAlpha(mouseOver ? 0.56f : 0.30f));
        g.strokePath(peakLinePath,
                     juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }

    if (mDrawPeak && mouseOver && maxDecibelValue > minDisplayDb + 0.1f)
    {
        constexpr float popupWidth = 112.0f;
        constexpr float popupHeight = 38.0f;
        auto popup = juce::Rectangle<float>(popupWidth, popupHeight)
                         .withCentre({ maxDecibelPoint.x,
                                       maxDecibelPoint.y - popupHeight * 0.72f });
        popup.setPosition(juce::jlimit(bounds.getX() + 4.0f,
                                       bounds.getRight() - popupWidth - 4.0f,
                                       popup.getX()),
                          juce::jlimit(bounds.getY() + 4.0f,
                                       bounds.getBottom() - popupHeight - 4.0f,
                                       popup.getY()));

        fire::ui::drawGlassPill(g, popup, fire::ui::colours::ember, true, false, false);
        g.setFont(fire::ui::displayFont(11.0f));
        g.setColour(fire::ui::colours::whiteHot);
        g.drawText(juce::String(maxDecibelValue, 1) + " dB",
                   popup.removeFromTop(popupHeight * 0.52f).reduced(8.0f, 0.0f),
                   juce::Justification::centredLeft);
        g.setFont(fire::ui::bodyFont(10.0f));
        g.setColour(fire::ui::colours::textSecondary);
        const auto frequencyText = maxFreq >= 1000.0f
                                     ? juce::String(maxFreq / 1000.0f, 2) + " kHz"
                                     : juce::String(juce::roundToInt(maxFreq)) + " Hz";
        g.drawText(frequencyText, popup.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
    }
}

void SpectrumComponent::resized()
{
    geometryDirty = true;
    rebuildPaths();
    repaint();
}

void SpectrumComponent::setSpecAlpha(float alpha)
{
    const float newAlpha = juce::jlimit(0.0f, 1.0f, alpha);
    if (! juce::approximatelyEqual(specAlpha, newAlpha))
    {
        specAlpha = newAlpha;
        repaint();
    }
}

void SpectrumComponent::mouseEnter(const juce::MouseEvent& event)
{
    const auto relativeEvent = event.getEventRelativeTo(this);
    setMouseOverSpectrum(getLocalBounds().contains(relativeEvent.getPosition()));
}

void SpectrumComponent::mouseMove(const juce::MouseEvent& event)
{
    const auto relativeEvent = event.getEventRelativeTo(this);
    setMouseOverSpectrum(getLocalBounds().contains(relativeEvent.getPosition()));
}

void SpectrumComponent::mouseExit(const juce::MouseEvent& event)
{
    const auto relativeEvent = event.getEventRelativeTo(this);
    setMouseOverSpectrum(getLocalBounds().contains(relativeEvent.getPosition()));
}

void SpectrumComponent::setMouseOverSpectrum(bool shouldBeOver)
{
    if (! mDrawPeak || mouseOver == shouldBeOver)
        return;

    mouseOver = shouldBeOver;
    if (mouseOver)
    {
        resetPeakData();
        std::copy(displayData.begin(), displayData.end(), maxData.begin());
        isPeakLineVisible = true;
        rebuildPaths();
        repaint();
    }
    else if (isPeakLineVisible && ! isTimerRunning())
    {
        startTimerHz(60);
    }
}

void SpectrumComponent::resetPeakData()
{
    maxData.fill(0.0f);
    maxFreq = 0.0f;
    maxDecibelValue = minDisplayDb;
    maxDecibelPoint = { -10.0f, -10.0f };
}

void SpectrumComponent::parentHierarchyChanged()
{
    if (observedMouseSource != nullptr)
        observedMouseSource->removeMouseListener(this);

    observedMouseSource = mDrawPeak ? getParentComponent() : nullptr;
    if (observedMouseSource != nullptr)
        observedMouseSource->addMouseListener(this, true);
}

void SpectrumComponent::visibilityChanged()
{
    if (! isShowing())
    {
        setMouseOverSpectrum(false);
        stopTimer();
        return;
    }

    if (pendingGeneration.load(std::memory_order_acquire) != consumedGeneration)
        startTimerHz(60);
}

void SpectrumComponent::updateAnimationTimer()
{
    const bool hasPendingFrame = pendingGeneration.load(std::memory_order_acquire) != consumedGeneration;
    const bool peakIsDecaying = mDrawPeak && isPeakLineVisible && ! mouseOver;
    if (isShowing() && (hasPendingFrame || interpolationActive || peakIsDecaying))
    {
        if (! isTimerRunning())
            startTimerHz(60);
    }
    else
    {
        stopTimer();
    }
}
