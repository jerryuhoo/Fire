/*
  ==============================================================================

    SpectrumBackground.cpp
    Created: 22 May 2024 3:13:06pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "SpectrumBackground.h"
#include "../../GUI/Skin.h"
#include "../../Utility/AudioHelpers.h" // Assuming transformToLog is here

const int SpectrumBackground::frequenciesForLines[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
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
SpectrumBackground::SpectrumBackground()
{
    cachedBackground = juce::Image(juce::Image::PixelFormat::ARGB, 1, 1, true);
    setOpaque(true);
    setInterceptsMouseClicks(false, false);
}

SpectrumBackground::~SpectrumBackground() = default;

void SpectrumBackground::paint(juce::Graphics& g)
{
    const float currentDisplayScale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (! juce::approximatelyEqual(currentDisplayScale, lastDisplayScale)
        || cachedLogicalBounds != getLocalBounds())
    {
        lastDisplayScale = currentDisplayScale;
        createBackgroundImage();
    }

    if (cachedBackground.isValid())
        g.drawImage(cachedBackground, getLocalBounds().toFloat());
}

void SpectrumBackground::lookAndFeelChanged()
{
    cachedBackground = {};
    cachedLogicalBounds = {};
    createBackgroundImage();
    repaint();
}

void SpectrumBackground::resized()
{
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* lnf = dynamic_cast<FireLookAndFeel*>(&editor->getLookAndFeel()))
            scale = lnf->scale;

    createBackgroundImage();
}

void SpectrumBackground::createBackgroundImage()
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    const int physicalWidth = juce::jmax(1, juce::roundToInt(getWidth() * lastDisplayScale));
    const int physicalHeight = juce::jmax(1, juce::roundToInt(getHeight() * lastDisplayScale));
    if (cachedBackground.isValid()
        && cachedBackground.getWidth() == physicalWidth
        && cachedBackground.getHeight() == physicalHeight
        && juce::approximatelyEqual(cachedUiScale, scale)
        && cachedLogicalBounds == bounds)
        return;

    cachedUiScale = scale;
    cachedLogicalBounds = bounds;
    cachedBackground = juce::Image(juce::Image::ARGB,
                                   physicalWidth,
                                   physicalHeight,
                                   true);

    juce::Graphics g(cachedBackground);
    g.addTransform(juce::AffineTransform::scale(lastDisplayScale));

    const auto area = bounds.toFloat();
    if (fire::ui::isVintage(*this)) g.fillAll(fire::ui::paletteFor(*this).canvas);
    else fire::ui::drawCanvas(g, area);

    // A very restrained technical grid gives the analyser depth without
    // competing with the moving spectrum.
    fire::ui::drawTechGrid(g, area, 24.0f * scale, 0.055f, fire::ui::skinFor(*this));

    const float headerHeight = juce::jmax(22.0f * scale, area.getHeight() * 0.19f);
    juce::ColourGradient headerShade(fire::ui::paletteFor(*this).surface2.withAlpha(0.72f),
                                     area.getX(), area.getY(),
                                     fire::ui::paletteFor(*this).surface0.withAlpha(0.16f),
                                     area.getX(), area.getY() + headerHeight, false);
    g.setGradientFill(headerShade);
    g.fillRect(area.withHeight(headerHeight));

    g.setColour(fire::ui::paletteFor(*this).hairline.withAlpha(0.26f));
    for (int division = 1; division < 5; ++division)
    {
        const float y = fire::ui::pixelAligned(area.getY() + area.getHeight() * division / 5.0f,
                                                lastDisplayScale);
        g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
    }

    for (const auto freq : frequenciesForLines)
    {
        const float xPos = fire::ui::pixelAligned(transformToLog(freq) * area.getWidth(),
                                                   lastDisplayScale);
        const bool major = freq == 20 || freq == 100 || freq == 200 || freq == 1000
                        || freq == 2000 || freq == 10000 || freq == 20000;
        g.setColour(fire::ui::paletteFor(*this).hairline.withAlpha(major ? 0.30f : 0.12f));
        g.drawVerticalLine(juce::roundToInt(xPos), headerHeight, area.getBottom());
    }

    g.setFont(fire::ui::bodyFont(11.0f * scale));
    g.setColour(fire::ui::paletteFor(*this).textSecondary.withAlpha(0.84f));
    for (const auto freq : frequenciesForTextLabels)
    {
        const float xPos = transformToLog(freq) * area.getWidth();

        const int scaledWidth = juce::roundToInt(64.0f * scale);
        const int scaledXOffset = scaledWidth / 2;

        juce::Rectangle<int> textBounds(juce::roundToInt(xPos) - scaledXOffset,
                                        0,
                                        scaledWidth,
                                        juce::roundToInt(headerHeight));

        const auto text = freq >= 1000 ? juce::String(freq / 1000) + " kHz"
                                       : juce::String(freq) + " Hz";

        auto justification = juce::Justification::centred;
        if (freq == 20)
            justification = juce::Justification::right;
        else if (freq == 20000)
            justification = juce::Justification::left;

        g.drawFittedText(text, textBounds, justification, 1);
    }
}
