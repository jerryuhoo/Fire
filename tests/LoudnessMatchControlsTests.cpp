#include <GUI/LoudnessMatchControls.h>
#include <catch2/catch_test_macros.hpp>
#include "helpers/RepaintRecorder.h"

#include <memory>
#include <stdexcept>

namespace
{
using Controls = fire::ui::LoudnessMatchControls;

juce::Button& button(Controls& controls, const juce::String& id)
{
    for (auto* child : controls.getChildren())
        if (auto* result = dynamic_cast<juce::Button*>(child);
            result != nullptr && result->getComponentID() == id)
            return *result;
    FAIL("Missing loudness match button: " << id);
    throw std::logic_error("Missing loudness match button");
}

juce::MouseEvent event(juce::Component& target, juce::ModifierKeys modifiers = {})
{
    const auto now = juce::Time::getCurrentTime();
    const auto point = target.getLocalBounds().toFloat().getCentre();
    return { juce::Desktop::getInstance().getMainMouseSource(), point, modifiers,
             0, 0, 0, 0, 0, &target, &target, now, point, now, 1, false };
}

void show(Controls& controls)
{
    controls.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    controls.setVisible(true);
    REQUIRE(controls.isShowing());
}
} // namespace

TEST_CASE("Loudness match controls distinguish learning and fixed compensation states",
          "[loudness-match][ui][presentation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Controls controls;
    auto& toggle = button(controls, "loudnessMatchToggle");
    auto& learn = button(controls, "loudnessMatchLearn");
    Controls::ViewState state;
    CHECK(toggle.getButtonText() == "Match");
    CHECK_FALSE(toggle.getToggleState());
    CHECK(learn.getButtonText().contains("A"));
    CHECK(learn.getButtonText().contains("Off"));
    CHECK_FALSE(learn.isEnabled());

    state.enabled = true;
    controls.setState(state);
    CHECK(toggle.getToggleState());
    CHECK(learn.isEnabled());
    CHECK(learn.getButtonText().contains("Learn"));

    state.side = 1;
    state.measuring = true;
    state.progress = 0.42f;
    controls.setState(state);
    CHECK(learn.getButtonText().startsWith("B"));
    CHECK(learn.getButtonText().contains("42%"));
    CHECK(learn.getTooltip().containsIgnoreCase("cancel"));

    state.noSignal = true;
    controls.setState(state);
    CHECK(learn.getButtonText().contains("Play audio"));

    state.noSignal = false;
    state.measuring = false;
    state.ready = true;
    state.gainDb = -2.3f;
    controls.setState(state);
    CHECK(learn.getButtonText().contains("-2.3 dB"));
    CHECK(learn.getTooltip().containsIgnoreCase("fixed"));
    CHECK(learn.getTooltip().containsIgnoreCase("same loop"));

    state.gainDb = 18.0f;
    state.limited = true;
    controls.setState(state);
    CHECK(learn.getButtonText().contains("+18.0 dB"));
    CHECK(learn.getButtonText().contains("LIMIT"));
    CHECK(learn.getTooltip().contains("±18 dB"));

    state.bypassed = true;
    controls.setState(state);
    CHECK(learn.getButtonText().contains("Bypassed"));
    CHECK_FALSE(learn.isEnabled());
}

TEST_CASE("Loudness match controls submit visible commands and permit learning cancellation",
          "[loudness-match][ui][interaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Controls controls;
    show(controls);
    auto& toggle = button(controls, "loudnessMatchToggle");
    auto& learn = button(controls, "loudnessMatchLearn");
    Controls::ViewState state;
    int toggles = 0, learns = 0;
    controls.onEnabledChanged = [&](bool enabled)
    {
        ++toggles;
        state.enabled = enabled;
        controls.setState(state);
    };
    controls.onLearn = [&] { ++learns; };

    learn.triggerClick();
    CHECK(learns == 0);
    toggle.triggerClick();
    CHECK(toggles == 1);
    CHECK(toggle.getToggleState());
    learn.triggerClick();
    CHECK(learns == 1);
    state.measuring = true;
    controls.setState(state);
    learn.triggerClick();
    CHECK(learns == 2);
    state.bypassed = true;
    controls.setState(state);
    learn.triggerClick();
    CHECK(learns == 2);
    toggle.triggerClick();
    CHECK(toggles == 2);
    CHECK_FALSE(toggle.getToggleState());
}

TEST_CASE("Loudness match controls reject hidden disabled and dismissed pointer sessions",
          "[loudness-match][ui][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int boundary = 0; boundary < 5; ++boundary)
    {
        DYNAMIC_SECTION("boundary " << boundary)
        {
            Controls controls;
            show(controls);
            Controls::ViewState state;
            state.enabled = true;
            controls.setState(state);
            auto& toggle = button(controls, "loudnessMatchToggle");
            auto& learn = button(controls, "loudnessMatchLearn");
            int calls = 0;
            controls.onEnabledChanged = [&](bool) { ++calls; };
            controls.onLearn = [&] { ++calls; };

            static_cast<juce::Component&>(learn).mouseDown(
                event(learn, juce::ModifierKeys::leftButtonModifier));
            if (boundary == 0)
                controls.setVisible(false);
            else if (boundary == 1)
                controls.setEnabled(false);
            else if (boundary == 2)
            {
                controls.removeFromDesktop();
                controls.dismiss();
            }
            else if (boundary == 3)
                controls.dismiss();
            else
            {
                state.side = 1;
                controls.setState(state);
            }

            if (boundary < 3)
            {
                toggle.triggerClick();
                learn.triggerClick();
                CHECK(calls == 0);
            }

            if (boundary == 0)
                controls.setVisible(true);
            else if (boundary == 1)
                controls.setEnabled(true);
            else if (boundary == 2)
                show(controls);
            static_cast<juce::Component&>(learn).mouseUp(event(learn));
            CHECK(calls == 0);
            learn.triggerClick();
            CHECK(calls == 1);
        }
    }
}

TEST_CASE("Loudness match callbacks tolerate synchronous owner deletion",
          "[loudness-match][ui][callback-safety]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (bool useLearn : { false, true })
    {
        auto controls = std::make_unique<Controls>();
        show(*controls);
        Controls::ViewState state;
        state.enabled = useLearn;
        controls->setState(state);
        int calls = 0;
        controls->onEnabledChanged = [&](bool) { ++calls; controls.reset(); };
        controls->onLearn = [&] { ++calls; controls.reset(); };
        auto& target = button(*controls, useLearn ? "loudnessMatchLearn" : "loudnessMatchToggle");
        auto staleClick = target.onClick;
        target.triggerClick();
        CHECK(controls == nullptr);
        CHECK(calls == 1);
        staleClick();
        CHECK(calls == 1);
    }
}

TEST_CASE("Loudness match controls keep narrow layouts separate and unchanged frames quiet",
          "[loudness-match][ui][layout][repaint]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Controls controls;
    show(controls);
    auto& toggle = button(controls, "loudnessMatchToggle");
    auto& learn = button(controls, "loudnessMatchLearn");
    for (int width : { 1, 8, 48, 96, 150, 164, 170, 328 })
    {
        for (int height : { 16, 32, 64 })
        {
            controls.setSize(width, height);
            CHECK_FALSE(toggle.getBounds().intersects(learn.getBounds()));
            CHECK(toggle.getX() >= 0);
            CHECK(learn.getRight() <= width);
            CHECK(toggle.getBottom() <= height);
            CHECK(learn.getBottom() <= height);
        }
    }

    controls.setSize(164, 32);
    Controls::ViewState state;
    state.enabled = true;
    state.measuring = true;
    state.progress = 0.25f;
    controls.setState(state);
    auto* recorder = new RepaintRecorder(controls);
    controls.setCachedComponentImage(recorder);
    recorder->clear();
    for (int frame = 0; frame < 60; ++frame)
        controls.setState(state);
    CHECK(recorder->dirtyAreas.isEmpty());
    state.progress = 0.75f;
    controls.setState(state);
    CHECK_FALSE(recorder->dirtyAreas.isEmpty());
    CHECK(learn.getButtonText().contains("75%"));
}
