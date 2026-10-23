#include "../PluginProcessor.h"
#include "ChainSnapshotCodec.h"
#include "ModuleSnapshotMapping.h"
#include <algorithm>
#include <functional>
#include <set>

namespace
{
using namespace fire;
using Values = std::map<juce::String,float>;
struct Placement { int node=-1; Values values; juce::Array<ModulationRouting> routes; };

float snapshotValue(const juce::ValueTree& state,const juce::String& id,float fallback)
{ return static_cast<float>(state.getChildWithProperty("id",id).getProperty("value",fallback)); }

bool snapshotModulePresent(const juce::ValueTree& state, int scope, int node)
{
    if (node < module_order::firstInsert)
        return snapshotValue(state, core_modules::presenceID(scope, node), 0) >= .5f;
    const auto slot = node - module_order::firstInsert;
    return snapshotValue(state, effects::parameterID(scope, slot, effects::typeField), 0) > 0
        || snapshotValue(state, core_modules::parameterID(scope, slot, core_modules::typeField), 0) > 0
        || snapshotValue(state, modulation_fx::parameterID(scope, slot), 0) > 0
        || snapshotValue(state, resonator_params::parameterID(scope, slot), 0) > .5f;
}

std::vector<int> snapshotOrder(const FireAudioProcessor::SerializablePresetStateSnapshot& state,int scope)
{
    std::vector<int> nodes;
    for(int node:module_order::defaults(scope)) if(module_order::valid(scope,node)) nodes.push_back(node);
    const int legacy=scope==0?3:5;
    std::stable_sort(nodes.begin()+legacy,nodes.end(),[&](int a,int b) {
        return snapshotValue(state.parameterState,effects::parameterID(scope,a-module_order::firstInsert,effects::orderField),0)
             < snapshotValue(state.parameterState,effects::parameterID(scope,b-module_order::firstInsert,effects::orderField),0);
    });
    std::array<int,module_order::capacity> positions{};
    for(size_t index=0;index<nodes.size();++index)
    {
        const auto stored=snapshotValue(state.parameterState,module_order::parameterID(scope,nodes[index]),-1);
        positions[static_cast<size_t>(nodes[index])]=stored<0?static_cast<int>(index):juce::jlimit(0,12,juce::roundToInt(stored));
    }
    std::stable_sort(nodes.begin(),nodes.end(),[&](int a,int b){return positions[static_cast<size_t>(a)]<positions[static_cast<size_t>(b)];});
    return nodes;
}

juce::StringArray allOwnedIDs(int scope,int node)
{
    juce::StringArray result;
    if(node<module_order::firstInsert) return chain::ownedParameterIDs(scope,node,core_modules::legacyType(scope,node));
    for(int type=1;type<static_cast<int>(effects::Type::count);++type)
        for(const auto& id:chain::ownedParameterIDs(scope,node,static_cast<effects::Type>(type))) result.addIfNotAlreadyThere(id);
    return result;
}

juce::Result planPlacements(const FireAudioProcessor& processor,int scope,const chain::Snapshot& snapshot,
                           bool replace,std::vector<Placement>& placements)
{
    if(!juce::isPositiveAndBelow(scope,effects::scopeCount)) return juce::Result::fail("Invalid destination chain.");
    if(!channel_mode::valid(snapshot.channelMode)) return juce::Result::fail("Invalid chain channel mode.");
    if(snapshot.routingMode<0 || snapshot.routingMode>1 || !std::isfinite(snapshot.parallelBlend)
        || snapshot.parallelBlend<0 || snapshot.parallelBlend>1)return juce::Result::fail("Invalid parallel chain settings.");
    if(snapshot.scopeRoutes.size()>chain::maximumRoutes || (!snapshot.includesRouting && !snapshot.scopeRoutes.isEmpty()))
        return juce::Result::fail("Invalid chain-level modulation routes.");
    for(const auto& route:snapshot.scopeRoutes)
    {
        auto canonical=route;canonical.sanitise();
        if(route.targetParameterID!="parallel.blend" || !mod_sources::validSource(route.sourceLfoIndex) || !canonical.hasSameState(route))
            return juce::Result::fail("Invalid chain-level modulation route.");
    }
    if(snapshot.modules.size()>chain::maximumModules) return juce::Result::fail("This chain has too many modules.");
    std::vector<int> candidates;
    for(int node:module_order::defaults(scope))
        if(module_order::valid(scope,node) && !(scope==0&&node==2)
            && (replace || !processor.isModulePresent(scope,node))) candidates.push_back(node);
    std::vector<std::vector<Placement>> choices(snapshot.modules.size());
    size_t routeCount=static_cast<size_t>(snapshot.scopeRoutes.size());
    for(size_t index=0;index<snapshot.modules.size();++index)
    {
        const auto& module=snapshot.modules[index];
        if(const auto valid=chain::validateModule(module);valid.failed()) return valid;
        if(!snapshot.includesRouting && !module.routes.isEmpty()) return juce::Result::fail("Routing was not enabled for this snapshot.");
        routeCount+=static_cast<size_t>(module.routes.size());
        if(routeCount>chain::maximumRoutes) return juce::Result::fail("This chain exceeds the modulation routing limit.");
        juce::String reason;
        for(int node:candidates)
        {
            if(node<module_order::firstInsert && core_modules::legacyType(scope,node)!=module.type) continue;
            Placement p;p.node=node;
            const auto mapped=chain::mapModule(processor,module,scope,node,snapshot.includesRouting,p.values,p.routes);
            if(mapped.wasOk()) choices[index].push_back(std::move(p));
            else if(reason.isEmpty()) reason=mapped.getErrorMessage();
        }
        if(choices[index].empty()) return juce::Result::fail(juce::String(effects::name(module.type))+": "+
            (reason.isNotEmpty()?reason:"No compatible free module slot. A chain supports eight inserted modules."));
        std::stable_sort(choices[index].begin(),choices[index].end(),[&](const auto& a,const auto& b){
            return ((a.node<module_order::firstInsert)==module.sourceLegacy)>
                   ((b.node<module_order::firstInsert)==module.sourceLegacy);
        });
    }
    std::array<int,module_order::capacity> owners;owners.fill(-1);
    std::vector<int> selected(snapshot.modules.size(),-1);
    std::function<bool(int,std::array<bool,module_order::capacity>&)> place;
    place=[&](int index,auto& visited) {
        for(size_t option=0;option<choices[static_cast<size_t>(index)].size();++option)
        {
            const int node=choices[static_cast<size_t>(index)][option].node;
            if(visited[static_cast<size_t>(node)])continue;
            visited[static_cast<size_t>(node)]=true;
            if(owners[static_cast<size_t>(node)]<0 || place(owners[static_cast<size_t>(node)],visited))
            {owners[static_cast<size_t>(node)]=index;selected[static_cast<size_t>(index)]=static_cast<int>(option);return true;}
        }
        return false;
    };
    for(size_t index=0;index<snapshot.modules.size();++index)
    {
        std::array<bool,module_order::capacity> visited{};
        if(!place(static_cast<int>(index),visited))return juce::Result::fail("Not enough compatible slots for the complete chain. No modules were changed.");
    }
    for(size_t index=0;index<snapshot.modules.size();++index) placements.push_back(std::move(choices[index][static_cast<size_t>(selected[index])]));
    return juce::Result::ok();
}
}

juce::Result FireAudioProcessor::captureModuleSnapshot(int scope,int node,bool routes,fire::chain::Snapshot& out) const
{
    const juce::ScopedLock lock(multibandTopologyWriterLock);
    fire::chain::Snapshot next;next.includesRouting=routes;
    fire::chain::Module module;
    const auto result=fire::chain::captureModule(*this,scope,node,module);
    if(result.failed())return result;
    if(!routes)module.routes.clear();next.modules.push_back(std::move(module));out=std::move(next);
    return juce::Result::ok();
}

juce::Result FireAudioProcessor::captureChainSnapshot(int scope,bool routes,fire::chain::Snapshot& out) const
{
    if(!juce::isPositiveAndBelow(scope,fire::effects::scopeCount))return juce::Result::fail("Invalid source chain.");
    const juce::ScopedLock lock(multibandTopologyWriterLock);
    const auto state=captureSerializablePresetStateSnapshot();
    fire::chain::Snapshot next;next.includesRouting=routes;
    next.channelMode=juce::roundToInt(snapshotValue(state.parameterState,fire::channel_mode::parameterID(scope),0));
    next.routingMode=juce::roundToInt(snapshotValue(state.parameterState,fire::parallel_chain::modeID(scope),0));
    next.parallelBlend=snapshotValue(state.parameterState,fire::parallel_chain::blendID(scope),fire::parallel_chain::defaultBlend);
    if(routes)for(auto route:state.routings)
        if(route.targetParameterID==fire::parallel_chain::blendID(scope))
        {route.targetParameterID="parallel.blend";next.scopeRoutes.add(std::move(route));}
    for(int node:snapshotOrder(state,scope))
    {
        if((scope==0&&node==2) || !snapshotModulePresent(state.parameterState,scope,node))continue;
        fire::chain::Module module;
        const auto captured=fire::chain::captureModuleFromSnapshot(*this,state,scope,node,module);
        if(captured.failed())return captured;
        if(!routes)module.routes.clear();next.modules.push_back(std::move(module));
    }
    out=std::move(next);return juce::Result::ok();
}

juce::Result FireAudioProcessor::pasteModuleSnapshot(int scope,int afterNode,const fire::chain::Snapshot& snapshot,int& newNode)
{
    if(snapshot.modules.size()!=1)return juce::Result::fail("The module clipboard must contain exactly one module.");
    if(!snapshot.scopeRoutes.isEmpty())return juce::Result::fail("A module clipboard cannot contain chain-level modulation.");
    const juce::ScopedLock lock(multibandTopologyWriterLock);
    if(afterNode>=0 && !isModulePresent(scope,afterNode))return juce::Result::fail("The destination module is no longer present.");
    std::vector<Placement> placements;
    if(const auto plan=planPlacements(*this,scope,snapshot,false,placements);plan.failed())return plan;
    const int node=placements.front().node;
    const auto clear=allOwnedIDs(scope,node);
    auto state=captureSerializablePresetStateSnapshot();
    auto routes=state.routings;
    for(int i=routes.size();--i>=0;)if(clear.contains(routes[i].targetParameterID))routes.remove(i);
    int populated=0;for(const auto& route:routes)if(route.targetParameterID.isNotEmpty())++populated;
    if(populated+placements.front().routes.size()>LfoManager::maximumModulationRoutings)return juce::Result::fail("Not enough modulation routing slots.");
    for(int i=routes.size();--i>=0;)if(routes[i].targetParameterID.isEmpty())routes.remove(i);
    routes.addArray(placements.front().routes);
    auto nodes=visibleModuleOrder(scope);
    const auto after=std::find(nodes.begin(),nodes.end(),afterNode);
    nodes.insert(after==nodes.end()?nodes.end():std::next(after),node);
    for(int id:getModuleOrder(scope))if(id>=0&&std::find(nodes.begin(),nodes.end(),id)==nodes.end())nodes.push_back(id);
    fire::module_order::Order order;order.fill(-1);std::copy(nodes.begin(),nodes.end(),order.begin());
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish{[this]{requestMultibandTopologyReset();lfoDataHasChanged();}};
    const auto write=[this](const juce::String& id,float value){if(auto* p=treeState.getParameter(id);p && !juce::exactlyEqual(p->getValue(),value))
        {p->beginChangeGesture();p->setValueNotifyingHost(value);p->endChangeGesture();}};
    for(const auto& id:clear)if(auto* p=treeState.getParameter(id))write(id,p->getDefaultValue());
    for(const auto& [id,value]:placements.front().values)write(id,value);
    if(node<fire::module_order::firstInsert)write(fire::core_modules::presenceID(scope,node),1);
    else
    {
        auto& rack = scope == 0 ? masterInserts : bands[static_cast<size_t>(scope - 1)]->inserts;
        rack.stageFrozenRecording(node - fire::module_order::firstInsert, snapshot.modules.front().frozen,
                                 multibandTopologyResetGeneration.load(std::memory_order_seq_cst) + 1u);
    }
    {const juce::ScopedLock routeLock(lfoManager->getLfoDataLock());lfoManager->getModulationRoutings()=std::move(routes);lfoManager->advanceModulationRoutingRevisionLocked();}
    writeModuleOrder(scope,order);newNode=node;
    return juce::Result::ok();
}

juce::Result FireAudioProcessor::pasteChainSnapshot(int scope,const fire::chain::Snapshot& snapshot)
{
    const juce::ScopedLock lock(multibandTopologyWriterLock);
    std::vector<Placement> placements;
    if(const auto plan=planPlacements(*this,scope,snapshot,true,placements);plan.failed())return plan;
    auto state=captureSerializablePresetStateSnapshot();
    juce::StringArray clear;
    clear.add(fire::parallel_chain::blendID(scope));
    for(int node:fire::module_order::defaults(scope))if(fire::module_order::valid(scope,node)&&!(scope==0&&node==2))
        for(const auto& id:allOwnedIDs(scope,node))clear.addIfNotAlreadyThere(id);
    auto routes=state.routings;
    for(int i=routes.size();--i>=0;)if(routes[i].targetParameterID.isEmpty()||clear.contains(routes[i].targetParameterID))routes.remove(i);
    for(const auto& placement:placements)routes.addArray(placement.routes);
    for(auto route:snapshot.scopeRoutes)
    {route.targetParameterID=fire::parallel_chain::blendID(scope);routes.add(std::move(route));}
    if(routes.size()>LfoManager::maximumModulationRoutings)return juce::Result::fail("Not enough modulation routing slots for the entire chain.");
    std::vector<int> nodes;for(const auto& p:placements)nodes.push_back(p.node);
    if(scope==0&&isModulePresent(0,2))nodes.push_back(2);
    for(int id:getModuleOrder(scope))if(id>=0&&std::find(nodes.begin(),nodes.end(),id)==nodes.end())nodes.push_back(id);
    fire::module_order::Order order;order.fill(-1);std::copy(nodes.begin(),nodes.end(),order.begin());
    beginMultibandTopologyEdit();
    const juce::ScopeGuard publish{[this]{requestMultibandTopologyReset();lfoDataHasChanged();}};
    const auto write=[this](const juce::String& id,float value){if(auto* p=treeState.getParameter(id);p && !juce::exactlyEqual(p->getValue(),value))
        {p->beginChangeGesture();p->setValueNotifyingHost(value);p->endChangeGesture();}};
    for(const auto& id:clear)if(auto* p=treeState.getParameter(id))write(id,p->getDefaultValue());
    for(int node:fire::module_order::defaults(scope))if(fire::module_order::valid(scope,node)&&!(scope==0&&node==2))
    {
        if(node<fire::module_order::firstInsert)write(fire::core_modules::presenceID(scope,node),0);
        else
        {
            const auto slot=node-fire::module_order::firstInsert;
            for(const auto& id:{fire::effects::parameterID(scope,slot,fire::effects::typeField),
                fire::core_modules::parameterID(scope,slot,fire::core_modules::typeField),
                fire::modulation_fx::parameterID(scope,slot),fire::resonator_params::parameterID(scope,slot)})write(id,0);
        }
    }
    state.frozenAudio[static_cast<size_t>(scope)]={};
    write(fire::channel_mode::parameterID(scope),static_cast<float>(snapshot.channelMode)/2.0f);
    write(fire::parallel_chain::modeID(scope),static_cast<float>(snapshot.routingMode));
    write(fire::parallel_chain::blendID(scope),snapshot.parallelBlend);
    for(size_t index=0;index<placements.size();++index)
    {
        const auto& p=placements[index];for(const auto& [id,value]:p.values)write(id,value);
        if(p.node<fire::module_order::firstInsert)write(fire::core_modules::presenceID(scope,p.node),1);
        else state.frozenAudio[static_cast<size_t>(scope)][static_cast<size_t>(p.node-fire::module_order::firstInsert)]=snapshot.modules[index].frozen;
    }
    {const juce::ScopedLock routeLock(lfoManager->getLfoDataLock());lfoManager->getModulationRoutings()=std::move(routes);lfoManager->advanceModulationRoutingRevisionLocked();}
    // Only the replaced scope receives new recording publications. Restaging
    // the serializable snapshot of another live Granular would clear its
    // running recorder when Freeze is off.
    auto& rack = scope == 0 ? masterInserts : bands[static_cast<size_t>(scope - 1)]->inserts;
    const auto publication = multibandTopologyResetGeneration.load(std::memory_order_seq_cst) + 1u;
    for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        rack.stageFrozenRecording(slot, state.frozenAudio[static_cast<size_t>(scope)][static_cast<size_t>(slot)], publication);
    writeModuleOrder(scope,order);
    return juce::Result::ok();
}
