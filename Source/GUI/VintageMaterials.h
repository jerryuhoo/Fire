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


// Pass only the editor body below the existing header. The centre stays
// untouched: inset the dark operating panels by 20 * scale at either side
// and 12 * scale above the bottom to expose the cabinet and its contact gap.
inline void drawWalnutBodyFrame(juce::Graphics& g, juce::Rectangle<float> bounds, float scale)
{
    if (bounds.isEmpty()) return;
    scale = juce::jmax(.25f, scale);
    const auto cheekWidth = juce::jmin(16.0f * scale, bounds.getWidth() * .08f);
    const auto bottomHeight = juce::jmin(8.0f * scale, bounds.getHeight() * .10f);
    const auto contactGap = juce::jmin(3.0f * scale, cheekWidth * .25f);
    const auto radius = juce::jmin(2.0f * scale, cheekWidth * .20f);
    static const juce::Image grain = juce::ImageFileFormat::loadFrom(
        BinaryData::vintage_walnut_png, BinaryData::vintage_walnut_pngSize);
    juce::Graphics::ScopedSaveState frameState(g);
    g.reduceClipRegion(bounds.toNearestInt());

    for (bool left : {true, false})
    {
        const auto cheek = left ? bounds.withWidth(cheekWidth) : bounds.withLeft(bounds.getRight() - cheekWidth);
        const auto contact = left ? cheek.withLeft(cheek.getRight()).withWidth(contactGap)
                                  : cheek.withRight(cheek.getX()).withLeft(cheek.getX() - contactGap);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(.68f),
                                             left ? contact.getTopLeft() : contact.getTopRight(),
                                             juce::Colours::transparentBlack,
                                             left ? contact.getTopRight() : contact.getTopLeft(), false));
        g.fillRect(contact);

        juce::Graphics::ScopedSaveState cheekState(g);
        juce::Path outline;
        outline.addRoundedRectangle(cheek, radius);
        g.reduceClipRegion(outline);
        g.setColour(juce::Colour(0xff51453a));
        g.fillRect(cheek);
        if (grain.isValid())
        {
            // Each solid side board follows the source's long vertical grain.
            // Uniform sampling keeps pores natural instead of compressing the
            // whole texture into a narrow strip. Opposite boards use distinct
            // sections of the same walnut stock.
            const auto textureScale = cheek.getHeight() / static_cast<float>(grain.getHeight());
            const auto sourceCentre = static_cast<float>(grain.getWidth()) * (left ? .23f : .71f);
            g.setOpacity(.68f);
            g.drawImageTransformed(grain, juce::AffineTransform(
                textureScale, 0.0f, cheek.getCentreX() - sourceCentre * textureScale,
                0.0f, textureScale, cheek.getY()));
            g.setOpacity(1.0f);
        }
        g.setColour(juce::Colour(0xff49423a).withAlpha(.23f));
        g.fillRect(cheek);
        const auto outer = left ? cheek.getTopLeft() : cheek.getTopRight();
        const auto inner = left ? cheek.getTopRight() : cheek.getTopLeft();
        juce::ColourGradient profile(juce::Colour(0xff17130f).withAlpha(.44f), outer,
                                    juce::Colour(0xff100d0a).withAlpha(.48f), inner, false);
        profile.addColour(.13, juce::Colour(0xffe3cfac).withAlpha(left ? .23f : .12f));
        profile.addColour(.32, juce::Colours::transparentBlack);
        profile.addColour(.78, juce::Colour(0xff211a14).withAlpha(.08f));
        g.setGradientFill(profile);
        g.fillRect(cheek);
        const auto highlightX = left ? cheek.getX() + 2.0f * scale : cheek.getRight() - 2.0f * scale;
        g.setColour(juce::Colour(0xffeddbbe).withAlpha(left ? .15f : .08f));
        g.drawLine(highlightX, cheek.getY() + radius, highlightX, cheek.getBottom() - bottomHeight, .65f * scale);
        const auto innerX = left ? cheek.getRight() - .5f * scale : cheek.getX() + .5f * scale;
        g.setColour(juce::Colour(0xff160f0b).withAlpha(.72f));
        g.drawLine(innerX, cheek.getY(), innerX, cheek.getBottom() - bottomHeight, .85f * scale);
    }

    const auto lowerRail = bounds.withTop(bounds.getBottom() - bottomHeight);
    const auto lowerContact = juce::Rectangle<float>(bounds.getX() + cheekWidth, lowerRail.getY() - contactGap,
                                                     juce::jmax(0.0f, bounds.getWidth() - 2.0f * cheekWidth), contactGap);
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, lowerContact.getTopLeft(),
                                         juce::Colours::black.withAlpha(.70f), lowerContact.getBottomLeft(), false));
    g.fillRect(lowerContact);
    // The lower front board has horizontal long grain and the exact finish
    // of the approved header; only this separate bottom strip is drawn here.
    drawWalnutRail(g, lowerRail, scale);
    g.setColour(juce::Colour(0xff170f09).withAlpha(.58f));
    for (float x : {bounds.getX() + cheekWidth, bounds.getRight() - cheekWidth})
        g.drawLine(x, lowerRail.getY() + .8f * scale, x, lowerRail.getBottom() - .8f * scale, .65f * scale);
}

}
