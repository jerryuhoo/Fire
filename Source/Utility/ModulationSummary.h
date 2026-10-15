#pragma once
#include "../DSP/ModulationRouting.h"

namespace fire::ui
{
struct ModulationSummary
{
    bool isModulated = false;
    int sourceLfoIndex = 0; // One-based display identity of the first source.
    float depth = 0, currentValue = 0;
    bool isBipolar = true, isBypassed = false;
    int routeCount = 0;
    bool usesMappedDisplay = false;
    float minimumOffset = 0, maximumOffset = 0, currentOffset = 0;
};

inline ModulationSummary summariseModulation(const juce::Array<ModulationRouting>& routes,
    const juce::String& target, const std::array<float, mod_sources::sourceCount>& values,
    const std::array<bool, mod_sources::sourceCount>& present)
{
    ModulationSummary result;
    float bypassedMin = 0, bypassedMax = 0;
    int active = 0;
    for (const auto& route : routes)
    {
        if (route.targetParameterID != target || !mod_sources::validSource(route.sourceLfoIndex)
            || !present[static_cast<size_t>(route.sourceLfoIndex)]) continue;
        const auto source = values[static_cast<size_t>(route.sourceLfoIndex)];
        if (result.routeCount++ == 0)
        {
            result.sourceLfoIndex = route.sourceLfoIndex + 1;
            result.depth = route.depth;
            result.isBipolar = route.isBipolar;
            result.currentValue = route.isBipolar ? source * 2 - 1 : source;
        }
        result.usesMappedDisplay |= route.hasAdvancedMapping();
        const auto macro = juce::isPositiveAndBelow(route.depthMacro, mod_sources::macroCount)
            ? values[static_cast<size_t>(mod_sources::firstMacro + route.depthMacro)] : 1.0f;
        const auto first = route.offset(0, macro), last = route.offset(1, macro);
        if (route.isBypassed)
        {
            bypassedMin += juce::jmin(first, last);
            bypassedMax += juce::jmax(first, last);
            continue;
        }
        ++active;
        result.minimumOffset += juce::jmin(first, last);
        result.maximumOffset += juce::jmax(first, last);
        result.currentOffset += route.offset(source, macro);
    }
    result.isModulated = result.routeCount > 0;
    result.usesMappedDisplay |= result.routeCount > 1;
    result.isBypassed = result.isModulated && active == 0;
    if (result.isBypassed)
    { result.minimumOffset = bypassedMin; result.maximumOffset = bypassedMax; }
    result.minimumOffset = juce::jlimit(-1.0f, 1.0f, result.minimumOffset);
    result.maximumOffset = juce::jlimit(-1.0f, 1.0f, result.maximumOffset);
    result.currentOffset = juce::jlimit(-1.0f, 1.0f, result.currentOffset);
    return result;
}
}
