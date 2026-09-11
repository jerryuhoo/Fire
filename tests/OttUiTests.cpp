#include <PluginEditor.h>
#include <Panels/SpectrogramPanel/OttBandControls.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace {
template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate predicate)
{
    if (auto* component = dynamic_cast<T*>(&root); component && predicate(*component)) return component;
    for (auto* child : root.getChildren()) if (auto* result = find<T>(*child, predicate)) return result;
    return nullptr;
}
void click(juce::Component& root, const juce::String& text)
{
    auto* b = find<juce::Button>(root, [&](auto& button) { return button.getButtonText() == text; });
    REQUIRE(b != nullptr);
    b->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
}
void set(FireAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> position, juce::ModifierKeys mods = {}, juce::Point<float> origin = {}, bool drag = false)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(), position, mods,
        0, 0, 0, 0, 0, &c, &c, now, origin, now, 1, drag};
}
struct Gestures : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool start) override { if (start) ++begins; else ++ends; }
};
void warmMeters(FireAudioProcessor& p, BandPanel& panel, Multiband& multiband)
{
    p.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 160; ++block)
    {
        for (int sample = 0; sample < 256; ++sample)
        {
            const auto t = static_cast<float>(block * 256 + sample) / 48000.0f;
            const auto input = 0.0005f * (std::sin(juce::MathConstants<float>::twoPi * 80 * t)
                + std::sin(juce::MathConstants<float>::twoPi * 700 * t)
                + std::sin(juce::MathConstants<float>::twoPi * 5000 * t));
            buffer.setSample(0, sample, input);
            buffer.setSample(1, sample, input * 0.8f);
        }
        p.processBlock(buffer, midi);
    }
    MeterValues values;
    REQUIRE(p.getLatestMeterValues(values));
    CHECK(values.ottInputLevelDb[0] > -100.0f);
    CHECK(values.ottGainChangeDb[0] > 0.0f);
    panel.presentMeterValues(values, 10000);
    multiband.presentOttMeters(values, 10000);
}
void save(juce::Component& component, const juce::String& name)
{
    const auto dir = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
    if (dir.isEmpty()) return;
    auto stream = juce::File(dir).getChildFile(name).createOutputStream();
    REQUIRE(stream != nullptr);
    stream->setPosition(0); stream->truncate();
    CHECK(juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), *stream));
}
}

TEST_CASE("OTT spectrum handles map drags to dB and close their host gestures", "[ott][ui][input]")
{
    FireAudioProcessor p;
    FireLookAndFeel look;
    OttBandControls controls(p, 0);
    controls.setLookAndFeel(&look);
    controls.setBounds(0, 0, 600, 200);
    controls.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    controls.setVisible(true);
    auto& slider = controls.up;
    auto* parameter = p.treeState.getParameter("ottUpward1");
    REQUIRE(parameter != nullptr);
    Gestures gestures;
    parameter->addListener(&gestures);
    const juce::ScopeGuard cleanup { [&] { controls.dismiss(); parameter->removeListener(&gestures); controls.setLookAndFeel(nullptr); } };
    auto point = juce::Point<float>(150, slider.lineY());
    CHECK(slider.hitTest(150, juce::roundToInt(point.y)));
    CHECK_FALSE(slider.hitTest(2, juce::roundToInt(point.y)));
    slider.mouseDown(event(slider, point, juce::ModifierKeys::leftButtonModifier, point));
    REQUIRE(slider.hasActivePointerGesture());
    CHECK(gestures.begins == 1);
    slider.mouseDrag(event(slider, point.translated(0, -20), juce::ModifierKeys::leftButtonModifier, point, true));
    CHECK(slider.getValue() == Catch::Approx(-38.0).margin(0.6));
    slider.mouseUp(event(slider, point.translated(0, -20), {}, point, true));
    CHECK(gestures.ends == 1);
    const auto value = slider.getValue();
    slider.mouseDown(event(slider, point, juce::ModifierKeys::rightButtonModifier, point));
    slider.mouseDrag(event(slider, point.translated(0, 30), juce::ModifierKeys::rightButtonModifier, point, true));
    CHECK(slider.getValue() == value);
    CHECK(gestures.begins == 1);
    point.y = slider.lineY();
    slider.mouseDown(event(slider, point, juce::ModifierKeys::leftButtonModifier, point));
    controls.setVisible(false);
    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(gestures.begins == gestures.ends);
}

TEST_CASE("OTT view follows bands and releases spectrum edits at workspace and topology boundaries", "[ott][ui][layout][lifecycle]")
{
    FireAudioProcessor p;
    p.hasUpdateCheckBeenPerformed = true;
    set(p, NUM_BANDS_ID, 3);
    set(p, "freq1", 200); set(p, "freq2", 2000);
    set(p, "lineState1", 1); set(p, "lineState2", 1);
    FireAudioProcessorEditor editor(p);
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    click(editor, "OTT");
    auto* panel = find<BandPanel>(editor, [](auto&) { return true; });
    auto* multiband = find<Multiband>(editor, [](auto&) { return true; });
    REQUIRE(panel != nullptr); REQUIRE(multiband != nullptr);
    CHECK(panel->isOttSelected());
    for (int band = 1; band <= 3; ++band)
        set(p, juce::String(OTT_ENABLED_ID) + juce::String(band), 1);
    set(p, "ottUpward1", -54);
    set(p, "ottUpward3", -60);
    warmMeters(p, *panel, *multiband);
    for (int frame = 0; frame < 75; ++frame) panel->animationTick(1.0f / 60.0f);
    save(editor, "fire-ott-three-bands.png");
    auto* threshold = find<OttBandControls::ThresholdSlider>(editor, [](auto& slider) { return slider.getComponentID() == "ottUpward2"; });
    REQUIRE(threshold != nullptr);
    REQUIRE(threshold->isShowing());
    const auto point = juce::Point<float>(threshold->getWidth() * 0.3f, threshold->lineY());
    CHECK(editor.getComponentAt(editor.getLocalPoint(threshold, point.toInt())) == threshold);
    threshold->mouseDown(event(*threshold, point, juce::ModifierKeys::leftButtonModifier, point));
    REQUIRE(threshold->hasActivePointerGesture());
    CHECK(panel->getFocusBandNum() == 1);
    click(editor, "MASTER LAB");
    CHECK_FALSE(threshold->hasActivePointerGesture());
    CHECK_FALSE(threshold->isShowing());
    click(editor, "BAND LAB");
    CHECK(threshold->isShowing());
    threshold->mouseDown(event(*threshold, point, juce::ModifierKeys::leftButtonModifier, point));
    set(p, NUM_BANDS_ID, 2);
    multiband->synchroniseBandCountFromParameter();
    CHECK_FALSE(threshold->hasActivePointerGesture());
    CHECK(panel->isOttSelected());
}

TEST_CASE("OTT knobs and spectrum thresholds share constraints and render at supported scales", "[ott][ui][render]")
{
    FireAudioProcessor p;
    p.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(p);
    editor.setSize(1000, 500);
    click(editor, "OTT");
    auto* up = find<ModulatableSlider>(editor, [](auto& slider) { return slider.parameterID == "ottUpward1"; });
    REQUIRE(up != nullptr);
    up->setValue(-6.0, juce::sendNotificationSync);
    CHECK(up->getValue() == Catch::Approx(-24.0).margin(0.1));
    CHECK(p.treeState.getRawParameterValue("ottUpward1")->load() == Catch::Approx(-24.0).margin(0.1));
    set(p, "ottUpward1", -48);
    set(p, "ottEnabled1", 1);
    for (int width : {1000, 2000})
    {
        editor.setSize(width, width / 2);
        auto* panel = find<BandPanel>(editor, [](auto&) { return true; });
        REQUIRE(panel != nullptr);
        panel->animationTick(1.0f / 60.0f);
        for (auto* name : ParameterIDAndName::ottControlIDs)
        {
            auto* control = find<ModulatableSlider>(*panel, [&](auto& slider) { return slider.parameterID == juce::String(name) + "1"; });
            REQUIRE(control != nullptr);
            CHECK(control->isVisible());
            CHECK(panel->getLocalBounds().contains(control->getBounds()));
        }
        save(editor, "fire-ott-" + juce::String(width) + ".png");
    }
}

TEST_CASE("OTT threshold displays follow effective automation without rewriting raw parameters", "[ott][ui][automation]")
{
    FireAudioProcessor p;
    OttBandControls controls(p, 0);
    controls.setSize(600, 200);
    set(p, "ottUpward1", -6);
    set(p, "ottDownward1", -36);
    controls.refresh();
    CHECK(controls.up.getValue() == Catch::Approx(-42));
    CHECK(controls.down.getValue() == Catch::Approx(-36));
    CHECK(p.treeState.getRawParameterValue("ottUpward1")->load() == Catch::Approx(-6));
    set(p, "ottDownward1", -18);
    controls.refresh();
    CHECK(controls.up.getValue() == Catch::Approx(-24));
    CHECK(controls.down.getValue() == Catch::Approx(-18));
    CHECK(p.treeState.getRawParameterValue("ottUpward1")->load() == Catch::Approx(-6));
}
