#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <Panels/ControlPanel/Graph Components/VUMeter.h>
#include <Panels/ControlPanel/Graph Components/VUPanel.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

struct VUMeterTestAccess
{
    static juce::Rectangle<int> leftBounds(const VUMeter& meter)
    {
        return meter.leftMeterBounds;
    }

    static juce::Rectangle<int> rightBounds(const VUMeter& meter)
    {
        return meter.rightMeterBounds;
    }
};

struct VUPanelTestAccess
{
    static void applyLevels(VUPanel& panel, const MeterValues& values)
    {
        panel.vuMeterIn.updateLevels(values);
        panel.vuMeterOut.updateLevels(values);
        panel.refreshReadoutText();
    }

    static void setStaleTimerTicks(VUPanel& panel, int ticks)
    {
        panel.staleTimerTicks = ticks;
    }

    static int staleTimerTicks(const VUPanel& panel)
    {
        return panel.staleTimerTicks;
    }

    static float inputRms(const VUPanel& panel)
    {
        return panel.vuMeterIn.getRmsLeftChannelLevel();
    }

    static float inputPeak(const VUPanel& panel)
    {
        return panel.vuMeterIn.getPeakLeftChannelLevel();
    }

    static float outputRms(const VUPanel& panel)
    {
        return panel.vuMeterOut.getRmsLeftChannelLevel();
    }

    static float outputPeak(const VUPanel& panel)
    {
        return panel.vuMeterOut.getPeakLeftChannelLevel();
    }

    static const juce::String& inputPeakText(const VUPanel& panel)
    {
        return panel.inputPeakText;
    }

    static const juce::String& inputRmsText(const VUPanel& panel)
    {
        return panel.inputRmsText;
    }

    static const juce::String& outputPeakText(const VUPanel& panel)
    {
        return panel.outputPeakText;
    }

    static const juce::String& outputRmsText(const VUPanel& panel)
    {
        return panel.outputRmsText;
    }

    static void setGraphShowing(VUPanel& panel, bool isShowing)
    {
        panel.graphShowingStateChanged(isShowing);
    }

    static void present(VUPanel& panel,
                        const MeterValues& values,
                        std::uint64_t generation)
    {
        panel.presentMeterValues(values, generation);
    }
};

struct MeterFreshnessTestAccess
{
    static bool hasCachedMeterValues(const FireAudioProcessorEditor& editor)
    {
        return editor.hasCachedMeterValues;
    }

    static std::uint64_t meterPacketGeneration(
        const FireAudioProcessorEditor& editor)
    {
        return editor.meterPacketGeneration;
    }

    static float cachedInputPeak(const FireAudioProcessorEditor& editor)
    {
        return editor.cachedMeterValues.inputPeak_L;
    }
};

namespace
{
void setChannelLayout(FireAudioProcessor& processor, int channelCount)
{
    REQUIRE((channelCount == 1 || channelCount == 2));
    const auto channelSet = channelCount == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    REQUIRE(processor.setBusesLayout(layout));
}

void paintMeter(VUMeter& meter)
{
    juce::Image image(juce::Image::ARGB,
                      juce::jmax(1, meter.getWidth()),
                      juce::jmax(1, meter.getHeight()),
                      true);
    juce::Graphics graphics(image);
    meter.paint(graphics);
}

void publishBypassedMeterPacket(FireAudioProcessor& processor,
                                float amplitude)
{
    constexpr int blockSize = 64;
    juce::AudioBuffer<float> buffer(2, blockSize);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample,
                             amplitude * (channel == 0 ? 1.0f : 0.5f));

    juce::MidiBuffer midi;
    processor.processBlockBypassed(buffer, midi);
}
} // namespace

TEST_CASE("VU meter updates bar bounds when the host channel layout changes",
          "[ui][meter][layout]")
{
    FireAudioProcessor processor;
    setChannelLayout(processor, 2);

    VUMeter meter(&processor);
    meter.setBounds(0, 0, 30, 120);
    paintMeter(meter);

    const auto componentBounds = meter.getBounds();
    CHECK(VUMeterTestAccess::leftBounds(meter)
          == juce::Rectangle<int>(0, 0, 10, 120));
    CHECK(VUMeterTestAccess::rightBounds(meter)
          == juce::Rectangle<int>(20, 0, 10, 120));

    setChannelLayout(processor, 1);
    REQUIRE(meter.getBounds() == componentBounds);
    paintMeter(meter);

    CHECK(VUMeterTestAccess::leftBounds(meter)
          == juce::Rectangle<int>(10, 0, 10, 120));
    CHECK(VUMeterTestAccess::rightBounds(meter).isEmpty());

    setChannelLayout(processor, 2);
    REQUIRE(meter.getBounds() == componentBounds);
    paintMeter(meter);

    CHECK(VUMeterTestAccess::leftBounds(meter)
          == juce::Rectangle<int>(0, 0, 10, 120));
    CHECK(VUMeterTestAccess::rightBounds(meter)
          == juce::Rectangle<int>(20, 0, 10, 120));
}

TEST_CASE("VU panel clears ballistics and readouts when its focus band changes",
          "[ui][meter][focus]")
{
    FireAudioProcessor processor;
    VUPanel panel(processor);

    MeterValues loudBand;
    loudBand.bandInputRMS_L[0] = 1.0f;
    loudBand.bandInputPeak_L[0] = 1.0f;
    loudBand.bandOutputRMS_L[0] = 0.75f;
    loudBand.bandOutputPeak_L[0] = 0.9f;
    VUPanelTestAccess::applyLevels(panel, loudBand);

    REQUIRE(VUPanelTestAccess::inputRms(panel) > 0.9f);
    REQUIRE(VUPanelTestAccess::inputPeak(panel) > 0.9f);
    REQUIRE(VUPanelTestAccess::outputRms(panel) > 0.9f);
    REQUIRE(VUPanelTestAccess::outputPeak(panel) > 0.9f);
    REQUIRE(VUPanelTestAccess::inputPeakText(panel) != "-96.0");
    REQUIRE(VUPanelTestAccess::outputPeakText(panel) != "-96.0");

    VUPanelTestAccess::setStaleTimerTicks(panel, 9);
    panel.setFocusBandNum(1);

    CHECK(VUPanelTestAccess::inputRms(panel) == 0.0f);
    CHECK(VUPanelTestAccess::inputPeak(panel) == 0.0f);
    CHECK(VUPanelTestAccess::outputRms(panel) == 0.0f);
    CHECK(VUPanelTestAccess::outputPeak(panel) == 0.0f);
    CHECK(VUPanelTestAccess::inputPeakText(panel) == "-96.0");
    CHECK(VUPanelTestAccess::inputRmsText(panel) == "-96.0");
    CHECK(VUPanelTestAccess::outputPeakText(panel) == "-96.0");
    CHECK(VUPanelTestAccess::outputRmsText(panel) == "-96.0");
    CHECK(VUPanelTestAccess::staleTimerTicks(panel) == 0);
}

TEST_CASE("VU panel never restores same-band ballistics without a newer packet",
          "[ui][meter][freshness]")
{
    FireAudioProcessor processor;
    VUPanel panel(processor);

    MeterValues firstLoudPacket;
    firstLoudPacket.bandInputRMS_L[0] = 1.0f;
    firstLoudPacket.bandInputPeak_L[0] = 1.0f;
    firstLoudPacket.bandOutputRMS_L[0] = 0.8f;
    firstLoudPacket.bandOutputPeak_L[0] = 0.9f;

    VUPanelTestAccess::setGraphShowing(panel, true);
    VUPanelTestAccess::present(panel, firstLoudPacket, 1);
    REQUIRE(VUPanelTestAccess::inputPeak(panel) > 0.9f);
    REQUIRE(VUPanelTestAccess::outputPeak(panel) > 0.9f);

    VUPanelTestAccess::setGraphShowing(panel, false);
    REQUIRE(VUPanelTestAccess::inputPeak(panel) == 0.0f);
    REQUIRE(VUPanelTestAccess::outputPeak(panel) == 0.0f);

    // Showing the same band again must not replay the last presented packet.
    VUPanelTestAccess::setGraphShowing(panel, true);
    VUPanelTestAccess::present(panel, firstLoudPacket, 1);
    CHECK(VUPanelTestAccess::inputPeak(panel) == 0.0f);
    CHECK(VUPanelTestAccess::outputPeak(panel) == 0.0f);
    CHECK(VUPanelTestAccess::inputPeakText(panel) == "-96.0");
    CHECK(VUPanelTestAccess::outputPeakText(panel) == "-96.0");

    // A packet drained while the graph is hidden is current when it returns.
    MeterValues secondLoudPacket = firstLoudPacket;
    secondLoudPacket.bandInputPeak_L[0] = 0.7f;
    secondLoudPacket.bandOutputPeak_L[0] = 0.6f;
    VUPanelTestAccess::setGraphShowing(panel, false);
    VUPanelTestAccess::present(panel, secondLoudPacket, 2);
    VUPanelTestAccess::setGraphShowing(panel, true);
    VUPanelTestAccess::present(panel, secondLoudPacket, 2);
    CHECK(VUPanelTestAccess::inputPeak(panel) > 0.9f);
    CHECK(VUPanelTestAccess::outputPeak(panel) > 0.9f);

    VUPanelTestAccess::setGraphShowing(panel, false);
}

TEST_CASE("Hidden and reopened editors establish fresh meter epochs",
          "[ui][editor][meter][freshness]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setChannelLayout(processor, 2);
    processor.prepareToPlay(48000.0, 64);
    processor.hasUpdateCheckBeenPerformed = true;

    // A packet produced before the editor exists is backlog, not a value the
    // newly opened UI may animate through.
    publishBypassedMeterPacket(processor, 0.8f);
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    REQUIRE_FALSE(MeterFreshnessTestAccess::hasCachedMeterValues(*editor));
    REQUIRE(MeterFreshnessTestAccess::meterPacketGeneration(*editor) == 0);

    MeterValues packet;
    CHECK_FALSE(processor.getLatestMeterValues(packet));

    // The editor timer remains the sole FIFO reader even while its peer is
    // hidden, retaining only the newest packet for a future visible frame.
    publishBypassedMeterPacket(processor, 0.35f);
    editor->timerCallback();
    REQUIRE(MeterFreshnessTestAccess::hasCachedMeterValues(*editor));
    REQUIRE(MeterFreshnessTestAccess::meterPacketGeneration(*editor) == 1);
    CHECK(MeterFreshnessTestAccess::cachedInputPeak(*editor)
          == Catch::Approx(0.35f));
    CHECK_FALSE(processor.getLatestMeterValues(packet));

    publishBypassedMeterPacket(processor, 0.55f);
    editor->timerCallback();
    REQUIRE(MeterFreshnessTestAccess::meterPacketGeneration(*editor) == 2);
    CHECK(MeterFreshnessTestAccess::cachedInputPeak(*editor)
          == Catch::Approx(0.55f));
    editor.reset();

    // Telemetry can accumulate while no editor exists. Reopening drains that
    // backlog but deliberately keeps the new cache invalid until DSP publishes
    // a post-open packet.
    publishBypassedMeterPacket(processor, 0.9f);
    editor = std::make_unique<FireAudioProcessorEditor>(processor);
    REQUIRE_FALSE(MeterFreshnessTestAccess::hasCachedMeterValues(*editor));
    REQUIRE(MeterFreshnessTestAccess::meterPacketGeneration(*editor) == 0);
    editor->timerCallback();
    CHECK_FALSE(MeterFreshnessTestAccess::hasCachedMeterValues(*editor));

    publishBypassedMeterPacket(processor, 0.0f);
    editor->timerCallback();
    REQUIRE(MeterFreshnessTestAccess::hasCachedMeterValues(*editor));
    REQUIRE(MeterFreshnessTestAccess::meterPacketGeneration(*editor) == 1);
    CHECK(MeterFreshnessTestAccess::cachedInputPeak(*editor)
          == Catch::Approx(0.0f).margin(1.0e-7f));
}
