#include <PluginEditor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <thread>

namespace
{
juce::RangedAudioParameter& parameter(FireAudioProcessor& processor, const juce::String& id)
{
    auto* result = processor.treeState.getParameter(id);
    REQUIRE(result != nullptr);
    return *result;
}
void gesture(FireAudioProcessor& processor, float value)
{
    auto& drive = parameter(processor, "drive1");
    drive.beginChangeGesture();
    drive.setValueNotifyingHost(value);
    drive.endChangeGesture();
}
}

TEST_CASE("Undo groups a complete knob gesture and new edits discard the redo branch", "[edit-history][state]")
{
    FireAudioProcessor processor;
    auto& drive = parameter(processor, "drive1");
    const auto initial = drive.getValue();
    drive.beginChangeGesture();
    for (float value : {0.1f, 0.3f, 0.7f}) drive.setValueNotifyingHost(value);
    drive.endChangeGesture();
    REQUIRE(processor.undoEdit());
    CHECK(drive.getValue() == Catch::Approx(initial));
    CHECK_FALSE(processor.canUndoEdit());
    REQUIRE(processor.redoEdit());
    CHECK(drive.getValue() == Catch::Approx(0.7f));
    REQUIRE(processor.undoEdit());
    gesture(processor, 0.4f);
    CHECK_FALSE(processor.canRedoEdit());
    REQUIRE(processor.undoEdit());
    CHECK(drive.getValue() == Catch::Approx(initial));
}

TEST_CASE("Undo restores effect identities order EQ nodes and LFO presence", "[edit-history][state][insertfx]")
{
    FireAudioProcessor processor;
    const int slot = processor.addInsertEffect(0, fire::effects::Type::delay);
    REQUIRE(slot == 0);
    REQUIRE(processor.undoEdit());
    CHECK(processor.getInsertEffectType(0, slot) == fire::effects::Type::none);
    REQUIRE(processor.redoEdit());
    CHECK(processor.getInsertEffectType(0, slot) == fire::effects::Type::delay);
    auto order = processor.getModuleOrder(0);
    processor.moveModuleBefore(0, fire::module_order::firstInsert, 0);
    REQUIRE(processor.undoEdit());
    CHECK(processor.getModuleOrder(0) == order);
    int node = processor.addEqNode(3000.0f, 5.0f);
    REQUIRE(node >= 0);
    REQUIRE(processor.undoEdit());
    CHECK_FALSE(processor.getEqNodeState(node).present);
    REQUIRE(processor.redoEdit());
    CHECK(processor.getEqNodeState(node).present);
    int source = processor.addLfo();
    REQUIRE(source == 4);
    REQUIRE(processor.undoEdit());
    CHECK_FALSE(processor.isLfoPresent(source));
    REQUIRE(processor.redoEdit());
    CHECK(processor.isLfoPresent(source));
}

TEST_CASE("Undo restores a complete curve together with modulation routing edits", "[edit-history][lfo][state]")
{
    FireAudioProcessor processor;
    auto original = processor.getLfoManager().captureSerializableStateSnapshot().lfoData[0];
    LfoData shape;
    shape.points = {{0.0f, 0.1f}, {0.3f, 0.9f}, {1.0f, 0.2f}};
    shape.curvatures = {0.5f, -0.5f};
    processor.getLfoManager().setLfoData(0, shape);
    processor.checkpointEditHistory();
    REQUIRE(processor.undoEdit());
    CHECK(processor.getLfoManager().captureSerializableStateSnapshot().lfoData[0].points == original.points);
    REQUIRE(processor.redoEdit());
    CHECK(processor.getLfoManager().captureSerializableStateSnapshot().lfoData[0].points == shape.points);
    REQUIRE(processor.assignLfoToTarget(0, "drive1") == LfoManager::AssignmentResult::changed);
    processor.checkpointEditHistory();
    REQUIRE(processor.undoEdit());
    CHECK_FALSE(processor.getModulationInfoForParameter("drive1").isModulated);
    REQUIRE(processor.redoEdit());
    CHECK(processor.getModulationInfoForParameter("drive1").isModulated);
    CHECK(processor.removeLfo(0));
    REQUIRE(processor.undoEdit());
    CHECK(processor.isLfoPresent(0));
    CHECK(processor.getModulationInfoForParameter("drive1").isModulated);
}

TEST_CASE("Undo survives editor sessions and retains the current window size", "[edit-history][ui]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    gesture(processor, 0.6f);
    processor.setSavedEditorSize(1600, 800);
    {
        FireAudioProcessorEditor editor(processor);
        REQUIRE(editor.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 'z')));
        CHECK(parameter(processor, "drive1").getValue() == Catch::Approx(0.0f));
    }
    FireAudioProcessorEditor reopened(processor);
    REQUIRE(reopened.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 'z')));
    CHECK(parameter(processor, "drive1").getValue() == Catch::Approx(0.6f));
    CHECK(processor.getSavedEditorSize().width == 1600);
}

TEST_CASE("Host restoration starts a fresh history and audio thread automation is not an edit", "[edit-history][state][threading]")
{
    FireAudioProcessor processor;
    auto& drive = parameter(processor, "drive1");
    std::thread audio([&] { drive.setValueNotifyingHost(0.3f); });
    audio.join();
    CHECK_FALSE(processor.canUndoEdit());
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    gesture(processor, 0.8f);
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK_FALSE(processor.canUndoEdit());
    CHECK_FALSE(processor.undoEdit());
    gesture(processor, 0.5f);
    REQUIRE(processor.undoEdit());
    CHECK(parameter(processor, "drive1").getValue() == Catch::Approx(0.3f));
}
