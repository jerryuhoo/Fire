#include <Panels/ControlPanel/Graph Components/GraphPanel.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

struct GraphTemplateInputTestAccess
{
    static bool hasActivePointer(const GraphTemplate& graph) noexcept
    {
        return graph.primaryPointerDown;
    }

    static void setPointerSourceIndex(GraphTemplate& graph, int index) noexcept
    {
        graph.pointerSourceIndex = index;
    }

    static void setAnimationTargets(GraphTemplate& graph,
                                    float hover,
                                    float press,
                                    float focus,
                                    float disabled) noexcept
    {
        graph.hoverAnimation.setTarget(hover);
        graph.pressAnimation.setTarget(press);
        graph.focusAnimation.setTarget(focus);
        graph.disabledAnimation.setTarget(disabled);
    }

    static bool advanceAnimation(GraphTemplate& graph,
                                 float deltaSeconds) noexcept
    {
        return graph.advanceAnimation(deltaSeconds);
    }

    static float hover(const GraphTemplate& graph) noexcept
    {
        return graph.hoverAnimation.current;
    }

    static float press(const GraphTemplate& graph) noexcept
    {
        return graph.pressAnimation.current;
    }

    static float focus(const GraphTemplate& graph) noexcept
    {
        return graph.focusAnimation.current;
    }

    static float disabled(const GraphTemplate& graph) noexcept
    {
        return graph.disabledAnimation.current;
    }
};

namespace
{
juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::ModifierKeys modifiers,
                                juce::Point<float> position = {})
{
    if (position == juce::Point<float>())
        position = component.getLocalBounds().toFloat().getCentre();

    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             modifiers,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             now,
             position,
             now,
             1,
             false };
}

std::vector<juce::ModifierKeys> rejectedModifiers()
{
    std::vector<juce::ModifierKeys> result {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::rightButtonModifier }
    };
#if JUCE_MAC
    result.emplace_back(juce::ModifierKeys::leftButtonModifier
                        | juce::ModifierKeys::ctrlModifier);
#endif
    return result;
}
} // namespace

TEST_CASE("Graph zoom accepts only an owned complete primary gesture",
          "[graph][input][primary][zoom]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    GraphPanel panel(processor);
    panel.setBounds(0, 0, 640, 360);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);

    auto& graph = *panel.getOscilloscope();
    REQUIRE(graph.isShowing());
    CHECK_FALSE(graph.getZoomState());

    for (const auto modifiers : rejectedModifiers())
    {
        graph.mouseDown(makeMouseEvent(graph, modifiers));
        graph.mouseUp(makeMouseEvent(graph, {}));
        CHECK_FALSE(GraphTemplateInputTestAccess::hasActivePointer(graph));
        CHECK_FALSE(graph.getZoomState());
    }

    graph.mouseDown(makeMouseEvent(
        graph, juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    REQUIRE(GraphTemplateInputTestAccess::hasActivePointer(graph));
    GraphTemplateInputTestAccess::setPointerSourceIndex(graph, 99);
    graph.mouseUp(makeMouseEvent(graph, {}));
    CHECK(GraphTemplateInputTestAccess::hasActivePointer(graph));
    CHECK_FALSE(graph.getZoomState());

    // A fresh down from the owning physical source recovers the missing up.
    GraphTemplateInputTestAccess::setPointerSourceIndex(
        graph, juce::Desktop::getInstance().getMainMouseSource().getIndex());
    graph.mouseDown(makeMouseEvent(
        graph, juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    graph.mouseUp(makeMouseEvent(graph, {}));
    CHECK_FALSE(GraphTemplateInputTestAccess::hasActivePointer(graph));
    CHECK(graph.getZoomState());
    CHECK(graph.getBounds() == panel.getLocalBounds().reduced(2));
}

TEST_CASE("Graph zoom release cannot cross hidden or disabled lifecycle",
          "[graph][input][lifecycle][zoom]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    GraphPanel panel(processor);
    panel.setBounds(0, 0, 640, 360);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    auto& graph = *panel.getOscilloscope();

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    graph.mouseDown(makeMouseEvent(graph, primary));
    REQUIRE(GraphTemplateInputTestAccess::advanceAnimation(
        graph, 1.0f / 60.0f));
    CHECK(GraphTemplateInputTestAccess::press(graph) > 0.0f);
    CHECK(GraphTemplateInputTestAccess::press(graph) < 1.0f);
    panel.setVisible(false);
    CHECK_FALSE(GraphTemplateInputTestAccess::hasActivePointer(graph));
    graph.mouseUp(makeMouseEvent(graph, {}));
    CHECK_FALSE(graph.getZoomState());

    panel.setVisible(true);
    graph.mouseDown(makeMouseEvent(graph, primary));
    graph.setEnabled(false);
    CHECK_FALSE(GraphTemplateInputTestAccess::hasActivePointer(graph));
    REQUIRE(GraphTemplateInputTestAccess::advanceAnimation(
        graph, 1.0f / 60.0f));
    CHECK(GraphTemplateInputTestAccess::disabled(graph) > 0.0f);
    CHECK(GraphTemplateInputTestAccess::disabled(graph) < 1.0f);
    graph.mouseUp(makeMouseEvent(graph, {}));
    CHECK_FALSE(graph.getZoomState());
}

TEST_CASE("Graph zoom callback tolerates synchronous graph deletion",
          "[graph][callback-safety][self-delete][zoom]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto graph = std::make_unique<GraphTemplate>();
    graph->setBounds(0, 0, 200, 100);
    graph->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    graph->setVisible(true);
    graph->setZoomRequestCallback([&graph] { graph.reset(); });

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    graph->mouseDown(makeMouseEvent(*graph, primary));
    graph->mouseUp(makeMouseEvent(*graph, {}));
    CHECK(graph == nullptr);
}

TEST_CASE("Interactive graph presentation advances continuously and hidden-idle",
          "[graph][animation][lifecycle][zoom]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Component owner;
    owner.setBounds(0, 0, 320, 180);
    owner.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    owner.setVisible(true);

    GraphTemplate graph;
    owner.addAndMakeVisible(graph);
    graph.setBounds(owner.getLocalBounds());
    graph.setZoomRequestCallback([] {});

    GraphTemplateInputTestAccess::setAnimationTargets(
        graph, 1.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(GraphTemplateInputTestAccess::advanceAnimation(
        graph, 1.0f / 60.0f));
    CHECK(GraphTemplateInputTestAccess::hover(graph) > 0.0f);
    CHECK(GraphTemplateInputTestAccess::hover(graph) < 1.0f);
    CHECK(GraphTemplateInputTestAccess::press(graph)
          > GraphTemplateInputTestAccess::hover(graph));
    CHECK(GraphTemplateInputTestAccess::focus(graph) > 0.0f);
    CHECK(GraphTemplateInputTestAccess::disabled(graph) > 0.0f);

    owner.setVisible(false);
    CHECK(GraphTemplateInputTestAccess::hover(graph) == 0.0f);
    CHECK(GraphTemplateInputTestAccess::press(graph) == 0.0f);
    CHECK(GraphTemplateInputTestAccess::focus(graph) == 0.0f);
    CHECK(GraphTemplateInputTestAccess::disabled(graph) == 0.0f);
}

TEST_CASE("Non-interactive graph presentation remains static",
          "[graph][animation][non-interactive]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    GraphTemplate graph;
    GraphTemplateInputTestAccess::setAnimationTargets(
        graph, 1.0f, 1.0f, 1.0f, 1.0f);

    // With no owner callback, lifecycle target refresh snaps every visual
    // channel to neutral instead of creating a hidden Timer workload.
    graph.setZoomRequestCallback({});
    CHECK(GraphTemplateInputTestAccess::hover(graph) == 0.0f);
    CHECK(GraphTemplateInputTestAccess::press(graph) == 0.0f);
    CHECK(GraphTemplateInputTestAccess::focus(graph) == 0.0f);
    CHECK(GraphTemplateInputTestAccess::disabled(graph) == 0.0f);
}
