#include <PluginEditor.h>
#include <Panels/SpectrogramPanel/OttBandControls.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>

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

TEST_CASE("OTT spectrum readouts fade only for an edit and retain line-only hit targets", "[ott][ui][input][motion]")
{
    FireAudioProcessor p;
    FireLookAndFeel look;
    OttBandControls controls(p, 0);
    controls.setLookAndFeel(&look);
    const juce::ScopeGuard cleanup { [&] { controls.dismiss(); controls.setLookAndFeel(nullptr); } };
    controls.setSize(600, 200);
    controls.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    controls.setVisible(true);
    auto& slider = controls.up;
    const auto point = juce::Point<float>(150, slider.lineY());
    slider.mouseEnter(event(slider, point));
    for (int frame = 0; frame < 30; ++frame) controls.refresh();
    CHECK(slider.getReadoutOpacity() == 0.0f);
    slider.mouseDown(event(slider, point, juce::ModifierKeys::leftButtonModifier, point));
    controls.refresh();
    CHECK(slider.getReadoutOpacity() > 0.0f);
    CHECK(slider.getReadoutOpacity() < 0.5f);
    for (int frame = 0; frame < 20; ++frame) controls.refresh();
    const auto draggingOpacity = slider.getReadoutOpacity();
    CHECK(draggingOpacity > 0.95f);
    CHECK_FALSE(slider.hitTest(150, juce::roundToInt(point.y + 15)));
    slider.mouseUp(event(slider, point, {}, point));
    controls.refresh();
    CHECK(slider.getReadoutOpacity() > 0.0f);
    CHECK(slider.getReadoutOpacity() < draggingOpacity);
    for (int frame = 0; frame < 60; ++frame) controls.refresh();
    CHECK(slider.getReadoutOpacity() == 0.0f);
    // There is no former circular handle extending above or below the line.
    const auto line = slider.createComponentSnapshot(slider.getLocalBounds());
    CHECK(line.getPixelAt(150, juce::roundToInt(slider.lineY()) + 3).getAlpha() == 0);
}

TEST_CASE("OTT knob values stay hidden on hover and fade around a drag", "[ott][ui][input][motion]")
{
    FireAudioProcessor p;
    p.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(p);
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    click(editor, "OTT");
    editor.stopTimer();
    auto* knob = find<ModulatableSlider>(editor, [](auto& s) { return s.parameterID == "ottDepth1"; });
    REQUIRE(knob != nullptr);
    const auto point = knob->getLocalBounds().toFloat().getCentre();
    knob->mouseEnter(event(*knob, point));
    for (int frame = 0; frame < 30; ++frame) knob->advanceAnimation(1.0f / 60.0f);
    CHECK(knob->getValueReadoutOpacity() == 0.0f);
    knob->mouseDown(event(*knob, point, juce::ModifierKeys::leftButtonModifier, point));
    REQUIRE(knob->hasActiveInteraction());
    knob->advanceAnimation(1.0f / 60.0f);
    CHECK(knob->getValueReadoutOpacity() > 0.0f);
    CHECK(knob->getValueReadoutOpacity() < 0.5f);
    for (int frame = 0; frame < 20; ++frame) knob->advanceAnimation(1.0f / 60.0f);
    CHECK(knob->getValueReadoutOpacity() > 0.95f);
    knob->mouseUp(event(*knob, point, {}, point));
    knob->advanceAnimation(1.0f / 60.0f);
    CHECK(knob->getValueReadoutOpacity() > 0.0f);
    for (int frame = 0; frame < 60; ++frame) knob->advanceAnimation(1.0f / 60.0f);
    CHECK(knob->getValueReadoutOpacity() == 0.0f);
    auto* valueLabel = find<juce::Label>(*knob, [](auto& label) { return label.isEditable(); });
    REQUIRE(valueLabel != nullptr);
    CHECK(valueLabel->getAlpha() == 0.0f);
    valueLabel->showEditor();
    knob->advanceAnimation(1.0f / 60.0f);
    CHECK(valueLabel->isBeingEdited());
    CHECK(valueLabel->getAlpha() == 1.0f);
    valueLabel->hideEditor(true);
    knob->dismissTransientInteraction();
    knob->setInteractionOnlyReadout(false);
    CHECK(knob->getValueReadoutOpacity() == 1.0f);
}

TEST_CASE("OTT spectral motion follows actual narrow tones and clears invalid or stale input", "[ott][ui][spectrum]")
{
    FireAudioProcessor p;
    p.hasUpdateCheckBeenPerformed = true;
    set(p, NUM_BANDS_ID, 1);
    FireAudioProcessorEditor editor(p);
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    click(editor, "OTT");
    editor.stopTimer();
    auto* multiband = find<Multiband>(editor, [](auto&) { return true; });
    REQUIRE(multiband != nullptr);
    std::array<float, 1024> spectrum {};
    const auto feed = [&] {
        for (int frame = 0; frame < 30; ++frame)
        {
            multiband->updateOttSpectrum(spectrum.data(), 1024, 48000.0f / 2048.0f);
            multiband->animationTick(1.0f / 60.0f);
        }
    };
    spectrum[20] = 128;
    feed();
    const auto low = multiband->getOttSpectrum(0);
    const auto lowPeak = std::max_element(low.begin(), low.end());
    const auto lowCentroid = multiband->getOttCentroid(0);
    CHECK(*lowPeak > 0.3f);
    spectrum.fill(0); spectrum[200] = 128;
    feed();
    const auto high = multiband->getOttSpectrum(0);
    const auto highPeak = std::max_element(high.begin(), high.end());
    CHECK(*highPeak > 0.3f);
    CHECK(std::distance(high.begin(), highPeak) > std::distance(low.begin(), lowPeak) + 5);
    CHECK(multiband->getOttCentroid(0) > lowCentroid + 0.2f);
    spectrum.fill(std::numeric_limits<float>::quiet_NaN());
    for (int frame = 0; frame < 120; ++frame)
    {
        multiband->updateOttSpectrum(spectrum.data(), 1024, 48000.0f / 2048.0f);
        multiband->animationTick(1.0f / 60.0f);
    }
    for (auto energy : multiband->getOttSpectrum(0)) CHECK(energy == 0.0f);
    spectrum.fill(0); spectrum[200] = 128;
    feed();
    juce::Thread::sleep(270);
    for (int frame = 0; frame < 120; ++frame) multiband->animationTick(1.0f / 60.0f);
    for (auto energy : multiband->getOttSpectrum(0)) CHECK(energy == 0.0f);
    feed();
    multiband->clearOttSpectrum();
    for (auto energy : multiband->getOttSpectrum(0)) CHECK(energy == 0.0f);
}

TEST_CASE("OTT ribbons distinguish lift and compression without animating silence", "[ott][ui][motion][render]")
{
    fire::ui::OttRippleMotion motion;
    for (int frame = 0; frame < 60; ++frame) motion.advance(1.0f / 60.0f, 0, false, false, 0.4f);
    CHECK(motion.phase == 0.0f);
    CHECK(motion.lift == 0.0f); CHECK(motion.press == 0.0f);
    for (int frame = 0; frame < 30; ++frame) motion.advance(1.0f / 60.0f, 12, false, false, 0.4f);
    CHECK(motion.lift > 0.7f); CHECK(motion.press == 0.0f);
    for (int frame = 0; frame < 90; ++frame) motion.advance(1.0f / 60.0f, -12, false, false, 0.4f);
    CHECK(motion.lift == 0.0f); CHECK(motion.press > 0.7f);
    for (int frame = 0; frame < 90; ++frame) motion.advance(1.0f / 60.0f, 0, false, false, 0.4f);
    CHECK(motion.lift == 0.0f); CHECK(motion.press == 0.0f);
    motion.advance(0.05f, 0, true, false, 0.4f);
    const auto preview = motion.liftPreview;
    motion.advance(0.05f, 0, false, false, 0.4f);
    CHECK(motion.liftPreview > 0.0f); CHECK(motion.liftPreview < preview);

    fire::ui::OttSpectrumProfile profile {};
    std::fill(profile.begin() + 10, profile.begin() + 15, 0.9f);
    const auto render = [&](bool downward) {
        juce::Image img(juce::Image::ARGB, 240, 100, true);
        juce::Graphics g(img);
        fire::ui::drawOttRipples(g, img.getBounds().toFloat(), 50, profile, 1.0f, downward, 0.25f, 1.0f);
        return img;
    };
    const auto upward = render(false), downward = render(true);
    CHECK(upward.getPixelAt(120, 44).getAlpha() > 0);
    CHECK(upward.getPixelAt(120, 56).getAlpha() == 0);
    CHECK(downward.getPixelAt(120, 56).getAlpha() > 0);
    CHECK(downward.getPixelAt(120, 44).getAlpha() == 0);
    CHECK(upward.getPixelAt(20, 44).getAlpha() == 0);
    CHECK(fire::ui::colours::ott != fire::ui::colourForRole(fire::ui::ModuleRole::compressor));
}

TEST_CASE("OTT editor receives the live audio FFT profile", "[ott][ui][spectrum][integration]")
{
    FireAudioProcessor p;
    p.hasUpdateCheckBeenPerformed = true;
    set(p, NUM_BANDS_ID, 1);
    set(p, "ottEnabled1", 1);
    p.setRateAndBufferSizeDetails(48000.0, 256);
    p.prepareToPlay(48000.0, 256);
    FireAudioProcessorEditor editor(p);
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    click(editor, "OTT");
    editor.stopTimer();
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 100; ++block)
    {
        for (int sample = 0; sample < 256; ++sample)
        {
            const auto value = 0.1f * std::sin(juce::MathConstants<float>::twoPi * 1000.0f
                * static_cast<float>(block * 256 + sample) / 48000.0f);
            audio.setSample(0, sample, value); audio.setSample(1, sample, value);
        }
        p.processBlock(audio, midi);
        if (block % 8 == 7) editor.timerCallback();
    }
    auto* multiband = find<Multiband>(editor, [](auto&) { return true; });
    REQUIRE(multiband != nullptr);
    const auto profile = multiband->getOttSpectrum(0);
    CHECK(*std::max_element(profile.begin(), profile.end()) > 0.01f);
    CHECK(multiband->getOttCentroid(0) > 0.4f);
    CHECK(multiband->getOttCentroid(0) < 0.8f);
}
