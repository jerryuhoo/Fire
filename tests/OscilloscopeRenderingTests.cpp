#include "Panels/ControlPanel/Graph Components/Oscilloscope.h"
#include "helpers/RepaintRecorder.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

struct OscilloscopeRenderingTestAccess
{
    static void seed(Oscilloscope& scope, const std::vector<float>& left,
                     const std::vector<float>& right, bool mono = false)
    {
        scope.historyL.clearQuick(); scope.historyR.clearQuick();
        if (! left.empty()) scope.historyL.addArray(left.data(), static_cast<int>(left.size()));
        if (! right.empty()) scope.historyR.addArray(right.data(), static_cast<int>(right.size()));
        scope.monoChannel = mono;
        scope.updateWaveformPaths();
    }
    static const juce::Path& trace(const Oscilloscope& scope, int channel)
    { return channel == 0 ? scope.waveformL : scope.waveformR; }
    static const juce::Path& envelope(const Oscilloscope& scope, int channel)
    { return channel == 0 ? scope.envelopeL : scope.envelopeR; }
    static juce::Rectangle<float> bounds(const Oscilloscope& scope) { return scope.getWaveformBounds(); }
    static juce::Rectangle<float> plot(const Oscilloscope& scope) { return scope.getGraphPlotBounds(); }
    static float centre(const Oscilloscope& scope, int channel) { return scope.channelCentres[static_cast<size_t>(channel)]; }
    static size_t buckets(const Oscilloscope& scope) { return scope.sampleIndexByPixel.size(); }
    static float light(const Oscilloscope& scope, int channel) { return scope.channelLight[static_cast<size_t>(channel)]; }
    static float newestLight(const Oscilloscope& scope, int channel) { return scope.newestLight[static_cast<size_t>(channel)]; }
    static bool running(const Oscilloscope& scope) { return scope.isTimerRunning(); }
    static bool synchronise(Oscilloscope& scope) { return scope.synchroniseHistorySource(); }
    static std::uint64_t generation(const Oscilloscope& scope) { return scope.lastHistoryGeneration; }
};

namespace
{
using Access = OscilloscopeRenderingTestAccess;
int commands(const juce::Path& path)
{
    juce::Path::Iterator iterator(path);
    int count = 0;
    while (iterator.next()) ++count;
    return count;
}
std::vector<float> sine(float amplitude, float frequency, float phase = 0.0f, int count = 400)
{
    std::vector<float> result(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index)
        result[static_cast<size_t>(index)] = amplitude * std::sin(static_cast<float>(index) * frequency + phase);
    return result;
}
void writeAtlas(FireAudioProcessor& processor, const juce::File& directory, int deviceScale)
{
    constexpr int width = 1260, height = 870;
    juce::Image image(juce::Image::ARGB, width * deviceScale, height * deviceScale, true);
    juce::Graphics graphics(image);
    graphics.fillAll(fire::ui::colours::canvas);
    graphics.addTransform(juce::AffineTransform::scale(static_cast<float>(deviceScale)));
    graphics.setColour(fire::ui::colours::textPrimary);
    graphics.setFont(fire::ui::labelFont(20));
    graphics.drawText("Waveform / signal-driven light", 24, 14, 1200, 28, juce::Justification::centredLeft);

    const auto draw = [&](int x, int y, int w, int h, float scale, const juce::String& title,
                          const std::vector<float>& left, const std::vector<float>& right, bool mono)
    {
        graphics.setColour(fire::ui::colours::textSecondary);
        graphics.setFont(fire::ui::bodyFont(12));
        graphics.drawText(title, x, y, w, 20, juce::Justification::centredLeft);
        Oscilloscope scope(processor);
        scope.setScale(scale);
        scope.setSize(w, h - 24);
        Access::seed(scope, left, right, mono);
        juce::Graphics::ScopedSaveState state(graphics);
        graphics.addTransform(juce::AffineTransform::translation(static_cast<float>(x), static_cast<float>(y + 24)));
        scope.paint(graphics);
    };
    auto left = sine(0.4f, 0.11f), right = sine(0.14f, 0.16f, 0.8f);
    for (size_t index = 0; index < left.size(); ++index)
        left[index] *= 0.25f + 0.75f * static_cast<float>(index) / 399.0f;
    draw(24, 56, 594, 195, 1, "Stereo: warm L / cool R, preserved level balance", left, right, false);
    auto antiphase = sine(0.55f, 0.09f);
    auto inverted = antiphase;
    for (auto& value : inverted) value = -value;
    draw(642, 56, 594, 195, 1, "Stereo: opposite polarity remains distinct", antiphase, inverted, false);
    std::vector<float> transients(400), rightTransients(400);
    transients[137] = 1.0f; transients[138] = -0.65f;
    rightTransients[289] = -0.8f; rightTransients[399] = 0.45f;
    draw(24, 258, 594, 195, 1, "Single-sample history transients and a live endpoint", transients, rightTransients, false);
    auto decay = sine(0.75f, 0.14f, 0.2f);
    for (size_t index = 0; index < decay.size(); ++index)
        decay[index] *= std::exp(-static_cast<float>(index) / 130.0f);
    draw(642, 258, 594, 195, 1, "Mono: a decaying waveform", decay, {}, true);
    draw(24, 460, 594, 195, 1, "Quiet signal: no abrupt auto-gain jump", sine(0.005f, 0.10f), sine(0.002f, 0.13f), false);
    draw(642, 460, 594, 195, 1, "Silence: only restrained lane references", std::vector<float>(400), std::vector<float>(400), false);
    draw(24, 673, 132, 164, 1, "132 px / transient", transients, rightTransients, false);
    draw(180, 673, 230, 164, 1, "230 px / stereo", antiphase, inverted, false);
    draw(434, 673, 340, 164, 1, "Steady DC / no decorative motion", std::vector<float>(400, 0.16f), std::vector<float>(400, -0.08f), false);
    draw(798, 673, 438, 164, 1.5f, "1.5x UI / mono", decay, {}, true);
    const auto file = directory.getChildFile("waveform-signal-flow-" + juce::String(deviceScale) + "x.png");
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(image, *stream));
}
}

TEST_CASE("Oscilloscope retains every positive and negative history peak when pixels are fewer than samples",
          "[oscilloscope][waveform-flow][ui][peaks]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    Oscilloscope scope(processor);
    scope.setSize(100, 110);
    std::vector<float> left(400), right(400);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 400; ++sample)
            for (float sign : {-1.0f, 1.0f})
            {
                auto& history = channel == 0 ? left : right;
                history[static_cast<size_t>(sample)] = sign;
                Access::seed(scope, left, right);
                const auto bounds = Access::trace(scope, channel).getBounds();
                const auto plot = Access::bounds(scope);
                const auto excursion = sign > 0 ? Access::centre(scope, channel) - bounds.getY()
                                                : bounds.getBottom() - Access::centre(scope, channel);
                CAPTURE(channel, sample, sign, excursion);
                REQUIRE(Access::buckets(scope) < history.size());
                CHECK(excursion > plot.getHeight() * 0.12f);
                CHECK(commands(Access::trace(scope, channel)) <= static_cast<int>(2 * Access::buckets(scope) + 2));
                history[static_cast<size_t>(sample)] = 0;
            }
}

TEST_CASE("Oscilloscope geometry remains finite bounded and resolution limited at sparse and dense histories",
          "[oscilloscope][waveform-flow][ui][layout][boundary]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    Oscilloscope scope(processor);
    constexpr std::array<juce::Point<int>, 5> sizes {{{20, 20}, {64, 48}, {120, 80}, {320, 160}, {960, 480}}};
    for (auto size : sizes)
        for (int count : {0, 1, 7, 400, 1600})
            for (bool mono : {false, true})
            {
                scope.setSize(size.x, size.y);
                auto left = sine(0.8f, 0.19f, 0.3f, count), right = sine(0.3f, 0.15f, 1.0f, count);
                if (count > 1) left[1] = std::numeric_limits<float>::quiet_NaN();
                if (count > 2) right[2] = std::numeric_limits<float>::infinity();
                Access::seed(scope, left, right, mono);
                const auto plot = Access::bounds(scope).expanded(0.01f);
                CAPTURE(size.x, size.y, count, mono);
                CHECK(Access::buckets(scope) <= static_cast<size_t>(std::max(0, juce::roundToInt(plot.getWidth()))));
                for (int channel = 0; channel < 2; ++channel)
                {
                    const auto& path = Access::trace(scope, channel);
                    if (! path.isEmpty()) CHECK(plot.contains(path.getBounds()));
                    CHECK(commands(path) <= static_cast<int>(2 * Access::buckets(scope) + 2));
                    juce::Path::Iterator point(path);
                    while (point.next()) { CHECK(std::isfinite(point.x1)); CHECK(std::isfinite(point.y1)); }
                }
                if (mono) CHECK(Access::trace(scope, 1).isEmpty());
            }
}

TEST_CASE("Waveform light is signal driven and the quiet display gain is continuous",
          "[oscilloscope][waveform-flow][ui][silence][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    Oscilloscope scope(processor);
    scope.setSize(320, 160);
    Access::seed(scope, std::vector<float>(400), std::vector<float>(400));
    const auto silent = renderRepaintTestComponent(scope);
    CHECK(Access::trace(scope, 0).isEmpty());
    CHECK(Access::trace(scope, 1).isEmpty());
    CHECK(Access::light(scope, 0) == 0.0f);
    CHECK(Access::newestLight(scope, 1) == 0.0f);
    Access::seed(scope, sine(1.0e-7f, 0.2f), sine(1.0e-7f, 0.1f));
    CHECK(repaintTestImagesMatch(silent, renderRepaintTestComponent(scope)));
    Access::seed(scope, sine(0.0049f, 0.13f), {}, true);
    const auto before = Access::trace(scope, 0).getBounds().getHeight();
    const auto beforeLight = Access::light(scope, 0);
    Access::seed(scope, sine(0.0051f, 0.13f), {}, true);
    const auto after = Access::trace(scope, 0).getBounds().getHeight();
    REQUIRE(before > 0.0f);
    CHECK(after > before);
    CHECK(after < before * 1.1f);
    CHECK(Access::light(scope, 0) > beforeLight);
    CHECK(Access::light(scope, 0) < beforeLight * 1.1f);
    auto stoppedSignal = sine(0.4f, 0.17f);
    for (size_t sample = 0; sample < stoppedSignal.size(); ++sample)
        if (sample < 80 || sample >= 140) stoppedSignal[sample] = 0.0f;
    Access::seed(scope, stoppedSignal, {}, true);
    CHECK(Access::trace(scope, 0).getBounds().getRight()
          < Access::bounds(scope).getX() + Access::bounds(scope).getWidth() * 0.5f);
    CHECK(Access::newestLight(scope, 0) == 0.0f);
}

TEST_CASE("Oscilloscope source epochs clear every illuminated layer before fresh history arrives",
          "[oscilloscope][waveform-flow][ui][history][epoch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    Oscilloscope scope(processor);
    scope.setSize(320, 160);
    Access::seed(scope, sine(0.6f, 0.11f), sine(0.3f, 0.16f));
    REQUIRE_FALSE(Access::envelope(scope, 0).isEmpty());
    processor.setHistoryArray(0);
    REQUIRE(Access::synchronise(scope));
    CHECK(Access::trace(scope, 0).isEmpty());
    CHECK(Access::envelope(scope, 0).isEmpty());
    CHECK(Access::envelope(scope, 1).isEmpty());
    CHECK(Access::light(scope, 0) == 0.0f);
    CHECK(Access::newestLight(scope, 1) == 0.0f);
    Oscilloscope empty(processor);
    empty.setSize(320, 160);
    CHECK(repaintTestImagesMatch(renderRepaintTestComponent(scope), renderRepaintTestComponent(empty)));
}

TEST_CASE("Stereo waveform pixels distinguish warm and cool lanes without painting outside the plot",
          "[oscilloscope][waveform-flow][ui][render][contrast]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    Oscilloscope scope(processor);
    scope.setSize(320, 160);
    for (float scale : {1.0f, 2.0f})
    {
        Access::seed(scope, {}, {});
        const auto empty = renderRepaintTestComponent(scope, scale);
        Access::seed(scope, sine(0.4f, 0.13f), sine(0.18f, 0.17f));
        const auto active = renderRepaintTestComponent(scope, scale);
        const auto clip = (Access::plot(scope).getSmallestIntegerContainer().toFloat() * scale).getSmallestIntegerContainer();
        const auto data = Access::bounds(scope) * scale;
        int warm = 0, cool = 0, outside = 0;
        for (int y = 0; y < active.getHeight(); ++y)
            for (int x = 0; x < active.getWidth(); ++x)
            {
                const auto actual = active.getPixelAt(x, y), reference = empty.getPixelAt(x, y);
                if (! clip.contains(x, y) && actual != reference) ++outside;
                if (! data.contains(static_cast<float>(x), static_cast<float>(y))) continue;
                if (y < data.getCentreY()
                    && static_cast<int>(actual.getRed()) > static_cast<int>(reference.getRed()) + 8
                    && static_cast<int>(actual.getRed()) > static_cast<int>(actual.getBlue()) + 15) ++warm;
                if (y > data.getCentreY()
                    && static_cast<int>(actual.getBlue()) > static_cast<int>(reference.getBlue()) + 8
                    && static_cast<int>(actual.getBlue()) > static_cast<int>(actual.getRed()) + 15
                    && static_cast<int>(actual.getGreen()) > static_cast<int>(actual.getRed()) + 10) ++cool;
            }
        CAPTURE(scale, warm, cool, outside);
        CHECK(warm > 40);
        CHECK(cool > 40);
        CHECK(outside == 0);
    }
}

TEST_CASE("Silent waveform histories do not repaint while growing and hidden scopes stop their timer",
          "[oscilloscope][waveform-flow][ui][silence][repaint][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(48000, 64);
    juce::Component host;
    Oscilloscope scope(processor);
    host.setSize(320, 160);
    scope.setBounds(host.getLocalBounds());
    host.addAndMakeVisible(scope);
    host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    host.setVisible(true);
    const juce::ScopeGuard cleanup {[&] { host.removeFromDesktop(); }};
    REQUIRE(scope.isShowing());
    REQUIRE(Access::running(scope));
    auto* recorder = new RepaintRecorder(scope);
    scope.setCachedComponentImage(recorder);
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    const auto tick = [&]
    {
        audio.clear();
        processor.processBlock(audio, midi);
        scope.timerCallback();
    };
    tick();
    const auto previousGeneration = Access::generation(scope);
    recorder->clear();
    for (int frame = 0; frame < 20; ++frame) tick();
    CHECK(Access::generation(scope) > previousGeneration);
    CHECK(recorder->dirtyAreas.isEmpty());
    host.setVisible(false);
    CHECK_FALSE(Access::running(scope));
    recorder->clear();
    tick();
    CHECK(recorder->dirtyAreas.isEmpty());
}

TEST_CASE("Waveform signal flow specimen covers stereo mono transients quiet silence and scale",
          "[oscilloscope][waveform-flow][ui][render][snapshot]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable("FIRE_WAVEFORM_SNAPSHOT_DIR", {});
    if (path.isEmpty()) return;
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const juce::File directory(path);
    REQUIRE(directory.createDirectory().wasOk());
    writeAtlas(processor, directory, 1);
    writeAtlas(processor, directory, 2);
}
