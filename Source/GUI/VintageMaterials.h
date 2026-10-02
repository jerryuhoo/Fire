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

// Cabinet stock for a wide header. At scale 1 a 1000 x 56 rail pairs with
// an inset panel at (8, 8, 984, 42), leaving an 8 px upper shoulder and a
// 6 px lower lip. Draw the dark inset panel and its own contact shadow after
// this rail; drawWalnut remains the material for the existing hardware cards.
inline void drawWalnutRail(juce::Graphics& g, juce::Rectangle<float> bounds, float scale)
{
    if (bounds.isEmpty()) return;
    scale = juce::jmax(.25f, scale);
    const auto radius = juce::jmin(4.0f * scale, bounds.getHeight() * .20f);
    const auto bevel = juce::jmin(3.0f * scale, bounds.getHeight() * .12f);
    static const juce::Image grain = juce::ImageFileFormat::loadFrom(
        BinaryData::vintage_walnut_png, BinaryData::vintage_walnut_pngSize);
    juce::Graphics::ScopedSaveState state(g);

    // A narrow contact shadow and dark bottom edge give the cabinet actual
    // depth before the polished wooden front is laid over its rear edge.
    g.setColour(juce::Colours::black.withAlpha(.36f));
    g.fillRoundedRectangle(bounds.translated(0, 2.0f * scale), radius);
    g.setColour(juce::Colour(0xff211c17));
    g.fillRoundedRectangle(bounds.translated(0, .8f * scale), radius);

    juce::Path outline;
    outline.addRoundedRectangle(bounds, radius);
    g.reduceClipRegion(outline);
    g.setColour(juce::Colour(0xff51453a));
    g.fillRect(bounds);
    if (grain.isValid())
    {
        // Rotate the source's long grain through 90 degrees and sample a
        // narrow central strip at a uniform scale. Stretching the entire
        // square bitmap into the header would produce a flat veneer effect.
        const auto textureScale = bounds.getWidth() / static_cast<float>(grain.getHeight());
        const auto grainCentre = static_cast<float>(grain.getWidth()) * .46f;
        g.setOpacity(.68f);
        g.drawImageTransformed(grain, juce::AffineTransform(
            0.0f, textureScale, bounds.getX(),
            -textureScale, 0.0f, bounds.getCentreY() + grainCentre * textureScale));
        g.setOpacity(1.0f);
    }
    // Neutral brown glaze keeps the walnut quiet beside a warm charcoal
    // control panel, while retaining visible pores and the horizontal grain.
    g.setColour(juce::Colour(0xff49423a).withAlpha(.23f));
    g.fillRect(bounds);
    juce::ColourGradient profile(juce::Colour(0xff17130f).withAlpha(.25f), bounds.getTopLeft(),
                                juce::Colour(0xff120e0b).withAlpha(.60f), bounds.getBottomLeft(), false);
    profile.addColour(.045, juce::Colour(0xffecdcc2).withAlpha(.20f));
    profile.addColour(.13, juce::Colour(0xffd5c3a8).withAlpha(.035f));
    profile.addColour(.53, juce::Colours::transparentBlack);
    profile.addColour(.84, juce::Colour(0xff211a14).withAlpha(.14f));
    g.setGradientFill(profile);
    g.fillRect(bounds);

    // The lower chamfer reflects less light than the top. These slim bands
    // follow the grain rather than boxing the header in a uniform outline.
    auto upperBevel = bounds.withHeight(bevel);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff160f0b).withAlpha(.36f), upperBevel.getTopLeft(),
                                         juce::Colour(0xffebd2ab).withAlpha(.18f), upperBevel.getBottomLeft(), false));
    g.fillRect(upperBevel);
    auto lowerBevel = bounds.withTop(bounds.getBottom() - bevel * 1.4f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, lowerBevel.getTopLeft(),
                                         juce::Colour(0xff100d0a).withAlpha(.52f), lowerBevel.getBottomLeft(), false));
    g.fillRect(lowerBevel);

    const auto cheekWidth = juce::jmin(9.0f * scale, bounds.getWidth() * .04f);
    for (bool left : {true, false})
    {
        const auto cheek = left ? bounds.withWidth(cheekWidth) : bounds.withLeft(bounds.getRight() - cheekWidth);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(left ? .29f : .44f),
                                             left ? cheek.getTopLeft() : cheek.getTopRight(),
                                             juce::Colours::transparentBlack,
                                             left ? cheek.getTopRight() : cheek.getTopLeft(), false));
        g.fillRect(cheek);
    }
    g.setColour(juce::Colour(0xffeddbbe).withAlpha(.19f));
    g.drawLine(bounds.getX() + radius, bounds.getY() + bevel,
               bounds.getRight() - radius, bounds.getY() + bevel, .65f * scale);
    g.setColour(juce::Colour(0xff130f0b).withAlpha(.68f));
    g.drawLine(bounds.getX() + radius, bounds.getBottom() - .55f * scale,
               bounds.getRight() - radius, bounds.getBottom() - .55f * scale, .85f * scale);
}

}
