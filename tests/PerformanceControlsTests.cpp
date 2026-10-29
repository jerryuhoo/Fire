#include <GUI/LfoPlaybackControls.h>
#include <GUI/MidiControls.h>
#include <GUI/EffectRackNavigation.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>

struct PerformanceControlsTestAccess
{
    static void tick(fire::ui::LfoPlaybackControls& controls) { controls.refresh(); }
    static void tick(fire::ui::MidiControls& controls) { controls.poll(); }
    static std::function<void(int)> choice(ContextAwareComboBox& combo) { return combo.createPopupResultHandler(); }
    static auto& seed(fire::ui::LfoPlaybackControls& c) { return c.seed; }
    static auto& note(fire::ui::LfoPlaybackControls& c) { return c.note; }
    static auto& playback(fire::ui::LfoPlaybackControls& c) { return c.playback; }
    static auto& wave(fire::ui::LfoPlaybackControls& c) { return c.wave; }
    static auto& trigger(fire::ui::LfoPlaybackControls& c) { return c.trigger; }
    static auto& row(fire::ui::MidiControls& c, int slot) { return c.rows[static_cast<size_t>(slot)]; }
    static auto token(fire::ui::MidiControls& c) { return c.learnToken; }
    static void open(LfoPanel& panel, bool midi) { panel.showPerformanceDialog(midi); }
    static auto dialog(LfoPanel& panel, bool midi) { return midi ? panel.midiDialog : panel.playbackDialog; }
    static void select(LfoPanel& panel, int slot) { panel.setLfo(slot); }
    static auto& button(LfoPanel& panel, bool midi) { return midi ? panel.midiControlsButton : panel.playbackSettingsButton; }
};
namespace
{
using Access = PerformanceControlsTestAccess;
namespace play = fire::lfo_playback;
namespace midi = fire::midi_mod;
void show(juce::Component& control)
{ control.addToDesktop(juce::ComponentPeer::windowIsTemporary); control.setVisible(true); }
float read(FireAudioProcessor& processor, const juce::String& id)
{ auto* value = processor.treeState.getRawParameterValue(id); REQUIRE(value); return value->load(); }
void choose(ContextAwareComboBox& combo, int id) { Access::choice(combo)(id); }
void processCC(FireAudioProcessor& processor, int channel, int number, int value)
{
    juce::AudioBuffer<float> audio(2, 64); audio.clear(); juce::MidiBuffer events;
    events.addEvent(juce::MidiMessage::controllerEvent(channel, number, value), 0);
    processor.processBlock(audio, events);
}
void writePreview(juce::Component& control, const juce::String& name)
{
    if (const auto* directory = std::getenv("FIRE_PERFORMANCE_PREVIEW_DIR"))
    {
        const auto image = control.createComponentSnapshot(control.getLocalBounds()); REQUIRE(image.isValid());
        const auto file = juce::File(directory).getChildFile(name + ".png");
        REQUIRE(file.getParentDirectory().createDirectory().wasOk());
        auto stream = file.createOutputStream(); REQUIRE(stream); stream->setPosition(0); stream->truncate();
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *stream));
    }
}
}

TEST_CASE("Playback controls bind their LFO and expose random triggering without changing its curve", "[performance-ui][lfo-playback][ui]")
{
    FireAudioProcessor processor;
    fire::ui::LfoPlaybackControls control(processor, 1); show(control);
    CHECK_FALSE(Access::seed(control).isEnabled());
    CHECK_FALSE(Access::trigger(control).isEnabled());
    choose(Access::playback(control), 3);
    choose(Access::wave(control), 3);
    Access::tick(control);
    CHECK(Access::seed(control).isEnabled());
    CHECK(Access::trigger(control).isEnabled());
    Access::seed(control).setValue(49152, juce::sendNotificationSync);
    Access::note(control).setValue(60, juce::sendNotificationSync);
    CHECK(read(processor, play::parameterID(1, play::playMode)) == play::oneShot);
    CHECK(read(processor, play::parameterID(1, play::waveMode)) == play::randomGlide);
    CHECK(read(processor, play::parameterID(1, play::seed)) == 49152);
    CHECK(read(processor, play::parameterID(1, play::triggerNote)) == 60);
    CHECK(read(processor, play::parameterID(0, play::playMode)) == play::loop);
    REQUIRE(processor.undoEdit());
    CHECK(read(processor, play::parameterID(1, play::triggerNote)) == -1);
    juce::Label* noteReadout = nullptr;
    for (auto* child : Access::note(control).getChildren())
        if (auto* label = dynamic_cast<juce::Label*>(child)) noteReadout = label;
    REQUIRE(noteReadout != nullptr);
    CHECK(noteReadout->getText() == "Any");
    auto* accessibility = Access::playback(control).getAccessibilityHandler(); REQUIRE(accessibility);
    CHECK(accessibility->getHelp().contains("full cycle"));
    CHECK(Access::wave(control).getTooltip().contains("Shape and Smooth only affect Shape output"));
}

TEST_CASE("Stale playback popups and numeric edits cannot affect a recycled LFO slot", "[performance-ui][ui][lifecycle]")
{
    FireAudioProcessor processor;
    fire::ui::LfoPlaybackControls control(processor, 0); show(control);
    choose(Access::wave(control), 2); Access::tick(control);
    auto pending = Access::choice(Access::playback(control));
    REQUIRE(processor.removeLfo(0));
    REQUIRE(processor.addLfo() == 0);
    pending(3);
    Access::seed(control).setValue(442, juce::sendNotificationSync);
    CHECK(read(processor, play::parameterID(0, play::playMode)) == play::loop);
    CHECK(read(processor, play::parameterID(0, play::seed)) == 0);
    Access::tick(control);
    CHECK_FALSE(Access::seed(control).isEnabled());
    auto second = std::make_unique<fire::ui::LfoPlaybackControls>(processor, 1); show(*second);
    pending = Access::choice(Access::playback(*second));
    second->setVisible(false); second->setVisible(true); pending(3);
    CHECK(read(processor, play::parameterID(1, play::playMode)) == play::loop);
    pending = Access::choice(Access::playback(*second)); second.reset(); pending(3);
    CHECK(read(processor, play::parameterID(1, play::playMode)) == play::loop);
}

TEST_CASE("MIDI controls learn CC and channel as one edit and cancel abandoned sessions", "[performance-ui][midi][ui][undo]")
{
    FireAudioProcessor processor; processor.prepareToPlay(48000, 64);
    fire::ui::MidiControls control(processor); show(control);
    Access::row(control, 2).learn.onClick();
    auto token = Access::token(control); REQUIRE(token != 0); CHECK(processor.isMidiLearnActive(token));
    processCC(processor, 3, 74, 96); Access::tick(control);
    CHECK(Access::token(control) == 0);
    CHECK(read(processor, midi::parameterID(2, midi::number)) == 74);
    CHECK(read(processor, midi::parameterID(2, midi::channel)) == 3);
    CHECK(read(processor, midi::parameterID(1, midi::number)) == -1);
    CHECK(Access::row(control, 2).learn.getButtonText() == "Learn");
    REQUIRE(processor.undoEdit());
    CHECK(read(processor, midi::parameterID(2, midi::number)) == -1);
    CHECK(read(processor, midi::parameterID(2, midi::channel)) == 0);
    // Restoring state invalidates the previous panel's pending interactions.
    Access::tick(control); CHECK_FALSE(Access::row(control, 2).learn.isEnabled());
    fire::ui::MidiControls current(processor); show(current);
    Access::row(current, 0).learn.onClick(); token = Access::token(current);
    current.setVisible(false); CHECK_FALSE(processor.isMidiLearnActive(token));
    current.setVisible(true); Access::row(current, 1).learn.onClick(); token = Access::token(current);
    Access::row(current, 1).number.setValue(1, juce::sendNotificationSync);
    CHECK_FALSE(processor.isMidiLearnActive(token));
    Access::row(current, 3).learn.onClick(); token = Access::token(current);
    const auto other = processor.beginMidiLearn(2); REQUIRE(other != token);
    Access::tick(current); CHECK(Access::token(current) == 0); CHECK(processor.isMidiLearnActive(other));
    processor.endMidiLearn(other); processor.releaseResources();
}

TEST_CASE("Performance dialogs keep the module rail pitch and close with their editing context", "[performance-ui][ui][lfo-bank][lifecycle]")
{
    FireAudioProcessor processor; LfoPanel panel(processor);
    panel.setSize(1000, 280); show(panel);
    const int pitch = panel.getBankRowPitch();
    Access::open(panel, false); auto playback = Access::dialog(panel, false); REQUIRE(playback);
    Access::open(panel, true); auto midi = Access::dialog(panel, true); REQUIRE(midi);
    Access::select(panel, 1);
    CHECK(Access::dialog(panel, false) == nullptr);
    CHECK(playback == nullptr);
    CHECK(Access::dialog(panel, true) != nullptr);
    CHECK(panel.getBankRowPitch() == pitch);
    panel.setVisible(false);
    CHECK(Access::dialog(panel, true) == nullptr);
    CHECK(midi == nullptr);
}

TEST_CASE("Performance dialogs release processor attachments before owner teardown and reopen after native close",
          "[performance-ui][ui][lifecycle][regression]")
{
    for (const bool midi : {false, true})
    {
        CAPTURE(midi);
        auto processor = std::make_unique<FireAudioProcessor>();
        auto panel = std::make_unique<LfoPanel>(*processor);
        panel->setSize(1000, 280); show(*panel);
        Access::open(*panel, midi);
        auto first = Access::dialog(*panel, midi);
        REQUIRE(first != nullptr);
        juce::Component::SafePointer<juce::Component> firstContent(first->getContentComponent());
        REQUIRE(firstContent != nullptr);
        first->closeButtonPressed();
        Access::open(*panel, midi);
        CHECK(first == nullptr);
        CHECK(firstContent == nullptr);

        auto replacement = Access::dialog(*panel, midi);
        REQUIRE(replacement != nullptr);
        CHECK(replacement->isShowing());
        juce::Component::SafePointer<juce::Component> content(replacement->getContentComponent());
        panel.reset();
        CHECK(replacement == nullptr);
        CHECK(content == nullptr);
        processor.reset();
        // Pending modal callbacks must not retain content with dead parameters.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    }
}

TEST_CASE("Playback and MIDI panels fit all skins and header buttons leave the bank unchanged", "[performance-ui][ui][skin][layout]")
{
    FireAudioProcessor processor;
    fire::ui::LfoPlaybackControls playback(processor, 0);
    fire::ui::MidiControls midi(processor);
    LfoPanel panel(processor); FireLookAndFeel look; panel.setLookAndFeel(&look);
    panel.setSize(1000, 280); show(panel);
    const auto initialViewport = panel.getBankViewport().getBounds();
    processor.treeState.getParameter(play::parameterID(0, play::waveMode))->setValueNotifyingHost(.5f);
    panel.animationTick();
    CHECK(Access::button(panel, false).getTooltip().contains("Random Hold"));
    for (const auto skin : fire::ui::skins)
    {
        look.setSkin(skin);
        for (juce::Component* control : std::array<juce::Component*, 3>{&playback, &midi, &panel})
        {
            fire::ui::setSkin(*control, skin); control->sendLookAndFeelChange();
            // These snapshot-only panels are not showing and their UI timers
            // are stopped. Apply the same refresh a live panel receives before
            // capturing parameter-dependent enabled states and help text.
            Access::tick(playback);
            Access::tick(midi);
            CHECK(Access::seed(playback).isEnabled());
            writePreview(*control, juce::String(fire::ui::skinName(skin)).toLowerCase()
                + (control == &playback ? "-playback" : control == &midi ? "-midi" : "-mod-forge"));
        }
        CHECK(panel.getBankViewport().getBounds() == initialViewport);
        CHECK_FALSE(Access::button(panel, false).getBounds().intersects(Access::button(panel, true).getBounds()));
        CHECK(panel.getLocalBounds().contains(Access::button(panel, false).getBounds()));
        for (auto* control : {static_cast<juce::Component*>(&playback), static_cast<juce::Component*>(&midi)})
            for (auto* child : control->getChildren()) CHECK(control->getLocalBounds().contains(child->getBounds()));
        for (int slot = 0; slot < 4; ++slot)
        {
            auto& row = Access::row(midi, slot);
            CHECK_FALSE(row.number.getBounds().intersects(row.channel.getBounds()));
            CHECK_FALSE(row.channel.getBounds().intersects(row.learn.getBounds()));
            CHECK_FALSE(row.learn.getBounds().intersects(row.meter));
            CHECK(row.learn.getWidth() >= 72);
        }
    }
    panel.setLookAndFeel(nullptr);
}
