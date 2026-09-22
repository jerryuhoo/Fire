#include <GUI/LookAndFeel.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;

struct DialFixture
{
    FireLookAndFeel lookAndFeel;
    ModulatableSlider slider;

    explicit DialFixture(int size = 120, float uiScale = 1.0f)
    {
        lookAndFeel.scale = uiScale;
        slider.setLookAndFeel(&lookAndFeel);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters(startAngle, endAngle, true);
        slider.setRange(0.0, 1.0);
        slider.setValue(0.5, juce::dontSendNotification);
        slider.setLabel("POSITION", fire::ui::colours::textSecondary);
        slider.setBounds(0, 0, size, size);
        slider.isModulated = true;
        slider.lfoSource = 1;
        slider.lfoAmount = 0.72;
        slider.lfoValue = 0.38;
    }

    ~DialFixture() { slider.setLookAndFeel(nullptr); }

    juce::Image render(bool entireComponent = false, int deviceScale = 1)
    {
        juce::Image image(juce::Image::ARGB, slider.getWidth() * deviceScale,
                          slider.getHeight() * deviceScale, true);
        juce::Graphics graphics(image);
        graphics.fillAll(fire::ui::colours::canvas);
        graphics.addTransform(juce::AffineTransform::scale(static_cast<float>(deviceScale)));
        if (entireComponent)
            slider.paintEntireComponent(graphics, true);
        else
            lookAndFeel.drawRotarySlider(graphics, 0, 0, slider.getWidth(), slider.getHeight(),
                                        0.5f, startAngle, endAngle, slider);
        return image;
    }
};

float brightness(juce::Colour colour)
{
    // Display RGB brightness is sufficient here: test the visible hierarchy,
    // without prescribing exact palette colours or anti-aliasing coverage.
    return 0.2126f * colour.getFloatRed() + 0.7152f * colour.getFloatGreen()
           + 0.0722f * colour.getFloatBlue();
}

float peakBrightness(const juce::Image& image, juce::Point<float> centre, float radius)
{
    const auto area = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f)
                          .withCentre(centre).getSmallestIntegerContainer()
                          .getIntersection(image.getBounds());
    float peak = 0.0f;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
            peak = juce::jmax(peak, brightness(image.getPixelAt(x, y)));
    return peak;
}

juce::Point<float> ringPoint(double proportion)
{
    // A broad sample patch around the inner ring of a 120 px dial. The tests
    // deliberately allow subpixel geometry changes instead of matching a PNG.
    const auto angle = startAngle + static_cast<float>(proportion) * (endAngle - startAngle);
    return {60.0f + 35.0f * std::sin(angle), 60.0f - 35.0f * std::cos(angle)};
}

struct ModulationCase
{
    const char* label;
    double amount;
    double value;
    bool bipolar;
    bool bypassed = false;
    bool enabled = true;
};

constexpr std::array<ModulationCase, 8> specimenCases {{
    {"Depth 0", 0.0, 0.38, true},
    {"Bipolar +", 0.72, 0.38, true},
    {"Bipolar -", -0.72, 0.38, true},
    {"Unipolar +", 0.36, 0.75, false},
    {"Unipolar -", -0.36, 0.75, false},
    {"Live = base", 0.72, 0.0, true},
    {"Bypassed", 0.72, 0.38, true, true},
    {"Disabled", 0.72, 0.38, true, false, false}
}};

void configure(DialFixture& fixture, int bank, const ModulationCase& setting)
{
    fixture.slider.lfoSource = bank;
    fixture.slider.lfoAmount = setting.amount;
    fixture.slider.lfoValue = setting.value;
    fixture.slider.isBipolar = setting.bipolar;
    fixture.slider.isBypassed = setting.bypassed;
    fixture.slider.setEnabled(setting.enabled);
}

void writeSpecimen(const juce::File& directory, int deviceScale)
{
    constexpr int width = 1080;
    constexpr int height = 800;
    juce::Image image(juce::Image::ARGB, width * deviceScale, height * deviceScale, true);
    juce::Graphics graphics(image);
    graphics.fillAll(fire::ui::colours::canvas);
    graphics.addTransform(juce::AffineTransform::scale(static_cast<float>(deviceScale)));
    graphics.setColour(fire::ui::colours::textPrimary);
    graphics.setFont(fire::ui::labelFont(18.0f));
    graphics.drawText("LFO range / live position / base origin", 20, 12, 1000, 28,
                      juce::Justification::centredLeft);
    graphics.setFont(fire::ui::bodyFont(12.0f));
    graphics.setColour(fire::ui::colours::textSecondary);
    graphics.drawText("Muted bank-colour range, bright outlined live dot, neutral inner origin mark",
                      20, 42, 1040, 20, juce::Justification::centredLeft);

    for (int bank = 1; bank <= 4; ++bank)
    {
        graphics.setColour(fire::ui::lfoBankColourForSource(bank));
        graphics.drawText("LFO " + juce::String(bank), 8, 110 + (bank - 1) * 128,
                          65, 20, juce::Justification::centred);
        for (size_t column = 0; column < specimenCases.size(); ++column)
        {
            const int x = 72 + static_cast<int>(column) * 125;
            const int y = 70 + (bank - 1) * 128;
            DialFixture fixture(108);
            configure(fixture, bank, specimenCases[column]);
            graphics.setColour(fire::ui::colours::textSecondary);
            graphics.setFont(fire::ui::bodyFont(11.0f));
            graphics.drawText(specimenCases[column].label, x, y, 120, 18,
                              juce::Justification::centred);
            juce::Graphics::ScopedSaveState state(graphics);
            graphics.addTransform(juce::AffineTransform::translation(
                static_cast<float>(x + 6), static_cast<float>(y + 18)));
            fixture.slider.paintEntireComponent(graphics, true);
        }
    }

    graphics.setColour(fire::ui::colours::textPrimary);
    graphics.setFont(fire::ui::labelFont(13.0f));
    graphics.drawText("Small controls / UI zoom (atlas rasterised at " + juce::String(deviceScale) + "x)",
                      20, 585, 1040, 22, juce::Justification::centredLeft);
    constexpr std::array<int, 4> sizes {{56, 72, 126, 168}};
    constexpr std::array<float, 4> scales {{0.75f, 1.0f, 1.5f, 2.0f}};
    for (size_t index = 0; index < sizes.size(); ++index)
    {
        const int x = 28 + static_cast<int>(index) * 260;
        DialFixture fixture(sizes[index], scales[index]);
        configure(fixture, static_cast<int>(index) + 1, specimenCases[5]);
        graphics.setColour(fire::ui::colours::textSecondary);
        graphics.setFont(fire::ui::bodyFont(12.0f));
        graphics.drawText(juce::String(sizes[index]) + " px / UI " + juce::String(scales[index], 2) + "x",
                          x, 612, 240, 20, juce::Justification::centred);
        juce::Graphics::ScopedSaveState state(graphics);
        graphics.addTransform(juce::AffineTransform::translation(
            static_cast<float>(x + (240 - sizes[index]) / 2), 630.0f));
        fixture.slider.paintEntireComponent(graphics, true);
    }

    const auto file = directory.getChildFile("modulation-contrast-" + juce::String(deviceScale) + "x.png");
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(image, *stream));
}

struct PointerPixels
{
    int segments = 0;
    int width = 0;
    int length = 0;
};

PointerPixels measureUprightPointer(const juce::Image& image)
{
    // With a midpoint value the pointer is upright. Inspect the inner face,
    // excluding labels and the value arc, and count separate bright marks.
    const int centreX = image.getWidth() / 2;
    const int firstY = juce::roundToInt(image.getHeight() * 0.14f);
    const int lastY = image.getHeight() / 2;
    PointerPixels result;
    int start = -1, longestStart = 0;
    for (int y = firstY; y <= lastY; ++y)
    {
        const bool bright = y < lastY && brightness(image.getPixelAt(centreX, y)) > 0.65f;
        if (bright && start < 0) { start = y; ++result.segments; }
        if (! bright && start >= 0)
        {
            if (y - start > result.length) { result.length = y - start; longestStart = start; }
            start = -1;
        }
    }
    const int middleY = longestStart + result.length / 2;
    for (int x = centreX - image.getWidth() / 8; x <= centreX + image.getWidth() / 8; ++x)
        if (brightness(image.getPixelAt(x, middleY)) > 0.65f) ++result.width;
    return result;
}

bool samePixels(const juce::Image& first, const juce::Image& second)
{
    if (first.getBounds() != second.getBounds()) return false;
    for (int y = 0; y < first.getHeight(); ++y)
        for (int x = 0; x < first.getWidth(); ++x)
            if (first.getPixelAt(x, y) != second.getPixelAt(x, y)) return false;
    return true;
}

void writeDriveSpecimen(const juce::File& directory, int deviceScale)
{
    constexpr int width = 1040, height = 800;
    juce::Image image(juce::Image::ARGB, width * deviceScale, height * deviceScale, true);
    juce::Graphics graphics(image);
    graphics.fillAll(fire::ui::colours::canvas);
    graphics.addTransform(juce::AffineTransform::scale(static_cast<float>(deviceScale)));
    graphics.setColour(fire::ui::colours::textPrimary);
    graphics.setFont(fire::ui::labelFont(18.0f));
    graphics.drawText("Drive / one proportional pointer", 20, 12, 1000, 28, juce::Justification::centredLeft);
    graphics.setFont(fire::ui::bodyFont(12.0f));
    graphics.setColour(fire::ui::colours::textSecondary);
    graphics.drawText("Top: no LFO. Bottom: assigned LFO with a range arc and source badge.",
                      20, 43, 1000, 22, juce::Justification::centredLeft);
    constexpr std::array<int, 3> sizes {160, 240, 320};
    for (int row = 0; row < 2; ++row)
        for (size_t column = 0; column < sizes.size(); ++column)
        {
            const auto size = sizes[column];
            const auto uiScale = static_cast<float>(size) / 160.0f;
            DialFixture fixture(size, uiScale);
            fixture.slider.setComponentID("drive");
            fixture.slider.setLabel("DRIVE", fire::ui::colours::drive);
            fixture.slider.setColour(juce::Slider::rotarySliderFillColourId, fire::ui::colours::drive);
            fixture.slider.isModulated = row != 0;
            const int x = 20 + static_cast<int>(column) * 340;
            const int y = 80 + row * 350;
            graphics.setColour(fire::ui::colours::textSecondary);
            graphics.setFont(fire::ui::bodyFont(12.0f));
            graphics.drawText(juce::String(size) + " px / UI " + juce::String(uiScale, 1) + "x",
                              x, y, 320, 20, juce::Justification::centred);
            juce::Graphics::ScopedSaveState state(graphics);
            graphics.addTransform(juce::AffineTransform::translation(
                static_cast<float>(x + (320 - size) / 2), static_cast<float>(y + 24 + (320 - size) / 2)));
            fixture.slider.paintEntireComponent(graphics, true);
        }
    auto stream = directory.getChildFile("drive-pointer-" + juce::String(deviceScale) + "x.png").createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(image, *stream));
}
} // namespace

TEST_CASE("Drive has one bright pointer with or without LFO and its width follows the dial size",
          "[drive-pointer][modulation-contrast][ui][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::vector<PointerPixels> measurements;
    // Keep UI scale fixed while changing the dial, so a fixed stroke multiplied
    // only by UI scale cannot satisfy the proportional-width check.
    for (int size : {160, 240, 320})
    {
        CAPTURE(size);
        DialFixture fixture(size);
        fixture.slider.setComponentID("drive");
        fixture.slider.isModulated = false;
        const auto plain = measureUprightPointer(fixture.render(false, 2));
        REQUIRE(plain.segments == 1);
        REQUIRE(plain.width > 0);
        REQUIRE(plain.length > 0);
        measurements.push_back(plain);
        fixture.slider.isModulated = true;
        fixture.slider.lfoValue = 0.0;
        const auto coincidentImage = fixture.render(false, 2);
        const auto assigned = measureUprightPointer(coincidentImage);
        CHECK(assigned.segments == 1);
        CHECK(assigned.width == plain.width);
        CHECK(assigned.length == plain.length);
        fixture.slider.lfoValue = 0.6;
        CHECK(samePixels(coincidentImage, fixture.render(false, 2)));
    }
    const auto& small = measurements.front();
    const auto& large = measurements.back();
    CHECK(large.width >= small.width * 1.8f);
    CHECK(large.width <= small.width * 2.4f);
    CHECK(large.length >= small.length * 1.8f);
    CHECK(large.length <= small.length * 2.4f);
}

TEST_CASE("Drive pointer specimen covers plain and modulated dials at multiple scales",
          "[drive-pointer][modulation-specimen][ui][render]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable("FIRE_MODULATION_SNAPSHOT_DIR", {});
    if (path.isEmpty()) return;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File directory(path);
    REQUIRE(directory.createDirectory().wasOk());
    writeDriveSpecimen(directory, 1);
    writeDriveSpecimen(directory, 2);
}

TEST_CASE("LFO range remains subordinate to live position and base origin for every bank",
          "[modulation-contrast][modulation][ui][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DialFixture fixture;
    for (int bank = 1; bank <= fire::ui::lfoBankCount; ++bank)
        for (size_t settingIndex = 1; settingIndex <= 4; ++settingIndex)
        {
            const auto& setting = specimenCases[settingIndex];
            configure(fixture, bank, setting);
            const auto image = fixture.render();
            const auto current = 0.5 + setting.amount * setting.value * (setting.bipolar ? 0.5 : 1.0);
            const auto rangeSample = setting.bipolar
                                         ? (setting.amount > 0.0 ? 0.38 : 0.62)
                                         : (setting.amount > 0.0 ? 0.62 : 0.38);
            const auto live = peakBrightness(image, ringPoint(current), 2.5f);
            const auto range = peakBrightness(image, ringPoint(rangeSample), 2.5f);
            const auto origin = peakBrightness(image, {60.0f, 30.0f}, 2.5f);
            CAPTURE(bank, settingIndex, live, range, origin);
            CHECK(range > 0.12f);
            CHECK(range < 0.45f);
            CHECK(live > 0.65f);
            CHECK(live > range + 0.30f);
            CHECK(origin > 0.60f);
            CHECK(origin > range + 0.25f);
        }
}

TEST_CASE("Coincident base and live positions remain visible while bypass is subdued",
          "[modulation-contrast][modulation][ui][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DialFixture fixture;
    for (int bank = 1; bank <= fire::ui::lfoBankCount; ++bank)
    {
        configure(fixture, bank, specimenCases[5]);
        const auto coincident = fixture.render();
        CAPTURE(bank);
        CHECK(peakBrightness(coincident, {60.0f, 25.0f}, 1.5f) > 0.65f);
        CHECK(peakBrightness(coincident, {60.0f, 30.0f}, 1.5f) > 0.60f);

        configure(fixture, bank, specimenCases[1]);
        const auto active = fixture.render();
        const auto livePoint = ringPoint(0.5 + 0.72 * 0.38 * 0.5);
        fixture.slider.isBypassed = true;
        const auto bypassed = fixture.render();
        CHECK(peakBrightness(active, livePoint, 2.5f)
              > peakBrightness(bypassed, livePoint, 2.5f) + 0.30f);
        fixture.slider.isBypassed = false;
        fixture.slider.setEnabled(false);
        const auto disabled = fixture.render();
        CHECK(peakBrightness(active, livePoint, 2.5f)
              > peakBrightness(disabled, livePoint, 2.5f) + 0.30f);
    }
}

TEST_CASE("Modulation painting stays inside the dial and badge at small sizes and HiDPI",
          "[modulation-contrast][modulation][ui][render][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr std::array<int, 4> sizes {{40, 56, 84, 112}};
    constexpr std::array<float, 4> scales {{0.75f, 1.0f, 1.5f, 2.0f}};
    for (size_t index = 0; index < sizes.size(); ++index)
        for (int deviceScale : {1, 2})
        {
            DialFixture fixture(sizes[index], scales[index]);
            const auto hitBounds = fixture.slider.getModulationHandleHitBounds();
            const auto valueBounds = fixture.slider.getValueDisplayBounds();
            fixture.slider.isModulated = false;
            const auto reference = fixture.render(true, deviceScale);
            const auto bounds = fixture.lookAndFeel.getSliderLayout(fixture.slider).sliderBounds.toFloat();
            const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f
                                - juce::jmax(5.0f, 7.0f * scales[index]);
            // The inner 75% of the dial contains the overlay. Allow one
            // logical pixel for antialiasing, without allowing the value arc.
            const auto allowedRadius = juce::jmax(0.0f, radius) * 0.75f + 1.0f;
            const auto badge = fixture.slider.getModulationHandleVisualBounds()
                                   .expanded(1.0f + scales[index]);
            for (double depth : {0.0, -1.0, 1.0})
            {
                fixture.slider.isModulated = true;
                fixture.slider.lfoAmount = depth;
                fixture.slider.lfoValue = 0.0;
                const auto image = fixture.render(true, deviceScale);
                int unexpectedPixels = 0;
                for (int y = 0; y < image.getHeight(); ++y)
                    for (int x = 0; x < image.getWidth(); ++x)
                    {
                        if (image.getPixelAt(x, y) == reference.getPixelAt(x, y)) continue;
                        const juce::Point<float> point {
                            (static_cast<float>(x) + 0.5f) / static_cast<float>(deviceScale),
                            (static_cast<float>(y) + 0.5f) / static_cast<float>(deviceScale)
                        };
                        if (! badge.contains(point)
                            && point.getDistanceFrom(bounds.getCentre()) > allowedRadius)
                            ++unexpectedPixels;
                    }
                CAPTURE(index, deviceScale, depth, unexpectedPixels);
                CHECK(unexpectedPixels == 0);
                CHECK(fixture.slider.getModulationHandleHitBounds() == hitBounds);
                CHECK(fixture.slider.getValueDisplayBounds() == valueBounds);
                CHECK(fixture.slider.getValue() == 0.5);
                CHECK(fixture.slider.lfoAmount == depth);
            }
        }
}

TEST_CASE("LFO contrast specimen covers banks polarity bypass and scale",
          "[modulation-contrast][modulation-specimen][ui][render]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable("FIRE_MODULATION_SNAPSHOT_DIR", {});
    if (path.isEmpty()) return;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File directory(path);
    REQUIRE(directory.createDirectory().wasOk());
    writeSpecimen(directory, 1);
    writeSpecimen(directory, 2);
}
