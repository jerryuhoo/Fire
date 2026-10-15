#include <PluginProcessor.h>
#include <DSP/ModulatedValueProvider.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <vector>

struct MultiSourceModulationTestAccess
{
    static const juce::AudioBuffer<float>& storage(const LfoManager& manager) { return manager.lfoOutputBuffer; }
    static size_t activeTargets(const LfoManager& manager) { return manager.runtimeActiveTargetCount; }
    static int remaining(const LfoManager& manager, int channel)
    { return manager.aggregateTransitions[static_cast<size_t>(channel - fire::mod_sources::firstAggregateSignal)].remaining; }
};

namespace
{
namespace multi_sources = fire::mod_sources;
void multiSet(FireAudioProcessor& processor, const juce::String& id, float plain)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}
ModulationRouting multiRoute(int source, const char* target, float depth, bool bipolar = false)
{
    ModulationRouting route { source, target, depth, bipolar, false };
    return route;
}
struct MultiFixture
{
    FireAudioProcessor processor;
    LfoManager& manager = processor.getLfoManager();
    juce::AudioBuffer<float> signals {multi_sources::signalCount, 4096};
    explicit MultiFixture(int capacity = 64)
    {
        processor.hasUpdateCheckBeenPerformed = true;
        multiSet(processor, "drive1", 75.0f);
        manager.prepare({48000, static_cast<juce::uint32>(capacity), 2});
    }
    void routes(juce::Array<ModulationRouting> next)
    {
        const juce::ScopedLock lock(manager.getLfoDataLock());
        manager.getModulationRoutings() = std::move(next);
        manager.advanceModulationRoutingRevisionLocked();
    }
    void render(int count = 64, int channels = multi_sources::signalCount)
    {
        signals.setSize(channels, count, false, false, true);
        manager.processBlock(signals, 48000, nullptr, count);
    }
    LfoManager::AudioThreadRoutingInfo info(const char* id = "drive1")
    {
        LfoManager::AudioThreadRoutingInfo result;
        REQUIRE(manager.getAudioThreadRoutingInfo(processor.treeState.getParameter(id), result));
        return result;
    }
    ModulatedValueProvider provider(const char* id = "drive1")
    {
        auto* parameter = processor.treeState.getParameter(id);
        const auto route = info(id);
        ModulatedValueProvider value;
        value.baseValue = parameter->convertFrom0to1(parameter->getValue());
        value.range = parameter->getNormalisableRange();
        value.modulationDepth = route.depth;
        value.isBipolar = route.isBipolar;
        value.signalIsOffset = route.signalIsOffset;
        value.lfoSignal = signals.getReadPointer(route.sourceLfoIndex);
        return value;
    }
};
}

TEST_CASE("Ordinary single source preserves the legacy recipe and exact arithmetic", "[multi-mod][dsp][legacy]")
{
    for (const bool bipolar : {false, true})
    {
        MultiFixture fixture;
        multiSet(fixture.processor, "macro1", .37f);
        fixture.routes({multiRoute(multi_sources::firstMacro, "drive1", -.61f, bipolar)});
        fixture.render();
        const auto info = fixture.info();
        CHECK(info.sourceLfoIndex == multi_sources::firstMacro);
        CHECK(info.depth == -.61f);
        CHECK(info.isBipolar == bipolar);
        CHECK_FALSE(info.signalIsOffset);
        const auto value = fixture.provider();
        for (int sample = 0; sample < fixture.signals.getNumSamples(); ++sample)
        {
            const auto raw = fixture.signals.getSample(info.sourceLfoIndex, sample);
            const auto mapped = bipolar ? raw * 2.0f - 1.0f : raw;
            const auto depth = bipolar ? -.61f * .5f : -.61f;
            const auto base = value.range.convertTo0to1(value.baseValue);
            const auto expected = value.range.convertFrom0to1(juce::jlimit(0.0f, 1.0f, base + mapped * depth));
            CHECK(value.get(sample) == expected);
        }
        // The manager's legacy representative starts from the parameter's
        // stored normalised value and finishes with the parameter's snapped
        // convertFrom0to1. The sample provider uses continuous NormalisableRange
        // conversions. In particular Drive's 0.01 interval can change the final
        // float by one ULP. Compare each preserved path with its own exact
        // arithmetic, without weakening the per-sample assertions.
        auto* parameter = fixture.processor.treeState.getParameter("drive1");
        float representativeSource = fixture.signals.getSample(info.sourceLfoIndex, 0);
        float representativeDepth = info.depth;
        if (bipolar)
        {
            representativeSource = representativeSource * 2.0f - 1.0f;
            representativeDepth *= .5f;
        }
        const auto representativeOffset = representativeSource * representativeDepth;
        const auto expectedRepresentative = parameter->convertFrom0to1(juce::jlimit(0.0f, 1.0f,
            parameter->getValue() + representativeOffset));
        CHECK(fixture.manager.getModulatedValue("drive1") == expectedRepresentative);
    }
}

TEST_CASE("Multiple contributors sum signed offsets before the target is clipped", "[multi-mod][dsp][sum]")
{
    MultiFixture fixture;
    multiSet(fixture.processor, "macro1", 1);
    multiSet(fixture.processor, "macro2", 1);
    fixture.routes({multiRoute(multi_sources::firstMacro, "drive1", 1),
                    multiRoute(multi_sources::firstMacro + 1, "drive1", -1)});
    fixture.render();
    const auto info = fixture.info();
    REQUIRE(info.signalIsOffset);
    REQUIRE(multi_sources::isAggregateSignal(info.sourceLfoIndex));
    CHECK(info.depth == 1);
    CHECK_FALSE(info.isBipolar);
    const auto value = fixture.provider();
    for (int sample = 0; sample < fixture.signals.getNumSamples(); ++sample)
    {
        CHECK(fixture.signals.getSample(info.sourceLfoIndex, sample) == 0);
        CHECK(value.get(sample) == Catch::Approx(75.0f));
    }
    CHECK(fixture.manager.getModulatedValue("drive1") == Catch::Approx(75.0f));
}

TEST_CASE("Advanced route ranges curves and macro depth match their sample mapping", "[multi-mod][dsp][mapping]")
{
    for (int curve = 0; curve < 4; ++curve)
    {
        CAPTURE(curve);
        MultiFixture fixture;
        multiSet(fixture.processor, "macro1", .25f);
        multiSet(fixture.processor, "macro2", .8f);
        auto route = multiRoute(multi_sources::firstMacro, "drive1", .5f);
        route.customRange = true; route.rangeStart = -.4f; route.rangeEnd = .8f;
        route.curve = curve; route.depthMacro = 1;
        fixture.routes({route});
        fixture.render();
        const auto info = fixture.info();
        REQUIRE(info.signalIsOffset);
        const float mapped[] {.25f, .0625f, .4375f, .15625f};
        const auto expectedOffset = (-.4f + 1.2f * mapped[curve]) * .5f * .8f;
        for (int sample = 0; sample < fixture.signals.getNumSamples(); ++sample)
            CHECK(fixture.signals.getSample(info.sourceLfoIndex, sample) == Catch::Approx(expectedOffset).margin(1.e-7f));
        CHECK(fixture.provider().get(0) == Catch::Approx((.75f + expectedOffset) * 100.0f));
    }
}

TEST_CASE("Aggregate configuration transitions are continuous and independent of callback partitioning", "[multi-mod][dsp][transition][partitioning]")
{
    const auto render = [](const std::vector<int>& partition)
    {
        MultiFixture fixture;
        multiSet(fixture.processor, "macro1", .25f);
        auto route = multiRoute(multi_sources::firstMacro, "drive1", 1);
        route.customRange = true; route.rangeEnd = .4f;
        fixture.routes({route}); fixture.render();
        const auto channel = fixture.info().sourceLfoIndex;
        CHECK(fixture.signals.getSample(channel, 63) == Catch::Approx(.1f));
        route.rangeEnd = 1;
        fixture.routes({route});
        std::vector<float> result;
        result.reserve(1000);
        int rendered = 0;
        size_t index = 0;
        while (rendered < 1000)
        {
            const auto count = juce::jmin(partition[index++ % partition.size()], 1000 - rendered);
            fixture.render(count);
            REQUIRE(fixture.info().sourceLfoIndex == channel);
            for (int sample = 0; sample < count; ++sample) result.push_back(fixture.signals.getSample(channel, sample));
            rendered += count;
        }
        CHECK(MultiSourceModulationTestAccess::remaining(fixture.manager, channel) == 0);
        return result;
    };
    const auto regular = render({64});
    const auto irregular = render({1, 7, 127, 31, 3});
    REQUIRE(regular.size() == irregular.size());
    CHECK(regular.front() == Catch::Approx(.1f));
    CHECK(regular[960] == Catch::Approx(.25f));
    for (size_t sample = 0; sample < regular.size(); ++sample)
    {
        CHECK(regular[sample] == irregular[sample]);
        if (sample > 0) CHECK(regular[sample] >= regular[sample - 1]);
    }
}

TEST_CASE("Depth macro motion follows samples without restarting configuration smoothing", "[multi-mod][dsp][macro-depth]")
{
    MultiFixture fixture;
    multiSet(fixture.processor, "macro1", .75f);
    auto route = multiRoute(multi_sources::firstMacro, "drive1", .8f);
    route.curve = 3; route.depthMacro = 1;
    fixture.routes({route}); fixture.render();
    const auto channel = fixture.info().sourceLfoIndex;
    multiSet(fixture.processor, "macro2", 1);
    for (int callback = 0; callback < 17; ++callback)
    {
        fixture.render();
        REQUIRE(MultiSourceModulationTestAccess::remaining(fixture.manager, channel) == 0);
        for (int sample = 0; sample < fixture.signals.getNumSamples(); ++sample)
        {
            const auto source = fixture.signals.getSample(multi_sources::firstMacro, sample);
            const auto depth = fixture.signals.getSample(multi_sources::firstMacro + 1, sample);
            CHECK(fixture.signals.getSample(channel, sample) == route.offset(source, depth));
        }
    }
}

TEST_CASE("Aggregate target channels remain stable when other targets are inserted removed or reordered", "[multi-mod][dsp][identity]")
{
    MultiFixture fixture;
    auto first = multiRoute(multi_sources::firstMacro, "drive1", .5f); first.curve = 1;
    auto second = multiRoute(multi_sources::firstMacro + 1, "bias1", .5f); second.curve = 2;
    fixture.routes({first, second}); fixture.render();
    const auto firstChannel = fixture.info("drive1").sourceLfoIndex;
    const auto secondChannel = fixture.info("bias1").sourceLfoIndex;
    fixture.routes({second, first}); fixture.render();
    CHECK(fixture.info("drive1").sourceLfoIndex == firstChannel);
    CHECK(fixture.info("bias1").sourceLfoIndex == secondChannel);
    auto third = multiRoute(multi_sources::firstMacro + 2, "mix1", .5f); third.curve = 1;
    fixture.routes({third, second}); fixture.render();
    CHECK(fixture.info("bias1").sourceLfoIndex == secondChannel);
    CHECK(fixture.info("mix1").sourceLfoIndex != secondChannel);
    CHECK(MultiSourceModulationTestAccess::remaining(fixture.manager, secondChannel) == 0);
}

TEST_CASE("Aborted modulation capture cannot publish a new aggregate recipe", "[multi-mod][dsp][snapshot]")
{
    MultiFixture fixture;
    const auto route = multiRoute(multi_sources::firstMacro, "drive1", .5f);
    fixture.routes({route}); fixture.render();
    CHECK_FALSE(fixture.info().signalIsOffset);
    fixture.routes({route, multiRoute(multi_sources::firstMacro + 1, "drive1", .5f)});
    std::atomic<std::uint32_t> generation {2};
    REQUIRE(fixture.manager.beginAudioThreadStateCapture(2, generation));
    CHECK(fixture.info().signalIsOffset);
    fixture.manager.abortAudioThreadStateCapture();
    CHECK_FALSE(fixture.info().signalIsOffset);
    REQUIRE(fixture.manager.beginAudioThreadStateCapture(2, generation));
    REQUIRE(fixture.manager.finishAudioThreadStateCapture(true));
    CHECK(fixture.info().signalIsOffset);
}

TEST_CASE("Legacy sized output buffers still expose the aggregate representative value", "[multi-mod][dsp][compatibility]")
{
    MultiFixture fixture;
    multiSet(fixture.processor, "macro1", .5f);
    multiSet(fixture.processor, "macro2", .25f);
    fixture.routes({multiRoute(multi_sources::firstMacro, "drive1", .2f),
                    multiRoute(multi_sources::firstMacro + 1, "drive1", -.4f)});
    fixture.render(64, multi_sources::sourceCount);
    CHECK(fixture.manager.getModulatedValue("drive1") == Catch::Approx(75.0f));
}

TEST_CASE("Multiple target assignment and bulk controls preserve every contributor", "[multi-mod][routing]")
{
    MultiFixture fixture;
    fixture.routes({});
    REQUIRE(fixture.manager.assignLfoToTarget(multi_sources::firstMacro, "drive1") == LfoManager::AssignmentResult::changed);
    REQUIRE(fixture.manager.assignLfoToTarget(multi_sources::firstMacro + 1, "drive1") == LfoManager::AssignmentResult::changed);
    CHECK(fixture.manager.assignLfoToTarget(multi_sources::firstMacro, "drive1") == LfoManager::AssignmentResult::unchanged);
    REQUIRE(fixture.manager.getModulationRoutingsCopy().size() == 2);
    CHECK(fixture.manager.invertModulationDepth("drive1"));
    for (const auto& route : fixture.manager.getModulationRoutingsCopy()) CHECK(route.depth == -.5f);
    CHECK(fixture.manager.toggleBypassForRouting("drive1"));
    for (const auto& route : fixture.manager.getModulationRoutingsCopy()) CHECK(route.isBypassed);
    CHECK(fixture.manager.toggleBypassForRouting("drive1"));
    for (const auto& route : fixture.manager.getModulationRoutingsCopy()) CHECK_FALSE(route.isBypassed);
    auto snapshot = fixture.manager.getModulationRoutingStateSnapshot();
    const auto added = fixture.manager.addEmptyModulationRoutingIfRevisionMatches(snapshot.revision);
    REQUIRE(added.changed);
    snapshot = fixture.manager.getModulationRoutingStateSnapshot();
    REQUIRE(fixture.manager.assignModulationRoutingIfRevisionMatches(2, snapshot.revision,
        snapshot.routings[2], multi_sources::firstMacro + 2, "drive1").changed);
    int contributors = 0;
    for (const auto& route : fixture.manager.getModulationRoutingsCopy()) contributors += route.targetParameterID == "drive1";
    CHECK(contributors == 3);
    CHECK(fixture.manager.clearModulationForTarget("drive1"));
    for (const auto& route : fixture.manager.getModulationRoutingsCopy()) CHECK(route.targetParameterID.isEmpty());
}

TEST_CASE("Maximum aggregate routing and oversized partitions retain prepared signal storage", "[multi-mod][dsp][realtime][capacity]")
{
    MultiFixture fixture(64);
    multiSet(fixture.processor, "macro1", .5f);
    juce::Array<ModulationRouting> routes;
    for (int i = 0; i < LfoManager::maximumModulationRoutings; ++i)
        routes.add(multiRoute(multi_sources::firstMacro, "drive1", i % 2 == 0 ? .5f : -.5f));
    fixture.routes(routes);
    std::array<const float*, multi_sources::signalCount> pointers;
    for (size_t channel = 0; channel < pointers.size(); ++channel)
        pointers[channel] = MultiSourceModulationTestAccess::storage(fixture.manager).getReadPointer(static_cast<int>(channel));
    for (int count : {1, 64, 127, 7, 2049, 4096})
    {
        fixture.render(count);
        const auto info = fixture.info();
        REQUIRE(info.signalIsOffset);
        for (int sample = 0; sample < count; ++sample) CHECK(fixture.signals.getSample(info.sourceLfoIndex, sample) == 0);
        const auto& storage = MultiSourceModulationTestAccess::storage(fixture.manager);
        CHECK(storage.getNumChannels() == multi_sources::signalCount);
        CHECK(storage.getNumSamples() == 64);
        for (size_t channel = 0; channel < pointers.size(); ++channel)
            CHECK(storage.getReadPointer(static_cast<int>(channel)) == pointers[channel]);
    }
}

TEST_CASE("A surviving high aggregate channel stays stable with a dense one-target lookup", "[multi-mod][dsp][identity][capacity]")
{
    MultiFixture fixture;
    fixture.routes({}); fixture.render();
    CHECK(MultiSourceModulationTestAccess::activeTargets(fixture.manager) == 0);
    juce::Array<ModulationRouting> routes;
    for (auto* parameter : fixture.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr || multi_sources::isParameterID(ranged->paramID)) continue;
        auto route = multiRoute(multi_sources::firstMacro, "", .5f);
        route.targetParameterID = ranged->paramID;
        route.curve = 1;
        routes.add(route);
        if (routes.size() == LfoManager::maximumModulationRoutings) break;
    }
    REQUIRE(routes.size() == LfoManager::maximumModulationRoutings);
    fixture.routes(routes); fixture.render();
    const auto survivor = routes.getLast();
    auto* parameter = fixture.processor.treeState.getParameter(survivor.targetParameterID);
    LfoManager::AudioThreadRoutingInfo before, after;
    REQUIRE(fixture.manager.getAudioThreadRoutingInfo(parameter, before));
    REQUIRE(before.sourceLfoIndex == multi_sources::signalCount - 1);
    fixture.routes({survivor}); fixture.render();
    REQUIRE(fixture.manager.getAudioThreadRoutingInfo(parameter, after));
    CHECK(after.sourceLfoIndex == before.sourceLfoIndex);
    CHECK(MultiSourceModulationTestAccess::activeTargets(fixture.manager) == 1);
    CHECK(MultiSourceModulationTestAccess::remaining(fixture.manager, after.sourceLfoIndex) == 0);
    // A candidate with no routes must not remove the active lookup on abort.
    fixture.routes({});
    std::atomic<std::uint32_t> generation {2};
    REQUIRE(fixture.manager.beginAudioThreadStateCapture(2, generation));
    CHECK_FALSE(fixture.manager.getAudioThreadRoutingInfo(parameter, after));
    fixture.manager.abortAudioThreadStateCapture();
    REQUIRE(fixture.manager.getAudioThreadRoutingInfo(parameter, after));
    CHECK(after.sourceLfoIndex == before.sourceLfoIndex);
    CHECK(MultiSourceModulationTestAccess::activeTargets(fixture.manager) == 1);
    fixture.render();
    CHECK(MultiSourceModulationTestAccess::activeTargets(fixture.manager) == 0);
    CHECK_FALSE(fixture.manager.getAudioThreadRoutingInfo(parameter, after));
}
