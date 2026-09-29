#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <initializer_list>

namespace fire::ui::brand
{
// Both marks use the same transparent 128 x 128 design space as the SVGs in
// assets/images. The paths are constructed once, never parsed in paint().
inline const juce::Path& firePath()
{
    static const auto path = []
    {
        juce::Path p;
        // The original character's central, forked stroke.
        p.startNewSubPath(52.0f, 8.0f);
        p.lineTo(78.0f, 8.0f);
        p.lineTo(78.0f, 48.0f);
        p.cubicTo(80.0f, 64.0f, 97.0f, 87.0f, 123.0f, 100.0f);
        p.lineTo(111.0f, 123.0f);
        p.cubicTo(91.0f, 114.0f, 77.0f, 99.0f, 69.0f, 85.0f);
        p.cubicTo(61.0f, 101.0f, 44.0f, 115.0f, 23.0f, 123.0f);
        p.lineTo(8.0f, 102.0f);
        p.cubicTo(32.0f, 91.0f, 45.0f, 78.0f, 50.0f, 61.0f);
        p.cubicTo(55.0f, 43.0f, 54.0f, 25.0f, 52.0f, 8.0f);
        p.closeSubPath();

        // Separate side strokes keep this recognisably 火 at small sizes.
        p.startNewSubPath(8.0f, 33.0f);
        p.lineTo(29.0f, 22.0f);
        p.lineTo(49.0f, 58.0f);
        p.lineTo(25.0f, 71.0f);
        p.closeSubPath();
        p.startNewSubPath(97.0f, 22.0f);
        p.lineTo(122.0f, 30.0f);
        p.lineTo(107.0f, 69.0f);
        p.lineTo(84.0f, 57.0f);
        p.closeSubPath();
        return p;
    }();
    return path;
}

inline const juce::Path& wingsPath()
{
    static const auto path = []
    {
        juce::Path p;
        p.setUsingNonZeroWinding(false);
        const auto polygon = [&p](std::initializer_list<juce::Point<float>> points)
        {
            auto point = points.begin();
            p.startNewSubPath(*point++);
            while (point != points.end()) p.lineTo(*point++);
            p.closeSubPath();
        };

        // Only the bird and its tapered feathers belong to the mark. The
        // source PNG's yellow top and lower corners were a square backing
        // plate, and are deliberately absent from this transparent silhouette.
        polygon({ {6, 8}, {70, 39}, {55, 116}, {44, 93},
                  {47, 76}, {37, 76}, {30, 63}, {43, 64}, {40, 57},
                  {27, 55}, {20, 39}, {35, 43}, {33, 37}, {16, 32} });
        // A four-corner head, a five-corner folded wing, and two triangles.
        // Each intended edge is a single straight segment: raster stair-steps
        // and flattened two-pixel tips are not part of the vector geometry.
        polygon({ {57, 14}, {70, 19}, {71, 34}, {59, 30} });
        polygon({ {122, 14}, {100, 90}, {91, 84}, {100, 76}, {88, 42} });
        // The first triangle is an open cutout, not a painted black insert.
        polygon({ {108, 35}, {98, 42}, {103, 54} });
        polygon({ {80, 51}, {91, 74}, {73, 82} });
        return p;
    }();
    return path;
}

namespace detail
{
inline juce::AffineTransform markTransform(juce::Rectangle<float> bounds)
{
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    return juce::AffineTransform::scale(side / 128.0f)
        .translated(bounds.getCentreX() - side * 0.5f,
                    bounds.getCentreY() - side * 0.5f);
}

inline bool usableBounds(juce::Rectangle<float> bounds)
{
    return ! bounds.isEmpty() && std::isfinite(bounds.getX())
        && std::isfinite(bounds.getY()) && std::isfinite(bounds.getWidth())
        && std::isfinite(bounds.getHeight());
}

inline const juce::Path& flameTonguePath()
{
    static const auto path = []
    {
        juce::Path p;
        p.startNewSubPath(-0.46f, 0.0f);
        p.cubicTo(-0.55f, -0.22f, -0.17f, -0.36f, -0.04f, -0.54f);
        p.cubicTo(0.08f, -0.72f, -0.06f, -0.81f, 0.11f, -1.0f);
        p.cubicTo(0.17f, -0.77f, 0.52f, -0.56f, 0.48f, -0.26f);
        p.cubicTo(0.46f, -0.12f, 0.24f, 0.0f, -0.46f, 0.0f);
        p.closeSubPath();
        return p;
    }();
    return path;
}

inline const juce::Path& fireCorePath()
{
    static const auto path = []
    {
        juce::Path p;
        p.startNewSubPath(65.0f, 28.0f);
        p.lineTo(71.0f, 22.0f);
        p.lineTo(71.0f, 63.0f);
        p.cubicTo(71.0f, 79.0f, 84.0f, 97.0f, 105.0f, 110.0f);
        p.lineTo(96.0f, 105.0f);
        p.cubicTo(80.0f, 96.0f, 73.0f, 87.0f, 69.0f, 78.0f);
        p.cubicTo(64.0f, 93.0f, 52.0f, 104.0f, 36.0f, 113.0f);
        p.lineTo(32.0f, 109.0f);
        p.cubicTo(55.0f, 96.0f, 65.0f, 80.0f, 65.0f, 59.0f);
        p.closeSubPath();
        return p;
    }();
    return path;
}

inline void drawFlameTongues(juce::Graphics& g, float energy, float phase, float attack)
{
    struct Tongue { float x, y, width, height, offset; };
    static constexpr std::array<Tongue, 5> tongues {{
        {64.0f, 12.0f, 4.5f, 11.0f, 0.0f},
        {24.0f, 26.0f, 5.3f, 22.0f, 1.7f},
        {103.0f, 25.0f, 5.1f, 22.0f, 3.2f},
        {45.0f, 76.0f, 4.1f, 21.0f, 4.5f},
        {101.0f, 91.0f, 4.0f, 19.0f, 2.2f}
    }};

    for (const auto& tongue : tongues)
    {
        const auto flicker = 0.76f + 0.16f * std::sin(phase * 2.0f + tongue.offset)
                                   + 0.08f * attack;
        const auto height = tongue.height * energy * flicker;
        const auto width = tongue.width * (0.45f + 0.55f * energy);
        const auto sway = 1.6f * energy * std::sin(phase + tongue.offset);
        const auto transform = juce::AffineTransform::scale(width, height)
            .sheared(-sway / juce::jmax(1.0f, height), 0.0f)
            .translated(tongue.x, tongue.y);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xffff6820).withAlpha(0.82f * energy),
            tongue.x + sway, tongue.y - height,
            juce::Colour(0xffffd348).withAlpha(energy), tongue.x, tongue.y, false));
        g.fillPath(flameTonguePath(), transform);
        g.setColour(juce::Colour(0xfffff0bc).withAlpha(energy * 0.66f));
        g.fillPath(flameTonguePath(),
                   juce::AffineTransform::scale(width * 0.28f, height * 0.72f)
                       .sheared(-sway / juce::jmax(1.0f, height), 0.0f)
                       .translated(tongue.x, tongue.y));
    }
}

inline void drawEmbers(juce::Graphics& g, float energy, float phase, float attack)
{
    // Just three short sparks. Their travel stays local to the upper strokes;
    // the character itself never jitters, scales, or loses its silhouette.
    static constexpr std::array<juce::Point<float>, 3> origins {{
        {31.0f, 27.0f}, {92.0f, 25.0f}, {78.0f, 29.0f}
    }};
    const auto strength = juce::jmax(0.0f, energy - 0.25f) / 0.75f;
    if (strength <= 0.0f) return;

    for (size_t i = 0; i < origins.size(); ++i)
    {
        // Integer cycles keep every spark continuous when the caller wraps
        // phase at twoPi. A squared fade reaches zero smoothly at rebirth.
        const auto cycles = i == 1 ? 2.0f : 1.0f;
        auto progress = std::fmod(phase / juce::MathConstants<float>::twoPi * cycles
                                 + static_cast<float>(i) * 0.37f, 1.0f);
        if (progress < 0.0f) progress += 1.0f;
        const auto fade = std::sin(progress * juce::MathConstants<float>::pi);
        const auto life = fade * fade;
        const auto x = origins[i].x + std::sin(phase + static_cast<float>(i))
                                           * progress * 2.5f;
        const auto y = origins[i].y - progress * (13.0f + 6.0f * strength);
        g.setColour(juce::Colour(0xffffe1a1).withAlpha(
            strength * life * (0.52f + 0.25f * attack)));
        g.drawLine(x, y, x + 0.4f, y + 1.7f + strength, 0.95f);
    }
}
} // namespace detail

inline void drawWingsMark(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (! detail::usableBounds(bounds)) return;
    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    g.addTransform(detail::markTransform(bounds));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffdf32), 30.0f, 8.0f,
                                          juce::Colour(0xffffb91e), 92.0f, 116.0f,
                                          false));
    g.fillPath(wingsPath());
}

inline void drawFireMark(juce::Graphics& g, juce::Rectangle<float> bounds,
                         float energy = 0.0f, float phase = 0.0f,
                         float attack = 0.0f)
{
    if (! detail::usableBounds(bounds)) return;
    energy = std::isfinite(energy) ? juce::jlimit(0.0f, 1.0f, energy) : 0.0f;
    attack = std::isfinite(attack) ? juce::jlimit(0.0f, 1.0f, attack) : 0.0f;
    phase = std::isfinite(phase) ? phase : 0.0f;
    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    g.addTransform(detail::markTransform(bounds));
    if (energy > 0.0f) detail::drawFlameTongues(g, energy, phase, attack);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffdc42), 64.0f, 8.0f,
                                          juce::Colour(0xffffbd25).interpolatedWith(
                                              juce::Colour(0xffff7423), energy),
                                          64.0f, 123.0f, false));
    g.fillPath(firePath());

    // Silence is exactly the static SVG, independently of animation phase or
    // residual attack. All moving and white-hot details are energy-gated.
    if (energy <= 0.0f) return;
    {
        const juce::Graphics::ScopedSaveState coreState(g);
        g.reduceClipRegion(firePath());
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xfffff4cb).withAlpha(energy * (0.23f + attack * 0.07f)),
            67.0f, 47.0f, juce::Colour(0xffffd968).withAlpha(energy * 0.08f),
            67.0f, 117.0f, false));
        g.fillPath(detail::fireCorePath());
    }
    detail::drawEmbers(g, energy, phase, attack);
}
} // namespace fire::ui::brand
