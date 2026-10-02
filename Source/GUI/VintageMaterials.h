#pragma once
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace fire::ui
{
inline void drawWalnut(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 4.0f)
{
    if (bounds.isEmpty()) return;
    static const juce::Image grain = juce::ImageFileFormat::loadFrom(
        BinaryData::vintage_walnut_png, BinaryData::vintage_walnut_pngSize);
    juce::Graphics::ScopedSaveState state(g);
    juce::Path outline; outline.addRoundedRectangle(bounds, radius);
    g.reduceClipRegion(outline);
    g.setColour(juce::Colour(0xff4a2e1c)); g.fillRect(bounds);
    g.drawImage(grain, bounds, juce::RectanglePlacement::fillDestination);
    juce::ColourGradient light(juce::Colour(0xffefc792).withAlpha(.14f), bounds.getTopLeft(),
        juce::Colours::black.withAlpha(.40f), bounds.getBottomRight(), false);
    light.addColour(.15, juce::Colours::transparentBlack);
    light.addColour(.80, juce::Colours::black.withAlpha(.09f));
    g.setGradientFill(light); g.fillRect(bounds);
    g.setColour(juce::Colour(0xfff4c697).withAlpha(.22f)); g.drawRoundedRectangle(bounds.reduced(.8f),radius,1);
    g.setColour(juce::Colours::black.withAlpha(.64f)); g.drawRoundedRectangle(bounds.reduced(.1f),radius,1);
}
}
