#include <Panels/SpectrogramPanel/SpectrumComponent.h>

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
