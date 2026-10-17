#include <GUI/ModulationSourceControls.h>
#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

struct EnvelopeControlsTestAccess
{
    static void tick(fire::ui::ModulationSourceControls& controls) { controls.timerCallback(); }
    static void expirePreview(fire::ui::EnvelopeListenButton& button) { button.previewDeadline = 0; button.poll(); }
    static std::function<void(int)> sourceResult(ContextAwareComboBox& combo)
    { return combo.createPopupResultHandler(); }
};

namespace
{
using Controls = fire::ui::ModulationSourceControls;
namespace params = fire::envelope_params;

template <typename T> T& control(juce::Component& root, const juce::String& id)
{
    std::function<T*(juce::Component&)> find = [&](auto& candidate) -> T*
    {
        if (auto* result = dynamic_cast<T*>(&candidate); result != nullptr && candidate.getComponentID() == id) return result;
        for (auto* child : candidate.getChildren()) if (auto* result = find(*child)) return result;
        return nullptr;
    };
    auto* result = find(root);
    REQUIRE(result != nullptr);
    return *result;
}
float parameter(FireAudioProcessor& processor, int envelope, int field)
{
    const auto* value = processor.treeState.getRawParameterValue(params::parameterID(envelope, field));
    REQUIRE(value != nullptr);
    return value->load();
}
juce::MouseEvent event(juce::Component& component, int modifiers,
                       juce::Point<float> point = {12, 12}, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(), point,
        juce::ModifierKeys(modifiers), 0, 0, 0, 0, 0, &component, &component,
        now, {12, 12}, now, 1, dragged};
}
struct HostGestures : juce::AudioProcessorParameter::Listener
{
    int started = 0, ended = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { starting ? ++started : ++ended; }
};
void show(Controls& controls)
{
    controls.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    controls.setVisible(true);
}
}

TEST_CASE("Envelope tabs rebind independent detectors while preserving macro identities and undo",
          "[envelope-controls][ui][undo]")
{
    FireAudioProcessor processor;
    Controls controls(processor);
    auto* macro = &control<PrimarySlider>(controls, "macro1");
    control<PrimarySlider>(controls, "envAttack").setValue(31.0, juce::sendNotificationSync);
    CHECK(parameter(processor, 0, params::attack) == Catch::Approx(31));
    processor.checkpointEditHistory();
    control<PrimaryTextButton>(controls, "envelope_tab_2").triggerClick();
    REQUIRE(controls.getSelectedEnvelope() == 1);
    control<PrimarySlider>(controls, "env2Attack").setValue(73, juce::sendNotificationSync);
    processor.checkpointEditHistory();
    REQUIRE(processor.undoEdit());
    CHECK(parameter(processor, 1, params::attack) == Catch::Approx(10));
    CHECK(parameter(processor, 0, params::attack) == Catch::Approx(31));
    REQUIRE(processor.redoEdit());
    CHECK(parameter(processor, 1, params::attack) == Catch::Approx(73));
    controls.selectEnvelope(3);
    auto& source = control<ContextAwareComboBox>(controls, "envelope_detector_source");
    source.setSelectedId(params::externalInput + 1, juce::sendNotificationSync);
    CHECK(parameter(processor, 3, params::source) == params::externalInput);
    auto& highPass = control<PrimaryToggleButton>(controls, "env4HighPassOn");
    highPass.triggerClick();
    EnvelopeControlsTestAccess::tick(controls);
    REQUIRE(parameter(processor, 3, params::highPassEnabled) == 1);
    auto& frequency = control<PrimarySlider>(controls, "env4HighPass");
    CHECK(frequency.isEnabled());
    frequency.setValue(320, juce::sendNotificationSync);
    CHECK(parameter(processor, 3, params::highPassFrequency) == Catch::Approx(320));
    controls.selectEnvelope(0);
    CHECK(control<ContextAwareComboBox>(controls, "envelope_detector_source").getSelectedId() == 1);
    CHECK_FALSE(control<PrimaryToggleButton>(controls, "env1HighPassOn").getToggleState());
    CHECK(&control<PrimarySlider>(controls, "macro1") == macro);
    macro->setValue(.65, juce::sendNotificationSync);
    CHECK(processor.treeState.getRawParameterValue("macro1")->load() == Catch::Approx(.65f));
}

TEST_CASE("Envelope rebinding closes gestures and rejects a stale detector menu result",
          "[envelope-controls][ui][gesture][lifecycle]")
{
    FireAudioProcessor processor;
    Controls controls(processor);
    show(controls);
    auto& attack = control<PrimarySlider>(controls, "envAttack");
    auto* attackParameter = processor.treeState.getParameter("envAttack");
    REQUIRE(attackParameter != nullptr);
    HostGestures gestures;
    attackParameter->addListener(&gestures);
    const juce::ScopeGuard cleanup {[&] { attackParameter->removeListener(&gestures); }};
    const auto down = attack.getLocalBounds().toFloat().getCentre();
    attack.mouseDown(event(attack, juce::ModifierKeys::leftButtonModifier, down));
    attack.mouseDrag(event(attack, juce::ModifierKeys::leftButtonModifier, down + juce::Point<float>{10, -14}, true));
    REQUIRE(gestures.started == 1);
    REQUIRE(gestures.ended == 0);
    auto staleMenu = EnvelopeControlsTestAccess::sourceResult(control<ContextAwareComboBox>(controls, "envelope_detector_source"));
    controls.selectEnvelope(2);
    CHECK(gestures.ended == 1);
    const auto oldValue = parameter(processor, 0, params::attack);
    attack.mouseDrag(event(attack, juce::ModifierKeys::leftButtonModifier, down + juce::Point<float>{40, -50}, true));
    attack.mouseUp(event(attack, 0, down));
    staleMenu(params::externalInput + 1);
    CHECK(parameter(processor, 0, params::attack) == oldValue);
    CHECK(parameter(processor, 2, params::attack) == Catch::Approx(10));
    CHECK(parameter(processor, 0, params::source) == params::mainInput);
    CHECK(parameter(processor, 2, params::source) == params::mainInput);
}

TEST_CASE("Detector listening is momentary and cancels at editor lifecycle boundaries",
          "[envelope-controls][ui][audition][lifecycle]")
{
    FireAudioProcessor processor;
    auto controls = std::make_unique<Controls>(processor);
    show(*controls);
    auto& listen = control<fire::ui::EnvelopeListenButton>(*controls, "envelope_listen");
    listen.mouseDown(event(listen, juce::ModifierKeys::rightButtonModifier));
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
    auto* accessibility = listen.getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    REQUIRE(accessibility->getActions().invoke(juce::AccessibilityActionType::press));
    CHECK(processor.getEnvelopeAuditionIndex() == 0);
    EnvelopeControlsTestAccess::expirePreview(listen);
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
    listen.mouseDown(event(listen, juce::ModifierKeys::leftButtonModifier));
    REQUIRE(processor.getEnvelopeAuditionIndex() == 0);
    listen.mouseUp(event(listen, 0));
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
    REQUIRE(listen.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)));
    REQUIRE(processor.getEnvelopeAuditionIndex() == 0);
    controls->selectEnvelope(1);
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
    // A repeat from the still-held old key must not audition the new tab.
    listen.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
    listen.keyStateChanged(false);
    listen.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
    REQUIRE(processor.getEnvelopeAuditionIndex() == 1);
    controls->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
    listen.keyStateChanged(false);
    listen.mouseDown(event(listen, juce::ModifierKeys::leftButtonModifier));
    const auto anotherEditor = processor.beginEnvelopeAudition(3);
    listen.mouseUp(event(listen, 0));
    CHECK(processor.getEnvelopeAuditionIndex() == 3);
    processor.endEnvelopeAudition(anotherEditor);
    listen.mouseDown(event(listen, juce::ModifierKeys::leftButtonModifier));
    controls->setVisible(false);
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
    controls->setVisible(true);
    listen.mouseDown(event(listen, juce::ModifierKeys::leftButtonModifier));
    REQUIRE(processor.getEnvelopeAuditionIndex() == 1);
    controls.reset();
    CHECK(processor.getEnvelopeAuditionIndex() == -1);
}

TEST_CASE("Envelope controls retain four-skin readability and fit compact and full dialogs",
          "[envelope-controls][ui][skin][layout]")
{
    FireAudioProcessor processor;
    Controls controls(processor);
    for (const auto skin : fire::ui::skins)
        for (const auto size : {juce::Point<int>{620, 340}, juce::Point<int>{740, 480}})
        {
            fire::ui::setSkin(controls, skin);
            controls.sendLookAndFeelChange();
            controls.setSize(size.x, size.y);
            for (int envelope = 0; envelope < params::count; ++envelope)
            {
                controls.selectEnvelope(envelope);
                for (auto* child : controls.getChildren())
                {
                    CAPTURE(child->getComponentID().toStdString(), size.x, envelope);
                    CHECK(controls.getLocalBounds().contains(child->getBounds()));
                    CHECK_FALSE(child->getBounds().isEmpty());
                }
                auto& source = control<ContextAwareComboBox>(controls, "envelope_detector_source");
                CHECK(source.findColour(juce::ComboBox::textColourId) == fire::ui::paletteFor(controls).textPrimary);
                for (int other = envelope + 1; other < params::count; ++other)
                    CHECK(fire::ui::modulationSourceColour(fire::mod_sources::envelopeSource(envelope))
                        != fire::ui::modulationSourceColour(fire::mod_sources::envelopeSource(other)));
            }
            const auto image = controls.createComponentSnapshot(controls.getLocalBounds());
            REQUIRE(image.isValid());
            if (const auto* directory = std::getenv("FIRE_ENVELOPE_PREVIEW_DIR"))
            {
                const auto file = juce::File(directory).getChildFile(juce::String(fire::ui::skinName(skin)).toLowerCase()
                    + "-envelopes-" + juce::String(size.x) + ".png");
                REQUIRE(file.getParentDirectory().createDirectory().wasOk());
                auto stream = file.createOutputStream();
                REQUIRE(stream != nullptr);
                stream->setPosition(0); stream->truncate();
                REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *stream));
            }
        }
}

TEST_CASE("Matrix labels new envelopes independently of the four performance macros",
          "[envelope-controls][ui][matrix]")
{
    FireAudioProcessor processor;
    processor.setMacroDisplayName(0, "Intensity");
    REQUIRE(processor.assignLfoToTarget(fire::mod_sources::envelopeSource(3), "drive1") == LfoManager::AssignmentResult::changed);
    ModulationMatrixPanel matrix(processor);
    matrix.setSize(800, 420);
    auto& menu = control<juce::ComboBox>(matrix, "matrix_source");
    CHECK(menu.getItemText(menu.indexOfItemId(fire::mod_sources::firstMacro + 1)) == "Macro 1 · Intensity");
    for (int envelope = 1; envelope < params::count; ++envelope)
    {
        const int source = fire::mod_sources::envelopeSource(envelope);
        CHECK(menu.getItemText(menu.indexOfItemId(source + 1)) == "Envelope " + juce::String(envelope + 1));
        CHECK(fire::mod_sources::badge(source) == "ENV" + juce::String(envelope + 1));
    }
}
