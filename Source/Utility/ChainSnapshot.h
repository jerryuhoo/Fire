#pragma once
#include "../DSP/InsertEffect.h"
#include "../DSP/ModulationRouting.h"
#include <map>
#include <memory>
#include <vector>

namespace fire::chain
{
struct Module
{
    effects::Type type = effects::Type::none;
    bool enabled = true, sourceLegacy = false;
    // Preserve the original Drive mode explicitly; conversion must not
    // silently turn a legacy linked output into modern compensation.
    bool legacyLinked = false, modernDriveComp = true;
    std::map<juce::String,float> values; // Canonical keys, source-representation normalised values.
    juce::Array<ModulationRouting> routes; // Targets are canonical keys, not instance parameter IDs.
    effects::FrozenRecordingPtr frozen;
};
struct Snapshot
{
    std::vector<Module> modules;
    bool includesRouting = false;
};
enum class ClipboardKind { module, chain };
}
