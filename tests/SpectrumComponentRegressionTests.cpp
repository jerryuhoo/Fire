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
