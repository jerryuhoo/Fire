#include <PluginEditor.h>
#include <GUI/InsertEffectControls.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{
template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate predicate)
{
    if (auto* result = dynamic_cast<T*>(&root); result && predicate(*result)) return result;
    for (auto* child : root.getChildren())
        if (auto* result = find<T>(*child, predicate)) return result;
    return nullptr;
}
juce::Button& buttonByID(juce::Component& root, const juce::String& id)
{
    auto* result = find<juce::Button>(root, [&](auto& b) { return b.getComponentID() == id; });
    REQUIRE(result != nullptr);
    return *result;
}
void clickText(juce::Component& root, const juce::String& text)
{
    auto* result = find<juce::Button>(root, [&](auto& b) { return b.getButtonText() == text; });
    REQUIRE(result != nullptr);
    result->triggerClick();
}
ModulatableSlider& sliderByID(juce::Component& root, const juce::String& id)
{
    auto* result = find<ModulatableSlider>(root, [&](auto& s) { return s.parameterID == id; });
    REQUIRE(result != nullptr);
    return *result;
}
void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
juce::MouseEvent event(juce::Component& control, juce::ModifierKeys mods = {})
{
    const auto now = juce::Time::getCurrentTime();
    const auto point = control.getLocalBounds().toFloat().getCentre();
    return {juce::Desktop::getInstance().getMainMouseSource(), point, mods,
            0, 0, 0, 0, 0, &control, &control, now, point, now, 1, false};
}
struct Gestures final : juce::AudioProcessorParameter::Listener
{
    int starts = 0, ends = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { if (starting) ++starts; else ++ends; }
};
void show(FireAudioProcessorEditor& editor)
{
    editor.stopTimer();
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    REQUIRE(editor.isShowing());
}
void snapshotIfRequested(FireAudioProcessorEditor& editor, int scope)
{
    const auto* path = std::getenv("FIRE_CLOUDS_UI_SNAPSHOT");
    if (! path || ! *path) return;
    editor.setSize(1000, 500);
    auto* panel = find<PanelBase>(editor, [](auto& p) { return p.isShowing(); });
    REQUIRE(panel != nullptr);
    const auto samplePoint = editor.getLocalPoint(panel, panel->getLocalBounds().getCentre());
    const bool timerWasRunning = editor.isTimerRunning();
    const auto previousInterval = editor.getTimerInterval();
    const juce::ScopeGuard restoreTimer {[&]
    {
        editor.stopTimer();
        if (timerWasRunning) editor.startTimer(previousInterval);
    }};
    // The editor owns the workspace reveal overlay. Advancing only the panel
    // clock leaves MASTER LAB covered even after its controls have settled.
    editor.startTimerHz(60);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
    bool revealHasSettled = false;
    for (int attempt = 0; attempt < 7; ++attempt)
    {
        juce::Image overlay(juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
        juce::Graphics graphics(overlay);
        editor.paintOverChildren(graphics);
        revealHasSettled = overlay.getPixelAt(samplePoint.x, samplePoint.y).getAlpha() == 0;
        if (revealHasSettled) break;
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    editor.stopTimer();
    REQUIRE(revealHasSettled);
    juce::File file(juce::String::fromUTF8(path));
    if (scope == 0) file = file.getSiblingFile(file.getFileNameWithoutExtension() + "-master.png");
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(
        editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f), *stream));
}
} // namespace

TEST_CASE("Clouds pages expose full controls without changing the six existing parameter IDs",
          "[clouds-ui][ui][layout][insertfx]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const int scope : {1, 0})
    {
        CAPTURE(scope);
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        const int slot = processor.addInsertEffect(scope, fire::effects::Type::granular);
        REQUIRE(slot >= 0);
        FireAudioProcessorEditor editor(processor);
        show(editor);
        if (scope == 0) clickText(editor, "MASTER LAB");
        buttonByID(editor, fire::effects::parameterID(scope, slot, fire::effects::typeField)).triggerClick();
        auto* controls = find<InsertEffectControls>(editor, [](auto& c) { return c.isShowing(); });
        REQUIRE(controls != nullptr);
        REQUIRE(controls->usesExpandedLayout());
        auto* panel = controls->findParentComponentOfClass<PanelBase>();
        REQUIRE(panel != nullptr);
        std::vector<juce::Component*> visible;
        const std::array<int, 6> displayedOrder {3, 0, 2, 1, 4, 5};
        const std::array<const char*, 6> displayedNames {"Position", "Size", "Pitch", "Density", "Texture", "Mix"};
        for (size_t i = 0; i < displayedOrder.size(); ++i)
        {
            auto& slider = sliderByID(*controls, fire::effects::parameterID(scope, slot, displayedOrder[i]));
            CHECK(slider.getTitle().endsWith(displayedNames[i]));
            visible.push_back(&slider);
        }
        for (int field : {fire::clouds_params::spreadField, fire::clouds_params::feedbackField, fire::clouds_params::reverbField})
        {
            auto& slider = sliderByID(*controls, fire::clouds_params::parameterID(scope, slot, field));
            const auto& registered = panel->getModulatableSliders();
            CHECK(std::find(registered.begin(), registered.end(), &slider) != registered.end());
            visible.push_back(&slider);
        }
        auto& freeze = buttonByID(*controls, fire::clouds_params::parameterID(scope, slot, fire::clouds_params::freezeField));
        visible.push_back(&freeze);
        CHECK(find<juce::ComboBox>(*controls, [](auto& combo) { return combo.isShowing(); }) == nullptr);

        for (auto size : {juce::Point<int>(1000, 500), juce::Point<int>(1400, 700), juce::Point<int>(2000, 1000)})
        {
            editor.setSize(size.x, size.y);
            CAPTURE(size.x, size.y);
            for (size_t i = 0; i < visible.size(); ++i)
            {
                REQUIRE(visible[i]->isShowing());
                const auto bounds = controls->getLocalArea(visible[i], visible[i]->getLocalBounds());
                CHECK_FALSE(bounds.isEmpty());
                CHECK(controls->getLocalBounds().contains(bounds));
                for (size_t j = i + 1; j < visible.size(); ++j)
                    CHECK_FALSE(bounds.intersects(controls->getLocalArea(visible[j], visible[j]->getLocalBounds())));
            }
            for (size_t i = 1; i < displayedOrder.size(); ++i)
                CHECK(visible[i - 1]->getRight() < visible[i]->getX());
            auto* graph = find<Oscilloscope>(*panel, [](auto&) { return true; });
            REQUIRE(graph != nullptr);
            CHECK_FALSE(graph->isShowing());
        }
        auto& density = sliderByID(*controls, fire::effects::parameterID(scope, slot, 1));
        CHECK(density.getTextFromValue(0.49) == "Off");
        CHECK(density.getTextFromValue(0.51) == "Off");
        CHECK(density.getValueFromText("Off") == Catch::Approx(0.5));
        CHECK(density.getTooltip().contains("Regular"));
        CHECK(density.getTooltip().contains("Random"));
        CHECK(sliderByID(*controls, fire::clouds_params::parameterID(scope, slot, fire::clouds_params::spreadField))
                  .getDoubleClickReturnValue() == Catch::Approx(0.5));
        snapshotIfRequested(editor, scope);
    }
}

TEST_CASE("Switching away from Clouds closes old gestures and safely restores the effect graph",
          "[clouds-ui][ui][lifecycle][insertfx]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    const int slot = processor.addInsertEffect(1, fire::effects::Type::granular);
    REQUIRE(slot >= 0);
    const auto typeID = fire::effects::parameterID(1, slot, fire::effects::typeField);
    const auto freezeID = fire::clouds_params::parameterID(1, slot, fire::clouds_params::freezeField);
    FireAudioProcessorEditor editor(processor);
    show(editor);
    buttonByID(editor, fire::effects::parameterID(1, slot, fire::effects::typeField)).triggerClick();
    auto* controls = find<InsertEffectControls>(editor, [](auto& c) { return c.isShowing(); });
    REQUIRE(controls != nullptr);
    auto& spread = sliderByID(*controls, fire::clouds_params::parameterID(1, slot, fire::clouds_params::spreadField));
    auto* parameter = processor.treeState.getParameter(spread.parameterID);
    REQUIRE(parameter != nullptr);
    Gestures gestures;
    parameter->addListener(&gestures);
    const juce::ScopeGuard removeListener {[&] {parameter->removeListener(&gestures);}};
    spread.mouseDown(event(spread, juce::ModifierKeys::leftButtonModifier));
    REQUIRE(spread.hasActiveInteraction());
    REQUIRE(gestures.starts == 1);
    set(processor, typeID, static_cast<float>(fire::effects::Type::delay));
    editor.timerCallback();
    CHECK_FALSE(spread.hasActiveInteraction());
    CHECK(gestures.ends == 1);
    CHECK_FALSE(controls->usesExpandedLayout());
    CHECK_FALSE(spread.isShowing());
    CHECK(sliderByID(*controls, fire::effects::parameterID(1, slot, 4)).getTitle().endsWith("Sync"));
    auto* graph = find<Oscilloscope>(*controls->getParentComponent(), [](auto&) { return true; });
    REQUIRE(graph != nullptr);
    REQUIRE(graph->isShowing());
    auto* handler = graph->getAccessibilityHandler();
    REQUIRE(handler != nullptr);
    REQUIRE(handler->getActions().invoke(juce::AccessibilityActionType::press));
    REQUIRE(graph->getZoomState());
    set(processor, typeID, static_cast<float>(fire::effects::Type::granular));
    editor.timerCallback();
    CHECK_FALSE(graph->getZoomState());
    CHECK_FALSE(graph->isShowing());
    CHECK(controls->isShowing());
    CHECK(spread.isShowing());

    buttonByID(*controls, freezeID).triggerClick();
    CHECK(processor.treeState.getRawParameterValue(freezeID)->load() == Catch::Approx(1.0f));
    clickText(*controls->getParentComponent(), "Drive");
    CHECK_FALSE(controls->isShowing());
    CHECK(processor.treeState.getRawParameterValue(freezeID)->load() == Catch::Approx(1.0f));
    buttonByID(editor, fire::effects::parameterID(1, slot, fire::effects::typeField)).triggerClick();
    CHECK(buttonByID(*controls, freezeID).getToggleState());
}

TEST_CASE("Clouds extra knob gestures remain attached to their original band until release",
          "[clouds-ui][ui][lifecycle][rebind]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    set(processor, NUM_BANDS_ID, 2);
    set(processor, "lineState1", 1);
    const int slot = processor.addInsertEffect(1, fire::effects::Type::granular);
    REQUIRE(slot == processor.addInsertEffect(2, fire::effects::Type::granular));
    FireAudioProcessorEditor editor(processor);
    show(editor);
    buttonByID(editor, fire::effects::parameterID(1, slot, fire::effects::typeField)).triggerClick();
    auto* panel = find<BandPanel>(editor, [](auto&) { return true; });
    REQUIRE(panel != nullptr);
    const auto originalID = fire::clouds_params::parameterID(1, slot, fire::clouds_params::spreadField);
    auto& spread = sliderByID(*panel, originalID);
    Gestures original, next;
    auto* originalParameter = processor.treeState.getParameter(originalID);
    auto* nextParameter = processor.treeState.getParameter(fire::clouds_params::parameterID(2, slot, fire::clouds_params::spreadField));
    REQUIRE(originalParameter != nullptr);
    REQUIRE(nextParameter != nullptr);
    originalParameter->addListener(&original);
    nextParameter->addListener(&next);
    const juce::ScopeGuard cleanup {[&] {originalParameter->removeListener(&original); nextParameter->removeListener(&next);}};
    spread.mouseDown(event(spread, juce::ModifierKeys::leftButtonModifier));
    REQUIRE(spread.hasActiveInteraction());
    panel->setFocusBandNum(1);
    CHECK(panel->getFocusBandNum() == 0);
    CHECK(spread.parameterID == originalID);
    spread.mouseUp(event(spread));
    CHECK(panel->getFocusBandNum() == 1);
    CHECK(spread.parameterID == fire::clouds_params::parameterID(2, slot, fire::clouds_params::spreadField));
    CHECK(original.starts == 1);
    CHECK(original.ends == 1);
    CHECK(next.starts == 0);
    CHECK(next.ends == 0);

    // Host automation can change the effect while a deferred band switch waits
    // for this same drag. Ending it re-enters bind for band 2; the old band 1
    // refresh must not overwrite that newer attachment context on return.
    panel->setFocusBandNum(0);
    spread.mouseDown(event(spread, juce::ModifierKeys::leftButtonModifier));
    REQUIRE(spread.hasActiveInteraction());
    panel->setFocusBandNum(1);
    set(processor, fire::effects::parameterID(1, slot, fire::effects::typeField),
        static_cast<float>(fire::effects::Type::delay));
    editor.timerCallback();
    CHECK(panel->getFocusBandNum() == 1);
    CHECK(spread.parameterID == fire::clouds_params::parameterID(2, slot, fire::clouds_params::spreadField));
    auto* controls = spread.findParentComponentOfClass<InsertEffectControls>();
    REQUIRE(controls != nullptr);
    CHECK(controls->usesExpandedLayout());
    CHECK_FALSE(spread.hasActiveInteraction());
    CHECK(original.starts == 2);
    CHECK(original.ends == 2);
    CHECK(next.starts == 0);
    CHECK(next.ends == 0);
}
