#pragma once
#include "ChainSnapshot.h"
#include "../PluginProcessor.h"
#include <map>

class FireAudioProcessor;

namespace fire::chain
{
// Message-thread preparation only. Capture reads one coherent public snapshot;
// mapping returns a complete proposed parameter patch without changing Fire.
juce::Result captureModule(const FireAudioProcessor&, int scope, int node, Module&);
juce::Result captureModuleFromSnapshot(const FireAudioProcessor&,
    const FireAudioProcessor::SerializablePresetStateSnapshot&, int scope, int node, Module&);
juce::Result mapModule(const FireAudioProcessor&, const Module&, int destinationScope,
                       int destinationNode, bool includeRoutes,
                       std::map<juce::String, float>& destinationNormalised,
                       juce::Array<ModulationRouting>& destinationRoutes);
juce::Result validateModule(const Module&);
Module defaultModule(effects::Type, bool sourceLegacy = false);
juce::StringArray ownedParameterIDs(int scope, int node, effects::Type);
}
