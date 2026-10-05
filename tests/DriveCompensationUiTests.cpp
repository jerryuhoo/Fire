#include <PluginEditor.h>
#include <Utility/DriveCompensationParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

struct DriveCompensationUiTestAccess
{
    static juce::Rectangle<int> driveCard(const BandPanel& panel) { return panel.knobsAreaRect; }
    static juce::Rectangle<int> outputCard(const BandPanel& panel) { return panel.outputAreaRect; }
};

namespace
{
template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate predicate)
{
    if (auto* result = dynamic_cast<T*>(&root); result && predicate(*result)) return result;
    for (auto* child : root.getChildren()) if (auto* result = find<T>(*child, predicate)) return result;
    return nullptr;
}

template <typename T>
T& byID(juce::Component& root, const juce::String& id)
{
    auto* result = find<T>(root, [&](auto& component) { return component.getComponentID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}

juce::Button& named(juce::Component& root, const juce::String& text)
{
    auto* result = find<juce::Button>(root, [&](auto& button) { return button.getButtonText() == text; });
    REQUIRE(result != nullptr);
    return *result;
}

void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(FireAudioProcessor& processor, const juce::String& id)
{
    auto* parameter = processor.treeState.getRawParameterValue(id);
    REQUIRE(parameter != nullptr);
    return parameter->load();
}

BandPanel& bandPanel(FireAudioProcessorEditor& editor)
{
    auto* panel = find<BandPanel>(editor, [](auto&) { return true; });
    REQUIRE(panel != nullptr);
    return *panel;
}
ModulatableSlider& output(BandPanel& panel, int band)
{
    const auto id = ParameterIDAndName::getIDString(OUTPUT_ID, band);
    auto* result = find<ModulatableSlider>(panel, [&](auto& slider) { return slider.getParamID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}
void show(FireAudioProcessorEditor& editor)
{
    editor.stopTimer();
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    REQUIRE(editor.isShowing());
}
void tick(BandPanel& panel) { panel.animationTick(1.0f / 60.0f); }

juce::String dbText(float value, bool estimated = false)
{
    if (std::abs(value) < 0.05f) value = 0;
    return (estimated ? juce::String::charToString(0x2248) + " " : juce::String())
        + (value < 0 ? juce::String::charToString(0x2212) : juce::String())
        + juce::String(std::abs(value), 1) + " dB";
}

juce::MouseEvent mouse(juce::Component& component, juce::ModifierKeys modifiers)
{
    const auto now = juce::Time::getCurrentTime();
    const auto point = component.getLocalBounds().toFloat().getCentre();
    return {juce::Desktop::getInstance().getMainMouseSource(), point, modifiers,
            0, 0, 0, 0, 0, &component, &component, now, point, now, 1, false};
}

struct HostGestures final : juce::AudioProcessorListener
{
    explicit HostGestures(FireAudioProcessor& p) : processor(p) { processor.addListener(this); }
    ~HostGestures() override { processor.removeListener(this); }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int index) override { events.push_back({index, true}); }
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int index) override { events.push_back({index, false}); }
    std::vector<bool> forParameter(const juce::RangedAudioParameter& parameter) const
    {
        std::vector<bool> result;
        for (const auto& event : events) if (event.first == parameter.getParameterIndex()) result.push_back(event.second);
        return result;
    }
    FireAudioProcessor& processor;
    std::vector<std::pair<int, bool>> events;
};

void audio(FireAudioProcessor& processor, int blocks = 80)
{
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 0.002f, buffer.getNumSamples());
        processor.processBlock(buffer, midi);
    }
}

void snapshot(FireAudioProcessorEditor& editor, const juce::String& name)
{
    const auto* directory = std::getenv("FIRE_DRIVE_COMP_UI_SNAPSHOT_DIR");
    if (! directory || ! *directory) return;
    const auto file = juce::File(juce::String::fromUTF8(directory)).getChildFile(name + ".png");
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream));
}
}

TEST_CASE("Gain Comp belongs to Drive and keeps Output independently editable",
          "[drive-comp-ui][ui][binding][visibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, "drive1", 60);
    set(processor, "output1", -3.4f);
    FireAudioProcessorEditor editor(processor);
    show(editor);
    auto& panel = bandPanel(editor);
    auto& comp = byID<juce::Button>(panel, "linked1");
    auto& readout = byID<juce::Label>(panel, "driveCompensationReadout");
    auto& upgrade = byID<juce::Button>(panel, "driveCompUpgrade");
    tick(panel);
    CHECK(comp.getButtonText() == "Gain Comp");
    CHECK(comp.getTitle().containsIgnoreCase("Drive volume compensation"));
    CHECK(comp.isShowing());
    CHECK(comp.getToggleState());
    CHECK_FALSE(upgrade.isShowing());
    CHECK(readout.getText() == dbText(-6.0f, true));
    CHECK(readout.getTooltip().containsIgnoreCase("Estimated"));
    CHECK(output(panel, 0).getValue() == Catch::Approx(-3.4f));
    comp.triggerClick();
    tick(panel);
    CHECK(get(processor, "linked1") == 0.0f);
    CHECK(readout.getText() == "Off");
    CHECK(get(processor, "output1") == Catch::Approx(-3.4f));
    comp.triggerClick();
    set(processor, "extreme1", 1);
    tick(panel);
    CHECK(readout.getText() == dbText(-6.0f * std::log2(10.0f), true));
    panel.driveBypassButton.triggerClick();
    tick(panel);
    CHECK(readout.getText() == "Bypassed");
    CHECK(comp.isEnabled());
    panel.driveBypassButton.triggerClick();
    for (const auto* module : {"Shape", "Compressor", "Stereo", "OTT"})
    {
        named(panel, module).triggerClick();
        tick(panel);
        CHECK_FALSE(comp.isShowing());
        CHECK_FALSE(readout.isShowing());
        CHECK_FALSE(upgrade.isShowing());
        CHECK(named(panel, "Safe").isShowing());
        CHECK(named(panel, "Extreme").isShowing());
    }
    named(panel, "Drive").triggerClick();
    tick(panel);
    CHECK(comp.isShowing());
    output(panel, 0).setValue(-1.7, juce::sendNotificationSync);
    set(processor, "drive1", 80);
    tick(panel);
    CHECK(get(processor, "output1") == Catch::Approx(-1.7f));
}

TEST_CASE("Opening and rebinding legacy Link is passive until the explicit Drive Comp upgrade",
          "[drive-comp-ui][ui][legacy][upgrade][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, NUM_BANDS_ID, 2);
    set(processor, "lineState1", 1);
    std::array<float, 2> storedOutputs {};
    for (int band = 0; band < 2; ++band)
    {
        set(processor, fire::drive_comp::parameterID(band), 0);
        set(processor, ParameterIDAndName::getIDString(LINKED_ID, band), static_cast<float>(band));
        set(processor, ParameterIDAndName::getIDString(DRIVE_ID, band), 40.0f + 20.0f * band);
        set(processor, ParameterIDAndName::getIDString(OUTPUT_ID, band), -4.5f + band);
        // Preserve the exact APVTS value after its normalised round trip.
        storedOutputs[static_cast<size_t>(band)] = get(processor, ParameterIDAndName::getIDString(OUTPUT_ID, band));
    }
    FireAudioProcessorEditor editor(processor);
    show(editor);
    auto& panel = bandPanel(editor);
    for (int band : {0, 1, 0})
    {
        panel.setFocusBandNum(band);
        tick(panel);
        CHECK(get(processor, fire::drive_comp::parameterID(band)) == 0.0f);
        CHECK(get(processor, ParameterIDAndName::getIDString(LINKED_ID, band)) == static_cast<float>(band));
        CHECK(get(processor, ParameterIDAndName::getIDString(DRIVE_ID, band)) == 40.0f + 20.0f * band);
        CHECK(get(processor, ParameterIDAndName::getIDString(OUTPUT_ID, band)) == storedOutputs[static_cast<size_t>(band)]);
        CHECK(byID<juce::Button>(panel, ParameterIDAndName::getIDString(LINKED_ID, band)).getButtonText() == "Legacy Link");
        CHECK(byID<juce::Label>(panel, "driveCompensationReadout").getText() == (band == 0 ? "Manual Output" : "Output linked"));
    }
    auto& upgrade = byID<juce::Button>(panel, "driveCompUpgrade");
    CHECK(upgrade.isShowing());
    CHECK(upgrade.getTooltip().containsIgnoreCase("change the sound"));
    HostGestures gestures(processor);
    upgrade.triggerClick();
    tick(panel);
    CHECK(get(processor, fire::drive_comp::parameterID(0)) == 1.0f);
    CHECK(get(processor, "linked1") == 1.0f);
    CHECK(get(processor, "output1") == storedOutputs[0]);
    CHECK(get(processor, fire::drive_comp::parameterID(1)) == 0.0f);
    CHECK_FALSE(upgrade.isShowing());
    CHECK(byID<juce::Button>(panel, "linked1").getButtonText() == "Gain Comp");
    CHECK(gestures.forParameter(*processor.treeState.getParameter(fire::drive_comp::parameterID(0))) == std::vector<bool> {true, false});
    CHECK(gestures.forParameter(*processor.treeState.getParameter("linked1")) == std::vector<bool> {true, false});
    CHECK(gestures.forParameter(*processor.treeState.getParameter("output1")).empty());
}

TEST_CASE("A held legacy upgrade cannot cross band page or visibility sessions",
          "[drive-comp-ui][ui][input][lifecycle][upgrade]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, NUM_BANDS_ID, 2);
    set(processor, "lineState1", 1);
    for (int band = 0; band < 2; ++band) set(processor, fire::drive_comp::parameterID(band), 0);
    FireAudioProcessorEditor editor(processor);
    show(editor);
    auto& panel = bandPanel(editor);
    auto& upgrade = byID<juce::Button>(panel, "driveCompUpgrade");
    auto& component = static_cast<juce::Component&>(upgrade);
    component.mouseDown(mouse(component, juce::ModifierKeys::leftButtonModifier));
    SECTION("band rebind") { panel.setFocusBandNum(1); }
    SECTION("hidden editor ABA") { editor.setVisible(false); editor.setVisible(true); }
    SECTION("another module") { named(panel, "Shape").triggerClick(); }
    component.mouseUp(mouse(component, {}));
    CHECK(get(processor, fire::drive_comp::parameterID(0)) == 0.0f);
    CHECK(get(processor, fire::drive_comp::parameterID(1)) == 0.0f);
    // A hidden programmatic trigger also cannot upgrade a different page.
    named(panel, "Shape").triggerClick();
    upgrade.triggerClick();
    CHECK(get(processor, fire::drive_comp::parameterID(0)) == 0.0f);
    CHECK(get(processor, fire::drive_comp::parameterID(1)) == 0.0f);
}

TEST_CASE("Gain Comp readout uses real modulated audio then explicitly estimates when callbacks stop",
          "[drive-comp-ui][ui][audio][telemetry]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, "drive1", 60); set(processor, "linked1", 1);
    set(processor, "safe1", 0); set(processor, "extreme1", 0);
    set(processor, "driveBypass1", 1); set(processor, "shapeBypass1", 0);
    set(processor, "output1", -2.4f);
    LfoData high;
    high.points = {{0, 1}, {1, 1}};
    high.curvatures = {0};
    processor.getLfoManager().setLfoData(0, high);
    REQUIRE(processor.assignLfoToTarget(0, "drive1") == LfoManager::AssignmentResult::changed);
    processor.setModulationDepth("drive1", 0.4f);
    processor.prepareToPlay(48000, 128);
    FireAudioProcessorEditor editor(processor);
    show(editor);
    auto& panel = bandPanel(editor);
    auto& readout = byID<juce::Label>(panel, "driveCompensationReadout");
    audio(processor);
    tick(panel);
    REQUIRE(processor.getBandDriveCompensationSequence(0) > 0);
    const auto measured = processor.getBandDriveCompensationDb(0);
    REQUIRE(std::isfinite(measured));
    CHECK(measured < -6.1f);
    CHECK(readout.getText() == dbText(measured));
    CHECK(readout.getTooltip().containsIgnoreCase("Applied"));
    CHECK(get(processor, "output1") == Catch::Approx(-2.4f));
    const auto sequence = processor.getBandDriveCompensationSequence(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(280));
    tick(panel);
    CHECK(processor.getBandDriveCompensationSequence(0) == sequence);
    CHECK(readout.getText() == dbText(-6.0f, true));
    CHECK(readout.getTooltip().containsIgnoreCase("Estimated"));
    audio(processor, 1);
    tick(panel);
    CHECK(readout.getText() == dbText(processor.getBandDriveCompensationDb(0)));
    processor.clearModulationForParameter("drive1");
    set(processor, "drive1", 0);
    audio(processor);
    tick(panel);
    CHECK(processor.getBandDriveCompensationDb(0) == Catch::Approx(0.0f).margin(0.01f));
    CHECK(readout.getText() == "0.0 dB");
    processor.releaseResources();
}

TEST_CASE("Drive compensation footer separates legacy actions from the dial and centres Safe Extreme",
          "[drive-comp-ui][ui][layout][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, "drive1", 60);
    FireAudioProcessorEditor editor(processor);
    show(editor);
    auto& panel = bandPanel(editor);
    for (bool modern : {true, false})
    {
        set(processor, fire::drive_comp::parameterID(0), modern ? 1.0f : 0.0f);
        tick(panel);
        for (int width : {1000, 1400, 2000})
        {
            CAPTURE(modern, width);
            editor.setSize(width, width / 2);
            tick(panel);
            const auto scale = static_cast<float>(width) / 1000.0f;
            const auto card = DriveCompensationUiTestAccess::driveCard(panel);
            auto* hero = panel.getDriveKnob();
            CHECK(hero->getWidth() == hero->getHeight());
            CHECK(hero->getWidth() > fire::ui::ordinaryKnobWidth(scale));
            auto& comp = byID<juce::Button>(panel, "linked1");
            auto& readout = byID<juce::Label>(panel, "driveCompensationReadout");
            auto& upgrade = byID<juce::Button>(panel, "driveCompUpgrade");
            CHECK(card.contains(hero->getBounds()));
            CHECK(card.contains(comp.getBounds()));
            CHECK(card.contains(readout.getBounds()));
            CHECK_FALSE(hero->getBounds().intersects(comp.getBounds()));
            CHECK_FALSE(hero->getBounds().intersects(readout.getBounds()));
            CHECK_FALSE(comp.getBounds().intersects(readout.getBounds()));
            CHECK(upgrade.isShowing() == ! modern);
            CHECK(readout.getY() >= hero->getBottom());
            CHECK(comp.getY() >= readout.getBottom());
            if (! modern)
            {
                CHECK(card.contains(upgrade.getBounds()));
                CHECK_FALSE(hero->getBounds().intersects(upgrade.getBounds()));
                CHECK_FALSE(readout.getBounds().intersects(upgrade.getBounds()));
                CHECK_FALSE(comp.getBounds().intersects(upgrade.getBounds()));
                CHECK(comp.getY() == upgrade.getY());
                CHECK(comp.getHeight() == upgrade.getHeight());
                CHECK(upgrade.getX() - comp.getRight() >= juce::roundToInt(8 * scale));
            }
            auto& safe = named(panel, "Safe");
            auto& extreme = named(panel, "Extreme");
            const auto outputCard = DriveCompensationUiTestAccess::outputCard(panel);
            CHECK(std::abs(safe.getBounds().getUnion(extreme.getBounds()).getCentreX() - outputCard.getCentreX()) <= 1);
            CHECK_FALSE(safe.getBounds().intersects(extreme.getBounds()));
            snapshot(editor, juce::String(modern ? "modern-" : "legacy-") + "drive-comp-" + juce::String(width));
        }
    }
}
