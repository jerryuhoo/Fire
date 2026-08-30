#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

struct EditorBackgroundCacheTestAccess final
{
    static std::uint64_t buildCount(
        const FireAudioProcessorEditor& editor) noexcept
    {
        return editor.backgroundCacheBuildCountForTesting;
    }

    static bool rebuildIsPending(
        const FireAudioProcessorEditor& editor) noexcept
    {
        return editor.backgroundCacheRebuildPending;
    }

    static juce::Point<int> cachedLogicalSize(
        const FireAudioProcessorEditor& editor) noexcept
    {
        return editor.backgroundCacheLogicalSize;
    }

    static juce::Point<int> cachedPixelSize(
        const FireAudioProcessorEditor& editor) noexcept
    {
        return { editor.backgroundCache.getWidth(),
                 editor.backgroundCache.getHeight() };
    }

    static float cachedDisplayScale(
        const FireAudioProcessorEditor& editor) noexcept
    {
        return editor.backgroundCacheDisplayScale;
    }

    static std::uint32_t debounceMs() noexcept
    {
        return FireAudioProcessorEditor::backgroundCacheResizeDebounceMs;
    }

    static void restartDebounce(FireAudioProcessorEditor& editor)
    {
        editor.backgroundCacheRebuildRequestedAtMs =
            juce::Time::getMillisecondCounter();
    }

    static void refreshAfter(FireAudioProcessorEditor& editor,
                             std::uint32_t elapsedMs)
    {
        editor.rebuildPendingBackgroundCache(
            editor.backgroundCacheRebuildRequestedAtMs + elapsedMs);
    }
};

namespace
{
juce::Image paintEditorBackground(FireAudioProcessorEditor& editor)
{
    juce::Image image(juce::Image::ARGB,
                      editor.getWidth(),
                      editor.getHeight(),
                      true);
    juce::Graphics graphics(image);
    editor.paint(graphics);
    return image;
}
} // namespace

TEST_CASE("Editor background cache coalesces a live resize burst",
          "[ui][editor][resize][background-cache][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->stopTimer();

    const auto initialBuildCount =
        EditorBackgroundCacheTestAccess::buildCount(*editor);
    const auto initialCachedSize =
        EditorBackgroundCacheTestAccess::cachedLogicalSize(*editor);
    REQUIRE(initialBuildCount > 0);
    REQUIRE(initialCachedSize
            == juce::Point<int>(editor->getWidth(), editor->getHeight()));

    editor->setSize(1100, 550);
    editor->setSize(1200, 600);
    editor->setSize(1400, 700);

    CHECK(EditorBackgroundCacheTestAccess::rebuildIsPending(*editor));
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == initialBuildCount);
    CHECK(EditorBackgroundCacheTestAccess::cachedLogicalSize(*editor)
          == initialCachedSize);

    EditorBackgroundCacheTestAccess::refreshAfter(
        *editor,
        EditorBackgroundCacheTestAccess::debounceMs() - 1);
    CHECK(EditorBackgroundCacheTestAccess::rebuildIsPending(*editor));
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == initialBuildCount);

    // Before the quiet period expires, paint() stretches the previous full
    // cache into the new destination. The expanded right/bottom edge remains
    // painted, and no opportunistic full-size allocation is performed.
    EditorBackgroundCacheTestAccess::restartDebounce(*editor);
    const auto interimImage = paintEditorBackground(*editor);
    CHECK(interimImage.getPixelAt(interimImage.getWidth() - 1,
                                  interimImage.getHeight() - 1)
              .getAlpha()
          > 0);
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == initialBuildCount);

    EditorBackgroundCacheTestAccess::refreshAfter(
        *editor,
        EditorBackgroundCacheTestAccess::debounceMs());
    CHECK_FALSE(EditorBackgroundCacheTestAccess::rebuildIsPending(*editor));
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == initialBuildCount + 1);
    CHECK(EditorBackgroundCacheTestAccess::cachedLogicalSize(*editor)
          == juce::Point<int>(editor->getWidth(), editor->getHeight()));
    CHECK(EditorBackgroundCacheTestAccess::cachedPixelSize(*editor)
          == juce::Point<int>(editor->getWidth(), editor->getHeight()));

    EditorBackgroundCacheTestAccess::refreshAfter(
        *editor,
        EditorBackgroundCacheTestAccess::debounceMs() * 2);
    editor->resized();
    CHECK_FALSE(EditorBackgroundCacheTestAccess::rebuildIsPending(*editor));
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == initialBuildCount + 1);
}

TEST_CASE("Editor background cache rebuilds immediately at the active DPI",
          "[ui][editor][resize][dpi][background-cache][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->stopTimer();

    const auto initialBuildCount =
        EditorBackgroundCacheTestAccess::buildCount(*editor);
    const auto logicalSize =
        juce::Point<int>(editor->getWidth(), editor->getHeight());

    const auto retinaSnapshot = editor->createComponentSnapshot(
        editor->getLocalBounds(), true, 2.0f);
    REQUIRE_FALSE(retinaSnapshot.isNull());
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == initialBuildCount + 1);
    CHECK(EditorBackgroundCacheTestAccess::cachedDisplayScale(*editor)
          == Catch::Approx(2.0f));
    CHECK(EditorBackgroundCacheTestAccess::cachedLogicalSize(*editor)
          == logicalSize);
    CHECK(EditorBackgroundCacheTestAccess::cachedPixelSize(*editor)
          == juce::Point<int>(logicalSize.x * 2, logicalSize.y * 2));
    CHECK_FALSE(EditorBackgroundCacheTestAccess::rebuildIsPending(*editor));

    const auto retinaBuildCount =
        EditorBackgroundCacheTestAccess::buildCount(*editor);
    editor->setSize(1200, 600);
    REQUIRE(EditorBackgroundCacheTestAccess::rebuildIsPending(*editor));

    // Painting another 2x frame during the live resize reuses the complete
    // Retina cache. The final quiet tick then rebuilds exactly at 2x.
    EditorBackgroundCacheTestAccess::restartDebounce(*editor);
    const auto resizedRetinaSnapshot = editor->createComponentSnapshot(
        editor->getLocalBounds(), true, 2.0f);
    REQUIRE_FALSE(resizedRetinaSnapshot.isNull());
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == retinaBuildCount);

    EditorBackgroundCacheTestAccess::refreshAfter(
        *editor,
        EditorBackgroundCacheTestAccess::debounceMs());
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == retinaBuildCount + 1);
    CHECK(EditorBackgroundCacheTestAccess::cachedPixelSize(*editor)
          == juce::Point<int>(2400, 1200));
    CHECK_FALSE(EditorBackgroundCacheTestAccess::rebuildIsPending(*editor));

    const auto settledRetinaBuildCount =
        EditorBackgroundCacheTestAccess::buildCount(*editor);
    const auto standardSnapshot = editor->createComponentSnapshot(
        editor->getLocalBounds(), true, 1.0f);
    REQUIRE_FALSE(standardSnapshot.isNull());
    CHECK(EditorBackgroundCacheTestAccess::buildCount(*editor)
          == settledRetinaBuildCount + 1);
    CHECK(EditorBackgroundCacheTestAccess::cachedDisplayScale(*editor)
          == Catch::Approx(1.0f));
    CHECK(EditorBackgroundCacheTestAccess::cachedPixelSize(*editor)
          == juce::Point<int>(editor->getWidth(), editor->getHeight()));
}
