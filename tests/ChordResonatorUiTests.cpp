#include <PluginEditor.h>
#include <GUI/InsertEffectControls.h>
#include <DSP/ChordResonator.h>
#include <Utility/ResonatorParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

struct ChordResonatorUiTestAccess
{
    static auto result(ContextAwareComboBox& menu) { return menu.createPopupResultHandler(); }
};

namespace
{
using Type = fire::effects::Type;
using Rack = fire::ui::EffectRackNavigation;

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

ModulatableSlider& knob(juce::Component& root, const juce::String& id)
{
    auto* result = find<ModulatableSlider>(root, [&](auto& slider) { return slider.getParamID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}

void show(juce::Component& component, int width, int height)
{
    component.setSize(width, height);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    REQUIRE(component.isShowing());
}

void click(juce::Component& root, const juce::String& text)
{
    auto* button = find<juce::Button>(root, [&](auto& b) { return b.getButtonText() == text; });
    REQUIRE(button != nullptr);
    button->triggerClick();
}

void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void waitFor(const std::function<bool()>& condition)
{
    const auto start = juce::Time::getMillisecondCounter();
    while (! condition() && juce::Time::getMillisecondCounter() - start < 500u)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    REQUIRE(condition());
}

struct GestureRecorder final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override
    {
        events.push_back(starting);
        auto callback = onGesture;
        if (callback) callback(starting);
    }
    std::vector<bool> events;
    std::function<void(bool)> onGesture;
};

// Observe the same forwarding path as a host. JUCE iterates a parameter's
// raw listener array by index; removing UI attachments from inside that raw
// callback can revisit the observing listener after its array index shifts.
struct HostGestureRecorder final : juce::AudioProcessorListener
{
    HostGestureRecorder(FireAudioProcessor& owner, int index) : processor(owner), parameterIndex(index)
    { processor.addListener(this); }
    ~HostGestureRecorder() override { processor.removeListener(this); }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int index) override { record(index, true); }
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int index) override { record(index, false); }
    void record(int index, bool starting)
    {
        if (index != parameterIndex) return;
        events.push_back(starting);
        auto callback = onGesture;
        if (callback) callback(starting);
    }
    FireAudioProcessor& processor;
    int parameterIndex;
    std::vector<bool> events;
    std::function<void(bool)> onGesture;
};

struct SelectorFixture
{
    FireAudioProcessor processor;
    FireLookAndFeel theme;
    std::array<ModulatableSlider, 6> knobs;
    std::array<ModulatableSlider, 3> extras;
    juce::Component owner;
    std::unique_ptr<InsertEffectControls> controls;

    SelectorFixture()
    {
        processor.hasUpdateCheckBeenPerformed = true;
        REQUIRE(processor.addInsertEffect(1, Type::chordResonator) == 0);
        REQUIRE(processor.addInsertEffect(2, Type::chordResonator) == 0);
        REQUIRE(processor.addInsertEffect(1, Type::granular) == 1);
        controls = std::make_unique<InsertEffectControls>(processor);
        controls->setLookAndFeel(&theme);
        controls->setControls({&knobs[0], &knobs[1], &knobs[2], &knobs[3], &knobs[4], &knobs[5]});
        controls->setCloudsControls({&extras[0], &extras[1], &extras[2]});
        owner.addAndMakeVisible(*controls);
        controls->setBounds(0, 0, 320, 220);
        controls->bind(1, 0);
        controls->setActive(true);
        show(owner, 340, 240);
    }
    ContextAwareComboBox& root()
    {
        auto* menu = find<ContextAwareComboBox>(*controls, [](auto& c) { return c.getTitle().endsWith("root note"); });
        REQUIRE(menu != nullptr);
        return *menu;
    }
    ContextAwareComboBox& chord()
    {
        auto* menu = find<ContextAwareComboBox>(*controls, [](auto& c) { return c.getTitle() == "Chord Resonator chord"; });
        REQUIRE(menu != nullptr);
        return *menu;
    }
    juce::RangedAudioParameter& parameter(int index, int scope = 1)
    {
        auto* result = processor.treeState.getParameter(fire::effects::parameterID(scope, 0, index));
        REQUIRE(result != nullptr);
        return *result;
    }
};

void snapshot(FireAudioProcessorEditor& editor, const juce::String& name)
{
    const auto* directory = std::getenv("FIRE_CHORD_RESONATOR_UI_SNAPSHOT_DIR");
    if (! directory || ! *directory) return;
    auto* rack = find<Rack>(editor, [](auto& r) { return r.isShowing(); });
    REQUIRE(rack != nullptr);
    rack->setSelectedSlot(0);
    editor.startTimerHz(60);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(500);
    editor.stopTimer();
    const auto file = juce::File(juce::String::fromUTF8(directory)).getChildFile(name + ".png");
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream));
}
}

TEST_CASE("Chord Resonator menus add independent instances in Master and all four bands",
          "[chord-resonator-ui][insertfx][ui][menu]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    CHECK(static_cast<int>(Type::chordResonator) == 8);
    for (int scope = 0; scope <= 4; ++scope)
    {
        CAPTURE(scope);
        Rack rack(processor, scope);
        rack.onSelectEffect = [&](int slot) { rack.setSelectedSlot(slot); };
        show(rack, 190, 310);
        const auto menu = rack.createAddMenu();
        int entries = 0;
        for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
            if (it.getItem().itemID == static_cast<int>(Type::chordResonator))
            {
                ++entries;
                CHECK(it.getItem().text == "Chord Resonator");
                CHECK(it.getItem().isEnabled);
            }
        REQUIRE(entries == 1);
        for (int slot = 0; slot < 2; ++slot)
        {
            rack.createAddMenuResultHandler()(static_cast<int>(Type::chordResonator));
            CHECK(processor.getInsertEffectType(scope, slot) == Type::chordResonator);
            auto& row = byID<juce::TextButton>(rack, fire::effects::parameterID(scope, slot, fire::effects::typeField));
            CHECK(row.getButtonText() == "Resonator");
            CHECK(row.getTitle() == "Chord Resonator");
            CHECK(row.getTooltip().contains("Chord Resonator"));
            CHECK(processor.treeState.getRawParameterValue(fire::resonator_params::parameterID(scope, slot))->load() == 1.0f);
            CHECK(processor.treeState.getRawParameterValue(fire::effects::parameterID(scope, slot, fire::effects::typeField))->load() == 0.0f);
            CHECK(processor.treeState.getRawParameterValue(fire::modulation_fx::parameterID(scope, slot))->load() == 0.0f);
        }
    }
}

TEST_CASE("Chord pages use musical selectors and four standard LFO capable knobs without replacing the waveform",
          "[chord-resonator-ui][insertfx][ui][layout][lfo][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, NUM_BANDS_ID, 4);
    for (int line = 0; line < 3; ++line) set(processor, ParameterIDAndName::getIDString(LINE_STATE_ID, line), 1);
    for (int scope = 0; scope <= 4; ++scope)
    {
        REQUIRE(processor.addInsertEffect(scope, Type::chordResonator) == 0);
        REQUIRE(processor.addInsertEffect(scope, Type::granular) == 1);
    }
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    show(editor, 1000, 500);
    auto* band = find<BandPanel>(editor, [](auto&) { return true; });
    auto* master = find<GlobalPanel>(editor, [](auto&) { return true; });
    REQUIRE(band != nullptr);
    REQUIRE(master != nullptr);
    for (int scope = 0; scope <= 4; ++scope)
    {
        CAPTURE(scope);
        click(editor, scope == 0 ? "MASTER LAB" : "BAND LAB");
        if (scope > 0) band->setFocusBandNum(scope - 1);
        auto& panel = scope == 0 ? static_cast<PanelBase&>(*master) : static_cast<PanelBase&>(*band);
        auto* controls = find<InsertEffectControls>(panel, [](auto&) { return true; });
        REQUIRE(controls != nullptr);
        byID<juce::Button>(panel, fire::effects::parameterID(scope, 1, fire::effects::typeField)).triggerClick();
        auto& spread = knob(*controls, fire::clouds_params::parameterID(scope, 1, fire::clouds_params::spreadField));
        auto& freeze = byID<juce::Button>(*controls, fire::clouds_params::parameterID(scope, 1, fire::clouds_params::freezeField));
        REQUIRE(spread.isShowing());
        byID<juce::Button>(panel, fire::effects::parameterID(scope, 0, fire::effects::typeField)).triggerClick();
        CHECK_FALSE(controls->usesFullWidthLayout());
        CHECK_FALSE(spread.isShowing());
        CHECK(spread.getParamID().isEmpty());
        CHECK_FALSE(freeze.isShowing());
        auto& root = byID<ContextAwareComboBox>(*controls, fire::effects::parameterID(scope, 0, 0));
        auto& chord = byID<ContextAwareComboBox>(*controls, fire::effects::parameterID(scope, 0, 1));
        CHECK(root.getNumItems() == 37);
        CHECK(root.getItemText(0) == "C2");
        CHECK(root.getItemText(36) == "C5");
        CHECK(root.getText() == "C3");
        CHECK(chord.getText() == "Minor 7");
        for (int index = 0; index < 8; ++index)
            CHECK(chord.getItemText(index) == fire::chord_resonator::chordNames[static_cast<size_t>(index)]);
        for (auto* menu : {&root, &chord})
        {
            CHECK(menu->isShowing());
            auto* accessibility = menu->getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK(accessibility->getRole() == juce::AccessibilityRole::comboBox);
            CHECK_FALSE(accessibility->getTitle().isEmpty());
            CHECK_FALSE(accessibility->getHelp().isEmpty());
        }
        auto* notes = find<juce::Label>(*controls, [](auto& label) { return label.getTitle() == "Selected chord notes"; });
        REQUIRE(notes != nullptr);
        CHECK(notes->getText() == "C3  D#3\nG3  A#3");
        std::vector<juce::Component*> shown {&root, &chord, notes};
        const std::array<const char*, 4> names {"Color", "Decay", "Width", "Mix"};
        for (int control = 0; control < 6; ++control)
        {
            const auto id = fire::effects::parameterID(scope, 0, control);
            auto& slider = knob(*controls, id);
            if (control < 2)
            {
                CHECK_FALSE(slider.isShowing());
                // These stable targets remain usable through the Matrix.
                CHECK(processor.assignLfoToTarget(0, id) == LfoManager::AssignmentResult::changed);
                continue;
            }
            CHECK(slider.isShowing());
            CHECK(slider.getTitle() == juce::String("Chord Resonator ") + names[static_cast<size_t>(control - 2)]);
            CHECK(slider.findColour(juce::Slider::rotarySliderFillColourId) == fire::ui::colours::chordResonator);
            const auto& registered = panel.getModulatableSliders();
            CHECK(std::find(registered.begin(), registered.end(), &slider) != registered.end());
            REQUIRE(slider.onLfoAssignmentRequested != nullptr);
            slider.onLfoAssignmentRequested(0, id);
            CHECK(processor.getModulationInfoForParameter(id).isModulated);
            shown.push_back(&slider);
        }
        auto& decay = knob(*controls, fire::effects::parameterID(scope, 0, 3));
        CHECK(decay.getTextFromValue(decay.getValueFromText("0.05 s")) == "0.05 s");
        auto* graph = find<Oscilloscope>(panel, [](auto&) { return true; });
        REQUIRE(graph != nullptr);
        CHECK(graph->isShowing());
        for (int width : {1000, 1400, 2000})
        {
            CAPTURE(width);
            editor.setSize(width, width / 2);
            const auto scale = static_cast<float>(width) / 1000.0f;
            const auto expectedWidth = fire::ui::ordinaryKnobWidth(scale);
            for (size_t first = 0; first < shown.size(); ++first)
            {
                const auto bounds = controls->getLocalArea(shown[first], shown[first]->getLocalBounds());
                CHECK_FALSE(bounds.isEmpty());
                CHECK(controls->getLocalBounds().contains(bounds));
                if (first >= 3)
                {
                    CHECK(shown[first]->getWidth() == expectedWidth);
                    CHECK(shown[first]->getHeight() == fire::ui::ordinaryKnobHeight(expectedWidth, scale));
                }
                for (size_t second = first + 1; second < shown.size(); ++second)
                    CHECK_FALSE(bounds.intersects(controls->getLocalArea(shown[second], shown[second]->getLocalBounds())));
            }
            if (scope <= 1) snapshot(editor, juce::String(scope == 0 ? "master-" : "band-") + "chord-resonator-" + juce::String(width));
        }
        byID<juce::Button>(panel, fire::effects::parameterID(scope, 1, fire::effects::typeField)).triggerClick();
        CHECK_FALSE(root.isShowing());
        CHECK_FALSE(chord.isShowing());
        CHECK(freeze.isShowing());
    }
}

TEST_CASE("Chord selectors map normalized parameters to discrete musical values and refresh host automation",
          "[chord-resonator-ui][ui][automation][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SelectorFixture fixture;
    auto& root = fixture.root();
    auto& chord = fixture.chord();
    auto& rootParameter = fixture.parameter(0);
    auto& chordParameter = fixture.parameter(1);
    GestureRecorder rootGestures, chordGestures;
    rootParameter.addListener(&rootGestures);
    chordParameter.addListener(&chordGestures);
    const juce::ScopeGuard remove {[&]
    {
        rootParameter.removeListener(&rootGestures);
        chordParameter.removeListener(&chordGestures);
    }};
    auto result = ChordResonatorUiTestAccess::result(root);
    result(25); // MIDI 60 / C4 is item 25, normalized 24/36.
    result(37); // Results are consumed exactly once.
    CHECK(root.getText() == "C4");
    CHECK(rootParameter.getValue() == Catch::Approx(24.0f / 36.0f));
    CHECK(rootGestures.events == std::vector<bool> {true, false});
    CHECK(static_cast<juce::Component&>(chord).keyPressed(juce::KeyPress(juce::KeyPress::downKey)));
    CHECK(chord.getText() == "Sus 2");
    CHECK(chordParameter.getValue() == Catch::Approx(4.0f / 7.0f));
    CHECK(chordGestures.events == std::vector<bool> {true, false});
    const auto rootEvents = rootGestures.events;
    const auto chordEvents = chordGestures.events;
    std::thread automation([&]
    {
        rootParameter.setValueNotifyingHost(1.0f);
        chordParameter.setValueNotifyingHost(2.0f / 7.0f);
    });
    automation.join();
    waitFor([&] { return root.getText() == "C5" && chord.getText() == "Major 7"; });
    CHECK(rootGestures.events == rootEvents);
    CHECK(chordGestures.events == chordEvents);
    auto* notes = find<juce::Label>(*fixture.controls, [](auto& label) { return label.getTitle() == "Selected chord notes"; });
    REQUIRE(notes != nullptr);
    CHECK(notes->getText() == "C5  E5\nG5  B5");
}

TEST_CASE("Chord selector popup contexts cannot cross rebind hide disable or same slot recreation",
          "[chord-resonator-ui][ui][popup][lifecycle][reuse]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SelectorFixture fixture;
    auto& parameter = fixture.parameter(0);
    auto stale = ChordResonatorUiTestAccess::result(fixture.root());
    SECTION("scope ABA") { fixture.controls->bind(2, 0); fixture.controls->bind(1, 0); }
    SECTION("effect page ABA") { fixture.controls->bind(1, 1); fixture.controls->bind(1, 0); }
    SECTION("visibility ABA") { fixture.controls->setActive(false); fixture.controls->setActive(true); }
    SECTION("enablement ABA") { fixture.controls->setEnabled(false); fixture.controls->setEnabled(true); }
    SECTION("same slot delete and recreate before the UI clock")
    {
        fixture.processor.removeInsertEffect(1, 0);
        REQUIRE(fixture.processor.addInsertEffect(1, Type::chordResonator) == 0);
        CHECK(fixture.processor.treeState.getRawParameterValue(fire::effects::parameterID(1, 0, fire::effects::typeField))->load() == 0.0f);
    }
    const auto before = parameter.getValue();
    stale(37);
    CHECK(parameter.getValue() == before);
    auto fresh = ChordResonatorUiTestAccess::result(fixture.root());
    fresh(37);
    CHECK(parameter.getValue() == 1.0f);
    CHECK(fixture.root().getText() == "C5");
}

TEST_CASE("Chord selector host gestures stay paired when the owning UI is hidden rebound or deleted synchronously",
          "[chord-resonator-ui][ui][gesture][reentrancy][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SelectorFixture fixture;
    auto& parameter = fixture.parameter(0);
    const auto before = parameter.getValue();
    HostGestureRecorder listener(fixture.processor, parameter.getParameterIndex());
    std::function<void()> boundary;
    const char* boundaryName = "";
    SECTION("hide and show during begin") { boundaryName = "hide/show"; boundary = [&] { fixture.controls->setActive(false); fixture.controls->setActive(true); }; }
    SECTION("rebind during begin") { boundaryName = "rebind"; boundary = [&] { fixture.controls->bind(2, 0); }; }
    SECTION("delete during begin") { boundaryName = "delete"; boundary = [&] { fixture.controls.reset(); }; }
    CAPTURE(boundaryName);
    listener.onGesture = [&](bool starting) { if (starting) boundary(); };
    auto result = ChordResonatorUiTestAccess::result(fixture.root());
    result(37);
    CHECK(listener.events == std::vector<bool> {true, false});
    CHECK(parameter.getValue() == before);
    if (fixture.controls) CHECK(fixture.root().getText() == "C3");
}
