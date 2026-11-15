#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/PresetParameterLocks.h>
#include "LegacyEnvelopeFixture.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
void lockedSet(FireAudioProcessor& p,const juce::String& id,float value)
{auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
float lockedGet(FireAudioProcessor& p,const juce::String& id)
{auto* parameter=p.treeState.getRawParameterValue(id);REQUIRE(parameter);return parameter->load();}
juce::XmlElement lockedPreset(FireAudioProcessor& p)
{juce::XmlElement xml("WINGSFIRE");state::saveStateToXml(p,xml);return xml;}
}

TEST_CASE("Only user preset loading retains locked base values while taking the incoming routes",
          "[preset-locks][preset][state][routing][edit-history]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;
    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fire-locked-"+juce::Uuid().toString());
    REQUIRE(directory.createDirectory().wasOk());const juce::ScopeGuard cleanup{[&]{directory.deleteRecursively();}};
    p.statePresets.setPresetDirectoryForTesting(directory);
    lockedSet(p,"output",-12);lockedSet(p,"drive1",70);
    REQUIRE(p.assignLfoToTarget(2,"output")==LfoManager::AssignmentResult::changed);
    const auto incoming=lockedPreset(p);auto preset=incoming;preset.setAttribute("presetName","Locked");
    REQUIRE(preset.writeTo(directory.getChildFile("Locked.fire")));p.statePresets.scanAllPresets();
    lockedSet(p,"output",-3);lockedSet(p,"drive1",12);REQUIRE(p.setPresetParameterLocked("output",true));p.checkpointEditHistory();
    juce::String tag;for(const auto& entry:p.statePresets.getBrowserEntries())if(entry.key=="Locked.fire")tag=entry.tag;REQUIRE(tag.isNotEmpty());
    REQUIRE(p.statePresets.loadPreset(tag));CHECK(lockedGet(p,"output")==Catch::Approx(-3));CHECK(lockedGet(p,"drive1")==Catch::Approx(70));
    CHECK_FALSE(p.isCurrentStateEquivalentToPreset(incoming));
    const auto routes=p.getLfoManager().getModulationRoutingsCopy();
    bool hasIncomingRoute=false;for(const auto& route:routes)hasIncomingRoute|=route.targetParameterID=="output"&&route.sourceLfoIndex==2;CHECK(hasIncomingRoute);
    REQUIRE(p.undoEdit());CHECK(lockedGet(p,"drive1")==Catch::Approx(12));CHECK(p.isPresetParameterLocked("output"));
    REQUIRE(state::loadStateFromXml(incoming,p));CHECK(lockedGet(p,"output")==Catch::Approx(-12));CHECK(p.isPresetParameterLocked("output"));
}

TEST_CASE("Legacy LFO smoothness promotion cannot overwrite a locked parameter base",
          "[preset-locks][preset][state][legacy][lfo]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;
    auto old=lockedPreset(p);fire::test::stripEnvelopePresetFamily(old);old.setAttribute("presetFormatVersion",1);
    old.removeAttribute("lfoSmooth1");old.getChildByName("LFO_STATE")->getFirstChildElement()->setAttribute("smoothness",.2f);
    old.getChildByName("MODULATION_STATE")->removeAttribute("schemaVersion");
    for(auto* route:old.getChildByName("MODULATION_STATE")->getChildIterator())
        for(const auto* attr:{"customRange","rangeStart","rangeEnd","curve","depthMacro"})route->removeAttribute(attr);
    lockedSet(p,"lfoSmooth1",.8f);REQUIRE(p.setPresetParameterLocked("lfoSmooth1",true));
    const auto overrides=fire::preset_locks::captureValues(p,p.getPresetParameterLocks());
    REQUIRE(state::loadStateFromXml(old,p,false,&overrides));CHECK(lockedGet(p,"lfoSmooth1")==Catch::Approx(.8f));
    CHECK(p.getLfoManager().getLfoDataCopy()[0].smoothness==Catch::Approx(.8f));
    REQUIRE(state::loadStateFromXml(old,p));CHECK(lockedGet(p,"lfoSmooth1")==Catch::Approx(.2f));
}

TEST_CASE("Invalid locked load overrides and damaged incoming presets fail before mutation",
          "[preset-locks][preset][state][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;lockedSet(p,"drive1",25);
    const auto before=lockedPreset(p);auto incoming=before;incoming.setAttribute("drive1",.9f);
    for(const auto& overrides:{fire::preset_locks::Values{{"output",1.5f}},fire::preset_locks::Values{{"numBands",.5f}},fire::preset_locks::Values{{"missing",.5f}}})
    {CHECK_FALSE(state::loadStateFromXml(incoming,p,false,&overrides));CHECK(p.isCurrentStateEquivalentToPreset(before));}
    REQUIRE(p.setPresetParameterLocked("output",true));const auto locked=fire::preset_locks::captureValues(p,p.getPresetParameterLocks());
    incoming.removeAttribute("drive1");CHECK_FALSE(state::loadStateFromXml(incoming,p,false,&locked));CHECK(p.isCurrentStateEquivalentToPreset(before));
}
