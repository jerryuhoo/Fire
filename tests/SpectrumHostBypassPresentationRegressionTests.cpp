#include <PluginEditor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

struct SpectrumHostBypassPresentationTestAccess final
{
    static void synchronise(FireAudioProcessorEditor& editor,
                            bool animate)
    {
        editor.synchroniseSpectrumHostBypassState(animate);
    }

    static void advanceIndicator(FireAudioProcessorEditor& editor,
                                 float deltaSeconds)
    {
        editor.hostBypassIndicatorOpacity.advance(deltaSeconds, 0.10f);
    }

    static void tickSpectra(FireAudioProcessorEditor& editor)
    {
        editor.processedSpectrum.timerCallback();
        editor.originalSpectrum.timerCallback();
    }

    static void suspendPresentation(FireAudioProcessorEditor& editor)
    {
        editor.suspendSpectrumPresentation();
    }

    static void snapIndicator(FireAudioProcessorEditor& editor,
                              float opacity)
    {
        editor.hostBypassIndicatorOpacity.snapTo(opacity);
    }

    static float indicatorOpacity(const FireAudioProcessorEditor& editor)
    {
        return editor.hostBypassIndicatorOpacity.current;
    }

    static float indicatorTarget(const FireAudioProcessorEditor& editor)
    {
        return editor.hostBypassIndicatorOpacity.target;
    }

    static bool processedSpectrumIsBypassed(
        const FireAudioProcessorEditor& editor)
    {
        return editor.processedSpectrum.hostBypassed;
    }

    static bool originalSpectrumIsBypassed(
        const FireAudioProcessorEditor& editor)
    {
        return editor.originalSpectrum.hostBypassed;
    }

    static bool spectraAwaitFreshFrames(
        const FireAudioProcessorEditor& editor)
    {
        return editor.processedSpectrum.awaitingFreshFrame
            && editor.originalSpectrum.awaitingFreshFrame;
    }

    static juce::Point<int> spectrumCentre(
        const FireAudioProcessorEditor& editor)
    {
        return editor.spectrumCardArea.getCentre();
    }
};

TEST_CASE("Editor propagates host bypass to both freshness-gated spectra",
          "[ui][editor][spectrum][host-bypass][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(48000.0, 512);
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        editor.removeFromDesktop();
    } };
    REQUIRE(editor.isShowing());

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> emptyBuffer;
    processor.processBlockBypassed(emptyBuffer, midi);
    SpectrumHostBypassPresentationTestAccess::synchronise(editor, true);

    CHECK(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::indicatorTarget(editor)
          == Catch::Approx(1.0f));

    SpectrumHostBypassPresentationTestAccess::advanceIndicator(editor, 0.05f);
    CHECK(SpectrumHostBypassPresentationTestAccess::indicatorOpacity(editor)
          > 0.0f);

    processor.processBlock(emptyBuffer, midi);
    SpectrumHostBypassPresentationTestAccess::synchronise(editor, true);
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::indicatorTarget(editor)
          == Catch::Approx(0.0f));
}

TEST_CASE("Host bypass status paints a themed overlay above the spectrum card",
          "[ui][editor][spectrum][host-bypass][visual][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);

    const auto centre =
        SpectrumHostBypassPresentationTestAccess::spectrumCentre(editor);
    REQUIRE(editor.getLocalBounds().contains(centre));

    juce::Image clearImage(juce::Image::ARGB,
                           editor.getWidth(),
                           editor.getHeight(),
                           true);
    juce::Graphics clearGraphics(clearImage);
    SpectrumHostBypassPresentationTestAccess::snapIndicator(editor, 0.0f);
    editor.paintOverChildren(clearGraphics);
    CHECK(clearImage.getPixelAt(centre.x, centre.y).getAlpha() == 0);

    juce::Image bypassImage(juce::Image::ARGB,
                            editor.getWidth(),
                            editor.getHeight(),
                            true);
    juce::Graphics bypassGraphics(bypassImage);
    SpectrumHostBypassPresentationTestAccess::snapIndicator(editor, 1.0f);
    editor.paintOverChildren(bypassGraphics);
    CHECK(bypassImage.getPixelAt(centre.x, centre.y).getAlpha() > 0);
}

TEST_CASE("Editor detects a complete host-bypass epoch between UI polls",
          "[ui][editor][spectrum][host-bypass][rapid][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(48000.0, 512);
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        editor.removeFromDesktop();
    } };
    REQUIRE(editor.isShowing());

    SpectrumHostBypassPresentationTestAccess::synchronise(editor, false);
    REQUIRE_FALSE(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));

    // No editor synchronisation occurs between these callbacks.  The final
    // boolean is unchanged, so only the monotonic processor epoch can reveal
    // that a complete bypass session crossed this presentation boundary.
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> emptyBuffer;
    processor.processBlockBypassed(emptyBuffer, midi);
    processor.processBlock(emptyBuffer, midi);
    REQUIRE_FALSE(processor.getBypassedState());

    SpectrumHostBypassPresentationTestAccess::synchronise(editor, true);

    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
}

TEST_CASE("A visible timer restores spectra suspended by a peer-detach fallback",
          "[ui][editor][spectrum][host-bypass][lifecycle][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(48000.0, 512);
    FireAudioProcessorEditor editor(processor);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    const juce::ScopeGuard cleanup { [&]
    {
        editor.removeFromDesktop();
    } };
    REQUIRE(editor.isShowing());

    SpectrumHostBypassPresentationTestAccess::synchronise(editor, false);
    REQUIRE_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    REQUIRE_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));

    // The hidden-timer fallback can suspend presentation without a matching
    // visibilityChanged() callback when a host detaches its peer.  If the host
    // restores that peer just as silently, the next visible timer tick must
    // override the unchanged processor-bypass cache and reopen both inputs.
    SpectrumHostBypassPresentationTestAccess::suspendPresentation(editor);
    REQUIRE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    REQUIRE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));

    editor.timerCallback();

    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::processedSpectrumIsBypassed(editor));
    CHECK_FALSE(SpectrumHostBypassPresentationTestAccess::originalSpectrumIsBypassed(editor));
    CHECK(SpectrumHostBypassPresentationTestAccess::spectraAwaitFreshFrames(editor));
}
