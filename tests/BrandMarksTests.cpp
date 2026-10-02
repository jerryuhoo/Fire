#include <GUI/BrandMarks.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace
{
juce::Image renderFire(float energy, float phase, float attack = 0.0f)
{
    juce::Image image(juce::Image::ARGB, 128, 128, true);
    {
        juce::Graphics graphics(image);
        fire::ui::brand::drawFireMark(graphics, image.getBounds().toFloat(),
                                     energy, phase, attack);
    }
    return image;
}

int differentPixels(const juce::Image& a, const juce::Image& b)
{
    int count = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            count += a.getPixelAt(x, y) != b.getPixelAt(x, y) ? 1 : 0;
    return count;
}

int premultipliedDifference(const juce::Image& a, const juce::Image& b)
{
    int difference = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
        {
            const auto lhs = a.getPixelAt(x, y);
            const auto rhs = b.getPixelAt(x, y);
            const auto lhsAlpha = static_cast<int>(lhs.getAlpha());
            const auto rhsAlpha = static_cast<int>(rhs.getAlpha());
            difference += std::abs(lhsAlpha - rhsAlpha);
            difference += std::abs(lhs.getRed() * lhsAlpha / 255
                                   - rhs.getRed() * rhsAlpha / 255);
            difference += std::abs(lhs.getGreen() * lhsAlpha / 255
                                   - rhs.getGreen() * rhsAlpha / 255);
            difference += std::abs(lhs.getBlue() * lhsAlpha / 255
                                   - rhs.getBlue() * rhsAlpha / 255);
        }
    return difference;
}
} // namespace

TEST_CASE("Brand vectors preserve the supplied characters and transparent gaps",
          "[brand][ui][vector]")
{
    const auto& fireGlyph = fire::ui::brand::firePath();
    CHECK(fireGlyph.contains(65.0f, 38.0f));
    CHECK(fireGlyph.contains(25.0f, 42.0f));
    CHECK(fireGlyph.contains(101.0f, 44.0f));
    CHECK(fireGlyph.contains(30.0f, 106.0f));
    CHECK(fireGlyph.contains(109.0f, 105.0f));
    CHECK_FALSE(fireGlyph.contains(50.0f, 32.0f));
    CHECK_FALSE(fireGlyph.contains(84.0f, 35.0f));
    CHECK_FALSE(fireGlyph.contains(69.0f, 114.0f));

    const auto& wings = fire::ui::brand::wingsPath();
    CHECK(wings.contains(80.0f, 70.0f));
    CHECK(wings.contains(50.0f, 74.0f));
    CHECK(wings.contains(94.0f, 49.0f));
    // The source's square backing must not become part of the bird.
    CHECK_FALSE(wings.contains(82.0f, 16.0f));
    CHECK_FALSE(wings.contains(25.0f, 104.0f));
    CHECK_FALSE(wings.contains(95.0f, 108.0f));
    CHECK_FALSE(wings.contains(103.0f, 44.0f));
    CHECK_FALSE(wings.contains(61.0f, 110.0f));
    CHECK_FALSE(wings.contains(1.0f, 1.0f));
}

TEST_CASE("Silent fire is pixel-identical for every animation phase",
          "[brand][ui][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto silent = renderFire(0.0f, 0.0f);
    for (const auto phase : { -15.0f, 0.4f, 3.0f, 25.0f })
        CHECK(differentPixels(silent, renderFire(0.0f, phase, 1.0f)) == 0);
    CHECK(differentPixels(silent, renderFire(-1.0f, 3.0f, 1.0f)) == 0);
    CHECK(differentPixels(silent,
                          renderFire(std::numeric_limits<float>::quiet_NaN(),
                                     3.0f, 1.0f)) == 0);
}

TEST_CASE("Audio drives local fire details while the solid character stays intact",
          "[brand][ui][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto silent = renderFire(0.0f, 0.0f);
    const auto burning = renderFire(0.85f, 0.7f, 0.4f);
    CHECK(differentPixels(silent, burning) > 100);
    CHECK(differentPixels(burning, renderFire(0.85f, 2.1f, 0.4f)) > 0);
    CHECK(differentPixels(burning, renderFire(0.85f, 0.7f, 1.0f)) > 0);
    int lostSolidPixels = 0;
    for (int y = 0; y < silent.getHeight(); ++y)
        for (int x = 0; x < silent.getWidth(); ++x)
            if (silent.getPixelAt(x, y).getAlpha() == 255)
                lostSolidPixels += burning.getPixelAt(x, y).getAlpha() != 255 ? 1 : 0;
    CHECK(lostSolidPixels == 0);
}

TEST_CASE("Fire animation remains continuous across phase wrap and spark rebirth",
          "[brand][ui][animation][phase]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr auto twoPi = juce::MathConstants<float>::twoPi;
    constexpr auto step = 0.0001f;
    for (const auto centre : {0.0f, twoPi * 0.315f, twoPi * 0.26f})
    {
        auto beforePhase = centre - step;
        if (beforePhase < 0.0f) beforePhase += twoPi;
        const auto before = renderFire(1.0f, beforePhase, 1.0f);
        const auto after = renderFire(1.0f, centre + step, 1.0f);
        // Allow subpixel rasterisation rounding, but less than one opaque
        // pixel's alpha of total change across the entire 128 x 128 mark.
        CHECK(premultipliedDifference(before, after) < 255);
    }
}

TEST_CASE("Small brand marks stay in their bounds and restore the graphics state",
          "[brand][ui][vector][clip]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto width : {24, 36})
    {
        for (const auto drawFire : {false, true})
        {
            juce::Image image(juce::Image::ARGB, 64, 64, true);
            const juce::Rectangle<int> bounds(9, 7, width, 40);
            const auto drawMark = [&](juce::Graphics& graphics)
            {
                if (drawFire)
                    fire::ui::brand::drawFireMark(graphics, bounds.toFloat(),
                                                 1.0f, 1.8f, 1.0f);
                else
                    fire::ui::brand::drawWingsMark(graphics, bounds.toFloat());
            };
            {
                juce::Graphics graphics(image);
                graphics.setColour(juce::Colours::magenta);
                drawMark(graphics);
            }

            // Native Direct2D drawing is committed when Graphics is destroyed;
            // inspect the clipping result only after that drawing session ends.

            int visible = 0;
            int escaped = 0;
            for (int y = 0; y < image.getHeight(); ++y)
                for (int x = 0; x < image.getWidth(); ++x)
                {
                    const auto alpha = image.getPixelAt(x, y).getAlpha();
                    if (bounds.contains(x, y)) visible += alpha > 0 ? 1 : 0;
                    else escaped += alpha != 0 ? 1 : 0;
                }
            CHECK(visible > width * width / (drawFire ? 4 : 5));
            CHECK(escaped == 0);

            {
                juce::Graphics graphics(image);
                graphics.setColour(juce::Colours::magenta);
                drawMark(graphics);
                // The sentinel must use the same context as drawMark to verify
                // that it restored both colour and clipping, not a fresh context.
                graphics.fillRect(0, 0, 2, 2);
            }
            CHECK(image.getPixelAt(0, 0) == juce::Colours::magenta);
        }
    }
}
