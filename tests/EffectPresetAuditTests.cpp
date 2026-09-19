#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
namespace fx = fire::effects;
namespace chain = fire::module_order;
void plain(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
juce::XmlElement snapshot(FireAudioProcessor& p)
{
    juce::XmlElement result("WINGSFIRE"); state::saveStateToXml(p, result); return result;
}
juce::XmlElement hostSnapshot(FireAudioProcessor& p)
{
    juce::MemoryBlock bytes; p.getStateInformation(bytes);
    auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(xml != nullptr); return *xml;
}
void restoreHost(FireAudioProcessor& p, const juce::XmlElement& xml)
{
    juce::MemoryBlock bytes; juce::AudioProcessor::copyXmlToBinary(xml, bytes);
    p.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
}
void checkState(FireAudioProcessor& p, const juce::XmlElement& expected)
{
    for (auto* parameter : p.getParameters())
    {
        auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        REQUIRE(identified != nullptr);
        CAPTURE(identified->paramID);
        if (fire::clouds_params::isReservedEngineParameterID(identified->paramID))
        {
            CHECK(expected.getDoubleAttribute(identified->paramID) == 1.0);
            CHECK_FALSE(parameter->isAutomatable());
            continue;
        }
        CHECK(parameter->getValue() == Catch::Approx(expected.getDoubleAttribute(identified->paramID)).margin(1.0e-6));
    }
    CHECK(p.isCurrentStateEquivalentToPreset(expected)); // Includes shape, polarity, depth and routing bypass.
}
void configure(FireAudioProcessor& p, int variant)
{
    // Exercise every old and new parameter, including inactive controls. Quantise
    // through the host parameter range, as the UI and host both do.
    int index = 0;
    for (auto* parameter : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter); REQUIRE(ranged != nullptr);
        const float value = static_cast<float>((++index * 17 + variant * 11) % 101) / 100.0f;
        ranged->setValueNotifyingHost(ranged->convertTo0to1(ranged->convertFrom0to1(value)));
    }
    plain(p, NUM_BANDS_ID, 4);
    for (int i = 0; i < 3; ++i)
    {
        plain(p, ParameterIDAndName::getIDString(LINE_STATE_ID, i), 1);
        plain(p, ParameterIDAndName::getIDString(FREQ_ID, i), std::array<float, 3>{200, 1400, 6500}[static_cast<size_t>(i)]);
    }
    for (int scope = 0; scope < fx::scopeCount; ++scope)
    {
        for (int slot = 0; slot < fx::slotCount; ++slot)
        {
            plain(p, fx::parameterID(scope, slot, fx::typeField), 1 + (slot + scope + variant) % 5);
            plain(p, fx::parameterID(scope, slot, fx::enabledField), (slot + variant) % 2);
            for (int control = 0; control < static_cast<int>(fx::controlCount); ++control)
            {
                const auto id = fx::parameterID(scope, slot, control);
                const auto value = static_cast<float>((scope * 13 + slot * 7 + control * 3 + variant) % 21) / 20;
                plain(p, id, value);
                if (control == 0 || control == 5)
                {
                    p.clearModulationForParameter(id);
                    REQUIRE(p.assignLfoToTarget((slot + scope + variant) % 4, id) == LfoManager::AssignmentResult::changed);
                    p.setModulationDepth(id, (slot % 2 == 0 ? -1.0f : 1.0f) * (0.1f + static_cast<float>(scope) * 0.2f));
                    if (slot % 3 == 0) p.getLfoManager().toggleBypassForRouting(id);
                    if (slot % 2 == 0) p.toggleBipolarMode(id);
                }
            }
        }
        for (int node : chain::defaults(scope)) if (node >= 0) p.moveModuleBefore(scope, node, -1);
        p.moveModuleBefore(scope, 5 + variant, 0);
        p.moveModuleBefore(scope, scope == 0 ? 1 : 2, 0);
    }
    for (int source = 0; source < 4; ++source)
    {
        LfoData shape;
        shape.points = {{0, 0.2f}, {0.2f + static_cast<float>(source) * 0.1f, 0.9f}, {1, 0.3f}};
        shape.curvatures = {0.4f, -0.7f};
        p.getLfoManager().setLfoData(source, shape);
    }
}
struct Directory
{
    juce::File path = juce::File::getCurrentWorkingDirectory().getChildFile("TestTemp").getChildFile("FirePresetAudit-" + juce::Uuid().toString());
    Directory() { REQUIRE(path.createDirectory().wasOk()); }
    ~Directory() { path.deleteRecursively(); }
};
void removeHostParameter(juce::XmlElement& host, const juce::String& id)
{
    auto* parameters = host.getChildByName("PARAMETERS"); REQUIRE(parameters != nullptr);
    auto* parameter = parameters->getChildByAttribute("id", id); REQUIRE(parameter != nullptr);
    parameters->removeChildElement(parameter, true);
    host.setAttribute("savedParameterCount", parameters->getNumChildElements());
}
}

TEST_CASE("Full effect presets survive disk round trips repeated serialization and both host AB sides", "[preset-audit][preset][state][insertfx][module-order]")
{
    Directory directory;
    FireAudioProcessor p;
    configure(p, 1);
    const auto a = snapshot(p);
    p.stateAB.copyAB(); p.stateAB.toggleAB();
    configure(p, 2);
    const auto b = snapshot(p);
    REQUIRE_FALSE(p.stateAB.isCurrentA());
    const auto host = hostSnapshot(p);
    FireAudioProcessor restored;
    restoreHost(restored, host);
    CHECK_FALSE(restored.stateAB.isCurrentA()); checkState(restored, b);
    restored.stateAB.toggleAB(); CHECK(restored.stateAB.isCurrentA()); checkState(restored, a);
    restored.stateAB.toggleAB(); checkState(restored, b);
    for (int scope = 0; scope < fx::scopeCount; ++scope) CHECK(restored.getModuleOrder(scope) == p.getModuleOrder(scope));

    state::StatePresets library(p, directory.path.getFullPathName());
    const auto file = directory.path.getChildFile(juce::String::fromUTF8("夜色.颗粒 delay.fire"));
    REQUIRE(library.savePreset(file).isNotEmpty());
    auto disk = juce::XmlDocument::parse(file); REQUIRE(disk != nullptr);
    checkState(p, *disk);
    configure(p, 3);
    juce::ComboBox menu; library.setPresetAndFolderNames(menu);
    REQUIRE(library.getCurrentPresetId() > 0);
    REQUIRE(library.loadPreset(library.comboBoxIdToTagNameMap[library.getCurrentPresetId()]));
    checkState(p, b);
    for (int round = 0; round < 6; ++round)
    {
        CAPTURE(round);
        auto xml = juce::XmlDocument::parse(snapshot(p).toString()); REQUIRE(xml != nullptr);
        REQUIRE(state::loadStateFromXml(*xml, restored)); checkState(restored, b);
        restoreHost(p, hostSnapshot(restored)); checkState(p, b);
    }
    p.removeInsertEffect(1, 0);
    REQUIRE_FALSE(p.isCurrentStateEquivalentToPreset(b));
    REQUIRE(p.addInsertEffect(1, fx::Type::delay) == 0);
    const auto reused = snapshot(p);
    REQUIRE(state::loadStateFromXml(reused, restored)); checkState(restored, reused);
    CHECK_FALSE(restored.getModulationInfoForParameter(fx::parameterID(1, 0, 0)).isModulated);
    CHECK(restored.getInsertEffectType(1, 0) == fx::Type::delay);
}

TEST_CASE("All stored legacy presets load cleanly and resave with empty optional chains", "[preset-audit][preset][state][legacy]")
{
    auto root = juce::File::getCurrentWorkingDirectory();
    for (int level = 0; level < 8 && ! root.getChildFile("tests/Presets").isDirectory(); ++level) root = root.getParentDirectory();
    const auto files = root.getChildFile("tests/Presets").findChildFiles(juce::File::findFiles, false, "*.fire");
    REQUIRE(files.size() >= 40);
    FireAudioProcessor loaded, restored;
    for (const auto& file : files)
    {
        CAPTURE(file.getFileName());
        loaded.addInsertEffect(0, fx::Type::reverb);
        loaded.moveModuleBefore(1, 2, 0);
        auto xml = juce::XmlDocument::parse(file); REQUIRE(xml != nullptr);
        REQUIRE(state::loadStateFromXml(*xml, loaded));
        CHECK(loaded.isCurrentStateEquivalentToPreset(*xml));
        for (int scope = 0; scope < fx::scopeCount; ++scope)
        {
            CHECK(loaded.getModuleOrder(scope) == chain::defaults(scope));
            for (int slot = 0; slot < fx::slotCount; ++slot) CHECK(loaded.getInsertEffectType(scope, slot) == fx::Type::none);
        }
        for (auto* id : fx::tapeIDs) CHECK(loaded.treeState.getRawParameterValue(id)->load() == Catch::Approx(0));
        const auto migrated = snapshot(loaded);
        REQUIRE(state::loadStateFromXml(migrated, restored)); checkState(restored, migrated);
    }
}

TEST_CASE("Incomplete or corrupt new parameter families cannot partially overwrite a preset or host state", "[preset-audit][preset][state][corrupt]")
{
    FireAudioProcessor source, destination;
    configure(source, 1); configure(destination, 2);
    const auto input = snapshot(source), untouched = snapshot(destination), host = hostSnapshot(source);
    juce::StringArray representatives {"ottDepth4", fx::tapeIDs[2]};
    for (int scope = 0; scope < fx::scopeCount; ++scope)
    {
        representatives.add(fx::parameterID(scope, 7, 5));
        representatives.add(fx::parameterID(scope, 0, fx::typeField));
        representatives.add(chain::parameterID(scope, 0));
        representatives.add(chain::parameterID(scope, 12));
    }
    for (const auto& id : representatives)
    {
        CAPTURE(id);
        auto truncated = input; truncated.removeAttribute(id);
        CHECK_FALSE(state::loadStateFromXml(truncated, destination));
        CHECK(destination.isCurrentStateEquivalentToPreset(untouched));
        auto truncatedHost = host; removeHostParameter(truncatedHost, id);
        restoreHost(destination, truncatedHost);
        CHECK(destination.isCurrentStateEquivalentToPreset(untouched));
    }
    for (const auto* marker : {"ottSchemaVersion", "insertEffectsSchemaVersion", "moduleOrderSchemaVersion"})
    {
        auto future = input; future.setAttribute(marker, 2);
        CHECK_FALSE(state::loadStateFromXml(future, destination));
        auto futureHost = host; futureHost.setAttribute(marker, 2); restoreHost(destination, futureHost);
        CHECK(destination.isCurrentStateEquivalentToPreset(untouched));
    }
    for (const auto* invalid : {"nan", "inf", "0.3oops", "-0.1", "1.1"})
    {
        auto corrupt = input; corrupt.setAttribute(chain::parameterID(1, 2), invalid);
        CHECK_FALSE(state::loadStateFromXml(corrupt, destination));
        CHECK(destination.isCurrentStateEquivalentToPreset(untouched));
    }
    // A damaged inactive A/B side falls back to the fully restored active state.
    auto corruptAB = host; auto* ab = corruptAB.getChildByName("AB_STATE"); REQUIRE(ab != nullptr);
    ab->removeAttribute(chain::parameterID(1, 0));
    restoreHost(destination, corruptAB); checkState(destination, input);
    destination.stateAB.toggleAB(); checkState(destination, input);
}

TEST_CASE("Band insertion and deletion migrate complete module chains before saving", "[preset-audit][preset][state][topology]")
{
    FireAudioProcessor p;
    plain(p, NUM_BANDS_ID, 2); plain(p, "lineState1", 1); plain(p, "freq1", 1500);
    p.addInsertEffect(1, fx::Type::granular); p.addInsertEffect(2, fx::Type::delay);
    plain(p, fx::parameterID(1, 0, 2), 0.73f); plain(p, fx::parameterID(2, 0, 2), 0.29f);
    p.moveModuleBefore(1, 5, 0); p.moveModuleBefore(2, 3, 0);
    const auto lowOrder = p.getModuleOrder(1), highOrder = p.getModuleOrder(2);
    p.assignLfoToTarget(1, fx::parameterID(1, 0, 2)); p.assignLfoToTarget(3, fx::parameterID(2, 0, 2));
    REQUIRE(p.addMultibandBand(0, 2, true, 300));
    CHECK(p.getModuleOrder(1) == chain::bandDefault);
    CHECK(p.getModuleOrder(2) == lowOrder); CHECK(p.getModuleOrder(3) == highOrder);
    CHECK(p.getInsertEffectType(1, 0) == fx::Type::none);
    CHECK(p.getInsertEffectType(2, 0) == fx::Type::granular);
    CHECK(p.getInsertEffectType(3, 0) == fx::Type::delay);
    FireAudioProcessor restored;
    auto inserted = snapshot(p); REQUIRE(state::loadStateFromXml(inserted, restored)); checkState(restored, inserted);
    REQUIRE(restored.deleteMultibandBand(0, 3));
    CHECK(restored.getModuleOrder(1) == lowOrder); CHECK(restored.getModuleOrder(2) == highOrder);
    CHECK(restored.treeState.getRawParameterValue(fx::parameterID(1, 0, 2))->load() == Catch::Approx(0.73f));
    CHECK(restored.treeState.getRawParameterValue(fx::parameterID(2, 0, 2))->load() == Catch::Approx(0.29f));
    CHECK(restored.getModulationInfoForParameter(fx::parameterID(1, 0, 2)).isModulated);
    CHECK(restored.getModulationInfoForParameter(fx::parameterID(2, 0, 2)).isModulated);
    auto deleted = snapshot(restored); restoreHost(p, hostSnapshot(restored)); checkState(p, deleted);
}

TEST_CASE("A host save during a reorder sees the complete preceding preset generation", "[preset-audit][preset][state][transaction]")
{
    FireAudioProcessor p;
    configure(p, 1);
    const auto before = snapshot(p);
    struct Listener : juce::AudioProcessorParameter::Listener
    {
        FireAudioProcessor& processor;
        std::unique_ptr<juce::XmlElement> flat, host;
        explicit Listener(FireAudioProcessor& p) : processor(p) {}
        void parameterValueChanged(int, float) override
        {
            if (flat) return;
            flat = std::make_unique<juce::XmlElement>(snapshot(processor));
            host = std::make_unique<juce::XmlElement>(hostSnapshot(processor));
        }
        void parameterGestureChanged(int, bool) override {}
    } listener(p);
    auto* parameter = p.treeState.getParameter(chain::parameterID(1, 0)); REQUIRE(parameter != nullptr);
    parameter->addListener(&listener);
    const juce::ScopeGuard cleanup {[&] { parameter->removeListener(&listener); }};
    p.moveModuleBefore(1, 0, -1);
    REQUIRE(listener.flat != nullptr); REQUIRE(listener.host != nullptr);
    FireAudioProcessor observed;
    REQUIRE(state::loadStateFromXml(*listener.flat, observed)); checkState(observed, before);
    restoreHost(observed, *listener.host); checkState(observed, before);
    const auto after = snapshot(p); CHECK_FALSE(p.isCurrentStateEquivalentToPreset(before));
    restoreHost(observed, hostSnapshot(p)); checkState(observed, after);
}

TEST_CASE("Preset modified status compares loaded parameter steps and still detects real edits", "[preset-audit][preset][state][equivalence]")
{
    FireAudioProcessor p;
    auto saved = snapshot(p);
    for (const juce::String& id : {juce::String(PEAK_GAIN_ID), juce::String(BIT_DEPTH_ID), juce::String("ottTime4"),
                                  fx::parameterID(0, 0, fx::typeField), chain::parameterID(1, 2)})
    {
        CAPTURE(id);
        auto input = saved;
        input.setAttribute(id, 0.1700000166893005);
        REQUIRE(state::loadStateFromXml(input, p));
        CHECK(p.isCurrentStateEquivalentToPreset(input));
        auto* parameter = p.treeState.getParameter(id); REQUIRE(parameter != nullptr);
        const auto& range = parameter->getNormalisableRange();
        REQUIRE(range.interval > 0);
        const auto value = parameter->convertFrom0to1(parameter->getValue());
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value + range.interval));
        CHECK_FALSE(p.isCurrentStateEquivalentToPreset(input));
        REQUIRE(state::loadStateFromXml(input, p));
        CHECK(p.isCurrentStateEquivalentToPreset(input));
    }
}

TEST_CASE("Preset and host restores reproduce the processed sound of reordered effect chains", "[preset-audit][preset][state][audio][hq]")
{
    const auto render = [](FireAudioProcessor& p) {
        constexpr int blockSize = 137, total = blockSize * 120;
        p.setRateAndBufferSizeDetails(48000, blockSize); p.prepareToPlay(48000, blockSize);
        juce::AudioBuffer<float> output(2, total), block(2, blockSize); juce::MidiBuffer midi;
        for (int start = 0; start < total; start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                block.setSample(0, i, 0.15f * std::sin(static_cast<float>(start + i) * 0.071f));
                block.setSample(1, i, 0.12f * std::cos(static_cast<float>(start + i) * 0.093f));
            }
            p.processBlock(block, midi);
            for (int channel = 0; channel < 2; ++channel) output.copyFrom(channel, start, block, channel, 0, blockSize);
        }
        return output;
    };
    for (bool hq : {false, true})
    {
        CAPTURE(hq);
        FireAudioProcessor original, fromPreset, fromHost;
        plain(original, HQ_ID, hq ? 1.0f : 0.0f);
        plain(original, NUM_BANDS_ID, 2); plain(original, "lineState1", 1); plain(original, "freq1", 800);
        for (int scope = 0; scope <= 2; ++scope)
        {
            for (int type = 1; type < static_cast<int>(fx::Type::count); ++type)
            {
                const auto slot = original.addInsertEffect(scope, static_cast<fx::Type>(type)); REQUIRE(slot >= 0);
                plain(original, fx::parameterID(scope, slot, 5), 0.4f);
            }
            original.moveModuleBefore(scope, 5, 0);
            original.moveModuleBefore(scope, 7, 1);
            if (scope > 0)
            {
                plain(original, "drive" + juce::String(scope), 32);
                plain(original, "linked" + juce::String(scope), 0);
                plain(original, "compressorBypass" + juce::String(scope), 1);
                plain(original, "compRatio" + juce::String(scope), 4);
                plain(original, "compThresh" + juce::String(scope), -20);
                plain(original, "ottEnabled" + juce::String(scope), 1);
                original.moveModuleBefore(scope, 2, 0);
                original.assignLfoToTarget(scope - 1, "drive" + juce::String(scope));
                original.setModulationDepth("drive" + juce::String(scope), 0.1f);
            }
        }
        plain(original, DOWNSAMPLE_BYPASS_ID, 1); plain(original, JITTER_ID, 0);
        plain(original, fx::tapeIDs[0], 0.35f); plain(original, fx::tapeIDs[2], 0.2f);
        const auto saved = snapshot(original);
        REQUIRE(state::loadStateFromXml(saved, fromPreset)); restoreHost(fromHost, hostSnapshot(original));
        const auto expected = render(original);
        CHECK(expected.getMagnitude(0, expected.getNumSamples()) > 0.001f);
        for (auto* restored : {&fromPreset, &fromHost})
        {
            const auto actual = render(*restored);
            float error = 0;
            bool finite = true;
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < expected.getNumSamples(); ++sample)
                {
                    finite = finite && std::isfinite(actual.getSample(channel, sample));
                    error = juce::jmax(error, std::abs(expected.getSample(channel, sample) - actual.getSample(channel, sample)));
                }
            CHECK(finite); CHECK(error < 1.0e-5f);
        }
    }
}
