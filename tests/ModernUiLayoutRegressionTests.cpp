#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <array>

namespace
{
template <typename T, typename Predicate>
T* findControl(juce::Component& parent, Predicate predicate)
{
    if (auto* control = dynamic_cast<T*>(&parent); control != nullptr && predicate(*control))
        return control;
    for (auto* child : parent.getChildren())
        if (auto* control = findControl<T>(*child, predicate))
            return control;
    return nullptr;
}

juce::Button& button(juce::Component& parent, const juce::String& text)
{
    auto* result = findControl<juce::Button>(parent, [&](auto& b) { return b.getButtonText() == text; });
    REQUIRE(result != nullptr);
    return *result;
}

void activate(juce::Button& control)
{
    control.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}

ModulatableSlider& slider(juce::Component& parent, const juce::String& parameter)
{
    auto* result = findControl<ModulatableSlider>(parent, [&](auto& s) { return s.parameterID == parameter; });
    REQUIRE(result != nullptr);
    return *result;
}

void snapshot(juce::Component& component, const juce::String& name)
{
    const auto directory = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
    if (directory.isEmpty())
        return;
    // Snapshot fixtures have no peer. Settle their selection rails just as
    // the editor's clock does when a workspace becomes hidden.
    if (auto* panel = findControl<BandPanel>(component, [](auto&) { return true; }))
        panel->animationTick(1.0f / 60.0f);
    if (auto* panel = findControl<GlobalPanel>(component, [](auto&) { return true; }))
        panel->animationTick(1.0f / 60.0f);
    if (auto* panel = findControl<LfoPanel>(component, [](auto&) { return true; }))
        panel->animationTick(1.0f / 60.0f);
    const auto file = juce::File(directory).getChildFile(name + ".png");
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    stream->setPosition(0);
    stream->truncate();
    juce::PNGImageFormat png;
    CHECK(png.writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), *stream));
}

void checkVisibleControls(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (! child->isVisible())
            continue;
        if (auto* control = dynamic_cast<ModulatableSlider*>(child))
        {
            CAPTURE(control->parameterID);
            CHECK(parent.getLocalBounds().contains(control->getBounds()));
            CHECK_FALSE(control->getValueDisplayBounds().isEmpty());
            CHECK(control->getLocalBounds().contains(control->getValueDisplayBounds()));
            CHECK(control->getValueDisplayBounds().getHeight() >= 16);
        }
        checkVisibleControls(*child);
    }
}
}

TEST_CASE("Rotary titles and value rows remain stable throughout inline editing",
          "[ui][modern][modulatable-slider][layout]")
{
    FireLookAndFeel look;
    ModulatableSlider control;
    control.setLookAndFeel(&look);
    const juce::ScopeGuard resetLook { [&] { control.setLookAndFeel(nullptr); } };
    control.setVisible(true);
    control.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    control.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.setLabel("Output", fire::ui::colours::flame);
    control.setRange(-48.0, 6.0, 0.1);
    control.setTextValueSuffix(" dB");
    control.setValue(-3.0, juce::dontSendNotification);
    control.isModulated = true;
    for (float scale : { 1.0f, 2.0f })
    {
        look.scale = scale;
        control.setBounds(0, 0, juce::roundToInt(76 * scale), juce::roundToInt(96 * scale));
        auto* title = findControl<juce::Label>(control, [](auto& label) { return label.getComponentID() == "parameter_title"; });
        REQUIRE(title != nullptr);
        const auto titleBounds = title->getBounds();
        const auto dialBounds = look.getSliderLayout(control).sliderBounds;
        const auto valueBounds = control.getValueDisplayBounds();
        const auto handle = control.getModulationHandleVisualBounds();
        CHECK(titleBounds.getBottom() <= dialBounds.getY());
        CHECK(valueBounds.getY() >= dialBounds.getBottom());
        CHECK(control.hitTest(valueBounds.getCentreX(), valueBounds.getCentreY()));
        control.mouseEnter(juce::MouseEvent { juce::Desktop::getInstance().getMainMouseSource(),
            dialBounds.getCentre().toFloat(), {}, 0, 0, 0, 0, 0,
            &control, &control, juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 1, false });
        REQUIRE(title->isVisible());
        CHECK(title->getBounds() == titleBounds);
        CHECK(look.getSliderLayout(control).sliderBounds == dialBounds);
        CHECK(look.getSliderLayout(control).textBoxBounds == valueBounds);
        CHECK(control.getModulationHandleVisualBounds() == handle);
        control.showTextBox();
        auto* editor = findControl<juce::TextEditor>(control, [](auto&) { return true; });
        REQUIRE(editor != nullptr);
        control.timerCallback(); // A pending pointer-exit callback cannot end an edit.
        REQUIRE(findControl<juce::TextEditor>(control, [](auto&) { return true; }) == editor);
        editor->setText("-6.5 dB");
        control.hideTextBox(false);
        CHECK(control.getValue() == Catch::Approx(-6.5).margin(1.0e-8));
        control.dismissTransientInteraction();
        CHECK(title->isVisible());
        CHECK(control.getValueDisplayBounds() == valueBounds);
    }
}

TEST_CASE("Workspace output controls align and spectrum collapse restores across workspaces",
          "[ui][modern][layout][spectrum][smoke]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    for (const auto size : { juce::Point<int>(1000, 500), juce::Point<int>(2000, 1000) })
    {
        editor.setSize(size.x, size.y);
        activate(button(editor, "BAND LAB"));
        auto& bandOutput = slider(editor, ParameterIDAndName::getIDString(OUTPUT_ID, 0));
        auto& bandMix = slider(editor, ParameterIDAndName::getIDString(MIX_ID, 0));
        const auto outputBounds = editor.getLocalArea(&bandOutput, bandOutput.getLocalBounds());
        const auto mixBounds = editor.getLocalArea(&bandMix, bandMix.getLocalBounds());
        activate(button(editor, "MASTER LAB"));
        auto& masterOutput = slider(editor, OUTPUT_ID);
        auto& masterMix = slider(editor, MIX_ID);
        CHECK(editor.getLocalArea(&masterOutput, masterOutput.getLocalBounds()) == outputBounds);
        CHECK(editor.getLocalArea(&masterMix, masterMix.getLocalBounds()) == mixBounds);

        activate(button(editor, "MOD FORGE"));
        auto* lfoEditor = findControl<LfoEditor>(editor, [](auto&) { return true; });
        auto* spectrum = findControl<SpectrumBackground>(editor, [](auto&) { return true; });
        REQUIRE(lfoEditor != nullptr);
        REQUIRE(spectrum != nullptr);
        const auto expandedHeight = lfoEditor->getHeight();
        auto& collapse = button(editor, "Hide spectrum");
        activate(collapse);
        CHECK(collapse.getToggleState());
        CHECK_FALSE(spectrum->isVisible());
        CHECK(lfoEditor->getHeight() > expandedHeight + size.y / 6);
        snapshot(editor, "fire-modern-mod-expanded-" + juce::String(size.x));
        activate(button(editor, "MASTER LAB"));
        CHECK(spectrum->isVisible());
        CHECK_FALSE(collapse.isVisible());
        activate(button(editor, "MOD FORGE"));
        CHECK_FALSE(spectrum->isVisible());
        activate(collapse);
        CHECK(spectrum->isVisible());
        CHECK(lfoEditor->getHeight() == expandedHeight);
    }

    editor.setVisible(true);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    auto& collapse = button(editor, "Hide spectrum");
    REQUIRE(static_cast<juce::Component&>(collapse).keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
    CHECK(collapse.getToggleState());
    auto* accessibility = collapse.getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    REQUIRE(accessibility->getActions().invoke(juce::AccessibilityActionType::toggle));
    CHECK_FALSE(collapse.getToggleState());
    auto* zoom = findControl<juce::Button>(editor, [](auto& b) { return b.getComponentID() == "zoom"; });
    REQUIRE(zoom != nullptr);
    activate(*zoom);
    CHECK_FALSE(collapse.isVisible());
    activate(*zoom);
    CHECK(collapse.isVisible());
}

TEST_CASE("Modern control pages render with live parameter and modulation values",
          "[ui][modern][smoke][render]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto* drive = processor.treeState.getParameter(ParameterIDAndName::getIDString(DRIVE_ID, 0));
    REQUIRE(drive != nullptr);
    drive->setValueNotifyingHost(drive->convertTo0to1(72.0f));
    processor.assignLfoToTarget(2, ParameterIDAndName::getIDString(DRIVE_ID, 0));
    processor.setModulationDepth(ParameterIDAndName::getIDString(DRIVE_ID, 0), 0.35f);
    FireAudioProcessorEditor editor(processor);
    for (const int width : { 1000, 2000 })
    {
        editor.setSize(width, width / 2);
        activate(button(editor, "BAND LAB"));
        for (const auto* module : { "Drive", "Shape", "Compressor", "Stereo" })
        {
            activate(button(editor, module));
            checkVisibleControls(editor);
            snapshot(editor, "fire-modern-" + juce::String(module) + "-" + juce::String(width));
        }
        activate(button(editor, "MASTER LAB"));
        for (const auto* module : { "Filter", "Lo-Fi", "Analysis" })
        {
            activate(button(editor, module));
            checkVisibleControls(editor);
            snapshot(editor, "fire-modern-" + juce::String(module) + "-" + juce::String(width));
        }
        activate(button(editor, "MOD FORGE"));
        snapshot(editor, "fire-modern-mod-" + juce::String(width));
    }
}

TEST_CASE("LFO waveform cache follows shape edits resize and display scale",
          "[ui][modern][lfo][render][cache]")
{
    const auto fingerprint = [](const juce::Image& image)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                hash = (hash ^ image.getPixelAt(x, y).getARGB()) * 1099511628211ull;
        return hash;
    };
    LfoEditor reused;
    reused.setSize(360, 180);
    LfoData shape;
    shape.points = { {0.0f, 0.1f}, {0.4f, 0.85f}, {1.0f, 0.1f} };
    shape.curvatures = { 0.3f, -0.5f };
    reused.setDataToDisplay(shape, { 2, 1 });
    const auto initialImage = reused.createComponentSnapshot(reused.getLocalBounds());
    const auto initial = fingerprint(initialImage);
    const auto accent = fire::ui::lfoBankColour(2);
    int accentPixels = 0;
    for (int y = 0; y < initialImage.getHeight(); ++y)
        for (int x = 0; x < initialImage.getWidth(); ++x)
        {
            const auto pixel = initialImage.getPixelAt(x, y);
            const auto distance = std::abs(int(pixel.getRed()) - int(accent.getRed()))
                                + std::abs(int(pixel.getGreen()) - int(accent.getGreen()))
                                + std::abs(int(pixel.getBlue()) - int(accent.getBlue()));
            if (distance < 40)
                ++accentPixels;
        }
    CHECK(accentPixels > 100);
    for (int revision = 2; revision < 5; ++revision)
    {
        shape.points[1].y -= 0.1f;
        shape.curvatures[0] += 0.2f;
        reused.setDataToDisplay(shape, { 2, static_cast<std::uint64_t>(revision) });
        for (int width : { 360, 480 })
        {
            reused.setSize(width, 180);
            for (float displayScale : { 1.0f, 2.0f, 1.0f })
            {
                LfoEditor fresh;
                fresh.setSize(width, 180);
                fresh.setDataToDisplay(shape, { 2, static_cast<std::uint64_t>(revision) });
                const auto render = [&](LfoEditor& editor)
                {
                    return fingerprint(editor.createComponentSnapshot(editor.getLocalBounds(), true, displayScale));
                };
                const auto actual = render(reused);
                CHECK(actual != initial);
                CHECK(actual == render(fresh));
            }
        }
    }
}

TEST_CASE("Selection springs overshoot gently and retain velocity on reversal",
          "[ui][modern][animation][spring]")
{
    fire::ui::SpringValue spring;
    spring.snapTo(0.0f);
    spring.setTarget(1.0f);
    float maximum = 0.0f;
    for (int frame = 0; frame < 120; ++frame)
    {
        spring.advance(1.0f / 60.0f);
        maximum = juce::jmax(maximum, spring.current);
    }
    CHECK(maximum > 1.02f);
    CHECK(maximum < 1.12f);
    CHECK(spring.isSettled());
    CHECK(spring.current == 1.0f);

    spring.setTarget(0.0f);
    for (int frame = 0; frame < 5; ++frame)
        spring.advance(1.0f / 60.0f);
    const auto position = spring.current;
    const auto velocity = spring.velocity;
    spring.setTarget(2.0f);
    CHECK(spring.current == position);
    CHECK(spring.velocity == velocity);
    for (int frame = 0; frame < 150; ++frame)
    {
        spring.advance(frame % 2 == 0 ? 0.012f : 0.026f);
        CHECK(std::isfinite(spring.current));
        CHECK(spring.current > -0.2f);
        CHECK(spring.current < 2.3f);
    }
    CHECK(spring.isSettled());
    CHECK(spring.current == 2.0f);
}

TEST_CASE("Selected controls and menu rows use uninterrupted solid fills",
          "[ui][modern][render][flat-controls]")
{
    FireLookAndFeel look;
    juce::Image image(juce::Image::ARGB, 200, 40, true);
    juce::Graphics graphics(image);
    graphics.fillAll(fire::ui::colours::canvas);
    juce::TextButton control;
    control.setBounds(0, 0, 200, 40);
    control.setToggleState(true, juce::dontSendNotification);
    control.setColour(juce::TextButton::textColourOffId, fire::ui::colours::filter);
    control.setColour(juce::TextButton::textColourOnId, fire::ui::colours::filter);
    look.drawButtonBackground(graphics, control, juce::Colours::transparentBlack, false, false);
    CHECK(image.getPixelAt(1, 20) == fire::ui::colours::raised);
    CHECK(image.getPixelAt(1, 20) == image.getPixelAt(100, 20));

    graphics.fillAll(fire::ui::colours::surface1);
    look.drawPopupMenuItem(graphics, {0, 0, 200, 40}, false, true, true,
                           false, false, "Preset", {}, nullptr, nullptr);
    CHECK(image.getPixelAt(4, 20) == fire::ui::colours::raised.brighter(0.06f));
    CHECK(image.getPixelAt(4, 20) == image.getPixelAt(8, 20));
}
