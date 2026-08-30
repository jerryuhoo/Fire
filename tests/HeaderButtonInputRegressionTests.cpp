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

    PrimaryHyperlinkButton hyperlinkButton;
    hyperlinkButton.setButtonText("Hyperlink");
    hyperlinkButton.setBounds(0, 0, 80, 24);
    hyperlinkButton.setVisible(true);
    callback(hyperlinkButton);
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
        forEachPrimaryButtonType([](auto& button)
        {
            int clickCount = 0;
            button.setClickingTogglesState(true);
            button.onClick = [&clickCount] { ++clickCount; };
            button.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            const juce::ScopeGuard removeFromDesktop {
                [&button] { button.removeFromDesktop(); }
            };
            auto* accessibility = button.getAccessibilityHandler();
            REQUIRE(accessibility != nullptr);

            REQUIRE(accessibility->getActions().invoke(
                juce::AccessibilityActionType::press));
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);

            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            CHECK(button.getToggleState());
            CHECK(clickCount == 1);
        });
    }

    SECTION("Return and Space keys")
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

TEST_CASE("Primary button commands may synchronously delete their control",
          "[header-button][ui][input][primary-button][keyboard][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto invokeDeletingCommand = [](const auto& invoke)
    {
        auto button = std::make_unique<PrimaryTextButton>("Delete");
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

TEST_CASE("A-B header actions survive synchronous editor deletion by the host",
          "[header-button][ui][preset][host][lifetime][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
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
