#pragma once

#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

namespace fire::tests
{
// These fixtures exercise a single route changing source, as in one matrix
// row's source selector. Public drag-to-assign now adds another contributor.
inline void assignSingleSourceRoute(FireAudioProcessor& processor,
                                    int source,
                                    const juce::String& target)
{
    auto& manager = processor.getLfoManager();
    const auto snapshot = manager.getModulationRoutingStateSnapshot();
    int row = -1;
    for (int index = 0; index < snapshot.routings.size(); ++index)
        if (snapshot.routings[index].targetParameterID == target)
        {
            REQUIRE(row == -1); // The fixture must not silently discard a sum.
            row = index;
        }
    if (row < 0)
    {
        REQUIRE(processor.assignLfoToTarget(source, target) == LfoManager::AssignmentResult::changed);
        return;
    }
    const auto result = manager.assignModulationRoutingIfRevisionMatches(
        row, snapshot.revision, snapshot.routings[row], source, target);
    REQUIRE(result.accepted);
    REQUIRE(result.routing.sourceLfoIndex == source);
    REQUIRE(result.routing.targetParameterID == target);
    if (result.changed) processor.lfoDataHasChanged();
}
}
