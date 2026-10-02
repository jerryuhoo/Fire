#include <GUI/EffectRackNavigation.h>
#include <PluginEditor.h>
#include <Panels/TopPanel/Preset.h>
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
    fire::ui::ModuleDragButton builtin {"Filter"};
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
    static_cast<juce::Component&>(row).mouseUp(event(row, destination, origin, juce::ModifierKeys::leftButtonModifier, true));
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
    static_cast<juce::Component&>(row).mouseUp(event(row, f.inRow(row, edge), origin, juce::ModifierKeys::leftButtonModifier, true));
    CHECK(f.order() == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 0});
    CHECK(f.selections == 0);
}

TEST_CASE("Real module rails freely interleave builtin modules and inserts and restore their saved order", "[module-order][ui][rack-interaction][preset]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    processor.addInsertEffect(0, fire::effects::Type::delay);
    processor.addInsertEffect(1, fire::effects::Type::chorus);
    FireAudioProcessorEditor editor(processor);
    editor.setSize(1000, 500);
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary); editor.setVisible(true);
    std::vector<fire::ui::EffectRackNavigation*> rails;
    std::function<void(juce::Component&)> collect = [&](auto& root) {
        if (auto* rail = dynamic_cast<fire::ui::EffectRackNavigation*>(&root)) rails.push_back(rail);
        for (auto* child : root.getChildren()) collect(*child);
    };
    collect(editor);
    REQUIRE(rails.size() == 2);
    const auto buttonNamed = [](juce::Component& root, const juce::String& name) {
        std::function<juce::Button*(juce::Component&)> find = [&](auto& c) -> juce::Button* {
            if (auto* b = dynamic_cast<juce::Button*>(&c); b && b->getButtonText() == name) return b;
            for (auto* child : c.getChildren()) if (auto* b = find(*child)) return b;
            return nullptr;
        };
        return find(root);
    };
    for (const auto scope : {0, 1})
    {
        CAPTURE(scope);
        auto* workspace = buttonNamed(editor, scope == 0 ? "MASTER LAB" : "BAND LAB");
        REQUIRE(workspace != nullptr); workspace->triggerClick();
        auto* nav = *std::find_if(rails.begin(), rails.end(), [scope](auto* rail) { return rail->getScope() == scope; });
        nav->refresh();
        REQUIRE(nav->isShowing());
        const std::vector<juce::String> names = scope == 0
            ? std::vector<juce::String>{"EQ", "Lo-Fi", "Analysis", "Delay"}
            : std::vector<juce::String>{"Drive", "Shape", "Compressor", "Stereo", "OTT", "Chorus"};
        std::vector<fire::ui::ModuleDragButton*> rows;
        for (const auto& name : names)
        {
            auto* row = dynamic_cast<fire::ui::ModuleDragButton*>(buttonNamed(*nav, name));
            REQUIRE(row != nullptr); rows.push_back(row);
        }
        // Move each real builtin and inserted row to the front, then to the end.
        // Native JUCE mouseUp carries the released left-button flag.
        for (size_t index = 0; index < rows.size(); ++index)
        {
            const int node = index + 1 == rows.size() ? fire::module_order::firstInsert : static_cast<int>(index);
            CAPTURE(node);
            auto& row = *rows[index];
            for (const bool toEnd : {false, true})
            {
                auto& viewport = nav->getViewport();
                viewport.setViewPosition(0, row.getY());
                const auto origin = row.getLocalBounds().toFloat().getCentre();
                row.mouseDown(event(row, origin, origin, juce::ModifierKeys::leftButtonModifier));
                const juce::Point<float> destination(70, toEnd ? viewport.getHeight() - 2.0f : 2.0f);
                row.mouseDrag(event(row, row.getLocalPoint(&viewport, destination), origin, juce::ModifierKeys::leftButtonModifier, true));
                for (int frame = 0; frame < 100; ++frame) nav->animationTick(1.0f / 60.0f);
                row.mouseUp(event(row, row.getLocalPoint(&viewport, destination), origin, juce::ModifierKeys::leftButtonModifier, true));
                const auto order = processor.getModuleOrder(scope);
                CHECK(order[toEnd ? rows.size() - 1 : 0] == node);
                if (toEnd)
                    for (auto* other : rows) CHECK(row.getY() >= other->getY());
                else
                    for (auto* other : rows) CHECK(row.getY() <= other->getY());
                CHECK(row.getToggleState());
                CHECK(row.getAlpha() == Catch::Approx(1));
            }
        }
        processor.moveModuleBefore(scope, scope == 0 ? 1 : 2, 0);
        processor.moveModuleBefore(scope, fire::module_order::firstInsert, 0);
        nav->refresh();
        const auto expected = processor.getModuleOrder(scope);
        juce::XmlElement preset("WINGSFIRE"); state::saveStateToXml(processor, preset);
        processor.moveModuleBefore(scope, expected[1], expected[0]);
        nav->refresh();
        auto& row = *rows.front();
        const auto origin = row.getLocalBounds().toFloat().getCentre();
        row.mouseDown(event(row, origin, origin, juce::ModifierKeys::leftButtonModifier));
        row.mouseDrag(event(row, origin.translated(0, 30), origin, juce::ModifierKeys::leftButtonModifier, true));
        REQUIRE(state::loadStateFromXml(preset, processor));
        row.mouseUp(event(row, origin.translated(0, 30), origin, juce::ModifierKeys::leftButtonModifier, true));
        CHECK(processor.getModuleOrder(scope) == expected);
        nav->refresh();
        for (size_t i = 1; i < rows.size(); ++i)
        {
            const auto byNode = [&](int node) { return rows[node >= fire::module_order::firstInsert ? rows.size() - 1 : static_cast<size_t>(node)]; };
            CHECK(byNode(expected[i - 1])->getY() < byNode(expected[i])->getY());
        }
        const auto directory = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
        if (directory.isNotEmpty())
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
            nav->getViewport().setViewPosition(0, 0);
            for (int frame = 0; frame < 60; ++frame) nav->animationTick(1.0f / 60.0f);
            auto stream = juce::File(directory).getChildFile(scope == 0 ? "master-module-order.png" : "band-module-order.png").createOutputStream();
            REQUIRE(stream != nullptr); stream->setPosition(0); stream->truncate();
            CHECK(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream));
        }
    }
}

TEST_CASE("Compressor rail labels fit beside power and removal controls at every editor scale", "[ui][layout][module-rail][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;processor.hasUpdateCheckBeenPerformed=true;
    REQUIRE(processor.addInsertEffect(0,fire::effects::Type::compressor)==0);
    REQUIRE(processor.addInsertEffect(1,fire::effects::Type::compressor)==0);
    FireAudioProcessorEditor editor(processor);editor.stopTimer();editor.setVisible(true);
    auto findText=[](auto&& self,juce::Component& root,const juce::String& name)->juce::TextButton*
    {
        if(!root.isVisible()) return nullptr;
        if(auto* b=dynamic_cast<juce::TextButton*>(&root);b && b->getButtonText()==name) return b;
        for(auto* child:root.getChildren()) if(auto* b=self(self,*child,name)) return b;
        return nullptr;
    };
    for(int width:{1000,1250,1400,2000})
    {
        editor.setSize(width,width/2);
        const float scale=static_cast<float>(width)/1000;
        for(const auto* workspace:{"BAND LAB","MASTER LAB"})
        {
            auto* tab=findText(findText,editor,workspace);REQUIRE(tab);tab->triggerClick();
            int checked=0;
            auto check=[&](auto&& self,juce::Component& root)->void
            {
                if(!root.isVisible()) return;
                if(auto* row=dynamic_cast<juce::TextButton*>(&root);row && row->getButtonText()=="Compressor"
                    && static_cast<bool>(row->getProperties().getWithDefault("fireModuleRail",false)))
                {
                    const auto font=row->getLookAndFeel().getTextButtonFont(*row,row->getHeight());
                    const auto required=juce::GlyphArrangement::getStringWidth(font,row->getButtonText());
                    auto text=row->getLocalBounds().reduced(juce::roundToInt(7*scale),1);
                    text.removeFromLeft(juce::roundToInt(25*scale));
                    text.removeFromRight(juce::roundToInt(static_cast<float>(row->getProperties().getWithDefault("fireModuleTrailingSpace",0.0f))));
                    CAPTURE(width,workspace,row->getWidth(),text.getWidth(),required);
                    CHECK(static_cast<float>(text.getWidth())>=required);++checked;
                }
                for(auto* child:root.getChildren()) self(self,*child);
            };
            check(check,editor);CHECK(checked>0);
            if(auto* row=findText(findText,editor,"Compressor")) row->triggerClick();
            const auto path=juce::SystemStats::getEnvironmentVariable("FIRE_RAIL_PREVIEW_DIR",{});
            if(path.isNotEmpty())
            {
                auto file=juce::File(path).getChildFile(juce::String(workspace).replaceCharacter(' ','-')+"-"+juce::String(width)+".png");
                file.getParentDirectory().createDirectory();auto output=file.createOutputStream();REQUIRE(output);
                output->setPosition(0);output->truncate();CHECK(juce::PNGImageFormat{}.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*output));
            }
        }
    }
}
