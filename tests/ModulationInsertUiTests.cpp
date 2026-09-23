#include <PluginEditor.h>
#include <GUI/EffectRackNavigation.h>
#include <GUI/InsertEffectControls.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace
{
using Type = fire::effects::Type;
using Rack = fire::ui::EffectRackNavigation;

template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate predicate)
{
    if (auto* result = dynamic_cast<T*>(&root); result != nullptr && predicate(*result)) return result;
    for (auto* child : root.getChildren())
        if (auto* result = find<T>(*child, predicate)) return result;
    return nullptr;
}

template <typename T>
T& byID(juce::Component& root, const juce::String& id)
{
    auto* result = find<T>(root, [&](const auto& component) { return component.getComponentID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}

ModulatableSlider& slider(juce::Component& root, const juce::String& id)
{
    auto* result = find<ModulatableSlider>(root, [&](const auto& control) { return control.getParamID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}

void click(juce::Component& root, const juce::String& text)
{
    auto* button = find<juce::Button>(root, [&](const auto& b) { return b.getButtonText() == text; });
    REQUIRE(button != nullptr);
    button->triggerClick();
}

void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

juce::MouseEvent event(juce::Component& control, juce::Point<float> point,
                       juce::Point<float> origin, bool dragged = false,
                       juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(), point, mods,
            0, 0, 0, 0, 0, &control, &control, now, origin, now, 1, dragged};
}

void show(juce::Component& component, int width, int height)
{
    component.setSize(width, height);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
    REQUIRE(component.isShowing());
}

std::vector<int> order(const FireAudioProcessor& processor, int scope)
{
    std::vector<int> result;
    for (int slot = 0; slot < fire::effects::slotCount; ++slot)
        if (processor.getInsertEffectType(scope, slot) != Type::none) result.push_back(slot);
    std::stable_sort(result.begin(), result.end(), [&](int a, int b)
    { return processor.getInsertEffectOrder(scope, a) < processor.getInsertEffectOrder(scope, b); });
    return result;
}

void checkMenu(const Rack& rack, bool enabled)
{
    const auto menu = rack.createAddMenu();
    for (const auto type : {Type::flanger, Type::phaser})
    {
        int count = 0;
        for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
            if (it.getItem().itemID == static_cast<int>(type))
            {
                ++count;
                CHECK(it.getItem().text == fire::effects::name(type));
                CHECK(it.getItem().isEnabled == enabled);
            }
        CHECK(count == 1);
    }
}

struct GestureRecorder final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { if (starting) ++begins; else ++ends; }
    int begins = 0, ends = 0;
};

void snapshot(FireAudioProcessorEditor& editor, const juce::String& name)
{
    const auto* directory = std::getenv("FIRE_MODULATION_INSERT_UI_SNAPSHOT_DIR");
    if (directory == nullptr || *directory == '\0') return;
    // The layout assertions resize after selecting the effect. Preserve that
    // deliberate test sequence, but reveal the selected row again in its
    // final geometry before exporting a visual review image.
    auto* rack = find<Rack>(editor, [](const auto& navigation) { return navigation.isShowing(); });
    REQUIRE(rack != nullptr);
    juce::TextButton* selectedRow = nullptr;
    for (int slot = 0; slot < fire::effects::slotCount; ++slot)
    {
        auto& row = byID<juce::TextButton>(*rack,
            fire::effects::parameterID(rack->getScope(), slot, fire::effects::typeField));
        if (row.getToggleState())
        {
            selectedRow = &row;
            rack->setSelectedSlot(slot);
            break;
        }
    }
    REQUIRE(selectedRow != nullptr);
    // Let the real editor clock settle workspace and module reveal overlays.
    // This extra dispatch happens only when visual review was requested.
    const bool wasRunning = editor.isTimerRunning();
    const auto interval = editor.getTimerInterval();
    const juce::ScopeGuard restore {[&]
    {
        editor.stopTimer();
        if (wasRunning) editor.startTimer(interval);
    }};
    editor.startTimerHz(60);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(500);
    editor.stopTimer();
    CHECK(rack->getViewport().getViewArea().contains(selectedRow->getBounds()));
    const auto file = juce::File(juce::String::fromUTF8(directory)).getChildFile(name + ".png");
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream));
}
}

TEST_CASE("Flanger and Phaser menus support independent repeated moving and removable inserts in every scope",
          "[modulation-insert-ui][insertfx][ui][menu][rack-interaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    CHECK(static_cast<int>(Type::flanger) == 6);
    CHECK(static_cast<int>(Type::phaser) == 7);
    for (int scope = 0; scope <= 4; ++scope)
    {
        CAPTURE(scope);
        Rack rack(processor, scope);
        rack.onSelectEffect = [&](int slot) { rack.setSelectedSlot(slot); };
        show(rack, 190, 310);
        checkMenu(rack, true);
        const std::array types {Type::flanger, Type::phaser, Type::flanger, Type::phaser};
        for (int slot = 0; slot < 4; ++slot)
        {
            rack.createAddMenuResultHandler()(static_cast<int>(types[static_cast<size_t>(slot)]));
            REQUIRE(processor.getInsertEffectType(scope, slot) == types[static_cast<size_t>(slot)]);
            auto& row = byID<juce::TextButton>(rack, fire::effects::parameterID(scope, slot, fire::effects::typeField));
            CHECK(row.getButtonText() == fire::effects::name(types[static_cast<size_t>(slot)]));
            CHECK_FALSE(row.getTooltip().isEmpty());
        }
        const auto parameterID = fire::effects::parameterID(scope, 0, 0);
        auto* parameter = processor.treeState.getParameter(parameterID);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(0.31f);
        REQUIRE(processor.assignLfoToTarget(0, parameterID) == LfoManager::AssignmentResult::changed);
        auto& row = byID<juce::TextButton>(rack, fire::effects::parameterID(scope, 0, fire::effects::typeField));
        const auto origin = row.getLocalBounds().toFloat().getCentre();
        const auto destination = row.getLocalPoint(&rack.getViewport(),
            juce::Point<float>(80, static_cast<float>(rack.getViewport().getHeight() - 3)));
        auto& component = static_cast<juce::Component&>(row);
        component.mouseDown(event(row, origin, origin));
        component.mouseDrag(event(row, destination, origin, true));
        component.mouseUp(event(row, destination, origin, true));
        CHECK(order(processor, scope) == std::vector<int> {1, 2, 3, 0});
        CHECK(processor.treeState.getParameter(parameterID) == parameter);
        CHECK(parameter->getValue() == Catch::Approx(0.31f));
        CHECK(processor.getModulationInfoForParameter(parameterID).isModulated);

        auto& remove = byID<CloseButton>(rack,
            fire::effects::parameterID(scope, 2, fire::effects::typeField) + "Remove");
        remove.setPresented(true, false);
        remove.triggerClick();
        CHECK(processor.getInsertEffectType(scope, 2) == Type::none);
        CHECK(processor.getInsertEffectType(scope, 1) == Type::phaser);
        rack.createAddMenuResultHandler()(static_cast<int>(Type::flanger));
        CHECK(processor.getInsertEffectType(scope, 2) == Type::flanger);
        CHECK(order(processor, scope) == std::vector<int> {1, 3, 0, 2});
        for (int slot = 4; slot < fire::effects::slotCount; ++slot)
            rack.createAddMenuResultHandler()(static_cast<int>(slot % 2 == 0 ? Type::flanger : Type::phaser));
        CHECK(order(processor, scope).size() == fire::effects::slotCount);
        checkMenu(rack, false);
        rack.createAddMenuResultHandler()(static_cast<int>(Type::phaser));
        CHECK(order(processor, scope).size() == fire::effects::slotCount);
    }
    for (int scope = 0; scope <= 4; ++scope)
    {
        CHECK(processor.getInsertEffectType(scope, 0) == Type::flanger);
        CHECK(processor.getInsertEffectType(scope, 1) == Type::phaser);
        CHECK(processor.getInsertEffectType(scope, 2) == Type::flanger);
    }
}

TEST_CASE("Modulation insert pages expose six physical controls and LFO targets without leaking Clouds controls",
          "[modulation-insert-ui][insertfx][ui][layout][lfo][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, NUM_BANDS_ID, 4);
    for (int line = 0; line < 3; ++line) set(processor, ParameterIDAndName::getIDString(LINE_STATE_ID, line), 1);
    for (int scope = 0; scope <= 4; ++scope)
    {
        REQUIRE(processor.addInsertEffect(scope, Type::flanger) == 0);
        REQUIRE(processor.addInsertEffect(scope, Type::phaser) == 1);
        REQUIRE(processor.addInsertEffect(scope, Type::granular) == 2);
    }
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    show(editor, 1000, 500);
    auto* band = find<BandPanel>(editor, [](const auto&) { return true; });
    auto* master = find<GlobalPanel>(editor, [](const auto&) { return true; });
    REQUIRE(band != nullptr);
    REQUIRE(master != nullptr);
    CHECK(fire::ui::effectColour(Type::flanger) != fire::ui::effectColour(Type::phaser));
    CHECK(fire::ui::effectColour(Type::flanger) != fire::ui::colours::textMuted);
    CHECK(fire::ui::effectColour(Type::phaser) != fire::ui::colours::textMuted);

    for (int scope = 0; scope <= 4; ++scope)
    {
        CAPTURE(scope);
        click(editor, scope == 0 ? "MASTER LAB" : "BAND LAB");
        if (scope > 0) band->setFocusBandNum(scope - 1);
        auto& panel = scope == 0 ? static_cast<PanelBase&>(*master) : static_cast<PanelBase&>(*band);
        auto* controls = find<InsertEffectControls>(panel, [](const auto&) { return true; });
        REQUIRE(controls != nullptr);
        byID<juce::Button>(panel, fire::effects::parameterID(scope, 2, fire::effects::typeField)).triggerClick();
        REQUIRE(controls->usesExpandedLayout());
        std::array<ModulatableSlider*, 3> extras;
        for (int index = 0; index < 3; ++index)
            extras[static_cast<size_t>(index)] = &slider(*controls,
                fire::clouds_params::parameterID(scope, 2, fire::clouds_params::spreadField + index));
        auto& freeze = byID<juce::Button>(*controls,
            fire::clouds_params::parameterID(scope, 2, fire::clouds_params::freezeField));

        for (int slot = 0; slot < 2; ++slot)
        {
            const auto type = slot == 0 ? Type::flanger : Type::phaser;
            CAPTURE(fire::effects::name(type));
            byID<juce::Button>(panel, fire::effects::parameterID(scope, slot, fire::effects::typeField)).triggerClick();
            REQUIRE(controls->isShowing());
            CHECK_FALSE(controls->usesExpandedLayout());
            CHECK_FALSE(controls->usesFullWidthLayout());
            CHECK_FALSE(freeze.isShowing());
            for (auto* extra : extras)
            {
                CHECK_FALSE(extra->isShowing());
                CHECK(extra->getParamID().isEmpty());
            }
            auto* graph = find<Oscilloscope>(panel, [](const auto&) { return true; });
            REQUIRE(graph != nullptr);
            CHECK(graph->isShowing());
            const std::array<const char*, 6> names {"Rate", "Depth", slot == 0 ? "Delay" : "Center", "Feedback", "Width", "Mix"};
            const std::array<const char*, 6> units {"Hz", "%", slot == 0 ? "ms" : "Hz", "%", "%", "%"};
            const std::array<float, 6> values {0.05f, 62.0f, slot == 0 ? 2.4f : 880.0f, -30.0f, 75.0f, 35.0f};
            std::array<ModulatableSlider*, 6> visible;
            for (int control = 0; control < 6; ++control)
            {
                const auto index = static_cast<size_t>(control);
                const auto id = fire::effects::parameterID(scope, slot, control);
                auto& knob = slider(*controls, id);
                visible[index] = &knob;
                const auto& definition = fire::effects::controls(type)[index];
                CHECK(knob.isShowing());
                CHECK(knob.getTitle() == juce::String(fire::effects::name(type)) + " " + names[index]);
                CHECK(knob.getTextFromValue(knob.getValue()).contains(units[index]));
                CHECK(definition.fromNormalised(static_cast<float>(knob.getValue())) == Catch::Approx(definition.initial).margin(0.01f));
                CHECK(knob.getDoubleClickReturnValue() == Catch::Approx(definition.toNormalised(definition.initial)));
                CHECK(knob.findColour(juce::Slider::rotarySliderFillColourId) == fire::ui::effectColour(type));
                const auto& registered = panel.getModulatableSliders();
                CHECK(std::find(registered.begin(), registered.end(), &knob) != registered.end());
                auto* accessible = knob.getAccessibilityHandler();
                REQUIRE(accessible != nullptr);
                CHECK_FALSE(accessible->getTitle().isEmpty());
                CHECK_FALSE(accessible->getHelp().isEmpty());
                REQUIRE(values[index] >= definition.minimum);
                REQUIRE(values[index] <= definition.maximum);
                const auto typed = juce::String(values[index], 2) + " " + units[index];
                knob.setValue(knob.getValueFromText(typed), juce::sendNotificationSync);
                CHECK(processor.treeState.getRawParameterValue(id)->load() == Catch::Approx(definition.toNormalised(values[index])).margin(1.0e-5f));
                CHECK(knob.getTextFromValue(knob.getValue()).getFloatValue() == Catch::Approx(values[index]).margin(0.051f));
                REQUIRE(knob.onLfoAssignmentRequested != nullptr);
                knob.onLfoAssignmentRequested(0, id);
                CHECK(processor.getModulationInfoForParameter(id).isModulated);
                CHECK(knob.isModulated);
            }
            CHECK(visible[0]->getTextFromValue(visible[0]->getValue()) == "0.05 Hz");
            CHECK(visible[3]->getTooltip().containsIgnoreCase("negative"));
            for (int width : {1000, 1400, 2000})
            {
                CAPTURE(width);
                editor.setSize(width, width / 2);
                const auto scale = static_cast<float>(width) / 1000.0f;
                const auto expectedWidth = fire::ui::ordinaryKnobWidth(scale);
                for (size_t first = 0; first < visible.size(); ++first)
                {
                    const auto bounds = controls->getLocalArea(visible[first], visible[first]->getLocalBounds());
                    CHECK(visible[first]->getWidth() == expectedWidth);
                    CHECK(visible[first]->getHeight() == fire::ui::ordinaryKnobHeight(expectedWidth, scale));
                    CHECK(controls->getLocalBounds().contains(bounds));
                    for (size_t second = first + 1; second < visible.size(); ++second)
                        CHECK_FALSE(bounds.intersects(controls->getLocalArea(visible[second], visible[second]->getLocalBounds())));
                }
            }
            if (scope <= 1)
            {
                editor.setSize(1000, 500);
                snapshot(editor, juce::String(scope == 0 ? "master-" : "band-") + fire::effects::name(type));
            }
        }
        CHECK(processor.treeState.getRawParameterValue(fire::clouds_params::parameterID(scope, 2, fire::clouds_params::freezeField))->load() == 0.0f);
    }
    for (int scope = 0; scope <= 4; ++scope)
        for (int slot = 0; slot < 2; ++slot)
        {
            const auto type = slot == 0 ? Type::flanger : Type::phaser;
            const auto raw = processor.treeState.getRawParameterValue(fire::effects::parameterID(scope, slot, 2))->load();
            CHECK(fire::effects::controls(type)[2].fromNormalised(raw) == Catch::Approx(slot == 0 ? 2.4f : 880.0f).margin(0.01f));
        }
}

TEST_CASE("Changing Flanger delay to Phaser center or Master closes the old parameter gesture",
          "[modulation-insert-ui][insertfx][ui][gesture][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    REQUIRE(processor.addInsertEffect(1, Type::flanger) == 0);
    REQUIRE(processor.addInsertEffect(1, Type::phaser) == 1);
    REQUIRE(processor.addInsertEffect(0, Type::phaser) == 0);
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    show(editor, 1000, 500);
    auto* band = find<BandPanel>(editor, [](const auto&) { return true; });
    REQUIRE(band != nullptr);
    byID<juce::Button>(*band, fire::effects::parameterID(1, 0, fire::effects::typeField)).triggerClick();
    auto& knob = slider(*band, fire::effects::parameterID(1, 0, 2));
    auto* delay = processor.treeState.getParameter(knob.getParamID());
    auto* center = processor.treeState.getParameter(fire::effects::parameterID(1, 1, 2));
    REQUIRE(delay != nullptr);
    REQUIRE(center != nullptr);
    GestureRecorder delayGestures, centerGestures;
    delay->addListener(&delayGestures);
    center->addListener(&centerGestures);
    const juce::ScopeGuard cleanup {[&]
    {
        delay->removeListener(&delayGestures);
        center->removeListener(&centerGestures);
    }};
    const auto origin = knob.getLocalBounds().toFloat().getCentre();
    knob.mouseDown(event(knob, origin, origin));
    REQUIRE(delayGestures.begins == 1);
    byID<juce::Button>(*band, fire::effects::parameterID(1, 1, fire::effects::typeField)).triggerClick();
    CHECK(delayGestures.ends == 1);
    CHECK_FALSE(knob.hasActiveInteraction());
    CHECK(knob.getTitle() == "Phaser Center");
    const auto centerBefore = center->getValue();
    knob.mouseUp(event(knob, origin, origin, false, {}));
    CHECK(center->getValue() == centerBefore);
    CHECK(centerGestures.begins == 0);
    knob.mouseDown(event(knob, origin, origin));
    REQUIRE(centerGestures.begins == 1);
    click(editor, "MASTER LAB");
    CHECK(centerGestures.ends == 1);
    CHECK_FALSE(knob.hasActiveInteraction());
    CHECK_FALSE(knob.isShowing());
    byID<juce::Button>(editor, fire::effects::parameterID(0, 0, fire::effects::typeField)).triggerClick();
    auto& masterCenter = slider(editor, fire::effects::parameterID(0, 0, 2));
    CHECK(masterCenter.isShowing());
    CHECK(masterCenter.getTitle() == "Phaser Center");
    CHECK(processor.treeState.getParameter(masterCenter.getParamID()) != center);
}
