#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace fire::ui
{
enum class Icon { none = 0, matrix, assign, points, brush, check, expand, restore };

struct IconGeometry { juce::Path lines, fills; };

inline const IconGeometry& iconGeometry(Icon icon)
{
    // All glyphs share a 24-unit canvas and rounded 1.6-unit strokes.
    // Geometry is constructed once, never on the animation clock.
    static const auto icons = []
    {
        std::array<IconGeometry, 8> result;
        auto& matrix = result[static_cast<size_t>(Icon::matrix)];
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
            {
                const auto rect = juce::Rectangle<float>(4.0f + x * 6.0f, 4.0f + y * 6.0f, 3.5f, 3.5f);
                if (x == y || (x == 2 && y == 0)) matrix.fills.addRoundedRectangle(rect, 0.8f);
                else matrix.lines.addRoundedRectangle(rect, 0.8f);
            }
        auto& assign = result[static_cast<size_t>(Icon::assign)];
        assign.lines.addEllipse(3, 9, 6, 6);
        assign.lines.startNewSubPath(9, 12); assign.lines.lineTo(12, 12);
        assign.lines.lineTo(12, 5); assign.lines.lineTo(20, 5);
        assign.lines.startNewSubPath(12, 12); assign.lines.lineTo(12, 19); assign.lines.lineTo(20, 19);
        for (float y : {5.0f, 19.0f})
        { assign.lines.startNewSubPath(17.5f, y - 2.5f); assign.lines.lineTo(20, y); assign.lines.lineTo(17.5f, y + 2.5f); }
        auto& points = result[static_cast<size_t>(Icon::points)];
        points.lines.startNewSubPath(5.5f, 16); points.lines.lineTo(10.7f, 7.5f);
        points.lines.startNewSubPath(13.7f, 7.2f); points.lines.lineTo(19.0f, 13.5f);
        for (auto p : {juce::Point<float>(4, 18), {12, 5}, {20, 15}})
            points.lines.addEllipse(p.x - 2, p.y - 2, 4, 4);
        auto& brush = result[static_cast<size_t>(Icon::brush)];
        brush.lines.startNewSubPath(10, 12); brush.lines.lineTo(17.5f, 3.5f);
        brush.lines.quadraticTo(19, 2.2f, 20.2f, 3.5f);
        brush.lines.quadraticTo(21.2f, 4.5f, 20, 6); brush.lines.lineTo(12, 14);
        brush.lines.closeSubPath();
        brush.lines.startNewSubPath(8.2f, 13); brush.lines.cubicTo(3, 12.5f, 6.5f, 20.0f, 2.5f, 20.5f);
        brush.lines.cubicTo(8.5f, 22, 13.0f, 18, 11.0f, 15); brush.lines.closeSubPath();
        auto& check = result[static_cast<size_t>(Icon::check)].lines;
        check.startNewSubPath(4, 12); check.lineTo(9.5f, 17.5f); check.lineTo(20, 6.5f);
        auto& expand = result[static_cast<size_t>(Icon::expand)].lines;
        expand.startNewSubPath(4, 10); expand.lineTo(4, 4); expand.lineTo(10, 4);
        expand.startNewSubPath(14, 20); expand.lineTo(20, 20); expand.lineTo(20, 14);
        auto& restore = result[static_cast<size_t>(Icon::restore)].lines;
        restore.startNewSubPath(4, 10); restore.lineTo(10, 10); restore.lineTo(10, 4);
        restore.startNewSubPath(14, 20); restore.lineTo(14, 14); restore.lineTo(20, 14);
        return result;
    }();
    return icons[static_cast<size_t>(juce::jlimit(0, 7, static_cast<int>(icon)))];
}

inline void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds, juce::Colour colour)
{
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    if (side <= 0 || icon == Icon::none) return;
    const auto square = bounds.withSizeKeepingCentre(side, side);
    const auto transform = juce::AffineTransform::scale(side / 24.0f).translated(square.getX(), square.getY());
    const auto& geometry = iconGeometry(icon);
    g.setColour(colour);
    g.fillPath(geometry.fills, transform);
    g.strokePath(geometry.lines, juce::PathStrokeType(juce::jmax(0.8f, 1.6f * side / 24.0f), juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded), transform);
}
} // namespace fire::ui
