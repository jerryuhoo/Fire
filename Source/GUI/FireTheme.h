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
#include "BrandMarks.h"
#include "Skin.h"
#include "VintageMaterials.h"
#include "../Utility/LfoBankParameters.h"
#include "../Utility/ModulationSources.h"
#include <array>
#include <cmath>
#include <initializer_list>

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
inline const juce::Colour textMuted { 0xff8590a1 };
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
inline const juce::Colour ott { 0xff96a7ee };
inline const juce::Colour ottLift { 0xffa3c4ff };
inline const juce::Colour ottPress { 0xffb29ae8 };
inline const juce::Colour chorus { 0xffb9a6dd };
inline const juce::Colour flanger { 0xff91bcb2 };
inline const juce::Colour phaser { 0xffc5a3bc };
inline const juce::Colour chordResonator { 0xffcdbb91 };
inline const juce::Colour delay { 0xff80b8c7 };
inline const juce::Colour reverb { 0xff8e9fce };
inline const juce::Colour granular { 0xffd2ab88 };
inline const juce::Colour stereo = signalCool;
inline const juce::Colour filter { 0xffff5da8 };
inline const juce::Colour loFi { 0xff9b6cff };
inline const juce::Colour limiter { 0xff7f8cff };
} // namespace colours

inline constexpr int lfoBankCount = fire::lfo_bank::capacity;

// Keep every source-specific modulation affordance on the same ordered
// palette. Model/routing indices are zero-based, while the small badges shown
// on controls use the one-based source number exposed by ModulatableSlider.
inline const std::array<juce::Colour, lfoBankCount> lfoBankColours {
    colours::modulation,
    colours::signalCool,
    colours::positive,
    colours::gold,
    juce::Colour {0xffe79590},
    juce::Colour {0xff98a8e8},
    juce::Colour {0xff87cbbb},
    juce::Colour {0xffcfb393},
    juce::Colour {0xffbe9ed1},
    juce::Colour {0xff90b7d0},
    juce::Colour {0xffbbc780},
    juce::Colour {0xffce93b2},
    juce::Colour {0xff81bfba},
    juce::Colour {0xffd8a176},
    juce::Colour {0xff9f9ad0},
    juce::Colour {0xff9db6a0}
};

inline bool isValidLfoBankIndex(int zeroBasedIndex) noexcept
{
    return zeroBasedIndex >= 0 && zeroBasedIndex < lfoBankCount;
}

inline bool isValidLfoSourceNumber(int oneBasedSource) noexcept
{
    return oneBasedSource >= 1 && oneBasedSource <= lfoBankCount;
}

inline juce::Colour lfoBankColour(int zeroBasedIndex) noexcept
{
    if (! isValidLfoBankIndex(zeroBasedIndex))
        return colours::disabled;

    return lfoBankColours[static_cast<size_t>(zeroBasedIndex)];
}

inline juce::Colour lfoBankColourForSource(int oneBasedSource) noexcept
{
    if (! isValidLfoSourceNumber(oneBasedSource))
        return colours::disabled;

    return lfoBankColour(oneBasedSource - 1);
}

inline bool isValidModulationSourceNumber(int source) noexcept
{ return source > 0 && source <= fire::mod_sources::sourceCount; }
inline juce::Colour modulationSourceColour(int index) noexcept
{
    if (index == fire::mod_sources::envelope) return colours::flame;
    if (index >= fire::mod_sources::firstMacro && index < fire::mod_sources::sourceCount)
        return lfoBankColours[static_cast<size_t>(index - fire::mod_sources::firstMacro + 4)];
    return lfoBankColour(index);
}
inline juce::Colour modulationSourceColourForSource(int source) noexcept
{ return isValidModulationSourceNumber(source) ? modulationSourceColour(source - 1) : colours::disabled; }

struct Metrics
{
    static constexpr float knobTitleHeight = 20.0f;
    static constexpr float knobValueHeight = 20.0f;
    static constexpr float knobWidth = 76.0f;
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

// A normal rotary includes its title and value row. Keep the same box in every
// workspace so the shared LookAndFeel also produces the same dial diameter.
inline int ordinaryKnobWidth(float scale, std::initializer_list<int> limits = {}) noexcept
{
    auto width = juce::roundToInt(Metrics::knobWidth * scale);
    for (const auto limit : limits) width = juce::jmin(width, limit);
    return juce::jmax(1, width);
}

inline int ordinaryKnobHeight(int width, float scale) noexcept
{
    return width + juce::roundToInt(Metrics::knobValueHeight * scale);
}

struct Motion
{
    static constexpr float hover = 0.12f;
    static constexpr float press = 0.075f;
    static constexpr float focus = 0.12f;
    static constexpr float disabled = 0.16f;
    static constexpr float selection = 0.08f;
    static constexpr float page = 0.16f;
    static constexpr float readoutIn = 0.075f;
    static constexpr float readoutOut = 0.12f;

    // Reach 95% within the specified duration, independent of timer jitter.
    static float step(float deltaSeconds, float duration) noexcept
    {
        return 1.0f - std::exp(-3.0f * juce::jlimit(0.0f, 0.05f, deltaSeconds)
                              / juce::jmax(0.01f, duration));
    }
};

inline juce::PopupMenu::Options prepareContextMenu(
    juce::PopupMenu& menu,
    juce::Component& target,
    juce::Point<int> screenPosition)
{
    // JUCE chooses the MenuWindow opacity, peer flags and initial metrics
    // before attaching it to an optional parent component. Bind the target
    // theme up front so those decisions cannot fall back to the stock UI.
    menu.setLookAndFeel(&target.getLookAndFeel());

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
        options = options.withParentComponent(topLevel);

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

// A lightly underdamped spring for selection surfaces only. Parameter values
// continue to follow the pointer directly; the animated surface is not a hit target.
struct SpringValue
{
    float current = 0.0f;
    float target = 0.0f;
    float velocity = 0.0f;

    void snapTo(float value) noexcept { current = target = value; velocity = 0.0f; }
    void setTarget(float value) noexcept { target = value; }
    bool isSettled() const noexcept
    {
        return std::abs(target - current) < 0.001f && std::abs(velocity) < 0.01f;
    }
    bool advance(float deltaSeconds) noexcept
    {
        if (! std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
            return ! isSettled();
        const auto dt = juce::jmin(0.05f, deltaSeconds);
        constexpr float omega = juce::MathConstants<float>::twoPi / 0.30f;
        constexpr float damping = 0.68f;
        const auto dampedOmega = omega * std::sqrt(1.0f - damping * damping);
        const auto displacement = current - target;
        const auto b = (velocity + damping * omega * displacement) / dampedOmega;
        const auto decay = std::exp(-damping * omega * dt);
        const auto sine = std::sin(dampedOmega * dt);
        const auto cosine = std::cos(dampedOmega * dt);
        current = target + decay * (displacement * cosine + b * sine);
        velocity = decay * ((b * dampedOmega - damping * omega * displacement) * cosine
                            - (displacement * dampedOmega + damping * omega * b) * sine);
        if (isSettled())
        {
            snapTo(target);
            return false;
        }
        return true;
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
                             ? juce::jlimit(4.0f * scale, 10.0f * scale, radius * 0.145f)
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
    modulation,
    ott
};

inline juce::Colour colourForRole(ModuleRole role)
{
    switch (role)
    {
        case ModuleRole::drive: return colours::drive;
        case ModuleRole::shape: return colours::shape;
        case ModuleRole::compressor: return colours::compressor;
        case ModuleRole::ott: return colours::ott;
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

inline int moduleRailWidth(float scale, int panelWidth)
{
    const auto proportional = juce::roundToInt(juce::jlimit(120.0f * scale,
        165.0f * scale, static_cast<float>(panelWidth) * .14f));
    // Reserve the card/viewport insets, row margins, power and remove controls
    // around the longest displayed module name, at the row font's maximum size.
    const auto label = juce::GlyphArrangement::getStringWidth(labelFont(15 * scale), "Compressor");
    return juce::jmax(proportional, static_cast<int>(std::ceil(label + 98 * scale)));
}

inline juce::Font displayFont(float height = 15.0f)
{
    return juce::Font { juce::FontOptions().withHeight(height).withStyle("Bold") };
}

inline juce::Font valueFont(float height = 12.0f)
{
    return juce::Font { juce::FontOptions()
                           .withName(juce::Font::getDefaultMonospacedFontName())
                           .withHeight(height) };
}

inline float pixelAligned(float value, float physicalScale = 1.0f)
{
    physicalScale = juce::jmax(1.0f, physicalScale);
    return (std::floor(value * physicalScale) + 0.5f) / physicalScale;
}

inline void drawCanvas(juce::Graphics& g, juce::Rectangle<float> bounds, Skin skin = Skin::modern)
{
    const auto& palette = skinPalette(skin);
    if (skin == Skin::vintage)
    {
        // A quiet painted-metal desk; the editor owns the separate walnut
        // cheeks and rails, so the body stays a consistent warm neutral.
        g.setGradientFill(juce::ColourGradient(palette.surface0.brighter(0.018f), bounds.getTopLeft(),
            palette.canvas, bounds.getBottomLeft(), false));
        g.fillRect(bounds);
        return;
    }
    juce::ColourGradient gradient(palette.surface1, bounds.getX(), bounds.getY(),
                                  palette.canvas, bounds.getX(), bounds.getBottom(), false);
    gradient.addColour(0.42, palette.surface0);
    g.setGradientFill(gradient);
    g.fillRect(bounds);
}

inline void drawTechGrid(juce::Graphics& g,
                         juce::Rectangle<float> bounds,
                         float spacing = 24.0f,
                         float alpha = 0.10f,
                         Skin skin = Skin::modern)
{
    if (bounds.isEmpty() || spacing <= 1.0f)
        return;

    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    g.setColour(skinPalette(skin).hairline.withAlpha(alpha));

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
    g.setColour(colours::surface1);
    g.fillRoundedRectangle(bounds, radius);
    juce::ignoreUnused(active);

    juce::ignoreUnused(accent);
}

inline void drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds,
                      juce::Colour accent, bool active, Skin skin)
{
    if (skin == Skin::modern) {drawPanel(g, bounds, accent, active); return;}
    if (bounds.isEmpty()) return;
    const auto& palette = skinPalette(skin);
    const auto area = bounds.reduced(.5f);
    juce::ColourGradient metal(palette.surface1.brighter(0.045f), area.getTopLeft(),
                               palette.surface0, area.getBottomLeft(), false);
    metal.addColour(0.42, palette.surface1);
    g.setGradientFill(metal);
    g.fillRoundedRectangle(area, Metrics::radius);
    g.setColour(palette.canvas.darker(0.17f).withAlpha(.82f));
    g.drawRoundedRectangle(area, Metrics::radius, 1);
    // One restrained upper lip reads as folded metal, without a white frame.
    const auto inset = area.reduced(1.5f);
    g.setColour(palette.textPrimary.withAlpha(.095f));
    g.drawLine(inset.getX() + Metrics::radius, inset.getY(),
               inset.getRight() - Metrics::radius, inset.getY(), .8f);
    g.setColour(palette.canvas.darker(.20f).withAlpha(.45f));
    g.drawLine(inset.getX() + Metrics::radius, inset.getBottom(),
               inset.getRight() - Metrics::radius, inset.getBottom(), .8f);
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

    g.setColour(base);
    g.fillRoundedRectangle(bounds, radius);
    juce::ignoreUnused(accent);
}

inline void drawSectionTitle(juce::Graphics& g,
                             juce::Rectangle<float> bounds,
                             const juce::String& text,
                             juce::Colour accent)
{
    g.setFont(labelFont(juce::jlimit(10.0f, 22.0f, bounds.getHeight() * 0.46f)));
    g.setColour(colours::textSecondary);
    g.drawText(text.toUpperCase(), bounds.withTrimmedLeft(Metrics::space8), juce::Justification::centredLeft);
    juce::ignoreUnused(accent);
}

inline void drawFireGlyph(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          float energy = 0.0f,
                          float phase = 0.0f,
                          float attack = 0.0f)
{
    brand::drawFireMark(g, bounds, energy, phase, attack);
}
} // namespace fire::ui
