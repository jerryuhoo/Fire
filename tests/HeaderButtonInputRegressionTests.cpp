#include <GUI/PrimaryButton.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>

struct PrimaryButtonTestAccess
{
    template <typename ButtonType>
    static void setTrackedPointerSource(
        PrimaryPointerButton<ButtonType>& button,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        button.pointerSourceType = type;
        button.pointerSourceIndex = index;
    }
};

namespace
{
juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::ModifierKeys modifiers,
                                bool wasDragged = false)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
    const auto time = juce::Time::getCurrentTime();
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
             time,
             position,
             time,
             1,
             wasDragged };
}

void beginPointerGesture(juce::Button& button,
                         juce::ModifierKeys modifiers)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(component, modifiers));
}

void dragPointerGesture(juce::Button& button,
                        juce::ModifierKeys modifiers)
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseDrag(makeMouseEvent(component, modifiers, true));
}

void endPointerGesture(juce::Button& button,
                       juce::ModifierKeys modifiers = {})
{
    auto& component = static_cast<juce::Component&>(button);
    component.mouseUp(makeMouseEvent(component, modifiers));
}

void performPointerGesture(juce::Button& button,
                           juce::ModifierKeys downModifiers,
                           juce::ModifierKeys upModifiers = {})
{
    beginPointerGesture(button, downModifiers);
    endPointerGesture(button, upModifiers);
}

template <typename Callback>
void forEachPrimaryButtonType(Callback&& callback)
{
    PrimaryTextButton textButton { "Text" };
    textButton.setBounds(0, 0, 80, 24);
    textButton.setVisible(true);
    callback(textButton);

    PrimaryToggleButton toggleButton { "Toggle" };
    toggleButton.setBounds(0, 0, 80, 24);
    toggleButton.setVisible(true);
    callback(toggleButton);
}

std::vector<juce::ModifierKeys> rejectedPointerModifiers()
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

void collectHeaderButtons(juce::Component& component,
                          std::vector<PrimaryTextButton*>& result)
{
    // Inc/Dec Sliders also use PrimaryTextButton children, but this helper is
    // intentionally scoped to editor header and preset actions.
    if (dynamic_cast<juce::Slider*>(&component) != nullptr)
        return;

    if (auto* button = dynamic_cast<PrimaryTextButton*>(&component))
    {
        const auto componentId = button->getComponentID();
        if (componentId.startsWith("header_")
            || componentId == "workspace_tab"
            || componentId == "zoom")
            result.push_back(button);
    }

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        if (auto* child = component.getChildComponent(childIndex))
            collectHeaderButtons(*child, result);
}
} // namespace

TEST_CASE("Primary buttons reject popup and auxiliary pointer gestures",
          "[header-button][ui][input][primary-button]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const auto modifiers : rejectedPointerModifiers())
    {
        CAPTURE(modifiers.getRawFlags());
        forEachPrimaryButtonType([modifiers](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };

            beginPointerGesture(button, modifiers);
            dragPointerGesture(button, modifiers);
            endPointerGesture(button);

            CHECK_FALSE(button.isDown());
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);
        });
    }
}

TEST_CASE("Primary buttons own complete primary pointer gestures",
          "[header-button][ui][input][primary-button][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("a primary click toggles exactly once")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };

            performPointerGesture(button, leftButton);

            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
            CHECK_FALSE(button.isDown());
        });
    }

    SECTION("a new down replaces stale ownership from the same source")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };

            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());
            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());
            endPointerGesture(button);

            CHECK(clickCount == 1);
            CHECK_FALSE(button.isDown());
        });
    }

    SECTION("a primary down replaces a stale rejected gesture")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };

            beginPointerGesture(
                button,
                juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier });
            beginPointerGesture(button, leftButton);
            endPointerGesture(button);

            CHECK(clickCount == 1);
            CHECK_FALSE(button.isDown());
        });
    }

    SECTION("drag and release from a different source cannot steal ownership")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());

            const auto source = juce::Desktop::getInstance().getMainMouseSource();
            PrimaryButtonTestAccess::setTrackedPointerSource(
                button,
                source.getType() == juce::MouseInputSource::mouse
                    ? juce::MouseInputSource::touch
                    : juce::MouseInputSource::mouse,
                source.getIndex() + 1);
            dragPointerGesture(button, leftButton);
            endPointerGesture(button);

            CHECK(button.isDown());
            CHECK(clickCount == 0);

            PrimaryButtonTestAccess::setTrackedPointerSource(
                button, source.getType(), source.getIndex());
            endPointerGesture(button);

            CHECK_FALSE(button.isDown());
            CHECK(clickCount == 1);
        });
    }
}

TEST_CASE("Primary buttons discard gestures at visibility and enablement boundaries",
          "[header-button][ui][input][primary-button][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("hide and show")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };

            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());
            button.setVisible(false);
            CHECK_FALSE(button.isDown());
            button.setVisible(true);
            endPointerGesture(button);

            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);

            performPointerGesture(button, leftButton);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("disable and enable")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };

            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());
            button.setEnabled(false);
            CHECK_FALSE(button.isDown());
            button.setEnabled(true);
            endPointerGesture(button);

            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);

            performPointerGesture(button, leftButton);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }
}

TEST_CASE("Primary buttons preserve keyboard and programmatic activation",
          "[header-button][ui][input][primary-button][keyboard]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("triggerClick")
    {
        forEachPrimaryButtonType([](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };
            button.triggerClick();
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("Return key")
    {
        forEachPrimaryButtonType([](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };

            REQUIRE(static_cast<juce::Component&>(button).keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey }));
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("Return callback may synchronously delete the button")
    {
        auto button = std::make_unique<PrimaryTextButton>("Delete");
        button->onClick = [&button] { button.reset(); };
        auto* rawButton = button.get();

        CHECK(rawButton->keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK(button == nullptr);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }
}

TEST_CASE("Editor header and preset actions use primary-only buttons",
          "[header-button][ui][input][editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    std::vector<PrimaryTextButton*> buttons;
    collectHeaderButtons(*editor, buttons);
    REQUIRE(buttons.size() == 11);

    std::map<juce::String, int> componentIdCounts;
    for (const auto* button : buttons)
        ++componentIdCounts[button->getComponentID()];

    CHECK(componentIdCounts["header_hq"] == 1);
    CHECK(componentIdCounts["workspace_tab"] == 3);
    CHECK(componentIdCounts["zoom"] == 1);
    CHECK(componentIdCounts["header_ab"] == 1);
    CHECK(componentIdCounts["header_action"] == 2);
    CHECK(componentIdCounts["header_previous"] == 1);
    CHECK(componentIdCounts["header_next"] == 1);
    CHECK(componentIdCounts["header_menu"] == 1);

    auto* zoom = dynamic_cast<PrimaryTextButton*>(editor->findChildWithID("zoom"));
    REQUIRE(zoom != nullptr);
    CHECK(zoom->getTitle() == "Toggle spectrum zoom");
    CHECK(zoom->getTooltip() == "Toggle spectrum zoom");

    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    for (auto* button : buttons)
    {
        CAPTURE(button->getComponentID(), button->getButtonText());
        int clickCount = 0;
        button->onClick = [&clickCount] { ++clickCount; };
        beginPointerGesture(*button, leftButton);
        REQUIRE(button->isDown());

        editor->setVisible(false);
        CHECK_FALSE(button->isDown());
        editor->setVisible(true);
        endPointerGesture(*button);

        CHECK_FALSE(button->isDown());
        CHECK(clickCount == 0);
        button->onClick = nullptr;
    }
}
