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
    panel.setVisible(false);
    CHECK_FALSE(GraphTemplateInputTestAccess::hasActivePointer(graph));
    graph.mouseUp(makeMouseEvent(graph, {}));
    CHECK_FALSE(graph.getZoomState());

    panel.setVisible(true);
    graph.mouseDown(makeMouseEvent(graph, primary));
    graph.setEnabled(false);
    CHECK_FALSE(GraphTemplateInputTestAccess::hasActivePointer(graph));
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
