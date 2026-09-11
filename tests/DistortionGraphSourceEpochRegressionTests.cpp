#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cmath>
#include <memory>

struct DistortionGraphSourceEpochTestAccess
{
    static void showDrivePreview(FireAudioProcessorEditor& editor)
    {
        editor.bandPanel.setGraphVisibilityForDriveDrag(true);
    }

    static void tickParameterPreview(FireAudioProcessorEditor& editor)
    {
        editor.bandPanel.timerCallback();
    }

    static void expireTelemetry(FireAudioProcessorEditor& editor)
    {
        editor.bandPanel.lastGraphTelemetryTimeMs -= 1000.0;
    }
    static void seedPresentedDrive(FireAudioProcessorEditor& editor,
                                   float drive)
    {
        editor.bandPanel.getDistortionGraph()->setState(0,
                                                        0.0f,
                                                        1.0f,
                                                        0.0f,
                                                        drive,
                                                        1.0f);
    }

    static float presentedDrive(FireAudioProcessorEditor& editor)
    {
        return editor.bandPanel.getDistortionGraph()->drive;
    }

    static bool curveIsDirty(const DistortionGraph& graph)
    {
        return graph.curveDirty;
    }

    static juce::Rectangle<float> curveBounds(const DistortionGraph& graph)
    {
        return graph.distortionCurve.getBounds();
    }
};

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 64;
constexpr std::uint64_t bandMask = 0x3u;

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void publishGraphPacket(FireAudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}

int packetBand(const DistortionGraphValues& values)
{
    return static_cast<int>(values.sourceToken & bandMask);
}
} // namespace

TEST_CASE("Distortion graph rejects queued packets after A to B focus changes",
          "[processor][graph-telemetry][source-epoch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(MODE_ID, 0),
                      2.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(MODE_ID, 1),
                      7.0f);

    processor.setUiFocusBand(0);
    publishGraphPacket(processor);

    processor.setUiFocusBand(1);
    DistortionGraphValues values;
    CHECK_FALSE(processor.getLatestDistortionGraphValues(values));
    // A rejected packet must still be consumed rather than blocking the FIFO.
    CHECK_FALSE(processor.getLatestDistortionGraphValues(values));

    publishGraphPacket(processor);
    REQUIRE(processor.getLatestDistortionGraphValues(values));
    CHECK(packetBand(values) == 1);
    CHECK(values.mode == 7);
}

TEST_CASE("Distortion graph source generations reject old A after A to B to A",
          "[processor][graph-telemetry][source-epoch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    processor.setUiFocusBand(0);
    publishGraphPacket(processor);

    DistortionGraphValues firstA;
    REQUIRE(processor.getLatestDistortionGraphValues(firstA));
    REQUIRE(packetBand(firstA) == 0);

    // Queue another packet from the first A epoch, then leave and return to A
    // without giving DSP an opportunity to publish for either new epoch.
    publishGraphPacket(processor);
    processor.setUiFocusBand(1);
    processor.setUiFocusBand(0);

    DistortionGraphValues currentA;
    CHECK_FALSE(processor.getLatestDistortionGraphValues(currentA));
    CHECK_FALSE(processor.getLatestDistortionGraphValues(currentA));

    // The first audio callback from the new A epoch restores telemetry, but
    // its token must differ from the earlier visit to the same band.
    publishGraphPacket(processor);
    REQUIRE(processor.getLatestDistortionGraphValues(currentA));
    CHECK(packetBand(currentA) == 0);
    CHECK(currentA.sourceToken != firstA.sourceToken);
}

TEST_CASE("Hidden editors drain distortion telemetry without presenting it",
          "[ui][editor][graph-telemetry][source-epoch][freshness]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.prepareToPlay(sampleRate, blockSize);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    REQUIRE_FALSE(editor->isShowing());

    constexpr float sentinelDrive = 0.375f;
    DistortionGraphSourceEpochTestAccess::seedPresentedDrive(*editor,
                                                             sentinelDrive);
    REQUIRE(DistortionGraphSourceEpochTestAccess::presentedDrive(*editor)
            == Catch::Approx(sentinelDrive));

    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0),
                      1.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(SAFE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(DRIVE_ID, 0),
                      100.0f);
    publishGraphPacket(processor);

    editor->timerCallback();
    CHECK(DistortionGraphSourceEpochTestAccess::presentedDrive(*editor)
          == Catch::Approx(sentinelDrive));

    DistortionGraphValues drainedValues;
    CHECK_FALSE(processor.getLatestDistortionGraphValues(drainedValues));
}

TEST_CASE("Distortion graph rebuilds a curve resized behind a hidden ancestor",
          "[ui][graph][visibility][resize][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    juce::Component desktopHost;
    DistortionGraph graph(processor);

    desktopHost.setVisible(false);
    desktopHost.setBounds(0, 0, 640, 360);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.addAndMakeVisible(graph);
    graph.setBounds(10, 10, 240, 140);

    desktopHost.setVisible(true);
    REQUIRE(graph.isShowing());
    REQUIRE_FALSE(
        DistortionGraphSourceEpochTestAccess::curveIsDirty(graph));
    const auto initialCurveBounds =
        DistortionGraphSourceEpochTestAccess::curveBounds(graph);
    REQUIRE_FALSE(initialCurveBounds.isEmpty());

    desktopHost.setVisible(false);
    REQUIRE_FALSE(graph.isShowing());
    graph.setBounds(10, 10, 400, 220);
    REQUIRE(DistortionGraphSourceEpochTestAccess::curveIsDirty(graph));
    CHECK(DistortionGraphSourceEpochTestAccess::curveBounds(graph)
          == initialCurveBounds);

    desktopHost.setVisible(true);
    REQUIRE(graph.isShowing());
    CHECK_FALSE(DistortionGraphSourceEpochTestAccess::curveIsDirty(graph));
    CHECK(DistortionGraphSourceEpochTestAccess::curveBounds(graph).getWidth()
          > initialCurveBounds.getWidth());
}

TEST_CASE("Drive edits cannot alternate the transfer graph between base and LFO values",
          "[ui][graph-telemetry][lfo][drive][regression]")
{
    class PlayHead final : public juce::AudioPlayHead
    {
    public:
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo position;
            position.setIsPlaying(playing);
            position.setBpm(120.0);
            position.setPpqPosition(0.0);
            return position;
        }
        bool playing = false;
    };

    for (const bool playing : { false, true })
    {
        CAPTURE(playing);
        PlayHead playHead;
        playHead.playing = playing;
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        processor.setPlayHead(&playHead);
        const auto driveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        setPlainParameter(processor, driveID, 20.0f);
        setPlainParameter(processor, ParameterIDAndName::getIDString(SAFE_ID, 0), 0.0f);
        setPlainParameter(processor, ParameterIDAndName::getIDString(EXTREME_ID, 0), 0.0f);
        setPlainParameter(processor, ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0), 1.0f);
        LfoData shape;
        shape.points = { {0.0f, 1.0f}, {1.0f, 1.0f} };
        processor.getLfoManager().setLfoData(0, shape);
        processor.assignLfoToTarget(0, driveID);
        processor.setModulationDepth(driveID, 0.4f);
        processor.prepareToPlay(sampleRate, blockSize);

        FireAudioProcessorEditor editor(processor);
        editor.setSize(1000, 500);
        editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor.setVisible(true);
        DistortionGraphSourceEpochTestAccess::showDrivePreview(editor);
        for (int block = 0; block < 20; ++block)
            publishGraphPacket(processor);
        editor.timerCallback();
        CHECK(processor.isDawPlaying() == playing);

        for (const float base : { 21.0f, 24.0f, 30.0f, 38.0f })
        {
            const auto beforeEdit = DistortionGraphSourceEpochTestAccess::presentedDrive(editor);
            setPlainParameter(processor, driveID, base);
            DistortionGraphSourceEpochTestAccess::tickParameterPreview(editor);
            CHECK(DistortionGraphSourceEpochTestAccess::presentedDrive(editor) == beforeEdit);

            publishGraphPacket(processor);
            editor.timerCallback();
            const auto modulated = DistortionGraphSourceEpochTestAccess::presentedDrive(editor);
            CHECK(modulated == Catch::Approx(std::exp2((base + 20.0f) * 6.5f / 100.0f)).margin(0.001f));
            DistortionGraphSourceEpochTestAccess::tickParameterPreview(editor);
            CHECK(DistortionGraphSourceEpochTestAccess::presentedDrive(editor) == modulated);
        }

        // The pending edit must remain available when audio callbacks stop.
        setPlainParameter(processor, driveID, 10.0f);
        DistortionGraphSourceEpochTestAccess::expireTelemetry(editor);
        DistortionGraphSourceEpochTestAccess::tickParameterPreview(editor);
        CHECK(DistortionGraphSourceEpochTestAccess::presentedDrive(editor)
              == Catch::Approx(std::exp2(10.0f * 6.5f / 100.0f)).margin(0.001f));
    }
}
