#include <PluginEditor.h>
#include <GUI/HardwareColourPanel.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

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
        }
    }
    REQUIRE(processor.setShapeMode(1, -1, 3)); juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    CHECK_FALSE(hardware->isShowing());
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
