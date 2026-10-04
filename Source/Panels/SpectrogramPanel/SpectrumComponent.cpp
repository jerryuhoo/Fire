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

bool hasVisibleEnergy(const std::array<float, 1024>& magnitudes, int numberOfBins)
{
    const auto floor = static_cast<float>(juce::jmax(1, numberOfBins)) * 1.0e-5f; // -100 dB.
    return std::any_of(magnitudes.begin(), magnitudes.begin() + numberOfBins,
                       [floor](float magnitude) { return magnitude > floor; });
}

struct SpectrumKnot { float x, y, peakY; };

void buildSmoothPath(juce::Path& path, const std::array<SpectrumKnot, 1024>& knots,
                     int count, bool peak)
{
    if (count == 0) return;
    const auto y = [peak](const SpectrumKnot& knot) { return peak ? knot.peakY : knot.y; };
    path.startNewSubPath(knots[0].x, y(knots[0]));
    if (count < 2) return;
    // Shape-preserving Hermite tangents: each FFT/pixel peak remains a knot,
    // extrema have zero slope, and a monotone interval cannot overshoot.
    // Fritsch-Butland, SIAM J. Sci. Comput. 5(2), 300-304 (1984).
    std::array<float, 1024> tangents;
    tangents[0] = (y(knots[1]) - y(knots[0])) / (knots[1].x - knots[0].x);
    tangents[static_cast<size_t>(count - 1)] =
        (y(knots[static_cast<size_t>(count - 1)]) - y(knots[static_cast<size_t>(count - 2)]))
        / (knots[static_cast<size_t>(count - 1)].x - knots[static_cast<size_t>(count - 2)].x);
    for (int index = 1; index + 1 < count; ++index)
    {
        const auto i = static_cast<size_t>(index);
        const auto leftWidth = knots[i].x - knots[i - 1].x;
        const auto rightWidth = knots[i + 1].x - knots[i].x;
        const auto leftSlope = (y(knots[i]) - y(knots[i - 1])) / leftWidth;
        const auto rightSlope = (y(knots[i + 1]) - y(knots[i])) / rightWidth;
        const auto leftWeight = 2 * rightWidth + leftWidth;
        const auto rightWeight = rightWidth + 2 * leftWidth;
        tangents[i] = leftSlope * rightSlope <= 0 ? 0
            : (leftWeight + rightWeight) / (leftWeight / leftSlope + rightWeight / rightSlope);
    }
    for (int index = 0; index + 1 < count; ++index)
    {
        const auto i = static_cast<size_t>(index);
        const auto third = (knots[i + 1].x - knots[i].x) / 3;
        path.cubicTo(knots[i].x + third, y(knots[i]) + tangents[i] * third,
                     knots[i + 1].x - third, y(knots[i + 1]) - tangents[i + 1] * third,
                     knots[i + 1].x, y(knots[i + 1]));
    }
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
    bool spectrumDataChanged = false;
    bool peakDataChanged = false;
    const auto newestGeneration = pendingGeneration.load(std::memory_order_acquire);

    if (hostBypassed)
    {
        if (presentationOpacity.isSettled()
            && presentationOpacity.current <= 0.001f
            && (! renderedDataIsClear || isPeakLineVisible))
        {
            // A held peak can outlive a silent live trace. Bypass closes both
            // presentations, even when live data already reached the floor.
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
        }
    }

    bool stillInterpolating = false;
    if (interpolationActive)
    {
        for (int i = 0; i < numberOfBins; ++i)
        {
            const float difference = targetData[static_cast<size_t>(i)]
                                   - displayData[static_cast<size_t>(i)];
            spectrumDataChanged = spectrumDataChanged || difference != 0.0f;

            if (std::abs(difference) > 1.0e-5f)
            {
                // A descending trace carries the recent energy briefly; its
                // motion is driven entirely by the newest FFT frame.
                const auto factor = difference < 0.0f ? releaseFactor : interpolationFactor;
                auto& displayed = displayData[static_cast<size_t>(i)];
                const auto next = displayed + difference * factor;
                if (next == displayed)
                    displayed = targetData[static_cast<size_t>(i)];
                else
                {
                    displayed = next;
                    stillInterpolating = true;
                }
            }
            else
            {
                displayData[static_cast<size_t>(i)] = targetData[static_cast<size_t>(i)];
            }
        }

        if (! hasVisibleEnergy(displayData, numberOfBins)
            && ! hasVisibleEnergy(targetData, numberOfBins))
        {
            // Once below the plotted floor there is no visible release left.
            // Keep held hover measurements separately, without a bright
            // horizontal trace or an inaudible interpolation tail at silence.
            displayData = targetData;
            stillInterpolating = false;
            renderedDataIsClear = true;
            spectrumDataChanged = true;
        }
        interpolationActive = stillInterpolating;
    }

    if (mDrawPeak && mouseOver && (spectrumDataChanged || geometryDirty)
        && ! hostBypassed && ! renderedDataIsClear)
    {
        for (int i = 0; i < numberOfBins; ++i)
        {
            auto& held = maxData[static_cast<size_t>(i)];
            const auto value = displayData[static_cast<size_t>(i)];
            if (value > held)
            {
                held = value;
                peakDataChanged = true;
            }
        }

        peakDataChanged = peakDataChanged || ! isPeakLineVisible;
        isPeakLineVisible = true;
    }
    else if (mDrawPeak && ! mouseOver && isPeakLineVisible)
    {
        float loudestPeak = 0.0f;
        for (int i = 0; i < numberOfBins; ++i)
        {
            auto& peak = maxData[static_cast<size_t>(i)];
            peak *= 0.88f;
            loudestPeak = juce::jmax(loudestPeak, peak);
        }

        if (magnitudeToDb(loudestPeak, numberOfBins) <= minDisplayDb + 0.5f)
        {
            maxData.fill(0.0f);
            isPeakLineVisible = false;
        }

        peakDataChanged = true;
    }

    // Opacity animations repaint the existing geometry. Frequency paths only
    // change when the samples, held peaks or component dimensions change.
    if (spectrumDataChanged || peakDataChanged || geometryDirty)
    {
        rebuildPaths(spectrumDataChanged || geometryDirty, peakDataChanged || geometryDirty);
        visualStateChanged = true;
    }

    if (visualStateChanged)
        repaint();

    updateAnimationTimer();
}

void SpectrumComponent::rebuildFrequencyLayout()
{
    frequencyLayoutDirty = false;
    visibleFrequencyBins = 0;
    const auto bounds = getLocalBounds().toFloat();
    for (int bin = 1; bin < numberOfBins; ++bin)
    {
        const float frequency = static_cast<float>(bin) * mBinWidth;
        if (frequency < minimumDisplayFrequency || frequency > maximumDisplayFrequency)
            continue;

        const float normalisedX = transformToLog(frequency);
        if (! std::isfinite(normalisedX))
            continue;

        const float x = bounds.getX() + normalisedX * bounds.getWidth();
        const int bucket = juce::jlimit(0, juce::jmax(0, getWidth() - 1),
                                       static_cast<int>(std::floor(x)));
        frequencyLayout[static_cast<size_t>(visibleFrequencyBins++)] = { bin, bucket, x };
    }
}

void SpectrumComponent::rebuildPaths(bool rebuildSpectrum, bool rebuildPeak)
{
    if (rebuildSpectrum)
    {
        spectrumLinePath.clear();
        spectrumFillPath.clear();
        spectrumRaster.dirty = true;
    }
    if (rebuildPeak)
    {
        peakLinePath.clear();
        peakRaster.dirty = true;
    }
    geometryDirty = false;

    const auto bounds = getLocalBounds().toFloat();
    const bool drawPeak = mDrawPeak && isPeakLineVisible && hasVisibleEnergy(maxData, numberOfBins);
    if ((renderedDataIsClear && ! drawPeak) || bounds.isEmpty()
        || numberOfBins < 2 || mBinWidth <= 0.0f)
        return;

    // The logarithmic x-axis depends only on size and the FFT frequency grid.
    if (frequencyLayoutDirty)
        rebuildFrequencyLayout();

    spectrumLinePath.preallocateSpace(juce::jmax(64, getWidth() * 7));
    peakLinePath.preallocateSpace(juce::jmax(64, getWidth() * 7));

    int currentBucket = -1;
    float currentBucketY = bounds.getBottom();
    float peakBucketY = bounds.getBottom();
    std::array<SpectrumKnot, 1024> knots;
    int knotCount = 0;

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
        knots[static_cast<size_t>(knotCount++)] = {x, currentBucketY, peakBucketY};
    };

    for (int position = 0; position < visibleFrequencyBins; ++position)
    {
        const auto& binPosition = frequencyLayout[static_cast<size_t>(position)];
        const int i = binPosition.bin;
        const int bucket = binPosition.bucket;
        // Keep the measured magnitude. Curve interpolation rounds the shape
        // between knots without averaging away an isolated FFT peak.
        const float currentDb = magnitudeToDb(displayData[static_cast<size_t>(i)], numberOfBins);
        const float currentY = dbToY(currentDb);
        const float peakDb = drawPeak ? magnitudeToDb(maxData[static_cast<size_t>(i)], numberOfBins)
                                     : minDisplayDb;
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

        if (drawPeak && peakDb > maxDecibelValue)
        {
            maxDecibelValue = peakDb;
            maxFreq = static_cast<float>(i) * mBinWidth;
            maxDecibelPoint = { binPosition.x, peakY };
        }
    }

    flushBucket(currentBucket);

    if (rebuildSpectrum && ! renderedDataIsClear && knotCount > 0)
    {
        buildSmoothPath(spectrumLinePath, knots, knotCount, false);
        spectrumFillPath = spectrumLinePath;
        spectrumFillPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumFillPath.lineTo(bounds.getX(), bounds.getBottom());
        spectrumFillPath.closeSubPath();
    }
    if (rebuildPeak && drawPeak) buildSmoothPath(peakLinePath, knots, knotCount, true);
}

void SpectrumComponent::drawSpectrumContent(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (mStyle == 1)
    {
        juce::ColourGradient fill(fire::ui::colours::flame.withAlpha(0.10f),
                                  bounds.getX(), bounds.getY(),
                                  fire::ui::colours::ember.withAlpha(0.0f),
                                  bounds.getX(), bounds.getBottom(), false);
        fill.addColour(0.42, fire::ui::colours::ember.withAlpha(0.045f));
        g.setGradientFill(fill);
        g.fillPath(spectrumFillPath);

        g.setColour(fire::ui::colours::ember.withAlpha(0.08f));
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(2.8f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));

        juce::ColourGradient heat(fire::ui::colours::whiteHot.withAlpha(0.94f),
                                  bounds.getX(), bounds.getY(),
                                  fire::ui::colours::ember.withAlpha(0.84f),
                                  bounds.getX(), bounds.getBottom(), false);
        heat.addColour(0.55, fire::ui::colours::flame.withAlpha(0.94f));
        g.setGradientFill(heat);
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(1.35f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.018f));
        g.fillPath(spectrumFillPath);
        g.setColour(juce::Colours::white.withAlpha(0.42f));
        g.strokePath(spectrumLinePath,
                     juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }
}

void SpectrumComponent::updateRaster(RasterCache& cache, float scale, bool peak)
{
    const auto width = juce::jmax(1, juce::roundToInt(std::ceil(getWidth() * scale)));
    const auto height = juce::jmax(1, juce::roundToInt(std::ceil(getHeight() * scale)));
    if (cache.image.getWidth() != width || cache.image.getHeight() != height || cache.scale != scale)
    {
        // Constant-colour traces need only coverage, avoiding RGB image
        // conversion/compositing work for the dry trace and held peaks.
        if (peak || mStyle != 1)
            cache.image = juce::Image(juce::Image::SingleChannel, width, height, true, juce::SoftwareImageType{});
        else
        {
#if JUCE_MAC
        // Native backing retains the CGImage between opacity-only paints;
        // drawing a SoftwareImage directly would copy its pixels each time.
        cache.image = juce::Image(juce::Image::ARGB, width, height, true);
#else
        cache.image = juce::Image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType{});
#endif
        }
        cache.scale = scale;
        cache.dirty = true;
    }
    if (! cache.dirty) return;
    cache.image.clear(cache.image.getBounds());
    juce::LowLevelGraphicsSoftwareRenderer renderer(cache.image);
    juce::Graphics rasterGraphics(renderer);
    rasterGraphics.addTransform(juce::AffineTransform::scale(scale));
    if (peak)
    {
        rasterGraphics.setColour(juce::Colours::white.withAlpha(0.56f));
        rasterGraphics.strokePath(peakLinePath, juce::PathStrokeType(1.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else drawSpectrumContent(rasterGraphics);
    cache.dirty = false;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    ++cache.renderCount;
#endif
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto opacity = juce::jlimit(0.0f, 1.0f, presentationOpacity.current);
    const auto hover = juce::jlimit(0.0f, 1.0f, hoverOpacity.current);
    if (bounds.isEmpty() || (spectrumLinePath.isEmpty() && peakLinePath.isEmpty()) || opacity <= 0.001f)
        return;

    const juce::Graphics::ScopedSaveState state(g);
    const auto scale = juce::jlimit(0.5f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (specAlpha > 0.001f && ! spectrumLinePath.isEmpty())
    {
        // Rasterising dense native cubic strokes can be much more expensive
        // than software rendering on macOS. Cache software layers at device
        // resolution; opacity-only frames then composite the same pixels.
        updateRaster(spectrumRaster, scale, false);
        if (mStyle == 1)
        {
            g.setOpacity(specAlpha * opacity);
            g.drawImage(spectrumRaster.image, bounds);
        }
        else
        {
            g.setColour(fire::ui::colours::textSecondary.withAlpha(specAlpha * opacity));
            g.drawImage(spectrumRaster.image, bounds, juce::RectanglePlacement::stretchToFit, true);
        }
    }

    if (mDrawPeak && isPeakLineVisible && hover > 0.001f
        && ! peakLinePath.isEmpty())
    {
        updateRaster(peakRaster, scale, true);
        g.setColour(fire::ui::colours::whiteHot.withAlpha(hover * opacity));
        g.drawImage(peakRaster.image, bounds, juce::RectanglePlacement::stretchToFit, true);
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
    frequencyLayoutDirty = true;
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
    resetPeakData();
    isPeakLineVisible = false;
    interpolationActive = false;
    spectrumLinePath.clear();
    spectrumFillPath.clear();
    peakLinePath.clear();
    spectrumRaster.dirty = peakRaster.dirty = true;
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

    frequencyLayoutDirty = frequencyLayoutDirty
                        || pendingNumberOfBins != numberOfBins
                        || pendingBinWidth != mBinWidth;

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
    if (renderedDataIsClear && ! hasVisibleEnergy(targetData, numberOfBins))
    {
        displayData = targetData;
        interpolationActive = false;
        return true;
    }
    interpolationActive = true;
    geometryDirty = geometryDirty || renderedDataIsClear || frequencyLayoutDirty;
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
    const bool hoverSessionIsActive = mDrawPeak && mouseOver && ! hostBypassed && ! awaitingFreshFrame;
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
