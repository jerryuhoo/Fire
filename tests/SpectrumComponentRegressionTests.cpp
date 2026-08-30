#include <Panels/SpectrogramPanel/SpectrumComponent.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

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
