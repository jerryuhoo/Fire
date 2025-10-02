/*
  ==============================================================================

    SpectrumBackground.cpp
    Created: 22 May 2024 3:13:06pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "SpectrumBackground.h"
#include "../../Utility/AudioHelpers.h" // Assuming transformToLog is here

const int SpectrumBackground::frequenciesForLines[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
const int SpectrumBackground::numberOfLines = 28;
const int SpectrumBackground::frequenciesForTextLabels[] = {
    20,
    100,
    200,
    1000,
    2000,
    10000,
    20000
};

//==============================================================================
SpectrumBackground::SpectrumBackground() : numberOfBins(1024), mBinWidth(44100 / (float) 2048)
{
    // Initialize with a placeholder image.
    cachedBackground = juce::Image(juce::Image::PixelFormat::ARGB, 1, 1, true);
}

SpectrumBackground::~SpectrumBackground()
{
}

void SpectrumBackground::paint(juce::Graphics& g)
{
    // 1. Get the real physical scale factor for this paint cycle
    const float currentDisplayScale = g.getInternalContext().getPhysicalPixelScaleFactor();

    // 2. Check if the scale has changed (or if it's the first time painting)
    if (currentDisplayScale != lastDisplayScale)
    {
        // If it changed, update our stored value and regenerate the background image
        lastDisplayScale = currentDisplayScale;
        createBackgroundImage();

        // Return immediately. The regeneration will trigger a new paint call.
        return;
    }
    // 3. If scale is unchanged, just draw the pre-rendered cache.
    g.drawImage(cachedBackground, getLocalBounds().toFloat());
}

void SpectrumBackground::resized()
{
    // First, get the scale factor from the parent editor, as before.
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* lnf = dynamic_cast<FireLookAndFeel*>(&editor->getLookAndFeel()))
            scale = lnf->scale;

    // Now, trigger the regeneration of the background image.
    // The resize itself will cause a repaint, which will then draw the new image.
    createBackgroundImage();
}

void SpectrumBackground::createBackgroundImage()
{
    // Get component bounds.
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    // Create a new image with the current component size.
    cachedBackground = juce::Image(juce::Image::ARGB,
                                   juce::roundToInt(getWidth() * lastDisplayScale),
                                   juce::roundToInt(getHeight() * lastDisplayScale),
                                   true);
    // Create a graphics context to draw onto our new image.
    juce::Graphics g(cachedBackground);
    g.addTransform(juce::AffineTransform::scale(lastDisplayScale));

    // --- All original painting logic is moved here ---

    // Paint background color onto the image.
    g.setColour(COLOUR6);
    g.fillAll();

    // Set a scalable font size for the frequency labels.
    g.setFont(12.0f * scale);

    // Paint horizontal lines and frequency numbers.
    g.setColour(juce::Colours::lightgrey.withAlpha(0.2f));
    auto horizontalLineY = (float) bounds.getHeight() / 5.0f;
    g.drawHorizontalLine((int) horizontalLineY, 0.0f, (float) bounds.getWidth());

    for (const auto freq : frequenciesForLines)
    {
        float xPos = transformToLog(freq) * bounds.getWidth();
        g.drawVerticalLine((int) xPos, horizontalLineY, (float) bounds.getHeight());
    }

    // Loop 2: Draw ONLY the text labels using the sparse array.
    for (const auto freq : frequenciesForTextLabels)
    {
        float xPos = transformToLog(freq) * bounds.getWidth();

        const int scaledWidth = (int) (60 * scale);
        const int scaledXOffset = (int) (30 * scale);

        juce::Rectangle<int> textBounds((int) xPos - scaledXOffset, 0, scaledWidth, (int) horizontalLineY);

        juce::String text;
        if (freq >= 1000)
            text = juce::String(freq / 1000) + "k";
        else
            text = juce::String(freq);

        auto justification = juce::Justification::centred;
        if (freq == 20)
            justification = juce::Justification::right;
        else if (freq == 20000)
            justification = juce::Justification::left;

        g.drawFittedText(text, textBounds, justification, 1);
    }
}