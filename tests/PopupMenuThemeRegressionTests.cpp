#include <GUI/FireTheme.h>
#include <GUI/LookAndFeel.h>
#include <GUI/ModulatableSlider.h>
#include <Panels/ControlPanel/LfoPanel.h>

#include <catch2/catch_test_macros.hpp>

namespace
{
juce::MouseEvent makePopupMouseUp(juce::Component& component,
                                  juce::Point<float> position)
{
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             time,
             position,
             time,
             1,
             false };
}

juce::Component* findPopupMenu(juce::Component& root)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (child != nullptr && child->getName() == "menu")
            return child;

        if (child != nullptr)
            if (auto* nested = findPopupMenu(*child))
                return nested;
    }

    return nullptr;
}

void dismissMenus()
{
    juce::PopupMenu::dismissAllActiveMenus();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
}
} // namespace

TEST_CASE("Fire context menus inherit their target theme and cursor anchor",
          "[ui][popup-menu][theme][anchor]")
{
    FireLookAndFeel lookAndFeel;
    lookAndFeel.scale = 1.5f;

    juce::Component root;
    root.setBounds(0, 0, 1000, 500);
    root.setLookAndFeel(&lookAndFeel);
    root.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    root.setVisible(true);

    SECTION("shared options retain the target, editor parent, and cursor")
    {
        juce::Component target;
        root.addAndMakeVisible(target);
        target.setBounds(100, 80, 120, 90);
        juce::PopupMenu menu;
        const auto cursor = root.localPointToGlobal(
            juce::Point<int> { 157, 113 });
        const auto options = fire::ui::prepareContextMenu(menu, target, cursor);

        CHECK(options.getTargetComponent() == &target);
        CHECK(options.getTopLevelTargetComponent() == &target);
        CHECK(options.getParentComponent() == &root);
        CHECK(options.getTargetScreenArea()
              == juce::Rectangle<int>(cursor.x, cursor.y, 1, 1));
        CHECK_FALSE(options.hasWatchedComponentBeenDeleted());
        CHECK_FALSE(lookAndFeel.findColour(
            juce::PopupMenu::backgroundColourId).isOpaque());
    }

    SECTION("slider assignment menu is an editor-owned themed child")
    {
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()
            == nullptr)
        {
            SUCCEED("Headless runner has no display for a real PopupMenu window");
            return;
        }

        ModulatableSlider slider;
        slider.parameterID = "drive0";
        root.addAndMakeVisible(slider);
        slider.setBounds(120, 70, 100, 120);

        slider.mouseUp(makePopupMouseUp(
            slider, slider.getLocalBounds().toFloat().getCentre()));
        const juce::ScopeGuard cleanup { [] { dismissMenus(); } };

        auto* popup = findPopupMenu(root);
        REQUIRE(popup != nullptr);
        CHECK(popup->getParentComponent() == &root);
        CHECK(&popup->getLookAndFeel() == &lookAndFeel);
        CHECK(popup->isOpaque()
              == ! juce::Desktop::canUseSemiTransparentWindows());
    }

    SECTION("LFO edit menu uses the same editor-owned theme")
    {
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()
            == nullptr)
        {
            SUCCEED("Headless runner has no display for a real PopupMenu window");
            return;
        }

        LfoEditor editor;
        root.addAndMakeVisible(editor);
        editor.setBounds(260, 70, 500, 300);

        editor.mouseUp(makePopupMouseUp(
            editor, editor.getLocalBounds().toFloat().getCentre()));
        const juce::ScopeGuard cleanup { [] { dismissMenus(); } };

        auto* popup = findPopupMenu(root);
        REQUIRE(popup != nullptr);
        CHECK(popup->getParentComponent() == &root);
        CHECK(&popup->getLookAndFeel() == &lookAndFeel);
        CHECK(popup->isOpaque()
              == ! juce::Desktop::canUseSemiTransparentWindows());
    }

    dismissMenus();
    root.setLookAndFeel(nullptr);
}
