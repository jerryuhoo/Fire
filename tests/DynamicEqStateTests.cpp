#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/DynamicEqStateValidation.h>
#include <Utility/ModuleSnapshotMapping.h>
#include <Utility/ChainSnapshotCodec.h>
#include "LegacyDynamicEqFixture.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
namespace de=fire::dynamic_eq;
namespace chain=fire::chain;
void dynamicSet(FireAudioProcessor& p,const juce::String& id,float value)
{auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
float dynamicGet(FireAudioProcessor& p,const juce::String& id)
{auto* parameter=p.treeState.getRawParameterValue(id);REQUIRE(parameter);return parameter->load();}
juce::XmlElement dynamicPreset(FireAudioProcessor& p)
{juce::XmlElement xml("WINGSFIRE");state::saveStateToXml(p,xml);return xml;}
juce::XmlElement dynamicHost(FireAudioProcessor& p)
{juce::MemoryBlock bytes;p.getStateInformation(bytes);auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize()));REQUIRE(xml);return *xml;}
void dynamicRestore(FireAudioProcessor& p,const juce::XmlElement& xml)
{juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(xml,bytes);p.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));}
void dynamicConfigure(FireAudioProcessor& p,int scope,int slot,int scene)
{
    for(int node=0;node<de::nodesPerModule;++node)
    {
        const std::array<float,de::fieldCount> values{1,-50.0f+node+scene,2.0f+node*.2f,scene==1?-12.0f:8.0f,5.0f+node,50.0f+scene+node};
        for(int field=0;field<de::fieldCount;++field)dynamicSet(p,de::parameterID(scope,slot,node,field),values[static_cast<size_t>(field)]);
    }
}
void dynamicDefaults(FireAudioProcessor& p)
{for(const auto& id:de::parameterIDs()){const auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);CHECK(parameter->getValue()==parameter->getDefaultValue());}}
void stripChainDynamic(juce::XmlElement& xml)
{
    for(auto* module:xml.getChildIterator())if(auto* values=module->getChildByName("VALUES"))
        for(int index=values->getNumChildElements();--index>=0;)
            if(values->getChildElement(index)->getStringAttribute("key").contains(".dynamic."))values->removeChildElement(values->getChildElement(index),true);
}
}

TEST_CASE("Dynamic EQ parameters append stable automation indices while remaining unavailable as LFO targets",
          "[dynamic-eq-state][state][parameters][routing]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;REQUIRE(p.getParameters().size()==7780);
    int index=4756;
    for(const auto& id:de::parameterIDs())
    {const auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);CHECK(parameter->getParameterIndex()==index++);CHECK(parameter->getVersionHint()==18);}
    dynamicDefaults(p);
    for(int field=0;field<de::fieldCount;++field)
        CHECK(p.assignLfoToTarget(0,de::parameterID(0,-1,0,field))==LfoManager::AssignmentResult::invalidRequest);
    CHECK(p.assignLfoToTarget(0,"peakGain")==LfoManager::AssignmentResult::changed);
}

TEST_CASE("Dynamic EQ settings roundtrip preset host and AB including inactive edge slots",
          "[dynamic-eq-state][state][preset][host][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;dynamicConfigure(p,0,-1,1);dynamicConfigure(p,4,7,1);
    const auto first=dynamicPreset(p);CHECK(first.getIntAttribute("presetFormatVersion")==8);CHECK(first.getIntAttribute("dynamicEqSchemaVersion")==1);
    p.stateAB.copyAB(false);p.stateAB.toggleAB();dynamicConfigure(p,0,-1,2);dynamicConfigure(p,4,7,2);
    const auto second=dynamicPreset(p),host=dynamicHost(p);CHECK(host.getIntAttribute("stateFormatVersion")==7);
    FireAudioProcessor restored;REQUIRE(state::loadStateFromXml(second,restored));CHECK(restored.isCurrentStateEquivalentToPreset(second));
    dynamicRestore(restored,host);CHECK(restored.isCurrentStateEquivalentToPreset(second));
    restored.stateAB.toggleAB();CHECK(restored.isCurrentStateEquivalentToPreset(first));
    restored.stateAB.toggleAB();CHECK(restored.isCurrentStateEquivalentToPreset(second));
}

TEST_CASE("Legacy static EQ presets and projects reset every dynamic setting to disabled defaults",
          "[dynamic-eq-state][state][legacy][preset][host]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;dynamicConfigure(p,0,-1,1);dynamicConfigure(p,4,7,1);
    auto preset=dynamicPreset(p);fire::test::stripDynamicEqPresetFamily(preset);REQUIRE(state::loadStateFromXml(preset,p));dynamicDefaults(p);
    auto host=dynamicHost(p);fire::test::stripDynamicEqHostFamily(host);dynamicConfigure(p,0,-1,2);dynamicRestore(p,host);dynamicDefaults(p);
    CHECK(p.isCurrentStateEquivalentToPreset(preset));
}

TEST_CASE("Malformed dynamic EQ preset families are rejected before any sound change",
          "[dynamic-eq-state][state][preset][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;dynamicConfigure(p,0,-1,1);
    const auto valid=dynamicPreset(p),before=dynamicPreset(p);
    auto reject=[&](juce::XmlElement bad){bad.setAttribute("drive1",.9f);CHECK_FALSE(state::loadStateFromXml(bad,p));CHECK(p.isCurrentStateEquivalentToPreset(before));};
    for(const auto* marker:{"0","2","1.5","1tail"}){auto bad=valid;bad.setAttribute("dynamicEqSchemaVersion",marker);reject(bad);}
    for(int field=0;field<de::fieldCount;++field)
    {
        const auto id=de::parameterID(4,7,11,field);auto missing=valid;missing.removeAttribute(id);reject(missing);
        for(const auto* value:{"-0.1","1.1","nan","0.5tail"}){auto bad=valid;bad.setAttribute(id,value);reject(bad);}
    }
    auto bad=valid;bad.setAttribute(de::parameterID(0,-1,0,de::enabled),.3f);reject(bad);
    bad=valid;bad.removeAttribute("dynamicEqSchemaVersion");reject(bad);bad.setAttribute("presetFormatVersion",6);reject(bad);
    bad=valid;fire::test::stripDynamicEqPresetFamily(bad);bad.setAttribute("presetFormatVersion",7);reject(bad);
    bad=valid;ModulationRouting{0,de::parameterID(0,-1,0,de::threshold),.4f,false,false}.writeToXml(*bad.getChildByName("MODULATION_STATE")->createNewChildElement("ROUTING"));reject(bad);
}

TEST_CASE("Malformed host dynamic EQ values and alternate snapshots never create partial settings",
          "[dynamic-eq-state][state][host][ab][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;dynamicConfigure(p,0,-1,1);
    const auto valid=dynamicHost(p),before=dynamicPreset(p);
    auto reject=[&](juce::XmlElement bad){bad.getChildByName("PARAMETERS")->getChildByAttribute("id","drive1")->setAttribute("value",90);
        dynamicRestore(p,bad);CHECK(p.isCurrentStateEquivalentToPreset(before));};
    for(int field=0;field<de::fieldCount;++field)
    {
        const auto id=de::parameterID(0,-1,11,field);
        auto bad=valid;bad.getChildByName("PARAMETERS")->getChildByAttribute("id",id)->setAttribute("value",de::maximums[static_cast<size_t>(field)]+1);reject(bad);
        bad=valid;auto* params=bad.getChildByName("PARAMETERS");params->removeChildElement(params->getChildByAttribute("id",id),true);bad.setAttribute("savedParameterCount",params->getNumChildElements());reject(bad);
    }
    auto bad=valid;bad.getChildByName("PARAMETERS")->getChildByAttribute("id",de::parameterID(0,-1,0,de::enabled))->setAttribute("value",.5f);reject(bad);
    bad=valid;bad.setAttribute("dynamicEqSchemaVersion",2);reject(bad);
    bad=valid;fire::test::stripDynamicEqHostFamily(bad);bad.setAttribute("stateFormatVersion",6);reject(bad);
    bad=valid;auto* alternate=bad.getChildByName("AB_STATE");REQUIRE(alternate);alternate->removeAttribute(de::parameterID(0,-1,0,de::ratio));
    dynamicRestore(p,bad);CHECK(p.isCurrentStateEquivalentToPreset(before));p.stateAB.toggleAB();CHECK(p.isCurrentStateEquivalentToPreset(before));
}

TEST_CASE("EQ chain format migrates complete absent old families and rejects partial dynamic settings",
          "[dynamic-eq-state][chain][codec][legacy][validation]")
{
    chain::Snapshot source;source.modules.push_back(chain::defaultModule(fire::effects::Type::eq));
    REQUIRE(source.modules[0].values.size()==162);auto current=chain::encode(source);REQUIRE(current);CHECK(current->getIntAttribute("schemaVersion")==4);
    chain::Snapshot decoded;REQUIRE(chain::decode(*current,decoded).wasOk());CHECK(decoded.modules[0].values==source.modules[0].values);
    for(int version:{1,2})
    {
        auto old=*current;fire::test::stripParallelChainSnapshot(old);old.setAttribute("schemaVersion",version);if(version==1)old.removeAttribute("channelMode");stripChainDynamic(old);
        REQUIRE(chain::decode(old,decoded).wasOk());CHECK(decoded.modules[0].values==source.modules[0].values);
        auto* added=old.getFirstChildElement()->getChildByName("VALUES")->createNewChildElement("PARAM");added->setAttribute("key","eq.0.dynamic.enabled");added->setAttribute("value",0);
        CHECK(chain::decode(old,decoded).failed());CHECK(decoded.modules[0].values==source.modules[0].values);
    }
    auto incomplete=*current;stripChainDynamic(incomplete);CHECK(chain::decode(incomplete,decoded).failed());
    auto routed=source;routed.includesRouting=true;routed.modules[0].routes.add({0,"eq.0.dynamic.threshold",.5f,false,false});CHECK(chain::encode(routed)==nullptr);
    auto invalid=source;invalid.modules[0].values["eq.0.dynamic.enabled"]=.2f;CHECK(chain::encode(invalid)==nullptr);
}

TEST_CASE("Dynamic EQ settings copy across native Master and inserted EQ without changing their units",
          "[dynamic-eq-state][chain][mapping][edit-history]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;dynamicConfigure(p,0,-1,1);
    chain::Snapshot source;REQUIRE(p.captureModuleSnapshot(0,0,false,source).wasOk());
    int node=-1;REQUIRE(p.pasteModuleSnapshot(1,-1,source,node).wasOk());const auto slot=node-fire::module_order::firstInsert;REQUIRE(slot>=0);
    for(int point=0;point<de::nodesPerModule;++point)for(int field=0;field<de::fieldCount;++field)
        CHECK(dynamicGet(p,de::parameterID(1,slot,point,field))==Catch::Approx(dynamicGet(p,de::parameterID(0,-1,point,field))));
    const auto before=dynamicPreset(p);p.checkpointEditHistory();REQUIRE(p.pasteChainSnapshot(0,{}).wasOk());
    for(int point=0;point<de::nodesPerModule;++point)CHECK(dynamicGet(p,de::parameterID(0,-1,point,de::enabled))==0);
    REQUIRE(p.undoEdit());CHECK(p.isCurrentStateEquivalentToPreset(before));
}

TEST_CASE("Band moves carry dynamic EQ and reused modules or points do not inherit old dynamics",
          "[dynamic-eq-state][state][topology][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;
    dynamicSet(p,NUM_BANDS_ID,2);dynamicSet(p,"lineState1",1);dynamicSet(p,"freq1",1500);
    const auto slot=p.addInsertEffect(1,fire::effects::Type::eq);REQUIRE(slot>=0);dynamicConfigure(p,1,slot,1);
    REQUIRE(p.addMultibandBand(0,2,true,300));CHECK(dynamicGet(p,de::parameterID(2,slot,5,de::enabled))==1);CHECK(dynamicGet(p,de::parameterID(1,slot,5,de::enabled))==0);
    REQUIRE(p.deleteMultibandBand(0,3));CHECK(dynamicGet(p,de::parameterID(1,slot,5,de::enabled))==1);
    p.removeInsertEffect(1,slot);REQUIRE(p.addInsertEffect(1,fire::effects::Type::eq)==slot);
    for(int point=0;point<de::nodesPerModule;++point)CHECK(dynamicGet(p,de::parameterID(1,slot,point,de::enabled))==0);
    const auto added=p.addEqNode(1000,0,fire::eq::Type::bell);REQUIRE(added>=0);dynamicSet(p,de::parameterID(0,-1,added,de::enabled),1);
    REQUIRE(p.removeEqNode(added));REQUIRE(p.addEqNode(1000,0,fire::eq::Type::bell)==added);CHECK(dynamicGet(p,de::parameterID(0,-1,added,de::enabled))==0);
}
