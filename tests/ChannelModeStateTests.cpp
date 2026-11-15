#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/ChannelModeStateValidation.h>
#include <Utility/ChainSnapshotCodec.h>
#include <Utility/ModuleSnapshotMapping.h>
#include "LegacyChannelModeFixture.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
namespace cm = fire::channel_mode;
namespace chain = fire::chain;
void modeSet(FireAudioProcessor& processor, int scope, int mode)
{
    auto* p = processor.treeState.getParameter(cm::parameterID(scope)); REQUIRE(p);
    p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(mode))); p->endChangeGesture();
}
int modeGet(FireAudioProcessor& processor,int scope)
{ return juce::roundToInt(processor.treeState.getRawParameterValue(cm::parameterID(scope))->load()); }
juce::XmlElement modePreset(FireAudioProcessor& processor)
{ juce::XmlElement xml("WINGSFIRE"); state::saveStateToXml(processor,xml); return xml; }
juce::XmlElement modeHost(FireAudioProcessor& processor)
{
    juce::MemoryBlock bytes; processor.getStateInformation(bytes);
    auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize())); REQUIRE(xml);
    return *xml;
}
void modeRestoreHost(FireAudioProcessor& processor,const juce::XmlElement& xml)
{
    juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(xml,bytes);
    processor.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));
}
void modeConfigure(FireAudioProcessor& processor,int offset)
{ for(int scope=0;scope<5;++scope) modeSet(processor,scope,(scope+offset)%3); }
void modeCheck(FireAudioProcessor& processor,int offset)
{ for(int scope=0;scope<5;++scope) CHECK(modeGet(processor,scope)==(scope+offset)%3); }
}

TEST_CASE("Channel modes retain stable appended IDs and survive preset project and AB round trips",
          "[mid-side][state][preset][host][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    REQUIRE(processor.getParameters().size()==7780);
    for(int scope=0;scope<5;++scope)
    {
        auto* p=processor.treeState.getParameter(cm::parameterID(scope));REQUIRE(p);
        CHECK(p->getParameterIndex()==4663+scope); CHECK(p->getVersionHint()==16); CHECK(p->getDefaultValue()==0.0f);
        CHECK(processor.assignLfoToTarget(0,cm::parameterID(scope))==LfoManager::AssignmentResult::invalidRequest);
    }
    modeConfigure(processor,1);
    const auto first=modePreset(processor);
    CHECK(first.getIntAttribute("presetFormatVersion")==8);
    CHECK(first.getIntAttribute(fire::channel_mode_state::marker)==1);
    processor.stateAB.copyAB(false);processor.stateAB.toggleAB();modeConfigure(processor,2);
    const auto second=modePreset(processor),host=modeHost(processor);
    CHECK(host.getIntAttribute("stateFormatVersion")==7);
    FireAudioProcessor restored;
    REQUIRE(state::loadStateFromXml(second,restored));modeCheck(restored,2);
    modeRestoreHost(restored,host);modeCheck(restored,2);
    restored.stateAB.toggleAB();modeCheck(restored,1);
    restored.stateAB.toggleAB();modeCheck(restored,2);
}

TEST_CASE("Pre Mid Side presets and host states reset all modes to Stereo",
          "[mid-side][state][legacy][preset][host]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source,restored;modeConfigure(source,1);modeConfigure(restored,2);
    auto preset=modePreset(source);fire::test::stripChannelModePresetFamily(preset);
    REQUIRE(state::loadStateFromXml(preset,restored));
    for(int scope=0;scope<5;++scope)CHECK(modeGet(restored,scope)==0);
    modeConfigure(restored,2);
    auto host=modeHost(source);fire::test::stripChannelModeHostFamily(host);modeRestoreHost(restored,host);
    for(int scope=0;scope<5;++scope)CHECK(modeGet(restored,scope)==0);
    CHECK(restored.isCurrentStateEquivalentToPreset(preset));
}

TEST_CASE("Malformed channel mode preset families fail transactionally",
          "[mid-side][state][preset][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor processor;modeConfigure(processor,1);
    const auto valid=modePreset(processor),before=modeHost(processor);
    auto reject=[&](juce::XmlElement candidate){candidate.setAttribute("drive1",.9f);
        CHECK_FALSE(state::loadStateFromXml(candidate,processor));CHECK(modeHost(processor).toString()==before.toString());};
    for(const auto* value:{"-0.1","1.1","0.3","nan","0.5tail"})
    {auto bad=valid;bad.setAttribute(cm::parameterID(1),value);reject(bad);}
    for(const auto* marker:{"0","2","1.5","1tail"})
    {auto bad=valid;bad.setAttribute(fire::channel_mode_state::marker,marker);reject(bad);}
    for(int scope=0;scope<5;++scope){auto bad=valid;bad.removeAttribute(cm::parameterID(scope));reject(bad);}
    auto markerMissing=valid;markerMissing.removeAttribute(fire::channel_mode_state::marker);reject(markerMissing);
    markerMissing.setAttribute("presetFormatVersion",4);reject(markerMissing);
    auto familyMissing=valid;fire::test::stripChannelModePresetFamily(familyMissing);familyMissing.setAttribute("presetFormatVersion",5);reject(familyMissing);
    auto routed=valid;ModulationRouting{0,cm::parameterID(1),.5f,false,false}.writeToXml(
        *routed.getChildByName("MODULATION_STATE")->createNewChildElement("ROUTING"));reject(routed);
}

TEST_CASE("Malformed channel mode host families fail before any sound is mutated",
          "[mid-side][state][host][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor processor;modeConfigure(processor,2);
    const auto valid=modeHost(processor),before=modePreset(processor);
    auto reject=[&](juce::XmlElement candidate){candidate.getChildByName("PARAMETERS")->getChildByAttribute("id","drive1")->setAttribute("value",90.0f);
        modeRestoreHost(processor,candidate);CHECK(processor.isCurrentStateEquivalentToPreset(before));};
    for(const auto* value:{"-1","3","0.5","nan","1tail"})
    {auto bad=valid;bad.getChildByName("PARAMETERS")->getChildByAttribute("id",cm::parameterID(2))->setAttribute("value",value);reject(bad);}
    for(const auto* marker:{"0","2","1.5","1tail"})
    {auto bad=valid;bad.setAttribute(fire::channel_mode_state::marker,marker);reject(bad);}
    for(int scope=0;scope<5;++scope)
    {auto bad=valid;auto* parameters=bad.getChildByName("PARAMETERS");parameters->removeChildElement(parameters->getChildByAttribute("id",cm::parameterID(scope)),true);
        bad.setAttribute("savedParameterCount",parameters->getNumChildElements());reject(bad);}
    auto markerMissing=valid;markerMissing.removeAttribute(fire::channel_mode_state::marker);reject(markerMissing);
    markerMissing.setAttribute("stateFormatVersion",3);reject(markerMissing);
    auto familyMissing=valid;fire::test::stripChannelModeHostFamily(familyMissing);familyMissing.setAttribute("stateFormatVersion",4);reject(familyMissing);
}

TEST_CASE("Chain mode schema round trips and v1 migration are strict and atomic",
          "[mid-side][chain][codec][state]")
{
    chain::Snapshot source;source.channelMode=cm::side;
    source.modules.push_back(chain::defaultModule(fire::effects::Type::delay));
    auto xml=chain::encode(source);REQUIRE(xml);CHECK(xml->getIntAttribute("schemaVersion")==4);
    chain::Snapshot restored;REQUIRE(chain::decode(*xml,restored).wasOk());CHECK(restored.channelMode==cm::side);
    for(const auto* mode:{"-1","3","1.5","1tail","nan"})
    {auto bad=*xml;bad.setAttribute("channelMode",mode);CHECK(chain::decode(bad,restored).failed());CHECK(restored.channelMode==cm::side);}
    auto missing=*xml;missing.removeAttribute("channelMode");CHECK(chain::decode(missing,restored).failed());
    fire::test::stripParallelChainSnapshot(missing);missing.setAttribute("schemaVersion",1);REQUIRE(chain::decode(missing,restored).wasOk());CHECK(restored.channelMode==cm::stereo);
    missing.setAttribute("channelMode",2);CHECK(chain::decode(missing,restored).failed());CHECK(restored.channelMode==cm::stereo);
    source.channelMode=3;CHECK(chain::encode(source)==nullptr);
}

TEST_CASE("Only whole chain replacement changes the destination channel mode in one undo step",
          "[mid-side][chain][operations][edit-history]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor processor;
    modeSet(processor,1,cm::side);modeSet(processor,0,cm::mid);modeSet(processor,2,cm::mid);
    REQUIRE(processor.addInsertEffect(1,fire::effects::Type::delay)==0);
    chain::Snapshot module;REQUIRE(processor.captureModuleSnapshot(1,fire::module_order::firstInsert,false,module).wasOk());
    int added=-1;REQUIRE(processor.pasteModuleSnapshot(0,-1,module,added).wasOk());CHECK(modeGet(processor,0)==cm::mid);
    chain::Snapshot source;REQUIRE(processor.captureChainSnapshot(1,false,source).wasOk());CHECK(source.channelMode==cm::side);
    processor.checkpointEditHistory();
    REQUIRE(processor.pasteChainSnapshot(2,source).wasOk());CHECK(modeGet(processor,2)==cm::side);CHECK(modeGet(processor,0)==cm::mid);
    REQUIRE(processor.undoEdit());CHECK(modeGet(processor,2)==cm::mid);
    REQUIRE(processor.redoEdit());CHECK(modeGet(processor,2)==cm::side);
    auto bad=source;bad.channelMode=-1;const auto before=modePreset(processor);
    CHECK(processor.pasteChainSnapshot(2,bad).failed());CHECK(processor.isCurrentStateEquivalentToPreset(before));
    REQUIRE(processor.pasteChainSnapshot(2,{}).wasOk());CHECK(modeGet(processor,2)==cm::stereo);
}

TEST_CASE("A damaged alternate channel mode snapshot falls back to the complete current sound",
          "[mid-side][state][ab][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor processor;modeConfigure(processor,1);
    processor.stateAB.copyAB(false);processor.stateAB.toggleAB();modeConfigure(processor,2);
    auto host=modeHost(processor);auto* alternate=host.getChildByName("AB_STATE");REQUIRE(alternate);
    SECTION("truncated") {alternate->removeAttribute(cm::parameterID(3));}
    SECTION("invalid mode") {alternate->setAttribute(cm::parameterID(2),.3f);}
    SECTION("future family") {alternate->setAttribute(fire::channel_mode_state::marker,2);}
    FireAudioProcessor restored;modeRestoreHost(restored,host);modeCheck(restored,2);
    restored.stateAB.toggleAB();modeCheck(restored,2);
}

TEST_CASE("Band insertion and deletion carry channel modes with the original chain",
          "[mid-side][state][topology]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor processor;
    auto set=[&](const juce::String& id,float value){auto* p=processor.treeState.getParameter(id);REQUIRE(p);p->setValueNotifyingHost(p->convertTo0to1(value));};
    set(NUM_BANDS_ID,2);set("lineState1",1);set("freq1",1500);
    modeSet(processor,0,cm::side);modeSet(processor,1,cm::mid);modeSet(processor,2,cm::side);
    processor.checkpointEditHistory();
    REQUIRE(processor.addMultibandBand(0,2,true,300));
    CHECK(modeGet(processor,0)==cm::side);CHECK(modeGet(processor,1)==cm::stereo);
    CHECK(modeGet(processor,2)==cm::mid);CHECK(modeGet(processor,3)==cm::side);
    REQUIRE(processor.undoEdit());CHECK(modeGet(processor,1)==cm::mid);CHECK(modeGet(processor,2)==cm::side);
    REQUIRE(processor.redoEdit());CHECK(modeGet(processor,1)==cm::stereo);CHECK(modeGet(processor,2)==cm::mid);
    REQUIRE(processor.deleteMultibandBand(0,3));
    CHECK(modeGet(processor,0)==cm::side);CHECK(modeGet(processor,1)==cm::mid);
    CHECK(modeGet(processor,2)==cm::side);CHECK(modeGet(processor,3)==cm::stereo);
}
