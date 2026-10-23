#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/ParallelChainStateValidation.h>
#include <Utility/ChainSnapshotCodec.h>
#include <Utility/ModuleSnapshotMapping.h>
#include "LegacyParallelChainFixture.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>

namespace
{
namespace pc=fire::parallel_chain;
namespace chain=fire::chain;
void parallelSet(FireAudioProcessor& p,const juce::String& id,float value)
{auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
juce::XmlElement parallelPreset(FireAudioProcessor& p)
{juce::XmlElement xml("WINGSFIRE");state::saveStateToXml(p,xml);return xml;}
juce::XmlElement parallelHost(FireAudioProcessor& p)
{juce::MemoryBlock bytes;p.getStateInformation(bytes);auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize()));REQUIRE(xml);return *xml;}
void parallelRestore(FireAudioProcessor& p,const juce::XmlElement& xml)
{juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(xml,bytes);p.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));}
void parallelConfigure(FireAudioProcessor& p,int scene)
{
    for(int scope=0;scope<pc::scopeCount;++scope)
    {
        parallelSet(p,pc::modeID(scope),(scope+scene)%2);parallelSet(p,pc::blendID(scope),.1f*(scope+scene));
        for(int node=0;node<fire::module_order::capacity;++node)if(pc::validNode(scope,node))parallelSet(p,pc::branchID(scope,node),(scope+node+scene)%2);
    }
}
void parallelDefaults(FireAudioProcessor& p)
{for(const auto& id:pc::parameterIDs()){const auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);CHECK(parameter->getValue()==parameter->getDefaultValue());}}
bool hasBlendRoute(FireAudioProcessor& p,int scope,int source)
{for(const auto& route:p.getLfoManager().getModulationRoutingsCopy())if(route.targetParameterID==pc::blendID(scope)&&route.sourceLfoIndex==source)return true;return false;}
struct ParallelPresetListener:juce::AudioProcessorParameter::Listener
{
    FireAudioProcessor& processor;juce::XmlElement preset;bool fired=false,loaded=false;
    bool loadOnGestureBegin=false;int beginCount=0,endCount=0;
    ParallelPresetListener(FireAudioProcessor& p,const juce::XmlElement& xml,bool onBegin=false)
        :processor(p),preset(xml),loadOnGestureBegin(onBegin){}
    void loadOnce(){if(!fired){fired=true;loaded=state::loadStateFromXml(preset,processor);}}
    void parameterValueChanged(int,float)override{if(!loadOnGestureBegin)loadOnce();}
    void parameterGestureChanged(int,bool starting)override
    {if(starting){++beginCount;if(loadOnGestureBegin)loadOnce();}else ++endCount;}
};
struct ParallelNotificationProbe:juce::ChangeListener,juce::AudioProcessorListener
{
    FireAudioProcessor& processor;int changes=0,hostNotifications=0,changesSeenByHost=0;
    explicit ParallelNotificationProbe(FireAudioProcessor& p):processor(p)
    {processor.addChangeListener(this);processor.addListener(this);}
    ~ParallelNotificationProbe()override{processor.removeListener(this);processor.removeChangeListener(this);}
    void changeListenerCallback(juce::ChangeBroadcaster*)override{++changes;}
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float)override{}
    void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details)override
    {
        if(!details.nonParameterStateChanged)return;
        ++hostNotifications;
        processor.dispatchPendingMessages();
        changesSeenByHost=changes;
    }
};
}

TEST_CASE("Parallel chain appends stable parameters and exposes only Blend to locks and internal modulation",
          "[parallel-state][state][parameters][routing][preset-locks]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;REQUIRE(pc::parameterIDs().size()==72);
    REQUIRE(p.getParameters().size()==7780);int index=7708;
    for(const auto& id:pc::parameterIDs())
    {
        auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);CHECK(parameter->getParameterIndex()==index++);CHECK(parameter->getVersionHint()==19);
        if(pc::isStructuralID(id))
        {CHECK(p.assignLfoToTarget(0,id)==LfoManager::AssignmentResult::invalidRequest);CHECK_FALSE(p.setPresetParameterLocked(id,true));}
    }
    parallelDefaults(p);CHECK(pc::branchID(0,2).isEmpty());CHECK(pc::branchID(0,3).isEmpty());
    REQUIRE(p.assignLfoToTarget(0,pc::blendID(1))==LfoManager::AssignmentResult::changed);
    REQUIRE(p.setPresetParameterLocked(pc::blendID(1),true));
    pc::Routing first,second;second.branches[1]=pc::b;CHECK(pc::sameAudibleRouting(first,second));
    first.mode=second.mode=pc::parallel;CHECK_FALSE(pc::sameAudibleRouting(first,second));
}

TEST_CASE("Parallel mode branches and blend recipes roundtrip preset host and both AB sounds",
          "[parallel-state][state][preset][host][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;parallelConfigure(p,1);
    REQUIRE(p.assignLfoToTarget(3,pc::blendID(4))==LfoManager::AssignmentResult::changed);
    const auto first=parallelPreset(p);CHECK(first.getIntAttribute("presetFormatVersion")==8);CHECK(first.getIntAttribute("parallelChainSchemaVersion")==1);
    p.stateAB.copyAB(false);p.stateAB.toggleAB();parallelConfigure(p,2);
    const auto second=parallelPreset(p),host=parallelHost(p);CHECK(host.getIntAttribute("stateFormatVersion")==7);
    FireAudioProcessor restored;REQUIRE(state::loadStateFromXml(second,restored));CHECK(restored.isCurrentStateEquivalentToPreset(second));
    parallelRestore(restored,host);CHECK(restored.isCurrentStateEquivalentToPreset(second));
    restored.stateAB.toggleAB();CHECK(restored.isCurrentStateEquivalentToPreset(first));
    restored.stateAB.toggleAB();CHECK(restored.isCurrentStateEquivalentToPreset(second));
}

TEST_CASE("Older serial sounds reset all parallel settings rather than retaining latent branches",
          "[parallel-state][state][legacy][preset][host]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;parallelConfigure(p,1);
    auto old=parallelPreset(p);fire::test::stripParallelPresetFamily(old);REQUIRE(state::loadStateFromXml(old,p));parallelDefaults(p);
    auto host=parallelHost(p);fire::test::stripParallelHostFamily(host);parallelConfigure(p,2);parallelRestore(p,host);parallelDefaults(p);
    CHECK(p.isCurrentStateEquivalentToPreset(old));
}

TEST_CASE("Malformed parallel families reject preset and host loads atomically",
          "[parallel-state][state][validation][preset][host]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;parallelConfigure(p,1);
    const auto preset=parallelPreset(p),host=parallelHost(p);
    auto rejectPreset=[&](juce::XmlElement bad){bad.setAttribute("drive1",.9f);CHECK_FALSE(state::loadStateFromXml(bad,p));CHECK(p.isCurrentStateEquivalentToPreset(preset));};
    for(const auto& id:{pc::modeID(0),pc::blendID(4),pc::branchID(4,12)})
    {
        auto missing=preset;missing.removeAttribute(id);rejectPreset(missing);
        for(const auto* value:{"-0.1","1.1","nan","0.2tail"}){auto bad=preset;bad.setAttribute(id,value);rejectPreset(bad);}
    }
    for(const auto& id:{pc::modeID(3),pc::branchID(0,5)})
    {
        auto bad=preset;bad.setAttribute(id,.3f);rejectPreset(bad);
        bad=preset;ModulationRouting{0,id,.5f,false,false}.writeToXml(*bad.getChildByName("MODULATION_STATE")->createNewChildElement("ROUTING"));rejectPreset(bad);
    }
    auto bad=preset;bad.removeAttribute("parallelChainSchemaVersion");rejectPreset(bad);bad.setAttribute("presetFormatVersion",7);rejectPreset(bad);
    bad=preset;bad.setAttribute("parallelChainSchemaVersion",2);rejectPreset(bad);
    bad=preset;fire::test::stripParallelPresetFamily(bad);bad.setAttribute("presetFormatVersion",8);rejectPreset(bad);
    auto rejectHost=[&](juce::XmlElement value){value.getChildByName("PARAMETERS")->getChildByAttribute("id","drive1")->setAttribute("value",90);
        parallelRestore(p,value);CHECK(p.isCurrentStateEquivalentToPreset(preset));};
    for(const auto& id:{pc::modeID(0),pc::blendID(4),pc::branchID(4,12)})
    {
        auto invalid=host;invalid.getChildByName("PARAMETERS")->getChildByAttribute("id",id)->setAttribute("value",2);rejectHost(invalid);
        invalid=host;auto* params=invalid.getChildByName("PARAMETERS");params->removeChildElement(params->getChildByAttribute("id",id),true);
        invalid.setAttribute("savedParameterCount",params->getNumChildElements());rejectHost(invalid);
    }
    auto invalid=host;fire::test::stripParallelHostFamily(invalid);invalid.setAttribute("stateFormatVersion",7);rejectHost(invalid);
}

TEST_CASE("Parallel chain codec preserves scope routes and migrates pre parallel formats",
          "[parallel-state][chain][codec][legacy]")
{
    chain::Snapshot source;source.includesRouting=true;source.routingMode=pc::parallel;source.parallelBlend=.23f;
    source.modules.push_back(chain::defaultModule(fire::effects::Type::delay));source.modules[0].branch=pc::b;
    ModulationRouting route{24,"parallel.blend",-.4f,false,false};route.customRange=true;route.rangeStart=-.1f;route.rangeEnd=.7f;route.curve=3;route.depthMacro=2;source.scopeRoutes.add(route);
    auto xml=chain::encode(source);REQUIRE(xml);CHECK(xml->getIntAttribute("schemaVersion")==4);
    chain::Snapshot restored;REQUIRE(chain::decode(*xml,restored).wasOk());CHECK(restored.routingMode==pc::parallel);CHECK(restored.parallelBlend==.23f);
    CHECK(restored.modules[0].branch==pc::b);REQUIRE(restored.scopeRoutes.size()==1);CHECK(restored.scopeRoutes[0].hasSameState(route));
    for(int version:{1,2,3})
    {
        auto old=*xml;fire::test::stripParallelChainSnapshot(old);old.setAttribute("schemaVersion",version);if(version==1)old.removeAttribute("channelMode");
        REQUIRE(chain::decode(old,restored).wasOk());CHECK(restored.routingMode==pc::serial);CHECK(restored.parallelBlend==pc::defaultBlend);
        CHECK(restored.modules[0].branch==pc::a);CHECK(restored.scopeRoutes.isEmpty());
    }
}

TEST_CASE("Parallel chain codec rejects invalid structural fields and shared routing overflow",
          "[parallel-state][chain][codec][validation]")
{
    chain::Snapshot source;source.includesRouting=true;source.modules.push_back(chain::defaultModule(fire::effects::Type::delay));source.scopeRoutes.add({0,"parallel.blend",.5f,false,false});
    auto valid=chain::encode(source);REQUIRE(valid);chain::Snapshot sentinel=source;
    for(const auto* attr:{"routingMode","parallelBlend"})
    {auto bad=*valid;bad.removeAttribute(attr);CHECK(chain::decode(bad,sentinel).failed());}
    auto bad=*valid;bad.getFirstChildElement()->setAttribute("branch",.2f);CHECK(chain::decode(bad,sentinel).failed());
    bad=*valid;auto* scope=bad.getChildByName("SCOPE_ROUTINGS");scope->getFirstChildElement()->setAttribute("target","routingMode");CHECK(chain::decode(bad,sentinel).failed());
    bad=*valid;scope=bad.getChildByName("SCOPE_ROUTINGS");bad.addChildElement(new juce::XmlElement(*scope));CHECK(chain::decode(bad,sentinel).failed());
    bad=*valid;bad.removeChildElement(bad.getChildByName("SCOPE_ROUTINGS"),true);CHECK(chain::decode(bad,sentinel).failed());
    auto excessive=source;for(int i=0;i<128;++i)excessive.modules[0].routes.add({0,"control.1",.5f,false,false});CHECK(chain::encode(excessive)==nullptr);
    source.includesRouting=false;CHECK(chain::encode(source)==nullptr);
    CHECK(sentinel.scopeRoutes.size()==1);CHECK(sentinel.modules[0].branch==pc::a);
}

TEST_CASE("Module paste preserves its branch while whole chain replacement also restores mode blend and routes",
          "[parallel-state][chain][operations][edit-history]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;REQUIRE(p.pasteChainSnapshot(1,{}).wasOk());
    const int slot=p.addInsertEffect(1,fire::effects::Type::delay);REQUIRE(slot>=0);const int node=fire::module_order::firstInsert+slot;
    REQUIRE(p.setModuleBranch(1,node,pc::b));REQUIRE(p.setChainRoutingMode(1,pc::parallel));parallelSet(p,pc::blendID(1),.23f);
    REQUIRE(p.assignLfoToTarget(2,pc::blendID(1))==LfoManager::AssignmentResult::changed);
    parallelSet(p,pc::blendID(0),.77f);REQUIRE(p.assignLfoToTarget(17,pc::blendID(0))==LfoManager::AssignmentResult::changed);
    chain::Snapshot module;REQUIRE(p.captureModuleSnapshot(1,node,true,module).wasOk());CHECK(module.modules[0].branch==pc::b);CHECK(module.scopeRoutes.isEmpty());
    int added=-1;REQUIRE(p.pasteModuleSnapshot(0,-1,module,added).wasOk());CHECK(p.getModuleBranch(0,added)==pc::b);
    CHECK(p.getChainRoutingMode(0)==pc::serial);CHECK(p.getParallelBlend(0)==Catch::Approx(.77f));CHECK(hasBlendRoute(p,0,17));
    chain::Snapshot full;REQUIRE(p.captureChainSnapshot(1,true,full).wasOk());REQUIRE(full.scopeRoutes.size()==1);
    const auto before=parallelPreset(p);p.checkpointEditHistory();REQUIRE(p.pasteChainSnapshot(0,full).wasOk());
    CHECK(p.getChainRoutingMode(0)==pc::parallel);CHECK(p.getParallelBlend(0)==Catch::Approx(.23f));CHECK(hasBlendRoute(p,0,2));CHECK_FALSE(hasBlendRoute(p,0,17));
    REQUIRE(p.undoEdit());CHECK(p.isCurrentStateEquivalentToPreset(before));REQUIRE(p.redoEdit());CHECK(p.getParallelBlend(0)==Catch::Approx(.23f));
    full.scopeRoutes.getReference(0).targetParameterID="wrong";const auto current=parallelPreset(p);CHECK(p.pasteChainSnapshot(0,full).failed());CHECK(p.isCurrentStateEquivalentToPreset(current));
}

TEST_CASE("Band move and slot reuse preserve or reset parallel branch settings consistently",
          "[parallel-state][state][topology][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;parallelSet(p,NUM_BANDS_ID,2);parallelSet(p,"lineState1",1);parallelSet(p,"freq1",1500);
    REQUIRE(p.setChainRoutingMode(1,pc::parallel));parallelSet(p,pc::blendID(1),.2f);REQUIRE(p.setModuleBranch(1,0,pc::b));
    REQUIRE(p.addMultibandBand(0,2,true,300));CHECK(p.getChainRoutingMode(1)==pc::serial);CHECK(p.getChainRoutingMode(2)==pc::parallel);CHECK(p.getModuleBranch(2,0)==pc::b);CHECK(p.getParallelBlend(2)==Catch::Approx(.2f));
    REQUIRE(p.deleteMultibandBand(0,3));CHECK(p.getChainRoutingMode(1)==pc::parallel);CHECK(p.getModuleBranch(1,0)==pc::b);
    p.removeModule(1,0);REQUIRE(p.restoreLegacyModule(1,0));CHECK(p.getModuleBranch(1,0)==pc::a);
    const int slot=p.addInsertEffect(1,fire::effects::Type::delay);REQUIRE(slot>=0);const int node=fire::module_order::firstInsert+slot;
    REQUIRE(p.setModuleBranch(1,node,pc::b));p.removeInsertEffect(1,slot);REQUIRE(p.addInsertEffect(1,fire::effects::Type::delay)==slot);CHECK(p.getModuleBranch(1,node)==pc::a);
}

TEST_CASE("Parallel action revisions distinguish own commits from synchronous preset replacement",
          "[parallel-state][state][transaction][reentrancy]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;std::uint64_t revision=0;
    REQUIRE(p.setChainRoutingMode(1,pc::parallel,&revision));CHECK(revision==p.getChainEditRevision());CHECK(revision!=0);
    auto replacement=parallelPreset(p);replacement.setAttribute(pc::modeID(1),0);
    REQUIRE(p.setChainRoutingMode(1,pc::serial));
    ParallelPresetListener listener(p,replacement);auto* parameter=p.treeState.getParameter(pc::modeID(1));parameter->addListener(&listener);
    const bool result=p.setChainRoutingMode(1,pc::parallel,&revision);parameter->removeListener(&listener);
    REQUIRE(listener.fired);REQUIRE(listener.loaded);CHECK_FALSE(result);CHECK(revision==0);CHECK(p.getChainRoutingMode(1)==pc::serial);
    replacement=parallelPreset(p);replacement.setAttribute(pc::branchID(1,0),1);
    ParallelPresetListener sameValue(p,replacement);parameter=p.treeState.getParameter(pc::branchID(1,0));parameter->addListener(&sameValue);
    const bool branchResult=p.setModuleBranch(1,0,pc::b,&revision);parameter->removeListener(&sameValue);
    REQUIRE(sameValue.fired);REQUIRE(sameValue.loaded);CHECK(branchResult);CHECK(revision==0);
}

TEST_CASE("Parallel gesture begin callbacks cannot write into a replacement preset",
          "[parallel-state][state][transaction][reentrancy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for(int action=0;action<3;++action)
    {
        CAPTURE(action);FireAudioProcessor p;
        auto replacement=parallelPreset(p);replacement.setAttribute("drive1",.63f);
        const bool modeAction=action==0;
        const auto id=modeAction?pc::modeID(1):pc::branchID(1,0);
        replacement.setAttribute(id,0);
        if(action==2)replacement.setAttribute(fire::core_modules::presenceID(1,0),0);
        ParallelPresetListener listener(p,replacement,true);
        auto* parameter=p.treeState.getParameter(id);REQUIRE(parameter);parameter->addListener(&listener);
        std::uint64_t revision=1234;
        const bool changed=modeAction?p.setChainRoutingMode(1,pc::parallel,&revision)
                                     :p.setModuleBranch(1,0,pc::b,&revision);
        parameter->removeListener(&listener);
        REQUIRE(listener.fired);REQUIRE(listener.loaded);
        CHECK_FALSE(changed);CHECK(revision==0);
        CHECK(listener.beginCount==1);CHECK(listener.endCount==1);
        CHECK(p.isCurrentStateEquivalentToPreset(replacement));
    }
}

TEST_CASE("Parallel actions queue their change message before final host notification",
          "[parallel-state][state][notifications]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;p.dispatchPendingMessages();
    ParallelNotificationProbe probe(p);
    REQUIRE(p.setChainRoutingMode(1,pc::parallel));
    CHECK(probe.hostNotifications>0);CHECK(probe.changesSeenByHost==1);
    const auto prior=probe.hostNotifications;
    REQUIRE(p.setModuleBranch(1,0,pc::b));
    CHECK(probe.hostNotifications>prior);CHECK(probe.changesSeenByHost==2);
    p.dispatchPendingMessages();CHECK(probe.changes==2);
}

TEST_CASE("Parallel Blend getters fall back safely for malformed raw values",
          "[parallel-state][state][non-finite]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;
    auto* value=p.treeState.getRawParameterValue(pc::blendID(1));REQUIRE(value);
    const float original=value->load();const juce::ScopeGuard restore{[&]{value->store(original);}};
    for(float input:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
    {value->store(input);CHECK(p.getParallelBlend(1)==pc::defaultBlend);}
    value->store(-2);CHECK(p.getParallelBlend(1)==0);
    value->store(2);CHECK(p.getParallelBlend(1)==1);
    value->store(.23f);CHECK(p.getParallelBlend(1)==Catch::Approx(.23f));
    CHECK(p.getParallelBlend(-1)==pc::defaultBlend);CHECK(p.getParallelBlend(5)==pc::defaultBlend);
}

TEST_CASE("Parallel host branch names describe native modules and permanent FX slots",
          "[parallel-state][state][parameters]")
{
    juce::ScopedJuceInitialiser_GUI gui;FireAudioProcessor p;
    const auto name=[&](int scope,int node)
    {const auto* parameter=p.treeState.getParameter(pc::branchID(scope,node));REQUIRE(parameter);return parameter->getName(128);};
    CHECK(name(0,0)=="Master EQ Branch");CHECK(name(0,1)=="Master Lo-Fi Branch");
    CHECK(name(1,0)=="Band 1 Drive Branch");CHECK(name(1,1)=="Band 1 Shape Branch");
    CHECK(name(1,2)=="Band 1 Compressor Branch");CHECK(name(1,3)=="Band 1 Stereo Branch");CHECK(name(1,4)=="Band 1 OTT Branch");
    CHECK(name(0,5)=="Master FX 1 Branch");CHECK(name(4,12)=="Band 4 FX 8 Branch");
}
