#pragma once
#include "ParallelChainParameters.h"
#include "CoreModuleParameters.h"
#include <memory>
#include <vector>

namespace fire::parallel_chain
{
inline void appendParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& parameters)
{
    for(int scope=0;scope<scopeCount;++scope)
    {
        const auto name=scope==0?juce::String("Master"):"Band "+juce::String(scope);
        parameters.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{modeID(scope),versionHint},
            name+" Chain Routing",juce::StringArray{"Serial","Parallel"},serial));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{blendID(scope),versionHint},
            name+" Parallel Blend",juce::NormalisableRange<float>{0,1},defaultBlend));
    }
    for(int scope=0;scope<scopeCount;++scope)for(int node=0;node<module_order::capacity;++node)if(validNode(scope,node))
    {
        const auto moduleName=node>=module_order::firstInsert
            ? "FX "+juce::String(node-module_order::firstInsert+1)
            : juce::String(effects::name(core_modules::legacyType(scope,node)));
        parameters.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{branchID(scope,node),versionHint},
            (scope==0?juce::String("Master"):"Band "+juce::String(scope))+" "+moduleName+" Branch",
            juce::StringArray{"A","B"},a));
    }
}
}
