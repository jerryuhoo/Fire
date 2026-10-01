#include <PluginEditor.h>
#include <Panels/TopPanel/Preset.h>
#include <Panels/ControlPanel/ModulationMatrixPanel.h>
#include <Utility/LfoBankParameters.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

struct LfoBankUiTestAccess
{
    static auto assignmentHandler(ModulatableSlider& slider)
    {
        return slider.createLfoAssignmentMenuResultHandler();
    }
    static auto assignmentMenu(ModulatableSlider& slider, const ModulatableSlider::LfoSourceMenuState& state)
    {
        return slider.createLfoAssignmentMenu(state);
    }
    static auto sourceHandler(ModulationMatrixRoutingComboBox& combo)
    {
        return combo.createPopupResultHandler();
    }
    static LfoPanel& lfoPanel(FireAudioProcessorEditor& editor) { return editor.lfoPanel; }
    static bool assigning(const FireAudioProcessorEditor& editor) { return editor.isLfoAssignMode; }
    static auto& sliders(FireAudioProcessorEditor& editor) { return editor.allModulatableSliders; }
};

namespace
{
template <typename Type>
Type* findWithID(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id)
        return dynamic_cast<Type*>(&root);
    for (auto* child : root.getChildren())
        if (auto* found = findWithID<Type>(*child, id))
            return found;
    return nullptr;
}

void fillBank(FireAudioProcessor& processor)
{
    for (int slot = fire::lfo_bank::defaultCount; slot < fire::lfo_bank::capacity; ++slot)
        REQUIRE(processor.addLfo() == slot);
}

void clearRoutesForUiFixture(FireAudioProcessor& processor)
{
    auto& manager = processor.getLfoManager();
    const juce::ScopedLock lock(manager.getLfoDataLock());
    manager.getModulationRoutings().clear();
    manager.advanceModulationRoutingRevisionLocked();
}

void waitFor(const std::function<bool()>& condition)
{
    const auto started = juce::Time::getMillisecondCounter();
    while (! condition() && juce::Time::getMillisecondCounter() - started < 500u)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    REQUIRE(condition());
}

juce::String targetID()
{
    return ParameterIDAndName::getIDString(DRIVE_ID, 0);
}

void show(juce::Component& component)
{
    component.setSize(800, 400);
    component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    component.setVisible(true);
}
}

TEST_CASE("LFO bank presets and A B retain sparse fixed source identities", "[lfo-bank][preset][state][ab]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    fillBank(source);
    for (int slot : {1, 7, 14})
        REQUIRE(source.removeLfo(slot));
    LfoData shape;
    shape.points = {{0.0f, 0.15f}, {0.35f, 0.9f}, {1.0f, 0.15f}};
    shape.curvatures = {0.5f, -0.25f};
    source.getLfoManager().setLfoData(15, shape);
    auto* phase = source.treeState.getParameter(fire::lfo_bank::parameterID(15, fire::lfo_bank::Field::phase));
    REQUIRE(phase != nullptr);
    phase->setValueNotifyingHost(0.73f);
    REQUIRE(source.assignLfoToTarget(15, targetID()) == LfoManager::AssignmentResult::changed);
    source.setModulationDepth(targetID(), -0.42f);

    juce::XmlElement preset("WINGSFIRE");
    state::saveStateToXml(source, preset);
    CHECK(preset.getIntAttribute("lfoBankSchemaVersion") == fire::lfo_bank::schemaVersion);
    REQUIRE(preset.getChildByName("LFO_STATE") != nullptr);
    CHECK(preset.getChildByName("LFO_STATE")->getNumChildElements() == fire::lfo_bank::capacity);
    FireAudioProcessor restored;
    REQUIRE(state::canLoadStateFromXml(preset, restored));
    REQUIRE(state::loadStateFromXml(preset, restored));
    for (int slot = 0; slot < fire::lfo_bank::capacity; ++slot)
        CHECK(restored.isLfoPresent(slot) == source.isLfoPresent(slot));
    const auto shapes = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(shapes.size() == fire::lfo_bank::capacity);
    REQUIRE(shapes[15].points.size() == 3);
    CHECK(shapes[15].points[1].y == Catch::Approx(0.9f));
    CHECK(restored.treeState.getParameter(phase->paramID)->getValue() == Catch::Approx(0.73f));
    const auto routes = restored.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(routes.size() == 1);
    CHECK(routes[0].sourceLfoIndex == 15);
    CHECK(routes[0].depth == Catch::Approx(-0.42f));

    restored.stateAB.copyAB(false);
    REQUIRE(restored.removeLfo(15));
    REQUIRE_FALSE(restored.isLfoPresent(15));
    restored.stateAB.toggleAB();
    CHECK(restored.isLfoPresent(15));
    CHECK_FALSE(restored.isLfoPresent(1));
    const auto abRoutes = restored.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(abRoutes.size() == 1);
    CHECK(abRoutes[0].sourceLfoIndex == 15);
}

TEST_CASE("Legacy four source presets reset the extended bank without moving their routes", "[lfo-bank][preset][legacy]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    REQUIRE(source.assignLfoToTarget(2, targetID()) == LfoManager::AssignmentResult::changed);
    juce::XmlElement legacy("WINGSFIRE");
    state::saveStateToXml(source, legacy);
    legacy.removeAttribute("lfoBankSchemaVersion");
    for (const auto& id : fire::lfo_bank::appendedParameterIDs())
        legacy.removeAttribute(id);
    auto* shapes = legacy.getChildByName("LFO_STATE");
    REQUIRE(shapes != nullptr);
    for (int index = shapes->getNumChildElements() - 1; index >= fire::lfo_bank::defaultCount; --index)
        shapes->removeChildElement(shapes->getChildElement(index), true);

    FireAudioProcessor restored;
    fillBank(restored);
    REQUIRE(restored.removeLfo(0));
    REQUIRE(state::canLoadStateFromXml(legacy, restored));
    REQUIRE(state::loadStateFromXml(legacy, restored));
    for (int index = 0; index < fire::lfo_bank::capacity; ++index)
        CHECK(restored.isLfoPresent(index) == (index < fire::lfo_bank::defaultCount));
    const auto routes = restored.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(routes.size() == 1);
    CHECK(routes[0].sourceLfoIndex == 2);
    const auto restoredShapes = restored.getLfoManager().getLfoDataCopy();
    REQUIRE(restoredShapes.size() == fire::lfo_bank::capacity);
    CHECK(restoredShapes[15].points == LfoData {}.points);
}

TEST_CASE("New LFO bank presets reject missing slots family data and out of range routing sources atomically", "[lfo-bank][preset][corrupt]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    REQUIRE(processor.assignLfoToTarget(2, targetID()) == LfoManager::AssignmentResult::changed);
    juce::XmlElement complete("WINGSFIRE");
    state::saveStateToXml(processor, complete);
    auto corrupt = complete;
    SECTION("incomplete appended parameter family")
    {
        corrupt.removeAttribute(fire::lfo_bank::presentParameterID(15));
    }
    SECTION("incomplete fixed shape bank")
    {
        auto* shapes = corrupt.getChildByName("LFO_STATE");
        REQUIRE(shapes != nullptr);
        shapes->removeChildElement(shapes->getChildElement(15), true);
    }
    SECTION("routing references an out of range source")
    {
        auto* routings = corrupt.getChildByName("MODULATION_STATE");
        REQUIRE(routings != nullptr);
        REQUIRE(routings->getChildElement(0) != nullptr);
        routings->getChildElement(0)->setAttribute("source", fire::mod_sources::sourceCount);
    }
    SECTION("future bank schema")
    {
        corrupt.setAttribute("lfoBankSchemaVersion", 2);
    }
    CHECK_FALSE(state::canLoadStateFromXml(corrupt, processor));
    CHECK_FALSE(state::loadStateFromXml(corrupt, processor));
    juce::XmlElement after("WINGSFIRE");
    state::saveStateToXml(processor, after);
    CHECK(after.isEquivalentTo(&complete, false));
}

TEST_CASE("A host disabled LFO source remains a loadable preset without retargeting its route", "[lfo-bank][preset][state]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor source;
    fillBank(source);
    REQUIRE(source.assignLfoToTarget(15, targetID()) == LfoManager::AssignmentResult::changed);
    auto* present = source.treeState.getParameter(fire::lfo_bank::presentParameterID(15));
    REQUIRE(present != nullptr);
    present->setValueNotifyingHost(0.0f);
    juce::XmlElement preset("WINGSFIRE");
    state::saveStateToXml(source, preset);
    FireAudioProcessor restored;
    REQUIRE(state::canLoadStateFromXml(preset, restored));
    REQUIRE(state::loadStateFromXml(preset, restored));
    CHECK_FALSE(restored.isLfoPresent(15));
    const auto routes = restored.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(routes.size() == 1);
    CHECK(routes[0].sourceLfoIndex == 15);
}

TEST_CASE("Matrix sources expose present slots and reject removed recreated menu identities", "[lfo-bank][modulation-matrix][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    clearRoutesForUiFixture(processor);
    fillBank(processor);
    REQUIRE(processor.removeLfo(1));
    REQUIRE(processor.assignLfoToTarget(0, targetID()) == LfoManager::AssignmentResult::changed);
    ModulationMatrixPanel panel(processor);
    show(panel);
    auto* combo = findWithID<ModulationMatrixRoutingComboBox>(panel, "matrix_source");
    REQUIRE(combo != nullptr);
    CHECK(combo->getNumItems() == 20);
    CHECK(combo->indexOfItemId(2) == -1);
    CHECK(combo->indexOfItemId(16) >= 0);
    auto stale = LfoBankUiTestAccess::sourceHandler(*combo);
    REQUIRE(processor.removeLfo(15));
    REQUIRE(processor.addLfo() == 1);
    REQUIRE(processor.addLfo() == 15);
    stale(16);
    auto routes = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(routes.size() == 1);
    CHECK(routes[0].sourceLfoIndex == 0);
    waitFor([&] { return ! panel.isUiRebuildPending(); });
    combo = findWithID<ModulationMatrixRoutingComboBox>(panel, "matrix_source");
    REQUIRE(combo != nullptr);
    auto fresh = LfoBankUiTestAccess::sourceHandler(*combo);
    fresh(16);
    routes = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(routes.size() == 1);
    CHECK(routes[0].sourceLfoIndex == 15);

    for (int slot = 0; slot < fire::lfo_bank::capacity; ++slot)
        REQUIRE(processor.removeLfo(slot));
    auto* add = findWithID<juce::Button>(panel, "matrix_add_route");
    REQUIRE(add != nullptr);
    waitFor([&] { return !panel.isUiRebuildPending() && add->isEnabled(); });
    CHECK(findWithID<ModulationMatrixRoutingComboBox>(panel, "matrix_source") == nullptr);
}

TEST_CASE("Slider assignment menus preserve sparse source IDs and guard asynchronous bank changes", "[lfo-bank][modulatable-slider][ui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto slider = std::make_unique<ModulatableSlider>();
    slider->parameterID = targetID();
    ModulatableSlider::LfoSourceMenuState state;
    state.presentMask = (1u << 0) | (1u << 15);
    state.revision = 7;
    slider->getLfoSourceMenuState = [&] { return state; };
    const auto menu = LfoBankUiTestAccess::assignmentMenu(*slider, state);
    juce::Array<int> ids;
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().itemID > 0)
            ids.add(it.getItem().itemID);
    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 1);
    CHECK(ids[1] == 16);
    int assignments = 0;
    slider->onLfoAssignmentRequested = [&](int index, const auto& target)
    {
        CHECK(index == 15);
        CHECK(target == targetID());
        ++assignments;
    };
    auto stale = LfoBankUiTestAccess::assignmentHandler(*slider);
    state.revision += 2; // Remove/re-add the same slot without a UI dispatch.
    stale(16);
    CHECK(assignments == 0);
    auto fresh = LfoBankUiTestAccess::assignmentHandler(*slider);
    fresh(16);
    fresh(16);
    CHECK(assignments == 1);
    auto absent = LfoBankUiTestAccess::assignmentHandler(*slider);
    state.presentMask = 1;
    absent(16);
    CHECK(assignments == 1);

    state.presentMask |= 1u << 15;
    auto deleted = LfoBankUiTestAccess::assignmentHandler(*slider);
    slider->getLfoSourceMenuState = [&]
    {
        slider.reset();
        return state;
    };
    deleted(16);
    CHECK(slider == nullptr);
    CHECK(assignments == 1);
}

TEST_CASE("Editor assigns high LFO sources and deletion cancels armed one shot callbacks", "[lfo-bank][editor][ui][assignment]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    clearRoutesForUiFixture(processor);
    processor.hasUpdateCheckBeenPerformed = true;
    fillBank(processor);
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->stopTimer();
    auto& panel = LfoBankUiTestAccess::lfoPanel(*editor);
    const auto& sliders = LfoBankUiTestAccess::sliders(*editor);
    REQUIRE_FALSE(sliders.empty());
    auto* target = sliders.front();
    REQUIRE(target != nullptr);
    REQUIRE(target->getParamID().isNotEmpty());
    target->onLfoAssignmentRequested(15, target->getParamID());
    CHECK(target->isModulated);
    CHECK(target->lfoSource == 16);
    processor.clearModulationForParameter(target->getParamID());
    panel.onAssignButtonClicked(15);
    REQUIRE(LfoBankUiTestAccess::assigning(*editor));
    auto queuedAssign = target->onClickInAssignMode;
    REQUIRE(queuedAssign != nullptr);
    REQUIRE(processor.removeLfo(15));
    panel.onLfoRemoved(15);
    panel.onCurrentLfoChanged(14);
    CHECK_FALSE(LfoBankUiTestAccess::assigning(*editor));
    CHECK(target->onClickInAssignMode == nullptr);
    REQUIRE(processor.addLfo() == 15);
    queuedAssign(target->getParamID());
    CHECK(processor.getLfoManager().getModulationRoutingsCopy().isEmpty());
    panel.onAssignButtonClicked(15);
    REQUIRE(LfoBankUiTestAccess::assigning(*editor));
    queuedAssign(target->getParamID());
    CHECK(processor.getLfoManager().getModulationRoutingsCopy().isEmpty());
    CHECK(LfoBankUiTestAccess::assigning(*editor));
    auto freshAssign = target->onClickInAssignMode;
    REQUIRE(freshAssign != nullptr);
    freshAssign(target->getParamID());
    const auto routes = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(routes.size() == 1);
    CHECK(routes[0].sourceLfoIndex == 15);
    panel.onCurrentLfoChanged(-1);
    CHECK_FALSE(LfoBankUiTestAccess::assigning(*editor));
}
