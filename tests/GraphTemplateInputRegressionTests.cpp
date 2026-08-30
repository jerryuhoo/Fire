#include <Panels/ControlPanel/Graph Components/GraphPanel.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/GlobalPanel.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

    static float focusTarget(const GraphTemplate& graph) noexcept
    {
        return graph.focusAnimation.target;
    }

    static bool isKeyboardFocusVisible(
        const GraphTemplate& graph) noexcept
    {
        return graph.keyboardFocusVisible;
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

std::vector<GraphTemplate*> directGraphs(juce::Component& owner)
{
    std::vector<GraphTemplate*> result;
    for (int index = 0; index < owner.getNumChildComponents(); ++index)
        if (auto* graph = dynamic_cast<GraphTemplate*>(owner.getChildComponent(index)))
            result.push_back(graph);
    return result;
}

juce::Button* findDirectButton(juce::Component& owner,
                               const juce::String& text)
{
    for (int index = 0; index < owner.getNumChildComponents(); ++index)
        if (auto* button = dynamic_cast<juce::Button*>(owner.getChildComponent(index));
            button != nullptr && button->getButtonText() == text)
            return button;
    return nullptr;
}

struct DirectComponentState
{
    juce::Component* component = nullptr;
    bool visible = false;
    bool enabled = false;
    bool wantsKeyboardFocus = false;
};

std::vector<DirectComponentState> captureDirectComponentStates(
    juce::Component& owner)
{
    std::vector<DirectComponentState> result;
    result.reserve(static_cast<size_t>(owner.getNumChildComponents()));
    for (int index = 0; index < owner.getNumChildComponents(); ++index)
    {
        auto* component = owner.getChildComponent(index);
        result.push_back({ component,
                           component->isVisible(),
                           component->isEnabled(),
                           component->getWantsKeyboardFocus() });
    }
    return result;
}

bool isStandardInteractiveControl(const juce::Component& component)
{
    return dynamic_cast<const juce::Button*>(&component) != nullptr
           || dynamic_cast<const juce::Slider*>(&component) != nullptr
           || dynamic_cast<const juce::ComboBox*>(&component) != nullptr;
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

TEST_CASE("Graph focus presentation follows keyboard input modality",
          "[graph][ui][input][focus][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    GraphTemplate graph;
    graph.setBounds(0, 0, 240, 120);
    graph.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    graph.setVisible(true);

    int zoomRequests = 0;
    graph.setZoomRequestCallback([&zoomRequests] { ++zoomRequests; });
    graph.grabKeyboardFocus();
    REQUIRE(graph.hasKeyboardFocus(true));

    graph.focusGained(juce::Component::focusChangedByTabKey);
    REQUIRE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    CHECK(GraphTemplateInputTestAccess::focusTarget(graph) == 1.0f);

    // A real accepted pointer gesture retains actual focus for keyboard use,
    // while immediately removing the focus-visible animation target.
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    graph.mouseDown(makeMouseEvent(graph, primary));
    REQUIRE(graph.hasKeyboardFocus(true));
    CHECK_FALSE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    CHECK(GraphTemplateInputTestAccess::focusTarget(graph) == 0.0f);
    graph.mouseUp(makeMouseEvent(graph, {}));
    CHECK(zoomRequests == 1);
    CHECK_FALSE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    CHECK(GraphTemplateInputTestAccess::focusTarget(graph) == 0.0f);

    // The next keyboard command restores the cue before invoking a callback
    // that is allowed to synchronously destroy the graph.
    REQUIRE(graph.keyPressed(juce::KeyPress { juce::KeyPress::returnKey }));
    CHECK(zoomRequests == 2);
    CHECK(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    CHECK(GraphTemplateInputTestAccess::focusTarget(graph) == 1.0f);

    graph.focusGained(juce::Component::focusChangedByMouseClick);
    CHECK_FALSE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    CHECK(GraphTemplateInputTestAccess::focusTarget(graph) == 0.0f);

    graph.focusGained(juce::Component::focusChangedDirectly);
    REQUIRE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    graph.setEnabled(false);
    CHECK_FALSE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    CHECK(GraphTemplateInputTestAccess::focusTarget(graph) == 0.0f);

    graph.setEnabled(true);
    graph.focusGained(juce::Component::focusChangedDirectly);
    REQUIRE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    graph.setVisible(false);
    CHECK_FALSE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
    CHECK(GraphTemplateInputTestAccess::focusTarget(graph) == 0.0f);

    graph.focusLost(juce::Component::focusChangedDirectly);
    CHECK_FALSE(GraphTemplateInputTestAccess::isKeyboardFocusVisible(graph));
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

TEST_CASE("Graph keyboard and accessibility activation tolerate owner deletion",
          "[graph][keyboard][accessibility][callback-safety][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("keyboard")
    {
        auto graph = std::make_unique<GraphTemplate>();
        graph->setBounds(0, 0, 200, 100);
        graph->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        graph->setVisible(true);
        graph->setZoomRequestCallback([&graph] { graph.reset(); });

        REQUIRE(graph->keyPressed(juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK(graph == nullptr);
    }

    SECTION("accessibility")
    {
        auto graph = std::make_unique<GraphTemplate>();
        graph->setBounds(0, 0, 200, 100);
        graph->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        graph->setVisible(true);
        graph->setZoomRequestCallback([&graph] { graph.reset(); });
        auto* handler = graph->getAccessibilityHandler();
        REQUIRE(handler != nullptr);

        REQUIRE(handler->getActions().invoke(
            juce::AccessibilityActionType::press));
        CHECK(graph == nullptr);
    }
}

TEST_CASE("Production control panels wire graph zoom into their live layouts",
          "[graph][integration][band-panel][global-panel]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("band graph expands across the module workspace")
    {
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1180, 430);
        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);
        panel.resized();

        // Shape has both an interactive mode control and DC switch in the
        // workspace that the enlarged transfer graph covers.
        auto* shapeButton = findDirectButton(panel, "Shape");
        REQUIRE(shapeButton != nullptr);
        shapeButton->triggerClick();

        auto graphs = directGraphs(panel);
        auto visible = std::find_if(graphs.begin(), graphs.end(),
                                    [](const auto* graph) { return graph->isShowing(); });
        REQUIRE(visible != graphs.end());
        auto* graph = *visible;
        const auto normalBounds = graph->getBounds();
        const auto normalStates = captureDirectComponentStates(panel);

        auto* handler = graph->getAccessibilityHandler();
        REQUIRE(handler != nullptr);
        CHECK_FALSE(handler->getTitle().trim().isEmpty());
        const auto initialAccessibleState = handler->getCurrentState();
        CHECK(initialAccessibleState.isExpandable());
        CHECK(initialAccessibleState.isCollapsed());
        CHECK_FALSE(initialAccessibleState.isExpanded());

        graph->mouseDown(makeMouseEvent(*graph, primary));
        graph->mouseUp(makeMouseEvent(*graph, {}));
        CHECK(graph->getZoomState());
        CHECK(graph->getWidth() > normalBounds.getWidth());
        const auto expandedAccessibleState = handler->getCurrentState();
        CHECK(expandedAccessibleState.isExpandable());
        CHECK(expandedAccessibleState.isExpanded());
        CHECK_FALSE(expandedAccessibleState.isCollapsed());

        int hiddenInteractiveWorkspaceControls = 0;
        for (const auto& state : normalStates)
        {
            auto* component = state.component;
            if (component == graph || ! state.visible
                || ! state.wantsKeyboardFocus
                || ! isStandardInteractiveControl(*component)
                || component->getBounds().isEmpty()
                || ! graph->getBounds().contains(component->getBounds()))
                continue;

            ++hiddenInteractiveWorkspaceControls;
            CHECK_FALSE(component->isVisible());
            CHECK_FALSE(component->isShowing());
        }
        REQUIRE(hiddenInteractiveWorkspaceControls > 0);

        REQUIRE(graph->keyPressed(juce::KeyPress { juce::KeyPress::spaceKey }));
        CHECK_FALSE(graph->getZoomState());
        CHECK(graph->getBounds() == normalBounds);
        const auto restoredAccessibleState = handler->getCurrentState();
        CHECK(restoredAccessibleState.isExpandable());
        CHECK(restoredAccessibleState.isCollapsed());
        CHECK_FALSE(restoredAccessibleState.isExpanded());

        for (const auto& state : normalStates)
        {
            CAPTURE(state.component->getTitle(), state.component->getName());
            CHECK(state.component->isVisible() == state.visible);
            CHECK(state.component->isEnabled() == state.enabled);
            CHECK(state.component->getWantsKeyboardFocus()
                  == state.wantsKeyboardFocus);
        }
    }

    SECTION("Drive preview defers its graph restoration until transfer zoom closes")
    {
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1180, 430);
        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);
        panel.resized();

        auto* drive = panel.getDriveKnob();
        auto* transferGraph = panel.getDistortionGraph();
        REQUIRE(drive != nullptr);
        REQUIRE(transferGraph != nullptr);
        REQUIRE(drive->isShowing());

        const auto graphs = directGraphs(panel);
        auto waveform = std::find_if(
            graphs.begin(), graphs.end(), [](const auto* graph)
            {
                return graph->getTitle().startsWithIgnoreCase("WAVEFORM");
            });
        REQUIRE(waveform != graphs.end());
        auto* waveformGraph = *waveform;
        REQUIRE(waveformGraph->isShowing());
        REQUIRE_FALSE(transferGraph->isShowing());

        drive->onMainDragStart = [&panel](ModulatableSlider*)
        {
            panel.setGraphVisibilityForDriveDrag(true);
        };
        drive->onMainDragEnd = [&panel](ModulatableSlider*)
        {
            panel.setGraphVisibilityForDriveDrag(false);
        };

        drive->mouseDown(makeMouseEvent(*drive, primary));
        REQUIRE(drive->hasActiveInteraction());
        CHECK_FALSE(waveformGraph->isShowing());
        REQUIRE(transferGraph->isShowing());

        auto* transferAccessibility =
            transferGraph->getAccessibilityHandler();
        REQUIRE(transferAccessibility != nullptr);
        REQUIRE(transferAccessibility->getActions().invoke(
            juce::AccessibilityActionType::press));

        // Zoom hides Drive and therefore completes its real Slider gesture.
        // The drag-end callback must not hide the graph that now owns zoom.
        CHECK_FALSE(drive->hasActiveInteraction());
        CHECK_FALSE(drive->isShowing());
        CHECK(transferGraph->getZoomState());
        CHECK(transferGraph->isShowing());
        CHECK(std::count_if(graphs.begin(), graphs.end(),
                            [](const auto* graph)
                            {
                                return graph->isShowing();
                            })
              == 1);

        REQUIRE(transferAccessibility->getActions().invoke(
            juce::AccessibilityActionType::press));
        CHECK_FALSE(transferGraph->getZoomState());
        CHECK_FALSE(transferGraph->isShowing());
        CHECK(waveformGraph->isShowing());
        CHECK(drive->isShowing());
        CHECK_FALSE(drive->hasActiveInteraction());

        // A normal keyboard module switch still ends a fresh Drive preview
        // immediately, after which the selected module owns graph visibility.
        drive->mouseDown(makeMouseEvent(*drive, primary));
        REQUIRE(drive->hasActiveInteraction());
        REQUIRE(transferGraph->isShowing());
        auto* shapeButton = findDirectButton(panel, "Shape");
        REQUIRE(shapeButton != nullptr);
        REQUIRE(static_cast<juce::Component&>(*shapeButton).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK_FALSE(drive->hasActiveInteraction());
        CHECK_FALSE(drive->isShowing());
        CHECK(transferGraph->isShowing());
        CHECK_FALSE(transferGraph->getZoomState());
        CHECK(std::count_if(graphs.begin(), graphs.end(),
                            [](const auto* graph)
                            {
                                return graph->isShowing();
                            })
              == 1);
    }

    SECTION("global graph zoom hides and restores its siblings")
    {
        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1180, 430);
        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);
        panel.resized();

        auto* analysisButton = findDirectButton(panel, "Analysis");
        REQUIRE(analysisButton != nullptr);
        analysisButton->triggerClick();

        auto graphs = directGraphs(panel);
        REQUIRE(graphs.size() == 3);
        REQUIRE(std::all_of(graphs.begin(), graphs.end(),
                            [](const auto* graph) { return graph->isShowing(); }));
        auto* graph = graphs.front();
        auto* preHiddenSibling = graphs.back();
        preHiddenSibling->setVisible(false);
        const auto normalBounds = graph->getBounds();
        const auto normalStates = captureDirectComponentStates(panel);

        graph->mouseDown(makeMouseEvent(*graph, primary));
        graph->mouseUp(makeMouseEvent(*graph, {}));
        CHECK(graph->getZoomState());
        CHECK(graph->getWidth() > normalBounds.getWidth());
        CHECK(std::count_if(graphs.begin(), graphs.end(),
                            [](const auto* candidate) { return candidate->isShowing(); })
              == 1);
        CHECK_FALSE(graphs[1]->isVisible());
        CHECK_FALSE(preHiddenSibling->isVisible());

        auto* handler = graph->getAccessibilityHandler();
        REQUIRE(handler != nullptr);
        REQUIRE(handler->getActions().invoke(
            juce::AccessibilityActionType::press));
        CHECK_FALSE(graph->getZoomState());

        for (const auto& state : normalStates)
        {
            CAPTURE(state.component->getTitle(), state.component->getName());
            CHECK(state.component->isVisible() == state.visible);
            CHECK(state.component->isEnabled() == state.enabled);
            CHECK(state.component->getWantsKeyboardFocus()
                  == state.wantsKeyboardFocus);
        }
    }
}

TEST_CASE("Legacy graph layout clears an unavailable distortion zoom",
          "[graph][layout][legacy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    GraphPanel panel(processor);
    panel.setBounds(0, 0, 640, 360);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);

    panel.toggleZoom(panel.getDistortionGraph());
    REQUIRE(panel.getDistortionGraph()->getZoomState());
    panel.setLayoutMode(GraphPanel::LayoutMode::Global);

    CHECK_FALSE(panel.getDistortionGraph()->getZoomState());
    CHECK(panel.getOscilloscope()->isShowing());
    CHECK(panel.getVuPanel()->isShowing());
    CHECK(panel.getWidthGraph()->isShowing());

    // A hidden Band-only graph cannot be selected programmatically while the
    // legacy panel is in Global mode, otherwise every visible graph is hidden.
    panel.toggleZoom(panel.getDistortionGraph());
    CHECK_FALSE(panel.getDistortionGraph()->getZoomState());
    CHECK(panel.getOscilloscope()->isShowing());
    CHECK(panel.getVuPanel()->isShowing());
    CHECK(panel.getWidthGraph()->isShowing());
}
