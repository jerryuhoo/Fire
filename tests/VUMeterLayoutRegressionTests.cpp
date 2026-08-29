#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <Panels/ControlPanel/Graph Components/VUMeter.h>
#include <Panels/ControlPanel/Graph Components/VUPanel.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>

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

    static void updateBallistics(VUMeter& meter,
                                 float rmsLeft,
                                 float rmsRight,
                                 float peakLeft,
                                 float peakRight)
    {
        meter.updateBallistics(rmsLeft, rmsRight, peakLeft, peakRight);
    }

    static float peakHoldLeft(const VUMeter& meter)
    {
        return meter.mPeakHoldCh0Level;
    }

    static float peakHoldRight(const VUMeter& meter)
    {
        return meter.mPeakHoldCh1Level;
    }

    static constexpr int peakHoldFrameCount()
    {
        return VUMeter::peakHoldFrames;
    }

    static int peakHoldCounterLeft(const VUMeter& meter)
    {
        return meter.mPeakHoldCh0DecayCounter;
    }

    static int peakHoldCounterRight(const VUMeter& meter)
    {
        return meter.mPeakHoldCh1DecayCounter;
    }

    static void reset(VUMeter& meter)
    {
        meter.resetLevels();
    }

    static std::array<float, 6> levels(const VUMeter& meter)
    {
        return {
            meter.mRmsCh0Level,
            meter.mRmsCh1Level,
            meter.mPeakCh0Level,
            meter.mPeakCh1Level,
            meter.mPeakHoldCh0Level,
            meter.mPeakHoldCh1Level
        };
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

    static float inputPeakRight(const VUPanel& panel)
    {
        return panel.vuMeterIn.getPeakRightChannelLevel();
    }

    static float outputRms(const VUPanel& panel)
    {
        return panel.vuMeterOut.getRmsLeftChannelLevel();
    }

    static float outputPeak(const VUPanel& panel)
    {
        return panel.vuMeterOut.getPeakLeftChannelLevel();
    }

    static float outputPeakRight(const VUPanel& panel)
    {
        return panel.vuMeterOut.getPeakRightChannelLevel();
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

TEST_CASE("VU meter peak holds decay independently per channel",
          "[ui][meter][ballistics][channels][regression]")
{
    FireAudioProcessor processor;
    VUMeter meter(&processor);

    const auto tick = [&meter](float peakLeft, float peakRight)
    {
        VUMeterTestAccess::updateBallistics(meter,
                                            0.0f,
                                            0.0f,
                                            peakLeft,
                                            peakRight);
    };
    const auto silenceTicks = [&tick](int count)
    {
        for (int frame = 0; frame < count; ++frame)
            tick(0.0f, 0.0f);
    };

    SECTION("a new left peak does not inherit right-channel decay")
    {
        tick(0.0f, 1.0f);
        silenceTicks(VUMeterTestAccess::peakHoldFrameCount() + 1);
        const auto decayingRight = VUMeterTestAccess::peakHoldRight(meter);
        REQUIRE(decayingRight < 1.0f);

        tick(1.0f, 0.0f);
        CHECK(VUMeterTestAccess::peakHoldLeft(meter) == Catch::Approx(1.0f));
        CHECK(VUMeterTestAccess::peakHoldRight(meter) < decayingRight);

        const auto rightAfterLeftPeak = VUMeterTestAccess::peakHoldRight(meter);
        silenceTicks(VUMeterTestAccess::peakHoldFrameCount());
        CHECK(VUMeterTestAccess::peakHoldLeft(meter) == Catch::Approx(1.0f));
        CHECK(VUMeterTestAccess::peakHoldRight(meter) < rightAfterLeftPeak);

        tick(0.0f, 0.0f);
        CHECK(VUMeterTestAccess::peakHoldLeft(meter) < 1.0f);
    }

    SECTION("a new right peak does not inherit left-channel decay")
    {
        tick(1.0f, 0.0f);
        silenceTicks(VUMeterTestAccess::peakHoldFrameCount() + 1);
        const auto decayingLeft = VUMeterTestAccess::peakHoldLeft(meter);
        REQUIRE(decayingLeft < 1.0f);

        tick(0.0f, 1.0f);
        CHECK(VUMeterTestAccess::peakHoldRight(meter) == Catch::Approx(1.0f));
        CHECK(VUMeterTestAccess::peakHoldLeft(meter) < decayingLeft);

        const auto leftAfterRightPeak = VUMeterTestAccess::peakHoldLeft(meter);
        silenceTicks(VUMeterTestAccess::peakHoldFrameCount());
        CHECK(VUMeterTestAccess::peakHoldRight(meter) == Catch::Approx(1.0f));
        CHECK(VUMeterTestAccess::peakHoldLeft(meter) < leftAfterRightPeak);

        tick(0.0f, 0.0f);
        CHECK(VUMeterTestAccess::peakHoldRight(meter) < 1.0f);
    }

    SECTION("new and equal peaks restart a channel's full hold interval")
    {
        tick(0.6f, 0.0f);
        silenceTicks(3);
        REQUIRE(VUMeterTestAccess::peakHoldCounterLeft(meter) == 3);

        tick(0.8f, 0.0f);
        CHECK(VUMeterTestAccess::peakHoldLeft(meter)
              == Catch::Approx(0.8f));
        CHECK(VUMeterTestAccess::peakHoldCounterLeft(meter) == 0);

        silenceTicks(3);
        REQUIRE(VUMeterTestAccess::peakHoldCounterLeft(meter) == 3);
        tick(0.8f, 0.0f);
        CHECK(VUMeterTestAccess::peakHoldLeft(meter)
              == Catch::Approx(0.8f));
        CHECK(VUMeterTestAccess::peakHoldCounterLeft(meter) == 0);

        for (int frame = 0;
             frame < VUMeterTestAccess::peakHoldFrameCount() + 5;
             ++frame)
        {
            tick(1.0f, 0.0f);
            CHECK(VUMeterTestAccess::peakHoldLeft(meter)
                  == Catch::Approx(1.0f));
            CHECK(VUMeterTestAccess::peakHoldCounterLeft(meter) == 0);
        }
    }

    SECTION("reset clears both peak-hold states")
    {
        tick(1.0f, 0.75f);
        silenceTicks(3);
        REQUIRE(VUMeterTestAccess::peakHoldLeft(meter) > 0.0f);
        REQUIRE(VUMeterTestAccess::peakHoldRight(meter) > 0.0f);
        REQUIRE(VUMeterTestAccess::peakHoldCounterLeft(meter) > 0);
        REQUIRE(VUMeterTestAccess::peakHoldCounterRight(meter) > 0);

        VUMeterTestAccess::reset(meter);
        CHECK(VUMeterTestAccess::peakHoldLeft(meter) == 0.0f);
        CHECK(VUMeterTestAccess::peakHoldRight(meter) == 0.0f);
        CHECK(VUMeterTestAccess::peakHoldCounterLeft(meter) == 0);
        CHECK(VUMeterTestAccess::peakHoldCounterRight(meter) == 0);

        silenceTicks(VUMeterTestAccess::peakHoldFrameCount() + 2);
        CHECK(VUMeterTestAccess::peakHoldLeft(meter) == 0.0f);
        CHECK(VUMeterTestAccess::peakHoldRight(meter) == 0.0f);
    }
}

TEST_CASE("VU meter contains invalid telemetry and recovers its ballistics",
          "[ui][meter][ballistics][robustness][regression]")
{
    FireAudioProcessor processor;
    VUMeter meter(&processor);

    const auto requireBoundedLevels = [&meter]
    {
        for (const auto level : VUMeterTestAccess::levels(meter))
        {
            REQUIRE(std::isfinite(level));
            REQUIRE(level >= 0.0f);
            REQUIRE(level <= 1.0f);
        }
    };

    struct Probe
    {
        const char* name;
        float value;
    };
    const std::array probes {
        Probe { "+Inf", std::numeric_limits<float>::infinity() },
        Probe { "NaN", std::numeric_limits<float>::quiet_NaN() },
        Probe { "-Inf", -std::numeric_limits<float>::infinity() },
        Probe { "negative", -1.0f },
        Probe { "huge finite", std::numeric_limits<float>::max() }
    };

    for (const auto& probe : probes)
    {
        DYNAMIC_SECTION(probe.name)
        {
            VUMeterTestAccess::reset(meter);

            MeterValues invalidValues;
            invalidValues.inputRMS_L = probe.value;
            invalidValues.inputRMS_R = probe.value;
            invalidValues.inputPeak_L = probe.value;
            invalidValues.inputPeak_R = probe.value;
            meter.updateLevels(invalidValues);
            requireBoundedLevels();

            MeterValues normalValues;
            normalValues.inputRMS_L = 0.25f;
            normalValues.inputRMS_R = 0.5f;
            normalValues.inputPeak_L = 0.5f;
            normalValues.inputPeak_R = 1.0f;
            meter.updateLevels(normalValues);
            requireBoundedLevels();
            REQUIRE(VUMeterTestAccess::peakHoldLeft(meter) > 0.0f);
            REQUIRE(VUMeterTestAccess::peakHoldRight(meter) > 0.0f);

            for (int frame = 0;
                 frame < VUMeterTestAccess::peakHoldFrameCount() + 200;
                 ++frame)
            {
                meter.decayToSilence();
                requireBoundedLevels();
            }

            for (const auto level : VUMeterTestAccess::levels(meter))
                CHECK(level == Catch::Approx(0.0f).margin(1.0e-6f));
        }
    }
}

TEST_CASE("VU readouts report the loudest visible channel and remain mono-safe",
          "[ui][meter][readout][channels][regression]")
{
    SECTION("right-only stereo signal")
    {
        FireAudioProcessor processor;
        setChannelLayout(processor, 2);
        VUPanel panel(processor);

        MeterValues values;
        values.bandInputRMS_R[0] = 0.5f;
        values.bandInputPeak_R[0] = 1.0f;
        values.bandOutputRMS_R[0] = 0.125f;
        values.bandOutputPeak_R[0] = 0.25f;
        VUPanelTestAccess::applyLevels(panel, values);

        REQUIRE(VUPanelTestAccess::inputPeakRight(panel) > 0.99f);
        REQUIRE(VUPanelTestAccess::outputPeakRight(panel) > 0.87f);
        CHECK(VUPanelTestAccess::inputPeakText(panel) == "0.0");
        CHECK(VUPanelTestAccess::inputRmsText(panel) == "-6.0");
        CHECK(VUPanelTestAccess::outputPeakText(panel) == "-12.0");
        CHECK(VUPanelTestAccess::outputRmsText(panel) == "-18.1");
    }

    SECTION("mono ignores an unused right-channel payload")
    {
        FireAudioProcessor processor;
        setChannelLayout(processor, 1);
        VUPanel panel(processor);

        MeterValues values;
        values.bandInputRMS_L[0] = 0.25f;
        values.bandInputPeak_L[0] = 0.5f;
        values.bandInputRMS_R[0] = 1.0f;
        values.bandInputPeak_R[0] = 1.0f;
        values.bandOutputRMS_L[0] = 0.125f;
        values.bandOutputPeak_L[0] = 0.25f;
        values.bandOutputRMS_R[0] = 1.0f;
        values.bandOutputPeak_R[0] = 1.0f;
        VUPanelTestAccess::applyLevels(panel, values);

        CHECK(VUPanelTestAccess::inputPeakText(panel) == "-6.0");
        CHECK(VUPanelTestAccess::inputRmsText(panel) == "-12.0");
        CHECK(VUPanelTestAccess::outputPeakText(panel) == "-12.0");
        CHECK(VUPanelTestAccess::outputRmsText(panel) == "-18.1");
    }
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

TEST_CASE("Global-only meter packets do not refresh band presentations",
          "[ui][meter][freshness][host-bypass][regression]")
{
    FireAudioProcessor processor;

    VUPanel bandPanel(processor);
    MeterValues freshBandPacket;
    freshBandPacket.bandInputRMS_L[0] = 1.0f;
    freshBandPacket.bandInputPeak_L[0] = 1.0f;
    freshBandPacket.bandOutputRMS_L[0] = 0.8f;
    freshBandPacket.bandOutputPeak_L[0] = 0.9f;

    VUPanelTestAccess::setGraphShowing(bandPanel, true);
    VUPanelTestAccess::present(bandPanel, freshBandPacket, 1);
    REQUIRE(VUPanelTestAccess::inputRms(bandPanel) > 0.99f);
    REQUIRE(VUPanelTestAccess::outputRms(bandPanel) > 0.95f);

    const auto inputBeforeGlobalOnlyPacket =
        VUPanelTestAccess::inputRms(bandPanel);
    const auto outputBeforeGlobalOnlyPacket =
        VUPanelTestAccess::outputRms(bandPanel);
    VUPanelTestAccess::setStaleTimerTicks(bandPanel, 3);

    MeterValues globalOnlyPacket = freshBandPacket;
    globalOnlyPacket.bandLevelsAreFresh = false;
    VUPanelTestAccess::present(bandPanel, globalOnlyPacket, 2);

    CHECK(VUPanelTestAccess::staleTimerTicks(bandPanel) == 3);
    CHECK(VUPanelTestAccess::inputRms(bandPanel)
          == Catch::Approx(inputBeforeGlobalOnlyPacket));
    CHECK(VUPanelTestAccess::outputRms(bandPanel)
          == Catch::Approx(outputBeforeGlobalOnlyPacket));

    // Rejecting a global-only packet must not consume this source's
    // generation. A complete replacement with the same generation is valid.
    globalOnlyPacket.bandLevelsAreFresh = true;
    globalOnlyPacket.bandInputRMS_L[0] = 0.0f;
    globalOnlyPacket.bandInputPeak_L[0] = 0.0f;
    globalOnlyPacket.bandOutputRMS_L[0] = 0.0f;
    globalOnlyPacket.bandOutputPeak_L[0] = 0.0f;
    VUPanelTestAccess::present(bandPanel, globalOnlyPacket, 2);

    CHECK(VUPanelTestAccess::staleTimerTicks(bandPanel) == 0);
    CHECK(VUPanelTestAccess::inputRms(bandPanel)
          < inputBeforeGlobalOnlyPacket);
    CHECK(VUPanelTestAccess::outputRms(bandPanel)
          < outputBeforeGlobalOnlyPacket);
    VUPanelTestAccess::setGraphShowing(bandPanel, false);

    VUPanel globalPanel(processor);
    globalPanel.setFocusBandNum(-1);
    MeterValues freshGlobalPacket;
    freshGlobalPacket.bandLevelsAreFresh = false;
    freshGlobalPacket.inputRMS_L = 1.0f;
    freshGlobalPacket.inputPeak_L = 1.0f;
    freshGlobalPacket.outputRMS_L = 0.75f;
    freshGlobalPacket.outputPeak_L = 0.9f;

    VUPanelTestAccess::setGraphShowing(globalPanel, true);
    VUPanelTestAccess::present(globalPanel, freshGlobalPacket, 1);
    CHECK(VUPanelTestAccess::inputRms(globalPanel) > 0.99f);
    CHECK(VUPanelTestAccess::outputRms(globalPanel) > 0.95f);
    VUPanelTestAccess::setGraphShowing(globalPanel, false);
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
