#include <PluginProcessor.h>
#include <Panels/ControlPanel/BandPanel.h>

#include <catch2/catch_test_macros.hpp>

struct BandPanelGraphTestAccess
{
    static void clearDirtyBands(BandPanel& panel)
    {
        panel.distortionGraphDirtyMask.store(0, std::memory_order_release);
    }

    static unsigned int dirtyBands(const BandPanel& panel)
    {
        return panel.distortionGraphDirtyMask.load(std::memory_order_acquire);
    }
};

namespace
{
void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}
} // namespace

TEST_CASE("Shape enable invalidates the stopped BandPanel transfer graph",
          "[band-panel][graph][shape][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    BandPanel panel(processor, {}, {}, {}, {}, {});

    BandPanelGraphTestAccess::clearDirtyBands(panel);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, 0),
                      1.0f);
    CHECK(BandPanelGraphTestAccess::dirtyBands(panel) == 0b0001u);

    BandPanelGraphTestAccess::clearDirtyBands(panel);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, 2),
                      1.0f);
    CHECK(BandPanelGraphTestAccess::dirtyBands(panel) == 0b0100u);
}
