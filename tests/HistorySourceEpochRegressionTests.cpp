#include <PluginProcessor.h>
#include "Panels/ControlPanel/Graph Components/Oscilloscope.h"
#include "Panels/ControlPanel/Graph Components/WidthGraph.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdint>

struct OscilloscopeHistorySourceTestAccess
{
    static void seedStaleHistory(Oscilloscope& graph)
    {
        graph.historyL.add(0.75f);
        graph.historyR.add(-0.5f);
        graph.historyScratch.left.add(0.25f);
        graph.historyScratch.right.add(-0.25f);
        graph.waveformL.startNewSubPath(1.0f, 1.0f);
        graph.waveformL.lineTo(2.0f, 2.0f);
        graph.waveformR.startNewSubPath(1.0f, 2.0f);
        graph.waveformR.lineTo(2.0f, 1.0f);
        graph.sampleIndexByPixel.push_back(0);
        graph.lastHistoryGeneration = 99;
    }

    static bool synchronise(Oscilloscope& graph)
    {
        return graph.synchroniseHistorySource();
    }

    static bool isHardCleared(const Oscilloscope& graph)
    {
        return graph.historyL.isEmpty()
               && graph.historyR.isEmpty()
               && graph.historyScratch.left.isEmpty()
               && graph.historyScratch.right.isEmpty()
               && graph.waveformL.isEmpty()
               && graph.waveformR.isEmpty()
               && graph.sampleIndexByPixel.empty()
               && graph.lastHistoryGeneration == 0;
    }
};

struct WidthGraphHistorySourceTestAccess
{
    static void seedStaleHistory(WidthGraph& graph)
    {
        graph.historyL.add(0.75f);
        graph.historyR.add(-0.5f);
        graph.historyScratch.left.add(0.25f);
        graph.historyScratch.right.add(-0.25f);
        graph.pointCloudCache = juce::Image(juce::Image::ARGB, 8, 8, true);
        graph.pointCloudCacheBounds = { 1.0f, 2.0f, 8.0f, 8.0f };
        graph.pointCloudCacheScale = 2.0f;
        graph.fadeFramesRemaining = 17;
        graph.cacheHasContent = true;
        graph.cacheGeometryDirty = false;
        graph.restoreTrailOnCacheRebuild = true;
        graph.lastHistoryGeneration = 99;
    }

    static bool synchronise(WidthGraph& graph)
    {
        return graph.synchroniseHistorySource();
    }

    static bool isHardCleared(const WidthGraph& graph)
    {
        return graph.historyL.isEmpty()
               && graph.historyR.isEmpty()
               && graph.historyScratch.left.isEmpty()
               && graph.historyScratch.right.isEmpty()
               && ! graph.pointCloudCache.isValid()
               && graph.pointCloudCacheBounds.isEmpty()
               && graph.pointCloudCacheScale == 0.0f
               && graph.fadeFramesRemaining == 0
               && ! graph.cacheHasContent
               && graph.cacheGeometryDirty
               && ! graph.restoreTrailOnCacheRebuild
               && graph.lastHistoryGeneration == 0;
    }

    static void rebuildCache(WidthGraph& graph)
    {
        graph.rebuildPointCloudCache(1.0f);
    }

    static bool matchesSnapshot(
        const WidthGraph& graph,
        const FireAudioProcessor::HistorySnapshot& snapshot)
    {
        return graph.lastHistoryGeneration == snapshot.generation
               && graph.historySourceToken == snapshot.sourceToken
               && WidthGraph::arraysMatch(graph.historyL, snapshot.left)
               && WidthGraph::arraysMatch(graph.historyR, snapshot.right);
    }

    static std::uint64_t lastGeneration(const WidthGraph& graph)
    {
        return graph.lastHistoryGeneration;
    }

    static std::uint64_t sourceToken(const WidthGraph& graph)
    {
        return graph.historySourceToken;
    }

    static int fadeFramesRemaining(const WidthGraph& graph)
    {
        return graph.fadeFramesRemaining;
    }

    static bool hasRenderableCache(const WidthGraph& graph)
    {
        return graph.pointCloudCache.isValid()
               && graph.cacheHasContent
               && ! graph.cacheGeometryDirty;
    }

    static bool cacheGeometryDirty(const WidthGraph& graph)
    {
        return graph.cacheGeometryDirty;
    }

    static bool shouldRestoreTrail(const WidthGraph& graph)
    {
        return graph.restoreTrailOnCacheRebuild;
    }
};

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 64;
constexpr int capturedSamplesPerBlock = 7;
constexpr int capturedSamplesAcrossTwoBlocks = 13;
constexpr std::uint64_t sourceMask = 0x7u;

void processHistoryBlock(FireAudioProcessor& processor, float value)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample, value);

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}

void processHistoryRampBlock(FireAudioProcessor& processor,
                             int numSamples,
                             int firstSample)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel,
                             sample,
                             static_cast<float>(firstSample + sample) / 128.0f);

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}
} // namespace

TEST_CASE("History source requests publish one epoch-tagged snapshot",
          "[processor][history][source-epoch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    FireAudioProcessor::HistorySnapshot initial;
    REQUIRE(processor.copyHistorySnapshot(initial));
    CHECK(initial.left.isEmpty());
    CHECK(initial.right.isEmpty());
    const auto initialToken = processor.getHistorySourceToken();
    CHECK((initialToken & sourceMask) == 4u);

    // Invalid indices normalise to the already-selected global source and
    // therefore must not create an artificial epoch.
    processor.setHistoryArray(99);
    CHECK(processor.getHistorySourceToken() == initialToken);

    processor.setHistoryArray(0);
    const auto firstAToken = processor.getHistorySourceToken();
    REQUIRE(firstAToken != initialToken);
    CHECK((firstAToken & sourceMask) == 0u);

    // Re-selecting the same normalised source is a no-op.
    processor.setHistoryArray(0);
    CHECK(processor.getHistorySourceToken() == firstAToken);

    FireAudioProcessor::HistorySnapshot beforeAudio;
    CHECK_FALSE(processor.copyHistorySnapshot(beforeAudio));
    CHECK(beforeAudio.left.isEmpty());
    CHECK(beforeAudio.right.isEmpty());

    processHistoryBlock(processor, 0.2f);
    FireAudioProcessor::HistorySnapshot firstA;
    REQUIRE(processor.copyHistorySnapshot(firstA));
    CHECK(firstA.sourceToken == firstAToken);
    CHECK(firstA.left.size() == capturedSamplesPerBlock);
    CHECK(firstA.right.size() == capturedSamplesPerBlock);
    CHECK(firstA.generation > initial.generation);

    processHistoryBlock(processor, 0.3f);
    FireAudioProcessor::HistorySnapshot accumulatedA;
    REQUIRE(processor.copyHistorySnapshot(accumulatedA));
    CHECK(accumulatedA.sourceToken == firstAToken);
    CHECK(accumulatedA.left.size() == capturedSamplesAcrossTwoBlocks);
    CHECK(accumulatedA.generation > firstA.generation);
}

TEST_CASE("History decimation is independent of host block partitioning",
          "[processor][history][partitioning][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor singleBlockProcessor;
    FireAudioProcessor splitBlockProcessor;
    singleBlockProcessor.prepareToPlay(sampleRate, blockSize * 2);
    splitBlockProcessor.prepareToPlay(sampleRate, blockSize * 2);

    processHistoryRampBlock(singleBlockProcessor, blockSize * 2, 0);
    processHistoryRampBlock(splitBlockProcessor, blockSize, 0);
    processHistoryRampBlock(splitBlockProcessor, blockSize, blockSize);

    FireAudioProcessor::HistorySnapshot singleBlock;
    FireAudioProcessor::HistorySnapshot splitBlocks;
    REQUIRE(singleBlockProcessor.copyHistorySnapshot(singleBlock));
    REQUIRE(splitBlockProcessor.copyHistorySnapshot(splitBlocks));
    REQUIRE(singleBlock.left.size() == capturedSamplesAcrossTwoBlocks);
    REQUIRE(splitBlocks.left.size() == singleBlock.left.size());
    REQUIRE(splitBlocks.right.size() == singleBlock.right.size());

    for (int sample = 0; sample < singleBlock.left.size(); ++sample)
    {
        CHECK(splitBlocks.left[sample]
              == Catch::Approx(singleBlock.left[sample]).margin(1.0e-6f));
        CHECK(splitBlocks.right[sample]
              == Catch::Approx(singleBlock.right[sample]).margin(1.0e-6f));
    }
}

TEST_CASE("History A to B to A cannot reuse the first A ring buffer",
          "[processor][history][source-epoch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    processor.setHistoryArray(0);
    processHistoryBlock(processor, 0.2f);
    FireAudioProcessor::HistorySnapshot firstA;
    REQUIRE(processor.copyHistorySnapshot(firstA));
    REQUIRE(firstA.left.size() == capturedSamplesPerBlock);

    processor.setHistoryArray(1);
    const auto bToken = processor.getHistorySourceToken();
    CHECK((bToken & sourceMask) == 1u);
    FireAudioProcessor::HistorySnapshot staleA;
    CHECK_FALSE(processor.copyHistorySnapshot(staleA));

    // Return before the audio thread sees B. The source bits match the first
    // request again, but its newer epoch must still invalidate firstA.
    processor.setHistoryArray(0);
    const auto secondAToken = processor.getHistorySourceToken();
    REQUIRE(secondAToken != firstA.sourceToken);
    CHECK((secondAToken & sourceMask) == 0u);
    CHECK_FALSE(processor.copyHistorySnapshot(staleA));

    processHistoryBlock(processor, 0.4f);
    FireAudioProcessor::HistorySnapshot secondA;
    REQUIRE(processor.copyHistorySnapshot(secondA));
    CHECK(secondA.sourceToken == secondAToken);
    // A source epoch starts empty and exposes only samples captured since the
    // audio thread accepted that exact request.
    CHECK(secondA.left.size() == capturedSamplesPerBlock);
    CHECK(secondA.right.size() == capturedSamplesPerBlock);
}

TEST_CASE("History graphs hard-clear on a source request without audio",
          "[ui][history][source-epoch][freshness]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    Oscilloscope oscilloscope(processor);
    WidthGraph widthGraph(processor);
    OscilloscopeHistorySourceTestAccess::seedStaleHistory(oscilloscope);
    WidthGraphHistorySourceTestAccess::seedStaleHistory(widthGraph);

    processor.setHistoryArray(0);
    REQUIRE(OscilloscopeHistorySourceTestAccess::synchronise(oscilloscope));
    REQUIRE(WidthGraphHistorySourceTestAccess::synchronise(widthGraph));
    CHECK(OscilloscopeHistorySourceTestAccess::isHardCleared(oscilloscope));
    CHECK(WidthGraphHistorySourceTestAccess::isHardCleared(widthGraph));

    FireAudioProcessor::HistorySnapshot notYetPublished;
    CHECK_FALSE(processor.copyHistorySnapshot(notYetPublished));
    CHECK(notYetPublished.left.isEmpty());
    CHECK(notYetPublished.right.isEmpty());
}

TEST_CASE("Width graph consumes hidden history before its first visible repaint",
          "[ui][history][width-graph][visibility][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    REQUIRE(processor.getTotalNumInputChannels() == 2);

    juce::Component desktopHost;
    desktopHost.setBounds(0, 0, 420, 260);
    desktopHost.setVisible(false);

    WidthGraph widthGraph(processor);
    desktopHost.addAndMakeVisible(widthGraph);
    widthGraph.setBounds(20, 20, 280, 180);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);
    REQUIRE(widthGraph.isShowing());

    WidthGraphHistorySourceTestAccess::rebuildCache(widthGraph);
    processHistoryBlock(processor, 0.2f);

    FireAudioProcessor::HistorySnapshot visibleSnapshot;
    REQUIRE(processor.copyHistorySnapshot(visibleSnapshot));
    widthGraph.timerCallback();
    REQUIRE(WidthGraphHistorySourceTestAccess::matchesSnapshot(
        widthGraph,
        visibleSnapshot));
    REQUIRE(WidthGraphHistorySourceTestAccess::hasRenderableCache(widthGraph));
    REQUIRE(WidthGraphHistorySourceTestAccess::fadeFramesRemaining(widthGraph)
            == 24);

    // Establish an observable old trail.  With no new generation this timer
    // only fades the existing cache; accepting the hidden generation must
    // reinforce the first visible frame back to the full trail lifetime.
    widthGraph.timerCallback();
    REQUIRE(WidthGraphHistorySourceTestAccess::fadeFramesRemaining(widthGraph)
            == 23);

    const auto stableSourceToken = processor.getHistorySourceToken();
    const auto visibleGeneration = visibleSnapshot.generation;
    desktopHost.setVisible(false);
    REQUIRE_FALSE(widthGraph.isShowing());

    SECTION("an existing point-cloud cache is refreshed synchronously")
    {
        processHistoryBlock(processor, 0.7f);
        FireAudioProcessor::HistorySnapshot hiddenSnapshot;
        REQUIRE(processor.copyHistorySnapshot(hiddenSnapshot));
        REQUIRE(hiddenSnapshot.sourceToken == stableSourceToken);
        REQUIRE(hiddenSnapshot.generation > visibleGeneration);
        REQUIRE(WidthGraphHistorySourceTestAccess::lastGeneration(widthGraph)
                == visibleGeneration);

        // Do not dispatch the message loop or invoke the graph timer manually:
        // the visibility transition itself must consume the newest snapshot
        // before its queued repaint can expose the old cache.
        desktopHost.setVisible(true);
        REQUIRE(widthGraph.isShowing());
        CHECK(WidthGraphHistorySourceTestAccess::matchesSnapshot(
            widthGraph,
            hiddenSnapshot));
        CHECK(WidthGraphHistorySourceTestAccess::fadeFramesRemaining(widthGraph)
              == 24);
        CHECK(processor.getHistorySourceToken() == stableSourceToken);
    }

    SECTION("the same generation resumes fading without reinforcement")
    {
        desktopHost.setVisible(true);
        REQUIRE(widthGraph.isShowing());
        CHECK(WidthGraphHistorySourceTestAccess::matchesSnapshot(
            widthGraph,
            visibleSnapshot));
        CHECK(WidthGraphHistorySourceTestAccess::fadeFramesRemaining(widthGraph)
              == 22);
        CHECK(processor.getHistorySourceToken() == stableSourceToken);
    }

    SECTION("an unpublished source epoch clears the hidden trail")
    {
        processor.setHistoryArray(0);
        const auto pendingSourceToken = processor.getHistorySourceToken();
        REQUIRE(pendingSourceToken != stableSourceToken);

        FireAudioProcessor::HistorySnapshot unpublishedSnapshot;
        REQUIRE_FALSE(processor.copyHistorySnapshot(unpublishedSnapshot));

        desktopHost.setVisible(true);
        REQUIRE(widthGraph.isShowing());
        CHECK(WidthGraphHistorySourceTestAccess::sourceToken(widthGraph)
              == pendingSourceToken);
        CHECK(WidthGraphHistorySourceTestAccess::isHardCleared(widthGraph));
    }

    SECTION("a cache invalidated by a hidden resize restores the newest history")
    {
        widthGraph.setBounds(20, 20, 320, 190);
        REQUIRE(WidthGraphHistorySourceTestAccess::cacheGeometryDirty(widthGraph));
        REQUIRE(WidthGraphHistorySourceTestAccess::shouldRestoreTrail(widthGraph));
        widthGraph.setBounds(20, 20, 300, 170);
        REQUIRE(WidthGraphHistorySourceTestAccess::cacheGeometryDirty(widthGraph));
        REQUIRE(WidthGraphHistorySourceTestAccess::shouldRestoreTrail(widthGraph));

        processHistoryBlock(processor, 0.7f);
        FireAudioProcessor::HistorySnapshot hiddenSnapshot;
        REQUIRE(processor.copyHistorySnapshot(hiddenSnapshot));
        REQUIRE(hiddenSnapshot.sourceToken == stableSourceToken);
        REQUIRE(hiddenSnapshot.generation > visibleGeneration);

        desktopHost.setVisible(true);
        REQUIRE(widthGraph.isShowing());
        CHECK(WidthGraphHistorySourceTestAccess::matchesSnapshot(
            widthGraph,
            hiddenSnapshot));
        CHECK(processor.getHistorySourceToken() == stableSourceToken);

        // Rebuilding the invalid cache must use the snapshot consumed by the
        // visibility callback, never the pre-hide history retained for resize.
        WidthGraphHistorySourceTestAccess::rebuildCache(widthGraph);
        CHECK(WidthGraphHistorySourceTestAccess::hasRenderableCache(widthGraph));
        CHECK(WidthGraphHistorySourceTestAccess::fadeFramesRemaining(widthGraph)
              == 24);
    }

    SECTION("zero bounds retain the newest hidden frame until geometry returns")
    {
        widthGraph.setBounds(0, 0, 0, 0);
        REQUIRE(WidthGraphHistorySourceTestAccess::cacheGeometryDirty(widthGraph));
        REQUIRE(WidthGraphHistorySourceTestAccess::shouldRestoreTrail(widthGraph));

        processHistoryBlock(processor, 0.7f);
        FireAudioProcessor::HistorySnapshot hiddenSnapshot;
        REQUIRE(processor.copyHistorySnapshot(hiddenSnapshot));
        REQUIRE(hiddenSnapshot.sourceToken == stableSourceToken);
        REQUIRE(hiddenSnapshot.generation > visibleGeneration);

        desktopHost.setVisible(true);
        REQUIRE(widthGraph.isShowing());
        CHECK(WidthGraphHistorySourceTestAccess::matchesSnapshot(
            widthGraph,
            hiddenSnapshot));
        CHECK(WidthGraphHistorySourceTestAccess::shouldRestoreTrail(widthGraph));

        widthGraph.setBounds(20, 20, 300, 170);
        CHECK(WidthGraphHistorySourceTestAccess::shouldRestoreTrail(widthGraph));
        WidthGraphHistorySourceTestAccess::rebuildCache(widthGraph);
        CHECK(WidthGraphHistorySourceTestAccess::hasRenderableCache(widthGraph));
        CHECK(WidthGraphHistorySourceTestAccess::fadeFramesRemaining(widthGraph)
              == 24);
    }
}
