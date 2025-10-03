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

    if (isMouseOn && ! mZoomState)
    {
        g.setColour(WIDTH_COLOUR.withAlpha(0.05f));
        g.fillAll();
    }
}

void WidthGraph::timerCallback()
{
    if (! pointCloudCache.isValid())
        return;

    // --- Key Step 1: Directly manipulate pixel data to achieve a transparent fade-out ---
    juce::Image::BitmapData bitmapData(pointCloudCache, juce::Image::BitmapData::readWrite);

    const int width = pointCloudCache.getWidth();
    const int height = pointCloudCache.getHeight();
    const float fadeFactor = 0.8f; // Controls the fade-out speed, a smaller value fades out faster

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // Get the color of the current pixel
            juce::Colour pixelColour = bitmapData.getPixelColour(x, y);

            // If this pixel is not completely transparent
            if (pixelColour.getAlpha() > 0)
            {
                // Lower its Alpha value
                juce::uint8 newAlpha = static_cast<juce::uint8>(pixelColour.getAlpha() * fadeFactor);

                // Set the new pixel color (only changing the Alpha)
                bitmapData.setPixelColour(x, y, pixelColour.withAlpha(newAlpha));
            }
        }
    }

    // --- Key Step 2: Draw the new points ---
    // Create a graphics context for our cached image
    juce::Graphics g(pointCloudCache);

    // Get the latest audio data
    historyL = processor.getHistoryArrayL();
    historyR = processor.getTotalNumInputChannels() == 2 ? processor.getHistoryArrayR() : historyL;

    // Apply coordinate transformations
    float pi = juce::MathConstants<float>::pi;
    float rotateAngle = pi / 4.0f;
    g.addTransform(juce::AffineTransform::scale(-1, -1, getWidth() / 2.0f, getHeight() / 2.0f));
    g.addTransform(juce::AffineTransform::rotation(rotateAngle, getWidth() / 2.0f, getHeight() / 2.0f));

    // Find the maximum value for normalization
    float maxValue = 0.0f;
    for (int i = 0; i < (int)historyL.size(); i++)
    {
        maxValue = std::max({ maxValue, std::abs(historyL[i]), std::abs(historyR[i]) });
    }

    // Draw the new points with a fully opaque color
    g.setColour(juce::Colours::skyblue);
    if (maxValue > 0.00001f)
    {
        const float scaleFactor = getHeight() / (4.0f * maxValue);
        for (int i = 0; i < (int)historyL.size(); i += 2)
        {
            float x = historyL[i] * scaleFactor;
            float y = historyR[i] * scaleFactor;
            g.fillRect(getWidth() / 2.0f + x, getHeight() / 2.0f + y, 1.0f, 1.0f);
        }
    }

    // Trigger a repaint
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
