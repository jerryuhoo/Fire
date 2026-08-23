#include <PluginProcessor.h>
#include <Panels/ControlPanel/Graph Components/VUMeter.h>

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
