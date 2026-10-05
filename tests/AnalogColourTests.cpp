#include <PluginEditor.h>
#include <GUI/HardwareColourPanel.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include "helpers/RepaintRecorder.h"

namespace
{
void set(FireAudioProcessor& processor, const juce::String& id, float value)
{auto* parameter = processor.treeState.getParameter(id); REQUIRE(parameter); parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
template <typename T, typename Predicate> T* find(juce::Component& root, Predicate predicate)
{
    if (auto* value = dynamic_cast<T*>(&root); value && predicate(*value)) return value;
    for (auto* child : root.getChildren()) if (auto* value = find<T>(*child, predicate)) return value;
    return nullptr;
}

void advanceHardwareFor(fire::ui::HardwareColourPanel& panel, int model, float drive,
                        float peak, float seconds, int framesPerSecond = 60)
{
    const auto frames = juce::jmax(1, juce::roundToInt(seconds * framesPerSecond));
    const auto dt = seconds / static_cast<float>(frames);
    for (int frame = 0; frame < frames; ++frame) panel.setState(model, drive, peak, dt);
}
float settledTubeBrightness(int model, float drive, float peak)
{
    fire::ui::HardwareColourPanel panel;
    advanceHardwareFor(panel, model, drive, peak, 2.0f);
    return panel.getFilamentBrightness();
}
void preview(juce::Component& component, const juce::String& name)
{
    if (const auto* directory = std::getenv("FIRE_HARDWARE_PREVIEW_DIR"))
    {
        juce::File target = juce::File(directory).getChildFile(name + ".png");
        target.getParentDirectory().createDirectory();
        auto output = target.createOutputStream(); REQUIRE(output);
        output->setPosition(0); output->truncate();
        juce::PNGImageFormat{}.writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), *output);
    }
}
}

TEST_CASE("Analog colour models retain distinct bounded dynamics and recover from malformed audio", "[analog-colour][dsp]")
{
    std::array<double, fire::analog::count> fingerprints {};
    for (int model = 0; model < fire::analog::count; ++model)
        for (double rate : {44100.0, 96000.0})
        {
            fire::analog::Stage engine; engine.prepare(rate); engine.setProfile(model + 12, rate);
            for (int sample = 0; sample < 8192; ++sample)
            {
                const auto input = .7f * std::sin(static_cast<float>(sample) * .127f) + .13f * std::sin(static_cast<float>(sample) * .37f);
                const auto value = engine.process(input);
                REQUIRE(std::isfinite(value)); REQUIRE(std::abs(value) < 3);
                if (rate == 44100) fingerprints[static_cast<size_t>(model)] += value * (sample % 17 + 1);
            }
            for (auto bad : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::max()})
                CHECK(std::isfinite(engine.process(bad)));
            engine.reset(); CHECK(engine.process(0) == 0);
        }
    for (size_t a = 0; a < fingerprints.size(); ++a)
        for (size_t b = a + 1; b < fingerprints.size(); ++b) CHECK(std::abs(fingerprints[a] - fingerprints[b]) > .001);
}

TEST_CASE("Analog modes preserve old automation anchors and round trip independent instances", "[analog-colour][state][preset][edit-history]")
{
    FireAudioProcessor processor;
    auto* legacy = processor.treeState.getParameter("mode1"); REQUIRE(legacy);
    CHECK(legacy->convertFrom0to1(1) == 11);
    REQUIRE(processor.setShapeMode(1, -1, 12));
    CHECK(processor.getShapeMode(1) == 12);
    REQUIRE(processor.undoEdit()); CHECK(processor.getShapeMode(1) == 3);
    REQUIRE(processor.redoEdit()); CHECK(processor.getShapeMode(1) == 12);
    const auto slot = processor.addInsertEffect(0, fire::effects::Type::shape); REQUIRE(slot == 0);
    REQUIRE(processor.setShapeMode(0, slot, 23));
    set(processor, fire::analog_params::driveID(0, slot), 42);
    REQUIRE(processor.assignLfoToTarget(fire::mod_sources::firstMacro, fire::analog_params::driveID(0, slot)) == LfoManager::AssignmentResult::changed);
    juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(processor, preset);
    FireAudioProcessor restored; REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK(restored.getShapeMode(1) == 12); CHECK(restored.getShapeMode(0, 0) == 23);
    juce::MemoryBlock host; processor.getStateInformation(host); restored.setStateInformation(host.getData(), static_cast<int>(host.getSize()));
    CHECK(restored.getShapeMode(0, 0) == 23);
    CHECK(restored.getModulationInfoForParameter(fire::analog_params::driveID(0, 0)).isModulated);
    preset.removeAttribute(fire::analog_params::bandID(0));
    CHECK_FALSE(state::loadStateFromXml(preset, restored));
}

TEST_CASE("Analog modes have sample partition independent audio and bounded switching", "[analog-colour][dsp][transition]")
{
    const auto render = [](int blockSize)
    {
        BandProcessor processor; processor.prepare({48000, 256, 2}, false);
        BandProcessingParameters p; p.isShapeEnabled = true; p.mode = 12; p.mixVal = 1;
        p.mixValProvider.baseValue = p.shapeMixValProvider.baseValue = 1;
        p.biasVal.range = {-1, 1}; p.recVal.range = {0, 1};
        juce::AudioBuffer<float> lfo, output(2, 4096), block(2, blockSize);
        for (int start = 0; start < 4096; start += blockSize)
        {
            const auto count = juce::jmin(blockSize, 4096 - start); block.setSize(2, count, false, false, true);
            p.mode = start < 2048 ? 12 : 23;
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < count; ++sample) block.setSample(channel, sample, .3f * std::sin((start + sample) * .097f + channel * .5f));
            processor.process(block, p, lfo);
            for (int channel = 0; channel < 2; ++channel) output.copyFrom(channel, start, block, channel, 0, count);
        }
        return output;
    };
    const auto expected = render(64), actual = render(256);
    float error = 0;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 4096; ++sample) error = juce::jmax(error, std::abs(expected.getSample(channel, sample) - actual.getSample(channel, sample)));
    CHECK(error < 1e-5f); CHECK(expected.getMagnitude(0, 4096) > .01f);
}

TEST_CASE("Analog pages show hardware and the tube exposure follows Drive", "[analog-colour][ui][hardware]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor; FireLookAndFeel look; BandPanel panel(processor, {}, {}, {}, {}, {});
    panel.setLookAndFeel(&look);
    set(processor, ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, 0), 1);
    set(processor, ParameterIDAndName::getIDString(DRIVE_ID, 0), 45);
    panel.setSize(984, 258); panel.addToDesktop(juce::ComponentPeer::windowIsTemporary); panel.setVisible(true);
    REQUIRE(processor.setShapeMode(1, -1, 12)); panel.setSwitch(1, true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* hardware = find<fire::ui::HardwareColourPanel>(panel, [](auto& display) {return display.isShowing();}); REQUIRE(hardware);
    for (int frame = 0; frame < 90; ++frame) hardware->setState(0, 0, 0);
    const auto dim = hardware->getFilamentBrightness();
    for (int frame = 0; frame < 90; ++frame) hardware->setState(0, 90, .3f);
    CHECK(hardware->getFilamentBrightness() > dim + .5f);
    preview(panel, "band-analog-tube");
    for (int frame = 0; frame < 60; ++frame) hardware->setState(11, 50, .2f);
    CHECK(hardware->getTransportPhase() > 0); preview(*hardware, "tape-transport");
    if (std::getenv("FIRE_HARDWARE_PREVIEW_DIR") != nullptr)
    {
        fire::ui::HardwareColourPanel detail;
        detail.setSize(500, 370);
        for (auto skin : {fire::ui::Skin::modern, fire::ui::Skin::vintage})
        {
            fire::ui::setSkin(detail, skin);
            const auto prefix = skin == fire::ui::Skin::vintage ? "vintage-" : "modern-";
            for (int model : {0, 6, 11})
            {
                for (int frame = 0; frame < 90; ++frame) detail.setState(model, 65, .3f);
                preview(detail, juce::String(prefix) + (model == 0 ? "tube" : model == 6 ? "diode" : "tape"));
            }
            advanceHardwareFor(detail, 0, 100, 0, 2.0f);
            preview(detail, juce::String(prefix) + "tube-silent");
            advanceHardwareFor(detail, 0, 5, juce::Decibels::decibelsToGain(-36.0f), 2.0f);
            preview(detail, juce::String(prefix) + "tube-soft");
            advanceHardwareFor(detail, 0, 5, juce::Decibels::decibelsToGain(-18.0f), 2.0f);
            preview(detail, juce::String(prefix) + "tube-loud");
        }
    }
    REQUIRE(processor.setShapeMode(1, -1, 3)); juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    CHECK_FALSE(hardware->isShowing());
}

TEST_CASE("Tube light needs audio and preserves quiet dynamics at low Drive",
          "[analog-colour][ui][hardware][filament][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int model = 0; model <= 5; ++model)
    {
        CAPTURE(model);
        fire::ui::HardwareColourPanel panel;
        CHECK(panel.getFilamentBrightness() == 0.0f);
        advanceHardwareFor(panel, model, 100, 0, 1.0f);
        CHECK(panel.getFilamentBrightness() == 0.0f);
        advanceHardwareFor(panel, model, 100, juce::Decibels::decibelsToGain(-90.0f), 1.0f);
        CHECK(panel.getFilamentBrightness() == 0.0f);

        const auto soft = settledTubeBrightness(model, 5, juce::Decibels::decibelsToGain(-36.0f));
        const auto loud = settledTubeBrightness(model, 5, juce::Decibels::decibelsToGain(-18.0f));
        CAPTURE(soft, loud);
        CHECK(soft > 0.20f);
        CHECK(soft < 0.26f);
        CHECK(loud > soft + 0.12f);
        CHECK(loud < 0.45f);

        float previous = -1.0f;
        for (const float drive : {0.0f, 5.0f, 25.0f, 50.0f, 100.0f})
        {
            const auto brightness = settledTubeBrightness(model, drive, juce::Decibels::decibelsToGain(-18.0f));
            CAPTURE(drive, brightness);
            CHECK(brightness > previous);
            CHECK(brightness <= 1.0f);
            previous = brightness;
        }
        CHECK(settledTubeBrightness(model, 0, 1) == Catch::Approx(0.45f).margin(1.0e-5f));
        CHECK(settledTubeBrightness(model, 100, 1) == Catch::Approx(1.0f).margin(1.0e-5f));
        CHECK(settledTubeBrightness(model, 150, 8) == Catch::Approx(1.0f).margin(1.0e-5f));
    }
}

TEST_CASE("A tube does not inherit the idle illumination of a circuit or tape panel",
          "[analog-colour][ui][hardware][filament][model-change]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const int previousModel : {6, 11})
    {
        CAPTURE(previousModel);
        fire::ui::HardwareColourPanel panel;
        advanceHardwareFor(panel, previousModel, 100, 0, 1.0f);
        REQUIRE(panel.getFilamentBrightness() > .7f);
        panel.setState(0, 100, 0);
        CHECK(panel.getFilamentBrightness() == 0.0f);
    }
}

TEST_CASE("Tube exposure uses a fast attack and a smooth release all the way to zero",
          "[analog-colour][ui][hardware][filament][envelope]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    fire::ui::HardwareColourPanel panel;
    advanceHardwareFor(panel, 0, 100, 1, 0.045f, 200);
    CHECK(panel.getFilamentBrightness() == Catch::Approx(0.63212056f).margin(2.0e-5f));
    advanceHardwareFor(panel, 0, 100, 1, 1.0f);
    const auto hot = panel.getFilamentBrightness();
    advanceHardwareFor(panel, 0, 100, 0, 0.18f, 50);
    CHECK(panel.getFilamentBrightness() == Catch::Approx(hot * 0.36787944f).margin(2.0e-5f));
    advanceHardwareFor(panel, 0, 100, 0, 2.0f);
    CHECK(panel.getFilamentBrightness() == 0.0f);
}

TEST_CASE("Tube envelopes follow elapsed time at 30 60 and 120 UI frames per second",
          "[analog-colour][ui][hardware][filament][frame-rate]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::array<std::array<float, 5>, 3> brightness {};
    const std::array frameRates {30, 60, 120};
    for (size_t index = 0; index < frameRates.size(); ++index)
    {
        fire::ui::HardwareColourPanel panel;
        const auto fps = frameRates[index];
        advanceHardwareFor(panel, 0, 5, juce::Decibels::decibelsToGain(-36.0f), .2f, fps);
        brightness[index][0] = panel.getFilamentBrightness();
        advanceHardwareFor(panel, 0, 5, juce::Decibels::decibelsToGain(-18.0f), .3f, fps);
        brightness[index][1] = panel.getFilamentBrightness();
        advanceHardwareFor(panel, 0, 80, juce::Decibels::decibelsToGain(-18.0f), .4f, fps);
        brightness[index][2] = panel.getFilamentBrightness();
        advanceHardwareFor(panel, 0, 80, 0, .6f, fps);
        brightness[index][3] = panel.getFilamentBrightness();
        advanceHardwareFor(panel, 0, 80, 0, 1.8f, fps);
        brightness[index][4] = panel.getFilamentBrightness();
        CHECK(brightness[index][4] == 0.0f);
    }
    for (size_t rate = 1; rate < frameRates.size(); ++rate)
        for (size_t point = 0; point < brightness[rate].size(); ++point)
        {
            CAPTURE(frameRates[rate], point);
            CHECK(brightness[rate][point] == Catch::Approx(brightness[0][point]).margin(2.0e-5f));
        }
}

TEST_CASE("The last extinguished tube frame repaints and matches a silent tube",
          "[analog-colour][ui][hardware][filament][repaint]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    fire::ui::HardwareColourPanel panel;
    panel.setSize(300, 225);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    REQUIRE(panel.isShowing());
    auto* recorder = new RepaintRecorder(panel);
    panel.setCachedComponentImage(recorder);
    for (const auto skin : {fire::ui::Skin::modern, fire::ui::Skin::vintage})
    {
        CAPTURE(static_cast<int>(skin));
        fire::ui::setSkin(panel, skin);
        advanceHardwareFor(panel, 0, 100, 1, 1.0f);
        REQUIRE(panel.getFilamentBrightness() > .99f);
        auto retained = renderRepaintTestComponent(panel);
        bool sawFinalRepaint = false;
        for (int frame = 0; frame < 180; ++frame)
        {
            recorder->clear();
            panel.setState(0, 100, 0, 1.0f / 60.0f);
            if (!recorder->dirtyAreas.isEmpty())
            {
                // The parent's backing surface is redrawn before a transparent
                // child; emulate that clearing before replaying the damage.
                for (const auto& area : recorder->dirtyAreas)
                    retained.clear(area, juce::Colours::transparentBlack);
                recorder->paintDirtyAreas(retained);
            }
            if (panel.getFilamentBrightness() == 0.0f)
            {
                CHECK_FALSE(recorder->dirtyAreas.isEmpty());
                sawFinalRepaint = !recorder->dirtyAreas.isEmpty();
                break;
            }
        }
        REQUIRE(sawFinalRepaint);
        CHECK(repaintTestImagesMatch(retained, renderRepaintTestComponent(panel)));
        fire::ui::HardwareColourPanel silent;
        silent.setSize(panel.getWidth(), panel.getHeight());
        fire::ui::setSkin(silent, skin);
        advanceHardwareFor(silent, 0, 100, 0, 1.0f);
        CHECK(repaintTestImagesMatch(retained, renderRepaintTestComponent(silent)));
    }
    panel.setCachedComponentImage(nullptr);
}

TEST_CASE("Tracked tube exposure rejects old peaks and releases after audio callbacks stop",
          "[analog-colour][ui][hardware][filament][freshness]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    fire::ui::HardwareColourPanel panel;
    advanceHardwareFor(panel, 0, 100, 1, 1.0f);
    REQUIRE(panel.getFilamentBrightness() > .99f);
    panel.setState(0, 100, 1, 1.0f / 60.0f, 40);
    CHECK(panel.getFilamentBrightness() == 0.0f);
    panel.setState(0, 100, 1, 1.0f / 60.0f, 40);
    CHECK(panel.getFilamentBrightness() == 0.0f);
    juce::Thread::sleep(20);
    panel.setState(0, 100, 1, 1.0f / 60.0f, 41);
    const auto active = panel.getFilamentBrightness();
    REQUIRE(active > .05f);
    // Deliberately pass a tiny nominal dt: tracked activity must use the real
    // elapsed 240 ms, both for freshness and for the release envelope.
    juce::Thread::sleep(240);
    panel.setState(0, 100, 1, 1.0f / 1000.0f, 41);
    CHECK(panel.getFilamentBrightness() <= active * .4f);
    CHECK(panel.getFilamentBrightness() >= 0.0f);
}

TEST_CASE("Hiding hardware clears its light and freshness without restarting tape reels",
          "[analog-colour][ui][hardware][filament][visibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    fire::ui::HardwareColourPanel panel;
    panel.setSize(300, 225);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    advanceHardwareFor(panel, 11, 70, .5f, 1.0f);
    const auto left = panel.getTransportPhase(0);
    const auto right = panel.getTransportPhase(1);
    REQUIRE(left > 0.0f);
    REQUIRE(panel.getFilamentBrightness() > 0.0f);
    panel.setVisible(false);
    CHECK(panel.getFilamentBrightness() == 0.0f);
    CHECK(panel.getTransportPhase(0) == left);
    CHECK(panel.getTransportPhase(1) == right);
    panel.setVisible(true);
    panel.setState(0, 100, 1, 1.0f / 60.0f, 99);
    CHECK(panel.getFilamentBrightness() == 0.0f);
    CHECK(panel.getTransportPhase(0) == left);
    CHECK(panel.getTransportPhase(1) == right);
}

TEST_CASE("Empty audio callbacks cannot keep hardware activity fresh",
          "[analog-colour][hardware][processor][freshness]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.prepareToPlay(48000, 64);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio(2, 64);
    audio.clear();
    const auto before = processor.getAudioActivitySequence();
    processor.processBlock(audio, midi);
    const auto after = processor.getAudioActivitySequence();
    CHECK(after == before + 1);
    juce::AudioBuffer<float> noSamples(2, 0), noChannels(0, 64);
    for (int callback = 0; callback < 4; ++callback)
    {
        processor.processBlock(noSamples, midi);
        processor.processBlock(noChannels, midi);
        CHECK(processor.getAudioActivitySequence() == after);
    }
    processor.processBlock(audio, midi);
    CHECK(processor.getAudioActivitySequence() == after + 1);
    processor.releaseResources();
}

TEST_CASE("Tape reels turn continuously across full rotations and transport changes", "[analog-colour][ui][hardware][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    fire::ui::HardwareColourPanel panel;
    constexpr auto twoPi = juce::MathConstants<float>::twoPi;
    std::array<float, 2> previous {panel.getTransportPhase(0), panel.getTransportPhase(1)};
    std::array<double, 2> distance {};
    std::array<int, 2> fullRotations {};
    for (int frame = 0; frame < 3600; ++frame)
    {
        // Vary the UI frame rate and Drive, pause, then resume the transport.
        const float dt = frame % 3 == 0 ? 1.0f / 30.0f : 1.0f / 60.0f;
        const float drive = frame < 900 ? 20.0f : frame < 1800 ? 95.0f : 50.0f;
        const float peak = frame >= 1800 && frame < 2100 ? 0.0f : .2f;
        // Changing only the skin must not restart or re-phase either reel.
        if (frame % 180 == 0)
            fire::ui::setSkin(panel, frame % 360 == 0 ? fire::ui::Skin::vintage : fire::ui::Skin::modern);
        panel.setState(11, drive, peak, dt);
        for (int reel = 0; reel < 2; ++reel)
        {
            const auto index = static_cast<size_t>(reel);
            const auto angle = panel.getTransportPhase(reel);
            const auto step = std::remainder(angle - previous[index], twoPi);
            CAPTURE(frame, reel, previous[index], angle);
            REQUIRE(std::isfinite(angle));
            // Forward motion must remain within one frame's maximum travel,
            // including when either reel's visible orientation wraps to zero.
            REQUIRE(step >= -1.0e-6f);
            REQUIRE(step <= 2.0f * 1.075f * dt + 1.0e-5f);
            if (angle < previous[index]) ++fullRotations[index];
            distance[index] += step;
            previous[index] = angle;
        }
    }
    for (int turns : fullRotations) CHECK(turns > 10);
    CHECK(distance[1] / distance[0] == Catch::Approx(1.075).margin(1.0e-4));
}


TEST_CASE("Line hardware skins preserve responsive filaments and continuous tape transport",
          "[skin][line-skin][analog-colour][ui][hardware][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    fire::ui::HardwareColourPanel panel;
    panel.setSize(340,260); panel.setVisible(true);
    for (const auto skin : {fire::ui::Skin::paper,fire::ui::Skin::ink})
    {
        fire::ui::setSkin(panel,skin);
        advanceHardwareFor(panel,0,35,0,2);
        const auto silent=panel.createComponentSnapshot(panel.getLocalBounds());
        CHECK(panel.getFilamentBrightness()==0);
        advanceHardwareFor(panel,0,35,.15f,1);
        CHECK(panel.getFilamentBrightness()>.20f);
        const auto active=panel.createComponentSnapshot(panel.getLocalBounds());
        int changed=0;
        for(int y=0;y<active.getHeight();++y)
            for(int x=0;x<active.getWidth();++x)
                changed+=silent.getPixelAt(x,y)!=active.getPixelAt(x,y);
        CHECK(changed>100);
        preview(panel,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-tube");
        const auto phase=panel.getTransportPhase(1);
        advanceHardwareFor(panel,11,30,.2f,.5f);
        CHECK(std::remainder(panel.getTransportPhase(1)-phase,juce::MathConstants<float>::twoPi)>0);
        preview(panel,juce::String(fire::ui::skinName(skin)).toLowerCase()+"-tape");
    }
}
