#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdlib>

namespace
{
template <typename T, typename Predicate>
T* findControl(juce::Component& owner, Predicate predicate)
{
    if (auto* control = dynamic_cast<T*>(&owner); control != nullptr && predicate(*control))
        return control;
    for (auto* child : owner.getChildren())
        if (auto* control = findControl<T>(*child, predicate))
            return control;
    return nullptr;
}

juce::Button& buttonWithId(juce::Component& owner, const juce::String& id)
{
    auto* result = findControl<juce::Button>(owner,
        [&](const auto& button) { return button.getComponentID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}

state::StateComponent& stateControls(FireAudioProcessorEditor& editor)
{
    auto* result = findControl<state::StateComponent>(editor, [](const auto&) { return true; });
    REQUIRE(result != nullptr);
    return *result;
}

void showEditor(FireAudioProcessorEditor& editor)
{
    editor.stopTimer();
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    REQUIRE(editor.isShowing());
}

void processAudio(FireAudioProcessor& processor, FireAudioProcessorEditor& editor, int blocks)
{
    constexpr int blockSize = 256;
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto phase = juce::MathConstants<float>::twoPi * 440.0f
                               * static_cast<float>(block * blockSize + sample) / 48000.0f;
            const auto value = 0.125f * std::sin(phase);
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }
        processor.processBlock(buffer, midi);
    }
    editor.timerCallback();
}

void exportSnapshotIfRequested(FireAudioProcessor& processor, FireAudioProcessorEditor& editor)
{
    const auto* path = std::getenv("FIRE_LOUDNESS_UI_SNAPSHOT");
    if (path == nullptr || *path == '\0')
        return;

    editor.setSize(1000, 500);
    buttonWithId(editor, "loudnessMatchToggle").triggerClick();
    processAudio(processor, editor, 64);
    const juce::File file(juce::String::fromUTF8(path));
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    const auto snapshot = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
    REQUIRE(snapshot.getWidth() == 1000);
    REQUIRE(snapshot.getHeight() == 500);
    CHECK(juce::PNGImageFormat().writeImageToStream(snapshot, *stream));
}
} // namespace

TEST_CASE("Editor loudness matching fits beside A B Copy and the preset browser",
          "[loudness-match][ui][editor][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    FireAudioProcessorEditor editor(processor);
    showEditor(editor);
    auto& states = stateControls(editor);
    auto& match = buttonWithId(editor, "loudnessMatchToggle");
    auto& learn = buttonWithId(editor, "loudnessMatchLearn");
    auto* ab = states.getToggleABButton();
    auto* copy = states.getCopyABButton();
    auto* preset = states.getPresetBox();
    REQUIRE(ab != nullptr);
    REQUIRE(copy != nullptr);
    REQUIRE(preset != nullptr);

    for (const auto size : { juce::Point<int>(1000, 500),
                              juce::Point<int>(1400, 700),
                              juce::Point<int>(2000, 1000),
                              juce::Point<int>(1000, 700) })
    {
        CAPTURE(size.x, size.y);
        editor.setSize(size.x, size.y);
        const std::array<juce::Component*, 5> controls { ab, copy, &match, &learn, preset };
        for (size_t i = 0; i < controls.size(); ++i)
        {
            CAPTURE(controls[i]->getTitle());
            REQUIRE(controls[i]->isShowing());
            const auto bounds = editor.getLocalArea(controls[i], controls[i]->getLocalBounds());
            CHECK_FALSE(bounds.isEmpty());
            CHECK(editor.getLocalBounds().contains(bounds));
            for (size_t j = i + 1; j < controls.size(); ++j)
            {
                const auto other = editor.getLocalArea(controls[j], controls[j]->getLocalBounds());
                CHECK_FALSE(bounds.intersects(other));
            }
        }

        const auto copyBounds = editor.getLocalArea(copy, copy->getLocalBounds());
        const auto matchBounds = editor.getLocalArea(&match, match.getLocalBounds());
        const auto learnBounds = editor.getLocalArea(&learn, learn.getLocalBounds());
        const auto presetBounds = editor.getLocalArea(preset, preset->getLocalBounds());
        CHECK(copyBounds.getRight() < matchBounds.getX());
        CHECK(matchBounds.getRight() < learnBounds.getX());
        CHECK(learnBounds.getRight() < presetBounds.getX());
        CHECK(presetBounds.getWidth() >= juce::roundToInt(180.0f * size.x / 1000.0f));
    }

    exportSnapshotIfRequested(processor, editor);
}

TEST_CASE("Editor loudness match buttons wire enable learn cancel Copy and A B to the processor",
          "[loudness-match][ui][editor][integration]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    FireAudioProcessorEditor editor(processor);
    showEditor(editor);
    auto& states = stateControls(editor);
    auto& match = buttonWithId(editor, "loudnessMatchToggle");
    auto& learn = buttonWithId(editor, "loudnessMatchLearn");
    CHECK_FALSE(processor.getLoudnessMatchState().enabled);
    CHECK(learn.getButtonText().contains("Off"));

    match.triggerClick();
    REQUIRE(processor.getLoudnessMatchState().enabled);
    REQUIRE(processor.getLoudnessMatchState().measuring);
    CHECK(processor.getLoudnessMatchState().side == 0);
    CHECK(match.getToggleState());
    processAudio(processor, editor, 32);
    CHECK(processor.getLoudnessMatchState().progress > 0.0f);
    CHECK(learn.getButtonText().startsWith("A"));
    CHECK(learn.getButtonText().contains("%"));

    learn.triggerClick();
    CHECK_FALSE(processor.getLoudnessMatchState().measuring);
    CHECK(processor.getLoudnessMatchState().enabled);
    learn.triggerClick();
    REQUIRE(processor.getLoudnessMatchState().measuring);

    states.getCopyABButton()->triggerClick();
    CHECK(processor.getLoudnessMatchState().side == 0);
    CHECK(processor.getLoudnessMatchState().enabled);
    CHECK(processor.getLoudnessMatchState().measuring);
    states.getToggleABButton()->triggerClick();
    REQUIRE(processor.getLoudnessMatchState().side == 1);
    CHECK(processor.getLoudnessMatchState().enabled);
    processAudio(processor, editor, 64);
    CHECK(processor.getLoudnessMatchState().measuring);
    CHECK(processor.getLoudnessMatchState().progress > 0.0f);
    CHECK(learn.getButtonText().startsWith("B"));
    CHECK(learn.getButtonText().contains("%"));

    learn.triggerClick();
    CHECK_FALSE(processor.getLoudnessMatchState().measuring);
    match.triggerClick();
    CHECK_FALSE(processor.getLoudnessMatchState().enabled);
    CHECK_FALSE(match.getToggleState());
    CHECK_FALSE(learn.isEnabled());
    CHECK(learn.getButtonText().contains("Off"));

    editor.setVisible(false);
    match.triggerClick();
    learn.triggerClick();
    CHECK_FALSE(processor.getLoudnessMatchState().enabled);
}
