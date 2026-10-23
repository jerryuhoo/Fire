#include "../PluginProcessor.h"
#include "ParallelChainParameters.h"
#include <cmath>

int FireAudioProcessor::getChainRoutingMode(int scope) const noexcept
{
    if(!fire::parallel_chain::validScope(scope))return fire::parallel_chain::serial;
    const auto* value=treeState.getRawParameterValue(fire::parallel_chain::modeID(scope));
    return value&&value->load(std::memory_order_relaxed)>.5f?fire::parallel_chain::parallel:fire::parallel_chain::serial;
}
int FireAudioProcessor::getModuleBranch(int scope,int node) const noexcept
{
    if(!fire::parallel_chain::validNode(scope,node))return fire::parallel_chain::a;
    const auto* value=treeState.getRawParameterValue(fire::parallel_chain::branchID(scope,node));
    return value&&value->load(std::memory_order_relaxed)>.5f?fire::parallel_chain::b:fire::parallel_chain::a;
}
float FireAudioProcessor::getParallelBlend(int scope) const noexcept
{
    if(!fire::parallel_chain::validScope(scope))return fire::parallel_chain::defaultBlend;
    const auto* value=treeState.getRawParameterValue(fire::parallel_chain::blendID(scope));
    const auto raw=value?value->load(std::memory_order_relaxed):fire::parallel_chain::defaultBlend;
    return std::isfinite(raw)?juce::jlimit(0.0f,1.0f,raw):fire::parallel_chain::defaultBlend;
}
bool FireAudioProcessor::setChainRoutingMode(int scope,int mode,std::uint64_t* committedRevision)
{
    if(committedRevision)*committedRevision=0;
    if(!fire::parallel_chain::validScope(scope) || mode<0 || mode>1)return false;
    {
        const juce::ScopedLock lock(multibandTopologyWriterLock);
        if(getChainRoutingMode(scope)==mode)return false;
        const auto resetBefore=multibandDspResetSequence.load(std::memory_order_acquire);
        {
            beginMultibandTopologyEdit();
            const juce::ScopeGuard publish{[this]{finishMainStateEdit(false);}};
            if(auto* parameter=treeState.getParameter(fire::parallel_chain::modeID(scope)))
            {
                parameter->beginChangeGesture();
                // A host may restore another sound from the gesture-begin
                // callback, before this action has written its requested value.
                // That replacement owns the state; never write into it.
                const bool replaced=mainStateEditRequiresDspReset;
                if(!replaced)parameter->setValueNotifyingHost(static_cast<float>(mode));
                parameter->endChangeGesture();
                if(replaced)return false;
            }
        }
        if(getChainRoutingMode(scope)!=mode)return false;
        const auto revision=getChainEditRevision();
        if(committedRevision && (revision&1u)==0 && resetBefore==multibandDspResetSequence.load(std::memory_order_acquire))
            *committedRevision=revision;
    }
    sendChangeMessage();lfoDataHasChanged();return true;
}
bool FireAudioProcessor::setModuleBranch(int scope,int node,int branch,std::uint64_t* committedRevision)
{
    if(committedRevision)*committedRevision=0;
    if(!fire::parallel_chain::validNode(scope,node) || branch<0 || branch>1)return false;
    {
        const juce::ScopedLock lock(multibandTopologyWriterLock);
        if(!isModulePresent(scope,node) || getModuleBranch(scope,node)==branch)return false;
        const auto resetBefore=multibandDspResetSequence.load(std::memory_order_acquire);
        {
            beginMultibandTopologyEdit();
            const juce::ScopeGuard publish{[this]{finishMainStateEdit(false);}};
            if(auto* parameter=treeState.getParameter(fire::parallel_chain::branchID(scope,node)))
            {
                parameter->beginChangeGesture();
                const bool replaced=mainStateEditRequiresDspReset;
                if(!replaced)parameter->setValueNotifyingHost(static_cast<float>(branch));
                parameter->endChangeGesture();
                if(replaced)return false;
            }
        }
        if(!isModulePresent(scope,node) || getModuleBranch(scope,node)!=branch)return false;
        const auto revision=getChainEditRevision();
        if(committedRevision && (revision&1u)==0 && resetBefore==multibandDspResetSequence.load(std::memory_order_acquire))
            *committedRevision=revision;
    }
    sendChangeMessage();lfoDataHasChanged();return true;
}
void FireAudioProcessor::resetModuleBranchParameter(int scope,int node)
{
    if(!fire::parallel_chain::validNode(scope,node))return;
    if(auto* parameter=treeState.getParameter(fire::parallel_chain::branchID(scope,node)))
    {parameter->beginChangeGesture();parameter->setValueNotifyingHost(0);parameter->endChangeGesture();}
}
