#include <Panels/ControlPanel/LfoPanel.h>
#include <PluginProcessor.h>
#include <Utility/LfoBankParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "helpers/RepaintRecorder.h"

#include <cmath>
#include <cstdlib>

struct LfoFlowTestAccess
{
    static void selectPoint(LfoEditor& editor, int index)
    {
        editor.selectedPointIndices = {index};
    }
    static float phase(const LfoEditor& editor) { return editor.playheadPos; }
    static float opacity(const LfoEditor& editor) { return editor.playheadOpacity; }
    static LfoEditor& editor(LfoPanel& panel) { return panel.lfoEditor; }
    static void selectSource(LfoPanel& panel, int index) { panel.setLfo(index); }
};

namespace
{
LfoData flatShape()
{
    LfoData shape;
    shape.points = {{0.0f, 0.375f}, {1.0f, 0.375f}};
    shape.curvatures = {0.0f};
    return shape;
}

LfoData curvedShape()
{
    LfoData shape;
    shape.points = {{0.0f, 0.18f}, {0.3f, 0.85f}, {0.66f, 0.34f}, {1.0f, 0.18f}};
    shape.curvatures = {1.1f, -0.75f, 0.35f};
    return shape;
}

void configure(LfoEditor& editor, const LfoData& shape)
{
    editor.setSize(800, 320);
    editor.setDataToDisplay(shape, {0, 1});
    editor.setPlayheadOpacity(0.0f);
    editor.setVisible(true);
}

struct ImageDifference
{
    int changedPixels = 0;
    juce::Rectangle<int> bounds;
};

ImageDifference difference(const juce::Image& first, const juce::Image& second,
                           juce::Rectangle<int> region = {})
{
    REQUIRE(first.getBounds() == second.getBounds());
    if (region.isEmpty()) region = first.getBounds();
    region = region.getIntersection(first.getBounds());
    ImageDifference result;
    for (int y = region.getY(); y < region.getBottom(); ++y)
        for (int x = region.getX(); x < region.getRight(); ++x)
            if (first.getPixelAt(x, y) != second.getPixelAt(x, y))
            {
                ++result.changedPixels;
                result.bounds = result.bounds.getUnion({x, y, 1, 1});
            }
    return result;
}

void setParameter(FireAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.treeState.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void prepareProcessor(FireAudioProcessor& processor)
{
    processor.hasUpdateCheckBeenPerformed = true;
    for (int source = 0; source < 2; ++source)
    {
        setParameter(processor, fire::lfo_bank::parameterID(source, fire::lfo_bank::Field::syncMode), 0.0f);
        setParameter(processor, fire::lfo_bank::parameterID(source, fire::lfo_bank::Field::rateHz), source == 0 ? 2.0f : 5.0f);
        setParameter(processor, fire::lfo_bank::parameterID(source, fire::lfo_bank::Field::phase), source == 0 ? 0.2f : 0.6f);
        processor.getLfoManager().setLfoData(source, curvedShape());
    }
    processor.prepareToPlay(48000.0, 256);
}

void processSilentAudio(FireAudioProcessor& processor)
{
    juce::AudioBuffer<float> audio(2, 256);
    audio.clear();
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);
}

void showPanel(LfoPanel& panel)
{
    panel.setSize(1000, 500);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    REQUIRE(panel.isShowing());
}

void snapshot(const juce::Image& image, const juce::String& name)
{
    const auto* directory = std::getenv("FIRE_LFO_FLOW_SNAPSHOT_DIR");
    if (directory == nullptr || *directory == '\0') return;
    const auto file = juce::File(juce::String::fromUTF8(directory)).getChildFile(name + ".png");
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(image, *stream));
}
}

TEST_CASE("LFO flow highlights a short curve segment and wraps continuously across both edges",
          "[lfo-flow][lfo][editor][ui][visual][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    LfoEditor editor;
    configure(editor, flatShape());
    editor.setPlayheadPosition(0.6f);
    const auto idle = renderRepaintTestComponent(editor);
    editor.setPlayheadOpacity(1.0f);
    const auto running = renderRepaintTestComponent(editor);
    const auto changed = difference(idle, running);
    REQUIRE(changed.changedPixels > 40);
    CHECK(changed.bounds.getWidth() < editor.getWidth() / 5);
    // A flow must hug the waveform. A full-height playhead or its old cap
    // would change pixels far outside this small horizontal curve band.
    CHECK(changed.bounds.getHeight() < 24);
    CHECK(changed.bounds.getY() >= 188);
    CHECK(changed.bounds.getBottom() <= 212);
    CHECK(changed.bounds.getX() < 480);
    CHECK(changed.bounds.getRight() > 480);
    snapshot(running, "lfo-flow-flat");

    editor.setPlayheadPosition(0.015f);
    const auto wrapped = renderRepaintTestComponent(editor);
    CHECK(difference(idle, wrapped, {0, 0, 64, 320}).changedPixels > 10);
    CHECK(difference(idle, wrapped, {736, 0, 64, 320}).changedPixels > 10);
    CHECK(difference(idle, wrapped, {100, 0, 600, 320}).changedPixels == 0);
    snapshot(wrapped, "lfo-flow-wrap");

    editor.setPlayheadOpacity(0.0f);
    CHECK(repaintTestImagesMatch(renderRepaintTestComponent(editor), idle));
    editor.setPlayheadOpacity(1.0f);
    editor.setPlayheadPosition(-1.0f);
    CHECK(repaintTestImagesMatch(renderRepaintTestComponent(editor), idle));
}

TEST_CASE("LFO flow stays below editable nodes and refreshes its shape and source colour",
          "[lfo-flow][lfo][editor][ui][visual][cache][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto shape = flatShape();
    shape.points.insert(shape.points.begin() + 1, {0.5f, 0.375f});
    shape.curvatures = {0.0f, 0.0f};
    LfoEditor editor;
    configure(editor, shape);
    LfoFlowTestAccess::selectPoint(editor, 1);
    editor.setPlayheadPosition(0.5f);
    const auto idle = renderRepaintTestComponent(editor);
    editor.setPlayheadOpacity(1.0f);
    const auto running = renderRepaintTestComponent(editor);
    CHECK(difference(idle, running).changedPixels > 40);
    // The selected node has an opaque centre. It must remain above the flow,
    // so the node's central pixels do not brighten as the highlight passes.
    CHECK(difference(idle, running, {399, 199, 2, 2}).changedPixels == 0);
    snapshot(running, "lfo-flow-node");

    const auto curve = curvedShape();
    for (int bank : {0, 3, 10, 15})
    {
        CAPTURE(bank);
        editor.setDataToDisplay(curve, {bank, 2});
        editor.setPlayheadPosition(0.58f);
        editor.setPlayheadOpacity(1.0f);
        const auto reused = renderRepaintTestComponent(editor);
        LfoEditor fresh;
        configure(fresh, curve);
        fresh.setDataToDisplay(curve, {bank, 2});
        fresh.setPlayheadPosition(0.58f);
        fresh.setPlayheadOpacity(1.0f);
        CHECK(repaintTestImagesMatch(reused, renderRepaintTestComponent(fresh)));
        snapshot(reused, "lfo-flow-curve-bank-" + juce::String(bank + 1));
    }
}

TEST_CASE("LFO flow follows real silent audio without a host playhead and fades when callbacks stop",
          "[lfo-flow][lfo][ui][audio][transport][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    prepareProcessor(processor);
    REQUIRE(processor.getPlayHead() == nullptr);
    LfoPanel panel(processor);
    showPanel(panel);
    auto& editor = LfoFlowTestAccess::editor(panel);
    panel.animationTick();
    CHECK(LfoFlowTestAccess::opacity(editor) == 0.0f);
    CHECK(LfoFlowTestAccess::phase(editor) < 0.0f);

    processSilentAudio(processor);
    panel.animationTick();
    const auto first = processor.getLfoVisualState(0);
    REQUIRE(first.phase >= 0.0f);
    CHECK_FALSE(processor.isDawPlaying());
    CHECK(LfoFlowTestAccess::phase(editor) == Catch::Approx(first.phase));
    CHECK(LfoFlowTestAccess::opacity(editor) == 1.0f);

    // Silence still contains real audio callbacks. The indicator is driven
    // by audio publication, not input level or the host's playing flag.
    for (int frame = 0; frame < 8; ++frame)
    {
        processSilentAudio(processor);
        panel.animationTick(0.1f);
        CHECK(LfoFlowTestAccess::opacity(editor) == 1.0f);
        CHECK(LfoFlowTestAccess::phase(editor)
              == Catch::Approx(processor.getLfoVisualState(0).phase));
    }
    const auto last = processor.getLfoVisualState(0);
    CHECK(last.renderSequence > first.renderSequence);
    CHECK(last.phase != first.phase);
    snapshot(panel.createComponentSnapshot(panel.getLocalBounds()), "lfo-flow-panel-running");

    panel.animationTick(0.1f);
    CHECK(LfoFlowTestAccess::opacity(editor) == 1.0f);
    panel.animationTick(0.1f);
    CHECK(LfoFlowTestAccess::opacity(editor) > 0.0f);
    CHECK(LfoFlowTestAccess::opacity(editor) < 1.0f);
    CHECK(LfoFlowTestAccess::phase(editor) == last.phase);
    panel.animationTick(0.3f);
    CHECK(LfoFlowTestAccess::opacity(editor) == 0.0f);
    CHECK(LfoFlowTestAccess::phase(editor) == last.phase);
    CHECK(processor.getLfoVisualState(0).renderSequence == last.renderSequence);
    snapshot(panel.createComponentSnapshot(panel.getLocalBounds()), "lfo-flow-panel-idle");

    processSilentAudio(processor);
    panel.animationTick();
    CHECK(LfoFlowTestAccess::opacity(editor) == 1.0f);
    CHECK(LfoFlowTestAccess::phase(editor) == Catch::Approx(processor.getLfoVisualState(0).phase));
    processor.releaseResources();
}

TEST_CASE("LFO flow rejects old source and visibility publications until fresh audio arrives",
          "[lfo-flow][lfo][ui][audio][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    prepareProcessor(processor);
    processSilentAudio(processor);
    LfoPanel panel(processor);
    showPanel(panel);
    auto& editor = LfoFlowTestAccess::editor(panel);
    panel.animationTick();
    CHECK(LfoFlowTestAccess::opacity(editor) == 0.0f);
    CHECK(LfoFlowTestAccess::phase(editor) < 0.0f);

    processSilentAudio(processor);
    panel.animationTick();
    REQUIRE(LfoFlowTestAccess::opacity(editor) == 1.0f);
    REQUIRE(processor.getLfoVisualState(1).phase >= 0.0f);
    LfoFlowTestAccess::selectSource(panel, 1);
    panel.animationTick();
    CHECK(panel.getCurrentLfoIndex() == 1);
    CHECK(LfoFlowTestAccess::opacity(editor) == 0.0f);
    CHECK(LfoFlowTestAccess::phase(editor) < 0.0f);
    processSilentAudio(processor);
    panel.animationTick();
    CHECK(LfoFlowTestAccess::opacity(editor) == 1.0f);
    CHECK(LfoFlowTestAccess::phase(editor) == Catch::Approx(processor.getLfoVisualState(1).phase));

    SECTION("hidden workspace does not revive its last visible frame")
    {
        panel.setVisible(false);
        processSilentAudio(processor);
        panel.animationTick();
        CHECK(LfoFlowTestAccess::opacity(editor) == 0.0f);
        panel.setVisible(true);
    }
    SECTION("disabled workspace does not revive its last enabled frame")
    {
        panel.setEnabled(false);
        processSilentAudio(processor);
        panel.animationTick();
        CHECK(LfoFlowTestAccess::opacity(editor) == 0.0f);
        panel.setEnabled(true);
    }
    panel.animationTick();
    CHECK(LfoFlowTestAccess::opacity(editor) == 0.0f);
    CHECK(LfoFlowTestAccess::phase(editor) < 0.0f);
    processSilentAudio(processor);
    panel.animationTick();
    CHECK(LfoFlowTestAccess::opacity(editor) == 1.0f);
    CHECK(LfoFlowTestAccess::phase(editor) == Catch::Approx(processor.getLfoVisualState(1).phase));
    processor.releaseResources();
}
