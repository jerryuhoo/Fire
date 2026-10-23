#include <PluginEditor.h>
#include <GUI/EffectRackNavigation.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>
#include <memory>

struct InsertOttGraphTestAccess
{
    static float input(const OttGraph& graph) { return graph.input; }
    static float gain(const OttGraph& graph) { return graph.gain; }
    static float activity(const OttGraph& graph) { return graph.activity; }
    static float shownInput(const OttGraph& graph) { return graph.shownInput; }
    static float shownGain(const OttGraph& graph) { return graph.shownGain; }
    static const auto& spectrum(const OttGraph& graph) { return graph.spectrum; }
    static const auto& motion(const OttGraph& graph) { return graph.motion; }
};

namespace
{
using Access = InsertOttGraphTestAccess;
namespace fx = fire::effects;

template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate predicate)
{
    if (auto* component = dynamic_cast<T*>(&root); component && predicate(*component)) return component;
    for (auto* child : root.getChildren())
        if (auto* result = find<T>(*child, predicate)) return result;
    return nullptr;
}

template <typename T>
T& component(juce::Component& root)
{
    auto* result = find<T>(root, [](auto&) { return true; });
    REQUIRE(result != nullptr);
    return *result;
}

void set(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void control(FireAudioProcessor& processor, int scope, int slot, int index, float value)
{
    const auto normalised = fx::controls(fx::Type::ott)[static_cast<size_t>(index)].toNormalised(value);
    set(processor, fx::parameterID(scope, slot, index), normalised);
}

void click(juce::Component& root, const juce::String& text)
{
    auto* button = find<juce::Button>(root, [&](auto& candidate) { return candidate.getButtonText() == text; });
    REQUIRE(button != nullptr);
    button->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
}

void zoom(GraphTemplate& graph)
{
    auto* accessibility = graph.getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    REQUIRE(accessibility->getActions().invoke(juce::AccessibilityActionType::press));
}

void chooseGraph(BandPanel& panel, int choice)
{
    auto* selector = find<ContextAwareComboBox>(panel, [](auto& candidate)
    { return candidate.getComponentID() == "band_graph_view"; });
    REQUIRE(selector != nullptr);
    for (int attempt = 0; selector->getSelectedId() != choice && attempt < 5; ++attempt)
        REQUIRE(static_cast<juce::Component&>(*selector).keyPressed(juce::KeyPress {
            selector->getSelectedId() < choice ? juce::KeyPress::rightKey : juce::KeyPress::leftKey }));
    REQUIRE(selector->getSelectedId() == choice);
}

void preview(juce::Component& root, const juce::String& name)
{
    const auto directory = juce::SystemStats::getEnvironmentVariable("FIRE_INSERT_OTT_PREVIEW_DIR", {});
    if (directory.isEmpty()) return;
    const auto file = juce::File(directory).getChildFile(name + ".png");
    REQUIRE(file.getParentDirectory().createDirectory());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    stream->setPosition(0);
    stream->truncate();
    CHECK(juce::PNGImageFormat().writeImageToStream(root.createComponentSnapshot(root.getLocalBounds()), *stream));
}

struct Fixture
{
    FireAudioProcessor processor;
    std::unique_ptr<FireAudioProcessorEditor> editor;
    BandPanel* band = nullptr;
    GlobalPanel* master = nullptr;
    std::uint64_t audioSample = 0;

    Fixture()
    {
        processor.hasUpdateCheckBeenPerformed = true;
        set(processor, NUM_BANDS_ID, 2);
        set(processor, "lineState1", 1);
        set(processor, "freq1", 1000);
        for (int scope : {0, 1, 2})
            for (int slot = 0; slot < 2; ++slot)
                REQUIRE(processor.addInsertEffect(scope, fx::Type::ott) == slot);
        editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setSize(1000, 500);
        editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor->setVisible(true);
        band = &component<BandPanel>(*editor);
        master = &component<GlobalPanel>(*editor);
    }

    juce::Component& select(int scope, int slot)
    {
        click(*editor, scope == 0 ? "MASTER LAB" : "BAND LAB");
        if (scope == 0)
        {
            master->focusInsertEffect(slot);
            master->animationTick(1.0f / 60.0f);
            return *master;
        }
        band->setFocusBandNum(scope - 1);
        auto& rail = component<fire::ui::EffectRackNavigation>(*band);
        REQUIRE(rail.onSelectEffect);
        rail.onSelectEffect(slot);
        band->animationTick(1.0f / 60.0f);
        return *band;
    }

    void tick(int scope, int count = 1)
    {
        for (int i = 0; i < count; ++i)
            if (scope == 0) master->animationTick(1.0f / 60.0f);
            else band->animationTick(1.0f / 60.0f);
    }

    void audio(int blocks = 160)
    {
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        for (int block = 0; block < blocks; ++block)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample, ++audioSample)
            {
                const auto seconds = static_cast<double>(audioSample) / 48000.0;
                const auto input = 0.0005f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 130.0 * seconds)
                    + std::sin(juce::MathConstants<double>::twoPi * 3100.0 * seconds));
                buffer.setSample(0, sample, input);
                buffer.setSample(1, sample, input * 0.8f);
            }
            processor.processBlock(buffer, midi);
        }
    }

    ModulatableSlider& knob(juce::Component& panel, int scope, int slot, int index)
    {
        const auto id = fx::parameterID(scope, slot, index);
        auto* result = find<ModulatableSlider>(panel, [&](auto& candidate) { return candidate.getParamID() == id; });
        REQUIRE(result != nullptr);
        return *result;
    }
};

void checkSilent(const OttGraph& graph)
{
    CHECK(Access::input(graph) <= -119.0f);
    CHECK(Access::gain(graph) == 0.0f);
    CHECK(Access::activity(graph) == 0.0f);
}

void seedGraph(OttGraph& graph)
{
    fire::ui::OttSpectrumProfile spectrum;
    spectrum.fill(0.8f);
    graph.setSpectrum(spectrum, 0.8f);
    graph.setLevels(-30.0f, 8.0f, 8.0f);
    for (int i = 0; i < 30; ++i) graph.advanceVisuals(1.0f / 60.0f, false, 0);
    REQUIRE(Access::motion(graph).lift > 0.1f);
}
}

TEST_CASE("Master and Band inserted OTT automatically use the shared dynamics graph", "[insert-ott-ui][ui][graph][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture;
    for (int scope : {0, 1, 2})
    {
        CAPTURE(scope);
        auto& panel = fixture.select(scope, 0);
        auto& ott = component<OttGraph>(panel);
        CHECK(ott.isShowing());
        CHECK_FALSE(component<Oscilloscope>(panel).isShowing());
        CHECK_FALSE(component<VUPanel>(panel).isShowing());
        CHECK_FALSE(component<WidthGraph>(panel).isShowing());
    }
    fixture.select(1, 0);
    chooseGraph(*fixture.band, 2);
    CHECK(component<Oscilloscope>(*fixture.band).isShowing());
    CHECK_FALSE(component<OttGraph>(*fixture.band).isShowing());
    chooseGraph(*fixture.band, 3);
    CHECK(component<DistortionGraph>(*fixture.band).isShowing());
    chooseGraph(*fixture.band, 4);
    CHECK(component<VUPanel>(*fixture.band).isShowing());
    chooseGraph(*fixture.band, 5);
    CHECK(component<WidthGraph>(*fixture.band).isShowing());
    chooseGraph(*fixture.band, 1);
    CHECK(component<OttGraph>(*fixture.band).isShowing());
}

TEST_CASE("Inserted OTT controls use native Band knob order at both supported scales and every skin", "[insert-ott-ui][ui][layout][skin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture;
    const auto stateBefore = fixture.processor.treeState.copyState().toXmlString();
    const auto originalSkin = fixture.editor->getSkinPreference();
    const juce::ScopeGuard restoreSkin { [&] { fixture.editor->setSkinPreference(originalSkin); } };
    for (const auto skin : fire::ui::skins)
    {
        fixture.editor->setSkinPreference(skin);
        for (const int width : {1000, 2000})
        {
            fixture.editor->setSize(width, width / 2);
            for (const int scope : {0, 1})
            {
                CAPTURE(static_cast<int>(skin), width, scope);
                auto& panel = fixture.select(scope, 0);
                auto& graph = component<OttGraph>(panel);
                REQUIRE(graph.isShowing());
                const auto graphArea = fixture.editor->getLocalArea(&graph, graph.getLocalBounds());
                CHECK(fixture.editor->getLocalBounds().contains(graphArea));
                std::array<juce::Rectangle<int>, 6> cells;
                for (int i = 0; i < 6; ++i)
                {
                    auto& knob = fixture.knob(panel, scope, 0, i);
                    REQUIRE(knob.isShowing());
                    cells[static_cast<size_t>(i)] = fixture.editor->getLocalArea(&knob, knob.getLocalBounds());
                    CHECK(fixture.editor->getLocalBounds().contains(cells[static_cast<size_t>(i)]));
                    CHECK_FALSE(cells[static_cast<size_t>(i)].intersects(graphArea));
                }
                // Depth / Time / Mix above Upward / Downward / Output.
                CHECK(cells[0].getY() == cells[1].getY());
                CHECK(cells[1].getY() == cells[5].getY());
                CHECK(cells[2].getY() == cells[3].getY());
                CHECK(cells[3].getY() == cells[4].getY());
                CHECK(cells[0].getBottom() <= cells[2].getY());
                CHECK(cells[0].getX() == cells[2].getX());
                CHECK(cells[1].getX() == cells[3].getX());
                CHECK(cells[5].getX() == cells[4].getX());
                CHECK(cells[0].getRight() <= cells[1].getX());
                CHECK(cells[1].getRight() <= cells[5].getX());
                seedGraph(graph);
                preview(*fixture.editor, juce::String(fire::ui::skinName(skin)).toLowerCase()
                    + (scope == 0 ? "-master-ott-" : "-band-insert-ott-") + juce::String(width));
            }
        }
    }
    CHECK(fixture.processor.treeState.copyState().toXmlString() == stateBefore);
}

TEST_CASE("Inserted OTT graph zoom hides all six controls and restores them when collapsed", "[insert-ott-ui][ui][zoom][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture;
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        auto& panel = fixture.select(scope, 0);
        auto& graph = component<OttGraph>(panel);
        const auto unzoomedBounds = graph.getBounds();
        zoom(graph);
        REQUIRE(graph.getZoomState());
        CHECK(graph.getWidth() > unzoomedBounds.getWidth());
        for (int i = 0; i < 6; ++i) CHECK_FALSE(fixture.knob(panel, scope, 0, i).isShowing());
        zoom(graph);
        CHECK_FALSE(graph.getZoomState());
        CHECK(graph.getBounds() == unzoomedBounds);
        for (int i = 0; i < 6; ++i) CHECK(fixture.knob(panel, scope, 0, i).isShowing());
        zoom(graph);
        fixture.select(scope, 1);
        CHECK_FALSE(graph.getZoomState());
        for (int i = 0; i < 6; ++i) CHECK(fixture.knob(panel, scope, 1, i).isShowing());
    }
}

TEST_CASE("Inserted OTT graphs read their own slot rather than native Band meter packets", "[insert-ott-ui][ui][telemetry][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture;
    for (int scope : {0, 1})
    {
        control(fixture.processor, scope, 0, 0, 100);
        control(fixture.processor, scope, 0, 2, -30);
        control(fixture.processor, scope, 1, 0, 0);
        control(fixture.processor, scope, 1, 4, -9);
    }
    fixture.processor.prepareToPlay(48000.0, 256);
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        auto& panel = fixture.select(scope, 0);
        fixture.audio();
        fixture.tick(scope, 30);
        auto& graph = component<OttGraph>(panel);
        auto first = fixture.processor.getInsertOttVisualState(scope, 0);
        REQUIRE(first.valid);
        REQUIRE(first.inputLevelDb > -100.0f);
        REQUIRE(first.dynamicsActivityDb > 0.1f);
        CHECK(Access::input(graph) == Catch::Approx(first.inputLevelDb));
        CHECK(Access::gain(graph) == Catch::Approx(first.gainChangeDb));
        CHECK(Access::activity(graph) == Catch::Approx(first.dynamicsActivityDb));

        MeterValues unrelatedNative;
        unrelatedNative.ottInputLevelDb.fill(-1.0f);
        unrelatedNative.ottGainChangeDb.fill(-39.0f);
        unrelatedNative.ottDynamicsActivityDb.fill(-29.0f);
        fixture.band->presentMeterValues(unrelatedNative, 100000 + scope);
        fixture.tick(scope);
        CHECK(Access::input(graph) == Catch::Approx(first.inputLevelDb));
        CHECK(Access::gain(graph) == Catch::Approx(first.gainChangeDb));

        fixture.select(scope, 1);
        fixture.audio(30);
        fixture.tick(scope, 30);
        const auto second = fixture.processor.getInsertOttVisualState(scope, 1);
        REQUIRE(second.valid);
        CHECK(Access::gain(graph) == Catch::Approx(second.gainChangeDb));
        CHECK(Access::activity(graph) == Catch::Approx(second.dynamicsActivityDb));
        CHECK(second.dynamicsActivityDb == Catch::Approx(0.0f).margin(0.001f));
        CHECK(std::abs(second.gainChangeDb - first.gainChangeDb) > 1.0f);
    }
}

TEST_CASE("Inserted OTT stops displaying stale gain after bypass zero Mix or suspended audio", "[insert-ott-ui][ui][telemetry][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture;
    fixture.processor.prepareToPlay(48000.0, 256);
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        auto& panel = fixture.select(scope, 0);
        auto& graph = component<OttGraph>(panel);
        control(fixture.processor, scope, 0, 0, 100);
        control(fixture.processor, scope, 0, 2, -30);
        fixture.audio();
        fixture.tick(scope, 30);
        REQUIRE(Access::activity(graph) > 0.1f);
        set(fixture.processor, fx::parameterID(scope, 0, fx::enabledField), 0);
        fixture.tick(scope);
        checkSilent(graph);
        set(fixture.processor, fx::parameterID(scope, 0, fx::enabledField), 1);
        fixture.audio(40);
        fixture.tick(scope, 30);
        REQUIRE(Access::activity(graph) > 0.1f);
        control(fixture.processor, scope, 0, 5, 0);
        fixture.tick(scope);
        CHECK(Access::gain(graph) == 0.0f);
        CHECK(Access::activity(graph) == 0.0f);
        control(fixture.processor, scope, 0, 5, 100);
        fixture.audio(40);
        fixture.tick(scope, 30);
        REQUIRE(Access::activity(graph) > 0.1f);
        juce::Thread::sleep(300);
        fixture.tick(scope, 180);
        checkSilent(graph);
        CHECK(Access::motion(graph).lift == 0.0f);
        CHECK(Access::motion(graph).press == 0.0f);
    }
}

TEST_CASE("OTT graph source changes clear the old ripple and leave native Band OTT intact", "[insert-ott-ui][ui][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture;
    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        auto& panel = fixture.select(scope, 0);
        auto& graph = component<OttGraph>(panel);
        seedGraph(graph);
        fixture.select(scope, 1);
        checkSilent(graph);
        CHECK(Access::motion(graph).lift == 0.0f);
        CHECK(Access::shownGain(graph) == 0.0f);
        for (auto value : Access::spectrum(graph)) CHECK(value == 0.0f);
        seedGraph(graph);
        fixture.processor.removeInsertEffect(scope, 1);
        REQUIRE(fixture.processor.addInsertEffect(scope, fx::Type::chorus) == 1);
        fixture.select(scope, 1);
        CHECK_FALSE(graph.isShowing());
        CHECK(component<Oscilloscope>(panel).isShowing());
        fixture.processor.removeInsertEffect(scope, 1);
        REQUIRE(fixture.processor.addInsertEffect(scope, fx::Type::ott) == 1);
        fixture.select(scope, 1);
        CHECK(graph.isShowing());
        checkSilent(graph);
        CHECK(Access::motion(graph).lift == 0.0f);
    }

    fixture.select(1, 0);
    auto& bandGraph = component<OttGraph>(*fixture.band);
    seedGraph(bandGraph);
    fixture.select(2, 0);
    checkSilent(bandGraph);
    CHECK(Access::motion(bandGraph).lift == 0.0f);
    fixture.band->setSwitch(4, true);
    REQUIRE(fixture.band->isOttSelected());
    REQUIRE(bandGraph.isShowing());
    for (auto* base : ParameterIDAndName::ottControlIDs)
    {
        const auto id = juce::String(base) + "2";
        auto* knob = find<ModulatableSlider>(*fixture.band, [&](auto& candidate) { return candidate.getParamID() == id; });
        REQUIRE(knob != nullptr);
        CHECK(knob->isShowing());
    }
    MeterValues native;
    native.ottInputLevelDb[1] = -44.0f;
    native.ottGainChangeDb[1] = 5.0f;
    native.ottDynamicsActivityDb[1] = 4.0f;
    fixture.band->presentMeterValues(native, 100001);
    fixture.tick(2);
    CHECK(Access::input(bandGraph) == -44.0f);
    CHECK(Access::gain(bandGraph) == 5.0f);
    CHECK(Access::activity(bandGraph) == 4.0f);
}

TEST_CASE("Editor FFT frames colour inserted OTT graphs and module changes clear their profile", "[insert-ott-ui][ui][fft][integration]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture;
    auto* channel=find<juce::ComboBox>(*fixture.editor,[](auto& box){return box.getComponentID()=="spectrum_channel";});
    REQUIRE(channel);const auto previousChannel=channel->getSelectedId();
    const juce::ScopeGuard restoreChannel{[&]{channel->setSelectedId(previousChannel,juce::sendNotificationSync);}};
    channel->setSelectedId(3,juce::sendNotificationSync);
    for (int scope : {0, 1})
        REQUIRE(fixture.processor.addInsertEffect(scope, fx::Type::chorus) == 2);
    fixture.processor.setRateAndBufferSizeDetails(48000.0, 256);
    fixture.processor.prepareToPlay(48000.0, 256);
    REQUIRE(fixture.processor.getSampleRate() == 48000.0);

    for (int scope : {0, 1})
    {
        CAPTURE(scope);
        auto& panel = fixture.select(scope, 0);
        auto& graph = component<OttGraph>(panel);
        REQUIRE(graph.isShowing());
        fixture.editor->timerCallback();
        REQUIRE(fixture.processor.isSpectrumCaptureEnabled());
        bool receivedSpectrum = false;
        // Exercise the real audio FIFO -> editor FFT -> selected panel path.
        // Short chunks keep the wall-clock animation delta representative of
        // a running editor, including on slower GUI test machines.
        for (int frame = 0; frame < 48 && !receivedSpectrum; ++frame)
        {
            fixture.audio(4);
            fixture.editor->timerCallback();
            for (auto value : Access::spectrum(graph))
                receivedSpectrum = receivedSpectrum || value > 0.0001f;
        }
        REQUIRE(receivedSpectrum);
        CHECK(Access::input(graph) > -100.0f);

        fixture.select(scope, 2);
        fixture.editor->timerCallback();
        CHECK_FALSE(graph.isShowing());
        CHECK(component<Oscilloscope>(panel).isShowing());
        for (auto value : Access::spectrum(graph)) CHECK(value == 0.0f);
        CHECK(Access::motion(graph).lift == 0.0f);
        CHECK(Access::motion(graph).press == 0.0f);
    }
}
