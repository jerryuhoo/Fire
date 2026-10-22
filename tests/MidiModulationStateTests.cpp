#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/MidiStateValidation.h>
#include <Utility/ChainSnapshotCodec.h>
#include "LegacyMidiFixture.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
namespace cc = fire::midi_mod;
namespace play = fire::lfo_playback;
void midiSet(FireAudioProcessor& p,const juce::String& id,int value)
{
    auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
}
int midiGet(FireAudioProcessor& p,const juce::String& id)
{const auto* parameter=p.treeState.getRawParameterValue(id);REQUIRE(parameter);return juce::roundToInt(parameter->load());}
juce::XmlElement midiPreset(FireAudioProcessor& p)
{juce::XmlElement xml("WINGSFIRE");state::saveStateToXml(p,xml);return xml;}
juce::XmlElement midiHost(FireAudioProcessor& p)
{
    juce::MemoryBlock bytes;p.getStateInformation(bytes);
    const auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize()));REQUIRE(xml);return *xml;
}
void midiRestore(FireAudioProcessor& p,const juce::XmlElement& xml)
{juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(xml,bytes);p.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));}
void midiConfigure(FireAudioProcessor& p,int scene)
{
    for(int slot=0;slot<cc::count;++slot)
    {midiSet(p,cc::parameterID(slot,cc::number),slot*17+scene);midiSet(p,cc::parameterID(slot,cc::channel),(slot+scene)%17);}
    for(int slot=0;slot<play::count;++slot)
    {
        midiSet(p,play::parameterID(slot,play::playMode),(slot+scene)%3);
        midiSet(p,play::parameterID(slot,play::waveMode),(slot+scene+1)%3);
        midiSet(p,play::parameterID(slot,play::seed),65535-slot*17-scene);
        midiSet(p,play::parameterID(slot,play::triggerNote),slot*7+scene);
        midiSet(p,play::parameterID(slot,play::triggerChannel),(slot+scene)%17);
    }
}
void midiDefault(FireAudioProcessor& p)
{
    for(const auto* ids:{&cc::parameterIDs(),&play::parameterIDs()})for(const auto& id:*ids)
    {const auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);CHECK(parameter->getValue()==parameter->getDefaultValue());}
}
void midiAddRoute(juce::XmlElement& xml,int source,const juce::String& target="drive1")
{ModulationRouting{source,target,.42f,false,false}.writeToXml(*xml.getChildByName("MODULATION_STATE")->createNewChildElement("ROUTING"));}
}

TEST_CASE("MIDI source and LFO playback parameters append stable IDs and default to original behaviour",
          "[midi-state][state][parameters]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;
    REQUIRE(p.getParameters().size()==7708);
    int index=4668;
    for(const auto* ids:{&cc::parameterIDs(),&play::parameterIDs()})for(const auto& id:*ids)
    {
        const auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);
        CHECK(parameter->getParameterIndex()==index++);CHECK(parameter->getVersionHint()==17);
        CHECK(p.assignLfoToTarget(0,id)==LfoManager::AssignmentResult::invalidRequest);
    }
    midiDefault(p);
    CHECK(fire::mod_sources::envelope==16);CHECK(fire::mod_sources::firstMacro==17);
    for(int source=24;source<28;++source)
    {CHECK(fire::mod_sources::isMidiCC(source));CHECK(fire::mod_sources::envelopeIndex(source)==-1);
        CHECK(fire::mod_sources::name(source) == "MIDI CC "+juce::String(source-23));}
    CHECK_FALSE(fire::mod_sources::validSource(28));CHECK(fire::mod_sources::envelopeIndex(23)==3);
}

TEST_CASE("MIDI settings playback seeds and new routes survive preset host and AB snapshots",
          "[midi-state][state][preset][host][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;midiConfigure(p,1);
    auto first=midiPreset(p);CHECK(first.getIntAttribute("presetFormatVersion")==7);
    for(int source=24;source<28;++source)midiAddRoute(first,source);
    REQUIRE(state::loadStateFromXml(first,p));REQUIRE(p.isCurrentStateEquivalentToPreset(first));
    p.stateAB.copyAB(false);p.stateAB.toggleAB();midiConfigure(p,2);
    const auto second=midiPreset(p),host=midiHost(p);CHECK(host.getIntAttribute("stateFormatVersion")==6);
    FireAudioProcessor restored;REQUIRE(state::loadStateFromXml(second,restored));CHECK(restored.isCurrentStateEquivalentToPreset(second));
    midiRestore(restored,host);CHECK(restored.isCurrentStateEquivalentToPreset(second));
    restored.stateAB.toggleAB();CHECK(restored.isCurrentStateEquivalentToPreset(first));
    restored.stateAB.toggleAB();CHECK(restored.isCurrentStateEquivalentToPreset(second));
}

TEST_CASE("Older sounds reset MIDI configuration and LFO playback while retaining their old sources",
          "[midi-state][state][legacy][preset][host]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;midiConfigure(p,1);
    auto preset=midiPreset(p);fire::test::stripMidiPresetFamily(preset);midiAddRoute(preset,23);
    REQUIRE(state::loadStateFromXml(preset,p));midiDefault(p);CHECK(p.isCurrentStateEquivalentToPreset(preset));
    auto host=midiHost(p);fire::test::stripMidiHostFamily(host);midiConfigure(p,2);midiRestore(p,host);midiDefault(p);
    auto forbidden=preset;midiAddRoute(forbidden,24);CHECK_FALSE(state::loadStateFromXml(forbidden,p));
    auto invalidHost=host;midiAddRoute(invalidHost,27);const auto before=midiPreset(p);midiRestore(p,invalidHost);
    CHECK(p.isCurrentStateEquivalentToPreset(before));
}

TEST_CASE("Partial malformed or future MIDI and playback preset families fail atomically",
          "[midi-state][state][preset][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;midiConfigure(p,1);
    const auto valid=midiPreset(p),before=midiHost(p);
    auto reject=[&](juce::XmlElement bad){bad.setAttribute("drive1",.99f);CHECK_FALSE(state::loadStateFromXml(bad,p));CHECK(midiHost(p).toString()==before.toString());};
    for(const auto* marker:{fire::midi_state::ccMarker,fire::midi_state::playbackMarker})
    {
        for(const auto* value:{"0","2","1.5","1tail"}){auto bad=valid;bad.setAttribute(marker,value);reject(bad);}
        auto missing=valid;missing.removeAttribute(marker);reject(missing);missing.setAttribute("presetFormatVersion",5);reject(missing);
    }
    for(const auto& id:{cc::parameterID(0,cc::number),cc::parameterID(3,cc::channel),play::parameterID(0,play::playMode),
        play::parameterID(15,play::waveMode),play::parameterID(4,play::seed),play::parameterID(2,play::triggerNote),play::parameterID(7,play::triggerChannel)})
    {
        auto missing=valid;missing.removeAttribute(id);reject(missing);
        for(const auto* value:{"-0.1","1.1","nan","0.1tail"}){auto bad=valid;bad.setAttribute(id,value);reject(bad);}
    }
    auto fractional=valid;fractional.setAttribute(play::parameterID(0,play::playMode),.2f);reject(fractional);
    auto missing=valid;fire::test::stripMidiPresetFamily(missing);missing.setAttribute("presetFormatVersion",6);reject(missing);
    for(const auto& target:{cc::parameterID(0,cc::number),play::parameterID(0,play::seed)})
    {auto bad=valid;midiAddRoute(bad,0,target);reject(bad);}
}

TEST_CASE("Malformed MIDI host family values never partially mutate live sound",
          "[midi-state][state][host][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;midiConfigure(p,1);
    const auto valid=midiHost(p),before=midiPreset(p);
    auto reject=[&](juce::XmlElement bad){bad.getChildByName("PARAMETERS")->getChildByAttribute("id","drive1")->setAttribute("value",90);
        midiRestore(p,bad);CHECK(p.isCurrentStateEquivalentToPreset(before));};
    for(const auto* marker:{fire::midi_state::ccMarker,fire::midi_state::playbackMarker})
    {auto bad=valid;bad.setAttribute(marker,2);reject(bad);bad=valid;bad.removeAttribute(marker);reject(bad);}
    const std::array<std::pair<juce::String,const char*>,7> invalid{{
        {cc::parameterID(0,cc::number),"128"},{cc::parameterID(3,cc::channel),"17"},
        {play::parameterID(0,play::playMode),"3"},{play::parameterID(15,play::waveMode),"-1"},
        {play::parameterID(4,play::seed),"65536"},{play::parameterID(2,play::triggerNote),"-2"},
        {play::parameterID(7,play::triggerChannel),"17"}}};
    for(const auto& [id,value]:invalid)
    {
        for(const auto* badValue:{value,"0.5","nan","1tail"})
        {auto bad=valid;bad.getChildByName("PARAMETERS")->getChildByAttribute("id",id)->setAttribute("value",badValue);reject(bad);}
        auto bad=valid;auto* params=bad.getChildByName("PARAMETERS");params->removeChildElement(params->getChildByAttribute("id",id),true);
        bad.setAttribute("savedParameterCount",params->getNumChildElements());reject(bad);
    }
    auto missing=valid;fire::test::stripMidiHostFamily(missing);missing.setAttribute("stateFormatVersion",5);reject(missing);
}

TEST_CASE("Malformed alternate MIDI configuration falls back to the current complete sound",
          "[midi-state][state][ab][validation]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;midiConfigure(p,1);p.stateAB.copyAB(false);p.stateAB.toggleAB();midiConfigure(p,2);
    auto host=midiHost(p);const auto current=midiPreset(p);auto* alternate=host.getChildByName("AB_STATE");REQUIRE(alternate);
    SECTION("missing CC"){alternate->removeAttribute(cc::parameterID(0,cc::number));}
    SECTION("invalid seed"){alternate->setAttribute(play::parameterID(0,play::seed),1.1f);}
    SECTION("future playback"){alternate->setAttribute(fire::midi_state::playbackMarker,2);}
    FireAudioProcessor restored;midiRestore(restored,host);CHECK(restored.isCurrentStateEquivalentToPreset(current));
    restored.stateAB.toggleAB();CHECK(restored.isCurrentStateEquivalentToPreset(current));
}

TEST_CASE("Chain CC routes retain destination MIDI definitions and roundtrip through codec",
          "[midi-state][chain][state][routing]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor source,destination;midiConfigure(source,1);midiConfigure(destination,2);
    const auto slot=source.addInsertEffect(1,fire::effects::Type::delay);REQUIRE(slot>=0);
    const auto target=fire::effects::parameterID(1,slot,0);
    REQUIRE(source.assignLfoToTarget(24,target)==LfoManager::AssignmentResult::changed);
    fire::chain::Snapshot captured;REQUIRE(source.captureModuleSnapshot(1,fire::module_order::firstInsert+slot,true,captured).wasOk());
    auto xml=fire::chain::encode(captured);REQUIRE(xml);CHECK(xml->getIntAttribute("schemaVersion")==3);
    fire::chain::Snapshot decoded;REQUIRE(fire::chain::decode(*xml,decoded).wasOk());
    int added=-1;REQUIRE(destination.pasteModuleSnapshot(0,-1,decoded,added).wasOk());
    for(int i=0;i<cc::count;++i)CHECK(midiGet(destination,cc::parameterID(i,cc::number))==i*17+2);
    CHECK(destination.getLfoManager().getModulationRoutingsCopy().getLast().sourceLfoIndex==24);
}

TEST_CASE("Removing and reusing an LFO resets playback choices and seed without resetting CC slots",
          "[midi-state][state][lfo-bank]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;midiConfigure(p,1);
    REQUIRE(p.removeLfo(1));
    for(int field=0;field<play::fieldCount;++field)CHECK(midiGet(p,play::parameterID(1,field))==play::defaults[static_cast<size_t>(field)]);
    REQUIRE(p.addLfo()==1);
    for(int field=0;field<play::fieldCount;++field)CHECK(midiGet(p,play::parameterID(1,field))==play::defaults[static_cast<size_t>(field)]);
    CHECK(midiGet(p,cc::parameterID(3,cc::number))==52);
}
