#include <PluginProcessor.h>
#include <Panels/TopPanel/Preset.h>
#include <Utility/PresetParameterLocks.h>
#include "LegacyMidiFixture.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <thread>

namespace
{
namespace locks = fire::preset_locks;
void lockSet(FireAudioProcessor& processor, const juce::String& id, float value, bool gesture = false)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter);
    if (gesture) parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    if (gesture) parameter->endChangeGesture();
}
float lockRead(FireAudioProcessor& processor, const juce::String& id)
{
    auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter);
    return parameter->convertFrom0to1(parameter->getValue());
}
juce::XmlElement lockSound(FireAudioProcessor& processor)
{
    juce::XmlElement xml("WINGSFIRE"); state::saveStateToXml(processor, xml); return xml;
}
juce::XmlElement lockHost(FireAudioProcessor& processor)
{
    juce::MemoryBlock data; processor.getStateInformation(data);
    auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
    REQUIRE(xml); return *xml;
}
void lockRestore(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock data; juce::AudioProcessor::copyXmlToBinary(xml, data);
    processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
}
bool loadUserSound(FireAudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::XmlElement library("PRESETS");
    auto preset = std::make_unique<juce::XmlElement>(xml);
    preset->setTagName("lock-test-preset");
    preset->setAttribute("presetName", "Lock Test");
    preset->setAttribute("presetKey", "User/Lock Test.fire");
    library.addChildElement(preset.release());
    return processor.statePresets.recursivePresetLoad(library, "lock-test-preset");
}
struct LockTemporaryFolder
{
    juce::File directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("fire-preset-lock-tests", {}, false);
    LockTemporaryFolder() { REQUIRE(directory.createDirectory().wasOk()); }
    ~LockTemporaryFolder() { directory.deleteRecursively(); }
};
}

TEST_CASE("Preset loading keeps locked base values and undoes the complete load in one step", "[preset-locks][preset][edit-history]")
{
    FireAudioProcessor donor, target;
    lockSet(donor, "drive1", 12); lockSet(donor, "bias1", -.2f); lockSet(donor, OUTPUT_ID, -7);
    REQUIRE(donor.assignLfoToTarget(0, "drive1") == LfoManager::AssignmentResult::changed);
    donor.setModulationDepth("drive1", .27f);
    const auto preset = lockSound(donor);
    lockSet(target, "drive1", 41); lockSet(target, "bias1", .3f); lockSet(target, OUTPUT_ID, -2);
    target.checkpointEditHistory();
    REQUIRE(target.setPresetParameterLocked("drive1", true));
    REQUIRE(target.setPresetParameterLocked(OUTPUT_ID, true));
    REQUIRE(loadUserSound(target, preset));
    CHECK(lockRead(target, "drive1") == Catch::Approx(41));
    CHECK(lockRead(target, OUTPUT_ID) == Catch::Approx(-2));
    CHECK(lockRead(target, "bias1") == Catch::Approx(-.2f));
    CHECK(target.getModulationInfoForParameter("drive1").isModulated); // Locks preserve base, not incoming routing.
    REQUIRE(target.undoEdit());
    CHECK(lockRead(target, "drive1") == Catch::Approx(41));
    CHECK(lockRead(target, "bias1") == Catch::Approx(.3f));
    CHECK_FALSE(target.getModulationInfoForParameter("drive1").isModulated);
    CHECK(target.isPresetParameterLocked("drive1"));
    REQUIRE(target.redoEdit());
    CHECK(lockRead(target, "bias1") == Catch::Approx(-.2f));
    CHECK(target.getModulationInfoForParameter("drive1").isModulated);
}

TEST_CASE("Locks are browsing preferences and do not dirty sound or create audio undo entries", "[preset-locks][state][edit-history]")
{
    FireAudioProcessor processor;
    const auto before = lockSound(processor);
    CHECK_FALSE(processor.canUndoEdit());
    REQUIRE(processor.setPresetParameterLocked("drive1", true));
    REQUIRE(processor.setPresetParameterLocked("macro1", true));
    CHECK(lockSound(processor).isEquivalentTo(&before, false));
    CHECK_FALSE(processor.canUndoEdit());
    processor.clearPresetParameterLocks();
    CHECK(processor.getPresetParameterLocks().isEmpty());
    CHECK_FALSE(processor.canUndoEdit());

    lockSet(processor, "drive1", 30, true);
    REQUIRE(processor.undoEdit()); REQUIRE(processor.canRedoEdit());
    REQUIRE(processor.setPresetParameterLocked("drive1", true));
    CHECK(processor.canRedoEdit());
    REQUIRE(processor.redoEdit());
    CHECK(lockRead(processor, "drive1") == Catch::Approx(30));
    CHECK(processor.isPresetParameterLocked("drive1"));
}

TEST_CASE("Locks allow direct gestures automation and AB changes", "[preset-locks][state][edit-history]")
{
    FireAudioProcessor processor;
    lockSet(processor, "drive1", 12);
    processor.stateAB.copyAB(false);
    REQUIRE(processor.setPresetParameterLocked("drive1", true));
    lockSet(processor, "drive1", 48, true);
    CHECK(lockRead(processor, "drive1") == Catch::Approx(48));
    REQUIRE(processor.undoEdit()); CHECK(lockRead(processor, "drive1") == Catch::Approx(12));
    REQUIRE(processor.redoEdit()); CHECK(lockRead(processor, "drive1") == Catch::Approx(48));
    processor.stateAB.toggleAB(); CHECK(lockRead(processor, "drive1") == Catch::Approx(12));
    processor.stateAB.toggleAB(); CHECK(lockRead(processor, "drive1") == Catch::Approx(48));
    CHECK(processor.isPresetParameterLocked("drive1"));
    auto* parameter = processor.treeState.getParameter("drive1"); REQUIRE(parameter);
    std::thread automation([&] { parameter->setValueNotifyingHost(parameter->convertTo0to1(23)); });
    automation.join();
    CHECK(lockRead(processor, "drive1") == Catch::Approx(23));
}

TEST_CASE("Generic state loads and host restoration never apply preset locks to sound", "[preset-locks][state]")
{
    FireAudioProcessor donor, target;
    lockSet(donor, "drive1", 7); const auto sound = lockSound(donor); const auto host = lockHost(donor);
    REQUIRE(target.setPresetParameterLocked("drive1", true)); lockSet(target, "drive1", 65);
    REQUIRE(state::loadStateFromXml(sound, target));
    CHECK(lockRead(target, "drive1") == Catch::Approx(7));
    CHECK(target.isPresetParameterLocked("drive1"));
    lockSet(target, "drive1", 51);
    lockRestore(target, host);
    CHECK(lockRead(target, "drive1") == Catch::Approx(7));
    CHECK(target.getPresetParameterLocks().isEmpty());
}

TEST_CASE("Loading a legacy preset preserves locked parameters introduced by newer versions", "[preset-locks][preset][legacy]")
{
    FireAudioProcessor donor, target;
    auto legacy = lockSound(donor);
    fire::test::stripMidiPresetFamily(legacy);
    const auto seed = fire::lfo_playback::parameterID(0, fire::lfo_playback::seed);
    lockSet(target, seed, 1259);
    REQUIRE(target.setPresetParameterLocked(seed, true));
    REQUIRE(loadUserSound(target, legacy));
    CHECK(lockRead(target, seed) == 1259);
    target.clearPresetParameterLocks();
    REQUIRE(loadUserSound(target, legacy));
    CHECK(lockRead(target, seed) == 0);
}

TEST_CASE("Invalid presets remain atomic even when their malformed parameter is locked", "[preset-locks][preset][validation]")
{
    FireAudioProcessor donor, target;
    auto invalid = lockSound(donor); invalid.setAttribute("drive1", "NaN");
    lockSet(target, "drive1", 35); lockSet(target, "bias1", .35f);
    target.checkpointEditHistory();
    REQUIRE(target.setPresetParameterLocked("drive1", true));
    const auto before = lockSound(target);
    const auto name = target.statePresets.getPresetName();
    CHECK_FALSE(loadUserSound(target, invalid));
    CHECK(lockSound(target).isEquivalentTo(&before, false));
    CHECK(target.statePresets.getPresetName() == name);
    CHECK(target.isPresetParameterLocked("drive1"));
}

TEST_CASE("Lockable parameter discovery excludes structural identities and reserved controls", "[preset-locks][state]")
{
    FireAudioProcessor processor;
    const auto entries = processor.getLockableParameters();
    REQUIRE_FALSE(entries.empty());
    juce::StringArray found;
    for (const auto& entry : entries)
    {
        CHECK(entry.name.isNotEmpty()); CHECK(processor.treeState.getParameter(entry.id) != nullptr);
        CHECK_FALSE(found.contains(entry.id)); found.add(entry.id);
    }
    CHECK(found.contains("drive1")); CHECK(found.contains(OUTPUT_ID)); CHECK(found.contains("lfoSeed1"));
    const juce::StringArray structural {"not-a-parameter", NUM_BANDS_ID, "lfoPresent1",
        fire::effects::parameterID(1, 0, fire::effects::typeField),
        fire::module_order::parameterID(1, 0), fire::core_modules::presenceID(1, 0),
        fire::eq::parameterID(1, fire::eq::Field::present)};
    for (const auto& id : structural)
    {
        CAPTURE(id); CHECK_FALSE(found.contains(id));
        CHECK_FALSE(processor.setPresetParameterLocked(id, true));
        CHECK_FALSE(processor.isPresetParameterLocked(id));
    }
}

TEST_CASE("Project lock preferences round trip while old or malformed preferences degrade safely", "[preset-locks][state][validation]")
{
    FireAudioProcessor donor, restored;
    lockSet(donor, "drive1", 29);
    REQUIRE(donor.setPresetParameterLocked("drive1", true));
    REQUIRE(donor.setPresetParameterLocked(OUTPUT_ID, true));
    auto host = lockHost(donor);
    lockRestore(restored, host);
    CHECK(restored.isPresetParameterLocked("drive1")); CHECK(restored.isPresetParameterLocked(OUTPUT_ID));
    CHECK(lockRead(restored, "drive1") == Catch::Approx(29));
    auto* other = host.getChildByName("otherState"); REQUIRE(other);

    SECTION("Missing preference fields clear previous project locks")
    {
        other->removeAttribute(locks::versionAttribute); other->removeAttribute(locks::valuesAttribute);
        lockSet(restored, "drive1", 3); lockRestore(restored, host);
        CHECK(restored.getPresetParameterLocks().isEmpty());
        CHECK(lockRead(restored, "drive1") == Catch::Approx(29));
    }
    SECTION("Unknown and structural IDs are skipped and known duplicates collapse")
    {
        other->setAttribute(locks::valuesAttribute, "[\"drive1\",\"unknown-old-id\",\"numBands\",\"drive1\"]");
        lockRestore(restored, host);
        REQUIRE(restored.getPresetParameterLocks().size() == 1);
        CHECK(restored.isPresetParameterLocked("drive1"));
        CHECK(lockRead(restored, "drive1") == Catch::Approx(29));
    }
    SECTION("Bad JSON does not block the sound")
    {
        other->setAttribute(locks::valuesAttribute, "{not an array");
        lockSet(restored, "drive1", 3); lockRestore(restored, host);
        CHECK(restored.getPresetParameterLocks().isEmpty());
        CHECK(lockRead(restored, "drive1") == Catch::Approx(29));
    }
    SECTION("Unsupported preference schema does not block the sound")
    {
        other->setAttribute(locks::versionAttribute, locks::schemaVersion + 1);
        lockSet(restored, "drive1", 3); lockRestore(restored, host);
        CHECK(restored.getPresetParameterLocks().isEmpty());
        CHECK(lockRead(restored, "drive1") == Catch::Approx(29));
    }
}

TEST_CASE("Saved fire presets contain the resulting sound but no project lock preferences", "[preset-locks][preset][filesystem]")
{
    LockTemporaryFolder temporary;
    FireAudioProcessor processor;
    state::StatePresets library(processor, temporary.directory.getFullPathName());
    lockSet(processor, "drive1", 32);
    REQUIRE(processor.setPresetParameterLocked("drive1", true));
    const auto file = temporary.directory.getChildFile("Locked Sound.fire");
    REQUIRE(library.savePreset(file) == "Locked Sound");
    const auto xml = juce::parseXML(file); REQUIRE(xml);
    CHECK_FALSE(xml->hasAttribute(locks::versionAttribute));
    CHECK_FALSE(xml->hasAttribute(locks::valuesAttribute));
    CHECK_FALSE(xml->toString().contains(locks::valuesAttribute));
    FireAudioProcessor restored; REQUIRE(state::loadStateFromXml(*xml, restored));
    CHECK(lockRead(restored, "drive1") == Catch::Approx(32));
    CHECK(restored.getPresetParameterLocks().isEmpty());
}
