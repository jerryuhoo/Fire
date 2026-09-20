#include <PluginEditor.h>
#include <GUI/EffectRackNavigation.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
template <typename T, typename Predicate>
T* find(juce::Component& root, Predicate predicate)
{
    if (auto* component = dynamic_cast<T*>(&root); component && predicate(*component)) return component;
    for (auto* child : root.getChildren()) if (auto* result = find<T>(*child, predicate)) return result;
    return nullptr;
}
void pump() {juce::MessageManager::getInstance()->runDispatchLoopUntil(40);}
void click(juce::Component& root, const juce::String& text)
{
    auto* button = find<juce::Button>(root, [&](auto& b) {return b.getButtonText() == text;});
    REQUIRE(button != nullptr); button->triggerClick(); pump();
}
juce::Component* popup(juce::Component& root)
{
    return find<juce::Component>(root, [](auto& c) {return c.getName() == "menu";});
}
juce::Component* menuItem(juce::Component& root, const juce::String& text)
{
    return find<juce::Component>(root, [&](auto& c) {
        auto* accessibility = c.getAccessibilityHandler();
        return accessibility && accessibility->getTitle() == text;
    });
}
void addFromMenu(FireAudioProcessorEditor& editor, const juce::String& buttonID, const juce::String& effect)
{
    juce::Process::makeForegroundProcess(); editor.toFront(true);
    editor.grabKeyboardFocus();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    CAPTURE(effect, juce::Process::isForegroundProcess());
    auto* button = find<juce::Button>(editor, [&](auto& b) {return b.getComponentID() == buttonID;});
    REQUIRE(button != nullptr); button->triggerClick();
    auto* menu = popup(editor);
    REQUIRE(menu != nullptr);
    auto* item = menuItem(*menu, effect);
    REQUIRE(item != nullptr);
    REQUIRE(item->getAccessibilityHandler()->getActions().invoke(juce::AccessibilityActionType::press));
    pump();
}
void save(juce::Component& c, const juce::String& name)
{
    const auto directory = juce::SystemStats::getEnvironmentVariable("FIRE_UI_SNAPSHOT_DIR", {});
    if (directory.isEmpty()) return;
    juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
    auto stream = juce::File(directory).getChildFile(name).createOutputStream();
    REQUIRE(stream != nullptr); stream->setPosition(0); stream->truncate();
    CHECK(juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()), *stream));
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> point, juce::ModifierKeys mods)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(), point, mods, 0, 0, 0, 0, 0, &c, &c, now, point, now, 1, false};
}
}

TEST_CASE("Master plus menu adds repeated effects and keeps five fixed rows in the scrolling rail", "[insertfx][ui][menu][layout]")
{
    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr)
        SKIP("Native menu test is run separately in a macOS display session");
    FireAudioProcessor p; p.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(p);
    editor.setSize(1000, 500); editor.addToDesktop(juce::ComponentPeer::windowIsTemporary); editor.setVisible(true);
    const juce::ScopeGuard menus {[] {juce::PopupMenu::dismissAllActiveMenus();}};
    click(editor, "MASTER LAB");
    auto* nav = find<fire::ui::EffectRackNavigation>(editor, [](auto& n) {return n.getScope() == 0;});
    REQUIRE(nav != nullptr);
    const auto rowPitch = nav->getRowPitch();
    CHECK(rowPitch * 5 <= nav->getViewport().getHeight());
    CHECK(nav->getViewport().getHeight() - rowPitch * 5 < 5);
    addFromMenu(editor, "addMasterEffect", "Chorus");
    addFromMenu(editor, "addMasterEffect", "Delay");
    addFromMenu(editor, "addMasterEffect", "Reverb");
    addFromMenu(editor, "addMasterEffect", "Granular");
    addFromMenu(editor, "addMasterEffect", "Delay");
    CHECK(p.getInsertEffectType(0, 0) == fire::effects::Type::chorus);
    CHECK(p.getInsertEffectType(0, 4) == fire::effects::Type::delay);
    CHECK(nav->getRowPitch() == rowPitch);
    CHECK(nav->getViewport().getViewPositionY() > 0);
    const auto id = fire::effects::parameterID(0, 4, 0);
    auto* time = find<ModulatableSlider>(editor, [&](auto& s) {return s.parameterID == id;});
    REQUIRE(time != nullptr); REQUIRE(time->isShowing());
    CHECK(time->getTextFromValue(time->getValue()).contains("375"));
    const auto value = time->getValueFromText("750 ms");
    time->setValue(value, juce::sendNotificationSync);
    CHECK(fire::effects::controls(fire::effects::Type::delay)[0].fromNormalised(p.treeState.getRawParameterValue(id)->load()) == Catch::Approx(750).margin(0.1));
    save(editor, "master-insert-delay.png");
    for (int width : {1000, 2000})
    {
        editor.setSize(width, width / 2); pump();
        CHECK(nav->getRowPitch() * 5 <= nav->getViewport().getHeight());
        for (int control = 0; control < 6; ++control)
        {
            auto* slider = find<ModulatableSlider>(editor, [&](auto& s) {return s.parameterID == fire::effects::parameterID(0, 4, control);});
            REQUIRE(slider != nullptr); CHECK(slider->isShowing());
            CHECK(editor.getLocalBounds().contains(editor.getLocalArea(slider, slider->getLocalBounds())));
        }
    }
}

TEST_CASE("Band insert gestures close before scope changes and menus cannot target a hidden workspace", "[insertfx][ui][lifecycle]")
{
    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr)
        SKIP("Native menu test is run separately in a macOS display session");
    FireAudioProcessor p; p.hasUpdateCheckBeenPerformed = true;
    auto* bands = p.treeState.getParameter(NUM_BANDS_ID);
    bands->setValueNotifyingHost(bands->convertTo0to1(2));
    p.treeState.getParameter("lineState1")->setValueNotifyingHost(1);
    FireAudioProcessorEditor editor(p);
    editor.setSize(1000, 500); editor.addToDesktop(juce::ComponentPeer::windowIsTemporary); editor.setVisible(true);
    const juce::ScopeGuard menus {[] {juce::PopupMenu::dismissAllActiveMenus();}};
    pump();
    addFromMenu(editor, "addBandEffect", "Granular");
    REQUIRE(p.getInsertEffectType(1, 0) == fire::effects::Type::granular);
    auto* panel = find<BandPanel>(editor, [](auto&) {return true;});
    REQUIRE(panel != nullptr);
    auto* slider = find<ModulatableSlider>(*panel, [](auto& s) {return s.parameterID == fire::effects::parameterID(1, 0, 0);});
    REQUIRE(slider != nullptr);
    const auto point = slider->getLocalBounds().toFloat().getCentre();
    slider->mouseDown(event(*slider, point, juce::ModifierKeys::leftButtonModifier));
    REQUIRE(slider->hasActiveInteraction());
    click(editor, "MASTER LAB");
    CHECK_FALSE(slider->hasActiveInteraction());
    click(editor, "BAND LAB");
    panel->setFocusBandNum(1);
    CHECK(p.getInsertEffectType(1, 0) == fire::effects::Type::granular);
    CHECK(p.getInsertEffectType(2, 0) == fire::effects::Type::none);
    addFromMenu(editor, "addBandEffect", "Reverb");
    CHECK(p.getInsertEffectType(2, 0) == fire::effects::Type::reverb);
    save(editor, "band-insert-reverb.png");
    auto* plus = find<juce::Button>(editor, [](auto& b) {return b.getComponentID() == "addBandEffect";});
    juce::Process::makeForegroundProcess(); editor.toFront(true); editor.grabKeyboardFocus();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    plus->triggerClick();
    auto* menu = popup(editor); REQUIRE(menu != nullptr);
    auto* item = menuItem(*menu, "Delay"); REQUIRE(item != nullptr);
    juce::Component::SafePointer<juce::Component> safeItem(item);
    click(editor, "MASTER LAB");
    if (safeItem)
        if (auto* handler = safeItem->getAccessibilityHandler()) handler->getActions().invoke(juce::AccessibilityActionType::press);
    pump();
    CHECK(p.getInsertEffectType(2, 1) == fire::effects::Type::none);
    CHECK(p.getInsertEffectType(0, 0) == fire::effects::Type::none);
}

TEST_CASE("Master Lo-Fi exposes tape wow and flutter while switching modules hides their controls", "[insertfx][ui][lofi]")
{
    FireAudioProcessor p; p.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(p);
    editor.setSize(1400, 700); editor.addToDesktop(juce::ComponentPeer::windowIsTemporary); editor.setVisible(true);
    click(editor, "MASTER LAB"); click(editor, "Lo-Fi");
    for (auto* id : fire::effects::tapeIDs)
    {
        auto* slider = find<ModulatableSlider>(editor, [&](auto& s) {return s.parameterID == id;});
        REQUIRE(slider != nullptr); CHECK(slider->isShowing());
        CHECK(slider->getValueReadoutOpacity() == 0);
        slider->setValue(0.5, juce::sendNotificationSync);
        CHECK(p.treeState.getRawParameterValue(id)->load() == Catch::Approx(0.5f));
    }
    save(editor, "master-lofi-tape.png");
    click(editor, "EQ");
    for (auto* id : fire::effects::tapeIDs)
        CHECK_FALSE(find<ModulatableSlider>(editor, [&](auto& s) {return s.parameterID == id;})->isShowing());
}
