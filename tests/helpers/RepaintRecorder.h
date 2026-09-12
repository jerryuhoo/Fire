#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Observe the actual regions passed to Component::repaint without a native
// peer, then render just those regions to catch stale overlay pixels.
class RepaintRecorder final : public juce::CachedComponentImage
{
public:
    explicit RepaintRecorder(juce::Component& componentToUse) : component(componentToUse) {}

    void paint(juce::Graphics& graphics) override { component.paint(graphics); }
    bool invalidateAll() override
    {
        dirtyAreas.add(component.getLocalBounds());
        ++fullInvalidations;
        return false;
    }
    bool invalidate(const juce::Rectangle<int>& area) override
    {
        dirtyAreas.add(area);
        return false;
    }
    void releaseResources() override {}
    void clear()
    {
        dirtyAreas.clear();
        fullInvalidations = 0;
    }
    void paintDirtyAreas(juce::Image& image, float scale = 1.0f)
    {
        juce::Graphics graphics(image);
        // A peer clips its damage in device pixels before painting the
        // scaled component. Applying a transformed logical clip instead
        // creates an antialiased path mask and repeatedly blends the old
        // backing pixels at its boundary, which is not a window repaint.
        juce::RectangleList<int> pixelAreas;
        for (const auto& area : dirtyAreas)
            pixelAreas.add((area.toFloat() * scale).getSmallestIntegerContainer());
        graphics.reduceClipRegion(pixelAreas);
        graphics.addTransform(juce::AffineTransform::scale(scale));
        component.paint(graphics);
    }

    juce::RectangleList<int> dirtyAreas;
    int fullInvalidations = 0;

private:
    juce::Component& component;
};

inline juce::Image renderRepaintTestComponent(juce::Component& component, float scale = 1.0f)
{
    // Native ellipse antialiasing may round a colour level differently with
    // a different clip boundary. Use JUCE's software rasteriser for exact
    // replay comparisons at integer scales.
    juce::Image image(juce::Image::ARGB,
        juce::jmax(1, juce::roundToInt(component.getWidth() * scale)),
        juce::jmax(1, juce::roundToInt(component.getHeight() * scale)), true,
        juce::SoftwareImageType());
    juce::Graphics graphics(image);
    graphics.addTransform(juce::AffineTransform::scale(scale));
    component.paint(graphics);
    return image;
}

inline bool repaintTestImagesMatch(const juce::Image& lhs, const juce::Image& rhs)
{
    if (lhs.getBounds() != rhs.getBounds())
        return false;
    for (int y = 0; y < lhs.getHeight(); ++y)
        for (int x = 0; x < lhs.getWidth(); ++x)
            if (lhs.getPixelAt(x, y) != rhs.getPixelAt(x, y))
                return false;
    return true;
}
