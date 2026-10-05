#include <GUI/SettingsComponent.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include "helpers/RepaintRecorder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

struct FireBrandingIntegrationTestAccess final
{
    static void useOriginalSkin(FireAudioProcessorEditor& editor)
    {
        // These motion/colour checks cover the original golden vector marks.
        // They must not depend on a user's locally saved Paper/Ink preference.
        editor.applySkin(fire::ui::Skin::modern);
    }
    static void tick(FireAudioProcessorEditor& editor)
    {
        // Exercise the real meter-consumption and repaint paths without
        // depending on a native event loop or elapsed test-machine time.
        editor.lastAnimationTimeSeconds =
            juce::Time::getMillisecondCounterHiRes() * 0.001 - 1.0 / 60.0;
        editor.timerCallback();
    }

    static float energy(const FireAudioProcessorEditor& editor)
    {
        return editor.fireLogoMotion.energy();
    }

    static float phase(const FireAudioProcessorEditor& editor)
    {
        return editor.fireLogoMotion.phase();
    }

    static bool isAnimating(const FireAudioProcessorEditor& editor)
    {
        return editor.fireLogoMotion.isAnimating();
    }

    static bool headerRepaintIsPending(const FireAudioProcessorEditor& editor)
    {
        return editor.headerRepaintPending;
    }

    static const MeterValues& meter(const FireAudioProcessorEditor& editor)
    {
        return editor.cachedMeterValues;
    }

    static void focusBand(FireAudioProcessorEditor& editor, int band)
    {
        editor.bandPanel.setFocusBandNum(band, true);
    }

    static juce::Rectangle<int> header(const FireAudioProcessorEditor& editor)
    {
        return editor.headerArea;
    }

    static std::array<juce::Rectangle<int>, 2> marks(
        const FireAudioProcessorEditor& editor)
    {
        return { editor.logoArea, editor.wingsArea };
    }

    static juce::Image renderHeaderArtwork(FireAudioProcessorEditor& editor,
                                          float scale = 1.0f)
    {
        const auto bounds = editor.headerArea;
        juce::Image result(juce::Image::ARGB,
                           juce::roundToInt(bounds.getWidth() * scale),
                           juce::roundToInt(bounds.getHeight() * scale), true,
                           juce::SoftwareImageType());
        juce::Graphics graphics(result);
        graphics.addTransform(juce::AffineTransform::scale(scale));
        editor.drawAnimatedHeader(graphics);
        return result;
    }

    static void finishBackgroundResize(FireAudioProcessorEditor& editor)
    {
        editor.rebuildPendingBackgroundCache(
            editor.backgroundCacheRebuildRequestedAtMs
            + FireAudioProcessorEditor::backgroundCacheResizeDebounceMs);
    }
};

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

void setParameter(FireAudioProcessor& processor,
                  const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

class BrandingFixture final
{
public:
    explicit BrandingFixture(int bandCount = 1)
    {
        processor.hasUpdateCheckBeenPerformed = true;
        // A dry global mix gives a known real output signal while all band
        // telemetry still comes from the actual multiband DSP graph.
        setParameter(processor, MIX_ID, 0.0f);
        setParameter(processor, HQ_ID, 0.0f);
        setParameter(processor, NUM_BANDS_ID, static_cast<float>(bandCount));
        if (bandCount == 4)
        {
            const std::array<float, 3> crossovers { 250.0f, 2000.0f, 6000.0f };
            for (int divider = 0; divider < 3; ++divider)
                setParameter(processor,
                    ParameterIDAndName::getIDString(FREQ_ID, divider),
                    crossovers[static_cast<size_t>(divider)]);
        }
        processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
        processor.prepareToPlay(sampleRate, blockSize);
        editor = std::make_unique<FireAudioProcessorEditor>(processor);
        FireBrandingIntegrationTestAccess::useOriginalSkin(*editor);
        editor->stopTimer();
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        REQUIRE(editor->isShowing());
    }

    ~BrandingFixture()
    {
        editor->setCachedComponentImage(nullptr);
        editor->removeFromDesktop();
    }

    void audio(float amplitude, int ticks)
    {
        for (int tick = 0; tick < ticks; ++tick)
        {
            publish(amplitude);
            FireBrandingIntegrationTestAccess::tick(*editor);
        }
    }

    void publish(float amplitude)
    {
        // One complete sine cycle per callback makes the RMS oracle stable
        // across blocks and keeps the fourth crossover band almost silent.
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = amplitude * static_cast<float>(std::sin(
                juce::MathConstants<double>::twoPi
                * static_cast<double>(absoluteSample++) / blockSize));
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }
        processor.processBlock(buffer, midi);
    }

    void noAudio(int ticks)
    {
        for (int tick = 0; tick < ticks; ++tick)
            FireBrandingIntegrationTestAccess::tick(*editor);
    }

    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    std::unique_ptr<FireAudioProcessorEditor> editor;

private:
    juce::AudioBuffer<float> buffer { 2, blockSize };
    juce::MidiBuffer midi;
    std::uint64_t absoluteSample = 0;
};

void writeSnapshot(juce::Component& component, const juce::String& name,
                   float scale = 1.0f)
{
    const auto directory = juce::SystemStats::getEnvironmentVariable(
        "FIRE_BRANDING_SNAPSHOT_DIR", {});
    if (directory.isEmpty())
        return;

    const auto folder = juce::File(directory);
    REQUIRE(folder.createDirectory().wasOk());
    auto stream = folder.getChildFile(name + ".png").createOutputStream();
    REQUIRE(stream != nullptr);
    stream->setPosition(0);
    stream->truncate();
    juce::PNGImageFormat png;
    REQUIRE(png.writeImageToStream(component.createComponentSnapshot(
        component.getLocalBounds(), true, scale), *stream));
}

void collectHeaderControls(juce::Component& component,
                           std::vector<juce::Component*>& result)
{
    if (dynamic_cast<juce::Button*>(&component) != nullptr
        || dynamic_cast<juce::ComboBox*>(&component) != nullptr)
    {
        result.push_back(&component);
        return;
    }
    for (auto* child : component.getChildren())
        if (child->isVisible())
            collectHeaderControls(*child, result);
}

int goldPixelCount(const juce::Image& image, juce::Rectangle<int> area)
{
    int result = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
        {
            const auto pixel = image.getPixelAt(x, y);
            if (pixel.getAlpha() > 128 && pixel.getRed() > 175
                && pixel.getGreen() > 80 && pixel.getBlue() < 130)
                ++result;
        }
    return result;
}
} // namespace

TEST_CASE("Header fire follows real output audio without a DAW playhead",
          "[branding][ui][audio][integration]")
{
    BrandingFixture fixture;
    auto& editor = *fixture.editor;
    REQUIRE(fixture.processor.getPlayHead() == nullptr);
    REQUIRE_FALSE(fixture.processor.isDawPlaying());

    fixture.audio(0.0f, 30);
    CHECK(FireBrandingIntegrationTestAccess::energy(editor) == 0.0f);
    CHECK_FALSE(FireBrandingIntegrationTestAccess::isAnimating(editor));
    const auto idle = FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor);

    fixture.audio(0.006f, 90);
    const auto quietEnergy = FireBrandingIntegrationTestAccess::energy(editor);
    REQUIRE(FireBrandingIntegrationTestAccess::meter(editor).outputRMS_L > 0.001f);
    CHECK(quietEnergy > 0.0f);
    CHECK(FireBrandingIntegrationTestAccess::isAnimating(editor));
    const auto quiet = FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor);
    CHECK_FALSE(repaintTestImagesMatch(idle, quiet));

    fixture.audio(0.45f, 90);
    const auto loudEnergy = FireBrandingIntegrationTestAccess::energy(editor);
    CHECK(loudEnergy > quietEnergy + 0.1f);
    CHECK_FALSE(fixture.processor.isDawPlaying());
    const auto loud = FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor);
    CHECK_FALSE(repaintTestImagesMatch(quiet, loud));
    const auto previousPhase = FireBrandingIntegrationTestAccess::phase(editor);
    fixture.audio(0.45f, 8);
    CHECK(FireBrandingIntegrationTestAccess::phase(editor) != previousPhase);
    CHECK_FALSE(repaintTestImagesMatch(loud,
        FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor)));

    fixture.audio(0.0f, 360);
    CHECK(FireBrandingIntegrationTestAccess::energy(editor) == 0.0f);
    CHECK_FALSE(FireBrandingIntegrationTestAccess::isAnimating(editor));
}

TEST_CASE("Header fire ignores an unrelated quiet focused band",
          "[branding][ui][audio][focus][integration]")
{
    BrandingFixture fixture(4);
    auto& editor = *fixture.editor;
    fixture.audio(0.18f, 120);
    const auto beforeFocus = FireBrandingIntegrationTestAccess::energy(editor);
    const auto& packet = FireBrandingIntegrationTestAccess::meter(editor);
    REQUIRE(packet.outputRMS_L > 0.05f);
    REQUIRE(packet.bandOutputRMS_L[3] < packet.outputRMS_L * 0.01f);

    FireBrandingIntegrationTestAccess::focusBand(editor, 3);
    fixture.audio(0.18f, 60);
    CHECK(FireBrandingIntegrationTestAccess::energy(editor)
          == Catch::Approx(beforeFocus).margin(0.03f));
    CHECK(FireBrandingIntegrationTestAccess::isAnimating(editor));
    CHECK(FireBrandingIntegrationTestAccess::meter(editor).bandOutputRMS_L[3]
          < FireBrandingIntegrationTestAccess::meter(editor).outputRMS_L * 0.01f);
}

TEST_CASE("Stopped audio callbacks settle header motion and repaint damage",
          "[branding][ui][audio][stale][repaint][integration]")
{
    BrandingFixture fixture;
    auto& editor = *fixture.editor;
    fixture.audio(0.35f, 90);
    REQUIRE(FireBrandingIntegrationTestAccess::energy(editor) > 0.1f);
    fixture.noAudio(360);
    REQUIRE(FireBrandingIntegrationTestAccess::energy(editor) == 0.0f);
    REQUIRE_FALSE(FireBrandingIntegrationTestAccess::isAnimating(editor));
    REQUIRE_FALSE(FireBrandingIntegrationTestAccess::headerRepaintIsPending(editor));

    const auto idle = FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor);
    const auto settledPhase = FireBrandingIntegrationTestAccess::phase(editor);
    auto* recorder = new RepaintRecorder(editor);
    editor.setCachedComponentImage(recorder);
    recorder->clear();

    fixture.noAudio(120);
    CHECK_FALSE(recorder->dirtyAreas.intersectsRectangle(
        FireBrandingIntegrationTestAccess::header(editor)));
    CHECK(FireBrandingIntegrationTestAccess::phase(editor) == settledPhase);
    CHECK(repaintTestImagesMatch(idle,
        FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor)));
    CHECK_FALSE(FireBrandingIntegrationTestAccess::headerRepaintIsPending(editor));

    // Stopping callbacks freezes the real PDC delay line as well. Its final
    // audible samples correctly rekindle the fire when processing resumes,
    // even though that first callback's input is entirely zero.
    fixture.audio(0.0f, 1);
    CHECK(FireBrandingIntegrationTestAccess::meter(editor).inputRMS_L == 0.0f);
    REQUIRE(FireBrandingIntegrationTestAccess::meter(editor).outputPeak_L > 0.001f);
    CHECK(FireBrandingIntegrationTestAccess::isAnimating(editor));

    // Once real output and presentation have both settled, fresh silent
    // packets must be just as idle as a stopped audio callback stream.
    fixture.audio(0.0f, 90);
    REQUIRE(FireBrandingIntegrationTestAccess::meter(editor).outputPeak_L == 0.0f);
    REQUIRE(FireBrandingIntegrationTestAccess::energy(editor) == 0.0f);
    REQUIRE_FALSE(FireBrandingIntegrationTestAccess::isAnimating(editor));
    REQUIRE_FALSE(FireBrandingIntegrationTestAccess::headerRepaintIsPending(editor));
    const auto silent = FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor);
    const auto silentPhase = FireBrandingIntegrationTestAccess::phase(editor);
    recorder->clear();

    fixture.audio(0.0f, 120);
    CHECK_FALSE(recorder->dirtyAreas.intersectsRectangle(
        FireBrandingIntegrationTestAccess::header(editor)));
    CHECK(FireBrandingIntegrationTestAccess::phase(editor) == silentPhase);
    CHECK(repaintTestImagesMatch(silent,
        FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor)));
    CHECK_FALSE(FireBrandingIntegrationTestAccess::headerRepaintIsPending(editor));
}

TEST_CASE("Header visibility sessions discard old and hidden audio packets",
          "[branding][ui][audio][lifecycle][integration]")
{
    BrandingFixture fixture;
    auto& editor = *fixture.editor;
    fixture.audio(0.35f, 90);
    REQUIRE(FireBrandingIntegrationTestAccess::isAnimating(editor));

    const auto exerciseBoundary = [&](bool detachPeer)
    {
        if (detachPeer)
            editor.removeFromDesktop();
        else
            editor.setVisible(false);
        REQUIRE_FALSE(editor.isShowing());
        fixture.noAudio(1); // Also covers hosts that detach without a hide callback.
        REQUIRE(FireBrandingIntegrationTestAccess::energy(editor) == 0.0f);
        REQUIRE_FALSE(FireBrandingIntegrationTestAccess::isAnimating(editor));

        // Leave a real loud packet queued at the visibility boundary. It must
        // never be interpreted as the first signal of the new visible session.
        fixture.publish(0.35f);
        if (detachPeer)
            editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor.setVisible(true);
        REQUIRE(editor.isShowing());
        fixture.noAudio(30);
        CHECK(FireBrandingIntegrationTestAccess::energy(editor) == 0.0f);
        CHECK_FALSE(FireBrandingIntegrationTestAccess::isAnimating(editor));

        fixture.audio(0.35f, 60);
        CHECK(FireBrandingIntegrationTestAccess::energy(editor) > 0.1f);
        CHECK(FireBrandingIntegrationTestAccess::isAnimating(editor));
    };

    SECTION("explicit hide and show") { exerciseBoundary(false); }
    SECTION("detached and restored native peer") { exerciseBoundary(true); }
}

TEST_CASE("Vector header marks preserve scaled layouts and control hit targets",
          "[branding][ui][layout][dpi][visual][integration]")
{
    BrandingFixture fixture;
    auto& editor = *fixture.editor;
    for (const int width : { 1000, 1400, 2000 })
    {
        CAPTURE(width);
        editor.setSize(width, width / 2);
        FireBrandingIntegrationTestAccess::finishBackgroundResize(editor);
        fixture.audio(0.0f, 360);
        const auto header = FireBrandingIntegrationTestAccess::header(editor);
        const auto marks = FireBrandingIntegrationTestAccess::marks(editor);
        CHECK(marks[0].getRight() < marks[1].getX());
        for (const auto& mark : marks)
        {
            CHECK_FALSE(mark.isEmpty());
            CHECK(header.contains(mark));
        }

        std::vector<juce::Component*> controls;
        collectHeaderControls(editor, controls);
        int headerControlCount = 0;
        for (auto* control : controls)
        {
            const auto bounds = editor.getLocalArea(control, control->getLocalBounds());
            if (! header.contains(bounds))
                continue;
            CAPTURE(control->getComponentID().toStdString());
            ++headerControlCount;
            CHECK_FALSE(bounds.isEmpty());
            CHECK_FALSE(bounds.intersects(marks[0]));
            CHECK_FALSE(bounds.intersects(marks[1]));
            auto* hit = editor.getComponentAt(bounds.getCentre());
            REQUIRE(hit != nullptr);
            CHECK((hit == control || control->isParentOf(hit)));
        }
        CHECK(headerControlCount >= 5);

        for (const float scale : { 1.0f, 2.0f })
        {
            CAPTURE(scale);
            const auto artwork = FireBrandingIntegrationTestAccess::renderHeaderArtwork(editor, scale);
            CHECK(artwork.getWidth() == juce::roundToInt(width * scale));
            for (const auto& mark : marks)
                CHECK(goldPixelCount(artwork,
                    (mark.toFloat() * scale).getSmallestIntegerContainer())
                      > juce::roundToInt(20.0f * scale * scale));
        }

        const auto prefix = "fire-branding-" + juce::String(width);
        writeSnapshot(editor, prefix + "-idle");
        fixture.audio(0.006f, 90);
        writeSnapshot(editor, prefix + "-low");
        fixture.audio(0.45f, 90);
        writeSnapshot(editor, prefix + "-high");
        fixture.audio(0.45f, 8);
        writeSnapshot(editor, prefix + "-high-phase2");
        if (width == 1400)
            writeSnapshot(editor, prefix + "-high-2x", 2.0f);
    }

    SettingsComponent settings(fixture.processor.getAppSettings());
    settings.setSize(400, 300);
    writeSnapshot(settings, "fire-branding-settings", 2.0f);
}
