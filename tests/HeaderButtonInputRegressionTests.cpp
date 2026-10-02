#include <GUI/PrimaryButton.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

struct PrimaryButtonTestAccess
{
    template <typename ButtonType>
    static bool hasPrimaryGesture(const PrimaryPointerButton<ButtonType>& button) noexcept
    {
        return button.pointerGesture == PrimaryPointerButton<ButtonType>::PointerGesture::primary;
    }

    template <typename ButtonType>
    static void setTrackedPointerSource(
        PrimaryPointerButton<ButtonType>& button,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        button.pointerSourceType = type;
        button.pointerSourceIndex = index;
    }

    template <typename ButtonType>
    static void setAnimationState(PrimaryPointerButton<ButtonType>& button,
                                  float hover,
                                  float press,
                                  float focus,
                                  float disabled) noexcept
    {
        button.hoverAnimation = hover;
        button.pressAnimation = press;
        button.focusAnimation = focus;
        button.disabledAnimation = disabled;
    }

    template <typename ButtonType>
    static bool isKeyboardFocusVisible(
        const PrimaryPointerButton<ButtonType>& button) noexcept
    {
        return button.focusModality.isKeyboardVisible();
    }

    template <typename ButtonType>
    static bool hasPresentedInteraction(
        const PrimaryPointerButton<ButtonType>& button) noexcept
    {
        return button.hasPresentedInteraction();
    }
};

namespace
{
template <typename Predicate>
bool dispatchUntil(const Predicate& finished)
{
    const auto started = juce::Time::getMillisecondCounter();
    while (!finished())
    {
        if (juce::Time::getMillisecondCounter() - started >= 1000
            || !juce::MessageManager::getInstance()->runDispatchLoopUntil(5))
            return false;
    }
    return true;
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

void detachTestPeerAndDrain(juce::Component& desktopHost,
                            juce::Component& child)
{
    desktopHost.removeFromDesktop();

    // On macOS the native peer can finish portions of its teardown on the
    // message queue.  Do not let callbacks belonging to this short-lived test
    // window escape into the next button instance (or the next test case),
    // after both stack components have already been destroyed.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    desktopHost.removeChildComponent(&child);
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

    PrimaryHyperlinkButton hyperlinkButton;
    hyperlinkButton.setButtonText("Hyperlink");
    hyperlinkButton.setBounds(0, 0, 80, 24);
    hyperlinkButton.setVisible(true);
    callback(hyperlinkButton);
}

template <typename Callback>
void forEachShowingPrimaryButtonType(Callback&& callback)
{
    forEachPrimaryButtonType([&callback](auto& button)
    {
        button.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        callback(button);
    });
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

PrimaryTextButton* findHeaderButton(juce::Component& editor,
                                    const juce::String& componentID,
                                    const juce::String& buttonText)
{
    std::vector<PrimaryTextButton*> buttons;
    collectHeaderButtons(editor, buttons);
    const auto match = std::find_if(
        buttons.begin(), buttons.end(), [&](const auto* button)
        {
            return button->getComponentID() == componentID
                && button->getButtonText() == buttonText;
        });
    return match != buttons.end() ? *match : nullptr;
}

class DeleteEditorOnStateNotification final
    : public juce::AudioProcessorListener
{
public:
    DeleteEditorOnStateNotification(
        FireAudioProcessor& processorToObserve,
        std::unique_ptr<FireAudioProcessorEditor>& editorToDelete)
        : processor(processorToObserve), editor(editorToDelete)
    {
        processor.addListener(this);
    }

    ~DeleteEditorOnStateNotification() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (! details.nonParameterStateChanged || notificationCount != 0)
            return;

        ++notificationCount;
        editor.reset();
        callbackCompleted = true;
    }

    FireAudioProcessor& processor;
    std::unique_ptr<FireAudioProcessorEditor>& editor;
    int notificationCount = 0;
    bool callbackCompleted = false;
};
} // namespace

TEST_CASE("Primary button keyboard modality stays idle without actual focus",
          "[header-button][ui][animation][focus][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PrimaryTextButton button { "Idle" };

    REQUIRE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
    CHECK_FALSE(button.hasKeyboardFocus(true));
    CHECK_FALSE(PrimaryButtonTestAccess::hasPresentedInteraction(button));
}

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

    SECTION("hover events from a different source cannot cancel ownership")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            auto& component = static_cast<juce::Component&>(button);
            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());

            const auto source = juce::Desktop::getInstance().getMainMouseSource();
            PrimaryButtonTestAccess::setTrackedPointerSource(
                button,
                source.getType() == juce::MouseInputSource::mouse
                    ? juce::MouseInputSource::touch
                    : juce::MouseInputSource::mouse,
                source.getIndex() + 1);

            component.mouseExit(makeMouseEvent(component, {}));
            CHECK(button.isDown());
            component.mouseMove(makeMouseEvent(component, {}));
            CHECK(button.isDown());
            component.mouseEnter(makeMouseEvent(component, {}));
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

TEST_CASE("Primary button state callbacks may synchronously delete their control",
          "[header-button][ui][input][primary-button][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    const auto makePressedButton = [&]
    {
        auto button = std::make_unique<PrimaryTextButton>("Delete");
        button->setBounds(0, 0, 80, 24);
        button->setVisible(true);
        beginPointerGesture(*button, leftButton);
        REQUIRE(button->isDown());
        return button;
    };

    SECTION("replacing stale ownership")
    {
        auto button = makePressedButton();
        auto* rawButton = button.get();
        const auto down = makeMouseEvent(*rawButton, leftButton);
        rawButton->onStateChange = [&button] { button.reset(); };

        static_cast<juce::Component&>(*rawButton).mouseDown(down);
        CHECK(button == nullptr);
    }

    SECTION("direct gesture dismissal")
    {
        auto button = makePressedButton();
        auto* rawButton = button.get();
        bool callbackStarted = false;
        rawButton->onStateChange = [&]
        {
            callbackStarted = true;
            button.reset();
        };

        rawButton->dismissPointerGesture();
        CHECK(callbackStarted);
        CHECK(button == nullptr);
    }

    SECTION("missing release recovery")
    {
        auto button = makePressedButton();
        auto* rawButton = button.get();
        const auto move = makeMouseEvent(*rawButton, {});
        bool callbackStarted = false;
        rawButton->onStateChange = [&]
        {
            callbackStarted = true;
            button.reset();
        };

        static_cast<juce::Component&>(*rawButton).mouseMove(move);
        CHECK(callbackStarted);
        CHECK(button == nullptr);
    }

    SECTION("hover state transition")
    {
        auto button = makePressedButton();
        auto* rawButton = button.get();
        const auto exit = makeMouseEvent(*rawButton, {});
        rawButton->onStateChange = [&button] { button.reset(); };

        static_cast<juce::Component&>(*rawButton).mouseExit(exit);
        CHECK(button == nullptr);
    }

    SECTION("visibility transition")
    {
        auto button = makePressedButton();
        auto* rawButton = button.get();
        rawButton->onStateChange = [&button] { button.reset(); };

        rawButton->setVisible(false);
        CHECK(button == nullptr);
    }

    SECTION("enablement transition")
    {
        auto button = makePressedButton();
        auto* rawButton = button.get();
        rawButton->onStateChange = [&button] { button.reset(); };

        rawButton->setEnabled(false);
        CHECK(button == nullptr);
    }

    SECTION("parent hierarchy transition")
    {
        juce::Component desktopHost;
        desktopHost.setBounds(0, 0, 120, 60);
        desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        desktopHost.setVisible(true);
        auto button = std::make_unique<PrimaryTextButton>("Delete");
        button->setBounds(0, 0, 80, 24);
        desktopHost.addAndMakeVisible(*button);
        beginPointerGesture(*button, leftButton);
        REQUIRE(button->isDown());

        auto* rawButton = button.get();
        rawButton->onStateChange = [&button] { button.reset(); };
        desktopHost.removeChildComponent(rawButton);

        CHECK(button == nullptr);
        desktopHost.removeFromDesktop();
    }

    SECTION("ancestor peer transition")
    {
        juce::Component desktopHost;
        desktopHost.setBounds(0, 0, 120, 60);
        desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        desktopHost.setVisible(true);
        auto button = std::make_unique<PrimaryTextButton>("Delete");
        button->setBounds(0, 0, 80, 24);
        desktopHost.addAndMakeVisible(*button);
        beginPointerGesture(*button, leftButton);
        REQUIRE(button->isDown());
        juce::MessageManager::getInstance()->runDispatchLoopUntil(350);

        bool callbackStarted = false;
        button->onStateChange = [&]
        {
            callbackStarted = true;
            button.reset();
        };
        desktopHost.removeFromDesktop();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(35);

        CHECK(callbackStarted);
        CHECK(button == nullptr);
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

TEST_CASE("Primary buttons discard gestures at hierarchy and peer boundaries",
          "[header-button][ui][input][primary-button][lifecycle][peer][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("removing the button from its parent is synchronous")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            juce::Component desktopHost;
            desktopHost.setBounds(0, 0, 120, 60);
            desktopHost.addAndMakeVisible(button);
            desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            desktopHost.setVisible(true);
            REQUIRE(button.isShowing());

            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());

            desktopHost.removeChildComponent(&button);

            CHECK_FALSE(button.isShowing());
            CHECK_FALSE(button.isDown());
            desktopHost.addAndMakeVisible(button);
            REQUIRE(button.isShowing());
            endPointerGesture(button);
            CHECK_FALSE(button.isDown());
            CHECK(clickCount == 0);

            performPointerGesture(button, leftButton);
            CHECK(clickCount == 1);
            detachTestPeerAndDrain(desktopHost, button);
        });
    }

    SECTION("a detached ancestor peer is recovered after the press settles")
    {
        forEachPrimaryButtonType([leftButton](auto& button)
        {
            juce::Component desktopHost;
            desktopHost.setBounds(0, 0, 120, 60);
            desktopHost.addAndMakeVisible(button);
            desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            desktopHost.setVisible(true);
            REQUIRE(button.isShowing());

            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            beginPointerGesture(button, leftButton);
            REQUIRE(button.isDown());

            // The lifecycle watch must outlive the short press animation.
            juce::MessageManager::getInstance()->runDispatchLoopUntil(350);
            REQUIRE(button.isDown());
            desktopHost.removeFromDesktop();
            REQUIRE_FALSE(button.isShowing());
            REQUIRE(dispatchUntil([&]
            {
                return !button.isDown() && !PrimaryButtonTestAccess::hasPrimaryGesture(button);
            }));

            CHECK_FALSE(button.isDown());
            desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            desktopHost.setVisible(true);
            REQUIRE(button.isShowing());
            endPointerGesture(button);
            CHECK_FALSE(button.isDown());
            CHECK(clickCount == 0);

            performPointerGesture(button, leftButton);
            CHECK(clickCount == 1);
            detachTestPeerAndDrain(desktopHost, button);
        });
    }
}

TEST_CASE("Primary buttons clear settled hover and focus after peer detachment",
          "[header-button][ui][animation][focus][lifecycle][peer][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Component desktopHost;
    PrimaryTextButton button { "Lifecycle" };
    desktopHost.setBounds(0, 0, 120, 60);
    button.setBounds(0, 0, 100, 28);
    desktopHost.addAndMakeVisible(button);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);
    REQUIRE(button.isShowing());

    button.grabKeyboardFocus();
    REQUIRE(button.hasKeyboardFocus(true));
    button.focusGained(juce::Component::focusChangedDirectly);
    REQUIRE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
    static_cast<juce::Component&>(button).mouseEnter(
        makeMouseEvent(button, {}));
    REQUIRE(button.getState() == juce::Button::buttonOver);

    juce::MessageManager::getInstance()->runDispatchLoopUntil(600);
    desktopHost.removeFromDesktop();
    REQUIRE_FALSE(button.isShowing());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(35);

    CHECK(button.getState() == juce::Button::buttonNormal);
    // Peer loss starts a fresh modality session. A later first direct focus is
    // keyboard-visible, while the detached control itself paints no focus.
    CHECK(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
    CHECK(button.getHoverAnimation() == 0.0f);
    CHECK(button.getPressAnimation() == 0.0f);
    CHECK(button.getFocusAnimation() == 0.0f);
}

TEST_CASE("Primary buttons recover when a pointer release is lost",
          "[header-button][ui][input][primary-button][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    forEachPrimaryButtonType([leftButton](auto& button)
    {
        int clickCount = 0;
        button.setClickingTogglesState(true);
        button.onClick = [&clickCount] { ++clickCount; };
        auto& component = static_cast<juce::Component&>(button);

        beginPointerGesture(button, leftButton);
        REQUIRE(button.isDown());

        component.mouseMove(makeMouseEvent(component, {}));
        CHECK_FALSE(button.isDown());

        endPointerGesture(button);
        CHECK_FALSE(button.getToggleState());
        CHECK(clickCount == 0);
    });
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
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("accessibility press")
    {
        forEachShowingPrimaryButtonType([](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };
            auto* accessibility = button.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            CHECK(accessibility->getRole()
                  == fire::ui::getButtonAccessibilityRole<
                         std::remove_cvref_t<decltype(button)>>());
            CHECK(accessibility->getValueInterface() != nullptr);

            REQUIRE(accessibility->getActions().invoke(
                juce::AccessibilityActionType::press));
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            REQUIRE(accessibility->getActions().contains(
                juce::AccessibilityActionType::toggle));
            REQUIRE(accessibility->getActions().invoke(
                juce::AccessibilityActionType::toggle));
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 2);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 2);
        });
    }

    SECTION("accessibility toggle preserves radio-group selection")
    {
        juce::Component parent;
        parent.setBounds(0, 0, 180, 40);
        parent.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        parent.setVisible(true);

        PrimaryTextButton first { "Band Lab" };
        PrimaryTextButton second { "Mod Forge" };
        first.setBounds(0, 0, 80, 24);
        second.setBounds(90, 0, 80, 24);
        parent.addAndMakeVisible(first);
        parent.addAndMakeVisible(second);

        constexpr int workspaceGroup = 1907;
        first.setClickingTogglesState(true);
        second.setClickingTogglesState(true);
        first.setRadioGroupId(workspaceGroup);
        second.setRadioGroupId(workspaceGroup);
        first.setToggleState(true, juce::dontSendNotification);

        int firstClickCount = 0;
        int secondClickCount = 0;
        first.onClick = [&firstClickCount] { ++firstClickCount; };
        second.onClick = [&secondClickCount] { ++secondClickCount; };

        auto* firstAccessibility = first.getAccessibilityHandler();
        auto* secondAccessibility = second.getAccessibilityHandler();
        REQUIRE(firstAccessibility != nullptr);
        REQUIRE(secondAccessibility != nullptr);
        CHECK(firstAccessibility->getRole()
              == juce::AccessibilityRole::radioButton);
        CHECK(secondAccessibility->getRole()
              == juce::AccessibilityRole::radioButton);

        REQUIRE(firstAccessibility->getActions().invoke(
            juce::AccessibilityActionType::toggle));
        CHECK(first.getToggleState());
        CHECK_FALSE(second.getToggleState());
        CHECK(firstClickCount == 0);
        CHECK(secondClickCount == 0);

        REQUIRE(secondAccessibility->getActions().invoke(
            juce::AccessibilityActionType::toggle));
        CHECK_FALSE(first.getToggleState());
        CHECK(second.getToggleState());
        // JUCE notifies the deselected sibling as part of a normal radio
        // click, so accessibility must preserve that existing contract too.
        CHECK(firstClickCount == 1);
        CHECK(secondClickCount == 1);
    }

    SECTION("hidden and peer-detached commands are inert")
    {
        forEachShowingPrimaryButtonType([](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };
            auto* accessibility = button.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);
            REQUIRE(accessibility->getActions().contains(
                juce::AccessibilityActionType::press));
            REQUIRE(accessibility->getActions().contains(
                juce::AccessibilityActionType::toggle));

            const auto checkInert = [&]
            {
                CHECK_FALSE(static_cast<juce::Component&>(button).keyPressed(
                    juce::KeyPress { juce::KeyPress::returnKey }));
                CHECK_FALSE(static_cast<juce::Component&>(button).keyPressed(
                    juce::KeyPress { juce::KeyPress::spaceKey }));
                REQUIRE(accessibility->getActions().invoke(
                    juce::AccessibilityActionType::press));
                REQUIRE(accessibility->getActions().invoke(
                    juce::AccessibilityActionType::toggle));
                CHECK_FALSE(button.getToggleState());
                CHECK(clickCount == 0);
            };

            button.setVisible(false);
            REQUIRE_FALSE(button.isShowing());
            checkInert();

            button.setVisible(true);
            REQUIRE(button.isShowing());
            button.removeFromDesktop();
            REQUIRE(button.isVisible());
            REQUIRE_FALSE(button.isShowing());
            checkInert();
        });
    }

    SECTION("Return and Space keys")
    {
        forEachShowingPrimaryButtonType([](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };

            REQUIRE(static_cast<juce::Component&>(button).keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey }));
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            REQUIRE(static_cast<juce::Component&>(button).keyPressed(
                juce::KeyPress { juce::KeyPress::spaceKey }));
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 2);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            CHECK_FALSE(button.getToggleState());
            CHECK(clickCount == 2);
        });
    }

    SECTION("disabled commands are inert")
    {
        forEachPrimaryButtonType([](auto& button)
        {
            int clickCount = 0;
            button.onClick = [&clickCount] { ++clickCount; };
            button.setEnabled(false);

            button.triggerClick();
            CHECK_FALSE(static_cast<juce::Component&>(button).keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey }));
            CHECK_FALSE(static_cast<juce::Component&>(button).keyPressed(
                juce::KeyPress { juce::KeyPress::spaceKey }));
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            CHECK(clickCount == 0);
        });
    }
}

TEST_CASE("Primary buttons distinguish pointer focus from keyboard focus",
          "[header-button][ui][input][primary-button][focus][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    forEachShowingPrimaryButtonType([leftButton](auto& button)
    {
        button.grabKeyboardFocus();
        REQUIRE(button.hasKeyboardFocus(true));

        button.focusGained(juce::Component::focusChangedByMouseClick);
        CHECK_FALSE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
        button.focusLost(juce::Component::focusChangedDirectly);
        CHECK_FALSE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
        button.focusGained(juce::Component::focusChangedDirectly);
        CHECK_FALSE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));

        // Keyboard use after a pointer-originated focus must reveal the focus
        // affordance again without delaying the command.
        REQUIRE(button.keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));

        beginPointerGesture(button, leftButton);
        CHECK_FALSE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
        endPointerGesture(button);
        CHECK_FALSE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));

        REQUIRE(button.keyPressed(
            juce::KeyPress { juce::KeyPress::spaceKey }));
        REQUIRE(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
        button.focusLost(juce::Component::focusChangedDirectly);
        CHECK(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
        button.focusGained(juce::Component::focusChangedDirectly);
        CHECK(PrimaryButtonTestAccess::isKeyboardFocusVisible(button));
    });
}

TEST_CASE("Primary button commands may synchronously delete their control",
          "[header-button][ui][input][primary-button][keyboard][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto invokeDeletingCommand = [](const auto& invoke)
    {
        auto button = std::make_unique<PrimaryTextButton>("Delete");
        button->setBounds(0, 0, 80, 24);
        button->setVisible(true);
        button->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        button->onClick = [&button] { button.reset(); };
        auto* rawButton = button.get();

        invoke(*rawButton);
        CHECK(button == nullptr);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    };

    SECTION("triggerClick")
    {
        invokeDeletingCommand([](PrimaryTextButton& button)
        {
            button.triggerClick();
        });
    }

    SECTION("Return")
    {
        invokeDeletingCommand([](PrimaryTextButton& button)
        {
            CHECK(button.keyPressed(
                juce::KeyPress { juce::KeyPress::returnKey }));
        });
    }

    SECTION("Space")
    {
        invokeDeletingCommand([](PrimaryTextButton& button)
        {
            CHECK(button.keyPressed(
                juce::KeyPress { juce::KeyPress::spaceKey }));
        });
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
    REQUIRE(buttons.size() == 12);

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
    CHECK(componentIdCounts["header_preset_browser"] == 1);

    auto* zoom = dynamic_cast<PrimaryTextButton*>(editor->findChildWithID("zoom"));
    REQUIRE(zoom != nullptr);
    CHECK(zoom->getTitle() == "Toggle spectrum zoom");
    CHECK(zoom->getTooltip() == "Toggle spectrum zoom");
    REQUIRE(zoom->isToggleable());

    auto* zoomAccessibility = zoom->getAccessibilityHandler();
    REQUIRE(zoomAccessibility != nullptr);
    REQUIRE(zoomAccessibility->getValueInterface() != nullptr);
    CHECK(zoomAccessibility->getCurrentState().isCheckable());
    CHECK_FALSE(zoomAccessibility->getCurrentState().isChecked());

    REQUIRE(zoom->keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    CHECK(zoom->getToggleState());
    CHECK(zoomAccessibility->getCurrentState().isChecked());

    REQUIRE(zoom->keyPressed(
        juce::KeyPress { juce::KeyPress::spaceKey }));
    CHECK_FALSE(zoom->getToggleState());
    CHECK_FALSE(zoomAccessibility->getCurrentState().isChecked());

    const auto checkButtonDescription =
        [&editor](const juce::String& componentID,
                  const juce::String& buttonText,
                  const juce::String& expectedTitle,
                  const juce::String& expectedTooltip)
    {
        auto* button = findHeaderButton(*editor, componentID, buttonText);
        REQUIRE(button != nullptr);
        CHECK(button->getTitle() == expectedTitle);
        CHECK(button->getTooltip() == expectedTooltip);
        REQUIRE(button->getAccessibilityHandler() != nullptr);
        CHECK(button->getAccessibilityHandler()->getTitle()
              == expectedTitle);
        CHECK(button->getAccessibilityHandler()->getHelp()
              == expectedTooltip);
    };

    checkButtonDescription("header_hq",
                           "HQ",
                           "High-quality oversampling",
                           "High-quality oversampling");
    checkButtonDescription("workspace_tab",
                           "BAND LAB",
                           "Band processing workspace",
                           "Edit multiband processing");
    checkButtonDescription("workspace_tab",
                           "MOD FORGE",
                           "LFO modulation workspace",
                           "Edit and assign LFO modulation");
    checkButtonDescription("workspace_tab",
                           "MASTER LAB",
                           "Master processing workspace",
                           "Edit global processing and filters");
    checkButtonDescription("header_action",
                           "Save",
                           "Save preset",
                           "Save preset");
    checkButtonDescription("header_preset_browser",
                           "- Init -",
                           "Browse presets",
                           "Open the full-page sound library");

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

TEST_CASE("Overlapping workspace hover press and focus keep a valid opacity",
          "[header-button][ui][animation][colour][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    PrimaryTextButton button { "MOD FORGE" };
    button.setBounds(0, 0, 120, 28);
    button.setComponentID("workspace_tab");
    button.getProperties().set("fireAnimatedSelection", true);
    button.setColour(juce::TextButton::buttonColourId,
                     juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOnId,
                     fire::ui::colours::modulation);
    button.setColour(juce::TextButton::textColourOffId,
                     fire::ui::colours::textMuted);

    // A real click can temporarily have all three animations at full weight.
    // The visual blend must saturate before it reaches Colour::withAlpha.
    PrimaryButtonTestAccess::setAnimationState(button, 1.0f, 1.0f, 1.0f, 0.0f);
    CHECK(fire::ui::headerInteractionWashAlpha(1.0f, 1.0f, 1.0f, 0.0f)
          == 1.0f);
    juce::Image image(juce::Image::ARGB, 120, 28, true);
    {
        juce::Graphics graphics(image);
        lookAndFeel.drawButtonBackground(graphics,
                                         button,
                                         juce::Colours::transparentBlack,
                                         true,
                                         true);
    }

    CHECK(image.getPixelAt(60, 14).getAlpha() > 0);
}

TEST_CASE("Editor peer detachment discards every header pointer gesture",
          "[header-button][ui][input][editor][peer][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    REQUIRE(editor->isShowing());

    std::vector<PrimaryTextButton*> buttons;
    collectHeaderButtons(*editor, buttons);
    REQUIRE(buttons.size() == 12);
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    int clickCount = 0;
    for (auto* button : buttons)
    {
        REQUIRE(button != nullptr);
        button->onClick = [&clickCount] { ++clickCount; };
        beginPointerGesture(*button, leftButton);
        REQUIRE(button->isDown());
    }

    editor->removeFromDesktop();
    REQUIRE(editor->isVisible());
    REQUIRE_FALSE(editor->isShowing());
    editor->timerCallback();

    for (auto* button : buttons)
    {
        CHECK_FALSE(button->isDown());
        endPointerGesture(*button);
        CHECK_FALSE(button->isDown());
        button->onClick = nullptr;
    }
    CHECK(clickCount == 0);
}

TEST_CASE("A-B header actions survive synchronous editor deletion by the host",
          "[header-button][ui][preset][host][lifetime][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    DeleteEditorOnStateNotification deleteOnChange(processor, editor);

    SECTION("toggle A-B")
    {
        auto* button = findHeaderButton(*editor, "header_ab", "A");
        REQUIRE(button != nullptr);
        REQUIRE(processor.stateAB.isCurrentA());

        CHECK(button->keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));

        CHECK(deleteOnChange.notificationCount == 1);
        CHECK(deleteOnChange.callbackCompleted);
        CHECK(editor == nullptr);
        CHECK_FALSE(processor.stateAB.isCurrentA());
        CHECK_FALSE(processor.isMultibandTopologyEditInProgress());
    }

    SECTION("copy current state")
    {
        auto* button = findHeaderButton(*editor, "header_action", "Copy");
        REQUIRE(button != nullptr);

        CHECK(button->keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));

        CHECK(deleteOnChange.notificationCount == 1);
        CHECK(deleteOnChange.callbackCompleted);
        CHECK(editor == nullptr);
        CHECK_FALSE(processor.isMultibandTopologyEditInProgress());
    }
}
