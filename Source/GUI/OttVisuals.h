#pragma once
#include "FireTheme.h"

namespace fire::ui
{
using OttSpectrumProfile = std::array<float, 24>;

inline bool advanceReadout(float& opacity, bool visible, float dt) noexcept
{
    const auto previous = opacity;
    const auto target = visible ? 1.0f : 0.0f;
    opacity += (target - opacity) * Motion::step(dt, visible ? Motion::readoutIn : Motion::readoutOut);
    if (std::abs(target - opacity) < 0.002f) opacity = target;
    return ! juce::approximatelyEqual(previous, opacity);
}

struct OttRippleMotion
{
    float lift = 0.0f, press = 0.0f, phase = 0.0f;
    float liftPreview = 0.0f, pressPreview = 0.0f;
    void reset() noexcept { lift = press = phase = liftPreview = pressPreview = 0.0f; }
    void advance(float dt, float activityDb, bool previewLift, bool previewPress, float centroid) noexcept
    {
        activityDb = std::isfinite(activityDb) ? activityDb : 0.0f;
        dt = std::isfinite(dt) ? juce::jlimit(0.0f, 0.05f, dt) : 0.0f;
        centroid = std::isfinite(centroid) ? juce::jlimit(0.0f, 1.0f, centroid) : 0.5f;
        advanceReadout(liftPreview, previewLift, dt);
        advanceReadout(pressPreview, previewPress, dt);
        const auto amount = std::sqrt(juce::jlimit(0.0f, 1.0f, std::abs(activityDb) / 18.0f));
        const auto liftTarget = juce::jmax(activityDb > 0.01f ? amount : 0.0f, previewLift ? 0.55f : 0.0f);
        const auto pressTarget = juce::jmax(activityDb < -0.01f ? amount : 0.0f, previewPress ? 0.55f : 0.0f);
        lift += (liftTarget - lift) * Motion::step(dt, liftTarget > lift ? 0.06f : 0.18f);
        press += (pressTarget - press) * Motion::step(dt, pressTarget > press ? 0.06f : 0.18f);
        if (lift < 0.002f && liftTarget == 0.0f) lift = 0.0f;
        if (press < 0.002f && pressTarget == 0.0f) press = 0.0f;
        if (lift + press > 0.002f)
            phase = std::fmod(phase + juce::jlimit(0.0f, 0.05f, dt)
                             * (0.75f + 1.05f * juce::jlimit(0.0f, 1.0f, centroid)), 1.0f);
    }
};

// Fixed-size profiles follow the logarithmic spectrum coordinates. Colour and
// opacity vary across the ribbon with spectral energy, rather than colouring
// the entire band equally or emitting unrelated decorative particles.
inline void drawOttRipples(juce::Graphics& g, juce::Rectangle<float> bounds,
                          float originY, const OttSpectrumProfile& spectrum,
                          float amount, bool downward, float phase, float scale,
                          float preview = 0.0f)
{
    if (amount < 0.003f || bounds.isEmpty()) return;
    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    const auto colour = downward ? colours::ottPress : colours::ottLift;
    const auto direction = downward ? 1.0f : -1.0f;
    const auto travel = juce::jmin(bounds.getHeight() * 0.38f, 42.0f * scale);
    for (int wave = 0; wave < 3; ++wave)
    {
        const auto progress = std::fmod(phase + static_cast<float>(wave) / 3.0f, 1.0f);
        const auto fade = amount * (1.0f - progress) * (1.0f - progress);
        juce::Path ribbon, crest;
        juce::ColourGradient fill(juce::Colours::transparentBlack, bounds.getX(), 0,
                                  juce::Colours::transparentBlack, bounds.getRight(), 0, false);
        juce::ColourGradient edge = fill;
        ribbon.startNewSubPath(bounds.getX(), originY);
        for (size_t i = 0; i < spectrum.size(); ++i)
        {
            const auto t = static_cast<float>(i) / static_cast<float>(spectrum.size() - 1);
            const auto energy = std::sqrt(juce::jmax(juce::jlimit(0.0f, 1.0f, spectrum[i]),
                                           preview * (0.18f + 0.18f * std::sin(t * juce::MathConstants<float>::pi))));
            const auto x = bounds.getX() + t * bounds.getWidth();
            const auto y = originY + direction * (3.0f * scale + travel * progress)
                                    * (0.24f + 0.76f * energy);
            const auto ink = colour.withMultipliedSaturation(0.30f + energy * 1.25f);
            const auto fillColour = ink.withAlpha(juce::jlimit(0.0f, 1.0f, fade * energy * 0.42f));
            const auto edgeColour = ink.withAlpha(juce::jlimit(0.0f, 1.0f, fade * energy * 0.85f));
            if (i + 1 == spectrum.size())
            {
                fill.setColour(fill.getNumColours() - 1, fillColour);
                edge.setColour(edge.getNumColours() - 1, edgeColour);
            }
            else
            {
                fill.addColour(t, fillColour);
                edge.addColour(t, edgeColour);
            }
            ribbon.lineTo(x, y);
            if (i == 0) crest.startNewSubPath(x, y); else crest.lineTo(x, y);
        }
        ribbon.lineTo(bounds.getRight(), originY);
        ribbon.closeSubPath();
        g.setGradientFill(fill);
        g.fillPath(ribbon);
        g.setGradientFill(edge);
        g.strokePath(crest, juce::PathStrokeType(1.15f * scale,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
}
