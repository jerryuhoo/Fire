#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/CloudsParameters.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
struct TemporaryPresetDirectory
{
    juce::File directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("FireCloudsReservedUi-" + juce::Uuid().toString());
    ~TemporaryPresetDirectory() { directory.deleteRecursively(); }
};

void writeParameter(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void dispatchDirtyUpdates()
{
    // Exercise the actual parameter listener and 30 Hz dirty-update timer.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
}

TEST_CASE("Reserved granular engine writes do not dirty selected preset UI", "[clouds][preset][ui][reserved][dirty]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TemporaryPresetDirectory temporary;
    REQUIRE(temporary.directory.createDirectory().wasOk());
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    REQUIRE(processor.addInsertEffect(0, fire::effects::Type::granular) == 0);

    juce::XmlElement preset("WINGSFIRE");
    preset.setAttribute("presetName", "Reserved");
    state::saveStateToXml(processor, preset);
    REQUIRE(preset.writeTo(temporary.directory.getChildFile("Reserved.fire")));
    state::StatePresets presets(processor, temporary.directory.getFullPathName());
    presets.setCurrentPresetKey("Reserved.fire");
    state::StateComponent component(processor.stateAB, presets, processor.treeState);
    component.synchronisePresetSelectionFromManager();
    dispatchDirtyUpdates();

    auto* box = component.getPresetBox();
    REQUIRE(box != nullptr);
    const auto selected = box->getSelectedId();
    REQUIRE(selected > 0);
    REQUIRE(box->getText() == "Reserved");

    for (int scope : {0, 1})
        for (float value : {0.0f, 1.0f})
        {
            CAPTURE(scope, value);
            writeParameter(processor, fire::clouds_params::parameterID(
                scope, 0, fire::clouds_params::engineField), value);
            dispatchDirtyUpdates();
            CHECK(box->getSelectedId() == selected);
            CHECK(box->getText() == "Reserved");
        }

    // A real Granular Size change must still use the usual dirty mechanism.
    writeParameter(processor, fire::effects::parameterID(0, 0, 0), 0.75f);
    dispatchDirtyUpdates();
    CHECK(box->getSelectedId() == 0);
    CHECK(box->getText() == "Reserved*");
}
