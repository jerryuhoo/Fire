#include <Panels/SpectrogramPanel/SpectrumComponent.h>
#include <Panels/SpectrogramPanel/SpectrumBackground.h>
#include <Panels/SpectrogramPanel/FFTProcessor.h>
#include <Panels/SpectrogramPanel/OttBandControls.h>
#include "helpers/RepaintRecorder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

struct SpectrumComponentTestAccess
{
    static void tick(SpectrumComponent& component)
    {
        component.timerCallback();
    }

    static void setMouseOver(SpectrumComponent& component, bool mouseOver)
    {
        component.setMouseOverSpectrum(mouseOver);
    }

    static float heldPeak(const SpectrumComponent& component, int bin)
    {
        return component.maxData[static_cast<size_t>(bin)];
    }

    static float displayedMagnitude(const SpectrumComponent& component,
                                    int bin)
    {
        return component.displayData[static_cast<size_t>(bin)];
    }

    static bool isPeakVisible(const SpectrumComponent& component)
    {
        return component.isPeakLineVisible;
    }

    static float presentationOpacity(const SpectrumComponent& component)
    {
        return component.presentationOpacity.current;
    }

    static float hoverOpacity(const SpectrumComponent& component)
    {
        return component.hoverOpacity.current;
    }

    static float hoverTarget(const SpectrumComponent& component)
    {
        return component.hoverOpacity.target;
    }

    static bool isMouseOver(const SpectrumComponent& component)
    {
        return component.mouseOver;
    }

    static bool isTimerRunning(const SpectrumComponent& component)
    {
        return component.isTimerRunning();
    }

    static void setInterpolationFactor(SpectrumComponent& component,
                                       float factor)
    {
        component.interpolationFactor = factor;
    }

    static juce::Rectangle<int> peakPillBounds(
        const SpectrumComponent& component)
    {
        constexpr float popupWidth = 112.0f;
        constexpr float popupHeight = 38.0f;
        const auto bounds = component.getLocalBounds().toFloat();
        auto popup = juce::Rectangle<float>(popupWidth, popupHeight)
                         .withCentre({ component.maxDecibelPoint.x,
                                       component.maxDecibelPoint.y
                                           - popupHeight * 0.72f });
        popup.setPosition(juce::jlimit(bounds.getX() + 4.0f,
                                       bounds.getRight() - popupWidth - 4.0f,
                                       popup.getX()),
                          juce::jlimit(bounds.getY() + 4.0f,
                                       bounds.getBottom() - popupHeight - 4.0f,
                                       popup.getY()));
        return popup.getSmallestIntegerContainer();
    }

    static bool isAwaitingFreshFrame(const SpectrumComponent& component)
    {
        return component.awaitingFreshFrame;
    }

    static bool renderedDataIsClear(const SpectrumComponent& component)
    {
        return component.renderedDataIsClear;
    }

    static bool hasSpectrumPath(const SpectrumComponent& component)
    {
        return ! component.spectrumLinePath.isEmpty();
    }

    static const juce::Path& spectrumPath(const SpectrumComponent& component)
    {
        return component.spectrumLinePath;
    }

    static float peakReadoutDb(const SpectrumComponent& component) { return component.maxDecibelValue; }
    static float peakReadoutFrequency(const SpectrumComponent& component) { return component.maxFreq; }
};

namespace
{
std::uint64_t renderedAlphaSum(SpectrumComponent& component,
                               juce::Rectangle<int> area)
{
    juce::Image image(juce::Image::ARGB,
                      juce::jmax(1, component.getWidth()),
                      juce::jmax(1, component.getHeight()),
                      true);
    juce::Graphics graphics(image);
    component.paintEntireComponent(graphics, true);

    area = area.getIntersection(image.getBounds());
    std::uint64_t total = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
            total += image.getPixelAt(x, y).getAlpha();

    return total;
}
} // namespace

TEST_CASE("Spectrum preserves isolated FFT peaks at their measured frequency and level",
          "[spectrum][spectrum-flow][ui][peak][measurement][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int bins = 1024;
    constexpr float binWidth = 48000.0f / 2048.0f;
    constexpr float peakDb = -12.0f;
    for (int width : {320, 1000})
        for (int bin : {2, 43, 427, 850})
        {
            CAPTURE(width, bin);
            SpectrumComponent spectrum {1, true};
            spectrum.setSize(width, 240);
            spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            spectrum.setVisible(true);
            const juce::ScopeGuard cleanup {[&] { spectrum.removeFromDesktop(); }};
            SpectrumComponentTestAccess::setInterpolationFactor(spectrum, 1.0f);
            std::array<float, bins> frame {};
            frame[static_cast<size_t>(bin)] = bins * juce::Decibels::decibelsToGain(peakDb);
            spectrum.updateSpectrum(frame.data(), bins, binWidth);
            SpectrumComponentTestAccess::tick(spectrum);
            const auto frequency = bin * binWidth;
            const auto expectedX = std::log10(frequency / 20.0f) / 3.0f * width;
            const auto expectedY = -peakDb / 100.0f * spectrum.getHeight();
            juce::Point<float> highest {-1.0f, 1000.0f};
            juce::Path::Iterator path(SpectrumComponentTestAccess::spectrumPath(spectrum));
            while (path.next())
                if ((path.elementType == juce::Path::Iterator::startNewSubPath
                     || path.elementType == juce::Path::Iterator::lineTo) && path.y1 < highest.y)
                    highest = {path.x1, path.y1};
            CHECK(highest.x == Catch::Approx(expectedX).margin(0.51f));
            CHECK(highest.y == Catch::Approx(expectedY).margin(0.001f));
            SpectrumComponentTestAccess::setMouseOver(spectrum, true);
            CHECK(SpectrumComponentTestAccess::peakReadoutDb(spectrum) == Catch::Approx(peakDb).margin(0.001f));
            CHECK(SpectrumComponentTestAccess::peakReadoutFrequency(spectrum) == Catch::Approx(frequency));
        }
}

TEST_CASE("Spectrum release follows the input monotonically then stops at silent presentation",
          "[spectrum][spectrum-flow][ui][release][repaint][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum {1, false};
    spectrum.setSize(800, 240);
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    const juce::ScopeGuard cleanup {[&] { spectrum.removeFromDesktop(); }};
    auto* recorder = new RepaintRecorder(spectrum);
    spectrum.setCachedComponentImage(recorder);
    std::array<float, 1024> frame {};
    frame[43] = 100.0f;
    spectrum.updateSpectrum(frame.data(), static_cast<int>(frame.size()), 48000.0f / 2048.0f);
    SpectrumComponentTestAccess::tick(spectrum);
    const auto firstAttack = SpectrumComponentTestAccess::displayedMagnitude(spectrum, 43);
    REQUIRE(firstAttack > 0.0f);
    REQUIRE(firstAttack < 100.0f);
    for (int tick = 0; tick < 120; ++tick) SpectrumComponentTestAccess::tick(spectrum);
    REQUIRE_FALSE(SpectrumComponentTestAccess::isTimerRunning(spectrum));

    frame.fill(0.0f);
    spectrum.updateSpectrum(frame.data(), static_cast<int>(frame.size()), 48000.0f / 2048.0f);
    SpectrumComponentTestAccess::tick(spectrum);
    auto previous = SpectrumComponentTestAccess::displayedMagnitude(spectrum, 43);
    CHECK(previous > 100.0f - firstAttack); // Release is slower than attack.
    CHECK(previous < 100.0f);
    for (int tick = 0; tick < 180; ++tick)
    {
        SpectrumComponentTestAccess::tick(spectrum);
        const auto current = SpectrumComponentTestAccess::displayedMagnitude(spectrum, 43);
        CHECK(current <= previous);
        CHECK(current >= 0.0f);
        previous = current;
    }
    CHECK(SpectrumComponentTestAccess::renderedDataIsClear(spectrum));
    CHECK_FALSE(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));
    CHECK_FALSE(SpectrumComponentTestAccess::isTimerRunning(spectrum));
    CHECK(renderedAlphaSum(spectrum, spectrum.getLocalBounds()) == 0u);
    recorder->clear();
    for (int frameIndex = 0; frameIndex < 40; ++frameIndex)
    {
        spectrum.updateSpectrum(frame.data(), static_cast<int>(frame.size()), 48000.0f / 2048.0f);
        SpectrumComponentTestAccess::tick(spectrum);
    }
    CHECK(recorder->dirtyAreas.isEmpty());
    CHECK_FALSE(SpectrumComponentTestAccess::isTimerRunning(spectrum));
}

TEST_CASE("A held spectrum peak survives live silence but cannot keep a bypass timer running",
          "[spectrum][spectrum-flow][ui][peak][host-bypass][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum {1, true};
    spectrum.setSize(800, 240);
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    const juce::ScopeGuard cleanup {[&] { spectrum.removeFromDesktop(); }};
    std::array<float, 1024> frame {};
    frame[43] = 100.0f;
    spectrum.updateSpectrum(frame.data(), static_cast<int>(frame.size()), 48000.0f / 2048.0f);
    for (int tick = 0; tick < 120; ++tick) SpectrumComponentTestAccess::tick(spectrum);
    SpectrumComponentTestAccess::setMouseOver(spectrum, true);
    const auto measuredDb = SpectrumComponentTestAccess::peakReadoutDb(spectrum);
    frame.fill(0.0f);
    spectrum.updateSpectrum(frame.data(), static_cast<int>(frame.size()), 48000.0f / 2048.0f);
    for (int tick = 0; tick < 180; ++tick) SpectrumComponentTestAccess::tick(spectrum);
    CHECK_FALSE(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));
    CHECK(SpectrumComponentTestAccess::isPeakVisible(spectrum));
    CHECK(SpectrumComponentTestAccess::peakReadoutDb(spectrum) == Catch::Approx(measuredDb));
    CHECK(renderedAlphaSum(spectrum, spectrum.getLocalBounds()) > 0u);
    spectrum.setHostBypassed(true);
    for (int tick = 0; tick < 180; ++tick) SpectrumComponentTestAccess::tick(spectrum);
    CHECK_FALSE(SpectrumComponentTestAccess::isTimerRunning(spectrum));
    CHECK_FALSE(SpectrumComponentTestAccess::isPeakVisible(spectrum));
    CHECK(SpectrumComponentTestAccess::heldPeak(spectrum, 43) == 0.0f);
    CHECK(SpectrumComponentTestAccess::peakReadoutDb(spectrum) == -100.0f);
    CHECK(renderedAlphaSum(spectrum, spectrum.getLocalBounds()) == 0u);
}

TEST_CASE("Spectrum energy and release snapshots use actual FFT frames",
          "[spectrum][spectrum-flow][ui][render][specimen]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable("FIRE_SPECTRUM_SNAPSHOT_DIR", {});
    if (path.isEmpty()) return;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File directory(path);
    REQUIRE(directory.createDirectory().wasOk());
    juce::Component canvas;
    SpectrumBackground background;
    SpectrumComponent original {0, false}, processed {1, false};
    canvas.setSize(1000, 230);
    for (auto* component : std::array<juce::Component*, 3> {&background, &original, &processed})
    {
        canvas.addAndMakeVisible(component);
        component->setBounds(canvas.getLocalBounds());
    }
    canvas.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    canvas.setVisible(true);
    const juce::ScopeGuard cleanup {[&] { canvas.removeFromDesktop(); }};
    original.setSpecAlpha(0.5f);
    SpectrumProcessor fft;
    std::array<float, SpectrumProcessor::fftBufferSize> input {}, output {};
    juce::Random noise(0x53504543);
    for (int sample = 0; sample < SpectrumProcessor::fftSize; ++sample)
    {
        const float phase = juce::MathConstants<float>::twoPi * sample / 48000.0f;
        input[static_cast<size_t>(sample)] = 0.30f * std::sin(phase * 220.0f)
            + 0.20f * std::sin(phase * 440.0f) + 0.12f * std::sin(phase * 1800.0f)
            + 0.07f * std::sin(phase * 6200.0f) + (noise.nextFloat() - 0.5f) * 0.025f;
        output[static_cast<size_t>(sample)] = std::tanh(input[static_cast<size_t>(sample)] * 2.0f) * 0.75f;
    }
    REQUIRE(fft.doProcessing(input.data(), static_cast<int>(input.size())));
    REQUIRE(fft.doProcessing(output.data(), static_cast<int>(output.size())));
    original.updateSpectrum(input.data(), SpectrumProcessor::numBins, 48000.0f / SpectrumProcessor::fftSize);
    processed.updateSpectrum(output.data(), SpectrumProcessor::numBins, 48000.0f / SpectrumProcessor::fftSize);
    const auto tick = [&]
    {
        SpectrumComponentTestAccess::tick(original);
        SpectrumComponentTestAccess::tick(processed);
    };
    const auto save = [&](const juce::String& name)
    {
        for (int deviceScale : {1, 2})
        {
            auto stream = directory.getChildFile("spectrum-" + name + "-" + juce::String(deviceScale) + "x.png").createOutputStream();
            REQUIRE(stream != nullptr);
            stream->setPosition(0); stream->truncate();
            CHECK(juce::PNGImageFormat().writeImageToStream(
                canvas.createComponentSnapshot(canvas.getLocalBounds(), true, static_cast<float>(deviceScale)), *stream));
        }
    };
    for (int frameIndex = 0; frameIndex < 120; ++frameIndex) tick();
    save("live");
    input.fill(0.0f);
    original.updateSpectrum(input.data(), SpectrumProcessor::numBins, 48000.0f / SpectrumProcessor::fftSize);
    processed.updateSpectrum(input.data(), SpectrumProcessor::numBins, 48000.0f / SpectrumProcessor::fftSize);
    for (int frameIndex = 0; frameIndex < 6; ++frameIndex) tick();
    save("release-100ms");
    for (int frameIndex = 0; frameIndex < 18; ++frameIndex) tick();
    save("release-400ms");
}

TEST_CASE("Spectrum frequency layout stays correct across size and FFT grid changes",
          "[spectrum][ui][layout][sample-rate][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum { 1, false };
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    SpectrumComponentTestAccess::setInterpolationFactor(spectrum, 1.0f);
    const juce::ScopeGuard cleanup { [&] { spectrum.removeFromDesktop(); } };

    std::array<float, 1024> frame {};
    for (size_t i = 0; i < frame.size(); ++i)
        frame[i] = 0.01f + static_cast<float>((i * 73) % 223);

    for (const auto width : { 267, 800, 503 })
        for (const auto bins : { 1024, 64 })
            for (const auto binWidth : { 44100.0f / 2048.0f, 96000.0f / 2048.0f })
            {
                CAPTURE(width, bins, binWidth);
                SpectrumComponent fresh { 1, false };
                fresh.setBounds(0, 0, width, 300);
                fresh.addToDesktop(juce::ComponentPeer::windowIsTemporary);
                fresh.setVisible(true);
                SpectrumComponentTestAccess::setInterpolationFactor(fresh, 1.0f);
                const juce::ScopeGuard freshCleanup { [&] { fresh.removeFromDesktop(); } };

                spectrum.setSize(width, 300);
                spectrum.updateSpectrum(frame.data(), bins, binWidth);
                fresh.updateSpectrum(frame.data(), bins, binWidth);
                SpectrumComponentTestAccess::tick(spectrum);
                SpectrumComponentTestAccess::tick(fresh);
                REQUIRE(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));
                CHECK(SpectrumComponentTestAccess::spectrumPath(spectrum)
                      == SpectrumComponentTestAccess::spectrumPath(fresh));
            }
}

TEST_CASE("Spectrum FFT positive magnitudes match the complete transform",
          "[spectrum][fft][regression]")
{
    SpectrumProcessor spectrum;
    juce::dsp::FFT referenceFFT(SpectrumProcessor::fftOrder);
    juce::dsp::WindowingFunction<float> referenceWindow(
        SpectrumProcessor::fftSize, juce::dsp::WindowingFunction<float>::blackman);
    std::array<float, SpectrumProcessor::fftBufferSize> actual {};
    std::array<float, SpectrumProcessor::fftBufferSize> expected {};
    juce::Random random(0x53504543);

    for (const auto signal : { 0, 1, 2 })
    {
        CAPTURE(signal);
        actual.fill(0.0f);
        for (int sample = 0; sample < SpectrumProcessor::fftSize; ++sample)
            actual[static_cast<size_t>(sample)] = signal == 0 ? 0.0f
                : signal == 1 ? std::sin(juce::MathConstants<float>::twoPi
                                         * 37.0f * static_cast<float>(sample)
                                         / static_cast<float>(SpectrumProcessor::fftSize))
                              : random.nextFloat() * 2.0f - 1.0f;
        expected = actual;
        referenceWindow.multiplyWithWindowingTable(expected.data(), SpectrumProcessor::fftSize);
        referenceFFT.performFrequencyOnlyForwardTransform(expected.data());
        REQUIRE(spectrum.doProcessing(actual.data(), static_cast<int>(actual.size())));
        for (int bin = 0; bin <= SpectrumProcessor::numBins; ++bin)
            CHECK(actual[static_cast<size_t>(bin)]
                  == Catch::Approx(expected[static_cast<size_t>(bin)]).margin(1.0e-5f));
    }
}

TEST_CASE("Spectrum frame copies clear all FFT scratch samples",
          "[spectrum][fft][fifo][regression]")
{
    SpectrumProcessor spectrum;
    std::array<float, SpectrumProcessor::fftBufferSize + 17> processed;
    std::array<float, SpectrumProcessor::fftBufferSize + 29> original;
    processed.fill(-99.0f);
    original.fill(-99.0f);
    for (int sample = 0; sample < SpectrumProcessor::fftSize; ++sample)
        spectrum.pushNextSamplePairIntoFifo(0.5f, -0.25f);
    REQUIRE(spectrum.popLatestFramePair(processed.data(), static_cast<int>(processed.size()),
                                        original.data(), static_cast<int>(original.size())));
    CHECK(std::all_of(processed.begin(), processed.begin() + SpectrumProcessor::fftSize,
                      [](float value) { return value == 0.5f; }));
    CHECK(std::all_of(original.begin(), original.begin() + SpectrumProcessor::fftSize,
                      [](float value) { return value == -0.25f; }));
    CHECK(std::all_of(processed.begin() + SpectrumProcessor::fftSize, processed.end(),
                      [](float value) { return value == 0.0f; }));
    CHECK(std::all_of(original.begin() + SpectrumProcessor::fftSize, original.end(),
                      [](float value) { return value == 0.0f; }));
}

TEST_CASE("OTT spectrum band stops repainting settled silence and resumes on visual changes",
          "[spectrum][ott][ui][repaint][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    OttBandControls controls(processor, 0);
    controls.setSize(600, 240);
    controls.setVisible(true);
    auto* recorder = new RepaintRecorder(controls);
    controls.setCachedComponentImage(recorder);
    controls.refresh();
    recorder->clear();

    for (int frame = 0; frame < 120; ++frame)
        controls.refresh();
    CHECK(recorder->dirtyAreas.isEmpty());

    fire::ui::OttSpectrumProfile profile {};
    profile[8] = 0.7f;
    controls.setSpectrum(profile, 0.4f);
    controls.refresh();
    REQUIRE_FALSE(recorder->dirtyAreas.isEmpty());
    recorder->clear();
    controls.refresh();
    CHECK(recorder->dirtyAreas.isEmpty());

    controls.setExternalInteraction(true, false);
    controls.refresh();
    REQUIRE_FALSE(recorder->dirtyAreas.isEmpty());
    controls.setExternalInteraction(false, false);
    for (int frame = 0; frame < 240; ++frame)
        controls.refresh();
    recorder->clear();
    controls.refresh();
    CHECK(recorder->dirtyAreas.isEmpty());

    auto* threshold = processor.treeState.getParameter("ottUpward1");
    REQUIRE(threshold != nullptr);
    threshold->setValueNotifyingHost(threshold->convertTo0to1(-54.0f));
    controls.refresh();
    REQUIRE_FALSE(recorder->dirtyAreas.isEmpty());
}

TEST_CASE("Spectrum peak line and readout pill fade on the 60 Hz presentation clock",
          "[spectrum][ui][peak][hover][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum { 1, true };
    spectrum.setBounds(0, 0, 800, 300);
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        spectrum.removeFromDesktop();
    } };
    REQUIRE(spectrum.isShowing());

    constexpr int numBins = 64;
    std::array<float, numBins> frame {};
    frame[12] = 100.0f;
    spectrum.updateSpectrum(frame.data(), numBins, 10.0f);
    for (int tick = 0; tick < 120; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);

    SpectrumComponentTestAccess::setMouseOver(spectrum, true);
    CHECK(SpectrumComponentTestAccess::isMouseOver(spectrum));
    CHECK(SpectrumComponentTestAccess::hoverTarget(spectrum)
          == Catch::Approx(1.0f));
    CHECK(SpectrumComponentTestAccess::hoverOpacity(spectrum)
          == Catch::Approx(0.0f));
    const auto pillBounds =
        SpectrumComponentTestAccess::peakPillBounds(spectrum);
    const auto idleAlpha = renderedAlphaSum(spectrum, pillBounds);

    SpectrumComponentTestAccess::tick(spectrum);
    const auto firstFadeIn = SpectrumComponentTestAccess::hoverOpacity(spectrum);
    const auto enteringAlpha = renderedAlphaSum(spectrum, pillBounds);
    CHECK(firstFadeIn > 0.0f);
    CHECK(firstFadeIn < 1.0f);
    REQUIRE(enteringAlpha > idleAlpha);

    for (int tick = 0; tick < 120; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);

    REQUIRE(SpectrumComponentTestAccess::hoverOpacity(spectrum)
            == Catch::Approx(1.0f));
    const auto establishedAlpha = renderedAlphaSum(spectrum, pillBounds);
    REQUIRE(establishedAlpha > enteringAlpha);
    CHECK(enteringAlpha - idleAlpha
          < (establishedAlpha - idleAlpha) / 2u);

    SpectrumComponentTestAccess::setMouseOver(spectrum, false);
    CHECK_FALSE(SpectrumComponentTestAccess::isMouseOver(spectrum));
    CHECK(SpectrumComponentTestAccess::hoverTarget(spectrum)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::hoverOpacity(spectrum)
          == Catch::Approx(1.0f));
    CHECK(renderedAlphaSum(spectrum, pillBounds) == establishedAlpha);

    SpectrumComponentTestAccess::tick(spectrum);
    const auto firstFadeOut = SpectrumComponentTestAccess::hoverOpacity(spectrum);
    const auto leavingAlpha = renderedAlphaSum(spectrum, pillBounds);
    CHECK(firstFadeOut > 0.0f);
    CHECK(firstFadeOut < 1.0f);
    CHECK(leavingAlpha > idleAlpha);
    CHECK(leavingAlpha < establishedAlpha);

    for (int tick = 0; tick < 120; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);

    CHECK(SpectrumComponentTestAccess::hoverOpacity(spectrum)
          == Catch::Approx(0.0f));
    CHECK(renderedAlphaSum(spectrum, pillBounds) == idleAlpha);
}

TEST_CASE("Spectrum hover presentation resets when hidden or detached from its peer",
          "[spectrum][ui][peak][hover][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum { 1, true };
    spectrum.setBounds(0, 0, 800, 300);
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        spectrum.removeFromDesktop();
    } };
    REQUIRE(spectrum.isShowing());

    const auto beginHover = [&]
    {
        SpectrumComponentTestAccess::setMouseOver(spectrum, true);
        SpectrumComponentTestAccess::tick(spectrum);
        REQUIRE(SpectrumComponentTestAccess::isMouseOver(spectrum));
        REQUIRE(SpectrumComponentTestAccess::hoverOpacity(spectrum) > 0.0f);
        REQUIRE(SpectrumComponentTestAccess::isPeakVisible(spectrum));
    };

    beginHover();
    spectrum.setVisible(false);

    CHECK_FALSE(SpectrumComponentTestAccess::isMouseOver(spectrum));
    CHECK(SpectrumComponentTestAccess::hoverOpacity(spectrum)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::hoverTarget(spectrum)
          == Catch::Approx(0.0f));
    CHECK_FALSE(SpectrumComponentTestAccess::isPeakVisible(spectrum));
    CHECK_FALSE(SpectrumComponentTestAccess::isTimerRunning(spectrum));

    spectrum.setVisible(true);
    REQUIRE(spectrum.isShowing());
    beginHover();

    // A settled hover retains the cheap liveness tick because JUCE does not
    // send a hierarchy callback when the top-level peer itself is removed.
    for (int tick = 0; tick < 120; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);
    REQUIRE(SpectrumComponentTestAccess::hoverOpacity(spectrum)
            == Catch::Approx(1.0f));
    REQUIRE(SpectrumComponentTestAccess::isTimerRunning(spectrum));

    spectrum.removeFromDesktop();
    SpectrumComponentTestAccess::tick(spectrum);

    CHECK_FALSE(SpectrumComponentTestAccess::isMouseOver(spectrum));
    CHECK(SpectrumComponentTestAccess::hoverOpacity(spectrum)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::hoverTarget(spectrum)
          == Catch::Approx(0.0f));
    CHECK_FALSE(SpectrumComponentTestAccess::isPeakVisible(spectrum));
    CHECK_FALSE(SpectrumComponentTestAccess::isTimerRunning(spectrum));
}

TEST_CASE("Spectrum grid changes discard peaks from the previous frequency scale",
          "[spectrum][ui][peak][sample-rate][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum { 1, true };
    spectrum.setBounds(0, 0, 800, 300);
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        spectrum.removeFromDesktop();
    } };
    REQUIRE(spectrum.isShowing());

    constexpr int numBins = 64;
    constexpr int probeBin = 12;
    std::array<float, numBins> firstFrame {};
    firstFrame[probeBin] = 100.0f;
    spectrum.updateSpectrum(firstFrame.data(), numBins, 10.0f);
    SpectrumComponentTestAccess::tick(spectrum);
    SpectrumComponentTestAccess::setMouseOver(spectrum, true);
    SpectrumComponentTestAccess::tick(spectrum);

    REQUIRE(SpectrumComponentTestAccess::isPeakVisible(spectrum));
    REQUIRE(SpectrumComponentTestAccess::heldPeak(spectrum, probeBin)
            > 10.0f);

    SpectrumComponentTestAccess::setMouseOver(spectrum, false);
    REQUIRE(SpectrumComponentTestAccess::isPeakVisible(spectrum));

    std::array<float, numBins> replacementFrame {};
    replacementFrame[probeBin] = 1.0f;
    spectrum.updateSpectrum(replacementFrame.data(), numBins, 20.0f);
    SpectrumComponentTestAccess::tick(spectrum);

    CHECK_FALSE(SpectrumComponentTestAccess::isPeakVisible(spectrum));
    CHECK(SpectrumComponentTestAccess::heldPeak(spectrum, probeBin)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::displayedMagnitude(spectrum, probeBin)
          == Catch::Approx(0.2f));
}

TEST_CASE("Host bypass fades and clears spectra before a fresh-frame resume",
          "[spectrum][ui][host-bypass][animation][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum { 1, true };
    spectrum.setBounds(0, 0, 800, 300);
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        spectrum.removeFromDesktop();
    } };
    REQUIRE(spectrum.isShowing());

    constexpr int numBins = 64;
    constexpr int oldProbeBin = 12;
    constexpr int resumedProbeBin = 18;
    std::array<float, numBins> oldFrame {};
    oldFrame[oldProbeBin] = 100.0f;
    spectrum.updateSpectrum(oldFrame.data(), numBins, 10.0f);
    for (int tick = 0; tick < 40; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);

    REQUIRE(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));
    REQUIRE(SpectrumComponentTestAccess::displayedMagnitude(spectrum,
                                                             oldProbeBin)
            > 90.0f);
    REQUIRE(SpectrumComponentTestAccess::presentationOpacity(spectrum)
            == Catch::Approx(1.0f));

    spectrum.setHostBypassed(true);
    SpectrumComponentTestAccess::tick(spectrum);
    const auto firstFadeOpacity =
        SpectrumComponentTestAccess::presentationOpacity(spectrum);
    CHECK(firstFadeOpacity > 0.0f);
    CHECK(firstFadeOpacity < 1.0f);
    CHECK(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));

    for (int tick = 0; tick < 180; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);

    CHECK(SpectrumComponentTestAccess::presentationOpacity(spectrum)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::renderedDataIsClear(spectrum));
    CHECK_FALSE(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));
    CHECK(SpectrumComponentTestAccess::displayedMagnitude(spectrum,
                                                           oldProbeBin)
          == Catch::Approx(0.0f));

    std::array<float, numBins> bypassShadowFrame {};
    bypassShadowFrame[resumedProbeBin] = 60.0f;
    spectrum.updateSpectrum(bypassShadowFrame.data(), numBins, 10.0f);
    spectrum.setHostBypassed(false);
    for (int tick = 0; tick < 20; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);

    // A bypass-shadow update is rejected as well. Leaving host bypass alone
    // is not permission to replay either it or the retained path; opacity
    // remains zero until a newer post-resume generation is submitted.
    CHECK(SpectrumComponentTestAccess::isAwaitingFreshFrame(spectrum));
    CHECK(SpectrumComponentTestAccess::presentationOpacity(spectrum)
          == Catch::Approx(0.0f));
    CHECK_FALSE(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));

    std::array<float, numBins> resumedFrame {};
    resumedFrame[resumedProbeBin] = 80.0f;
    spectrum.updateSpectrum(resumedFrame.data(), numBins, 10.0f);
    SpectrumComponentTestAccess::tick(spectrum);

    // The fresh frame is installed from silence while opacity is still zero.
    CHECK_FALSE(SpectrumComponentTestAccess::isAwaitingFreshFrame(spectrum));
    CHECK(SpectrumComponentTestAccess::presentationOpacity(spectrum)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::displayedMagnitude(spectrum,
                                                           oldProbeBin)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::displayedMagnitude(spectrum,
                                                           resumedProbeBin)
          == Catch::Approx(16.0f));

    SpectrumComponentTestAccess::tick(spectrum);
    CHECK(SpectrumComponentTestAccess::presentationOpacity(spectrum) > 0.0f);
    CHECK(SpectrumComponentTestAccess::displayedMagnitude(spectrum,
                                                           oldProbeBin)
          == Catch::Approx(0.0f));
}

TEST_CASE("Host bypass opacity is applied to every rendered spectrum style",
          "[spectrum][ui][host-bypass][animation][render][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const auto style : { 1, 2 })
    {
        DYNAMIC_SECTION("style " << style)
        {
            SpectrumComponent spectrum { style, false };
            spectrum.setBounds(0, 0, 800, 300);
            spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            spectrum.setVisible(true);
            const juce::ScopeGuard cleanup { [&]
            {
                spectrum.removeFromDesktop();
            } };
            REQUIRE(spectrum.isShowing());

            constexpr int numBins = 64;
            constexpr int probeBin = 12;
            std::array<float, numBins> frame {};
            frame[probeBin] = 100.0f;
            spectrum.updateSpectrum(frame.data(), numBins, 10.0f);
            for (int tick = 0; tick < 120; ++tick)
                SpectrumComponentTestAccess::tick(spectrum);

            REQUIRE(SpectrumComponentTestAccess::presentationOpacity(spectrum)
                    == Catch::Approx(1.0f));
            const auto fullAlpha = renderedAlphaSum(spectrum,
                                                     spectrum.getLocalBounds());
            REQUIRE(fullAlpha > 0u);

            spectrum.setHostBypassed(true);
            for (int tick = 0; tick < 4; ++tick)
                SpectrumComponentTestAccess::tick(spectrum);

            const auto fadeOutOpacity =
                SpectrumComponentTestAccess::presentationOpacity(spectrum);
            REQUIRE(fadeOutOpacity > 0.0f);
            REQUIRE(fadeOutOpacity < 1.0f);
            REQUIRE(SpectrumComponentTestAccess::hasSpectrumPath(spectrum));
            const auto fadingOutAlpha = renderedAlphaSum(
                spectrum, spectrum.getLocalBounds());
            CHECK(fadingOutAlpha > 0u);
            CHECK(fadingOutAlpha < fullAlpha);

            for (int tick = 0; tick < 180; ++tick)
                SpectrumComponentTestAccess::tick(spectrum);

            REQUIRE(SpectrumComponentTestAccess::presentationOpacity(spectrum)
                    == Catch::Approx(0.0f));
            CHECK(renderedAlphaSum(spectrum, spectrum.getLocalBounds()) == 0u);

            spectrum.setHostBypassed(false);
            // Install the fresh frame completely on its zero-opacity consume
            // tick. The next rendered change is therefore presentation fade
            // alone, not a confounding spectrum-geometry interpolation.
            SpectrumComponentTestAccess::setInterpolationFactor(spectrum,
                                                                  1.0f);
            spectrum.updateSpectrum(frame.data(), numBins, 10.0f);
            SpectrumComponentTestAccess::tick(spectrum);

            REQUIRE_FALSE(
                SpectrumComponentTestAccess::isAwaitingFreshFrame(spectrum));
            REQUIRE(SpectrumComponentTestAccess::presentationOpacity(spectrum)
                    == Catch::Approx(0.0f));
            CHECK(renderedAlphaSum(spectrum, spectrum.getLocalBounds()) == 0u);

            SpectrumComponentTestAccess::tick(spectrum);
            const auto fadeInOpacity =
                SpectrumComponentTestAccess::presentationOpacity(spectrum);
            REQUIRE(fadeInOpacity > 0.0f);
            REQUIRE(fadeInOpacity < 1.0f);
            const auto fadingInAlpha = renderedAlphaSum(
                spectrum, spectrum.getLocalBounds());
            CHECK(fadingInAlpha > 0u);
            CHECK(fadingInAlpha < fullAlpha);

            for (int tick = 0; tick < 180; ++tick)
                SpectrumComponentTestAccess::tick(spectrum);

            REQUIRE(SpectrumComponentTestAccess::presentationOpacity(spectrum)
                    == Catch::Approx(1.0f));
            CHECK(renderedAlphaSum(spectrum, spectrum.getLocalBounds())
                  == fullAlpha);
        }
    }
}

TEST_CASE("A rapid host-bypass reversal never fades the retained frame back in",
          "[spectrum][ui][host-bypass][animation][freshness][rapid][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectrumComponent spectrum { 1, true };
    spectrum.setBounds(0, 0, 800, 300);
    spectrum.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    spectrum.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        spectrum.removeFromDesktop();
    } };
    REQUIRE(spectrum.isShowing());

    constexpr int numBins = 64;
    constexpr int oldProbeBin = 12;
    constexpr int resumedProbeBin = 18;
    std::array<float, numBins> oldFrame {};
    oldFrame[oldProbeBin] = 100.0f;
    spectrum.updateSpectrum(oldFrame.data(), numBins, 10.0f);
    for (int tick = 0; tick < 40; ++tick)
        SpectrumComponentTestAccess::tick(spectrum);

    spectrum.setHostBypassed(true);
    SpectrumComponentTestAccess::tick(spectrum);
    spectrum.setHostBypassed(false);

    std::array<float, numBins> resumedFrame {};
    resumedFrame[resumedProbeBin] = 80.0f;
    spectrum.updateSpectrum(resumedFrame.data(), numBins, 10.0f);

    auto previousOpacity =
        SpectrumComponentTestAccess::presentationOpacity(spectrum);
    for (int tick = 0;
         tick < 180
         && SpectrumComponentTestAccess::isAwaitingFreshFrame(spectrum);
         ++tick)
    {
        SpectrumComponentTestAccess::tick(spectrum);
        const auto opacity =
            SpectrumComponentTestAccess::presentationOpacity(spectrum);
        CHECK(opacity <= previousOpacity + 1.0e-6f);
        previousOpacity = opacity;
    }

    REQUIRE_FALSE(SpectrumComponentTestAccess::isAwaitingFreshFrame(spectrum));
    CHECK(SpectrumComponentTestAccess::presentationOpacity(spectrum)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::displayedMagnitude(spectrum,
                                                           oldProbeBin)
          == Catch::Approx(0.0f));
    CHECK(SpectrumComponentTestAccess::displayedMagnitude(spectrum,
                                                           resumedProbeBin)
          == Catch::Approx(16.0f));

    SpectrumComponentTestAccess::tick(spectrum);
    CHECK(SpectrumComponentTestAccess::presentationOpacity(spectrum) > 0.0f);
}
