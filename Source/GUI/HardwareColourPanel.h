#pragma once

#include "FireTheme.h"
#include "Skin.h"
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
        setDescription("Tube filaments follow input level, with Drive adding intensity. Silence extinguishes the glow. Tape reels follow the audio transport.");
    }
    void setState(int model, float drive, float peak, float dt = 1.0f / 60.0f, std::uint64_t sequence = std::numeric_limits<std::uint64_t>::max())
    {
        const auto now = juce::Time::getMillisecondCounter();
        auto time = juce::jlimit(0.0f, .1f, std::isfinite(dt) ? dt : 0.0f);
        auto filamentTime = time;
        if (sequence != untrackedSequence)
        {
            const auto elapsed = now - lastUiTick;
            if (previousSequence == untrackedSequence || elapsed > 250u)
            {
                // A retained meter is not proof of current audio. Seed the
                // stream on first display/resume, then wait for a new block.
                previousSequence = sequence;
                lastFreshTick = now;
                receivedFreshAudio = false;
                light = transport = 0.0f;
            }
            else
            {
                filamentTime = elapsed * .001f;
                time = juce::jlimit(0.0f, .1f, elapsed * .001f);
                if (sequence != previousSequence)
                {
                    previousSequence = sequence;
                    lastFreshTick = now;
                    receivedFreshAudio = true;
                }
            }
            lastUiTick = now;
            if (!receivedFreshAudio || now - lastFreshTick > 180u) peak = 0.0f;
        }
        else
            previousSequence = untrackedSequence;
        model = juce::jlimit(0, analog::count - 1, model);
        const auto driveAmount = juce::jlimit(0.0f, 1.0f, std::isfinite(drive) ? drive * .01f : 0.0f);
        const auto input = juce::jlimit(0.0f, 1.0f, std::isfinite(peak) ? peak : 0.0f);
        const bool tube = model <= 5;
        if (tube && currentModel > 5) light = 0.0f;
        // Perceptual level mapping exposes quiet musical detail without
        // amplifying the noise floor. Drive enhances light only when audio
        // exists; its square-root taper is useful at normal low settings.
        const auto level = juce::jlimit(0.0f, 1.0f,
            (juce::Decibels::gainToDecibels(input, -72.0f) + 72.0f) / 72.0f);
        const auto target = tube ? std::pow(level, 1.35f) * (.45f + .55f * std::sqrt(driveAmount))
                                 : .10f + .65f * std::sqrt(driveAmount) + .25f * std::sqrt(input);
        const auto response = tube ? (target > light ? 1.0f / .045f : 1.0f / .180f) : 7.0f;
        light += (target - light) * (1 - std::exp(-(tube ? filamentTime : time) * response));
        if (tube && target == 0.0f && light < .001f) light = 0.0f;
        const auto running = input > .0001f ? 1.0f : 0.0f;
        transport += (running - transport) * (1 - std::exp(-time * 5));
        const auto advance = time * transport * (1.5f + driveAmount * .5f);
        // Each reel wraps its own angle. Scaling the already-wrapped left
        // angle would snap the faster right reel backwards once per left turn.
        phase = std::fmod(phase + advance, juce::MathConstants<float>::twoPi);
        rightPhase = std::fmod(rightPhase + advance * 1.075f, juce::MathConstants<float>::twoPi);
        const bool changed = currentModel != model || std::abs(light - paintedLight) > .003f
                             || (light == 0.0f && paintedLight != 0.0f)
                             || std::abs(driveAmount - currentDrive) > .001f || (model == 11 && transport > .001f);
        currentModel = model; currentDrive = driveAmount;
        if (changed && isShowing()) {paintedLight = light; repaint();}
    }
    float getFilamentBrightness() const noexcept
    {
        // The host may paint a retained editor before its UI timer resumes.
        if (previousSequence != untrackedSequence
            && juce::Time::getMillisecondCounter() - lastUiTick > 250u) return 0.0f;
        return light;
    }
    float getTransportPhase(int reel = 0) const noexcept {return reel == 0 ? phase : rightPhase;}
    void paint(juce::Graphics& g) override
    {
        if (isLineSkin(*this)) {paintLineHardware(g); return;}
        const bool vintage = isVintage(*this);
        const auto area = getLocalBounds().toFloat().reduced(4);
        const auto scale = juce::jlimit(.45f, 2.5f, juce::jmin(area.getWidth() / 300.0f, area.getHeight() / 225.0f));
        const auto faceplate = vintage ? area.reduced(18 * scale, 10 * scale) : area;
        const auto edge = vintage ? juce::Colour(0xffa89470) : juce::Colour(0xff66717a);
        g.setColour(juce::Colours::black.withAlpha(.30f));
        g.fillRoundedRectangle(area.translated(0, 2), 10 * scale);
        if (vintage)
        {
            // Substantial walnut cheeks frame a separate, recessed metal fascia.
            // Keeping the mechanical layout in the original area preserves reel
            // size and scale legibility while the border supplies the woodwork.
            drawWalnut(g, area, 10 * scale);
            g.setColour(juce::Colour(0xffedc490).withAlpha(.20f));
            g.drawRoundedRectangle(area.reduced(1.4f * scale), 9 * scale, .8f * scale);
            g.setColour(juce::Colour(0xff170e09).withAlpha(.80f));
            g.fillRoundedRectangle(faceplate.expanded(3 * scale).translated(0, 1 * scale), 5 * scale);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffefe2c1), faceplate.getTopLeft(),
                                                 juce::Colour(0xff756144), faceplate.getBottomRight(), false));
            g.fillRoundedRectangle(faceplate.expanded(1.1f * scale), 3.5f * scale);
            // Light catching the rounded outer edges separates real wood from
            // the dark contact line around the inset champagne faceplate.
            const auto cheekWidth = faceplate.getX() - area.getX();
            for (bool left : {true, false})
            {
                const auto cheek = juce::Rectangle<float>(left ? area.getX() + 2 * scale : faceplate.getRight() + 3 * scale,
                                                          area.getY() + 10 * scale, cheekWidth - 5 * scale, area.getHeight() - 20 * scale);
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff4d3a5).withAlpha(left ? .12f : .05f), cheek.getTopLeft(),
                                                     juce::Colours::black.withAlpha(left ? .05f : .24f), cheek.getTopRight(), false));
                g.fillRoundedRectangle(cheek, 2 * scale);
            }
        }
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xffe0d2ad) : juce::Colour(0xff303840), faceplate.getX(), faceplate.getY(),
                                             vintage ? juce::Colour(0xffb7a783) : juce::Colour(0xff171d23), faceplate.getRight(), faceplate.getBottom(), false));
        g.fillRoundedRectangle(faceplate, (vintage ? 3.0f : 10.0f) * scale);
        g.setColour(edge.withAlpha(.75f)); g.drawRoundedRectangle(faceplate.reduced(.5f), (vintage ? 3.0f : 10.0f) * scale, .8f);
        g.setColour(juce::Colours::white.withAlpha(vintage ? .20f : .08f));
        g.drawRoundedRectangle(faceplate.reduced(1.5f), (vintage ? 2.0f : 9.0f) * scale, .7f);
        g.saveState();
        g.reduceClipRegion(faceplate.reduced(5).toNearestInt());
        g.setColour((vintage ? juce::Colours::black : juce::Colours::white).withAlpha(.025f));
        for (float y = faceplate.getY() + 2; y < faceplate.getBottom(); y += 2 * scale)
            g.drawHorizontalLine(juce::roundToInt(y), faceplate.getX() + 6, faceplate.getRight() - 6);
        g.restoreState();
        for (auto point : {faceplate.getTopLeft(), faceplate.getTopRight(), faceplate.getBottomLeft(), faceplate.getBottomRight()})
        {
            const auto inset = (vintage ? 7.0f : 10.0f) * scale;
            const auto x = point.x < faceplate.getCentreX() ? point.x + inset : point.x - inset;
            const auto y = point.y < faceplate.getCentreY() ? point.y + inset : point.y - inset;
            paintScrew(g, {x, y}, 2.5f * scale, vintage);
        }
        auto body = area.reduced(17 * scale, 10 * scale);
        auto title = body.removeFromTop(25 * scale);
        g.setFont(labelFont(13 * scale));
        g.setColour(vintage ? juce::Colour(0xff40382e) : juce::Colour(0xffe0e6e9));
        g.drawFittedText(analog::names[static_cast<size_t>(juce::jmax(0, currentModel))], title.toNearestInt(), juce::Justification::centred, 1);
        auto status = body.removeFromBottom(19 * scale);
        g.setFont(valueFont(8.0f * scale));
        g.setColour(vintage ? juce::Colour(0xff685a45) : juce::Colour(0xff94a2aa));
        g.drawText(currentModel == 11 ? (transport > .1f ? "STEREO TAPE  /  RUNNING" : "STEREO TAPE  /  IDLE")
                                     : "ANALOG COLOUR  /  SIGNAL DRIVEN", status, juce::Justification::centred);
        if (currentModel == 11) paintTape(g, body);
        else if (currentModel <= 5) paintTube(g, body);
        else paintCircuit(g, body);
    }
private:
    void paintLineHardware(juce::Graphics& g)
    {
        const auto& palette = paletteFor(*this);
        auto area = getLocalBounds().toFloat().reduced(4);
        if (area.isEmpty()) return;
        const auto unit = juce::jlimit(.45f,2.5f,juce::jmin(area.getWidth()/300.0f,area.getHeight()/225.0f));
        const auto stroke = juce::jmax(.75f,1.1f*unit);
        const auto skin=skinFor(*this);
        const auto displayScale=g.getInternalContext().getPhysicalPixelScaleFactor();
        if (lineBackground.isNull() || lineBackgroundSize!=getLocalBounds() || lineBackgroundSkin!=skin
            || !juce::approximatelyEqual(lineBackgroundScale,displayScale))
        {
            lineBackgroundSize=getLocalBounds(); lineBackgroundSkin=skin; lineBackgroundScale=displayScale;
            lineBackground=juce::Image(juce::Image::ARGB,juce::jmax(1,juce::roundToInt(getWidth()*displayScale)),
                juce::jmax(1,juce::roundToInt(getHeight()*displayScale)),true);
            juce::Graphics backdrop(lineBackground);
            backdrop.addTransform(juce::AffineTransform::scale(displayScale));
            backdrop.fillAll(palette.canvas);
            drawPortfolioDots(backdrop,area,skin,unit);
        }
        g.drawImage(lineBackground,getLocalBounds().toFloat());
        auto body = area.reduced(14*unit,9*unit);
        auto title = body.removeFromTop(23*unit);
        g.setColour(palette.accent);
        g.setFont(portfolioMonoFont(juce::jmax(9.0f,10*unit)));
        g.drawText(analog::names[static_cast<size_t>(juce::jmax(0,currentModel))],title,juce::Justification::centred);
        auto caption = body.removeFromBottom(17*unit);
        g.setColour(palette.textMuted);
        g.setFont(portfolioMonoFont(juce::jmax(7.5f,8*unit)));
        g.drawText(currentModel==11 ? (transport>.1f ? "TAPE / RUNNING" : "TAPE / IDLE")
                                   : "ANALOG / SIGNAL DRIVEN",caption,juce::Justification::centred);
        g.setColour(palette.hairline);
        g.drawLine(area.getX()+12*unit,caption.getY()-4*unit,area.getRight()-12*unit,caption.getY()-4*unit,stroke*.65f);
        const auto brightness = getFilamentBrightness();
        if (currentModel==11)
        {
            const auto size = juce::jmin(body.getWidth()*.40f,body.getHeight()*.73f);
            const auto left = body.getCentre()+juce::Point<float>{-size*.57f,-body.getHeight()*.12f};
            const auto right = body.getCentre()+juce::Point<float>{size*.57f,-body.getHeight()*.12f};
            for (int reel=0;reel<2;++reel)
            {
                const auto centre = reel==0 ? left : right;
                auto disc = juce::Rectangle<float>(size,size).withCentre(centre);
                g.setColour(palette.accent.withAlpha(.86f));
                g.drawEllipse(disc,stroke);
                g.setColour(palette.hairline);
                g.drawEllipse(disc.reduced(size*.065f),stroke*.70f);
                juce::Graphics::ScopedSaveState save(g);
                g.addTransform(juce::AffineTransform::rotation(getTransportPhase(reel),centre.x,centre.y));
                g.setColour(palette.accent.withAlpha(.70f));
                for (int spoke=0;spoke<3;++spoke)
                {
                    const auto angle = juce::MathConstants<float>::twoPi*static_cast<float>(spoke)/3;
                    juce::Path aperture;
                    aperture.startNewSubPath(0,-size*.13f);
                    aperture.cubicTo(size*.10f,-size*.13f,size*.19f,-size*.27f,size*.21f,-size*.35f);
                    aperture.cubicTo(size*.11f,-size*.42f,-size*.11f,-size*.42f,-size*.21f,-size*.35f);
                    aperture.cubicTo(-size*.19f,-size*.27f,-size*.10f,-size*.13f,0,-size*.13f);
                    aperture.closeSubPath();
                    g.strokePath(aperture,juce::PathStrokeType(stroke*.80f),
                        juce::AffineTransform::rotation(angle).translated(centre.x,centre.y));
                }
                g.drawEllipse(disc.withSizeKeepingCentre(size*.09f,size*.09f),stroke);
            }
            const auto deckY = body.getY()+body.getHeight()*.81f;
            const auto guideL = juce::Point<float>{left.x-size*.18f,deckY};
            const auto guideR = juce::Point<float>{right.x+size*.18f,deckY};
            juce::Path ribbon;
            ribbon.startNewSubPath(left.x-size*.34f,left.y+size*.27f);
            ribbon.lineTo(guideL.x,guideL.y);
            ribbon.lineTo(guideR.x,guideR.y);
            ribbon.lineTo(right.x+size*.34f,right.y+size*.27f);
            g.setColour(palette.accent.withAlpha(.70f));
            g.strokePath(ribbon,juce::PathStrokeType(stroke,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            for (const auto guide : {guideL,guideR})
                g.drawEllipse(juce::Rectangle<float>(size*.07f,size*.07f).withCentre(guide),stroke*.8f);
            const auto head = juce::Rectangle<float>(size*.30f,size*.19f).withCentre({body.getCentreX(),deckY});
            g.setColour(palette.canvas); g.fillRoundedRectangle(head,2*unit);
            g.setColour(palette.accent); g.drawRoundedRectangle(head,2*unit,stroke);
            return;
        }
        if (currentModel<=5)
        {
            const auto height = body.getHeight()*.87f;
            const auto tube = body.withSizeKeepingCentre(height*.53f,height);
            g.setColour(palette.accent.withAlpha(.75f));
            g.drawRoundedRectangle(tube,tube.getWidth()*.42f,stroke);
            const auto socket = tube.withSizeKeepingCentre(tube.getWidth()*.95f,height*.11f)
                .withBottomY(tube.getBottom()+height*.035f);
            g.setColour(palette.canvas);g.fillRoundedRectangle(socket,2*unit);
            g.setColour(palette.accent);g.drawRoundedRectangle(socket,2*unit,stroke);
            for (int pin=0;pin<5;++pin)
            {
                const auto x = socket.getX()+socket.getWidth()*(.2f+static_cast<float>(pin)*.15f);
                g.drawLine(x,socket.getBottom(),x,socket.getBottom()+height*.05f,stroke*.75f);
            }
            auto plate = tube.reduced(tube.getWidth()*.23f,height*.25f);
            g.setColour(palette.textSecondary.withAlpha(.65f));
            g.drawRect(plate.withHeight(height*.20f),stroke*.8f);
            for (int grid=0;grid<3;++grid)
            {
                const auto y = plate.getY()+height*(.27f+static_cast<float>(grid)*.065f);
                g.drawLine(plate.getX()-3*unit,y,plate.getRight()+3*unit,y,stroke*.6f);
            }
            juce::Path heater;
            const auto y = tube.getY()+height*.69f;
            heater.startNewSubPath(plate.getX(),y);
            for (int wire=1;wire<=8;++wire)
                heater.lineTo(plate.getX()+plate.getWidth()*static_cast<float>(wire)/8,
                    y+(wire%2==0 ? 0.0f : height*.07f));
            g.setColour(palette.accent.withAlpha(brightness));
            g.strokePath(heater,juce::PathStrokeType(juce::jmax(1.0f,2*unit),
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            g.setColour(palette.accent.withAlpha(brightness*.05f));
            g.fillEllipse(plate.withHeight(height*.19f).withY(y-height*.06f).expanded(5*unit));
            return;
        }
        const auto circuit = body.withSizeKeepingCentre(juce::jmin(body.getWidth()*.76f,body.getHeight()*1.25f),body.getHeight()*.72f);
        g.setColour(palette.textSecondary);
        g.drawRoundedRectangle(circuit,3*unit,stroke);
        const auto mid = circuit.getCentre();
        const auto span = circuit.getWidth()*.35f;
        g.drawLine(circuit.getX()-8*unit,mid.y,mid.x-span*.32f,mid.y,stroke);
        g.drawLine(mid.x+span*.32f,mid.y,circuit.getRight()+8*unit,mid.y,stroke);
        juce::Path diode;
        g.setColour(palette.accent.withAlpha(.62f+brightness*.38f));
        if (currentModel==10)
        {
            for (int winding=0;winding<4;++winding)
            {
                const auto y=mid.y-span*.22f+static_cast<float>(winding)*span*.11f;
                for (const auto x : {mid.x-span*.20f,mid.x+span*.20f})
                    g.drawEllipse(juce::Rectangle<float>(span*.22f,span*.18f).withCentre({x,y}),stroke*.80f);
            }
            for (const auto x : {mid.x-unit*2,mid.x+unit*2})
                g.drawLine(x,mid.y-span*.35f,x,mid.y+span*.30f,stroke);
        }
        else
        {
            diode.addTriangle({mid.x-span*.28f,mid.y-span*.23f},
                              {mid.x-span*.28f,mid.y+span*.23f},{mid.x+span*.28f,mid.y});
            g.strokePath(diode,juce::PathStrokeType(stroke));
            g.drawLine(mid.x+span*.28f,mid.y-span*.23f,mid.x+span*.28f,mid.y+span*.23f,stroke);
        }
        const auto meter = circuit.withTrimmedTop(circuit.getHeight()*.77f).reduced(10*unit,0);
        g.setColour(palette.hairline);g.drawLine(meter.getX(),meter.getCentreY(),meter.getRight(),meter.getCentreY(),stroke);
        g.setColour(palette.accent);
        g.drawLine(meter.getX(),meter.getCentreY(),meter.getX()+meter.getWidth()*brightness,meter.getCentreY(),stroke*1.4f);
    }

    void visibilityChanged() override
    {
        if (!isShowing())
        {
            light = paintedLight = transport = 0.0f;
            previousSequence = untrackedSequence;
            receivedFreshAudio = false;
        }
    }
    juce::Image lineBackground;
    juce::Rectangle<int> lineBackgroundSize;
    Skin lineBackgroundSkin=Skin::modern;
    float lineBackgroundScale=0.0f;
    struct Artwork
    {
        juce::Image off = juce::ImageFileFormat::loadFrom(BinaryData::analog_tube_off_png, BinaryData::analog_tube_off_pngSize);
        juce::Image on = juce::ImageFileFormat::loadFrom(BinaryData::analog_tube_on_png, BinaryData::analog_tube_on_pngSize);
        juce::Image reel = juce::ImageFileFormat::loadFrom(BinaryData::analog_tape_reel_png, BinaryData::analog_tape_reel_pngSize);
    };
    static const Artwork& artwork() {static const Artwork images; return images;}
    static void paintScrew(juce::Graphics& g, juce::Point<float> centre, float radius, bool vintage)
    {
        auto bounds = juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre);
        g.setColour(juce::Colours::black.withAlpha(.4f)); g.fillEllipse(bounds.expanded(.6f));
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xffc9b184) : juce::Colour(0xffa6adb2), bounds.getTopLeft(),
                                             vintage ? juce::Colour(0xff776346) : juce::Colour(0xff424a50), bounds.getBottomRight(), false));
        g.fillEllipse(bounds);
        g.setColour(juce::Colour(0xff252523));
        g.drawLine(centre.x - radius * .56f, centre.y + radius * .22f, centre.x + radius * .56f, centre.y - radius * .22f, juce::jmax(.65f, radius * .27f));
    }
    static void paintMetalDisc(juce::Graphics& g, juce::Point<float> centre, float radius, bool vintage)
    {
        const auto bounds = juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre);
        g.setColour(juce::Colours::black.withAlpha(.45f)); g.fillEllipse(bounds.expanded(radius * .16f).translated(0, radius * .16f));
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xffddc496) : juce::Colour(0xffe2e7e8), bounds.getTopLeft(),
                                             vintage ? juce::Colour(0xff7c674b) : juce::Colour(0xff505d66), bounds.getBottomRight(), false));
        g.fillEllipse(bounds);
        g.setColour(juce::Colours::white.withAlpha(.45f)); g.drawEllipse(bounds.reduced(.5f), .65f);
        g.setColour(juce::Colour(0xff242b30)); g.fillEllipse(bounds.reduced(radius * .42f));
        g.setColour(vintage ? juce::Colour(0xffbaa179) : juce::Colour(0xffb2bdc2)); g.fillEllipse(bounds.reduced(radius * .72f));
    }
    void paintTube(juce::Graphics& g, juce::Rectangle<float> body)
    {
        const bool vintage = isVintage(*this);
        const auto& images = artwork();
        const auto brightness = getFilamentBrightness();
        const auto tubeHeight = body.getHeight() * .94f;
        auto tube = body.withSizeKeepingCentre(tubeHeight * .46f, tubeHeight).translated(0, -body.getHeight() * .025f);
        const auto socket = juce::Rectangle<float>(tube.getWidth() * 1.32f, body.getHeight() * .10f)
            .withCentre({tube.getCentreX(), tube.getBottom() - body.getHeight() * .014f});
        const auto glow = tube.expanded(tube.getWidth() * .38f, 0);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff99a40).withAlpha(brightness * .30f), glow.getCentre(),
                                             juce::Colours::transparentBlack, glow.getCentre() + juce::Point<float>(glow.getWidth() * .5f, 0), true));
        g.fillEllipse(glow);
        // The perforated guard and socket ground the glass in the faceplate.
        const auto guard = body.withSizeKeepingCentre(body.getWidth() * .70f, body.getHeight() * .65f);
        g.setColour((vintage ? juce::Colour(0xff62503b) : juce::Colour(0xff0b1015)).withAlpha(.32f));
        for (int side : {-1, 1})
            for (int slot = 0; slot < 8; ++slot)
                g.fillRoundedRectangle(guard.getCentreX() + side * guard.getWidth() * .37f - guard.getWidth() * .065f,
                                       guard.getY() + slot * guard.getHeight() / 8, guard.getWidth() * .13f, body.getHeight() * .014f, 1);
        g.setColour(juce::Colours::black.withAlpha(.45f)); g.fillEllipse(socket.expanded(5, 3).translated(0, 3));
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xff7e6b51) : juce::Colour(0xff79828a), socket.getTopLeft(),
                                             juce::Colour(0xff20282b), socket.getBottomRight(), false));
        g.fillEllipse(socket);
        g.setColour(juce::Colour(0xffc0bba9).withAlpha(.6f)); g.drawEllipse(socket.reduced(1), .7f);
        g.setOpacity(.96f);
        g.drawImage(images.off, tube.getX(), tube.getY(), tube.getWidth(), tube.getHeight(), 354, 9, 518, 1220);
        g.setOpacity(juce::jlimit(0.0f, 1.0f, brightness));
        g.drawImage(images.on, tube.getX(), tube.getY(), tube.getWidth(), tube.getHeight(), 354, 9, 518, 1220);
        g.setOpacity(1);
    }
    void paintTape(juce::Graphics& g, juce::Rectangle<float> body)
    {
        const bool vintage = isVintage(*this);
        const auto size = juce::jmin(body.getHeight() * .76f, body.getWidth() * .44f);
        const auto left = body.getCentre() + juce::Point<float>{-size * .565f, -body.getHeight() * .15f};
        const auto right = body.getCentre() + juce::Point<float>{size * .565f, -body.getHeight() * .15f};
        const auto deckY = body.getY() + body.getHeight() * .78f;
        const auto guideL = juce::Point<float>{left.x - size * .17f, deckY};
        const auto guideR = juce::Point<float>{right.x + size * .17f, deckY};
        auto deck = juce::Rectangle<float>(guideL.x - size * .13f, deckY - size * .13f, guideR.x - guideL.x + size * .26f, size * .29f);
        g.setColour(juce::Colours::black.withAlpha(.24f)); g.fillRoundedRectangle(deck.translated(0, 2), size * .025f);
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xff9c8966) : juce::Colour(0xff505b63), deck.getTopLeft(),
                                             vintage ? juce::Colour(0xff746044) : juce::Colour(0xff212a31), deck.getBottomRight(), false));
        g.fillRoundedRectangle(deck, size * .025f);
        g.setColour(juce::Colours::white.withAlpha(.15f)); g.drawRoundedRectangle(deck.reduced(.5f), size * .025f, .65f);
        // A continuous ribbon runs around the two guides, beneath the head shield.
        juce::Path tape;
        tape.startNewSubPath(left.x - size * .34f, left.y + size * .27f);
        tape.lineTo(guideL.x - size * .045f, guideL.y - size * .023f);
        tape.quadraticTo(guideL.x - size * .042f, guideL.y + size * .058f, guideL.x + size * .045f, guideL.y + size * .058f);
        tape.lineTo(guideR.x - size * .045f, guideR.y + size * .058f);
        tape.quadraticTo(guideR.x + size * .042f, guideR.y + size * .058f, guideR.x + size * .045f, guideR.y - size * .023f);
        tape.lineTo(right.x + size * .34f, right.y + size * .27f);
        g.setColour(juce::Colours::black.withAlpha(.7f)); g.strokePath(tape, juce::PathStrokeType(size * .023f));
        g.setColour(juce::Colour(0xff785035)); g.strokePath(tape, juce::PathStrokeType(size * .013f));
        const auto& image = artwork().reel;
        for (int reel = 0; reel < 2; ++reel)
        {
            const auto centre = reel == 0 ? left : right;
            const auto disc = juce::Rectangle<float>(size, size).withCentre(centre);
            g.setColour(juce::Colours::black.withAlpha(.4f)); g.fillEllipse(disc.reduced(size * .055f).translated(0, size * .035f));
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff13181b), disc.getTopLeft(),
                                                 vintage ? juce::Colour(0xffac9870) : juce::Colour(0xff727f87), disc.getBottomRight(), false));
            g.fillEllipse(disc.reduced(size * .043f));
            {
                juce::Graphics::ScopedSaveState save(g);
                g.addTransform(juce::AffineTransform::rotation(getTransportPhase(reel), centre.x, centre.y));
                g.drawImage(image, disc, juce::RectanglePlacement::centred);
            }
            // The axle and deck lighting stay still while each reel rotates independently.
            paintMetalDisc(g, centre, size * .043f, vintage);
            g.setColour(juce::Colours::white.withAlpha(.10f)); g.drawEllipse(disc.reduced(size * .055f), size * .005f);
        }
        paintMetalDisc(g, guideL, size * .047f, vintage);
        paintMetalDisc(g, guideR, size * .047f, vintage);
        const auto head = juce::Rectangle<float>(size * .36f, size * .22f).withCentre({body.getCentreX(), deckY});
        g.setColour(juce::Colours::black.withAlpha(.4f)); g.fillRoundedRectangle(head.expanded(2).translated(0, 2), size * .035f);
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xffd6c39d) : juce::Colour(0xffb8c2c7), head.getTopLeft(),
                                             vintage ? juce::Colour(0xff8b7759) : juce::Colour(0xff4d5b65), head.getBottomRight(), false));
        g.fillRoundedRectangle(head, size * .025f);
        g.setColour(juce::Colours::white.withAlpha(.55f)); g.drawRoundedRectangle(head.reduced(.6f), size * .025f, .7f);
        auto inset = head.reduced(size * .04f, size * .055f);
        g.setColour(juce::Colour(0xff1b2429)); g.fillRoundedRectangle(inset, size * .014f);
        g.setColour(juce::Colour(0xff819197));
        for (int line = 1; line < 4; ++line)
            g.drawVerticalLine(juce::roundToInt(inset.getX() + inset.getWidth() * line / 4), inset.getY() + 2, inset.getBottom() - 2);
        g.setFont(labelFont(size * .037f)); g.setColour(vintage ? juce::Colour(0xff4d402c) : juce::Colour(0xffc8d0d4));
        g.drawText("2 TRACK", deck.withTrimmedRight(deck.getWidth() * .72f).translated(size * .03f, 0), juce::Justification::centred);
        const auto led = juce::Point<float>(deck.getRight() - size * .08f, deck.getCentreY());
        g.setColour(juce::Colour(0xff70baa1).withAlpha(.2f + .65f * transport)); g.fillEllipse(led.x - size * .012f, led.y - size * .012f, size * .024f, size * .024f);
    }
    void paintCircuit(juce::Graphics& g, juce::Rectangle<float> body)
    {
        const bool vintage = isVintage(*this);
        auto chassis = body.reduced(body.getWidth() * .13f, body.getHeight() * .025f);
        const auto unit = juce::jmin(chassis.getWidth() / 210.0f, chassis.getHeight() / 150.0f);
        const auto accent = vintage ? juce::Colour(0xffa17544) : juce::Colour(0xff83b7c3);
        g.setColour(juce::Colours::black.withAlpha(.36f)); g.fillRoundedRectangle(chassis.translated(0, 3 * unit), 8 * unit);
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xff584c3d) : juce::Colour(0xff37424a), chassis.getTopLeft(),
                                             vintage ? juce::Colour(0xff2d2924) : juce::Colour(0xff17212a), chassis.getBottomRight(), false));
        g.fillRoundedRectangle(chassis, 8 * unit);
        g.setColour(vintage ? juce::Colour(0xff967f5b) : juce::Colour(0xff75868f)); g.drawRoundedRectangle(chassis.reduced(.6f), 8 * unit, 1);
        g.setColour(juce::Colours::white.withAlpha(.10f)); g.drawRoundedRectangle(chassis.reduced(2 * unit), 6 * unit, .7f);
        auto meter = chassis.reduced(11 * unit).withHeight(chassis.getHeight() * .57f);
        g.setColour(juce::Colours::black.withAlpha(.7f)); g.fillRoundedRectangle(meter.expanded(2 * unit), 5 * unit);
        g.setGradientFill(juce::ColourGradient(vintage ? juce::Colour(0xfff0d9a5) : juce::Colour(0xffe5e6d6), meter.getTopLeft(),
                                             vintage ? juce::Colour(0xffbda472) : juce::Colour(0xffb0b8ab), meter.getBottomRight(), false));
        g.fillRoundedRectangle(meter, 3 * unit);
        g.setColour(juce::Colour(0xff6c604a).withAlpha(.6f)); g.drawRoundedRectangle(meter.reduced(.5f), 3 * unit, .8f);
        const auto pivot = juce::Point<float>(meter.getCentreX(), meter.getBottom() - 8 * unit);
        const auto radius = juce::jmin(meter.getWidth() * .43f, meter.getHeight() * .80f);
        const auto pointAt = [&](float value, float r)
        {
            const auto angle = juce::degreesToRadians(-64.0f + value * 128.0f);
            return pivot + juce::Point<float>(std::sin(angle) * r, -std::cos(angle) * r);
        };
        juce::Path arc;
        for (int i = 0; i <= 48; ++i)
        {
            const auto point = pointAt(i / 48.0f, radius);
            if (i == 0) arc.startNewSubPath(point); else arc.lineTo(point);
        }
        g.setColour(juce::Colour(0xff574f40)); g.strokePath(arc, juce::PathStrokeType(.75f * unit));
        for (int tick = 0; tick <= 24; ++tick)
        {
            const auto value = tick / 24.0f;
            const bool major = tick % 4 == 0;
            g.setColour(tick >= 20 ? juce::Colour(0xffac4b39) : juce::Colour(0xff464b40));
            g.drawLine({pointAt(value, radius - (major ? 7 : 4) * unit), pointAt(value, radius)}, (major ? 1.0f : .6f) * unit);
        }
        const std::array<const char*, 7> values {"0", "2", "4", "6", "8", "10", "12"};
        g.setFont(valueFont(6.5f * unit));
        for (int tick = 0; tick < 7; ++tick)
        {
            const auto point = pointAt(tick / 6.0f, radius - 15 * unit);
            g.setColour(tick >= 5 ? juce::Colour(0xffa44433) : juce::Colour(0xff514b3b));
            g.drawText(values[static_cast<size_t>(tick)], juce::Rectangle<float>(19 * unit, 9 * unit).withCentre(point), juce::Justification::centred);
        }
        g.setFont(labelFont(7 * unit)); g.setColour(juce::Colour(0xff766c54));
        g.drawText("COLOUR", meter.withTrimmedTop(meter.getHeight() * .58f), juce::Justification::centred);
        const auto needle = pointAt(.12f + light * .73f, radius - 2 * unit);
        g.setColour(juce::Colours::black.withAlpha(.17f)); g.drawLine({pivot.translated(1.5f * unit, 1 * unit), needle.translated(1.5f * unit, 1 * unit)}, 1.4f * unit);
        g.setColour(juce::Colour(0xffa44831)); g.drawLine({pivot, needle}, 1.1f * unit);
        g.setColour(juce::Colour(0xff313932)); g.fillEllipse(pivot.x - 3 * unit, pivot.y - 3 * unit, 6 * unit, 6 * unit);
        // A restrained glass reflection makes the scale read as a recessed meter.
        juce::Path reflection;
        reflection.startNewSubPath(meter.getTopLeft()); reflection.lineTo(meter.getTopRight());
        reflection.lineTo(meter.getRight(), meter.getY() + meter.getHeight() * .14f);
        reflection.lineTo(meter.getX(), meter.getY() + meter.getHeight() * .37f); reflection.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(.13f)); g.fillPath(reflection);
        const auto knob = juce::Point<float>(chassis.getCentreX(), chassis.getBottom() - 26 * unit);
        paintMetalDisc(g, knob, 15 * unit, vintage);
        g.setColour(juce::Colour(0xff181d20)); g.fillEllipse(knob.x - 12 * unit, knob.y - 12 * unit, 24 * unit, 24 * unit);
        const auto driveAngle = juce::degreesToRadians(-135.0f + 270.0f * currentDrive);
        const auto driveVector = juce::Point<float>(std::sin(driveAngle), -std::cos(driveAngle));
        g.setColour(accent); g.drawLine({knob + driveVector * (10 * unit), knob + driveVector * (5 * unit)}, 1.7f * unit);
        for (int side : {-1, 1})
        {
            const auto x = knob.x + side * chassis.getWidth() * .32f;
            g.setColour(juce::Colours::black.withAlpha(.35f)); g.fillEllipse(x - 4.5f * unit, knob.y - 4.5f * unit, 9 * unit, 9 * unit);
            g.setColour(side < 0 ? accent.withAlpha(.35f + .65f * light) : juce::Colour(0xff959e99));
            g.fillEllipse(x - 2.5f * unit, knob.y - 2.5f * unit, 5 * unit, 5 * unit);
        }
        g.setFont(valueFont(6.0f * unit)); g.setColour(vintage ? juce::Colour(0xffc5ae86) : juce::Colour(0xffaab7bc));
        g.drawText("DRIVE", juce::Rectangle<float>{knob.x - 25 * unit, knob.y + 17 * unit, 50 * unit, 9 * unit}, juce::Justification::centred);
        g.drawText(currentModel == 7 ? "Ge" : currentModel == 8 ? "Si" : currentModel == 9 ? "FET" : currentModel == 10 ? "IRON" : "DIODE",
                   juce::Rectangle<float>{chassis.getX() + 9 * unit, knob.y + 11 * unit, chassis.getWidth() * .27f, 11 * unit}, juce::Justification::centred);
        g.drawText("COLOUR", juce::Rectangle<float>{chassis.getRight() - chassis.getWidth() * .30f, knob.y + 11 * unit, chassis.getWidth() * .27f, 11 * unit}, juce::Justification::centred);
    }
    int currentModel = 0;
    static constexpr auto untrackedSequence = std::numeric_limits<std::uint64_t>::max();
    float light = 0, paintedLight = 0, currentDrive = 0, transport = 0, phase = 0, rightPhase = .8f;
    std::uint64_t previousSequence = untrackedSequence;
    std::uint32_t lastFreshTick = 0, lastUiTick = 0;
    bool receivedFreshAudio = false;
};
}
