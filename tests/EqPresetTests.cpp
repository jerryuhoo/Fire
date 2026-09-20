#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("EQ preset snapshots restore nodes and reject partial parameter families",
          "[eq][preset][state][regression]")
{
    FireAudioProcessor source, restored;
    source.hasUpdateCheckBeenPerformed = restored.hasUpdateCheckBeenPerformed = true;
    REQUIRE(source.addEqNode(3200, -8, fire::eq::Type::highShelf) == 3);
    REQUIRE(source.removeEqNode(0));
    juce::XmlElement preset("WINGSFIRE");
    state::saveStateToXml(source, preset);
    CHECK(preset.getIntAttribute("eqSchemaVersion") == 1);
    REQUIRE(state::canLoadStateFromXml(preset, restored));
    REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK_FALSE(restored.getEqNodeState(0).present);
    CHECK(restored.getEqNodeState(3).type == fire::eq::Type::highShelf);
    CHECK(restored.getEqNodeState(3).gainDb == Catch::Approx(-8));

    auto damaged = preset;
    damaged.removeAttribute("eqNode4Gain");
    CHECK_FALSE(state::canLoadStateFromXml(damaged, restored));
    CHECK_FALSE(state::loadStateFromXml(damaged, restored));
    CHECK(restored.getEqNodeState(3).gainDb == Catch::Approx(-8));

    auto future = preset;
    future.setAttribute("eqSchemaVersion", 2);
    CHECK_FALSE(state::canLoadStateFromXml(future, restored));

    auto legacy = preset;
    legacy.removeAttribute("eqSchemaVersion");
    for (const auto& id : fire::eq::appendedParameterIDs()) legacy.removeAttribute(id);
    REQUIRE(state::canLoadStateFromXml(legacy, restored));
    REQUIRE(state::loadStateFromXml(legacy, restored));
    for (int slot = 0; slot < fire::eq::maxNodes; ++slot)
    {
        CAPTURE(slot);
        CHECK(restored.getEqNodeState(slot).present == (slot < 3));
        CHECK(restored.getEqNodeState(slot).type == fire::eq::defaultType(slot));
    }
}
