#include <GUI/EffectRackNavigation.h>
#include <GUI/InsertEffectControls.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <vector>

namespace
{
using Rack = fire::ui::EffectRackNavigation;
namespace fx = fire::effects;

std::vector<juce::PopupMenu::Item> items(const juce::PopupMenu& menu)
{
    std::vector<juce::PopupMenu::Item> result;
    for (juce::PopupMenu::MenuItemIterator iterator(menu); iterator.next();)
        if (! iterator.getItem().isSeparator) result.push_back(iterator.getItem());
    return result;
}

juce::String enableID(int scope, int node)
{
    if (scope == 0) return node == 0 ? FILTER_BYPASS_ID : DOWNSAMPLE_BYPASS_ID;
    constexpr std::array<const char*, 5> ids {
        DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, COMP_BYPASS_ID, WIDTH_BYPASS_ID, OTT_ENABLED_ID
    };
    return ParameterIDAndName::getIDString(ids[static_cast<size_t>(node)], scope - 1);
}

struct Fixture
{
    FireAudioProcessor processor;
    FireLookAndFeel look;
    std::array<fire::ui::ModuleDragButton, 5> buttons;
    std::array<PrimaryToggleButton, 5> powers;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 5> attachments;
    std::unique_ptr<Rack> navigation;
    int selectedBuiltin = -1, selectedInsert = -1;

    explicit Fixture(int scope)
    {
        navigation = std::make_unique<Rack>(processor, scope);
        navigation->setLookAndFeel(&look);
        const auto count = scope == 0 ? 3 : 5;
        std::vector<Rack::Row> rows;
        for (int node = 0; node < count; ++node)
        {
            auto& button = buttons[static_cast<size_t>(node)];
            button.setButtonText("Module " + juce::String(node));
            button.setClickingTogglesState(true);
            button.setRadioGroupId(731);
            button.onClick = [this, node]
            {
                if (! buttons[static_cast<size_t>(node)].getToggleState()) return;
                selectedBuiltin = node;
                selectedInsert = -1;
                if (navigation) navigation->setSelectedSlot(-1);
            };
            if (scope == 0 && node == 2)
            {
                rows.push_back({ &button, nullptr });
                continue;
            }
            const auto id = enableID(scope, node);
            auto* parameter = processor.treeState.getParameter(id);
            REQUIRE(parameter != nullptr);
            parameter->setValueNotifyingHost(0.0f);
            auto& power = powers[static_cast<size_t>(node)];
            power.setClickingTogglesState(true);
            attachments[static_cast<size_t>(node)] =
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, id, power);
            rows.push_back({ &button, &power });
        }
        navigation->setBuiltins(std::move(rows));
        navigation->onSelectEffect = [this](int slot)
        {
            selectedInsert = slot;
            selectedBuiltin = -1;
            if (navigation) navigation->setSelectedSlot(slot);
        };
        navigation->setBounds(0, 0, 210, 300);
        navigation->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        navigation->setVisible(true);
        REQUIRE(navigation->isShowing());
    }

    ~Fixture()
    {
        if (navigation)
        {
            navigation->onSelectEffect = nullptr;
            navigation->dismiss();
            navigation->setLookAndFeel(nullptr);
            navigation.reset();
        }
    }

    int insertCount(int scope) const
    {
        int result = 0;
        for (int slot = 0; slot < fx::slotCount; ++slot)
            if (processor.getInsertEffectType(scope, slot) != fx::Type::none) ++result;
        return result;
    }
};

struct Gestures : juce::AudioProcessorParameter::Listener
{
    int starts = 0, ends = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { if (starting) ++starts; else ++ends; }
};

juce::TextButton* findButton(juce::Component& root, const juce::String& id)
{
    if (auto* button = dynamic_cast<juce::TextButton*>(&root); button && button->getComponentID() == id) return button;
    for (auto* child : root.getChildren()) if (auto* result = findButton(*child, id)) return result;
    return nullptr;
}
}

TEST_CASE("Chain menus expose existing builtins and a single canonical Master Lo-Fi",
          "[chain][insertfx][ui][menu][builtins]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture band(1), master(0);
    juce::StringArray bandNames, masterNames;
    for (const auto& item : items(band.navigation->createAddMenu())) bandNames.add(item.text);
    for (const auto& item : items(master.navigation->createAddMenu())) masterNames.add(item.text);
    CHECK(bandNames == juce::StringArray { "Drive", "Shape", "Compressor", "OTT", "Stereo",
                                         "Chorus", "Delay", "Reverb", "Granular", "Lo-Fi" });
    CHECK(masterNames == juce::StringArray { "EQ", "Lo-Fi", "Chorus", "Delay", "Reverb", "Granular" });
    for (const auto& item : items(master.navigation->createAddMenu()))
        if (item.text == "Lo-Fi") CHECK(item.itemID == Rack::builtinMenuItemID(1));
    CHECK_FALSE(master.navigation->activateBuiltin(2)); // Analysis remains a view-only row.
}

TEST_CASE("Builtin menu selections enable once select the existing row and preserve the chain",
          "[chain][insertfx][ui][menu][builtins][gestures]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture(1);
    const auto order = fixture.processor.getModuleOrder(1);
    for (int node = 0; node < 5; ++node)
    {
        CAPTURE(node);
        auto* parameter = fixture.processor.treeState.getParameter(enableID(1, node));
        REQUIRE(parameter != nullptr);
        Gestures gestures;
        parameter->addListener(&gestures);
        const juce::ScopeGuard detach { [&] { parameter->removeListener(&gestures); } };
        for (int repeat = 0; repeat < 2; ++repeat)
            fixture.navigation->createAddMenuResultHandler()(Rack::builtinMenuItemID(node));
        CHECK(parameter->getValue() == 1.0f);
        CHECK(fixture.selectedBuiltin == node);
        CHECK(fixture.selectedInsert == -1);
        CHECK(gestures.starts == 1);
        CHECK(gestures.ends == 1);
        CHECK(fixture.insertCount(1) == 0);
        CHECK(fixture.processor.getModuleOrder(1) == order);
    }
}

TEST_CASE("Full insert racks still allow their builtins and Master Lo-Fi consumes no slot",
          "[chain][insertfx][ui][menu][capacity][lofi]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture(0);
    for (int slot = 0; slot < fx::slotCount; ++slot)
        REQUIRE(fixture.processor.addInsertEffect(0, fx::Type::delay) == slot);
    fixture.navigation->refresh();
    fixture.navigation->setSelectedSlot(7);
    for (const auto& item : items(fixture.navigation->createAddMenu()))
    {
        if (item.itemID < 100) CHECK_FALSE(item.isEnabled);
        else CHECK(item.isEnabled);
    }
    const auto order = fixture.processor.getModuleOrder(0);
    fixture.navigation->createAddMenuResultHandler()(Rack::builtinMenuItemID(1));
    CHECK(fixture.processor.treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID)->load() == 1.0f);
    CHECK(fixture.selectedBuiltin == 1);
    const auto viewTop = fixture.navigation->getViewport().getViewPositionY();
    CHECK(fixture.buttons[1].getY() >= viewTop);
    CHECK(fixture.buttons[1].getBottom() <= viewTop + fixture.navigation->getViewport().getHeight());
    CHECK(fixture.insertCount(0) == fx::slotCount);
    CHECK(fixture.processor.getModuleOrder(0) == order);
}

TEST_CASE("Chain menu results cannot replay across sessions scope visibility or enablement",
          "[chain][insertfx][ui][menu][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture(1);
    auto oldResult = fixture.navigation->createAddMenuResultHandler();
    SECTION("a newer menu") { fixture.navigation->createAddMenuResultHandler(); }
    SECTION("scope ABA") { fixture.navigation->setScope(2); fixture.navigation->setScope(1); }
    SECTION("hidden ABA") { fixture.navigation->setVisible(false); fixture.navigation->setVisible(true); }
    SECTION("disabled ABA") { fixture.navigation->setEnabled(false); fixture.navigation->setEnabled(true); }
    SECTION("dismissed") { fixture.navigation->dismiss(); }
    SECTION("completed once")
    {
        oldResult(Rack::builtinMenuItemID(1));
        REQUIRE(fixture.selectedBuiltin == 1);
        fixture.processor.treeState.getParameter(enableID(1, 1))->setValueNotifyingHost(0.0f);
    }
    oldResult(Rack::builtinMenuItemID(1));
    oldResult(static_cast<int>(fx::Type::delay));
    CHECK(fixture.processor.treeState.getRawParameterValue(enableID(1, 1))->load() == 0.0f);
    CHECK(fixture.insertCount(1) == 0);
    CHECK(fixture.insertCount(2) == 0);
}

TEST_CASE("Builtin activation tolerates synchronous scope changes and owner destruction",
          "[chain][insertfx][ui][menu][callback-safety]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture(1);
    auto result = fixture.navigation->createAddMenuResultHandler();
    SECTION("scope changed while enabling")
    {
        fixture.powers[1].onClick = [&] { fixture.navigation->setScope(2); };
        result(Rack::builtinMenuItemID(1));
        CHECK(fixture.selectedBuiltin == -1);
        CHECK(fixture.navigation->getScope() == 2);
    }
    SECTION("owner destroyed while enabling")
    {
        fixture.powers[1].onClick = [&] { fixture.navigation->setLookAndFeel(nullptr); fixture.navigation.reset(); };
        result(Rack::builtinMenuItemID(1));
        CHECK(fixture.navigation == nullptr);
    }
}

TEST_CASE("Historical insert Lo-Fi stays selectable and uses the shared Lo-Fi control grouping",
          "[chain][insertfx][ui][lofi][compatibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture fixture(0);
    REQUIRE(fixture.processor.addInsertEffect(0, fx::Type::lofi) == 0);
    fixture.navigation->refresh();
    auto* row = findButton(*fixture.navigation, fx::parameterID(0, 0, fx::typeField));
    REQUIRE(row != nullptr);
    row->triggerClick();
    CHECK(fixture.selectedInsert == 0);
    CHECK(fixture.processor.getInsertEffectType(0, 0) == fx::Type::lofi);

    std::array<ModulatableSlider, 6> sliders;
    std::array<ModulatableSlider*, 6> pointers;
    for (size_t i = 0; i < sliders.size(); ++i) pointers[i] = &sliders[i];
    InsertEffectControls controls(fixture.processor);
    controls.setControls(pointers);
    controls.setBounds(0, 0, 600, 240);
    controls.bind(0, 0);
    CHECK(controls.usesFullWidthLayout());
    CHECK_FALSE(controls.usesExpandedLayout());
    CHECK(sliders[0].getY() == sliders[1].getY());
    CHECK(sliders[1].getY() == sliders[5].getY());
    CHECK(sliders[2].getY() == sliders[3].getY());
    CHECK(sliders[3].getY() == sliders[4].getY());
    CHECK(sliders[2].getY() > sliders[0].getY());
    CHECK(sliders[0].getX() < sliders[1].getX());
    CHECK(sliders[1].getX() < sliders[5].getX());
    for (size_t i = 0; i < sliders.size(); ++i)
        CHECK(sliders[i].parameterID == fx::parameterID(0, 0, static_cast<int>(i)));
    CHECK(sliders[0].getTextFromValue(1.0).contains("32"));
    CHECK(sliders[1].getTextFromValue(1.0).contains("24"));
}
