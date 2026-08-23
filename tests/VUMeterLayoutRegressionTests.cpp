#include <PluginProcessor.h>
#include <Panels/ControlPanel/Graph Components/VUMeter.h>
#include <Panels/ControlPanel/Graph Components/VUPanel.h>

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
