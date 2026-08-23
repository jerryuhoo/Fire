/*
  ==============================================================================

    FireTheme.h
    Shared visual language for the Fire editor.

    The theme deliberately keeps expensive effects out of paint callbacks.  The
    helpers below are made from gradients, paths and pixel-aligned strokes that
    are cheap enough for a CPU-rendered plug-in UI and remain crisp on HiDPI
    displays.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace fire::ui
{
namespace colours
{
inline const juce::Colour canvas { 0xff080a0e };
inline const juce::Colour surface0 { 0xff0d1118 };
inline const juce::Colour surface1 { 0xff121824 };
inline const juce::Colour surface2 { 0xff182131 };
inline const juce::Colour raised { 0xff202b3a };
inline const juce::Colour hairline { 0xff2a3545 };
inline const juce::Colour edgeHot { 0xff5a2a22 };

inline const juce::Colour textPrimary { 0xfff3f6fa };
inline const juce::Colour textSecondary { 0xffa8b2c1 };
inline const juce::Colour textMuted { 0xff687386 };
inline const juce::Colour disabled { 0xff414b59 };

inline const juce::Colour ember { 0xffff4d2e };
inline const juce::Colour flame { 0xffff7a1a };
inline const juce::Colour gold { 0xffffc247 };
inline const juce::Colour whiteHot { 0xfffff1d0 };
inline const juce::Colour signalCool { 0xff42d6ff };
inline const juce::Colour modulation { 0xffb968ff };
inline const juce::Colour positive { 0xff59e39b };
inline const juce::Colour warning { 0xffffb23f };
inline const juce::Colour danger { 0xffff3d55 };

inline const juce::Colour drive = ember;
inline const juce::Colour shape = gold;
inline const juce::Colour compressor = positive;
inline const juce::Colour stereo = signalCool;
inline const juce::Colour filter { 0xffff5da8 };
inline const juce::Colour loFi { 0xff9b6cff };
inline const juce::Colour limiter { 0xff7f8cff };
} // namespace colours

struct Metrics
{
    static constexpr float space2 = 2.0f;
    static constexpr float space4 = 4.0f;
    static constexpr float space8 = 8.0f;
    static constexpr float space12 = 12.0f;
    static constexpr float space16 = 16.0f;
    static constexpr float space24 = 24.0f;
    static constexpr float radiusSmall = 4.0f;
    static constexpr float radius = 8.0f;
    static constexpr float radiusLarge = 12.0f;
};

inline juce::PopupMenu::Options prepareContextMenu(
    juce::PopupMenu& menu,
    juce::Component& target,
    juce::Point<int> screenPosition)
{
    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(target)
                       .withTargetScreenArea(
                           juce::Rectangle<int>(screenPosition.x,
                                                screenPosition.y,
                                                1,
                                                1))
                       .withDeletionCheck(target);

    if (auto* topLevel = target.getTopLevelComponent();
        topLevel != nullptr && topLevel != &target)
    {
        // A child MenuWindow inherits the editor theme without borrowing a
        // raw LookAndFeel pointer that could outlive the editor instance.
        menu.setLookAndFeel(nullptr);
        options = options.withParentComponent(topLevel);
    }
    else
    {
        // Desktop menus have no parent from which to inherit a theme. The
        // deletion check above keeps their lifetime tied to the target.
        menu.setLookAndFeel(&target.getLookAndFeel());
    }

    return options;
}

// Critically damped scalar motion for selection rails and short UI
// transitions.  It begins and ends at rest, remains stable across uneven
// message-thread frame times, and allocates nothing while ticking.
struct DampedValue
{
    float current = 0.0f;
    float target = 0.0f;
    float velocity = 0.0f;

    void snapTo(float value) noexcept
    {
        current = target = value;
        velocity = 0.0f;
    }

    void setTarget(float value) noexcept
    {
        target = value;
    }

    bool advance(float deltaSeconds, float responseSeconds = 0.20f) noexcept
    {
        if (! std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
            return ! isSettled();

        deltaSeconds = juce::jlimit(0.0f, 0.05f, deltaSeconds);
        responseSeconds = juce::jmax(0.04f, responseSeconds);

        const auto omega = 2.0f / responseSeconds;
        const auto x = omega * deltaSeconds;
        const auto decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        const auto displacement = current - target;
        const auto impulse = (velocity + omega * displacement) * deltaSeconds;
        const auto previousError = target - current;

        velocity = (velocity - omega * impulse) * decay;
        current = target + (displacement + impulse) * decay;

        // A rapid reversal can carry the old velocity through the new target.
        // Selection frames should settle crisply instead of bouncing past a
        // neighbouring button.
        const auto newError = target - current;
        if (previousError == 0.0f || previousError * newError <= 0.0f)
        {
            current = target;
            velocity = 0.0f;
            return false;
        }

        if (isSettled())
        {
            current = target;
            velocity = 0.0f;
            return false;
        }

        return true;
    }

    bool isSettled(float positionTolerance = 0.001f,
                   float velocityTolerance = 0.01f) const noexcept
    {
        return std::abs(target - current) <= positionTolerance
               && std::abs(velocity) <= velocityTolerance;
    }
};

// Geometry shared by the Drive renderer and its regression tests.  The DSP
// publishes reduction as actual/requested Drive, so the effective point on the
// knob is the requested position multiplied by that ratio.  Treating the
// ratio as an absolute knob position makes the Safe meter over-report Drive.
struct DialArcState
{
    float requestedProportion = 0.0f;
    float effectiveProportion = 0.0f;

    bool hasReduction() const noexcept
    {
        return effectiveProportion + 0.0001f < requestedProportion;
    }
};

inline DialArcState calculateDialArcState(float requestedProportion,
                                          float reductionRatio,
                                          bool isDrive) noexcept
{
    if (! std::isfinite(requestedProportion))
        requestedProportion = 0.0f;
    if (! std::isfinite(reductionRatio))
        reductionRatio = 1.0f;

    DialArcState state;
    state.requestedProportion = juce::jlimit(0.0f, 1.0f, requestedProportion);
    const auto reduction = juce::jlimit(0.0f, 1.0f, reductionRatio);
    state.effectiveProportion = isDrive
                                    ? state.requestedProportion * reduction
                                    : state.requestedProportion;
    return state;
}

inline float dialArcStroke(float radius, float scale, bool isDrive) noexcept
{
    radius = std::isfinite(radius) ? juce::jmax(0.0f, radius) : 0.0f;
    scale = std::isfinite(scale) ? juce::jmax(0.1f, scale) : 1.0f;

    const auto desired = isDrive
                             ? juce::jlimit(5.0f * scale, 12.0f * scale, radius * 0.18f)
                             : juce::jlimit(1.5f, 3.4f * scale, radius * 0.09f);
    return juce::jmin(desired, juce::jmax(1.0f, radius * 0.45f));
}

enum class ModuleRole
{
    neutral,
    drive,
    shape,
    compressor,
    stereo,
    filter,
    loFi,
    limiter,
    modulation
};

inline juce::Colour colourForRole(ModuleRole role)
{
    switch (role)
    {
        case ModuleRole::drive: return colours::drive;
        case ModuleRole::shape: return colours::shape;
        case ModuleRole::compressor: return colours::compressor;
        case ModuleRole::stereo: return colours::stereo;
        case ModuleRole::filter: return colours::filter;
        case ModuleRole::loFi: return colours::loFi;
        case ModuleRole::limiter: return colours::limiter;
        case ModuleRole::modulation: return colours::modulation;
        case ModuleRole::neutral: break;
    }

    return colours::flame;
}

inline juce::Font bodyFont(float height = 13.0f)
{
    return juce::Font { juce::FontOptions().withHeight(height) };
}

inline juce::Font labelFont(float height = 11.0f)
{
    return juce::Font { juce::FontOptions().withHeight(height).withStyle("Bold") };
}

inline juce::Font displayFont(float height = 15.0f)
{
    return juce::Font { juce::FontOptions().withHeight(height).withStyle("Bold") };
}

inline float pixelAligned(float value, float physicalScale = 1.0f)
{
    physicalScale = juce::jmax(1.0f, physicalScale);
    return (std::floor(value * physicalScale) + 0.5f) / physicalScale;
}

inline void drawCanvas(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient gradient(colours::surface1, bounds.getX(), bounds.getY(),
                                  colours::canvas, bounds.getX(), bounds.getBottom(), false);
    gradient.addColour(0.42, colours::surface0);
    g.setGradientFill(gradient);
    g.fillRect(bounds);
}

inline void drawTechGrid(juce::Graphics& g,
                         juce::Rectangle<float> bounds,
                         float spacing = 24.0f,
                         float alpha = 0.10f)
{
    if (bounds.isEmpty() || spacing <= 1.0f)
        return;

    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    g.setColour(colours::hairline.withAlpha(alpha));

    const auto firstX = std::floor(bounds.getX() / spacing) * spacing;
    const auto firstY = std::floor(bounds.getY() / spacing) * spacing;

    for (float x = firstX; x <= bounds.getRight(); x += spacing)
        g.drawVerticalLine(juce::roundToInt(x), bounds.getY(), bounds.getBottom());

    for (float y = firstY; y <= bounds.getBottom(); y += spacing)
        g.drawHorizontalLine(juce::roundToInt(y), bounds.getX(), bounds.getRight());
}

inline void drawPanel(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      juce::Colour accent = juce::Colours::transparentBlack,
                      bool active = false,
                      float radius = Metrics::radius)
{
    if (bounds.isEmpty())
        return;

    bounds = bounds.reduced(0.5f);
    juce::ColourGradient fill(colours::surface1.withAlpha(0.94f), bounds.getX(), bounds.getY(),
                              colours::surface0.withAlpha(0.98f), bounds.getX(), bounds.getBottom(), false);
    fill.addColour(0.38, colours::surface1.darker(0.06f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, radius);

    // A card has one quiet neutral edge.  Module colour belongs to controls
    // and selection state, not to decorative corner rails.
    g.setColour(colours::hairline.withAlpha(active ? 0.48f : 0.30f));
    g.drawRoundedRectangle(bounds, radius, 1.0f);

    juce::ignoreUnused(accent);
}

inline void drawGlassPill(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          juce::Colour accent,
                          bool active,
                          bool hovered,
                          bool pressed)
{
    if (bounds.isEmpty())
        return;

    const auto radius = juce::jmin(bounds.getHeight() * 0.5f, Metrics::radius);
    auto base = active ? colours::raised : colours::surface1;
    if (hovered)
        base = base.brighter(0.06f);
    if (pressed)
        base = base.darker(0.10f);

    juce::ColourGradient fill(base.brighter(0.05f), bounds.getX(), bounds.getY(),
                              base.darker(0.12f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour((active ? accent : colours::hairline).withAlpha(active ? 0.72f : 0.75f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);

    if (active)
    {
        g.setColour(accent.withAlpha(0.95f));
        g.fillRoundedRectangle(bounds.getX() + 1.0f,
                               bounds.getY() + bounds.getHeight() * 0.24f,
                               2.0f,
                               bounds.getHeight() * 0.52f,
                               1.0f);
    }
}

inline void drawSectionTitle(juce::Graphics& g,
                             juce::Rectangle<float> bounds,
                             const juce::String& text,
                             juce::Colour accent)
{
    g.setFont(labelFont(juce::jlimit(9.0f, 13.0f, bounds.getHeight() * 0.36f)));
    g.setColour(colours::textSecondary);
    g.drawText(text.toUpperCase(), bounds.withTrimmedLeft(Metrics::space8), juce::Justification::centredLeft);
    juce::ignoreUnused(accent);
}

inline void drawFireGlyph(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          float energy = 0.5f)
{
    if (bounds.isEmpty())
        return;

    energy = juce::jlimit(0.0f, 1.0f, energy);
    juce::Path flame;
    flame.startNewSubPath(bounds.getCentreX(), bounds.getY());
    flame.cubicTo(bounds.getRight() - bounds.getWidth() * 0.05f,
                  bounds.getY() + bounds.getHeight() * 0.34f,
                  bounds.getRight(),
                  bounds.getBottom() - bounds.getHeight() * 0.20f,
                  bounds.getCentreX(),
                  bounds.getBottom());
    flame.cubicTo(bounds.getX(),
                  bounds.getBottom() - bounds.getHeight() * 0.22f,
                  bounds.getX() + bounds.getWidth() * 0.16f,
                  bounds.getY() + bounds.getHeight() * 0.48f,
                  bounds.getCentreX(),
                  bounds.getY());
    flame.closeSubPath();

    juce::ColourGradient heat(colours::whiteHot.withAlpha(0.92f), bounds.getCentreX(), bounds.getBottom(),
                              colours::ember.withAlpha(0.68f + energy * 0.25f), bounds.getCentreX(), bounds.getY(), false);
    heat.addColour(0.55, colours::flame.withAlpha(0.90f));
    g.setGradientFill(heat);
    g.fillPath(flame);
}
} // namespace fire::ui
