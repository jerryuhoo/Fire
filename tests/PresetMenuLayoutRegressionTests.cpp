#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>

namespace
{
juce::Button* findButtonWithID(juce::Component& root,
                               const juce::String& componentID)
{
    if (auto* button = dynamic_cast<juce::Button*>(&root);
        button != nullptr && button->getComponentID() == componentID)
        return button;

    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
        if (auto* child = root.getChildComponent(childIndex))
            if (auto* match = findButtonWithID(*child, componentID))
                return match;

    return nullptr;
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

state::StateComponent* findStateComponent(juce::Component* component)
{
    for (auto* candidate = component; candidate != nullptr;
         candidate = candidate->getParentComponent())
        if (auto* stateComponent = dynamic_cast<state::StateComponent*>(candidate))
            return stateComponent;

    return nullptr;
}

void dismissMenus()
{
    juce::PopupMenu::dismissAllActiveMenus();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
}

void checkPresetMenuAtEditorSize(int width, int height, float expectedScale)
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, width, height);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    auto* menuButton = findButtonWithID(*editor, "header_menu");
    REQUIRE(menuButton != nullptr);
    auto* stateComponent = findStateComponent(menuButton);
    REQUIRE(stateComponent != nullptr);

    const auto options = stateComponent->getPresetMenuOptionsForTesting();
    CHECK(options.getTargetComponent() == menuButton);
    CHECK(options.getTopLevelTargetComponent() == menuButton);
    CHECK(options.getParentComponent() == editor.get());
    CHECK(options.getTargetScreenArea() == menuButton->getScreenBounds());
    CHECK(options.getMinimumWidth()
          == juce::roundToInt(250.0f * expectedScale));
    CHECK(options.getStandardItemHeight()
          == juce::roundToInt(30.0f * expectedScale));
    CHECK_FALSE(options.hasWatchedComponentBeenDeleted());

    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()
        == nullptr)
    {
        SUCCEED("Headless runner has no display for a real PopupMenu window");
        return;
    }

    menuButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    const juce::ScopeGuard cleanup { [] { dismissMenus(); } };

    auto* popup = findPopupMenu(*editor);
    REQUIRE(popup != nullptr);
    CHECK(popup->getParentComponent() == editor.get());
    CHECK(popup->getWidth() >= options.getMinimumWidth());

    auto* menuLookAndFeel = dynamic_cast<FireLookAndFeel*>(&popup->getLookAndFeel());
    REQUIRE(menuLookAndFeel != nullptr);
    CHECK(menuLookAndFeel->scale == Catch::Approx(expectedScale));

    const auto popupScreenBounds = popup->getScreenBounds();
    const auto targetScreenBounds = menuButton->getScreenBounds();
    CHECK(std::abs(popupScreenBounds.getY() - targetScreenBounds.getBottom()) <= 1);
}
} // namespace

TEST_CASE("Preset menu uses editor scale and remains anchored to its button",
          "[preset][ui][popup-menu][scale][anchor]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("minimum editor size uses one-to-one menu metrics")
    {
        checkPresetMenuAtEditorSize(1000, 500, 1.0f);
    }

    SECTION("maximum editor size doubles all menu metrics")
    {
        checkPresetMenuAtEditorSize(2000, 1000, 2.0f);
    }
}

TEST_CASE("Preset menu options watch their owner for deletion",
          "[preset][ui][popup-menu][lifetime]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    juce::Component editorHost;
    editorHost.setBounds(0, 0, 1000, 500);

    auto stateComponent = std::make_unique<state::StateComponent>(
        processor.stateAB, processor.statePresets, processor.treeState);
    editorHost.addAndMakeVisible(*stateComponent);
    stateComponent->setBounds(150, 10, 700, 40);

    const auto options = stateComponent->getPresetMenuOptionsForTesting();
    CHECK_FALSE(options.hasWatchedComponentBeenDeleted());
    stateComponent.reset();
    CHECK(options.hasWatchedComponentBeenDeleted());
}

TEST_CASE("Preset browser exposes a descriptive accessible identity",
          "[preset][ui][accessibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    state::StateComponent stateComponent(
        processor.stateAB, processor.statePresets, processor.treeState);
    stateComponent.setBounds(0, 0, 520, 64);
    stateComponent.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    stateComponent.setVisible(true);
    auto* presetBox = stateComponent.getPresetBox();
    REQUIRE(presetBox != nullptr);
    CHECK(presetBox->getTitle() == "Preset browser");
    CHECK(presetBox->getTooltip() == "Select a preset");

    auto* accessibility = presetBox->getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    CHECK(accessibility->getTitle() == "Preset browser");

    auto* toggleAB = stateComponent.getToggleABButton();
    REQUIRE(toggleAB != nullptr);
    CHECK(toggleAB->getTitle() == "A/B state");
    CHECK(toggleAB->getTooltip() == "Switch between the A and B states");
    auto* toggleAccessibility = toggleAB->getAccessibilityHandler();
    REQUIRE(toggleAccessibility != nullptr);
    CHECK(toggleAccessibility->getTitle() == "A/B state");
    CHECK(toggleAccessibility->getHelp() == toggleAB->getTooltip());

    auto* copyAB = stateComponent.getCopyABButton();
    REQUIRE(copyAB != nullptr);
    CHECK(copyAB->getTitle() == "Copy A/B state");
    CHECK(copyAB->getTooltip()
          == "Copy the current state to the other A/B slot");
    auto* copyAccessibility = copyAB->getAccessibilityHandler();
    REQUIRE(copyAccessibility != nullptr);
    CHECK(copyAccessibility->getTitle() == "Copy A/B state");
    CHECK(copyAccessibility->getHelp() == copyAB->getTooltip());
}
