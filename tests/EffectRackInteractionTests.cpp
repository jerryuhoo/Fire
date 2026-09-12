#include <GUI/EffectRackNavigation.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
template <typename T>
T* findByID(juce::Component& root, const juce::String& id)
{
    if (auto* c = dynamic_cast<T*>(&root); c && c->getComponentID() == id) return c;
    for (auto* child : root.getChildren()) if (auto* result = findByID<T>(*child, id)) return result;
    return nullptr;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> point, juce::Point<float> origin,
                       juce::ModifierKeys mods = {}, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(), point, mods, 0, 0, 0, 0, 0, &c, &c, now, origin, now, 1, dragged};
}
struct OrderChanges : juce::AudioProcessorParameter::Listener
{
    int values = 0, starts = 0, ends = 0;
    void parameterValueChanged(int, float) override { ++values; }
    void parameterGestureChanged(int, bool starting) override {if (starting) ++starts; else ++ends;}
};
struct Fixture
{
    FireAudioProcessor processor;
    FireLookAndFeel look;
    PrimaryTextButton builtin {"Filter"};
    fire::ui::EffectRackNavigation nav {processor, 0};
    int selections = 0;
    explicit Fixture(int count = 4)
    {
        for (int i = 0; i < count; ++i) processor.addInsertEffect(0, fire::effects::Type::delay);
        nav.setLookAndFeel(&look);
        nav.setBuiltins({{&builtin, nullptr}});
        nav.onSelectEffect = [this](int slot) { ++selections; nav.setSelectedSlot(slot); };
        nav.setBounds(0, 0, 200, 300);
        nav.addToDesktop(juce::ComponentPeer::windowIsTemporary); nav.setVisible(true);
        nav.refresh(); nav.setSelectedSlot(0);
    }
    ~Fixture() {nav.onSelectEffect = nullptr; nav.dismiss(); nav.setLookAndFeel(nullptr);}
    juce::TextButton& row(int slot)
    {
        auto* button = findByID<juce::TextButton>(nav, fire::effects::parameterID(nav.getScope(), slot, fire::effects::typeField));
        REQUIRE(button != nullptr); return *button;
    }
    CloseButton& remove(int slot)
    {
        auto* button = findByID<CloseButton>(nav, fire::effects::parameterID(nav.getScope(), slot, fire::effects::typeField) + "Remove");
        REQUIRE(button != nullptr); return *button;
    }
    std::vector<int> order(int scope = 0)
    {
        std::vector<int> result;
        for (int slot = 0; slot < fire::effects::slotCount; ++slot)
            if (processor.getInsertEffectType(scope, slot) != fire::effects::Type::none) result.push_back(slot);
        std::stable_sort(result.begin(), result.end(), [&](int a, int b) {return processor.getInsertEffectOrder(scope, a) < processor.getInsertEffectOrder(scope, b);});
        return result;
    }
    void tick(int frames = 15) {for (int i = 0; i < frames; ++i) nav.animationTick(1.0f / 60.0f);}
    juce::Point<float> inRow(juce::Component& row, juce::Point<float> viewportPoint)
    { return row.getLocalPoint(&nav.getViewport(), viewportPoint); }
};
}

TEST_CASE("Rack removal appears on row hover and closes without an invisible hit target", "[insertfx][ui][rack-interaction]")
{
    Fixture f;
    auto& row = f.row(0);
    auto& remove = f.remove(0);
    CHECK_FALSE(remove.isPresented()); CHECK_FALSE(remove.isVisible());
    const auto originalBounds = row.getBounds();
    auto point = row.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(f.nav).mouseMove(event(row, point, point));
    f.tick(1); CHECK(remove.isPresented());
    f.tick(1); CHECK(remove.getVisibilityAnimation() > 0); CHECK(remove.getVisibilityAnimation() < 1);
    f.tick();
    CHECK(row.getBounds() == originalBounds);
    CHECK(row.getBounds().contains(remove.getBounds()));
    // Moving from the name to its sibling delete button must keep it revealed.
    point = remove.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(f.nav).mouseMove(event(remove, point, point));
    f.tick(); CHECK(remove.isPresented());
    static_cast<juce::Component&>(f.nav).mouseMove(event(f.nav, {-20, -20}, {-20, -20}));
    f.tick(1); CHECK_FALSE(remove.isPresented());
    bool clicks = true, children = true; remove.getInterceptsMouseClicks(clicks, children);
    CHECK_FALSE(clicks); CHECK_FALSE(children);
    remove.triggerClick();
    CHECK(f.processor.getInsertEffectType(0, 0) == fire::effects::Type::delay);
    f.tick(60); CHECK_FALSE(remove.isVisible());
}

TEST_CASE("Inline rack deletion selects a neighbour and cannot replay after rebinding", "[insertfx][ui][rack-interaction]")
{
    Fixture f;
    auto& row = f.row(0);
    const auto point = row.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(f.nav).mouseMove(event(row, point, point)); f.tick();
    f.remove(0).triggerClick();
    CHECK(f.processor.getInsertEffectType(0, 0) == fire::effects::Type::none);
    CHECK(f.row(1).getToggleState()); CHECK(f.selections == 1);
    f.processor.addInsertEffect(2, fire::effects::Type::chorus);
    f.processor.addInsertEffect(2, fire::effects::Type::reverb);
    auto& next = f.row(1);
    const auto nextPoint = next.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(f.nav).mouseMove(event(next, nextPoint, nextPoint)); f.tick();
    auto& close = f.remove(1);
    const auto closePoint = close.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(close).mouseDown(event(close, closePoint, closePoint, juce::ModifierKeys::leftButtonModifier));
    f.nav.setScope(2);
    static_cast<juce::Component&>(close).mouseUp(event(close, closePoint, closePoint));
    CHECK(f.processor.getInsertEffectType(0, 1) == fire::effects::Type::delay);
    CHECK(f.processor.getInsertEffectType(2, 0) == fire::effects::Type::chorus);
    CHECK(f.processor.getInsertEffectType(2, 1) == fire::effects::Type::reverb);
}

TEST_CASE("Rack drag commits one ordered transaction and preserves parameter and LFO identities", "[insertfx][ui][rack-interaction][state]")
{
    Fixture f;
    f.nav.setSelectedSlot(1);
    const auto controlID = fire::effects::parameterID(0, 0, 0);
    auto* parameter = f.processor.treeState.getParameter(controlID);
    parameter->setValueNotifyingHost(0.27f);
    REQUIRE(f.processor.assignLfoToTarget(0, controlID) == LfoManager::AssignmentResult::changed);
    OrderChanges changes;
    for (int i = 0; i < 4; ++i) f.processor.treeState.getParameter(fire::effects::parameterID(0, i, fire::effects::orderField))->addListener(&changes);
    const juce::ScopeGuard cleanup {[&] {
        for (int i = 0; i < 4; ++i) f.processor.treeState.getParameter(fire::effects::parameterID(0, i, fire::effects::orderField))->removeListener(&changes);
    }};
    auto& row = f.row(0);
    int clicks = 0; row.onClick = [&] {++clicks;};
    const auto origin = row.getLocalBounds().toFloat().getCentre();
    static_cast<juce::Component&>(row).mouseDown(event(row, origin, origin, juce::ModifierKeys::leftButtonModifier));
    auto destination = f.inRow(row, {90, static_cast<float>(f.nav.getViewport().getHeight() - 3)});
    static_cast<juce::Component&>(row).mouseDrag(event(row, destination, origin, juce::ModifierKeys::leftButtonModifier, true));
    CHECK(f.order() == std::vector<int>{0, 1, 2, 3});
    CHECK(changes.values == 0); CHECK(changes.starts == 0);
    static_cast<juce::Component&>(row).mouseUp(event(row, destination, origin, {}, true));
    CHECK(f.order() == std::vector<int>{1, 2, 3, 0});
    CHECK(changes.values == 4); CHECK(changes.starts == 4); CHECK(changes.ends == 4);
    CHECK(clicks == 0); CHECK(f.selections == 1);
    CHECK(f.processor.treeState.getParameter(controlID) == parameter);
    CHECK(parameter->getValue() == Catch::Approx(0.27f));
    CHECK(f.processor.getModulationInfoForParameter(controlID).isModulated);
}

TEST_CASE("Rack drag cancels on escape outside release and intervening scope or order changes", "[insertfx][ui][rack-interaction][lifecycle]")
{
    Fixture f;
    auto& row = f.row(0);
    const auto origin = row.getLocalBounds().toFloat().getCentre();
    const auto destination = origin.translated(0, 120);
    const auto start = [&] {
        static_cast<juce::Component&>(row).mouseDown(event(row, origin, origin, juce::ModifierKeys::leftButtonModifier));
        static_cast<juce::Component&>(row).mouseDrag(event(row, destination, origin, juce::ModifierKeys::leftButtonModifier, true));
    };
    SECTION("Escape") {start(); REQUIRE(static_cast<juce::Component&>(row).keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));}
    SECTION("Outside") {start(); static_cast<juce::Component&>(row).mouseUp(event(row, {-100, -100}, origin, {}, true));}
    SECTION("Scope") {start(); f.nav.setScope(2);}
    SECTION("Disable") {start(); f.nav.setEnabled(false); f.nav.setEnabled(true);}
    SECTION("Auxiliary") {
        static_cast<juce::Component&>(row).mouseDown(event(row, origin, origin, juce::ModifierKeys::middleButtonModifier));
        static_cast<juce::Component&>(row).mouseDrag(event(row, destination, origin, juce::ModifierKeys::middleButtonModifier, true));
    }
    SECTION("External order update") {
        start(); f.processor.moveInsertEffect(0, 2, -1);
        static_cast<juce::Component&>(row).mouseUp(event(row, destination, origin, {}, true));
        CHECK(f.order() == std::vector<int>{0, 2, 1, 3});
        return;
    }
    static_cast<juce::Component&>(row).mouseUp(event(row, destination, origin, {}, true));
    CHECK(f.order() == std::vector<int>{0, 1, 2, 3});
    CHECK(f.selections == 0);
}

TEST_CASE("Rack drag auto-scrolls to offscreen effects and keeps the five-row pitch", "[insertfx][ui][rack-interaction][scroll]")
{
    Fixture f(8);
    auto& row = f.row(0);
    const auto origin = row.getLocalBounds().toFloat().getCentre();
    const auto pitch = f.nav.getRowPitch();
    const auto edge = juce::Point<float>(90, static_cast<float>(f.nav.getViewport().getHeight() - 2));
    static_cast<juce::Component&>(row).mouseDown(event(row, origin, origin, juce::ModifierKeys::leftButtonModifier));
    static_cast<juce::Component&>(row).mouseDrag(event(row, f.inRow(row, edge), origin, juce::ModifierKeys::leftButtonModifier, true));
    f.tick(90);
    CHECK(f.nav.getViewport().getViewPositionY() > pitch * 3);
    CHECK(f.nav.getRowPitch() == pitch);
    static_cast<juce::Component&>(row).mouseUp(event(row, f.inRow(row, edge), origin, {}, true));
    CHECK(f.order() == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 0});
    CHECK(f.selections == 0);
}
