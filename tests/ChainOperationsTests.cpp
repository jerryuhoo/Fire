#include <PluginProcessor.h>
#include <Utility/ChainSnapshotCodec.h>
#include <Utility/ModuleSnapshotMapping.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

namespace
{
namespace cs = fire::chain;
namespace fx = fire::effects;
constexpr int firstInsert = fire::module_order::firstInsert;
void chainSet(FireAudioProcessor& processor, const juce::String& id, float value, bool normalised = false)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(normalised ? value : parameter->convertTo0to1(value));
}
float chainGet(FireAudioProcessor& processor, const juce::String& id)
{ const auto* parameter = processor.treeState.getRawParameterValue(id); REQUIRE(parameter != nullptr); return parameter->load(); }
juce::MemoryBlock chainHost(FireAudioProcessor& processor)
{ juce::MemoryBlock bytes; processor.getStateInformation(bytes); return bytes; }
juce::String chainText(const cs::Snapshot& snapshot)
{ auto xml = cs::encode(snapshot); REQUIRE(xml != nullptr); return xml->toString(); }
cs::Snapshot oneModule(fx::Type type)
{ cs::Snapshot snapshot; snapshot.modules.push_back(cs::defaultModule(type)); return snapshot; }
cs::Snapshot captureChain(FireAudioProcessor& processor, int scope, bool routes = true)
{ cs::Snapshot result; REQUIRE(processor.captureChainSnapshot(scope, routes, result).wasOk()); return result; }
void appendRoutes(FireAudioProcessor& processor, const juce::Array<ModulationRouting>& additions)
{
    auto& manager = processor.getLfoManager();
    const juce::ScopedLock lock(manager.getLfoDataLock());
    auto& routes = manager.getModulationRoutings();
    for (int index = routes.size(); --index >= 0;) if (routes[index].targetParameterID.isEmpty()) routes.remove(index);
    routes.addArray(additions); manager.advanceModulationRoutingRevisionLocked();
}
bool containsRoute(FireAudioProcessor& processor, const ModulationRouting& expected)
{
    for (const auto& route : processor.getLfoManager().getModulationRoutingsCopy())
        if (route.hasSameState(expected)) return true;
    return false;
}
ModulationRouting chainRoute(int source, const juce::String& target)
{
    ModulationRouting route {source, target, -.37f, false, false};
    route.customRange = true; route.rangeStart = -.2f; route.rangeEnd = .7f; route.curve = 3; route.depthMacro = 2;
    return route;
}
}

TEST_CASE("Module paste across Band and Master copies settings and only explicit modulation mappings",
          "[chain][operations][routing][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto slot = processor.addInsertEffect(1, fx::Type::delay); REQUIRE(slot == 0);
    chainSet(processor, fx::parameterID(1, slot, 0), .33f, true);
    chainSet(processor, fx::parameterID(1, slot, 5), .72f, true);
    chainSet(processor, fx::parameterID(1, slot, fx::enabledField), 0);
    chainSet(processor, "macro3", .81f); chainSet(processor, "env2Source", 4);
    chainSet(processor, "output2", -3.2f);
    const auto sourceRoute = chainRoute(23, fx::parameterID(1, slot, 0));
    const auto unrelated = chainRoute(17, "drive2");
    appendRoutes(processor, {sourceRoute, unrelated});
    const auto untouchedBand = chainText(captureChain(processor, 2));
    cs::Snapshot settings;
    REQUIRE(processor.captureModuleSnapshot(1, firstInsert, false, settings).wasOk());
    REQUIRE(settings.modules.size() == 1); CHECK(settings.modules[0].routes.isEmpty());
    int settingsNode = -1;
    REQUIRE(processor.pasteModuleSnapshot(0, 1, settings, settingsNode).wasOk());
    REQUIRE(settingsNode >= firstInsert);
    const auto settingsSlot = settingsNode - firstInsert;
    CHECK(processor.getInsertEffectType(0, settingsSlot) == fx::Type::delay);
    CHECK(chainGet(processor, fx::parameterID(0, settingsSlot, 0)) == Catch::Approx(.33f));
    CHECK(chainGet(processor, fx::parameterID(0, settingsSlot, 5)) == Catch::Approx(.72f));
    CHECK(chainGet(processor, fx::parameterID(0, settingsSlot, fx::enabledField)) == 0);
    CHECK_FALSE(processor.getModulationInfoForParameter(fx::parameterID(0, settingsSlot, 0)).isModulated);
    cs::Snapshot withRoutes;
    REQUIRE(processor.captureModuleSnapshot(1, firstInsert, true, withRoutes).wasOk());
    int routedNode = -1;
    REQUIRE(processor.pasteModuleSnapshot(0, settingsNode, withRoutes, routedNode).wasOk());
    auto expected = sourceRoute; expected.targetParameterID = fx::parameterID(0, routedNode - firstInsert, 0);
    CHECK(containsRoute(processor, expected)); CHECK(containsRoute(processor, sourceRoute)); CHECK(containsRoute(processor, unrelated));
    CHECK(chainGet(processor, "macro3") == Catch::Approx(.81f)); CHECK(chainGet(processor, "env2Source") == 4);
    CHECK(chainGet(processor, "output2") == Catch::Approx(-3.2f));
    CHECK(chainText(captureChain(processor, 2)) == untouchedBand);
    const auto order = processor.getModuleOrder(0);
    const auto first = std::find(order.begin(), order.end(), settingsNode);
    REQUIRE(first != order.end()); REQUIRE(std::next(first) != order.end()); CHECK(*std::next(first) == routedNode);
}

TEST_CASE("Whole chain paste preserves ordering and is one undoable replacement including routes",
          "[chain][operations][order][edit-history]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    REQUIRE(processor.pasteChainSnapshot(1, {}).wasOk());
    REQUIRE(processor.addInsertEffect(1, fx::Type::delay) == 0);
    REQUIRE(processor.addInsertEffect(1, fx::Type::chorus) == 1);
    REQUIRE(processor.addInsertEffect(1, fx::Type::reverb) == 2);
    processor.moveModuleBefore(1, firstInsert + 2, firstInsert);
    chainSet(processor, fx::parameterID(1, 2, 5), .63f, true);
    const auto sourceRoute = chainRoute(22, fx::parameterID(1, 2, 5));
    const auto retained = chainRoute(19, "drive3");
    const auto replaced = chainRoute(20, "drive2");
    appendRoutes(processor, {sourceRoute, retained, replaced});
    processor.checkpointEditHistory();
    const auto source = captureChain(processor, 1), before = captureChain(processor, 2);
    REQUIRE(source.modules.size() == 3); CHECK(source.modules[0].type == fx::Type::reverb);
    REQUIRE(processor.pasteChainSnapshot(2, source).wasOk());
    const auto pasted = captureChain(processor, 2);
    CHECK(chainText(pasted) == chainText(source));
    CHECK(containsRoute(processor, retained)); CHECK_FALSE(containsRoute(processor, replaced));
    REQUIRE(processor.undoEdit());
    CHECK(chainText(captureChain(processor, 2)) == chainText(before)); CHECK(containsRoute(processor, replaced));
    REQUIRE(processor.redoEdit());
    CHECK(chainText(captureChain(processor, 2)) == chainText(pasted));
    CHECK(chainText(captureChain(processor, 1)) == chainText(source));
}

TEST_CASE("An empty chain clears DSP modules while keeping Master Analysis and unrelated scopes",
          "[chain][operations][empty][edit-history]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    REQUIRE(processor.addInsertEffect(0, fx::Type::phaser) == 0);
    chainSet(processor, "output1", -5.0f);
    const auto band = chainText(captureChain(processor, 1));
    const auto before = chainText(captureChain(processor, 0));
    REQUIRE(processor.isModulePresent(0, 2));
    REQUIRE(processor.pasteChainSnapshot(0, {}).wasOk());
    CHECK(captureChain(processor, 0).modules.empty()); CHECK(processor.isModulePresent(0, 2));
    for (int node : processor.getModuleOrder(0)) if (node >= 0 && node != 2) CHECK_FALSE(processor.isModulePresent(0, node));
    CHECK(chainGet(processor, "output1") == Catch::Approx(-5)); CHECK(chainText(captureChain(processor, 1)) == band);
    REQUIRE(processor.undoEdit()); CHECK(chainText(captureChain(processor, 0)) == before);
    REQUIRE(processor.redoEdit()); CHECK(captureChain(processor, 0).modules.empty()); CHECK(processor.isModulePresent(0, 2));
}

TEST_CASE("Capacity routing and invalid snapshots reject entire chain operations before mutation",
          "[chain][operations][validation][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    chainSet(processor, "drive1", 38.0f);
    SECTION("full insert rack")
    {
        for (int slot = 0; slot < 8; ++slot) REQUIRE(processor.addInsertEffect(0, fx::Type::delay) == slot);
        const auto before = chainHost(processor); const auto revision = processor.getChainEditRevision(); int added = 987;
        CHECK(processor.pasteModuleSnapshot(0, firstInsert, oneModule(fx::Type::delay), added).failed());
        CHECK(added == 987); CHECK(chainHost(processor) == before); CHECK(processor.getChainEditRevision() == revision);
    }
    SECTION("whole chain cannot fit its module types")
    {
        cs::Snapshot tooMany; for (int index = 0; index < 9; ++index) tooMany.modules.push_back(cs::defaultModule(fx::Type::delay));
        const auto before = chainHost(processor);
        CHECK(processor.pasteChainSnapshot(0, tooMany).failed()); CHECK(chainHost(processor) == before);
    }
    SECTION("route capacity cannot discard another band's mappings")
    {
        juce::Array<ModulationRouting> routes;
        for (int index = 0; index < 128; ++index) routes.add(chainRoute(index % 24, "drive4"));
        appendRoutes(processor, routes);
        auto incoming = oneModule(fx::Type::delay); incoming.includesRouting = true;
        incoming.modules[0].routes.add(chainRoute(23, "control.1"));
        const auto before = chainHost(processor); int added = 987;
        CHECK(processor.pasteModuleSnapshot(1, 0, incoming, added).failed()); CHECK(added == 987);
        CHECK(processor.pasteChainSnapshot(1, incoming).failed()); CHECK(chainHost(processor) == before);
    }
    SECTION("invalid settings and scope")
    {
        auto invalid = oneModule(fx::Type::delay); invalid.modules[0].values["control.1"] = std::numeric_limits<float>::quiet_NaN();
        const auto before = chainHost(processor); int added = 987;
        CHECK(processor.pasteChainSnapshot(1, invalid).failed());
        CHECK(processor.pasteModuleSnapshot(1, 0, invalid, added).failed());
        CHECK(processor.pasteChainSnapshot(5, oneModule(fx::Type::delay)).failed());
        CHECK(processor.pasteModuleSnapshot(1, 99, oneModule(fx::Type::delay), added).failed());
        CHECK(added == 987); CHECK(chainHost(processor) == before);
        auto sentinel = oneModule(fx::Type::chorus); const auto expected = chainText(sentinel);
        CHECK(processor.captureModuleSnapshot(1, 99, true, sentinel).failed()); CHECK(chainText(sentinel) == expected);
        CHECK(processor.captureChainSnapshot(5, true, sentinel).failed()); CHECK(chainText(sentinel) == expected);
    }
}

TEST_CASE("Frozen material follows module paste and a single undo redo transaction",
          "[chain][operations][frozen-audio][edit-history]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    auto incoming = oneModule(fx::Type::granular);
    auto material = std::make_shared<fx::FrozenRecording>();
    material->head = material->validFrames = 64; material->samples[0][0] = 12345; material->samples[1][31] = -23456;
    incoming.modules[0].frozen = material; incoming.modules[0].values["granular.freeze"] = 1;
    int added = -1; REQUIRE(processor.pasteModuleSnapshot(0, 1, incoming, added).wasOk());
    REQUIRE(added >= firstInsert);
    cs::Snapshot copied; REQUIRE(processor.captureModuleSnapshot(0, added, false, copied).wasOk());
    REQUIRE(copied.modules[0].frozen != nullptr); CHECK(copied.modules[0].frozen->samples == material->samples);
    REQUIRE(processor.undoEdit()); CHECK_FALSE(processor.isModulePresent(0, added));
    REQUIRE(processor.redoEdit()); REQUIRE(processor.captureModuleSnapshot(0, added, false, copied).wasOk());
    REQUIRE(copied.modules[0].frozen != nullptr); CHECK(copied.modules[0].frozen->samples == material->samples);
}

TEST_CASE("A captured and pasted insert chain renders the same audible signal",
          "[chain][operations][audio][dsp]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source, restored;
    REQUIRE(source.pasteChainSnapshot(0, {}).wasOk());
    REQUIRE(source.addInsertEffect(0, fx::Type::chorus) == 0);
    REQUIRE(source.addInsertEffect(0, fx::Type::delay) == 1);
    chainSet(source, fx::parameterID(0, 0, 5), .6f, true);
    chainSet(source, fx::parameterID(0, 1, 0), .15f, true);
    chainSet(source, fx::parameterID(0, 1, 5), .35f, true);
    const auto captured = captureChain(source, 0, false);
    REQUIRE(restored.pasteChainSnapshot(0, captured).wasOk());
    for (auto* processor : {&source, &restored})
    { processor->setRateAndBufferSizeDetails(48000, 128); processor->prepareToPlay(48000, 128); }
    juce::AudioBuffer<float> a(2, 128), b(2, 128); juce::MidiBuffer midi;
    float maximumError = 0; double energy = 0;
    for (int block = 0; block < 120; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 128; ++sample)
                a.setSample(channel, sample, .12f * std::sin((block * 128 + sample) * .071f + channel * .3f));
        b.makeCopyOf(a); source.processBlock(a, midi); restored.processBlock(b, midi);
        if (block < 64) continue;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 128; ++sample)
            {
                REQUIRE(std::isfinite(a.getSample(channel, sample))); REQUIRE(std::isfinite(b.getSample(channel, sample)));
                maximumError = std::max(maximumError, std::abs(a.getSample(channel, sample) - b.getSample(channel, sample)));
                energy += a.getSample(channel, sample) * a.getSample(channel, sample);
            }
    }
    CHECK(energy > 1); CHECK(maximumError < 0.00001f);
}
