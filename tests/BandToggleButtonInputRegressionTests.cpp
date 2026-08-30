#include <Panels/SpectrogramPanel/EnableButton.h>
#include <Panels/SpectrogramPanel/SoloButton.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>
#include <memory>

struct BandToggleButtonPointerTestAccess
{
    template <typename ButtonType>
    static bool hasPrimaryPointer(const ButtonType& button)
    {
        return button.primaryPointerDown;
    }

    template <typename ButtonType>
    static void setTrackedPointerSource(
        ButtonType& button,
        juce::MouseInputSource::InputSourceType sourceType,
        int sourceIndex)
    {
        button.pointerSourceType = sourceType;
        button.pointerSourceIndex = sourceIndex;
    }

    template <typename ButtonType>
    static float hover(const ButtonType& button)
    {
        return button.hoverAnimation.current;
    }

    template <typename ButtonType>
    static float press(const ButtonType& button)
    {
        return button.pressAnimation.current;
    }

    template <typename ButtonType>
    static float enabled(const ButtonType& button)
    {
        return button.enabledAnimation.current;
    }

    template <typename ButtonType>
    static void advance(ButtonType& button, float seconds)
    {
        button.advanceAnimation(seconds);
    }

    template <typename ButtonType>
    static void notifyEnablementChanged(ButtonType& button)
    {
        button.enablementChanged();
    }

    template <typename ButtonType>
    static void notifyFocusGained(ButtonType& button)
    {
        button.focusGained(juce::Component::focusChangedDirectly);
    }

    template <typename ButtonType>
    static void notifyFocusLost(ButtonType& button)
    {
        button.focusLost(juce::Component::focusChangedDirectly);
    }

    template <typename ButtonType>
    static void runTimerCallback(ButtonType& button)
    {
        button.timerCallback();
    }
};

namespace
{
template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root);
        match != nullptr && match->isVisible() && ! match->getBounds().isEmpty())
        return match;

    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
        if (auto* child = root.getChildComponent(childIndex))
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;

    return nullptr;
}

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

template <typename Callback>
void forEachBandToggle(Callback&& callback)
{
    {
        INFO("SoloButton");
        SoloButton button;
        button.setBounds(0, 0, 24, 24);
        button.setVisible(true);
        callback(button);
    }

    {
        INFO("EnableButton");
        EnableButton button;
        button.setBounds(0, 0, 24, 24);
        button.setVisible(true);
        callback(button);
    }
}

template <typename ButtonType>
void checkRejectedGesture(ButtonType& button,
                          juce::ModifierKeys downModifiers,
                          juce::ModifierKeys upModifiers = {})
{
    int clickCount = 0;
    button.onClick = [&clickCount] { ++clickCount; };
    auto& component = static_cast<juce::Component&>(button);

    component.mouseDown(makeMouseEvent(button, downModifiers));
    CHECK_FALSE(button.isDown());
    component.mouseDrag(makeMouseEvent(button, downModifiers, true));
    CHECK_FALSE(button.isDown());
    component.mouseUp(makeMouseEvent(button, upModifiers, true));

    CHECK_FALSE(button.getToggleState());
    CHECK(clickCount == 0);
}

template <typename ButtonType>
void checkActivationMayDeleteButton(const juce::KeyPress& key)
{
    auto button = std::make_unique<ButtonType>();
    button->setBounds(0, 0, 24, 24);
    button->setVisible(true);
    auto* rawButton = button.get();
    rawButton->onClick = [&button] { button.reset(); };

    CHECK(static_cast<juce::Component&>(*rawButton).keyPressed(key));
    CHECK(button == nullptr);
}

template <typename ButtonType>
void checkStateCallbackMayDeleteButton()
{
    auto button = std::make_unique<ButtonType>();
    button->setBounds(0, 0, 24, 24);
    button->setVisible(true);
    auto* rawButton = button.get();
    auto& component = static_cast<juce::Component&>(*rawButton);
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    component.mouseDown(makeMouseEvent(*rawButton, leftButton));
    REQUIRE(rawButton->isDown());
    rawButton->onStateChange = [&button] { button.reset(); };
    component.mouseExit(makeMouseEvent(*rawButton, {}));

    CHECK(button == nullptr);
}

template <typename ButtonType, typename Callback>
void checkLifecycleStateCallbackMayDeleteButton(Callback&& callback)
{
    auto button = std::make_unique<ButtonType>();
    button->setBounds(0, 0, 24, 24);
    button->setVisible(true);
    auto* rawButton = button.get();

    rawButton->setState(juce::Button::buttonDown);
    REQUIRE(rawButton->isDown());
    rawButton->onStateChange = [&button] { button.reset(); };

    callback(*rawButton);

    CHECK(button == nullptr);
}

template <typename ButtonType>
void checkHiddenTimerCallbackMayDeleteButton()
{
    juce::Component hiddenParent;
    hiddenParent.setVisible(false);

    auto button = std::make_unique<ButtonType>();
    button->setBounds(0, 0, 24, 24);
    hiddenParent.addAndMakeVisible(*button);
    auto* rawButton = button.get();

    rawButton->setState(juce::Button::buttonDown);
    REQUIRE(rawButton->isDown());
    rawButton->onStateChange = [&button] { button.reset(); };

    BandToggleButtonPointerTestAccess::runTimerCallback(*rawButton);

    CHECK(button == nullptr);
}
} // namespace

TEST_CASE("Band toggles reject popup and auxiliary mouse gestures",
          "[band-toggle][multiband][ui][input]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("physical right button")
    {
        forEachBandToggle([](auto& button)
        {
            checkRejectedGesture(
                button,
                juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier });
        });
    }

    SECTION("middle button")
    {
        forEachBandToggle([](auto& button)
        {
            checkRejectedGesture(
                button,
                juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier });
        });
    }

#if JUCE_MAC
    SECTION("macOS Control-click")
    {
        forEachBandToggle([](auto& button)
        {
            checkRejectedGesture(
                button,
                juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                                     | juce::ModifierKeys::ctrlModifier },
                juce::ModifierKeys { juce::ModifierKeys::ctrlModifier });
        });
    }
#endif
}

TEST_CASE("Band toggle interaction visuals transition and reset when hidden",
          "[band-toggle][multiband][ui][animation][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    forEachBandToggle([leftButton](auto& button)
    {
        auto& component = static_cast<juce::Component&>(button);
        component.mouseEnter(makeMouseEvent(button, {}));
        BandToggleButtonPointerTestAccess::advance(button, 1.0f / 60.0f);
        CHECK(BandToggleButtonPointerTestAccess::hover(button) > 0.0f);
        CHECK(BandToggleButtonPointerTestAccess::hover(button) < 1.0f);

        component.mouseDown(makeMouseEvent(button, leftButton));
        BandToggleButtonPointerTestAccess::advance(button, 1.0f / 60.0f);
        CHECK(BandToggleButtonPointerTestAccess::press(button) > 0.0f);

        button.setEnabled(false);
        REQUIRE_FALSE(button.isDown());
        BandToggleButtonPointerTestAccess::advance(button, 1.0f / 60.0f);
        CHECK(BandToggleButtonPointerTestAccess::enabled(button) > 0.0f);
        CHECK(BandToggleButtonPointerTestAccess::enabled(button) < 1.0f);

        button.setVisible(false);
        CHECK(BandToggleButtonPointerTestAccess::hover(button) == 0.0f);
        CHECK(BandToggleButtonPointerTestAccess::press(button) == 0.0f);
    });
}

TEST_CASE("Band toggles pair only accepted primary mouse gestures",
          "[band-toggle][multiband][ui][input][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("a primary click toggles exactly once")
    {
        forEachBandToggle([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);

            component.mouseDown(makeMouseEvent(button, leftButton));
            REQUIRE(button.isDown());
            component.mouseUp(makeMouseEvent(button, {}));

            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("an accepted drag is forwarded to JUCE before release")
    {
        forEachBandToggle([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);

            component.mouseDown(makeMouseEvent(button, leftButton));
            REQUIRE(button.isDown());

            // The headless mouse source is not over this unattached component.
            // Button::mouseDrag therefore leaves its down state only when the
            // accepted drag was actually forwarded.
            component.mouseDrag(makeMouseEvent(button, leftButton, true));
            CHECK_FALSE(button.isDown());
            component.mouseUp(makeMouseEvent(button, {}, true));

            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);
        });
    }

    SECTION("a new rejected down cancels stale primary ownership")
    {
        forEachBandToggle([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);

            component.mouseDown(makeMouseEvent(button, leftButton));
            REQUIRE(button.isDown());
            component.mouseDown(makeMouseEvent(
                button,
                juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier }));
            CHECK_FALSE(button.isDown());
            component.mouseUp(makeMouseEvent(button, {}));

            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);
        });
    }
}

TEST_CASE("Band toggles discard pointer ownership when hidden or disabled",
          "[band-toggle][multiband][ui][input][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("hidden")
    {
        forEachBandToggle([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);

            component.mouseDown(makeMouseEvent(button, leftButton));
            REQUIRE(button.isDown());
            button.setVisible(false);
            CHECK_FALSE(button.isDown());
            button.setVisible(true);

            // Recreate a hostile stale visual state: the old release must
            // still be ignored after the control returns.
            button.setState(juce::Button::buttonDown);
            component.mouseUp(makeMouseEvent(button, {}));
            CHECK_FALSE(button.isDown());
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);
        });
    }

    SECTION("disabled")
    {
        forEachBandToggle([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);

            component.mouseDown(makeMouseEvent(button, leftButton));
            REQUIRE(button.isDown());
            button.setEnabled(false);
            CHECK_FALSE(button.isDown());
            button.setEnabled(true);

            button.setState(juce::Button::buttonDown);
            component.mouseUp(makeMouseEvent(button, {}));
            CHECK_FALSE(button.isDown());
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 0);
        });
    }
}

TEST_CASE("Band toggles recover when a primary pointer release is lost",
          "[band-toggle][multiband][ui][input][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    forEachBandToggle([leftButton](auto& button)
    {
        int clickCount = 0;
        button.onClick = [&clickCount] { ++clickCount; };
        auto& component = static_cast<juce::Component&>(button);

        component.mouseDown(makeMouseEvent(button, leftButton));
        REQUIRE(button.isDown());

        // Pointer capture was lost, and the next owner event proves that its
        // primary button is no longer held.
        component.mouseMove(makeMouseEvent(button, {}));
        CHECK_FALSE(button.isDown());

        // A delayed release from the abandoned gesture must be inert.
        component.mouseUp(makeMouseEvent(button, {}));
        CHECK_FALSE(button.getToggleState());
        CHECK(clickCount == 0);
    });
}

TEST_CASE("Band toggles preserve keyboard and programmatic activation",
          "[band-toggle][multiband][ui][input][keyboard]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("triggerClick")
    {
        forEachBandToggle([](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            button.triggerClick();

            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("Return key")
    {
        forEachBandToggle([](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);

            REQUIRE(component.keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey }));

            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("Space key")
    {
        forEachBandToggle([](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);

            REQUIRE(component.keyPressed(
                juce::KeyPress { juce::KeyPress::spaceKey }));

            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }
}

TEST_CASE("Band toggle commands cannot outlive their visible topology slot",
          "[band-toggle][multiband][ui][input][keyboard][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    forEachBandToggle([](auto& button)
    {
        int clickCount = 0;
        button.onClick = [&clickCount] { ++clickCount; };

        // The command is committed before a host/topology update can hide the
        // fixed-index control. No queued command may be left for that slot.
        button.triggerClick();
        CHECK(button.getToggleState());
        CHECK(clickCount == 1);
        button.setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(button.getToggleState());
        CHECK(clickCount == 1);

        button.setVisible(true);
        button.setToggleState(false, juce::dontSendNotification);
        juce::Component hiddenParent;
        hiddenParent.addAndMakeVisible(button);
        hiddenParent.setVisible(false);
        button.triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK_FALSE(button.getToggleState());
        CHECK(clickCount == 1);
    });
}

TEST_CASE("Band toggles keep pointer ownership across foreign hover events",
          "[band-toggle][multiband][ui][input][source][multitouch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    forEachBandToggle([leftButton](auto& button)
    {
        int clickCount = 0;
        button.onClick = [&clickCount] { ++clickCount; };
        auto& component = static_cast<juce::Component&>(button);
        component.mouseDown(makeMouseEvent(button, leftButton));
        REQUIRE(button.isDown());
        REQUIRE(BandToggleButtonPointerTestAccess::hasPrimaryPointer(button));

        const auto source = juce::Desktop::getInstance().getMainMouseSource();
        BandToggleButtonPointerTestAccess::setTrackedPointerSource(
            button,
            source.getType() == juce::MouseInputSource::mouse
                ? juce::MouseInputSource::touch
                : juce::MouseInputSource::mouse,
            source.getIndex() + 23);

        component.mouseExit(makeMouseEvent(button, {}));
        CHECK(button.isDown());
        component.mouseMove(makeMouseEvent(button, {}));
        CHECK(button.isDown());
        component.mouseEnter(makeMouseEvent(button, {}));
        CHECK(button.isDown());
        CHECK(clickCount == 0);

        BandToggleButtonPointerTestAccess::setTrackedPointerSource(
            button, source.getType(), source.getIndex());
        component.mouseUp(makeMouseEvent(button, {}));

        CHECK_FALSE(button.isDown());
        CHECK(button.getToggleState());
        CHECK(clickCount == 1);
    });
}

TEST_CASE("Band toggle callbacks may synchronously delete their control",
          "[band-toggle][multiband][ui][input][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("keyboard activation")
    {
        checkActivationMayDeleteButton<SoloButton>(
            juce::KeyPress { juce::KeyPress::returnKey });
        checkActivationMayDeleteButton<EnableButton>(
            juce::KeyPress { juce::KeyPress::spaceKey });
    }

    SECTION("hover state transition")
    {
        checkStateCallbackMayDeleteButton<SoloButton>();
        checkStateCallbackMayDeleteButton<EnableButton>();
    }
}

TEST_CASE("Band toggle lifecycle callbacks stop after synchronous deletion",
          "[band-toggle][multiband][ui][animation][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("visibility change")
    {
        checkLifecycleStateCallbackMayDeleteButton<SoloButton>(
            [](auto& button) { button.setVisible(false); });
        checkLifecycleStateCallbackMayDeleteButton<EnableButton>(
            [](auto& button) { button.setVisible(false); });
    }

    SECTION("enablement change")
    {
        checkLifecycleStateCallbackMayDeleteButton<SoloButton>(
            [](auto& button)
            {
                BandToggleButtonPointerTestAccess::notifyEnablementChanged(button);
            });
        checkLifecycleStateCallbackMayDeleteButton<EnableButton>(
            [](auto& button)
            {
                BandToggleButtonPointerTestAccess::notifyEnablementChanged(button);
            });
    }

    SECTION("focus gained")
    {
        checkLifecycleStateCallbackMayDeleteButton<SoloButton>(
            [](auto& button)
            {
                BandToggleButtonPointerTestAccess::notifyFocusGained(button);
            });
        checkLifecycleStateCallbackMayDeleteButton<EnableButton>(
            [](auto& button)
            {
                BandToggleButtonPointerTestAccess::notifyFocusGained(button);
            });
    }

    SECTION("focus lost")
    {
        checkLifecycleStateCallbackMayDeleteButton<SoloButton>(
            [](auto& button)
            {
                BandToggleButtonPointerTestAccess::notifyFocusLost(button);
            });
        checkLifecycleStateCallbackMayDeleteButton<EnableButton>(
            [](auto& button)
            {
                BandToggleButtonPointerTestAccess::notifyFocusLost(button);
            });
    }

    SECTION("ancestor-hidden timer cleanup")
    {
        checkHiddenTimerCallbackMayDeleteButton<SoloButton>();
        checkHiddenTimerCallbackMayDeleteButton<EnableButton>();
    }

    SECTION("direct gesture dismissal")
    {
        checkLifecycleStateCallbackMayDeleteButton<SoloButton>(
            [](auto& button) { button.dismissPointerGesture(); });
        checkLifecycleStateCallbackMayDeleteButton<EnableButton>(
            [](auto& button) { button.dismissPointerGesture(); });
    }
}

TEST_CASE("Editor hiding discards active band-toggle gestures",
          "[band-toggle][multiband][ui][input][host-visibility][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    auto* soloButton = findDescendant<SoloButton>(*editor);
    auto* enableButton = findDescendant<EnableButton>(*editor);
    REQUIRE(soloButton != nullptr);
    REQUIRE(enableButton != nullptr);

    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    const auto checkButton = [&](auto& button)
    {
        int clickCount = 0;
        button.onClick = [&clickCount] { ++clickCount; };
        button.setToggleState(false, juce::dontSendNotification);
        auto& component = static_cast<juce::Component&>(button);
        CAPTURE(button.isVisible(), button.isEnabled(), button.getBounds().toString());

        component.mouseDown(makeMouseEvent(button, leftButton));
        REQUIRE(button.isDown());

        editor->setVisible(false);
        CHECK_FALSE(button.isDown());

        editor->setVisible(true);
        component.mouseUp(makeMouseEvent(button, {}));

        CHECK_FALSE(button.isDown());
        CHECK_FALSE(button.getToggleState());
        CHECK(clickCount == 0);
        button.onClick = nullptr;
    };

    checkButton(*soloButton);
    checkButton(*enableButton);
}
