#pragma once

#include "FireTheme.h"
#include "../DSP/AnalogDistortion.h"
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <limits>

namespace fire::ui
{
// A signal-driven faceplate. The photographic layers stay cached; only the
// filament exposure and tape transport change during visible UI frames.
class HardwareColourPanel final : public juce::Component
{
public:
    HardwareColourPanel()
    {
        setInterceptsMouseClicks(false, false);
        setTitle("Analog hardware display");
        setDescription("Tube filament brightness follows Drive and signal energy. Tape reels follow the audio transport.");
    }
    void setState(int model, float drive, float peak, float dt = 1.0f / 60.0f, std::uint64_t sequence = std::numeric_limits<std::uint64_t>::max())
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (sequence != previousSequence) {previousSequence = sequence; lastFreshTick = now;}
        if (sequence != std::numeric_limits<std::uint64_t>::max() && now - lastFreshTick > 180u) peak = 0;
        model = juce::jlimit(0, analog::count - 1, model);
        const auto driveAmount = juce::jlimit(0.0f, 1.0f, std::isfinite(drive) ? drive * .01f : 0.0f);
        const auto input = juce::jlimit(0.0f, 1.0f, std::isfinite(peak) ? peak : 0.0f);
        const auto target = .10f + .65f * std::sqrt(driveAmount) + .25f * std::sqrt(input);
        const auto time = juce::jlimit(0.0f, .1f, dt);
        light += (target - light) * (1 - std::exp(-time * 7));
        const auto running = input > .0001f ? 1.0f : 0.0f;
        transport += (running - transport) * (1 - std::exp(-time * 5));
        phase = std::fmod(phase + time * transport * (1.5f + driveAmount * .5f), juce::MathConstants<float>::twoPi);
        const bool changed = currentModel != model || std::abs(light - paintedLight) > .003f || (model == 11 && transport > .001f);
        currentModel = model;
        if (changed && isShowing()) {paintedLight = light; repaint();}
    }
    float getFilamentBrightness() const noexcept {return light;}
    float getTransportPhase() const noexcept {return phase;}
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(4);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff282420), area.getX(), area.getY(), juce::Colour(0xff101317), area.getRight(), area.getBottom(), false));
        g.fillRoundedRectangle(area, 10);
        g.setColour(juce::Colour(0xff74634a).withAlpha(.32f)); g.drawRoundedRectangle(area.reduced(.5f), 10, 1);
        g.setColour(juce::Colours::white.withAlpha(.022f));
        for (float y = area.getY() + 2; y < area.getBottom(); y += 3) g.drawHorizontalLine(juce::roundToInt(y), area.getX() + 8, area.getRight() - 8);
        for (auto point : {area.getTopLeft(), area.getTopRight(), area.getBottomLeft(), area.getBottomRight()})
        {
            const auto x = point.x < area.getCentreX() ? point.x + 10 : point.x - 10;
            const auto y = point.y < area.getCentreY() ? point.y + 10 : point.y - 10;
            g.setColour(juce::Colour(0xff515354)); g.fillEllipse(x - 2, y - 2, 4, 4);
            g.setColour(juce::Colour(0xff151719)); g.drawLine(x - 1.2f, y + .4f, x + 1.2f, y - .4f, .7f);
        }
        auto body = area.reduced(15, 12);
        auto title = body.removeFromTop(25);
        g.setFont(labelFont(juce::jlimit(10.0f, 14.0f, getHeight() * .06f)));
        g.setColour(juce::Colour(0xffd6c5a6));
        g.drawFittedText(analog::names[static_cast<size_t>(juce::jmax(0, currentModel))], title.toNearestInt(), juce::Justification::centred, 1);
        auto status = body.removeFromBottom(20);
        g.setFont(valueFont(10)); g.setColour(juce::Colour(0xffb1a288));
        g.drawText(currentModel == 11 ? (transport > .1f ? "TAPE TRANSPORT  /  RUNNING" : "TAPE TRANSPORT  /  IDLE")
                                     : "ANALOG COLOUR  /  SIGNAL DRIVEN", status, juce::Justification::centred);
        if (currentModel == 11) paintTape(g, body);
        else if (currentModel <= 5) paintTube(g, body);
        else paintCircuit(g, body);
    }
private:
    struct Artwork
    {
        juce::Image off = juce::ImageFileFormat::loadFrom(BinaryData::analog_tube_off_png, BinaryData::analog_tube_off_pngSize);
        juce::Image on = juce::ImageFileFormat::loadFrom(BinaryData::analog_tube_on_png, BinaryData::analog_tube_on_pngSize);
        juce::Image reel = juce::ImageFileFormat::loadFrom(BinaryData::analog_tape_reel_png, BinaryData::analog_tape_reel_pngSize);
    };
    static const Artwork& artwork() {static const Artwork images; return images;}
    void paintTube(juce::Graphics& g, juce::Rectangle<float> body)
    {
        const auto& images = artwork();
        auto tube = body.withSizeKeepingCentre(body.getHeight() * .46f, body.getHeight());
        g.setColour(juce::Colours::black.withAlpha(.55f)); g.fillEllipse(tube.getX() - 8, tube.getBottom() - 9, tube.getWidth() + 16, 12);
        g.setOpacity(.91f);
        g.drawImage(images.off, tube.getX(), tube.getY(), tube.getWidth(), tube.getHeight(), 354, 9, 518, 1220);
        g.setOpacity(juce::jlimit(0.0f, 1.0f, light));
        g.drawImage(images.on, tube.getX(), tube.getY(), tube.getWidth(), tube.getHeight(), 354, 9, 518, 1220);
        g.setOpacity(1);
    }
    void paintTape(juce::Graphics& g, juce::Rectangle<float> body)
    {
        const auto size = juce::jmin(body.getHeight() * .68f, body.getWidth() * .46f);
        const auto left = body.getCentre() + juce::Point<float>{-size * .57f, -body.getHeight() * .12f};
        const auto right = body.getCentre() + juce::Point<float>{size * .57f, -body.getHeight() * .12f};
        const auto& image = artwork().reel;
        for (int reel = 0; reel < 2; ++reel)
        {
            const auto centre = reel == 0 ? left : right;
            juce::Graphics::ScopedSaveState save(g);
            const auto angle = phase * (reel == 0 ? 1.0f : 1.075f) + (reel == 0 ? 0 : .8f);
            g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
            g.drawImage(image, juce::Rectangle<float>{size, size}.withCentre(centre), juce::RectanglePlacement::centred);
        }
        const auto y = left.y + size * .40f;
        g.setColour(juce::Colour(0xff60422a));
        g.drawLine(left.x - size * .35f, y, left.x - size * .15f, y + size * .25f, 2);
        g.drawLine(left.x - size * .15f, y + size * .25f, right.x + size * .15f, y + size * .25f, 2);
        g.drawLine(right.x + size * .15f, y + size * .25f, right.x + size * .35f, y, 2);
        auto head = juce::Rectangle<float>{size * .32f, size * .20f}.withCentre({body.getCentreX(), y + size * .24f});
        g.setColour(juce::Colour(0xff333536)); g.fillRoundedRectangle(head, 3);
        g.setColour(juce::Colour(0xff96938c)); g.drawRoundedRectangle(head.reduced(.7f), 3, 1);
    }
    void paintCircuit(juce::Graphics& g, juce::Rectangle<float> body)
    {
        auto chassis = body.reduced(body.getWidth() * .20f, 5);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff51493e), chassis.getX(), chassis.getY(), juce::Colour(0xff191d20), chassis.getRight(), chassis.getBottom(), false));
        g.fillRoundedRectangle(chassis, 8);
        g.setColour(juce::Colour(0xff9b8664)); g.drawRoundedRectangle(chassis.reduced(1), 8, 1);
        const auto centre = chassis.getCentre();
        g.setColour(juce::Colour(0xfff0a34d).withAlpha(light)); g.fillEllipse(centre.x - 4, chassis.getY() + 14, 8, 8);
        auto meter = chassis.reduced(10).withTrimmedTop(chassis.getHeight() * .25f).withHeight(chassis.getHeight() * .27f);
        g.setColour(juce::Colour(0xffcabb9b)); g.fillRoundedRectangle(meter, 3);
        g.setColour(juce::Colour(0xff453426));
        for (int mark = 0; mark < 7; ++mark) g.drawVerticalLine(juce::roundToInt(meter.getX() + (mark + 1) * meter.getWidth() / 8), meter.getY() + 4, meter.getY() + 9);
        g.drawLine(meter.getCentreX(), meter.getBottom() - 3, meter.getX() + meter.getWidth() * (.16f + .68f * light), meter.getY() + 5, 1.2f);
        g.setColour(juce::Colour(0xff15181a)); g.fillEllipse(centre.x - 14, chassis.getBottom() - 43, 28, 28);
        g.setColour(juce::Colour(0xffa59b87)); g.drawEllipse(centre.x - 14, chassis.getBottom() - 43, 28, 28, 1);
    }
    int currentModel = 0;
    float light = .1f, paintedLight = .1f, transport = 0, phase = 0;
    std::uint64_t previousSequence = std::numeric_limits<std::uint64_t>::max();
    std::uint32_t lastFreshTick = 0;
};
}
