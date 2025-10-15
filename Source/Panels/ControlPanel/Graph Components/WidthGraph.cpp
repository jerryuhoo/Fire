/*
  ==============================================================================

    WidthGraph.cpp
    Created: 14 Dec 2020 3:40:40pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "WidthGraph.h"
#include "../../../GUI/InterfaceDefines.h"

//==============================================================================
WidthGraph::WidthGraph(FireAudioProcessor& p) : processor(p)
{
    startTimerHz(60);
}

WidthGraph::~WidthGraph()
{
    stopTimer();
}

void WidthGraph::paint(juce::Graphics& g)
{
    // The paint() function remains unchanged, still extremely simple and fast
    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds(), 1);
    g.drawImage(pointCloudCache, getLocalBounds().toFloat());
}

void WidthGraph::timerCallback()
{
    // Do nothing if the cache image is not valid.
    if (! pointCloudCache.isValid())
        return;

    // --- Efficient fade-out implementation ---
    // 1. Create a graphics context to draw onto our cached image.
    juce::Graphics g(pointCloudCache);

    // 2. Overlay a semi-transparent dark rectangle on the entire image.
    // This makes all existing points a little dimmer, simulating a fade-out effect.
    g.setColour(COLOUR7.withAlpha(0.2f)); // The alpha value controls the fade-out speed.
    g.fillRect(pointCloudCache.getBounds().reduced(1)); // Use reduced(1) to avoid covering the border.

    // --- Key Step 2: Draw the new points ---
    // (This part of the code is identical to your original version).

    // Get the latest audio data from the history buffer.
    historyL = processor.getHistoryArrayL();
    historyR = processor.getTotalNumInputChannels() == 2 ? processor.getHistoryArrayR() : historyL;

    // Apply coordinate transformations for the goniometer effect.
    float pi = juce::MathConstants<float>::pi;
    float rotateAngle = pi / 4.0f;
    g.addTransform(juce::AffineTransform::scale(-1, -1, getWidth() / 2.0f, getHeight() / 2.0f));
    g.addTransform(juce::AffineTransform::rotation(rotateAngle, getWidth() / 2.0f, getHeight() / 2.0f));

    // Find the maximum value for normalization.
    float maxValue = 0.0f;
    for (int i = 0; i < (int) historyL.size(); i++)
    {
        maxValue = std::max({ maxValue, std::abs(historyL[i]), std::abs(historyR[i]) });
    }

    // Draw the new points.
    g.setColour(juce::Colours::skyblue);
    if (maxValue > 0.00001f)
    {
        const float scaleFactor = getHeight() / (4.0f * maxValue);
        // Iterate by 2 for performance, drawing every other point.
        for (int i = 0; i < (int) historyL.size(); i += 2)
        {
            float x = historyL[i] * scaleFactor;
            float y = historyR[i] * scaleFactor;
            g.fillRect(getWidth() / 2.0f + x, getHeight() / 2.0f + y, 1.0f, 1.0f);
        }
    }

    // Trigger a repaint to show the updated image on screen.
    repaint();
}

void WidthGraph::resized()
{
    // When the component is resized, recreate a transparent cached image that matches the new dimensions
    if (getWidth() > 0 && getHeight() > 0)
    {
        pointCloudCache = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
    }
}
