#include <Panels/ControlPanel/BandPanel.h>
#include <catch2/catch_test_macros.hpp>

#include <memory>

struct GraphViewSelectorTestAccess
{
    static std::function<void(int)> popupResult(ContextAwareComboBox& menu)
    { return menu.createPopupResultHandler(); }
    static bool running(const Oscilloscope& graph) { return graph.isTimerRunning(); }
    static bool running(const VUPanel& graph) { return graph.isTimerRunning(); }
    static bool running(const WidthGraph& graph) { return graph.isTimerRunning(); }
};

namespace
{
template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate predicate)
{
    if (auto* result = dynamic_cast<T*>(&root); result && predicate(*result)) return result;
    for (auto* child : root.getChildren())
        if (auto* result = find<T>(*child, predicate)) return result;
    return nullptr;
}
ContextAwareComboBox& menu(BandPanel& panel)
{
    auto* result = find<ContextAwareComboBox>(panel, [](auto& box)
    { return box.getComponentID() == "band_graph_view"; });
    REQUIRE(result != nullptr);
    return *result;
}
void choose(BandPanel& panel, int itemId)
{
    auto result = GraphViewSelectorTestAccess::popupResult(menu(panel));
    result(itemId);
}
void show(BandPanel& panel)
{
    panel.setBounds(0, 0, 984, 258);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    REQUIRE(panel.isShowing());
}
template <typename Graph>
Graph& graph(BandPanel& panel)
{
    auto* result = find<Graph>(panel, [](auto&) { return true; });
    REQUIRE(result != nullptr);
    return *result;
}
void checkSingleView(BandPanel& panel, GraphTemplate& selected)
{
    for (auto* child : panel.getChildren())
        if (auto* candidate = dynamic_cast<GraphTemplate*>(child))
            CHECK(candidate->isShowing() == (candidate == &selected));
    CHECK(GraphViewSelectorTestAccess::running(graph<Oscilloscope>(panel))
          == (&selected == &graph<Oscilloscope>(panel)));
    CHECK(GraphViewSelectorTestAccess::running(graph<VUPanel>(panel))
          == (&selected == &graph<VUPanel>(panel)));
    CHECK(GraphViewSelectorTestAccess::running(graph<WidthGraph>(panel))
          == (&selected == &graph<WidthGraph>(panel)));
}
juce::MouseEvent event(juce::Component& component, juce::ModifierKeys mods = {})
{
    const auto now = juce::Time::getCurrentTime();
    const auto point = component.getLocalBounds().toFloat().getCentre();
    return {juce::Desktop::getInstance().getMainMouseSource(), point, mods,
            0, 0, 0, 0, 0, &component, &component, now, point, now, 1, false};
}
void zoom(GraphTemplate& target)
{
    auto* handler = target.getAccessibilityHandler();
    REQUIRE(handler != nullptr);
    REQUIRE(handler->getActions().invoke(juce::AccessibilityActionType::press));
}
} // namespace

TEST_CASE("Band graph menu switches views and keeps hidden visualiser clocks stopped",
          "[graph-view][ui][layout][graph]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    BandPanel panel(processor, {}, {}, {}, {}, {});
    show(panel);
    auto& selector = menu(panel);
    REQUIRE(selector.getSelectedId() == 1);
    CHECK(selector.getText() == "Auto: Waveform");
    checkSingleView(panel, graph<Oscilloscope>(panel));
    choose(panel, 4);
    checkSingleView(panel, graph<VUPanel>(panel));
    choose(panel, 5);
    checkSingleView(panel, graph<WidthGraph>(panel));
    choose(panel, 3);
    checkSingleView(panel, graph<DistortionGraph>(panel));
    choose(panel, 2);
    checkSingleView(panel, graph<Oscilloscope>(panel));

    choose(panel, 1);
    panel.setSwitch(1, true);
    CHECK(selector.getText() == "Auto: Transfer");
    checkSingleView(panel, graph<DistortionGraph>(panel));
    choose(panel, 5);
    panel.setSwitch(2, true);
    CHECK(selector.getSelectedId() == 5);
    checkSingleView(panel, graph<WidthGraph>(panel));
    choose(panel, 1);
    CHECK(selector.getText() == "Auto: Meters");
    checkSingleView(panel, graph<VUPanel>(panel));
    panel.setSwitch(4, true);
    CHECK(selector.getText() == "Auto: OTT dynamics");
    checkSingleView(panel, graph<OttGraph>(panel));

    for (float scale : {1.0f, 1.4f, 2.0f})
    {
        panel.setScale(scale);
        panel.setSize(juce::roundToInt(984 * scale), juce::roundToInt(258 * scale));
        const auto menuBounds = panel.getLocalArea(&selector, selector.getLocalBounds());
        CHECK(panel.getLocalBounds().contains(menuBounds));
        CHECK(menuBounds.getWidth() >= juce::roundToInt(140 * scale));
        CHECK_FALSE(menuBounds.intersects(graph<OttGraph>(panel).getBounds()));
    }
    auto* accessibility = selector.getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    CHECK(accessibility->getTitle() == "Band 1 graph view");
    CHECK_FALSE(accessibility->getHelp().isEmpty());
    panel.setVisible(false);
    CHECK_FALSE(GraphViewSelectorTestAccess::running(graph<Oscilloscope>(panel)));
    CHECK_FALSE(GraphViewSelectorTestAccess::running(graph<VUPanel>(panel)));
    CHECK_FALSE(GraphViewSelectorTestAccess::running(graph<WidthGraph>(panel)));
}

TEST_CASE("Drive transfer preview restores the chosen graph and view selection survives zoom",
          "[graph-view][ui][graph][preview][zoom]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    BandPanel panel(processor, {}, {}, {}, {}, {});
    show(panel);
    auto* drive = panel.getDriveKnob();
    drive->onMainDragStart = [&panel](ModulatableSlider*) { panel.setGraphVisibilityForDriveDrag(true); };
    drive->onMainDragEnd = [&panel](ModulatableSlider*) { panel.setGraphVisibilityForDriveDrag(false); };
    choose(panel, 5);
    drive->mouseDown(event(*drive, juce::ModifierKeys::leftButtonModifier));
    REQUIRE(drive->hasActiveInteraction());
    checkSingleView(panel, graph<DistortionGraph>(panel));
    CHECK(menu(panel).getSelectedId() == 3);
    drive->mouseUp(event(*drive));
    checkSingleView(panel, graph<WidthGraph>(panel));
    CHECK(menu(panel).getSelectedId() == 5);

    drive->mouseDown(event(*drive, juce::ModifierKeys::leftButtonModifier));
    zoom(graph<DistortionGraph>(panel));
    CHECK_FALSE(drive->hasActiveInteraction());
    CHECK(graph<DistortionGraph>(panel).getZoomState());
    CHECK(menu(panel).isShowing());
    choose(panel, 4);
    CHECK(graph<VUPanel>(panel).getZoomState());
    CHECK_FALSE(graph<DistortionGraph>(panel).getZoomState());
    checkSingleView(panel, graph<VUPanel>(panel));
    zoom(graph<VUPanel>(panel));
    CHECK_FALSE(graph<VUPanel>(panel).getZoomState());
    CHECK(menu(panel).getSelectedId() == 4);
    CHECK(drive->isShowing());
}

TEST_CASE("Graph view popup results cannot cross band module or visibility sessions",
          "[graph-view][ui][lifecycle][popup]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int boundary = 0; boundary < 4; ++boundary)
    {
        CAPTURE(boundary);
        FireAudioProcessor processor;
        auto* bandCount = processor.treeState.getParameter(NUM_BANDS_ID);
        REQUIRE(bandCount != nullptr);
        bandCount->setValueNotifyingHost(bandCount->convertTo0to1(2));
        BandPanel panel(processor, {}, {}, {}, {}, {});
        show(panel);
        choose(panel, 5);
        auto stale = GraphViewSelectorTestAccess::popupResult(menu(panel));
        if (boundary == 0) { panel.setVisible(false); panel.setVisible(true); }
        else if (boundary == 1) { panel.setEnabled(false); panel.setEnabled(true); }
        else if (boundary == 2) panel.setFocusBandNum(1);
        else panel.setSwitch(1, true);
        stale(4);
        CHECK(menu(panel).getSelectedId() == 5);
        checkSingleView(panel, graph<WidthGraph>(panel));
        if (boundary == 2) CHECK(menu(panel).getTitle() == "Band 2 graph view");
        CHECK(static_cast<juce::Component&>(menu(panel)).keyPressed(juce::KeyPress {juce::KeyPress::leftKey}));
        CHECK(menu(panel).getSelectedId() == 4);
        checkSingleView(panel, graph<VUPanel>(panel));
    }
}

TEST_CASE("UI-only contextual selections tolerate synchronous owner deletion",
          "[graph-view][ui][context-menu][callback-safety]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (bool keyboard : {false, true})
    {
        auto selector = std::make_unique<ContextAwareComboBox>();
        selector->setBounds(0, 0, 160, 28);
        selector->addItem("First", 1);
        selector->addItem("Second", 2);
        selector->setSelectedId(1, juce::dontSendNotification);
        selector->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        selector->setVisible(true);
        int selected = 0;
        selector->configurePopupSession([] {return std::uint64_t(1);}, [] {return true;},
            [&](int itemId) { selected = itemId; selector.reset(); });
        if (keyboard)
            CHECK(static_cast<juce::Component&>(*selector).keyPressed(juce::KeyPress {juce::KeyPress::rightKey}));
        else
        {
            auto result = GraphViewSelectorTestAccess::popupResult(*selector);
            result(2);
        }
        CHECK(selected == 2);
        CHECK(selector == nullptr);
    }
}

TEST_CASE("Full-width insert pages hide the graph selector and invalidate its old choices",
          "[graph-view][ui][lifecycle][insertfx]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto type : {fire::effects::Type::granular, fire::effects::Type::lofi})
    {
        FireAudioProcessor processor;
        const int slot = processor.addInsertEffect(1, type);
        REQUIRE(slot >= 0);
        BandPanel panel(processor, {}, {}, {}, {}, {});
        show(panel);
        choose(panel, 5);
        auto stale = GraphViewSelectorTestAccess::popupResult(menu(panel));
        const auto id = fire::effects::parameterID(1, slot, fire::effects::typeField);
        auto* insert = find<juce::Button>(panel, [&](auto& button) { return button.getComponentID() == id; });
        REQUIRE(insert != nullptr);
        insert->triggerClick();
        CHECK_FALSE(menu(panel).isShowing());
        for (auto* child : panel.getChildren())
            if (auto* candidate = dynamic_cast<GraphTemplate*>(child))
                CHECK_FALSE(candidate->isShowing());
        stale(4);
        panel.setSwitch(0, true);
        CHECK(menu(panel).isShowing());
        CHECK(menu(panel).getSelectedId() == 5);
        checkSingleView(panel, graph<WidthGraph>(panel));
    }
}
