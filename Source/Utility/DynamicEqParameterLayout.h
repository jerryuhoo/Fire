#pragma once
#include "DynamicEqParameters.h"
#include <memory>
#include <vector>

namespace fire::dynamic_eq
{
inline void appendParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& parameters)
{
    const auto append=[&](int scope,int slot)
    {
        const auto prefix=scope==0?juce::String("Master"):"Band "+juce::String(scope);
        for(int node=0;node<nodesPerModule;++node)
            for(int field=0;field<fieldCount;++field)
            {
                const juce::ParameterID id{parameterID(scope,slot,node,field),versionHint};
                const auto name=prefix+(slot<0?juce::String():" FX "+juce::String(slot+1))
                    +" EQ "+juce::String(node+1)+" Dynamic "+suffixes[static_cast<size_t>(field)];
                if(field==enabled)parameters.push_back(std::make_unique<juce::AudioParameterBool>(id,name,false));
                else parameters.push_back(std::make_unique<juce::AudioParameterFloat>(id,name,rangeFor(field),defaults[static_cast<size_t>(field)]));
            }
    };
    append(0,-1);
    for(int scope=0;scope<scopeCount;++scope)for(int slot=0;slot<slotsPerScope;++slot)append(scope,slot);
}
}
