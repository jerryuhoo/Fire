#include <PluginProcessor.h>
#include <Utility/ChainSnapshotCodec.h>
#include <Utility/ModuleSnapshotMapping.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
juce::String serialisedChain(FireAudioProcessor& processor)
{
    fire::chain::Snapshot snapshot;
    REQUIRE(processor.captureChainSnapshot(1, true, snapshot).wasOk());
    auto xml = fire::chain::encode(snapshot);
    REQUIRE(xml != nullptr);
    return xml->toString();
}

class ReentrantChainReader final : public juce::AudioProcessorParameter::Listener
{
public:
    ReentrantChainReader(FireAudioProcessor& owner, juce::RangedAudioParameter& watched)
        : processor(owner), parameter(watched) { parameter.addListener(this); }
    ~ReentrantChainReader() override { parameter.removeListener(this); }
    void parameterValueChanged(int, float) override { observations.add(serialisedChain(processor)); }
    void parameterGestureChanged(int, bool) override {}
    juce::StringArray observations;
private:
    FireAudioProcessor& processor;
    juce::RangedAudioParameter& parameter;
};
}

TEST_CASE("Chain paste publishes one complete snapshot to reentrant parameter listeners",
          "[chain-operations][state][publication]")
{
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(1, fire::effects::Type::phaser) == 0);
    const auto target = fire::effects::parameterID(1, 0, 0);
    auto* parameter = processor.treeState.getParameter(target);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(.73f);
    REQUIRE(processor.assignLfoToTarget(0, target) == LfoManager::AssignmentResult::changed);
    const auto before = serialisedChain(processor);
    ReentrantChainReader reader(processor, *parameter);

    fire::chain::Snapshot replacement;
    replacement.modules.push_back(fire::chain::defaultModule(fire::effects::Type::delay));
    REQUIRE(processor.pasteChainSnapshot(1, replacement).wasOk());
    REQUIRE_FALSE(reader.observations.isEmpty());
    for (const auto& observation : reader.observations) CHECK(observation == before);
    CHECK(serialisedChain(processor) != before);
    fire::chain::Snapshot after;
    REQUIRE(processor.captureChainSnapshot(1, true, after).wasOk());
    REQUIRE(after.modules.size() == 1);
    CHECK(after.modules.front().type == fire::effects::Type::delay);
    CHECK(after.modules.front().routes.isEmpty());
}
