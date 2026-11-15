#pragma once
#include "ModuleOrder.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace fire::parallel_chain
{
inline constexpr int schemaVersion=1,versionHint=19;
inline constexpr int scopeCount=effects::scopeCount,branchCount=2;
inline constexpr int moduleParameterCount=62,parameterCount=2*scopeCount+moduleParameterCount;
enum Mode { serial=0,parallel=1 };
enum Branch { a=0,b=1 };
inline constexpr float defaultBlend=.5f;
inline bool validScope(int scope) noexcept{return scope>=0&&scope<scopeCount;}
inline bool validNode(int scope,int node) noexcept
{return module_order::valid(scope,node)&&!(scope==0&&node==2);}
inline const juce::String& emptyID(){static const juce::String empty;return empty;}
inline const juce::String& modeID(int scope)
{
    static const std::array<juce::String,scopeCount> ids{"masterChainRouting","bandChainRouting1","bandChainRouting2","bandChainRouting3","bandChainRouting4"};
    return validScope(scope)?ids[static_cast<size_t>(scope)]:emptyID();
}
inline const juce::String& blendID(int scope)
{
    static const std::array<juce::String,scopeCount> ids{"masterParallelBlend","bandParallelBlend1","bandParallelBlend2","bandParallelBlend3","bandParallelBlend4"};
    return validScope(scope)?ids[static_cast<size_t>(scope)]:emptyID();
}
inline juce::String branchBase(int node,bool master)
{return juce::String(master?"masterModuleBranch":"bandModuleBranch")+juce::String(node)+(master?"":"Band");}
inline const juce::String& branchID(int scope,int node)
{
    static const auto ids=[]
    {
        std::array<std::array<juce::String,module_order::capacity>,scopeCount> result;
        for(int s=0;s<scopeCount;++s)for(int n=0;n<module_order::capacity;++n)
            if(validNode(s,n))result[static_cast<size_t>(s)][static_cast<size_t>(n)]=branchBase(n,s==0)+(s==0?juce::String():juce::String(s));
        return result;
    }();
    return validNode(scope,node)?ids[static_cast<size_t>(scope)][static_cast<size_t>(node)]:emptyID();
}
inline const juce::StringArray& parameterIDs()
{
    static const auto ids=[]
    {
        juce::StringArray result;
        for(int scope=0;scope<scopeCount;++scope){result.add(modeID(scope));result.add(blendID(scope));}
        for(int scope=0;scope<scopeCount;++scope)
            for(int node=0;node<module_order::capacity;++node)if(validNode(scope,node))result.add(branchID(scope,node));
        return result;
    }();return ids;
}
inline bool isParameterID(const juce::String& id)
{
    if(!id.startsWith("masterChainRouting") && !id.startsWith("bandChainRouting")
        && !id.startsWith("masterParallelBlend") && !id.startsWith("bandParallelBlend")
        && !id.startsWith("masterModuleBranch") && !id.startsWith("bandModuleBranch"))return false;
    return parameterIDs().contains(id);
}
inline bool isBlendID(const juce::String& id)
{for(int scope=0;scope<scopeCount;++scope)if(id==blendID(scope))return true;return false;}
inline bool isStructuralID(const juce::String& id){return isParameterID(id)&&!isBlendID(id);}
struct Routing
{
    int mode=serial;
    std::array<int,module_order::capacity> branches{};
};
inline bool sameAudibleRouting(const Routing& first,const Routing& second) noexcept
{return first.mode==second.mode && (first.mode==serial || first.branches==second.branches);}
struct Configuration : Routing {float blend=defaultBlend;};
}
