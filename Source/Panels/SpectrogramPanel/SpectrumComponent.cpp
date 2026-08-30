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
    presentationOpacity.snapTo(1.0f);
    hoverOpacity.snapTo(0.0f);
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

    // The editor normally stops publishing while host-bypassed, but retain
    // the guard here because this method is intentionally thread-safe. An
    // update already copying when bypass begins is invalidated when the
    // message thread advances consumedGeneration at that boundary.
    if (! acceptingSpectrumUpdates.load(std::memory_order_acquire))
        return;

    const auto binsToCopy = juce::jlimit(0, static_cast<int>(pendingData.size()), numBins);
    {
        const juce::ScopedLock locker(dataLock);
        if (! acceptingSpectrumUpdates.load(std::memory_order_acquire))
            return;

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
        resetHoverPresentation();
        stopTimer();
        return;
    }

    bool visualStateChanged = presentationOpacity.advance(1.0f / 60.0f, 0.11f);
    visualStateChanged = hoverOpacity.advance(1.0f / 60.0f, 0.10f)
                      || visualStateChanged;
    const auto newestGeneration = pendingGeneration.load(std::memory_order_acquire);

    if (hostBypassed)
    {
        if (presentationOpacity.isSettled()
            && presentationOpacity.current <= 0.001f
            && ! renderedDataIsClear)
        {
            resetRenderedData();
            visualStateChanged = true;
        }
    }
    else if (newestGeneration != consumedGeneration)
    {
        // A fast bypass toggle may deliver a fresh FFT frame before the old
        // trace has completed its fade. Keep only the newest pending frame and
        // consume it at the zero-opacity boundary; the old path can therefore
        // never reverse direction or blend into the resumed frame.
        const bool mayConsume = ! awaitingFreshFrame
                             || (presentationOpacity.isSettled()
                                 && presentationOpacity.current <= 0.001f);
        if (mayConsume && consumePendingFrame(awaitingFreshFrame))
        {
            if (awaitingFreshFrame)
            {
                awaitingFreshFrame = false;
                presentationOpacity.setTarget(1.0f);
            }
            visualStateChanged = true;
        }
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

    if (mDrawPeak && mouseOver && visualStateChanged
        && ! hostBypassed && ! renderedDataIsClear)
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
    if (renderedDataIsClear || bounds.isEmpty()
        || numberOfBins < 2 || mBinWidth <= 0.0f)
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
    const auto opacity = juce::jlimit(0.0f, 1.0f, presentationOpacity.current);
    const auto hover = juce::jlimit(0.0f, 1.0f, hoverOpacity.current);
    if (bounds.isEmpty() || spectrumLinePath.isEmpty() || opacity <= 0.001f)
        return;

    const juce::Graphics::ScopedSaveState state(g);
    const auto presentedSpectrumAlpha = [this, opacity](float alpha)
    {
        return specAlpha * alpha * opacity;
    };

    if (mStyle == 1)
    {
        juce::ColourGradient fill(fire::ui::colours::flame.withAlpha(
                                      presentedSpectrumAlpha(0.16f)),
                                  bounds.getX(), bounds.getY(),
                                  fire::ui::colours::ember.withAlpha(0.0f),
                                  bounds.getX(), bounds.getBottom(), false);
        fill.addColour(0.42, fire::ui::colours::ember.withAlpha(
                                 presentedSpectrumAlpha(0.09f)));
        g.setGradientFill(fill);
        g.fillPath(spectrumFillPath);

        g.setColour(fire::ui::colours::ember.withAlpha(
            presentedSpectrumAlpha(0.13f)));
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));

        juce::ColourGradient heat(fire::ui::colours::whiteHot.withAlpha(
                                      presentedSpectrumAlpha(0.90f)),
                                  bounds.getX(), bounds.getY(),
                                  fire::ui::colours::ember.withAlpha(
                                      presentedSpectrumAlpha(0.88f)),
                                  bounds.getX(), bounds.getBottom(), false);
        heat.addColour(0.55, fire::ui::colours::flame.withAlpha(
                                 presentedSpectrumAlpha(0.94f)));
        g.setGradientFill(heat);
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(1.45f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour(fire::ui::colours::signalCool.withAlpha(
            presentedSpectrumAlpha(0.055f)));
        g.fillPath(spectrumFillPath);
        g.setColour(fire::ui::colours::signalCool.withAlpha(
            presentedSpectrumAlpha(0.52f)));
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }

    if (mDrawPeak && isPeakLineVisible && hover > 0.001f
        && ! peakLinePath.isEmpty())
    {
        g.setColour(fire::ui::colours::whiteHot.withAlpha(0.56f * hover * opacity));
        g.strokePath(peakLinePath,
                     juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }

    if (mDrawPeak && hover > 0.001f
        && maxDecibelValue > minDisplayDb + 0.1f)
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

        // setOpacity() is a property of JUCE's current FillType and is lost
        // when drawGlassPill() selects its gradients/colours. A clipped layer
        // applies the hover fade to the complete pill without allocating a
        // full-component intermediate surface.
        const juce::Graphics::ScopedSaveState popupState(g);
        g.reduceClipRegion(popup.getSmallestIntegerContainer().expanded(1));
        g.beginTransparencyLayer(hover * opacity);

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

        g.endTransparencyLayer();
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

void SpectrumComponent::setHostBypassed(bool shouldBeBypassed, bool animate)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (hostBypassed == shouldBeBypassed)
    {
        // A hidden editor may be reattached while an earlier fade was paused.
        // Its first visible frame must already be clear when the caller asks
        // for a non-animated synchronisation of the same bypass state.
        if (shouldBeBypassed && ! animate)
        {
            presentationOpacity.snapTo(0.0f);
            resetRenderedData();
            repaint();
            updateAnimationTimer();
        }
        return;
    }

    hostBypassed = shouldBeBypassed;
    if (shouldBeBypassed)
        acceptingSpectrumUpdates.store(false, std::memory_order_release);

    // Reject every generation published before this presentation boundary.
    // Holding the same lock as updateSpectrum() also closes an update that
    // passed the atomic admission check immediately before bypass began.
    {
        const juce::ScopedLock locker(dataLock);
        consumedGeneration = pendingGeneration.load(std::memory_order_relaxed);
    }

    if (! shouldBeBypassed)
        acceptingSpectrumUpdates.store(true, std::memory_order_release);

    awaitingFreshFrame = true;
    presentationOpacity.setTarget(0.0f);

    if (! animate || ! isShowing()
        || (shouldBeBypassed && presentationOpacity.isSettled()
            && presentationOpacity.current <= 0.001f))
    {
        presentationOpacity.snapTo(0.0f);
        resetRenderedData();
    }

    repaint();
    updateAnimationTimer();
}

void SpectrumComponent::resetRenderedData()
{
    targetData.fill(0.0f);
    displayData.fill(0.0f);
    smoothedData.fill(0.0f);
    resetPeakData();
    isPeakLineVisible = false;
    interpolationActive = false;
    spectrumLinePath.clear();
    spectrumFillPath.clear();
    peakLinePath.clear();
    geometryDirty = false;
    renderedDataIsClear = true;
}

bool SpectrumComponent::consumePendingFrame(bool startFromSilence)
{
    const juce::ScopedLock locker(dataLock);
    const auto generation = pendingGeneration.load(std::memory_order_relaxed);
    if (generation == consumedGeneration)
        return false;

    const bool spectralGridChanged =
        pendingNumberOfBins != numberOfBins
        || ! juce::approximatelyEqual(pendingBinWidth, mBinWidth);

    if (startFromSilence || spectralGridChanged)
    {
        // Every array index now represents a different frequency. Neither an
        // interpolated trace nor a held peak may cross that boundary. The same
        // reset is mandatory at host-bypass resume even when the grid itself
        // did not change.
        resetRenderedData();
    }

    targetData = pendingData;
    numberOfBins = pendingNumberOfBins;
    mBinWidth = pendingBinWidth;
    consumedGeneration = generation;
    interpolationActive = true;
    renderedDataIsClear = false;
    return true;
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
    shouldBeOver = shouldBeOver && mDrawPeak && isShowing();
    if (! mDrawPeak || mouseOver == shouldBeOver)
        return;

    mouseOver = shouldBeOver;
    hoverOpacity.setTarget(mouseOver ? 1.0f : 0.0f);
    if (mouseOver)
    {
        resetPeakData();
        std::copy(displayData.begin(), displayData.end(), maxData.begin());
        isPeakLineVisible = true;
        rebuildPaths();
        repaint();
    }
    updateAnimationTimer();
}

void SpectrumComponent::resetHoverPresentation()
{
    mouseOver = false;
    hoverOpacity.snapTo(0.0f);
    resetPeakData();
    isPeakLineVisible = false;
    peakLinePath.clear();
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

    if (getPeer() == nullptr)
    {
        resetHoverPresentation();
        stopTimer();
    }
}

void SpectrumComponent::visibilityChanged()
{
    if (! isShowing())
    {
        resetHoverPresentation();
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
    const bool opacityIsAnimating = ! presentationOpacity.isSettled();
    const bool hoverIsAnimating = ! hoverOpacity.isSettled();
    // JUCE does not emit a hierarchy callback when a top-level peer is
    // removed. Retain the inexpensive 60 Hz liveness check while a hover
    // session is active, so peer detach clears it within one frame even when
    // audio/FFT publication has stopped and the fade itself has settled.
    const bool hoverSessionIsActive = mDrawPeak && mouseOver;
    const bool canPresentPendingFrame = hasPendingFrame && ! hostBypassed;
    if (isShowing() && (canPresentPendingFrame || interpolationActive
                        || peakIsDecaying || opacityIsAnimating
                        || hoverIsAnimating || hoverSessionIsActive))
    {
        if (! isTimerRunning())
            startTimerHz(60);
    }
    else
    {
        stopTimer();
    }
}
