#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>

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

void setRoutes(FireAudioProcessor& processor, int count, bool bypassed = false)
{
    const auto targets = ParameterIDAndName::getAllModulatableTargets();
    REQUIRE_FALSE(targets.empty());
    auto& manager = processor.getLfoManager();
    const juce::ScopedLock lock(manager.getLfoDataLock());
    auto& routings = manager.getModulationRoutings();
    routings.clear();
    constexpr std::array<float, 4> amounts {0.35f, -0.60f, 0.25f, 0.80f};
    for (int i = 0; i < count; ++i)
        routings.add({i % 4, targets[static_cast<size_t>(i) % targets.size()].parameterID,
                      amounts[static_cast<size_t>(i) % amounts.size()], i % 2 == 0, bypassed});
    manager.advanceModulationRoutingRevisionLocked();
}

void show(ModulationMatrixPanel& panel, int width = 800, int height = 400)
{
    panel.setSize(width, height);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
}

std::array<juce::Component*, 6> rowControls(ModulationMatrixRow& row)
{
    auto* source = row.findChildWithID("matrix_source");
    auto* destination = row.findChildWithID("matrix_destination");
    auto* amount = row.findChildWithID("matrix_amount");
    auto* polarity = find<juce::Button>(row, [](auto& button) { return button.getTitle().endsWith(" polarity"); });
    auto* active = find<juce::Button>(row, [](auto& button) { return button.getTitle().endsWith(" active"); });
    auto* remove = row.findChildWithID("remove_button");
    std::array<juce::Component*, 6> result {source, destination, amount, polarity, active, remove};
    for (auto* control : result) REQUIRE(control != nullptr);
    return result;
}

void saveSnapshot(ModulationMatrixPanel& panel, const juce::String& name)
{
    const auto* directory = std::getenv("FIRE_MATRIX_UI_SNAPSHOT_DIR");
    if (! directory || ! *directory) return;
    const auto file = juce::File(juce::String::fromUTF8(directory)).getChildFile(name + ".png");
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(stream->setPosition(0));
    REQUIRE(stream->truncate().wasOk());
    CHECK(juce::PNGImageFormat().writeImageToStream(
        panel.createComponentSnapshot(panel.getLocalBounds(), true, 1.0f), *stream));
}
} // namespace

TEST_CASE("Matrix columns stay aligned at minimum width and throughout the scroll range",
          "[matrix-visual][modulation-matrix][ui][layout]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setRoutes(processor, LfoManager::maximumModulationRoutings);
    ModulationMatrixPanel panel(processor);
    show(panel);
    auto* viewport = find<juce::Viewport>(panel, [](auto&) { return true; });
    auto* header = find<ModulationMatrixHeader>(panel, [](auto&) { return true; });
    REQUIRE(viewport != nullptr);
    REQUIRE(header != nullptr);
    auto* content = viewport->getViewedComponent();
    REQUIRE(content != nullptr);
    REQUIRE(content->getNumChildComponents() == LfoManager::maximumModulationRoutings);
    auto* add = dynamic_cast<juce::Button*>(panel.findChildWithID("matrix_add_route"));
    REQUIRE(add != nullptr);
    CHECK_FALSE(add->isEnabled());
    CHECK(find<juce::Button>(panel, [](auto& button) { return button.getButtonText() == "Close"; }) == nullptr);

    for (auto size : {juce::Point<int>(620, 300), juce::Point<int>(800, 400), juce::Point<int>(1100, 600)})
    {
        CAPTURE(size.x, size.y);
        panel.setSize(size.x, size.y);
        CHECK(content->getWidth() == viewport->getMaximumVisibleWidth());
        CHECK(header->getWidth() == content->getWidth());
        CHECK(viewport->getVerticalScrollBar().isVisible());
        CHECK_FALSE(viewport->getHorizontalScrollBar().isVisible());
        auto* row = dynamic_cast<ModulationMatrixRow*>(content->getChildComponent(0));
        REQUIRE(row != nullptr);
        const auto controls = rowControls(*row);
        for (size_t i = 0; i < controls.size(); ++i)
        {
            CHECK(row->getLocalBounds().contains(controls[i]->getBounds()));
            CHECK_FALSE(controls[i]->getBounds().isEmpty());
            CHECK(controls[i]->getExplicitFocusOrder() == static_cast<int>(i + 1));
            for (size_t j = i + 1; j < controls.size(); ++j)
                CHECK(controls[i]->getRight() <= controls[j]->getX());
        }
        const std::array<const char*, 5> captions {"SOURCE", "DESTINATION", "DEPTH", "POLARITY", "ACTIVE"};
        for (size_t i = 0; i < captions.size(); ++i)
        {
            auto* label = find<juce::Label>(*header, [&](auto& candidate) { return candidate.getText() == captions[i]; });
            REQUIRE(label != nullptr);
            CHECK(label->getBounds().getCentreX() == controls[i]->getBounds().getCentreX());
        }
        viewport->setViewPosition(0, content->getHeight());
        auto* last = content->getChildComponent(content->getNumChildComponents() - 1);
        REQUIRE(last != nullptr);
        CHECK(viewport->getViewArea().contains(last->getBounds()));
        viewport->setViewPosition(0, 0);
    }
}

TEST_CASE("Matrix percentage readouts and Active accessibility agree with routing state",
          "[matrix-visual][modulation-matrix][ui][accessibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (bool bypassed : {false, true})
    {
        CAPTURE(bypassed);
        FireAudioProcessor processor;
        setRoutes(processor, 1, bypassed);
        ModulationMatrixPanel panel(processor);
        show(panel);
        auto* row = find<ModulationMatrixRow>(panel, [](auto&) { return true; });
        REQUIRE(row != nullptr);
        const auto controls = rowControls(*row);
        auto* amount = dynamic_cast<juce::Slider*>(controls[2]);
        auto* active = dynamic_cast<juce::Button*>(controls[4]);
        REQUIRE(amount != nullptr);
        REQUIRE(active != nullptr);
        CHECK(amount->getTextFromValue(0.25) == "+25%");
        CHECK(amount->getTextFromValue(-0.40) == "-40%");
        CHECK(amount->getTextFromValue(0.0) == "0%");
        CHECK(amount->getValueFromText("-40%") == Catch::Approx(-0.4));
        CHECK(amount->getTextBoxWidth() >= 48);
        CHECK(active->getToggleState() == ! bypassed);
        CHECK(active->getButtonText() == (bypassed ? "Off" : "On"));
        auto* handler = active->getAccessibilityHandler();
        REQUIRE(handler != nullptr);
        CHECK(handler->getCurrentState().isCheckable());
        CHECK(handler->getCurrentState().isChecked() == ! bypassed);
        for (auto* control : controls)
        {
            auto* accessibility = control->getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK_FALSE(accessibility->getTitle().isEmpty());
            CHECK_FALSE(accessibility->getHelp().isEmpty());
        }
        active->triggerClick();
        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        REQUIRE(routings.size() == 1);
        CHECK(routings[0].isBypassed == ! bypassed);
    }
}

TEST_CASE("Matrix empty state leads into a routing and exports review snapshots on request",
          "[matrix-visual][modulation-matrix][ui][empty-state][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setRoutes(processor, 0);
    ModulationMatrixPanel panel(processor);
    show(panel);
    auto* empty = find<juce::Label>(panel, [](auto& label) { return label.getText() == "No modulation routings yet"; });
    auto* add = dynamic_cast<juce::Button*>(panel.findChildWithID("matrix_add_route"));
    REQUIRE(empty != nullptr);
    REQUIRE(add != nullptr);
    CHECK(empty->isShowing());
    CHECK(add->isEnabled());
    CHECK(add->getButtonText() == "Add routing");
    saveSnapshot(panel, "matrix-empty");
    add->triggerClick();
    // Adding a routing queues an AsyncUpdater rebuild. Allow a busy native
    // message queue to deliver it, without invoking the rebuild from the test.
    const auto waitStarted = juce::Time::getMillisecondCounter();
    while ((empty->isShowing()
            || find<ModulationMatrixRow>(panel, [](auto&) { return true; }) == nullptr)
           && juce::Time::getMillisecondCounter() - waitStarted < 500u)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    CAPTURE(panel.isUiRebuildPending());
    CHECK(processor.getLfoManager().getModulationRoutingsCopy().size() == 1);
    CHECK_FALSE(empty->isShowing());
    CHECK(find<ModulationMatrixRow>(panel, [](auto&) { return true; }) != nullptr);

    setRoutes(processor, 4);
    panel.buildUiFromProcessorState();
    saveSnapshot(panel, "matrix-routes");
    panel.setSize(620, 300);
    saveSnapshot(panel, "matrix-minimum");
    setRoutes(processor, 4, true);
    panel.buildUiFromProcessorState();
    panel.setSize(800, 400);
    saveSnapshot(panel, "matrix-bypassed");
}
